#include "fm_history.h"
#include <stdarg.h>

#define TAG "flipmesh"

void fm_history_add(FlipMeshApp* app, const char* text, uint32_t from, uint32_t to, bool is_tx) {
    if(!app || !text) return;

    furi_mutex_acquire(app->lock, FuriWaitForever);

    FMMessage* msg = &app->history.buf[app->history.head];
    snprintf(msg->text, sizeof(msg->text), "%s", text);
    msg->from      = from;
    msg->to        = to;
    msg->outgoing     = is_tx;
    msg->ts = furi_get_tick() / 1000; /* seconds since boot */

    app->history.head = (app->history.head + 1) % FM_MSG_HISTORY;
    if(app->history.count < FM_MSG_HISTORY) app->history.count++;

    furi_mutex_release(app->lock);
}

void fm_log(FlipMeshApp* app, const char* fmt, ...) {
    if(!app) return;

    char buf[FM_LOG_COLS];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    furi_mutex_acquire(app->lock, FuriWaitForever);
    snprintf(app->log[app->log_head], FM_LOG_COLS, "%s", buf);
    app->log_head = (app->log_head + 1) % FM_LOG_ROWS;
    if(app->log_frozen && app->log_scroll < FM_LOG_ROWS - 7) {
        app->log_scroll++;
    }
    furi_mutex_release(app->lock);

    FURI_LOG_I(TAG, "%s", buf);
}

void fm_status(FlipMeshApp* app, const char* msg) {
    if(!app || !msg) return;
    furi_mutex_acquire(app->lock, FuriWaitForever);
    snprintf(app->status_line, sizeof(app->status_line), "%s", msg);
    furi_mutex_release(app->lock);
    view_port_update(app->vp);
}
