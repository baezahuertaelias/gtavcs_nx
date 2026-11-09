// CPU validation boundary only. No window, user input, audio device or capture.
#include "platform/platform.h"
#include <chrono>
#include <thread>

namespace platform {
namespace { bool active = false; int width = 640, height = 448; }
void setStartPlacement(const WindowPlacement &) {}
void onPlacementChanged(PlacementChanged) {}
void setFullscreen(bool) {}
bool fullscreen() { return false; }
bool init(const WindowDesc &desc) { width=desc.width; height=desc.height; active=true; return true; }
void shutdown() { active=false; }
bool ready() { return active; }
void *nativeWindow() { return nullptr; }
bool windowHidden() { return true; }
const char *const *vulkanInstanceExtensions(uint32_t *count) { if(count) *count=0; return nullptr; }
bool vulkanCreateSurface(void *, uint64_t *) { return false; }
void pumpEvents(bool) {}
int64_t inputSnapshotNs() { return 0; }
bool quitRequested() { return false; }
void beginFrame() {}
void endUiFrame() {}
void waitNextFrame() {}
void setFrameLead(uint64_t,FrameLead) {}
bool frameLeadRan() { return false; }
bool frameSlept() { return false; }
void setFrameRate(double) {}
double displayRefreshHz() { return 60; }
void delayPrecise(uint64_t ns) { std::this_thread::sleep_for(std::chrono::nanoseconds(ns)); }
void windowSize(int &w,int &h) { w=width; h=height; }
bool windowFocused() { return false; }
bool keyDown(int) { return false; }
bool keyPressed(int) { return false; }
bool mouseDown(MouseButton) { return false; }
bool mousePosition(float &x,float &y) { x=y=0; return false; }
bool gamepadConnected() { return false; }
bool gamepadButton(GamepadButton) { return false; }
float gamepadAxis(GamepadAxis) { return 0; }
const char *gamepadName() { return ""; }
void gamepadRumble(float,float) {}
void setMouseCaptured(bool) {}
bool mouseCaptured() { return false; }
void takeMouseMotion(float &dx,float &dy) { dx=dy=0; }
const char *keyName(int) { return "?"; }
bool audioAvailable() { return false; }
bool audioOpen(uint32_t,uint32_t,AudioFill) { return false; }
void audioClose() {}
bool writePng(const char *,const uint8_t *,int,int) { return false; }
}
