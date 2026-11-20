#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

juce::Colour modeColour(int mode);
juce::Colour stringColour(int idx);
namespace theme {
    const juce::Colour beige(0xffe8dcc4), panel(0xfff4ecdc), ink(0xff1f1a12), outline(0xff6e5e44), muted(0xff5a4e3a), keybed(0xff2b241a);
}

class CameraView : public juce::Component {
public:
    explicit CameraView(TenringsProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void ingestFx(double now);           // new plucks -> wobble + sparks
    bool animating(double now) const;
    TenringsProcessor::Overlay ov; juce::Image img; int flashNote = -1; double flashT = 0;
    tr::StringField sf; bool beigeOnly = false;
    double nowOverride = -1;             // test hook: freeze animation time (s)
    double now() const { return nowOverride >= 0 ? nowOverride : juce::Time::getMillisecondCounterHiRes() * 0.001; }
    struct Wobble { double t0 = -100; float u = 0.5f, amp = 0; };
    struct Spark { float x = 0, y = 0, vx = 0, vy = 0; double t0 = 0; int col = 0; };
    std::array<Wobble, tr::kMaxStrings> wob{}; std::vector<Spark> sparks; uint32_t seenFx = 0;
private:
    void paintStrings(juce::Graphics&, float W, float H, bool onCam);
    void paintSkeleton(juce::Graphics&, float W, float H, juce::Colour c);
    TenringsProcessor& proc;
    juce::Random rnd { 1234 };
    juce::Point<float> downPos, lastPos; double lastMs = 0; bool creating = false, dragged = false;
};

class TenringsEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TenringsEditor(TenringsProcessor&);
    ~TenringsEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();                      // test hook: same as one timer tick
    // test hooks
    juce::TextButton modeBtn[4];
    CameraView view;
    juce::ToggleButton camBtn {"Camera"}, soundBtn {"Built-in sound"}, midiBtn {"Send MIDI"}, vportBtn {"Virtual port"}, easyBtn {"Easy 5-note"},
                       beigeBtn {"Beige only (hide camera)"}, lenPitchBtn {"Length sets pitch"};
    juce::TextButton undoBtn {"Undo string"}, clearBtn {"Clear strings"};
    juce::ComboBox keyBox, scaleBox, hatBox;
    juce::Slider octSl, sensSl, slideSl, vibSl, bendSl, volSl;
    juce::Label status, stringsInfo;
private:
    void timerCallback() override;
    void applyColours();
    TenringsProcessor& proc;
    juce::LookAndFeel_V4 lnf;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    juce::OwnedArray<juce::Label> labels;
    int shownMode = -1;
};
