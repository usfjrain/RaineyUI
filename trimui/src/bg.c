/* Selectable menu backgrounds: static images or animated JPEG/PNG frame folders.
 * Built-ins: <app>/res/backgrounds, user's own: /mnt/UDISK/RaineyUI/backgrounds. */
#include "rainey.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define MAX_BGS 64
#define MAX_FRAMES 600

typedef struct {
    char name[96];
    char path[640]; /* "" = neon grid only */
    bool anim;
} BgEntry;

static BgEntry bgs[MAX_BGS];
static int nbgs;
static int cur = -1;

static SDL_Texture *tex;
static int tex_w, tex_h;

/* animation */
static char **frames;
static int nframes;
static int fps = 15;
static SDL_Thread *worker;
static SDL_atomic_t stop_flag;
static SDL_mutex *mx;
static SDL_Surface *ready;
static Uint32 anim_t0;

static bool is_img(const char *n) {
    const char *d = strrchr(n, '.');
    return d && (!strcasecmp(d, ".png") || !strcasecmp(d, ".jpg") || !strcasecmp(d, ".jpeg") ||
                 !strcasecmp(d, ".bmp"));
}

static int cmp_str(const void *a, const void *b) {
    return strcasecmp(*(char *const *)a, *(char *const *)b);
}

static int dir_frames(const char *dir, char ***out) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    char **list = NULL;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < MAX_FRAMES) {
        if (e->d_name[0] == '.' || !is_img(e->d_name)) continue;
        list = realloc(list, sizeof(char *) * (n + 1));
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        list[n++] = strdup(p);
    }
    closedir(d);
    qsort(list, n, sizeof(char *), cmp_str);
    *out = list;
    return n;
}

static void add_dir(const char *root) {
    DIR *d = opendir(root);
    if (!d) return;
    char *names[MAX_BGS];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < MAX_BGS) {
        if (e->d_name[0] == '.') continue;
        names[n++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof(char *), cmp_str);
    for (int i = 0; i < n; i++) {
        char p[640];
        snprintf(p, sizeof p, "%s/%s", root, names[i]);
        struct stat st;
        if (nbgs < MAX_BGS && stat(p, &st) == 0) {
            BgEntry *b = &bgs[nbgs];
            if (S_ISDIR(st.st_mode)) {
                char **fr = NULL;
                int k = dir_frames(p, &fr);
                for (int j = 0; j < k; j++) free(fr[j]);
                free(fr);
                if (k > 0) {
                    snprintf(b->name, sizeof b->name, "%s", names[i]);
                    snprintf(b->path, sizeof b->path, "%s", p);
                    b->anim = k > 1;
                    nbgs++;
                }
            } else if (is_img(names[i])) {
                snprintf(b->name, sizeof b->name, "%s", names[i]);
                char *dot = strrchr(b->name, '.');
                if (dot) *dot = 0;
                snprintf(b->path, sizeof b->path, "%s", p);
                b->anim = false;
                nbgs++;
            }
        }
        free(names[i]);
    }
}

void bg_scan(void) {
    char keep[96] = "";
    if (cur >= 0 && cur < nbgs) snprintf(keep, sizeof keep, "%s", bgs[cur].name);
    nbgs = 0;
    snprintf(bgs[0].name, sizeof bgs[0].name, "Neon grid");
    bgs[0].path[0] = 0;
    bgs[0].anim = false;
    nbgs = 1;
    char p[600];
    snprintf(p, sizeof p, "%s/res/backgrounds", g_appdir);
    add_dir(p);
    mkdirs(DATA_DIR "/backgrounds");
    add_dir(DATA_DIR "/backgrounds");
    if (keep[0]) {
        for (int i = 0; i < nbgs; i++)
            if (!strcmp(bgs[i].name, keep)) cur = i;
    }
}

int bg_count(void) { return nbgs; }
int bg_current(void) { return cur; }
const char *bg_name(int i) { return i >= 0 && i < nbgs ? bgs[i].name : ""; }
bool bg_is_anim(int i) { return i >= 0 && i < nbgs && bgs[i].anim; }

static int worker_fn(void *arg) {
    (void)arg;
    int last = -1;
    while (!SDL_AtomicGet(&stop_flag)) {
        int idx = (int)(((Uint64)(SDL_GetTicks() - anim_t0) * fps / 1000) % nframes);
        if (idx != last) {
            SDL_Surface *s = IMG_Load(frames[idx]);
            if (s) {
                SDL_Surface *c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
                SDL_FreeSurface(s);
                if (c) {
                    SDL_LockMutex(mx);
                    if (ready) SDL_FreeSurface(ready);
                    ready = c;
                    SDL_UnlockMutex(mx);
                }
            }
            last = idx;
        }
        SDL_Delay(4);
    }
    return 0;
}

static void stop_anim(void) {
    if (worker) {
        SDL_AtomicSet(&stop_flag, 1);
        SDL_WaitThread(worker, NULL);
        worker = NULL;
    }
    if (ready) {
        SDL_FreeSurface(ready);
        ready = NULL;
    }
    for (int i = 0; i < nframes; i++) free(frames[i]);
    free(frames);
    frames = NULL;
    nframes = 0;
}

void bg_select(int i) {
    if (i < 0 || i >= nbgs) i = 0;
    stop_anim();
    if (tex) SDL_DestroyTexture(tex);
    tex = NULL;
    cur = i;
    BgEntry *b = &bgs[i];
    if (!b->path[0]) return;
    struct stat st;
    if (stat(b->path, &st) == 0 && S_ISDIR(st.st_mode)) {
        nframes = dir_frames(b->path, &frames);
        if (!nframes) return;
        char fp[700];
        snprintf(fp, sizeof fp, "%s/fps.txt", b->path);
        FILE *f = fopen(fp, "r");
        fps = 15;
        if (f) {
            if (fscanf(f, "%d", &fps) != 1 || fps < 1 || fps > 60) fps = 15;
            fclose(f);
        }
        if (nframes == 1) { /* folder with one image: treat as static */
            tex = gfx_load_abs(frames[0]);
            stop_anim();
            return;
        }
        if (!mx) mx = SDL_CreateMutex();
        SDL_AtomicSet(&stop_flag, 0);
        anim_t0 = SDL_GetTicks();
        worker = SDL_CreateThread(worker_fn, "bg-anim", NULL);
    } else {
        tex = gfx_load_abs(b->path);
    }
}

void bg_select_name(const char *name) {
    int idx = 1 < nbgs ? 1 : 0; /* default: first built-in image */
    for (int i = 0; i < nbgs; i++)
        if (name && !strcmp(bgs[i].name, name)) idx = i;
    bg_select(idx);
}

void bg_shutdown(void) {
    stop_anim();
    if (tex) SDL_DestroyTexture(tex);
    tex = NULL;
}

/* draws the selected background image (cover-fit); returns false for "Neon grid" */
bool bg_draw(int alpha) {
    if (worker) {
        SDL_Surface *s = NULL;
        SDL_LockMutex(mx);
        s = ready;
        ready = NULL;
        SDL_UnlockMutex(mx);
        if (s) {
            if (!tex || tex_w != s->w || tex_h != s->h) {
                if (tex) SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                                        s->w, s->h);
                if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                tex_w = s->w;
                tex_h = s->h;
            }
            if (tex) SDL_UpdateTexture(tex, NULL, s->pixels, s->pitch);
            SDL_FreeSurface(s);
        }
    }
    if (!tex) return false;
    gfx_image_cover(tex, 0, 0, SCREEN_W, SCREEN_H, alpha);
    return true;
}
