// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#include "fm_settings.h"
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SETTINGS_VERSION 2

/* ── Save ─────────────────────────────────────────────────────────────────── */

static void write_kv(File* f, const char* key, const char* val) {
    char line[80];
    size_t n = (size_t)snprintf(line, sizeof(line), "%s=%s\n", key, val);
    storage_file_write(f, line, n);
}

static void write_kv_int(File* f, const char* key, int v) {
    char val[12];
    snprintf(val, sizeof(val), "%d", v);
    write_kv(f, key, val);
}

void settings_save(FlipMeshApp* app) {
    if(!app) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
#if defined(FM_APP_BT)
    storage_common_mkdir(storage, "/ext/flipmesh-bt");
#else
    storage_common_mkdir(storage, "/ext/flipmesh");
#endif

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FM_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        write_kv_int(file, "version", SETTINGS_VERSION);
#if !defined(FM_APP_BT)
        write_kv_int(file, "uart_id", (int)app->uart_id);
        write_kv_int(file, "baud", (int)app->baud);
        write_kv_int(file, "hb_idx", (int)app->hb_idx);
#else
        write_kv_int(file, "ble_auto", app->ble_auto_reconnect ? 1 : 0);
        write_kv_int(file, "ble_mesh_hb", app->transport_heartbeat_allowed ? 1 : 0);
#endif
        write_kv_int(file, "vibro", app->vib_on ? 1 : 0);
        write_kv_int(file, "led", app->led_on ? 1 : 0);
        write_kv_int(file, "ringtone", (int)app->tone);
        write_kv_int(file, "scroll_speed", (int)app->scroll_spd);
        write_kv_int(file, "scroll_fps", (int)app->framerate);
        write_kv_int(file, "lmh_mode", (int)app->long_msg);
        write_kv_int(file, "channels", (int)app->num_ch);
        write_kv_int(file, "timestamps", app->show_ts ? 1 : 0);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ── Load ─────────────────────────────────────────────────────────────────── */

/*
 * Character-by-character FSM parser. Scans the buffer once, accumulating
 * key and value token in-place. On newline, dispatches the pair.
 * No strchr, no memcpy line extraction, no pointer arithmetic over lines.
 */

typedef enum { PS_KEY, PS_VAL, PS_DONE } ParsePhase;

static void dispatch(FlipMeshApp* app, const char* key, const char* val) {
    int v = atoi(val);
#if !defined(FM_APP_BT)
    if(!strcmp(key, "uart_id")) {
        app->uart_id = (v == 1) ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
        return;
    }
    if(!strcmp(key, "baud")) {
        if(v > 0) app->baud = (uint32_t)v;
        return;
    }
    if(!strcmp(key, "hb_idx")) {
        if(v >= 0 && v <= 2) app->hb_idx = (uint8_t)v;
        return;
    }
#else
    if(!strcmp(key, "ble_auto")) {
        app->ble_auto_reconnect = (v != 0);
        return;
    }
    if(!strcmp(key, "ble_mesh_hb")) {
        app->transport_heartbeat_allowed = (v != 0);
        return;
    }
#endif
    if(!strcmp(key, "vibro")) {
        app->vib_on = (v != 0);
    } else if(!strcmp(key, "led")) {
        app->led_on = (v != 0);
    } else if(!strcmp(key, "ringtone")) {
        if(v >= 0 && v < FM_TONE_COUNT) app->tone = (FMTone)v;
    } else if(!strcmp(key, "scroll_speed")) {
        if(v >= 1 && v <= 10) app->scroll_spd = (uint8_t)v;
    } else if(!strcmp(key, "scroll_fps")) {
        if(v >= 1 && v <= 10) app->framerate = (uint8_t)v;
    } else if(!strcmp(key, "lmh_mode")) {
        if(v >= 0 && v < FM_LMH_COUNT) app->long_msg = (FMLongMsg)v;
    } else if(!strcmp(key, "channels")) {
        if(v >= 1 && v <= FM_MAX_CHANNELS) app->num_ch = (uint8_t)v;
    } else if(!strcmp(key, "timestamps")) {
        app->show_ts = (v != 0);
    }
    /* "version" key is accepted silently */
}

void settings_load(FlipMeshApp* app) {
    if(!app) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, FM_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    char buf[384];
    uint16_t n = storage_file_read(file, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    /* Single-pass FSM: scan byte by byte, no line extraction */
    char key[32];
    char val[32];
    uint8_t ki = 0, vi = 0;
    ParsePhase phase = PS_KEY;

    for(uint16_t i = 0; i <= n; i++) {
        char c = buf[i]; /* '\0' at i==n acts as final line terminator */

        if(c == '\r') continue;

        if(c == '=' && phase == PS_KEY) {
            key[ki] = '\0';
            phase   = PS_VAL;

        } else if((c == '\n' || c == '\0') && phase != PS_DONE) {
            val[vi] = '\0';
            if(ki > 0) dispatch(app, key, val);
            ki = vi = 0;
            phase   = PS_KEY;

        } else if(phase == PS_KEY && ki < (uint8_t)(sizeof(key) - 1)) {
            key[ki++] = c;

        } else if(phase == PS_VAL && vi < (uint8_t)(sizeof(val) - 1)) {
            val[vi++] = c;
        }
    }
}
