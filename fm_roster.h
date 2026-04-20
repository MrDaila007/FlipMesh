#pragma once
#include "flipmesh.h"

/* Find or create a node entry by ID; evicts oldest if full. */
FMNode* fm_node_get(FlipMeshApp* app, uint32_t node_id);

/* Update from a full NodeInfo protobuf message received during config sync. */
void fm_node_update_info(FlipMeshApp* app, const meshtastic_NodeInfo* info);

/* Update from a User protobuf (NODEINFO_APP packet payload). */
void fm_node_update_user(FlipMeshApp* app, uint32_t node_id, const meshtastic_User* user);

/* Update signal quality data from a received packet. */
void fm_node_update_signal(FlipMeshApp* app, uint32_t node_id, float snr, int32_t rssi);

/* Update device metrics from a TELEMETRY_APP packet. */
void fm_node_update_metrics(FlipMeshApp* app, uint32_t node_id,
    uint32_t battery, float voltage, float ch_util, float air_util, uint32_t uptime);

/* Update environmental metrics from a TELEMETRY_APP packet. */
void fm_node_update_env(FlipMeshApp* app, uint32_t node_id,
    float temp, float humidity, float pressure);

/* Store last known GPS position for a node. */
void fm_node_update_pos(FlipMeshApp* app, uint32_t node_id,
    const meshtastic_Position* pos);

/* Mark that a new direct message arrived from this node. */
void fm_node_mark_dm(FlipMeshApp* app, uint32_t node_id);

/* Format a node ID as a 4-char hex display string (e.g. "!A3F0"). */
void fm_node_fmt_id(uint32_t node_id, char* buf, size_t len);

/* Return the display name for a node (short_name if set, else "!XXXX"). */
void fm_node_display(const FMNode* entry, char* buf, size_t len);
