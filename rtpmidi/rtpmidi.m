/* rtpmidi.m - macOS backend driving Core MIDI Network Session, with
 * Bonjour discovery of peers over _apple-midi._udp.
 *
 * Interface (on the one status outlet):
 *   enabled <0|1>
 *   policy <anyone|contacts|none>
 *   scanning <0|1>
 *   peer <name> <host> <port>          - advertised on the network
 *   peer-removed <name>                - service went away
 *   connected <host> <port>            - we joined a host
 *   disconnected
 *
 * MIDI bytes still flow through Core MIDI; once enabled and connected,
 * "Network Session 1" appears as a standard MIDI port in Pd's MIDI prefs.
 *
 * Copyright (C) 2025 Patrice Colet
 * GPL-2.0+
 */

#include "m_pd.h"
#import <Foundation/Foundation.h>
#import <CoreMIDI/MIDINetworkSession.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef VERSION
# define VERSION "0.1"
#endif

static t_class *rtpmidi_class;

typedef struct _rtpmidi {
    t_object x_obj;
    t_outlet *x_status_out;
    void    *x_hosts;        /* retained NSMutableArray<MIDINetworkHost*> */
    void    *x_browser;      /* retained NSNetServiceBrowser or nil */
    void    *x_delegate;     /* retained delegate, owns resolved dict */
    t_clock *x_poll_clock;
    int      x_scanning;
} t_rtpmidi;

/* Forward */
static void rtpmidi_emit_peer(t_rtpmidi *x, NSString *name,
                              NSString *host, int port, BOOL added);

/*=== Bonjour delegate ======================================================*/

@interface RtpMidiBrowseDelegate : NSObject
    <NSNetServiceBrowserDelegate, NSNetServiceDelegate>
{
    void *owner;                        /* t_rtpmidi *, unretained */
    NSMutableArray     *resolving;      /* NSNetService in-flight */
    NSMutableDictionary *resolved;      /* name -> @{@"host":str, @"port":num} */
}
- (id)initWithOwner:(void*)o;
- (void)setOwner:(void*)o;
- (NSDictionary *)lookup:(NSString*)name;
- (NSArray *)peerNames;
- (NSUInteger)peerCount;
- (void)reset;
@end

@implementation RtpMidiBrowseDelegate

- (id)initWithOwner:(void*)o {
    self = [super init];
    if (self) {
        owner = o;
        resolving = [[NSMutableArray alloc] init];
        resolved  = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void)setOwner:(void*)o { owner = o; }

- (NSDictionary *)lookup:(NSString*)name {
    return [resolved objectForKey:name];
}

- (NSArray *)peerNames {
    return [resolved allKeys];
}

- (NSUInteger)peerCount {
    return [resolved count];
}

- (void)reset {
    for (NSNetService *s in resolving) {
        [s setDelegate:nil];
        [s stop];
    }
    [resolving removeAllObjects];
    [resolved removeAllObjects];
}

- (void)dealloc {
    [self reset];
    [resolving release];
    [resolved release];
    [super dealloc];
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)b
           didFindService:(NSNetService*)s
               moreComing:(BOOL)more {
    (void)b; (void)more;
    [s setDelegate:self];
    [resolving addObject:s];
    [s resolveWithTimeout:5.0];
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)b
         didRemoveService:(NSNetService*)s
               moreComing:(BOOL)more {
    (void)b; (void)more;
    NSString *name = [[s.name copy] autorelease];
    [resolved removeObjectForKey:name];
    if (owner) rtpmidi_emit_peer((t_rtpmidi*)owner, name, nil, 0, NO);
}

- (void)netServiceDidResolveAddress:(NSNetService*)s {
    [resolving removeObject:s];
    /* Pull a numeric IP from the resolved addresses. MIDINetworkHost
     * validates hostnames strictly (no underscores, no trailing dot), so
     * feeding it the literal Bonjour .local name often fails - passing the
     * IP directly sidesteps that. Prefer IPv4, fall back to IPv6. */
    NSString *ip = nil;
    char buf[INET6_ADDRSTRLEN];
    for (NSData *data in s.addresses) {
        const struct sockaddr *sa = (const struct sockaddr *)data.bytes;
        if (sa->sa_family == AF_INET) {
            const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;
            if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
                ip = [NSString stringWithUTF8String:buf];
                break;
            }
        }
    }
    if (!ip) {
        for (NSData *data in s.addresses) {
            const struct sockaddr *sa = (const struct sockaddr *)data.bytes;
            if (sa->sa_family == AF_INET6) {
                const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)sa;
                if (inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf))) {
                    ip = [NSString stringWithUTF8String:buf];
                    break;
                }
            }
        }
    }
    if (!ip) return;

    [resolved setObject:@{@"host": ip, @"port": @(s.port)}
                 forKey:s.name];
    /* Emit the readable hostname for display; internally we keep the IP. */
    NSString *display = s.hostName ?: ip;
    if (display && [display hasSuffix:@"."]) {
        display = [display substringToIndex:display.length - 1];
    }
    if (owner) rtpmidi_emit_peer((t_rtpmidi*)owner,
                                  s.name, display, (int)s.port, YES);
}

- (void)netService:(NSNetService*)s didNotResolve:(NSDictionary*)err {
    (void)err;
    [resolving removeObject:s];
}

@end

/*=== session helpers =======================================================*/

static inline MIDINetworkSession *rtpmidi_session(void)
{
    return [MIDINetworkSession defaultSession];
}

static void rtpmidi_emit_peer(t_rtpmidi *x, NSString *name,
                              NSString *host, int port, BOOL added)
{
    if (added) {
        t_atom a[3];
        SETSYMBOL(&a[0], gensym([name UTF8String]));
        SETSYMBOL(&a[1], gensym([host UTF8String]));
        SETFLOAT(&a[2], (t_float)port);
        outlet_anything(x->x_status_out, gensym("peer"), 3, a);
    } else {
        t_atom a[1];
        SETSYMBOL(&a[0], gensym([name UTF8String]));
        outlet_anything(x->x_status_out, gensym("peer-removed"), 1, a);
    }
}

/*=== messages ==============================================================*/

static void rtpmidi_enable(t_rtpmidi *x, t_floatarg f)
{
    int on = (f != 0);
    rtpmidi_session().enabled = on ? YES : NO;
    t_atom a;
    SETFLOAT(&a, (t_float)on);
    outlet_anything(x->x_status_out, gensym("enabled"), 1, &a);
}

static void rtpmidi_policy(t_rtpmidi *x, t_symbol *s)
{
    MIDINetworkConnectionPolicy p;
    if (s == gensym("anyone")) {
        p = MIDINetworkConnectionPolicy_Anyone;
    } else if (s == gensym("contacts")) {
        p = MIDINetworkConnectionPolicy_HostsInContactList;
    } else if (s == gensym("none")) {
        p = MIDINetworkConnectionPolicy_NoOne;
    } else {
        pd_error(x, "rtpmidi: policy must be 'anyone', 'contacts' or 'none'");
        return;
    }
    rtpmidi_session().connectionPolicy = p;
    t_atom a;
    SETSYMBOL(&a, s);
    outlet_anything(x->x_status_out, gensym("policy"), 1, &a);
}

static void rtpmidi_poll_tick(t_rtpmidi *x)
{
    if (!x->x_scanning) return;
    /* Pump any Bonjour events that have arrived. Running with distantPast
     * returns immediately after processing pending sources. */
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate distantPast]];
    clock_delay(x->x_poll_clock, 250.0);
}

static void rtpmidi_scan(t_rtpmidi *x, t_floatarg f)
{
    int want = (f != 0);
    if (want && !x->x_scanning) {
        NSNetServiceBrowser *b = [[NSNetServiceBrowser alloc] init];
        [b setDelegate:(RtpMidiBrowseDelegate*)x->x_delegate];
        [b searchForServicesOfType:@"_apple-midi._udp." inDomain:@"local."];
        x->x_browser = b;
        x->x_scanning = 1;
        clock_delay(x->x_poll_clock, 0);
    } else if (!want && x->x_scanning) {
        NSNetServiceBrowser *b = (NSNetServiceBrowser*)x->x_browser;
        [b stop];
        [b setDelegate:nil];
        [b release];
        x->x_browser = NULL;
        x->x_scanning = 0;
        [(RtpMidiBrowseDelegate*)x->x_delegate reset];
    }
    t_atom a;
    SETFLOAT(&a, (t_float)x->x_scanning);
    outlet_anything(x->x_status_out, gensym("scanning"), 1, &a);
}

static void rtpmidi_connect(t_rtpmidi *x, t_symbol *sel, int argc, t_atom *argv)
{
    (void)sel;
    if (argc < 1 || argv[0].a_type != A_SYMBOL) {
        pd_error(x, "rtpmidi: connect <name-or-host> [port]");
        return;
    }
    t_symbol *query_sym = atom_getsymbol(&argv[0]);
    NSString *query = [NSString stringWithUTF8String:query_sym->s_name];

    NSString *addr = nil;
    int port = 5004;
    int port_from_args = (argc >= 2);
    if (port_from_args) {
        port = (int)atom_getfloat(&argv[1]);
        if (port <= 0 || port > 65535) {
            pd_error(x, "rtpmidi: invalid port %d", port);
            return;
        }
    }

    /* Resolve via Bonjour cache first; fall back to treating the argument
     * as a hostname/IP if we haven't seen it advertised. */
    RtpMidiBrowseDelegate *del = (RtpMidiBrowseDelegate*)x->x_delegate;
    NSDictionary *peer = [del lookup:query];
    if (peer) {
        addr = peer[@"host"];
        if (!port_from_args) port = [peer[@"port"] intValue];
        post("rtpmidi: resolved '%s' -> %s:%d",
             [query UTF8String], [addr UTF8String], port);
    } else {
        addr = query;
        NSArray *known = [del peerNames];
        post("rtpmidi: no Bonjour entry for '%s' (%lu known: %s); trying as hostname",
             [query UTF8String],
             (unsigned long)[del peerCount],
             [[known componentsJoinedByString:@", "] UTF8String]);
    }

    /* Bonjour returns fully-qualified names with a trailing dot
     * (e.g. "nidmi_imu.local."). MIDINetworkHost rejects that form. */
    if ([addr hasSuffix:@"."]) {
        addr = [addr substringToIndex:addr.length - 1];
    }

    @autoreleasepool {
        /* Sanity probe: does MIDINetworkHost construction work AT ALL in
         * this process? If a trivially-valid call returns nil, the issue
         * is environmental (entitlement, session init, framework state),
         * not our input. */
        MIDINetworkHost *probe =
            [MIDINetworkHost hostWithName:@"probe"
                                  address:@"127.0.0.1"
                                     port:5004];
        post("rtpmidi: sanity probe hostWithName:address:port: = %s",
             probe ? "OK" : "FAIL");

        /* Try the three documented factories in turn:
         *   1. hostWithName:netServiceName:netServiceDomain: - lets CoreMIDI
         *      resolve via Bonjour internally. Uses the Pd-supplied query as
         *      the service name.
         *   2. hostWithName:address:port: with a few sanitized label variants.
         */
        MIDINetworkHost *host =
            [MIDINetworkHost hostWithName:query
                           netServiceName:query
                         netServiceDomain:@"local."];
        if (host) {
            post("rtpmidi: host built via Bonjour name '%s'",
                 [query UTF8String]);
        } else {
            NSString *sanitized =
                [query stringByReplacingOccurrencesOfString:@"_" withString:@"-"];
            NSArray *nameAttempts = @[query, sanitized, addr, @"rtpmidi-peer"];
            NSString *acceptedName = nil;
            for (NSString *nm in nameAttempts) {
                host = [MIDINetworkHost hostWithName:nm address:addr port:port];
                if (host) { acceptedName = nm; break; }
            }
            if (host && ![acceptedName isEqual:query]) {
                post("rtpmidi: label '%s' was rejected; using '%s' instead",
                     [query UTF8String], [acceptedName UTF8String]);
            }
        }
        if (!host) {
            pd_error(x, "rtpmidi: could not build host for %s:%d",
                     [addr UTF8String], port);
            return;
        }
        MIDINetworkSession *sess = rtpmidi_session();
        [sess addContact:host];
        MIDINetworkConnection *conn =
            [MIDINetworkConnection connectionWithHost:host];
        if (conn) [sess addConnection:conn];
        [(NSMutableArray *)x->x_hosts addObject:host];

        t_atom out[2];
        SETSYMBOL(&out[0], gensym([addr UTF8String]));
        SETFLOAT(&out[1], (t_float)port);
        outlet_anything(x->x_status_out, gensym("connected"), 2, out);
    }
}

static void rtpmidi_disconnect(t_rtpmidi *x)
{
    @autoreleasepool {
        MIDINetworkSession *sess = rtpmidi_session();
        NSMutableArray *arr = (NSMutableArray *)x->x_hosts;
        for (MIDINetworkHost *host in arr) {
            for (MIDINetworkConnection *conn in [[sess.connections copy] autorelease]) {
                if ([conn.host isEqual:host]) [sess removeConnection:conn];
            }
            [sess removeContact:host];
        }
        [arr removeAllObjects];
    }
    outlet_anything(x->x_status_out, gensym("disconnected"), 0, NULL);
}

static void rtpmidi_bang(t_rtpmidi *x)
{
    MIDINetworkSession *sess = rtpmidi_session();
    t_atom a;
    SETFLOAT(&a, sess.isEnabled ? 1.f : 0.f);
    outlet_anything(x->x_status_out, gensym("enabled"), 1, &a);
    t_atom b;
    SETFLOAT(&b, (t_float)sess.connections.count);
    outlet_anything(x->x_status_out, gensym("connections"), 1, &b);
}

/*=== lifecycle =============================================================*/

static void *rtpmidi_new(void)
{
    t_rtpmidi *x = (t_rtpmidi *)pd_new(rtpmidi_class);
    x->x_status_out = outlet_new(&x->x_obj, 0);
    x->x_hosts    = [[NSMutableArray alloc] init];
    x->x_delegate = [[RtpMidiBrowseDelegate alloc] initWithOwner:x];
    x->x_browser  = NULL;
    x->x_scanning = 0;
    x->x_poll_clock = clock_new(x, (t_method)rtpmidi_poll_tick);
    post("rtpmidi v%s - Core MIDI Network Session driver + Bonjour (macOS)",
         VERSION);
    return (void *)x;
}

static void rtpmidi_free(t_rtpmidi *x)
{
    if (x->x_scanning) rtpmidi_scan(x, 0);
    if (x->x_poll_clock) {
        clock_free(x->x_poll_clock);
        x->x_poll_clock = NULL;
    }
    rtpmidi_disconnect(x);
    [(NSMutableArray *)x->x_hosts release];
    x->x_hosts = NULL;
    [(RtpMidiBrowseDelegate*)x->x_delegate setOwner:NULL];
    [(RtpMidiBrowseDelegate*)x->x_delegate release];
    x->x_delegate = NULL;
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
}
