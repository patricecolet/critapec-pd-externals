/* uint32tobytes - Un external pour Pure Data qui convertit un uint32_t en bytes little-endian
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

static t_class *uint32tobytes_class;

typedef struct _uint32tobytes
{
    t_object x_obj;
    t_outlet *x_out;
} t_uint32tobytes;

static void uint32tobytes_float(t_uint32tobytes *x, t_floatarg f)
{
    t_atom byte_list[4];
    union {
        uint32_t i;
        uint8_t bytes[4];
    } converter;
    
    // Convertir le float en uint32_t (troncature pour valeurs négatives)
    if (f < 0) {
        converter.i = 0;
    } else if (f > 4294967295.0) {
        converter.i = 4294967295U;
    } else {
        converter.i = (uint32_t)f;
    }
    
    // Sortie des bytes en little-endian
    SETFLOAT(&byte_list[0], (t_float)converter.bytes[0]);
    SETFLOAT(&byte_list[1], (t_float)converter.bytes[1]);
    SETFLOAT(&byte_list[2], (t_float)converter.bytes[2]);
    SETFLOAT(&byte_list[3], (t_float)converter.bytes[3]);
    
    outlet_list(x->x_out, &s_list, 4, byte_list);
}

static void *uint32tobytes_new(void)
{
    t_uint32tobytes *x = (t_uint32tobytes *)pd_new(uint32tobytes_class);
    x->x_out = outlet_new(&x->x_obj, &s_list);
    return (void *)x;
}

void uint32tobytes_setup(void)
{
    uint32tobytes_class = class_new(gensym("uint32tobytes"),
        (t_newmethod)uint32tobytes_new,
        0,
        sizeof(t_uint32tobytes),
        CLASS_DEFAULT,
        0);
    
    class_addfloat(uint32tobytes_class, uint32tobytes_float);
    
    post("uint32tobytes v%s - converts uint32 to little-endian bytes", VERSION);
}







