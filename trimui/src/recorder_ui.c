/* Screen Recorder app (`rainey --rec-app`): service switch + settings, a file browser for internal
 * storage and the SD card, a player for recordings and an image viewer. */
#include "rainey.h"
#include "recorder.h"
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

extern char g_shot_request[256];

#define REC_INIT "/etc/init.d/raineyrec"
#define REC_RCD "/etc/rc.d/S98zraineyrec"

typedef enum { R_HOME, R_BROWSE, R_PLAY, R_VIEW } RScreen;
static RScreen scr = R_HOME;
static RecConfig cfg;
static int focus;
static char msg[160];
static Uint32 msg_until;
static bool want_quit;

static void toast(const char *m) {
    snprintf(msg, sizeof msg, "%s", m);
    msg_until = SDL_GetTicks() + 2200;
}

static void draw_toast(void) {
    if (!msg[0] || SDL_TICKS_PASSED(SDL_GetTicks(), msg_until)) return;
    int w = gfx_text_w(F_BODY, msg) + 40;
    int x = (SCREEN_W - w) / 2, y = SCREEN_H - 110;
    gfx_round_fill(x, y, w, 46, 6, C_PANEL, 235);
    gfx_round_ring(x, y, w, 46, 6, 1, C_NEON_BLUE, 185);
    gfx_text_center(F_BODY, msg, SCREEN_W / 2, y + 10, C_TEXT, 255);
}

static void toggle(int x, int y, bool on, bool focused) {
    int w = 78, h = 44;
    gfx_round_fill(x, y, w, h, 22, on ? C_NEON_BLUE : 0x2A3350, 255);
    if (focused) gfx_round_ring(x - 4, y - 4, w + 8, h + 8, 26, 2, C_NEON_PINK, 255);
    gfx_round_fill(on ? x + w - 40 : x + 4, y + 4, 36, 36, 18, on ? 0x000000 : 0xA7B0C0, 255);
}

static void chips(int rx, int y, const char *const *opts, int n, int sel) {
    int x = rx;
    for (int k = n - 1; k >= 0; k--) {
        int w = gfx_text_w(F_BODY, opts[k]) + 28;
        x -= w;
        bool on = sel == k;
        gfx_round_fill(x, y + 2, w, 42, 6, on ? C_NEON_BLUE : C_PANEL, on ? 60 : 200);
        gfx_round_ring(x, y + 2, w, 42, 6, 2, on ? C_NEON_BLUE : C_NEON_PURPLE, on ? 255 : 110);
        gfx_text_center(F_BODY, opts[k], x + w / 2, y + 10, C_TEXT, 255);
        x -= 8;
    }
}

static void fmt_size(char *o, size_t n, unsigned long long b) {
    if (b >= 1ull << 30) snprintf(o, n, "%.1f GB", b / 1073741824.0);
    else if (b >= 1ull << 20) snprintf(o, n, "%.1f MB", b / 1048576.0);
    else if (b >= 1024) snprintf(o, n, "%llu KB", b / 1024);
    else snprintf(o, n, "%llu B", b);
}

static unsigned long long free_bytes(const char *p) {
    struct statvfs s;
    return statvfs(p, &s) == 0 ? (unsigned long long)s.f_bavail * s.f_frsize : 0;
}

static bool sd_mounted(void) {
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return false;
    char line[512];
    bool ok = false;
    while (fgets(line, sizeof line, f))
        if (strstr(line, " /mnt/SDCARD ")) ok = true;
    fclose(f);
    return ok;
}

/* ---------------- service control ---------------- */

static int service_pid(void) {
    FILE *f = fopen(REC_PID, "r");
    if (!f) return 0;
    int pid = 0;
    if (fscanf(f, "%d", &pid) != 1) pid = 0;
    fclose(f);
    return pid > 0 && kill(pid, 0) == 0 ? pid : 0;
}

static bool recording(long *since, char *path, size_t n) {
    FILE *f = fopen(REC_STATE, "r");
    if (!f) return false;
    char line[300];
    bool rec = fgets(line, sizeof line, f) && !strncmp(line, "rec", 3);
    if (rec) {
        if (fgets(line, sizeof line, f) && since) *since = atol(line);
        if (fgets(line, sizeof line, f) && path) {
            line[strcspn(line, "\n")] = 0;
            snprintf(path, n, "%s", line);
        }
    }
    fclose(f);
    return rec;
}

static void service_set(bool on) {
    char cmd[1200];
    if (on) {
        if (!service_pid()) {
            snprintf(cmd, sizeof cmd,
                     "rm -f " REC_PID "; LD_LIBRARY_PATH=/usr/trimui/lib start-stop-daemon -S -b -p " REC_PID
                     " -x '%s/rainey' -- --rec-daemon",
                     g_appdir);
            system(cmd);
        }
        /* start at boot too */
        snprintf(cmd, sizeof cmd, "cp '%s/recorder.init' " REC_INIT " && chmod +x " REC_INIT
                 " && ln -sf ../init.d/raineyrec " REC_RCD, g_appdir);
        system(cmd);
    } else {
        int pid = service_pid();
        if (pid) {
            kill(pid, SIGTERM); /* finishes a running recording first */
            for (int i = 0; i < 50 && kill(pid, 0) == 0; i++) SDL_Delay(100);
        }
        unlink(REC_RCD);
        unlink(REC_INIT);
    }
    sync();
}

/* ---------------- file browser ---------------- */

typedef struct {
    char name[256];
    bool dir;
    unsigned long long size;
    time_t mtime;
} Entry;

static Entry *ents;
static int nents, ent_cap, bsel, bscroll;
static char cwd[1024] = ""; /* "" = list of storage roots */
static bool confirm_delete;

static int ent_cmp(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    if (x->dir != y->dir) return x->dir ? -1 : 1;
    if (!strncmp(x->name, "Recording ", 10) && !strncmp(y->name, "Recording ", 10))
        return strcmp(y->name, x->name); /* newest recording first */
    return strcasecmp(x->name, y->name);
}

static void add_ent(const char *name, bool dir, unsigned long long size, time_t mt) {
    if (nents == ent_cap) {
        ent_cap = ent_cap ? ent_cap * 2 : 128;
        ents = realloc(ents, ent_cap * sizeof *ents);
    }
    Entry *e = &ents[nents++];
    snprintf(e->name, sizeof e->name, "%s", name);
    e->dir = dir;
    e->size = size;
    e->mtime = mt;
}

static void browse_load(const char *path, const char *select_name) {
    snprintf(cwd, sizeof cwd, "%s", path);
    nents = 0;
    bsel = bscroll = 0;
    if (!cwd[0]) {
        add_ent("Recordings", true, 0, 0);
        add_ent("Internal storage", true, 0, 0);
        if (sd_mounted()) add_ent("SD card", true, 0, 0);
        return;
    }
    DIR *d = opendir(cwd);
    if (!d) {
        toast("Can't open this folder");
        return;
    }
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char p[1300];
        snprintf(p, sizeof p, "%s/%s", cwd, de->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        add_ent(de->d_name, S_ISDIR(st.st_mode), st.st_size, st.st_mtime);
    }
    closedir(d);
    qsort(ents, nents, sizeof *ents, ent_cmp);
    if (select_name)
        for (int i = 0; i < nents; i++)
            if (!strcmp(ents[i].name, select_name)) bsel = i;
}

static const char *root_path(const char *name) {
    if (!strcmp(name, "Recordings")) return REC_DIR;
    if (!strcmp(name, "Internal storage")) return "/mnt/UDISK";
    return "/mnt/SDCARD";
}

static const char *ext_of(const char *n) {
    const char *d = strrchr(n, '.');
    return d ? d + 1 : "";
}

static bool is_image(const char *n) {
    const char *e = ext_of(n);
    return !strcasecmp(e, "png") || !strcasecmp(e, "jpg") || !strcasecmp(e, "jpeg") || !strcasecmp(e, "bmp");
}

/* ---------------- player / viewer ---------------- */

static MovInfo mi;
static FILE *mf;
static SDL_Texture *frame_tex, *view_tex;
static int cur_frame = -1;
static double play_ms;
static bool paused;
static Uint32 last_tick, overlay_until;
static unsigned char *fbuf;
static size_t fbuf_cap;
static char open_name[256];

static void show_frame(int i) {
    if (i == cur_frame || i < 0 || i >= mi.count) return;
    uint32_t sz = mi.sizes[i];
    if (sz > fbuf_cap) {
        fbuf_cap = sz;
        fbuf = realloc(fbuf, fbuf_cap);
    }
    if (fseeko(mf, (off_t)mi.offsets[i], SEEK_SET) != 0 || fread(fbuf, 1, sz, mf) != sz) return;
    SDL_Surface *s = IMG_Load_RW(SDL_RWFromConstMem(fbuf, sz), 1);
    if (!s) return;
    if (frame_tex) SDL_DestroyTexture(frame_tex);
    frame_tex = SDL_CreateTextureFromSurface(g_ren, s);
    SDL_FreeSurface(s);
    cur_frame = i;
}

static bool play_open(const char *path, const char *name) {
    if (!mov_info_load(&mi, path)) return false;
    mf = fopen(path, "rb");
    if (!mf) {
        mov_info_free(&mi);
        return false;
    }
    snprintf(open_name, sizeof open_name, "%s", name);
    cur_frame = -1;
    play_ms = 0;
    paused = false;
    last_tick = SDL_GetTicks();
    overlay_until = last_tick + 2500;
    show_frame(0);
    return true;
}

static void play_close(void) {
    if (mf) fclose(mf);
    mf = NULL;
    mov_info_free(&mi);
    if (frame_tex) SDL_DestroyTexture(frame_tex);
    frame_tex = NULL;
}

static int frame_at(double ms) {
    int lo = 0, hi = mi.count - 1;
    while (lo < hi) {
        int m = (lo + hi + 1) / 2;
        if (mi.times[m] <= ms) lo = m;
        else hi = m - 1;
    }
    return lo;
}

static void fmt_time(char *o, size_t n, double ms) {
    int s = (int)(ms / 1000);
    snprintf(o, n, "%d:%02d", s / 60, s % 60);
}

/* ---------------- screens ---------------- */

enum { H_SERVICE, H_SIZE, H_FPS, H_RECORDINGS, H_BROWSE, H_EXIT, H_N };

static int count_recordings(void) {
    DIR *d = opendir(REC_DIR);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)))
        if (!strcasecmp(ext_of(e->d_name), "mov")) n++;
    closedir(d);
    return n;
}

static void home_draw(void) {
    gfx_text(F_HUGE, "Screen Recorder", 28, 18, C_TEXT, 255);
    gfx_text(F_BODY, "Record any game or app, then watch or manage your files", 28, 76, C_MUTED, 255);
    bool svc = service_pid() != 0;
    long since = 0;
    bool rec = svc && recording(&since, NULL, 0);
    /* status card */
    int cy = 122;
    gfx_round_fill(28, cy, SCREEN_W - 56, 96, 12, C_PANEL, 220);
    gfx_round_ring(28, cy, SCREEN_W - 56, 96, 12, 2, rec ? 0xFF2040 : svc ? C_NEON_BLUE : C_NEON_PURPLE, 200);
    if (rec) {
        char t[64];
        snprintf(t, sizeof t, "● Recording  %ld:%02ld", (time(NULL) - since) / 60, (time(NULL) - since) % 60);
        gfx_text(F_TITLE, t, 52, cy + 14, 0xFF4060, 255);
        gfx_text(F_BODY, "Press SELECT + L1 + R1 to stop.", 52, cy + 60, C_TEXT, 255);
    } else if (svc) {
        gfx_text(F_TITLE, "Ready", 52, cy + 14, C_NEON_BLUE, 255);
        gfx_text(F_BODY, "In any game, press SELECT + L1 + R1 to start or stop.", 52, cy + 60, C_TEXT, 255);
    } else {
        gfx_text(F_TITLE, "Recorder is off", 52, cy + 14, C_MUTED, 255);
        gfx_text(F_BODY, "Turn it on below, then press SELECT + L1 + R1 in a game.", 52, cy + 60, C_TEXT, 255);
    }
    const char *labels[H_N] = {"Recorder", "Video size", "Frame rate", "Recordings", "Browse files", "Exit"};
    int y = 246;
    for (int i = 0; i < H_N; i++) {
        bool f = focus == i;
        if (f) {
            gfx_round_fill(20, y - 4, SCREEN_W - 40, 52, 8, C_NEON_BLUE, 26);
            gfx_round_ring(20, y - 4, SCREEN_W - 40, 52, 8, 1, C_NEON_BLUE, 140);
        }
        gfx_text(F_LABEL, labels[i], 36, y + 9, C_TEXT, 255);
        int rx = SCREEN_W - 48;
        char v[64];
        switch (i) {
        case H_SERVICE: toggle(rx - 78, y + 1, svc, f); break;
        case H_SIZE: {
            static const char *o[] = {"1024x768", "512x384"};
            chips(rx, y, o, 2, cfg.half);
            break;
        }
        case H_FPS: {
            static const char *o[] = {"15 fps", "20 fps", "30 fps"};
            chips(rx, y, o, 3, cfg.fps);
            break;
        }
        case H_RECORDINGS:
            snprintf(v, sizeof v, "%d  ›", count_recordings());
            gfx_text(F_BODY, v, rx - gfx_text_w(F_BODY, v), y + 10, C_MUTED, 255);
            break;
        case H_BROWSE:
            gfx_text(F_BODY, "Internal + SD card  ›", rx - gfx_text_w(F_BODY, "Internal + SD card  ›"), y + 10,
                     C_MUTED, 255);
            break;
        case H_EXIT: gfx_text(F_BODY, "A", rx - 20, y + 10, C_MUTED, 255); break;
        }
        y += 56;
    }
    char fs[64], line[160];
    fmt_size(fs, sizeof fs, free_bytes("/mnt/UDISK"));
    snprintf(line, sizeof line, "Saved to Recordings on internal storage · %s free · video only, no sound", fs);
    gfx_text(F_SMALL, line, 36, y + 10, C_MUTED, 255);
    static const char *h[] = {"A", "Select", "←→", "Change", "B", "Exit"};
    ui_hints(h, 3);
}

static void home_input(InputState *in) {
    if (in->pressed[B_B] || in->quit) { want_quit = true; return; }
    if (in->pressed[B_DOWN] && focus < H_N - 1) focus++;
    if (in->pressed[B_UP] && focus > 0) focus--;
    int d = in->pressed[B_RIGHT] ? 1 : in->pressed[B_LEFT] ? -1 : in->pressed[B_A] ? 1 : 0;
    if (!d) return;
    switch (focus) {
    case H_SERVICE: {
        bool on = !service_pid();
        cfg.enabled = on;
        rec_config_save(&cfg);
        service_set(on);
        toast(on ? "Recorder on: SELECT + L1 + R1 in any game" : "Recorder off");
        break;
    }
    case H_SIZE: cfg.half = !cfg.half; rec_config_save(&cfg); break;
    case H_FPS: cfg.fps = (cfg.fps + REC_FPS_N + d) % REC_FPS_N; rec_config_save(&cfg); break;
    case H_RECORDINGS:
        if (in->pressed[B_A] || in->pressed[B_RIGHT]) {
            mkdirs(REC_DIR);
            browse_load(REC_DIR, NULL);
            scr = R_BROWSE;
        }
        break;
    case H_BROWSE:
        if (in->pressed[B_A] || in->pressed[B_RIGHT]) {
            browse_load("", NULL);
            scr = R_BROWSE;
        }
        break;
    case H_EXIT:
        if (in->pressed[B_A]) want_quit = true;
        break;
    }
}

#define ROWS 10

static void browse_draw(void) {
    const char *title = !cwd[0] ? "Files" : !strcmp(cwd, REC_DIR) ? "Recordings"
                        : !strcmp(cwd, "/mnt/UDISK") ? "Internal storage"
                        : !strcmp(cwd, "/mnt/SDCARD") ? "SD card" : strrchr(cwd, '/') + 1;
    gfx_text(F_HUGE, title, 28, 18, C_TEXT, 255);
    gfx_text_ellipsis(F_SMALL, cwd[0] ? cwd : "Choose a storage location", 30, 80, SCREEN_W - 60, C_MUTED, 255);
    if (bsel < bscroll) bscroll = bsel;
    if (bsel >= bscroll + ROWS) bscroll = bsel - ROWS + 1;
    int y = 118;
    if (!nents)
        gfx_text_center(F_BODY, !strcmp(cwd, REC_DIR) ? "No recordings yet. Press SELECT + L1 + R1 in a game."
                                                     : "This folder is empty.",
                        SCREEN_W / 2, 300, C_MUTED, 255);
    for (int i = bscroll; i < nents && i < bscroll + ROWS; i++) {
        Entry *e = &ents[i];
        bool f = i == bsel;
        if (f) {
            gfx_round_fill(20, y - 2, SCREEN_W - 40, 52, 8, C_NEON_BLUE, 30);
            gfx_round_ring(20, y - 2, SCREEN_W - 40, 52, 8, 1, C_NEON_BLUE, 150);
        }
        const char *ic = e->dir ? "▸" : !strcasecmp(ext_of(e->name), "mov") ? "▶" : is_image(e->name) ? "▣" : "·";
        gfx_text(F_LABEL, ic, 34, y + 8, e->dir ? C_NEON_PURPLE : C_NEON_BLUE, 255);
        gfx_text_ellipsis(F_BODY, e->name, 70, y + 12, SCREEN_W - 70 - 300, C_TEXT, 255);
        if (cwd[0]) {
            char info[64] = "", sz[24];
            if (!e->dir) {
                fmt_size(sz, sizeof sz, e->size);
                struct tm tm;
                localtime_r(&e->mtime, &tm);
                snprintf(info, sizeof info, "%s   %02d/%02d %02d:%02d", sz, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                         tm.tm_min);
            }
            gfx_text(F_SMALL, info, SCREEN_W - 48 - gfx_text_w(F_SMALL, info), y + 16, C_MUTED, 255);
        }
        y += 54;
    }
    if (nents > ROWS) {
        char pos[32];
        snprintf(pos, sizeof pos, "%d / %d", bsel + 1, nents);
        gfx_text(F_SMALL, pos, SCREEN_W - 48 - gfx_text_w(F_SMALL, pos), 84, C_MUTED, 255);
    }
    if (confirm_delete && nents) {
        gfx_fill(0, 0, SCREEN_W, SCREEN_H, 0x000000, 160);
        int w = 760, h = 200, x = (SCREEN_W - w) / 2, yy = 250;
        gfx_round_fill(x, yy, w, h, 14, C_PANEL, 250);
        gfx_round_ring(x, yy, w, h, 14, 2, C_NEON_PINK, 220);
        gfx_text_center(F_TITLE, "Delete this file?", SCREEN_W / 2, yy + 24, C_TEXT, 255);
        gfx_text_ellipsis(F_BODY, ents[bsel].name, x + 30, yy + 84, w - 60, C_MUTED, 255);
        gfx_text_center(F_BODY, "A  Delete        B  Keep", SCREEN_W / 2, yy + 140, C_TEXT, 255);
        return;
    }
    bool file = nents && !ents[bsel].dir && cwd[0];
    const char *h[] = {"A", "Open", "B", "Back", "X", "Delete", "L/R", "Page"};
    ui_hints(h, file ? 4 : 2);
}

static void browse_up(void) {
    if (!cwd[0] || !strcmp(cwd, REC_DIR) || !strcmp(cwd, "/mnt/UDISK") || !strcmp(cwd, "/mnt/SDCARD")) {
        bool from_root = !cwd[0];
        if (from_root || !strcmp(cwd, REC_DIR)) { scr = R_HOME; return; }
        browse_load("", NULL);
        return;
    }
    char parent[1024], name[256];
    snprintf(parent, sizeof parent, "%s", cwd);
    char *sl = strrchr(parent, '/');
    snprintf(name, sizeof name, "%s", sl + 1);
    *sl = 0;
    browse_load(parent, name);
}

static void browse_input(InputState *in) {
    if (confirm_delete) {
        if (in->pressed[B_A]) {
            char p[1300];
            snprintf(p, sizeof p, "%s/%s", cwd, ents[bsel].name);
            if (unlink(p) == 0) {
                sync();
                toast("Deleted");
                int keep = bsel;
                browse_load(cwd, NULL);
                bsel = keep < nents ? keep : nents - 1;
                if (bsel < 0) bsel = 0;
            } else {
                toast("Couldn't delete this file");
            }
            confirm_delete = false;
        } else if (in->pressed[B_B]) {
            confirm_delete = false;
        }
        return;
    }
    if (in->pressed[B_B]) { browse_up(); return; }
    if (in->pressed[B_DOWN] && bsel < nents - 1) bsel++;
    if (in->pressed[B_UP] && bsel > 0) bsel--;
    if (in->pressed[B_R] || in->pressed[B_RIGHT]) bsel = bsel + ROWS < nents ? bsel + ROWS : (nents ? nents - 1 : 0);
    if (in->pressed[B_L] || in->pressed[B_LEFT]) bsel = bsel > ROWS ? bsel - ROWS : 0;
    if (!nents) return;
    Entry *e = &ents[bsel];
    if (in->pressed[B_X] && cwd[0] && !e->dir) { confirm_delete = true; return; }
    if (!in->pressed[B_A]) return;
    if (!cwd[0]) {
        mkdirs(REC_DIR);
        browse_load(root_path(e->name), NULL);
        return;
    }
    char p[1300];
    snprintf(p, sizeof p, "%s/%s", cwd, e->name);
    if (e->dir) {
        browse_load(p, NULL);
    } else if (!strcasecmp(ext_of(e->name), "mov")) {
        if (play_open(p, e->name)) scr = R_PLAY;
        else toast("This video can't be played here (only RaineyUI recordings)");
    } else if (is_image(e->name)) {
        if (view_tex) SDL_DestroyTexture(view_tex);
        view_tex = gfx_load_abs(p);
        if (view_tex) {
            snprintf(open_name, sizeof open_name, "%s", e->name);
            scr = R_VIEW;
        } else {
            toast("Can't open this picture");
        }
    } else {
        toast("Can't open this type of file");
    }
}

static void play_draw(void) {
    gfx_fill(0, 0, SCREEN_W, SCREEN_H, 0x000000, 255);
    Uint32 now = SDL_GetTicks();
    if (!paused) play_ms += now - last_tick;
    last_tick = now;
    if (play_ms >= mi.duration_ms) {
        play_ms = mi.duration_ms;
        paused = true;
        overlay_until = now + 100000;
    }
    show_frame(frame_at(play_ms));
    if (frame_tex) gfx_image_fit(frame_tex, 0, 0, SCREEN_W, SCREEN_H, 255);
    if (paused || now < overlay_until) {
        gfx_fill(0, SCREEN_H - 110, SCREEN_W, 110, 0x000000, 170);
        gfx_text_ellipsis(F_BODY, open_name, 28, SCREEN_H - 100, SCREEN_W - 56, C_TEXT, 255);
        char a[16], b[16], t[48];
        fmt_time(a, sizeof a, play_ms);
        fmt_time(b, sizeof b, mi.duration_ms);
        snprintf(t, sizeof t, "%s / %s%s", a, b, paused ? "   ❚❚ paused" : "");
        gfx_text(F_SMALL, t, 28, SCREEN_H - 64, C_MUTED, 255);
        int bw = SCREEN_W - 56;
        gfx_round_fill(28, SCREEN_H - 30, bw, 8, 4, 0x2A3350, 255);
        int fw = mi.duration_ms ? (int)(bw * play_ms / mi.duration_ms) : 0;
        if (fw > 8) gfx_round_fill(28, SCREEN_H - 30, fw, 8, 4, C_NEON_BLUE, 255);
        static const char *h = "A pause   ←→ 5 s   L/R 30 s   B back";
        gfx_text(F_SMALL, h, SCREEN_W - 28 - gfx_text_w(F_SMALL, h), SCREEN_H - 64, C_MUTED, 255);
    }
}

static void play_input(InputState *in) {
    if (in->pressed[B_B] || in->quit) {
        play_close();
        scr = R_BROWSE;
        return;
    }
    double step = in->pressed[B_RIGHT] ? 5000 : in->pressed[B_LEFT] ? -5000 : in->pressed[B_R] ? 30000
                : in->pressed[B_L] ? -30000 : 0;
    if (step) {
        play_ms += step;
        if (play_ms < 0) play_ms = 0;
        if (play_ms > mi.duration_ms) play_ms = mi.duration_ms;
        overlay_until = SDL_GetTicks() + 2500;
    }
    if (in->pressed[B_A]) {
        if (play_ms >= mi.duration_ms) play_ms = 0;
        paused = !paused;
        overlay_until = SDL_GetTicks() + 2500;
    }
}

static void view_draw(void) {
    gfx_fill(0, 0, SCREEN_W, SCREEN_H, 0x000000, 255);
    if (view_tex) gfx_image_fit(view_tex, 0, 0, SCREEN_W, SCREEN_H - 48, 255);
    const char *h[] = {"B", "Back"};
    ui_hints(h, 1);
    gfx_text_ellipsis(F_SMALL, open_name, 200, SCREEN_H - 36, SCREEN_W - 230, C_MUTED, 255);
}

int recorder_app_main(void) {
    rec_config_load(&cfg);
    /* the switch reflects the service; turn it back on if it was enabled but isn't running */
    if (cfg.enabled && !service_pid()) service_set(true);
    bg_scan();
    bg_select_name(g_settings.background);
    Uint32 t0 = SDL_GetTicks();
    while (!want_quit) {
        InputState in;
        input_poll(&in);
        if (in.quit) break;
        switch (scr) {
        case R_HOME: home_input(&in); break;
        case R_BROWSE: browse_input(&in); break;
        case R_PLAY: play_input(&in); break;
        case R_VIEW:
            if (in.pressed[B_B] || in.pressed[B_A] || in.quit) scr = R_BROWSE;
            break;
        }
        if (scr != R_PLAY && scr != R_VIEW) gfx_background((SDL_GetTicks() - t0) / 1000.0f);
        switch (scr) {
        case R_HOME: home_draw(); break;
        case R_BROWSE: browse_draw(); break;
        case R_PLAY: play_draw(); break;
        case R_VIEW: view_draw(); break;
        }
        draw_toast();
        SDL_RenderPresent(g_ren);
        if (g_shot_request[0]) {
            gfx_screenshot(g_shot_request);
            g_shot_request[0] = 0;
        }
    }
    play_close();
    bg_shutdown();
    return 0;
}
