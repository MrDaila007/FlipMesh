// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#include "fm_bt_transport.h"
#include "../../core/fm_protocol.h"
#include "../../core/fm_transport.h"

#include <stdio.h>
#include <string.h>

bool fm_transport_tx(FlipMeshApp* app, const uint8_t* payload, size_t len) {
    (void)payload;
    (void)len;
    if(!app || !app->bt_link_ready) return false;
    /* Placeholder until BLE GATT client is available on this platform. */
    app->ble_tx_fail++;
    return false;
}

bool fm_transport_ready(const FlipMeshApp* app) {
    return app && app->bt_link_ready;
}

void fm_bt_transport_init(FlipMeshApp* app) {
    if(!app) return;
    app->bt_link_ready = false;
    app->ble_auto_reconnect = true;
    app->transport_heartbeat_allowed = false;
    /* Stock FW: no public BLE Central API for external FAPs. */
    snprintf(app->ble_status, sizeof(app->ble_status), "Use FlipMesh UART");
}

void fm_bt_scan_action(FlipMeshApp* app) {
    if(!app) return;
    snprintf(app->ble_status, sizeof(app->ble_status), "No scan (stock FW)");
    fm_log(app, "BLE scan needs FW API (docs/ble-feasibility-report.md)");
    if(app->vp) view_port_update(app->vp);
}

void fm_bt_connect_action(FlipMeshApp* app) {
    if(!app) return;
    if(app->bt_link_ready) {
        fm_proto_transport_set_ready(app, false);
        fm_log(app, "BLE disconnected (stub)");
    } else {
        fm_log(app, "BLE link needs FW API (stock has no GATT client for FAPs)");
        snprintf(app->ble_status, sizeof(app->ble_status), "No link (stock FW)");
    }
    if(app->vp) view_port_update(app->vp);
}
