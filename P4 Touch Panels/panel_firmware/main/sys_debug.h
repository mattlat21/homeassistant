#pragma once

#include <stddef.h>
#include <stdint.h>

/** Live heap / SPIRAM snapshot for Debug UI and MQTT `status/memory`. */
typedef struct {
    size_t heap_free;
    size_t heap_min_free;
    size_t internal_free;
    size_t internal_total;
    size_t internal_largest;
    size_t spiram_free;
    size_t spiram_total;
    size_t spiram_largest;
    size_t dma_free;
    uint32_t boot_count;
    const char *restart_reason;
} sys_debug_snapshot_t;

void sys_debug_get_snapshot(sys_debug_snapshot_t *out);

/** Format bytes as `123 B`, `45.6 KiB`, or `1.23 MiB` into @a out. */
void sys_debug_format_bytes(size_t bytes, char *out, size_t out_sz);
