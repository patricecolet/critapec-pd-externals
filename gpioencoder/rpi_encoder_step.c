#include "m_pd.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__) && !defined(RPI_ENCODER_FORCE_STUB)
#define RPI_ENCODER_HAVE_GPIOD 1
#endif

#ifdef RPI_ENCODER_HAVE_GPIOD
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <gpiod.h>
#endif

#ifndef RPI_ENCODER_DEFAULT_INTERVAL
#define RPI_ENCODER_DEFAULT_INTERVAL 2.0
#endif

static t_class *rpi_encoder_step_class;

typedef struct _rpi_encoder_step {
    t_object  x_obj;
    t_outlet *x_value_out;
    t_outlet *x_switch_out;
    t_clock  *x_clock;

    t_float x_min;
    t_float x_max;
    t_float x_step;
    t_float x_value;
    t_float x_interval;
    t_float x_switch_value;
    int     x_partial_steps;

#ifdef RPI_ENCODER_HAVE_GPIOD
    struct gpiod_chip *x_chip;
    struct gpiod_line *x_line_a;
    struct gpiod_line *x_line_b;
    struct gpiod_line *x_line_sw;

    int x_line_a_offset;
    int x_line_b_offset;
    int x_line_sw_offset;

    atomic_int x_running;
    atomic_int x_pending_steps;
    atomic_int x_switch_state;
    pthread_t  x_thread;
#else
    int x_stub_warned;
#endif
} t_rpi_encoder_step;

static void rpi_encoder_step_tick(t_rpi_encoder_step *x);

static inline t_float rpi_encoder_step_clamp(t_float v, t_float min, t_float max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    return v;
}

static void rpi_encoder_step_bang(t_rpi_encoder_step *x)
{
    outlet_float(x->x_value_out, x->x_value);
    outlet_float(x->x_switch_out, x->x_switch_value);
}

#ifdef RPI_ENCODER_HAVE_GPIOD

#ifndef GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP
#define GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP 0
#endif

static int rpi_encoder_step_read_phase(t_rpi_encoder_step *x)
{
    int a = gpiod_line_get_value(x->x_line_a);
    int b = gpiod_line_get_value(x->x_line_b);
    if (a < 0 || b < 0)
        return -1;
    return ((a ? 1 : 0) << 1) | (b ? 1 : 0);
}

static int rpi_encoder_step_decode_direction(int previous, int current)
{
    static const int table[16] = {
        0, -1, +1, 0,
        +1, 0, 0, -1,
        -1, 0, 0, +1,
        0, +1, -1, 0
    };
    if (previous < 0 || current < 0)
        return 0;
    return table[(previous << 2) | current];
}

static void rpi_encoder_step_close_lines(t_rpi_encoder_step *x)
{
    if (x->x_line_a) {
        gpiod_line_release(x->x_line_a);
        x->x_line_a = NULL;
    }
    if (x->x_line_b) {
        gpiod_line_release(x->x_line_b);
        x->x_line_b = NULL;
    }
    if (x->x_line_sw) {
        gpiod_line_release(x->x_line_sw);
        x->x_line_sw = NULL;
    }
    if (x->x_chip) {
        gpiod_chip_close(x->x_chip);
        x->x_chip = NULL;
    }
}

static int rpi_encoder_request_line_events(struct gpiod_chip *chip, int offset,
                                           struct gpiod_line **line)
{
    *line = NULL;
    struct gpiod_line *ln = gpiod_chip_get_line(chip, offset);
    if (!ln)
        return -1;
    int ret = gpiod_line_request_both_edges_events_flags(
        ln, "rpi_encoder_step", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    if (ret) {
        gpiod_line_release(ln);
        return -1;
    }
    *line = ln;
    return 0;
}

static int rpi_encoder_request_switch(struct gpiod_chip *chip, int offset,
                                      struct gpiod_line **line)
{
    *line = NULL;
    struct gpiod_line *ln = gpiod_chip_get_line(chip, offset);
    if (!ln)
        return -1;
    int ret = gpiod_line_request_input_flags(
        ln, "rpi_encoder_step", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    if (ret) {
        gpiod_line_release(ln);
        return -1;
    }
    *line = ln;
    return 0;
}

static struct gpiod_chip *rpi_encoder_open_chip(int chip_index)
{
    struct gpiod_chip *chip = gpiod_chip_open_by_number(chip_index);
    if (!chip) {
        char name[32];
        snprintf(name, sizeof(name), "gpiochip%d", chip_index);
        chip = gpiod_chip_open_by_name(name);
    }
    return chip;
}

static int rpi_encoder_step_open_lines(t_rpi_encoder_step *x, int chip_index,
                                       int line_a, int line_b, int line_sw)
{
    rpi_encoder_step_close_lines(x);

    x->x_chip = rpi_encoder_open_chip(chip_index);
    if (!x->x_chip) {
        pd_error(x, "rpi_encoder_step: impossible d'ouvrir gpiochip %d", chip_index);
        return -1;
    }

    if (rpi_encoder_request_line_events(x->x_chip, line_a, &x->x_line_a)) {
        pd_error(x, "rpi_encoder_step: impossible de réserver la ligne A (%d)", line_a);
        rpi_encoder_step_close_lines(x);
        return -1;
    }
    if (rpi_encoder_request_line_events(x->x_chip, line_b, &x->x_line_b)) {
        pd_error(x, "rpi_encoder_step: impossible de réserver la ligne B (%d)", line_b);
        rpi_encoder_step_close_lines(x);
        return -1;
    }
    if (rpi_encoder_request_switch(x->x_chip, line_sw, &x->x_line_sw)) {
        pd_error(x, "rpi_encoder_step: impossible de réserver le switch (%d)", line_sw);
        rpi_encoder_step_close_lines(x);
        return -1;
    }

    x->x_line_a_offset = line_a;
    x->x_line_b_offset = line_b;
    x->x_line_sw_offset = line_sw;

    int sw = gpiod_line_get_value(x->x_line_sw);
    if (sw >= 0)
        atomic_store(&x->x_switch_state, sw ? 1 : 0);
    atomic_store(&x->x_pending_steps, 0);
    x->x_partial_steps = 0;

    return 0;
}

static void *rpi_encoder_step_poll(void *userdata)
{
    t_rpi_encoder_step *x = (t_rpi_encoder_step *)userdata;
    int fd_a = gpiod_line_event_get_fd(x->x_line_a);
    int fd_b = gpiod_line_event_get_fd(x->x_line_b);
    struct pollfd pfds[2];
    pfds[0].fd = fd_a;
    pfds[0].events = POLLIN;
    pfds[1].fd = fd_b;
    pfds[1].events = POLLIN;

    int last_state = rpi_encoder_step_read_phase(x);

    while (atomic_load(&x->x_running)) {
        int ret = poll(pfds, 2, 100);
        if (ret < 0) {
            continue;
        }
        if (ret > 0) {
            if (pfds[0].revents & POLLIN) {
                struct gpiod_line_event ev;
                gpiod_line_event_read(x->x_line_a, &ev);
            }
            if (pfds[1].revents & POLLIN) {
                struct gpiod_line_event ev;
                gpiod_line_event_read(x->x_line_b, &ev);
            }

            int state = rpi_encoder_step_read_phase(x);
            int dir = rpi_encoder_step_decode_direction(last_state, state);
            if (dir != 0)
                atomic_fetch_add(&x->x_pending_steps, dir);
            last_state = state;
        }

        int sw = gpiod_line_get_value(x->x_line_sw);
        if (sw >= 0)
            atomic_store(&x->x_switch_state, sw ? 1 : 0);
    }

    return NULL;
}

static int rpi_encoder_step_start_thread(t_rpi_encoder_step *x)
{
    atomic_store(&x->x_running, 1);
    if (pthread_create(&x->x_thread, NULL, rpi_encoder_step_poll, x)) {
        pd_error(x, "rpi_encoder_step: impossible de démarrer le thread GPIO");
        atomic_store(&x->x_running, 0);
        return -1;
    }
    return 0;
}

static void rpi_encoder_step_stop_thread(t_rpi_encoder_step *x)
{
    if (!atomic_load(&x->x_running))
        return;
    atomic_store(&x->x_running, 0);
    pthread_join(x->x_thread, NULL);
}

#endif /* RPI_ENCODER_HAVE_GPIOD */

static void rpi_encoder_step_set_interval(t_rpi_encoder_step *x, t_floatarg f)
{
    if (f <= 0) {
        pd_error(x, "rpi_encoder_step: intervalle doit être > 0 ms");
        return;
    }
    x->x_interval = f;
}

static void rpi_encoder_step_set_step(t_rpi_encoder_step *x, t_floatarg f)
{
    if (f == 0) {
        pd_error(x, "rpi_encoder_step: step ne peut pas être 0");
        return;
    }
    x->x_step = fabs(f);
}

static void rpi_encoder_step_set_min(t_rpi_encoder_step *x, t_floatarg f)
{
    x->x_min = f;
    if (x->x_min > x->x_max) {
        t_float tmp = x->x_min;
        x->x_min = x->x_max;
        x->x_max = tmp;
    }
    x->x_value = rpi_encoder_step_clamp(x->x_value, x->x_min, x->x_max);
}

static void rpi_encoder_step_set_max(t_rpi_encoder_step *x, t_floatarg f)
{
    x->x_max = f;
    if (x->x_max < x->x_min) {
        t_float tmp = x->x_max;
        x->x_max = x->x_min;
        x->x_min = tmp;
    }
    x->x_value = rpi_encoder_step_clamp(x->x_value, x->x_min, x->x_max);
}

static void rpi_encoder_step_set_value(t_rpi_encoder_step *x, t_floatarg f)
{
    x->x_value = rpi_encoder_step_clamp(f, x->x_min, x->x_max);
    x->x_partial_steps = 0;
    outlet_float(x->x_value_out, x->x_value);
}

static void rpi_encoder_step_reset(t_rpi_encoder_step *x)
{
    x->x_value = rpi_encoder_step_clamp(0, x->x_min, x->x_max);
    x->x_partial_steps = 0;
    outlet_float(x->x_value_out, x->x_value);
}

static void rpi_encoder_step_tick(t_rpi_encoder_step *x)
{
#ifdef RPI_ENCODER_HAVE_GPIOD
    int delta = atomic_exchange(&x->x_pending_steps, 0);
    if (delta != 0) {
        x->x_partial_steps += delta;
        int clicks = x->x_partial_steps / 4;
        if (clicks != 0) {
            x->x_partial_steps -= clicks * 4;
            x->x_value = rpi_encoder_step_clamp(
                x->x_value + ((t_float)clicks * x->x_step), x->x_min, x->x_max);
            outlet_float(x->x_value_out, x->x_value);
        }
    }
    x->x_switch_value = (t_float)atomic_load(&x->x_switch_state);
    outlet_float(x->x_switch_out, x->x_switch_value);
#else
    if (!x->x_stub_warned) {
        pd_error(x, "rpi_encoder_step: compilé sans support libgpiod (plateforme non Linux)");
        x->x_stub_warned = 1;
    }
    outlet_float(x->x_switch_out, x->x_switch_value);
#endif
    clock_delay(x->x_clock, x->x_interval);
}

static void rpi_encoder_step_free(t_rpi_encoder_step *x)
{
    if (x->x_clock) {
        clock_free(x->x_clock);
        x->x_clock = NULL;
    }
#ifdef RPI_ENCODER_HAVE_GPIOD
    rpi_encoder_step_stop_thread(x);
    rpi_encoder_step_close_lines(x);
#endif
}

static void *rpi_encoder_step_new(t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    t_float chip_index = 0;
    t_float line_a = 17;
    t_float line_b = 27;
    t_float line_sw = 22;
    t_float fmin = 0;
    t_float fmax = 127;
    t_float fstep = 1;
    t_float finterval = RPI_ENCODER_DEFAULT_INTERVAL;

    for (int i = 0; i < argc && i < 8; ++i) {
        t_float val = atom_getfloat(argv + i);
        switch (i) {
        case 0: chip_index = val; break;
        case 1: line_a = val; break;
        case 2: line_b = val; break;
        case 3: line_sw = val; break;
        case 4: fmin = val; break;
        case 5: fmax = val; break;
        case 6: fstep = val; break;
        case 7: finterval = val; break;
        default: break;
        }
    }

#ifndef RPI_ENCODER_HAVE_GPIOD
    (void)chip_index;
    (void)line_a;
    (void)line_b;
    (void)line_sw;
#endif

    t_rpi_encoder_step *x = (t_rpi_encoder_step *)pd_new(rpi_encoder_step_class);

    x->x_min = (fmin == 0.0 && fmax == 0.0) ? 0.0 : fmin;
    x->x_max = (fmin == 0.0 && fmax == 0.0) ? 127.0 : (fmax != 0.0 ? fmax : 127.0);
    if (x->x_min == x->x_max)
        x->x_max = x->x_min + 1.0;
    if (x->x_min > x->x_max) {
        t_float tmp = x->x_min;
        x->x_min = x->x_max;
        x->x_max = tmp;
    }

    if (fstep == 0.0)
        fstep = 1.0;
    x->x_step = fabs(fstep);
    x->x_value = rpi_encoder_step_clamp(0, x->x_min, x->x_max);
    x->x_switch_value = 1.0;
    x->x_partial_steps = 0;

    if (finterval <= 0)
        finterval = RPI_ENCODER_DEFAULT_INTERVAL;
    x->x_interval = finterval;

    x->x_value_out = outlet_new(&x->x_obj, &s_float);
    x->x_switch_out = outlet_new(&x->x_obj, &s_float);
    x->x_clock = clock_new(x, (t_method)rpi_encoder_step_tick);

#ifdef RPI_ENCODER_HAVE_GPIOD
    atomic_store(&x->x_pending_steps, 0);
    atomic_store(&x->x_switch_state, 1);
    x->x_partial_steps = 0;
    if (rpi_encoder_step_open_lines(x,
            (chip_index < 0) ? 0 : (int)chip_index,
            (line_a <= 0) ? 17 : (int)line_a,
            (line_b <= 0) ? 27 : (int)line_b,
            (line_sw <= 0) ? 22 : (int)line_sw) == 0) {
        if (rpi_encoder_step_start_thread(x) != 0) {
            rpi_encoder_step_close_lines(x);
        }
    }
#else
    x->x_stub_warned = 0;
    post("rpi_encoder_step: compilation sans libgpiod (mode stub)");
#endif

    clock_delay(x->x_clock, x->x_interval);
    return (void *)x;
}

void rpi_encoder_step_setup(void)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
#endif
    rpi_encoder_step_class = class_new(gensym("rpi_encoder_step"),
        (t_newmethod)rpi_encoder_step_new,
        (t_method)rpi_encoder_step_free,
        sizeof(t_rpi_encoder_step),
        CLASS_DEFAULT,
        A_GIMME, 0);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    class_addmethod(rpi_encoder_step_class,
        (t_method)rpi_encoder_step_set_min, gensym("min"), A_FLOAT, 0);
    class_addmethod(rpi_encoder_step_class,
        (t_method)rpi_encoder_step_set_max, gensym("max"), A_FLOAT, 0);
    class_addmethod(rpi_encoder_step_class,
        (t_method)rpi_encoder_step_set_step, gensym("step"), A_FLOAT, 0);
    class_addmethod(rpi_encoder_step_class,
        (t_method)rpi_encoder_step_set_value, gensym("set"), A_FLOAT, 0);
    class_addmethod(rpi_encoder_step_class,
        (t_method)rpi_encoder_step_reset, gensym("reset"), 0);
    class_addmethod(rpi_encoder_step_class,
        (t_method)rpi_encoder_step_set_interval, gensym("interval"), A_FLOAT, 0);

    class_addbang(rpi_encoder_step_class, rpi_encoder_step_bang);

    post("rpi_encoder_step - encodeur quadrature + switch pour Raspberry Pi");
#ifdef RPI_ENCODER_HAVE_GPIOD
    post("  Support libgpiod actif (Linux)");
#else
    post("  Mode stub (libgpiod non disponible)");
#endif
}


