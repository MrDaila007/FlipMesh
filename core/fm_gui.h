// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#pragma once
#include "flipmesh.h"

void render_cb(Canvas* canvas, void* ctx);
void input_cb(InputEvent* event, void* ctx);

uint32_t kb_back_callback(void* ctx);
void text_input_callback(void* ctx);
