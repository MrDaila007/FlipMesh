#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
    FuriHalSerialIdUsart  = 0,
    FuriHalSerialIdLpuart = 1,
    FuriHalSerialIdMax,
} FuriHalSerialId;

typedef struct FuriHalSerialHandle FuriHalSerialHandle;

static inline void furi_hal_serial_tx(FuriHalSerialHandle* h,
                                       const uint8_t* buf, size_t len) {
    (void)h;
    (void)buf;
    (void)len;
}
