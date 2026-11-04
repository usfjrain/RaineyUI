#ifndef RAINEY_LEDS_H
#define RAINEY_LEDS_H
#include <stdbool.h>
#include <stdint.h>

#define LED_ZONES 6
#define LED_EFFECTS 8
#define LED_SPEED_COUNT 7
#define LED_PRESETS 5

typedef struct {
    bool on;
    int brightness;              /* 0-60 (driver limit) */
    uint32_t color[LED_ZONES];   /* 0xRRGGBB */
    uint8_t effect[LED_ZONES];   /* index into LED_EFFECT_NAMES */
    uint8_t speed[LED_ZONES];    /* index into LED_SPEEDS */
} LedState;

extern const char *const LED_ZONE_IDS[LED_ZONES];
extern const char *const LED_ZONE_NAMES[LED_ZONES];
extern const char *const LED_EFFECT_NAMES[LED_EFFECTS];
extern const int LED_SPEEDS[LED_SPEED_COUNT];

extern LedState led_cur;
extern LedState led_presets[LED_PRESETS];
extern bool led_preset_used[LED_PRESETS];
extern int led_active_preset;

void leds_defaults(LedState *s);
void leds_load(void);
void leds_save(void);
void leds_apply(const LedState *s);
int leds_cli_apply(void);

#endif
