#include "fm_roster.h"
#include <string.h>
#include <stdio.h>

FMNode* fm_node_get(FlipMeshApp* app, uint32_t node_id) {
    FMRoster* r = &app->roster;

    for(uint8_t i = 0; i < r->count; i++) {
        if(r->nodes[i].node_id == node_id) return &r->nodes[i];
    }

    uint8_t idx;
    if(r->count < FM_ROSTER_MAX) {
        idx = r->count++;
    } else {
        /* Evict the oldest entry (smallest last_heard). */
        idx = 0;
        for(uint8_t i = 1; i < FM_ROSTER_MAX; i++) {
            if(r->nodes[i].last_heard < r->nodes[idx].last_heard) idx = i;
        }
    }

    FMNode* e = &r->nodes[idx];
    memset(e, 0, sizeof(FMNode));
    e->node_id = node_id;
    return e;
}

void fm_node_update_info(FlipMeshApp* app, const meshtastic_NodeInfo* info) {
    FMNode* e = fm_node_get(app, info->num);

    if(info->has_user) {
        strncpy(e->long_name,  info->user.long_name,  sizeof(e->long_name) - 1);
        strncpy(e->short_name, info->user.short_name, sizeof(e->short_name) - 1);
        e->hw_model = (uint8_t)info->user.hw_model;
    }

    e->last_heard = info->last_heard;
    e->snr        = info->snr;
    e->hops_away  = info->hops_away;
    e->via_mqtt   = info->via_mqtt;

    if(info->has_position && info->position.has_latitude_i && info->position.has_longitude_i) {
        e->has_position  = true;
        e->latitude_i    = info->position.lat_i;
        e->longitude_i   = info->position.lon_i;
        e->pos_timestamp = info->position.time;
    }

    if(info->has_device_metrics) {
        e->battery_pct = (uint8_t)info->device_metrics.battery_pct;
        e->voltage       = info->device_metrics.voltage;
        e->ch_util       = info->device_metrics.channel_utilization;
        e->air_util      = info->device_metrics.air_util_tx;
        e->uptime_seconds = info->device_metrics.uptime_s;
        e->has_telemetry = true;
    }
}

void fm_node_update_user(FlipMeshApp* app, uint32_t node_id, const meshtastic_User* user) {
    FMNode* e = fm_node_get(app, node_id);
    strncpy(e->long_name,  user->long_name,  sizeof(e->long_name) - 1);
    strncpy(e->short_name, user->short_name, sizeof(e->short_name) - 1);
    e->hw_model = (uint8_t)user->hw_model;
}

void fm_node_update_signal(FlipMeshApp* app, uint32_t node_id, float snr, int32_t rssi) {
    FMNode* e = fm_node_get(app, node_id);
    e->snr        = snr;
    e->rssi       = (int16_t)rssi;
    e->last_heard = (uint32_t)(furi_get_tick() / 1000);
}

void fm_node_update_metrics(FlipMeshApp* app, uint32_t node_id,
    uint32_t battery, float voltage, float ch_util, float air_util, uint32_t uptime) {
    FMNode* e     = fm_node_get(app, node_id);
    e->battery_pct = (uint8_t)battery;
    e->voltage       = voltage;
    e->ch_util       = ch_util;
    e->air_util      = air_util;
    e->uptime_seconds = uptime;
    e->has_telemetry = true;
}

void fm_node_update_env(FlipMeshApp* app, uint32_t node_id,
    float temp, float humidity, float pressure) {
    FMNode* e    = fm_node_get(app, node_id);
    e->temperature  = temp;
    e->humidity     = humidity;
    e->pressure     = pressure;
    e->has_env_metrics = true;
}

void fm_node_update_pos(FlipMeshApp* app, uint32_t node_id,
    const meshtastic_Position* pos) {
    FMNode* e    = fm_node_get(app, node_id);
    e->has_position = true;
    e->latitude_i   = pos->latitude_i;
    e->longitude_i  = pos->longitude_i;
    e->pos_timestamp = pos->time;
}

void fm_node_mark_dm(FlipMeshApp* app, uint32_t node_id) {
    for(uint8_t i = 0; i < app->roster.count; i++) {
        if(app->roster.nodes[i].node_id == node_id) {
            app->roster.nodes[i].unread_dm = true;
            return;
        }
    }
    /* Node not yet in roster — create it so the DM flag is preserved. */
    FMNode* e = fm_node_get(app, node_id);
    e->has_new_dm = true;
}

void fm_node_fmt_id(uint32_t node_id, char* buf, size_t len) {
    snprintf(buf, len, "!%04lX", (unsigned long)(node_id & 0xFFFF));
}

void fm_node_display(const FMNode* entry, char* buf, size_t len) {
    if(entry->short_name[0] != '\0') {
        snprintf(buf, len, "%s", entry->short_name);
    } else {
        fm_node_fmt_id(entry->node_id, buf, len);
    }
}
