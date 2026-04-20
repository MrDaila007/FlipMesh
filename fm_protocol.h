#pragma once
#include "flipmesh.h"

/* Called from app entry: send want_config_id to start config sync */
void fm_proto_sync(FlipMeshApp* app);

/* Sends a periodic heartbeat to keep connection alive */
void fm_proto_heartbeat(FlipMeshApp* app);

/* Send a text message to the given node (0xFFFFFFFF = broadcast) */
void fm_proto_send_text(FlipMeshApp* app, const char* text, uint32_t to_node);

/* RX thread entry point */
int32_t fm_rx_thread(void* ctx);
