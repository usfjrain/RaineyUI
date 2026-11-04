/* On-device installer support (Apps/RaineyUI-Installer on an SD card):
 *   rainey --install-ui <worker.sh>   confirm screen, runs the worker, shows its progress
 *   rainey --unzip <zip> <dir>        extract a zip (the firmware has no unzip)
 *   rainey --patch-core <in> <out>    C port of tools/glibc_downgrade.py
 */
#include "rainey.h"
#include "miniz.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#define STATUS_FILE "/tmp/rainey_install.status"
#define WORKER_LOG "/tmp/rainey_install.log"

extern char g_shot_request[256];

/* ---------------- --unzip ---------------- */

int unzip_cli(const char *zip, const char *outdir) {
    mz_zip_archive za;
    memset(&za, 0, sizeof za);
    if (!mz_zip_reader_init_file(&za, zip, 0)) {
        fprintf(stderr, "unzip: cannot open %s\n", zip);
        return 1;
    }
    int n = (int)mz_zip_reader_get_num_files(&za), bad = 0;
    for (int i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&za, i, &st)) { bad++; continue; }
        if (strstr(st.m_filename, "..")) continue; /* never write outside outdir */
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", outdir, st.m_filename);
        if (mz_zip_reader_is_file_a_directory(&za, i)) { mkdirs(path); continue; }
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", path);
        char *slash = strrchr(dir, '/');
        if (slash) { *slash = 0; mkdirs(dir); }
        if (!mz_zip_reader_extract_to_file(&za, i, path, 0)) {
            fprintf(stderr, "unzip: failed %s\n", st.m_filename);
            bad++;
        }
    }
    mz_zip_reader_end(&za);
    return bad ? 1 : 0;
}

/* ---------------- --patch-core (see tools/glibc_downgrade.py) ---------------- */

static uint32_t elf_hash(const char *s) {
    uint32_t h = 0, g;
    for (; *s; s++) {
        h = (h << 4) + (unsigned char)*s;
        if ((g = h & 0xF0000000u)) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

static uint16_t rd16(const unsigned char *p) { return p[0] | p[1] << 8; }
static uint32_t rd32(const unsigned char *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }
static uint64_t rd64(const unsigned char *p) { return rd32(p) | (uint64_t)rd32(p + 4) << 32; }
static void wr16(unsigned char *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void wr32(unsigned char *p, uint32_t v) { wr16(p, v); wr16(p + 2, v >> 16); }

/* offset of a whole NUL-terminated string s inside the string table, or -1 */
static long find_str(const unsigned char *tab, size_t size, const char *s) {
    size_t l = strlen(s) + 1;
    for (size_t o = 1; o + l <= size; o++)
        if (tab[o - 1] == 0 && !memcmp(tab + o, s, l)) return (long)o;
    return -1;
}

static bool all_digits(const char *s) {
    if (!*s) return false;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return false;
    return true;
}

int patch_core_cli(const char *in, const char *out) {
    FILE *f = fopen(in, "rb");
    if (!f) { perror(in); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *d = malloc(size);
    if (!d || fread(d, 1, size, f) != (size_t)size) { fclose(f); free(d); return 1; }
    fclose(f);
    if (size < 64 || memcmp(d, "\x7f" "ELF", 4) || d[4] != 2 || d[5] != 1) {
        fprintf(stderr, "patch-core: %s is not a 64-bit little-endian ELF\n", in);
        free(d);
        return 1;
    }
    uint64_t shoff = rd64(d + 0x28);
    uint16_t shentsize = rd16(d + 0x3A), shnum = rd16(d + 0x3C);
    int patched = 0;
    for (int i = 0; i < shnum; i++) {
        unsigned char *sh = d + shoff + (uint64_t)i * shentsize;
        if (rd32(sh + 4) != 0x6FFFFFFE) continue; /* SHT_GNU_verneed */
        uint64_t vr = rd64(sh + 24);
        uint32_t link = rd32(sh + 40), cnt = rd32(sh + 44);
        unsigned char *strsh = d + shoff + (uint64_t)link * shentsize;
        unsigned char *tab = d + rd64(strsh + 24);
        size_t tabsize = rd64(strsh + 32);
        uint64_t p = vr;
        for (uint32_t v = 0; v < cnt; v++) {
            uint16_t vn_cnt = rd16(d + p + 2);
            uint32_t vn_aux = rd32(d + p + 8), vn_next = rd32(d + p + 12);
            uint64_t a = p + vn_aux;
            for (uint16_t k = 0; k < vn_cnt; k++) {
                unsigned char *ax = d + a;
                uint32_t name_off = rd32(ax + 8), next = rd32(ax + 12);
                const char *name = (const char *)tab + name_off;
                const char *target = NULL;
                if (!strncmp(name, "GLIBC_2.", 8) && all_digits(name + 8) && atoi(name + 8) > 33)
                    target = "GLIBC_2.17";
                else if (!strncmp(name, "GLIBCXX_3.4.", 12) && all_digits(name + 12) && atoi(name + 12) > 28)
                    target = "GLIBCXX_3.4";
                if (target) {
                    long toff = find_str(tab, tabsize, target);
                    if (toff < 0) { /* overwrite in place; the target is never longer */
                        size_t nl = strlen(name), tl = strlen(target);
                        if (tl > nl) { free(d); return 1; }
                        memset(tab + name_off, 0, nl);
                        memcpy(tab + name_off, target, tl);
                        toff = name_off;
                    }
                    wr32(ax, elf_hash(target));
                    wr16(ax + 4, rd16(ax + 4) & ~0x2); /* clear VER_FLG_WEAK */
                    wr32(ax + 8, (uint32_t)toff);
                    patched++;
                }
                if (!next) break;
                a += next;
            }
            if (!vn_next) break;
            p += vn_next;
        }
    }
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s.tmp", out);
    f = fopen(tmp, "wb");
    bool ok = f && fwrite(d, 1, size, f) == (size_t)size;
    if (f) ok = (fclose(f) == 0) && ok;
    free(d);
    if (!ok || rename(tmp, out) != 0) { perror(out); unlink(tmp); return 1; }
    printf("patched %d version need(s)\n", patched);
    return 0;
}

/* ---------------- --install-ui ---------------- */

typedef struct {
    char state[16]; /* RUN, DONE, FAIL */
    int pct;
    char title[160], detail[200];
} Status;

static bool read_status(Status *s) {
    FILE *f = fopen(STATUS_FILE, "r");
    if (!f) return false;
    char line[256];
    int n = 0;
    while (n < 4 && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (n == 0) snprintf(s->state, sizeof s->state, "%s", line);
        if (n == 1) s->pct = atoi(line);
        if (n == 2) snprintf(s->title, sizeof s->title, "%s", line);
        if (n == 3) snprintf(s->detail, sizeof s->detail, "%s", line);
        n++;
    }
    fclose(f);
    if (n < 3) return false;
    if (n < 4) s->detail[0] = 0;
    return true;
}

static pid_t start_worker(const char *worker) {
    unlink(STATUS_FILE);
    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(WORKER_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
        execl("/bin/sh", "sh", worker, (char *)NULL);
        _exit(127);
    }
    return pid;
}

int install_ui_main(SDL_Window *win, SDL_Renderer *ren, const char *worker) {
    (void)win;
    (void)ren;
    bg_scan();
    bg_select_name("Rainey (animated)");
    SDL_Texture *logo = gfx_load("res/logo.png");
    bool update = file_exists("/mnt/UDISK/Apps/RaineyUI/rainey");
    enum { CONFIRM, RUNNING, FINISHED } phase = CONFIRM;
    Status st = {"RUN", 0, "Starting…", ""};
    pid_t pid = -1;
    float shown = 0;
    Uint32 t0 = SDL_GetTicks();
    for (;;) {
        InputState in;
        input_poll(&in);
        if (phase == CONFIRM) {
            if (in.pressed[B_A]) {
                pid = start_worker(worker);
                phase = RUNNING;
            } else if (in.pressed[B_B] || in.quit) {
                break;
            }
        } else if (phase == RUNNING) {
            read_status(&st);
            int ws;
            bool alive = pid > 0 && waitpid(pid, &ws, WNOHANG) == 0;
            if (!strcmp(st.state, "DONE") || !strcmp(st.state, "FAIL")) {
                phase = FINISHED;
            } else if (!alive) {
                read_status(&st);
                if (strcmp(st.state, "DONE") && strcmp(st.state, "FAIL")) {
                    snprintf(st.state, sizeof st.state, "FAIL");
                    snprintf(st.title, sizeof st.title, "The installer stopped unexpectedly.");
                    snprintf(st.detail, sizeof st.detail, "Details: /mnt/UDISK/RaineyUI/install.log");
                }
                phase = FINISHED;
            }
        } else if (in.pressed[B_A] || in.pressed[B_B] || in.quit) {
            break;
        }

        float t = (SDL_GetTicks() - t0) / 1000.0f;
        gfx_background(t);
        int cx = SCREEN_W / 2;
        if (logo) gfx_image_fit(logo, cx - 220, 70, 440, 170, 255);
        bool done = phase == FINISHED && !strcmp(st.state, "DONE");
        bool failed = phase == FINISHED && !done;
        const char *heading = phase == CONFIRM ? (update ? "Update RaineyUI" : "Install RaineyUI")
                              : done           ? (update ? "RaineyUI is updated!" : "RaineyUI is installed!")
                              : failed         ? "Installation stopped"
                                               : (update ? "Updating RaineyUI…" : "Installing RaineyUI…");
        gfx_text_center(F_HUGE, heading, cx, 280, failed ? C_NEON_PINK : C_TEXT, 255);
        int px = 112, pw = SCREEN_W - 224, py = 470;
        if (phase == CONFIRM) {
            const char *lines[] = {
                "RaineyUI installs to internal storage.",
                "Your games, saves and SD card are not changed.",
                "Emulators download over Wi-Fi (a few minutes).",
            };
            for (int i = 0; i < 3; i++) gfx_text_center(F_BODY, lines[i], cx, 370 + i * 34, C_MUTED, 255);
            const char *h[] = {"A", update ? "Update" : "Install", "B", "Cancel"};
            ui_hints(h, 2);
        } else {
            float target = done ? 100.0f : (float)st.pct;
            shown += (target - shown) * 0.15f;
            gfx_text_center(F_LABEL, st.title, cx, 380, failed ? C_TEXT : C_NEON_BLUE, 255);
            gfx_text_center(F_SMALL, st.detail, cx, 420, C_MUTED, 255);
            if (!failed) {
                gfx_round_fill(px, py, pw, 28, 14, C_PANEL, 230);
                int fw = (int)(pw * shown / 100.0f);
                if (fw > 28) gfx_round_fill(px, py, fw, 28, 14, done ? C_NEON_PURPLE : C_NEON_BLUE, 255);
                gfx_round_ring(px, py, pw, 28, 14, 2, C_NEON_PURPLE, 160);
                char pct[16];
                snprintf(pct, sizeof pct, "%d%%", (int)(shown + 0.5f));
                gfx_text_center(F_BODY, pct, cx, py + 44, C_TEXT, 255);
            }
            if (done) {
                gfx_text_center(F_BODY, "Open Apps → RaineyUI to play. You can delete the installer.",
                                cx, 580, C_TEXT, 255);
                static const char *h[] = {"A", "Finish"};
                ui_hints(h, 1);
            } else if (failed) {
                static const char *h[] = {"A", "Close"};
                ui_hints(h, 1);
            } else {
                gfx_text_center(F_SMALL, "Keep the Brick on and connected to Wi-Fi.", cx, 580, C_MUTED, 255);
            }
        }
        SDL_RenderPresent(g_ren);
        if (g_shot_request[0]) {
            gfx_screenshot(g_shot_request);
            g_shot_request[0] = 0;
        }
    }
    if (pid > 0 && phase == RUNNING) kill(pid, SIGTERM);
    if (logo) SDL_DestroyTexture(logo);
    bg_shutdown();
    return 0;
}
