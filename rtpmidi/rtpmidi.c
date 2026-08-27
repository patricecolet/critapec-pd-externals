/* rtpmidi.c - Non-macOS stub. Compiled on Linux and Windows only; the macOS
 * backend lives in rtpmidi.m. Loads, posts a setup hint, and errors on any
 * action message so the user sees what to configure manually.
 *
 * Copyright (C) 2025 Patrice Colet
 * GPL-2.0+
 */

#include "m_pd.h"

#ifndef VERSION
# define VERSION "0.1"
#endif

static t_class *rtpmidi_class;

typedef struct _rtpmidi {
    t_object x_obj;
    t_outlet *x_status_out;
} t_rtpmidi;

static void rtpmidi_platform_hint(void)
{
#if defined(__linux__)
    post("rtpmidi: Linux - install and enable rtpmidid (Avahi handles discovery); see README");
#elif defined(_WIN32)
    post("rtpmidi: Windows - install Tobias Erichsen's rtpMIDI driver; see README");
#else
    post("rtpmidi: unknown platform - see README for manual setup");
#endif
}

static void rtpmidi_not_supported(t_rtpmidi *x, t_symbol *s)
{
    pd_error(x, "rtpmidi: '%s' is macOS-only; configure RTP-MIDI manually on this OS (see README)",
             s ? s->s_name : "(command)");
}

static void rtpmidi_enable(t_rtpmidi *x, t_floatarg f)
{
    (void)f;
    rtpmidi_not_supported(x, gensym("enable"));
}

static void rtpmidi_policy(t_rtpmidi *x, t_symbol *s)
{
    (void)s;
    rtpmidi_not_supported(x, gensym("policy"));
}

static void rtpmidi_connect(t_rtpmidi *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s; (void)argc; (void)argv;
    rtpmidi_not_supported(x, gensym("connect"));
}

static void rtpmidi_scan(t_rtpmidi *x, t_floatarg f)
{
    (void)f;
    rtpmidi_not_supported(x, gensym("scan"));
}

static void rtpmidi_disconnect(t_rtpmidi *x)
{
    rtpmidi_not_supported(x, gensym("disconnect"));
}

static void rtpmidi_bang(t_rtpmidi *x)
{
    rtpmidi_platform_hint();
    (void)x;
}

static void *rtpmidi_new(void)
{
    t_rtpmidi *x = (t_rtpmidi *)pd_new(rtpmidi_class);
    x->x_status_out = outlet_new(&x->x_obj, 0);
    rtpmidi_platform_hint();
    return (void *)x;
}

static void rtpmidi_free(t_rtpmidi *x)
{
    (void)x;
}

void rtpmidi_setup(void)
{
    rtpmidi_class = class_new(gensym("rtpmidi"),
        (t_newmethod)rtpmidi_new,
        (t_method)rtpmidi_free,
        sizeof(t_rtpmidi),
        CLASS_DEFAULT,
        0);
    class_addmethod(rtpmidi_class, (t_method)rtpmidi_enable,
        gensym("enable"), A_FLOAT, 0);
    class_addmethod(rtpmidi_class, (t_method)rtpmidi_policy,
        gensym("policy"), A_SYMBOL, 0);
    class_addmethod(rtpmidi_class, (t_method)rtpmidi_scan,
        gensym("scan"), A_FLOAT, 0);
    class_addmethod(rtpmidi_class, (t_method)rtpmidi_connect,
        gensym("connect"), A_GIMME, 0);
    class_addmethod(rtpmidi_class, (t_method)rtpmidi_disconnect,
        gensym("disconnect"), 0);
    class_addbang(rtpmidi_class, (t_method)rtpmidi_bang);
    post("rtpmidi v%s - RTP-MIDI helper (non-macOS stub)", VERSION);
}
