#include "fm_channel.h"
#include "fm_history.h"

#define TAG "flipmesh"

static FMChanMeta chan_meta[FM_CHAN_META_MAX];

void fm_ch_init(FlipMeshApp* app) {
    if(!app) return;
    app->active_ch = 0;
    app->num_ch    = 3;
    for(uint8_t i = 0; i < FM_CHAN_META_MAX; i++) {
        chan_meta[i].name[0] = '\0';
        chan_meta[i].active  = false;
        chan_meta[i].role    = (i == 0) ? 1 : 2;
    }
}

void fm_ch_next(FlipMeshApp* app) {
    if(!app) return;
    app->active_ch = (uint8_t)((app->active_ch + 1) % app->num_ch);
    char s[40];
    snprintf(s, sizeof(s), "Channel: %s", fm_ch_label(app, app->active_ch));
    fm_status(app, s);
    fm_log(app, "Switched to %s", fm_ch_label(app, app->active_ch));
}

void fm_ch_set(FlipMeshApp* app, uint8_t idx) {
    if(!app || idx >= FM_FM_MAX_CHANNELS) return;
    app->active_ch = idx;
    if(idx >= app->num_ch) app->num_ch = idx + 1;
}

void fm_ch_set_meta(FlipMeshApp* app, uint8_t idx,
                    const char* name, uint8_t role) {
    (void)app;
    if(idx >= FM_CHAN_META_MAX || !name) return;
    strncpy(chan_meta[idx].name, name, sizeof(chan_meta[idx].name) - 1);
    chan_meta[idx].name[sizeof(chan_meta[idx].name) - 1] = '\0';
    chan_meta[idx].active = true;
    chan_meta[idx].role   = role;
}

const char* fm_ch_label(FlipMeshApp* app, uint8_t idx) {
    (void)app;
    if(idx >= FM_CHAN_META_MAX) return "?";
    if(chan_meta[idx].name[0] != '\0') return chan_meta[idx].name;
    static char fb[8];
    snprintf(fb, sizeof(fb), "Ch %u", (unsigned)idx);
    return fb;
}
