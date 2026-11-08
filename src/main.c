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
#define PCM_FRAMES 1024
#define PCM_BYTES (PCM_FRAMES * 2 * sizeof(int16_t))

static int nx_socket = -1;
static unsigned audio_high_water, audio_empty;
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
    snprintf(message, sizeof(message), "GTA VCS NX platform smoke failed\n%s\n"
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

static SDL_GameController *open_controller(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *pad = SDL_GameControllerOpen(i);
            if (pad) {
                diagnostic("[input] open id=%d name=%s mapping=%s\n",
                    SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)),
                    SDL_GameControllerName(pad), SDL_GameControllerGetAttached(pad) ? "attached" : "detached");
                return pad;
            }
        }
    }
    return NULL;
}

static GtavcsPad sample_controller(SDL_GameController *controller)
{
    GtavcsPad pad = {0};
    if (!controller || !SDL_GameControllerGetAttached(controller)) return pad;
    pad.connected = 1;
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX && i < 32; ++i)
        if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)i))
            pad.buttons |= UINT32_C(1) << i;
    for (int i = 0; i < 6; ++i)
        pad.axes[i] = SDL_GameControllerGetAxis(controller, (SDL_GameControllerAxis)i);
    return pad;
}

/* A bounded queue makes the scaffold safe if rendering stalls. The integrated
 * runtime will use its own producer ring on an audio worker; this smoke test
 * owns all SDL_AudioStream access on the main thread. No double audren init. */
static int pump_audio(SDL_AudioDeviceID device, SDL_AudioStream *stream)
{
    int16_t pcm[PCM_FRAMES * 2];
    unsigned char converted[PCM_BYTES];
    const Uint32 queued = SDL_GetQueuedAudioSize(device);
    if (!queued) ++audio_empty; /* Empty observations, not a measured underrun count. */
    if (queued > audio_high_water) audio_high_water = queued;
    if (queued >= 3 * PCM_BYTES) return 0;
    for (int block = 0; block < 3; ++block) {
        if (SDL_GetQueuedAudioSize(device) > 2 * PCM_BYTES) break;
        const size_t frames = gtavcs_core_audio(pcm, PCM_FRAMES);
        if (frames > PCM_FRAMES) return SDL_SetError("Core returned too many audio frames");
        if (!frames) break;
        if (SDL_AudioStreamPut(stream, pcm, (int)(frames * 2 * sizeof(int16_t))) < 0) return -1;
        int available;
        while ((available = SDL_AudioStreamAvailable(stream)) > 0) {
            const int bytes = SDL_AudioStreamGet(stream, converted, sizeof(converted));
            if (bytes < 0) return -1;
            if (!bytes) break;
            if (SDL_QueueAudio(device, converted, (Uint32)bytes) < 0) return -1;
            const Uint32 depth = SDL_GetQueuedAudioSize(device);
            if (depth > audio_high_water) audio_high_water = depth;
        }
        if (available < 0) return -1;
        if (SDL_GetQueuedAudioSize(device) >= 3 * PCM_BYTES) break;
    }
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    int result = EXIT_FAILURE, core_started = 0;
    SDL_Window *window = NULL;
    SDL_GLContext context = NULL;
    SDL_GameController *controller = NULL;
    SDL_AudioDeviceID audio = 0;
    SDL_AudioStream *stream = NULL;
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
    diagnostic("[boot] platform smoke only; HOS=%08x libnx=%s applet=%d\n",
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
    window = SDL_CreateWindow("GTA VCS NX - platform smoke", SDL_WINDOWPOS_UNDEFINED,
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
    if (SDL_GL_SetSwapInterval(1) < 0)
        diagnostic("[boot] vsync unavailable: %s\n", SDL_GetError());
    controller = open_controller();

    SDL_AudioSpec wanted = {0}, obtained = {0};
    wanted.freq = 48000;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = PCM_FRAMES;
    audio = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
    if (!audio) goto sdl_error;
    stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, 48000,
        obtained.format, obtained.channels, obtained.freq);
    if (!stream) goto sdl_error;
    diagnostic("[audio] device=%u rate=%d format=%04x channels=%u samples=%u queue_limit=%zu\n",
        audio, obtained.freq, obtained.format, obtained.channels, obtained.samples, 3 * PCM_BYTES);
    if (gtavcs_core_boot(ASSET_ROOT) != 0) {
        diagnostic("[boot] core boot failed\n");
        goto cleanup;
    }
    core_started = 1;
    if (pump_audio(audio, stream) < 0) goto sdl_error;
    SDL_PauseAudioDevice(audio, 0);

    int running = 1;
    Uint64 previous = SDL_GetPerformanceCounter();
    const double frequency = (double)SDL_GetPerformanceFrequency();
    int focused = 1, previous_width = 0, previous_height = 0;
    appletHook(&hook, lifecycle_hook, NULL);
    hook_started = 1;
    Uint64 last_stats = previous;
    unsigned long frames_presented = 0;
    GtavcsPad previous_pad = {0};
    while (running && appletMainLoop()) {
        const int active = appletGetFocusState() == AppletFocusState_InFocus;
        if (active != focused || resumed || focus_notifications) {
            diagnostic("[lifecycle] resume=%d focus_notifications=%u\n", resumed, focus_notifications);
            resumed = 0;
            focus_notifications = 0;
            focused = active;
            SDL_PauseAudioDevice(audio, 1);
            SDL_ClearQueuedAudio(audio);
            SDL_AudioStreamClear(stream);
            previous = SDL_GetPerformanceCounter();
            diagnostic("[lifecycle] focus=%d; audio paused and cleared\n", focused);
            if (focused) {
                if (pump_audio(audio, stream) < 0) goto sdl_error;
                SDL_PauseAudioDevice(audio, 0);
            }
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
                SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == event.cdevice.which) {
                SDL_GameControllerClose(controller);
                diagnostic("[input] removed id=%d; neutral until resampled\n", event.cdevice.which);
                controller = open_controller();
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED && !controller)
                controller = open_controller();
        }
        if (!running) break;
        if (!focused) { SDL_Delay(20); continue; }
        const GtavcsPad pad = sample_controller(controller);
        if (memcmp(&pad, &previous_pad, sizeof(pad)) != 0) {
            diagnostic("[input] connected=%d buttons=%08x axes=%d,%d,%d,%d,%d,%d\n",
                pad.connected, pad.buttons, pad.axes[0], pad.axes[1], pad.axes[2],
                pad.axes[3], pad.axes[4], pad.axes[5]);
            previous_pad = pad;
        }
        /* Smoke-test exit only; Plus becomes PS2 Start when the core is wired. */
        if (pad.buttons & (UINT32_C(1) << SDL_CONTROLLER_BUTTON_START)) break;
        int width, height;
        SDL_GL_GetDrawableSize(window, &width, &height);
        if (width != previous_width || height != previous_height) {
            diagnostic("[video] drawable=%dx%d mode=%d; one SDL GL context\n",
                width, height, appletGetOperationMode());
            previous_width = width; previous_height = height;
        }
        if (width <= 0 || height <= 0) { SDL_Delay(10); continue; }
        const Uint64 now = SDL_GetPerformanceCounter();
        const double dt = (double)(now - previous) / frequency;
        previous = now;
        if ((double)(now - last_stats) / frequency >= 5.0) {
            diagnostic("[audio] queued=%u high_water=%u empty_observations=%u frames=%lu\n",
                SDL_GetQueuedAudioSize(audio), audio_high_water, audio_empty, frames_presented);
            last_stats = now;
        }
        glViewport(0, 0, width, height);
        glClearColor(0.12f, 0.32f, 0.70f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        if (frames_presented == 0) {
            unsigned char pixel[4] = {0};
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            diagnostic("[video] clear center RGBA=%u,%u,%u,%u error=%04x\n",
                pixel[0], pixel[1], pixel[2], pixel[3], glGetError());
        }
        const int state = gtavcs_core_frame(&pad, width, height, dt);
        if (state < 0) { diagnostic("[core] frame failed\n"); goto cleanup; }
        if (state == 0) break;
        if (pump_audio(audio, stream) < 0) goto sdl_error;
        SDL_GL_SwapWindow(window);
        ++frames_presented;
        SDL_Delay(1); /* Avoid a busy loop if swap interval is unsupported. */
    }
    result = EXIT_SUCCESS;
    goto cleanup;

sdl_error:
    diagnostic("[SDL] %s\n", SDL_GetError());
cleanup:;
    char failure[sizeof(last_message)];
    memcpy(failure, last_message, sizeof(failure));
    if (hook_started) appletUnhook(&hook);
    if (audio) SDL_PauseAudioDevice(audio, 1);
    if (core_started) gtavcs_core_shutdown();
    if (audio) SDL_CloseAudioDevice(audio);
    if (stream) SDL_FreeAudioStream(stream);
    if (controller) SDL_GameControllerClose(controller);
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); /* Also unwind a partially successful SDL_Init. */
    if (result == EXIT_SUCCESS)
        diagnostic("[shutdown] audio/stream/controller/context/window/SDL closed; high_water=%u\n", audio_high_water);
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
