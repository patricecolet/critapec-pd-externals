/* cb4tech-parse - Un external pour Pure Data qui parse les données des capteurs CB4Tech
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

// Structure pour stocker les valeurs des capteurs
typedef struct __attribute__((packed)) {
    uint32_t absolute_encoder;
    int16_t encoder;
    int16_t velocity;
    uint16_t absolute_force1; // Force continue actuelle drum 1 (unfiltered)
    uint16_t absolute_force2; // Force continue actuelle drum 2 (unfiltered)
    uint16_t continuous_force1; // force continue actuelle drum 1 (filtered)
    uint16_t continuous_force2; // Force continue actuelle drum 2 (filtered)
    uint8_t drum1; // Force de frappe détectée drum 1 (0-127)
    uint8_t drum2; // Force de frappe détectée drum 2 (0-127)
    int8_t joystick_x;
    int8_t joystick_y;
    int8_t joystick_z;
    uint8_t joystick_button;
    uint8_t selecter;
    int16_t ext_adc0;
    int16_t ext_adc1;
    uint8_t ext_button1; // Bouton 1 d'extension (0=relâché, 1=appuyé)
    uint8_t ext_button2; // Bouton 2 d'extension (0=relâché, 1=appuyé)
} sensors_values_t;

#define STRUCT_SIZE sizeof(sensors_values_t)

static t_class *cb4tech_parse_class;

typedef struct _cb4tech_parse
{
    t_object x_obj;
    t_outlet *x_out;
    
    unsigned char buffer[STRUCT_SIZE];
    int buffer_index;
} t_cb4tech_parse;

static void cb4tech_parse_list(t_cb4tech_parse *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s; // unused
    
    // Vérifier que nous avons exactement la taille attendue
    if (argc != STRUCT_SIZE) {
        pd_error(x, "cb4tech-parse: expected %d bytes, got %d", (int)STRUCT_SIZE, argc);
        return;
    }
    
    // Copier les bytes dans le buffer
    for (int i = 0; i < argc && i < (int)STRUCT_SIZE; i++) {
        x->buffer[i] = (unsigned char)atom_getfloat(&argv[i]);
    }
    
    // Parser la structure
    sensors_values_t *sensors = (sensors_values_t *)x->buffer;
    
    t_atom output[6];
    
    // Sortir les valeurs groupées par type de contrôleur
    
    // button (button1 button2)
    SETFLOAT(&output[0], (t_float)sensors->ext_button1);
    SETFLOAT(&output[1], (t_float)sensors->ext_button2);
    outlet_anything(x->x_out, gensym("button"), 2, output);
    
    // drum (drum1 drum2 continuous_force1 continuous_force2 absolute_force1 absolute_force2)
    SETFLOAT(&output[0], (t_float)sensors->drum1);
    SETFLOAT(&output[1], (t_float)sensors->drum2);
    SETFLOAT(&output[2], (t_float)sensors->continuous_force1);
    SETFLOAT(&output[3], (t_float)sensors->continuous_force2);
    SETFLOAT(&output[4], (t_float)sensors->absolute_force1);
    SETFLOAT(&output[5], (t_float)sensors->absolute_force2);
    outlet_anything(x->x_out, gensym("drum"), 6, output);
    
    // joystick (x y z bouton)
    SETFLOAT(&output[0], (t_float)sensors->joystick_x);
    SETFLOAT(&output[1], (t_float)sensors->joystick_y);
    SETFLOAT(&output[2], (t_float)sensors->joystick_z);
    SETFLOAT(&output[3], (t_float)sensors->joystick_button);
    outlet_anything(x->x_out, gensym("joystick"), 4, output);
    
    // selector
    SETFLOAT(&output[0], (t_float)sensors->selecter);
    outlet_anything(x->x_out, gensym("selector"), 1, output);
    
    // adc (adc0 adc1)
    SETFLOAT(&output[0], (t_float)sensors->ext_adc0);
    SETFLOAT(&output[1], (t_float)sensors->ext_adc1);
    outlet_anything(x->x_out, gensym("adc"), 2, output);
    
    // encoder (encoder velocity absolute_encoder)
    SETFLOAT(&output[0], (t_float)sensors->encoder);
    SETFLOAT(&output[1], (t_float)sensors->velocity);
    SETFLOAT(&output[2], (t_float)sensors->absolute_encoder);
    outlet_anything(x->x_out, gensym("encoder"), 3, output);
}

static void cb4tech_parse_bang(t_cb4tech_parse *x)
{
    (void)x; // unused
    post("cb4tech-parse: expected data size = %d bytes", (int)STRUCT_SIZE);
}

static void *cb4tech_parse_new(void)
{
    t_cb4tech_parse *x = (t_cb4tech_parse *)pd_new(cb4tech_parse_class);
    
    // Créer un seul outlet
    x->x_out = outlet_new(&x->x_obj, &s_list);
    
    x->buffer_index = 0;
    
    return (void *)x;
}

void cb4tech_parse_setup(void)
{
    cb4tech_parse_class = class_new(gensym("cb4tech-parse"),
        (t_newmethod)cb4tech_parse_new,
        0,
        sizeof(t_cb4tech_parse),
        CLASS_DEFAULT,
        0);
    
    class_addlist(cb4tech_parse_class, cb4tech_parse_list);
    class_addbang(cb4tech_parse_class, cb4tech_parse_bang);
    
    post("cb4tech-parse v%s - parses CB4Tech sensor data (%d bytes)", VERSION, (int)STRUCT_SIZE);
}

