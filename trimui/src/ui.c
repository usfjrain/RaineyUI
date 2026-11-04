/* RaineyUI frontend: Home carousel, Library, Detail, Collections, Settings. */
#include "rainey.h"
#include "leds.h"
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

extern char g_shot_request[256];

typedef struct {
    char *path;
    char *name;
    char *folder; /* stock folder name, e.g. SFC (for Imgs lookup) */
    int sys;
    bool art_checked;
    char *art_path;
    SDL_Texture *art;
    uint32_t art_used;
    long long size;
    bool incomplete; /* disc index whose track files are missing */
} Game;

typedef enum { S_SPLASH, S_HOME, S_LIBRARY, S_DETAIL, S_COLLECTIONS, S_SETTINGS, S_LEDS, S_WIFI } Screen;

static Game *games;
static int ngames, cap_games;
static int *sys_games[16];
static int sys_count[16];
static SDL_Texture *sys_art[16];
static SDL_Texture *tex_logo, *tex_library, *tex_settings, *tex_boot;

static char **favs;
static int nfavs;
typedef struct {
    char *path;
    long long ts;
} Recent;
static Recent recents[24];
static int nrecents;

/* ui state */
static Screen screen = S_SPLASH;
static int home_focus;          /* system index */
static int home_row;            /* 0 carousel, -1 top buttons, 1 recent row */
static int home_top;            /* 0 library, 1 settings */
static int home_recent;
static float carousel_x;
static int lib_sys;             /* system shown in library */
static int lib_focus;
static float lib_scroll;
static bool lib_list;
static int detail_game;
static int detail_btn;
static Screen detail_back;
static int coll_tab;            /* 0 favorites, 1 recent */
static int coll_focus;
static float coll_scroll;
static int set_focus;
static char toast[256];
static Uint32 toast_until;
static bool want_exit;
static uint32_t art_clock;

/* ---------------- data ---------------- */

static const char *ext_of(const char *name) {
    const char *d = strrchr(name, '.');
    return d ? d + 1 : "";
}

static bool ext_ok(const SystemDef *s, const char *name) {
    const char *e = ext_of(name);
    if (!*e || strlen(e) > 6) return false;
    char key[16];
    snprintf(key, sizeof key, ",%s,", e);
    for (char *p = key; *p; p++) *p = (char)tolower((unsigned char)*p);
    return strstr(s->exts, key) != NULL;
}

static bool dir_has_ext(const char *dir, const char *ext) {
    DIR *d = opendir(dir);
    if (!d) return false;
    struct dirent *e;
    bool found = false;
    while ((e = readdir(d)))
        if (!strcasecmp(ext_of(e->d_name), ext)) {
            found = true;
            break;
        }
    closedir(d);
    return found;
}

static void add_game(int sys, const char *path, const char *folder) {
    if (ngames == cap_games) {
        cap_games = cap_games ? cap_games * 2 : 256;
        games = realloc(games, sizeof(Game) * cap_games);
    }
    Game *g = &games[ngames++];
    memset(g, 0, sizeof *g);
    g->path = strdup(path);
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    g->name = strdup(base);
    char *dot = strrchr(g->name, '.');
    if (dot && dot != g->name) *dot = 0;
    g->folder = strdup(folder);
    g->sys = sys;
    g->size = -1; /* computed when the detail screen needs it (keeps big SD scans fast) */
    const char *x = ext_of(base);
    if (!strcasecmp(x, "gdi") || !strcasecmp(x, "cue")) {
        /* a .gdi/.cue is only an index: make sure the track files it names are there */
        FILE *f = fopen(path, "r");
        char line[1024], dir[1024], tp[1600];
        snprintf(dir, sizeof dir, "%.*s", (int)(base - path - 1), path);
        while (f && fgets(line, sizeof line, f)) {
            char *q1 = strchr(line, '"'), *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
            char track[512] = "";
            if (q1 && q2) snprintf(track, sizeof track, "%.*s", (int)(q2 - q1 - 1), q1 + 1);
            else if (!strcasecmp(x, "gdi")) {
                int a, b2, c, d2;
                if (sscanf(line, "%d %d %d %d %511s", &a, &b2, &c, &d2, track) != 5) track[0] = 0;
            }
            if (!track[0]) continue;
            snprintf(tp, sizeof tp, "%s/%s", dir, track);
            if (!file_exists(tp)) {
                g->incomplete = true;
                break;
            }
        }
        if (f) fclose(f);
    }
}

static long long game_size(Game *g) {
    if (g->size >= 0) return g->size;
    struct stat st;
    g->size = stat(g->path, &st) == 0 ? st.st_size : 0;
    const char *base = strrchr(g->path, '/');
    base = base ? base + 1 : g->path;
    /* disc index files (.gdi/.cue/.m3u): count the track files that share the name */
    const char *x = ext_of(base);
    if (!strcasecmp(x, "gdi") || !strcasecmp(x, "cue") || !strcasecmp(x, "m3u")) {
        char dir[1024];
        snprintf(dir, sizeof dir, "%.*s", (int)(base - g->path - 1), g->path);
        DIR *d = opendir(dir);
        struct dirent *e;
        size_t nl = strlen(g->name);
        while (d && (e = readdir(d))) {
            if (e->d_name[0] == '.' || !strcmp(e->d_name, base) || strncmp(e->d_name, g->name, nl))
                continue;
            char fp[1400];
            snprintf(fp, sizeof fp, "%s/%s", dir, e->d_name);
            if (stat(fp, &st) == 0 && S_ISREG(st.st_mode)) g->size += st.st_size;
        }
        if (d) closedir(d);
    }
    return g->size;
}

static void scan_dir(int sys, const char *dir, const char *folder, int depth) {
    DIR *d = opendir(dir);
    if (!d) return;
    const SystemDef *s = &SYSTEMS[sys];
    bool has_cue = !strcmp(s->id, "playstation") &&
                   (dir_has_ext(dir, "cue") || dir_has_ext(dir, "m3u"));
    bool has_m3u = (!strcmp(s->id, "playstation") || !strcmp(s->id, "dreamcast")) && dir_has_ext(dir, "m3u");
    bool has_gdi = !strcmp(s->id, "dreamcast") && dir_has_ext(dir, "gdi");
    struct dirent *e;
    char path[1024];
    static const char *skip_dirs[] = {"Imgs", "images", "media", "manuals", "videos", "boxart",
                                      "snap", "titles", "data", "cheats", "saves", "states",
                                      "PPSSPP_STATE", "downloaded_images", NULL};
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        bool isdir;
        if (e->d_type == DT_DIR) isdir = true;
        else if (e->d_type == DT_REG) isdir = false;
        else {
            struct stat st;
            if (stat(path, &st) != 0) continue;
            isdir = S_ISDIR(st.st_mode);
        }
        if (isdir) {
            bool skip = depth >= 2;
            for (int k = 0; skip_dirs[k] && !skip; k++)
                if (!strcasecmp(e->d_name, skip_dirs[k])) skip = true;
            if (!skip) scan_dir(sys, path, folder, depth + 1);
            continue;
        }
        if (!ext_ok(s, e->d_name)) continue;
        const char *x = ext_of(e->d_name);
        if (has_cue && (!strcasecmp(x, "bin") || !strcasecmp(x, "img")) &&
            strcmp(s->id, "genesis"))
            continue;
        if (has_m3u && !strcasecmp(x, "cue")) continue;
        if (has_gdi && !strcasecmp(x, "cue")) continue; /* same disc as the .gdi beside it */
        add_game(sys, path, folder);
    }
    closedir(d);
}

static int cmp_games(const void *a, const void *b) {
    return strcasecmp(games[*(const int *)a].name, games[*(const int *)b].name);
}

static void free_games(void) {
    for (int i = 0; i < ngames; i++) {
        free(games[i].path);
        free(games[i].name);
        free(games[i].folder);
        free(games[i].art_path);
        if (games[i].art) SDL_DestroyTexture(games[i].art);
    }
    ngames = 0;
    for (int s = 0; s < NUM_SYSTEMS; s++) {
        free(sys_games[s]);
        sys_games[s] = NULL;
        sys_count[s] = 0;
    }
}

static void scan_all(void) {
    free_games();
    Uint32 t0 = SDL_GetTicks();
    /* internal storage, SD card in TrimUI layout (Roms/FC…), SD card in EmulationStation /
     * Batocera / Anbernic layout (nes, snes, psx… at the card root). Read-only on the card. */
    const char *roots[] = {ROMS_INTERNAL, ROMS_SDCARD, SDCARD_ROOT};
    struct { dev_t dev; ino_t ino; } seen[256];
    int nseen = 0;
    for (int s = 0; s < NUM_SYSTEMS; s++) {
        for (int r = 0; r < 3; r++)
            for (int f = 0; f < 16 && SYSTEMS[s].folders[f]; f++) {
                char dir[512];
                snprintf(dir, sizeof dir, "%s/%s", roots[r], SYSTEMS[s].folders[f]);
                struct stat st;
                if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
                bool dup = false; /* same folder reached via another name (case-insensitive FAT) */
                for (int k = 0; k < nseen; k++)
                    if (seen[k].dev == st.st_dev && seen[k].ino == st.st_ino) dup = true;
                if (dup) continue;
                if (nseen < 256) {
                    seen[nseen].dev = st.st_dev;
                    seen[nseen].ino = st.st_ino;
                    nseen++;
                }
                scan_dir(s, dir, SYSTEMS[s].folders[f], 0);
            }
    }
    fprintf(stderr, "scan took %u ms\n", SDL_GetTicks() - t0);
    for (int s = 0; s < NUM_SYSTEMS; s++) {
        int n = 0;
        for (int i = 0; i < ngames; i++)
            if (games[i].sys == s) n++;
        sys_games[s] = malloc(sizeof(int) * (n ? n : 1));
        sys_count[s] = 0;
        for (int i = 0; i < ngames; i++)
            if (games[i].sys == s) sys_games[s][sys_count[s]++] = i;
        qsort(sys_games[s], sys_count[s], sizeof(int), cmp_games);
        /* same game on internal storage and the SD card: list it once, prefer internal */
        int w = 0;
        for (int i = 0; i < sys_count[s]; i++) {
            int gi = sys_games[s][i];
            if (w && !strcmp(games[sys_games[s][w - 1]].name, games[gi].name)) {
                Game *prev = &games[sys_games[s][w - 1]], *cur = &games[gi];
                bool better = prev->incomplete && !cur->incomplete;
                if (!better && prev->incomplete == cur->incomplete && !strncmp(cur->path, "/mnt/UDISK", 10))
                    better = true;
                if (better) sys_games[s][w - 1] = gi;
                continue;
            }
            sys_games[s][w++] = gi;
        }
        sys_count[s] = w;
    }
    fprintf(stderr, "scan: %d games\n", ngames);
}

static int game_by_path(const char *p) {
    for (int i = 0; i < ngames; i++)
        if (!strcmp(games[i].path, p)) return i;
    return -1;
}

static void lists_load(void) {
    char line[1200];
    FILE *f = fopen(DATA_DIR "/favorites.txt", "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!*line) continue;
            favs = realloc(favs, sizeof(char *) * (nfavs + 1));
            favs[nfavs++] = strdup(line);
        }
        fclose(f);
    }
    f = fopen(DATA_DIR "/recent.txt", "r");
    if (f) {
        while (nrecents < 24 && fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            char *tab = strchr(line, '\t');
            if (!tab) continue;
            *tab = 0;
            recents[nrecents].ts = atoll(line);
            recents[nrecents].path = strdup(tab + 1);
            nrecents++;
        }
        fclose(f);
    }
}

static void lists_save(void) {
    mkdirs(DATA_DIR);
    FILE *f = fopen(DATA_DIR "/favorites.txt", "w");
    if (f) {
        for (int i = 0; i < nfavs; i++) fprintf(f, "%s\n", favs[i]);
        fclose(f);
    }
    f = fopen(DATA_DIR "/recent.txt", "w");
    if (f) {
        for (int i = 0; i < nrecents; i++) fprintf(f, "%lld\t%s\n", recents[i].ts, recents[i].path);
        fclose(f);
    }
    sync();
}

static int fav_index(const char *p) {
    for (int i = 0; i < nfavs; i++)
        if (!strcmp(favs[i], p)) return i;
    return -1;
}

static void toggle_fav(int gi) {
    int i = fav_index(games[gi].path);
    if (i >= 0) {
        free(favs[i]);
        memmove(&favs[i], &favs[i + 1], sizeof(char *) * (nfavs - i - 1));
        nfavs--;
        snprintf(toast, sizeof toast, "Removed from favorites");
    } else {
        favs = realloc(favs, sizeof(char *) * (nfavs + 1));
        favs[nfavs++] = strdup(games[gi].path);
        snprintf(toast, sizeof toast, "Added to favorites");
    }
    toast_until = SDL_GetTicks() + 1600;
    lists_save();
}

static void push_recent(const char *p) {
    for (int i = 0; i < nrecents; i++)
        if (!strcmp(recents[i].path, p)) {
            free(recents[i].path);
            memmove(&recents[i], &recents[i + 1], sizeof(Recent) * (nrecents - i - 1));
            nrecents--;
            break;
        }
    if (nrecents == 24) {
        free(recents[23].path);
        nrecents--;
    }
    memmove(&recents[1], &recents[0], sizeof(Recent) * nrecents);
    recents[0].path = strdup(p);
    recents[0].ts = (long long)time(NULL);
    nrecents++;
    lists_save();
}

/* collection items resolved against the current scan */
static int coll_items(int tab, int *out, int max) {
    int n = 0;
    if (tab == 0) {
        for (int i = 0; i < nfavs && n < max; i++) {
            int g = game_by_path(favs[i]);
            if (g >= 0) out[n++] = g;
        }
    } else {
        for (int i = 0; i < nrecents && n < max; i++) {
            int g = game_by_path(recents[i].path);
            if (g >= 0) out[n++] = g;
        }
    }
    return n;
}

/* ---------------- art ---------------- */

static void art_evict(void) {
    int loaded = 0, oldest = -1;
    for (int i = 0; i < ngames; i++)
        if (games[i].art) {
            loaded++;
            if (oldest < 0 || games[i].art_used < games[oldest].art_used) oldest = i;
        }
    if (loaded > 64 && oldest >= 0) {
        SDL_DestroyTexture(games[oldest].art);
        games[oldest].art = NULL;
    }
}

static int art_loads_this_frame;

static SDL_Texture *game_art(int gi) {
    Game *g = &games[gi];
    g->art_used = ++art_clock;
    if (g->art) return g->art;
    if (!g->art_checked) {
        g->art_checked = true;
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", g->path);
        char *sl = strrchr(dir, '/');
        if (sl) *sl = 0;
        char parent[1024];
        snprintf(parent, sizeof parent, "%s", dir);
        char *ps = strrchr(parent, '/');
        if (ps) *ps = 0;
        const char *exts[] = {"png", "jpg"};
        char cand[1400];
        for (int e = 0; e < 2 && !g->art_path; e++) {
            /* %1 = dir, folder or parent; %2 = game name; %3 = ext */
            struct { const char *fmt; int base; } tries[] = {
                {"%s/Imgs/%s.%s", 0},          {"/mnt/UDISK/Imgs/%s/%s.%s", 1},
                {"/mnt/SDCARD/Imgs/%s/%s.%s", 1}, {"%s/images/%s.%s", 0},
                {"%s/images/%s-image.%s", 0},  {"%s/boxart/%s.%s", 0},
                {"%s/images/%s.%s", 2},        {"%s/images/%s-image.%s", 2},
                {"%s/%s.%s", 0}};
            for (size_t t = 0; t < sizeof tries / sizeof *tries; t++) {
                const char *a = tries[t].base == 1 ? g->folder : tries[t].base == 2 ? parent : dir;
                snprintf(cand, sizeof cand, tries[t].fmt, a, g->name, exts[e]);
                if (file_exists(cand)) {
                    g->art_path = strdup(cand);
                    break;
                }
            }
        }
    }
    if (g->art_path && art_loads_this_frame < 2) {
        art_loads_this_frame++;
        g->art = gfx_load_abs(g->art_path);
        if (!g->art) {
            free(g->art_path);
            g->art_path = NULL;
        }
        art_evict();
    }
    return g->art ? g->art : sys_art[g->sys];
}

/* ---------------- drawing pieces ---------------- */

void ui_hints(const char *const *pairs, int n) {
    int x = 28, y = SCREEN_H - 38;
    gfx_fill(0, SCREEN_H - 48, SCREEN_W, 48, 0x050812, 170);
    gfx_fill(0, SCREEN_H - 48, SCREEN_W, 1, C_NEON_PURPLE, 90);
    for (int i = 0; i < n; i++) {
        const char *k = pairs[i * 2], *v = pairs[i * 2 + 1];
        int kw = gfx_text_w(F_SMALL, k) + 16;
        gfx_round_fill(x, y - 2, kw, 28, 6, C_NEON_BLUE, 40);
        gfx_round_ring(x, y - 2, kw, 28, 6, 1, C_NEON_BLUE, 160);
        gfx_text(F_SMALL, k, x + 8, y + 2, C_TEXT, 255);
        x += kw + 8;
        x += gfx_text(F_SMALL, v, x, y + 2, C_MUTED, 255) + 22;
    }
}

void ui_toast(const char *m) {
    snprintf(toast, sizeof toast, "%s", m);
    toast_until = SDL_GetTicks() + 1800;
}

static void draw_toast(void) {
    if (!toast[0] || SDL_TICKS_PASSED(SDL_GetTicks(), toast_until)) return;
    int w = gfx_text_w(F_BODY, toast) + 40;
    int x = (SCREEN_W - w) / 2, y = SCREEN_H - 110;
    gfx_round_fill(x, y, w, 46, 6, C_PANEL, 235);
    gfx_round_ring(x, y, w, 46, 6, 1, C_NEON_BLUE, 185);
    gfx_text_center(F_BODY, toast, SCREEN_W / 2, y + 10, C_TEXT, 255);
}

static void draw_card(int x, int y, int w, int h, int art_h, SDL_Texture *art, const char *title,
                      const char *sub, bool focused, float glow) {
    if (focused) gfx_glow(x, y, w, h, 10, C_NEON_BLUE, glow);
    gfx_round_fill(x, y, w, h, 10, 0x080D1D, 235);
    gfx_round_ring(x, y, w, h, 10, focused ? 4 : 2, focused ? C_NEON_BLUE : C_NEON_PURPLE,
                   focused ? 255 : 120);
    int pad = focused ? 14 : 12;
    if (art) {
        gfx_image_cover(art, x + pad, y + pad, w - pad * 2, art_h, 255);
        gfx_round_ring(x + pad - 1, y + pad - 1, w - pad * 2 + 2, art_h + 2, 8, 2,
                       focused ? C_NEON_BLUE : 0x2A3350, focused ? 220 : 200);
    }
    int ty = y + pad + art_h + 12;
    int lines = gfx_text_wrap(F_LABEL, title, x + pad, ty, w - pad * 2, 2, 32, C_TEXT, 255);
    if (sub) gfx_text(F_SMALL, sub, x + pad, ty + lines * 32 + 2, C_MUTED, 255);
}

static void draw_toggle(int x, int y, bool on, bool focused) {
    int w = 78, h = 44;
    gfx_round_fill(x, y, w, h, 22, on ? C_NEON_BLUE : 0x2A3350, 255);
    if (on) /* striped like the Android switch */
        for (int i = 6; i < h - 6; i += 4) gfx_fill(x + 12, y + i, w - 24, 1, 0x0B7EA0, 255);
    if (focused) gfx_round_ring(x - 4, y - 4, w + 8, h + 8, 26, 2, C_NEON_PINK, 255);
    int cx = on ? x + w - 40 : x + 4;
    gfx_round_fill(cx, y + 4, 36, 36, 18, on ? 0x000000 : 0xA7B0C0, 255);
}

/* ---------------- screens ---------------- */

static void home_draw(float t) {
    /* header */
    if (tex_logo) gfx_image_fit(tex_logo, 28, 14, 76, 76, 255);
    gfx_text(F_BODY, "Select a system", 28, 98, C_MUTED, 255);
    bool top = home_row == -1;
    int sx = SCREEN_W - 28 - 82, lx = sx - 14 - 82;
    if (top && home_top == 0) gfx_glow(lx, 12, 82, 82, 8, C_NEON_BLUE, 1);
    if (top && home_top == 1) gfx_glow(sx, 12, 82, 82, 8, C_NEON_BLUE, 1);
    if (tex_library) gfx_image_cover(tex_library, lx, 12, 82, 82, 255);
    if (tex_settings) gfx_image_cover(tex_settings, sx, 12, 82, 82, 255);
    if (top) {
        int fx = home_top == 0 ? lx : sx, fw = 82;
        gfx_round_ring(fx - 3, 9, fw + 6, 88, 9, 3, C_NEON_BLUE, 255);
    }

    /* carousel */
    const int cw = 300, ch = 300, gap = 30, cy = 150;
    float target = home_focus * (cw + gap);
    float maxs = NUM_SYSTEMS * (cw + gap) - gap - (SCREEN_W - 56);
    if (target > maxs) target = maxs;
    if (target < 0) target = 0;
    carousel_x += (target - carousel_x) * 0.22f;
    for (int i = 0; i < NUM_SYSTEMS; i++) {
        bool f = i == home_focus && home_row == 0;
        float x = 28 + i * (cw + gap) - carousel_x;
        if (x > SCREEN_W || x + cw < 0) continue;
        int grow = f ? 14 : 0;
        char sub[48];
        snprintf(sub, sizeof sub, "%d game%s", sys_count[i], sys_count[i] == 1 ? "" : "s");
        draw_card((int)x - grow / 2, cy - grow / 2, cw + grow, ch + grow, 186 + grow,
                  sys_art[i], SYSTEMS[i].name, sub, f, 0.75f + 0.25f * sinf(t * 3));
    }

    /* recently played row */
    int ids[24];
    int n = coll_items(1, ids, 24);
    if (n > 4) n = 4;
    int ry = 490;
    gfx_text(F_LABEL, "Recently Played", 28, ry, C_NEON_BLUE, 255);
    if (!n) {
        gfx_text(F_SMALL, "Games you play will show up here.", 28, ry + 40, C_MUTED, 255);
    }
    for (int i = 0; i < n; i++) {
        bool f = home_row == 1 && home_recent == i;
        int x = 28 + i * 246, y = ry + 38, w = 230, h = 168;
        if (f) gfx_glow(x, y, w, h, 8, C_NEON_BLUE, 1);
        gfx_round_fill(x, y, w, h, 8, 0x080D1D, 230);
        gfx_image_cover(game_art(ids[i]), x + 8, y + 8, w - 16, 112, 255);
        gfx_round_ring(x, y, w, h, 8, f ? 3 : 1, f ? C_NEON_BLUE : C_NEON_PURPLE, f ? 255 : 120);
        gfx_text_ellipsis(F_SMALL, games[ids[i]].name, x + 10, y + 130, w - 20, C_TEXT, 255);
    }
    static const char *h[] = {"A", "Open", "←→", "System", "↑", "Library/Settings", "START",
                              "Settings"};
    ui_hints(h, 4);
}

static void home_input(InputState *in) {
    int ids[24];
    int nrec = coll_items(1, ids, 4);
    if (in->pressed[B_START]) {
        bg_scan();
        screen = S_SETTINGS;
        set_focus = 0;
        return;
    }
    if (home_row == -1) {
        if (in->pressed[B_LEFT]) home_top = 0;
        if (in->pressed[B_RIGHT]) home_top = 1;
        if (in->pressed[B_DOWN] || in->pressed[B_B]) home_row = 0;
        if (in->pressed[B_A]) {
            if (home_top == 0) {
                screen = S_COLLECTIONS;
                coll_focus = 0;
            } else {
                bg_scan();
                screen = S_SETTINGS;
                set_focus = 0;
            }
        }
        return;
    }
    if (home_row == 1) {
        if (in->pressed[B_LEFT] && home_recent > 0) home_recent--;
        if (in->pressed[B_RIGHT] && home_recent < nrec - 1) home_recent++;
        if (in->pressed[B_UP] || in->pressed[B_B]) home_row = 0;
        if (in->pressed[B_A] && home_recent < nrec) {
            detail_game = ids[home_recent];
            detail_btn = 0;
            detail_back = S_HOME;
            screen = S_DETAIL;
        }
        return;
    }
    if (in->pressed[B_LEFT] || in->pressed[B_L]) home_focus = (home_focus + NUM_SYSTEMS - 1) % NUM_SYSTEMS;
    if (in->pressed[B_RIGHT] || in->pressed[B_R]) home_focus = (home_focus + 1) % NUM_SYSTEMS;
    if (in->pressed[B_UP]) home_row = -1;
    if (in->pressed[B_DOWN] && nrec > 0) {
        home_row = 1;
        if (home_recent >= nrec) home_recent = 0;
    }
    if (in->pressed[B_A]) {
        lib_sys = home_focus;
        lib_focus = 0;
        lib_scroll = 0;
        screen = S_LIBRARY;
    }
}

static void empty_panel(const char *title, const char *body, int y) {
    gfx_round_fill(28, y, SCREEN_W - 56, 200, 8, C_PANEL, 220);
    gfx_round_ring(28, y, SCREEN_W - 56, 200, 8, 1, C_NEON_BLUE, 185);
    gfx_text(F_TITLE, title, 52, y + 24, C_TEXT, 255);
    gfx_text_wrap(F_BODY, body, 52, y + 80, SCREEN_W - 104, 4, 30, C_MUTED, 255);
}

/* shared grid/list renderer for a set of game ids */
static void draw_game_set(const int *ids, int n, int focus, float *scroll, bool list, int top) {
    if (list) {
        const int rh = 58;
        int view = SCREEN_H - 60 - top;
        float target = focus * rh - view / 2 + rh / 2;
        float maxs = n * rh - view;
        if (target > maxs) target = maxs;
        if (target < 0) target = 0;
        *scroll += (target - *scroll) * 0.3f;
        SDL_Rect clip = {0, top, SCREEN_W, view};
        SDL_RenderSetClipRect(g_ren, &clip);
        for (int i = 0; i < n; i++) {
            int y = top + i * rh - (int)*scroll;
            if (y + rh < top || y > top + view) continue;
            bool f = i == focus;
            if (f) gfx_glow(28, y, SCREEN_W - 56, rh - 8, 6, C_NEON_BLUE, 0.8f);
            gfx_round_fill(28, y, SCREEN_W - 56, rh - 8, 6, f ? 0x0B2A3A : C_PANEL, f ? 240 : 190);
            gfx_round_ring(28, y, SCREEN_W - 56, rh - 8, 6, f ? 2 : 1, f ? C_NEON_BLUE : C_NEON_PURPLE,
                           f ? 255 : 90);
            gfx_image_cover(game_art(ids[i]), 36, y + 5, 70, 40, 255);
            if (fav_index(games[ids[i]].path) >= 0) gfx_text(F_BODY, "★", 118, y + 11, C_NEON_PINK, 255);
            if (games[ids[i]].incomplete) gfx_text(F_BODY, "⚠", 118, y + 11, 0xFFB000, 255);
            gfx_text_ellipsis(F_BODY, games[ids[i]].name, 146, y + 11, SCREEN_W - 340, C_TEXT, 255);
            const char *sn = SYSTEMS[games[ids[i]].sys].name;
            gfx_text(F_SMALL, sn, SCREEN_W - 44 - gfx_text_w(F_SMALL, sn), y + 14, C_MUTED, 255);
        }
        SDL_RenderSetClipRect(g_ren, NULL);
        return;
    }
    const int cols = 4, gap = 16, cw = (SCREEN_W - 56 - gap * (cols - 1)) / cols, ch = 290;
    int view = SCREEN_H - 52 - top;
    int row = focus / cols;
    float target = row * (ch + gap) - (view - ch) / 2;
    int rows = (n + cols - 1) / cols;
    float maxs = rows * (ch + gap) - view;
    if (target > maxs) target = maxs;
    if (target < 0) target = 0;
    *scroll += (target - *scroll) * 0.25f;
    SDL_Rect clip = {0, top - 14, SCREEN_W, view + 14};
    SDL_RenderSetClipRect(g_ren, &clip);
    for (int i = 0; i < n; i++) {
        int x = 28 + (i % cols) * (cw + gap);
        int y = top + (i / cols) * (ch + gap) - (int)*scroll;
        if (y + ch < top - 14 || y > top + view) continue;
        bool f = i == focus;
        draw_card(x, y, cw, ch, 176, game_art(ids[i]), games[ids[i]].name, NULL, f, 1.0f);
        if (fav_index(games[ids[i]].path) >= 0)
            gfx_text(F_TITLE, "★", x + cw - 40, y + 14, C_NEON_PINK, 255);
        if (games[ids[i]].incomplete) {
            gfx_round_fill(x + 16, y + 16, 150, 30, 6, 0x351023, 235);
            gfx_text(F_SMALL, "⚠ incomplete", x + 24, y + 20, 0xFFC7D8, 255);
        }
    }
    SDL_RenderSetClipRect(g_ren, NULL);
}

static void library_draw(void) {
    const SystemDef *s = &SYSTEMS[lib_sys];
    gfx_text(F_HUGE, s->name, 28, 18, C_TEXT, 255);
    char sub[64];
    snprintf(sub, sizeof sub, "%d game%s", sys_count[lib_sys], sys_count[lib_sys] == 1 ? "" : "s");
    gfx_text(F_BODY, sub, 28, 76, C_MUTED, 255);
    int bx = SCREEN_W - 28;
    const char *labels[] = {"B Back", lib_list ? "X Grid" : "X List", "Y Favorite"};
    for (int i = 0; i < 3; i++) {
        int w = gfx_text_w(F_LABEL, labels[i]) + 36;
        bx -= w;
        gfx_neon_button(bx, 24, w, 56, labels[i], false);
        bx -= 12;
    }
    char all[64];
    snprintf(all, sizeof all, "All %s Games", s->name);
    gfx_text(F_LABEL, all, 28, 112, C_NEON_BLUE, 255);
    if (!sys_count[lib_sys]) {
        char body[400];
        snprintf(body, sizeof body,
                 "Copy %s games into %s/%s on internal storage (or Roms/%s on an SD card), "
                 "then press SELECT to rescan.",
                 s->name, ROMS_INTERNAL, s->folders[0], s->folders[0]);
        empty_panel("No games yet", body, 170);
    } else {
        draw_game_set(sys_games[lib_sys], sys_count[lib_sys], lib_focus, &lib_scroll, lib_list, 160);
    }
    static const char *h[] = {"A", "Details", "B", "Back", "X", "Grid/List", "Y", "Favorite",
                              "SELECT", "Rescan", "L/R", "Page"};
    ui_hints(h, 6);
}

static void set_nav(InputState *in, int *focus, int n, int cols) {
    if (!n) return;
    int step = cols;
    if (in->pressed[B_RIGHT]) *focus += cols > 1 ? 1 : 0;
    if (in->pressed[B_LEFT]) *focus -= cols > 1 ? 1 : 0;
    if (in->pressed[B_DOWN]) *focus += step;
    if (in->pressed[B_UP]) *focus -= step;
    if (in->pressed[B_R]) *focus += cols > 1 ? cols * 2 : 10;
    if (in->pressed[B_L]) *focus -= cols > 1 ? cols * 2 : 10;
    if (*focus >= n) *focus = in->pressed[B_DOWN] || in->pressed[B_R] || in->pressed[B_RIGHT] ? n - 1 : n - 1;
    if (*focus < 0) *focus = 0;
}

static void rescan(void) {
    char keep[1024] = "";
    if (detail_game >= 0 && detail_game < ngames) snprintf(keep, sizeof keep, "%s", games[detail_game].path);
    scan_all();
    detail_game = keep[0] ? game_by_path(keep) : -1;
    if (detail_game < 0) detail_game = 0;
    snprintf(toast, sizeof toast, "Found %d game%s", ngames, ngames == 1 ? "" : "s");
    toast_until = SDL_GetTicks() + 1800;
}

static void library_input(InputState *in) {
    int n = sys_count[lib_sys];
    if (in->pressed[B_B]) {
        screen = S_HOME;
        return;
    }
    if (in->pressed[B_X]) lib_list = !lib_list;
    if (in->pressed[B_SELECT]) {
        rescan();
        n = sys_count[lib_sys];
        if (lib_focus >= n) lib_focus = n ? n - 1 : 0;
    }
    set_nav(in, &lib_focus, n, lib_list ? 1 : 4);
    if (!n) return;
    int gi = sys_games[lib_sys][lib_focus];
    if (in->pressed[B_Y]) toggle_fav(gi);
    if (in->pressed[B_A]) {
        detail_game = gi;
        detail_btn = 0;
        detail_back = S_LIBRARY;
        screen = S_DETAIL;
    }
}

static void fmt_size(long long b, char *out, size_t n) {
    if (b >= 1 << 30) snprintf(out, n, "%.2f GB", b / 1073741824.0);
    else if (b >= 1 << 20) snprintf(out, n, "%.1f MB", b / 1048576.0);
    else snprintf(out, n, "%lld KB", b / 1024);
}

static void detail_draw(void) {
    Game *g = &games[detail_game];
    int ax = 28, ay = 120, aw = 340, ah = 470;
    gfx_round_fill(ax, ay, aw, ah, 10, 0x080D1D, 255);
    gfx_image_cover(game_art(detail_game), ax + 3, ay + 3, aw - 6, ah - 6, 255);
    gfx_round_ring(ax, ay, aw, ah, 10, 3, C_NEON_BLUE, 255);
    int x = 404;
    int lines = gfx_text_wrap(F_HUGE, g->name, x, 116, SCREEN_W - x - 28, 2, 52, C_TEXT, 255);
    int y = 116 + lines * 52 + 10;
    gfx_text(F_LABEL, SYSTEMS[g->sys].name, x, y, C_NEON_BLUE, 255);
    y += 52;
    bool fav = fav_index(g->path) >= 0;
    const char *labels[3] = {"Play", fav ? "Unfavorite" : "Favorite", "Back"};
    int bx = x;
    for (int i = 0; i < 3; i++) {
        int w = gfx_text_w(F_LABEL, labels[i]) + 40;
        gfx_neon_button(bx, y, w, 62, labels[i], detail_btn == i);
        bx += w + 14;
    }
    y += 96;
    char size[32], buf[256];
    fmt_size(game_size(g), size, sizeof size);
    const char *where = !strncmp(g->path, "/mnt/SDCARD", 11) ? "SD card" : "Internal storage";
    long long last = 0;
    for (int i = 0; i < nrecents; i++)
        if (!strcmp(recents[i].path, g->path)) last = recents[i].ts;
    char lastbuf[64] = "Never";
    if (last > 1000000) {
        time_t tt = (time_t)last;
        strftime(lastbuf, sizeof lastbuf, "%Y-%m-%d %H:%M", localtime(&tt));
    }
    const char *slash = strrchr(g->path, '/');
    struct {
        const char *k, *v;
    } rows[] = {{"File:", slash ? slash + 1 : g->path},
                {"Size:", size},
                {"Storage:", where},
                {"Emulator:", SYSTEMS[g->sys].core_label},
                {"Last played:", lastbuf}};
    for (int i = 0; i < 5; i++) {
        int kw = gfx_text(F_BODY, rows[i].k, x, y, C_MUTED, 255);
        gfx_text_ellipsis(F_BODY, rows[i].v, x + kw + 12, y, SCREEN_W - x - kw - 40, C_TEXT, 255);
        y += 44;
    }
    const char *ln = SYSTEMS[g->sys].launcher;
    if (g->incomplete) {
        gfx_round_fill(x - 8, y + 2, SCREEN_W - x - 20, 64, 8, 0x351023, 235);
        gfx_round_ring(x - 8, y + 2, SCREEN_W - x - 20, 64, 8, 1, 0xFF6B9A, 255);
        gfx_text(F_BODY, "⚠ Track files are missing next to this disc", x + 4, y + 8, 0xFFC7D8, 255);
        gfx_text(F_SMALL, "Copy the full game (index + all Track .bin files) to play it.", x + 4, y + 38,
                 C_TEXT, 255);
        ui_hints((const char *const[]){"B", "Back", "Y", "Favorite"}, 2);
        return;
    }
    snprintf(buf, sizeof buf, "%s",
             !strcmp(ln, "retroarch") ? "In game: MENU = TrimUI menu (save/load/exit), MENU+START quits."
             : !strcmp(ln, "drastic") ? "In game: MENU opens the DraStic menu (Exit to return)."
                                      : "In game: MENU or SELECT+START opens Rainey Options.");
    gfx_text(F_SMALL, buf, x, y + 8, C_MUTED, 255);
    static const char *h[] = {"A", "Select", "B", "Back", "Y", "Favorite", "←→", "Move"};
    ui_hints(h, 4);
}

static void ui_save_state(void) {
    FILE *f = fopen(UI_STATE_FILE, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d %d %d %d %d\n%s\n", (int)screen, home_focus, lib_sys, lib_focus,
            lib_list, (int)detail_back, coll_tab, coll_focus,
            detail_game >= 0 && detail_game < ngames ? games[detail_game].path : "");
    fclose(f);
}

static bool ui_load_state(void) {
    FILE *f = fopen(UI_STATE_FILE, "r");
    if (!f) return false;
    int sc, hf, ls, lf, ll, db, ct, cf;
    char path[1024] = "";
    bool ok = fscanf(f, "%d %d %d %d %d %d %d %d\n", &sc, &hf, &ls, &lf, &ll, &db, &ct, &cf) == 8;
    if (ok && fgets(path, sizeof path, f)) path[strcspn(path, "\n")] = 0;
    fclose(f);
    if (!ok) return false;
    home_focus = hf % NUM_SYSTEMS;
    lib_sys = ls % NUM_SYSTEMS;
    lib_focus = lf;
    lib_list = ll;
    detail_back = (Screen)db;
    coll_tab = ct;
    coll_focus = cf;
    screen = (Screen)sc;
    if (screen == S_SPLASH) screen = S_HOME;
    if (lib_focus >= sys_count[lib_sys]) lib_focus = 0;
    detail_game = path[0] ? game_by_path(path) : -1;
    if (screen == S_DETAIL && detail_game < 0) screen = S_HOME;
    carousel_x = home_focus * 330;
    return true;
}

static void launch(int gi) {
    Game *g = &games[gi];
    if (g->incomplete) {
        snprintf(toast, sizeof toast, "Can't play: this disc's track files are missing");
        toast_until = SDL_GetTicks() + 2500;
        return;
    }
    const SystemDef *s = &SYSTEMS[g->sys];
    char core[700];
    snprintf(core, sizeof core, "%s/%s", g_appdir, s->core);
    if (!file_exists(core)) {
        snprintf(toast, sizeof toast, "Emulator missing: %s", s->core_label);
        toast_until = SDL_GetTicks() + 2500;
        return;
    }
    push_recent(g->path);
    FILE *f = fopen(LAUNCH_FILE, "w");
    if (!f) return;
    fprintf(f, "%s\n%s\n%s\n%s\n%s\n", s->id, core, g->path, g->name, s->launcher);
    fclose(f);
    ui_save_state();
    want_exit = true;
}

static void detail_input(InputState *in) {
    if (in->pressed[B_LEFT] && detail_btn > 0) detail_btn--;
    if (in->pressed[B_RIGHT] && detail_btn < 2) detail_btn++;
    if (in->pressed[B_Y]) toggle_fav(detail_game);
    if (in->pressed[B_B]) {
        screen = detail_back;
        return;
    }
    if (in->pressed[B_A]) {
        if (detail_btn == 0) launch(detail_game);
        else if (detail_btn == 1) toggle_fav(detail_game);
        else screen = detail_back;
    }
}

static int coll_ids[2048];

static void collections_draw(void) {
    gfx_text(F_HUGE, "Library", 28, 18, C_TEXT, 255);
    gfx_text(F_BODY, "Favorites and recently played across all systems", 28, 76, C_MUTED, 255);
    const char *tabs[] = {"Favorites", "Recently Played"};
    int x = 28;
    for (int i = 0; i < 2; i++) {
        int w = gfx_text_w(F_LABEL, tabs[i]) + 40;
        gfx_neon_button(x, 112, w, 52, tabs[i], coll_tab == i);
        x += w + 14;
    }
    gfx_text(F_SMALL, "L / R switch tabs", x + 10, 128, C_MUTED, 255);
    int n = coll_items(coll_tab, coll_ids, 2048);
    if (!n) {
        empty_panel(coll_tab == 0 ? "No favorites yet" : "Nothing played yet",
                    coll_tab == 0 ? "Press Y on any game to add it to your favorites."
                                  : "Pick a system on the home screen and start a game.",
                    190);
    } else {
        if (coll_focus >= n) coll_focus = n - 1;
        draw_game_set(coll_ids, n, coll_focus, &coll_scroll, true, 186);
    }
    static const char *h[] = {"A", "Details", "B", "Back", "Y", "Favorite", "L/R", "Tab"};
    ui_hints(h, 4);
}

static void collections_input(InputState *in) {
    if (in->pressed[B_B]) {
        screen = S_HOME;
        return;
    }
    if (in->pressed[B_L] || in->pressed[B_R]) {
        coll_tab = !coll_tab;
        coll_focus = 0;
        coll_scroll = 0;
        return;
    }
    int n = coll_items(coll_tab, coll_ids, 2048);
    if (in->pressed[B_DOWN] && coll_focus < n - 1) coll_focus++;
    if (in->pressed[B_UP] && coll_focus > 0) coll_focus--;
    if (!n) return;
    if (in->pressed[B_Y]) {
        toggle_fav(coll_ids[coll_focus]);
        n = coll_items(coll_tab, coll_ids, 2048);
        if (coll_focus >= n) coll_focus = n ? n - 1 : 0;
    }
    if (in->pressed[B_A] && n) {
        detail_game = coll_ids[coll_focus];
        detail_btn = 0;
        detail_back = S_COLLECTIONS;
        screen = S_DETAIL;
    }
}

enum { SET_BG, SET_BG_DIM, SET_SCANLINES, SET_GAME_SCANLINES, SET_LAYOUT, SET_SCALE, SET_SMOOTH, SET_LEDS, SET_WIFI, SET_RESCAN, SET_EXIT, SET_N };
static const char *scale_names[] = {"Auto", "1x", "2x", "3x", "4x", "Stretch"};

static void settings_draw(void) {
    gfx_text(F_HUGE, "Settings", 28, 18, C_TEXT, 255);
    gfx_text(F_BODY, "Display, controls and ROM folders", 28, 76, C_MUTED, 255);
    gfx_neon_button(SCREEN_W - 28 - 120, 24, 120, 56, "B Back", false);
    static float set_scroll;
    {
        int yy = 0, fy = 0;
        for (int i = 0; i < SET_N; i++) {
            if (i == SET_WIFI) yy += 40;
            if (i == set_focus) fy = yy;
            yy += 54 + (i == SET_BG && set_focus == SET_BG ? 22 : 0);
        }
        float target = fy + 54 - (712 - 118);
        if (target < 0) target = 0;
        set_scroll += (target - set_scroll) * 0.3f;
    }
    SDL_Rect sclip = {0, 108, SCREEN_W, 720 - 108};
    SDL_RenderSetClipRect(g_ren, &sclip);
    int y = 118 - (int)set_scroll;
    const char *labels[SET_N] = {"Background", "Background brightness",
                                 "Scanlines (menus)", "Scanlines (in game)", "Button layout",
                                 "Video scale", "Smooth video", "LED lights", "Wi-Fi transfer", "Rescan games",
                                 "Exit RaineyUI"};
    for (int i = 0; i < SET_N; i++) {
        bool f = set_focus == i;
        int h = 50;
        if (i == SET_WIFI) {
            y += 4;
            gfx_text(F_LABEL, "Library", 28, y, C_NEON_BLUE, 255);
            y += 36;
        }
        int hh = h + 2 + (i == SET_BG ? 22 : 0);
        if (f) gfx_round_fill(20, y - 4, SCREEN_W - 40, hh, 8, C_NEON_BLUE, 26);
        if (f) gfx_round_ring(20, y - 4, SCREEN_W - 40, hh, 8, 1, C_NEON_BLUE, 140);
        gfx_text(F_LABEL, labels[i], 36, y + 9, C_TEXT, 255);
        int rx = SCREEN_W - 48;
        switch (i) {
        case SET_SCANLINES: draw_toggle(rx - 78, y + 1, g_settings.scanlines, f); break;
        case SET_GAME_SCANLINES: draw_toggle(rx - 78, y + 1, g_settings.game_scanlines, f); break;
        case SET_SMOOTH: draw_toggle(rx - 78, y + 1, g_settings.smooth, f); break;
        case SET_BG: {
            int ci = bg_current();
            char v[140], cnt[24];
            snprintf(v, sizeof v, "‹ %s ›", ci >= 0 ? bg_name(ci) : "?");
            snprintf(cnt, sizeof cnt, "%d/%d", ci + 1, bg_count());
            int vw = gfx_text_w(F_LABEL, v);
            gfx_text(F_LABEL, v, rx - vw, y + 9, f ? C_NEON_BLUE : C_TEXT, 255);
            if (f && bg_is_anim(ci)) gfx_text(F_SMALL, "animated", rx - gfx_text_w(F_SMALL, "animated"), y + 38, C_NEON_PINK, 255);
            int cw = gfx_text_w(F_SMALL, cnt);
            gfx_text(F_SMALL, cnt, rx - vw - 16 - cw, y + 14, C_MUTED, 255);
            if (f)
                gfx_text(F_SMALL, "+ add your own: /mnt/UDISK/RaineyUI/backgrounds", 36, y + 38,
                         C_MUTED, 255);
            break;
        }
        case SET_BG_DIM: {
            const char *o[] = {"Dim", "Normal", "Bright"};
            int x = rx;
            for (int k = 2; k >= 0; k--) {
                int w = gfx_text_w(F_BODY, o[k]) + 28;
                x -= w;
                bool on = g_settings.bg_dim == k;
                gfx_round_fill(x, y + 2, w, 42, 6, on ? C_NEON_BLUE : C_PANEL, on ? 60 : 200);
                gfx_round_ring(x, y + 2, w, 42, 6, 2, on ? C_NEON_BLUE : C_NEON_PURPLE, on ? 255 : 110);
                gfx_text_center(F_BODY, o[k], x + w / 2, y + 10, C_TEXT, 255);
                x -= 8;
            }
            break;
        }
        case SET_LAYOUT: {
            const char *o[] = {"Nintendo", "Xbox"};
            int x = rx;
            for (int k = 1; k >= 0; k--) {
                int w = gfx_text_w(F_LABEL, o[k]) + 32;
                x -= w;
                gfx_neon_button(x, y, w, 46, o[k], g_settings.swap_ab == k);
                x -= 10;
            }
            break;
        }
        case SET_SCALE: {
            int x = rx;
            for (int k = 5; k >= 0; k--) {
                int w = gfx_text_w(F_BODY, scale_names[k]) + 24;
                x -= w;
                bool on = g_settings.scale == k;
                gfx_round_fill(x, y + 2, w, 42, 6, on ? C_NEON_BLUE : C_PANEL, on ? 60 : 200);
                gfx_round_ring(x, y + 2, w, 42, 6, 2, on ? C_NEON_BLUE : C_NEON_PURPLE, on ? 255 : 110);
                gfx_text_center(F_BODY, scale_names[k], x + w / 2, y + 10, C_TEXT, 255);
                x -= 8;
            }
            break;
        }
        case SET_LEDS: {
            char sub[48];
            if (!led_cur.on) snprintf(sub, sizeof sub, "Off");
            else if (led_active_preset >= 0) snprintf(sub, sizeof sub, "Preset %d", led_active_preset + 1);
            else snprintf(sub, sizeof sub, "Custom");
            int dx = rx - 40 - LED_ZONES * 26;
            for (int z = 0; z < LED_ZONES; z++) {
                bool off = !led_cur.on || led_cur.effect[z] == 0;
                gfx_round_fill(dx + z * 26, y + 13, 20, 20, 10, off ? 0x1A1F2E : led_cur.color[z], 255);
            }
            gfx_text(F_BODY, sub, dx - 20 - gfx_text_w(F_BODY, sub), y + 10, C_MUTED, 255);
            gfx_text(F_BODY, "›", rx - 20, y + 10, C_MUTED, 255);
            break;
        }
        case SET_WIFI:
            gfx_text(F_BODY, "Upload from a browser  ›", rx - gfx_text_w(F_BODY, "Upload from a browser  ›"),
                     y + 10, C_MUTED, 255);
            break;
        case SET_RESCAN:
        case SET_EXIT:
            gfx_text(F_BODY, "A", rx - 20, y + 10, C_MUTED, 255);
            break;
        }
        y += h + 4 + (f && i == SET_BG ? 22 : 0);
    }
    SDL_RenderSetClipRect(g_ren, NULL);
    if (0) {
    char info[512];
    snprintf(info, sizeof info,
             "ROMs: %s/<FC SFC MD PS DC ARCADE GB GBA N64 NDS>  (SD: /Roms/…)   BIOS: %s/bios   "
             "Box art: Imgs/<folder>/<game>.png",
             ROMS_INTERNAL, DATA_DIR);
    gfx_text_wrap(F_SMALL, info, 28, y + 8, SCREEN_W - 56, 3, 24, C_MUTED, 255);
    }
    static const char *h[] = {"A", "Change", "←→", "Adjust", "B", "Back"};
    ui_hints(h, 3);
}

static void settings_input(InputState *in) {
    if (in->pressed[B_B] || in->pressed[B_START]) {
        settings_save();
        screen = S_HOME;
        return;
    }
    if (in->pressed[B_DOWN] && set_focus < SET_N - 1) set_focus++;
    if (in->pressed[B_UP] && set_focus > 0) set_focus--;
    int d = in->pressed[B_RIGHT] ? 1 : in->pressed[B_LEFT] ? -1 : in->pressed[B_A] ? 1 : 0;
    if (!d) return;
    bool changed = true;
    switch (set_focus) {
    case SET_SCANLINES: g_settings.scanlines = !g_settings.scanlines; break;
    case SET_GAME_SCANLINES: g_settings.game_scanlines = !g_settings.game_scanlines; break;
    case SET_SMOOTH: g_settings.smooth = !g_settings.smooth; break;
    case SET_LAYOUT: g_settings.swap_ab = !g_settings.swap_ab; break;
    case SET_SCALE: g_settings.scale = (g_settings.scale + 6 + d) % 6; break;
    case SET_BG: {
        int n = bg_count();
        if (n > 0) {
            bg_select((bg_current() + n + d) % n);
            snprintf(g_settings.background, sizeof g_settings.background, "%s", bg_name(bg_current()));
        }
        break;
    }
    case SET_BG_DIM: g_settings.bg_dim = (g_settings.bg_dim + 3 + d) % 3; break;
    case SET_LEDS:
        changed = false;
        if (in->pressed[B_A] || in->pressed[B_RIGHT]) {
            leds_screen_enter();
            screen = S_LEDS;
        }
        break;
    case SET_WIFI:
        changed = false;
        if (in->pressed[B_A] || in->pressed[B_RIGHT]) {
            wifi_start();
            screen = S_WIFI;
        }
        break;
    case SET_RESCAN:
        changed = false;
        if (in->pressed[B_A]) rescan();
        break;
    case SET_EXIT:
        changed = false;
        if (in->pressed[B_A]) want_exit = true;
        break;
    }
    if (changed) settings_save();
}

/* ---------------- main loop ---------------- */

int ui_main(SDL_Window *win, SDL_Renderer *ren) {
    (void)win;
    mkdirs(DATA_DIR);
    mkdirs(DATA_DIR "/bios");
    for (int s = 0; s < NUM_SYSTEMS; s++) {
        char d[300];
        snprintf(d, sizeof d, "%s/%s", ROMS_INTERNAL, SYSTEMS[s].folders[0]);
        mkdirs(d);
        char rel[128];
        snprintf(rel, sizeof rel, "res/%s", SYSTEMS[s].image);
        sys_art[s] = gfx_load(rel);
    }
    tex_logo = gfx_load("res/logo.png");
    tex_library = gfx_load("res/library_button.png");
    tex_settings = gfx_load("res/settings_button.png");
    leds_load();
    leds_apply(&led_cur);
    if (bios_import_from_sd()) ui_toast("Imported BIOS files from the SD card");
    bg_scan();
    bg_select_name(g_settings.background[0] ? g_settings.background : "Rainey");
    lists_load();
    scan_all();
    detail_game = 0;
    bool restored = ui_load_state();
    remove(UI_STATE_FILE);
    if (!restored) {
        tex_boot = gfx_load("res/boot_screen.png");
        screen = S_SPLASH;
    }
    Uint32 start = SDL_GetTicks();
    InputState in;
    while (!want_exit) {
        input_poll(&in);
        if (in.quit) break;
        float t = (SDL_GetTicks() - start) / 1000.0f;
        static Uint32 next_flag_check;
        if (SDL_TICKS_PASSED(SDL_GetTicks(), next_flag_check)) { /* files arrived via USB/Wi-Fi */
            next_flag_check = SDL_GetTicks() + 1000;
            if (!remove("/tmp/rainey_rescan") && screen != S_SPLASH) rescan();
        }
        art_loads_this_frame = 0;
        SDL_SetRenderDrawColor(ren, 5, 8, 18, 255);
        SDL_RenderClear(ren);
        if (screen == S_SPLASH) {
            float a = t < 0.5f ? t / 0.5f : t > 1.6f ? 1 - (t - 1.6f) / 0.4f : 1;
            if (a < 0) a = 0;
            if (tex_boot) gfx_image_fit(tex_boot, 0, 0, SCREEN_W, SCREEN_H, (int)(a * 255));
            bool any = false;
            for (int b = 0; b < B_COUNT; b++) any |= in.pressed[b];
            if (t > 2.0f || any) {
                screen = S_HOME;
                if (tex_boot) SDL_DestroyTexture(tex_boot);
                tex_boot = NULL;
            }
        } else {
            gfx_background(t);
            switch (screen) {
            case S_HOME: home_input(&in); break;
            case S_LIBRARY: library_input(&in); break;
            case S_DETAIL: detail_input(&in); break;
            case S_COLLECTIONS: collections_input(&in); break;
            case S_SETTINGS: settings_input(&in); break;
            case S_LEDS:
                if (leds_screen_input(&in)) screen = S_SETTINGS;
                break;
            case S_WIFI:
                if (wifi_screen_input(&in)) {
                    screen = S_SETTINGS;
                    rescan();
                }
                break;
            default: break;
            }
            switch (screen) {
            case S_HOME: home_draw(t); break;
            case S_LIBRARY: library_draw(); break;
            case S_DETAIL: detail_draw(); break;
            case S_COLLECTIONS: collections_draw(); break;
            case S_SETTINGS: settings_draw(); break;
            case S_LEDS: leds_screen_draw(); break;
            case S_WIFI: wifi_screen_draw(); break;
            default: break;
            }
            draw_toast();
            if (g_settings.scanlines) gfx_scanlines(46);
        }
        if (g_shot_request[0]) {
            gfx_screenshot(g_shot_request);
            g_shot_request[0] = 0;
        }
        SDL_RenderPresent(ren);
    }
    wifi_stop();
    bg_shutdown();
    return 0;
}
