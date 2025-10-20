/* float2byte - Un external pour Pure Data qui convertit un float32 en bytes little-endian
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

static t_class *float2byte_class;

typedef struct _float2byte
{
    t_object x_obj;
    t_outlet *x_out;
} t_float2byte;

static void float2byte_float(t_float2byte *x, t_floatarg f)
{
    t_atom byte_list[4];
    union {
        float f;
        uint32_t i;
        uint8_t bytes[4];
    } converter;
    
    converter.f = (float)f;
    
    // Sortie des bytes en little-endian
    SETFLOAT(&byte_list[0], (t_float)converter.bytes[0]);
    SETFLOAT(&byte_list[1], (t_float)converter.bytes[1]);
    SETFLOAT(&byte_list[2], (t_float)converter.bytes[2]);
    SETFLOAT(&byte_list[3], (t_float)converter.bytes[3]);
    
    outlet_list(x->x_out, &s_list, 4, byte_list);
}

static void *float2byte_new(void)
{
    t_float2byte *x = (t_float2byte *)pd_new(float2byte_class);
    x->x_out = outlet_new(&x->x_obj, &s_list);
    return (void *)x;
}

void float2byte_setup(void)
{
    float2byte_class = class_new(gensym("float2byte"),
        (t_newmethod)float2byte_new,
        0,
        sizeof(t_float2byte),
        CLASS_DEFAULT,
        0);
    
    class_addfloat(float2byte_class, float2byte_float);
    
    post("float2byte v%s - converts float32 to little-endian bytes", VERSION);
}

