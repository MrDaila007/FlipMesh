#pragma once
#include "flipmesh.h"

void fm_uart_open(FlipMeshApp* app);
void fm_uart_close(FlipMeshApp* app);
void fm_uart_reopen(FlipMeshApp* app, FuriHalSerialId new_id, uint32_t new_baud);
void fm_hb_start(FlipMeshApp* app);
void fm_hb_stop(FlipMeshApp* app);
