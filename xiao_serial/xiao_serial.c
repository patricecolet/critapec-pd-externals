/* xiao_serial - Un external pour Pure Data qui communique via port série USB
 * avec un XIAO ESP32S3
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
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <glob.h>

#ifndef VERSION
# define VERSION "0.1"
#endif

#if (defined(__linux__) || defined(__APPLE__)) && !defined(XIAO_SERIAL_FORCE_STUB)
#define XIAO_SERIAL_HAVE_TERMIOS 1
#endif

#ifdef XIAO_SERIAL_HAVE_TERMIOS
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#endif

#define RX_BUFFER_SIZE 1024
#define DEFAULT_BAUDRATE 115200

static t_class *xiao_serial_class;

typedef struct _xiao_serial {
    t_object x_obj;
    t_outlet *x_out;
    t_clock *x_clock;
    
    t_float x_interval;  // Intervalle de polling en ms
    
#ifdef XIAO_SERIAL_HAVE_TERMIOS
    int x_fd;                    // File descriptor du port série
    char x_port[256];            // Chemin du port
    int x_baudrate;              // Vitesse de transmission
    
    // Thread de lecture
    pthread_t x_thread;
    atomic_int x_thread_running;
    
    // Buffer circulaire thread-safe pour les données reçues
    unsigned char x_rx_buffer[RX_BUFFER_SIZE];
    atomic_int x_rx_head;
    atomic_int x_rx_tail;
    pthread_mutex_t x_rx_mutex;
    pthread_mutex_t x_tx_mutex;
#else
    int x_stub_warned;
#endif
} t_xiao_serial;

static void xiao_serial_tick(t_xiao_serial *x);

#ifdef XIAO_SERIAL_HAVE_TERMIOS

static int xiao_serial_get_baudrate(int baud)
{
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B500000
        case 500000: return B500000;
#endif
#ifdef B576000
        case 576000: return B576000;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
#ifdef B1000000
        case 1000000: return B1000000;
#endif
        default: return B115200;
    }
}

static int xiao_serial_configure_port(t_xiao_serial *x, int fd)
{
    struct termios tty;
    
    if (tcgetattr(fd, &tty) != 0) {
        pd_error(x, "xiao_serial: erreur tcgetattr: %s", strerror(errno));
        return -1;
    }
    
    // Configurer la vitesse
    cfsetospeed(&tty, xiao_serial_get_baudrate(x->x_baudrate));
    cfsetispeed(&tty, xiao_serial_get_baudrate(x->x_baudrate));
    
    // 8 bits, pas de parité, 1 bit de stop (8N1)
    tty.c_cflag &= ~PARENB;  // Pas de parité
    tty.c_cflag &= ~CSTOPB;  // 1 bit de stop
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 8 bits
    tty.c_cflag &= ~CRTSCTS; // Pas de RTS/CTS
    tty.c_cflag |= CREAD | CLOCAL; // Activer réception et mode local
    
    // Mode raw (pas de traitement de caractères)
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ISIG;
    
    // Désactiver le contrôle de flux logiciel
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
    
    // Pas de traitement de sortie
    tty.c_oflag &= ~OPOST;
    
    // Timeout : VMIN=0, VTIME=1 (0.1 seconde)
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        pd_error(x, "xiao_serial: erreur tcsetattr: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

static void *xiao_serial_read_thread(void *userdata)
{
    t_xiao_serial *x = (t_xiao_serial *)userdata;
    struct pollfd pfd;
    char buffer[256];
    
    pfd.fd = x->x_fd;
    pfd.events = POLLIN;
    
    while (atomic_load(&x->x_thread_running)) {
        int ret = poll(&pfd, 1, 100); // Timeout 100ms
        
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(x->x_fd, buffer, sizeof(buffer));
            
            if (n > 0) {
                // Ajouter les données au buffer circulaire
                pthread_mutex_lock(&x->x_rx_mutex);
                
                int head = atomic_load(&x->x_rx_head);
                int tail = atomic_load(&x->x_rx_tail);
                
                // Calcul de l'espace disponible dans le buffer circulaire
                int space;
                if (head > tail) {
                    space = head - tail - 1;
                } else if (tail > head) {
                    space = RX_BUFFER_SIZE - tail + head - 1;
                } else {
                    // head == tail : buffer vide, tout l'espace est disponible
                    space = RX_BUFFER_SIZE - 1;
                }
                if (space < 0) space = 0;
                
                if (space >= n) {
                    for (int i = 0; i < n; i++) {
                        x->x_rx_buffer[tail] = buffer[i];
                        tail = (tail + 1) % RX_BUFFER_SIZE;
                    }
                    atomic_store(&x->x_rx_tail, tail);
                }
                // Si le buffer est plein, on ignore les nouvelles données
                
                pthread_mutex_unlock(&x->x_rx_mutex);
            } else if (n == 0) {
                // EOF
                break;
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                // Erreur de lecture
                break;
            }
        }
    }
    
    return NULL;
}

static int xiao_serial_start_thread(t_xiao_serial *x)
{
    atomic_store(&x->x_thread_running, 1);
    atomic_store(&x->x_rx_head, 0);
    atomic_store(&x->x_rx_tail, 0);
    
    if (pthread_create(&x->x_thread, NULL, xiao_serial_read_thread, x)) {
        pd_error(x, "xiao_serial: impossible de démarrer le thread de lecture");
        atomic_store(&x->x_thread_running, 0);
        return -1;
    }
    return 0;
}

static void xiao_serial_stop_thread(t_xiao_serial *x)
{
    if (!atomic_load(&x->x_thread_running))
        return;
    
    atomic_store(&x->x_thread_running, 0);
    pthread_join(x->x_thread, NULL);
}

static void xiao_serial_close_port(t_xiao_serial *x)
{
    if (x->x_fd < 0)
        return;
    
    xiao_serial_stop_thread(x);
    close(x->x_fd);
    x->x_fd = -1;
    post("xiao_serial: port fermé");
}

#endif /* XIAO_SERIAL_HAVE_TERMIOS */

static void xiao_serial_tick(t_xiao_serial *x)
{
#ifdef XIAO_SERIAL_HAVE_TERMIOS
    if (x->x_fd >= 0 && atomic_load(&x->x_thread_running)) {
        // Lire les données du buffer circulaire et les envoyer
        pthread_mutex_lock(&x->x_rx_mutex);
        
        int head = atomic_load(&x->x_rx_head);
        int tail = atomic_load(&x->x_rx_tail);
        
        // Calcul du nombre de bytes disponibles
        int count;
        if (tail > head) {
            count = tail - head;
        } else if (head > tail) {
            count = RX_BUFFER_SIZE - head + tail;
        } else {
            count = 0;  // Buffer vide
        }
        
        if (count > 0) {
            // Chercher les lignes séparées par \n et les envoyer une par une
            while (count > 0 && head != tail) {
                int line_end = -1;
                int line_start = head;
                int max_check = (tail > head) ? (tail - head) : (RX_BUFFER_SIZE - head);
                
                // Chercher le prochain \n dans le buffer
                for (int i = 0; i < max_check && i < count; i++) {
                    int pos = (head + i) % RX_BUFFER_SIZE;
                    if (x->x_rx_buffer[pos] == '\n') {
                        line_end = pos;
                        break;
                    }
                }
                
                // Si pas de \n trouvé dans la première partie, vérifier le reste du buffer (wraparound)
                if (line_end == -1 && tail < head) {
                    for (int i = 0; i < tail; i++) {
                        if (x->x_rx_buffer[i] == '\n') {
                            line_end = i;
                            break;
                        }
                    }
                }
                
                int line_length;
                if (line_end != -1) {
                    // Ligne trouvée jusqu'au \n
                    if (line_end >= line_start) {
                        line_length = line_end - line_start;
                    } else {
                        // Wraparound
                        line_length = (RX_BUFFER_SIZE - line_start) + line_end;
                    }
                    // Avancer head après le \n (inclure le \n dans le déplacement)
                    head = (line_end + 1) % RX_BUFFER_SIZE;
                } else {
                    // Pas de \n trouvé dans le buffer, attendre plus de données
                    pthread_mutex_unlock(&x->x_rx_mutex);
                    break;
                }
                
                if (line_length > 0) {
                    // Créer la liste pour cette ligne (sans le \n)
                    t_atom *atoms = (t_atom *)getbytes(line_length * sizeof(t_atom));
                    if (atoms) {
                        int pos = line_start;
                        for (int i = 0; i < line_length; i++) {
                            SETFLOAT(&atoms[i], (t_float)(unsigned char)x->x_rx_buffer[pos]);
                            pos = (pos + 1) % RX_BUFFER_SIZE;
                        }
                        atomic_store(&x->x_rx_head, head);
                        pthread_mutex_unlock(&x->x_rx_mutex);
                        
                        outlet_list(x->x_out, &s_list, line_length, atoms);
                        freebytes(atoms, line_length * sizeof(t_atom));
                        
                        pthread_mutex_lock(&x->x_rx_mutex);
                        // Recalculer count pour la prochaine itération
                        int new_head = atomic_load(&x->x_rx_head);
                        int new_tail = atomic_load(&x->x_rx_tail);
                        if (new_tail > new_head) {
                            count = new_tail - new_head;
                        } else if (new_head > new_tail) {
                            count = RX_BUFFER_SIZE - new_head + new_tail;
                        } else {
                            count = 0;
                        }
                    } else {
                        pthread_mutex_unlock(&x->x_rx_mutex);
                        break;
                    }
                } else {
                    // Ligne vide (juste \n), l'ignorer et continuer
                    count = (tail > head) ? (tail - head) : (RX_BUFFER_SIZE - head + tail);
                }
            }
            
            if (head == tail || count == 0) {
                pthread_mutex_unlock(&x->x_rx_mutex);
            }
        } else {
            pthread_mutex_unlock(&x->x_rx_mutex);
        }
    }
#endif
    
    // Continuer le clock
    if (x->x_interval > 0 && x->x_fd >= 0) {
        clock_delay(x->x_clock, x->x_interval);
    }
}

static void xiao_serial_open(t_xiao_serial *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;  // Supprimer l'avertissement unused parameter
#ifdef XIAO_SERIAL_HAVE_TERMIOS
    if (argc < 1) {
        pd_error(x, "xiao_serial: besoin du chemin du port (ex: /dev/ttyUSB0)");
        return;
    }
    
    // Fermer le port précédent si ouvert
    xiao_serial_close_port(x);
    
    // Lire le chemin du port
    t_symbol *port_sym = atom_getsymbol(argv);
    if (!port_sym) {
        pd_error(x, "xiao_serial: le chemin du port doit être un symbole");
        return;
    }
    
    strncpy(x->x_port, port_sym->s_name, 255);
    x->x_port[255] = '\0';
    
    // Lire le baudrate (optionnel, défaut 115200)
    x->x_baudrate = DEFAULT_BAUDRATE;
    if (argc >= 2) {
        x->x_baudrate = (int)atom_getfloat(argv + 1);
    }
    
    // Ouvrir le port série
    x->x_fd = open(x->x_port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (x->x_fd < 0) {
        pd_error(x, "xiao_serial: impossible d'ouvrir %s: %s", x->x_port, strerror(errno));
        return;
    }
    
    // Configurer le port
    if (xiao_serial_configure_port(x, x->x_fd) < 0) {
        close(x->x_fd);
        x->x_fd = -1;
        return;
    }
    
    // Démarrer le thread de lecture
    if (xiao_serial_start_thread(x) < 0) {
        close(x->x_fd);
        x->x_fd = -1;
        return;
    }
    
    // Démarrer le clock
    clock_delay(x->x_clock, x->x_interval);
    
    post("xiao_serial: port %s ouvert à %d bauds", x->x_port, x->x_baudrate);
#else
    (void)s;
    (void)argc;
    (void)argv;
    if (!x->x_stub_warned) {
        pd_error(x, "xiao_serial: compilé sans support termios (plateforme non Linux)");
        x->x_stub_warned = 1;
    }
#endif
}

static void xiao_serial_close(t_xiao_serial *x)
{
#ifdef XIAO_SERIAL_HAVE_TERMIOS
    xiao_serial_close_port(x);
#else
    (void)x;
#endif
}

static void xiao_serial_list(t_xiao_serial *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    
#ifdef XIAO_SERIAL_HAVE_TERMIOS
    if (x->x_fd < 0) {
        pd_error(x, "xiao_serial: port non ouvert");
        return;
    }
    
    if (argc == 0)
        return;
    
    // Convertir les floats en bytes et envoyer
    unsigned char *buffer = (unsigned char *)getbytes(argc);
    if (!buffer) {
        pd_error(x, "xiao_serial: erreur d'allocation mémoire");
        return;
    }
    
    for (int i = 0; i < argc; i++) {
        t_float f = atom_getfloat(argv + i);
        if (f < 0) f = 0;
        if (f > 255) f = 255;
        buffer[i] = (unsigned char)f;
    }
    
    pthread_mutex_lock(&x->x_tx_mutex);
    ssize_t written = write(x->x_fd, buffer, argc);
    pthread_mutex_unlock(&x->x_tx_mutex);
    
    freebytes(buffer, argc);
    
    if (written < 0) {
        pd_error(x, "xiao_serial: erreur d'écriture: %s", strerror(errno));
    }
#else
    (void)argc;
    (void)argv;
    if (!x->x_stub_warned) {
        pd_error(x, "xiao_serial: compilé sans support termios");
        x->x_stub_warned = 1;
    }
#endif
}

static void xiao_serial_set_interval(t_xiao_serial *x, t_floatarg f)
{
    if (f <= 0) {
        pd_error(x, "xiao_serial: intervalle doit être > 0 ms");
        return;
    }
    x->x_interval = f;
}

static void xiao_serial_bang(t_xiao_serial *x)
{
    // Envoyer un bang pour forcer un tick
    xiao_serial_tick(x);
}

static void xiao_serial_scan(t_xiao_serial *x)
{
    // Lister les ports série disponibles
    glob_t glob_result;
    int i;
    int count = 0;
    
    // Patterns pour les ports série sur macOS et Linux
    const char *patterns[] = {
        "/dev/cu.*",      // macOS callout ports
        "/dev/tty.usb*",  // macOS USB ports
        "/dev/ttyUSB*",   // Linux USB serial
        "/dev/ttyACM*",   // Linux ACM (CDC) ports
        NULL
    };
    
    // Compter d'abord le nombre total de ports
    for (i = 0; patterns[i] != NULL; i++) {
        if (glob(patterns[i], GLOB_NOSORT, NULL, &glob_result) == 0) {
            count += glob_result.gl_pathc;
            globfree(&glob_result);
        }
    }
    
    if (count == 0) {
        outlet_float(x->x_out, 0);  // Aucun port trouvé
        return;
    }
    
    // Allouer un tableau d'atoms pour les ports
    t_atom *atoms = (t_atom *)getbytes(count * sizeof(t_atom));
    if (!atoms) {
        outlet_float(x->x_out, 0);
        return;
    }
    
    int idx = 0;
    
    // Collecter tous les ports
    for (i = 0; patterns[i] != NULL; i++) {
        if (glob(patterns[i], GLOB_NOSORT, NULL, &glob_result) == 0) {
            for (size_t j = 0; j < glob_result.gl_pathc && idx < count; j++) {
                SETSYMBOL(&atoms[idx], gensym(glob_result.gl_pathv[j]));
                idx++;
            }
            globfree(&glob_result);
        }
    }
    
    // Envoyer la liste des ports
    outlet_list(x->x_out, &s_list, idx, atoms);
    freebytes(atoms, count * sizeof(t_atom));
}

static void *xiao_serial_new(t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    (void)argc;
    (void)argv;
    
    t_xiao_serial *x = (t_xiao_serial *)pd_new(xiao_serial_class);
    
    x->x_out = outlet_new(&x->x_obj, &s_list);
    x->x_interval = 10.0; // 10ms par défaut
    
#ifdef XIAO_SERIAL_HAVE_TERMIOS
    x->x_fd = -1;
    x->x_port[0] = '\0';
    x->x_baudrate = DEFAULT_BAUDRATE;
    atomic_store(&x->x_thread_running, 0);
    atomic_store(&x->x_rx_head, 0);
    atomic_store(&x->x_rx_tail, 0);
    pthread_mutex_init(&x->x_rx_mutex, NULL);
    pthread_mutex_init(&x->x_tx_mutex, NULL);
#else
    x->x_stub_warned = 0;
#endif
    
    x->x_clock = clock_new(x, (t_method)xiao_serial_tick);
    
    return (void *)x;
}

static void xiao_serial_free(t_xiao_serial *x)
{
    if (x->x_clock) {
        clock_free(x->x_clock);
        x->x_clock = NULL;
    }
    
#ifdef XIAO_SERIAL_HAVE_TERMIOS
    xiao_serial_close_port(x);
    pthread_mutex_destroy(&x->x_rx_mutex);
    pthread_mutex_destroy(&x->x_tx_mutex);
#endif
}

void xiao_serial_setup(void)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
#endif
    xiao_serial_class = class_new(gensym("xiao_serial"),
        (t_newmethod)xiao_serial_new,
        (t_method)xiao_serial_free,
        sizeof(t_xiao_serial),
        CLASS_DEFAULT,
        0);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    
    class_addmethod(xiao_serial_class,
        (t_method)xiao_serial_open, gensym("open"), A_GIMME, 0);
    class_addmethod(xiao_serial_class,
        (t_method)xiao_serial_close, gensym("close"), 0);
    class_addmethod(xiao_serial_class,
        (t_method)xiao_serial_set_interval, gensym("interval"), A_FLOAT, 0);
    class_addmethod(xiao_serial_class,
        (t_method)xiao_serial_list, gensym("list"), A_GIMME, 0);
    class_addmethod(xiao_serial_class,
        (t_method)xiao_serial_scan, gensym("scan"), 0);
    class_addbang(xiao_serial_class, xiao_serial_bang);
    
    post("xiao_serial v%s - Communication série USB avec XIAO ESP32S3", VERSION);
#ifdef XIAO_SERIAL_HAVE_TERMIOS
    post("  Support termios actif (Linux)");
#else
    post("  Mode stub (termios non disponible)");
#endif
}
