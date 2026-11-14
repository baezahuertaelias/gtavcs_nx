// Host runner of the RHI conformance scenes (rhi_conformance.h), for development and the desktop reference:
//   rhi_conformance --backend vulkan --out DIR   upstream's Vulkan backend, offscreen (lavapipe on a headless host)
//   rhi_conformance --backend opengl --out DIR   the port's OpenGL backend on a surfaceless EGL context (Mesa)
// DIR gets one .bin per readback, index.tsv (name kind width height tolerance file), checks.tsv and log.txt.
// tools/rhi_compare.py compares two such folders. The Switch runner is switch_main.cpp.
#include "rhi_tests/rhi_conformance.h"

#include "rhi/rhi.h"

#if defined(VCS_HAVE_OPENGL)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/resource.h>

namespace
{
    uint64_t rssBytes()
    {
        std::ifstream f("/proc/self/statm");
        uint64_t size = 0, resident = 0;
        f >> size >> resident;
        return resident * 4096u;
    }

#if defined(VCS_HAVE_OPENGL)
    EGLDisplay g_display = EGL_NO_DISPLAY;
    EGLContext g_loader = EGL_NO_CONTEXT; // shares the main context's objects (PS2X_GL_WORKER_CONTEXT)

    bool loaderAvailable() { return g_loader != EGL_NO_CONTEXT; }

    bool makeLoaderCurrent(bool current)
    {
        return eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, current ? g_loader : EGL_NO_CONTEXT) == EGL_TRUE;
    }

    bool makeEglContext(std::string &error)
    {
        auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        EGLDisplay display = getPlatformDisplay ? getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                                                : EGL_NO_DISPLAY;
        EGLint major = 0, minor = 0;
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
        {
            error = "no surfaceless EGL display";
            return false;
        }
        const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                     EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
        EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
        if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
        {
            error = "no GL 3.3 core context";
            return false;
        }
        g_display = display;
        g_loader = eglCreateContext(display, EGL_NO_CONFIG_KHR, context, attributes);
        return true;
    }

    void *glProc(const char *name) { return reinterpret_cast<void *>(eglGetProcAddress(name)); }
#endif
}

int main(int argc, char **argv)
{
    std::string backendName = "vulkan", out = "rhi-out";
    uint32_t churn = 200;
    for (int i = 1; i + 1 < argc; i += 2)
    {
        if (!std::strcmp(argv[i], "--backend"))
            backendName = argv[i + 1];
        else if (!std::strcmp(argv[i], "--out"))
            out = argv[i + 1];
        else if (!std::strcmp(argv[i], "--churn"))
            churn = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
    rhi::DeviceDesc desc;
    if (!rhi::backendFromName(backendName.c_str(), desc.backend) || !rhi::backendBuilt(desc.backend))
    {
        std::fprintf(stderr, "backend %s not built\n", backendName.c_str());
        return 2;
    }
    desc.offscreen = true;
    desc.width = 640;
    desc.height = 448;
    desc.debug = true;
    std::string error;
#if defined(VCS_HAVE_OPENGL)
    if (desc.backend == rhi::Backend::OpenGL)
    {
        if (!makeEglContext(error))
        {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 2;
        }
        desc.gl.getProcAddress = glProc;
        desc.gl.loaderAvailable = loaderAvailable;
        desc.gl.makeLoaderCurrent = makeLoaderCurrent;
    }
#endif
    auto device = rhi::createDevice(desc, error);
    if (!device)
    {
        std::fprintf(stderr, "device: %s\n", error.c_str());
        return 2;
    }
    std::string mk = "mkdir -p '" + out + "'";
    if (std::system(mk.c_str()) != 0)
        return 2;
    std::ofstream index(out + "/index.tsv"), checks(out + "/checks.tsv"), log(out + "/log.txt");
    log << "backend " << rhi::backendName(device->backend()) << " on " << device->adapterName() << "\n";
    int images = 0;
    conformance::Sink sink;
    sink.image = [&](const conformance::Image &img) {
        std::string file = img.name;
        for (char &c : file)
            if (c == '/')
                c = '.';
        file += ".bin";
        std::ofstream(out + "/" + file, std::ios::binary).write(reinterpret_cast<const char *>(img.data.data()),
                                                                 static_cast<std::streamsize>(img.data.size()));
        index << img.name << '\t' << img.kind << '\t' << img.width << '\t' << img.height << '\t' << img.tolerance << '\t' << file << '\n';
        ++images;
    };
    sink.check = [&](const std::string &name, bool passed, const std::string &detail) {
        checks << name << '\t' << (passed ? "PASS" : "FAIL") << '\t' << detail << '\n';
        if (!passed)
            std::fprintf(stderr, "FAIL %s %s\n", name.c_str(), detail.c_str());
    };
    sink.log = [&](const std::string &text) { log << text << '\n'; };
    sink.processMemory = rssBytes;
    conformance::Options options;
    options.churnRounds = churn;
    const int failures = conformance::run(*device, sink, options);
    log << "images " << images << " failures " << failures << "\n";
    std::printf("%s: %d images, %d failed checks (%s)\n", rhi::backendName(device->backend()), images, failures, out.c_str());
    device.reset();
    return failures ? 1 : 0;
}
