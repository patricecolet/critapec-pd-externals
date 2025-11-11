/* encoder2note - Un external pour Pure Data qui convertit une valeur d'encodeur en note musicale
 * avec demi-tons et centièmes de ton selon la position du levier de vitesse
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

#ifndef VERSION
# define VERSION "0.3"
#endif

static t_class *encoder2note_class;

typedef struct _encoder2note
{
    t_object x_obj;
    t_float x_gear;              // Position du levier de vitesse (demi-tons par tour)
    t_float x_prev_turns;        // Nombre de tours précédent
    t_float x_accumulator;       // Accumulateur de demi-tons
    t_float x_min_note;          // Note minimum (ambitus)
    t_float x_max_note;          // Note maximum (ambitus)
    int x_first_value;           // Flag pour la première valeur
    t_outlet *x_out;             // Sortie unique pour demi-tons + centièmes
} t_encoder2note;

static void encoder2note_float(t_encoder2note *x, t_floatarg f)
{
    // f représente le nombre de tours
    
    // Si c'est la première valeur, initialiser sans calculer de delta
    if (x->x_first_value) {
        x->x_prev_turns = f;
        x->x_first_value = 0;
        outlet_float(x->x_out, x->x_accumulator);
        return;
    }
    
    // Calculer le delta depuis la dernière valeur
    t_float delta_turns = f - x->x_prev_turns;
    
    // Calculer le changement en demi-tons selon la vitesse actuelle
    t_float semitone_delta = delta_turns * x->x_gear;
    
    // Accumuler le changement
    x->x_accumulator += semitone_delta;
    
    // Limiter l'accumulateur à l'ambitus
    if (x->x_accumulator < x->x_min_note) {
        x->x_accumulator = x->x_min_note;
    } else if (x->x_accumulator > x->x_max_note) {
        x->x_accumulator = x->x_max_note;
    }
    
    // Sauvegarder la valeur actuelle pour le prochain delta
    x->x_prev_turns = f;
    
    // Sortir la valeur complète
    outlet_float(x->x_out, x->x_accumulator);
}

static void encoder2note_reset(t_encoder2note *x)
{
    x->x_accumulator = 0.0;
    x->x_first_value = 1;
    outlet_float(x->x_out, 0.0);
}

static void encoder2note_set(t_encoder2note *x, t_floatarg f)
{
    // Permet de définir manuellement l'accumulateur
    x->x_accumulator = f;
    
    // Limiter à l'ambitus
    if (x->x_accumulator < x->x_min_note) {
        x->x_accumulator = x->x_min_note;
    } else if (x->x_accumulator > x->x_max_note) {
        x->x_accumulator = x->x_max_note;
    }
    
    outlet_float(x->x_out, x->x_accumulator);
}

static void encoder2note_min(t_encoder2note *x, t_floatarg f)
{
    // Définir la note minimum
    x->x_min_note = f;
    
    // Si l'accumulateur actuel est en dessous, le ramener au min
    if (x->x_accumulator < x->x_min_note) {
        x->x_accumulator = x->x_min_note;
        outlet_float(x->x_out, x->x_accumulator);
    }
}

static void encoder2note_max(t_encoder2note *x, t_floatarg f)
{
    // Définir la note maximum
    x->x_max_note = f;
    
    // Si l'accumulateur actuel est au dessus, le ramener au max
    if (x->x_accumulator > x->x_max_note) {
        x->x_accumulator = x->x_max_note;
        outlet_float(x->x_out, x->x_accumulator);
    }
}

static void encoder2note_gear(t_encoder2note *x, t_floatarg f)
{
    // Définir la vitesse avec validation
    if (f < 1.0) {
        x->x_gear = 1.0;
        pd_error(x, "encoder2note: vitesse < 1, limitée à 1");
    } else if (f > 48.0) {
        x->x_gear = 48.0;
        pd_error(x, "encoder2note: vitesse > 48, limitée à 48");
    } else {
        x->x_gear = f;
    }
}

static void *encoder2note_new(t_floatarg f, t_floatarg min_note, t_floatarg max_note)
{
    t_encoder2note *x = (t_encoder2note *)pd_new(encoder2note_class);
    
    // Définir la vitesse initiale (par défaut 12, limité entre 1 et 48)
    if (f == 0.0) {
        x->x_gear = 12.0;  // Défaut
    } else if (f < 1.0) {
        x->x_gear = 1.0;   // Minimum
        pd_error(x, "encoder2note: vitesse < 1, limitée à 1");
    } else if (f > 48.0) {
        x->x_gear = 48.0;  // Maximum
        pd_error(x, "encoder2note: vitesse > 48, limitée à 48");
    } else {
        x->x_gear = f;
    }
    
    // Définir l'ambitus (par défaut: -48 à +48 demi-tons, soit 8 octaves)
    x->x_min_note = (min_note != 0.0) ? min_note : -48.0;
    x->x_max_note = (max_note != 0.0) ? max_note : 48.0;
    
    // Vérifier que min < max
    if (x->x_min_note >= x->x_max_note) {
        pd_error(x, "encoder2note: note min doit être < note max, utilisation des valeurs par défaut");
        x->x_min_note = -48.0;
        x->x_max_note = 48.0;
    }
    
    // Initialiser les variables
    x->x_prev_turns = 0.0;
    x->x_accumulator = 0.0;
    x->x_first_value = 1;
    
    // Créer un inlet supplémentaire pour la vitesse (limité entre 1 et 48)
    floatinlet_new(&x->x_obj, &x->x_gear);
    
    // Créer l'outlet unique
    x->x_out = outlet_new(&x->x_obj, &s_float);
    
    return (void *)x;
}

void encoder2note_setup(void)
{
    encoder2note_class = class_new(gensym("encoder2note"),
        (t_newmethod)encoder2note_new,
        0,
        sizeof(t_encoder2note),
        CLASS_DEFAULT,
        A_DEFFLOAT,
        A_DEFFLOAT,
        A_DEFFLOAT,
        0);
    
    class_addfloat(encoder2note_class, encoder2note_float);
    class_addmethod(encoder2note_class, (t_method)encoder2note_reset, 
                    gensym("reset"), 0);
    class_addmethod(encoder2note_class, (t_method)encoder2note_set, 
                    gensym("set"), A_FLOAT, 0);
    class_addmethod(encoder2note_class, (t_method)encoder2note_min, 
                    gensym("min"), A_FLOAT, 0);
    class_addmethod(encoder2note_class, (t_method)encoder2note_max, 
                    gensym("max"), A_FLOAT, 0);
    class_addmethod(encoder2note_class, (t_method)encoder2note_gear, 
                    gensym("gear"), A_FLOAT, 0);
    
    post("encoder2note v%s - convertit tours d'encodeur en demi-tons (avec ambitus)", VERSION);
    post("  Usage: [encoder2note vitesse min_note max_note]");
    post("  Vitesse: 1 à 48 demi-tons/tour (défaut: 12)");
    post("  Ambitus: min et max en demi-tons (défaut: -48 à +48)");
    post("  Messages: reset, set N, min N, max N, gear N (1-48)");
}

