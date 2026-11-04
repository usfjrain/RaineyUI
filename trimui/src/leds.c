/* LED lights for TrimUI Brick: per-zone color + animation, 5 presets.
 * Driver: /sys/class/led_anim (see its `help` node). */
#include "leds.h"
#include "rainey.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LED_SYS "/sys/class/led_anim"
#define LED_CFG DATA_DIR "/leds.cfg"

const char *const LED_ZONE_IDS[LED_ZONES] = {"l", "r", "m", "f1", "f2", "rear"};
const char *const LED_ZONE_NAMES[LED_ZONES] = {"Left", "Right", "Middle", "F1", "F2", "Rear"};
/* driver effect ids: 0 disable 1 linear 2 breath 3 sniff 4 static 5 blink1 6 blink2 7 blink3 */
const char *const LED_EFFECT_NAMES[LED_EFFECTS] = {"Off",    "Static",  "Breath",  "Pulse",
                                                   "Sniff",  "Blink 1", "Blink 2", "Blink 3"};
static const int effect_driver_id[LED_EFFECTS] = {0, 4, 2, 1, 3, 5, 6, 7};
const int LED_SPEEDS[LED_SPEED_COUNT] = {300, 600, 1000, 1500, 2000, 3000, 5000};

LedState led_cur;
LedState led_presets[LED_PRESETS];
bool led_preset_used[LED_PRESETS];
int led_active_preset = -1;

void leds_defaults(LedState *s) {
    memset(s, 0, sizeof *s);
    s->on = true;
    s->brightness = 40;
    const uint32_t c[LED_ZONES] = {0x21D4FD, 0x21D4FD, 0xB83BFF, 0xFF4FD8, 0xFF4FD8, 0xB83BFF};
    for (int z = 0; z < LED_ZONES; z++) {
        s->color[z] = c[z];
        s->effect[z] = 2; /* breath: Rainey neon */
        s->speed[z] = 4;
    }
}

static void wr(const char *node, const char *val) {
    char p[128];
    snprintf(p, sizeof p, LED_SYS "/%s", node);
    FILE *f = fopen(p, "w");
    if (!f) return;
    fputs(val, f);
    fclose(f);
}

void leds_apply(const LedState *s) {
    char v[32];
    wr("enable", s->on ? "1" : "0");
    wr("effect_enable", "1");
    int b = s->brightness < 0 ? 0 : s->brightness > 60 ? 60 : s->brightness;
    snprintf(v, sizeof v, "%d", b);
    wr("max_scale", v);
    wr("max_scale_lr", v);
    wr("max_scale_f1f2", v);
    wr("max_scale_rear", v);
    for (int z = 0; z < LED_ZONES; z++) {
        const char *id = LED_ZONE_IDS[z];
        char node[48];
        bool off = !s->on || s->effect[z] == 0;
        /* "disable" leaves the last frame lit, so "Off" is static black */
        snprintf(v, sizeof v, "%06X ", off ? 0 : (unsigned)(s->color[z] & 0xFFFFFF));
        snprintf(node, sizeof node, "effect_rgb_hex_%s", id);
        wr(node, v);
        int sp = s->speed[z] < LED_SPEED_COUNT ? s->speed[z] : 2;
        snprintf(v, sizeof v, "%d", LED_SPEEDS[sp]);
        snprintf(node, sizeof node, "effect_duration_%s", id);
        wr(node, v);
        snprintf(node, sizeof node, "effect_cycles_%s", id);
        wr(node, "-1");
        snprintf(v, sizeof v, "%d", off ? 4 : effect_driver_id[s->effect[z] % LED_EFFECTS]);
        snprintf(node, sizeof node, "effect_%s", id);
        wr(node, v);
    }
}

static void write_state(FILE *f, const char *tag, const LedState *s) {
    fprintf(f, "%s %d %d", tag, s->on, s->brightness);
    for (int z = 0; z < LED_ZONES; z++)
        fprintf(f, " %06X %d %d", (unsigned)s->color[z], s->effect[z], s->speed[z]);
    fprintf(f, "\n");
}

static bool read_state(const char *line, LedState *s) {
    int on, br, n = 0;
    if (sscanf(line, "%*s %d %d%n", &on, &br, &n) != 2) return false;
    LedState t;
    t.on = on;
    t.brightness = br;
    const char *p = line + n;
    for (int z = 0; z < LED_ZONES; z++) {
        unsigned c;
        int e, sp, k = 0;
        if (sscanf(p, " %x %d %d%n", &c, &e, &sp, &k) != 3) return false;
        t.color[z] = c & 0xFFFFFF;
        t.effect[z] = (uint8_t)(e >= 0 && e < LED_EFFECTS ? e : 0);
        t.speed[z] = (uint8_t)(sp >= 0 && sp < LED_SPEED_COUNT ? sp : 2);
        p += k;
    }
    *s = t;
    return true;
}

void leds_load(void) {
    leds_defaults(&led_cur);
    memset(led_preset_used, 0, sizeof led_preset_used);
    led_active_preset = -1;
    FILE *f = fopen(LED_CFG, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        int slot;
        if (!strncmp(line, "current ", 8)) {
            read_state(line, &led_cur);
        } else if (sscanf(line, "preset%d", &slot) == 1 && slot >= 1 && slot <= LED_PRESETS) {
            if (read_state(line, &led_presets[slot - 1])) led_preset_used[slot - 1] = true;
        } else if (sscanf(line, "active %d", &slot) == 1) {
            led_active_preset = slot >= 1 && slot <= LED_PRESETS ? slot - 1 : -1;
        }
    }
    fclose(f);
}

void leds_save(void) {
    mkdirs(DATA_DIR);
    FILE *f = fopen(LED_CFG ".tmp", "w");
    if (!f) return;
    fprintf(f, "# RaineyUI LED lights: on brightness then per zone (L R M F1 F2 Rear) color effect speed\n");
    write_state(f, "current", &led_cur);
    for (int i = 0; i < LED_PRESETS; i++) {
        if (!led_preset_used[i]) continue;
        char tag[16];
        snprintf(tag, sizeof tag, "preset%d", i + 1);
        write_state(f, tag, &led_presets[i]);
    }
    fprintf(f, "active %d\n", led_active_preset + 1);
    fclose(f);
    rename(LED_CFG ".tmp", LED_CFG);
    sync();
}

int leds_cli_apply(void) {
    leds_load();
    leds_apply(&led_cur);
    return 0;
}
