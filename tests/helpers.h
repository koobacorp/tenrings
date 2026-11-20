#pragma once
#include "engine/Engine.h"
#include <cstdio>
#include <string>
static int g_pass = 0, g_fail = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
namespace th {
using namespace tr;
// Angled/top-down palm-down piano hand: knuckles project forward from palm.
// A press dips the tip wristward in projection while the proximal finger stays extended.
inline Hand hand(float x, float y, std::array<float,5> curl = {0,0,0,0,0}) {
    Hand h; h.present = true; h.palm = {x, y};
    const float size = 0.08f;
    for (int i = 0; i < 5; ++i) {
        float fx = x + (i - 2) * 0.02f;
        h.mcp[(size_t)i] = {fx, y - size};
        float reach = (i == 0 ? 0.85f : i == 4 ? 1.0f : 1.2f);
        h.pip[i] = {fx, y - size - size * reach * 0.45f};
        h.hasPip[i] = true;
        h.tip[(size_t)i] = {fx, y - size - size * reach + size * curl[(size_t)i]};
    }
    h.mcp[2] = {x, y - size};
    return h;
}
inline Frame frame(double t, const Hand* L, const Hand* R) { Frame f; f.t = t; if (L) f.hand[0] = *L; if (R) f.hand[1] = *R; return f; }
inline int count(const std::vector<Ev>& v, EvType t, int note = -1) { int c = 0; for (auto& e : v) if (e.type == t && (note < 0 || e.note == note)) ++c; return c; }
}
