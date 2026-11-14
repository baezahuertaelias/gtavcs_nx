// A texture-loading GL context for the OpenGL RHI's opt-in free-threaded creation (PS2X_GL_WORKER_CONTEXT, M05-09):
// made on the main thread from SDL's current EGL context, sharing its objects, and made current on a worker thread
// without a surface (EGL_KHR_surfaceless_context). It never touches the window or its surface.
#pragma once

namespace gl_loader
{
    bool create();            // main thread, SDL's GL context current; false (logged) when EGL cannot make one
    bool available();
    bool makeCurrent(bool current); // on the calling thread; false releases it
    void destroy();           // main thread, after every user is gone
}
