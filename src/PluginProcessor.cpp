#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tr;
using APVTS = juce::AudioProcessorValueTreeState;

juce::StringArray keyNames() { return {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}; }

APVTS::ParameterLayout TenringsProcessor::layout() {
    using namespace juce;
    APVTS::ParameterLayout l;
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"mode",1}, "Mode", StringArray{"Drum","Keyboard","Lead","Strings"}, 0));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"camera",1}, "Camera", true));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"sound",1}, "Built-in Sound", true));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"sendmidi",1}, "Send MIDI", true));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"vport",1}, "Virtual MIDI Port", false));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"key",1}, "Key", keyNames(), 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"scale",1}, "Scale", StringArray{"Major","Minor"}, 1));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"easy5",1}, "Easy 5-Note", false));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{"octave",1}, "Octave", -2, 2, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"sens",1}, "Sensitivity", 0.f, 1.f, 0.5f));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{"hatrate",1}, "Hat Roll", StringArray{"1/8","1/16"}, 1));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"slide",1}, "Lead Slide", 0.f, 1.f, 0.5f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"vibrato",1}, "Lead Vibrato", 0.f, 1.f, 0.5f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"bend",1}, "Lead Bend", 0.f, 1.f, 1.f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{"volume",1}, "Volume", 0.f, 1.f, 0.8f));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"lenpitch",2}, "Length Sets Pitch", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{"beige",2}, "Beige Only", false));
    return l;
}

TenringsProcessor::TenringsProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "TENRINGS", layout()) {
    pMode = apvts.getRawParameterValue("mode"); pCam = apvts.getRawParameterValue("camera"); pSound = apvts.getRawParameterValue("sound");
    pMidi = apvts.getRawParameterValue("sendmidi"); pVport = apvts.getRawParameterValue("vport"); pKey = apvts.getRawParameterValue("key");
    pScale = apvts.getRawParameterValue("scale"); pEasy = apvts.getRawParameterValue("easy5"); pOct = apvts.getRawParameterValue("octave");
    pSens = apvts.getRawParameterValue("sens"); pHat = apvts.getRawParameterValue("hatrate"); pSlide = apvts.getRawParameterValue("slide");
    pVib = apvts.getRawParameterValue("vibrato"); pBend = apvts.getRawParameterValue("bend"); pVol = apvts.getRawParameterValue("volume");
    pLenPitch = apvts.getRawParameterValue("lenpitch"); pBeige = apvts.getRawParameterValue("beige");
    evScratch.reserve(256); uiScratch.reserve(64);
    for (auto id : {"camera","vport"}) apvts.addParameterListener(id, this);
    camera = CameraTracker::create(
        [this](const Frame& f) { onTrackingFrame(f); },
        [this](const uint8_t* px, int w, int h, int stride) {
            std::unique_lock<std::mutex> lk(previewMx, std::try_to_lock);
            if (!lk.owns_lock()) return;
            if (!preview.isValid() || preview.getWidth() != w || preview.getHeight() != h) preview = juce::Image(juce::Image::ARGB, w, h, false);
            juce::Image::BitmapData bd(preview, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < h; ++y) { auto* d = bd.getLinePointer(y); std::memcpy(d, px + (size_t)y * stride, (size_t)w * 4); for (int x = 0; x < w; ++x) d[x * 4 + 3] = 255; }
            previewFresh = true;
        });
    updateCamera();
}

TenringsProcessor::~TenringsProcessor() {
    alive->store(false);
    for (auto id : {"camera","vport"}) apvts.removeParameterListener(id, this);
    if (camera) camera->stop();
    camera.reset();
    const juce::SpinLock::ScopedLockType sl(vportLock); virtualOut.reset();
}

void TenringsProcessor::parameterChanged(const juce::String& id, float) {
    if (juce::MessageManager::existsAndIsCurrentThread()) { if (id == "camera") updateCamera(); else updateVirtualPort(); return; }
    juce::MessageManager::callAsync([alive = alive, this, id] {
        if (!alive->load()) return;
        if (id == "camera") updateCamera(); else updateVirtualPort();
    });
}

void TenringsProcessor::updateCamera() {
    if (!camera) return;
    if (pCam->load() > 0.5f) camera->start(); else { camera->stop(); engineResetReq = true; }
}

void TenringsProcessor::updateVirtualPort() {
    bool want = pVport->load() > 0.5f;
    const juce::SpinLock::ScopedLockType sl(vportLock);
    if (want && virtualOut == nullptr && !virtualPortStubOpen) {
#if JUCE_MAC
        virtualOut = juce::MidiOutput::createNewDevice("TENRINGS"); // CoreMIDI virtual source FL sees as a keyboard
#endif
        if (virtualOut == nullptr) virtualPortStubOpen = true;      // non-Mac: counted only (test hook)
    } else if (!want) { virtualOut.reset(); virtualPortStubOpen = false; }
}

bool TenringsProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto o = l.getMainOutputChannelSet();
    return o == juce::AudioChannelSet::stereo() || o == juce::AudioChannelSet::mono();
}

void TenringsProcessor::prepareToPlay(double s, int) {
    sr = s; synth.prepare(s); hat.sr = s; nPending = 0;
    vportBuf.ensureSize(4096);
}

Params TenringsProcessor::readParams() const {
    Params p;
    p.mode = (int)pMode->load(); p.key = (int)pKey->load(); p.scale = (int)pScale->load(); p.easy5 = pEasy->load() > 0.5f;
    p.octave = (int)std::lround(pOct->load()); p.sensitivity = pSens->load(); p.slide = pSlide->load(); p.vibrato = pVib->load(); p.bend = pBend->load();
    p.lengthPitch = pLenPitch->load() > 0.5f;
    return p;
}

void TenringsProcessor::onTrackingFrame(const Frame& f) {
    std::lock_guard<std::mutex> eg(engineMx);
    evScratch.clear();
    if (engineResetReq.exchange(false)) engine.allOff(evScratch);
    engine.setParams(readParams(), evScratch);
    if (pCam->load() > 0.5f) engine.process(f, evScratch);
    else engine.allOff(evScratch);
    for (auto& e : evScratch) trackQ.push(e);
    hatHold = engine.hatHold && engine.p.mode == Drum && pCam->load() > 0.5f;
    handsVisible = engine.handsVisible;
    lastFrameTime = f.t;
    std::unique_lock<std::mutex> lk(overlayMx, std::try_to_lock);
    if (lk.owns_lock()) {
        overlay.frame = f; overlay.labels = engine.labels; overlay.leadSlot = engine.leadSlot; overlay.leadNote = engine.leadNote; overlay.leadBend = engine.leadBendSemis;
        overlay.leadVol = engine.leadVolume; overlay.hatHold = hatHold; overlay.mode = engine.p.mode; overlayFresh = true;
    }
}

void TenringsProcessor::padHit(int note, int vel) { uiQ.push({EvType::NoteOn, (uint8_t)note, (uint8_t)vel, 0, true}); }

// ---- Strings mode, message thread ----
void TenringsProcessor::stringsUndo() { std::lock_guard<std::mutex> eg(engineMx); engine.strings.undo(); }
void TenringsProcessor::stringsClear() { std::lock_guard<std::mutex> eg(engineMx); engine.strings.clear(); }
bool TenringsProcessor::stringsAdd(Pt a, Pt b) { std::lock_guard<std::mutex> eg(engineMx); return engine.strings.add(a, b); }
int TenringsProcessor::mouseStrum(Pt from, Pt to, float speed) {
    if ((int)pMode->load() != Strings) return 0;
    std::lock_guard<std::mutex> eg(engineMx); uiScratch.clear();
    int c = engine.strings.strumSegment(from, to, speed, uiClock(), readParams(), uiScratch);
    for (auto& e : uiScratch) uiQ.push(e);
    return c;
}
int TenringsProcessor::mousePluck(Pt at) {
    if ((int)pMode->load() != Strings) return 0;
    std::lock_guard<std::mutex> eg(engineMx); uiScratch.clear();
    int c = engine.strings.pluckNear(at, uiClock(), readParams(), uiScratch);
    for (auto& e : uiScratch) uiQ.push(e);
    return c;
}
void TenringsProcessor::readStrings(StringField& out) { std::lock_guard<std::mutex> eg(engineMx); out = engine.strings; }
int TenringsProcessor::stringCount() { std::lock_guard<std::mutex> eg(engineMx); return engine.strings.n; }
int TenringsProcessor::stringNote(int i) { std::lock_guard<std::mutex> eg(engineMx); return i >= 0 && i < engine.strings.n ? engine.strings.noteOf(i, readParams()) : -1; }

bool TenringsProcessor::readOverlay(Overlay& o) { std::lock_guard<std::mutex> lk(overlayMx); o = overlay; bool f = overlayFresh; overlayFresh = false; return f; }
bool TenringsProcessor::readPreview(juce::Image& img) {
    std::lock_guard<std::mutex> lk(previewMx); if (!previewFresh || !preview.isValid()) return false;
    img = preview.createCopy(); previewFresh = false; return true;
}

juce::String TenringsProcessor::statusText() const {
    if (pCam->load() < 0.5f) return "Camera off - pads, piano roll and MIDI input still play";
    switch (cameraStatus()) {
        case CamStatus::NoPermission: return "No camera permission - System Settings > Privacy & Security > Camera > enable FL Studio. Pads still work.";
        case CamStatus::NoCamera: return "No camera found - pads, piano roll and MIDI input still play";
        case CamStatus::HostBlocked: return "This host app has no camera permission entry, so macOS won't let it use the camera. Pads and MIDI still work.";
        case CamStatus::Starting: return "Starting camera...";
        case CamStatus::Off: return "Camera stopped";
        case CamStatus::Running: break;
    }
    int h = handsVisible.load();
    juce::String fps = juce::String(camera ? camera->fps() : 0.0, 0) + " fps";
    if (h == 0) return "No hands in frame - silent (" + fps + ")";
    return juce::String(h) + (h == 1 ? " hand" : " hands") + " tracked (" + fps + ")";
}

void TenringsProcessor::emit(const Ev& e, int s, juce::MidiBuffer& out, bool sendMidi, bool sound) {
    juce::MidiMessage m;
    switch (e.type) {
        case EvType::NoteOn:
            if (!tracker.noteOn(e.note)) { auto off = juce::MidiMessage::noteOff(1, e.note); if (sendMidi) out.addEvent(off, s); vportBuf.addEvent(off, s); if (sound) synth.noteOff(e.note); }
            m = juce::MidiMessage::noteOn(1, e.note, (juce::uint8)std::max<int>(1, e.vel));
            if (sound) synth.noteOn(e.note, e.vel);
            if (e.autoOff) {
                for (int i = 0; i < nPending; ++i) if (pending[(size_t)i].note == e.note) { pending[(size_t)i] = pending[(size_t)--nPending]; break; } // re-hit: one note-off, for the new note
                if (nPending < (int)pending.size()) pending[(size_t)nPending++] = { e.note, std::max(1, (int)(sr * e.holdMs * 0.001)) + s };
            }
            break;
        case EvType::NoteOff:
            synth.noteOff(e.note);
            if (!tracker.noteOff(e.note)) return;
            m = juce::MidiMessage::noteOff(1, e.note); break;
        case EvType::Bend: m = juce::MidiMessage::pitchWheel(1, e.value); synth.pitchBend(e.value); break;
        case EvType::CC: m = juce::MidiMessage::controllerEvent(1, e.note, e.value); synth.cc(e.note, e.value); break;
    }
    if (sendMidi) out.addEvent(m, s);
    vportBuf.addEvent(m, s);
}

void TenringsProcessor::flushOutgoing(juce::MidiBuffer& out, bool toMidi) {
    tracker.allOff([&](int note) { auto off = juce::MidiMessage::noteOff(1, note); if (toMidi) out.addEvent(off, 0); vportBuf.addEvent(off, 0); });
    if (toMidi) out.addEvent(juce::MidiMessage::pitchWheel(1, 8192), 0);
    synth.allOff(); nPending = 0;
}

// Host bypass: silent output, every held note released, nothing new played, incoming MIDI not echoed.
void TenringsProcessor::processBlockBypassed(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    buf.clear(); vportBuf.clear();
    juce::MidiBuffer out;
    Ev e; while (trackQ.pop(e)) {} while (uiQ.pop(e)) {}
    if (!wasBypassed) { flushOutgoing(out, lastSend); engineResetReq = true; wasBypassed = true; }
    hat.process(false, 120, false, buf.getNumSamples(), [](int) {});
    midi.swapWith(out);
    if (pVport->load() > 0.5f && !vportBuf.isEmpty()) {
        const juce::SpinLock::ScopedTryLockType tl(vportLock);
        if (tl.isLocked()) { if (virtualOut) virtualOut->sendBlockOfMessagesNow(vportBuf); if (virtualOut || virtualPortStubOpen) virtualPortMessages += vportBuf.getNumEvents(); }
    }
}

void TenringsProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    wasBypassed = false;
    const int n = buf.getNumSamples();
    buf.clear();
    const int mode = (int)pMode->load();
    const bool sendMidi = pMidi->load() > 0.5f, sound = pSound->load() > 0.5f, cam = pCam->load() > 0.5f;
    synth.mode = mode; synth.bendRange = bendRangeFor(readParams());
    vportBuf.clear();
    juce::MidiBuffer out;

    double bpm = 120;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) if (*b > 0) bpm = *b;
    bpmSeen = bpm;

    auto flushAll = [&](bool toMidi) { flushOutgoing(out, toMidi); };
    // mode switch / camera off / MIDI-send off => everything silent immediately (no stuck notes)
    if (mode != lastMode) { if (lastMode >= 0) { flushAll(lastSend); engineResetReq = true; } lastMode = mode; }
    if (cam != lastCam) { if (!cam) { flushAll(lastSend); hatHold = false; } lastCam = cam; }
    if (sendMidi != lastSend) { if (!sendMidi) { tracker.allOff([&](int note) { out.addEvent(juce::MidiMessage::noteOff(1, note), 0); }); } lastSend = sendMidi; }
    if (sound != lastSound) { if (!sound) synth.allOff(); lastSound = sound; }

    // incoming MIDI (piano roll playback / keyboard) plays built-in sounds; never echoed to output (no feedback loops)
    juce::MidiBuffer incoming; incoming.swapWith(midi);

    Ev e;
    while (trackQ.pop(e)) emit(e, 0, out, sendMidi, sound);
    while (uiQ.pop(e)) if (mode == Drum || mode == Strings) emit(e, 0, out, sendMidi, sound);

    // hat roll (tempo-synced) – generated here so timing is sample-accurate
    hat.process(hatHold.load() && mode == Drum && cam, bpm, pHat->load() > 0.5f, n, [&](int s) {
        emit({EvType::NoteOn, (uint8_t)kCHat, 96, 0, true}, s, out, sendMidi, sound);
    });
    // scheduled drum note-offs
    for (int i = 0; i < nPending;) {
        pending[(size_t)i].samplesLeft -= n;
        if (pending[(size_t)i].samplesLeft <= 0) {
            int nt = pending[(size_t)i].note;
            if (tracker.noteOff(nt)) { auto off = juce::MidiMessage::noteOff(1, nt); if (sendMidi) out.addEvent(off, n - 1); vportBuf.addEvent(off, n - 1); }
            pending[(size_t)i] = pending[(size_t)--nPending];
        } else ++i;
    }

    // render with incoming MIDI sample-accurately
    float* L = buf.getWritePointer(0);
    const float gain = pVol->load() * 1.6f;
    int pos = 0;
    auto renderTo = [&](int end) { if (end > pos) { if (sound) synth.render(L + pos, end - pos, gain); pos = end; } };
    for (const auto md : incoming) {
        const auto& m = md.getMessage(); int s = juce::jlimit(0, n, md.samplePosition);
        renderTo(s);
        if (!sound) continue;
        if (m.isNoteOn()) synth.noteOn(m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff()) synth.noteOff(m.getNoteNumber());
        else if (m.isPitchWheel()) synth.pitchBend(m.getPitchWheelValue());
        else if (m.isController()) synth.cc(m.getControllerNumber(), m.getControllerValue());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allOff();
    }
    renderTo(n);
    for (int c = 1; c < buf.getNumChannels(); ++c) buf.copyFrom(c, 0, buf, 0, 0, n);

    midi.swapWith(out);
    if (pVport->load() > 0.5f && !vportBuf.isEmpty()) {
        const juce::SpinLock::ScopedTryLockType tl(vportLock);
        if (tl.isLocked()) {
            if (virtualOut) virtualOut->sendBlockOfMessagesNow(vportBuf);
            if (virtualOut || virtualPortStubOpen) virtualPortMessages += vportBuf.getNumEvents();
        }
    }
}

void TenringsProcessor::getStateInformation(juce::MemoryBlock& d) {
    auto x = apvts.copyState().createXml(); if (!x) return;
    x->deleteAllChildElementsWithTagName("STRINGS");
    auto* ss = x->createNewChildElement("STRINGS");
    { std::lock_guard<std::mutex> eg(engineMx);
      for (int i = 0; i < engine.strings.n; ++i) { const auto& st = engine.strings.s[(size_t)i]; auto* e = ss->createNewChildElement("S");
          e->setAttribute("ax", st.a.x); e->setAttribute("ay", st.a.y); e->setAttribute("bx", st.b.x); e->setAttribute("by", st.b.y);
          e->setAttribute("deg", st.seqDeg); e->setAttribute("col", st.colour); e->setAttribute("note", engine.strings.noteOf(i, readParams())); } }
    copyXmlToBinary(*x, d);
}
void TenringsProcessor::setStateInformation(const void* data, int size) {
    auto x = getXmlFromBinary(data, size);
    if (!x || !x->hasTagName(apvts.state.getType())) return;
    std::array<StringDef, kMaxStrings> defs{}; int n = 0;
    if (auto* ss = x->getChildByName("STRINGS"))
        for (auto* e : ss->getChildWithTagNameIterator("S")) {
            if (n >= kMaxStrings) break;
            defs[(size_t)n++] = { {(float)e->getDoubleAttribute("ax"), (float)e->getDoubleAttribute("ay")}, {(float)e->getDoubleAttribute("bx"), (float)e->getDoubleAttribute("by")},
                                  e->getIntAttribute("deg"), e->getIntAttribute("col") };
        }
    x->deleteAllChildElementsWithTagName("STRINGS");   // keep the parameter tree clean
    apvts.replaceState(juce::ValueTree::fromXml(*x));
    std::lock_guard<std::mutex> eg(engineMx); engine.strings.setAll(defs.data(), n);   // old projects (no STRINGS) start empty
}

juce::AudioProcessorEditor* TenringsProcessor::createEditor() { return new TenringsEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TenringsProcessor(); }
