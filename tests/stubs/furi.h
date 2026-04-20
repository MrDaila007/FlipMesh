#pragma once
/* Minimal Furi RTOS stub for host-side unit tests.
 * Provides just enough types and macros for the modules under test. */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Opaque kernel types — used only as pointers */
typedef struct FuriMutex       FuriMutex;
typedef struct FuriThread      FuriThread;
typedef struct FuriStreamBuffer FuriStreamBuffer;
typedef struct FuriTimer       FuriTimer;

#define FuriWaitForever 0xFFFFFFFFU

/* Mutex — noops; NULL lock pointer is safe with these stubs */
static inline void furi_mutex_acquire(FuriMutex* m, uint32_t t) {
    (void)m;
    (void)t;
}
static inline void furi_mutex_release(FuriMutex* m) {
    (void)m;
}

/* Time — implemented in tests/stubs.c, controllable via test_set_tick() */
uint32_t furi_get_tick(void);
uint32_t furi_kernel_get_tick_frequency(void);

/* Records */
void* furi_record_open(const char* name);
void  furi_record_close(const char* name);

/* Logging — all suppressed */
#define FURI_LOG_I(...) ((void)0)
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
