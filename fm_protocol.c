#include "fm_protocol.h"
#include "fm_history.h"
#include "fm_notify.h"
#include "fm_roster.h"
#include "fm_uart.h"

#define TAG "flipmesh"

/* ── Framing ──────────────────────────────────────────────────────────────── */

static void framing_reset(FlipMeshApp* app) {
    app->hdr_pos   = 0;
    app->frame_len = 0;
    app->frame_pos = 0;
}

/* Returns true when a complete frame is in frame_buf[0..frame_len-1]. */
static bool framing_feed(FlipMeshApp* app, uint8_t b) {
    if(app->hdr_pos < 4) {
        app->hdr[app->hdr_pos++] = b;
        if(app->hdr_pos == 1 && app->hdr[0] != FM_MAGIC0) {
            app->rx_err_magic++;
            app->bad_streak++;
            app->hdr_pos = 0;
            if(app->bad_streak >= 5) {
                fm_log(app, "Resyncing...");
                app->bad_streak = 0;
            }
        } else if(app->hdr_pos == 2 && app->hdr[1] != FM_MAGIC1) {
            app->rx_err_magic++;
            app->bad_streak++;
            app->hdr_pos = 0;
            if(app->bad_streak >= 5) {
                fm_log(app, "Resyncing...");
                app->bad_streak = 0;
            }
        } else if(app->hdr_pos == 4) {
            app->bad_streak = 0;
            app->frame_len = ((uint16_t)app->hdr[2] << 8) | (uint16_t)app->hdr[3];
            app->frame_pos = 0;
            if(app->frame_len == 0 || app->frame_len > FM_MAX_FRAME) {
                app->rx_err_len++;
                fm_log(app, "Bad len: %u", app->frame_len);
                framing_reset(app);
            }
        }
        return false;
    }

    app->frame_buf[app->frame_pos++] = b;
    if(app->frame_pos == app->frame_len) {
        return true;
    }
    if(app->frame_pos > app->frame_len) {
        app->rx_err_len++;
        framing_reset(app);
    }
    return false;
}

/* ── TX helpers ───────────────────────────────────────────────────────────── */

static void send_frame(FlipMeshApp* app, const uint8_t* payload, size_t len) {
    if(!app || !app->serial) return;
    uint8_t hdr[4] = {
        FM_MAGIC0,
        FM_MAGIC1,
        (uint8_t)((len >> 8) & 0xFF),
        (uint8_t)(len & 0xFF),
    };
    furi_hal_serial_tx(app->serial, hdr, sizeof(hdr));
    furi_hal_serial_tx(app->serial, payload, len);
    app->tx_ok++;
}

void fm_proto_heartbeat(FlipMeshApp* app) {
    if(!app || !app->serial) return;
    meshtastic_ToRadio to    = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_heartbeat_tag;
    to.heartbeat.nonce = ++app->hb_nonce;

    uint8_t buf[32];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        send_frame(app, buf, os.bytes_written);
        fm_log(app, "Heartbeat sent (%lu)", (unsigned long)app->hb_nonce);
    }
}

void fm_proto_sync(FlipMeshApp* app) {
    if(!app || !app->serial) return;
    app->conn    = FM_CONN_SYNC;
    app->sync_id = (uint32_t)furi_hal_random_get();
    if(app->sync_id == 0) app->sync_id = 1;

    meshtastic_ToRadio to    = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_want_config_id_tag;
    to.want_config_id = app->sync_id;

    uint8_t buf[32];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        send_frame(app, buf, os.bytes_written);
        fm_log(app, "Config sync started (id=%lu)", (unsigned long)app->sync_id);
        fm_status(app, "Syncing...");
    }
}

void fm_proto_send_text(FlipMeshApp* app, const char* text, uint32_t to_node) {
    if(!app || !app->serial || !text || text[0] == '\0') return;

    meshtastic_ToRadio to    = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_packet_tag;

    meshtastic_MeshPacket* p = &to.packet;
    p->to        = to_node;
    p->channel   = app->active_ch;
    p->id        = (uint32_t)furi_hal_random_get();
    p->hop_limit = 3;
    p->want_ack  = true;

    app->echo_ids[app->echo_head] = p->id;
    app->echo_head = (app->echo_head + 1) % FM_ECHO_RING;

    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    meshtastic_Data* d       = &p->decoded;
    d->portnum               = meshtastic_PortNum_TEXT_MESSAGE_APP;
    d->want_response         = false;

    size_t tlen = strlen(text);
    if(tlen > sizeof(d->payload.bytes) - 1) tlen = sizeof(d->payload.bytes) - 1;
    memcpy(d->payload.bytes, text, tlen);
    d->payload.size = (pb_size_t)tlen;

    uint8_t buf[FM_MAX_FRAME];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(!pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        app->tx_err++;
        fm_log(app, "TX encode fail");
        fm_status(app, "Send failed");
        return;
    }

    send_frame(app, buf, os.bytes_written);
    fm_history_add(app, text, app->self_id, to_node, true);
    fm_log(app, "TX: %s", text);
    fm_status(app, "Sent!");
}

/* ── RX decode ────────────────────────────────────────────────────────────── */

static bool is_echo(FlipMeshApp* app, uint32_t pkt_id) {
    if(pkt_id == 0) return false;
    for(uint8_t i = 0; i < FM_ECHO_RING; i++) {
        if(app->echo_ids[i] == pkt_id) return true;
    }
    return false;
}

static void handle_text(FlipMeshApp* app, const meshtastic_MeshPacket* p,
                        const uint8_t* payload, size_t plen) {
    if(plen == 0 || plen >= FM_MSG_TEXT_MAX) return;

    char text[FM_MSG_TEXT_MAX];
    memcpy(text, payload, plen);
    text[plen] = '\0';

    fm_history_add(app, text, p->from, p->to, false);

    if(p->to == app->self_id) {
        fm_node_mark_dm(app, p->from);
    }

    fm_log(app, "RX msg: %s", text);
    fm_status(app, "New message");
    fm_notify_message(app);
    view_port_update(app->vp);
}

static void handle_telemetry(FlipMeshApp* app, uint32_t from_id,
                             const uint8_t* payload, size_t plen) {
    meshtastic_Telemetry tel = meshtastic_Telemetry_init_default;
    pb_istream_t is = pb_istream_from_buffer(payload, plen);
    if(!pb_decode(&is, meshtastic_Telemetry_fields, &tel)) return;

    if(tel.which_variant == meshtastic_Telemetry_device_metrics_tag) {
        const meshtastic_DeviceMetrics* dm = &tel.variant.device_metrics;
        fm_node_update_metrics(app, from_id,
            dm->battery_level, dm->voltage,
            dm->channel_utilization, dm->air_util_tx,
            dm->uptime_seconds);
        fm_log(app, "Telemetry from %08lX: bat=%lu%%",
                 (unsigned long)from_id, (unsigned long)dm->battery_level);
    } else if(tel.which_variant == meshtastic_Telemetry_environment_metrics_tag) {
        const meshtastic_EnvironmentMetrics* em = &tel.variant.environment_metrics;
        fm_node_update_env(app, from_id,
            em->temperature, em->relative_humidity, em->barometric_pressure);
        fm_log(app, "Env from %08lX: %.1fC", (unsigned long)from_id, (double)em->temperature);
    }
}

static void handle_nodeinfo(FlipMeshApp* app, const meshtastic_MeshPacket* p,
                            const uint8_t* payload, size_t plen) {
    meshtastic_User user = meshtastic_User_init_default;
    pb_istream_t is = pb_istream_from_buffer(payload, plen);
    if(!pb_decode(&is, meshtastic_User_fields, &user)) return;
    fm_node_update_user(app, p->from, &user);
    fm_log(app, "NodeInfo: %s (%s)", user.long_name, user.short_name);
}

static void handle_position(FlipMeshApp* app, const meshtastic_MeshPacket* p,
                            const uint8_t* payload, size_t plen) {
    meshtastic_Position pos = meshtastic_Position_init_default;
    pb_istream_t is = pb_istream_from_buffer(payload, plen);
    if(!pb_decode(&is, meshtastic_Position_fields, &pos)) return;
    if(pos.has_latitude_i && pos.has_longitude_i) {
        fm_node_update_pos(app, p->from, &pos);
        fm_log(app, "Position from %08lX", (unsigned long)p->from);
    }
}

static void decode_packet(FlipMeshApp* app, const meshtastic_MeshPacket* p) {
    if(p->which_payload_variant != meshtastic_MeshPacket_decoded_tag) return;

    const meshtastic_Data* d = &p->decoded;
    const uint8_t* pl        = d->payload.bytes;
    size_t         pl_len    = d->payload.size;

    switch(d->portnum) {
    case meshtastic_PortNum_TEXT_MESSAGE_APP:
        handle_text(app, p, pl, pl_len);
        break;
    case meshtastic_PortNum_TELEMETRY_APP:
        handle_telemetry(app, p->from, pl, pl_len);
        break;
    case meshtastic_PortNum_NODEINFO_APP:
        handle_nodeinfo(app, p, pl, pl_len);
        break;
    case meshtastic_PortNum_POSITION_APP:
        handle_position(app, p, pl, pl_len);
        break;
    case meshtastic_PortNum_ROUTING_APP:
        fm_log(app, "Routing from %08lX", (unsigned long)p->from);
        break;
    default:
        fm_log(app, "Port %d from %08lX", (int)d->portnum, (unsigned long)p->from);
        break;
    }
}

static void decode_fromradio(FlipMeshApp* app, const uint8_t* frame, size_t len) {
    meshtastic_FromRadio from = meshtastic_FromRadio_init_default;
    pb_istream_t is = pb_istream_from_buffer(frame, len);

    if(!pb_decode(&is, meshtastic_FromRadio_fields, &from)) {
        app->rx_err_decode++;
        fm_log(app, "Decode fail");
        return;
    }
    app->rx_ok++;

    switch(from.which_payload_variant) {
    case meshtastic_FromRadio_packet_tag: {
        const meshtastic_MeshPacket* p = &from.packet;
        if(is_echo(app, p->id)) break;

        app->last_from = p->from;
        app->last_to   = p->to;
        if(p->rx_rssi != 0 || p->rx_snr != 0) {
            app->last_rssi = p->rx_rssi;
            app->last_snr  = p->rx_snr;
            app->has_signal = true;
        }

        furi_mutex_acquire(app->lock, FuriWaitForever);
        fm_node_update_signal(app, p->from, p->rx_snr, p->rx_rssi);
        decode_packet(app, p);
        furi_mutex_release(app->lock);
        break;
    }

    case meshtastic_FromRadio_my_info_tag: {
        const meshtastic_MyNodeInfo* info = &from.my_info;
        app->self_id = info->my_node_num;
        fm_log(app, "My ID: %08lX", (unsigned long)app->self_id);
        break;
    }

    case meshtastic_FromRadio_node_info_tag: {
        furi_mutex_acquire(app->lock, FuriWaitForever);
        fm_node_update_info(app, &from.node_info);
        furi_mutex_release(app->lock);
        break;
    }

    case meshtastic_FromRadio_config_complete_id_tag: {
        uint32_t rcvd = from.config_complete_id;
        if(rcvd == app->sync_id) {
            app->conn = FM_CONN_LIVE;
            fm_status(app, "Connected");
            fm_log(app, "Sync complete. Nodes: %u", app->roster.count);
            fm_hb_start(app);
        } else {
            fm_log(app, "Sync ID mismatch (%lu)", (unsigned long)rcvd);
        }
        break;
    }

    case meshtastic_FromRadio_metadata_tag: {
        const meshtastic_DeviceMetadata* meta = &from.metadata;
        fm_log(app, "FW: %s", meta->firmware_version);
        break;
    }

    default:
        break;
    }
}

/* ── RX thread ────────────────────────────────────────────────────────────── */

int32_t fm_rx_thread(void* ctx) {
    FlipMeshApp* app = (FlipMeshApp*)ctx;
    framing_reset(app);
    uint8_t b;
    while(!app->quit) {
        if(furi_stream_buffer_receive(app->rx_stream, &b, 1, 100) > 0) {
            if(framing_feed(app, b)) {
                decode_fromradio(app, app->frame_buf, app->frame_len);
                framing_reset(app);
            }
        }
    }
    return 0;
}
