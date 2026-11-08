#include "core.h"
#include <stdio.h>
#include <string.h>

int gtavcs_core_boot(const char *asset_root)
{
    fprintf(stderr, "[smoke] asset root: %s\n", asset_root);
    fprintf(stderr, "[smoke] platform only; recompiled game is NOT linked\n");
    return 0;
}

int gtavcs_core_frame(const GtavcsPad *pad, int width, int height, double dt)
{
    (void)pad; (void)width; (void)height; (void)dt;
    return 1;
}

size_t gtavcs_core_audio(int16_t *samples, size_t capacity_frames)
{
    memset(samples, 0, capacity_frames * 2 * sizeof(*samples));
    return capacity_frames; /* Silence exercises streaming, not game audio. */
}

void gtavcs_core_shutdown(void) {}
