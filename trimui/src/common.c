#include "rainey.h"
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <strings.h>

/* Systems the Brick can run with bundled software-rendered libretro cores.
 * Folder names follow the stock TrimUI layout so an existing Roms/ tree works. */
const SystemDef SYSTEMS[] = {
    {"nes", "NES", {"FC", "NES", "nes", "fds", "famicom", NULL}, ",nes,fds,unf,unif,zip,", "cores/fceumm_libretro.so",
     "nes.png", true, "rainey", "FCEUmm"},
    {"snes", "SNES", {"SFC", "SNES", "snes", "sfc", NULL}, ",sfc,smc,fig,swc,bs,zip,", "cores/snes9x_libretro.so",
     "snes.png", true, "rainey", "Snes9x"},
    {"genesis", "Genesis", {"MD", "MS", "GG", "SMS", "megadrive", "genesis", "mastersystem", "gamegear", "sg1000", NULL}, ",md,gen,smd,bin,sms,gg,sg,68k,zip,",
     "cores/genesis_plus_gx_libretro.so", "sega_genesis.png", true, "rainey", "Genesis Plus GX"},
    {"playstation", "PlayStation", {"PS", "PS1", "psx", "ps1", NULL}, ",cue,chd,pbp,m3u,iso,img,bin,",
     "cores/pcsx_rearmed_libretro.so", "ps1.png", false, "rainey", "PCSX ReARMed"},
    {"dreamcast", "Dreamcast", {"DC", "dreamcast", NULL}, ",gdi,cdi,chd,cue,m3u,",
     "emus/RetroArch/.retroarch/cores/flycast_libretro.so", "sega_dreamcast.png", false,
     "retroarch", "Flycast (RetroArch)"},
    {"arcade", "Arcade", {"ARCADE", "NEOGEO", "FBNEO", "CPS1", "CPS2", "CPS3", "arcade", "fbneo", "neogeo", "cps1", "cps2", "cps3", "mame", NULL}, ",zip,7z,", "cores/fbneo_libretro.so",
     "arcade.png", false, "rainey", "FinalBurn Neo"},
    {"gb", "Game Boy", {"GB", "GBC", "gb", "gbc", NULL}, ",gb,gbc,dmg,zip,", "cores/gambatte_libretro.so",
     "gameboy.png", true, "rainey", "Gambatte"},
    {"gba", "Game Boy Advance", {"GBA", "gba", NULL}, ",gba,zip,", "cores/mgba_libretro.so",
     "game_boy_advance.png", true, "rainey", "mGBA"},
    {"n64", "Nintendo 64", {"N64", "n64", NULL}, ",z64,n64,v64,zip,7z,",
     "emus/RetroArch/.retroarch/cores/mupen64plus_libretro.so", "nintendo_64.png", false,
     "retroarch", "Mupen64Plus GLES2 (RetroArch)"},
    {"nds", "Nintendo DS", {"NDS", "nds", NULL}, ",nds,zip,7z,", "emus/drastic/drastic_2.5.2.2",
     "nintendo_ds.png", false, "drastic", "DraStic"},
};
const int NUM_SYSTEMS = sizeof SYSTEMS / sizeof SYSTEMS[0];

Settings g_settings = {.scanlines = false, .game_scanlines = false, .swap_ab = false,
                       .smooth = false, .scale = 0, .background = "", .bg_dim = 1};
char g_appdir[512] = ".";

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

void mkdirs(const char *path) {
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

void settings_load(void) {
    g_settings.bg_dim = 1;
    FILE *f = fopen(DATA_DIR "/settings.cfg", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = line, *val = eq + 1;
        int v = atoi(val);
        if (!strcmp(k, "scanlines")) g_settings.scanlines = v;
        else if (!strcmp(k, "game_scanlines")) g_settings.game_scanlines = v;
        else if (!strcmp(k, "swap_ab")) g_settings.swap_ab = v;
        else if (!strcmp(k, "smooth")) g_settings.smooth = v;
        else if (!strcmp(k, "scale")) g_settings.scale = (v >= 0 && v <= 5) ? v : 0;
        else if (!strcmp(k, "bg_dim")) g_settings.bg_dim = (v >= 0 && v <= 2) ? v : 1;
        else if (!strcmp(k, "dc_res")) g_settings.dc_res = (v >= 0 && v <= 4) ? v : 0;
        else if (!strcmp(k, "background"))
            snprintf(g_settings.background, sizeof g_settings.background, "%s", val);
    }
    fclose(f);
}

void settings_save(void) {
    mkdirs(DATA_DIR);
    FILE *f = fopen(DATA_DIR "/settings.cfg", "w");
    if (!f) return;
    fprintf(f, "scanlines=%d\ngame_scanlines=%d\nswap_ab=%d\nsmooth=%d\nscale=%d\nbg_dim=%d\ndc_res=%d\nbackground=%s\n",
            g_settings.scanlines, g_settings.game_scanlines, g_settings.swap_ab,
            g_settings.smooth, g_settings.scale, g_settings.bg_dim, g_settings.dc_res,
            g_settings.background);
    fclose(f);
    sync();
}

/* ---------------- input ---------------- */

static SDL_GameController *pad;
static SDL_Joystick *joy;
static bool cur[B_COUNT];
static Uint32 repeat_at[B_COUNT];
static char dbg_queue[1024];
static int dbg_pos;
char g_shot_request[256];

SDL_GameController *input_controller(void) { return pad; }

static void open_pad(void) {
    if (pad || joy) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            pad = SDL_GameControllerOpen(i);
            if (pad) {
                fprintf(stderr, "controller: %s\n", SDL_GameControllerName(pad));
                return;
            }
        }
    }
    if (SDL_NumJoysticks() > 0) {
        joy = SDL_JoystickOpen(0);
        if (joy) fprintf(stderr, "joystick (raw): %s\n", SDL_JoystickName(joy));
    }
}

void input_init(void) {
    char db[600];
    snprintf(db, sizeof db, "%s/gamecontrollerdb.txt", g_appdir);
    if (file_exists(db)) SDL_GameControllerAddMappingsFromFile(db);
    open_pad();
}

static Button map_sdl_button(int b) {
    /* SDL uses Xbox positional names. Brick face buttons are Nintendo-labelled:
     * east = A, south = B, north = X, west = Y. */
    Button r;
    switch (b) {
    case SDL_CONTROLLER_BUTTON_B: r = B_A; break;
    case SDL_CONTROLLER_BUTTON_A: r = B_B; break;
    case SDL_CONTROLLER_BUTTON_Y: r = B_X; break;
    case SDL_CONTROLLER_BUTTON_X: r = B_Y; break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return B_L;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return B_R;
    case SDL_CONTROLLER_BUTTON_BACK: return B_SELECT;
    case SDL_CONTROLLER_BUTTON_START: return B_START;
    case SDL_CONTROLLER_BUTTON_GUIDE: return B_MENU;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return B_L3;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return B_R3;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return B_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return B_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return B_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return B_RIGHT;
    default: return B_COUNT;
    }
    if (g_settings.swap_ab) {
        if (r == B_A) r = B_B;
        else if (r == B_B) r = B_A;
        else if (r == B_X) r = B_Y;
        else if (r == B_Y) r = B_X;
    }
    return r;
}

static Button map_raw_button(int b) {
    /* evdev order for the Xbox-style TRIMUI Player1 device */
    static const Button m[] = {B_B, B_A, B_X, B_Y, B_L, B_R, B_SELECT, B_START, B_MENU, B_L3, B_R3};
    if (b < 0 || b >= (int)(sizeof m / sizeof m[0])) return B_COUNT;
    Button r = m[b];
    if (g_settings.swap_ab) {
        if (r == B_A) r = B_B;
        else if (r == B_B) r = B_A;
        else if (r == B_X) r = B_Y;
        else if (r == B_Y) r = B_X;
    }
    return r;
}

static Button map_key(SDL_Keycode k) {
    switch (k) {
    case SDLK_UP: return B_UP;
    case SDLK_DOWN: return B_DOWN;
    case SDLK_LEFT: return B_LEFT;
    case SDLK_RIGHT: return B_RIGHT;
    case SDLK_RETURN: case SDLK_SPACE: return B_A;
    case SDLK_BACKSPACE: case SDLK_ESCAPE: return B_B;
    case SDLK_HOME: case SDLK_AC_HOME: case SDLK_MENU: case SDLK_APPLICATION: return B_MENU;
    default: return B_COUNT;
    }
}

static void debug_inject(InputState *in) {
    if (!dbg_queue[dbg_pos]) {
        FILE *f = fopen(DEBUG_INPUT_FILE, "r");
        if (!f) return;
        size_t n = fread(dbg_queue, 1, sizeof dbg_queue - 1, f);
        dbg_queue[n] = 0;
        fclose(f);
        remove(DEBUG_INPUT_FILE);
        dbg_pos = 0;
    }
    char tok[256];
    int n = 0;
    while (dbg_queue[dbg_pos] && (dbg_queue[dbg_pos] == ' ' || dbg_queue[dbg_pos] == '\n'))
        dbg_pos++;
    while (dbg_queue[dbg_pos] && dbg_queue[dbg_pos] != ' ' && dbg_queue[dbg_pos] != '\n' &&
           n < 255)
        tok[n++] = dbg_queue[dbg_pos++];
    tok[n] = 0;
    if (!n) return;
    static const char *names[B_COUNT] = {"up", "down", "left", "right", "a", "b", "x", "y", "l",
                                         "r", "l2", "r2", "select", "start", "menu", "l3", "r3"};
    for (int i = 0; i < B_COUNT; i++)
        if (!strcmp(tok, names[i])) in->pressed[i] = true;
    if (!strncmp(tok, "shot:", 5)) snprintf(g_shot_request, sizeof g_shot_request, "%s", tok + 5);
    if (!strcmp(tok, "quit")) in->quit = true;
}

void input_poll(InputState *in) {
    memset(in->pressed, 0, sizeof in->pressed);
    in->quit = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        Button b = B_COUNT;
        bool down = false;
        switch (e.type) {
        case SDL_QUIT: in->quit = true; break;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_JOYDEVICEADDED: open_pad(); break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            b = map_sdl_button(e.cbutton.button);
            down = e.type == SDL_CONTROLLERBUTTONDOWN;
            break;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            if (!pad) {
                b = map_raw_button(e.jbutton.button);
                down = e.type == SDL_JOYBUTTONDOWN;
            }
            break;
        case SDL_JOYHATMOTION:
            if (!pad) {
                int v = e.jhat.value;
                cur[B_UP] = v & SDL_HAT_UP;
                cur[B_DOWN] = v & SDL_HAT_DOWN;
                cur[B_LEFT] = v & SDL_HAT_LEFT;
                cur[B_RIGHT] = v & SDL_HAT_RIGHT;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (!e.key.repeat) {
                b = map_key(e.key.keysym.sym);
                down = e.type == SDL_KEYDOWN;
            }
            break;
        }
        if (b != B_COUNT) {
            if (down && !cur[b]) {
                in->pressed[b] = true;
                repeat_at[b] = SDL_GetTicks() + 320;
            }
            cur[b] = down;
        }
    }
    /* analog */
    if (pad) {
        in->lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        in->ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        in->rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
        in->ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
        bool l2 = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000;
        bool r2 = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
        if (l2 && !cur[B_L2]) in->pressed[B_L2] = true;
        if (r2 && !cur[B_R2]) in->pressed[B_R2] = true;
        cur[B_L2] = l2;
        cur[B_R2] = r2;
    } else if (joy) {
        in->lx = SDL_JoystickGetAxis(joy, 0);
        in->ly = SDL_JoystickGetAxis(joy, 1);
        in->rx = SDL_JoystickNumAxes(joy) > 3 ? SDL_JoystickGetAxis(joy, 3) : 0;
        in->ry = SDL_JoystickNumAxes(joy) > 4 ? SDL_JoystickGetAxis(joy, 4) : 0;
        bool l2 = SDL_JoystickNumAxes(joy) > 2 && SDL_JoystickGetAxis(joy, 2) > 12000;
        bool r2 = SDL_JoystickNumAxes(joy) > 5 && SDL_JoystickGetAxis(joy, 5) > 12000;
        if (l2 && !cur[B_L2]) in->pressed[B_L2] = true;
        if (r2 && !cur[B_R2]) in->pressed[B_R2] = true;
        cur[B_L2] = l2;
        cur[B_R2] = r2;
    }
    /* d-pad auto-repeat for menus */
    Uint32 now = SDL_GetTicks();
    for (int b = B_UP; b <= B_RIGHT; b++) {
        if (cur[b] && !in->pressed[b] && SDL_TICKS_PASSED(now, repeat_at[b])) {
            in->pressed[b] = true;
            repeat_at[b] = now + 85;
        }
    }
    for (int b = B_L; b <= B_R; b++) {
        if (cur[b] && !in->pressed[b] && SDL_TICKS_PASSED(now, repeat_at[b])) {
            in->pressed[b] = true;
            repeat_at[b] = now + 180;
        }
    }
    memcpy(in->down, cur, sizeof cur);
    debug_inject(in);
}

/* ---------------- BIOS from an inserted SD card ----------------
 * Emulators save data into their BIOS folder (Dreamcast VMU, flash), so the BIOS lives on
 * internal storage. Copy the ones RaineyUI's emulators use from the card if missing.
 * Only reads from the card. */
static bool copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;
    char tmp[700];
    snprintf(tmp, sizeof tmp, "%s.tmp", dst);
    FILE *out = fopen(tmp, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    static char buf[1 << 16];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, n, out) != n) ok = false;
    fclose(in);
    if (fclose(out) != 0) ok = false;
    if (ok) ok = rename(tmp, dst) == 0;
    else remove(tmp);
    return ok;
}

static int import_dir(const char *srcdir, const char *const *names, const char *const *dests) {
    DIR *d = opendir(srcdir);
    if (!d) return 0;
    int copied = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        for (int i = 0; names[i]; i++) {
            if (strcasecmp(e->d_name, names[i])) continue;
            char src[600], dst[600];
            snprintf(src, sizeof src, "%s/%s", srcdir, e->d_name);
            snprintf(dst, sizeof dst, "%s", dests[i]);
            char dir[600];
            snprintf(dir, sizeof dir, "%s", dst);
            *strrchr(dir, '/') = 0;
            mkdirs(dir);
            if (!file_exists(dst) && copy_file(src, dst)) {
                fprintf(stderr, "bios: imported %s\n", src);
                copied++;
            }
        }
    }
    closedir(d);
    return copied;
}

int bios_import_from_sd(void) {
    static const char *const top[] = {"scph5501.bin", "scph5500.bin", "scph5502.bin", "scph1001.bin",
                                      "scph7001.bin", "scph101.bin",  "gba_bios.bin", "neogeo.zip",
                                      "dc_boot.bin",  "dc_flash.bin", NULL};
    static const char *const top_dst[] = {
        DATA_DIR "/bios/scph5501.bin", DATA_DIR "/bios/scph5500.bin", DATA_DIR "/bios/scph5502.bin",
        DATA_DIR "/bios/scph1001.bin", DATA_DIR "/bios/scph7001.bin", DATA_DIR "/bios/scph101.bin",
        DATA_DIR "/bios/gba_bios.bin", DATA_DIR "/bios/fbneo/neogeo.zip", DATA_DIR "/bios/dc/dc_boot.bin",
        DATA_DIR "/bios/dc/dc_flash.bin"};
    static const char *const dc[] = {"dc_boot.bin", "dc_flash.bin", NULL};
    static const char *const dc_dst[] = {DATA_DIR "/bios/dc/dc_boot.bin", DATA_DIR "/bios/dc/dc_flash.bin"};
    int n = 0;
    n += import_dir(SDCARD_ROOT "/bios/dc", dc, dc_dst); /* prefer the dc/ subfolder copies */
    n += import_dir(SDCARD_ROOT "/bios", top, top_dst);
    n += import_dir(SDCARD_ROOT "/BIOS", top, top_dst);
    if (n) sync();
    return n;
}
