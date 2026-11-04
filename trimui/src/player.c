/* Minimal libretro frontend for RaineyUI on TrimUI (software-rendered cores). */
#include "rainey.h"
#include "libretro.h"
#include "miniz.h"
#include <dlfcn.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

extern char g_shot_request[256];

static struct {
    void *h;
    void (*init)(void);
    void (*deinit)(void);
    void (*get_system_info)(struct retro_system_info *);
    void (*get_system_av_info)(struct retro_system_av_info *);
    void (*set_environment)(retro_environment_t);
    void (*set_video_refresh)(retro_video_refresh_t);
    void (*set_audio_sample)(retro_audio_sample_t);
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*set_input_poll)(retro_input_poll_t);
    void (*set_input_state)(retro_input_state_t);
    void (*set_controller_port_device)(unsigned, unsigned);
    void (*reset)(void);
    void (*run)(void);
    size_t (*serialize_size)(void);
    bool (*serialize)(void *, size_t);
    bool (*unserialize)(const void *, size_t);
    bool (*load_game)(const struct retro_game_info *);
    void (*unload_game)(void);
    void *(*get_memory_data)(unsigned);
    size_t (*get_memory_size)(unsigned);
} core;

static char sys_id[64], core_path[600], rom_path[1024], game_name[512];
static char system_dir[600], save_dir[600], state_base[1200], srm_path[1200];
static char tmp_rom[1024];
static enum retro_pixel_format pixfmt = RETRO_PIXEL_FORMAT_0RGB1555;
static struct retro_system_av_info av;
static SDL_Texture *frame_tex;
static int tex_w, tex_h, frame_w, frame_h;
static SDL_AudioDeviceID audio_dev;
static int audio_rate;
static InputState in;
static bool quit_flag;
static volatile sig_atomic_t got_signal;
static int16_t audio_buf[8192 * 2];
static size_t audio_frames;

/* ---- core options: legacy SET_VARIABLES, defaults = first value ---- */
typedef struct {
    char *key;
    char *value;
} Var;
static Var vars[512];
static int nvars;
static bool vars_dirty;

static void var_set(const char *key, const char *val, bool override) {
    for (int i = 0; i < nvars; i++)
        if (!strcmp(vars[i].key, key)) {
            if (override) {
                free(vars[i].value);
                vars[i].value = strdup(val);
            }
            return;
        }
    if (nvars < 512) {
        vars[nvars].key = strdup(key);
        vars[nvars].value = strdup(val);
        nvars++;
    }
}

static void load_option_overrides(void) {
    /* user overrides: DATA_DIR/options/<core>.opt  (key = "value" lines) */
    const char *base = strrchr(core_path, '/');
    base = base ? base + 1 : core_path;
    char name[256];
    snprintf(name, sizeof name, "%s", base);
    char *p = strstr(name, "_libretro");
    if (p) *p = 0;
    char path[700];
    snprintf(path, sizeof path, "%s/options/%s.opt", DATA_DIR, name);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char k[256], v[256];
        if (sscanf(line, " %255[^= ] = \"%255[^\"]\"", k, v) == 2) var_set(k, v, true);
    }
    fclose(f);
}

static void core_log(enum retro_log_level level, const char *fmt, ...) {
    if (level < RETRO_LOG_WARN) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static bool env_cb(unsigned cmd, void *data) {
    switch (cmd & ~RETRO_ENVIRONMENT_EXPERIMENTAL) {
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)data = true; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        enum retro_pixel_format f = *(enum retro_pixel_format *)data;
        if (f > RETRO_PIXEL_FORMAT_RGB565) return false;
        pixfmt = f;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *(const char **)data = system_dir; return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)data = save_dir; return true;
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY: *(const char **)data = system_dir; return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = core_log;
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        const struct retro_variable *v = data;
        for (; v && v->key; v++) {
            /* "Description; default|other|..." */
            const char *semi = strchr(v->value, ';');
            if (!semi) continue;
            semi++;
            while (*semi == ' ') semi++;
            char def[256];
            size_t n = strcspn(semi, "|");
            if (n >= sizeof def) n = sizeof def - 1;
            memcpy(def, semi, n);
            def[n] = 0;
            var_set(v->key, def, false);
        }
        load_option_overrides();
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;
        for (int i = 0; i < nvars; i++)
            if (!strcmp(vars[i].key, v->key)) {
                v->value = vars[i].value;
                return true;
            }
        v->value = NULL;
        return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)data = vars_dirty;
        vars_dirty = false;
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned *)data = 0; return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        av = *(const struct retro_system_av_info *)data;
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        av.geometry = *(const struct retro_game_geometry *)data;
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        if (data) *(int *)data = 3;
        return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE: *(unsigned *)data = RETRO_LANGUAGE_ENGLISH; return true;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
        return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE: {
        const struct retro_message *m = data;
        if (m && m->msg) fprintf(stderr, "core: %s\n", m->msg);
        return true;
    }
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION: *(unsigned *)data = 0; return true;
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING: *(bool *)data = false; return true;
    case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS: *(unsigned *)data = 1; return true;
    default: return false;
    }
}

static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data) return; /* dupe */
    Uint32 fmt = pixfmt == RETRO_PIXEL_FORMAT_XRGB8888 ? SDL_PIXELFORMAT_ARGB8888
                 : pixfmt == RETRO_PIXEL_FORMAT_RGB565 ? SDL_PIXELFORMAT_RGB565
                                                       : SDL_PIXELFORMAT_RGB555;
    static Uint32 cur_fmt;
    if (!frame_tex || (int)w > tex_w || (int)h > tex_h || fmt != cur_fmt) {
        if (frame_tex) SDL_DestroyTexture(frame_tex);
        tex_w = (int)w > tex_w ? (int)w : tex_w;
        tex_h = (int)h > tex_h ? (int)h : tex_h;
        if (tex_w < (int)av.geometry.max_width) tex_w = av.geometry.max_width;
        if (tex_h < (int)av.geometry.max_height) tex_h = av.geometry.max_height;
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, g_settings.smooth ? "1" : "0");
        frame_tex = SDL_CreateTexture(g_ren, fmt, SDL_TEXTUREACCESS_STREAMING, tex_w, tex_h);
        cur_fmt = fmt;
    }
    SDL_Rect r = {0, 0, (int)w, (int)h};
    if (pixfmt == RETRO_PIXEL_FORMAT_XRGB8888) {
        /* force alpha opaque: update via lock and copy rows */
        void *dst;
        int dpitch;
        if (SDL_LockTexture(frame_tex, &r, &dst, &dpitch) == 0) {
            for (unsigned y = 0; y < h; y++) {
                const uint32_t *s = (const uint32_t *)((const uint8_t *)data + y * pitch);
                uint32_t *d = (uint32_t *)((uint8_t *)dst + y * dpitch);
                for (unsigned x = 0; x < w; x++) d[x] = s[x] | 0xFF000000u;
            }
            SDL_UnlockTexture(frame_tex);
        }
    } else {
        SDL_UpdateTexture(frame_tex, &r, data, (int)pitch);
    }
    frame_w = w;
    frame_h = h;
}

static void flush_audio(void) {
    if (audio_dev && audio_frames) {
        SDL_QueueAudio(audio_dev, audio_buf, (Uint32)(audio_frames * 4));
    }
    audio_frames = 0;
}

static size_t audio_batch_cb(const int16_t *data, size_t frames) {
    size_t left = frames;
    while (left) {
        size_t space = 8192 - audio_frames;
        size_t n = left < space ? left : space;
        memcpy(audio_buf + audio_frames * 2, data, n * 4);
        audio_frames += n;
        data += n * 2;
        left -= n;
        if (audio_frames == 8192) flush_audio();
    }
    return frames;
}

static void audio_cb(int16_t l, int16_t r) {
    int16_t s[2] = {l, r};
    audio_batch_cb(s, 1);
}

static void input_poll_cb(void) {}

static bool menu_open;

static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || menu_open) return 0;
    if (device == RETRO_DEVICE_JOYPAD || device == RETRO_DEVICE_ANALOG) {
        static const int map[16] = {
            [RETRO_DEVICE_ID_JOYPAD_B] = B_B,         [RETRO_DEVICE_ID_JOYPAD_Y] = B_Y,
            [RETRO_DEVICE_ID_JOYPAD_SELECT] = B_SELECT, [RETRO_DEVICE_ID_JOYPAD_START] = B_START,
            [RETRO_DEVICE_ID_JOYPAD_UP] = B_UP,       [RETRO_DEVICE_ID_JOYPAD_DOWN] = B_DOWN,
            [RETRO_DEVICE_ID_JOYPAD_LEFT] = B_LEFT,   [RETRO_DEVICE_ID_JOYPAD_RIGHT] = B_RIGHT,
            [RETRO_DEVICE_ID_JOYPAD_A] = B_A,         [RETRO_DEVICE_ID_JOYPAD_X] = B_X,
            [RETRO_DEVICE_ID_JOYPAD_L] = B_L,         [RETRO_DEVICE_ID_JOYPAD_R] = B_R,
            [RETRO_DEVICE_ID_JOYPAD_L2] = B_L2,       [RETRO_DEVICE_ID_JOYPAD_R2] = B_R2,
            [RETRO_DEVICE_ID_JOYPAD_L3] = B_L3,       [RETRO_DEVICE_ID_JOYPAD_R3] = B_R3,
        };
        if (device == RETRO_DEVICE_ANALOG) {
            if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
                return id == RETRO_DEVICE_ID_ANALOG_X ? in.lx : in.ly;
            if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                return id == RETRO_DEVICE_ID_ANALOG_X ? in.rx : in.ry;
            return 0;
        }
        /* left stick doubles as d-pad for digital-only systems */
        bool dl = in.lx < -20000, dr = in.lx > 20000, du = in.ly < -20000, dd = in.ly > 20000;
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
            int16_t m = 0;
            for (int i = 0; i < 16; i++)
                if (in.down[map[i]]) m |= 1 << i;
            if (du) m |= 1 << RETRO_DEVICE_ID_JOYPAD_UP;
            if (dd) m |= 1 << RETRO_DEVICE_ID_JOYPAD_DOWN;
            if (dl) m |= 1 << RETRO_DEVICE_ID_JOYPAD_LEFT;
            if (dr) m |= 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT;
            return m;
        }
        if (id >= 16) return 0;
        if (id == RETRO_DEVICE_ID_JOYPAD_UP && du) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_DOWN && dd) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_LEFT && dl) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_RIGHT && dr) return 1;
        return in.down[map[id]];
    }
    return 0;
}

#define SYM(n)                                                    \
    do {                                                          \
        *(void **)&core.n = dlsym(core.h, "retro_" #n);           \
        if (!core.n) {                                            \
            fprintf(stderr, "missing symbol retro_%s\n", #n);     \
            return false;                                         \
        }                                                         \
    } while (0)

static bool load_core(void) {
    core.h = dlopen(core_path, RTLD_NOW | RTLD_LOCAL);
    if (!core.h) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return false;
    }
    SYM(init); SYM(deinit); SYM(get_system_info); SYM(get_system_av_info);
    SYM(set_environment); SYM(set_video_refresh); SYM(set_audio_sample);
    SYM(set_audio_sample_batch); SYM(set_input_poll); SYM(set_input_state);
    SYM(set_controller_port_device); SYM(reset); SYM(run); SYM(serialize_size);
    SYM(serialize); SYM(unserialize); SYM(load_game); SYM(unload_game);
    SYM(get_memory_data); SYM(get_memory_size);
    return true;
}

static const char *core_valid_ext;

static bool ext_supported(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || !core_valid_ext) return false;
    char list[1024], *save = NULL;
    snprintf(list, sizeof list, "%s", core_valid_ext);
    for (char *t = strtok_r(list, "|", &save); t; t = strtok_r(NULL, "|", &save))
        if (!strcasecmp(t, dot + 1) && strcasecmp(t, "zip")) return true;
    return false;
}

/* Unpack the first supported file of a .zip into /tmp (cartridge systems). */
static bool extract_zip(const char *zip, char *out, size_t outn) {
    mz_zip_archive za;
    memset(&za, 0, sizeof za);
    if (!mz_zip_reader_init_file(&za, zip, 0)) return false;
    bool ok = false;
    int n = (int)mz_zip_reader_get_num_files(&za);
    for (int i = 0; i < n && !ok; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&za, i, &st) || st.m_is_directory) continue;
        const char *fn = strrchr(st.m_filename, '/');
        fn = fn ? fn + 1 : st.m_filename;
        if (!ext_supported(fn)) continue;
        mkdirs("/tmp/rainey_rom");
        snprintf(out, outn, "/tmp/rainey_rom/%s", fn);
        ok = mz_zip_reader_extract_to_file(&za, i, out, 0);
    }
    mz_zip_reader_end(&za);
    return ok;
}

static void *read_file(const char *p, size_t *size) {
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *b = malloc(n > 0 ? n : 1);
    if (b && fread(b, 1, n, f) != (size_t)n) {
        free(b);
        b = NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return b;
}

static void sram_load(void) {
    size_t sz = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void *mem = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!sz || !mem) return;
    size_t n;
    void *b = read_file(srm_path, &n);
    if (!b) {
        /* first play of a game that came with a save next to it (e.g. from another device's
         * SD card): read it from there; new saves go to internal storage */
        char alt[1200];
        snprintf(alt, sizeof alt, "%s", rom_path);
        char *dot = strrchr(alt, '.');
        if (dot) {
            strcpy(dot, ".srm");
            b = read_file(alt, &n);
            if (b) fprintf(stderr, "sram: using %s\n", alt);
        }
    }
    if (b) {
        memcpy(mem, b, n < sz ? n : sz);
        free(b);
    }
}

static void sram_save(void) {
    size_t sz = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void *mem = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!sz || !mem) return;
    FILE *f = fopen(srm_path, "wb");
    if (!f) return;
    fwrite(mem, 1, sz, f);
    fclose(f);
    sync();
}

static int slot;
static char msg[128];
static Uint32 msg_until;

static void notify(const char *m) {
    snprintf(msg, sizeof msg, "%s", m);
    msg_until = SDL_GetTicks() + 1800;
}

static void state_save(void) {
    size_t sz = core.serialize_size();
    if (!sz) {
        notify("This core does not support save states");
        return;
    }
    void *b = malloc(sz);
    char p[1300];
    snprintf(p, sizeof p, "%s.st%d", state_base, slot);
    if (b && core.serialize(b, sz)) {
        FILE *f = fopen(p, "wb");
        if (f) {
            fwrite(b, 1, sz, f);
            fclose(f);
            sync();
            char m[64];
            snprintf(m, sizeof m, "Saved state slot %d", slot);
            notify(m);
        }
    } else {
        notify("Save state failed");
    }
    free(b);
}

static void state_load(void) {
    char p[1300];
    snprintf(p, sizeof p, "%s.st%d", state_base, slot);
    size_t n;
    void *b = read_file(p, &n);
    char m[64];
    if (b && core.unserialize(b, n)) snprintf(m, sizeof m, "Loaded state slot %d", slot);
    else snprintf(m, sizeof m, "No state in slot %d", slot);
    notify(m);
    free(b);
}

static void draw_frame(void) {
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    if (!frame_tex || !frame_w) return;
    float aspect = av.geometry.aspect_ratio > 0 ? av.geometry.aspect_ratio
                                                : (float)frame_w / frame_h;
    SDL_Rect dst;
    int sc = g_settings.scale;
    if (sc == 5) {
        dst = (SDL_Rect){0, 0, SCREEN_W, SCREEN_H};
    } else if (sc >= 1 && sc <= 4) {
        int w = frame_w * sc, h = frame_h * sc;
        while ((w > SCREEN_W || h > SCREEN_H) && sc > 1) {
            sc--;
            w = frame_w * sc;
            h = frame_h * sc;
        }
        dst = (SDL_Rect){(SCREEN_W - w) / 2, (SCREEN_H - h) / 2, w, h};
    } else {
        int w = SCREEN_W, h = (int)lroundf(SCREEN_W / aspect);
        if (h > SCREEN_H) {
            h = SCREEN_H;
            w = (int)lroundf(SCREEN_H * aspect);
        }
        dst = (SDL_Rect){(SCREEN_W - w) / 2, (SCREEN_H - h) / 2, w, h};
    }
    SDL_Rect src = {0, 0, frame_w, frame_h};
    SDL_RenderCopy(g_ren, frame_tex, &src, &dst);
    if (g_settings.game_scanlines) {
        SDL_Rect clip = dst;
        SDL_RenderSetClipRect(g_ren, &clip);
        gfx_scanlines(70);
        SDL_RenderSetClipRect(g_ren, NULL);
    }
}

/* ---------------- Rainey Options (pause menu) ---------------- */

enum { M_RESUME, M_RESET, M_SAVE, M_LOAD, M_SCALE0, M_SCALE_LAST = M_SCALE0 + 5, M_SCAN, M_QUIT, M_N };

static void menu_button(int x, int y, int w, int h, const char *a, const char *b, bool focus,
                        bool active) {
    if (focus) gfx_glow(x, y, w, h, 10, C_NEON_BLUE, 1);
    gfx_round_fill(x, y, w, h, 10, focus || active ? 0x0D3A52 : 0x0B1730, 240);
    gfx_round_ring(x, y, w, h, 10, focus ? 3 : 1, C_NEON_BLUE, focus ? 255 : 150);
    int fh = gfx_font_h(F_LABEL);
    if (b) {
        gfx_text_center(F_LABEL, a, x + w / 2, y + h / 2 - fh, C_TEXT, 255);
        gfx_text_center(F_BODY, b, x + w / 2, y + h / 2 + 2, active ? C_NEON_BLUE : 0xD6DCE6, 255);
    } else {
        gfx_text_center(F_LABEL, a, x + w / 2, y + (h - fh) / 2, C_TEXT, 255);
    }
}

static int menu_focus;

static void menu_draw(void) {
    draw_frame();
    gfx_fill(0, 0, SCREEN_W, SCREEN_H, 0x000000, 150);
    int px = 64, py = 20, pw = SCREEN_W - 128, ph = SCREEN_H - 40;
    gfx_round_fill(px, py, pw, ph, 18, 0x050A16, 245);
    gfx_round_ring(px, py, pw, ph, 18, 3, C_NEON_BLUE, 255);
    gfx_text_center(F_HUGE, "RAINEY OPTIONS", SCREEN_W / 2, py + 22, C_TEXT, 255);
    gfx_text_center(F_BODY, game_name, SCREEN_W / 2, py + 82, C_MUTED, 255);
    int x0 = px + 38, cw = (pw - 76 - 16) / 2;
    int y = py + 124;
    gfx_text(F_BODY, "GAME", x0, y, C_NEON_BLUE, 255);
    y += 34;
    char save[32], load[32];
    snprintf(save, sizeof save, "State slot %d", slot);
    snprintf(load, sizeof load, "State slot %d", slot);
    menu_button(x0, y, cw, 86, "Resume", "Back to game", menu_focus == M_RESUME, false);
    menu_button(x0 + cw + 16, y, cw, 86, "Reset", "Restart core", menu_focus == M_RESET, false);
    y += 100;
    menu_button(x0, y, cw, 86, "Save", save, menu_focus == M_SAVE, false);
    menu_button(x0 + cw + 16, y, cw, 86, "Load", load, menu_focus == M_LOAD, false);
    y += 106;
    gfx_text(F_BODY, "VIDEO SCALE", x0, y, C_NEON_BLUE, 255);
    y += 34;
    const char *sn[] = {"Auto", "1x", "2x", "3x", "4x", "Stretch"};
    int sw = (pw - 76 - 5 * 12) / 6;
    for (int i = 0; i < 6; i++) {
        bool act = g_settings.scale == i;
        menu_button(x0 + i * (sw + 12), y, sw, 86, sn[i], act ? "Active" : "Scale",
                    menu_focus == M_SCALE0 + i, act);
    }
    y += 106;
    gfx_text(F_BODY, "DISPLAY & EXIT", x0, y, C_NEON_BLUE, 255);
    y += 34;
    menu_button(x0, y, cw, 86, g_settings.game_scanlines ? "Scanlines On" : "Scanlines Off",
                "Toggle CRT lines", menu_focus == M_SCAN, false);
    menu_button(x0 + cw + 16, y, cw, 86, "Quit", "Return to Rainey", menu_focus == M_QUIT, false);
    gfx_text_center(F_SMALL, "Shortcuts: L/R change slot  •  MENU or SELECT+START options  •  B resume",
                    SCREEN_W / 2, py + ph - 34, C_MUTED, 255);
}

static void menu_input(void) {
    int f = menu_focus;
    /* grid navigation: rows [0,1] [2,3] [4..9] [10,11] */
    int row = f <= 1 ? 0 : f <= 3 ? 1 : f <= M_SCALE_LAST ? 2 : 3;
    int col = row == 2 ? f - M_SCALE0 : (row == 0 ? f : row == 1 ? f - 2 : f - M_SCAN);
    if (in.pressed[B_LEFT]) col--;
    if (in.pressed[B_RIGHT]) col++;
    if (in.pressed[B_UP]) {
        if (row == 3) col = col ? 5 : 0;
        else if (row == 2) col = col >= 3 ? 1 : 0;
        row--;
    } else if (in.pressed[B_DOWN]) {
        if (row == 1) col = col ? 5 : 0;
        else if (row == 2) col = col >= 3 ? 1 : 0;
        row++;
    }
    if (row < 0) row = 0;
    if (row > 3) row = 3;
    int maxc = row == 2 ? 5 : 1;
    if (col < 0) col = 0;
    if (col > maxc) col = maxc;
    menu_focus = row == 0 ? col : row == 1 ? 2 + col : row == 2 ? M_SCALE0 + col : M_SCAN + col;
    if (in.pressed[B_L]) slot = (slot + 9) % 10;
    if (in.pressed[B_R]) slot = (slot + 1) % 10;
    if (in.pressed[B_B] || in.pressed[B_MENU]) {
        menu_open = false;
        return;
    }
    if (!in.pressed[B_A]) return;
    switch (menu_focus) {
    case M_RESUME: menu_open = false; break;
    case M_RESET: core.reset(); menu_open = false; notify("Reset"); break;
    case M_SAVE: state_save(); menu_open = false; break;
    case M_LOAD: state_load(); menu_open = false; break;
    case M_SCAN: g_settings.game_scanlines = !g_settings.game_scanlines; settings_save(); break;
    case M_QUIT: quit_flag = true; break;
    default:
        if (menu_focus >= M_SCALE0 && menu_focus <= M_SCALE_LAST) {
            g_settings.scale = menu_focus - M_SCALE0;
            settings_save();
        }
    }
}

static void on_signal(int s) {
    (void)s;
    got_signal = 1;
}

static bool read_launch(const char *file) {
    FILE *f = fopen(file, "r");
    if (!f) return false;
    char *outs[] = {sys_id, core_path, rom_path, game_name};
    size_t sizes[] = {sizeof sys_id, sizeof core_path, sizeof rom_path, sizeof game_name};
    for (int i = 0; i < 4; i++) {
        if (!fgets(outs[i], (int)sizes[i], f)) {
            fclose(f);
            return false;
        }
        outs[i][strcspn(outs[i], "\r\n")] = 0;
    }
    fclose(f);
    return true;
}

static void show_error(const char *title, const char *detail) {
    Uint32 until = SDL_GetTicks() + 6000;
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), until)) {
        input_poll(&in);
        if (in.pressed[B_A] || in.pressed[B_B] || in.quit) break;
        gfx_background(SDL_GetTicks() / 1000.f);
        gfx_round_fill(80, 230, SCREEN_W - 160, 280, 10, 0x351023, 235);
        gfx_round_ring(80, 230, SCREEN_W - 160, 280, 10, 2, 0xFF6B9A, 255);
        gfx_text(F_TITLE, title, 110, 260, 0xFFC7D8, 255);
        gfx_text_wrap(F_BODY, detail, 110, 320, SCREEN_W - 220, 5, 30, C_TEXT, 255);
        gfx_text(F_SMALL, "Press A or B to return", 110, 470, C_MUTED, 255);
        SDL_RenderPresent(g_ren);
        SDL_Delay(16);
    }
}

int player_main(SDL_Window *win, SDL_Renderer *ren, const char *launch_file) {
    (void)win;
    (void)ren;
    if (!read_launch(launch_file)) return 1;
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);

    snprintf(system_dir, sizeof system_dir, "%s/bios", DATA_DIR);
    snprintf(save_dir, sizeof save_dir, "%s/saves/%s", DATA_DIR, sys_id);
    mkdirs(system_dir);
    mkdirs(save_dir);
    char states_dir[700];
    snprintf(states_dir, sizeof states_dir, "%s/states/%s", DATA_DIR, sys_id);
    mkdirs(states_dir);
    const char *base = strrchr(rom_path, '/');
    base = base ? base + 1 : rom_path;
    char stem[512];
    snprintf(stem, sizeof stem, "%s", base);
    char *dot = strrchr(stem, '.');
    if (dot) *dot = 0;
    snprintf(srm_path, sizeof srm_path, "%s/%s.srm", save_dir, stem);
    snprintf(state_base, sizeof state_base, "%s/%s", states_dir, stem);

    if (!load_core()) {
        show_error("Could not load core", core_path);
        return 1;
    }
    core.set_environment(env_cb);
    core.set_video_refresh(video_cb);
    core.set_audio_sample(audio_cb);
    core.set_audio_sample_batch(audio_batch_cb);
    core.set_input_poll(input_poll_cb);
    core.set_input_state(input_state_cb);
    core.init();

    struct retro_system_info si;
    memset(&si, 0, sizeof si);
    core.get_system_info(&si);
    core_valid_ext = si.valid_extensions;
    fprintf(stderr, "core %s %s, need_fullpath=%d\n", si.library_name, si.library_version,
            si.need_fullpath);

    const char *content = rom_path;
    const SystemDef *sd = NULL;
    for (int i = 0; i < NUM_SYSTEMS; i++)
        if (!strcmp(SYSTEMS[i].id, sys_id)) sd = &SYSTEMS[i];
    const char *ext = strrchr(rom_path, '.');
    if (sd && sd->extract_zip && ext && !strcasecmp(ext, ".zip") && !ext_supported("x.zip")) {
        if (!extract_zip(rom_path, tmp_rom, sizeof tmp_rom)) {
            show_error("Could not open archive", "No supported game file was found inside this .zip.");
            core.deinit();
            return 1;
        }
        content = tmp_rom;
    }

    struct retro_game_info gi = {content, NULL, 0, NULL};
    void *buf = NULL;
    if (!si.need_fullpath) {
        size_t n;
        buf = read_file(content, &n);
        gi.data = buf;
        gi.size = n;
    }
    if (!core.load_game(&gi)) {
        show_error("Game failed to start",
                   "The core could not load this file. Check that it is a valid dump and that "
                   "any BIOS files are in /mnt/UDISK/RaineyUI/bios.");
        core.deinit();
        free(buf);
        if (tmp_rom[0]) remove(tmp_rom);
        return 1;
    }
    core.get_system_av_info(&av);
    core.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    sram_load();
    fprintf(stderr, "av: %ux%u aspect %.3f fps %.3f rate %.1f\n", av.geometry.base_width,
            av.geometry.base_height, av.geometry.aspect_ratio, av.timing.fps,
            av.timing.sample_rate);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    audio_rate = (int)lround(av.timing.sample_rate > 0 ? av.timing.sample_rate : 44100);
    want.freq = audio_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    else fprintf(stderr, "audio: %s\n", SDL_GetError());

    double fps = av.timing.fps > 0 ? av.timing.fps : 60.0;
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 frame_ticks = (Uint64)(freq / fps);
    Uint64 next = SDL_GetPerformanceCounter();
    Uint32 sram_timer = SDL_GetTicks();
    bool combo_latch = false;

    while (!quit_flag && !got_signal) {
        input_poll(&in);
        if (in.quit) break;
        bool combo = in.down[B_SELECT] && in.down[B_START];
        if (in.pressed[B_MENU] || (combo && !combo_latch)) {
            if (!menu_open) {
                menu_open = true;
                menu_focus = M_RESUME;
                sram_save();
                if (audio_dev) SDL_PauseAudioDevice(audio_dev, 1);
                in.pressed[B_MENU] = false;
            }
        }
        combo_latch = combo;
        if (menu_open) {
            menu_input();
            menu_draw();
            if (!menu_open && audio_dev) {
                SDL_ClearQueuedAudio(audio_dev);
                SDL_PauseAudioDevice(audio_dev, 0);
            }
            if (g_shot_request[0]) {
                gfx_screenshot(g_shot_request);
                g_shot_request[0] = 0;
            }
            SDL_RenderPresent(g_ren);
            SDL_Delay(16);
            next = SDL_GetPerformanceCounter();
            continue;
        }
        core.run();
        flush_audio();
        draw_frame();
        if (msg[0] && !SDL_TICKS_PASSED(SDL_GetTicks(), msg_until)) {
            int w = gfx_text_w(F_BODY, msg) + 32;
            gfx_round_fill(16, 16, w, 44, 6, C_PANEL, 220);
            gfx_round_ring(16, 16, w, 44, 6, 1, C_NEON_BLUE, 200);
            gfx_text(F_BODY, msg, 32, 25, C_TEXT, 255);
        }
        if (g_shot_request[0]) {
            gfx_screenshot(g_shot_request);
            g_shot_request[0] = 0;
        }
        SDL_RenderPresent(g_ren);

        /* pacing: audio queue is the master clock, timer as fallback */
        if (audio_dev) {
            Uint32 bytes_per_sec = (Uint32)have.freq * 4;
            Uint32 high = bytes_per_sec / 12; /* ~83ms */
            Uint32 q = SDL_GetQueuedAudioSize(audio_dev);
            if (q > bytes_per_sec / 2) SDL_ClearQueuedAudio(audio_dev); /* recover after stalls */
            while (SDL_GetQueuedAudioSize(audio_dev) > high && !got_signal) SDL_Delay(1);
        } else {
            next += frame_ticks;
            Uint64 now = SDL_GetPerformanceCounter();
            if (next > now) SDL_Delay((Uint32)((next - now) * 1000 / freq));
            else next = now;
        }
        if (SDL_TICKS_PASSED(SDL_GetTicks(), sram_timer + 30000)) {
            sram_save();
            sram_timer = SDL_GetTicks();
        }
    }
    sram_save();
    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    core.unload_game();
    core.deinit();
    free(buf);
    if (tmp_rom[0]) remove(tmp_rom);
    if (frame_tex) SDL_DestroyTexture(frame_tex);
    return 0;
}
