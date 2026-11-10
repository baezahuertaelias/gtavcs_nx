// CPU validation: reuse the upstream null RHI, with unavailable capture/UI.
#include "render/presenter.h"
#include "render/gs_shaders.h"
#include "rhi/rhi.h"
#include <algorithm>
#include <atomic>

namespace present {
namespace { std::unique_ptr<rhi::Device> output; std::atomic<float> aspect{4.0f/3.0f}; }
void setBackend(int) {}
int backend() { return static_cast<int>(rhi::Backend::Null); }
bool backendNeedsVulkanWindow() { return false; }
bool init(int,int) { rhi::DeviceDesc desc{}; desc.backend=rhi::Backend::Null; std::string error; output=rhi::createDevice(desc,error); return bool(output); }
void shutdown() { output.reset(); }
rhi::Device *device() { return output.get(); }
Picture createPicture(int w,int h,bool linear) { Picture p{}; p.width=w; p.height=h; p.linear=linear; return p; }
void updatePicture(const Picture &,const void *) {}
void beginFrame() {}
void drawWindow(const Picture &,int,int,float,int,int,float) {}
void present() {}
void setFrameInputTime(int64_t) {}
void setPresentMode(int) {}
bool capture(const Picture &,int,int,float,int,int,std::vector<uint8_t> &rgb,float) { rgb.clear(); return false; }
bool frameLuma(const Picture &,int,int,float &) { return false; }
void setDisplayAspect(float value) { if(value>0) aspect.store(value); }
float displayAspect(bool showing3d) { return showing3d ? aspect.load() : 4.0f/3.0f; }
void fitRect(float a,float frame,float width,float height,float &x,float &y,float &w,float &h) {
    if(a<=0 || width<=0 || height<=0) { x=y=w=h=0; return; }
    float fw=width,fh=height;
    if(frame>0) { fw=std::min(width,height*frame); fh=std::min(height,width/frame); }
    w=std::min(fw,fh*a); h=std::min(fh,fw/a); x=(width-w)/2; y=(height-h)/2;
}
}
namespace render {
const GsShaders &gsShaders(rhi::Backend) { static const GsShaders empty{}; return empty; }
}
