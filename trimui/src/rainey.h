/* RaineyUI for TrimUI Brick / Smart Pro (tg5040 / tg3040), SDL2 port. */
#ifndef RAINEY_H
#define RAINEY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 1024
#define SCREEN_H 768

#define DATA_DIR "/mnt/UDISK/RaineyUI"
#define ROMS_INTERNAL "/mnt/UDISK/Roms"
#define ROMS_SDCARD "/mnt/SDCARD/Roms"
#define SDCARD_ROOT "/mnt/SDCARD"
#define LAUNCH_FILE "/tmp/rainey_launch"
#define UI_STATE_FILE "/tmp/rainey_ui_state"
#define DEBUG_INPUT_FILE "/tmp/rainey_input"

/* theme (from app/.../ui/theme/RaineyTheme.kt) */
#define C_ARCADE_BLACK 0x050812
#define C_PANEL 0x111827
#define C_NEON_BLUE 0x21D4FD
#define C_NEON_PURPLE 0xB83BFF
#define C_NEON_PINK 0xFF4FD8
#define C_TEXT 0xF4F7FB
#define C_MUTED 0xA7B0C0

typedef struct {
    const char *id;
    const char *name;
    const char *folders[16]; /* folder names: stock TrimUI first, then other layouts (EmulationStation/Batocera…) */
    const char *exts;        /* ",ext,ext," */
    const char *core;        /* path relative to the app dir */
    const char *image;       /* res/<image> */
    bool extract_zip;        /* frontend unpacks .zip before handing to core */
    const char *launcher;    /* "rainey" built-in player, "retroarch", "drastic" */
    const char *core_label;  /* shown on the detail screen */
} SystemDef;

extern const SystemDef SYSTEMS[];
extern const int NUM_SYSTEMS;

typedef struct {
    bool scanlines;       /* UI scanline overlay */
    bool game_scanlines;  /* in-game scanline overlay */
    bool swap_ab;         /* Xbox-style layout: swap A/B and X/Y */
    bool smooth;          /* linear filtering in game */
    int scale;            /* 0 auto(aspect fit), 1-4 fixed integer, 5 stretch */
    char background[96];  /* background name ("" = default) */
    int bg_dim;           /* 0 dim, 1 normal, 2 bright */
    int dc_res;           /* Dreamcast internal resolution, index into DC_RES */
} Settings;

extern Settings g_settings;
extern char g_appdir[512];

int bios_import_from_sd(void);
void settings_load(void);
void settings_save(void);
void mkdirs(const char *path);
bool file_exists(const char *path);

/* ---- gfx helpers (gfx.c) ---- */
typedef enum { F_SMALL, F_BODY, F_LABEL, F_TITLE, F_HUGE, F_COUNT } FontId;

bool gfx_init(SDL_Renderer *r);
void gfx_quit(void);
SDL_Texture *gfx_load(const char *relpath);         /* relative to appdir */
SDL_Texture *gfx_load_abs(const char *path);
void gfx_color(uint32_t rgb, int a);
void gfx_fill(int x, int y, int w, int h, uint32_t rgb, int a);
void gfx_round_fill(int x, int y, int w, int h, int r, uint32_t rgb, int a);
void gfx_round_ring(int x, int y, int w, int h, int r, int t, uint32_t rgb, int a);
void gfx_glow(int x, int y, int w, int h, int r, uint32_t rgb, float strength);
void gfx_image_cover(SDL_Texture *t, int x, int y, int w, int h, int alpha);
void gfx_image_fit(SDL_Texture *t, int x, int y, int w, int h, int alpha);
int gfx_text(FontId f, const char *s, int x, int y, uint32_t rgb, int a);  /* returns width */
int gfx_text_w(FontId f, const char *s);
int gfx_font_h(FontId f);
void gfx_text_center(FontId f, const char *s, int cx, int y, uint32_t rgb, int a);
int gfx_text_wrap(FontId f, const char *s, int x, int y, int w, int max_lines, int line_h,
                  uint32_t rgb, int a);
void gfx_text_ellipsis(FontId f, const char *s, int x, int y, int w, uint32_t rgb, int a);
void gfx_background(float t);
void gfx_scanlines(int alpha);
void gfx_neon_button(int x, int y, int w, int h, const char *label, bool active);
void gfx_screenshot(const char *path);

extern SDL_Renderer *g_ren;

/* ---- shared UI helpers (ui.c) and the LED screen (leds_ui.c) ---- */
void ui_hints(const char *const *pairs, int n);
void ui_toast(const char *msg);

/* ---- menu backgrounds (bg.c) ---- */
void bg_scan(void);
int bg_count(void);
int bg_current(void);
const char *bg_name(int i);
bool bg_is_anim(int i);
void bg_select(int i);
void bg_select_name(const char *name);
bool bg_draw(int alpha);
void bg_shutdown(void);

/* ---- entry points ---- */
int ui_main(SDL_Window *win, SDL_Renderer *ren);
int player_main(SDL_Window *win, SDL_Renderer *ren, const char *launch_file);
int install_ui_main(SDL_Window *win, SDL_Renderer *ren, const char *worker);
int unzip_cli(const char *zip, const char *outdir);
int patch_core_cli(const char *in, const char *out);

/* ---- input abstraction shared by UI + player menu ---- */
typedef enum {
    B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_X, B_Y, B_L, B_R, B_L2, B_R2,
    B_SELECT, B_START, B_MENU, B_L3, B_R3, B_COUNT
} Button;

typedef struct {
    bool down[B_COUNT];
    bool pressed[B_COUNT];   /* edge this frame (incl. key repeat for dpad) */
    int16_t lx, ly, rx, ry;
    bool quit;
} InputState;

bool wifi_start(void);
void wifi_stop(void);
void wifi_screen_draw(void);
bool wifi_screen_input(InputState *in);
int sort_cli(void);

void leds_screen_enter(void);
void leds_screen_draw(void);
bool leds_screen_input(InputState *in);

void input_init(void);
void input_poll(InputState *in);   /* processes SDL events */
SDL_GameController *input_controller(void);

#endif
