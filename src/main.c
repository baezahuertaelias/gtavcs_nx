#include <glad/gl.h>
#include <SDL.h>
#include <switch.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include "core.h"

#define ASSET_ROOT "sdmc:/switch/gtavcs_nx/gamefolder"

static int nx_socket = -1;
static char last_message[512];
static int resumed;
static unsigned focus_notifications;

static void lifecycle_hook(AppletHookType type, void *parameter)
{
    (void)parameter;
    if (type == AppletHookType_OnResume) resumed = 1;
    if (type == AppletHookType_OnFocusState) ++focus_notifications;
}

/* stderr remains on SD; stdout is optionally sent to the nxlink listener. */
static void diagnostic(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(last_message, sizeof(last_message), format, args);
    va_end(args);
    fputs(last_message, stderr);
    fflush(stderr);
    if (nx_socket >= 0) { fputs(last_message, stdout); fflush(stdout); }
}

static void fatal_screen(void)
{
    /* Only called before SDL init or after its window/context have been freed. */
    PrintConsole *screen = consoleInit(NULL);
    char message[768];
    snprintf(message, sizeof(message), "GTA VCS NX failed\n%s\n"
           "Check /switch/gtavcs_nx/gtavcs-boot.log or gamefolder/gtavcs.log.\n"
           "Returning in 15 seconds.\n", last_message);
    /* nxlink may own stdout's file descriptor. Draw via the public console
     * renderer so fatal text is visible regardless of stdio redirection. */
    int x = 0, y = 0;
    if (screen && screen->consoleInitialised) {
        for (const unsigned char *p = (const unsigned char *)message; *p; ++p) {
            if (*p == '\n') { x = 0; ++y; continue; }
            if (x >= screen->consoleWidth) { x = 0; ++y; }
            if (y >= screen->consoleHeight) break;
            if (*p >= screen->font.asciiOffset &&
                *p - screen->font.asciiOffset < screen->font.numChars)
                screen->renderer->drawChar(screen, x, y, *p - screen->font.asciiOffset);
            ++x;
        }
    }
    for (int i = 0; i < 900 && appletMainLoop(); ++i) consoleUpdate(NULL);
    consoleExit(NULL);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    int result = EXIT_FAILURE, core_started = 0;
    SDL_Window *window = NULL;
    SDL_GLContext context = NULL;
    FILE *log = NULL;
    int sockets_started = 0;
    AppletHookCookie hook;
    int hook_started = 0;
    if (__nxlink_host.s_addr && R_SUCCEEDED(socketInitializeDefault())) {
        sockets_started = 1;
        nx_socket = nxlinkConnectToHost(true, false);
    }
    log = freopen("sdmc:/switch/gtavcs_nx/gtavcs-boot.log", "w", stderr);
    if (!log) {
        snprintf(last_message, sizeof(last_message), "Cannot open /switch/gtavcs_nx/gtavcs-boot.log");
        goto cleanup;
    }
    setvbuf(log, NULL, _IOLBF, 0);
    diagnostic("[boot] GTA VCS NX; HOS=%08x libnx=%s applet=%d\n",
        hosversionGet(), GTAVCS_LIBNX_PACKAGE, appletGetAppletType());
    if (appletGetAppletType() != AppletType_Application &&
        appletGetAppletType() != AppletType_SystemApplication) {
        diagnostic("[boot] Full application memory required; launch using title override\n");
        goto cleanup;
    }

    /* Standard libnx startup mounts sdmc. Do not mount a second copy or change
     * cwd later: workers and all relative asset/mod lookups share this root. */
    if (chdir(ASSET_ROOT) != 0) {
        diagnostic("[boot] chdir(%s): %s\n", ASSET_ROOT, strerror(errno));
        goto cleanup;
    }
    log = freopen("gtavcs.log", "w", stderr);
    if (!log) {
        snprintf(last_message, sizeof(last_message), "Cannot open gamefolder/gtavcs.log");
        goto cleanup;
    }
    setvbuf(log, NULL, _IOLBF, 0);
    diagnostic("[boot] checked chdir succeeded: %s\n", ASSET_ROOT);
    SDL_SetMainReady();
    /* SDL owns audren (opened by the core's platform layer) and the controllers. */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0)
        goto sdl_error;
    if (appletGetAppletType() == AppletType_Application) {
        const Result rc = appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleepNotify);
        if (R_FAILED(rc)) {
            diagnostic("[lifecycle] Cannot enable focus notifications: %08x\n", rc);
            goto cleanup;
        }
    }
    diagnostic("[boot] SDL %s; audio %s\n", SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver());

    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) < 0 ||
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) < 0 ||
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) < 0 ||
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) < 0 ||
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) < 0 ||
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) < 0) goto sdl_error;

    const int docked = appletGetOperationMode() == AppletOperationMode_Console;
    window = SDL_CreateWindow("GTA VCS NX", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, docked ? 1920 : 1280, docked ? 1080 : 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!window) goto sdl_error;
    context = SDL_GL_CreateContext(window);
    if (!context || SDL_GL_MakeCurrent(window, context) < 0) goto sdl_error;
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) || !GLAD_GL_VERSION_3_3) {
        diagnostic("[boot] OpenGL 3.3 entry points unavailable\n");
        goto cleanup;
    }
    diagnostic("[boot] GL %s; renderer %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    /* The core paces host frames (platform::waitNextFrame, 60 Hz). Until the GL
     * renderer presents (M05) a vsynced swap would halve that pacing. */
    if (SDL_GL_SetSwapInterval(0) < 0)
        diagnostic("[boot] swap interval 0 unavailable: %s\n", SDL_GetError());

    /* From here the core owns the SDL event queue, the controllers and audio. */
    const int boot = gtavcs_core_boot(ASSET_ROOT);
    if (boot != 0) {
        diagnostic("[boot] core boot failed (exit code %d): %s\n", boot, gtavcs_core_error());
        goto cleanup;
    }
    core_started = 1;

    int running = 1;
    u64 active_time = 0;
    const int suspension_clock = R_SUCCEEDED(appletGetProgramTotalActiveTime(&active_time));
    u64 suspended_epoch = suspension_clock ? armTicksToNs(armGetSystemTick()) - active_time : 0;
    diagnostic("[lifecycle] suspension_clock=%d\n", suspension_clock);
    int focused = 1, previous_width = 0, previous_height = 0;
    appletHook(&hook, lifecycle_hook, NULL);
    hook_started = 1;
    unsigned long frames_presented = 0;
    while (running && appletMainLoop()) {
        /* Focus messages can be coalesced while Horizon suspends the process.
         * Compare real ticks to libnx's clock excluding accumulated suspension;
         * an ordinary render stall advances both clocks. */
        if (suspension_clock) {
            if (R_FAILED(appletGetProgramTotalActiveTime(&active_time))) {
                diagnostic("[lifecycle] suspension clock failed\n");
                goto cleanup;
            }
            const u64 epoch = armTicksToNs(armGetSystemTick()) - active_time;
            if (epoch > suspended_epoch && epoch - suspended_epoch > UINT64_C(100000000)) {
                diagnostic("[lifecycle] confirmed suspension_ns=%llu\n",
                    (unsigned long long)(epoch - suspended_epoch));
                resumed = 1;
            }
            suspended_epoch = epoch;
        }
        const int active = appletGetFocusState() == AppletFocusState_InFocus;
        if (active != focused || resumed || focus_notifications) {
            diagnostic("[lifecycle] resume=%d focus_notifications=%u focus=%d\n", resumed, focus_notifications, active);
            resumed = 0;
            focus_notifications = 0;
            /* A suspension seen without a focus change still restarts pacing. */
            if (active && focused) gtavcs_core_set_focus(0);
            focused = active;
            gtavcs_core_set_focus(focused);
        }
        if (!focused) { SDL_Delay(20); continue; }
        int width, height;
        SDL_GL_GetDrawableSize(window, &width, &height);
        if (width != previous_width || height != previous_height) {
            diagnostic("[video] drawable=%dx%d mode=%d; one SDL GL context\n",
                width, height, appletGetOperationMode());
            previous_width = width; previous_height = height;
        }
        const int state = gtavcs_core_frame();
        if (state < 0) { diagnostic("[core] frame failed: %s\n", gtavcs_core_error()); goto cleanup; }
        if (state == 0) { diagnostic("[core] orderly stop after %lu frames\n", frames_presented); break; }
        /* Null RHI until M05: the runtime draws nothing, so present a cleared
         * frame to keep the display owned and the swap path exercised. */
        if (width > 0 && height > 0) {
            glViewport(0, 0, width, height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            SDL_GL_SwapWindow(window);
            ++frames_presented;
        }
    }
    if (running && !appletMainLoop())
        diagnostic("[lifecycle] applet asked the program to exit\n");
    result = EXIT_SUCCESS;
    goto cleanup;

sdl_error:
    diagnostic("[SDL] %s\n", SDL_GetError());
cleanup:;
    char failure[sizeof(last_message)];
    memcpy(failure, last_message, sizeof(failure));
    if (hook_started) appletUnhook(&hook);
    /* Joins every core worker and releases the disc before SDL goes. */
    if (core_started) gtavcs_core_shutdown();
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); /* Also unwind a partially successful SDL_Init. */
    if (result == EXIT_SUCCESS)
        diagnostic("[shutdown] core, context, window and SDL closed\n");
    else {
        memcpy(last_message, failure, sizeof(last_message));
        fatal_screen();
    }
    if (log) { fflush(log); fclose(log); }
    if (nx_socket >= 0) {
        /* nxlink dup2's the socket onto stdout. libc must close that FILE
         * while the socket device is still registered, before socketExit. */
        fflush(stdout);
        fclose(stdout);
        close(nx_socket);
    }
    if (sockets_started) socketExit();
    return result;
}
