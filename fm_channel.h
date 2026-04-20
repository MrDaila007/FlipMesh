#pragma once
#include "flipmesh.h"

/* Per-channel metadata populated from device config sync */
typedef struct {
    char    name[12];   /* user-set label, or empty */
    bool    active;     /* received from device */
    uint8_t role;       /* 0=disabled 1=primary 2=secondary */
} FMChanMeta;

#define FM_CHAN_META_MAX FM_MAX_CHANNELS

void        fm_ch_init(FlipMeshApp* app);
void        fm_ch_next(FlipMeshApp* app);
void        fm_ch_set(FlipMeshApp* app, uint8_t idx);
void        fm_ch_set_meta(FlipMeshApp* app, uint8_t idx,
                           const char* name, uint8_t role);
const char* fm_ch_label(FlipMeshApp* app, uint8_t idx);
