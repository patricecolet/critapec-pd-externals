/* bytes2float - Un external pour Pure Data qui convertit 4 bytes little-endian en float32
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
#include <string.h>

#ifndef VERSION
# define VERSION "0.1"
#endif

static t_class *bytes2float_class;

typedef struct _bytes2float
{
    t_object x_obj;
    t_outlet *x_out_float;
    t_outlet *x_out_extra;
} t_bytes2float;

static void bytes2float_list(t_bytes2float *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s; // unused
    
    if (argc == 0) {
        return;
    }
    
    // Vérifier la validité des bytes (doivent être entre 0 et 255)
    int has_invalid_bytes = 0;
    for (int i = 0; i < argc; i++) {
        t_float val = atom_getfloat(&argv[i]);
        if (val < 0.0 || val > 255.0) {
            has_invalid_bytes = 1;
            break;
        }
    }
    
    // Si bytes invalides, sortir tout sur le 2e outlet
    if (has_invalid_bytes) {
        outlet_list(x->x_out_extra, &s_list, argc, argv);
        return;
    }
    
    // Préparer les 4 bytes (compléter avec 0 si moins de 4)
    uint8_t bytes[4] = {0, 0, 0, 0};
    int bytes_to_copy = (argc < 4) ? argc : 4;
    
    for (int i = 0; i < bytes_to_copy; i++) {
        bytes[i] = (uint8_t)atom_getfloat(&argv[i]);
    }
    
    // Convertir les 4 bytes en float
    union {
        float f;
        uint32_t i;
        uint8_t bytes[4];
    } converter;
    
    converter.bytes[0] = bytes[0];
    converter.bytes[1] = bytes[1];
    converter.bytes[2] = bytes[2];
    converter.bytes[3] = bytes[3];
    
    // Sortir le float sur le 1er outlet
    outlet_float(x->x_out_float, converter.f);
    
    // Si plus de 4 bytes, sortir le reste sur le 2e outlet
    if (argc > 4) {
        outlet_list(x->x_out_extra, &s_list, argc - 4, &argv[4]);
    }
}

static void *bytes2float_new(void)
{
    t_bytes2float *x = (t_bytes2float *)pd_new(bytes2float_class);
    x->x_out_float = outlet_new(&x->x_obj, &s_float);
    x->x_out_extra = outlet_new(&x->x_obj, &s_list);
    return (void *)x;
}

void bytes2float_setup(void)
{
    bytes2float_class = class_new(gensym("bytes2float"),
        (t_newmethod)bytes2float_new,
        0,
        sizeof(t_bytes2float),
        CLASS_DEFAULT,
        0);
    
    class_addlist(bytes2float_class, bytes2float_list);
    
    post("bytes2float v%s - converts little-endian bytes to float32", VERSION);
}


