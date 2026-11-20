// TENRINGS built-in sounds: all synthesized in code (original, no samples).
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include "Engine.h"

namespace tr {

class Synth {
public:
    enum Kind { None, Kick, Snare, CHat, OHat, Perc, Key, LeadV };
    void prepare(double sampleRate) {
        sr = sampleRate; allOff();
        // body resonance (TPT state-variable band-pass ~210 Hz) + warm top-end roll-off, shared by all plucks
        const double g = std::tan(3.14159265358979 * 210.0 / sr), k = 1.0 / 1.4;
        bodyA1 = (float)(1.0 / (1.0 + g * (g + k))); bodyA2 = (float)g * bodyA1; bodyA3 = (float)g * bodyA2;
        pluckLp = 1.f - (float)std::exp(-6.283185307 * 4200.0 / sr);
        envFall = (float)std::exp(-1.0 / (0.01 * sr));
    }
    static constexpr float kCeiling = 0.985f; // output never exceeds -0.13 dBFS (safety ceiling below -0.1 dBFS)
    int mode = Drum;
    int bendRange = 2;

    void noteOn(int note, int vel) {
        float v = vel / 127.f;
        if (mode == Lead) { leadOn(note, v); return; }
        if (mode == Strings) { pluckOn(note, v); return; }
        Kind k = Key;
        if (mode == Drum) {
            if (note == kKick) k = Kick; else if (note == kSnare) k = Snare; else if (note == kCHat) k = CHat;
            else if (note == kOHat) k = OHat; else if (note == kPerc) k = Perc;
        }
        if (k == CHat) for (auto& vc : voices) if (vc.kind == OHat) vc.choke = true; // closed hat chokes open hat
        Voice& vc = alloc();
        vc = {}; vc.kind = k; vc.note = note; vc.vel = v; vc.gate = true;
        vc.freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    }
    void noteOff(int note) {
        if (mode == Lead) { leadOff(note); return; }
        if (mode == Strings) { for (auto& pv : plucks) if (pv.on && pv.note == note && !pv.rel) pv.rel = true; return; }
        for (auto& vc : voices) if (vc.kind == Key && vc.note == note) vc.gate = false;
    }
    void pitchBend(int v) { bendSemis = (v - 8192) / 8192.f * (float)bendRange; }
    void cc(int num, int val) { if (num == 11) expr = val / 127.f; if (num == 123 || num == 120) allOff(); }
    void allOff() { for (auto& vc : voices) vc = {}; for (auto& pv : plucks) pv.on = false; lead = {}; held = 0; bendSemis = 0; expr = 1; }
    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.kind != None; for (auto& pv : plucks) c += pv.on; return c + (lead.amp > 1e-4f || lead.gate ? 1 : 0); }
    int activePlucks() const { int c = 0; for (auto& pv : plucks) c += pv.on; return c; }

    // adds into out (mono), returns nothing
    void render(float* out, int n, float gain) {
        for (int i = 0; i < n; ++i) {
            float s = 0;
            for (auto& vc : voices) if (vc.kind != None) s += voice(vc);
            s += leadSample();
            for (auto& pv : plucks) if (pv.on) s += pluckSample(pv);
            // soft safety ceiling: transparent at normal levels, never past kCeiling, never NaN/Inf
            float y = kCeiling * std::tanh(s * gain / kCeiling);
            if (!std::isfinite(y)) { allOff(); y = 0; }
            out[i] += y;
        }
    }

private:
    struct Voice { Kind kind = None; int note = 0; float vel = 0; bool gate = false, choke = false; double t = 0, ph = 0, ph2 = 0; double freq = 440; float env = 0, lp = 0, lp2 = 0, hp = 0, hpPrev = 0; };
    std::array<Voice,24> voices{};
    double sr = 48000; uint32_t rng = 0x1234567;
    float bendSemis = 0, expr = 1;
    float noise() { rng = rng * 1664525u + 1013904223u; return (float)((rng >> 9) * (1.0 / 4194304.0)) - 1.f; }
    Voice& alloc() { Voice* best = &voices[0]; for (auto& v : voices) { if (v.kind == None) return v; if (v.t > best->t) best = &v; } return *best; }

    float voice(Voice& v) {
        const double dt = 1.0 / sr; float t = (float)v.t, out = 0; v.t += dt;
        const float twoPi = 6.2831853f;
        switch (v.kind) {
            case Kick: { double f = 45 + 110 * std::exp(-t * 35); v.ph += f * dt; float e = std::exp(-t * 7.f);
                out = (float)std::sin(twoPi * v.ph) * e * 1.1f + noise() * std::exp(-t * 300) * 0.15f; if (t > 0.6f) v.kind = None; break; }
            case Snare: { v.ph += 190 * dt; float tone = (float)std::sin(twoPi * v.ph) * std::exp(-t * 25.f) * 0.5f;
                float nz = noise(); v.hp = nz - v.hpPrev; v.hpPrev = nz; out = tone + v.hp * std::exp(-t * 14.f) * 0.45f; if (t > 0.4f) v.kind = None; break; }
            case CHat: case OHat: { float nz = noise(); float h = nz - v.hpPrev; v.hpPrev = nz; float h2 = h - v.lp; v.lp = h;
                float dec = v.kind == CHat ? 60.f : 7.f; if (v.choke) dec = 120.f; out = h2 * std::exp(-t * dec) * 0.28f; if (t > (v.kind == CHat ? 0.12f : 0.8f) || (v.choke && t > 0.05f)) { if (v.choke || v.kind == CHat) v.kind = None; else if (t > 0.8f) v.kind = None; } break; }
            case Perc: { // clap: three quick noise bursts + tail
                float nz = noise(); v.lp += (nz - v.lp) * 0.35f; float bp = nz - v.lp;
                float e = 0; for (int k = 0; k < 3; ++k) { float tk = t - k * 0.011f; if (tk >= 0) e = std::max(e, std::exp(-tk * 180.f)); }
                e = std::max(e, t > 0.022f ? std::exp(-(t - 0.022f) * 18.f) * 0.6f : 0.f); out = bp * e * 0.5f; if (t > 0.35f) v.kind = None; break; }
            case Key: { double f = v.freq * std::pow(2.0, bendSemis / 12.0); v.ph += f * dt; v.ph2 += f * 2.0 * dt;
                float target = v.gate ? 0.30f + 0.70f * std::exp(-t * 2.2f) : 0.f;
                float rate = 1.f - std::exp(-(float)dt / (v.gate ? (t < 0.012f ? 0.006f : 0.035f) : 0.045f));
                v.env += (target - v.env) * rate;
                float raw = (float)std::sin(twoPi * v.ph) + 0.10f * (float)std::sin(twoPi * v.ph2) * std::exp(-t * 3.f);
                float cutoff = 1.f - std::exp(-twoPi * 1800.f * (float)dt);
                v.lp += cutoff * (raw - v.lp); v.lp2 += cutoff * (v.lp - v.lp2);
                out = v.lp2 * v.env * 0.40f;
                if (!v.gate && v.env < 1e-4f) v.kind = None; break; }
            default: break;
        }
        return out * v.vel;
    }


    // ---- Strings: warm plucked string (Karplus-Strong loop + soft pick + small wooden body), all original code ----
    static constexpr int kLoop = 4096;
    struct PluckVoice { bool on = false, rel = false; int note = 0, w = 0; float D = 100, last = 0, g = 1, gRel = 1, amp = 0;
                        float dcx = 0, dcy = 0, ic1 = 0, ic2 = 0, lp = 0, env = 0; double t = 0; std::array<float, kLoop> buf{}; };
    std::array<PluckVoice, 12> plucks{};
    float bodyA1 = 0, bodyA2 = 0, bodyA3 = 0, pluckLp = 0.4f, envFall = 0.998f;
    void pluckOn(int note, float v) {
        double f = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        while (sr / f > kLoop - 16) f *= 2.0;                       // keep the loop inside its buffer at any sample rate
        for (auto& pv : plucks) if (pv.on && pv.note == note) { pv.rel = true; pv.gRel = (float)std::pow(10.0, -3.0 / (0.06 * f)); } // re-pluck damps the old ring
        PluckVoice* vp = nullptr; for (auto& pv : plucks) if (!pv.on) { vp = &pv; break; }
        if (!vp) { vp = &plucks[0]; for (auto& pv : plucks) if (pv.t > vp->t) vp = &pv; }
        PluckVoice& p = *vp;
        p.on = true; p.rel = false; p.note = note; p.t = 0; p.last = 0; p.dcx = p.dcy = p.ic1 = p.ic2 = p.lp = 0; p.env = 1;
        p.D = (float)(sr / f) - 0.5f;                               // loop averaging filter adds half a sample
        const int len = (int)std::ceil(p.D) + 2;
        // excitation: velocity-bright noise burst, pick-position comb (warm, harp/koto-like), zero mean, unit peak
        std::array<float, kLoop>& b = p.buf; b.fill(0.f);
        const float bright = 0.12f + 0.62f * v; float y = 0;
        for (int i = 0; i < len; ++i) { y += bright * (noise() - y); b[(size_t)i] = y; }
        const int P = std::max(1, (int)(p.D * 0.14f));
        for (int i = len - 1; i >= P; --i) b[(size_t)i] -= b[(size_t)(i - P)];
        float mean = 0, pk = 1e-6f; for (int i = 0; i < len; ++i) mean += b[(size_t)i]; mean /= (float)len;
        for (int i = 0; i < len; ++i) { b[(size_t)i] -= mean; pk = std::max(pk, std::fabs(b[(size_t)i])); }
        for (int i = 0; i < len; ++i) b[(size_t)i] /= pk;
        p.w = len;
        const double t60 = std::clamp(3.4 * std::pow(220.0 / f, 0.45), 1.0, 5.5);   // low strings ring longer
        p.g = (float)std::pow(10.0, -3.0 / (t60 * f));
        p.gRel = (float)std::pow(10.0, -3.0 / (0.8 * f));            // note-off (piano roll): soft damp over ~0.8 s
        p.amp = (0.12f + 0.88f * std::pow(v, 1.2f)) * 0.42f;   // wide, natural velocity range
    }
    float pluckSample(PluckVoice& p) {
        constexpr int m = kLoop - 1;
        const float rp = (float)p.w - p.D; const int i0 = (int)std::floor(rp); const float fr = rp - (float)i0;
        const float y = p.buf[(size_t)(i0 & m)] * (1.f - fr) + p.buf[(size_t)((i0 + 1) & m)] * fr;
        p.buf[(size_t)(p.w & m)] = 0.5f * (y + p.last) * (p.rel ? std::min(p.g, p.gRel) : p.g);
        p.last = y; p.w = (p.w + 1) & m;   // stays small so float read positions keep full precision
        p.dcy = y - p.dcx + 0.995f * p.dcy; p.dcx = y;              // DC blocker
        const float v3 = p.dcy - p.ic2, v1 = bodyA1 * p.ic1 + bodyA2 * v3, v2 = p.ic2 + bodyA2 * p.ic1 + bodyA3 * v3;
        p.ic1 = 2.f * v1 - p.ic1; p.ic2 = 2.f * v2 - p.ic2;
        p.lp += pluckLp * ((p.dcy + 0.45f * v1) - p.lp);              // body + gentle top roll-off
        const float out = p.lp * p.amp;
        p.env = std::max(std::fabs(out), p.env * envFall);
        p.t += 1.0 / sr;
        if ((p.t > 0.05 && p.env < 2e-5f) || p.t > 12.0) p.on = false;
        return out;
    }

    // mono singing lead with glide (driven by pitch bend + legato smoothing)
    struct LeadVoice { bool gate = false; int note = -1; float vel = 0, amp = 0, curSemi = 60, lp = 0, lp2 = 0; double ph = 0, vph = 0; } lead;
    int held = 0;
    void leadOn(int note, float v) { if (!lead.gate) lead.curSemi = (float)note; lead.gate = true; lead.note = note; lead.vel = v; held++; }
    void leadOff(int note) { held = held > 0 ? held - 1 : 0; if (note == lead.note && held == 0) lead.gate = false; }
    float leadSample() {
        if (!lead.gate && lead.amp < 1e-5f) return 0;
        lead.curSemi += ((float)lead.note - lead.curSemi) * (1.f - std::exp(-1.f / (0.0052f * (float)sr))); // short legato glide
        double f = 440.0 * std::pow(2.0, (lead.curSemi + bendSemis - 69.0) / 12.0);
        lead.ph += f / sr; lead.ph -= std::floor(lead.ph);
        // Phase-aligned sine + quiet triangle: no discontinuous raw saw.
        float phase = (float)lead.ph;
        float tri = 2.f / 3.14159265f * std::asin(std::sin(6.2831853f * phase));
        float raw = 0.80f * std::sin(6.2831853f * phase) + 0.20f * tri;
        float cutoff = 1.f - std::exp(-6.2831853f * 1400.f / (float)sr);
        lead.lp += (raw - lead.lp) * cutoff;
        lead.lp2 += (lead.lp - lead.lp2) * cutoff;
        float target = lead.gate ? lead.vel * expr : 0.f;
        lead.amp += (target - lead.amp) * (1.f - std::exp(-1.f / ((lead.gate ? 0.007f : 0.045f) * (float)sr)));
        return lead.lp2 * lead.amp * 0.45f;
    }
};

} // namespace tr
