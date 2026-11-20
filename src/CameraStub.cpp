// Linux / non-Mac: no camera. Tests inject landmark frames directly into the processor.
#include "Camera.h"
#if !defined(__APPLE__)
namespace tr {
namespace {
class StubCam : public CameraTracker {
public:
    void start() override { st = CamStatus::NoCamera; }
    void stop() override { st = CamStatus::Off; }
    CamStatus status() const override { return st; }
    double fps() const override { return 0; }
    CamStatus st = CamStatus::Off;
};
}
std::unique_ptr<CameraTracker> CameraTracker::create(FrameFn, PreviewFn) { return std::make_unique<StubCam>(); }
}
#endif
