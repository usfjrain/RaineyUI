/* Sort files dropped into /mnt/UDISK/RaineyUI/inbox (from the Mac app over USB or the
 * Wi-Fi upload page) into the right ROM / BIOS / art / background folders. */
#include "rainey.h"
#include "miniz.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define INBOX DATA_DIR "/inbox"

typedef struct {
    char *buf;
    size_t len, cap;
    int moved, unknown;
} Report;

static void rep(Report *r, const char *fmt, const char *a, const char *b) {
    char line[1200];
    int n = snprintf(line, sizeof line, fmt, a, b);
    if (n < 0) return;
    if (r->len + n + 2 > r->cap) {
        r->cap = (r->len + n + 2) * 2;
        r->buf = realloc(r->buf, r->cap);
    }
    memcpy(r->buf + r->len, line, n);
    r->len += n;
    r->buf[r->len++] = '\n';
    r->buf[r->len] = 0;
}

static const char *ext_of(const char *n) {
    const char *d = strrchr(n, '.');
    return d ? d + 1 : "";
}

static bool ext_in(const char *e, const char *list) {
    char k[24];
    if (!*e || strlen(e) > 16) return false;
    snprintf(k, sizeof k, ",%s,", e);
    for (char *p = k; *p; p++) *p = (char)tolower((unsigned char)*p);
    return strstr(list, k) != NULL;
}

static const char *sys_name_for_folder(const char *folder) {
    for (int i = 0; i < NUM_SYSTEMS; i++)
        for (int f = 0; f < 16 && SYSTEMS[i].folders[f]; f++)
            if (!strcmp(SYSTEMS[i].folders[f], folder)) return SYSTEMS[i].name;
    return folder;
}

/* console folder for a plain (non-archive) game file extension, or NULL */
static const char *folder_for_ext(const char *e) {
    if (ext_in(e, ",nes,fds,unf,unif,")) return "FC";
    if (ext_in(e, ",sfc,smc,fig,swc,bs,")) return "SFC";
    if (ext_in(e, ",md,gen,smd,68k,")) return "MD";
    if (ext_in(e, ",sms,")) return "MS";
    if (ext_in(e, ",gg,")) return "GG";
    if (ext_in(e, ",gb,gbc,dmg,")) return "GB";
    if (ext_in(e, ",gba,")) return "GBA";
    if (ext_in(e, ",z64,n64,v64,")) return "N64";
    if (ext_in(e, ",nds,")) return "NDS";
    if (ext_in(e, ",gdi,cdi,")) return "DC";
    if (ext_in(e, ",cso,")) return "PSP";
    if (ext_in(e, ",pbp,iso,")) return "PS";
    return NULL;
}

/* PSP UMD images carry "PSP GAME" as the ISO 9660 system identifier */
static bool iso_is_psp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char id[32] = {0};
    bool ok = fseek(f, 0x8008, SEEK_SET) == 0 && fread(id, 1, sizeof id, f) == sizeof id;
    fclose(f);
    return ok && !memcmp(id, "PSP GAME", 8);
}

/* PS1 discs converted for the PSP (EBOOT.PBP) have a PSISOIMG/PSTITLEIMG data section */
static bool pbp_is_psp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char h[40];
    char magic[8] = {0};
    bool ok = fread(h, 1, sizeof h, f) == sizeof h && !memcmp(h, "\0PBP", 4);
    if (ok) {
        long psar = h[36] | h[37] << 8 | h[38] << 16 | (long)h[39] << 24;
        ok = fseek(f, psar, SEEK_SET) == 0 && fread(magic, 1, 8, f) == 8;
    }
    fclose(f);
    return ok && memcmp(magic, "PSISOIMG", 8) && memcmp(magic, "PSTITLEI", 8);
}

static bool chd_is_dreamcast(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    static char buf[1 << 18];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    for (size_t i = 0; i + 4 <= n; i++)
        if (!memcmp(buf + i, "CHGD", 4)) return true; /* GD-ROM metadata tag */
    return false;
}

static const char *folder_for_zip(const char *path) {
    mz_zip_archive za;
    memset(&za, 0, sizeof za);
    if (!mz_zip_reader_init_file(&za, path, 0)) return "ARCADE";
    const char *res = NULL;
    int n = (int)mz_zip_reader_get_num_files(&za);
    for (int i = 0; i < n && !res; i++) {
        char name[512];
        mz_zip_reader_get_filename(&za, i, name, sizeof name);
        res = folder_for_ext(ext_of(name));
        if (!res && ext_in(ext_of(name), ",bin,") && n == 1) res = "MD";
    }
    mz_zip_reader_end(&za);
    return res ? res : "ARCADE"; /* MAME/FBNeo sets hold extension-less ROM chips */
}

static const char *bios_dest(const char *name);

/* Stock folder (e.g. "SFC") for a game file found in a folder the user picked in Settings →
 * ROM folders, or NULL when it isn't a game. Only reads the file; nothing is moved. */
const char *rom_folder_guess(const char *path) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *e = ext_of(name);
    if (!strcasecmp(name, "neogeo.zip") || bios_dest(name)) return NULL;
    if (ext_in(e, ",zip,")) return folder_for_zip(path);
    if (ext_in(e, ",7z,")) return "ARCADE";
    if (ext_in(e, ",chd,")) return chd_is_dreamcast(path) ? "DC" : "PS";
    if (ext_in(e, ",bin,")) return "MD"; /* disc track files are skipped by the caller */
    if (ext_in(e, ",iso,")) return iso_is_psp(path) ? "PSP" : "PS";
    if (ext_in(e, ",pbp,")) return pbp_is_psp(path) ? "PSP" : "PS";
    if (ext_in(e, ",cue,m3u,")) return "PS";
    return folder_for_ext(e);
}

static const char *bios_dest(const char *name) {
    static const char *dc[] = {"dc_boot.bin", "dc_flash.bin", "dc_bios.bin", "dc_nvmem.bin"};
    for (size_t i = 0; i < sizeof dc / sizeof *dc; i++)
        if (!strcasecmp(name, dc[i])) return DATA_DIR "/bios/dc";
    if (!strncasecmp(name, "scph", 4) || !strncasecmp(name, "psxonpsp", 8) ||
        !strcasecmp(name, "ps1_rom.bin") || !strcasecmp(name, "gba_bios.bin") ||
        !strcasecmp(name, "gb_bios.bin") || !strcasecmp(name, "gbc_bios.bin") ||
        !strcasecmp(name, "disksys.rom") || !strcasecmp(name, "bios7.bin") ||
        !strcasecmp(name, "bios9.bin") || !strcasecmp(name, "firmware.bin"))
        return DATA_DIR "/bios";
    return NULL;
}

/* does a game with this stem exist in a ROM folder? returns the folder name */
static const char *game_folder_for_stem(const char *stem) {
    for (int i = 0; i < NUM_SYSTEMS; i++)
        for (int f = 0; f < 16 && SYSTEMS[i].folders[f]; f++) {
            char dir[300];
            snprintf(dir, sizeof dir, "%s/%s", ROMS_INTERNAL, SYSTEMS[i].folders[f]);
            DIR *d = opendir(dir);
            if (!d) continue;
            struct dirent *e;
            const char *hit = NULL;
            size_t sl = strlen(stem);
            while ((e = readdir(d)))
                if (!strncmp(e->d_name, stem, sl) && (e->d_name[sl] == '.' || e->d_name[sl] == 0)) {
                    hit = SYSTEMS[i].folders[f];
                    break;
                }
            closedir(d);
            if (hit) return hit;
        }
    return NULL;
}

static bool move_to(const char *src, const char *dir, const char *name) {
    mkdirs(dir);
    char dst[1400];
    snprintf(dst, sizeof dst, "%s/%s", dir, name);
    struct stat st;
    if (stat(dst, &st) == 0 && S_ISDIR(st.st_mode)) {
        char cmd[3000];
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", dst); /* replace a re-uploaded disc folder */
        if (!strchr(dst, '\'')) system(cmd);
    }
    return rename(src, dst) == 0;
}

static void sort_file(Report *r, const char *path, const char *name) {
    const char *e = ext_of(name);
    char stem[512];
    snprintf(stem, sizeof stem, "%s", name);
    char *dot = strrchr(stem, '.');
    if (dot) *dot = 0;
    char dir[600];
    const char *bios = bios_dest(name);
    const char *folder = NULL;
    if (bios) {
        if (move_to(path, bios, name)) rep(r, "%s → BIOS (%s)", name, bios + strlen(DATA_DIR) + 1);
        r->moved++;
        return;
    }
    if (ext_in(e, ",png,jpg,jpeg,")) {
        const char *gf = game_folder_for_stem(stem);
        if (gf) {
            snprintf(dir, sizeof dir, "/mnt/UDISK/Imgs/%s", gf);
            if (move_to(path, dir, name)) rep(r, "%s → box art for %s", name, sys_name_for_folder(gf));
        } else if (move_to(path, DATA_DIR "/backgrounds", name)) {
            rep(r, "%s → Backgrounds%s", name, "");
        }
        r->moved++;
        return;
    }
    if (!strcasecmp(name, "neogeo.zip")) folder = "ARCADE";
    else if (ext_in(e, ",zip,")) folder = folder_for_zip(path);
    else if (ext_in(e, ",7z,")) folder = "ARCADE";
    else if (ext_in(e, ",chd,")) folder = chd_is_dreamcast(path) ? "DC" : "PS";
    else if (ext_in(e, ",bin,")) folder = "MD";
    else if (ext_in(e, ",iso,")) folder = iso_is_psp(path) ? "PSP" : "PS";
    else if (ext_in(e, ",pbp,")) folder = pbp_is_psp(path) ? "PSP" : "PS";
    else folder = folder_for_ext(e);
    if (folder) {
        snprintf(dir, sizeof dir, "%s/%s", ROMS_INTERNAL, folder);
        if (move_to(path, dir, name)) rep(r, "%s → %s", name, sys_name_for_folder(folder));
        r->moved++;
    } else if (move_to(path, "/mnt/UDISK/Downloads", name)) {
        rep(r, "%s → Downloads (unknown type)%s", name, "");
        r->unknown++;
    }
}

/* .gdi / .cue: move the index file plus the track files it names into Roms/<folder>/<stem>/ */
static void sort_disc(Report *r, const char *dirpath, const char *name, const char *folder) {
    char idx[1200], stem[512], dest[900];
    snprintf(idx, sizeof idx, "%s/%s", dirpath, name);
    snprintf(stem, sizeof stem, "%s", name);
    *strrchr(stem, '.') = 0;
    snprintf(dest, sizeof dest, "%s/%s/%s", ROMS_INTERNAL, folder, stem);
    mkdirs(dest);
    FILE *f = fopen(idx, "r");
    char line[1024];
    while (f && fgets(line, sizeof line, f)) {
        char *q1 = strchr(line, '"'), *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
        char track[512] = "";
        if (q1 && q2) {
            snprintf(track, sizeof track, "%.*s", (int)(q2 - q1 - 1), q1 + 1);
        } else if (!strcasecmp(ext_of(name), "gdi")) {
            int a, b, c, d;
            char t[512];
            if (sscanf(line, "%d %d %d %d %511s", &a, &b, &c, &d, t) == 5) snprintf(track, sizeof track, "%s", t);
        }
        if (!track[0] || strchr(track, '/')) continue;
        char src[1200];
        snprintf(src, sizeof src, "%s/%s", dirpath, track);
        if (access(src, F_OK) == 0) move_to(src, dest, track);
    }
    if (f) fclose(f);
    move_to(idx, dest, name);
    rep(r, "%s → %s (disc with tracks)", name, sys_name_for_folder(folder));
    r->moved++;
}

static void sort_dir(Report *r, const char *dirpath, int depth) {
    /* pass 1: disc index files first so their tracks travel with them */
    DIR *d = opendir(dirpath);
    if (!d) return;
    struct dirent *e;
    char path[1200];
    while ((e = readdir(d))) {
        const char *x = ext_of(e->d_name);
        if (e->d_name[0] == '.') continue;
        if (ext_in(x, ",gdi,")) sort_disc(r, dirpath, e->d_name, "DC");
        else if (ext_in(x, ",cue,")) sort_disc(r, dirpath, e->d_name, "PS");
    }
    rewinddir(d);
    /* animated background: a folder holding only images */
    if (depth == 1) {
        int imgs = 0, numbered = 0, other = 0;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.' || !strcmp(e->d_name, "fps.txt")) continue;
            const char *x = ext_of(e->d_name);
            if (ext_in(x, ",png,jpg,jpeg,")) {
                imgs++;
                if (x - 2 >= e->d_name && isdigit((unsigned char)x[-2])) numbered++; /* f0001.jpg */
            } else {
                other++;
            }
        }
        rewinddir(d);
        if (imgs > 1 && !other && numbered == imgs) {
            closedir(d);
            const char *nm = strrchr(dirpath, '/') + 1;
            if (move_to(dirpath, DATA_DIR "/backgrounds", nm)) rep(r, "%s/ → Backgrounds (animated)", nm, "");
            r->moved++;
            return;
        }
    }
    while ((e = readdir(d))) {
        if (!strncmp(e->d_name, "._", 2) || !strcmp(e->d_name, ".DS_Store")) { /* macOS litter */
            snprintf(path, sizeof path, "%s/%s", dirpath, e->d_name);
            remove(path);
            continue;
        }
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s", dirpath, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < 4) sort_dir(r, path, depth + 1);
        } else if (!strstr(e->d_name, ".part")) {
            sort_file(r, path, e->d_name);
        }
    }
    closedir(d);
    if (depth > 0) rmdir(dirpath); /* only removes it once empty */
}

/* returns a malloc'd human-readable report */
char *sort_inbox(int *moved) {
    Report r = {0};
    mkdirs(INBOX);
    sort_dir(&r, INBOX, 0);
    if (!r.moved && !r.unknown) rep(&r, "Nothing to sort — the inbox is empty.%s%s", "", "");
    sync();
    FILE *f = fopen("/tmp/rainey_rescan", "w"); /* tell a running RaineyUI to rescan */
    if (f) fclose(f);
    if (moved) *moved = r.moved + r.unknown;
    return r.buf;
}

int sort_cli(void) {
    int n = 0;
    char *s = sort_inbox(&n);
    fputs(s ? s : "", stdout);
    free(s);
    return 0;
}
