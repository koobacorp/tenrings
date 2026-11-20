#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <mutex>
#include "engine/Engine.h"
#include "engine/Synth.h"
#include "Camera.h"

class TenringsProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener {
public:
    TenringsProcessor();
    ~TenringsProcessor() override;
    void prepareToPlay(double sr, int bs) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TENRINGS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; } // plucked strings ring out
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    // Tracking-thread entry (camera callback, or tests). Never called on the audio thread.
    void onTrackingFrame(const tr::Frame& f);
    // Mouse pads (message thread)
    void padHit(int note, int vel = 110);
    // Strings mode (message thread): undo / clear / add, mouse pluck + strum, snapshot for drawing
    void stringsUndo(); void stringsClear(); bool stringsAdd(tr::Pt a, tr::Pt b);
    int mouseStrum(tr::Pt from, tr::Pt to, float speed); int mousePluck(tr::Pt at);
    void readStrings(tr::StringField& out);
    int stringCount(); int stringNote(int i);

    // UI-side state
    struct Overlay { tr::Frame frame; std::array<std::array<tr::Engine::KeyLabel,5>,2> labels{}; int leadSlot = -1; int leadNote = -1; float leadBend = 0; int leadVol = 0; bool hatHold = false; int mode = 0; };
    bool readOverlay(Overlay& o);
    bool readPreview(juce::Image& img);
    juce::String statusText() const;
    tr::Params currentParams() const { return readParams(); }
    tr::CamStatus cameraStatus() const { return camera ? camera->status() : tr::CamStatus::Off; }

    // test hooks
    std::atomic<int> virtualPortMessages { 0 };
    bool virtualPortOpen() const { return virtualOut != nullptr || virtualPortStubOpen.load(); }
    double lastBpm() const { return bpmSeen.load(); }
    int activeOutgoingNotes() const { return tracker.count(); }
    int synthVoices() const { return synth.activeVoices(); }

private:
    void parameterChanged(const juce::String& id, float v) override;
    tr::Params readParams() const;
    void updateCamera();
    void updateVirtualPort();
    void emit(const tr::Ev& e, int sample, juce::MidiBuffer& out, bool sendMidi, bool sound);
    void flushOutgoing(juce::MidiBuffer& out, bool toMidi);
    double uiClock() const { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    std::atomic<float>* pMode, *pCam, *pSound, *pMidi, *pVport, *pKey, *pScale, *pEasy, *pOct, *pSens, *pHat, *pSlide, *pVib, *pBend, *pVol, *pLenPitch, *pBeige;

    // tracking thread
    tr::Engine engine; std::vector<tr::Ev> evScratch, uiScratch;
    std::mutex engineMx; // guards engine between the tracking thread and message-thread string edits (never taken on the audio thread)
    std::atomic<bool> engineResetReq { false };
    std::atomic<bool> hatHold { false }; std::atomic<int> handsVisible { 0 };
    std::atomic<double> lastFrameTime { 0 };
    // queues into audio thread
    tr::Spsc<tr::Ev, 2048> trackQ, uiQ;
    // audio thread
    tr::Synth synth; tr::NoteTracker tracker; tr::HatRoll hat;
    struct PendingOff { int note; int samplesLeft; };
    std::array<PendingOff, 64> pending{}; int nPending = 0;
    int lastMode = -1; bool lastSend = true, lastSound = true, lastCam = true, wasBypassed = false;
    std::atomic<double> bpmSeen { 120 };
    double sr = 48000;
    juce::MidiBuffer vportBuf;

    // overlay / preview (UI only; tracking thread uses try_lock so it never waits)
    std::mutex overlayMx; Overlay overlay; bool overlayFresh = false;
    std::mutex previewMx; juce::Image preview; bool previewFresh = false;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    std::unique_ptr<tr::CameraTracker> camera;
    std::unique_ptr<juce::MidiOutput> virtualOut; juce::SpinLock vportLock; std::atomic<bool> virtualPortStubOpen { false };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TenringsProcessor)
};
