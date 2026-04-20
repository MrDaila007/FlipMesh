// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#include "fm_uart.h"
#include "../../core/fm_history.h"
#include "../../core/fm_protocol.h"

#define TAG "flipmesh"

static void rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    FlipMeshApp* app = (FlipMeshApp*)ctx;
    if(!app) return;
    if(event == FuriHalSerialRxEventData) {
        uint8_t b = furi_hal_serial_async_rx(handle);
        app->rx_bytes++;
        furi_stream_buffer_send(app->rx_stream, &b, 1, 0);
    }
}

void fm_uart_open(FlipMeshApp* app) {
    if(!app) return;
    fm_uart_close(app);
    app->serial = furi_hal_serial_control_acquire(app->uart_id);
    furi_hal_serial_init(app->serial, app->baud);
    furi_hal_serial_async_rx_start(app->serial, rx_cb, app, false);
    fm_log(app, "UART: %s @ %lu",
             (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART",
             (unsigned long)app->baud);
    char s[64];
    snprintf(s, sizeof(s), "%s @ %lu baud",
             (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART",
             (unsigned long)app->baud);
    fm_status(app, s);
}

void fm_uart_close(FlipMeshApp* app) {
    if(!app) return;
    fm_hb_stop(app);
    if(app->serial) {
        furi_hal_serial_async_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        app->serial = NULL;
    }
}

void fm_uart_reopen(FlipMeshApp* app, FuriHalSerialId new_id, uint32_t new_baud) {
    if(!app) return;
    app->uart_id = new_id;
    app->baud = new_baud;
    fm_uart_open(app);
}
