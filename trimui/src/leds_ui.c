/* RaineyUI "LED Lights" screen: per-light color + animation + speed, 5 presets. */
#include "leds.h"
#include "rainey.h"
#include <stdio.h>
#include <string.h>

enum { ROW_POWER, ROW_BRIGHT, ROW_ZONE0, ROW_PRESETS = ROW_ZONE0 + LED_ZONES, ROW_N };

static int row, col, preset_sel;
static bool picker;
static int pick_idx;          /* 0-23 palette, 24-26 custom R/G/B */
static uint32_t pick_orig;
static uint32_t pick_custom;

static const uint32_t PALETTE[24] = {
    0xFF0000, 0xFF5A1F, 0xFF9500, 0xFFC400, 0xFFFF00, 0x9DFF00, 0x00FF3C, 0x00FF9D,
    0x00FFD5, 0x00FFFF, 0x21D4FD, 0x0090FF, 0x0033FF, 0x4B00FF, 0x7A2CFF, 0xB83BFF,
    0xE100FF, 0xFF00FF, 0xFF4FD8, 0xFF0080, 0xFF3355, 0xFFFFFF, 0xFFD8A0, 0xB0E0FF};

static void changed(void) {
    led_active_preset = -1;
    leds_apply(&led_cur);
}

void leds_screen_enter(void) {
    row = ROW_ZONE0;
    col = 0;
    picker = false;
    preset_sel = led_active_preset >= 0 ? led_active_preset : 0;
}

static void swatch(int x, int y, int w, int h, uint32_t c, bool focus) {
    if (focus) {
        gfx_round_ring(x - 7, y - 7, w + 14, h + 14, 11, 4, 0xFFFFFF, 255);
        gfx_round_ring(x - 10, y - 10, w + 20, h + 20, 13, 3, C_NEON_PINK, 255);
    }
    gfx_round_fill(x, y, w, h, 6, c, 255);
    gfx_round_ring(x, y, w, h, 6, 1, 0x5A6378, 255);
}

static void cell(int x, int y, int w, int h, const char *label, bool focus) {
    if (focus) gfx_glow(x, y, w, h, 6, C_NEON_BLUE, 1);
    gfx_round_fill(x, y, w, h, 6, focus ? 0x0D3A52 : C_PANEL, 230);
    gfx_round_ring(x, y, w, h, 6, focus ? 3 : 1, focus ? C_NEON_BLUE : C_NEON_PURPLE, focus ? 255 : 110);
    int fh = gfx_font_h(F_BODY);
    gfx_text_center(F_BODY, label, x + w / 2, y + (h - fh) / 2, C_TEXT, 255);
    if (focus) {
        gfx_text(F_BODY, "‹", x + 8, y + (h - fh) / 2, C_NEON_BLUE, 255);
        gfx_text(F_BODY, "›", x + w - 20, y + (h - fh) / 2, C_NEON_BLUE, 255);
    }
}

static void speed_label(int sp, char *out, size_t n) {
    snprintf(out, n, "%.1fs", LED_SPEEDS[sp] / 1000.0);
}

static void draw_picker(void) {
    gfx_fill(0, 0, SCREEN_W, SCREEN_H, 0x000000, 160);
    int px = 140, py = 120, pw = SCREEN_W - 280, ph = 500;
    gfx_round_fill(px, py, pw, ph, 16, 0x050A16, 248);
    gfx_round_ring(px, py, pw, ph, 16, 3, C_NEON_BLUE, 255);
    char t[64];
    snprintf(t, sizeof t, "%s light color", LED_ZONE_NAMES[row - ROW_ZONE0]);
    gfx_text_center(F_TITLE, t, SCREEN_W / 2, py + 20, C_TEXT, 255);
    int sw = 72, gap = 14, gx = px + (pw - (8 * sw + 7 * gap)) / 2, gy = py + 80;
    for (int i = 0; i < 24; i++)
        swatch(gx + (i % 8) * (sw + gap), gy + (i / 8) * (sw - 16 + gap), sw, sw - 16, PALETTE[i],
               pick_idx == i);
    int cy = gy + 3 * (sw - 16 + gap) + 20;
    gfx_text(F_LABEL, "Custom", gx, cy + 12, C_NEON_BLUE, 255);
    const char *ch[3] = {"R", "G", "B"};
    for (int k = 0; k < 3; k++) {
        int v = (pick_custom >> (16 - 8 * k)) & 255;
        char lbl[16];
        snprintf(lbl, sizeof lbl, "%s %3d", ch[k], v);
        cell(gx + 130 + k * 150, cy, 136, 50, lbl, pick_idx == 24 + k);
    }
    swatch(gx + 130 + 3 * 150, cy, 90, 50, pick_custom, false);
    gfx_text_center(F_SMALL,
                    "D-pad pick  •  Custom: ←→ channel, L/R −/+16, L2/R2 −/+1  •  A done  •  B cancel",
                    SCREEN_W / 2, py + ph - 40, C_MUTED, 255);
}

void leds_screen_draw(void) {
    gfx_text(F_HUGE, "LED Lights", 28, 18, C_TEXT, 255);
    gfx_text(F_BODY, "Color and animation for each light, plus 5 presets", 28, 76, C_MUTED, 255);
    gfx_neon_button(SCREEN_W - 28 - 120, 24, 120, 56, "B Back", false);

    int y = 118;
    bool f = row == ROW_POWER;
    if (f) gfx_round_fill(20, y - 4, SCREEN_W - 40, 54, 8, C_NEON_BLUE, 26);
    gfx_text(F_LABEL, "Lights", 36, y + 8, C_TEXT, 255);
    /* toggle */
    int tx = 300;
    gfx_round_fill(tx, y, 78, 44, 22, led_cur.on ? C_NEON_BLUE : 0x2A3350, 255);
    if (f) gfx_round_ring(tx - 4, y - 4, 86, 52, 26, 2, C_NEON_PINK, 255);
    gfx_round_fill(led_cur.on ? tx + 38 : tx + 4, y + 4, 36, 36, 18, led_cur.on ? 0 : 0xA7B0C0, 255);

    y += 58;
    f = row == ROW_BRIGHT;
    if (f) gfx_round_fill(20, y - 4, SCREEN_W - 40, 54, 8, C_NEON_BLUE, 26);
    gfx_text(F_LABEL, "Brightness", 36, y + 8, C_TEXT, 255);
    int bx = 300, bw = 420;
    gfx_round_fill(bx, y + 14, bw, 16, 8, 0x2A3350, 255);
    gfx_round_fill(bx, y + 14, bw * led_cur.brightness / 60, 16, 8, C_NEON_BLUE, 255);
    gfx_round_fill(bx + bw * led_cur.brightness / 60 - 12, y + 10, 24, 24, 12, f ? C_NEON_PINK : C_TEXT, 255);
    char bv[16];
    snprintf(bv, sizeof bv, "%d%%", led_cur.brightness * 100 / 60);
    gfx_text(F_LABEL, bv, bx + bw + 24, y + 8, C_TEXT, 255);

    y += 66;
    const int cx[3] = {230, 420, 680}, cw[3] = {160, 230, 160};
    gfx_text(F_SMALL, "LIGHT", 36, y, C_NEON_BLUE, 255);
    gfx_text(F_SMALL, "COLOR", cx[0], y, C_NEON_BLUE, 255);
    gfx_text(F_SMALL, "ANIMATION", cx[1], y, C_NEON_BLUE, 255);
    gfx_text(F_SMALL, "SPEED", cx[2], y, C_NEON_BLUE, 255);
    y += 26;
    for (int z = 0; z < LED_ZONES; z++) {
        bool rf = row == ROW_ZONE0 + z;
        if (rf) gfx_round_fill(20, y - 3, SCREEN_W - 40, 50, 8, C_NEON_BLUE, 22);
        gfx_text(F_LABEL, LED_ZONE_NAMES[z], 36, y + 8, rf ? C_TEXT : 0xD6DCE6, 255);
        swatch(cx[0], y, cw[0], 44, led_cur.color[z], rf && col == 0);
        cell(cx[1], y, cw[1], 44, LED_EFFECT_NAMES[led_cur.effect[z]], rf && col == 1);
        char sp[16];
        speed_label(led_cur.speed[z], sp, sizeof sp);
        cell(cx[2], y, cw[2], 44, sp, rf && col == 2);
        y += 50;
    }

    y += 8;
    gfx_text(F_LABEL, "Presets", 36, y, C_NEON_BLUE, 255);
    gfx_text(F_SMALL, "A load  •  Y save current  •  X clear", 190, y + 6, C_MUTED, 255);
    y += 36;
    int pw = (SCREEN_W - 56 - 4 * 14) / 5;
    for (int i = 0; i < LED_PRESETS; i++) {
        int x = 28 + i * (pw + 14);
        bool pf = row == ROW_PRESETS && preset_sel == i;
        bool act = led_active_preset == i;
        if (pf) gfx_glow(x, y, pw, 84, 8, C_NEON_BLUE, 1);
        gfx_round_fill(x, y, pw, 84, 8, pf ? 0x0D3A52 : C_PANEL, 235);
        gfx_round_ring(x, y, pw, 84, 8, pf ? 3 : 1, act ? C_NEON_PINK : pf ? C_NEON_BLUE : C_NEON_PURPLE,
                       255);
        char name[24];
        snprintf(name, sizeof name, act ? "Preset %d ●" : "Preset %d", i + 1);
        gfx_text_center(F_BODY, name, x + pw / 2, y + 8, C_TEXT, 255);
        if (led_preset_used[i]) {
            const LedState *p = &led_presets[i];
            int dw = 22, dg = 4, dx = x + (pw - (LED_ZONES * dw + (LED_ZONES - 1) * dg)) / 2;
            for (int z = 0; z < LED_ZONES; z++) {
                bool off = !p->on || p->effect[z] == 0;
                gfx_round_fill(dx + z * (dw + dg), y + 46, dw, dw, 11, off ? 0x1A1F2E : p->color[z], 255);
                if (off) gfx_round_ring(dx + z * (dw + dg), y + 46, dw, dw, 11, 1, 0x5A6378, 255);
            }
        } else {
            gfx_text_center(F_SMALL, "Empty", x + pw / 2, y + 48, C_MUTED, 255);
        }
    }

    static const char *h1[] = {"←→", "Column", "A/L/R", "Change", "SELECT", "Copy to all", "B", "Back"};
    static const char *h2[] = {"←→", "Preset", "A", "Load", "Y", "Save", "X", "Clear", "B", "Back"};
    if (row == ROW_PRESETS) ui_hints(h2, 5);
    else ui_hints(h1, 4);
    if (picker) draw_picker();
}

static void picker_input(InputState *in) {
    int z = row - ROW_ZONE0;
    if (in->pressed[B_B]) {
        led_cur.color[z] = pick_orig;
        picker = false;
        changed();
        return;
    }
    if (in->pressed[B_A]) {
        picker = false;
        return;
    }
    bool custom = pick_idx >= 24;
    if (!custom) {
        if (in->pressed[B_LEFT] && pick_idx % 8) pick_idx--;
        if (in->pressed[B_RIGHT] && pick_idx % 8 < 7) pick_idx++;
        if (in->pressed[B_UP] && pick_idx >= 8) pick_idx -= 8;
        if (in->pressed[B_DOWN]) pick_idx = pick_idx + 8 < 24 ? pick_idx + 8 : 24 + (pick_idx % 8) * 3 / 8;
    } else {
        int k = pick_idx - 24;
        if (in->pressed[B_LEFT] && k > 0) pick_idx--;
        if (in->pressed[B_RIGHT] && k < 2) pick_idx++;
        if (in->pressed[B_UP]) pick_idx = 16 + k * 3;
        int d = (in->pressed[B_R] ? 16 : 0) - (in->pressed[B_L] ? 16 : 0) + (in->pressed[B_R2] ? 1 : 0) -
                (in->pressed[B_L2] ? 1 : 0);
        if (d) {
            int sh = 16 - 8 * k;
            int v = (int)((pick_custom >> sh) & 255) + d;
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            pick_custom = (pick_custom & ~(0xFFu << sh)) | ((uint32_t)v << sh);
        }
    }
    uint32_t c = pick_idx < 24 ? PALETTE[pick_idx] : pick_custom;
    if (c != led_cur.color[z]) {
        led_cur.color[z] = c;
        changed(); /* live preview on the device */
    }
}

/* returns true when the screen should close */
bool leds_screen_input(InputState *in) {
    if (picker) {
        picker_input(in);
        return false;
    }
    if (in->pressed[B_B]) {
        leds_save();
        return true;
    }
    if (in->pressed[B_UP] && row > 0) row--;
    if (in->pressed[B_DOWN] && row < ROW_N - 1) row++;

    if (row == ROW_POWER) {
        if (in->pressed[B_A] || in->pressed[B_LEFT] || in->pressed[B_RIGHT]) {
            led_cur.on = !led_cur.on;
            changed();
        }
    } else if (row == ROW_BRIGHT) {
        int d = (in->pressed[B_RIGHT] || in->pressed[B_R] ? 5 : 0) - (in->pressed[B_LEFT] || in->pressed[B_L] ? 5 : 0);
        if (d) {
            led_cur.brightness += d;
            if (led_cur.brightness < 0) led_cur.brightness = 0;
            if (led_cur.brightness > 60) led_cur.brightness = 60;
            changed();
        }
    } else if (row < ROW_PRESETS) {
        int z = row - ROW_ZONE0;
        if (in->pressed[B_LEFT] && col > 0) col--;
        if (in->pressed[B_RIGHT] && col < 2) col++;
        int d = (in->pressed[B_R] ? 1 : 0) - (in->pressed[B_L] ? 1 : 0);
        if (in->pressed[B_A] && col != 0) d = 1;
        if (col == 0 && in->pressed[B_A]) {
            picker = true;
            pick_orig = led_cur.color[z];
            pick_custom = led_cur.color[z];
            pick_idx = 24;
            for (int i = 0; i < 24; i++)
                if (PALETTE[i] == led_cur.color[z]) pick_idx = i;
        } else if (col == 0 && d) {
            int i = 0;
            for (int k = 0; k < 24; k++)
                if (PALETTE[k] == led_cur.color[z]) i = k;
            led_cur.color[z] = PALETTE[(i + 24 + d) % 24];
            changed();
        } else if (col == 1 && d) {
            led_cur.effect[z] = (uint8_t)((led_cur.effect[z] + LED_EFFECTS + d) % LED_EFFECTS);
            changed();
        } else if (col == 2 && d) {
            int s = led_cur.speed[z] + d;
            led_cur.speed[z] = (uint8_t)(s < 0 ? 0 : s >= LED_SPEED_COUNT ? LED_SPEED_COUNT - 1 : s);
            changed();
        }
        if (in->pressed[B_SELECT]) {
            for (int k = 0; k < LED_ZONES; k++) {
                led_cur.color[k] = led_cur.color[z];
                led_cur.effect[k] = led_cur.effect[z];
                led_cur.speed[k] = led_cur.speed[z];
            }
            changed();
            ui_toast("Copied to all lights");
        }
    } else {
        if (in->pressed[B_LEFT] && preset_sel > 0) preset_sel--;
        if (in->pressed[B_RIGHT] && preset_sel < LED_PRESETS - 1) preset_sel++;
        char m[48];
        if (in->pressed[B_Y]) {
            led_presets[preset_sel] = led_cur;
            led_preset_used[preset_sel] = true;
            led_active_preset = preset_sel;
            leds_save();
            snprintf(m, sizeof m, "Saved to Preset %d", preset_sel + 1);
            ui_toast(m);
        } else if (in->pressed[B_A]) {
            if (led_preset_used[preset_sel]) {
                led_cur = led_presets[preset_sel];
                leds_apply(&led_cur);
                led_active_preset = preset_sel;
                leds_save();
                snprintf(m, sizeof m, "Loaded Preset %d", preset_sel + 1);
            } else {
                snprintf(m, sizeof m, "Preset %d is empty: press Y to save", preset_sel + 1);
            }
            ui_toast(m);
        } else if (in->pressed[B_X] && led_preset_used[preset_sel]) {
            led_preset_used[preset_sel] = false;
            if (led_active_preset == preset_sel) led_active_preset = -1;
            leds_save();
            snprintf(m, sizeof m, "Cleared Preset %d", preset_sel + 1);
            ui_toast(m);
        }
    }
    return false;
}
