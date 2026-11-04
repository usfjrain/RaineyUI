#include "rainey.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

SDL_Renderer *g_ren;
static TTF_Font *fonts[F_COUNT];
static const int font_px[F_COUNT] = {17, 22, 26, 34, 46};
static const bool font_bold[F_COUNT] = {false, false, false, true, true};

/* ---- text cache ---- */
#define TCACHE 384
typedef struct {
    char *s;
    int font;
    uint32_t rgb;
    SDL_Texture *tex;
    int w, h;
    uint32_t used;
} TextEntry;
static TextEntry tcache[TCACHE];
static uint32_t tclock;

bool gfx_init(SDL_Renderer *r) {
    g_ren = r;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    char path[700];
    for (int i = 0; i < F_COUNT; i++) {
        snprintf(path, sizeof path, "%s/res/%s", g_appdir,
                 font_bold[i] ? "DejaVuSansMono-Bold.ttf" : "DejaVuSansMono.ttf");
        fonts[i] = TTF_OpenFont(path, font_px[i]);
        if (!fonts[i]) {
            fprintf(stderr, "font %s: %s\n", path, TTF_GetError());
            return false;
        }
    }
    return true;
}

void gfx_quit(void) {
    for (int i = 0; i < TCACHE; i++) {
        if (tcache[i].tex) SDL_DestroyTexture(tcache[i].tex);
        free(tcache[i].s);
    }
    memset(tcache, 0, sizeof tcache);
    for (int i = 0; i < F_COUNT; i++)
        if (fonts[i]) TTF_CloseFont(fonts[i]);
}

SDL_Texture *gfx_load_abs(const char *path) {
    SDL_Surface *s = IMG_Load(path);
    if (!s) return NULL;
    SDL_Texture *t = SDL_CreateTextureFromSurface(g_ren, s);
    SDL_FreeSurface(s);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

SDL_Texture *gfx_load(const char *rel) {
    char path[700];
    snprintf(path, sizeof path, "%s/%s", g_appdir, rel);
    SDL_Texture *t = gfx_load_abs(path);
    if (!t) fprintf(stderr, "image %s: %s\n", path, IMG_GetError());
    return t;
}

void gfx_color(uint32_t rgb, int a) {
    SDL_SetRenderDrawColor(g_ren, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, a);
}

void gfx_fill(int x, int y, int w, int h, uint32_t rgb, int a) {
    gfx_color(rgb, a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(g_ren, &r);
}

static int corner_inset(int r, int row) {
    /* row: 0..r-1 from the top edge of the corner box */
    if (r <= 0 || row >= r) return 0;
    float dy = r - row - 0.5f;
    float dx = sqrtf((float)r * r - dy * dy);
    return (int)lroundf(r - dx);
}

static void spans(int x, int y, int w, int h, int r, int t, bool ring) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    SDL_Rect rows[1024];
    int n = 0;
    for (int yy = 0; yy < h; yy++) {
        int crow = yy < r ? yy : (yy >= h - r ? h - 1 - yy : r);
        int o = corner_inset(r, crow);
        if (!ring || yy < t || yy >= h - t) {
            rows[n++] = (SDL_Rect){x + o, y + yy, w - 2 * o, 1};
        } else {
            int ir = r - t;
            int iy = yy - t, ih = h - 2 * t;
            int icrow = iy < ir ? iy : (iy >= ih - ir ? ih - 1 - iy : ir);
            int i = t + (ir > 0 ? corner_inset(ir, icrow) : 0);
            if (i < o) i = o;
            rows[n++] = (SDL_Rect){x + o, y + yy, i - o > 0 ? i - o : 1, 1};
            rows[n++] = (SDL_Rect){x + w - i, y + yy, i - o > 0 ? i - o : 1, 1};
        }
        if (n >= 1022) {
            SDL_RenderFillRects(g_ren, rows, n);
            n = 0;
        }
    }
    if (n) SDL_RenderFillRects(g_ren, rows, n);
}

void gfx_round_fill(int x, int y, int w, int h, int r, uint32_t rgb, int a) {
    gfx_color(rgb, a);
    spans(x, y, w, h, r, 0, false);
}

void gfx_round_ring(int x, int y, int w, int h, int r, int t, uint32_t rgb, int a) {
    gfx_color(rgb, a);
    spans(x, y, w, h, r, t, true);
}

void gfx_glow(int x, int y, int w, int h, int r, uint32_t rgb, float strength) {
    if (strength <= 0.01f) return;
    for (int i = 6; i >= 1; i--) {
        int a = (int)(strength * (60 - i * 8));
        if (a <= 0) continue;
        gfx_round_ring(x - i * 2, y - i * 2, w + i * 4, h + i * 4, r + i * 2, 2, rgb, a);
    }
}

void gfx_image_cover(SDL_Texture *t, int x, int y, int w, int h, int alpha) {
    if (!t) return;
    int tw, th;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    SDL_Rect src;
    float ta = (float)tw / th, da = (float)w / h;
    if (ta > da) {
        src.h = th;
        src.w = (int)(th * da);
        src.x = (tw - src.w) / 2;
        src.y = 0;
    } else {
        src.w = tw;
        src.h = (int)(tw / da);
        src.x = 0;
        src.y = (th - src.h) / 2;
    }
    SDL_Rect dst = {x, y, w, h};
    SDL_SetTextureAlphaMod(t, alpha);
    SDL_RenderCopy(g_ren, t, &src, &dst);
}

void gfx_image_fit(SDL_Texture *t, int x, int y, int w, int h, int alpha) {
    if (!t) return;
    int tw, th;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    float s = fminf((float)w / tw, (float)h / th);
    SDL_Rect dst = {x + (w - (int)(tw * s)) / 2, y + (h - (int)(th * s)) / 2, (int)(tw * s),
                    (int)(th * s)};
    SDL_SetTextureAlphaMod(t, alpha);
    SDL_RenderCopy(g_ren, t, NULL, &dst);
}

static TextEntry *text_get(FontId f, const char *s, uint32_t rgb) {
    if (!s || !*s) return NULL;
    tclock++;
    int lru = 0;
    for (int i = 0; i < TCACHE; i++) {
        TextEntry *e = &tcache[i];
        if (e->s && e->font == (int)f && e->rgb == rgb && strcmp(e->s, s) == 0) {
            e->used = tclock;
            return e;
        }
        if (!e->s) {
            lru = i;
            tcache[lru].used = 0;
        } else if (e->used < tcache[lru].used) {
            lru = i;
        }
    }
    TextEntry *e = &tcache[lru];
    if (e->tex) SDL_DestroyTexture(e->tex);
    free(e->s);
    memset(e, 0, sizeof *e);
    SDL_Color c = {(rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(fonts[f], s, c);
    if (!surf) return NULL;
    e->tex = SDL_CreateTextureFromSurface(g_ren, surf);
    e->w = surf->w;
    e->h = surf->h;
    SDL_FreeSurface(surf);
    e->s = strdup(s);
    e->font = f;
    e->rgb = rgb;
    e->used = tclock;
    return e;
}

int gfx_text(FontId f, const char *s, int x, int y, uint32_t rgb, int a) {
    TextEntry *e = text_get(f, s, rgb);
    if (!e || !e->tex) return 0;
    SDL_SetTextureAlphaMod(e->tex, a);
    SDL_Rect d = {x, y, e->w, e->h};
    SDL_RenderCopy(g_ren, e->tex, NULL, &d);
    return e->w;
}

int gfx_text_w(FontId f, const char *s) {
    int w = 0, h = 0;
    if (s && *s) TTF_SizeUTF8(fonts[f], s, &w, &h);
    return w;
}

int gfx_font_h(FontId f) { return TTF_FontHeight(fonts[f]); }

void gfx_text_center(FontId f, const char *s, int cx, int y, uint32_t rgb, int a) {
    gfx_text(f, s, cx - gfx_text_w(f, s) / 2, y, rgb, a);
}

void gfx_text_ellipsis(FontId f, const char *s, int x, int y, int w, uint32_t rgb, int a) {
    if (gfx_text_w(f, s) <= w) {
        gfx_text(f, s, x, y, rgb, a);
        return;
    }
    char buf[512];
    snprintf(buf, sizeof buf, "%s", s);
    int n = (int)strlen(buf);
    while (n > 0) {
        n--;
        while (n > 0 && (buf[n] & 0xC0) == 0x80) n--; /* utf8 boundary */
        buf[n] = 0;
        char t[520];
        snprintf(t, sizeof t, "%s…", buf);
        if (gfx_text_w(f, t) <= w) {
            gfx_text(f, t, x, y, rgb, a);
            return;
        }
    }
}

int gfx_text_wrap(FontId f, const char *s, int x, int y, int w, int max_lines, int line_h,
                  uint32_t rgb, int a) {
    char line[512] = "";
    const char *p = s;
    int lines = 0;
    while (*p && lines < max_lines) {
        /* next word */
        const char *q = p;
        while (*q && *q != ' ') q++;
        char cand[512];
        int ll = (int)strlen(line);
        snprintf(cand, sizeof cand, "%s%s%.*s", line, ll ? " " : "", (int)(q - p), p);
        if (gfx_text_w(f, cand) <= w || ll == 0) {
            snprintf(line, sizeof line, "%s", cand);
            p = *q ? q + 1 : q;
            if (ll == 0 && gfx_text_w(f, line) > w && lines == max_lines - 1) break;
        } else {
            if (lines == max_lines - 1) break;
            gfx_text_ellipsis(f, line, x, y + lines * line_h, w, rgb, a);
            lines++;
            line[0] = 0;
        }
    }
    if (line[0] && lines < max_lines) {
        if (*p) {
            char rest[1024];
            snprintf(rest, sizeof rest, "%s %s", line, p);
            gfx_text_ellipsis(f, rest, x, y + lines * line_h, w, rgb, a);
        } else {
            gfx_text_ellipsis(f, line, x, y + lines * line_h, w, rgb, a);
        }
        lines++;
    }
    return lines;
}

/* ArcadeBackground from ArcadeComponents.kt */
void gfx_background(float t) {
    float drift = fmodf(t / 9.0f, 1.0f);
    int w = SCREEN_W, h = SCREEN_H;
    /* vertical gradient ArcadeBlack -> 0A1024 -> 150B24 */
    SDL_Vertex v[8];
    const uint32_t cols[3] = {C_ARCADE_BLACK, 0x0A1024, 0x150B24};
    for (int i = 0; i < 3; i++) {
        uint32_t c = cols[i];
        SDL_Color sc = {(c >> 16) & 255, (c >> 8) & 255, c & 255, 255};
        float yy = h * i / 2.0f;
        v[i * 2] = (SDL_Vertex){{0, yy}, sc, {0, 0}};
        v[i * 2 + 1] = (SDL_Vertex){{(float)w, yy}, sc, {0, 0}};
    }
    int idx[12] = {0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4};
    SDL_RenderGeometry(g_ren, NULL, v, 6, idx, 12);
    /* background brightness: Dim / Normal / Bright */
    static const float img_a[3] = {0.28f, 0.42f, 0.70f}, shade[3] = {0.58f, 0.48f, 0.26f};
    int dim = g_settings.bg_dim >= 0 && g_settings.bg_dim <= 2 ? g_settings.bg_dim : 1;
    bg_draw((int)(img_a[dim] * 255));
    gfx_fill(0, 0, w, h, 0x000000, (int)(shade[dim] * 255));
    float horizon = h * 0.68f;
    gfx_fill((int)(w * (0.05f + drift * 0.04f)), 0, (int)(w * 0.22f), h, C_NEON_PURPLE, 26);
    gfx_fill((int)(w * (0.72f - drift * 0.04f)), 0, (int)(w * 0.18f), h, C_NEON_BLUE, 20);
    gfx_color(C_NEON_BLUE, 36);
    for (int i = 0; i <= 9; i++) {
        float y = horizon + fmodf(i * 34.f + drift * 34.f, 340.f);
        if (y > h) continue;
        SDL_RenderDrawLineF(g_ren, 0, y, w, y);
        SDL_RenderDrawLineF(g_ren, 0, y + 1, w, y + 1);
    }
    gfx_color(C_NEON_PURPLE, 41);
    for (int i = -8; i <= 8; i++) {
        float x = w / 2.f + i * 96.f;
        SDL_RenderDrawLineF(g_ren, w / 2.f, horizon, x, h);
        SDL_RenderDrawLineF(g_ren, w / 2.f + 1, horizon, x + 1, h);
    }
}

void gfx_scanlines(int alpha) {
    static SDL_Rect rows[SCREEN_H / 3 + 1];
    int n = 0;
    for (int y = 0; y < SCREEN_H; y += 3) rows[n++] = (SDL_Rect){0, y, SCREEN_W, 1};
    gfx_color(0x000000, alpha);
    SDL_RenderFillRects(g_ren, rows, n);
}

/* NeonButton */
void gfx_neon_button(int x, int y, int w, int h, const char *label, bool active) {
    if (active) gfx_glow(x, y, w, h, 7, C_NEON_BLUE, 1.0f);
    gfx_round_fill(x, y, w, h, 7, active ? C_NEON_BLUE : C_PANEL, active ? 56 : 210);
    gfx_round_ring(x, y, w, h, 7, 2, active ? C_NEON_BLUE : C_NEON_PURPLE, active ? 255 : 128);
    int th = gfx_font_h(F_LABEL);
    gfx_text_center(F_LABEL, label, x + w / 2, y + (h - th) / 2, C_TEXT, 255);
}

void gfx_screenshot(const char *path) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32,
                                                    SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    if (SDL_RenderReadPixels(g_ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0)
        IMG_SavePNG(s, path);
    SDL_FreeSurface(s);
}
