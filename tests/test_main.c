// SPDX-License-Identifier: GPL-3.0-or-later
/* Host unit tests for flipmesh core (history, roster). */

#include "furi.h"
#include "flipmesh.h"
#include "fm_history.h"
#include "fm_roster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures;

#define TEST_ASSERT(cond, msg)                                                         \
    do {                                                                             \
        if(!(cond)) {                                                                \
            fprintf(stderr, "FAIL %s:%d: %s — (%s)\n", __FILE__, __LINE__, msg, #cond); \
            g_failures++;                                                            \
        }                                                                            \
    } while(0)

static FlipMeshApp* make_app(void) {
    FlipMeshApp* app = calloc(1, sizeof(FlipMeshApp));
    TEST_ASSERT(app != NULL, "calloc app");
    return app;
}

static void free_app(FlipMeshApp* app) {
    free(app);
}

static void test_history_ring(void) {
    FlipMeshApp* app = make_app();
    test_set_tick(8000);

    fm_history_add(app, "first", 1, 2, false);
    TEST_ASSERT(app->history.count == 1, "count after one add");
    TEST_ASSERT(app->history.head == 0, "head after one add");
    TEST_ASSERT(strcmp(app->history.buf[0].text, "first") == 0, "first text");

    char msg[24];
    for(int i = 0; i < FM_MSG_HISTORY - 1; i++) {
        snprintf(msg, sizeof(msg), "m%d", i);
        fm_history_add(app, msg, 1, 2, false);
    }
    TEST_ASSERT(app->history.count == FM_MSG_HISTORY, "full buffer");

    fm_history_add(app, "overflow", 3, 4, true);
    TEST_ASSERT(app->history.count == FM_MSG_HISTORY, "still full after overflow");
    TEST_ASSERT(app->history.head == 1, "head advanced after eviction");

    TEST_ASSERT(strcmp(app->history.buf[app->history.head].text, "m0") == 0, "oldest is m0");
    uint8_t newest = (uint8_t)((app->history.head + app->history.count - 1) % FM_MSG_HISTORY);
    TEST_ASSERT(strcmp(app->history.buf[newest].text, "overflow") == 0, "newest text");
    TEST_ASSERT(app->history.buf[newest].outgoing == true, "tx flag on newest");

    free_app(app);
}

static void test_history_null_safe(void) {
    fm_history_add(NULL, "x", 0, 0, false);
    fm_history_add((FlipMeshApp*)(void*)1, NULL, 0, 0, false);
}

static void test_fm_log_ring(void) {
    FlipMeshApp* app = make_app();
    for(int i = 0; i < FM_LOG_ROWS + 3; i++) {
        fm_log(app, "row%d", i);
    }
    TEST_ASSERT(app->log_head == 3u, "log_head after wrap");
    free_app(app);
}

static void test_roster_get_and_display(void) {
    FlipMeshApp* app = make_app();
    FMNode* a = fm_node_get(app, 0xA3F0C0DEu);
    TEST_ASSERT(app->roster.count == 1, "one node");
    TEST_ASSERT(a->node_id == 0xA3F0C0DEu, "node id");

    char buf[16];
    fm_node_fmt_id(0x0000BEEFu, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "!BEEF") == 0, "fmt id low 16 bits");

    strncpy(a->short_name, "AB", sizeof(a->short_name));
    fm_node_display(a, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "AB") == 0, "display short name");

    a->short_name[0] = '\0';
    fm_node_display(a, buf, sizeof(buf));
    TEST_ASSERT(strcmp(buf, "!C0DE") == 0, "display hex when no short");

    free_app(app);
}

static void test_roster_mark_dm_creates_node(void) {
    FlipMeshApp* app = make_app();
    fm_node_mark_dm(app, 0x55u);
    TEST_ASSERT(app->roster.count >= 1, "roster has node");
    FMNode* e = fm_node_get(app, 0x55u);
    TEST_ASSERT(e->unread_dm == true, "unread set");
    free_app(app);
}

static void test_roster_evict_oldest_last_heard(void) {
    FlipMeshApp* app = make_app();
    for(uint32_t i = 0; i < FM_ROSTER_MAX; i++) {
        FMNode* e = fm_node_get(app, 100u + i);
        e->last_heard = (uint32_t)i;
    }
    TEST_ASSERT(app->roster.count == FM_ROSTER_MAX, "roster full");

    FMNode* n = fm_node_get(app, 999u);
    (void)n;
    bool found_old = false;
    for(uint8_t i = 0; i < app->roster.count; i++) {
        if(app->roster.nodes[i].node_id == 100u) found_old = true;
    }
    TEST_ASSERT(!found_old, "node 100 evicted");
    found_old = false;
    for(uint8_t i = 0; i < app->roster.count; i++) {
        if(app->roster.nodes[i].node_id == 999u) found_old = true;
    }
    TEST_ASSERT(found_old, "node 999 present");

    free_app(app);
}

static void test_roster_update_signal_last_heard(void) {
    FlipMeshApp* app = make_app();
    test_set_tick(12000);
    fm_node_update_signal(app, 7u, 2.5f, -80);
    FMNode* e = fm_node_get(app, 7u);
    TEST_ASSERT(e->last_heard == 12u, "last_heard from tick ms/1000");
    TEST_ASSERT(e->snr > 2.4f && e->snr < 2.6f, "snr stored");
    free_app(app);
}

static void test_roster_update_info(void) {
    FlipMeshApp* app = make_app();

    meshtastic_NodeInfo info = meshtastic_NodeInfo_init_zero;
    info.num        = 42u;
    info.has_user   = true;
    strncpy(info.user.long_name, "Long Name", sizeof(info.user.long_name));
    strncpy(info.user.short_name, "LN", sizeof(info.user.short_name));
    info.user.hw_model = meshtastic_HardwareModel_TBEAM;
    info.last_heard    = 9001u;
    info.snr           = 3.0f;
    info.hops_away     = 2u;
    info.via_mqtt      = true;

    info.has_position = true;
    info.position.has_latitude_i  = true;
    info.position.latitude_i      = 55000000;
    info.position.has_longitude_i = true;
    info.position.longitude_i     = 37000000;
    info.position.time            = 12345u;

    info.has_device_metrics              = true;
    info.device_metrics.battery_level    = 77u;
    info.device_metrics.voltage          = 4.1f;
    info.device_metrics.channel_utilization = 0.2f;
    info.device_metrics.air_util_tx      = 0.05f;
    info.device_metrics.uptime_seconds   = 60u;

    fm_node_update_info(app, &info);

    FMNode* e = fm_node_get(app, 42u);
    TEST_ASSERT(strcmp(e->long_name, "Long Name") == 0, "long name");
    TEST_ASSERT(strcmp(e->short_name, "LN") == 0, "short name");
    TEST_ASSERT(e->last_heard == 9001u, "last_heard from info");
    TEST_ASSERT(e->hops == 2u, "hops");
    TEST_ASSERT(e->via_mqtt == true, "mqtt flag");
    TEST_ASSERT(e->has_gps == true, "gps flag");
    TEST_ASSERT(e->lat_i == 55000000, "lat");
    TEST_ASSERT(e->lon_i == 37000000, "lon");
    TEST_ASSERT(e->has_metrics == true, "metrics");
    TEST_ASSERT(e->battery_pct == 77u, "battery");

    free_app(app);
}

int main(void) {
    test_history_null_safe();
    test_history_ring();
    test_fm_log_ring();
    test_roster_get_and_display();
    test_roster_mark_dm_creates_node();
    test_roster_evict_oldest_last_heard();
    test_roster_update_signal_last_heard();
    test_roster_update_info();

    if(g_failures != 0) {
        fprintf(stderr, "\n%d test assertion(s) failed.\n", g_failures);
        return 1;
    }
    fprintf(stderr, "All flipmesh host tests passed.\n");
    return 0;
}
