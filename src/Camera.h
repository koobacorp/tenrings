#pragma once
#include <functional>
#include <memory>
#include "engine/Engine.h"
namespace juce { class Image; }

namespace tr {
enum class CamStatus { Off = 0, Starting, Running, NoPermission, NoCamera, HostBlocked };

// Mac: AVFoundation (60 fps when available, 720p preferred) + Apple Vision hand pose (2 hands, 21 points).
// The frame callback is invoked on a dedicated high-priority (user-interactive QoS) serial queue.
class CameraTracker {
public:
    using FrameFn = std::function<void(const Frame&)>;
    using PreviewFn = std::function<void(const uint8_t* bgra, int w, int h, int stride)>; // already mirrored, downscaled
    static std::unique_ptr<CameraTracker> create(FrameFn onFrame, PreviewFn onPreview);
    virtual ~CameraTracker() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual CamStatus status() const = 0;
    virtual double fps() const = 0;
};
}
