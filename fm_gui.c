// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#include "fm_gui.h"
#include "fm_notify.h"
#include "fm_uart.h"
#include "fm_protocol.h"
#include "fm_history.h"
#include "fm_roster.h"
#include "fm_position.h"
#include "fm_channel.h"
#include "fm_settings.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <string.h>
#include <stdio.h>

/* ── Constants ────────────────────────────────────────────────────────────── */

static const uint32_t baud_options[] = {
    9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};
#define BAUD_COUNT ((uint8_t)(sizeof(baud_options) / sizeof(baud_options[0])))

static const char* lmh_names[]       = {"Scroll", "Wrap"};
static const char* hb_names[]        = {"10s", "30s", "60s"};
static const char* conn_labels[]     = {"--", "SYNC", "OK"};

/* ── Drawing helpers ──────────────────────────────────────────────────────── */

static uint8_t baud_to_idx(uint32_t baud) {
    for(uint8_t i = 0; i < BAUD_COUNT; i++) {
        if(baud_options[i] == baud) return i;
    }
    return 4; /* default 115200 */
}

static void draw_str_ellipsis(Canvas* canvas, int x, int y, int max_w, const char* s) {
    if(!s || max_w <= 0) return;
    if(canvas_string_width(canvas, s) <= max_w) {
        canvas_draw_str(canvas, x, y, s);
        return;
    }
    char buf[48];
    size_t n = strlen(s);
    if(n >= sizeof(buf) - 4) n = sizeof(buf) - 5;
    memcpy(buf, s, n);
    while(n > 0) {
        buf[n] = buf[n + 1] = buf[n + 2] = '.';
        buf[n + 3] = '\0';
        if(canvas_string_width(canvas, buf) <= max_w) {
            canvas_draw_str(canvas, x, y, buf);
            return;
        }
        n--;
    }
    canvas_draw_str(canvas, x, y, "...");
}

/* RSSI strength: 0-3 dots */
static uint8_t rssi_dots(int16_t rssi) {
    if(rssi >= -70) return 3;
    if(rssi >= -90) return 2;
    if(rssi >= -110) return 1;
    return 0;
}

static void draw_rssi_dots(Canvas* canvas, int x, int y, uint8_t dots) {
    /* 3 small filled/empty circles side by side */
    for(uint8_t i = 0; i < 3; i++) {
        int cx = x + i * 5;
        if(i < dots) {
            canvas_draw_disc(canvas, cx, y, 2);
        } else {
            canvas_draw_circle(canvas, cx, y, 2);
        }
    }
}

/* Status bar: top 10 px */
static void draw_statusbar(Canvas* canvas, FlipMeshApp* app) {
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);

    /* Connection state */
    const char* cstate = conn_labels[(uint8_t)app->conn < 3 ? app->conn : 0];
    canvas_draw_str(canvas, 1, 8, cstate);

    /* Own node name or ID */
    char own[8];
    if(app->self_short[0]) {
        snprintf(own, sizeof(own), "%s", app->self_short);
    } else if(app->self_id) {
        snprintf(own, sizeof(own), "!%04lX", (unsigned long)(app->self_id & 0xFFFF));
    } else {
        snprintf(own, sizeof(own), "?");
    }
    int own_w = canvas_string_width(canvas, own);
    canvas_draw_str(canvas, 64 - own_w / 2, 8, own);

    /* Channel */
    char ch[6];
    snprintf(ch, sizeof(ch), "ch%u", (unsigned)app->active_ch);
    canvas_draw_str(canvas, 90, 8, ch);

    /* RSSI dots */
    if(app->has_signal) {
        draw_rssi_dots(canvas, 112, 5, rssi_dots((int16_t)app->last_rssi));
    }

    canvas_set_color(canvas, ColorBlack);
}

/* Page navigation header (below statusbar) */
static void draw_nav_header(Canvas* canvas, FlipMeshApp* app, const char* title) {
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);

    /* Left/right arrows */
    canvas_draw_str(canvas, 1, 20, "<");
    canvas_draw_str(canvas, 121, 20, ">");

    /* Title centered */
    int tw = canvas_string_width(canvas, title);
    canvas_draw_str(canvas, (128 - tw) / 2, 20, title);

    /* Page indicator "N/7" */
    char pind[8];
    snprintf(pind, sizeof(pind), "%u/%u", (unsigned)(app->page + 1), (unsigned)FM_PAGE_COUNT);
    int pw = canvas_string_width(canvas, pind);
    canvas_draw_str(canvas, 128 - pw - 1, 20, pind);
}

/* Text wrap helpers */
static int wrap_count_lines(Canvas* canvas, const char* text, int max_w) {
    if(!text || !text[0]) return 1;
    char buf[48];
    size_t pos = 0, len = strlen(text);
    int lines = 0;
    while(pos < len) {
        size_t ll = 0, ls = 0;
        while(pos + ll < len) {
            buf[ll] = text[pos + ll];
            buf[ll + 1] = '\0';
            if(text[pos + ll] == ' ') ls = ll;
            if(canvas_string_width(canvas, buf) > max_w) {
                ll = (ls > 0) ? ls : (ll > 0 ? ll - 1 : 0);
                break;
            }
            ll++;
            if(ll >= sizeof(buf) - 2) break;
        }
        if(ll == 0 && pos < len) ll = 1;
        pos += ll;
        while(pos < len && text[pos] == ' ') pos++;
        lines++;
    }
    return lines > 0 ? lines : 1;
}

static void draw_wrapped_text(Canvas* canvas, int x, int y, int max_w,
                              const char* text, Color col) {
    if(!text || !text[0]) return;
    char buf[48];
    size_t pos = 0, len = strlen(text);
    int cy = y;
    canvas_set_color(canvas, col);
    while(pos < len && cy < 64) {
        size_t ll = 0, ls = 0;
        while(pos + ll < len) {
            buf[ll] = text[pos + ll];
            buf[ll + 1] = '\0';
            if(text[pos + ll] == ' ') ls = ll;
            if(canvas_string_width(canvas, buf) > max_w) {
                if(ls > 0) { ll = ls; buf[ll] = '\0'; }
                else if(ll > 0) { ll--; buf[ll] = '\0'; }
                break;
            }
            ll++;
            if(ll >= sizeof(buf) - 2) break;
        }
        if(ll == 0 && pos < len) { buf[0] = text[pos]; buf[1] = '\0'; ll = 1; }
        canvas_draw_str(canvas, x, cy, buf);
        pos += ll;
        while(pos < len && text[pos] == ' ') pos++;
        cy += 9;
    }
}

/* FMMessage bubble */
static void draw_bubble(Canvas* canvas, FlipMeshApp* app,
                        int x, int y, int max_w, const FMMessage* msg) {
    canvas_set_font(canvas, FontSecondary);

    const char* s = msg->text;
    bool is_tx    = msg->outgoing;

    /* Sender label */
    char sender[8];
    if(is_tx) {
        snprintf(sender, sizeof(sender), "Me");
    } else {
        snprintf(sender, sizeof(sender), "!%04lX", (unsigned long)(msg->from & 0xFFFF));
        /* Try to get short_name from roster */
        furi_mutex_acquire(app->lock, FuriWaitForever);
        for(uint8_t i = 0; i < app->roster.count; i++) {
            if(app->roster.nodes[i].node_id == msg->from &&
               app->roster.nodes[i].short_name[0]) {
                snprintf(sender, sizeof(sender), "%s", app->roster.nodes[i].short_name);
                break;
            }
        }
        furi_mutex_release(app->lock);
    }

    /* Timestamp */
    char ts[8] = "";
    if(app->show_ts && msg->ts > 0) {
        uint32_t t  = msg->ts;
        uint32_t h  = (t / 3600) % 24;
        uint32_t m  = (t / 60) % 60;
        snprintf(ts, sizeof(ts), "%02lu:%02lu", (unsigned long)h, (unsigned long)m);
    }

    int sender_w = canvas_string_width(canvas, sender);
    int pad      = 3;
    int inner_w  = max_w - pad * 2;
    int text_w   = canvas_string_width(canvas, s);
    int bubble_h = 12;
    int bubble_w;

    if(app->long_msg == FM_LMH_WRAP && text_w > inner_w) {
        int lines = wrap_count_lines(canvas, s, inner_w);
        bubble_h  = 2 + lines * 9 + 2;
        bubble_w  = max_w;
    } else {
        bubble_w = text_w + pad * 2;
        if(bubble_w > max_w) bubble_w = max_w;
        if(bubble_w < 20) bubble_w = 20;
    }

    int bx      = is_tx ? (x + max_w - bubble_w) : x;
    int name_x  = is_tx ? (bx + bubble_w - sender_w) : bx;
    int name_y  = y + 7;
    int bubble_y = y + 10;

    Color bbg  = is_tx ? ColorBlack : ColorWhite;
    Color tcol = is_tx ? ColorWhite : ColorBlack;

    /* Sender name */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, name_x, name_y, sender);
    if(app->show_ts && ts[0]) {
        int ts_w = canvas_string_width(canvas, ts);
        int ts_x = is_tx ? bx - ts_w - 2 : bx + bubble_w + 2;
        canvas_draw_str(canvas, ts_x, name_y, ts);
    }

    /* Bubble background */
    if(is_tx) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, bx, bubble_y, bubble_w, bubble_h, 3);
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_rbox(canvas, bx, bubble_y, bubble_w, bubble_h, 3);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, bx, bubble_y, bubble_w, bubble_h, 3);
    }

    int ix = bx + pad;
    int iy = bubble_y + 9;

    if(text_w <= inner_w) {
        canvas_set_color(canvas, tcol);
        canvas_draw_str(canvas, ix, iy, s);
    } else if(app->long_msg == FM_LMH_WRAP) {
        draw_wrapped_text(canvas, ix, iy, inner_w, s, tcol);
    } else {
        /* Scrolling text */
        uint32_t speed = 24 * (uint32_t)(11 - app->scroll_spd);
        uint32_t step  = furi_get_tick() / speed;
        uint16_t gap   = 12;
        uint16_t cycle = (uint16_t)text_w + gap;
        uint16_t off   = (uint16_t)(step % cycle);
        int x1 = ix - (int)off;
        int x2 = x1 + (int)cycle;
        canvas_set_color(canvas, tcol);
        canvas_draw_str(canvas, x1, iy, s);
        canvas_draw_str(canvas, x2, iy, s);
        /* Mask sides */
        canvas_set_color(canvas, bbg);
        canvas_draw_box(canvas, bx, bubble_y, pad, bubble_h);
        canvas_draw_box(canvas, bx + bubble_w - pad, bubble_y, pad, bubble_h);
        canvas_set_color(canvas, ColorWhite);
        if(bx > 0) canvas_draw_box(canvas, 0, bubble_y, bx, bubble_h);
        if(bx + bubble_w < 128) canvas_draw_box(canvas, bx + bubble_w, bubble_y, 128 - bx - bubble_w, bubble_h);
    }
    canvas_set_color(canvas, ColorBlack);
}

/* ── Page renderers ───────────────────────────────────────────────────────── */

static void render_messages(Canvas* canvas, FlipMeshApp* app) {
    char title[24];
    snprintf(title, sizeof(title), "Messages");
    draw_statusbar(canvas, app);
    draw_nav_header(canvas, app, title);

    canvas_set_font(canvas, FontSecondary);

    /* Collect broadcast message indices */
    uint8_t bcast[FM_MSG_HISTORY];
    uint8_t bc = 0;
    for(uint8_t i = 0; i < app->history.count; i++) {
        uint8_t idx = (uint8_t)((app->history.head + FM_MSG_HISTORY - app->history.count + i) % FM_MSG_HISTORY);
        if(app->history.buf[idx].to == 0xFFFFFFFF || app->history.buf[idx].outgoing) {
            bcast[bc++] = idx;
        }
    }

    if(bc == 0) {
        canvas_draw_str(canvas, 10, 38, "No broadcast traffic");
        canvas_draw_str(canvas, 14, 50, "OK = send message");
        return;
    }

    /* Apply scroll offset from the end */
    int start = (int)bc - 1 - (int)app->msg_scroll;
    if(start < 0) start = 0;

    int y = 24;
    for(int i = start; i >= 0 && y < 60; i--) {
        FMMessage* msg   = &app->history.buf[bcast[i]];
        int msg_height = 22;
        if(app->long_msg == FM_LMH_WRAP) {
            int tw = canvas_string_width(canvas, msg->text);
            if(tw > 116) {
                int ln = wrap_count_lines(canvas, msg->text, 116);
                msg_height = 12 + ln * 9 + 2;
            }
        }
        draw_bubble(canvas, app, 4, y, 120, msg);
        y += msg_height;
    }
}

static void render_nodes(Canvas* canvas, FlipMeshApp* app) {
    draw_statusbar(canvas, app);

    FMRoster* r = &app->roster;

    /* ── FM_ROSTER_CHAT ── */
    if(r->view == FM_ROSTER_CHAT && r->sel < r->count) {
        FMNode* node = &r->nodes[r->sel];
        char name[16];
        fm_node_display(node, name, sizeof(name));
        draw_nav_header(canvas, app, name);

        canvas_set_font(canvas, FontSecondary);
        uint8_t chat[FM_MSG_HISTORY];
        uint8_t cc = 0;
        for(uint8_t i = 0; i < app->history.count; i++) {
            uint8_t idx = (uint8_t)((app->history.head + FM_MSG_HISTORY - app->history.count + i) % FM_MSG_HISTORY);
            const FMMessage* m = &app->history.buf[idx];
            if((m->from == node->node_id && m->to == app->self_id) ||
               (m->from == app->self_id && m->to == node->node_id)) {
                chat[cc++] = idx;
            }
        }

        if(cc == 0) {
            canvas_draw_str(canvas, 10, 38, "No messages yet");
            return;
        }

        int y    = 24;
        int strt = (int)cc - 1 - (int)r->chat_scroll;
        if(strt < 0) strt = 0;
        for(int i = strt; i >= 0 && y < 60; i--) {
            draw_bubble(canvas, app, 4, y, 120, &app->history.buf[chat[i]]);
            y += 22;
        }
        return;
    }

    /* ── FM_ROSTER_INFO ── */
    if(r->view == FM_ROSTER_INFO && r->sel < r->count) {
        FMNode* e = &r->nodes[r->sel];
        char name[16];
        fm_node_display(e, name, sizeof(name));
        draw_nav_header(canvas, app, name);
        canvas_set_font(canvas, FontSecondary);

        int y = 24;
        if(e->long_name[0]) {
            canvas_draw_str(canvas, 2, y, e->long_name);
            y += 10;
        }

        char buf[48];
        if(e->last_heard) {
            uint32_t age = furi_get_tick() / 1000;
            if(age > e->last_heard) age -= e->last_heard; else age = 0;
            if(age < 60)      snprintf(buf, sizeof(buf), "Seen: %lus ago", (unsigned long)age);
            else if(age < 3600) snprintf(buf, sizeof(buf), "Seen: %lum ago", (unsigned long)(age / 60));
            else              snprintf(buf, sizeof(buf), "Seen: %luh ago", (unsigned long)(age / 3600));
            canvas_draw_str(canvas, 2, y, buf);
            y += 9;
        }

        snprintf(buf, sizeof(buf), "SNR:%.1f RSSI:%d", (double)e->snr, (int)e->rssi);
        canvas_draw_str(canvas, 2, y, buf);
        y += 9;

        if(e->hops) {
            snprintf(buf, sizeof(buf), "Hops: %u%s", e->hops, e->via_mqtt ? " (MQTT)" : "");
            canvas_draw_str(canvas, 2, y, buf);
            y += 9;
        }

        if(e->has_metrics) {
            snprintf(buf, sizeof(buf), "Bat: %u%% %.2fV", e->battery_pct, (double)e->voltage);
            canvas_draw_str(canvas, 2, y, buf);
            y += 9;
            if(e->ch_util > 0) {
                snprintf(buf, sizeof(buf), "ChUtil:%.1f%% Air:%.1f%%",
                         (double)e->ch_util, (double)e->air_util);
                canvas_draw_str(canvas, 2, y, buf);
            }
        } else if(e->has_env) {
            snprintf(buf, sizeof(buf), "%.1fC %.0f%% %.0fhPa",
                     (double)e->temp_c, (double)e->humidity, (double)e->pressure_hpa);
            canvas_draw_str(canvas, 2, y, buf);
        } else {
            canvas_draw_str(canvas, 2, y, "No telemetry");
        }
        return;
    }

    /* ── FM_ROSTER_LIST ── */
    draw_nav_header(canvas, app, "Nodes");

    if(r->count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 38, "No nodes discovered");
        canvas_draw_str(canvas, 10, 50, "Waiting for traffic...");
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    int y = 24;
    for(uint8_t i = 0; i < r->count && y < 62; i++) {
        FMNode* e = &r->nodes[i];
        bool selected = (i == r->sel);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        /* DM badge */
        if(e->unread_dm) canvas_draw_str(canvas, 1, y, "!");

        /* Display name */
        char name[8];
        fm_node_display(e, name, sizeof(name));
        canvas_draw_str(canvas, 8, y, name);

        /* RSSI dots */
        draw_rssi_dots(canvas, 50, y - 4, rssi_dots(e->rssi));

        /* Hops */
        char hops[6];
        if(e->hops > 0) snprintf(hops, sizeof(hops), "%uh", e->hops);
        else snprintf(hops, sizeof(hops), "dir");
        canvas_draw_str(canvas, 68, y, hops);

        /* Last seen */
        if(e->last_heard) {
            uint32_t age = furi_get_tick() / 1000;
            if(age > e->last_heard) age -= e->last_heard; else age = 0;
            char seen[10];
            if(age < 60)       snprintf(seen, sizeof(seen), "%lus", (unsigned long)age);
            else if(age < 3600) snprintf(seen, sizeof(seen), "%lum", (unsigned long)(age / 60));
            else               snprintf(seen, sizeof(seen), "%luh", (unsigned long)(age / 3600));
            int sw = canvas_string_width(canvas, seen);
            canvas_draw_str(canvas, 127 - sw, y, seen);
        }

        canvas_set_color(canvas, ColorBlack);
        y += 10;
    }
}

static void render_position(Canvas* canvas, FlipMeshApp* app) {
    draw_statusbar(canvas, app);
    draw_nav_header(canvas, app, "Position");

    canvas_set_font(canvas, FontSecondary);

    /* Collect nodes with position */
    FMRoster* r = &app->roster;
    uint8_t pos_nodes[FM_ROSTER_MAX];
    uint8_t pc = 0;
    for(uint8_t i = 0; i < r->count; i++) {
        if(r->nodes[i].has_gps) pos_nodes[pc++] = i;
    }

    if(pc == 0) {
        canvas_draw_str(canvas, 4, 38, "No position data yet");
        canvas_draw_str(canvas, 4, 50, "Waiting for GPS...");
        return;
    }

    /* Reference for distance: first node with position */
    int32_t ref_lat = r->nodes[pos_nodes[0]].lat_i;
    int32_t ref_lon = r->nodes[pos_nodes[0]].lon_i;

    int y       = 24;
    uint8_t off = app->roster.pos_scroll;
    for(uint8_t i = off; i < pc && y < 62; i++) {
        FMNode* e = &r->nodes[pos_nodes[i]];
        char name[8];
        fm_node_display(e, name, sizeof(name));

        char coord[32];
        position_format_coords(e->lat_i, e->lon_i, coord, sizeof(coord));

        canvas_draw_str(canvas, 2, y, name);
        draw_str_ellipsis(canvas, 28, y, 98, coord);
        y += 9;

        if(i != pos_nodes[0]) {
            uint32_t dist = position_calc_distance_m(ref_lat, ref_lon,
                                                      e->lat_i, e->lon_i);
            char distbuf[12];
            position_format_distance(dist, distbuf, sizeof(distbuf));
            char distline[24];
            snprintf(distline, sizeof(distline), "  -> %s", distbuf);
            canvas_draw_str(canvas, 2, y, distline);
        }
        y += 9;
    }
}

static void render_stats(Canvas* canvas, FlipMeshApp* app) {
    draw_statusbar(canvas, app);
    draw_nav_header(canvas, app, "Stats");

    canvas_set_font(canvas, FontSecondary);
    char buf[48];
    int y = 24;

    snprintf(buf, sizeof(buf), "%s @ %lu",
             (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART",
             (unsigned long)app->baud);
    canvas_draw_str(canvas, 2, y, buf); y += 9;

    snprintf(buf, sizeof(buf), "RX: %lu bytes / %lu frm",
             (unsigned long)app->rx_bytes, (unsigned long)app->rx_ok);
    canvas_draw_str(canvas, 2, y, buf); y += 9;

    snprintf(buf, sizeof(buf), "Err: mag=%lu len=%lu dec=%lu",
             (unsigned long)app->rx_err_magic,
             (unsigned long)app->rx_err_len,
             (unsigned long)app->rx_err_decode);
    canvas_draw_str(canvas, 2, y, buf); y += 9;

    snprintf(buf, sizeof(buf), "TX: %lu frm  nodes: %u",
             (unsigned long)app->tx_ok, (unsigned)app->roster.count);
    canvas_draw_str(canvas, 2, y, buf); y += 9;

    snprintf(buf, sizeof(buf), "HB: %lu sent", (unsigned long)app->hb_nonce);
    canvas_draw_str(canvas, 2, y, buf);
}

static void render_signal(Canvas* canvas, FlipMeshApp* app) {
    draw_statusbar(canvas, app);
    draw_nav_header(canvas, app, "Signal");

    canvas_set_font(canvas, FontSecondary);
    char buf[48];
    int y = 24;

    if(app->self_id) {
        snprintf(buf, sizeof(buf), "My ID: !%08lX", (unsigned long)app->self_id);
        canvas_draw_str(canvas, 2, y, buf); y += 9;
    }

    if(app->has_signal) {
        snprintf(buf, sizeof(buf), "From: !%04lX", (unsigned long)(app->last_from & 0xFFFF));
        canvas_draw_str(canvas, 2, y, buf); y += 9;

        snprintf(buf, sizeof(buf), "RSSI: %ld dBm", (long)app->last_rssi);
        canvas_draw_str(canvas, 2, y, buf); y += 9;

        snprintf(buf, sizeof(buf), "SNR:  %.2f dB", (double)app->last_snr);
        canvas_draw_str(canvas, 2, y, buf); y += 9;

        draw_rssi_dots(canvas, 2, y - 3, rssi_dots((int16_t)app->last_rssi));
    } else {
        canvas_draw_str(canvas, 10, 38, "No signal data yet");
    }
}

static void render_logs(Canvas* canvas, FlipMeshApp* app) {
    draw_statusbar(canvas, app);
    draw_nav_header(canvas, app, app->log_frozen ? "Logs [PAUSE]" : "Logs");

    canvas_set_font(canvas, FontSecondary);

    int vis    = 5;
    int y0     = 23;
    int top_ln = (app->log_head + FM_LOG_ROWS - vis - (int)app->log_scroll) % FM_LOG_ROWS;

    for(int i = 0; i < vis; i++) {
        int ln = (top_ln + i) % FM_LOG_ROWS;
        if(app->log[ln][0]) {
            canvas_draw_str(canvas, 2, y0 + i * 9, app->log[ln]);
        }
    }
}

static void render_settings(Canvas* canvas, FlipMeshApp* app) {
    draw_statusbar(canvas, app);
    draw_nav_header(canvas, app, "Settings");

    canvas_set_font(canvas, FontSecondary);

    typedef struct { const char* label; char val[24]; } Row;
    Row rows[FM_SET_COUNT];

    snprintf(rows[FM_SET_UART].val, 24, "%s",
             (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART");
    rows[FM_SET_UART].label = "UART";

    snprintf(rows[FM_SET_BAUD].val, 24, "%lu", (unsigned long)app->baud);
    rows[FM_SET_BAUD].label = "Baud";

    snprintf(rows[FM_SET_VIBRO].val, 24, "%s", app->vib_on ? "On" : "Off");
    rows[FM_SET_VIBRO].label = "Vibro";

    snprintf(rows[FM_SET_LED].val, 24, "%s", app->led_on ? "On" : "Off");
    rows[FM_SET_LED].label = "LED";

    snprintf(rows[FM_SET_TONE].val, 24, "%s",
             fm_tone_name(app->tone));
    rows[FM_SET_TONE].label = "Ringtone";

    snprintf(rows[FM_SET_SCROLL_SPD].val, 24, "%u", app->scroll_spd);
    rows[FM_SET_SCROLL_SPD].label = "Scroll spd";

    snprintf(rows[FM_SET_FRAMERATE].val, 24, "%u fps", app->framerate);
    rows[FM_SET_FRAMERATE].label = "Framerate";

    snprintf(rows[FM_SET_LONG_MSG].val, 24, "%s", lmh_names[app->long_msg]);
    rows[FM_SET_LONG_MSG].label = "Long msg";

    snprintf(rows[FM_SET_HEARTBEAT].val, 24, "%s",
             hb_names[app->hb_idx < 3 ? app->hb_idx : 1]);
    rows[FM_SET_HEARTBEAT].label = "Heartbeat";

    snprintf(rows[FM_SET_CHANNELS].val, 24, "%u", app->num_ch);
    rows[FM_SET_CHANNELS].label = "Channels";

    snprintf(rows[FM_SET_TIMESTAMPS].val, 24, "%s", app->show_ts ? "On" : "Off");
    rows[FM_SET_TIMESTAMPS].label = "Timestamps";

    int y = 24;
    for(uint8_t i = 0; i < FM_SET_COUNT && y < 64; i++) {
        bool sel = (i == app->set_cursor);
        bool editing = sel && app->set_editing;

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str(canvas, 2, y, rows[i].label);

        if(editing) {
            canvas_draw_str(canvas, 70, y, "<");
            canvas_draw_str(canvas, 118, y, ">");
            int vw = canvas_string_width(canvas, rows[i].val);
            canvas_draw_str(canvas, 74 + (40 - vw) / 2, y, rows[i].val);
        } else {
            canvas_draw_str(canvas, 74, y, rows[i].val);
        }

        canvas_set_color(canvas, ColorBlack);
        y += 10;
    }
}

/* ── Render dispatch ──────────────────────────────────────────────────────── */

void render_cb(Canvas* canvas, void* ctx) {
    FlipMeshApp* app = (FlipMeshApp*)ctx;
    if(!app) return;

    furi_mutex_acquire(app->lock, FuriWaitForever);
    canvas_clear(canvas);

    switch(app->page) {
    case FM_PAGE_MESSAGES: render_messages(canvas, app); break;
    case FM_PAGE_NODES:    render_nodes(canvas, app);    break;
    case FM_PAGE_POSITION: render_position(canvas, app); break;
    case FM_PAGE_STATS:    render_stats(canvas, app);    break;
    case FM_PAGE_SIGNAL:   render_signal(canvas, app);   break;
    case FM_PAGE_LOGS:     render_logs(canvas, app);     break;
    case FM_PAGE_SETTINGS: render_settings(canvas, app); break;
    default:            break;
    }

    furi_mutex_release(app->lock);
}

/* ── Input handling ───────────────────────────────────────────────────────── */

static void setting_change(FlipMeshApp* app, int dir) {
    switch(app->set_cursor) {
    case FM_SET_UART:
        app->uart_id = (app->uart_id == FuriHalSerialIdUsart)
            ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
        fm_uart_reopen(app, app->uart_id, app->baud);
        break;
    case FM_SET_BAUD: {
        uint8_t idx = (baud_to_idx(app->baud) + BAUD_COUNT + (uint8_t)dir) % BAUD_COUNT;
        app->baud = baud_options[idx];
        fm_uart_reopen(app, app->uart_id, app->baud);
        break;
    }
    case FM_SET_VIBRO:
        app->vib_on = !app->vib_on;
        break;
    case FM_SET_LED:
        app->led_on = !app->led_on;
        break;
    case FM_SET_TONE: {
        int r = (int)app->tone + dir;
        if(r < 0) r = FM_TONE_COUNT - 1;
        if(r >= FM_TONE_COUNT) r = 0;
        app->tone = (FMTone)r;
        fm_notify_message(app);
        break;
    }
    case FM_SET_SCROLL_SPD: {
        int v = (int)app->scroll_spd + dir;
        if(v < 1) v = 10;
        if(v > 10) v = 1;
        app->scroll_spd = (uint8_t)v;
        break;
    }
    case FM_SET_FRAMERATE: {
        int v = (int)app->framerate + dir;
        if(v < 1) v = 10;
        if(v > 10) v = 1;
        app->framerate = (uint8_t)v;
        break;
    }
    case FM_SET_LONG_MSG: {
        int v = (int)app->long_msg + dir;
        if(v < 0) v = FM_LMH_COUNT - 1;
        if(v >= FM_LMH_COUNT) v = 0;
        app->long_msg = (FMLongMsg)v;
        break;
    }
    case FM_SET_HEARTBEAT: {
        int v = (int)app->hb_idx + dir;
        if(v < 0) v = 2;
        if(v > 2) v = 0;
        app->hb_idx = (uint8_t)v;
        fm_hb_start(app);
        break;
    }
    case FM_SET_CHANNELS: {
        int v = (int)app->num_ch + dir;
        if(v < 1) v = FM_MAX_CHANNELS;
        if(v > FM_MAX_CHANNELS) v = 1;
        app->num_ch = (uint8_t)v;
        break;
    }
    case FM_SET_TIMESTAMPS:
        app->show_ts = !app->show_ts;
        break;
    default:
        break;
    }
    settings_save(app);
}

void input_cb(InputEvent* event, void* ctx) {
    FlipMeshApp* app = (FlipMeshApp*)ctx;
    if(!app) return;
    if(event->type != InputTypeShort && event->type != InputTypeLong) return;

    bool is_long = (event->type == InputTypeLong);

    furi_mutex_acquire(app->lock, FuriWaitForever);

    switch(app->page) {

    /* ── Messages ── */
    case FM_PAGE_MESSAGES:
        switch(event->key) {
        case InputKeyLeft:
            app->page = (uint8_t)((FM_PAGE_COUNT + app->page - 1) % FM_PAGE_COUNT);
            break;
        case InputKeyRight:
            app->page = (uint8_t)((app->page + 1) % FM_PAGE_COUNT);
            break;
        case InputKeyUp:
            if(app->msg_scroll + 1 < app->history.count)
                app->msg_scroll++;
            break;
        case InputKeyDown:
            if(app->msg_scroll > 0) app->msg_scroll--;
            break;
        case InputKeyOk:
            if(is_long) {
                fm_ch_next(app);
            } else {
                app->kb_active = true;
            }
            break;
        case InputKeyBack:
            app->quit = true;
            break;
        default: break;
        }
        break;

    /* ── Nodes ── */
    case FM_PAGE_NODES:
        switch(event->key) {
        case InputKeyLeft:
            if(app->roster.view != FM_ROSTER_LIST) {
                app->roster.view = FM_ROSTER_LIST;
            } else {
                app->page = (uint8_t)((FM_PAGE_COUNT + app->page - 1) % FM_PAGE_COUNT);
            }
            break;
        case InputKeyRight:
            if(app->roster.view != FM_ROSTER_LIST) {
                app->roster.view = FM_ROSTER_LIST;
            } else {
                app->page = (uint8_t)((app->page + 1) % FM_PAGE_COUNT);
            }
            break;
        case InputKeyUp:
            if(app->roster.view == FM_ROSTER_LIST) {
                if(app->roster.sel > 0) app->roster.sel--;
            } else if(app->roster.view == FM_ROSTER_CHAT) {
                if(app->roster.chat_scroll + 1 < app->history.count)
                    app->roster.chat_scroll++;
            }
            break;
        case InputKeyDown:
            if(app->roster.view == FM_ROSTER_LIST) {
                if(app->roster.sel + 1 < app->roster.count)
                    app->roster.sel++;
            } else if(app->roster.view == FM_ROSTER_CHAT) {
                if(app->roster.chat_scroll > 0) app->roster.chat_scroll--;
            }
            break;
        case InputKeyOk:
            if(app->roster.view == FM_ROSTER_LIST) {
                if(is_long) {
                    app->roster.view = FM_ROSTER_INFO;
                } else {
                    app->roster.view = FM_ROSTER_CHAT;
                    app->roster.chat_scroll = 0;
                    if(app->roster.sel < app->roster.count)
                        app->roster.nodes[app->roster.sel].unread_dm = false;
                }
            } else if(app->roster.view == FM_ROSTER_CHAT) {
                if(!is_long) app->kb_active = true;
            }
            break;
        case InputKeyBack:
            if(app->roster.view != FM_ROSTER_LIST) {
                app->roster.view = FM_ROSTER_LIST;
            } else {
                app->quit = true;
            }
            break;
        default: break;
        }
        break;

    /* ── Position ── */
    case FM_PAGE_POSITION:
        switch(event->key) {
        case InputKeyLeft:
            app->page = (uint8_t)((FM_PAGE_COUNT + app->page - 1) % FM_PAGE_COUNT);
            break;
        case InputKeyRight:
            app->page = (uint8_t)((app->page + 1) % FM_PAGE_COUNT);
            break;
        case InputKeyUp:
            if(app->roster.pos_scroll > 0) app->roster.pos_scroll--;
            break;
        case InputKeyDown:
            app->roster.pos_scroll++;
            break;
        case InputKeyBack:
            app->quit = true;
            break;
        default: break;
        }
        break;

    /* ── Stats / Signal / Logs (read-only pages) ── */
    case FM_PAGE_STATS:
    case FM_PAGE_SIGNAL:
        switch(event->key) {
        case InputKeyLeft:
            app->page = (uint8_t)((FM_PAGE_COUNT + app->page - 1) % FM_PAGE_COUNT);
            break;
        case InputKeyRight:
            app->page = (uint8_t)((app->page + 1) % FM_PAGE_COUNT);
            break;
        case InputKeyBack:
            app->quit = true;
            break;
        default: break;
        }
        break;

    case FM_PAGE_LOGS:
        switch(event->key) {
        case InputKeyLeft:
            app->page = (uint8_t)((FM_PAGE_COUNT + app->page - 1) % FM_PAGE_COUNT);
            break;
        case InputKeyRight:
            app->page = (uint8_t)((app->page + 1) % FM_PAGE_COUNT);
            break;
        case InputKeyOk:
            app->log_frozen = !app->log_frozen;
            if(!app->log_frozen) app->log_scroll = 0;
            break;
        case InputKeyUp:
            if(app->log_frozen && app->log_scroll + 1 < FM_LOG_ROWS)
                app->log_scroll++;
            break;
        case InputKeyDown:
            if(app->log_frozen && app->log_scroll > 0)
                app->log_scroll--;
            break;
        case InputKeyBack:
            app->quit = true;
            break;
        default: break;
        }
        break;

    /* ── Settings ── */
    case FM_PAGE_SETTINGS:
        switch(event->key) {
        case InputKeyLeft:
            if(app->set_editing) {
                setting_change(app, -1);
            } else {
                app->page = (uint8_t)((FM_PAGE_COUNT + app->page - 1) % FM_PAGE_COUNT);
            }
            break;
        case InputKeyRight:
            if(app->set_editing) {
                setting_change(app, 1);
            } else {
                app->page = (uint8_t)((app->page + 1) % FM_PAGE_COUNT);
            }
            break;
        case InputKeyUp:
            if(!app->set_editing && app->set_cursor > 0)
                app->set_cursor--;
            break;
        case InputKeyDown:
            if(!app->set_editing && app->set_cursor + 1 < FM_SET_COUNT)
                app->set_cursor++;
            break;
        case InputKeyOk:
            app->set_editing = !app->set_editing;
            break;
        case InputKeyBack:
            if(app->set_editing) {
                app->set_editing = false;
            } else {
                app->quit = true;
            }
            break;
        default: break;
        }
        break;

    default: break;
    }

    furi_mutex_release(app->lock);
    view_port_update(app->vp);
}

/* ── Keyboard callbacks ───────────────────────────────────────────────────── */

uint32_t kb_back_callback(void* ctx) {
    (void)ctx;
    return VIEW_NONE;
}

void text_input_callback(void* ctx) {
    FlipMeshApp* app = (FlipMeshApp*)ctx;
    if(!app || app->kb_buf[0] == '\0') return;

    uint32_t dest = 0xFFFFFFFF;
    if(app->page == FM_PAGE_NODES &&
       app->roster.view == FM_ROSTER_CHAT &&
       app->roster.sel < app->roster.count) {
        dest = app->roster.nodes[app->roster.sel].node_id;
    }

    fm_proto_send_text(app, app->kb_buf, dest);
}
