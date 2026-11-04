/* Screen recorder service (`rainey --rec-daemon`).
 *
 * Runs in the background (started from the Screen Recorder app). SELECT + L1 + R1 starts and stops
 * a recording of whatever is on screen: frames are read from the framebuffer (/dev/fb0, the buffer
 * currently displayed), JPEG-encoded on worker threads and written to a QuickTime .mov
 * (Motion JPEG, variable frame rate from real capture times), which plays in QuickTime, VLC and
 * on phones. The LEDs pulse red while recording. No audio.
 */
#include "rainey.h"
#include "recorder.h"
#include "leds.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#include "stb/stb_image_write.h"

const int REC_FPS[REC_FPS_N] = {15, 20, 30};

/* ---------------- config + state files ---------------- */

void rec_config_load(RecConfig *c) {
    c->enabled = false;
    c->half = false;
    c->fps = 1; /* 20 fps */
    FILE *f = fopen(REC_CFG, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        int v;
        if (sscanf(line, "enabled=%d", &v) == 1) c->enabled = v;
        else if (sscanf(line, "half=%d", &v) == 1) c->half = v;
        else if (sscanf(line, "fps=%d", &v) == 1 && v >= 0 && v < REC_FPS_N) c->fps = v;
    }
    fclose(f);
}

void rec_config_save(const RecConfig *c) {
    mkdirs(DATA_DIR);
    FILE *f = fopen(REC_CFG, "w");
    if (!f) return;
    fprintf(f, "enabled=%d\nhalf=%d\nfps=%d\n", c->enabled, c->half, c->fps);
    fclose(f);
    sync();
}

static void write_state(const char *s) {
    FILE *f = fopen(REC_STATE ".tmp", "w");
    if (!f) return;
    fputs(s, f);
    fclose(f);
    rename(REC_STATE ".tmp", REC_STATE);
}

/* ---------------- QuickTime writer ---------------- */

typedef struct {
    unsigned char *p;
    size_t n, cap;
} Buf;

static void b_need(Buf *b, size_t k) {
    if (b->n + k <= b->cap) return;
    b->cap = (b->n + k) * 2 + 1024;
    b->p = realloc(b->p, b->cap);
}
static void b_raw(Buf *b, const void *d, size_t k) { b_need(b, k); memcpy(b->p + b->n, d, k); b->n += k; }
static void b_u8(Buf *b, unsigned v) { unsigned char c = v; b_raw(b, &c, 1); }
static void b_u16(Buf *b, unsigned v) { b_u8(b, v >> 8); b_u8(b, v); }
static void b_u32(Buf *b, uint32_t v) { b_u16(b, v >> 16); b_u16(b, v & 0xFFFF); }
static void b_u64(Buf *b, uint64_t v) { b_u32(b, v >> 32); b_u32(b, (uint32_t)v); }
static void b_4cc(Buf *b, const char *s) { b_raw(b, s, 4); }
static void b_zero(Buf *b, int k) { while (k--) b_u8(b, 0); }
static size_t b_open(Buf *b, const char *type) { size_t at = b->n; b_u32(b, 0); b_4cc(b, type); return at; }
static void b_close(Buf *b, size_t at) {
    uint32_t sz = (uint32_t)(b->n - at);
    unsigned char *p = b->p + at;
    p[0] = sz >> 24; p[1] = sz >> 16; p[2] = sz >> 8; p[3] = sz;
}
static void b_matrix(Buf *b) {
    static const uint32_t m[9] = {0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000};
    for (int i = 0; i < 9; i++) b_u32(b, m[i]);
}

typedef struct {
    FILE *f;
    char path[256];
    int w, h;
    uint64_t pos;                 /* write offset */
    uint32_t *sizes, *deltas;     /* per sample */
    uint64_t *offsets;
    int count, cap;
    uint32_t last_ms;
} MovWriter;

static bool mov_open(MovWriter *m, const char *path, int w, int h) {
    memset(m, 0, sizeof *m);
    snprintf(m->path, sizeof m->path, "%s", path);
    m->f = fopen(path, "wb");
    if (!m->f) return false;
    setvbuf(m->f, NULL, _IOFBF, 1 << 20);
    m->w = w;
    m->h = h;
    Buf b = {0};
    b_u32(&b, 20); b_4cc(&b, "ftyp"); b_4cc(&b, "qt  "); b_u32(&b, 0x20050300); b_4cc(&b, "qt  ");
    b_u32(&b, 1); b_4cc(&b, "mdat"); b_u64(&b, 0); /* 64-bit size, patched on close */
    fwrite(b.p, 1, b.n, m->f);
    m->pos = b.n;
    free(b.p);
    return true;
}

static bool mov_add(MovWriter *m, const void *jpg, uint32_t size, uint32_t t_ms) {
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 1024;
        m->sizes = realloc(m->sizes, m->cap * sizeof *m->sizes);
        m->deltas = realloc(m->deltas, m->cap * sizeof *m->deltas);
        m->offsets = realloc(m->offsets, m->cap * sizeof *m->offsets);
    }
    if (fwrite(jpg, 1, size, m->f) != size) return false;
    if (m->count > 0) m->deltas[m->count - 1] = t_ms > m->last_ms ? t_ms - m->last_ms : 1;
    m->sizes[m->count] = size;
    m->offsets[m->count] = m->pos;
    m->deltas[m->count] = 50;
    m->pos += size;
    m->last_ms = t_ms;
    m->count++;
    return true;
}

static bool mov_close(MovWriter *m, int fps) {
    if (!m->f) return false;
    if (m->count > 0) m->deltas[m->count - 1] = 1000 / fps;
    uint64_t dur = 0;
    for (int i = 0; i < m->count; i++) dur += m->deltas[i];
    /* patch the mdat size */
    uint64_t mdat = m->pos - 20;
    fflush(m->f);
    unsigned char sz[8];
    for (int i = 0; i < 8; i++) sz[i] = mdat >> (56 - 8 * i);
    fseek(m->f, 28, SEEK_SET);
    fwrite(sz, 1, 8, m->f);
    fseek(m->f, 0, SEEK_END);

    Buf b = {0};
    size_t moov = b_open(&b, "moov");
    size_t a = b_open(&b, "mvhd");
    b_u32(&b, 0); b_u32(&b, 0); b_u32(&b, 0); b_u32(&b, 1000); b_u32(&b, (uint32_t)dur);
    b_u32(&b, 0x10000); b_u16(&b, 0x100); b_zero(&b, 10); b_matrix(&b);
    b_zero(&b, 24); b_u32(&b, 2);
    b_close(&b, a);
    size_t trak = b_open(&b, "trak");
    a = b_open(&b, "tkhd");
    b_u32(&b, 0xF); b_u32(&b, 0); b_u32(&b, 0); b_u32(&b, 1); b_u32(&b, 0); b_u32(&b, (uint32_t)dur);
    b_zero(&b, 8); b_u16(&b, 0); b_u16(&b, 0); b_u16(&b, 0); b_u16(&b, 0); b_matrix(&b);
    b_u32(&b, (uint32_t)m->w << 16); b_u32(&b, (uint32_t)m->h << 16);
    b_close(&b, a);
    size_t mdia = b_open(&b, "mdia");
    a = b_open(&b, "mdhd");
    b_u32(&b, 0); b_u32(&b, 0); b_u32(&b, 0); b_u32(&b, 1000); b_u32(&b, (uint32_t)dur); b_u16(&b, 0x55C4); b_u16(&b, 0);
    b_close(&b, a);
    a = b_open(&b, "hdlr");
    b_u32(&b, 0); b_4cc(&b, "mhlr"); b_4cc(&b, "vide"); b_zero(&b, 12); b_u8(&b, 12); b_raw(&b, "VideoHandler", 12);
    b_close(&b, a);
    size_t minf = b_open(&b, "minf");
    a = b_open(&b, "vmhd");
    b_u32(&b, 1); b_u16(&b, 0x40); b_zero(&b, 6);
    b_close(&b, a);
    a = b_open(&b, "hdlr");
    b_u32(&b, 0); b_4cc(&b, "dhlr"); b_4cc(&b, "alis"); b_zero(&b, 12); b_u8(&b, 11); b_raw(&b, "DataHandler", 11);
    b_close(&b, a);
    size_t dinf = b_open(&b, "dinf");
    a = b_open(&b, "dref");
    b_u32(&b, 0); b_u32(&b, 1); b_u32(&b, 12); b_4cc(&b, "alis"); b_u32(&b, 1);
    b_close(&b, a);
    b_close(&b, dinf);
    size_t stbl = b_open(&b, "stbl");
    a = b_open(&b, "stsd");
    b_u32(&b, 0); b_u32(&b, 1);
    b_u32(&b, 86); b_4cc(&b, "jpeg"); b_zero(&b, 6); b_u16(&b, 1);
    b_u16(&b, 0); b_u16(&b, 0); b_u32(&b, 0); b_u32(&b, 0); b_u32(&b, 512);
    b_u16(&b, m->w); b_u16(&b, m->h); b_u32(&b, 72 << 16); b_u32(&b, 72 << 16); b_u32(&b, 0); b_u16(&b, 1);
    b_u8(&b, 12); b_raw(&b, "Photo - JPEG", 12); b_zero(&b, 19);
    b_u16(&b, 24); b_u16(&b, 0xFFFF);
    b_close(&b, a);
    a = b_open(&b, "stts"); /* run-length (count, delta) */
    b_u32(&b, 0);
    size_t cnt_at = b.n;
    b_u32(&b, 0);
    uint32_t runs = 0;
    for (int i = 0; i < m->count;) {
        int j = i;
        while (j < m->count && m->deltas[j] == m->deltas[i]) j++;
        b_u32(&b, j - i); b_u32(&b, m->deltas[i]);
        runs++;
        i = j;
    }
    b.p[cnt_at] = runs >> 24; b.p[cnt_at + 1] = runs >> 16; b.p[cnt_at + 2] = runs >> 8; b.p[cnt_at + 3] = runs;
    b_close(&b, a);
    a = b_open(&b, "stsc");
    b_u32(&b, 0); b_u32(&b, 1); b_u32(&b, 1); b_u32(&b, 1); b_u32(&b, 1);
    b_close(&b, a);
    a = b_open(&b, "stsz");
    b_u32(&b, 0); b_u32(&b, 0); b_u32(&b, m->count);
    for (int i = 0; i < m->count; i++) b_u32(&b, m->sizes[i]);
    b_close(&b, a);
    a = b_open(&b, "co64");
    b_u32(&b, 0); b_u32(&b, m->count);
    for (int i = 0; i < m->count; i++) b_u64(&b, m->offsets[i]);
    b_close(&b, a);
    b_close(&b, stbl);
    b_close(&b, minf);
    b_close(&b, mdia);
    b_close(&b, trak);
    b_close(&b, moov);
    bool ok = fwrite(b.p, 1, b.n, m->f) == b.n;
    free(b.p);
    ok = (fflush(m->f) == 0) && ok;
    fsync(fileno(m->f));
    ok = (fclose(m->f) == 0) && ok;
    m->f = NULL;
    free(m->sizes);
    free(m->deltas);
    free(m->offsets);
    if (m->count == 0) unlink(m->path);
    return ok;
}

/* ---------------- capture pipeline ---------------- */

#define SLOTS 6
#define ENCODERS 2
enum { SLOT_FREE, SLOT_RAW, SLOT_BUSY, SLOT_DONE };

typedef struct {
    int state;
    uint64_t seq;
    uint32_t t_ms;
    unsigned char *rgb;
    Buf jpg;
} Slot;

static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    Slot slot[SLOTS];
    bool running, capturing;
    uint64_t next_seq, next_write;
    int w, h, fps, quality;
    MovWriter mov;
    bool write_error;
    pthread_t cap_th, enc_th[ENCODERS], wr_th;
    int fb;
    size_t fblen;
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;
    struct timespec t0;
    int dropped;
} R = {.mu = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER, .fb = -1};

static uint32_t now_ms(const struct timespec *t0) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((t.tv_sec - t0->tv_sec) * 1000 + (t.tv_nsec - t0->tv_nsec) / 1000000);
}

/* framebuffer (currently displayed buffer) -> packed RGB, full or half size.
 * The framebuffer is uncached memory: one bulk pread() is ~20x faster than reading it pixel by pixel. */
static unsigned char *rawbuf;

static void grab(unsigned char *dst) {
    struct fb_var_screeninfo v = R.vi;
    ioctl(R.fb, FBIOGET_VSCREENINFO, &v);
    size_t len = (size_t)R.vi.yres * R.fi.line_length;
    off_t off = (off_t)v.yoffset * R.fi.line_length + (off_t)v.xoffset * 4;
    if ((size_t)off + len > R.fblen) off = 0;
    if (pread(R.fb, rawbuf, len, off) != (ssize_t)len) return;
    int ro = v.red.offset / 8, go = v.green.offset / 8, bo = v.blue.offset / 8;
    int step = R.w == (int)R.vi.xres ? 1 : 2;
    for (int y = 0; y < R.h; y++) {
        const unsigned char *s = rawbuf + (size_t)(y * step) * R.fi.line_length;
        unsigned char *d = dst + (size_t)y * R.w * 3;
        for (int x = 0; x < R.w; x++, s += 4 * step, d += 3) {
            d[0] = s[ro];
            d[1] = s[go];
            d[2] = s[bo];
        }
    }
}

static void *capture_thread(void *arg) {
    (void)arg;
    uint32_t interval = 1000 / R.fps, next = 0;
    while (R.capturing) {
        uint32_t t = now_ms(&R.t0);
        if (t < next) {
            usleep((next - t) * 1000);
            continue;
        }
        next = (next ? next : t) + interval;
        if (t > next + interval) next = t + interval; /* fell behind: don't burst */
        pthread_mutex_lock(&R.mu);
        int k = -1;
        for (int i = 0; i < SLOTS; i++)
            if (R.slot[i].state == SLOT_FREE) { k = i; break; }
        if (k < 0) { R.dropped++; pthread_mutex_unlock(&R.mu); continue; }
        R.slot[k].state = SLOT_BUSY;
        pthread_mutex_unlock(&R.mu);
        grab(R.slot[k].rgb);
        pthread_mutex_lock(&R.mu);
        R.slot[k].seq = R.next_seq++;
        R.slot[k].t_ms = t;
        R.slot[k].state = SLOT_RAW;
        pthread_cond_broadcast(&R.cv);
        pthread_mutex_unlock(&R.mu);
    }
    return NULL;
}

static void jpg_sink(void *ctx, void *data, int size) { b_raw(ctx, data, size); }

static void *encode_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&R.mu);
    for (;;) {
        int k = -1;
        for (int i = 0; i < SLOTS; i++)
            if (R.slot[i].state == SLOT_RAW && (k < 0 || R.slot[i].seq < R.slot[k].seq)) k = i;
        if (k < 0) {
            if (!R.running) break;
            pthread_cond_wait(&R.cv, &R.mu);
            continue;
        }
        R.slot[k].state = SLOT_BUSY;
        pthread_mutex_unlock(&R.mu);
        R.slot[k].jpg.n = 0;
        stbi_write_jpg_to_func(jpg_sink, &R.slot[k].jpg, R.w, R.h, 3, R.slot[k].rgb, R.quality);
        pthread_mutex_lock(&R.mu);
        R.slot[k].state = SLOT_DONE;
        pthread_cond_broadcast(&R.cv);
    }
    pthread_mutex_unlock(&R.mu);
    return NULL;
}

static void *write_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&R.mu);
    for (;;) {
        int k = -1;
        for (int i = 0; i < SLOTS; i++)
            if (R.slot[i].state == SLOT_DONE && R.slot[i].seq == R.next_write) k = i;
        if (k < 0) {
            bool pending = false;
            for (int i = 0; i < SLOTS; i++)
                if (R.slot[i].state != SLOT_FREE) pending = true;
            if (!R.running && !pending) break;
            pthread_cond_wait(&R.cv, &R.mu);
            continue;
        }
        pthread_mutex_unlock(&R.mu);
        if (!R.write_error && !mov_add(&R.mov, R.slot[k].jpg.p, (uint32_t)R.slot[k].jpg.n, R.slot[k].t_ms))
            R.write_error = true;
        pthread_mutex_lock(&R.mu);
        R.slot[k].state = SLOT_FREE;
        R.next_write++;
        pthread_cond_broadcast(&R.cv);
    }
    pthread_mutex_unlock(&R.mu);
    return NULL;
}

static uint64_t free_mb(const char *path) {
    struct statvfs s;
    if (statvfs(path, &s) != 0) return 0;
    return (uint64_t)s.f_bavail * s.f_frsize / (1024 * 1024);
}

static void leds_recording(bool on) {
    if (on) {
        LedState s;
        leds_defaults(&s);
        s.on = true;
        s.brightness = 40;
        for (int z = 0; z < LED_ZONES; z++) {
            s.color[z] = 0xFF0010;
            s.effect[z] = 2; /* Breath */
            s.speed[z] = 2;
        }
        leds_apply(&s);
    } else if (file_exists(DATA_DIR "/leds.cfg")) {
        leds_cli_apply(); /* back to the user's RaineyUI LED preset */
    } else {
        FILE *f = fopen("/sys/class/led_anim/enable", "w"); /* no preset: lights off again */
        if (f) { fputs("0", f); fclose(f); }
    }
}

static bool rec_start(void) {
    RecConfig c;
    rec_config_load(&c);
    if (free_mb("/mnt/UDISK") < REC_MIN_FREE_MB) {
        fprintf(stderr, "rec: not enough free space\n");
        return false;
    }
    R.fb = open("/dev/fb0", O_RDONLY);
    if (R.fb < 0 || ioctl(R.fb, FBIOGET_VSCREENINFO, &R.vi) || ioctl(R.fb, FBIOGET_FSCREENINFO, &R.fi) ||
        R.vi.bits_per_pixel != 32) {
        fprintf(stderr, "rec: framebuffer unavailable\n");
        if (R.fb >= 0) close(R.fb);
        R.fb = -1;
        return false;
    }
    R.fblen = R.fi.smem_len;
    rawbuf = malloc((size_t)R.vi.yres * R.fi.line_length);
    R.w = c.half ? R.vi.xres / 2 : R.vi.xres;
    R.h = c.half ? R.vi.yres / 2 : R.vi.yres;
    R.fps = REC_FPS[c.fps];
    R.quality = c.half ? 85 : 75;
    mkdirs(REC_DIR);
    char path[256];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    snprintf(path, sizeof path, REC_DIR "/Recording %04d-%02d-%02d %02d.%02d.%02d.mov", tm.tm_year + 1900,
             tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    if (!mov_open(&R.mov, path, R.w, R.h)) {
        free(rawbuf);
        rawbuf = NULL;
        close(R.fb);
        R.fb = -1;
        return false;
    }
    for (int i = 0; i < SLOTS; i++) {
        R.slot[i].state = SLOT_FREE;
        R.slot[i].rgb = malloc((size_t)R.w * R.h * 3);
        R.slot[i].jpg = (Buf){0};
    }
    R.next_seq = R.next_write = 0;
    R.write_error = false;
    R.dropped = 0;
    clock_gettime(CLOCK_MONOTONIC, &R.t0);
    R.running = R.capturing = true;
    for (int i = 0; i < ENCODERS; i++) pthread_create(&R.enc_th[i], NULL, encode_thread, NULL);
    pthread_create(&R.wr_th, NULL, write_thread, NULL);
    pthread_create(&R.cap_th, NULL, capture_thread, NULL);
    char st[300];
    snprintf(st, sizeof st, "rec\n%ld\n%s\n", (long)now, path);
    write_state(st);
    leds_recording(true);
    fprintf(stderr, "rec: started %s (%dx%d @ %d fps)\n", path, R.w, R.h, R.fps);
    return true;
}

static void rec_stop(void) {
    if (!R.running) return;
    R.capturing = false;
    pthread_join(R.cap_th, NULL);
    pthread_mutex_lock(&R.mu);
    R.running = false;
    pthread_cond_broadcast(&R.cv);
    pthread_mutex_unlock(&R.mu);
    for (int i = 0; i < ENCODERS; i++) pthread_join(R.enc_th[i], NULL);
    pthread_join(R.wr_th, NULL);
    int frames = R.mov.count;
    bool ok = mov_close(&R.mov, R.fps) && !R.write_error;
    for (int i = 0; i < SLOTS; i++) {
        free(R.slot[i].rgb);
        free(R.slot[i].jpg.p);
    }
    free(rawbuf);
    rawbuf = NULL;
    close(R.fb);
    R.fb = -1;
    leds_recording(false);
    write_state("idle\n");
    fprintf(stderr, "rec: stopped, %d frames (%d dropped)%s\n", frames, R.dropped, ok ? "" : " WRITE ERROR");
}

/* ---------------- daemon: hotkey + control ---------------- */

static volatile sig_atomic_t quit_flag;
static void on_signal(int s) { (void)s; quit_flag = 1; }

static int open_pad(void) {
    DIR *d = opendir("/dev/input");
    if (!d) return -1;
    struct dirent *e;
    int fd = -1;
    while ((e = readdir(d)) && fd < 0) {
        if (strncmp(e->d_name, "event", 5)) continue;
        char p[64], name[128] = {0};
        snprintf(p, sizeof p, "/dev/input/%s", e->d_name);
        int f = open(p, O_RDONLY | O_NONBLOCK);
        if (f < 0) continue;
        ioctl(f, EVIOCGNAME(sizeof name - 1), name);
        if (strstr(name, "Player1") || strstr(name, "X-Box") || strstr(name, "Xbox")) fd = f;
        else close(f);
    }
    closedir(d);
    return fd;
}

int rec_daemon_main(void) {
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGPIPE, SIG_IGN);
    FILE *pf = fopen(REC_PID, "w");
    if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }
    write_state("idle\n");
    leds_load();
    int pad = -1;
    bool sel = false, l1 = false, r1 = false, armed = true;
    Uint32 last_check = 0;
    while (!quit_flag) {
        if (pad < 0) {
            pad = open_pad();
            if (pad < 0) { sleep(1); continue; }
        }
        struct pollfd p = {pad, POLLIN, 0};
        int r = poll(&p, 1, 250);
        if (r > 0) {
            struct input_event ev[32];
            ssize_t n = read(pad, ev, sizeof ev);
            if (n < 0 && errno != EAGAIN) { close(pad); pad = -1; continue; }
            for (int i = 0; i < n / (ssize_t)sizeof ev[0]; i++) {
                if (ev[i].type != EV_KEY) continue;
                bool down = ev[i].value != 0;
                if (ev[i].code == BTN_SELECT) sel = down;
                else if (ev[i].code == BTN_TL) l1 = down;
                else if (ev[i].code == BTN_TR) r1 = down;
            }
            if (sel && l1 && r1) {
                if (armed) {
                    armed = false;
                    if (R.running) rec_stop();
                    else rec_start();
                }
            } else if (!sel && !l1 && !r1) {
                armed = true;
            }
        }
        /* control file from the app ("stop"), and the free-space guard */
        Uint32 t = (Uint32)time(NULL);
        if (t != last_check) {
            last_check = t;
            if (access(REC_CMD, F_OK) == 0) {
                char cmd[16] = {0};
                FILE *f = fopen(REC_CMD, "r");
                if (f) { fgets(cmd, sizeof cmd, f); fclose(f); }
                unlink(REC_CMD);
                if (!strncmp(cmd, "stop", 4) && R.running) rec_stop();
                if (!strncmp(cmd, "start", 5) && !R.running) rec_start();
            }
            if (R.running && (R.write_error || free_mb("/mnt/UDISK") < REC_MIN_FREE_MB / 2)) {
                fprintf(stderr, "rec: stopping (storage full or write error)\n");
                rec_stop();
            }
        }
    }
    rec_stop();
    unlink(REC_PID);
    unlink(REC_STATE);
    return 0;
}

/* ---------------- reader (for the in-app player) ---------------- */

static uint32_t be32(const unsigned char *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint64_t be64(const unsigned char *p) { return (uint64_t)be32(p) << 32 | be32(p + 4); }

static void parse_atoms(MovInfo *mi, const unsigned char *p, size_t n) {
    size_t i = 0;
    while (i + 8 <= n) {
        uint32_t sz = be32(p + i);
        const unsigned char *t = p + i + 4;
        if (sz < 8 || i + sz > n) break;
        const unsigned char *body = p + i + 8;
        size_t blen = sz - 8;
        if (!memcmp(t, "trak", 4) || !memcmp(t, "mdia", 4) || !memcmp(t, "minf", 4) || !memcmp(t, "stbl", 4)) {
            parse_atoms(mi, body, blen);
        } else if (!memcmp(t, "tkhd", 4) && blen >= 84) {
            mi->w = be32(body + 76) >> 16;
            mi->h = be32(body + 80) >> 16;
        } else if (!memcmp(t, "stsz", 4) && blen >= 12) {
            int c = be32(body + 8);
            if (12 + (size_t)c * 4 <= blen && !mi->sizes) {
                mi->count = c;
                mi->sizes = malloc(c * sizeof *mi->sizes);
                for (int k = 0; k < c; k++) mi->sizes[k] = be32(body + 12 + k * 4);
            }
        } else if ((!memcmp(t, "co64", 4) || !memcmp(t, "stco", 4)) && blen >= 8 && !mi->offsets) {
            int c = be32(body + 4), w = t[3] == '4' ? 8 : 4;
            if (8 + (size_t)c * w <= blen) {
                mi->offsets = malloc(c * sizeof *mi->offsets);
                mi->noffsets = c;
                for (int k = 0; k < c; k++)
                    mi->offsets[k] = w == 8 ? be64(body + 8 + k * 8) : be32(body + 8 + k * 4);
            }
        } else if (!memcmp(t, "stts", 4) && blen >= 8 && !mi->times) {
            int runs = be32(body + 4);
            mi->ntimes = 0;
            for (int k = 0; k < runs && 8 + (size_t)k * 8 + 8 <= blen; k++) mi->ntimes += be32(body + 8 + k * 8);
            mi->times = malloc((mi->ntimes + 1) * sizeof *mi->times);
            uint32_t t_ms = 0;
            int idx = 0;
            for (int k = 0; k < runs && 8 + (size_t)k * 8 + 8 <= blen; k++) {
                uint32_t c = be32(body + 8 + k * 8), d = be32(body + 12 + k * 8);
                for (uint32_t j = 0; j < c; j++) { mi->times[idx++] = t_ms; t_ms += d; }
            }
            mi->duration_ms = t_ms;
        }
        i += sz;
    }
}

bool mov_info_load(MovInfo *mi, const char *path) {
    memset(mi, 0, sizeof *mi);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char h[16];
    uint64_t pos = 0;
    bool ok = false;
    while (fseeko(f, (off_t)pos, SEEK_SET) == 0 && fread(h, 1, 8, f) == 8) {
        uint64_t sz = be32(h);
        if (sz == 1) {
            if (fread(h + 8, 1, 8, f) != 8) break;
            sz = be64(h + 8);
        }
        if (sz < 8) break;
        if (!memcmp(h + 4, "moov", 4) && sz < (64u << 20)) {
            unsigned char *b = malloc(sz);
            fseeko(f, (off_t)pos + 8, SEEK_SET);
            if (b && fread(b, 1, sz - 8, f) == sz - 8) {
                parse_atoms(mi, b, sz - 8);
                ok = mi->count > 0 && mi->noffsets == mi->count && mi->ntimes == mi->count;
            }
            free(b);
            break;
        }
        pos += sz;
    }
    fclose(f);
    if (!ok) mov_info_free(mi);
    return ok;
}

void mov_info_free(MovInfo *mi) {
    free(mi->sizes);
    free(mi->offsets);
    free(mi->times);
    memset(mi, 0, sizeof *mi);
}
