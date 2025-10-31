/* bytes2int - Un external pour Pure Data qui convertit 4 bytes en int32 (endianness sélectionnable)
 *
 * Copyright (C) 2025 Patrice Colet
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "m_pd.h"
#include <stdint.h>

#ifndef VERSION
# define VERSION "0.1"
#endif

static t_class *bytes2int_class;

typedef struct _bytes2int
{
    t_object x_obj;
    t_outlet *x_out_int;   // outlet 0: int32 converti
    t_outlet *x_out_extra; // outlet 1: bytes supplémentaires ou invalides
    int x_little_endian;   // 1 = little (défaut), 0 = big
} t_bytes2int;

static void bytes2int_set_endian(t_bytes2int *x, t_symbol *s)
{
    if (s == gensym("big")) {
        x->x_little_endian = 0;
    } else if (s == gensym("little")) {
        x->x_little_endian = 1;
    } else {
        pd_error(x, "bytes2int: endian doit être 'little' ou 'big'");
        return;
    }
}

static void bytes2int_list(t_bytes2int *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s; // unused
    if (argc == 0) {
        return;
    }

    // Vérifier la validité des bytes (0..255)
    int has_invalid = 0;
    for (int i = 0; i < argc; i++) {
        t_float v = atom_getfloat(&argv[i]);
        if (v < 0.0 || v > 255.0) {
            has_invalid = 1;
            break;
        }
    }
    if (has_invalid) {
        outlet_list(x->x_out_extra, &s_list, argc, argv);
        return;
    }

    // Préparer les 4 bytes (compléter avec 0 si < 4)
    uint8_t b[4] = {0, 0, 0, 0};
    int n = argc < 4 ? argc : 4;
    for (int i = 0; i < n; i++) {
        b[i] = (uint8_t)atom_getfloat(&argv[i]);
    }

    // Conversion vers int32 selon l'endianness
    uint32_t u = 0;
    if (x->x_little_endian) {
        u = ((uint32_t)b[0]) |
            ((uint32_t)b[1] << 8) |
            ((uint32_t)b[2] << 16) |
            ((uint32_t)b[3] << 24);
    } else {
        u = ((uint32_t)b[0] << 24) |
            ((uint32_t)b[1] << 16) |
            ((uint32_t)b[2] << 8)  |
            ((uint32_t)b[3]);
    }
    int32_t i32 = (int32_t)u;

    outlet_float(x->x_out_int, (t_float)i32);

    // Si plus de 4 bytes, sortir le restant sur outlet 1
    if (argc > 4) {
        outlet_list(x->x_out_extra, &s_list, argc - 4, &argv[4]);
    }
}

static void *bytes2int_new(t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    t_bytes2int *x = (t_bytes2int *)pd_new(bytes2int_class);

    x->x_out_int = outlet_new(&x->x_obj, &s_float);
    x->x_out_extra = outlet_new(&x->x_obj, &s_list);
    x->x_little_endian = 1; // défaut: little

    if (argc >= 1 && argv[0].a_type == A_SYMBOL) {
        bytes2int_set_endian(x, argv[0].a_w.w_symbol);
    }

    return (void *)x;
}

void bytes2int_setup(void)
{
    bytes2int_class = class_new(gensym("bytes2int"),
        (t_newmethod)bytes2int_new,
        0,
        sizeof(t_bytes2int),
        CLASS_DEFAULT,
        A_GIMME, 0);

    class_addlist(bytes2int_class, bytes2int_list);
    class_addmethod(bytes2int_class, (t_method)bytes2int_set_endian, gensym("endian"), A_SYMBOL, 0);

    post("bytes2int v%s - converts 4 bytes to int32 (little/big)", VERSION);
}
