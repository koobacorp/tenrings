#include "PluginEditor.h"
using namespace tr;
using namespace theme;

// Mode colours, deepened for the beige look: every one is >= 4.7:1 against the beige and carries white text at >= 6.4:1.
juce::Colour modeColour(int m) {
    switch (m) { case Drum: return juce::Colour(0xff9a3e00); case Keys: return juce::Colour(0xff006a61); case Lead: return juce::Colour(0xff6a2fb0); default: return juce::Colour(0xff276b19); }
}
// Neon string colours: each >= 3.4:1 against the beige so a string never disappears (brightened when drawn over the camera).
juce::Colour stringColour(int i) {
    static const juce::uint32 c[kMaxStrings] = {0xff00855f, 0xffc81e73, 0xff0070a8, 0xffb35f00, 0xff8a3fd9, 0xffc8202c, 0xff2f5be0, 0xff4f7d00};
    return juce::Colour(c[((i % kMaxStrings) + kMaxStrings) % kMaxStrings]);
}

static const char* drumName(int n) { switch (n) { case kKick: return "KICK"; case kSnare: return "SNARE"; case kPerc: return "PERC"; case kCHat: return "HAT ROLL"; case kOHat: return "OPEN HAT"; } return ""; }
static juce::String noteLabel(int n) { return juce::String(noteName(n)) + juce::String(n / 12 - 1); }

static constexpr double kWobbleLife = 0.9, kSparkLife = 0.6;

void CameraView::ingestFx(double t) {
    if (sf.n < (int)wob.size()) for (int i = sf.n; i < (int)wob.size(); ++i) wob[(size_t)i] = {};
    if (sf.fxSeq < seenFx) seenFx = sf.fxSeq;
    const float W = (float)getWidth(), H = (float)getHeight();
    uint32_t first = std::max(seenFx + 1, sf.fxSeq > sf.fx.size() ? sf.fxSeq - (uint32_t)sf.fx.size() + 1 : 1u);
    for (uint32_t q = first; q <= sf.fxSeq; ++q) {
        const auto& f = sf.fx[(size_t)((q - 1) % sf.fx.size())];
        if (f.seq != q || f.str < 0 || f.str >= sf.n) continue;
        const auto& st = sf.s[(size_t)f.str];
        wob[(size_t)f.str] = { t, f.u, 4.f + 10.f * f.vel / 127.f };
        const float px = (st.a.x + (st.b.x - st.a.x) * f.u) * W, py = (st.a.y + (st.b.y - st.a.y) * f.u) * H;
        const int count = 10 + f.vel / 10;
        for (int k = 0; k < count; ++k) {
            const float ang = rnd.nextFloat() * juce::MathConstants<float>::twoPi, sp = 60.f + rnd.nextFloat() * (120.f + 2.f * f.vel);
            sparks.push_back({px, py, std::cos(ang) * sp, std::sin(ang) * sp - 40.f, t + rnd.nextFloat() * 0.05, st.colour});
        }
    }
    seenFx = sf.fxSeq;
    sparks.erase(std::remove_if(sparks.begin(), sparks.end(), [t](const Spark& s) { return t - s.t0 > kSparkLife; }), sparks.end());
    if (sparks.size() > 600) sparks.erase(sparks.begin(), sparks.begin() + (long)(sparks.size() - 600));
}
bool CameraView::animating(double t) const {
    if (!sparks.empty() || sf.phase != StringField::Idle) return true;
    for (int i = 0; i < sf.n; ++i) if (t - wob[(size_t)i].t0 < kWobbleLife) return true;
    return false;
}

void CameraView::paintSkeleton(juce::Graphics& g, float W, float H, juce::Colour c) {
    for (int h = 0; h < 2; ++h) {
        const auto& hd = ov.frame.hand[(size_t)h]; if (!hd.present) continue;
        auto P = [&](Pt p) { return juce::Point<float>(p.x * W, p.y * H); };
        g.setColour(c.withAlpha(0.85f));
        for (int i = 0; i < 5; ++i) {
            if (!hd.tipOk[(size_t)i]) continue;
            juce::Path pth; pth.startNewSubPath(P(hd.palm)); pth.lineTo(P(hd.mcp[(size_t)i]));
            if (hd.hasPip[(size_t)i]) pth.lineTo(P(hd.pip[(size_t)i]));
            pth.lineTo(P(hd.tip[(size_t)i]));
            g.strokePath(pth, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            auto t = P(hd.tip[(size_t)i]); g.fillEllipse(t.x - 4.5f, t.y - 4.5f, 9.f, 9.f);
        }
        juce::Path knuckles; bool firstK = true;
        for (int i = 1; i < 5; ++i) { auto k = P(hd.mcp[(size_t)i]); if (firstK) knuckles.startNewSubPath(k); else knuckles.lineTo(k); firstK = false; }
        g.strokePath(knuckles, juce::PathStrokeType(2.f));
        auto pc = P(hd.palm); g.drawEllipse(pc.x - 7, pc.y - 7, 14, 14, 2.5f);
    }
}

void CameraView::paintStrings(juce::Graphics& g, float W, float H, bool onCam) {
    const double t = now();
    const Params prm = proc.currentParams();
    const auto ink = onCam ? juce::Colours::white : theme::ink;
    for (int i = 0; i < sf.n; ++i) {
        const auto& st = sf.s[(size_t)i];
        const auto base = stringColour(st.colour);
        const auto core = onCam ? base.brighter(0.9f) : base;
        const juce::Point<float> a(st.a.x * W, st.a.y * H), b(st.b.x * W, st.b.y * H);
        const auto d = b - a; const float len = std::max(1.f, d.getDistanceFromOrigin());
        const juce::Point<float> nrm(-d.y / len, d.x / len);
        const auto& w = wob[(size_t)i]; const double age = t - w.t0;
        const float live = age >= 0 && age < kWobbleLife ? (float)std::exp(-age * 5.0) : 0.f;
        juce::Path pth;
        constexpr int N = 48;
        for (int k = 0; k <= N; ++k) {
            const float u = (float)k / N;
            float shape = live > 0 ? (u < w.u ? u / std::max(0.02f, w.u) : (1.f - u) / std::max(0.02f, 1.f - w.u)) : 0.f;
            const float disp = live * w.amp * shape * (float)std::cos(juce::MathConstants<double>::twoPi * 9.0 * age);
            const auto p = a + d * u + nrm * disp;
            if (k == 0) pth.startNewSubPath(p); else pth.lineTo(p);
        }
        const float flash = live > 0 ? (float)std::exp(-age * 9.0) : 0.f;
        g.setColour(base.withAlpha(0.16f + 0.3f * flash)); g.strokePath(pth, juce::PathStrokeType(12.f + 10.f * flash, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(base.withAlpha(0.38f + 0.4f * flash)); g.strokePath(pth, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(flash > 0.3f ? core.brighter(0.6f) : core); g.strokePath(pth, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // tick marks along the string + end caps
        g.setColour(core);
        for (int k = 1; k < 8; ++k) { const auto c = a + d * (k / 8.f); g.drawLine(juce::Line<float>(c - nrm * 4.f, c + nrm * 4.f), 1.6f); }
        for (auto e : {a, b}) { g.fillEllipse(e.x - 4.f, e.y - 4.f, 8.f, 8.f); g.setColour(ink.withAlpha(0.8f)); g.drawEllipse(e.x - 4.f, e.y - 4.f, 8.f, 8.f, 1.2f); g.setColour(core); }
        // note name in a small pill at the left end
        const auto left = a.x <= b.x ? a : b;
        juce::Rectangle<float> pill(left.x - 40.f, left.y - 11.f, 34.f, 20.f);
        pill.setX(juce::jlimit(2.f, W - 36.f, pill.getX())); pill.setY(juce::jlimit(2.f, H - 22.f, pill.getY()));
        g.setColour((onCam ? juce::Colours::black : theme::panel).withAlpha(0.85f)); g.fillRoundedRectangle(pill, 6.f);
        g.setColour(onCam ? core : base); g.drawRoundedRectangle(pill, 6.f, 1.5f);
        g.setColour(onCam ? juce::Colours::white : theme::ink); g.setFont(juce::FontOptions(12.f, juce::Font::bold));
        g.drawText(noteLabel(sf.noteOf(i, prm)), pill, juce::Justification::centred);
    }
    // sparks in each string's colour
    for (const auto& s : sparks) {
        const float age = (float)(t - s.t0); if (age < 0 || age > kSparkLife) continue;
        const float k = 1.f - age / (float)kSparkLife;
        const float x = s.x + s.vx * age, y = s.y + s.vy * age + 0.5f * 260.f * age * age;
        const auto c = stringColour(s.col);
        g.setColour((onCam ? c.brighter(1.f) : c).withAlpha(k));
        g.drawLine(x, y, x - s.vx * 0.025f, y - s.vy * 0.025f, 1.5f + 1.5f * k);
        g.fillEllipse(x - 2.f * k - 1.f, y - 2.f * k - 1.f, 4.f * k + 2.f, 4.f * k + 2.f);
    }
    // string being made: dotted line between the index tips (+ hold ring while touching)
    const bool mousePreview = creating && dragged;
    if (sf.phase != StringField::Idle || mousePreview) {
        juce::Point<float> a(sf.pa.x * W, sf.pa.y * H), b(sf.pb.x * W, sf.pb.y * H);
        if (mousePreview) { a = downPos; b = lastPos; }
        juce::Path line, dashed; line.startNewSubPath(a); line.lineTo(b);
        const float dashes[] = {7.f, 6.f};
        juce::PathStrokeType(3.f).createDashedStroke(dashed, line, dashes, 2);
        if (onCam) { g.setColour(juce::Colours::black.withAlpha(0.6f)); g.strokePath(line, juce::PathStrokeType(5.f)); }
        g.setColour(onCam ? juce::Colours::white : theme::ink); g.fillPath(dashed);
        const auto mid = (a + b) * 0.5f;
        if (sf.phase == StringField::Touch) {
            juce::Path ring; ring.addCentredArc(mid.x, mid.y, 16.f, 16.f, 0.f, 0.f, juce::MathConstants<float>::twoPi * sf.holdProgress(), true);
            g.setColour(modeColour(Strings)); g.strokePath(ring, juce::PathStrokeType(4.f));
        } else {
            const bool longEnough = sdist({a.x / W, a.y / H}, {b.x / W, b.y / H}) >= kMinStringLen;
            g.setFont(juce::FontOptions(13.f, juce::Font::bold)); g.setColour(onCam ? juce::Colours::white : theme::ink);
            g.drawText(longEnough ? (mousePreview ? "release to set" : "hold still to set") : "pull apart", juce::Rectangle<float>(mid.x - 70, mid.y - 30, 140, 18), juce::Justification::centred);
        }
    }
}

void CameraView::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat(); const float W = r.getWidth(), H = r.getHeight();
    const int mode = (int)proc.apvts.getRawParameterValue("mode")->load();
    const auto col = modeColour(mode);
    const bool onCam = img.isValid() && !beigeOnly;
    const auto ink = onCam ? juce::Colours::white : theme::ink;
    g.fillAll(theme::beige);
    if (onCam) g.drawImage(img, r, juce::RectanglePlacement::stretchToFit);
    auto X = [&](float x) { return x * W; }; auto Y = [&](float y) { return y * H; };
    if (mode != Strings) {   // center dead zone (face); Strings only listens to fingertips, so no dead zone there
        g.setColour(ink.withAlpha(0.07f)); g.fillRect(X(kDeadLo), 0.f, X(kDeadHi) - X(kDeadLo), H);
        g.setColour(ink.withAlpha(0.6f)); g.setFont(13.f);
        g.drawText("dead zone", juce::Rectangle<float>(X(kDeadLo), H - 22, X(kDeadHi) - X(kDeadLo), 18), juce::Justification::centred);
    }
    const bool flash = juce::Time::getMillisecondCounterHiRes() - flashT < 120;
    if (mode == Drum) {
        struct Pad { juce::Rectangle<float> rc; int note; };
        Pad pads[] = { {{X(kDeadHi), 0, W - X(kDeadHi), Y(kRightSplitY)}, kSnare}, {{X(kDeadHi), Y(kRightSplitY), W - X(kDeadHi), H - Y(kRightSplitY)}, kKick},
                       {{0, 0, X(kHatEdge), H}, kCHat}, {{X(kHatEdge), 0, X(kDeadLo) - X(kHatEdge), Y(kLeftUpperY)}, kOHat},
                       {{X(kHatEdge), Y(kLeftUpperY), X(kDeadLo) - X(kHatEdge), H - Y(kLeftUpperY)}, kPerc} };
        for (auto& p : pads) {
            bool lit = (flash && flashNote == p.note) || (p.note == kCHat && ov.hatHold);
            g.setColour(col.withAlpha(lit ? 0.85f : 0.12f)); g.fillRoundedRectangle(p.rc.reduced(4), 10);
            g.setColour(col); g.drawRoundedRectangle(p.rc.reduced(4), 10, 2.5f);
            g.setFont(juce::FontOptions(p.note == kKick || p.note == kSnare ? 26.f : 17.f, juce::Font::bold));
            g.setColour(lit ? juce::Colours::white : ink); g.drawText(drumName(p.note), p.rc, juce::Justification::centred);
        }
    } else if (mode == Lead) {
        for (int i = 0; i < Engine::kSlots; ++i) {
            float x0 = X(Engine::kLeadX0 + i * Engine::kSlotW);
            bool on = ov.leadNote >= 0 && ov.leadSlot == i;
            g.setColour(col.withAlpha(on ? 0.5f : 0.12f)); g.fillRect(x0 + 1, 0.f, X(Engine::kSlotW) - 2, H);
        }
        g.setColour(col.withAlpha(0.15f)); g.fillRect(0.f, 0.f, X(kDeadLo), H);
        g.setColour(ink); g.setFont(juce::FontOptions(15.f, juce::Font::bold));
        g.drawText("VOLUME (bow)", juce::Rectangle<float>(0, 8, X(kDeadLo), 20), juce::Justification::centred);
        g.drawText("MOVE, LAND = NOTE / UP = BEND", juce::Rectangle<float>(X(Engine::kLeadX0), 8, W - X(Engine::kLeadX0), 20), juce::Justification::centred);
        if (ov.leadNote >= 0) {
            g.setFont(juce::FontOptions(30.f, juce::Font::bold)); g.setColour(onCam ? col.brighter(0.8f) : col);
            g.drawText(noteLabel(ov.leadNote) + (std::abs(ov.leadBend) > 0.05f ? juce::String(ov.leadBend, 2) : juce::String()),
                       juce::Rectangle<float>(X(kDeadLo), 30, X(kDeadHi) - X(kDeadLo), 40), juce::Justification::centred);
            float vh = ov.leadVol / 127.f * (H - 60); g.setColour(col); g.fillRect(8.f, H - 20 - vh, 14.f, vh);
        }
    } else if (mode == Strings) {
        paintStrings(g, W, H, onCam);
    }
    // hands: skeleton outlines when the camera picture is hidden (and always in Strings), fingertips per mode
    if (!onCam || mode == Strings) paintSkeleton(g, W, H, mode == Strings ? (onCam ? juce::Colours::white : theme::muted) : col);
    for (int h = 0; h < 2 && mode != Strings; ++h) {
        const auto& hd = ov.frame.hand[(size_t)h]; if (!hd.present) continue;
        g.setColour(col.withAlpha(0.9f)); g.drawEllipse(X(hd.palm.x) - 14, Y(hd.palm.y) - 14, 28, 28, 3);
        for (int i = 0; i < 5; ++i) {
            if (!hd.valid[(size_t)i]) continue;
            auto t = hd.tip[(size_t)i];
            if (mode == Keys) {
                const auto& lb = ov.labels[(size_t)h][(size_t)i];
                g.setColour(lb.down ? col : col.withAlpha(0.35f)); g.fillEllipse(X(t.x) - 13, Y(t.y) - 13, 26, 26);
                if (lb.note >= 0) { g.setColour(lb.down ? juce::Colours::white : ink); g.setFont(juce::FontOptions(12.f, juce::Font::bold));
                    g.drawText(noteLabel(lb.note), juce::Rectangle<float>(X(t.x) - 18, Y(t.y) - 8, 36, 16), juce::Justification::centred); }
            } else { g.setColour(ink.withAlpha(0.8f)); g.fillEllipse(X(t.x) - 3, Y(t.y) - 3, 6, 6); }
        }
    }
    if (mode == Keys) {
        g.setColour(onCam ? juce::Colours::black.withAlpha(0.85f) : theme::panel); g.fillRect(0.f, 0.f, W, 48.f);
        g.setColour(ink); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
        g.drawText("AIR PIANO - hands flat, palms down", juce::Rectangle<float>(4, 4, W - 8, 20), juce::Justification::centred);
        g.setFont(11.f);
        g.drawText("Angle camera down. Hold fingers at rest, press one down, lift to release.", juce::Rectangle<float>(4, 25, W - 8, 18), juce::Justification::centred);
        const char* fingers[] = {"TH", "IN", "MID", "RNG", "PK"};
        // Two independent banks, spatially ordered like a piano (left pinky -> right pinky).
        float kw = (W - 24.f) / 10.f, kh = std::min(106.f, H * 0.27f);
        g.setColour(theme::keybed); g.fillRect(0.f, H - kh - 24.f, W, kh + 24.f);
        for (int h = 0; h < 2; ++h) {
            g.setColour(juce::Colours::ivory); g.setFont(11.f);
            g.drawText(h == 0 ? "LEFT HAND" : "RIGHT HAND", juce::Rectangle<float>(h * W * 0.5f, H - kh - 22.f, W * 0.5f, 18.f), juce::Justification::centred);
            for (int j = 0; j < 5; ++j) {
                int i = h == 0 ? 4 - j : j;
                const auto& lb = ov.labels[h][i];
                bool active = lb.note >= 0 && ov.frame.hand[h].present;
                bool down = active && lb.down;
                juce::Rectangle<float> key(8.f + (h * 5 + j) * kw + h * 8.f, H - kh, kw - 3.f, kh - 5.f);
                g.setColour(down ? col : active ? juce::Colours::ivory : juce::Colour(0xff4a4237));
                g.fillRoundedRectangle(key, 4.f);
                g.setColour(down ? juce::Colours::white : active ? juce::Colours::black : juce::Colours::white.withAlpha(0.75f));
                g.setFont(juce::FontOptions(13.f, juce::Font::bold));
                g.drawText(active ? noteLabel(lb.note) : "-", key.reduced(1.f).withHeight(kh * 0.55f), juce::Justification::centred);
                g.setFont(10.f); g.drawText(fingers[i], key.withTrimmedTop(kh * 0.6f), juce::Justification::centred);
            }
        }
    }
    if (mode == Strings) {
        juce::Rectangle<float> bar(0.f, 0.f, W, 26.f);
        g.setColour(onCam ? juce::Colours::black.withAlpha(0.7f) : theme::panel.withAlpha(0.92f)); g.fillRect(bar);
        g.setColour(ink); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
        g.drawText(sf.n >= kMaxStrings ? "8 / 8 strings - Undo or Clear to make room. Swipe a fingertip through a string to pluck."
                                       : "Touch index fingertips, pull apart, hold still = new string. Swipe through to pluck.  (" + juce::String(sf.n) + " / 8)",
                   bar.reduced(6.f, 0.f), juce::Justification::centred);
    }
    g.setColour(col); g.drawRect(getLocalBounds(), 3);
}

void CameraView::mouseDown(const juce::MouseEvent& e) {
    const int mode = (int)proc.apvts.getRawParameterValue("mode")->load();
    downPos = lastPos = e.position; lastMs = juce::Time::getMillisecondCounterHiRes(); dragged = false;
    creating = mode == Strings && (e.mods.isPopupMenu() || e.mods.isShiftDown() || e.mods.isCommandDown());
    if (mode != Drum) return;
    Pt p{ e.position.x / (float)getWidth(), e.position.y / (float)getHeight() };
    int note = Engine::padAt(p);
    if (note >= 0) { proc.padHit(note); flashNote = note; flashT = juce::Time::getMillisecondCounterHiRes(); repaint(); }
}
// Strings with the mouse: drag across strings = strum, click on a string = pluck, shift/right-drag = draw a new string.
void CameraView::mouseDrag(const juce::MouseEvent& e) {
    if ((int)proc.apvts.getRawParameterValue("mode")->load() != Strings) return;
    if (e.position.getDistanceFrom(downPos) > 4.f) dragged = true;
    const float W = (float)getWidth(), H = (float)getHeight();
    if (!creating) {
        const double ms = juce::Time::getMillisecondCounterHiRes();
        const Pt a{lastPos.x / W, lastPos.y / H}, b{e.position.x / W, e.position.y / H};
        const float speed = sdist(a, b) / (float)std::max(0.008, (ms - lastMs) * 0.001);
        proc.mouseStrum(a, b, std::max(speed, 0.6f));
        lastMs = ms;
    }
    lastPos = e.position; repaint();
}
void CameraView::mouseUp(const juce::MouseEvent& e) {
    if ((int)proc.apvts.getRawParameterValue("mode")->load() != Strings) { creating = false; return; }
    const float W = (float)getWidth(), H = (float)getHeight();
    if (creating && dragged) proc.stringsAdd({downPos.x / W, downPos.y / H}, {e.position.x / W, e.position.y / H});
    else if (!dragged) proc.mousePluck({e.position.x / W, e.position.y / H});
    creating = false; dragged = false; repaint();
}

TenringsEditor::TenringsEditor(TenringsProcessor& p) : AudioProcessorEditor(&p), view(p), proc(p) {
    setLookAndFeel(&lnf);
    const char* names[4] = {"DRUM", "KEYBOARD", "LEAD", "STRINGS"};
    for (int i = 0; i < 4; ++i) {
        modeBtn[i].setButtonText(names[i]); modeBtn[i].setClickingTogglesState(false);
        modeBtn[i].onClick = [this, i] { if (auto* prm = proc.apvts.getParameter("mode")) { prm->beginChangeGesture(); prm->setValueNotifyingHost(prm->convertTo0to1((float)i)); prm->endChangeGesture(); } };
        addAndMakeVisible(modeBtn[i]);
    }
    addAndMakeVisible(view);
    auto& s = proc.apvts;
    for (auto [b, id] : std::initializer_list<std::pair<juce::ToggleButton*, const char*>>{{&camBtn,"camera"},{&beigeBtn,"beige"},{&soundBtn,"sound"},{&midiBtn,"sendmidi"},{&vportBtn,"vport"},{&easyBtn,"easy5"},{&lenPitchBtn,"lenpitch"}}) {
        addAndMakeVisible(*b); bAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(s, id, *b)); }
    keyBox.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 1);
    scaleBox.addItemList({"Major","Minor"}, 1); hatBox.addItemList({"Hats 1/8","Hats 1/16"}, 1);
    for (auto [c, id] : std::initializer_list<std::pair<juce::ComboBox*, const char*>>{{&keyBox,"key"},{&scaleBox,"scale"},{&hatBox,"hatrate"}}) {
        addAndMakeVisible(*c); cAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(s, id, *c)); }
    auto addSl = [&](juce::Slider& sl, const char* id, const char* label) {
        sl.setSliderStyle(juce::Slider::LinearHorizontal); sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 22);
        addAndMakeVisible(sl); sAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, id, sl));
        auto* l = labels.add(new juce::Label({}, label)); l->attachToComponent(&sl, true); l->setColour(juce::Label::textColourId, theme::ink); };
    addSl(octSl, "octave", "Octave"); addSl(sensSl, "sens", "Sensitivity"); addSl(slideSl, "slide", "Slide");
    addSl(vibSl, "vibrato", "Vibrato"); addSl(bendSl, "bend", "Bend"); addSl(volSl, "volume", "Volume");
    undoBtn.onClick = [this] { proc.stringsUndo(); refresh(); };
    clearBtn.onClick = [this] { proc.stringsClear(); refresh(); };
    addAndMakeVisible(undoBtn); addAndMakeVisible(clearBtn);
    for (auto* l : {&status, &stringsInfo}) { l->setColour(juce::Label::textColourId, theme::ink); addAndMakeVisible(*l); }
    status.setFont(juce::Font(juce::FontOptions(15.f, juce::Font::bold))); stringsInfo.setFont(juce::Font(juce::FontOptions(14.f, juce::Font::bold)));
    setResizable(true, true); setResizeLimits(720, 480, 2400, 1600); setSize(1000, 680);
    applyColours();
    startTimerHz(60);
}
TenringsEditor::~TenringsEditor() { setLookAndFeel(nullptr); }

void TenringsEditor::applyColours() {
    int m = (int)proc.apvts.getRawParameterValue("mode")->load(); shownMode = m;
    auto c = modeColour(m);
    lnf.setColourScheme({ beige, panel, panel, outline, ink, c, juce::Colours::white, c, ink });
    for (int i = 0; i < 4; ++i) {
        auto mc = modeColour(i);
        modeBtn[i].setColour(juce::TextButton::buttonColourId, i == m ? mc : panel);
        modeBtn[i].setColour(juce::TextButton::textColourOffId, i == m ? juce::Colours::white : mc);
        modeBtn[i].setColour(juce::ComboBox::outlineColourId, i == m ? mc : outline);
    }
    for (juce::Slider* sl : {&octSl, &sensSl, &slideSl, &vibSl, &bendSl, &volSl}) {
        sl->setColour(juce::Slider::thumbColourId, c); sl->setColour(juce::Slider::trackColourId, c.withAlpha(0.85f));
        sl->setColour(juce::Slider::backgroundColourId, outline.withAlpha(0.35f));
        sl->setColour(juce::Slider::textBoxTextColourId, ink); sl->setColour(juce::Slider::textBoxBackgroundColourId, panel); sl->setColour(juce::Slider::textBoxOutlineColourId, outline);
    }
    for (juce::ToggleButton* b : {&camBtn, &beigeBtn, &soundBtn, &midiBtn, &vportBtn, &easyBtn, &lenPitchBtn}) {
        b->setColour(juce::ToggleButton::tickColourId, c); b->setColour(juce::ToggleButton::textColourId, ink); b->setColour(juce::ToggleButton::tickDisabledColourId, outline); }
    for (juce::ComboBox* cb : {&keyBox, &scaleBox, &hatBox}) {
        cb->setColour(juce::ComboBox::backgroundColourId, panel); cb->setColour(juce::ComboBox::textColourId, ink);
        cb->setColour(juce::ComboBox::outlineColourId, outline); cb->setColour(juce::ComboBox::arrowColourId, ink); }
    for (juce::TextButton* b : {&undoBtn, &clearBtn}) {
        b->setColour(juce::TextButton::buttonColourId, panel); b->setColour(juce::TextButton::textColourOffId, c); b->setColour(juce::ComboBox::outlineColourId, c); }
    // per-mode controls
    bool melodic = m != Drum;
    keyBox.setEnabled(melodic); scaleBox.setEnabled(melodic); easyBtn.setEnabled(melodic); octSl.setEnabled(melodic);
    hatBox.setVisible(m == Drum); slideSl.setVisible(m == Lead); vibSl.setVisible(m == Lead); bendSl.setVisible(m == Lead);
    for (juce::Component* k : std::initializer_list<juce::Component*>{&lenPitchBtn, &undoBtn, &clearBtn, &stringsInfo}) k->setVisible(m == Strings);
    resized(); repaint();
}

void TenringsEditor::paint(juce::Graphics& g) {
    g.fillAll(beige);
    auto c = modeColour(shownMode);
    g.setColour(c); g.fillRect(0, 0, getWidth(), 5);
    g.setFont(juce::FontOptions(26.f, juce::Font::bold)); g.drawText("TENRINGS", 14, 10, 200, 40, juce::Justification::centredLeft);
}

void TenringsEditor::resized() {
    auto r = getLocalBounds().reduced(10); auto top = r.removeFromTop(48); top.removeFromLeft(170);
    for (int i = 0; i < 4; ++i) modeBtn[i].setBounds(top.removeFromLeft(top.getWidth() / (4 - i)).reduced(4));
    status.setBounds(r.removeFromBottom(28));
    auto side = r.removeFromRight(juce::jmax(300, r.getWidth() / 3)); r.removeFromRight(8);
    view.setBounds(r);
    const int rowHeight = std::clamp(side.getHeight() / 17, 21, 32);
    auto row = [&](int h = 0) { return side.removeFromTop(h ? h : rowHeight).reduced(2); };
    camBtn.setBounds(row()); beigeBtn.setBounds(row()); soundBtn.setBounds(row()); midiBtn.setBounds(row()); vportBtn.setBounds(row());
    side.removeFromTop(6);
    auto kr = row(); keyBox.setBounds(kr.removeFromLeft(kr.getWidth() / 2).reduced(2)); scaleBox.setBounds(kr.reduced(2));
    easyBtn.setBounds(row());
    for (juce::Slider* sl : {&octSl, &sensSl, &volSl}) { auto rr = row(rowHeight + 2); rr.removeFromLeft(90); sl->setBounds(rr); }
    if (hatBox.isVisible()) hatBox.setBounds(row(rowHeight + 2));
    for (juce::Slider* sl : {&slideSl, &vibSl, &bendSl}) if (sl->isVisible()) { auto rr = row(rowHeight + 2); rr.removeFromLeft(90); sl->setBounds(rr); }
    if (lenPitchBtn.isVisible()) {
        side.removeFromTop(4);
        lenPitchBtn.setBounds(row());
        auto br = row(rowHeight + 6); undoBtn.setBounds(br.removeFromLeft(br.getWidth() / 2).reduced(2)); clearBtn.setBounds(br.reduced(2));
        stringsInfo.setBounds(row(rowHeight * 2));
    }
}

void TenringsEditor::refresh() {
    if ((int)proc.apvts.getRawParameterValue("mode")->load() != shownMode) applyColours();
    bool dirty = proc.readOverlay(view.ov);
    dirty |= proc.readPreview(view.img);
    const bool beige = proc.apvts.getRawParameterValue("beige")->load() > 0.5f;
    dirty |= beige != view.beigeOnly; view.beigeOnly = beige;
    const uint32_t oldSeq = view.sf.fxSeq; const int oldN = view.sf.n; const int oldPhase = view.sf.phase;
    proc.readStrings(view.sf);
    dirty |= view.sf.fxSeq != oldSeq || view.sf.n != oldN || (int)view.sf.phase != oldPhase;
    const double t = view.now();
    view.ingestFx(t);
    status.setText(proc.statusText(), juce::dontSendNotification);
    if (shownMode == Strings) {
        const auto prm = proc.currentParams();
        juce::String info = "Strings: " + juce::String(view.sf.n) + " / 8";
        if (view.sf.n < kMaxStrings) {
            StringField probe = view.sf; probe.add({0.f, 0.f}, {1.f, 0.f});
            info += prm.lengthPitch ? "   next: set by length" : "   next note: " + noteLabel(probe.noteOf(probe.n - 1, prm));
        }
        stringsInfo.setText(info, juce::dontSendNotification);
        undoBtn.setEnabled(view.sf.n > 0); clearBtn.setEnabled(view.sf.n > 0);
    }
    if (dirty || view.animating(t) || juce::Time::getMillisecondCounterHiRes() - view.flashT < 200) view.repaint();
}
void TenringsEditor::timerCallback() { refresh(); }
