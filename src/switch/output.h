/* Mode-appropriate output on one fixed window surface (M06-20).
 *
 * switch-sdl2 2.28 leaks the EGL surface's buffers at every window resize (M05), so the window keeps one size for
 * the whole run: the largest output, 1920x1080. The picture is drawn at the operation mode's output size (1280x720
 * handheld, 1920x1080 docked) and the layer is cropped to that region (nwindowSetCrop), which the compositor then
 * scales to the panel or the TV. Nothing is reallocated at a dock change. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define GTAVCS_SURFACE_WIDTH 1920
#define GTAVCS_SURFACE_HEIGHT 1080

/* Main thread, once per loop iteration: reads the operation mode and, when the output size changes, sets the crop
 * of the default NWindow. `surface_width` x `surface_height` is the drawable SDL reports. Returns 1 on a change. */
int gtavcs_output_update(int surface_width, int surface_height);
/* The current output size: what the renderer treats as the window's back buffer (0 x 0 before the first update). */
void gtavcs_output_size(int *width, int *height);

#ifdef __cplusplus
}
#endif
