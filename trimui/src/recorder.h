#ifndef RAINEY_RECORDER_H
#define RAINEY_RECORDER_H
#include <stdbool.h>
#include <stdint.h>

#define REC_DIR "/mnt/UDISK/Recordings"
#define REC_CFG "/mnt/UDISK/RaineyUI/recorder.cfg"
#define REC_PID "/tmp/rainey_rec.pid"
#define REC_STATE "/tmp/rainey_rec.state" /* "idle" or "rec\n<start time>\n<path>" */
#define REC_CMD "/tmp/rainey_rec.cmd"     /* "start" / "stop" from the app */
#define REC_MIN_FREE_MB 300
#define REC_FPS_N 3

extern const int REC_FPS[REC_FPS_N];

typedef struct {
    bool enabled; /* service runs (also at boot) */
    bool half;    /* 512x384 instead of 1024x768 */
    int fps;      /* index into REC_FPS */
} RecConfig;

void rec_config_load(RecConfig *c);
void rec_config_save(const RecConfig *c);
int rec_daemon_main(void);

/* sample tables of a recording (our own Motion-JPEG .mov) for the in-app player */
typedef struct {
    int w, h, count, noffsets, ntimes;
    uint32_t *sizes, *times;
    uint64_t *offsets;
    uint32_t duration_ms;
} MovInfo;

bool mov_info_load(MovInfo *mi, const char *path);
void mov_info_free(MovInfo *mi);

int recorder_app_main(void);

#endif
