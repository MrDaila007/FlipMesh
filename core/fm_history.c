// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#include "fm_history.h"
#include <stdarg.h>

#define TAG "flipmesh"

/*
 * Ring buffer layout:
 *   head  = index of the OLDEST message
 *   count = number of valid messages
 *   i-th message (0=oldest): buf[(head + i) % FM_MSG_HISTORY]
 *   write slot for new message:
 *     - if not full: buf[(head + count) % FM_MSG_HISTORY]
 *     - if full:     buf[head], then advance head (evict oldest)
 */

void fm_history_add(FlipMeshApp* app, const char* text, uint32_t from, uint32_t to, bool is_tx) {
    if(!app || !text) return;

    furi_mutex_acquire(app->lock, FuriWaitForever);

    uint8_t slot;
    if(app->history.count < FM_MSG_HISTORY) {
        slot = (app->history.head + app->history.count) % FM_MSG_HISTORY;
        app->history.count++;
    } else {
        /* Buffer full — overwrite the oldest slot and advance head */
        slot = app->history.head;
        app->history.head = (app->history.head + 1) % FM_MSG_HISTORY;
    }

    FMMessage* m = &app->history.buf[slot];
    snprintf(m->text, sizeof(m->text), "%s", text);
    m->from     = from;
    m->to       = to;
    m->outgoing = is_tx;
    m->ts       = (uint32_t)(furi_get_tick() / furi_kernel_get_tick_frequency());

    furi_mutex_release(app->lock);
}

void fm_log(FlipMeshApp* app, const char* fmt, ...) {
    if(!app) return;

    va_list args;
    va_start(args, fmt);
    char line[FM_LOG_COLS];
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    FURI_LOG_I(TAG, "%s", line);

    furi_mutex_acquire(app->lock, FuriWaitForever);

    /* Write into the next available log row */
    uint8_t row = app->log_head % FM_LOG_ROWS;
    snprintf(app->log[row], FM_LOG_COLS, "%s", line);
    app->log_head = (uint8_t)((app->log_head + 1) % FM_LOG_ROWS);

    /* When frozen, keep the scroll window tracking new entries */
    if(app->log_frozen) {
        uint8_t visible = 7; /* rows visible on screen */
        if(app->log_scroll + visible < FM_LOG_ROWS)
            app->log_scroll++;
    }

    furi_mutex_release(app->lock);
}

void fm_status(FlipMeshApp* app, const char* msg) {
    if(!app || !msg) return;
    furi_mutex_acquire(app->lock, FuriWaitForever);
    snprintf(app->status_line, sizeof(app->status_line), "%s", msg);
    furi_mutex_release(app->lock);
    view_port_update(app->vp);
}
