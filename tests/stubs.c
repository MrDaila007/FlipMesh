// SPDX-License-Identifier: GPL-3.0-or-later
/* Host-side implementations for tests/stubs/furi.h */

#include "furi.h"

static uint32_t s_tick_ms;

void test_set_tick(uint32_t ms) {
    s_tick_ms = ms;
}

uint32_t furi_get_tick(void) {
    return s_tick_ms;
}

uint32_t furi_kernel_get_tick_frequency(void) {
    return 1000;
}

void* furi_record_open(const char* name) {
    (void)name;
    return NULL;
}

void furi_record_close(const char* name) {
    (void)name;
}
