// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#include "../../core/flipmesh.h"
#include "../../core/fm_transport.h"
#include <furi_hal.h>

bool fm_transport_tx(FlipMeshApp* app, const uint8_t* payload, size_t len) {
    if(!app || !app->serial || !payload || len == 0) return false;

    uint8_t hdr[4] = {
        FM_MAGIC0,
        FM_MAGIC1,
        (uint8_t)((len >> 8) & 0xFF),
        (uint8_t)(len & 0xFF),
    };
    furi_hal_serial_tx(app->serial, hdr, sizeof(hdr));
    furi_hal_serial_tx(app->serial, payload, len);
    app->tx_ok++;
    return true;
}

bool fm_transport_ready(const FlipMeshApp* app) {
    return app && app->serial;
}
