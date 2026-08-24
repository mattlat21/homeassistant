#include "sys_debug.h"

#include <stdio.h>

#include "app_prefs.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

void sys_debug_get_snapshot(sys_debug_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }
    out->heap_free = esp_get_free_heap_size();
    out->heap_min_free = esp_get_minimum_free_heap_size();
    out->internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    out->internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    out->internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    out->spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->spiram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    out->spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    out->dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    out->boot_count = app_prefs_get_boot_count();
    out->restart_reason = app_prefs_get_restart_reason_str();
}

void sys_debug_format_bytes(size_t bytes, char *out, size_t out_sz)
{
    if (out == NULL || out_sz == 0) {
        return;
    }
    if (bytes < 1024u) {
        snprintf(out, out_sz, "%u B", (unsigned)bytes);
        return;
    }
    if (bytes < 1024u * 1024u) {
        double kib = (double)bytes / 1024.0;
        snprintf(out, out_sz, "%.1f KiB", kib);
        return;
    }
    double mib = (double)bytes / (1024.0 * 1024.0);
    snprintf(out, out_sz, "%.2f MiB", mib);
}
