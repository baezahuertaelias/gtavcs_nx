/* Mode-appropriate output on one fixed window surface (M06-20); see output.h. */
#include <switch.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "output.h"

static atomic_int output_width, output_height;
static int crop_surface_width, crop_surface_height;

int gtavcs_output_update(int surface_width, int surface_height)
{
    /* M08-02: VCS_NX_OUTPUT=1080 (a run's nxrun.cfg) draws the docked 1920x1080 output in handheld too, scaled down by
     * the panel: the menus' 1080p layout checked without a dock. Not a player setting. */
    const char *forced = getenv("VCS_NX_OUTPUT");
    const int docked = appletGetOperationMode() == AppletOperationMode_Console || (forced && strcmp(forced, "1080") == 0);
    int width = docked ? 1920 : 1280, height = docked ? 1080 : 720;
    if (width > surface_width) width = surface_width;
    if (height > surface_height) height = surface_height;
    if (width == atomic_load(&output_width) && height == atomic_load(&output_height) &&
        surface_width == crop_surface_width && surface_height == crop_surface_height)
        return 0;
    /* The renderer draws the output through the window's lower-left corner (GL counts rows from the bottom; the RHI
     * flips its top-left rectangles by the back buffer height it is given, which is the output's). Mesa's Switch EGL
     * stores the window's rows top-down and queues through nwindowQueueBuffer without a transform, so that corner is
     * the buffer's rows surface_height - height .. surface_height. The whole surface is no crop at all (all zeros),
     * the M05 path. */
    const int whole = width == surface_width && height == surface_height;
    const Result rc = whole ? nwindowSetCrop(nwindowGetDefault(), 0, 0, 0, 0)
                            : nwindowSetCrop(nwindowGetDefault(), 0, surface_height - height, width, surface_height);
    fprintf(stderr, "[video] output %dx%d on the %dx%d surface, crop %s (%08x)\n", width, height, surface_width,
        surface_height, whole ? "none" : "lower-left", rc);
    if (R_FAILED(rc)) return 0; /* keep the previous size: the crop it was drawn for still applies */
    crop_surface_width = surface_width;
    crop_surface_height = surface_height;
    atomic_store(&output_width, width);
    atomic_store(&output_height, height);
    return 1;
}

void gtavcs_output_size(int *width, int *height)
{
    *width = atomic_load(&output_width);
    *height = atomic_load(&output_height);
}
