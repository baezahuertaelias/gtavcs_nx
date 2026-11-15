// A texture-loading GL context for the OpenGL RHI's opt-in free-threaded creation (PS2X_GL_WORKER_CONTEXT, M05-09):
// made on the main thread by SDL, sharing the window context's objects (checked before it is reported available),
// and made current on a worker thread without a surface (EGL_KHR_surfaceless_context). It never draws to the window.
#pragma once

namespace gl_loader
{
    bool create();            // main thread, SDL's GL context current; false (logged) when none shares objects
    bool available();
    bool makeCurrent(bool current); // on the calling thread; false releases it
    void destroy();           // main thread, after every user is gone
}
