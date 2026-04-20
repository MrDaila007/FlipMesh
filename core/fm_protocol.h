// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#pragma once
#include "flipmesh.h"

/* Called from app entry: send want_config_id to start config sync */
void fm_proto_sync(FlipMeshApp* app);

/* Sends a periodic heartbeat to keep connection alive */
void fm_proto_heartbeat(FlipMeshApp* app);

/* Send a text message to the given node (0xFFFFFFFF = broadcast) */
void fm_proto_send_text(FlipMeshApp* app, const char* text, uint32_t to_node);

/* Raw FromRadio protobuf (UART framing stripped or BLE characteristic payload). */
void fm_proto_deliver_fromradio(FlipMeshApp* app, const uint8_t* payload, size_t len);

void fm_hb_start(FlipMeshApp* app);
void fm_hb_stop(FlipMeshApp* app);

void fm_proto_transport_set_ready(FlipMeshApp* app, bool ready);

/* RX thread entry point (UART app) */
int32_t fm_rx_thread(void* ctx);
