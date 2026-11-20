// Pure engine tests (no JUCE): zones, jabs, rebound, hats, keys, lead, silence rules, pairing, hat-roll timing.
#include "helpers.h"
#include "engine/Synth.h"
using namespace th;

struct Sim {
    Engine e; double t = 0; std::vector<Ev> ev;
    Sim(Params p = {}) { e.p = p; }
    void step(const Hand* L, const Hand* R, int frames = 1) { for (int i = 0; i < frames; ++i) { t += 1.0 / 60; e.process(frame(t, L, R), ev); } }
    void clear() { ev.clear(); }
};

static void testDrums() {
    Params p; p.mode = Drum;
    { // high-right jab = snare only; fast return down = nothing; later low jab = kick only
        Sim s(p); Hand R = hand(0.8f, 0.40f); s.step(nullptr, &R, 5);
        for (float y : {0.34f, 0.28f}) { R = hand(0.8f, y); s.step(nullptr, &R); }
        s.step(nullptr, &R, 3);
        CHECK(count(s.ev, EvType::NoteOn, kSnare) == 1); CHECK(count(s.ev, EvType::NoteOn) == 1);
        s.clear();
        for (float y : {0.36f, 0.44f, 0.52f, 0.60f}) { R = hand(0.8f, y); s.step(nullptr, &R); }
        s.step(nullptr, &R, 30);
        CHECK(count(s.ev, EvType::NoteOn) == 0); // down-return after snare does NOT trigger kick
        s.clear();
        for (float y : {0.66f, 0.72f}) { R = hand(0.8f, y); s.step(nullptr, &R); }
        s.step(nullptr, &R, 3);
        CHECK(count(s.ev, EvType::NoteOn, kKick) == 1); CHECK(count(s.ev, EvType::NoteOn) == 1);
        for (auto& x : s.ev) CHECK(x.autoOff);
    }
    { // velocity follows jab speed
        Sim a(p), b(p); Hand R = hand(0.8f, 0.7f); a.step(nullptr, &R, 3); b.step(nullptr, &R, 3);
        Hand R1 = hand(0.8f, 0.745f); a.step(nullptr, &R1); Hand R2 = hand(0.8f, 0.82f); b.step(nullptr, &R2);
        CHECK(a.ev.size() == 1 && b.ev.size() == 1); if (a.ev.size() == 1 && b.ev.size() == 1) CHECK(b.ev[0].vel > a.ev[0].vel);
    }
    { // slow drift never triggers; sensitivity changes threshold
        Sim s(p); for (int i = 0; i < 40; ++i) { Hand R = hand(0.95f - i * 0.008f, 0.3f + i * 0.01f); s.step(nullptr, &R); }
        CHECK(s.ev.empty());
        Params lo = p; lo.sensitivity = 0.f; Params hi = p; hi.sensitivity = 1.f;
        Sim a(lo), b(hi); Hand R = hand(0.8f, 0.7f); a.step(nullptr, &R, 3); b.step(nullptr, &R, 3);
        Hand R1 = hand(0.8f, 0.73f); a.step(nullptr, &R1); b.step(nullptr, &R1); // 1.8 widths/s
        CHECK(a.ev.empty()); CHECK(b.ev.size() == 1);
    }
    { // perc / open hat zones on left, center dead zone, hat hold zone
        Sim s(p); Hand L = hand(0.3f, 0.6f); s.step(&L, nullptr, 3); L = hand(0.3f, 0.68f); s.step(&L, nullptr); s.step(&L, nullptr, 20);
        CHECK(count(s.ev, EvType::NoteOn, kPerc) == 1);
        s.step(nullptr, nullptr); s.clear(); L = hand(0.3f, 0.25f); s.step(&L, nullptr, 3); L = hand(0.3f, 0.18f); s.step(&L, nullptr); s.step(&L, nullptr, 3);
        CHECK(count(s.ev, EvType::NoteOn, kOHat) == 1); CHECK(count(s.ev, EvType::NoteOn) == 1);
        s.step(nullptr, nullptr); s.clear(); Hand C = hand(0.5f, 0.5f); s.step(&C, nullptr, 3); C = hand(0.5f, 0.6f); s.step(&C, nullptr); C = hand(0.5f, 0.4f); s.step(nullptr, &C, 3); C = hand(0.52f, 0.3f); s.step(nullptr, &C);
        CHECK(s.ev.empty()); // face/chest zone never triggers
        L = hand(0.08f, 0.5f); s.step(&L, nullptr); CHECK(s.e.hatHold);
        L = hand(0.25f, 0.5f); s.step(&L, nullptr); CHECK(!s.e.hatHold);   // leaving edge stops roll
        L = hand(0.08f, 0.5f); s.step(&L, nullptr); CHECK(s.e.hatHold);
        s.step(nullptr, nullptr); CHECK(!s.e.hatHold);                        // leaving frame stops roll
        s.step(nullptr, nullptr, 30); CHECK(s.ev.empty());                    // no hands = silence
        CHECK(Engine::padAt({0.05f, 0.5f}) == kCHat); CHECK(Engine::padAt({0.8f, 0.2f}) == kSnare); CHECK(Engine::padAt({0.8f, 0.8f}) == kKick);
        CHECK(Engine::padAt({0.3f, 0.8f}) == kPerc); CHECK(Engine::padAt({0.3f, 0.1f}) == kOHat); CHECK(Engine::padAt({0.5f, 0.5f}) == -1);
    }
}

static void tap(Sim& s, const Hand* L, Hand* R, int hand, int finger, float curl, float x) {
    std::array<float,5> c{}; c[(size_t)finger] = curl; Hand h = th::hand(x, 0.5f, c);
    if (hand == RightHand) s.step(L, &h); else s.step(&h, R);
}

static void testKeys() {
    Params p; p.mode = Keys; p.key = 0; p.scale = 1;
    auto noteOf = [](const Sim& s) { for (auto& e : s.ev) if (e.type == EvType::NoteOn) return (int)e.note; return -1; };
    { Sim s(p); Hand R = hand(0.8f, 0.5f); s.step(nullptr, &R, 5);
      tap(s, nullptr, nullptr, RightHand, 1, 0.35f, 0.8f); tap(s, nullptr, nullptr, RightHand, 1, 0.7f, 0.8f); s.step(nullptr, nullptr, 0);
      CHECK(count(s.ev, EvType::NoteOn) == 1); CHECK(noteOf(s) == 62);
      for (int i = 0; i < 5; ++i) tap(s, nullptr, nullptr, RightHand, 1, 0.7f, 0.8f);
      CHECK(count(s.ev, EvType::NoteOn) == 1);
      R = hand(0.8f, 0.5f); s.step(nullptr, &R, 2);
      CHECK(count(s.ev, EvType::NoteOff, 62) == 1); }
    { // thumb..pinky consecutive; left hand = five below
      int expectR[5] = {60, 62, 63, 65, 67}, expectL[5] = {58, 56, 55, 53, 51};
      for (int f = 0; f < 5; ++f) {
          Sim s(p); Hand R = hand(0.8f, 0.5f); s.step(nullptr, &R, 5); tap(s, nullptr, nullptr, RightHand, f, 0.8f, 0.8f); CHECK(noteOf(s) == expectR[f]);
          Sim l(p); Hand L = hand(0.2f, 0.5f); l.step(&L, nullptr, 5); tap(l, nullptr, nullptr, LeftHand, f, 0.8f, 0.2f); CHECK(noteOf(l) == expectL[f]);
      } }
    { // whole-hand slide shifts position; held note keeps its pitch (pairing)
      Sim s(p); Hand R = hand(0.75f, 0.5f); s.step(nullptr, &R, 5);
      for (int i = 1; i <= 10; ++i) { Hand r = hand(0.75f + 0.0077f * i, 0.5f); s.step(nullptr, &r); }
      CHECK(s.e.handShift(RightHand) == 1);
      s.clear(); tap(s, nullptr, nullptr, RightHand, 0, 0.8f, 0.827f); CHECK(noteOf(s) == 62);
      Sim s2(p); Hand r2 = hand(0.75f, 0.5f); s2.step(nullptr, &r2, 5); Hand r3 = hand(0.75f + 0.07f * 0.6f, 0.5f); s2.step(nullptr, &r3, 3);
      CHECK(s2.e.handShift(RightHand) == 0); } // small move inside hysteresis does not shift
    { // easy 5, octave, key, scale
      auto first = [&](Params q, int finger) { Sim s(q); Hand R = hand(0.8f, 0.5f); s.step(nullptr, &R, 5); tap(s, nullptr, nullptr, RightHand, finger, 0.8f, 0.8f); return noteOf(s); };
      Params e5 = p; e5.easy5 = true; CHECK(first(e5, 1) == 63); CHECK(first(e5, 4) == 70);
      Params o = p; o.octave = 1; CHECK(first(o, 0) == 72); o.octave = -2; CHECK(first(o, 0) == 36);
      Params k = p; k.key = 2; CHECK(first(k, 0) == 62);
      Params mj = p; mj.scale = 0; CHECK(first(mj, 2) == 64); }
    { // out of frame cuts held notes; center dead zone never plays; slow drift no trigger
      Sim s(p); Hand R = hand(0.8f, 0.5f); s.step(nullptr, &R, 5); tap(s, nullptr, nullptr, RightHand, 2, 0.8f, 0.8f);
      CHECK(count(s.ev, EvType::NoteOn) == 1); s.step(nullptr, nullptr); CHECK(count(s.ev, EvType::NoteOff) == 1);
      s.step(nullptr, nullptr); s.clear(); Hand C = hand(0.5f, 0.5f); s.step(nullptr, &C, 5); tap(s, nullptr, nullptr, RightHand, 2, 0.9f, 0.5f); CHECK(s.ev.empty());
      Sim d(p); for (int i = 0; i < 200; ++i) { std::array<float,5> c{}; c[1] = i * 0.005f; Hand h = hand(0.8f, 0.5f, c); d.step(nullptr, &h); }
      CHECK(count(d.ev, EvType::NoteOn) == 0); }
    { // mode switch = all notes off
      Sim s(p); Hand R = hand(0.8f, 0.5f); s.step(nullptr, &R, 5); tap(s, nullptr, nullptr, RightHand, 0, 0.8f, 0.8f); tap(s, nullptr, nullptr, RightHand, 0, 0.8f, 0.8f);
      Params d = p; d.mode = Drum; s.e.setParams(d, s.ev); CHECK(count(s.ev, EvType::NoteOff) == count(s.ev, EvType::NoteOn)); }
    { // tap velocity follows speed
      Sim a(p), b(p); Hand R = hand(0.8f, 0.5f); a.step(nullptr, &R, 5); b.step(nullptr, &R, 5);
      tap(a, nullptr, nullptr, RightHand, 1, 0.3f, 0.8f); tap(a, nullptr, nullptr, RightHand, 1, 0.5f, 0.8f);
      tap(b, nullptr, nullptr, RightHand, 1, 1.2f, 0.8f);
      int va = 0, vb = 0; for (auto& e : a.ev) if (e.type == EvType::NoteOn) va = e.vel; for (auto& e : b.ev) if (e.type == EvType::NoteOn) vb = e.vel;
      CHECK(va > 0 && vb > va); }
}

static int lastBend(const std::vector<Ev>& v) { int b = -1; for (auto& e : v) if (e.type == EvType::Bend) b = e.value; return b; }
static int lastCC(const std::vector<Ev>& v) { int b = -1; for (auto& e : v) if (e.type == EvType::CC) b = e.value; return b; }

static void testLead() {
    Params p; p.mode = Lead; p.scale = 1;
    const float x2 = Engine::kLeadX0 + Engine::kSlotW * 2.5f;
    Hand L = hand(0.2f, 0.4f);
    { Sim s(p); Hand R = hand(x2, 0.5f); s.step(&L, &R, 3);
      CHECK(count(s.ev, EvType::NoteOn, 63) == 1);
      // small wobble = vibrato, note unchanged
      s.clear(); for (int i = 0; i < 30; ++i) { Hand r = hand(x2 + ((i % 4) < 2 ? 0.012f : -0.012f), 0.5f); s.step(&L, &r); }
      CHECK(count(s.ev, EvType::NoteOn) == 0); CHECK(count(s.ev, EvType::Bend) > 5);
      // move inside hysteresis: still no change
      s.clear(); Hand r = hand(x2 + Engine::kSlotW * 0.7f, 0.5f); s.step(&L, &r, 10); CHECK(count(s.ev, EvType::NoteOn) == 0);
      // clear move = next in-key note, legato (on before off)
      s.clear(); r = hand(x2 + Engine::kSlotW, 0.5f); s.step(&L, &r, 4);
      CHECK(count(s.ev, EvType::NoteOn, 65) == 1); CHECK(count(s.ev, EvType::NoteOff, 63) == 1);
      auto firstOn = std::find_if(s.ev.begin(), s.ev.end(), [](const Ev& e) { return e.type == EvType::NoteOn || e.type == EvType::NoteOff; });
      CHECK(firstOn != s.ev.end() && firstOn->type == EvType::NoteOn);
      CHECK(lastBend(s.ev) < 8192 - 1000); // slide: starts below and glides up into the new note
      s.step(&L, &r, 60); CHECK(std::abs(lastBend(s.ev) - 8192) < 50);
    }
    { // vibrato 0 = wobble gives no bend; slide 0 = no glide
      Params q = p; q.vibrato = 0; q.slide = 0; Sim s(q); Hand R = hand(x2, 0.5f); s.step(&L, &R, 3);
      s.clear(); for (int i = 0; i < 20; ++i) { Hand r = hand(x2 + ((i % 4) < 2 ? 0.012f : -0.012f), 0.5f); s.step(&L, &r); }
      CHECK(count(s.ev, EvType::Bend) == 0);
      Hand r = hand(x2 + Engine::kSlotW, 0.5f); s.step(&L, &r, 4); CHECK(lastBend(s.ev) == -1 || std::abs(lastBend(s.ev) - 8192) < 30); }
    { // up = bend toward NEXT in-key note (minor: Eb -> F = +2 = full range), capped; bend param scales it
      Sim s(p); Hand R = hand(x2, 0.5f); s.step(&L, &R, 3);
      for (int i = 1; i <= 20; ++i) { Hand r = hand(x2, 0.5f - 0.01f * i); s.step(&L, &r); }
      CHECK(count(s.ev, EvType::NoteOn) == 1); CHECK(lastBend(s.ev) == 16383);
      Params h = p; h.bend = 0.5f; Sim s2(h); s2.step(&L, &R, 3); for (int i = 1; i <= 20; ++i) { Hand r = hand(x2, 0.5f - 0.01f * i); s2.step(&L, &r); }
      CHECK(std::abs(lastBend(s2.ev) - 12288) < 120);
      Params e5 = p; e5.easy5 = true; CHECK(bendRangeFor(e5) == 3); CHECK(bendRangeFor(p) == 2); }
    { // volume (CC11) and dip retrigger
      Sim s(p); Hand R = hand(x2, 0.5f); Hand Lhi = hand(0.2f, 0.2f); s.step(&Lhi, &R, 3); int hi = lastCC(s.ev);
      for (int i = 1; i <= 30; ++i) { Hand l = hand(0.2f, 0.2f + 0.02f * i); s.step(&l, &R); if (i == 30) break; }
      // slow move down (0.02/frame = 1.2/s) is below dip threshold => no retrigger
      CHECK(count(s.ev, EvType::NoteOn) == 1); int lo = lastCC(s.ev); CHECK(hi > 100); CHECK(lo < 20);
      s.clear(); Hand l1 = hand(0.2f, 0.4f); s.step(&l1, &R, 5); s.clear(); Hand l2 = hand(0.2f, 0.5f); s.step(&l2, &R);
      CHECK(count(s.ev, EvType::NoteOff, 63) == 1); CHECK(count(s.ev, EvType::NoteOn, 63) == 1); }
    { // either hand absent = silent
      Sim s(p); Hand R = hand(x2, 0.5f); s.step(&L, &R, 3); s.step(nullptr, &R);
      CHECK(count(s.ev, EvType::NoteOff) == 1); s.clear(); s.step(nullptr, &R, 10); CHECK(s.ev.empty());
      s.step(&L, &R); CHECK(count(s.ev, EvType::NoteOn) == 1); s.clear(); s.step(&L, nullptr); CHECK(count(s.ev, EvType::NoteOff) == 1);
      s.clear(); s.step(&L, nullptr, 10); s.step(nullptr, nullptr, 10); CHECK(count(s.ev, EvType::NoteOn) == 0);
      Hand Rc = hand(0.5f, 0.5f); s.step(&L, &Rc, 5); CHECK(count(s.ev, EvType::NoteOn) == 0); } // right hand in dead zone
    { // mode switch from lead
      Sim s(p); Hand R = hand(x2, 0.5f); s.step(&L, &R, 3); Params k = p; k.mode = Keys; s.e.setParams(k, s.ev); CHECK(count(s.ev, EvType::NoteOff) == 1); }
}

static void testHatRollAndTracker() {
    auto hits = [](double bpm, bool six, int samples, bool hold = true) { HatRoll h; h.sr = 48000; int c = 0; for (int i = 0; i < samples; i += 512) h.process(hold, bpm, six, std::min(512, samples - i), [&](int) { ++c; }); return c; };
    CHECK(hits(120, true, 48000) == 8); CHECK(hits(120, false, 48000) == 4); CHECK(hits(60, true, 48000) == 4); CHECK(hits(140, true, 48000 * 3) == 28);
    CHECK(hits(120, true, 48000, false) == 0); CHECK(hits(0, true, 48000) == 8); // unknown tempo -> 120
    { HatRoll h; h.sr = 48000; int c = 0; h.process(true, 120, true, 24000, [&](int) { ++c; }); h.process(true, 240, true, 24000, [&](int) { ++c; }); CHECK(c == 4 + 8); } // tempo change
    NoteTracker t; CHECK(t.noteOn(60)); CHECK(!t.noteOn(60)); CHECK(t.noteOff(60)); CHECK(!t.noteOff(60)); t.noteOn(1); t.noteOn(2); int n = 0; t.allOff([&](int) { ++n; }); CHECK(n == 2 && t.count() == 0);
    Spsc<int, 8> q; for (int i = 0; i < 7; ++i) CHECK(q.push(i)); CHECK(!q.push(9)); int v; CHECK(q.pop(v) && v == 0);
    // synth makes sound for every kind and goes silent after offs
    for (int m = 0; m < 3; ++m) { Synth s; s.prepare(48000); s.mode = m; std::vector<float> b(4800, 0.f);
        for (int note : {kKick, kSnare, kCHat, kOHat, kPerc, 64}) { std::fill(b.begin(), b.end(), 0.f); s.noteOn(note, 110); s.render(b.data(), 4800, 1); float pk = 0; for (float x : b) pk = std::max(pk, std::fabs(x)); CHECK(pk > 0.01f); s.noteOff(note); }
        s.allOff(); std::fill(b.begin(), b.end(), 0.f); s.render(b.data(), 4800, 1); float pk = 0; for (float x : b) pk = std::max(pk, std::fabs(x)); CHECK(pk < 1e-6f); }
}

// v1.1 piano-posture regressions: no palm-facing-camera curl assumption.
static Hand transformed(Hand h, float angle, float zoom, Pt move = {}) {
    Pt center = h.palm;
    auto xf = [&](Pt p) { float x = (p.x-center.x)*zoom, y = (p.y-center.y)*zoom;
        return Pt{center.x + std::cos(angle)*x - std::sin(angle)*y + move.x,
                  center.y + std::sin(angle)*x + std::cos(angle)*y + move.y}; };
    for (int i=0; i<5; ++i) { h.tip[i]=xf(h.tip[i]); h.mcp[i]=xf(h.mcp[i]); h.pip[i]=xf(h.pip[i]); }
    h.palm=xf(h.palm); return h;
}
static void testAirPiano() {
    Params p; p.mode=Keys;
    for (float angle : {-0.65f, 0.f, 0.55f}) {
        Sim s(p); Hand rest=transformed(hand(.8f,.6f),angle,1.1f); s.step(nullptr,&rest,10);
        // Slightly coupled neighbors plus hand rotation/zoom while one finger presses.
        Hand press=transformed(hand(.8f,.6f,{.08f,.65f,.10f,.06f,.08f}),angle+.12f,1.16f,{0,.01f});
        s.step(nullptr,&press);
        CHECK(count(s.ev,EvType::NoteOn)==1); CHECK(count(s.ev,EvType::NoteOn,62)==1);
        for (int i=0;i<5;++i) CHECK(s.e.labels[1][i].down==(i==1));
        s.step(nullptr,&press,120); CHECK(count(s.ev,EvType::NoteOn)==1); CHECK(count(s.ev,EvType::NoteOff)==0);
        s.step(nullptr,&rest); CHECK(count(s.ev,EvType::NoteOff,62)==1);
        for (const auto& lb:s.e.labels[1]) CHECK(!lb.down);
    }
    { // Common-mode drop and all-finger flex/rock never make a five-note burst.
        Sim s(p); Hand h=hand(.8f,.6f); s.step(nullptr,&h,5);
        for(int n=0;n<40;++n) { h=transformed(hand(.8f,.6f,{.6f,.62f,.59f,.61f,.60f}),.1f,1.08f,{0,.04f}); s.step(nullptr,&h); }
        CHECK(count(s.ev,EvType::NoteOn)==0);
        for(const auto& lb:s.e.labels[1]) CHECK(!lb.down);
        h=hand(.8f,.6f); s.step(nullptr,&h,5); CHECK(count(s.ev,EvType::NoteOn)==0);
    }
    { // Two independent notes can be held/released separately, never lighting siblings.
        Sim s(p); Hand h=hand(.8f,.5f); s.step(nullptr,&h,5);
        h=hand(.8f,.5f,{0,.65f,0,.65f,0}); s.step(nullptr,&h);
        CHECK(count(s.ev,EvType::NoteOn)==2); CHECK(s.e.labels[1][1].down && s.e.labels[1][3].down);
        h=hand(.8f,.5f,{0,0,0,.65f,0}); s.step(nullptr,&h);
        CHECK(count(s.ev,EvType::NoteOff,62)==1); CHECK(s.e.labels[1][3].down);
        s.step(nullptr,nullptr); CHECK(count(s.ev,EvType::NoteOff)==2);
    }
    { // Missing tip is an immediate release, not an apparent downward press.
        Sim s(p); Hand h=hand(.8f,.5f); s.step(nullptr,&h,5);
        h=hand(.8f,.5f,{0,.7f,0,0,0}); s.step(nullptr,&h);
        h.valid[1]=false; h.tip[1]=h.palm; s.step(nullptr,&h);
        CHECK(count(s.ev,EvType::NoteOff,62)==1); CHECK(!s.e.labels[1][1].down && s.e.labels[1][1].note==-1);
        h.valid={false,false,false,true,true}; s.step(nullptr,&h);
        CHECK(count(s.ev,EvType::NoteOn)==1);
    }
    { // Slow individual drift remains silent over many seconds.
        Sim s(p);
        for(int n=0;n<600;++n) { Hand h=hand(.8f,.5f,{0,n*.003f,0,0,0}); s.step(nullptr,&h); }
        CHECK(count(s.ev,EvType::NoteOn)==0);
    }
    { // Sensitivity still controls small press threshold.
        Params lo=p,hi=p;lo.sensitivity=0;hi.sensitivity=1; Sim a(lo),b(hi);
        Hand h=hand(.8f,.5f);a.step(nullptr,&h,5);b.step(nullptr,&h,5);
        h=hand(.8f,.5f,{0,.12f,0,0,0});a.step(nullptr,&h);b.step(nullptr,&h);
        CHECK(count(a.ev,EvType::NoteOn)==0);CHECK(count(b.ev,EvType::NoteOn)==1);
    }
    // No frame-count debounce added to key onset at 30/60/120 fps.
    for(double fps : {30.,60.,120.}) {
        Engine e;e.p=p; std::vector<Ev> out;Hand h=hand(.8f,.5f);
        e.process(frame(1,&h,&h),out); // each side is deliberately identical; no movement yet
        h=hand(.8f,.5f,{0,0,.55f,0,0});e.process(frame(1+1/fps,nullptr,&h),out);
        CHECK(count(out,EvType::NoteOn)==1);
    }
    { // Re-entering after a mode change learns a fresh rest rather than stale baselines.
        Sim s(p);Hand h=hand(.8f,.5f);s.step(nullptr,&h,4);h=hand(.8f,.5f,{0,.6f,0,0,0});s.step(nullptr,&h);
        Params drum=p;drum.mode=Drum;s.e.setParams(drum,s.ev);s.e.setParams(p,s.ev);s.clear();s.step(nullptr,&h,5);
        CHECK(count(s.ev,EvType::NoteOn)==0);
    }
}
static void testLeadLandings() {
    Params p;p.mode=Lead;p.vibrato=0;
    Hand l=hand(.2f,.4f);
    auto at=[](float slot) { return hand(Engine::kLeadX0+Engine::kSlotW*(slot+.5f),.5f); };
    for(bool reverse : {false,true}) {
        Sim s(p);float start=reverse?5:0,dest=reverse?0:5;
        Hand r=at(start);s.step(&l,&r,5);s.clear();
        for(int n=1;n<=18;++n) { r=at(start+(dest-start)*n/18.f);s.step(&l,&r); }
        CHECK(count(s.ev,EvType::NoteOn)==0); // crosses five scale degrees silently
        s.step(&l,&r,3);CHECK(count(s.ev,EvType::NoteOn)==1);
        CHECK(count(s.ev,EvType::NoteOn,degreeToNote((int)dest,p))==1);
        CHECK(count(s.ev,EvType::NoteOff,degreeToNote((int)start,p))==1);
        CHECK(s.e.leadSlot==(int)dest);
    }
    { Sim s(p);Hand r=at(1);s.step(&l,&r,5);s.clear();
      for(int n=1;n<=30;++n) { r=at(1+n/30.f);s.step(&l,&r); }
      CHECK(count(s.ev,EvType::NoteOn)==0);s.step(&l,&r,3);
      CHECK(count(s.ev,EvType::NoteOn,degreeToNote(2,p))==1); }
    { // Leave frame mid-flight: pending target is forgotten and old note stops.
      Sim s(p);Hand r=at(0);s.step(&l,&r,4);s.clear();r=at(5);s.step(&l,&r);
      s.step(nullptr,&r);CHECK(count(s.ev,EvType::NoteOn)==0);CHECK(count(s.ev,EvType::NoteOff)==1);
      CHECK(s.e.leadSlot==-1); }
}
// Snapshot of v1.0 recipes, used ONLY as a spectral regression reference.
static std::vector<float> oldTone(int mode,int note) {
    std::vector<float> out(24000);double ph=0,ph2=0,f=440*std::pow(2.,(note-69)/12.);float env=0,lp=0;
    for(int i=0;i<(int)out.size();++i) { float t=i/48000.f; ph+=f/48000;ph2+=2*f/48000;float raw;
      if(mode==Keys) { float target=.25f+.75f*std::exp(-t*2.5f);env+=(target-env)*(t<.004f?.05f:.001f);
        raw=(std::sin(6.2831853*ph)+.35f*std::sin(6.2831853*ph2)*std::exp(-t*4))*env*.35f; }
      else { ph-=std::floor(ph);float saw=2*ph-1,tri=1-4*std::fabs(ph-.5);lp+=(.55f*saw+.45f*tri-lp)*.18f;env+=(1-env)*.003f;raw=lp*env*.45f; }
      out[i]=std::tanh(raw);
    } return out;
}
static double rms(const std::vector<float>& b) { double e=0;for(float x:b)e+=x*x;return std::sqrt(e/b.size()); }
static double brightness(const std::vector<float>& b,int note) {
    double f=440*std::pow(2.,(note-69)/12.),high=0,total=0;
    // Hann-windowed harmonic energy: >=4kHz vs all harmonic energy, normalized for level.
    for(int h=1;h*f<22000;++h) { double re=0,im=0;constexpr int n=8192;
        for(int i=0;i<n;++i) {double w=.5-.5*std::cos(6.283185307*i/(n-1));double a=6.283185307*h*f*i/48000;
            re+=b[4800+i]*w*std::cos(a);im+=b[4800+i]*w*std::sin(a);}
        double power=re*re+im*im;total+=power;if(h*f>=4000)high+=power;
    } return high/total;
}
static void testWarmDSP() {
    for(int mode : {Keys,Lead}) {
        Synth s;s.prepare(48000);s.mode=mode;std::vector<float> b(24000);s.noteOn(96,127);s.render(b.data(),(int)b.size(),1);
        auto old=oldTone(mode,96);double now=brightness(b,96),before=brightness(old,96);
        std::printf("%s >=4k harmonic energy: old %.6f, new %.6f\n",mode==Keys?"Keys":"Lead",before,now);
        CHECK(rms(b)>.015);CHECK(now<before*.25);CHECK(now<.01);
        s.noteOff(96);std::vector<float> tail(48000);s.render(tail.data(),(int)tail.size(),1);
        CHECK(std::fabs(tail.back())<1e-5);CHECK(s.activeVoices()==0);
        Synth quiet;quiet.prepare(48000);quiet.mode=mode;std::vector<float> q(24000);quiet.noteOn(96,30);quiet.render(q.data(),(int)q.size(),1);
        CHECK(rms(b)>rms(q)*2);
    }
    { // Lead pitch, expression and gate act on the new oscillator too.
        auto render=[](int bend,int expression) { Synth s;s.prepare(48000);s.mode=Lead;s.noteOn(69,127);s.pitchBend(bend);s.cc(11,expression);
            std::vector<float> b(24000);s.render(b.data(),(int)b.size(),1);return b; };
        auto plain=render(8192,127),bent=render(16383,127),silent=render(8192,0);
        auto crossings=[](const std::vector<float>& b) {int c=0;for(int i=4801;i<(int)b.size();++i)c+=b[i-1]<=0&&b[i]>0;return c;};
        CHECK(crossings(bent)>crossings(plain)*1.1);CHECK(rms(silent)<1e-6);
    }
    { // Envelopes and filters are consistent in seconds across sample rates.
        for(double sr : {44100.,48000.,96000.}) for(int mode : {Keys,Lead}) {
            Synth s;s.prepare(sr);s.mode=mode;s.noteOn(69,100);std::vector<float> b((int)(sr*.2));s.render(b.data(),(int)b.size(),1);CHECK(rms(b)>.03);
            s.noteOff(69);std::vector<float> tail((int)sr);s.render(tail.data(),(int)tail.size(),1);CHECK(s.activeVoices()==0);
        }
    }
}

// ================= v1.2 Strings mode =================
static Hand tipHand(Pt idx) { Hand h = hand(idx.x, idx.y + 0.2f); h.tip[1] = idx; return h; }
static Hand oneTip(Pt t) { Hand h; h.present = true; h.palm = {t.x, t.y + 0.2f}; for (auto& p : h.tip) p = h.palm; h.tip[1] = t; h.tipOk = {false, true, false, false, false}; return h; }
static Hand fullHand(Pt c) { Hand h; h.present = true; h.palm = {c.x, c.y + 0.15f}; for (int i = 0; i < 5; ++i) h.tip[(size_t)i] = {c.x + (i - 2) * 0.03f, c.y}; return h; }
static Pt lerpP(Pt a, Pt b, float k) { return {a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k}; }
// touch index tips at the midpoint, pull apart to a/b, hold still, then hands leave
static void drawString(Sim& s, Pt a, Pt b, int holdFrames = 25, bool leave = true) {
    Pt m = lerpP(a, b, 0.5f);
    Hand L = tipHand({m.x - 0.004f, m.y}), R = tipHand({m.x + 0.004f, m.y}); s.step(&L, &R, 15);
    for (int k = 1; k <= 20; ++k) { L = tipHand(lerpP(m, a, k / 20.f)); R = tipHand(lerpP(m, b, k / 20.f)); s.step(&L, &R); }
    s.step(&L, &R, holdFrames);
    if (leave) s.step(nullptr, nullptr, 2);
}
static Params strP(int key = 0, int scale = 1) { Params p; p.mode = Strings; p.key = key; p.scale = scale; return p; }
static bool near2(Pt a, Pt b, float tol = 0.012f) { return std::fabs(a.x - b.x) < tol && std::fabs(a.y - b.y) < tol; }

static void testStringsCreate() {
    { Sim s(strP()); drawString(s, {0.25f, 0.5f}, {0.75f, 0.45f});
      CHECK(s.e.strings.n == 1); CHECK(near2(s.e.strings.s[0].a, {0.25f, 0.5f})); CHECK(near2(s.e.strings.s[0].b, {0.75f, 0.45f}));
      CHECK(s.e.strings.s[0].seqDeg == 0); CHECK(count(s.ev, EvType::NoteOn) == 0); CHECK(s.e.strings.phase == StringField::Idle); }
    { // string is "set" while hands still there (hold still), and stays when hands move away
      Sim s(strP()); drawString(s, {0.3f, 0.3f}, {0.7f, 0.3f}, 25, false); CHECK(s.e.strings.n == 1);
      Hand L = tipHand({0.2f, 0.8f}), R = tipHand({0.8f, 0.8f}); s.step(&L, &R, 10); CHECK(s.e.strings.n == 1); CHECK(near2(s.e.strings.s[0].a, {0.3f, 0.3f})); }
    { // brief touch (< 0.15 s) never starts a string
      Sim s(strP()); Hand L = tipHand({0.496f, 0.5f}), R = tipHand({0.504f, 0.5f}); s.step(&L, &R, 6);
      for (int k = 1; k <= 20; ++k) { L = tipHand({0.5f - 0.02f * k, 0.5f}); R = tipHand({0.5f + 0.02f * k, 0.5f}); s.step(&L, &R); }
      s.step(&L, &R, 40); CHECK(s.e.strings.n == 0); }
    { // tips passing close for one frame: nothing
      Sim s(strP()); for (int k = -10; k <= 10; ++k) { Hand L = tipHand({0.5f + 0.01f * k, 0.5f}), R = tipHand({0.5f - 0.01f * k, 0.52f}); s.step(&L, &R); }
      s.step(nullptr, nullptr, 30); CHECK(s.e.strings.n == 0); }
    { // one hand alone cannot create
      Sim s(strP()); Hand R = tipHand({0.5f, 0.5f}); s.step(nullptr, &R, 40); CHECK(s.e.strings.n == 0); CHECK(s.e.strings.phase == StringField::Idle); }
    { // too short when a hand leaves: cancelled; long enough when a hand leaves: kept
      Sim s(strP()); Hand L = tipHand({0.498f, 0.5f}), R = tipHand({0.502f, 0.5f}); s.step(&L, &R, 15); CHECK(s.e.strings.phase == StringField::Stretch);
      L = tipHand({0.47f, 0.5f}); R = tipHand({0.53f, 0.5f}); s.step(&L, &R, 2); s.step(nullptr, &R); CHECK(s.e.strings.n == 0);
      s.step(nullptr, nullptr, 5); L = tipHand({0.498f, 0.5f}); R = tipHand({0.502f, 0.5f}); s.step(&L, &R, 15);
      for (int k = 1; k <= 10; ++k) { L = tipHand({0.5f - 0.03f * k, 0.5f}); R = tipHand({0.5f + 0.03f * k, 0.5f}); s.step(&L, &R); }
      s.step(&L, nullptr); CHECK(s.e.strings.n == 1); CHECK(std::fabs(stringLength(s.e.strings.s[0]) - 0.6f * kAspect) < 0.02f); }
    { // max 8 strings; the 9th gesture is ignored
      Sim s(strP()); for (int i = 0; i < 9; ++i) drawString(s, {0.2f, 0.1f + i * 0.09f}, {0.8f, 0.1f + i * 0.09f});
      CHECK(s.e.strings.n == kMaxStrings); CHECK(s.e.strings.created == kMaxStrings);
      bool distinct = true; for (int i = 0; i < 8; ++i) for (int j = i + 1; j < 8; ++j) distinct &= s.e.strings.s[i].colour != s.e.strings.s[j].colour; CHECK(distinct); }
    { // mode switch drops a half-made string but keeps finished ones
      Sim s(strP()); drawString(s, {0.2f, 0.3f}, {0.8f, 0.3f}); Hand L = tipHand({0.498f, 0.6f}), R = tipHand({0.502f, 0.6f}); s.step(&L, &R, 15);
      CHECK(s.e.strings.phase == StringField::Stretch); std::vector<Ev> o; Params k = strP(); k.mode = Keys; s.e.setParams(k, o);
      CHECK(s.e.strings.phase == StringField::Idle); CHECK(s.e.strings.n == 1); }
    { // corrupt saved data is cleaned up
      StringField f; StringDef d[3] = {{{0.1f, 0.1f}, {0.9f, 0.1f}, 0, 0}, {{NAN, 0.2f}, {0.9f, 0.2f}, 1, 1}, {{-3.f, 0.5f}, {4.f, 0.5f}, 999, -1}};
      f.setAll(d, 3); CHECK(f.n == 2); CHECK(f.s[1].a.x == 0.f && f.s[1].b.x == 1.f); CHECK(f.s[1].seqDeg == 63); CHECK(f.s[1].colour >= 0 && f.s[1].colour < 8); }
}

static void testStringsPitch() {
    const int eMinor[8] = {64, 66, 67, 69, 71, 72, 74, 76};
    { Params p = strP(4, 1); StringField f; for (int i = 0; i < 8; ++i) CHECK(f.add({0.2f, 0.1f * i + 0.1f}, {0.8f, 0.1f * i + 0.1f}));
      for (int i = 0; i < 8; ++i) CHECK(f.noteOf(i, p) == eMinor[i]);
      f.undo(); CHECK(f.n == 7); CHECK(f.add({0.2f, 0.9f}, {0.7f, 0.9f})); CHECK(f.noteOf(7, p) == 76);   // reuses the freed note
      f.undo(); f.undo(); f.undo(); CHECK(f.add({0.2f, 0.9f}, {0.7f, 0.9f})); CHECK(f.noteOf(5, p) == 72);
      Params g = strP(7, 0); CHECK(f.noteOf(0, g) == 67); CHECK(f.noteOf(2, g) == 71);                    // key/scale change retunes
      Params o = p; o.octave = -1; CHECK(f.noteOf(0, o) == 52);
      f.clear(); CHECK(f.n == 0); CHECK(f.add({0.2f, 0.5f}, {0.8f, 0.5f})); CHECK(f.noteOf(0, p) == 64); }
    { // length mode: longer = lower, always in key
      Params p = strP(4, 1); p.lengthPitch = true; StringField f;
      f.add({0.1f, 0.5f}, {0.1f + 0.9f / kAspect, 0.5f});   // 0.9 long
      f.add({0.4f, 0.3f}, {0.4f + 0.3f / kAspect, 0.3f});   // 0.3 long
      f.add({0.05f, 0.7f}, {0.95f, 0.7f});                  // ~1.6 long
      CHECK(f.noteOf(0, p) == 64); CHECK(f.noteOf(1, p) > f.noteOf(0, p)); CHECK(f.noteOf(2, p) < f.noteOf(0, p));
      const auto& st = scaleSteps(1, false);
      for (int i = 0; i < 3; ++i) { int pc = ((f.noteOf(i, p) - 4) % 12 + 12) % 12; CHECK(std::find(st.begin(), st.end(), pc) != st.end()); }
      Params q = p; q.lengthPitch = false; CHECK(f.noteOf(1, q) == 66); }
}

static void testStringsPluck() {
    auto withString = [](Params p = strP()) { Sim s(p); s.e.strings.add({0.3f, 0.5f}, {0.7f, 0.5f}); return s; };
    { // one crossing = one note, MIDI held 600 ms then auto-off
      Sim s = withString(); Hand R = oneTip({0.5f, 0.42f}); s.step(nullptr, &R, 3); R = oneTip({0.5f, 0.56f}); s.step(nullptr, &R); s.step(nullptr, &R, 5);
      CHECK(count(s.ev, EvType::NoteOn) == 1); CHECK(count(s.ev, EvType::NoteOn, 60) == 1);
      if (!s.ev.empty()) { CHECK(s.ev[0].autoOff); CHECK(s.ev[0].holdMs == kPluckHoldMs); }
      CHECK(s.e.strings.fxSeq == 1); CHECK(std::fabs(s.e.strings.fx[0].u - 0.5f) < 0.02f); }
    { // jitter on the line never re-triggers; leave and come back = new note
      Sim s = withString(); Hand R = oneTip({0.5f, 0.42f}); s.step(nullptr, &R, 3); R = oneTip({0.5f, 0.49f}); s.step(nullptr, &R);
      for (int i = 0; i < 30; ++i) { R = oneTip({0.5f, i % 2 ? 0.49f : 0.512f}); s.step(nullptr, &R); }
      CHECK(count(s.ev, EvType::NoteOn) == 1); s.clear();
      R = oneTip({0.5f, 0.40f}); s.step(nullptr, &R, 3); R = oneTip({0.5f, 0.6f}); s.step(nullptr, &R); CHECK(count(s.ev, EvType::NoteOn) == 1); }
    { // fast back-and-forth on one string inside 90 ms: one note
      Sim s = withString(); Hand R = oneTip({0.5f, 0.4f}); s.step(nullptr, &R, 3);
      R = oneTip({0.5f, 0.6f}); s.step(nullptr, &R); R = oneTip({0.5f, 0.4f}); s.step(nullptr, &R); CHECK(count(s.ev, EvType::NoteOn) == 1);
      s.step(nullptr, &R, 6); R = oneTip({0.5f, 0.6f}); s.step(nullptr, &R); CHECK(count(s.ev, EvType::NoteOn) == 2); }
    { // outside the string, at its very ends, slow drift, or hand vanishing/reappearing on the other side: nothing
      Sim s = withString(); Hand R = oneTip({0.75f, 0.4f}); s.step(nullptr, &R, 3); R = oneTip({0.75f, 0.6f}); s.step(nullptr, &R); s.step(nullptr, nullptr);
      R = oneTip({0.302f, 0.4f}); s.step(nullptr, &R, 3); R = oneTip({0.302f, 0.6f}); s.step(nullptr, &R); s.step(nullptr, nullptr);
      for (int i = 0; i < 60; ++i) { R = oneTip({0.55f, 0.47f + 0.002f * i}); s.step(nullptr, &R); }
      s.step(nullptr, nullptr); R = oneTip({0.45f, 0.4f}); s.step(nullptr, &R, 3); s.step(nullptr, nullptr); R = oneTip({0.45f, 0.6f}); s.step(nullptr, &R, 3);
      CHECK(count(s.ev, EvType::NoteOn) == 0); }
    { // hands swapping labels (left/right identity jump) is not a giant swipe
      Sim s = withString(); Hand L = oneTip({0.35f, 0.3f}), R = oneTip({0.65f, 0.7f}); s.step(&L, &R, 3); s.step(&R, &L); CHECK(count(s.ev, EvType::NoteOn) == 0); }
    { // swipe speed -> velocity; sensitivity lowers the speed needed
      auto vel = [&](float step, float sens) { Params p = strP(); p.sensitivity = sens; Sim s = withString(p); Hand R = oneTip({0.5f, 0.4f}); s.step(nullptr, &R, 3);
          R = oneTip({0.5f, 0.5f - step * 0.5f}); s.step(nullptr, &R);
          R = oneTip({0.5f, 0.5f + step * 0.5f}); s.step(nullptr, &R); return s.ev.empty() ? -1 : (int)s.ev[0].vel; };
      CHECK(vel(0.30f, 0.5f) > vel(0.03f, 0.5f)); CHECK(vel(0.03f, 0.5f) > 0); CHECK(vel(0.004f, 0.f) == -1); CHECK(vel(0.004f, 1.f) > 0); }
    { // whole-hand strum across 3 strings: 3 notes, one each, in crossing order; strum back up reverses
      Sim s(strP(4, 1)); for (float y : {0.4f, 0.5f, 0.6f}) s.e.strings.add({0.2f, y}, {0.8f, y});
      Hand R = fullHand({0.5f, 0.3f}); s.step(nullptr, &R, 3); R = fullHand({0.52f, 0.7f}); s.step(nullptr, &R);
      CHECK(count(s.ev, EvType::NoteOn) == 3); if (s.ev.size() == 3) { CHECK(s.ev[0].note == 64); CHECK(s.ev[1].note == 66); CHECK(s.ev[2].note == 67); }
      s.step(nullptr, &R, 6); s.clear(); R = fullHand({0.5f, 0.3f}); s.step(nullptr, &R);
      CHECK(count(s.ev, EvType::NoteOn) == 3); if (s.ev.size() == 3) { CHECK(s.ev[0].note == 67); CHECK(s.ev[2].note == 64); }
      CHECK(s.e.strings.plucks == 6); }
    { // both hands strum at once: each string still plays once inside the debounce window
      Sim s(strP()); for (float y : {0.4f, 0.6f}) s.e.strings.add({0.1f, y}, {0.9f, y});
      Hand L = fullHand({0.3f, 0.3f}), R = fullHand({0.7f, 0.3f}); s.step(&L, &R, 3); L = fullHand({0.3f, 0.7f}); R = fullHand({0.7f, 0.7f}); s.step(&L, &R);
      CHECK(count(s.ev, EvType::NoteOn) == 2); }
    { // a just-made string ignores the hands that made it, then plays
      Sim s(strP()); drawString(s, {0.3f, 0.5f}, {0.7f, 0.5f}, 25, false); CHECK(s.e.strings.n == 1);
      Hand L = tipHand({0.3f, 0.5f}), R = fullHand({0.5f, 0.42f}); s.step(&L, &R); R = fullHand({0.5f, 0.6f}); s.step(&L, &R); CHECK(count(s.ev, EvType::NoteOn) == 0);
      s.step(&L, &R, 20); R = fullHand({0.5f, 0.4f}); s.step(&L, &R); CHECK(count(s.ev, EvType::NoteOn) == 1); }
    { // creating a string never plucks existing ones it is drawn across
      Sim s(strP()); s.e.strings.add({0.5f, 0.2f}, {0.5f, 0.8f}); drawString(s, {0.3f, 0.5f}, {0.7f, 0.5f}); CHECK(s.e.strings.n == 2); CHECK(count(s.ev, EvType::NoteOn) == 0); }
    { // mouse: drag across = strum; click near = pluck; far click = nothing
      StringField f; Params p = strP(); std::vector<Ev> o; f.add({0.2f, 0.4f}, {0.8f, 0.4f}); f.add({0.2f, 0.6f}, {0.8f, 0.6f});
      CHECK(f.strumSegment({0.5f, 0.3f}, {0.5f, 0.7f}, 2.f, 1.0, p, o) == 2); CHECK(o.size() == 2); if (o.size() == 2) CHECK(o[0].note == 60 && o[1].note == 62);
      CHECK(f.pluckNear({0.4f, 0.61f}, 2.0, p, o) == 1); CHECK(f.pluckNear({0.4f, 0.5f}, 3.0, p, o) == 0); CHECK(f.pluckNear({0.95f, 0.4f}, 4.0, p, o) == 0); }
    { // other modes never pluck strings; Strings mode ignores drum jabs and keyboard presses
      Params p = strP(); p.mode = Drum; Sim s(p); s.e.strings.add({0.3f, 0.5f}, {0.7f, 0.5f}); Hand R = oneTip({0.5f, 0.42f}); s.step(nullptr, &R, 3); R = oneTip({0.5f, 0.56f}); s.step(nullptr, &R);
      CHECK(s.e.strings.plucks == 0);
      Sim q(strP()); Hand D = hand(0.8f, 0.4f); q.step(nullptr, &D, 3); D = hand(0.8f, 0.33f); q.step(nullptr, &D, 3); CHECK(q.ev.empty()); }
    { // long random run: every event is a paired one-shot note; never more than 8 strings
      Sim s(strP()); uint32_t r = 99; auto rnd = [&] { r = r * 1664525u + 1013904223u; return (float)(r >> 8) / 16777216.f; };
      for (int i = 0; i < 4000; ++i) { Hand L = fullHand({rnd(), rnd()}), R = fullHand({rnd(), rnd()}); if (i % 7 == 0) { L = tipHand({0.5f, 0.5f}); R = tipHand({0.51f, 0.5f}); }
          s.step(rnd() > 0.1f ? &L : nullptr, rnd() > 0.1f ? &R : nullptr); }
      CHECK(s.e.strings.n <= kMaxStrings); bool ok = true; for (auto& e : s.ev) ok &= e.type == EvType::NoteOn && e.autoOff && e.vel >= 1 && e.vel <= 127; CHECK(ok); }
}

static double autocorrHz(const std::vector<float>& b, int from, int len, double sr, double expect) {
    int lo = (int)(sr / (expect * 1.1)), hi = (int)(sr / (expect * 0.9)); double best = -1e9; int bl = lo;
    for (int l = lo; l <= hi; ++l) { double c = 0; for (int i = from; i < from + len; ++i) c += (double)b[(size_t)i] * b[(size_t)(i + l)]; if (c > best) { best = c; bl = l; } }
    // parabolic refinement
    auto C = [&](int l) { double c = 0; for (int i = from; i < from + len; ++i) c += (double)b[(size_t)i] * b[(size_t)(i + l)]; return c; };
    double y0 = C(bl - 1), y1 = C(bl), y2 = C(bl + 1), d = (y0 - y2) / (2 * (y0 - 2 * y1 + y2));
    return sr / (bl + (std::isfinite(d) ? d : 0));
}
static void testPluckDSP() {
    for (double sr : {44100., 48000., 96000., 192000.}) {
        { // pitch is right (within 0.6%) for low, mid and high strings
            for (int note : {45, 57, 69, 81}) { Synth s; s.prepare(sr); s.mode = Strings; s.noteOn(note, 110); std::vector<float> b((size_t)(sr * 0.5)); s.render(b.data(), (int)b.size(), 0.5f);
                double f = 440.0 * std::pow(2.0, (note - 69) / 12.0), got = autocorrHz(b, (int)(sr * 0.1), (int)(sr * 0.1), sr, f);
                if (std::fabs(got / f - 1) > 0.006) std::printf("pluck pitch sr=%.0f note %d: want %.2f got %.2f\n", sr, note, f, got);
                CHECK(std::fabs(got / f - 1) < 0.006); }
        }
        { // loudest possible strum (8 strings, full velocity, max volume): under the ceiling, no NaN, rings out
            Synth s; s.prepare(sr); s.mode = Strings; for (int n : {40, 43, 45, 47, 50, 52, 55, 57}) s.noteOn(n, 127);
            std::vector<float> b((size_t)(sr * 13)); s.render(b.data(), (int)b.size(), 1.6f);
            float pk = 0; bool fin = true; for (float x : b) { pk = std::max(pk, std::fabs(x)); fin &= std::isfinite(x); }
            CHECK(fin); CHECK(pk < 0.98855f); CHECK(pk > 0.3f); CHECK(s.activePlucks() == 0); CHECK(std::fabs(b.back()) < 1e-6f);
        }
        { // 50 rapid re-plucks of the same note + 30 others: voices capped, ceiling holds
            Synth s; s.prepare(sr); s.mode = Strings; std::vector<float> b((size_t)(sr * 0.02)); float pk = 0; bool fin = true;
            for (int i = 0; i < 80; ++i) { s.noteOn(i < 50 ? 64 : 40 + i % 30, 127); std::fill(b.begin(), b.end(), 0.f); s.render(b.data(), (int)b.size(), 1.6f);
                for (float x : b) { pk = std::max(pk, std::fabs(x)); fin &= std::isfinite(x); } }
            CHECK(fin); CHECK(pk < 0.98855f); CHECK(s.activePlucks() <= 12);
        }
    }
    { // one pluck at default volume is clearly audible but not hot; velocity changes level; note-off damps the ring
        auto run = [](int vel, bool off) { Synth s; s.prepare(48000); s.mode = Strings; s.noteOn(60, vel); std::vector<float> b(48000 * 2);
            s.render(b.data(), 12000, 1.28f); if (off) s.noteOff(60); s.render(b.data() + 12000, (int)b.size() - 12000, 1.28f); return b; };
        auto hard = run(127, false), soft = run(30, false), damped = run(127, true);
        auto seg = [](const std::vector<float>& b, int a, int n) { double e = 0; for (int i = a; i < a + n; ++i) e += (double)b[(size_t)i] * b[(size_t)i]; return std::sqrt(e / n); };
        float pk = 0; for (float x : hard) pk = std::max(pk, std::fabs(x));
        std::printf("pluck: peak %.3f (%.1f dBFS), rms first 0.25 s %.4f\n", pk, 20 * std::log10(pk), seg(hard, 0, 12000));
        CHECK(pk > 0.15f && pk < 0.8f); CHECK(seg(hard, 0, 12000) > 0.03); CHECK(seg(hard, 0, 12000) > seg(soft, 0, 12000) * 1.8);
        CHECK(seg(hard, 48000, 24000) > 0.002); CHECK(seg(damped, 48000, 24000) < seg(hard, 48000, 24000) * 0.3);
        CHECK(seg(hard, 0, 12000) > seg(hard, 48000, 24000) * 2.5); }  // it decays like a plucked string
    { // drums/keys/lead also stay under the ceiling at max volume (ceiling applies to every mode)
        for (int mode : {Drum, Keys, Lead}) { Synth s; s.prepare(48000); s.mode = mode; for (int n : {36, 38, 39, 42, 60, 64, 67, 72}) s.noteOn(n, 127);
            std::vector<float> b(24000); s.render(b.data(), (int)b.size(), 1.6f); float pk = 0; for (float x : b) pk = std::max(pk, std::fabs(x)); CHECK(pk < 0.98855f); }
    }
}

int main() {
    testDrums(); testKeys(); testLead(); testHatRollAndTracker();
    testAirPiano(); testLeadLandings(); testWarmDSP();
    testStringsCreate(); testStringsPitch(); testStringsPluck(); testPluckDSP();
    std::printf("engine tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
