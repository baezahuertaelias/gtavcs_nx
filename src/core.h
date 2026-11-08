#ifndef GTAVCS_CORE_H
#define GTAVCS_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t buttons; /* SDL controller button index -> bit; positions, not labels */
    int16_t axes[6];  /* left X/Y, right X/Y, left/right trigger */
    int connected;
} GtavcsPad;

/* Called with the GL context current on the main thread. These are port-owned
 * seams, not existing upstream functions. No exceptions cross this C ABI.
 * boot must unwind any partially initialized core state on failure. */
int gtavcs_core_boot(const char *asset_root);
/* >0: continue, 0: orderly exit, <0: error. Drawable size can change each frame. */
int gtavcs_core_frame(const GtavcsPad *pad, int width, int height, double dt);
/* Stereo interleaved signed 16-bit PCM, 48000 Hz. Return frames actually written.
 * This temporary pull API must not advance guest time to manufacture samples. */
size_t gtavcs_core_audio(int16_t *samples, size_t capacity_frames);
/* Stop/join every producer and game thread before returning. */
void gtavcs_core_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
