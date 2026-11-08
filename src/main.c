#include <glad/gl.h>
#include <SDL.h>
#include <switch.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core.h"

#define ASSET_ROOT "sdmc:/switch/gtavcs_nx/gamefolder"
#define PCM_FRAMES 1024
#define PCM_BYTES (PCM_FRAMES * 2 * sizeof(int16_t))

static SDL_GameController *open_controller(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *pad = SDL_GameControllerOpen(i);
            if (pad) return pad;
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
    if (SDL_GetQueuedAudioSize(device) >= 3 * PCM_BYTES) return 0;
    for (int block = 0; block < 3; ++block) {
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

    /* Standard libnx startup mounts sdmc. Do not mount a second copy or change
     * cwd later: workers and all relative asset/mod lookups share this root. */
    if (chdir(ASSET_ROOT) != 0) {
        fprintf(stderr, "[boot] chdir(%s): %s\n", ASSET_ROOT, strerror(errno));
        return EXIT_FAILURE;
    }
    log = freopen("gtavcs.log", "w", stderr);
    if (!log) return EXIT_FAILURE;
    setvbuf(log, NULL, _IOLBF, 0);
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0)
        goto sdl_error;
    fprintf(stderr, "[boot] SDL %s; audio %s\n", SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver());

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
        fprintf(stderr, "[boot] OpenGL 3.3 entry points unavailable\n");
        goto cleanup;
    }
    fprintf(stderr, "[boot] GL %s; renderer %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    if (SDL_GL_SetSwapInterval(1) < 0)
        fprintf(stderr, "[boot] vsync unavailable: %s\n", SDL_GetError());
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
    if (gtavcs_core_boot(ASSET_ROOT) != 0) {
        fprintf(stderr, "[boot] core boot failed\n");
        goto cleanup;
    }
    core_started = 1;
    if (pump_audio(audio, stream) < 0) goto sdl_error;
    SDL_PauseAudioDevice(audio, 0);

    int running = 1;
    Uint64 previous = SDL_GetPerformanceCounter();
    const double frequency = (double)SDL_GetPerformanceFrequency();
    while (running && appletMainLoop()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
                SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == event.cdevice.which) {
                SDL_GameControllerClose(controller);
                controller = open_controller();
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED && !controller)
                controller = open_controller();
        }
        if (!running) break;
        const GtavcsPad pad = sample_controller(controller);
        /* Smoke-test exit only; Plus becomes PS2 Start when the core is wired. */
        if (pad.buttons & (UINT32_C(1) << SDL_CONTROLLER_BUTTON_START)) break;
        int width, height;
        SDL_GL_GetDrawableSize(window, &width, &height);
        if (width <= 0 || height <= 0) { SDL_Delay(10); continue; }
        const Uint64 now = SDL_GetPerformanceCounter();
        const double dt = (double)(now - previous) / frequency;
        previous = now;
        glViewport(0, 0, width, height);
        glClearColor(0.06f, 0.10f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        const int state = gtavcs_core_frame(&pad, width, height, dt);
        if (state < 0) { fprintf(stderr, "[core] frame failed\n"); goto cleanup; }
        if (state == 0) break;
        if (pump_audio(audio, stream) < 0) goto sdl_error;
        SDL_GL_SwapWindow(window);
        SDL_Delay(1); /* Avoid a busy loop if swap interval is unsupported. */
    }
    result = EXIT_SUCCESS;
    goto cleanup;

sdl_error:
    fprintf(stderr, "[SDL] %s\n", SDL_GetError());
cleanup:
    if (audio) SDL_PauseAudioDevice(audio, 1);
    if (core_started) gtavcs_core_shutdown();
    if (audio) SDL_CloseAudioDevice(audio);
    if (stream) SDL_FreeAudioStream(stream);
    if (controller) SDL_GameControllerClose(controller);
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); /* Also unwind a partially successful SDL_Init. */
    if (log) fflush(log);
    return result;
}
