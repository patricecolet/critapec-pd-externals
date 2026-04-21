/* rtpmidi - Pure Data external to auto-connect an RTP-MIDI session.
 *
 * Stub skeleton: compiles on every platform and prints setup guidance on
 * load. The macOS backend (Core MIDI Network Session via MIDINetworkSession)
 * will be implemented next, in Objective-C, behind __APPLE__.
 *
 * Copyright (C) 2025 Patrice Colet
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
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

static void rtpmidi_platform_hint(t_rtpmidi *x)
{
    (void)x;
#if defined(__APPLE__)
    post("rtpmidi: macOS detected — Core MIDI Network Session driver not yet implemented");
#elif defined(__linux__)
    post("rtpmidi: Linux detected — install and run rtpmidid (https://github.com/davidmoreno/rtpmidid)");
    post("rtpmidi: Avahi handles peer discovery; see README for setup");
#elif defined(_WIN32)
    post("rtpmidi: Windows detected — install Tobias Erichsen's rtpMIDI driver (https://www.tobias-erichsen.de/software/rtpmidi.html)");
#else
    post("rtpmidi: unknown platform — see README for manual setup");
#endif
}

static void *rtpmidi_new(void)
{
    t_rtpmidi *x = (t_rtpmidi *)pd_new(rtpmidi_class);
    x->x_status_out = outlet_new(&x->x_obj, &s_symbol);
    rtpmidi_platform_hint(x);
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

    post("rtpmidi v%s - RTP-MIDI session connector (skeleton)", VERSION);
}
