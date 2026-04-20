#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>

#include "lib/nanopb/pb.h"
#include "lib/nanopb/pb_encode.h"
#include "lib/nanopb/pb_decode.h"

#include "lib/meshtastic_api/meshtastic/mesh.pb.h"
#include "lib/meshtastic_api/meshtastic/portnums.pb.h"
#include "lib/meshtastic_api/meshtastic/telemetry.pb.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Wire protocol ───────────────────────────────────────────────────────── */
#define FM_MAGIC0       0x94
#define FM_MAGIC1       0xC3
#define FM_MAX_FRAME    512
#define FM_RX_BUF      4096

/* ── Display layout ──────────────────────────────────────────────────────── */
#define FM_LOG_ROWS   20
#define FM_FM_LOG_COLS   64

/* ── Pages ───────────────────────────────────────────────────────────────── */
#define FM_FM_PAGE_MESSAGES  0
#define FM_FM_PAGE_NODES     1
#define FM_FM_PAGE_POSITION  2
#define FM_FM_PAGE_STATS     3
#define FM_FM_PAGE_SIGNAL    4
#define FM_FM_PAGE_LOGS      5
#define FM_FM_PAGE_SETTINGS  6
#define FM_FM_PAGE_COUNT     7

/* ── Limits ──────────────────────────────────────────────────────────────── */
#define FM_FM_MSG_HISTORY   16
#define FM_FM_MSG_TEXT_MAX  200
#define FM_ROSTER_MAX    32
#define FM_ECHO_RING     32
#define FM_FM_MAX_CHANNELS  8

/* ── Paths ───────────────────────────────────────────────────────────────── */
#define FM_FM_SETTINGS_PATH "/ext/flipmesh/settings.cfg"

/* ── Tones (notification sounds) ─────────────────────────────────────────── */
typedef enum {
    FMToneOff = 0,
    FMTonePing,
    FMToneChime,
    FMToneAscend,
    FMTonePulse,
    FMToneBell,
    FMToneMorse,
    FMToneSweep,
    FMToneBounce,
    FMToneAlert,
    FMToneTwo,
    FMToneThree,
    FMToneDeep,
    FMToneCrisscross,
    FMToneRamp,
    FMToneClick,
    FMToneWarp,
    FMToneChord,
    FMToneBlip,
    FM_TONE_COUNT,
} FMTone;

/* ── Long-message rendering ──────────────────────────────────────────────── */
typedef enum {
    FM_LMH_SCROLL = 0,
    FM_LMH_WRAP,
    FM_FM_LMH_COUNT,
} FMLongMsg;

/* ── Connection state ────────────────────────────────────────────────────── */
typedef enum {
    FM_CONN_IDLE = 0,
    FM_CONN_SYNC,
    FM_CONN_LIVE,
} FMConn;

/* ── Roster sub-view ─────────────────────────────────────────────────────── */
typedef enum {
    FM_ROSTER_LIST = 0,
    FM_ROSTER_CHAT,
    FM_ROSTER_INFO,
} FMRosterView;

/* ── Settings items ──────────────────────────────────────────────────────── */
typedef enum {
    FM_SET_UART = 0,
    FM_SET_BAUD,
    FM_SET_VIBRO,
    FM_SET_LED,
    FM_SET_TONE,
    FM_SET_SCROLL_SPD,
    FM_SET_FRAMERATE,
    FM_SET_LONG_MSG,
    FM_SET_HEARTBEAT,
    FM_SET_CHANNELS,
    FM_SET_TIMESTAMPS,
    FM_SET_COUNT,
} FMSetting;

/* ── Data structures ─────────────────────────────────────────────────────── */

typedef struct {
    uint32_t node_id;
    char     long_name[40];
    char     short_name[5];
    uint8_t  hw_model;
    uint32_t last_heard;    /* epoch seconds from device */
    float    snr;
    int16_t  rssi;
    uint8_t  battery_pct;
    float    voltage;
    float    ch_util;
    float    air_util;
    uint32_t uptime_s;
    bool     has_metrics;
    bool     has_env;
    float    temp_c;
    float    humidity;
    float    pressure_hpa;
    bool     has_gps;
    int32_t  lat_i;         /* degrees × 1e7 */
    int32_t  lon_i;
    uint32_t gps_time;
    uint8_t  hops;
    bool     via_mqtt;
    bool     unread_dm;
} FMNode;

typedef struct {
    FMNode        nodes[FM_ROSTER_MAX];
    uint8_t       count;
    uint8_t       sel;
    FMRosterView  view;
    uint8_t       chat_scroll;
    uint8_t       pos_scroll;
} FMRoster;

typedef struct {
    char     text[FM_FM_MSG_TEXT_MAX];
    uint32_t from;
    uint32_t to;
    bool     outgoing;
    uint32_t ts;            /* seconds since boot */
} FMFMMessage;

typedef struct {
    FMFMMessage buf[FM_FM_MSG_HISTORY];
    uint8_t   head;
    uint8_t   count;
} FMHistory;

/* ── Main application context ────────────────────────────────────────────── */

typedef struct {
    Gui*               gui;
    ViewPort*          vp;
    FuriMutex*         lock;

    /* Serial */
    FuriHalSerialId      uart_id;
    uint32_t             baud;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer*    rx_stream;
    FuriTimer*           hb_timer;
    uint32_t             hb_nonce;

    /* RX thread */
    FuriThread*   rx_thread;
    volatile bool quit;

    /* Framing state */
    uint8_t  hdr[4];
    uint8_t  hdr_pos;
    uint16_t frame_len;
    uint16_t frame_pos;
    uint8_t* frame_buf;     /* heap, FM_MAX_FRAME bytes */
    uint32_t bad_streak;

    /* Stats */
    uint32_t rx_bytes;
    uint32_t rx_ok;
    uint32_t rx_err_magic;
    uint32_t rx_err_len;
    uint32_t rx_err_decode;
    uint32_t tx_ok;
    uint32_t tx_err;

    /* Connection */
    FMConn   conn;
    uint32_t sync_id;

    /* Logging */
    char    log[FM_LOG_ROWS][FM_FM_LOG_COLS];
    uint8_t log_head;
    char    status_line[FM_FM_LOG_COLS];

    /* Data */
    FMHistory history;
    FMRoster  roster;

    /* Own identity */
    uint32_t self_id;
    char     self_short[5];
    char     self_long[40];

    /* Signal */
    uint32_t last_from;
    uint32_t last_to;
    float    last_snr;
    int32_t  last_rssi;
    bool     has_signal;

    /* Echo ring */
    uint32_t echo_ids[FM_ECHO_RING];
    uint8_t  echo_head;

    /* UI state */
    uint8_t  page;
    uint8_t  msg_scroll;
    bool     log_frozen;
    uint8_t  log_scroll;

    /* Settings */
    bool      vib_on;
    bool      led_on;
    FMTone    tone;
    uint8_t   scroll_spd;
    uint8_t   framerate;
    FMLongMsg long_msg;
    uint8_t   hb_idx;       /* 0=10s 1=30s 2=60s */
    bool      show_ts;
    uint8_t   active_ch;
    uint8_t   num_ch;

    /* Settings UI */
    uint8_t  set_cursor;
    bool     set_editing;

    /* Keyboard */
    bool            kb_active;
    char            kb_buf[64];
    ViewDispatcher* kb_vd;
    TextInput*      kb_input;
} FlipMeshApp;

/* ── Shared helpers ──────────────────────────────────────────────────────── */
void fm_log(FlipMeshApp* app, const char* fmt, ...);
void fm_status(FlipMeshApp* app, const char* msg);

int32_t flipmesh_app_entry(void* p);
