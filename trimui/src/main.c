#include "rainey.h"
#include "libretro.h"
#include "leds.h"
#include <dlfcn.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Keep libpthread in the global scope: libretro cores patched by
 * tools/glibc_downgrade.py resolve pthread and sem symbols (GLIBC_2.17) from it. */
static void *keep_pthread(void *a) { return a; }

/* GCC 11+ cores (snes9x, gambatte, fbneo) reference this libstdc++ 3.4.29 helper,
 * which the device's GCC 10 libstdc++ lacks. Exported from the executable. */
__attribute__((visibility("default"), noreturn)) void _ZSt28__throw_bad_array_new_lengthv(void) {
    fprintf(stderr, "fatal: std::bad_array_new_length\n");
    abort();
}

int main(int argc, char **argv) {
    pthread_t th;
    if (pthread_create(&th, NULL, keep_pthread, NULL) == 0) pthread_join(th, NULL);

    char exe[512];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) {
        exe[n] = 0;
        snprintf(g_appdir, sizeof g_appdir, "%s", dirname(exe));
    }
    if (argc >= 3 && !strcmp(argv[1], "--probe")) { /* check cores load on this firmware */
        int bad = 0;
        for (int i = 2; i < argc; i++) {
            void *h = dlopen(argv[i], RTLD_NOW | RTLD_LOCAL);
            void (*info)(struct retro_system_info *) = h ? dlsym(h, "retro_get_system_info") : NULL;
            if (info) {
                struct retro_system_info si = {0};
                info(&si);
                printf("OK   %s (%s %s)\n", argv[i], si.library_name, si.library_version);
            } else {
                printf("FAIL %s: %s\n", argv[i], dlerror());
                bad++;
            }
        }
        return bad;
    }
    if (argc >= 2 && !strcmp(argv[1], "--leds-apply")) return leds_cli_apply(); /* boot hook */
    if (argc >= 2 && !strcmp(argv[1], "--sort-inbox")) return sort_cli();      /* Send to Brick app */
    if (argc >= 3 && !strcmp(argv[1], "--racmd")) { /* send a RetroArch network command */
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(55355)};
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ssize_t r = sendto(fd, argv[2], strlen(argv[2]), 0, (struct sockaddr *)&a, sizeof a);
        close(fd);
        return r < 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "--unzip")) return unzip_cli(argv[2], argv[3]);           /* installer */
    if (argc >= 4 && !strcmp(argv[1], "--patch-core")) return patch_core_cli(argv[2], argv[3]); /* installer */
    bool play = argc >= 3 && !strcmp(argv[1], "--play");
    bool install = argc >= 3 && !strcmp(argv[1], "--install-ui");

    settings_load();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
    SDL_Window *win = SDL_CreateWindow("RaineyUI", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!win) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        return 1;
    }
    Uint32 flags = SDL_RENDERER_ACCELERATED | (play ? 0 : SDL_RENDERER_PRESENTVSYNC);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, flags);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) {
        fprintf(stderr, "renderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_RendererInfo ri;
    SDL_GetRendererInfo(ren, &ri);
    int ww, wh;
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    fprintf(stderr, "video driver %s, renderer %s, output %dx%d\n", SDL_GetCurrentVideoDriver(),
            ri.name, ww, wh);
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    if (!gfx_init(ren)) return 1;
    input_init();

    int rc = play ? player_main(win, ren, argv[2])
             : install ? install_ui_main(win, ren, argv[2])
                       : ui_main(win, ren);

    gfx_quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return rc;
}
