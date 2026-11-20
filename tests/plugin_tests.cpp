// Plugin-level tests: every control changes behaviour, MIDI out pairing, sound on/off, incoming MIDI, tempo.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "helpers.h"
using namespace th;

struct FakeHead : juce::AudioPlayHead {
    double bpm = 120; bool hasBpm = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; if (hasBpm) p.setBpm(bpm); p.setIsPlaying(true); return p; }
};

struct Rig {
    TenringsProcessor p; FakeHead head; juce::AudioBuffer<float> buf { 2, 512 }; juce::MidiBuffer midi; double t = 100;
    std::vector<juce::MidiMessage> out; float peak = 0;
    Rig() { p.setPlayHead(&head); p.prepareToPlay(48000, 512); set("camera", 1); block(); }
    void set(const char* id, float plain) { auto* prm = p.apvts.getParameter(id); prm->setValueNotifyingHost(prm->convertTo0to1(plain)); }
    void block(int n = 1, juce::MidiBuffer* in = nullptr) {
        for (int i = 0; i < n; ++i) { midi.clear(); if (in && i == 0) midi = *in; p.processBlock(buf, midi);
            for (const auto m : midi) out.push_back(m.getMessage());
            peak = std::max(peak, buf.getMagnitude(0, 0, buf.getNumSamples())); }
    }
    void frame(const Hand* L, const Hand* R, int frames = 1) { for (int i = 0; i < frames; ++i) { t += 1.0 / 60; p.onTrackingFrame(th::frame(t, L, R)); block(); } }
    void clear() { out.clear(); peak = 0; }
    int ons(int note = -1) const { int c = 0; for (auto& m : out) if (m.isNoteOn() && (note < 0 || m.getNoteNumber() == note)) ++c; return c; }
    int offs(int note = -1) const { int c = 0; for (auto& m : out) if (m.isNoteOff() && (note < 0 || m.getNoteNumber() == note)) ++c; return c; }
    int bends() const { int c = 0; for (auto& m : out) c += m.isPitchWheel(); return c; }
    int lastBend() const { int b = -1; for (auto& m : out) if (m.isPitchWheel()) b = m.getPitchWheelValue(); return b; }
    int lastCC11() const { int b = -1; for (auto& m : out) if (m.isController() && m.getControllerNumber() == 11) b = m.getControllerValue(); return b; }
    int firstOnNote() const { for (auto& m : out) if (m.isNoteOn()) return m.getNoteNumber(); return -1; }
    void snare() { Hand R = hand(0.8f, 0.4f); frame(nullptr, &R, 3); R = hand(0.8f, 0.33f); frame(nullptr, &R); frame(nullptr, &R, 2); frame(nullptr, nullptr); }
    void keyTap(int finger, bool right = true, float x = 0.8f) { Hand h = hand(x, 0.5f); right ? frame(nullptr, &h, 4) : frame(&h, nullptr, 4);
        std::array<float,5> c{}; c[(size_t)finger] = 0.8f; Hand d = hand(x, 0.5f, c); right ? frame(nullptr, &d, 2) : frame(&d, nullptr, 2); }
    void keyRelease(bool right = true, float x = 0.8f) { Hand h = hand(x, 0.5f); right ? frame(nullptr, &h, 2) : frame(&h, nullptr, 2); }
    bool paired() const { int on[128] = {}; for (auto& m : out) { if (m.isNoteOn()) on[m.getNoteNumber()]++; else if (m.isNoteOff()) on[m.getNoteNumber()]--; } for (int v : on) if (v != 0) return false; return true; }
};

static const float kX2 = tr::Engine::kLeadX0 + tr::Engine::kSlotW * 2.5f;

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    // ---- plugin identity ----
    { Rig r; CHECK(r.p.acceptsMidi()); CHECK(r.p.producesMidi()); CHECK(!r.p.isMidiEffect()); CHECK(r.p.getName() == "TENRINGS"); CHECK(r.p.apvts.getParameter("mode") != nullptr); }

    // ---- DRUM via plugin: snare -> MIDI 38 + sound + auto note-off ----
    { Rig r; r.clear(); r.snare(); r.block(20); CHECK(r.ons(38) == 1); CHECK(r.offs(38) == 1); CHECK(r.ons() == 1); CHECK(r.peak > 0.01f); CHECK(r.paired()); }
    // mode param: Drum vs Keyboard vs Lead respond differently to the same gesture
    { Rig r; r.set("mode", 1); r.block(); r.clear(); r.snare(); r.block(5); CHECK(r.ons(38) == 0); }
    // ---- send MIDI off: no outgoing MIDI but sound still plays ----
    { Rig r; r.set("sendmidi", 0); r.block(); r.clear(); r.snare(); r.block(20); CHECK(r.out.empty()); CHECK(r.peak > 0.01f); }
    // ---- built-in sound off: silent audio while MIDI still emitted ----
    { Rig r; r.set("sound", 0); r.block(); r.clear(); r.snare(); r.block(20); CHECK(r.ons(38) == 1); CHECK(r.peak == 0.f); }
    // ---- volume ----
    { Rig a, b; b.set("volume", 0.2f); a.block(); b.block(); a.clear(); b.clear(); a.snare(); b.snare(); a.block(10); b.block(10); CHECK(a.peak > b.peak * 1.3f); CHECK(b.peak > 0); }
    // ---- camera off: tracked frames do nothing; pads + incoming MIDI still play ----
    { Rig r; r.set("camera", 0); r.block(); r.clear(); r.snare(); r.block(5); CHECK(r.ons() == 0);
      r.p.padHit(36); r.block(20); CHECK(r.ons(36) == 1); CHECK(r.offs(36) == 1); CHECK(r.peak > 0.01f);
      r.clear(); juce::MidiBuffer in; in.addEvent(juce::MidiMessage::noteOn(1, 38, (juce::uint8)110), 10); r.block(1, &in); r.block(4);
      CHECK(r.peak > 0.01f); CHECK(r.ons() == 0); } // incoming MIDI not echoed (no feedback loop)
    // pads only in drum mode
    { Rig r; r.set("mode", 1); r.block(); r.clear(); r.p.padHit(36); r.block(5); CHECK(r.ons() == 0); }
    // ---- incoming MIDI plays built-in sound in each mode only when sound is on ----
    for (int m = 0; m < 4; ++m) { Rig r; r.set("mode", (float)m); r.block(); r.clear();
      juce::MidiBuffer in; in.addEvent(juce::MidiMessage::noteOn(1, m == 0 ? 36 : 64, (juce::uint8)110), 0); r.block(1, &in); r.block(8); CHECK(r.peak > 0.01f);
      Rig q; q.set("mode", (float)m); q.set("sound", 0); q.block(); q.clear(); q.block(1, &in); q.block(8); CHECK(q.peak == 0.f); }
    // ---- hat roll: tempo synced, 1/8 vs 1/16, tempo change, stops on leave ----
    { auto hats = [](double bpm, int rate) { Rig r; r.head.bpm = bpm; r.set("hatrate", (float)rate); r.block(); r.clear();
          Hand L = hand(0.08f, 0.5f); r.frame(&L, nullptr); r.block(93); // 94 blocks*512 = 48128 samples ~ 1 s
          int n = r.ons(42); r.clear(); r.frame(nullptr, nullptr); r.block(100); CHECK(r.ons(42) == 0); return n; };
      CHECK(hats(120, 1) == 9); CHECK(hats(120, 0) == 5); CHECK(hats(60, 1) == 5); CHECK(hats(150, 1) == 11);
      Rig r; r.head.hasBpm = false; r.block(); CHECK(std::abs(r.p.lastBpm() - 120) < 1e-9); r.head.hasBpm = true; r.head.bpm = 97; r.block(); CHECK(std::abs(r.p.lastBpm() - 97) < 1e-9); }
    // ---- sensitivity ----
    { auto hit = [](float sens) { Rig r; r.set("sens", sens); r.block(); r.clear(); Hand R = hand(0.8f, 0.7f); r.frame(nullptr, &R, 3); R = hand(0.8f, 0.73f); r.frame(nullptr, &R); r.block(3); return r.ons(); };
      CHECK(hit(0.f) == 0); CHECK(hit(1.f) == 1); }

    // ---- KEYBOARD: finger pairs, key, scale, easy5, octave, out-of-frame, mode switch ----
    { Rig r; r.set("mode", 1); r.block(); r.clear(); r.keyTap(1); CHECK(r.ons(62) == 1); r.keyRelease(); CHECK(r.offs(62) == 1); CHECK(r.paired()); CHECK(r.peak > 0.005f); }
    { auto note = [](std::initializer_list<std::pair<const char*, float>> ps, int finger, bool right = true) { Rig r; r.set("mode", 1); for (auto& [id, v] : ps) r.set(id, v); r.block(); r.clear(); r.keyTap(finger, right, right ? 0.8f : 0.2f); return r.firstOnNote(); };
      CHECK(note({}, 0) == 60); CHECK(note({{"key", 2}}, 0) == 62); CHECK(note({{"scale", 0}}, 2) == 64); CHECK(note({{"scale", 1}}, 2) == 63);
      CHECK(note({{"easy5", 1}}, 1) == 63); CHECK(note({{"octave", 1}}, 0) == 72); CHECK(note({{"octave", -1}}, 0) == 48); CHECK(note({}, 0, false) == 58); }
    { Rig r; r.set("mode", 1); r.block(); r.keyTap(2); r.clear(); r.frame(nullptr, nullptr); CHECK(r.offs() == 1); CHECK(r.p.activeOutgoingNotes() == 0); }
    { Rig r; r.set("mode", 1); r.block(); r.keyTap(2); r.clear(); r.set("mode", 0); r.block(); CHECK(r.offs() == 1); CHECK(r.p.activeOutgoingNotes() == 0); r.block(40); CHECK(r.p.synthVoices() == 0); }
    // send MIDI turned off while a key is held => note-off sent, nothing stuck
    { Rig r; r.set("mode", 1); r.block(); r.keyTap(2); r.clear(); r.set("sendmidi", 0); r.block(); CHECK(r.offs() == 1); r.clear(); r.keyRelease(); CHECK(r.out.empty()); }
    // camera turned off while holding => all off
    { Rig r; r.set("mode", 1); r.block(); r.keyTap(2); r.clear(); r.set("camera", 0); r.block(); CHECK(r.offs() == 1); CHECK(r.p.activeOutgoingNotes() == 0); }

    // ---- LEAD: needs both hands; bend/CC11; slide/vibrato/bend controls change output ----
    { Rig r; r.set("mode", 2); r.block(); r.clear(); Hand L = hand(0.2f, 0.4f), R = hand(kX2, 0.5f);
      r.frame(nullptr, &R, 3); CHECK(r.ons() == 0); r.frame(&L, &R, 3); CHECK(r.ons(63) == 1); CHECK(r.lastCC11() >= 0); CHECK(r.peak > 0.005f);
      r.clear(); r.frame(nullptr, &R); CHECK(r.offs(63) == 1); }
    auto leadWobbleBends = [](float vib) { Rig r; r.set("mode", 2); r.set("vibrato", vib); r.block(); Hand L = hand(0.2f, 0.4f), R = hand(kX2, 0.5f); r.frame(&L, &R, 3); r.clear();
        for (int i = 0; i < 20; ++i) { Hand w = hand(kX2 + ((i % 4) < 2 ? 0.012f : -0.012f), 0.5f); r.frame(&L, &w); } return r.bends(); };
    CHECK(leadWobbleBends(1.f) > 5); CHECK(leadWobbleBends(0.f) == 0);
    auto leadSlideBend = [](float slide) { Rig r; r.set("mode", 2); r.set("slide", slide); r.set("vibrato", 0); r.block(); Hand L = hand(0.2f, 0.4f), R = hand(kX2, 0.5f); r.frame(&L, &R, 3); r.clear();
        Hand n = hand(kX2 + tr::Engine::kSlotW, 0.5f); r.frame(&L, &n, 4); return r.lastBend(); };
    CHECK(leadSlideBend(1.f) < 8192 - 1000); { int b = leadSlideBend(0.f); CHECK(b == -1 || std::abs(b - 8192) < 30); }
    auto leadUp = [](float bend) { Rig r; r.set("mode", 2); r.set("bend", bend); r.set("vibrato", 0); r.block(); Hand L = hand(0.2f, 0.4f), R = hand(kX2, 0.5f); r.frame(&L, &R, 3);
        for (int i = 1; i <= 20; ++i) { Hand u = hand(kX2, 0.5f - 0.01f * i); r.frame(&L, &u); } return r.lastBend(); };
    CHECK(leadUp(1.f) == 16383); CHECK(std::abs(leadUp(0.5f) - 12288) < 150); CHECK(leadUp(0.f) == -1 || std::abs(leadUp(0.f) - 8192) < 30);
    // lead pitch-bend range drives the built-in synth too (easy5 minor => 3 semitones) – scale/key apply in lead
    { Rig r; r.set("mode", 2); r.set("key", 5); r.block(); Hand L = hand(0.2f, 0.4f), R = hand(kX2, 0.5f); r.clear(); r.frame(&L, &R, 3); CHECK(r.firstOnNote() == 68); }

    // ---- v1.1: actual plugin queue/MIDI/overlay uses independent piano presses ----
    { Rig r; r.set("mode", 1); r.block(); Hand h=hand(.8f,.5f); r.frame(nullptr,&h,5); r.clear();
      h=hand(.8f,.5f,{.07f,.65f,.09f,.05f,.06f}); r.frame(nullptr,&h);
      CHECK(r.ons()==1 && r.ons(62)==1); TenringsProcessor::Overlay ov; CHECK(r.p.readOverlay(ov));
      for(int i=0;i<5;++i) CHECK(ov.labels[1][i].down==(i==1));
      h=hand(.8f,.5f); r.frame(nullptr,&h); CHECK(r.offs(62)==1); CHECK(r.paired()); }
    { Rig r; r.set("mode", 1); r.block(); Hand h=hand(.8f,.5f); r.frame(nullptr,&h,5); r.clear();
      h=hand(.8f,.5f,{.6f,.6f,.6f,.6f,.6f}); r.frame(nullptr,&h,8); CHECK(r.ons()==0); }
    // ---- v1.1: passing lead slots never produces intermediate MIDI note-ons ----
    { Rig r; r.set("mode", 2); r.set("vibrato",0); r.block(); Hand l=hand(.2f,.4f),h=hand(Engine::kLeadX0+Engine::kSlotW*.5f,.5f);
      r.frame(&l,&h,5); r.clear();
      for(int i=1;i<=15;++i) { h=hand(Engine::kLeadX0+Engine::kSlotW*(.5f+5.f*i/15.f),.5f); r.frame(&l,&h); }
      CHECK(r.ons()==0); r.frame(&l,&h,3); CHECK(r.ons()==1 && r.ons(68)==1); CHECK(r.offs(60)==1);
      TenringsProcessor::Overlay ov;r.p.readOverlay(ov);CHECK(ov.leadSlot==5); }

    // ---- virtual MIDI port toggle ----
    { Rig r; CHECK(!r.p.virtualPortOpen()); r.set("vport", 1); r.block(); CHECK(r.p.virtualPortOpen()); r.snare(); r.block(20); CHECK(r.p.virtualPortMessages.load() >= 2);
      int before = r.p.virtualPortMessages.load(); r.set("vport", 0); r.block(); CHECK(!r.p.virtualPortOpen()); r.snare(); r.block(20); CHECK(r.p.virtualPortMessages.load() == before);
      // vport delivers even with wrapper MIDI off (pick ONE route in FL to avoid doubles)
      Rig q; q.set("vport", 1); q.set("sendmidi", 0); q.block(); q.clear(); q.snare(); q.block(20); CHECK(q.out.empty()); CHECK(q.p.virtualPortMessages.load() >= 2); }

    // ---- no hands = silence; long random run has no stuck notes ----
    { Rig r; r.clear(); r.frame(nullptr, nullptr, 60); CHECK(r.out.empty()); CHECK(r.peak == 0.f); }
    { juce::Random rnd(7); for (int m = 0; m < 4; ++m) { Rig r; r.set("mode", (float)m); r.block(); r.clear();
        for (int i = 0; i < 1500; ++i) { Hand L = hand(rnd.nextFloat(), rnd.nextFloat(), {rnd.nextFloat(), rnd.nextFloat(), rnd.nextFloat(), rnd.nextFloat(), rnd.nextFloat()});
            Hand R = hand(rnd.nextFloat(), rnd.nextFloat(), {rnd.nextFloat(), rnd.nextFloat(), rnd.nextFloat(), rnd.nextFloat(), rnd.nextFloat()});
            bool lp = rnd.nextInt(10) > 1, rp = rnd.nextInt(10) > 1; r.frame(lp ? &L : nullptr, rp ? &R : nullptr); }
        r.frame(nullptr, nullptr, 3); r.block(20); CHECK(r.paired()); CHECK(r.p.activeOutgoingNotes() == 0); } }

    // ---- state save/restore keeps every parameter ----
    { Rig a; a.set("mode", 2); a.set("key", 7); a.set("octave", -1); a.set("hatrate", 0); a.set("slide", 0.2f);
      juce::MemoryBlock mb; a.p.getStateInformation(mb); Rig b; b.p.setStateInformation(mb.getData(), (int)mb.getSize());
      for (auto id : {"mode", "key", "octave", "hatrate", "slide"}) CHECK(std::abs(a.p.apvts.getRawParameterValue(id)->load() - b.p.apvts.getRawParameterValue(id)->load()) < 1e-6f); }
    // ---- editor builds, resizes, switches colours ----
    { Rig r; std::unique_ptr<juce::AudioProcessorEditor> ed(r.p.createEditor()); CHECK(ed != nullptr); ed->setSize(1200, 800); ed->setSize(720, 480);
      CHECK(r.p.statusText().isNotEmpty()); }

    // Keyboard UI snapshot + pixel checks use the same labels as outgoing MIDI.
    { Rig r; r.set("mode",1); r.block(); Hand l=hand(.2f,.5f),h=hand(.8f,.5f); r.frame(&l,&h,5);
      h=hand(.8f,.5f,{0,.65f,0,0,0});r.frame(&l,&h);
      auto ed=std::make_unique<TenringsEditor>(r.p); ed->setSize(1000,680);r.p.readOverlay(ed->view.ov);
      auto image=ed->createComponentSnapshot(ed->getLocalBounds());CHECK(image.isValid());
      juce::PNGImageFormat png;juce::FileOutputStream stream(juce::File::getCurrentWorkingDirectory().getChildFile("keyboard-v1.1.png"));
      CHECK(png.writeImageToStream(image,stream));
      // Key bank pixels: only right index is turquoise, adjacent keys stay ivory.
      auto bank=ed->view.createComponentSnapshot(ed->view.getLocalBounds());
      float kw=(bank.getWidth()-24.f)/10.f;int y=bank.getHeight()-12;
      auto lit=bank.getPixelAt((int)(8+6*kw+8+kw*.5f),y);
      auto up=bank.getPixelAt((int)(8+7*kw+8+kw*.5f),y);
      auto kc=modeColour(tr::Keys);
      CHECK(std::abs(lit.getGreen()-kc.getGreen())<30 && lit.getRed()<60);CHECK(up.getRed()>200 && up.getGreen()>200);
      for (auto dims : {juce::Point<int>{720,480},juce::Point<int>{1400,900}}) {
        ed->setSize(dims.x,dims.y);auto snap=ed->createComponentSnapshot(ed->getLocalBounds());CHECK(snap.isValid());
      }
    }


    // ======================= v1.2: Strings mode, beige look, ceiling, bypass =======================
    auto tipHand = [](Pt idx) { Hand h = hand(idx.x, idx.y + 0.2f); h.tip[1] = idx; return h; };
    auto oneTip = [](Pt t) { Hand h; h.present = true; h.palm = {t.x, t.y + 0.2f}; for (auto& q : h.tip) q = h.palm; h.tip[1] = t; h.tipOk = {false, true, false, false, false}; return h; };
    auto fullHand = [](Pt c) { Hand h; h.present = true; h.palm = {c.x, c.y + 0.15f}; for (int i = 0; i < 5; ++i) { h.tip[(size_t)i] = {c.x + (i - 2) * 0.03f, c.y}; h.mcp[(size_t)i] = {c.x + (i - 2) * 0.025f, c.y + 0.09f}; h.pip[(size_t)i] = {c.x + (i - 2) * 0.028f, c.y + 0.05f}; h.hasPip[(size_t)i] = true; } return h; };
    auto lerpP = [](Pt a, Pt b, float k) { return Pt{a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k}; };
    auto drawString = [&](Rig& r, Pt a, Pt b) { Pt m = lerpP(a, b, 0.5f); Hand L = tipHand({m.x - 0.004f, m.y}), R = tipHand({m.x + 0.004f, m.y}); r.frame(&L, &R, 15);
        for (int k = 1; k <= 20; ++k) { L = tipHand(lerpP(m, a, k / 20.f)); R = tipHand(lerpP(m, b, k / 20.f)); r.frame(&L, &R); } r.frame(&L, &R, 25); r.frame(nullptr, nullptr, 2); };
    auto pluckDown = [&](Rig& r, float x, float y0, float y1) { r.frame(nullptr, nullptr, 20); Hand R = oneTip({x, y0}); r.frame(nullptr, &R, 3); R = oneTip({x, y1}); r.frame(nullptr, &R); r.frame(nullptr, nullptr); };
    auto finite = [](const juce::AudioBuffer<float>& b) { for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (!std::isfinite(b.getSample(c, i))) return false; return true; };
    const float kCeil = 0.98855f; // -0.1 dBFS

    // identity: version, latency, parameters
    { Rig r; CHECK(juce::String(JucePlugin_VersionString) == "1.2.0"); CHECK(r.p.getLatencySamples() == 0);
      auto* mp = dynamic_cast<juce::AudioParameterChoice*>(r.p.apvts.getParameter("mode")); CHECK(mp != nullptr && mp->choices.size() == 4 && mp->choices[3] == "Strings");
      CHECK(r.p.apvts.getParameter("lenpitch") != nullptr); CHECK(r.p.apvts.getParameter("beige") != nullptr); CHECK(r.p.getTailLengthSeconds() >= 2.0); }

    // camera gesture -> string -> pluck: MIDI on, held 600 ms, off; built-in sound under the ceiling
    { Rig r; r.set("mode", 3); r.block(); r.clear(); drawString(r, {0.3f, 0.5f}, {0.7f, 0.5f});
      CHECK(r.p.stringCount() == 1); CHECK(r.ons() == 0); CHECK(r.p.stringNote(0) == 60);
      r.clear(); pluckDown(r, 0.5f, 0.4f, 0.6f); CHECK(r.ons(60) == 1); CHECK(r.offs() == 0); CHECK(r.p.activeOutgoingNotes() == 1);
      r.block(45); CHECK(r.offs(60) == 0); r.block(15); CHECK(r.offs(60) == 1); CHECK(r.paired()); CHECK(r.p.activeOutgoingNotes() == 0);
      CHECK(r.peak > 0.02f); CHECK(r.peak < kCeil); CHECK(r.p.synthVoices() >= 1); r.block(400); CHECK(r.p.synthVoices() == 0); }
    // key / scale / octave / length mode all reach the strings
    { Rig r; r.set("mode", 3); r.set("key", 4); r.block(); drawString(r, {0.3f, 0.3f}, {0.7f, 0.3f}); drawString(r, {0.3f, 0.6f}, {0.7f, 0.6f});
      CHECK(r.p.stringCount() == 2); CHECK(r.p.stringNote(0) == 64); CHECK(r.p.stringNote(1) == 66);
      r.clear(); pluckDown(r, 0.5f, 0.5f, 0.7f); CHECK(r.ons(66) == 1);
      r.set("octave", 1); r.block(); CHECK(r.p.stringNote(0) == 76); r.set("octave", 0); r.set("scale", 0); r.block(); CHECK(r.p.stringNote(1) == 66);
      r.set("lenpitch", 1); r.block(); CHECK(r.p.stringNote(0) == r.p.stringNote(1)); // same length = same pitch
      CHECK(r.p.stringsAdd({0.05f, 0.9f}, {0.95f, 0.9f})); CHECK(r.p.stringNote(2) < r.p.stringNote(0)); }   // longer = lower
    // strum: 3 strings, one hand sweep = 3 notes; re-pluck while a MIDI note is still held stays paired
    { Rig r; r.set("mode", 3); r.block(); for (float y : {0.35f, 0.5f, 0.65f}) CHECK(r.p.stringsAdd({0.2f, y}, {0.8f, y}));
      r.clear(); Hand R = fullHand({0.5f, 0.3f}); r.frame(nullptr, &R, 3); R = fullHand({0.5f, 0.7f}); r.frame(nullptr, &R);
      CHECK(r.ons() == 3); CHECK(r.ons(60) == 1 && r.ons(62) == 1 && r.ons(63) == 1);
      r.frame(nullptr, &R, 8); R = fullHand({0.5f, 0.3f}); r.frame(nullptr, &R); CHECK(r.ons() == 6);
      r.block(80); CHECK(r.paired()); CHECK(r.offs() == 6); CHECK(r.p.activeOutgoingNotes() == 0); CHECK(r.peak < kCeil); }
    // undo / clear / mouse pluck + strum (Strings mode only)
    { Rig r; r.set("mode", 3); r.block(); r.p.stringsAdd({0.2f, 0.4f}, {0.8f, 0.4f}); r.p.stringsAdd({0.2f, 0.6f}, {0.8f, 0.6f}); CHECK(r.p.stringCount() == 2);
      r.clear(); CHECK(r.p.mousePluck({0.5f, 0.41f}) == 1); r.block(); CHECK(r.ons(60) == 1);
      CHECK(r.p.mouseStrum({0.4f, 0.3f}, {0.4f, 0.7f}, 2.f) == 1); r.block(); CHECK(r.ons(62) == 1);   // string 0 still inside its 90 ms debounce
      r.p.stringsUndo(); CHECK(r.p.stringCount() == 1); r.p.stringsClear(); CHECK(r.p.stringCount() == 0); r.block(70); CHECK(r.paired());
      Rig d; d.p.stringsAdd({0.2f, 0.4f}, {0.8f, 0.4f}); d.clear(); CHECK(d.p.mousePluck({0.5f, 0.4f}) == 0); CHECK(d.p.mouseStrum({0.5f, 0.3f}, {0.5f, 0.5f}, 2.f) == 0); d.block(3); CHECK(d.ons() == 0); }
    // switching away mid-note: MIDI note-off right away, strings kept; out-of-frame = silence
    { Rig r; r.set("mode", 3); r.block(); r.p.stringsAdd({0.3f, 0.5f}, {0.7f, 0.5f}); pluckDown(r, 0.5f, 0.4f, 0.6f); CHECK(r.p.activeOutgoingNotes() == 1);
      r.clear(); r.set("mode", 1); r.block(); CHECK(r.offs(60) == 1); CHECK(r.p.activeOutgoingNotes() == 0); CHECK(r.p.stringCount() == 1);
      Rig q; q.set("mode", 3); q.block(); q.p.stringsAdd({0.3f, 0.5f}, {0.7f, 0.5f}); q.clear(); q.frame(nullptr, nullptr, 60); CHECK(q.out.empty()); CHECK(q.peak == 0.f); }
    // state: strings + new switches survive save/restore; old projects load with no strings; no duplicate data on re-save
    { Rig a; a.set("mode", 3); a.set("key", 4); a.set("lenpitch", 1); a.set("beige", 1); a.block();
      a.p.stringsAdd({0.1f, 0.2f}, {0.9f, 0.25f}); a.p.stringsAdd({0.3f, 0.5f}, {0.6f, 0.55f}); a.p.stringsAdd({0.2f, 0.8f}, {0.7f, 0.7f});
      juce::MemoryBlock mb; a.p.getStateInformation(mb); Rig b; b.p.setStateInformation(mb.getData(), (int)mb.getSize());
      CHECK(b.p.stringCount() == 3); for (int i = 0; i < 3; ++i) CHECK(b.p.stringNote(i) == a.p.stringNote(i));
      StringField fa, fb; a.p.readStrings(fa); b.p.readStrings(fb); bool same = true;
      for (int i = 0; i < 3; ++i) same &= std::abs(fa.s[i].a.x - fb.s[i].a.x) < 1e-5f && std::abs(fa.s[i].b.y - fb.s[i].b.y) < 1e-5f && fa.s[i].colour == fb.s[i].colour && fa.s[i].seqDeg == fb.s[i].seqDeg;
      CHECK(same); for (auto id : {"mode", "key", "lenpitch", "beige"}) CHECK(std::abs(a.p.apvts.getRawParameterValue(id)->load() - b.p.apvts.getRawParameterValue(id)->load()) < 1e-6f);
      CHECK(!b.p.apvts.state.getChildWithName("STRINGS").isValid());
      juce::MemoryBlock mb2; b.p.getStateInformation(mb2); auto x2 = juce::AudioProcessor::getXmlFromBinary(mb2.getData(), (int)mb2.getSize());
      int nStrings = 0; if (x2) for (auto* e : x2->getChildWithTagNameIterator("STRINGS")) { (void)e; ++nStrings; } CHECK(nStrings == 1);
      // a v1.1 project (parameters only) loads cleanly, with no strings
      auto old = a.p.apvts.copyState().createXml(); old->deleteAllChildElementsWithTagName("STRINGS"); juce::MemoryBlock mo; juce::AudioProcessor::copyXmlToBinary(*old, mo);
      b.p.setStateInformation(mo.getData(), (int)mo.getSize()); CHECK(b.p.stringCount() == 0); CHECK((int)b.p.apvts.getRawParameterValue("mode")->load() == 3);
      b.p.setStateInformation("junk", 4); CHECK(b.p.stringCount() == 0); }
    // host bypass: silent, held notes released once, no orphan note-offs afterwards
    { Rig r; r.set("mode", 1); r.block(); r.keyTap(2); CHECK(r.p.activeOutgoingNotes() == 1); r.clear();
      juce::MidiBuffer mb; r.buf.applyGain(0.f); r.buf.setSample(0, 0, 0.5f); r.p.processBlockBypassed(r.buf, mb);
      int offs = 0; for (const auto m : mb) offs += m.getMessage().isNoteOff(); CHECK(offs == 1); CHECK(r.buf.getMagnitude(0, r.buf.getNumSamples()) == 0.f);
      juce::MidiBuffer mb2; mb2.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0); r.p.processBlockBypassed(r.buf, mb2); CHECK(mb2.isEmpty()); CHECK(r.buf.getMagnitude(0, r.buf.getNumSamples()) == 0.f);
      r.block(); r.keyRelease(); r.block(5); CHECK(r.offs() == 0); CHECK(r.p.activeOutgoingNotes() == 0); }
    // safety ceiling at max volume: drums, and wild strumming on 8 strings; never NaN
    { Rig r; r.set("volume", 1.f); r.block(); r.clear(); for (int i = 0; i < 6; ++i) { r.p.padHit(36, 127); r.p.padHit(38, 127); r.p.padHit(42, 127); r.block(3); }
      CHECK(r.peak < kCeil); CHECK(r.peak > 0.3f); CHECK(finite(r.buf));
      Rig s; s.set("mode", 3); s.set("volume", 1.f); s.block(); for (int i = 0; i < 8; ++i) s.p.stringsAdd({0.1f, 0.1f + 0.1f * i}, {0.9f, 0.12f + 0.1f * i});
      s.clear(); bool fin = true; juce::Random rnd(3);
      for (int k = 0; k < 40; ++k) for (int j = 0; j < 4; ++j) { float y = k % 2 ? 0.02f + 0.31f * j : 0.95f - 0.31f * j;
          Hand A = fullHand({0.3f + 0.02f * rnd.nextFloat(), y}), B = fullHand({0.7f, 0.97f - y}); s.frame(&A, &B, j == 3 ? 4 : 1); fin &= finite(s.buf); }
      s.block(100); CHECK(fin); CHECK(s.peak < kCeil); CHECK(s.peak > 0.2f); CHECK(s.paired()); CHECK(s.ons() >= 100); }

    // ---- editor: 4 modes, beige look, Strings controls, beige-only hides the camera ----
    { Rig r; auto ed = std::make_unique<TenringsEditor>(r.p); ed->setSize(1000, 680); ed->refresh();
      auto px = ed->createComponentSnapshot(ed->getLocalBounds()).getPixelAt(ed->getWidth() - 6, ed->getHeight() / 2);
      CHECK(std::abs(px.getRed() - 0xe8) < 6 && std::abs(px.getGreen() - 0xdc) < 6 && std::abs(px.getBlue() - 0xc4) < 6);
      CHECK(!ed->undoBtn.isVisible()); ed->modeBtn[3].onClick(); ed->refresh();
      CHECK((int)r.p.apvts.getRawParameterValue("mode")->load() == 3); CHECK(ed->undoBtn.isVisible() && ed->clearBtn.isVisible() && ed->lenPitchBtn.isVisible()); CHECK(!ed->hatBox.isVisible());
      r.p.stringsAdd({0.2f, 0.5f}, {0.8f, 0.5f}); ed->refresh(); CHECK(ed->stringsInfo.getText().contains("1 / 8")); CHECK(ed->stringsInfo.getText().contains("D4"));
      ed->undoBtn.onClick(); CHECK(r.p.stringCount() == 0);
      // camera picture vs beige only
      ed->view.img = juce::Image(juce::Image::ARGB, 64, 36, true); ed->view.img.clear(ed->view.img.getBounds(), juce::Colours::red);
      auto mid = [&] { return ed->view.createComponentSnapshot(ed->view.getLocalBounds()).getPixelAt(ed->view.getWidth() / 3, ed->view.getHeight() * 2 / 3); };
      CHECK(mid().getRed() > 200 && mid().getGreen() < 60);
      r.set("beige", 1); ed->refresh(); auto b = mid(); CHECK(std::abs(b.getRed() - 0xe8) < 8 && std::abs(b.getBlue() - 0xc4) < 8);
      for (auto dims : {juce::Point<int>{720, 480}, juce::Point<int>{1600, 1000}, juce::Point<int>{2400, 1600}}) { ed->setSize(dims.x, dims.y); CHECK(ed->createComponentSnapshot(ed->getLocalBounds()).isValid()); }
      CHECK(ed->getConstrainer() != nullptr && ed->isResizable()); }

    // ---- screenshots: every mode on beige + Strings with strings, plucks, sparks (simulated hands) ----
    { auto fakeCamera = [] { juce::Image im(juce::Image::ARGB, 320, 180, true); juce::Graphics g(im);
          g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a3b2c), 0, 0, juce::Colour(0xff1d1814), 0, 180, false)); g.fillAll();
          g.setColour(juce::Colour(0xff6b5a44)); g.fillRect(20, 15, 70, 45); g.setColour(juce::Colour(0xff8a7a62)); g.fillRect(24, 19, 62, 37);
          g.setColour(juce::Colour(0xff2a3540)); g.fillRect(230, 0, 90, 120); g.setColour(juce::Colour(0xff6e604f)); g.fillEllipse(135, 50, 50, 60); g.fillRect(115, 105, 90, 75);
          return im; };
      auto shot = [](juce::Component& c, const char* name) { auto im = c.createComponentSnapshot(c.getLocalBounds()); juce::PNGImageFormat png;
          juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(name); f.deleteFile(); juce::FileOutputStream st(f); return im.isValid() && png.writeImageToStream(im, st); };
      for (int m = 0; m < 3; ++m) { Rig r; r.set("mode", (float)m); r.set("beige", 1); r.block();
          Hand L = hand(.25f, .62f), R = hand(.78f, .45f); r.frame(&L, &R, 5);
          if (m == 1) { R = hand(.78f, .45f, {0, .65f, 0, 0, 0}); r.frame(&L, &R); }
          if (m == 0) r.p.padHit(38);
          auto ed = std::make_unique<TenringsEditor>(r.p); ed->setSize(1000, 680); ed->refresh(); if (m == 0) { ed->view.flashNote = 38; ed->view.flashT = juce::Time::getMillisecondCounterHiRes(); }
          const char* nm[3] = {"v12-drum-beige.png", "v12-keyboard-beige.png", "v12-lead-beige.png"}; CHECK(shot(*ed, nm[m])); }
      { Rig r; r.set("mode", 0); r.block(); Hand L = hand(.25f, .62f), R = hand(.78f, .45f); r.frame(&L, &R, 5);
        auto ed = std::make_unique<TenringsEditor>(r.p); ed->setSize(1000, 680); ed->refresh(); ed->view.img = fakeCamera(); CHECK(shot(*ed, "v12-drum-camera.png")); }
      for (int beigeOnly = 0; beigeOnly < 2; ++beigeOnly) {
          Rig r; r.set("mode", 3); r.set("key", 4); r.set("beige", (float)beigeOnly); r.block();
          drawString(r, {0.18f, 0.30f}, {0.80f, 0.22f}); drawString(r, {0.12f, 0.48f}, {0.86f, 0.40f}); drawString(r, {0.25f, 0.62f}, {0.75f, 0.72f});
          drawString(r, {0.30f, 0.15f}, {0.62f, 0.82f}); drawString(r, {0.58f, 0.12f}, {0.90f, 0.66f});
          Hand L = fullHand({0.30f, 0.36f}), R = fullHand({0.70f, 0.55f}); r.frame(&L, &R, 3); R = fullHand({0.66f, 0.80f}); r.frame(&L, &R);
          CHECK(r.p.stringCount() == 5); CHECK(r.ons() >= 2);
          auto ed = std::make_unique<TenringsEditor>(r.p); ed->setSize(1100, 720); if (!beigeOnly) ed->view.img = fakeCamera();
          ed->view.nowOverride = 1000.0; ed->refresh(); ed->view.nowOverride = 1000.11;
          CHECK(!ed->view.sparks.empty()); CHECK(ed->view.animating(1000.11));
          CHECK(shot(*ed, beigeOnly ? "v12-strings-beige.png" : "v12-strings-camera.png")); }
      { Rig r; r.set("mode", 3); r.set("key", 4); r.set("beige", 1); r.block(); r.p.stringsAdd({0.15f, 0.25f}, {0.85f, 0.3f}); r.p.stringsAdd({0.2f, 0.75f}, {0.8f, 0.7f});
        Hand L = tipHand({0.496f, 0.5f}), R = tipHand({0.504f, 0.5f}); r.frame(&L, &R, 15);
        for (int k = 1; k <= 12; ++k) { L = tipHand(lerpP({0.5f, 0.5f}, {0.28f, 0.55f}, k / 12.f)); R = tipHand(lerpP({0.5f, 0.5f}, {0.72f, 0.45f}, k / 12.f)); r.frame(&L, &R); }
        auto ed = std::make_unique<TenringsEditor>(r.p); ed->setSize(1000, 680); ed->refresh(); CHECK(ed->view.sf.phase == StringField::Stretch);
        CHECK(shot(*ed, "v12-strings-create.png")); }
    }

    std::printf("plugin tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
