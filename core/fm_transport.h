// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE

#pragma once

#include "flipmesh.h"

bool fm_transport_tx(FlipMeshApp* app, const uint8_t* payload, size_t len);
bool fm_transport_ready(const FlipMeshApp* app);
