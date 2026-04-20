#include "flipmesh.h"
#include "fm_gui.h"
#include "fm_uart.h"
#include "fm_protocol.h"
#include "fm_settings.h"
#include "fm_channel.h"
#include "fm_notify.h"

int32_t flipmesh_app_entry(void* p) {
    (void)p;

    FlipMeshApp* app = malloc(sizeof(FlipMeshApp));
    memset(app, 0, sizeof(FlipMeshApp));

    /* Heap-allocate frame buffer to keep app struct size down */
    app->frame_buf = malloc(FM_MAX_FRAME);
    if(!app->frame_buf) {
        free(app);
        return -1;
    }

    app->lock = furi_mutex_alloc(FuriMutexTypeNormal);

    /* Defaults */
    app->uart_id             = FuriHalSerialIdUsart;
    app->baud                = 115200;
    app->vib_on        = true;
    app->led_on          = true;
    app->tone     = FMTonePing;
    app->scroll_spd        = 5;
    app->framerate    = 5;
    app->long_msg            = FM_LMH_SCROLL;
    app->hb_idx = 1; /* 30 s */
    app->show_ts     = false;
    app->page             = FM_PAGE_MESSAGES;
    app->conn          = FM_CONN_IDLE;

    fm_ch_init(app);
    settings_load(app);

    snprintf(app->status_line, sizeof(app->status_line), "Connecting...");

    app->rx_stream = furi_stream_buffer_alloc(FM_RX_BUF, 1);

    app->gui = furi_record_open(RECORD_GUI);
    app->vp  = view_port_alloc();
    view_port_draw_callback_set(app->vp, render_cb, app);
    view_port_input_callback_set(app->vp, input_cb, app);
    gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);

    fm_uart_open(app);

    app->quit = false;
    app->rx_thread   = furi_thread_alloc_ex("zm_rx", 4096, fm_rx_thread, app);
    furi_thread_start(app->rx_thread);

    /* Give UART a moment then kick off config sync */
    furi_delay_ms(500);
    fm_proto_sync(app);

    static const uint32_t frame_delays[] = {
        1000, 500, 333, 250, 200, 166, 142, 125, 111, 100
    };

    while(!app->quit) {
        if(app->kb_active) {
            gui_remove_view_port(app->gui, app->vp);

            app->kb_vd = view_dispatcher_alloc();
            app->kb_input    = text_input_alloc();

            text_input_set_header_text(app->kb_input, "Send FMMessage:");
            text_input_set_result_callback(
                app->kb_input,
                text_input_callback,
                app,
                app->kb_buf,
                sizeof(app->kb_buf),
                false);

            view_dispatcher_add_view(app->kb_vd, 0,
                                     text_input_get_view(app->kb_input));
            view_set_previous_callback(text_input_get_view(app->kb_input),
                                       kb_back_callback);
            view_dispatcher_attach_to_gui(app->kb_vd, app->gui,
                                          ViewDispatcherTypeFullscreen);
            view_dispatcher_switch_to_view(app->kb_vd, 0);
            view_dispatcher_run(app->kb_vd);

            view_dispatcher_remove_view(app->kb_vd, 0);
            text_input_free(app->kb_input);
            view_dispatcher_free(app->kb_vd);
            app->kb_active = false;

            gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);
        } else {
            uint32_t now   = furi_get_tick();
            uint32_t delay = frame_delays[app->framerate > 0 ?
                                          app->framerate - 1 : 0];
            static uint32_t last_render = 0;
            if(now - last_render >= delay) {
                view_port_update(app->vp);
                last_render = now;
            } else {
                furi_delay_ms(10);
            }
        }
    }

    settings_save(app);

    fm_hb_stop(app);

    app->quit = true;
    furi_thread_join(app->rx_thread);
    furi_thread_free(app->rx_thread);

    fm_uart_close(app);

    gui_remove_view_port(app->gui, app->vp);
    view_port_free(app->vp);
    furi_record_close(RECORD_GUI);

    furi_stream_buffer_free(app->rx_stream);
    furi_mutex_free(app->lock);

    free(app->frame_buf);
    free(app);

    return 0;
}
