// TENRINGS core engine: pure C++ (no JUCE), tested on Linux.
// Coordinates: normalized, MIRRORED like a mirror. x=0 screen-left (user's left hand side), y=0 top.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tr {

enum Mode { Drum = 0, Keys = 1, Lead = 2, Strings = 3 };
enum HandId { LeftHand = 0, RightHand = 1 };

struct Pt { float x = 0, y = 0; };
struct Hand {
    bool present = false; bool pianoBasis = true; Pt palm;
    std::array<Pt,5> tip{}, mcp{}, pip{};
    // Missing/occluded joints must release that finger, not masquerade as a press.
    std::array<bool,5> valid{true,true,true,true,true}, hasPip{};
    std::array<bool,5> tipOk{true,true,true,true,true}; // fingertip itself was seen (Strings mode only needs the tip)
}; // fingers: thumb,index,middle,ring,pinky
struct Frame { std::array<Hand,2> hand{}; double t = 0; };

// MIDI note numbers for the kit
constexpr int kKick = 36, kSnare = 38, kPerc = 39, kCHat = 42, kOHat = 46;

// Zones (mirrored coords)
constexpr float kDeadLo = 0.40f, kDeadHi = 0.60f;   // center dead zone (face/chest)
constexpr float kHatEdge = 0.15f;                    // far-left hold zone for hat roll
constexpr float kRightSplitY = 0.50f;                // right hand: above = snare, below = kick
constexpr float kLeftUpperY = 0.33f;                 // left hand: above = open hat
constexpr float kReboundWindow = 0.30f;              // s: opposite-direction motion after a hit is a return, not a hit

enum class EvType : uint8_t { NoteOn, NoteOff, Bend, CC };
struct Ev {
    EvType type = EvType::NoteOn;
    uint8_t note = 0, vel = 0;
    int value = 0;          // bend 0..16383, CC value
    bool autoOff = false;   // drum one-shots: audio thread sends the note-off
    uint16_t holdMs = 30;   // autoOff: how long the MIDI note is held before its note-off
};

struct Params {
    int mode = Drum;
    int key = 0;            // 0=C..11=B
    int scale = 1;          // 0 major, 1 minor
    bool easy5 = false;     // pentatonic
    int octave = 0;         // -2..+2
    float sensitivity = 0.5f; // 0..1, higher = easier trigger
    float slide = 0.5f, vibrato = 0.5f, bend = 1.0f; // 0..1
    bool lengthPitch = false; // Strings: longer string = lower note (snapped to scale)
};

// ---------------- scales ----------------
inline const std::vector<int>& scaleSteps(int scale, bool easy5) {
    static const std::vector<int> major{0,2,4,5,7,9,11}, minor{0,2,3,5,7,8,10},
                                  majP{0,2,4,7,9}, minP{0,3,5,7,10};
    if (easy5) return scale == 0 ? majP : minP;
    return scale == 0 ? major : minor;
}
inline int floorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
// degree 0 = root in octave 4 (C4=60) + octave shift
inline int degreeToNote(int deg, const Params& p) {
    const auto& s = scaleSteps(p.scale, p.easy5);
    int n = (int)s.size();
    int o = floorDiv(deg, n), i = deg - o * n;
    int note = 60 + p.key + 12 * (p.octave + o) + s[(size_t)i];
    return std::clamp(note, 0, 127);
}
inline const char* noteName(int n) {
    static const char* nm[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return nm[((n % 12) + 12) % 12];
}
// Pitch-bend range (semitones) for Lead: 2, or the largest scale step if bigger (pentatonic minor needs 3).
inline int bendRangeFor(const Params& p) {
    const auto& s = scaleSteps(p.scale, p.easy5);
    int mx = 0; for (size_t i = 0; i < s.size(); ++i) { int nx = (i + 1 < s.size() ? s[i+1] : s[0] + 12); mx = std::max(mx, nx - s[i]); }
    return std::max(2, mx);
}

// ---------------- hat roll (runs on audio thread, cheap) ----------------
struct HatRoll {
    double sr = 48000; double phase = 0; bool wasHolding = false;
    // returns sample offsets of hat hits within this block
    template <class F> void process(bool holding, double bpm, bool sixteenth, int numSamples, F&& onHit) {
        if (bpm <= 0 || !std::isfinite(bpm)) bpm = 120;
        double interval = sr * 60.0 / bpm * (sixteenth ? 0.25 : 0.5);
        if (!holding) { wasHolding = false; return; }
        if (!wasHolding) { wasHolding = true; phase = 1.0; } // first hit immediately
        const double inc = 1.0 / interval; // phase in grid units: tempo changes keep position
        for (int i = 0; i < numSamples; ++i) {
            if (phase >= 1.0 - 1e-9) { phase -= 1.0; if (phase < 0) phase = 0; onHit(i); }
            phase += inc;
        }
    }
};

// ---------------- Strings mode: air strings you draw and pluck (runs on tracking thread) ----------------
constexpr int kMaxStrings = 8;
constexpr float kAspect = 16.f / 9.f;      // camera picture is ~16:9: distances are measured in picture-height units
constexpr float kTouchDist = 0.05f;        // both index tips this close = "touching"
constexpr float kTouchHold = 0.15f;        // s they must stay touching before a string starts (no accidental strings)
constexpr float kMinStringLen = 0.12f;     // shorter pulls are cancelled
constexpr float kStillSpeed = 0.45f;       // tips slower than this (height units / s) count as "holding still"
constexpr float kStillTime = 0.30f;        // s of holding still at full length = string is set
constexpr float kStretchTimeout = 10.f;    // s: give up on a stretch that never settles
constexpr float kRearmDist = 0.025f;       // a finger must move this far from a string before it can pluck it again
constexpr float kRetrigger = 0.09f;        // s: one string never fires twice inside this (strums = one note per string)
constexpr float kNewStringGrace = 0.30f;   // s after a string is set before it can be plucked
constexpr int kPluckHoldMs = 600;          // MIDI note length of a pluck
constexpr float kMaxJump = 0.45f;          // a fingertip 'moving' further than this in one frame is a tracking jump (hands swapped), not a swipe

struct StringDef { Pt a, b; int seqDeg = 0; int colour = 0; };
inline float sdist(Pt a, Pt b) { return std::hypot((a.x - b.x) * kAspect, a.y - b.y); }
inline float stringLength(const StringDef& s) { return sdist(s.a, s.b); }
// Length mode: about 0.9 picture-heights = root; every 0.09 shorter = one scale step up (longer = lower).
inline int lengthDegree(float len) { return std::clamp((int)std::lround((0.9f - len) / 0.09f), -7, 9); }
// Does fingertip path p0->p1 cross string a->b? s = where along the path, u = where along the string.
inline bool segCross(Pt p0, Pt p1, Pt a, Pt b, float& s, float& u) {
    const float rx = (p1.x - p0.x) * kAspect, ry = p1.y - p0.y, qx = (b.x - a.x) * kAspect, qy = b.y - a.y;
    const float den = rx * qy - ry * qx;
    if (std::fabs(den) < 1e-9f) return false;
    const float wx = (a.x - p0.x) * kAspect, wy = a.y - p0.y;
    s = (wx * qy - wy * qx) / den; u = (wx * ry - wy * rx) / den;
    return s >= 0.f && s <= 1.f && u >= 0.f && u <= 1.f;
}
// Distance from p to the string, and whether p projects inside the string's span.
inline float distToString(Pt p, const StringDef& st, bool& inside) {
    const float qx = (st.b.x - st.a.x) * kAspect, qy = st.b.y - st.a.y, wx = (p.x - st.a.x) * kAspect, wy = p.y - st.a.y;
    const float l2 = std::max(1e-9f, qx * qx + qy * qy), u = (wx * qx + wy * qy) / l2;
    inside = u >= 0.f && u <= 1.f;
    return std::fabs(qx * wy - qy * wx) / std::sqrt(l2);
}

class StringField {
public:
    std::array<StringDef, kMaxStrings> s{}; int n = 0;
    enum Phase { Idle = 0, Touch, Stretch };
    Phase phase = Idle;
    Pt pa{}, pb{};              // preview line (index tips) while touching / stretching
    float touchT = 0, stillT = 0, stretchT = 0;
    bool createArmed = true;    // tips must separate after a string is set before another can start
    struct Fx { int str = -1; float u = 0.5f; int vel = 0; uint32_t seq = 0; };
    std::array<Fx, 16> fx{}; uint32_t fxSeq = 0;   // recent plucks for the wobble/sparks animation
    int created = 0, plucks = 0;                   // counters (UI + tests)

    float holdProgress() const { return phase == Touch ? std::clamp(touchT / kTouchHold, 0.f, 1.f) : phase == Stretch ? 1.f : 0.f; }
    int noteOf(int i, const Params& p) const {
        const int deg = p.lengthPitch ? lengthDegree(stringLength(s[(size_t)i])) : s[(size_t)i].seqDeg;
        return degreeToNote(deg, p);
    }

    bool add(Pt a, Pt b, double t = -1e9) {
        if (n >= kMaxStrings || sdist(a, b) < kMinStringLen) return false;
        StringDef& st = s[(size_t)n]; st = {a, b, nextSeqDeg(), nextColour()};
        createdT[(size_t)n] = t; lastPluckT[(size_t)n] = -1e9; disarmString(n);
        ++n; ++created; return true;
    }
    void undo() { if (n > 0) { --n; disarmString(n); } }
    void clear() { n = 0; for (int i = 0; i < kMaxStrings; ++i) disarmString(i); }
    void setAll(const StringDef* defs, int count) {
        clear();
        for (int i = 0; i < std::min(count, kMaxStrings); ++i) {
            StringDef d = defs[i];
            if (!std::isfinite(d.a.x) || !std::isfinite(d.a.y) || !std::isfinite(d.b.x) || !std::isfinite(d.b.y)) continue;
            d.a = {std::clamp(d.a.x, 0.f, 1.f), std::clamp(d.a.y, 0.f, 1.f)}; d.b = {std::clamp(d.b.x, 0.f, 1.f), std::clamp(d.b.y, 0.f, 1.f)};
            d.seqDeg = std::clamp(d.seqDeg, 0, 63); d.colour = ((d.colour % kMaxStrings) + kMaxStrings) % kMaxStrings;
            s[(size_t)n] = d; createdT[(size_t)n] = -1e9; lastPluckT[(size_t)n] = -1e9; ++n;
        }
    }
    // Mode switch / camera off: drop any half-made string and forget finger history (strings stay).
    void reset() {
        phase = Idle; touchT = stillT = stretchT = 0; createArmed = true;
        for (auto& h : prevOk) h.fill(false);
        for (int i = 0; i < kMaxStrings; ++i) disarmString(i);
        lastT = -1;
    }

    void process(const Frame& fr, const Params& p, std::vector<Ev>& out) {
        const float dt = lastT < 0 ? 1.f / 60.f : (float)std::clamp(fr.t - lastT, 1e-4, 0.1);
        lastT = fr.t;
        const Hand& L = fr.hand[LeftHand]; const Hand& R = fr.hand[RightHand];
        const bool both = L.present && R.present && L.tipOk[1] && R.tipOk[1];
        const float d = both ? sdist(L.tip[1], R.tip[1]) : 1e9f;
        switch (phase) {
            case Idle:
                if (!createArmed && d > kTouchDist * 2.5f) createArmed = true;
                if (createArmed && both && d < kTouchDist && n < kMaxStrings) { phase = Touch; touchT = 0; pa = L.tip[1]; pb = R.tip[1]; }
                break;
            case Touch:
                if (!both || d > kTouchDist * 1.6f) { phase = Idle; touchT = 0; break; }
                pa = L.tip[1]; pb = R.tip[1]; touchT += dt;
                if (touchT >= kTouchHold) { phase = Stretch; stillT = stretchT = 0; }
                break;
            case Stretch: {
                if (!both) { finishStretch(fr.t); break; }   // a hand left: keep the string if it is long enough
                const Pt na = L.tip[1], nb = R.tip[1];
                const float speed = std::max(sdist(na, pa), sdist(nb, pb)) / dt;
                pa = na; pb = nb; stretchT += dt;
                if (sdist(pa, pb) >= kMinStringLen && speed < kStillSpeed) stillT += dt; else stillT = 0;
                if (stillT >= kStillTime) finishStretch(fr.t);
                else if (stretchT > kStretchTimeout) { phase = Idle; createArmed = false; }
                break;
            }
        }
        const bool allowPluck = phase == Idle;
        struct Hit { int str; float s, u, speed; };
        std::array<Hit, 2 * 5 * kMaxStrings> hits{}; int nh = 0;
        for (int h = 0; h < 2; ++h) {
            const Hand& hd = fr.hand[(size_t)h];
            for (int f = 0; f < 5; ++f) {
                if (!hd.present || !hd.tipOk[(size_t)f]) { prevOk[(size_t)h][(size_t)f] = false; continue; }
                const Pt c = hd.tip[(size_t)f];
                if (prevOk[(size_t)h][(size_t)f] && allowPluck) {
                    const Pt p0 = prevTip[(size_t)h][(size_t)f];
                    const float jump = sdist(p0, c), speed = jump / dt;
                    for (int i = 0; i < n && jump <= kMaxJump; ++i) {
                        float sp, u;
                        if (!armed[(size_t)h][(size_t)f][(size_t)i] || fr.t - createdT[(size_t)i] < kNewStringGrace) continue;
                        if (!segCross(p0, c, s[(size_t)i].a, s[(size_t)i].b, sp, u) || u < 0.02f || u > 0.98f) continue;
                        armed[(size_t)h][(size_t)f][(size_t)i] = false;   // one crossing = one note for this finger
                        hits[(size_t)nh++] = {i, sp, u, speed};
                    }
                }
                for (int i = 0; i < n; ++i) {
                    bool inside; const float dd = distToString(c, s[(size_t)i], inside);
                    if (!inside || dd > kRearmDist) armed[(size_t)h][(size_t)f][(size_t)i] = true;
                }
                prevTip[(size_t)h][(size_t)f] = c; prevOk[(size_t)h][(size_t)f] = true;
            }
        }
        // strum: order by when each string was crossed during this frame; each string plays once
        std::sort(hits.begin(), hits.begin() + nh, [](const Hit& a, const Hit& b) { return a.s < b.s; });
        for (int k = 0; k < nh; ++k) pluck(hits[(size_t)k].str, hits[(size_t)k].u, hits[(size_t)k].speed, fr.t, p, out);
    }

    // Mouse drag across strings (message thread, via the processor's lock). speed in height units / s.
    int strumSegment(Pt p0, Pt p1, float speed, double t, const Params& p, std::vector<Ev>& out) {
        struct Hit { int str; float s, u; }; std::array<Hit, kMaxStrings> hits{}; int nh = 0;
        for (int i = 0; i < n; ++i) { float sp, u; if (segCross(p0, p1, s[(size_t)i].a, s[(size_t)i].b, sp, u)) hits[(size_t)nh++] = {i, sp, u}; }
        std::sort(hits.begin(), hits.begin() + nh, [](const Hit& a, const Hit& b) { return a.s < b.s; });
        int c = 0; for (int k = 0; k < nh; ++k) c += pluck(hits[(size_t)k].str, hits[(size_t)k].u, speed, t, p, out);
        return c;
    }
    // Mouse click on (near) a string.
    int pluckNear(Pt q, double t, const Params& p, std::vector<Ev>& out, int vel = 100) {
        int best = -1; float bd = 0.03f;
        for (int i = 0; i < n; ++i) { bool in; float d = distToString(q, s[(size_t)i], in); if (in && d < bd) { bd = d; best = i; } }
        if (best < 0) return 0;
        const StringDef& st = s[(size_t)best];
        const float qx = (st.b.x - st.a.x) * kAspect, qy = st.b.y - st.a.y;
        const float u = ((q.x - st.a.x) * kAspect * qx + (q.y - st.a.y) * qy) / std::max(1e-9f, qx * qx + qy * qy);
        return emitPluck(best, u, vel, t, p, out) ? 1 : 0;
    }

    static float minSpeed(const Params& p) { return 0.30f - 0.25f * std::clamp(p.sensitivity, 0.f, 1.f); }
    static uint8_t velFor(float speed, const Params& p) {
        const float lo = minSpeed(p);
        return (uint8_t)std::clamp((int)std::lround(35.f + 92.f * std::clamp((speed - lo) / 2.2f, 0.f, 1.f)), 1, 127);
    }

private:
    std::array<double, kMaxStrings> createdT{}, lastPluckT{};
    std::array<std::array<std::array<bool, kMaxStrings>, 5>, 2> armed{};
    std::array<std::array<Pt, 5>, 2> prevTip{};
    std::array<std::array<bool, 5>, 2> prevOk{};
    double lastT = -1;

    int nextSeqDeg() const { for (int d = 0;; ++d) { bool used = false; for (int i = 0; i < n; ++i) used |= s[(size_t)i].seqDeg == d; if (!used) return d; } }
    int nextColour() const { for (int c = 0; c < kMaxStrings; ++c) { bool used = false; for (int i = 0; i < n; ++i) used |= s[(size_t)i].colour == c; if (!used) return c; } return n % kMaxStrings; }
    void disarmString(int i) { for (auto& h : armed) for (auto& f : h) f[(size_t)i] = false; }
    void finishStretch(double t) {
        if (sdist(pa, pb) >= kMinStringLen) add(pa, pb, t);
        phase = Idle; touchT = stillT = stretchT = 0; createArmed = false;
    }
    int pluck(int i, float u, float speed, double t, const Params& p, std::vector<Ev>& out) {
        if (speed < minSpeed(p)) return 0;   // drifting slowly through a string is not a pluck
        return emitPluck(i, u, velFor(speed, p), t, p, out) ? 1 : 0;
    }
    bool emitPluck(int i, float u, int vel, double t, const Params& p, std::vector<Ev>& out) {
        if (t - lastPluckT[(size_t)i] < kRetrigger) return false;
        lastPluckT[(size_t)i] = t;
        Ev e{EvType::NoteOn, (uint8_t)noteOf(i, p), (uint8_t)std::clamp(vel, 1, 127), 0, true}; e.holdMs = (uint16_t)kPluckHoldMs;
        out.push_back(e);
        fx[(size_t)(fxSeq % fx.size())] = {i, std::clamp(u, 0.f, 1.f), vel, fxSeq + 1}; ++fxSeq; ++plucks;
        return true;
    }
};

// ---------------- engine (runs on tracking thread) ----------------
class Engine {
public:
    Params p;
    bool hatHold = false;
    int handsVisible = 0;

    // overlay snapshot info
    struct KeyLabel { Pt pos; int note = -1; bool down = false; };
    std::array<std::array<KeyLabel,5>,2> labels{};
    int leadSlot = -1; int leadNote = -1; float leadBendSemis = 0; int leadVolume = 0; int lastDrum = -1;
    StringField strings;   // Strings mode (kept across mode switches; saved with the project)

    void setParams(const Params& np, std::vector<Ev>& out) {
        if (np.mode != p.mode) { allOff(out); }
        else if (np.mode == Keys && (np.key != p.key || np.scale != p.scale || np.easy5 != p.easy5 || np.octave != p.octave)) {
            // held notes keep their pitch until released (pairing preserved)
        }
        p = np;
    }

    void allOff(std::vector<Ev>& out) {
        for (int h = 0; h < 2; ++h) {
            for (auto& f : kb[h].f) if (f.note >= 0) { out.push_back({EvType::NoteOff, (uint8_t)f.note, 0}); f.note = -1; f.down = false; }
            kb[h] = {}; labels[h] = {}; dr[h] = {};
        }
        leadStop(out);
        hatHold = false;
        strings.reset();
    }

    void process(const Frame& fr, std::vector<Ev>& out) {
        handsVisible = (int)fr.hand[0].present + (int)fr.hand[1].present;
        switch (p.mode) {
            case Drum: drums(fr, out); break;
            case Keys: keys(fr, out); break;
            case Lead: lead(fr, out); break;
            case Strings: strings.process(fr, p, out); break;
        }
        lastT = fr.t;
    }

    float jabThreshold() const { return 3.2f - 2.4f * std::clamp(p.sensitivity, 0.f, 1.f); } // screen-widths / s
    float tapThreshold() const { return 0.30f - 0.18f * std::clamp(p.sensitivity, 0.f, 1.f); } // hand-size units

private:
    double lastT = 0;
    static uint8_t velFromSpeed(float speed, float thr) {
        float v = 45.f + 82.f * std::clamp((speed - thr) / (2.f * thr), 0.f, 1.f);
        return (uint8_t)std::clamp((int)std::lround(v), 1, 127);
    }

    // ---- Drum ----
    struct DrumHand { bool prevValid = false; Pt prev; double prevT = 0; bool armed = true; Pt lastDir{}; double lastHitT = -10; };
    std::array<DrumHand,2> dr{};

    void drums(const Frame& fr, std::vector<Ev>& out) {
        const float thr = jabThreshold();
        for (int h = 0; h < 2; ++h) {
            const Hand& hd = fr.hand[h]; DrumHand& d = dr[h];
            if (!hd.present) { d.prevValid = false; d.armed = true; if (h == LeftHand) hatHold = false; continue; }
            const Pt pos = hd.palm;
            if (h == LeftHand) hatHold = pos.x < kHatEdge;
            if (!d.prevValid) { d.prev = pos; d.prevT = fr.t; d.prevValid = true; continue; }
            float dt = (float)std::max(1e-4, fr.t - d.prevT);
            Pt v{(pos.x - d.prev.x) / dt, (pos.y - d.prev.y) / dt};
            float speed = std::hypot(v.x, v.y);
            const bool fromHatEdge = (h == LeftHand) && d.prev.x < kHatEdge; // leaving the hat roll is not a perc hit
            d.prev = pos; d.prevT = fr.t;
            if (d.armed && speed > thr) {
                d.armed = false;   // one hit per motion: fire at motion START
                Pt dir{v.x / speed, v.y / speed};
                bool rebound = (fr.t - d.lastHitT) < kReboundWindow && (dir.x * d.lastDir.x + dir.y * d.lastDir.y) < 0.f;
                if (rebound || fromHatEdge) { d.lastDir = dir; continue; }  // return stroke: ignored
                int note = zoneNote(h, pos);
                if (note < 0) continue;
                out.push_back({EvType::NoteOn, (uint8_t)note, velFromSpeed(speed, thr), 0, true});
                lastDrum = note; d.lastDir = dir; d.lastHitT = fr.t;
            } else if (!d.armed && speed < thr * 0.35f) d.armed = true;
        }
    }
public:
    static int zoneNote(int h, Pt pos) {
        if (pos.x >= kDeadLo && pos.x <= kDeadHi) return -1;
        if (h == RightHand) { if (pos.x <= kDeadHi) return -1; return pos.y < kRightSplitY ? kSnare : kKick; }
        if (pos.x >= kDeadLo) return -1;
        if (pos.x < kHatEdge) return -1;              // hat roll is a hold, not a jab
        return pos.y < kLeftUpperY ? kOHat : kPerc;
    }
    // Mouse-pad layout uses the same zones
    static int padAt(Pt pos) { if (pos.x > kDeadHi) return zoneNote(RightHand, pos); if (pos.x < kDeadLo) return pos.x < kHatEdge ? kCHat : zoneNote(LeftHand, pos); return -1; }
private:

    // ---- Keyboard ----
    struct Finger { bool down = false; bool init = false; float base = 0, prevR = 0; int note = -1; };
    struct KbHand { bool active = false; float anchorX = 0; int shift = 0; double prevT = 0; std::array<Finger,5> f{}; };
    std::array<KbHand,2> kb{};
public:
    static constexpr float kShiftWidth = 0.07f, kShiftHyst = 0.2f;
    int handShift(int h) const { return kb[h].shift; }
private:
    void releaseHand(int h, std::vector<Ev>& out) {
        for (auto& f : kb[h].f) { if (f.note >= 0) out.push_back({EvType::NoteOff, (uint8_t)f.note, 0}); f = {}; }
        kb[h].active = false; labels[h] = {};
    }
    void keys(const Frame& fr, std::vector<Ev>& out) {
        const float thr = tapThreshold();
        for (int h = 0; h < 2; ++h) {
            const Hand& hd = fr.hand[h]; KbHand& k = kb[h];
            bool inDead = hd.present && hd.palm.x >= kDeadLo && hd.palm.x <= kDeadHi;
            if (!hd.present || !hd.pianoBasis || inDead) { releaseHand(h, out); for (auto& l : labels[h]) l.note = -1; continue; }
            if (!k.active) { k.active = true; k.anchorX = hd.palm.x; k.shift = 0; k.prevT = fr.t; }
            // whole-hand slide shifts position along the scale (with hysteresis)
            float rel = (hd.palm.x - k.anchorX) / kShiftWidth;
            while (rel > k.shift + 0.5f + kShiftHyst) ++k.shift;
            while (rel < k.shift - 0.5f - kShiftHyst) --k.shift;
            float dt = (float)std::max(1e-4, fr.t - k.prevT); k.prevT = fr.t;
            // Hand-local forward axis: invariant to translation, in-plane tilt and zoom.
            // In an angled, palm-down piano view a downward press shortens the finger's
            // projected reach. No open-palm pose or absolute screen-Y threshold is used.
            Pt forward{hd.mcp[2].x - hd.palm.x, hd.mcp[2].y - hd.palm.y};
            float size = std::max(0.03f, std::hypot(forward.x, forward.y));
            forward.x /= size; forward.y /= size;
            std::array<float,5> raw{}, deltas{}; int nValid = 0;
            for (int i = 0; i < 5; ++i) {
                Finger& f = k.f[i];
                if (!hd.valid[i]) {
                    if (f.note >= 0) out.push_back({EvType::NoteOff, (uint8_t)f.note, 0});
                    f = {}; labels[h][i] = {}; continue;
                }
                auto reach = [&](Pt root) { return -((hd.tip[i].x - root.x) * forward.x +
                                                       (hd.tip[i].y - root.y) * forward.y) / size; };
                // Distal reach helps separate a real finger press from knuckle rocking.
                raw[i] = hd.hasPip[i] ? 0.7f * reach(hd.mcp[i]) + 0.3f * reach(hd.pip[i]) : reach(hd.mcp[i]);
                if (!f.init) { f.init = true; f.base = raw[i]; f.prevR = 0; }
                deltas[nValid++] = raw[i] - f.base;
            }
            // Fail closed if too few independent fingers are visible to reject common motion.
            if (nValid < 3) { releaseHand(h, out); continue; }
            std::sort(deltas.begin(), deltas.begin() + nValid);
            float common = nValid % 2 ? deltas[nValid/2] : 0.5f * (deltas[nValid/2-1] + deltas[nValid/2]);
            for (int i = 0; i < 5; ++i) {
                if (!hd.valid[i]) continue;
                Finger& f = k.f[i];
                float depth = raw[i] - f.base - common;
                float vel = (depth - f.prevR) / dt;

                int deg = (h == RightHand) ? (i + k.shift) : (-1 - i + k.shift); // left: thumb=-1 .. pinky=-5
                int note = degreeToNote(deg, p);
                if (!f.down) {
                    bool onset = depth > thr || (depth > thr * 0.5f && vel > thr * 8.f); // early trigger at motion start
                    if (onset && vel > 0.55f) {
                        f.down = true; f.note = note;
                        uint8_t v = (uint8_t)std::clamp((int)std::lround(40 + 87 * std::clamp(vel / (thr * 150.f), 0.f, 1.f)), 1, 127);
                        out.push_back({EvType::NoteOn, (uint8_t)note, v});
                    } else {
                        // Time-based, per-finger rest tracking ONLY while up. Never chase a held key.
                        float adapt = 1.f - std::exp(-dt / 0.25f);
                        f.base += depth * adapt;
                        depth *= 1.f - adapt;
                    }
                } else if (depth < thr * 0.45f) {
                    f.down = false; out.push_back({EvType::NoteOff, (uint8_t)f.note, 0}); f.note = -1;
                }
                f.prevR = depth;
                labels[h][i] = {hd.tip[i], f.down ? f.note : note, f.down};
            }
        }
    }

    // ---- Lead ----
    struct LeadState { bool on = false; int slot = 0; int note = -1; float xEma = 0; float anchorY = 0; float glide = 0;
                       int pendingSlot = -1; float prevX = 0, settled = 0;
                       int lastBend = -1, lastCC = -1; float prevLy = 0; bool dipArmed = true; double prevT = 0; } ld;
public:
    static constexpr float kLeadX0 = 0.62f, kSlotW = 0.054f, kLeadHyst = 0.30f; static constexpr int kSlots = 7;
    float slotPos(float x) const { return std::clamp((x - kLeadX0) / kSlotW, 0.f, (float)kSlots - 0.001f); }
private:
    void leadStop(std::vector<Ev>& out) {
        if (ld.note >= 0) { out.push_back({EvType::NoteOff, (uint8_t)ld.note, 0}); out.push_back({EvType::Bend, 0, 0, 8192}); }
        ld = {}; leadSlot = -1; leadNote = -1; leadBendSemis = 0;
    }
    void lead(const Frame& fr, std::vector<Ev>& out) {
        const Hand& R = fr.hand[RightHand]; const Hand& L = fr.hand[LeftHand];
        bool ok = R.present && L.present && R.palm.x > kDeadHi && L.palm.x < kDeadLo;
        if (!ok) { leadStop(out); return; }
        float dt = ld.on ? (float)std::max(1e-4, fr.t - ld.prevT) : 1.f / 60.f;
        float s = slotPos(R.palm.x);
        int cc = std::clamp((int)std::lround((0.9f - L.palm.y) / 0.8f * 127.f), 0, 127);
        int baseDeg = 0;
        auto noteFor = [&](int slot) { return degreeToNote(baseDeg + slot, p); };
        if (!ld.on) {
            ld.on = true; ld.slot = (int)s; ld.note = noteFor(ld.slot); ld.xEma = R.palm.x; ld.anchorY = R.palm.y;
            ld.prevX = R.palm.x; ld.prevLy = L.palm.y; ld.prevT = fr.t;
            out.push_back({EvType::CC, 11, 0, cc}); ld.lastCC = cc;
            out.push_back({EvType::Bend, 0, 0, 8192}); ld.lastBend = 8192;
            out.push_back({EvType::NoteOn, (uint8_t)ld.note, (uint8_t)std::max(1, cc)});
        } else {
            // Landing detector: crossing a slot does NOT play it. Commit only after
            // horizontal motion settles for 35 ms (typically 3 frames at 60 fps).
            float off = s - (ld.slot + 0.5f);
            int target = std::fabs(off) > 0.5f + kLeadHyst ? (int)s : ld.slot;
            float speed = std::fabs(R.palm.x - ld.prevX) / (dt * kSlotW);
            if (target != ld.pendingSlot) { ld.pendingSlot = target; ld.settled = 0; }
            if (target != ld.slot && speed < 0.8f) ld.settled += std::min(dt, 0.025f);
            else ld.settled = 0;
            if (target != ld.slot && ld.settled >= 0.035f) {
                int nn = noteFor(target);
                ld.glide += (float)(ld.note - nn) * std::clamp(p.slide, 0.f, 1.f);
                out.push_back({EvType::NoteOn, (uint8_t)nn, (uint8_t)std::max(1, cc)});
                out.push_back({EvType::NoteOff, (uint8_t)ld.note, 0});
                ld.slot = target; ld.note = nn; ld.anchorY = R.palm.y; ld.settled = 0;
            }
            ld.prevX = R.palm.x;
            // left-hand dip = retrigger
            float lvy = (L.palm.y - ld.prevLy) / dt;
            float dipThr = jabThreshold() * 0.8f;
            if (ld.dipArmed && lvy > dipThr) {
                ld.dipArmed = false;
                out.push_back({EvType::NoteOff, (uint8_t)ld.note, 0});
                out.push_back({EvType::NoteOn, (uint8_t)ld.note, (uint8_t)std::max(1, cc)});
            } else if (!ld.dipArmed && lvy < dipThr * 0.3f) ld.dipArmed = true;
            ld.prevLy = L.palm.y; ld.prevT = fr.t;
        }
        int range = bendRangeFor(p);
        float glideTau = 0.02f + 0.3f * std::clamp(p.slide, 0.f, 1.f);
        ld.glide *= std::exp(-dt / glideTau);
        if (std::fabs(ld.glide) < 0.005f) ld.glide = 0;
        ld.glide = std::clamp(ld.glide, (float)-range, (float)range);
        // vibrato: the small shake around the held note (high-passed x), never changes the note
        ld.xEma += (R.palm.x - ld.xEma) * 0.15f;
        float vib = std::clamp((R.palm.x - ld.xEma) / 0.012f, -1.f, 1.f) * 0.5f * std::clamp(p.vibrato, 0.f, 1.f);
        // up-bend: aims at the NEXT in-key note, capped there
        if (R.palm.y > ld.anchorY) ld.anchorY = R.palm.y;
        float up = std::clamp((ld.anchorY - R.palm.y) / 0.15f, 0.f, 1.f) * std::clamp(p.bend, 0.f, 1.f);
        int next = noteFor(ld.slot + 1);
        float upSemis = up * (float)(next - ld.note);
        float total = std::clamp(ld.glide + vib + upSemis, (float)-range, (float)range);
        int pb = std::clamp((int)std::lround(8192 + total / range * 8191.f), 0, 16383);
        if (pb != ld.lastBend) { out.push_back({EvType::Bend, 0, 0, pb}); ld.lastBend = pb; }
        if (cc != ld.lastCC) { out.push_back({EvType::CC, 11, 0, cc}); ld.lastCC = cc; }
        leadSlot = ld.slot; leadNote = ld.note; leadBendSemis = total; leadVolume = cc;
    }
};

// ---------------- outgoing note tracker (audio thread): pairing + no stuck notes ----------------
struct NoteTracker {
    std::array<bool,128> on{};
    // returns false if the event should be dropped (orphan note-off)
    bool noteOn(int n) { bool wasOn = on[(size_t)n]; on[(size_t)n] = true; return !wasOn; }
    bool noteOff(int n) { if (!on[(size_t)n]) return false; on[(size_t)n] = false; return true; }
    template <class F> void allOff(F&& f) { for (int n = 0; n < 128; ++n) if (on[(size_t)n]) { on[(size_t)n] = false; f(n); } }
    int count() const { int c = 0; for (bool b : on) c += b; return c; }
};

// ---------------- lock-free SPSC queue ----------------
template <class T, size_t N> class Spsc {
    std::array<T,N> buf; std::atomic<size_t> w{0}, r{0};
public:
    bool push(const T& v) { size_t wi = w.load(std::memory_order_relaxed), nx = (wi + 1) % N; if (nx == r.load(std::memory_order_acquire)) return false; buf[wi] = v; w.store(nx, std::memory_order_release); return true; }
    bool pop(T& v) { size_t ri = r.load(std::memory_order_relaxed); if (ri == w.load(std::memory_order_acquire)) return false; v = buf[ri]; r.store((ri + 1) % N, std::memory_order_release); return true; }
};

} // namespace tr
