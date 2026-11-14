// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#pragma once
// "3D 2.0" lit knobs, JUCE-free. Each knob is a height field (radial profile plus angular detail)
// with per-segment materials, shaded per pixel in linear light under a fixed studio rig and seen by a
// slightly tilted camera, so the side wall and the cast shadow are visible. render() returns a
// premultiplied 0xAARRGGBB Sprite that a host blits over any panel. The light stays fixed in panel
// space while the knob turns. Angles follow the classic knobs: zero is straight up, clockwise positive.
// This is a port of GoodLookinDri's knob3d module and is meant to produce the same pictures.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace goodlookinui::knob3d {

inline constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------------------- vectors
struct V3 { float x = 0, y = 0, z = 0; };
inline V3 v3(float x, float y, float z) { return {x, y, z}; }
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline V3 operator*(V3 a, V3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline V3& operator+=(V3& a, V3 b) { a = a + b; return a; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline V3 norm(V3 a) { return a * (1.0f / std::sqrt(std::max(dot(a, a), 1e-24f))); }
inline V3 grey(float g) { return {g, g, g}; }
inline V3 lerp3(V3 a, V3 b, float t) { return a + (b - a) * t; }
inline V3 reflect(V3 i, V3 n) { return i - n * (2.0f * dot(i, n)); }
inline float clamp01(float x) { return std::min(1.0f, std::max(0.0f, x)); }
// Smoothstep that also accepts e0 > e1 (falling edge).
inline float ss(float e0, float e1, float x) { float t = clamp01((x - e0) / (e1 - e0)); return t * t * (3.0f - 2.0f * t); }
inline float mixf(float a, float b, float t) { return a + (b - a) * t; }
inline float toLinear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
inline V3 srgb(float r, float g, float b) { return {toLinear(r), toLinear(g), toLinear(b)}; }
inline float remEuclid(float a, float b) { float r = std::fmod(a, b); return r < 0 ? r + std::fabs(b) : r; }

inline float hash2(int x, int y) {
    std::uint32_t h = std::uint32_t(x) * 0x8da6b343u ^ std::uint32_t(y) * 0xd8163841u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return float(h & 0xffffff) / 16777216.0f;
}
inline float vnoise(float x, float y) {
    const float xf = std::floor(x), yf = std::floor(y), fx = x - xf, fy = y - yf;
    const int xi = int(xf), yi = int(yf);
    const float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    const float a = hash2(xi, yi), b = hash2(xi + 1, yi), c = hash2(xi, yi + 1), d = hash2(xi + 1, yi + 1);
    return mixf(mixf(a, b, sx), mixf(c, d, sx), sy);
}
struct Rng {
    std::uint32_t s;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return float(s >> 8) / 16777216.0f; }
};
inline float tonemap(float c) {
    const float knee = 0.7f;
    return c <= knee ? c : knee + (1.0f - knee) * (1.0f - std::exp(-(c - knee) / (1.0f - knee)));
}
inline float toSrgb(float c) {
    c = tonemap(std::max(c, 0.0f));
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// ---------------------------------------------------------------------------------------- model
// Colours are 0xAARRGGBB throughout.
struct Material {
    float base[3] = {0.5f, 0.5f, 0.5f};   // linear RGB
    float metallic = 0, roughness = 0.5f, f0 = 0.04f, anisotropy = 0, clearCoat = 0, grain = 0;
    float accent = 0, accentShade = 1, rings = 0, woodAmt = 0, sheen = 0, translucency = 0;

    static Material metal(float r, float g, float b, float roughness) {
        Material m; m.base[0] = toLinear(r); m.base[1] = toLinear(g); m.base[2] = toLinear(b);
        m.metallic = 1; m.roughness = roughness; return m;
    }
    static Material plastic(float r, float g, float b, float roughness) { auto m = metal(r, g, b, roughness); m.metallic = 0; return m; }
    Material& with_aniso(float a) { anisotropy = a; return *this; }
    Material& with_coat(float c) { clearCoat = c; return *this; }
    Material& with_grain(float g) { grain = g; return *this; }
    Material& with_accent(float a) { accent = a; return *this; }
    Material& with_accent_shade(float a, float s) { accent = a; accentShade = s; return *this; }
    Material& with_rings(float r) { rings = r; return *this; }
    Material& with_wood(float w) { woodAmt = w; return *this; }
    Material& with_sheen(float s) { sheen = s; return *this; }
    Material& with_translucency(float t) { translucency = t; return *this; }
    Material& with_roughness(float r) { roughness = r; return *this; }
    Material& with_metallic(float m) { metallic = m; return *this; }
    Material& with_f0(float f) { f0 = f; return *this; }

    static Material wood(float r, float g, float b) { return plastic(r, g, b, 0.55f).with_wood(1.0f).with_coat(0.85f); }
    static Material walnut() { return wood(0.36f, 0.20f, 0.10f); }
    static Material maple() { return wood(0.80f, 0.62f, 0.38f).with_wood(0.6f); }
    static Material brass() { return metal(0.93f, 0.74f, 0.36f, 0.16f); }
    static Material bronze() { return metal(0.70f, 0.46f, 0.26f, 0.30f).with_grain(0.03f); }
    static Material chrome() { return metal(0.95f, 0.95f, 0.96f, 0.04f); }
    static Material satin_aluminium() { return metal(0.93f, 0.93f, 0.925f, 0.42f).with_grain(0.02f); }
    static Material spun_aluminium() { return metal(0.90f, 0.905f, 0.91f, 0.09f).with_aniso(0.55f).with_rings(0.5f); }
    static Material die_cast() { return plastic(0.74f, 0.75f, 0.76f, 0.5f).with_metallic(0.5f).with_grain(0.05f); }
    static Material anodised(float r, float g, float b) { return metal(r, g, b, 0.34f).with_grain(0.02f); }
    static Material rubber(float r, float g, float b) { return plastic(r, g, b, 0.85f).with_f0(0.03f).with_sheen(0.6f); }
    static Material cream() { return plastic(0.86f, 0.80f, 0.64f, 0.42f).with_coat(0.35f); }
    static Material black_gloss() { return plastic(0.055f, 0.054f, 0.053f, 0.5f).with_coat(0.9f); }
    static Material black_polished() { return plastic(0.07f, 0.07f, 0.072f, 0.34f).with_f0(0.02f).with_coat(0.85f).with_grain(0.02f); }
    static Material black_matte() { return plastic(0.13f, 0.13f, 0.135f, 0.5f); }
    static Material translucent(float r, float g, float b) { return plastic(r, g, b, 0.5f).with_coat(1.0f).with_translucency(0.8f); }
    static Material gloss(float r, float g, float b) { return plastic(r, g, b, 0.5f).with_coat(0.9f); }
    static Material white_paint() { return plastic(0.94f, 0.93f, 0.89f, 0.6f); }
    static Material ink() { return plastic(0.03f, 0.03f, 0.033f, 0.9f); }

    Material tinted(std::uint32_t accentColour) const {
        Material m = *this;
        if (m.accent > 0) {
            const float c[3] = {float((accentColour >> 16) & 255), float((accentColour >> 8) & 255), float(accentColour & 255)};
            for (int i = 0; i < 3; ++i) {
                const float l = toLinear(c[i] / 255.0f) * m.accentShade;
                m.base[i] += (l - m.base[i]) * std::min(m.accent, 1.0f);
            }
        }
        return m;
    }
};

enum class Shape { Flat, Line, Dome, Fillet };
inline constexpr Shape Flat = Shape::Flat, Line = Shape::Line, Dome = Shape::Dome, Fillet = Shape::Fillet;

// One ring of the radial profile; radii and heights are in units of the knob radius.
struct Segment {
    float r0 = 0, r1 = 0, h0 = 0, h1 = 0; Shape shape = Shape::Flat; Material material;
    float height(float u) const {
        const float t = std::min(1.0f, std::max(0.0f, (u - r0) / std::max(r1 - r0, 1e-6f)));
        switch (shape) {
            case Shape::Flat: return h0;
            case Shape::Line: return h0 + (h1 - h0) * t;
            case Shape::Dome: return h1 + (h0 - h1) * std::sqrt(1.0f - t * t);
            case Shape::Fillet: return h1 + (h0 - h1) * (1.0f - std::sqrt(1.0f - (1.0f - t) * (1.0f - t)));
        }
        return h0;
    }
};
inline Segment seg(Shape s, float r0, float r1, float h0, float h1, const Material& m) { return {r0, r1, h0, h1, s, m}; }

enum class AngularKind { Knurl, Flutes, Scallops, Teeth, Bar };
// Angular detail that pulls the profile inwards, so it also shapes the silhouette.
struct Angular {
    AngularKind kind = AngularKind::Knurl;
    unsigned count = 0; float depth = 0, rIn = 0, rOut = 1;
    float duty = 0;                                    // Teeth
    float tipWidth = 0, tailWidth = 0, tail = 0;       // Bar
    static Angular knurl(unsigned n, float depth, float rIn) { Angular a; a.kind = AngularKind::Knurl; a.count = n; a.depth = depth; a.rIn = rIn; return a; }
    static Angular flutes(unsigned n, float depth, float rIn) { auto a = knurl(n, depth, rIn); a.kind = AngularKind::Flutes; return a; }
    static Angular scallops(unsigned n, float depth, float rIn) { auto a = knurl(n, depth, rIn); a.kind = AngularKind::Scallops; return a; }
    static Angular teeth(unsigned n, float depth, float duty, float rIn) { auto a = knurl(n, depth, rIn); a.kind = AngularKind::Teeth; a.duty = duty; return a; }
    Angular r_out(float v) const { Angular a = *this; a.rOut = v; return a; }
};

enum class PlanKind { Round, Polygon, Bar };
// Plan shape of a body: what a horizontal slice through it looks like.
struct Plan {
    PlanKind kind = PlanKind::Round;
    unsigned sides = 0; float rounding = 0;
    float tip = 0, tail = 0, tipWidth = 0, tailWidth = 0;
    static Plan round() { return {}; }
    static Plan polygon(unsigned sides, float rounding) { Plan p; p.kind = PlanKind::Polygon; p.sides = sides; p.rounding = rounding; return p; }
    static Plan bar(float tip, float tail, float tipWidth, float tailWidth, float rounding) {
        Plan p; p.kind = PlanKind::Bar; p.tip = tip; p.tail = tail; p.tipWidth = tipWidth; p.tailWidth = tailWidth; p.rounding = rounding; return p;
    }
};

// An extra solid on the knob (fin, beak, hub, second tier).
struct Part {
    std::vector<Segment> profile; Plan plan; std::vector<Angular> angular;
    static Part make(std::vector<Segment> p, Plan plan) { Part r; r.profile = std::move(p); r.plan = plan; return r; }
    Part& with_angular(const Angular& a) { angular.push_back(a); return *this; }
};

struct Insert { float r = 0, depth = 0; Material material; };
struct SetScrew { float angle = 0, height = 0, radius = 0; };

enum class PointerKind { Line, Groove, Dot, Ridge };
struct Pointer {
    PointerKind kind = PointerKind::Line; float r0 = 0, r1 = 0, width = 0, depth = 0; Material material;
    static Pointer line(float r0, float r1, float w, const Material& m) { return {PointerKind::Line, r0, r1, w, 0, m}; }
    static Pointer groove(float r0, float r1, float w, float depth, const Material& m) { return {PointerKind::Groove, r0, r1, w, depth, m}; }
    static Pointer dot(float r, float diameter, float depth, const Material& m) { return {PointerKind::Dot, r, r, diameter, depth, m}; }
    static Pointer ridge(float r0, float r1, float w, float height, const Material& m) { return {PointerKind::Ridge, r0, r1, w, height, m}; }
};

// Fixed studio lighting and camera, defined in panel space.
struct Rig {
    float keyAzimuth = 20.0f * kPi / 180.0f, keyElevation = 65.0f * kPi / 180.0f, keyIntensity = 1.0f, keySize = 0.15f;
    float shadowSoftness = 0.7f, ambient = 1.0f, environment = 1.0f;
    std::uint32_t floor = 0xffc0c0c0u;
    float tilt = 14.0f * kPi / 180.0f, shadow = 0.72f, occlusion = 0.6f, rim = 1.0f;
    static Rig studio() { return {}; }
};

// Premultiplied sRGB sprite, 0xAARRGGBB, row-major. origin is where the knob's base centre sits (device px).
struct Sprite { int w = 0, h = 0; std::vector<std::uint32_t> px; float ox = 0, oy = 0; };

struct KnobModel {
    std::vector<Segment> profile, under; std::vector<Angular> angular;
    bool hasPointer = false; Pointer pointer;
    float height = 1.0f, edge = 0.015f; Plan plan;
    std::vector<Part> parts; std::vector<Pointer> marks;
    bool hasInsert = false; Insert insert; bool hasScrew = false; SetScrew screw;

    static KnobModel make(std::vector<Segment> p) { KnobModel m; m.profile = std::move(p); return m; }
    // Stepped two-tier body: a wide lower ring of radius 1 and a cap of radius upperR.
    static KnobModel two_tier(float lowerH, float upperR, float upperH, const Material& lower, const Material& upper) {
        const float c = 0.035f;
        return make({seg(Dome, 0.0f, upperR - c, upperH + 0.012f, upperH, upper),
                     seg(Line, upperR - c, upperR, upperH, upperH - c, upper),
                     seg(Line, upperR, upperR + 0.012f, upperH - c, lowerH, upper),
                     seg(Flat, upperR + 0.012f, 0.95f - c, lowerH, lowerH, lower),
                     seg(Line, 0.95f - c, 0.95f, lowerH, lowerH - c, lower),
                     seg(Line, 0.95f, 0.975f, lowerH - c, 0.03f, lower)});
    }
    KnobModel& with_under(std::vector<Segment> u) { under = std::move(u); return *this; }
    KnobModel& with_angular(const Angular& a) { angular.push_back(a); return *this; }
    KnobModel& with_plan(const Plan& p) { plan = p; return *this; }
    KnobModel& with_part(const Part& p) { parts.push_back(p); return *this; }
    KnobModel& with_pointer(const Pointer& p) { hasPointer = true; pointer = p; return *this; }
    KnobModel& with_mark(const Pointer& p) { marks.push_back(p); return *this; }
    KnobModel& with_insert(float r, float depth, const Material& m) { hasInsert = true; insert = {r, depth, m}; return *this; }
    KnobModel& with_set_screw(float angle, float h, float r) { hasScrew = true; screw = {angle, h, r}; return *this; }
    KnobModel& with_edge(float e) { edge = e; return *this; }
    KnobModel& with_height(float h) { height = h; return *this; }

    // Where the knob appears to sit under the rig's tilted camera, in knob radii: how far the centre
    // of its silhouette lies above the base centre, and the radius enclosing the silhouette.
    std::pair<float, float> visualFrame(const Rig& rig) const {
        const float t = std::tan(rig.tilt) * height;
        float up = 0, down = 0, wide = 0;
        auto point = [&](float r, float h) { up = std::max(up, r + h * t); down = std::max(down, r - h * t); wide = std::max(wide, r); };
        auto addSegs = [&](const std::vector<Segment>& v) {
            for (auto& s : v) { point(s.r0, s.h0); point(s.r1, s.shape == Shape::Flat ? s.h0 : s.h1); }
        };
        addSegs(profile); addSegs(under);
        for (auto& p : parts) {
            float top = 0; for (auto& s : p.profile) top = std::max(top, std::max(s.h0, s.h1));
            if (p.plan.kind == PlanKind::Bar) point(p.plan.tip, 0.5f * top); else addSegs(p.profile);
        }
        return {0.5f * (up - down), std::max(wide, 0.5f * (up + down))};
    }
};

// ------------------------------------------------------------------------------------ hashing
struct Hasher {
    std::uint64_t h = 1469598103934665603ull;
    void bytes(const void* p, size_t n) { auto* b = static_cast<const unsigned char*>(p); for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; } }
    void f(float v) { bytes(&v, sizeof v); }
    void u(std::uint64_t v) { bytes(&v, sizeof v); }
};
inline void hashMat(Hasher& h, const Material& m) {
    for (float v : m.base) h.f(v);
    for (float v : {m.metallic, m.roughness, m.f0, m.anisotropy, m.clearCoat, m.grain, m.accent, m.accentShade, m.rings, m.woodAmt, m.sheen, m.translucency}) h.f(v);
}
inline void hashSegs(Hasher& h, const std::vector<Segment>& v) {
    h.u(v.size()); for (auto& s : v) { for (float x : {s.r0, s.r1, s.h0, s.h1}) h.f(x); h.u(unsigned(s.shape)); hashMat(h, s.material); }
}
inline void hashAng(Hasher& h, const std::vector<Angular>& v) {
    h.u(v.size()); for (auto& a : v) { h.u(unsigned(a.kind)); h.u(a.count); for (float x : {a.depth, a.rIn, a.rOut, a.duty, a.tipWidth, a.tailWidth, a.tail}) h.f(x); }
}
inline void hashPlan(Hasher& h, const Plan& p) { h.u(unsigned(p.kind)); h.u(p.sides); for (float x : {p.rounding, p.tip, p.tail, p.tipWidth, p.tailWidth}) h.f(x); }
inline void hashPtr(Hasher& h, const Pointer& p) { h.u(unsigned(p.kind)); for (float x : {p.r0, p.r1, p.width, p.depth}) h.f(x); hashMat(h, p.material); }
inline std::uint64_t fingerprint(const KnobModel& m) {
    Hasher h; hashSegs(h, m.profile); hashSegs(h, m.under); hashAng(h, m.angular);
    h.u(m.hasPointer); if (m.hasPointer) hashPtr(h, m.pointer);
    h.f(m.height); h.f(m.edge); hashPlan(h, m.plan);
    h.u(m.parts.size()); for (auto& p : m.parts) { hashSegs(h, p.profile); hashPlan(h, p.plan); hashAng(h, p.angular); }
    h.u(m.marks.size()); for (auto& p : m.marks) hashPtr(h, p);
    h.u(m.hasInsert); if (m.hasInsert) { h.f(m.insert.r); h.f(m.insert.depth); hashMat(h, m.insert.material); }
    h.u(m.hasScrew); if (m.hasScrew) { h.f(m.screw.angle); h.f(m.screw.height); h.f(m.screw.radius); }
    return h.h;
}
inline std::uint64_t fingerprint(const Rig& r, bool withFloor = true) {
    Hasher h; for (float x : {r.keyAzimuth, r.keyElevation, r.keyIntensity, r.keySize, r.shadowSoftness, r.ambient, r.environment, r.tilt, r.shadow, r.occlusion, r.rim}) h.f(x);
    h.u(withFloor ? r.floor : 0u); return h.h;
}

// ------------------------------------------------------------------------------------ parallel rows
template <class T, class F>
void parRows(std::vector<T>& data, int width, int height, F f) {
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const int threads = int(std::min<unsigned>(hw, unsigned(std::max(1, height / 8))));
    if (threads <= 1) {
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) data[size_t(y) * size_t(width) + size_t(x)] = f(x, y);
        return;
    }
    const int per = std::max(1, (height + threads - 1) / threads);
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        const int y0 = t * per, y1 = std::min(height, y0 + per);
        if (y0 >= y1) break;
        pool.emplace_back([&, y0, y1] { for (int y = y0; y < y1; ++y) for (int x = 0; x < width; ++x) data[size_t(y) * size_t(width) + size_t(x)] = f(x, y); });
    }
    for (auto& t : pool) t.join();
}

// ------------------------------------------------------------------------------------ field
namespace detail {
inline constexpr int LUT_N = 2048;
inline constexpr float LUT_SPAN = 1.12f;
inline constexpr int AO_DIRS = 10;
inline constexpr float AO_ROT = 0.3f;
inline constexpr float CURV = 2.0f;

struct Mat { V3 diff, f0; float rough = 0, aniso = 0, coat = 0, sheen = 0, trans = 0; };
struct Surf { float gx = 0, gy = 0, bx = 0, by = 0, ao = 1, curv = 0; Mat mat; };
struct Decal { float ang = 0, z = 0, rad = 1; };

struct Mod { Angular a; float amp = 0; float cut0 = 0, cut1 = 0; float fade = 0.22f; };
struct Ptr { PointerKind kind; float r0, r1, hw, depth; Material mat; };

struct Table {
    std::vector<float> lut; std::vector<std::pair<float, Material>> segs; float zmax = 0, k = 1;
    Table(const std::vector<Segment>& profile, float r, float hs, float edge, std::uint32_t accent) {
        auto rawAt = [&](float u) -> float {
            for (auto& s : profile) if (u < s.r1) return u >= s.r0 ? s.height(u) * hs : 0.0f;
            return 0.0f;
        };
        std::vector<float> raw(LUT_N);
        for (int i = 0; i < LUT_N; ++i) raw[size_t(i)] = rawAt(float(i) / float(LUT_N - 1) * LUT_SPAN);
        const float binsPerPx = float(LUT_N - 1) / (LUT_SPAN * r);
        const float sigma = std::max(edge * r, 0.55f) * binsPerPx;
        const int rad = int(std::ceil(sigma * 3.0f));
        std::vector<float> wts; float wsum = 0;
        for (int k2 = -rad; k2 <= rad; ++k2) { wts.push_back(std::exp(-float(k2) * float(k2) / (2.0f * sigma * sigma))); wsum += wts.back(); }
        lut.assign(LUT_N, 0.0f);
        for (int i = 0; i < LUT_N; ++i) {
            float acc = 0;
            for (int k2 = -rad; k2 <= rad; ++k2) { const int j = std::min(std::abs(i + k2), LUT_N - 1); acc += raw[size_t(j)] * wts[size_t(k2 + rad)]; }
            lut[size_t(i)] = acc / wsum; zmax = std::max(zmax, lut[size_t(i)]);
        }
        for (auto& s : profile) segs.push_back({s.r1, s.material.tinted(accent)});
        k = binsPerPx;
    }
    float at(float rho) const {
        const float f = std::max(rho, 0.0f) * k;
        if (f >= float(LUT_N - 1)) return 0.0f;
        const int i = int(f);
        return mixf(lut[size_t(i)], lut[size_t(i) + 1], f - float(i));
    }
    Material mat(float u) const {
        for (auto& s : segs) if (u < s.first) return s.second;
        return segs.empty() ? Material::plastic(0.5f, 0.5f, 0.5f, 0.5f) : segs.back().second;
    }
};

inline float smin(float a, float b, float k) {
    if (k <= 1e-5f) return std::min(a, b);
    const float h = std::max(k - std::fabs(a - b), 0.0f) / k;
    return std::min(a, b) - h * h * k * 0.25f;
}

// Polar radius of a tapered bar (tip at 1 ahead, tail behind), corners rounded.
inline float barRadius(float phi, float tipW, float tailW, float tail) {
    const float s = std::fabs(std::sin(phi)), c = std::cos(phi);
    float acc = 0;
    if (c > 1e-3f) acc += std::pow(c, 8.0f);
    if (c < -1e-3f) acc += std::pow(-c / std::max(tail, 0.05f), 8.0f);
    const float b = (tipW - tailW) / (1.0f + tail), a = std::max(tailW + b * tail, 0.02f), den = s - b * c;
    if (den > 1e-4f) acc += std::pow(den / a, 8.0f);
    return std::min(std::pow(std::max(acc, 1e-6f), -0.125f), 1.0f);
}

struct Layer {
    Table table; Plan plan; std::vector<Mod> mods; float modFrom = std::numeric_limits<float>::max();
    Layer(const std::vector<Segment>& profile, Plan plan_, const std::vector<Angular>& angular, float r, float hs, float edge, std::uint32_t accent)
        : table(profile, r, hs, edge, accent), plan(plan_) {
        for (auto& a : angular) {
            if (!(a.kind == AngularKind::Bar || (a.count > 0 && a.depth > 0))) continue;
            Mod m; m.a = a; m.amp = a.depth;
            switch (a.kind) {
                case AngularKind::Knurl: {
                    const float pitch = 2.0f * kPi * r * std::max(a.rIn, 0.3f) / float(a.count);
                    m.amp *= ss(1.8f, 3.6f, pitch); m.fade = 0.04f; break; }
                case AngularKind::Scallops: {
                    const float e = 1.0f - a.depth, ca = std::cos(0.3f * 2.0f * kPi / float(a.count));
                    const float rc = e < ca - 1e-3f ? (2.0f * e * ca - 1.0f - e * e) / (2.0f * (e - ca)) : 0.2f;
                    m.cut0 = e + rc; m.cut1 = rc; m.fade = 0.03f; break; }
                case AngularKind::Flutes: m.fade = 0.08f; break;
                case AngularKind::Teeth: m.fade = 0.03f; break;
                case AngularKind::Bar: break;
            }
            mods.push_back(m);
        }
        for (auto& m : mods) modFrom = std::min(modFrom, m.a.rIn - m.fade);
    }

    // Pseudo-radius in pixels: the radius at which the round profile is read.
    float rho(float x, float y, float rr, float r) const {
        float out = rr; float phi = 0; bool havePhi = false;
        if (plan.kind == PlanKind::Polygon) {
            const float n = float(std::max(plan.sides, 3u));
            phi = std::atan2(x, y); havePhi = true;
            const float a = kPi / n;
            const float th = (remEuclid(phi / (2.0f * a) + 0.5f, 1.0f) - 0.5f) * 2.0f * a;
            const float eps = std::min(1.0f, std::max(0.0f, plan.rounding)) * a * 0.9f;
            const float ths = a - std::sqrt((a - std::fabs(th)) * (a - std::fabs(th)) + eps * eps);
            out = rr * std::cos(ths) / std::cos(a - eps);
        } else if (plan.kind == PlanKind::Bar) {
            const float xn = std::fabs(x) / r, yn = y / r, len = std::max(plan.tip + plan.tail, 1e-3f);
            const float w = plan.tailWidth + (plan.tipWidth - plan.tailWidth) * ((yn + plan.tail) / len);
            const float dSide = (w - xn) * len / std::sqrt(len * len + (plan.tipWidth - plan.tailWidth) * (plan.tipWidth - plan.tailWidth));
            const float d = smin(smin(plan.tip - yn, yn + plan.tail, plan.rounding), dSide, plan.rounding);
            return (1.0f - d) * r;
        }
        const float u = rr / r;
        if (mods.empty() || u < modFrom) return out;
        if (!havePhi) phi = std::atan2(x, y);
        for (auto& m : mods) {
            const float w = ss(m.a.rIn - m.fade, m.a.rIn, u) * (m.a.rOut >= 1.0f ? 1.0f : ss(m.a.rOut + 0.08f, m.a.rOut, u));
            if (w <= 0) continue;
            const float n = float(m.a.count); float scale = 1;
            switch (m.a.kind) {
                case AngularKind::Flutes: scale = 1.0f - m.amp * (0.5f + 0.5f * std::cos(n * phi)); break;
                case AngularKind::Knurl: { const float c = std::fabs(std::sin(0.5f * n * phi)); scale = 1.0f - m.amp * (1.0f - c) * (1.0f - c); break; }
                case AngularKind::Scallops: {
                    const float pitch = 2.0f * kPi / n, d = remEuclid(phi / pitch, 1.0f) * pitch - 0.5f * pitch;
                    const float dc = m.cut0, rc = m.cut1, sd = std::sin(d);
                    const float disc = rc * rc - dc * dc * sd * sd;
                    scale = disc > 0 ? std::min(dc * std::cos(d) - std::sqrt(disc), 1.0f) : 1.0f; break; }
                case AngularKind::Teeth: {
                    const float d = std::fabs(remEuclid(phi * n / (2.0f * kPi), 1.0f) - 0.5f);
                    const float half = 0.5f * std::min(0.98f, std::max(0.02f, m.a.duty));
                    scale = 1.0f - m.amp * ss(0.5f - half + 0.03f, 0.5f - half - 0.03f, d); break; }
                case AngularKind::Bar: scale = barRadius(phi, m.a.tipWidth, m.a.tailWidth, m.a.tail); break;
            }
            out /= std::max(mixf(1.0f, scale, w), 0.04f);
        }
        return out;
    }
};

inline Mat blend(const Mat& a, const Mat& b, float k) {
    return {lerp3(a.diff, b.diff, k), lerp3(a.f0, b.f0, k), mixf(a.rough, b.rough, k), mixf(a.aniso, b.aniso, k), mixf(a.coat, b.coat, k), mixf(a.sheen, b.sheen, k), mixf(a.trans, b.trans, k)};
}

template <class H>
float horizonAo(const H& height, float x, float y, float h0, float rot, float sc, int dirs) {
    static const float DIST[6] = {1.5f, 4.5f, 10.0f, 19.0f, 34.0f, 56.0f};
    float occ = 0;
    for (int d = 0; d < dirs; ++d) {
        const float a = rot + float(d) * (2.0f * kPi / float(dirs)), s = std::sin(a), c = std::cos(a);
        float maxs = 0;
        for (float t0 : DIST) { const float t = t0 * sc; maxs = std::max(maxs, (height(x + c * t, y + s * t) - h0) / t); }
        occ += maxs / std::sqrt(1.0f + maxs * maxs);
    }
    return 1.0f - occ / float(dirs);
}

struct Field {
    float r = 0, zmaxPx = 0;
    std::vector<Layer> layers; std::vector<Ptr> ptrs;
    bool hasInsert = false; float insR = 0, insDepth = 0; Material insMat;
    bool hasScrew = false; Decal screw;

    Field(const KnobModel& model, float radiusPx, std::uint32_t accent) {
        r = std::max(radiusPx, 2.0f);
        const float hs = model.height * r;
        layers.reserve(2 + model.parts.size());
        layers.emplace_back(model.profile, model.plan, model.angular, r, hs, model.edge, accent);
        if (!model.under.empty()) layers.emplace_back(model.under, Plan::round(), std::vector<Angular>{}, r, hs, model.edge, accent);
        for (auto& p : model.parts) layers.emplace_back(p.profile, p.plan, p.angular, r, hs, model.edge, accent);
        for (auto& l : layers) zmaxPx = std::max(zmaxPx, l.table.zmax);
        auto mk = [&](const Pointer& p) {
            Ptr q; q.kind = p.kind; q.r0 = p.r0 * r; q.r1 = p.r1 * r;
            q.hw = std::max(p.width * r * 0.5f, p.kind == PointerKind::Dot ? 1.8f : 1.2f);
            q.depth = p.depth * r; q.mat = p.material.tinted(accent); return q;
        };
        if (model.hasPointer) ptrs.push_back(mk(model.pointer));
        for (auto& m : model.marks) ptrs.push_back(mk(m));
        for (auto& p : ptrs) if (p.kind == PointerKind::Ridge) zmaxPx += p.depth;
        if (model.hasInsert) { hasInsert = true; insR = model.insert.r * r; insDepth = model.insert.depth * hs; insMat = model.insert.material.tinted(accent); }
        if (model.hasScrew) { hasScrew = true; screw = {model.screw.angle, model.screw.height * hs, std::max(model.screw.radius * r, 1.0f)}; }
    }

    float radius() const { return r; }
    float zmax() const { return zmaxPx; }
    float bound() const { return r * 1.04f + 1.5f; }
    bool decal(Decal& d) const { if (hasScrew) d = screw; return hasScrew; }

    float ptrMask(const Ptr& p, float x, float y) const {
        if (p.kind == PointerKind::Dot) { const float d = std::sqrt(x * x + (y - p.r1) * (y - p.r1)); return ss(p.hw + 0.5f, p.hw - 0.5f, d); }
        if (y < p.r0 - 1.0f || y > p.r1 + 1.0f) return 0;
        return ss(p.hw + 0.5f, p.hw - 0.5f, std::fabs(x)) * ss(p.r0 - 0.7f, p.r0 + 0.7f, y) * ss(p.r1 + 0.7f, p.r1 - 0.7f, y);
    }
    float bodyHeight(float x, float y, float rr) const {
        float h = 0; for (auto& l : layers) h = std::max(h, l.table.at(l.rho(x, y, rr, r))); return h;
    }
    float height(float x, float y) const {
        const float rr = std::sqrt(x * x + y * y);
        if (rr > bound()) return 0;
        float h = bodyHeight(x, y, rr);
        if (h <= 0) return 0;
        if (hasInsert) h -= insDepth * ss(insR + 0.6f, insR - 0.6f, rr);
        for (auto& p : ptrs) {
            switch (p.kind) {
                case PointerKind::Line: break;
                case PointerKind::Groove: h -= p.depth * ptrMask(p, x, y); break;
                case PointerKind::Dot:
                    if (p.depth > 0) { const float d2 = (x * x + (y - p.r1) * (y - p.r1)) / (p.hw * p.hw); h -= p.depth * std::max(1.0f - d2, 0.0f); }
                    break;
                case PointerKind::Ridge:
                    if (h > 0.3f && y > p.r0 - 1.0f && y < p.r1 + 1.0f) {
                        const float t = x / (p.hw + 0.5f), across = std::sqrt(std::max(1.0f - t * t, 0.0f));
                        h += p.depth * across * ss(p.r0 - 0.7f, p.r0 + 0.7f, y) * ss(p.r1 + 0.7f, p.r1 - 0.7f, y);
                    }
                    break;
            }
        }
        return std::max(h, 0.0f);
    }

    void convert(const Material& m, float x, float y, float rr, Mat& mat, float& bx, float& by) const {
        const V3 base = v3(m.base[0], m.base[1], m.base[2]);
        V3 tint = grey(1.0f); float rough = m.roughness; bx = 0; by = 0;
        const float rx = x / rr, ry = y / rr;
        const float rings = m.rings + (m.anisotropy > 0 ? m.grain : 0.0f);
        if (rings > 0) {
            const float ring = vnoise(rr * 1.3f, 3.0f) * 0.6f + vnoise(rr * 0.33f, 9.0f) * 0.4f;
            tint = tint * (1.0f + 0.45f * rings * (ring - 0.5f));
            const float b = (vnoise(rr * 2.3f, 1.0f) - 0.5f) * 0.25f * rings;
            bx += rx * b; by += ry * b;
        }
        if (m.woodAmt > 0) {
            const float sx = x / r, sy = y / r;
            const float g = sx + 0.035f * (vnoise(sy * 0.9f + 2.0f, sx * 0.7f) - 0.5f) + 0.006f * (vnoise(sy * 4.0f, sx * 3.0f + 4.0f) - 0.5f);
            auto oct = [&](float f, float fy, float seed) { return (vnoise(g * f + seed, sy * fy + seed * 1.7f) - 0.5f) * ss(1.4f, 3.5f, r / f); };
            const float band = oct(4.0f, 0.12f, 1.0f) * 0.7f + oct(11.0f, 0.18f, 5.0f) * 0.7f + oct(30.0f, 0.3f, 9.0f) * 0.6f + oct(75.0f, 0.6f, 13.0f) * 0.45f;
            const float pore = std::max(vnoise(g * 110.0f, sy * 9.0f) - 0.62f, 0.0f) * 2.6f * ss(1.4f, 3.0f, r / 110.0f);
            const float t = std::max(1.0f + m.woodAmt * (1.25f * band - 0.6f * pore), 0.25f);
            tint = tint * v3(t, std::pow(t, 1.25f), std::pow(t, 1.6f));
            rough = std::min(1.0f, std::max(0.05f, rough + 0.2f * m.woodAmt * band));
        }
        if (m.grain > 0 && m.anisotropy <= 0) {
            if (m.clearCoat > 0) {
                const float sx = x / r, sy = y / r;
                bx += (vnoise(sx * 2.3f + 4.0f, sy * 2.3f) - 0.5f) * 0.6f * m.grain + (vnoise(x * 0.45f, y * 0.45f) - 0.5f) * 0.05f * m.grain;
                by += (vnoise(sx * 2.3f, sy * 2.3f + 7.0f) - 0.5f) * 0.6f * m.grain + (vnoise(x * 0.45f + 9.0f, y * 0.45f) - 0.5f) * 0.05f * m.grain;
            } else {
                const float s = vnoise(x * 1.1f, y * 1.1f) * 0.6f + vnoise(x * 0.31f + 5.0f, y * 0.31f) * 0.4f;
                tint = tint * (1.0f + m.grain * (s - 0.5f));
                rough = std::max(rough + 0.3f * m.grain * (s - 0.5f), 0.02f);
                bx += (vnoise(x * 0.9f + 3.0f, y * 0.9f) - 0.5f) * 0.12f * m.grain;
                by += (vnoise(x * 0.9f, y * 0.9f + 3.0f) - 0.5f) * 0.12f * m.grain;
            }
        }
        mat.diff = base * tint * (1.0f - m.metallic);
        mat.f0 = lerp3(grey(m.f0), base * tint, m.metallic);
        mat.rough = rough; mat.aniso = m.anisotropy; mat.coat = m.clearCoat; mat.sheen = m.sheen; mat.trans = m.translucency;
    }

    void material(float x, float y, Mat& mat, float& bx, float& by) const {
        const float rr = std::max(std::sqrt(x * x + y * y), 1e-4f);
        Material src = Material::plastic(0.5f, 0.5f, 0.5f, 0.5f); float best = -1;
        for (auto& l : layers) {
            const float rho = l.rho(x, y, rr, r), h = l.table.at(rho);
            if (h > best) { best = h; src = l.table.mat(rho / r); }
        }
        convert(src, x, y, rr, mat, bx, by);
        if (hasInsert) {
            const float k = ss(insR + 0.5f, insR - 0.5f, rr);
            if (k > 0) { Mat pm; float pbx, pby; convert(insMat, x, y, rr, pm, pbx, pby); mat = blend(mat, pm, k); bx = mixf(bx, pbx, k); by = mixf(by, pby, k); }
        }
        for (auto& p : ptrs) {
            const float k = ptrMask(p, x, y);
            if (k > 0) { Mat pm; float a, b; convert(p.mat, x, y, rr, pm, a, b); mat = blend(mat, pm, k); bx *= 1.0f - k; by *= 1.0f - k; }
        }
    }

    Surf surfN(float x, float y, float rot, int dirs) const {
        const float e = 0.35f;
        Surf s;
        s.gx = (height(x + e, y) - height(x - e, y)) / (2.0f * e);
        s.gy = (height(x, y + e) - height(x, y - e)) / (2.0f * e);
        material(x, y, s.mat, s.bx, s.by);
        const float h = height(x, y);
        s.ao = horizonAo([&](float a, float b) { return height(a, b); }, x, y, h, rot, r / 90.0f, dirs);
        const float d = std::max(0.022f * r, 0.8f);
        const float around = height(x - d, y) + height(x + d, y) + height(x, y - d) + height(x, y + d);
        s.curv = std::min(CURV, std::max(-CURV, (4.0f * h - around) / (4.0f * d)));
        return s;
    }
    float floorAoN(float x, float y, float rot, int dirs) const {
        return horizonAo([&](float a, float b) { return height(a, b); }, x, y, 0.0f, rot, r / 90.0f, dirs);
    }
    Surf surf(float x, float y, float rot) const { return surfN(x, y, rot, AO_DIRS); }
    float floorAo(float x, float y, float rot) const { return floorAoN(x, y, rot, AO_DIRS); }
};

// ------------------------------------------------------------------------------------ shading
inline const V3 KEY = {3.6f, 3.5f, 3.35f};
inline constexpr int NLEVELS = 10;
inline const float LEVELS[NLEVELS] = {0.0f, 0.05f, 0.10f, 0.16f, 0.24f, 0.34f, 0.48f, 0.65f, 0.85f, 1.0f};
inline const int SIZES[NLEVELS] = {224, 192, 160, 128, 96, 64, 48, 32, 24, 24};
using Env5 = std::array<float, 5>;

inline float softbox(V3 d, V3 c, V3 hint, float hu, float hv, float blur) {
    const float cd = dot(d, c);
    if (cd <= 0.02f) return 0;
    const V3 bx = norm(hint - c * dot(hint, c)), by = cross(c, bx);
    const float u = dot(d, bx) / cd, v = dot(d, by) / cd;
    const float a = ss(hu + blur, hu - blur, std::fabs(u)) * ss(hv + blur, hv - blur, std::fabs(v));
    const float q = (u / hu) * (u / hu) + (v / hv) * (v / hv);
    const float fall = 1.0f - 0.35f * std::min(q, 2.0f) * 0.5f;
    const float spread = (hu * hv) / (std::max(hu, blur) * std::max(hv, blur));
    return a * fall * spread;
}

inline Env5 envAt(const Rig& rig, V3 keyDir, V3 view, V3 d, float rough) {
    const float blur = 0.015f + 1.4f * rough * rough, e = rig.environment;
    const float horizon = ss(-0.40f - blur, 0.0f + blur, d.z);
    const V3 wall = lerp3(srgb(0.20f, 0.195f, 0.19f), srgb(0.50f, 0.49f, 0.48f), clamp01(0.5f + 0.5f * d.y));
    V3 other = wall * (e * horizon);
    float floorW = (1.0f - horizon) * (0.60f + 0.40f * clamp01(0.5f - 0.5f * d.y));
    const float zen = ss(0.50f - blur, 0.96f, d.z);
    const float sweep = ss(-0.45f - blur, 0.45f + blur, d.y - 0.35f * d.x);
    other += srgb(0.66f, 0.66f, 0.67f) * (zen * (0.22f + 0.78f * sweep) * e);
    other += v3(1.5f, 1.6f, 1.75f) * (softbox(d, norm(v3(0.86f, 0.1f, 0.48f)), v3(0, 1, 0), 0.8f, 0.07f, blur) * e);
    other += v3(1.25f, 1.2f, 1.1f) * (softbox(d, norm(v3(-0.80f, 0.25f, 0.54f)), v3(0, 1, 0), 0.55f, 0.035f, blur) * e);
    other += v3(1.25f, 1.22f, 1.16f) * (softbox(d, norm(v3(-0.55f, 0.62f, 0.56f)), v3(1, 0, 0), 0.55f, 0.26f, blur) * e);
    other += v3(1.0f, 0.90f, 0.78f) * (softbox(d, norm(v3(-0.35f, -0.85f, 0.3f)), v3(1, 0, 0), 0.5f, 0.10f, blur) * e);
    const float edgeL = softbox(d, norm(v3(-0.90f, 0.30f, 0.30f)), v3(0, 0, 1), 0.50f, 0.055f, blur);
    const float edgeR = softbox(d, norm(v3(0.93f, -0.12f, 0.34f)), v3(0, 0, 1), 0.50f, 0.045f, blur);
    other += v3(1.25f, 1.27f, 1.35f) * (edgeL * e) + v3(1.0f, 1.0f, 1.05f) * (edgeR * e);
    const float band = ss(-0.02f - blur, 0.07f + 0.5f * blur, d.z) * ss(0.40f + blur, 0.16f, d.z);
    const float h = std::max(std::sqrt(d.x * d.x + d.y * d.y), 1e-4f);
    const float side = 0.30f + 0.55f * (d.x / h) * (d.x / h) + 0.35f * clamp01(d.y / h);
    other += v3(0.55f, 0.58f, 0.64f) * (band * side * rig.rim * e);
    const float rd = std::sqrt((d.x - view.x) * (d.x - view.x) + (d.y - view.y) * (d.y - view.y)) / std::max(d.z, 0.05f);
    const float gobo = 1.0f - 0.25f * ss(0.20f + blur + 0.06f, 0.04f, rd);
    other = other * gobo; floorW *= gobo;
    const float k = softbox(d, keyDir, v3(1, 0, 0), rig.keySize * 1.25f, rig.keySize, blur);
    return {other.x, other.y, other.z, floorW, k * rig.keyIntensity};
}

inline V3 octInv(float u, float v) {
    const float px = 2.0f * u - 1.0f, py = 2.0f * v - 1.0f, z = 1.0f - std::fabs(px) - std::fabs(py);
    if (z >= 0) return norm(v3(px, py, z));
    return norm(v3((1.0f - std::fabs(py)) * std::copysign(1.0f, px), (1.0f - std::fabs(px)) * std::copysign(1.0f, py), z));
}

struct EnvLevel {
    int n = 0; std::vector<Env5> px;
    Env5 at(float u, float v) const {
        const float m = float(n - 1);
        const float fx = std::min(std::max(u * m, 0.0f), m - 0.001f), fy = std::min(std::max(v * m, 0.0f), m - 0.001f);
        const int ix = int(fx), iy = int(fy); const float tx = fx - float(ix), ty = fy - float(iy);
        const size_t i = size_t(iy) * size_t(n) + size_t(ix);
        const Env5 &a = px[i], &b = px[i + 1], &c = px[i + size_t(n)], &d = px[i + size_t(n) + 1];
        Env5 o;
        for (int k = 0; k < 5; ++k) { const float top = a[size_t(k)] + (b[size_t(k)] - a[size_t(k)]) * tx, bot = c[size_t(k)] + (d[size_t(k)] - c[size_t(k)]) * tx; o[size_t(k)] = top + (bot - top) * ty; }
        return o;
    }
};

inline V3 viewOf(const Rig& rig) { return v3(0.0f, -std::sin(rig.tilt), std::cos(rig.tilt)); }
inline V3 keyOf(const Rig& rig) { return v3(-std::cos(rig.keyElevation) * std::sin(rig.keyAzimuth), std::cos(rig.keyElevation) * std::cos(rig.keyAzimuth), std::sin(rig.keyElevation)); }

struct EnvMaps {
    EnvLevel levels[NLEVELS];
    explicit EnvMaps(const Rig& rig) {
        const V3 key = keyOf(rig), view = viewOf(rig);
        std::vector<std::thread> pool;
        for (int li = 0; li < NLEVELS; ++li) {
            levels[li].n = SIZES[li]; levels[li].px.assign(size_t(SIZES[li]) * size_t(SIZES[li]), Env5{});
            pool.emplace_back([&, li] {
                auto& lv = levels[li]; const int n = lv.n;
                for (size_t i = 0; i < lv.px.size(); ++i)
                    lv.px[i] = envAt(rig, key, view, octInv(float(int(i) % n) / float(n - 1), float(int(i) / n) / float(n - 1)), LEVELS[li]);
            });
        }
        for (auto& t : pool) t.join();
    }
    Env5 sample(V3 d, float rough) const {
        const float s = 1.0f / std::max(std::fabs(d.x) + std::fabs(d.y) + std::fabs(d.z), 1e-9f);
        float px = d.x * s, py = d.y * s;
        if (d.z < 0) { const float ax = std::fabs(px), ay = std::fabs(py); px = (1.0f - ay) * (d.x < 0 ? -1.0f : 1.0f); py = (1.0f - ax) * (d.y < 0 ? -1.0f : 1.0f); }
        const float u = px * 0.5f + 0.5f, v = py * 0.5f + 0.5f;
        rough = std::min(1.0f, std::max(0.0f, rough));
        int i = 0; while (i + 2 < NLEVELS && LEVELS[i + 1] <= rough) ++i;
        const float t = (rough - LEVELS[i]) / (LEVELS[i + 1] - LEVELS[i]);
        const Env5 a = levels[i].at(u, v);
        if (t <= 0.001f) return a;
        const Env5 b = levels[i + 1].at(u, v);
        Env5 o; for (size_t c = 0; c < 5; ++c) o[c] = a[c] + (b[c] - a[c]) * t;
        return o;
    }
};

struct Lights {
    Rig rig; V3 key, view, floor, floorRefl; float rimGain = 0, kickGain = 0; std::shared_ptr<const EnvMaps> env;
    explicit Lights(const Rig& r) : rig(r) {
        floor = v3(toLinear(float((r.floor >> 16) & 255) / 255.0f), toLinear(float((r.floor >> 8) & 255) / 255.0f), toLinear(float(r.floor & 255) / 255.0f));
        const float lum = 0.2126f * floor.x + 0.7152f * floor.y + 0.0722f * floor.z;
        floorRefl = floor * 0.60f + grey(0.035f) + grey(0.035f * r.rim) * std::pow(1.0f - lum, 3.0f);
        rimGain = 0.17f * r.rim * (0.22f + 0.78f * std::pow(std::max(1.0f - std::sqrt(lum), 0.0f), 2.0f));
        kickGain = r.rim * (0.38f + 0.62f * std::pow(std::max(1.0f - std::sqrt(lum), 0.0f), 2.0f));
        static std::mutex mu; static std::vector<std::pair<std::uint64_t, std::shared_ptr<const EnvMaps>>> envs;
        const std::uint64_t fp = fingerprint(r, false);
        {
            std::lock_guard<std::mutex> lk(mu);
            for (auto& e : envs) if (e.first == fp) env = e.second;
            if (!env) {
                env = std::make_shared<const EnvMaps>(r);
                if (envs.size() >= 6) envs.erase(envs.begin());
                envs.push_back({fp, env});
            }
        }
        key = keyOf(r); view = viewOf(r);
    }
    struct Look { float k; V3 room, panel; };
    Look look(V3 d, float rough) const { const Env5 s = env->sample(d, rough); return {s[4], v3(s[0], s[1], s[2]), floorRefl * s[3]}; }
};

inline V3 schlickR(V3 f0, float c, float rough) {
    const float m = std::pow(1.0f - std::min(1.0f, std::max(0.0f, c)), 5.0f), g = std::pow(std::max(1.0f - rough, 0.0f), 2.0f);
    const V3 top = v3(std::max(g, f0.x), std::max(g, f0.y), std::max(g, f0.z));
    return f0 + (top - f0) * m;
}

inline const V3 KICK_L = {-0.800f, -0.555f, 0.228f};
inline const V3 KICK_R = {0.853f, -0.452f, 0.261f};

// Shade one surface point. v points to the camera, t runs across the lathe grooves, fv says how much of
// the lit panel a mirror here can see, curv is the convexity of the surface.
inline V3 shade(const Lights& l, V3 n, V3 v, V3 t, const Mat& m, float ao, float sh, float fv, float curv) {
    const float nv = std::max(dot(n, v), 1e-3f);
    const float metal = ss(0.2f, 0.6f, (m.f0.x + m.f0.y + m.f0.z) / 3.0f);
    const float keyGain = mixf((1.0f + 2.0f * ss(0.15f, 0.6f, m.rough)) * (1.0f - 0.6f * m.coat), 1.0f - 0.45f * ss(0.2f, 0.5f, m.rough), metal);
    const float specGain = 1.0f - 0.24f * metal * ss(0.2f, 0.45f, m.rough);
    const float specAo = std::pow(ao, 1.3f) * mixf(1.0f, sh, 0.15f);
    const int taps = m.aniso > 0.01f ? 9 : 1;
    const float rough = taps > 1 ? std::max(m.rough, std::sqrt(2.2f * m.aniso / float(taps) / 1.4f)) : m.rough;
    V3 spec = grey(0);
    for (int j = 0; j < taps; ++j) {
        const float s = taps > 1 ? ((float(j) + 0.5f) / float(taps) * 2.0f - 1.0f) * m.aniso : 0.0f;
        const V3 nj = taps > 1 ? norm(n + t * s) : n;
        const V3 r = reflect(v * -1.0f, nj);
        const V3 f = schlickR(m.f0, std::max(dot(nj, v), 0.0f), m.rough);
        const auto lk = l.look(r, rough);
        spec += f * (KEY * (lk.k * sh * keyGain) + (lk.room + lk.panel * fv) * specAo);
    }
    spec = spec * (specGain / float(taps));
    const Rig& rig = l.rig;
    const float ndl = clamp01((dot(n, l.key) + 0.25f) / 1.25f);
    const V3 amb = (grey(0.20f) * (0.5f + 0.5f * n.y) + grey(0.13f) * std::max(n.z, 0.0f) + grey(0.30f) * std::max(0.25f * n.z - 0.9f * n.y, 0.0f) + grey(0.05f) + (l.floor * 0.55f + grey(0.02f)) * (0.5f - 0.5f * n.z)) * rig.ambient;
    const float fd = 1.0f - schlickR(grey(0.04f), nv, 0.0f).x;
    const V3 light = grey(0.78f * rig.keyIntensity * ndl * sh) + amb * (ao * mixf(1.0f, sh, 0.2f));
    V3 diffuse = m.diff * light * fd;
    if (m.trans > 0) {
        const float wrap = clamp01((dot(n, l.key) + 0.95f) / 1.95f);
        const V3 deep = v3(std::pow(m.diff.x, 0.6f), std::pow(m.diff.y, 0.6f), std::pow(m.diff.z, 0.6f)) * m.diff * 2.2f;
        const V3 through = deep * ((0.50f * wrap * wrap + 0.22f * std::pow(1.0f - nv, 2.0f)) * mixf(0.6f, 1.0f, sh) * ao);
        diffuse = diffuse * (1.0f - 0.45f * m.trans) + through * m.trans;
    }
    if (m.sheen > 0) {
        const float g = std::pow(1.0f - nv, 3.0f);
        diffuse += (m.diff * 0.5f + grey(0.5f)) * light * (0.22f * m.sheen * g);
        diffuse += (m.diff + grey(0.03f)) * (0.9f * m.sheen * ndl * sh * (1.0f - nv * nv));
    }
    V3 col = diffuse + spec;
    if (m.coat > 0) {
        const float fc = m.coat * schlickR(grey(0.04f), nv, 0.0f).x;
        const V3 r = reflect(v * -1.0f, n);
        const auto lk = l.look(r, 0.03f + 0.15f * (1.0f - m.coat));
        col = col * (1.0f - fc) + (KEY * (lk.k * 5.0f * sh) + (lk.room * (grey(0.22f) + lk.room * 2.1f) + lk.panel * (0.5f * fv)) * specAo) * fc;
    }
    if (l.rimGain > 0) {
        const float w = std::pow(1.0f - nv, 3.0f) * clamp01(0.3f + 0.9f * n.y + 0.5f * std::fabs(n.x));
        const float gl = std::max(m.coat, 1.0f - m.rough);
        col += v3(0.86f, 0.90f, 1.0f) * (l.rimGain * w * (0.55f + 0.45f * gl) * ao);
    }
    const float gloss = std::min(1.0f, std::max(0.0f, std::max(m.coat, 1.0f - m.rough)));
    if (l.kickGain > 0) {
        const float p = mixf(5.0f, 30.0f, gloss * gloss);
        const float dl = std::max(dot(n, KICK_L), 0.0f), dr = std::max(dot(n, KICK_R), 0.0f);
        const V3 lobe = v3(1.0f, 0.98f, 0.94f) * std::pow(dl, p) + v3(0.86f, 0.92f, 1.0f) * (std::pow(dr, p) * 0.8f);
        const V3 fk = lerp3(grey(0.05f + 0.10f * gloss + 0.05f * m.sheen), m.f0 * 0.16f, metal);
        const V3 wide = (m.diff + grey(0.004f)) * ((dl * dl + dr * dr * 0.8f) * 0.7f);
        col += (lobe * fk + wide) * (l.kickGain * (0.35f + 0.65f * ao) * mixf(0.55f, 1.0f, sh));
    }
    if (curv > 0) {
        const float k = ss(0.05f, 0.55f, curv);
        const float facing = 0.42f + 0.58f * clamp01(0.5f + 0.5f * dot(n, l.key)) * mixf(0.5f, 1.0f, sh);
        const V3 fe = lerp3(grey(0.13f + 0.20f * gloss), m.f0 * 0.55f, metal) + m.diff * 0.35f;
        col += v3(1.0f, 0.99f, 0.97f) * fe * (k * facing * (0.4f + 0.6f * ao) * (0.45f + 0.55f * std::min(l.kickGain, 1.0f)));
    } else {
        col = col * (1.0f - 0.4f * ss(0.05f, 0.6f, -curv));
    }
    return col;
}

// ------------------------------------------------------------------------------------ render
struct Quality { int grid; bool jitter; };
inline constexpr Quality DIRECT{4, true};
inline constexpr Quality FRAME{2, false};

inline std::pair<float, float> wideTans(const Rig& rig, float rise) {
    const float a = std::max(std::min(rig.shadowSoftness, rise - 0.6f), 0.05f);
    return {a, std::max(std::min(2.0f * a, rise - 0.6f), a)};
}

template <class S>
struct Ctx {
    const S& f; const Lights& l; Quality q; float tanTilt, sc, sa, ca, tdx, tdy, ldx, ldy, rise, tanA, tanB, bound, zmax, r;
    Ctx(const S& f_, const Lights& l_, float angle, Quality q_) : f(f_), l(l_), q(q_) {
        sa = std::sin(angle); ca = std::cos(angle);
        tanTilt = std::tan(l.rig.tilt);
        const V3 k = l.key; const float lh = std::max(std::sqrt(k.x * k.x + k.y * k.y), 1e-4f);
        const float dx = k.x / lh, dy = k.y / lh;
        rise = k.z / lh; sc = f.radius() / 90.0f;
        tdx = tanTilt * sa; tdy = -tanTilt * ca;
        ldx = dx * ca - dy * sa; ldy = dx * sa + dy * ca;
        auto tn = wideTans(l.rig, rise); tanA = tn.first; tanB = tn.second;
        bound = f.bound(); zmax = f.zmax(); r = f.radius();
    }
    void cap(float px, float py, float& x, float& y) const { x = px * ca - py * sa; y = px * sa + py * ca; }
    void panel(float x, float y, float& ox, float& oy) const { ox = x * ca + y * sa; oy = -x * sa + y * ca; }

    bool trace(float px, float py, float& ox, float& oy, float& oz) const {
        const float reach = zmax * tanTilt, b = bound;
        const float cl = std::min(std::max(py, 0.0f), reach);
        if (std::fabs(px) > b || py < -b || py > b + reach || px * px + (py - cl) * (py - cl) > b * b) return false;
        float cx, cy; cap(px, py, cx, cy);
        auto over = [&](float z) { return f.height(cx + tdx * z, cy + tdy * z) - z; };
        const float top = zmax + 0.5f;
        const int steps = std::max(int(std::ceil(reach / 0.7f)), 1);
        float zHi = top;
        for (int i = 1; i <= steps; ++i) {
            const float z = top * (1.0f - float(i) / float(steps));
            if (over(z) > 0.02f) {
                float lo = z, hi = zHi;
                for (int k = 0; k < 8; ++k) { const float mid = 0.5f * (lo + hi); if (over(mid) > 0.02f) lo = mid; else hi = mid; }
                const float zz = 0.5f * (lo + hi);
                ox = cx + tdx * zz; oy = cy + tdy * zz; oz = zz; return true;
            }
            zHi = z;
        }
        return false;
    }

    float shadow(float x, float y, float h0, float minW) const {
        float ra = 1, rb = 1;
        const float step = 0.8f * std::max(sc, 0.5f);
        float t = 0.6f * std::max(sc, 0.5f);
        const float tMax = (zmax - h0) / (rise - tanB) + 1.0f, b2 = bound * bound;
        while (t < tMax) {
            const float qx = x + ldx * t, qy = y + ldy * t;
            if (qx * qx + qy * qy > b2 && qx * ldx + qy * ldy > 0) break;
            const float hq = f.height(qx, qy);
            if (hq > h0 + 0.01f) {
                const float clear = h0 + t * rise + 0.25f - hq;
                ra = std::min(ra, clear / (t * tanA + minW));
                rb = std::min(rb, clear / (t * tanB + 2.0f * minW));
                if (rb < -1.0f) break;
            }
            t += std::max(t * 0.16f, step);
        }
        return 0.55f * ss(-0.6f, 1.0f, std::max(ra, -1.0f)) + 0.45f * ss(-0.6f, 1.0f, std::max(rb, -1.0f));
    }

    bool sample(float px, float py, float rot, V3& out) const {
        float x, y, z; if (!trace(px, py, x, y, z)) return false;
        const Surf s = f.surf(x, y, rot);
        float nx, ny, bx, by, hx, hy; panel(-s.gx, -s.gy, nx, ny); panel(s.bx, s.by, bx, by); panel(x, y, hx, hy);
        V3 n = norm(norm(v3(nx, ny, 1.0f)) + v3(bx, by, 0.0f));
        const V3 v = norm(l.view + v3(-hx, -hy, 0.0f) * (0.16f / r));
        if (dot(n, v) < 0.02f) n = norm(n + v * (0.02f - dot(n, v)));
        const float wallAo = 0.72f + 0.28f * ss(0.0f, 0.45f * r, z);
        float ao = mixf(wallAo, s.ao, ss(0.3f, 0.8f, n.z));
        Mat m = s.mat;
        Decal d;
        if (f.decal(d) && n.z < 0.7f) {
            const float rr0 = std::sqrt(x * x + y * y);
            const float da = (remEuclid(std::atan2(x, y) - d.ang + kPi, 2.0f * kPi) - kPi) * rr0;
            const float q2 = std::sqrt(da * da + (z - d.z) * (z - d.z)) / d.rad;
            if (q2 < 1.4f) {
                const float k = ss(1.0f, 0.7f, q2);
                const float lip = ss(1.4f, 1.05f, q2) * ss(0.7f, 1.05f, q2) * (z < d.z ? 1.0f : 0.25f);
                m.diff = m.diff * (1.0f - 0.94f * k) + grey(0.06f * lip);
                m.f0 = m.f0 * (1.0f - 0.92f * k);
                m.coat *= 1.0f - k; ao *= 1.0f - 0.85f * k;
            }
        }
        const float rr = std::max(std::sqrt(hx * hx + hy * hy), 1e-4f);
        const float sh = shadow(x, y, z, 0.0f);
        const float fv = 0.2f + 0.8f * ss(0.0f, 0.9f * r, z);
        const float nearK = 1.0f + 0.07f * std::min(1.2f, std::max(-1.2f, (hy - 0.6f * hx) / r));
        out = shade(l, n, v, v3(hx / rr, hy / rr, 0.0f), m, ao, sh, fv, s.curv) * nearK;
        return true;
    }

    float floorDark(float px, float py, float rot) const {
        const Rig& rig = l.rig;
        const float reach = r * 1.75f + 2.0f;
        if (px * px + py * py > (reach + zmax) * (reach + zmax)) return 0;
        float x, y; cap(px, py, x, y);
        const float sh = shadow(x, y, 0.0f, 0.045f * r), ao = f.floorAo(x, y, rot);
        const float keep = (1.0f - rig.shadow * (1.0f - sh)) * (1.0f - rig.occlusion * std::pow(1.0f - ao, 1.3f));
        return 1.0f - keep;
    }
};

struct Layout { int w, h; float ox, oy, fade; };
inline Layout layoutOf(float r, float bound, float zmax, const Lights& l) {
    const float b = bound; const V3 k = l.key;
    const float kh = std::max(std::sqrt(k.x * k.x + k.y * k.y), 1e-4f);
    const float cast = 0.8f * zmax / (k.z / kh - wideTans(l.rig, k.z / kh).second);
    const float soft = 0.45f * r + 3.0f;
    const float left = std::ceil(b + soft + std::max(cast * k.x / kh, 0.0f));
    const float right = std::ceil(b + soft + std::max(-cast * k.x / kh, 0.0f));
    const float up = std::ceil(b + std::max(zmax * std::tan(l.rig.tilt), soft + std::max(-cast * k.y / kh, 0.0f))) + 1.0f;
    const float down = std::ceil(b + soft + std::max(cast * k.y / kh, 0.0f));
    return {int(left + right), int(up + down), left, up, std::max(0.2f * r, 3.0f)};
}

template <class S>
std::uint32_t pixelOf(const Ctx<S>& ctx, int ix, int iy, float ox, float oy, int w, int h, float fade) {
    static const float RG[4][2] = {{0.375f, 0.125f}, {0.875f, 0.375f}, {0.125f, 0.625f}, {0.625f, 0.875f}};
    Rng rng{(std::uint32_t(ix) * 9781u + std::uint32_t(iy) * 6271u) | 1u};
    V3 col = grey(0); int hits = 0; const int g = ctx.q.grid; int n = g * g;
    const float bx = float(ix) - ox, by = oy - float(iy);
    for (int j = 0; j < g; ++j) for (int i = 0; i < g; ++i) {
        float fx, fy;
        if (ctx.q.jitter) { fx = (float(i) + rng.next()) / float(g); fy = (float(j) + rng.next()) / float(g); }
        else if (g == 2) { fx = RG[j * 2 + i][0]; fy = RG[j * 2 + i][1]; }
        else { fx = (float(i) + 0.5f) / float(g); fy = (float(j) + 0.5f) / float(g); }
        V3 c; if (ctx.sample(bx + fx, by - fy, AO_ROT, c)) { col += c; ++hits; }
    }
    const int shaded = hits;
    if (!ctx.q.jitter && hits > 0 && hits < n) {
        constexpr int E = 4; int fine = 0;
        for (int j = 0; j < E; ++j) for (int i = 0; i < E; ++i) {
            float a, b, c; fine += ctx.trace(bx + (float(i) + 0.5f) / float(E), by - (float(j) + 0.5f) / float(E), a, b, c) ? 1 : 0;
        }
        if (fine > 0) { hits = fine; n = E * E; }
    }
    float dark = 0;
    if (hits < n) dark = ctx.floorDark(bx + 0.5f, by - 0.5f, AO_ROT) * float(n - hits) / float(n);
    if (hits == 0 && dark < 1e-4f) return 0;
    const float edge = std::min(std::min(float(ix) + 0.5f, float(w) - float(ix) - 0.5f), std::min(float(iy) + 0.5f, float(h) - float(iy) - 0.5f));
    dark *= ss(0.5f, fade, edge);
    const float cov = float(hits) / float(n);
    const float alpha = std::min(cov + dark, 1.0f);
    const V3 c = shaded > 0 ? col * (1.0f / float(shaded)) : col;
    const float a8 = std::min(std::floor(alpha * 255.0f + 0.5f), 255.0f);
    std::uint32_t out = std::uint32_t(a8) << 24;
    const int shifts[3] = {16, 8, 0}; const float lin[3] = {c.x, c.y, c.z};
    for (int k = 0; k < 3; ++k) {
        const int sft = shifts[k];
        const float d = hash2(ix * 3 + sft, iy) + hash2(ix, iy * 3 + sft + 77) - 1.0f;
        const float v = std::min(std::max(std::floor(toSrgb(lin[k]) * cov * 255.0f + 0.5f + d * 0.5f * cov), 0.0f), a8);
        out |= std::uint32_t(v) << sft;
    }
    return out;
}

template <class S>
Sprite renderSurface(const S& surface, const Lights& lights, float angle, Quality q) {
    const Ctx<S> ctx(surface, lights, angle, q);
    const Layout lay = layoutOf(ctx.r, ctx.bound, ctx.zmax, lights);
    Sprite sp; sp.w = lay.w; sp.h = lay.h; sp.ox = lay.ox; sp.oy = lay.oy;
    sp.px.assign(size_t(lay.w) * size_t(lay.h), 0u);
    parRows(sp.px, lay.w, lay.h, [&](int x, int y) { return pixelOf(ctx, x, y, lay.ox, lay.oy, lay.w, lay.h, lay.fade); });
    return sp;
}

// ------------------------------------------------------------------------------------ bake
// Angle-independent cap-space buffer (height, gradient, material, bump, occlusion) resolved once per
// (model, radius, accent); a frame turns the view ray and the light into cap space and re-lights it.
inline constexpr int CH = 15;
inline constexpr float BUMP = 0.8f;
inline std::uint8_t q8(float v) { return std::uint8_t(std::min(1.0f, std::max(0.0f, v)) * 255.0f + 0.5f); }

struct Grid {
    struct Cell { float gx, gy; std::uint8_t b[CH]; };
    float r = 0, zmax_ = 0; int n = 0; float k = 1, half = 0;
    std::vector<float> h; std::vector<Cell> cells;
    int fnn = 0; float fk = 1, fhalf = 0; std::vector<float> fao;
    bool hasDecal = false; Decal decal_;

    explicit Grid(const Field& field) {
        r = field.r; zmax_ = field.zmax();
        k = std::min(4.0f, std::max(1.5f, 120.0f / r));
        half = field.bound() + 1.0f;
        n = int(std::ceil(2.0f * half * k)) + 2;
        h.assign(size_t(n) * size_t(n), 0.0f);
        parRows(h, n, n, [&](int i, int j) { return field.height(float(i) / k - half, float(j) / k - half); });
        Cell blank{0, 0, {0, 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 127, 127, 255, 127}};
        cells.assign(size_t(n) * size_t(n), blank);
        const float lim = (half + 1.0f) * (half + 1.0f);
        parRows(cells, n, n, [&](int i, int j) {
            const float x = float(i) / k - half, y = float(j) / k - half;
            if (x * x + y * y > lim) return blank;
            const Surf s = field.surfN(x, y, AO_ROT, AO_DIRS); const Mat& m = s.mat;
            auto sq = [](float v) { return q8(std::sqrt(std::max(v, 0.0f))); };
            auto bump = [](float v) { return std::uint8_t(std::min(0.5f, std::max(-0.5f, v / (2.0f * BUMP))) * 254.0f + 127.5f); };
            return Cell{s.gx, s.gy, {sq(m.diff.x), sq(m.diff.y), sq(m.diff.z), sq(m.f0.x), sq(m.f0.y), sq(m.f0.z), q8(m.rough), q8(m.aniso), q8(m.coat), q8(m.sheen), q8(m.trans),
                                    bump(s.bx), bump(s.by), q8(s.ao), std::uint8_t(std::min(0.5f, std::max(-0.5f, s.curv / (2.0f * CURV))) * 254.0f + 127.5f)}};
        });
        fk = std::min(2.0f, std::max(0.5f, 36.0f / r)); fhalf = r * 1.75f + 3.0f;
        fnn = int(std::ceil(2.0f * fhalf * fk)) + 2;
        fao.assign(size_t(fnn) * size_t(fnn), 1.0f);
        parRows(fao, fnn, fnn, [&](int i, int j) { return field.floorAoN(float(i) / fk - fhalf, float(j) / fk - fhalf, AO_ROT, AO_DIRS); });
        hasDecal = field.decal(decal_);
    }
    size_t bytes() const { return h.size() * 4 + cells.size() * sizeof(Cell) + fao.size() * 4; }
    float radius() const { return r; }
    float bound() const { return r * 1.04f + 1.5f; }
    bool decal(Decal& d) const { if (hasDecal) d = decal_; return hasDecal; }
    float zmax() const { return zmax_; }

    bool locate(float x, float y, size_t& i, float& tx, float& ty) const {
        const float u = (x + half) * k, v = (y + half) * k, m = float(n - 1);
        if (!(u >= 0 && v >= 0 && u < m && v < m)) return false;
        const int iu = int(u), iv = int(v);
        i = size_t(iv) * size_t(n) + size_t(iu); tx = u - float(iu); ty = v - float(iv); return true;
    }
    float height(float x, float y) const {
        size_t i; float tx, ty; if (!locate(x, y, i, tx, ty)) return 0;
        const float top = h[i] + (h[i + 1] - h[i]) * tx, bot = h[i + size_t(n)] + (h[i + size_t(n) + 1] - h[i + size_t(n)]) * tx;
        return top + (bot - top) * ty;
    }
    Surf surf(float x, float y, float) const {
        float o[CH] = {}; float gx = 0, gy = 0; size_t i; float tx, ty;
        if (locate(x, y, i, tx, ty)) {
            const size_t idx[4] = {i, i + 1, i + size_t(n), i + size_t(n) + 1};
            float w[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty}; float sum = 0;
            for (int q = 0; q < 4; ++q) { if (h[idx[q]] <= 0) w[q] *= 0.02f; sum += w[q]; }
            const float inv = 1.0f / (std::max(sum, 1e-9f) * 255.0f);
            for (int q = 0; q < 4; ++q) {
                const Cell& c = cells[idx[q]]; const float wq = w[q] * inv;
                gx += c.gx * wq * 255.0f; gy += c.gy * wq * 255.0f;
                for (int a = 0; a < CH; ++a) o[a] += float(c.b[a]) * wq;
            }
        } else { o[13] = 1.0f; o[14] = 127.0f / 255.0f; }
        auto bump = [](float v) { return (v * 255.0f - 127.0f) / 254.0f * 2.0f * BUMP; };
        Surf s; s.gx = gx; s.gy = gy; s.bx = bump(o[11]); s.by = bump(o[12]); s.ao = o[13];
        s.curv = (o[14] * 255.0f - 127.0f) / 254.0f * 2.0f * CURV;
        s.mat = {v3(o[0] * o[0], o[1] * o[1], o[2] * o[2]), v3(o[3] * o[3], o[4] * o[4], o[5] * o[5]), o[6], o[7], o[8], o[9], o[10]};
        return s;
    }
    float floorAo(float x, float y, float) const {
        const float u = (x + fhalf) * fk, v = (y + fhalf) * fk, m = float(fnn - 1);
        if (!(u >= 0 && v >= 0 && u < m && v < m)) return 1.0f;
        const int iu = int(u), iv = int(v); const float tx = u - float(iu), ty = v - float(iv);
        const size_t q = size_t(iv) * size_t(fnn) + size_t(iu), nn = size_t(fnn);
        const float top = fao[q] + (fao[q + 1] - fao[q]) * tx, bot = fao[q + nn] + (fao[q + nn + 1] - fao[q + nn]) * tx;
        return top + (bot - top) * ty;
    }
};
} // namespace detail

// Everything about a knob that does not depend on its angle. frame() turns it and re-lights it.
class KnobBake {
public:
    KnobBake(const KnobModel& model, const Rig& rig, float radiusPx, std::uint32_t accent) : field_(model, radiusPx, accent), lights_(rig) {}
    // The bake holds no lighting; the cache re-lights it for any rig.
    Sprite frameLit(const detail::Lights& lights, float angle) { return detail::renderSurface(grid(), lights, angle, detail::FRAME); }
    Sprite frame(float angle) { return frameLit(lights_, angle); }
    size_t bytes() const { return (grid_ ? grid_->bytes() : 0) + 40000; }
    void warm() { grid(); }
private:
    const detail::Grid& grid() { if (!grid_) grid_ = std::make_unique<detail::Grid>(field_); return *grid_; }
    detail::Field field_; detail::Lights lights_; std::unique_ptr<detail::Grid> grid_;
};

// Render one knob to a premultiplied sprite with its shadow, straight from the model (reference quality).
inline Sprite render(const KnobModel& model, const Rig& rig, float radiusPx, float angle, std::uint32_t accent) {
    const detail::Field field(model, radiusPx, accent);
    return detail::renderSurface(field, detail::Lights(rig), angle, detail::DIRECT);
}

// Process-wide sprite cache with a byte budget; least recently used out first.
class KnobCache {
public:
    static constexpr unsigned ANGLE_STEPS = 1024;
    explicit KnobCache(size_t bytes = size_t(64) << 20) : cap_(bytes) {}
    static KnobCache& global() { static KnobCache g; return g; }

    // Radius snapped to half pixels, angle snapped to ANGLE_STEPS over the sweep of 1.6 pi.
    static std::pair<float, float> quantise(float radiusPx, float angle) {
        const float step = 1.6f * kPi / float(ANGLE_STEPS);
        return {std::max(std::round(radiusPx * 2.0f), 4.0f) / 2.0f, std::round(angle / step) * step};
    }
    // Sprite for a model. `floorColour` is the panel colour under the knob.
    std::shared_ptr<const Sprite> get(const KnobModel& model, const Rig& rigIn, float radiusPx, float angle, std::uint32_t accent, std::uint32_t floorColour) {
        Rig rig = rigIn; rig.floor = floorColour;
        const auto [radius, ang] = quantise(radiusPx, angle);
        const std::uint64_t mf = fingerprint(model), rf = fingerprint(rig);
        const unsigned r2 = unsigned(radius * 2.0f);
        const int aq = int(std::lround(ang / (1.6f * kPi / float(ANGLE_STEPS))));
        const SpriteKey key{mf, rf, r2, aq, accent};
        const BakeKey bkey{mf, r2, accent};
        std::shared_ptr<KnobBake> bake;
        {
            std::lock_guard<std::mutex> g(mu_);
            const auto tick = ++tick_;
            if (auto it = sprites_.find(key); it != sprites_.end()) { it->second.second = tick; return it->second.first; }
            if (bakes_.size() >= 24 && !bakes_.count(bkey)) {
                auto old = std::min_element(bakes_.begin(), bakes_.end(), [](auto& a, auto& b) { return a.second.second < b.second.second; });
                bakes_.erase(old);
            }
            auto it = bakes_.find(bkey);
            if (it == bakes_.end()) it = bakes_.emplace(bkey, std::make_pair(std::make_shared<KnobBake>(model, rig, radius, accent), tick)).first;
            it->second.second = tick; bake = it->second.first;
        }
        bake->warm();
        auto sprite = std::make_shared<const Sprite>(bake->frameLit(detail::Lights(rig), ang));
        std::lock_guard<std::mutex> g(mu_);
        if (auto it = sprites_.find(key); it != sprites_.end()) return it->second.first;
        bytes_ += spriteBytes(*sprite);
        sprites_[key] = {sprite, tick_};
        size_t bakeBytes = 0; for (auto& b : bakes_) bakeBytes += b.second.first->bytes();
        while (bakeBytes > cap_ / 2 && bakes_.size() > 1) {
            auto old = bakes_.end(); for (auto it = bakes_.begin(); it != bakes_.end(); ++it) if (it->first != bkey && (old == bakes_.end() || it->second.second < old->second.second)) old = it;
            if (old == bakes_.end()) break;
            bakeBytes -= old->second.first->bytes(); bakes_.erase(old);
        }
        while (bytes_ + bakeBytes > cap_ && sprites_.size() > 1) {
            auto old = sprites_.end(); for (auto it = sprites_.begin(); it != sprites_.end(); ++it) if (it->first != key && (old == sprites_.end() || it->second.second < old->second.second)) old = it;
            if (old == sprites_.end()) break;
            bytes_ -= spriteBytes(*old->second.first); sprites_.erase(old);
        }
        return sprite;
    }
    std::pair<size_t, size_t> stats() const { std::lock_guard<std::mutex> g(mu_); return {sprites_.size(), bytes_}; }
    void clear() { std::lock_guard<std::mutex> g(mu_); sprites_.clear(); bakes_.clear(); bytes_ = 0; tick_ = 0; }
private:
    using SpriteKey = std::tuple<std::uint64_t, std::uint64_t, unsigned, int, std::uint32_t>;
    using BakeKey = std::tuple<std::uint64_t, unsigned, std::uint32_t>;
    static size_t spriteBytes(const Sprite& s) { return s.px.size() * 4 + 64; }
    mutable std::mutex mu_; size_t cap_; size_t bytes_ = 0; std::uint64_t tick_ = 0;
    std::map<SpriteKey, std::pair<std::shared_ptr<const Sprite>, std::uint64_t>> sprites_;
    std::map<BakeKey, std::pair<std::shared_ptr<KnobBake>, std::uint64_t>> bakes_;
};

} // namespace goodlookinui::knob3d
