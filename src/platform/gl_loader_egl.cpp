// See gl_loader_egl.h.
//
// The context is made by SDL with SDL_GL_SHARE_WITH_CURRENT_CONTEXT (SDL2's EGL backend passes the window context as
// eglCreateContext's share context and keeps its own display and config), then made current on worker threads
// without a surface. The first M05 Switch run made it with eglCreateContext directly: the window context did not
// see its objects (glBindTexture "non-gen name", glWaitSync "not a valid sync object") and sampling them crashed the
// console. So create() now proves the sharing before reporting a loader: a worker makes a texture and a fence on the
// loader, and the window context must know both; otherwise the loader is destroyed and the OpenGL RHI keeps every
// texture on the owner thread.
#include "platform/gl_loader_egl.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <SDL.h>
#include <glad/gl.h>

#include <cstdio>
#include <cstring>
#include <thread>

namespace gl_loader
{
    namespace
    {
        EGLDisplay g_display = EGL_NO_DISPLAY;
        SDL_GLContext g_context = nullptr;

        bool current(bool on)
        {
            eglBindAPI(EGL_OPENGL_API); // per-thread EGL state: a worker starts with OpenGL ES
            return eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                                  on ? static_cast<EGLContext>(g_context) : EGL_NO_CONTEXT) == EGL_TRUE;
        }

        // a texture and a fence made on the loader by a worker must exist in the window context
        bool shares()
        {
            GLuint texture = 0;
            GLsync fence = nullptr;
            bool made = false;
            std::thread worker([&] {
                if (!current(true))
                    return;
                const uint32_t pixel = 0xFF00FF00u;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
                glBindTexture(GL_TEXTURE_2D, 0);
                fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                glFinish();
                made = glGetError() == GL_NO_ERROR;
                current(false);
            });
            worker.join();
            const bool texShared = made && glIsTexture(texture) == GL_TRUE;
            const bool syncShared = made && glIsSync(fence) == GL_TRUE;
            while (glGetError() != GL_NO_ERROR)
            {
            }
            std::fprintf(stderr, "[gl-loader] sharing check: made %d, texture seen by the window context %d, fence %d\n", made,
                         texShared, syncShared);
            if (texShared && syncShared)
            {
                glDeleteSync(fence);
                glDeleteTextures(1, &texture);
                return true;
            }
            return false; // the loader's own objects go with it (destroy)
        }
    }

    bool create()
    {
        if (g_context)
            return true;
        SDL_Window *window = SDL_GL_GetCurrentWindow();
        SDL_GLContext windowContext = SDL_GL_GetCurrentContext();
        g_display = eglGetCurrentDisplay();
        if (!window || !windowContext || g_display == EGL_NO_DISPLAY)
        {
            std::fprintf(stderr, "[gl-loader] no current SDL GL window and context to share\n");
            return false;
        }
        const char *extensions = eglQueryString(g_display, EGL_EXTENSIONS);
        if (!extensions || !std::strstr(extensions, "EGL_KHR_surfaceless_context"))
        {
            std::fprintf(stderr, "[gl-loader] EGL_KHR_surfaceless_context missing: no loader context\n");
            return false;
        }
        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)))
        {
            std::fprintf(stderr, "[gl-loader] GL entry points unavailable\n");
            return false;
        }
        // SDL makes the new context current on this thread: the window context is made current again at once
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        g_context = SDL_GL_CreateContext(window);
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
        if (SDL_GL_MakeCurrent(window, windowContext) != 0)
            std::fprintf(stderr, "[gl-loader] could not make the window context current again: %s\n", SDL_GetError());
        if (!g_context)
        {
            std::fprintf(stderr, "[gl-loader] SDL_GL_CreateContext (shared): %s\n", SDL_GetError());
            return false;
        }
        if (!shares())
        {
            std::fprintf(stderr, "[gl-loader] the loader context does not share the window context's objects: none\n");
            destroy();
            return false;
        }
        std::fprintf(stderr, "[gl-loader] loader context made by SDL, sharing the window context's objects (checked)\n");
        return true;
    }

    bool available() { return g_context != nullptr; }

    bool makeCurrent(bool on) { return g_context && current(on); }

    void destroy()
    {
        if (g_context)
            SDL_GL_DeleteContext(g_context);
        g_context = nullptr;
        g_display = EGL_NO_DISPLAY;
    }
}
