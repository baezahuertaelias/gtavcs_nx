// See gl_loader_egl.h.
#include "platform/gl_loader_egl.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cstdio>
#include <cstring>

namespace gl_loader
{
    namespace
    {
        EGLDisplay g_display = EGL_NO_DISPLAY;
        EGLContext g_context = EGL_NO_CONTEXT;
    }

    bool create()
    {
        if (g_context != EGL_NO_CONTEXT)
            return true;
        EGLDisplay display = eglGetCurrentDisplay();
        EGLContext share = eglGetCurrentContext();
        if (display == EGL_NO_DISPLAY || share == EGL_NO_CONTEXT)
        {
            std::fprintf(stderr, "[gl-loader] no current EGL context to share\n");
            return false;
        }
        const char *extensions = eglQueryString(display, EGL_EXTENSIONS);
        if (!extensions || !std::strstr(extensions, "EGL_KHR_surfaceless_context"))
        {
            std::fprintf(stderr, "[gl-loader] EGL_KHR_surfaceless_context missing: no loader context\n");
            return false;
        }
        EGLint configId = 0, count = 0;
        eglQueryContext(display, share, EGL_CONFIG_ID, &configId);
        const EGLint wanted[] = {EGL_CONFIG_ID, configId, EGL_NONE};
        EGLConfig config = nullptr;
        if (!eglChooseConfig(display, wanted, &config, 1, &count) || count != 1)
        {
            std::fprintf(stderr, "[gl-loader] the window context's config (%d) not found\n", configId);
            return false;
        }
        const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                     EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
        eglBindAPI(EGL_OPENGL_API);
        g_context = eglCreateContext(display, config, share, attributes);
        if (g_context == EGL_NO_CONTEXT)
        {
            std::fprintf(stderr, "[gl-loader] eglCreateContext failed (0x%x)\n", eglGetError());
            return false;
        }
        g_display = display;
        std::fprintf(stderr, "[gl-loader] loader context made (config %d, shares the window context)\n", configId);
        return true;
    }

    bool available() { return g_context != EGL_NO_CONTEXT; }

    bool makeCurrent(bool current)
    {
        if (g_context == EGL_NO_CONTEXT)
            return false;
        eglBindAPI(EGL_OPENGL_API); // per-thread EGL state: a worker starts with OpenGL ES
        return eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, current ? g_context : EGL_NO_CONTEXT) == EGL_TRUE;
    }

    void destroy()
    {
        if (g_context != EGL_NO_CONTEXT)
            eglDestroyContext(g_display, g_context);
        g_context = EGL_NO_CONTEXT;
        g_display = EGL_NO_DISPLAY;
    }
}
