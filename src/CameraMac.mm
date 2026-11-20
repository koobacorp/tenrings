// macOS camera + Apple Vision hand tracking for TENRINGS. Compiled with ARC.
#include "Camera.h"
#import <AVFoundation/AVFoundation.h>
#import <Vision/Vision.h>
#import <CoreVideo/CoreVideo.h>
#include <atomic>
#include <vector>
#include <chrono>

using namespace tr;

@interface TRCamDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property (nonatomic, assign) void* owner;
@end

namespace {
class MacCam;
}

namespace {
class MacCam : public CameraTracker {
public:
    MacCam(FrameFn f, PreviewFn p) : onFrame(std::move(f)), onPreview(std::move(p)) {
        dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0);
        queue = dispatch_queue_create("com.koobacorp.tenrings.tracking", attr);
        request = [[VNDetectHumanHandPoseRequest alloc] init];
        request.maximumHandCount = 2;
        delegate = [[TRCamDelegate alloc] init];
        delegate.owner = this;
    }
    ~MacCam() override { stop(); dispatch_sync(queue, ^{ delegate.owner = nullptr; }); }

    void start() override {
        if (session != nil && session.running) return;
        // macOS kills a host that touches the camera without NSCameraUsageDescription: check first.
        if ([[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSCameraUsageDescription"] == nil) { st = CamStatus::HostBlocked; return; }
        AVAuthorizationStatus a = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
        if (a == AVAuthorizationStatusDenied || a == AVAuthorizationStatusRestricted) { st = CamStatus::NoPermission; return; }
        if (a == AVAuthorizationStatusNotDetermined) {
            st = CamStatus::Starting;
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
                dispatch_async(dispatch_get_main_queue(), ^{ if (granted) this->startSession(); else this->st = CamStatus::NoPermission; });
            }];
            return;
        }
        startSession();
    }
    void stop() override {
        if (session != nil) { [session stopRunning]; session = nil; }
        st = CamStatus::Off;
    }
    CamStatus status() const override { return st.load(); }
    double fps() const override { return measuredFps.load(); }

    void startSession() {
        st = CamStatus::Starting;
        AVCaptureDevice* dev = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        if (dev == nil) { st = CamStatus::NoCamera; return; }
        NSError* err = nil;
        AVCaptureDeviceInput* in = [AVCaptureDeviceInput deviceInputWithDevice:dev error:&err];
        if (in == nil) { st = CamStatus::NoCamera; return; }
        session = [[AVCaptureSession alloc] init];
        [session beginConfiguration];
        if ([session canAddInput:in]) [session addInput:in];
        AVCaptureVideoDataOutput* out = [[AVCaptureVideoDataOutput alloc] init];
        out.alwaysDiscardsLateVideoFrames = YES; // never queue stale frames: latency over completeness
        out.videoSettings = @{ (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };
        [out setSampleBufferDelegate:delegate queue:queue];
        if ([session canAddOutput:out]) [session addOutput:out];
        [session commitConfiguration];
        // Speed first: choose the highest frame rate, preferring <=1280 wide (720p@60 beats 1080p@30).
        AVCaptureDeviceFormat* best = nil; double bestFps = 0; int bestW = 0;
        for (AVCaptureDeviceFormat* f in dev.formats) {
            CMVideoDimensions d = CMVideoFormatDescriptionGetDimensions(f.formatDescription);
            if (d.width > 1280 || d.width < 480) continue;
            for (AVFrameRateRange* r in f.videoSupportedFrameRateRanges) {
                if (r.maxFrameRate > bestFps + 0.5 || (std::abs(r.maxFrameRate - bestFps) < 0.5 && d.width > bestW)) { best = f; bestFps = r.maxFrameRate; bestW = d.width; }
            }
        }
        if (best != nil && [dev lockForConfiguration:&err]) {
            dev.activeFormat = best;
            double use = std::min(bestFps, 60.0);
            dev.activeVideoMinFrameDuration = CMTimeMake(1000, (int32_t)(use * 1000));
            dev.activeVideoMaxFrameDuration = CMTimeMake(1000, (int32_t)(use * 1000));
            [dev unlockForConfiguration];
        }
        [session startRunning];
        st = session.running ? CamStatus::Running : CamStatus::NoCamera;
    }

    void handle(CMSampleBufferRef sb) {
        CVPixelBufferRef px = CMSampleBufferGetImageBuffer(sb);
        if (px == nullptr) return;
        auto now = std::chrono::steady_clock::now();
        double t = std::chrono::duration<double>(now.time_since_epoch()).count();
        if (lastT > 0) { double inst = 1.0 / std::max(1e-3, t - lastT); measuredFps = measuredFps * 0.9 + inst * 0.1; }
        lastT = t;
        // 1) tracking first, so notes never wait for the picture
        Frame fr; fr.t = t;
        VNImageRequestHandler* h = [[VNImageRequestHandler alloc] initWithCVPixelBuffer:px options:@{}];
        NSError* err = nil;
        if ([h performRequests:@[request] error:&err]) {
            std::vector<Hand> hands;
            for (VNHumanHandPoseObservation* obs in request.results) {
                Hand hd; int okCount = 0;
                auto get = [&](VNHumanHandPoseObservationJointName j, Pt& p) {
                    NSError* e = nil; VNRecognizedPoint* rp = [obs recognizedPointForJointName:j error:&e];
                    if (rp == nil || rp.confidence < 0.3f) return false;
                    p.x = 1.f - (float)rp.location.x; p.y = 1.f - (float)rp.location.y; // mirror + top-left origin
                    return true;
                };
                okCount += get(VNHumanHandPoseObservationJointNameWrist, hd.palm);
                VNHumanHandPoseObservationJointName tips[5] = { VNHumanHandPoseObservationJointNameThumbTip, VNHumanHandPoseObservationJointNameIndexTip, VNHumanHandPoseObservationJointNameMiddleTip, VNHumanHandPoseObservationJointNameRingTip, VNHumanHandPoseObservationJointNameLittleTip };
                VNHumanHandPoseObservationJointName mcps[5] = { VNHumanHandPoseObservationJointNameThumbMP, VNHumanHandPoseObservationJointNameIndexMCP, VNHumanHandPoseObservationJointNameMiddleMCP, VNHumanHandPoseObservationJointNameRingMCP, VNHumanHandPoseObservationJointNameLittleMCP };
                VNHumanHandPoseObservationJointName pips[5] = { VNHumanHandPoseObservationJointNameThumbIP, VNHumanHandPoseObservationJointNameIndexPIP, VNHumanHandPoseObservationJointNameMiddlePIP, VNHumanHandPoseObservationJointNameRingPIP, VNHumanHandPoseObservationJointNameLittlePIP };
                for (int i = 0; i < 5; ++i) {
                    bool tipOK = get(tips[i], hd.tip[i]), mcpOK = get(mcps[i], hd.mcp[i]);
                    hd.tipOk[i] = tipOK;   // Strings mode plucks with any seen fingertip
                    if (!tipOK) hd.tip[i] = hd.palm;
                    if (!mcpOK) hd.mcp[i] = hd.palm;
                    if (i == 2) hd.pianoBasis = mcpOK;
                    hd.hasPip[i] = get(pips[i], hd.pip[i]);
                    hd.valid[i] = tipOK && mcpOK && hd.hasPip[i];
                }
                // palm center = wrist..middle MCP midpoint (more stable than wrist alone)
                if (okCount) { hd.palm.x = 0.5f * (hd.palm.x + hd.mcp[2].x); hd.palm.y = 0.5f * (hd.palm.y + hd.mcp[2].y); hd.present = true; hands.push_back(hd); }
            }
            // Assign by mirrored screen side: the hand further right is the right hand.
            if (hands.size() >= 2) { if (hands[0].palm.x > hands[1].palm.x) std::swap(hands[0], hands[1]); fr.hand[0] = hands[0]; fr.hand[1] = hands[1]; }
            else if (hands.size() == 1) fr.hand[hands[0].palm.x >= 0.5f ? 1 : 0] = hands[0];
        }
        if (onFrame) onFrame(fr);
        // 2) preview: every 2nd frame, downscaled 4x, mirrored
        if (onPreview && (++frameCount % 2) == 0) {
            CVPixelBufferLockBaseAddress(px, kCVPixelBufferLock_ReadOnly);
            const uint8_t* base = (const uint8_t*)CVPixelBufferGetBaseAddress(px);
            int w = (int)CVPixelBufferGetWidth(px), hgt = (int)CVPixelBufferGetHeight(px); size_t stride = CVPixelBufferGetBytesPerRow(px);
            int ow = w / 4, oh = hgt / 4; small.resize((size_t)ow * oh * 4);
            for (int y = 0; y < oh; ++y) { const uint32_t* src = (const uint32_t*)(base + (size_t)(y * 4) * stride); uint32_t* dst = (uint32_t*)small.data() + (size_t)y * ow;
                for (int x = 0; x < ow; ++x) dst[x] = src[(ow - 1 - x) * 4]; }
            CVPixelBufferUnlockBaseAddress(px, kCVPixelBufferLock_ReadOnly);
            onPreview(small.data(), ow, oh, ow * 4);
        }
    }

    FrameFn onFrame; PreviewFn onPreview;
    dispatch_queue_t queue;
    AVCaptureSession* session = nil;
    VNDetectHumanHandPoseRequest* request;
    TRCamDelegate* delegate;
    std::atomic<CamStatus> st { CamStatus::Off };
    std::atomic<double> measuredFps { 0 };
    double lastT = 0; uint64_t frameCount = 0; std::vector<uint8_t> small;
};
}

@implementation TRCamDelegate
- (void)captureOutput:(AVCaptureOutput*)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection*)connection {
    (void)output; (void)connection;
    if (self.owner != nullptr) static_cast<MacCam*>(self.owner)->handle(sampleBuffer);
}
@end

std::unique_ptr<CameraTracker> CameraTracker::create(FrameFn f, PreviewFn p) { return std::make_unique<MacCam>(std::move(f), std::move(p)); }
