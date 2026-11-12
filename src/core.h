#ifndef GTAVCS_CORE_H
#define GTAVCS_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Port-owned seam between src/main.c (process, SDL2, window, GL context, applet
 * lifecycle) and the C++ core adapter (src/core/core.cpp: upstream's launcher steps,
 * PS2Runtime and the SDL2 platform layer). No exceptions cross this C ABI.
 *
 * Order on the main thread, with the GL context current:
 *   gtavcs_core_boot  once, after SDL_Init(VIDEO|AUDIO|GAMECONTROLLER) and the one
 *                     SDL_WINDOW_OPENGL window; it adopts that window, never makes another.
 *   gtavcs_core_frame once per host frame while in focus; it pumps the SDL event queue
 *                     (the core owns events from boot to shutdown) and paces itself.
 *   gtavcs_core_set_focus on every focus change (HOME, sleep, resume).
 *   gtavcs_core_shutdown once, even after a failed boot: stops and joins every worker
 *                     and releases the disc before returning. SDL and the window stay. */

/* 0 on success; nonzero is a GTAVCS_EXIT_* code, with the reason in the log.
 * A failed boot has already unwound what it started. */
int gtavcs_core_boot(const char *asset_root);
/* >0: continue, 0: orderly stop (game ended, quit, timed run finished), <0: error. */
int gtavcs_core_frame(void);
void gtavcs_core_set_focus(int focused);
void gtavcs_core_shutdown(void);
/* One line naming why boot or a frame failed; empty when nothing failed. */
const char *gtavcs_core_error(void);

enum {
    GTAVCS_EXIT_ERROR = 1,          /* exception, runtime stop, bad run configuration */
    GTAVCS_EXIT_DISC_UNREADABLE = 3, /* folder disc manifest or files invalid */
    GTAVCS_EXIT_WRONG_DISC = 4,     /* extracted ELF missing or not SLUS-21590 v1.00 */
    GTAVCS_EXIT_INIT_FAILED = 5     /* runtime, platform or presenter could not start */
};

#ifdef __cplusplus
}
#endif
#endif
