// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>
#include "Retro.h"

// Rocker switches, toggles and push buttons, drawn in code and lit by one studio light (key 20 deg
// left of up, 65 deg above the panel), matching GoodLookinDri's switches. Every function draws one
// state (`on` / `down`); the host component decides what a click means.
//
// Drop shadows, inner shadows, multi-stop gradients, conic sweeps and feathered glows are built on
// JUCE Graphics (DropShadow, ColourGradient). Additive and screen light is painted source-over,
// which looks the same on the dark panels these parts sit on.
namespace goodlookinui::juce_adapter::switches {
using juce::Colour; using juce::Graphics; using juce::Point; using juce::Rectangle;

// ---- shared lit-drawing helpers --------------------------------------------------
namespace sw3d {
using Pt = Point<float>;
using Rc = Rectangle<float>;
constexpr float kPi = juce::MathConstants<float>::pi, kHalfPi = juce::MathConstants<float>::halfPi;

inline Colour Al(Colour c, float a) { return c.withAlpha(juce::jlimit(0.0f, 1.0f, a)); }
inline Colour Br(Colour c, float amt) {
    amt = std::max(0.0f, amt);
    auto ch = [&](juce::uint8 v) { return (juce::uint8) juce::jlimit(0.0f, 255.0f, std::round(float(v) + amt * (255.0f - float(v)))); };
    return Colour(ch(c.getRed()), ch(c.getGreen()), ch(c.getBlue()), c.getAlpha());
}
inline Colour Dk(Colour c, float amt) {
    amt = juce::jlimit(0.0f, 1.0f, amt);
    auto ch = [&](juce::uint8 v) { return (juce::uint8) juce::jlimit(0.0f, 255.0f, std::round(float(v) * (1.0f - amt))); };
    return Colour(ch(c.getRed()), ch(c.getGreen()), ch(c.getBlue()), c.getAlpha());
}
inline Colour Mix(Colour a, Colour b, float t) {
    t = juce::jlimit(0.0f, 1.0f, t);
    auto ch = [&](juce::uint8 x, juce::uint8 y) { return (juce::uint8) std::round(float(x) * (1 - t) + float(y) * t); };
    return Colour(ch(a.getRed(), b.getRed()), ch(a.getGreen(), b.getGreen()), ch(a.getBlue(), b.getBlue()), ch(a.getAlpha(), b.getAlpha()));
}
inline const Colour BLACK(0xff000000), WHITE(0xffffffff);
inline Rc BoxAt(float w, float h, Pt c) { return {c.x - w * 0.5f, c.y - h * 0.5f, w, h}; }
inline Rc Red(Rc r, float dx, float dy) { return {r.getX() + dx, r.getY() + dy, std::max(0.0f, r.getWidth() - dx * 2), std::max(0.0f, r.getHeight() - dy * 2)}; }
inline Rc Exp(Rc r, float d) { return Red(r, -d, -d); }
inline float Len(Pt p) { return std::sqrt(p.x * p.x + p.y * p.y); }

struct Stops {
    std::vector<std::pair<float, Colour>> v;
    Stops(std::initializer_list<std::pair<float, Colour>> l) : v(l) {
        for (auto& s : v) s.first = juce::jlimit(0.0f, 1.0f, s.first);
        std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first < b.first; });
        if (v.size() > 6) v.resize(6);
        if (v.empty()) v.push_back({0.0f, Colour(0x00000000)});
    }
    explicit Stops(std::vector<std::pair<float, Colour>> l) : v(std::move(l)) {
        for (auto& s : v) s.first = juce::jlimit(0.0f, 1.0f, s.first);
        std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first < b.first; });
        if (v.size() > 6) v.resize(6);
        if (v.empty()) v.push_back({0.0f, Colour(0x00000000)});
    }
    static Stops two(Colour a, Colour b) { return Stops{{0.0f, a}, {1.0f, b}}; }
    Colour first() const { return v.front().second; }
    Colour last() const { return v.back().second; }
    Colour at(float t) const {
        if (!(t > v[0].first)) return v[0].second;
        for (size_t i = 1; i < v.size(); ++i)
            if (t <= v[i].first) { const float sp = v[i].first - v[i - 1].first; return Mix(v[i - 1].second, v[i].second, sp > 0 ? (t - v[i - 1].first) / sp : 1.0f); }
        return v.back().second;
    }
    void apply(juce::ColourGradient& g) const {
        for (size_t i = 0; i < v.size(); ++i) {
            if (i == 0 && v[i].first <= 0.0f) continue;
            if (i + 1 == v.size() && v[i].first >= 1.0f) continue;
            g.addColour(v[i].first, v[i].second);
        }
    }
};

struct Shape {
    bool ellipse = false; Rc r; float k = 0;
    static Shape E(Rc r) { return {true, r, 0}; }
    static Shape R(Rc r, float k) { return {false, r, k}; }
    Shape translated(Pt d) const { return {ellipse, r.translated(d.x, d.y), k}; }
    Shape expanded(float d) const { return {ellipse, Exp(r, d), std::max(0.0f, k + d)}; }
    juce::Path path() const {
        juce::Path p;
        if (ellipse) p.addEllipse(r);
        else { const float rad = std::min({k, r.getWidth() * 0.5f, r.getHeight() * 0.5f}); if (rad < 0.05f) p.addRectangle(r); else p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), rad); }
        return p;
    }
};

struct Paint {
    enum Kind { Solid, Linear, Radial, Conic } kind = Solid;
    Colour col; Stops st{{0.0f, Colour(0x00000000)}}; float angle = 0, rx = 0, ry = 0, start = 0; Pt c, focus;
    static Paint S(Colour c) { Paint p; p.kind = Solid; p.col = c; return p; }
    static Paint L(Stops s, float a) { Paint p; p.kind = Linear; p.st = std::move(s); p.angle = a; return p; }
    static Paint Rad(Stops s, Pt c, float rx, float ry, Pt focus) { Paint p; p.kind = Radial; p.st = std::move(s); p.c = c; p.rx = rx; p.ry = ry; p.focus = focus; return p; }
    static Paint Con(Stops s, Pt c, float start) { Paint p; p.kind = Conic; p.st = std::move(s); p.c = c; p.start = start; return p; }
};
enum class Blend { Over, Add, Screen };

// The one studio light.
struct Rig {
    float key[3], keyCol[3] = {1.0f, 0.97f, 0.92f}, fill[3] = {0.78f, 0.84f, 0.95f}, softness = 0.27f, ambient = 0.32f;
    Rig() {
        const float az = 20.0f * kPi / 180.0f, el = 65.0f * kPi / 180.0f;
        key[0] = -std::sin(az) * std::cos(el); key[1] = -std::cos(az) * std::cos(el); key[2] = std::sin(el);
    }
    Pt screenDir() const { const float l = std::sqrt(key[0] * key[0] + key[1] * key[1]); return l < 1e-6f ? Pt(0, 0) : Pt(key[0] / l, key[1] / l); }
    Pt shadowOffset(float h) const { const float k = h / std::max(key[2], 0.05f); return {-key[0] * k, -key[1] * k}; }
    float shadowSigma(float h) const { return std::max(h * softness, 0.5f); }
    static float dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
    float diffuse(const float* n) const { return std::max(0.0f, dot(n, key)); }
    float specular(const float* n, float shin) const {
        const float h[3] = {key[0], key[1], key[2] + 1.0f}; const float l = std::max(std::sqrt(dot(h, h)), 1e-6f);
        return std::pow(std::max(0.0f, dot(n, h) / l), shin);
    }
    void shade(const float* n, float* out) const {
        const float d = diffuse(n) * (1.0f - ambient);
        for (int i = 0; i < 3; ++i) out[i] = fill[i] * ambient + keyCol[i] * d;
    }
};
// The rig as seen from a frame that will be turned by `ang` afterwards.
inline Rig RigIn(float ang) {
    Rig l; const float x = l.key[0], y = l.key[1], s = std::sin(ang), c = std::cos(ang);
    l.key[0] = x * c + y * s; l.key[1] = -x * s + y * c; return l;
}
inline float LitAngle(const Rig& rig) { const Pt d = rig.screenDir(); return (d.x == 0 && d.y == 0) ? kHalfPi : std::atan2(-d.y, -d.x); }
inline float ConicStart(const Rig& rig) { const Pt d = rig.screenDir(); return std::atan2(d.y, d.x); }
inline Colour ShadeCol(const Rig& rig, Colour base, float nx, float ny, float nz, float gloss) {
    const float l = std::max(std::sqrt(nx * nx + ny * ny + nz * nz), 1e-6f); const float n[3] = {nx / l, ny / l, nz / l};
    float k[3], f[3]; rig.shade(n, k); const float up[3] = {0, 0, 1}; rig.shade(up, f);
    const float sp = rig.specular(n, 28.0f) * gloss * 255.0f;
    auto ch = [&](juce::uint8 v, int i) { return (juce::uint8) juce::jlimit(0.0f, 255.0f, std::round(float(v) * k[i] / std::max(f[i], 1e-3f) + sp * rig.keyCol[i])); };
    return Colour(ch(base.getRed(), 0), ch(base.getGreen(), 1), ch(base.getBlue(), 2), base.getAlpha());
}
inline Stops ChromeConic(Colour hi, Colour lo) {
    const Colour mid = Mix(lo, hi, 0.62f);
    return Stops{{0.0f, hi}, {0.22f, lo}, {0.46f, mid}, {0.56f, mid}, {0.80f, lo}, {1.0f, hi}};
}

// Scene-like painter over a JUCE Graphics.
struct Sc {
    Graphics& g; float devScale = 1.0f;
    explicit Sc(Graphics& gr) : g(gr) { devScale = std::max(0.25f, (float) gr.getInternalContext().getPhysicalPixelScaleFactor()); }

    static juce::ColourGradient linear(const Stops& s, Rc r, float angle) {
        const Pt c = r.getCentre(), d(std::cos(angle), std::sin(angle));
        const float L = std::abs(r.getWidth() * 0.5f * d.x) + std::abs(r.getHeight() * 0.5f * d.y);
        juce::ColourGradient cg(s.first(), c.x - d.x * L, c.y - d.y * L, s.last(), c.x + d.x * L, c.y + d.y * L, false);
        s.apply(cg); return cg;
    }
    void fillOne(const Shape& sh, const Paint& p, float alphaMul) {
        if (sh.r.getWidth() <= 0 || sh.r.getHeight() <= 0) return;
        auto path = sh.path();
        switch (p.kind) {
            case Paint::Solid: g.setColour(alphaMul < 1 ? Al(p.col, p.col.getFloatAlpha() * alphaMul) : p.col); g.fillPath(path); break;
            case Paint::Linear: g.setGradientFill(linear(p.st, sh.r, p.angle)); g.fillPath(path); break;
            case Paint::Radial: {
                const float rx = std::max(p.rx, 0.01f), ry = std::max(p.ry, 0.01f);
                const float off = Len(p.focus - p.c) / rx;
                const float R = rx * (1.0f + 0.5f * std::min(off, 1.0f));
                juce::ColourGradient cg(p.st.first(), p.focus.x, p.focus.y, p.st.last(), p.focus.x + R, p.focus.y, true);
                p.st.apply(cg);
                juce::FillType ft(cg); ft.transform = juce::AffineTransform::scale(1.0f, ry / rx, p.focus.x, p.focus.y);
                g.setFillType(ft); g.fillPath(path); break; }
            case Paint::Conic: {
                g.saveState(); g.reduceClipRegion(path);
                const float R = sh.r.getWidth() + sh.r.getHeight(); const int N = 96;
                for (int i = 0; i < N; ++i) {
                    const float t0 = float(i) / N, t1 = float(i + 1) / N + 0.006f;
                    const float a0 = p.start + 2 * kPi * t0, a1 = p.start + 2 * kPi * t1;
                    juce::Path w; w.startNewSubPath(p.c); w.lineTo(p.c.x + std::cos(a0) * R, p.c.y + std::sin(a0) * R); w.lineTo(p.c.x + std::cos(a1) * R, p.c.y + std::sin(a1) * R); w.closeSubPath();
                    g.setColour(p.st.at((t0 + t1) * 0.5f)); g.fillPath(w);
                }
                g.restoreState(); break; }
        }
    }
    // feather > 0 fades a solid edge inward (smoothstep over `feather`), stacked as translucent layers.
    void fillShape(const Shape& sh, const Paint& p, float feather = 0.0f, Blend = Blend::Over) {
        if (feather > 0.0f && p.kind == Paint::Solid) {
            const int n = 8; const float a0 = p.col.getFloatAlpha(); float prev = 0;
            for (int i = 1; i <= n; ++i) {
                const float s = feather * float(i) / n, u = float(i) / n, cov = u * u * (3 - 2 * u) * a0;
                const float a = prev < 0.999f ? (cov - prev) / (1.0f - prev) : 0.0f; prev = cov;
                if (a <= 0.002f) continue;
                const auto shr = sh.expanded(-s); if (shr.r.getWidth() <= 0 || shr.r.getHeight() <= 0) break;
                g.setColour(Al(p.col, a)); g.fillPath(shr.path());
            }
            return;
        }
        fillOne(sh, p, 1.0f);
    }
    void fillRound(Rc r, float radius, Colour top, Colour bottom, bool vertical) {
        if (top == bottom) g.setColour(top);
        else if (vertical) g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
        else g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getRight(), r.getBottom(), false));
        g.fillPath(Shape::R(r, radius).path());
    }
    void solidRound(Rc r, float radius, Colour c) { fillRound(r, radius, c, c, true); }
    void strokeRound(Rc r, float radius, Colour c, float w) { g.setColour(c); g.strokePath(Shape::R(r, radius).path(), juce::PathStrokeType(w)); }
    void strokeEllipse(Rc r, Colour c, float w) { g.setColour(c); g.drawEllipse(r, w); }
    void fillEllipseSolid(Rc r, Colour c) { g.setColour(c); g.fillEllipse(r); }
    void roundLinear(Rc r, float radius, const Stops& s, float a) { fillShape(Shape::R(r, radius), Paint::L(s, a)); }
    void ellipseLinear(Rc r, const Stops& s, float a) { fillShape(Shape::E(r), Paint::L(s, a)); }
    void roundRadial(Rc r, float radius, const Stops& s, Pt focus) { fillShape(Shape::R(r, radius), Paint::Rad(s, r.getCentre(), r.getWidth() * 0.5f, r.getHeight() * 0.5f, focus)); }
    void ellipseRadial(Rc r, const Stops& s, Pt focus) { fillShape(Shape::E(r), Paint::Rad(s, r.getCentre(), r.getWidth() * 0.5f, r.getHeight() * 0.5f, focus)); }
    void ellipseConic(Rc r, const Stops& s, float start) { fillShape(Shape::E(r), Paint::Con(s, r.getCentre(), start)); }
    void glowEllipse(Rc r, Colour c, float feather, Blend b) { fillShape(Shape::E(r), Paint::S(c), feather, b); }
    void glowRound(Rc r, float radius, Colour c, float feather, Blend b) { fillShape(Shape::R(r, radius), Paint::S(c), feather, b); }
    void line(Pt a, Pt b, Colour c, float w) { g.setColour(c); g.drawLine(a.x, a.y, b.x, b.y, w); }
    void hline(float y, float x0, float x1, Colour c, float w) { line({x0, y}, {x1, y}, c, w); }
    static juce::Path poly(const std::vector<Pt>& p, bool closed = true) {
        juce::Path path; for (size_t i = 0; i < p.size(); ++i) i == 0 ? path.startNewSubPath(p[i]) : path.lineTo(p[i]); if (closed && !p.empty()) path.closeSubPath(); return path;
    }
    void fillPoly(const std::vector<Pt>& p, Colour top, Colour bottom) {
        if (p.size() < 3) return; auto path = poly(p); auto b = path.getBounds();
        if (top == bottom) g.setColour(top); else g.setGradientFill(juce::ColourGradient(top, b.getX(), b.getY(), bottom, b.getRight(), b.getBottom(), false));
        g.fillPath(path);
    }
    void strokePoly(const std::vector<Pt>& p, Colour c, float w, bool closed) { if (p.size() < 2) return; g.setColour(c); g.strokePath(poly(p, closed), juce::PathStrokeType(w)); }
    void strokeArc(Pt c, float radius, float from, float to, Colour col, float w) {
        juce::Path a; a.addCentredArc(c.x, c.y, radius, radius, 0, from + kHalfPi, to + kHalfPi, true); g.setColour(col); g.strokePath(a, juce::PathStrokeType(w));
    }
    void clipRound(Rc r, float radius) { g.saveState(); g.reduceClipRegion(Shape::R(r, radius).path()); }
    void popClip() { g.restoreState(); }
    void textAt(Pt at, const juce::String& t, Colour c, float size) {
        g.setColour(c); g.setFont(juce::FontOptions(size, juce::Font::bold)); g.drawText(t, BoxAt(400, size * 2.0f, at), juce::Justification::centred, false);
    }
    void textC(Rc r, const juce::String& t, Colour c, float size) {
        g.setColour(c); g.setFont(juce::FontOptions(size, juce::Font::bold)); g.drawText(t, r, juce::Justification::centred, false);
    }
    void shadow(const Shape& sh, float sigma, Colour c) {
        if (sh.r.getWidth() <= 0 || sh.r.getHeight() <= 0 || c.getAlpha() == 0) return;
        juce::DropShadow ds(c, juce::jmax(1, int(std::round(sigma * 1.7f))), {0, 0}); ds.drawForPath(g, sh.path());
    }
    void innerShadow(const Shape& sh, Pt offset, float sigma, Colour c) {
        if (sh.r.getWidth() <= 0 || sh.r.getHeight() <= 0) return;
        auto path = sh.path(); g.saveState(); g.reduceClipRegion(path);
        juce::Path fr; fr.setUsingNonZeroWinding(false);
        fr.addRectangle(Exp(sh.r, std::abs(offset.x) + std::abs(offset.y) + sigma * 5.0f + 6.0f));
        fr.addPath(sh.translated(offset).path());
        juce::DropShadow ds(c, juce::jmax(1, int(std::round(sigma * 1.7f))), {0, 0}); ds.drawForPath(g, fr);
        g.restoreState();
    }
};

inline void dropShadow(Sc& s, const Shape& sh, const Rig& rig, float height, float alpha) {
    s.shadow(sh.translated(rig.shadowOffset(height)), rig.shadowSigma(height), Al(BLACK, alpha));
}

// Neon tube along a path: soft halo, sleeve, tube, hot core. Under 0.2 of full light it is dark glass.
inline void Tube(Sc& s, const juce::Path& p, Colour c, float w, float strength, bool glow) {
    Graphics& g = s.g; const float k = strength * c.getFloatAlpha();
    auto st = [&](Colour col, float width) { g.setColour(col); g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); };
    if (k < 0.2f) {
        const Colour c1 = Al(c, 1.0f);
        st(Al(BLACK, 0.5f), w + 1.0f); st(Mix(Dk(c1, 0.66f), Colour(0xff3a4046), 0.30f), w); st(Al(Mix(c1, WHITE, 0.45f), 0.22f), w * 0.3f);
        return;
    }
    if (!glow) { st(c, w); return; }
    for (int j = 4; j >= 1; --j) st(Al(c, 0.05f * k * float(5 - j)), w + (3.5f + w * 1.6f) * 0.5f * float(j));
    st(Al(c, 0.30f * k), w + 2.2f); st(c, w);
    st(Al(Br(c, 0.7f), 0.9f * std::sqrt(juce::jlimit(0.0f, 1.0f, k))), w * 0.35f);
}
inline juce::Path RoundPath(Rc r, float radius) { return Shape::R(r, radius).path(); }

// Octagon (top-right-only = false) of Retro's chamfer, as points.
inline std::vector<Pt> ChamferPts(Rc r, float ch) {
    return {{r.getX() + ch, r.getY()}, {r.getRight() - ch, r.getY()}, {r.getRight(), r.getY() + ch}, {r.getRight(), r.getBottom() - ch},
            {r.getRight() - ch, r.getBottom()}, {r.getX() + ch, r.getBottom()}, {r.getX(), r.getBottom() - ch}, {r.getX(), r.getY() + ch}};
}

// Recess a key sits in: dark at the lit side where the rim shades it, a lip of light opposite.
inline void KeyWell(Sc& s, Rc well, float radius, const Rig& rig) {
    s.roundLinear(Exp(well, 0.8f), radius + 0.8f, Stops{{0.0f, Colour(0xff000000)}, {0.6f, Colour(0xff16181a)}, {1.0f, Colour(0xff4a4e53)}}, LitAngle(rig));
    s.solidRound(well, radius, Colour(0xff050606));
    s.innerShadow(Shape::R(well, radius), rig.shadowOffset(2.0f), 1.4f, Al(BLACK, 0.9f));
}
inline void Spill(Sc& s, const Shape& shape, Colour colour, float k) {
    const float d = std::min(shape.r.getHeight(), shape.r.getWidth()) * 0.42f;
    s.fillShape(shape.expanded(d * 0.9f), Paint::S(Al(colour, 0.17f * k)), d * 1.15f, Blend::Over);
    s.fillShape(shape.expanded(d), Paint::S(Al(colour, 0.38f * k)), d * 1.25f, Blend::Add);
}
// Light from inside a translucent cap: edge falloff, hot centre, then bloom.
inline void LampGlow(Sc& s, const Shape& shape, Colour colour, bool glow) {
    const Rc r = shape.r; const Pt c = r.getCentre();
    s.fillShape(shape, Paint::Rad(Stops{{0.0f, Al(BLACK, 0.0f)}, {0.5f, Al(BLACK, 0.08f)}, {1.0f, Al(BLACK, 0.48f)}}, c, r.getWidth() * 0.60f, r.getHeight() * 0.95f, c));
    const Colour warm = Br(colour, 0.45f), hot = Mix(colour, Colour(0xfffff6e6), 0.80f);
    s.fillShape(shape, Paint::Rad(Stops{{0.0f, Al(hot, 0.98f)}, {0.30f, Al(Mix(hot, warm, 0.4f), 0.82f)}, {0.60f, Al(warm, 0.40f)}, {0.85f, Al(warm, 0.10f)}, {1.0f, Al(warm, 0.0f)}}, c, r.getWidth() * 0.50f, r.getHeight() * 0.72f, c));
    if (glow) Spill(s, shape, colour, 1.0f);
}
// Neutral darkening round a lit part, drawn first, so bloom has something to stand against on pale panels.
inline void LitSurround(Sc& s, const Shape& shape) {
    const float d = std::min(shape.r.getHeight(), shape.r.getWidth()) * 0.22f;
    s.shadow(shape.expanded(d), d * 1.1f, Al(BLACK, 0.20f));
}
inline void HaloText(Sc& s, Rc r, const juce::String& text, Colour ink, Colour halo, float size) {
    const Pt at = r.getCentre(); const float d = std::max(0.5f, 1.0f / s.devScale);
    s.glowRound(Exp(BoxAt(size * 0.55f * float(text.length()), size * 0.5f, at), size * 0.6f), size * 0.6f, Al(halo, 0.16f), size * 0.6f, Blend::Add);
    const Pt offs[4] = {{-d, 0}, {d, 0}, {0, -d}, {0, d}};
    for (auto o : offs) s.textAt({at.x + o.x, at.y + o.y}, text, Al(halo, 0.22f), size);
    s.textAt(at, text, ink, size);
}
// Octagon as a raised pad: eight bevel facets shaded from their normals, lit creases, graded top.
inline void FacetPad(Sc& s, const std::vector<Pt>& outer, const std::vector<Pt>& inner, Colour base, Colour topA, Colour topB, const Rig& rig, float gloss) {
    const size_t n = std::min(outer.size(), inner.size());
    s.fillPoly(outer, Dk(base, 0.4f), Dk(base, 0.4f));
    std::vector<float> face; const Pt sd = rig.screenDir();
    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + 1) % n; const Pt e = outer[j] - outer[i]; const float len = std::max(Len(e), 1e-4f);
        const float nx = e.y / len, ny = -e.x / len;
        const Colour col = ShadeCol(rig, base, nx * 0.95f, ny * 0.95f, 0.52f, gloss);
        s.fillPoly({outer[i], outer[j], inner[j], inner[i]}, col, col);
        face.push_back(nx * sd.x + ny * sd.y);
    }
    for (size_t i = 0; i < n; ++i) s.line(outer[i], inner[i], Al(BLACK, 0.28f), 0.5f);
    s.fillPoly(inner, topA, topB);
    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + 1) % n;
        if (face[i] > 0) s.line(inner[i], inner[j], Al(WHITE, 0.34f * face[i]), 0.6f); else s.line(inner[i], inner[j], Al(BLACK, 0.30f * -face[i]), 0.6f);
    }
    s.strokePoly(outer, Al(BLACK, 0.7f), 0.7f, true);
}
inline void FacetWell(Sc& s, const std::vector<Pt>& path, const Rig& rig) {
    s.fillPoly(path, Colour(0xff030404), Colour(0xff030404));
    const Pt d = rig.screenDir();
    for (size_t i = 0; i < path.size(); ++i) {
        const Pt p = path[i], q = path[(i + 1) % path.size()], e = q - p; const float len = std::max(Len(e), 1e-4f), f = (e.y * d.x - e.x * d.y) / len;
        s.line(p, q, f < 0 ? Al(WHITE, 0.26f * -f) : Al(BLACK, 0.6f * f), 0.8f);
    }
}
inline float KeySize(Rc face) { return std::min(face.getHeight() * 0.42f, 13.0f); }

// Rectangular cap in a well: skirt, bevelled rim, dished or barrelled face.
inline void CapKey(Sc& s, Rc r, const juce::String& text, float radius, Colour base, Colour ink, Colour colour, bool lit, bool down, bool glow, float travel, bool concave) {
    Rig rig; const float la = LitAngle(rig);
    if (lit && glow) LitSurround(s, Shape::R(Exp(r, 2.0f), radius + 1.5f));
    KeyWell(s, Exp(r, 2.0f), radius + 1.5f, rig);
    if (lit) s.glowRound(Exp(r, 2.0f), radius + 1.5f, Al(Br(colour, 0.3f), 0.60f), 2.6f, Blend::Add);
    const Rc face(r.getX(), r.getY() + (down ? travel * 0.75f : 0.0f), r.getWidth(), r.getHeight() - travel);
    const Rc cap(r.getX(), face.getY(), r.getWidth(), r.getBottom() - face.getY());
    dropShadow(s, Shape::R(cap, radius), rig, down ? 1.0f : 2.0f + travel, down ? 0.5f : 0.7f);
    const Colour body = lit ? Dk(colour, 0.34f) : base;
    s.roundLinear(cap, radius, Stops{{0.0f, Dk(body, 0.45f)}, {1.0f, Dk(body, 0.78f)}}, kHalfPi);
    s.roundLinear(face, radius, Stops{{0.0f, Br(body, 0.55f)}, {0.45f, Br(body, 0.05f)}, {1.0f, Dk(body, 0.50f)}}, la);
    const float bev = juce::jlimit(1.0f, 2.2f, face.getHeight() * 0.055f);
    const Rc inner = Red(face, bev, bev); const float ir = std::max(radius - bev, 0.5f);
    s.roundLinear(inner, ir, concave ? Stops{{0.0f, Dk(body, 0.20f)}, {0.5f, Dk(body, 0.04f)}, {1.0f, Br(body, 0.16f)}} : Stops{{0.0f, Br(body, 0.22f)}, {0.5f, body}, {1.0f, Dk(body, 0.26f)}}, la);
    const Pt c = inner.getCentre();
    if (concave) s.fillShape(Shape::R(inner, ir), Paint::Rad(Stops{{0.0f, Al(BLACK, 0.16f)}, {1.0f, Al(BLACK, 0.0f)}}, c, inner.getWidth() * 0.6f, inner.getHeight() * 0.75f, {c.x, c.y - inner.getHeight() * 0.1f}));
    if (lit) LampGlow(s, Shape::R(inner, ir), colour, glow);
    const float gh = inner.getHeight() * 0.36f, gy = concave ? inner.getBottom() - gh - 0.5f : inner.getY() + 0.5f;
    s.glowRound({inner.getX() + 1.0f, gy, inner.getWidth() - 2.0f, gh}, std::min(ir, gh * 0.5f), Al(WHITE, down ? 0.06f : lit ? 0.20f : 0.11f), gh * 0.45f, Blend::Screen);
    s.strokeRound(face, radius, Al(BLACK, 0.45f), 0.6f);
    if (down) {
        s.innerShadow(Shape::R(face, radius), rig.shadowOffset(travel + 2.0f), 1.8f, Al(BLACK, lit ? 0.30f : 0.55f));
        if (!lit) s.solidRound(face, radius, Al(BLACK, 0.10f));
    }
    if (lit) {
        s.strokeRound(Red(face, 0.35f, 0.35f), radius, Al(Mix(colour, WHITE, 0.62f), 0.80f), 0.7f);
        s.strokeRound(inner, ir, Al(Mix(colour, WHITE, 0.4f), 0.28f), 0.6f);
    }
    s.textC(face, text, ink, KeySize(face));
}
// Black moulded ring with an inward chamfer, for round buttons.
inline void DarkBezel(Sc& s, Rc ring, Rc hole, const Rig& rig) {
    const float la = LitAngle(rig);
    dropShadow(s, Shape::E(ring), rig, 2.0f, 0.6f);
    s.ellipseLinear(ring, Stops{{0.0f, Colour(0xff6a6f75)}, {0.3f, Colour(0xff26292c)}, {1.0f, Colour(0xff050606)}}, la);
    const Rc mid((ring.getX() + hole.getX()) * 0.5f, (ring.getY() + hole.getY()) * 0.5f, (ring.getWidth() + hole.getWidth()) * 0.5f, (ring.getHeight() + hole.getHeight()) * 0.5f);
    s.ellipseLinear(mid, Stops{{0.0f, Colour(0xff000000)}, {0.6f, Colour(0xff141618)}, {1.0f, Colour(0xff45494e)}}, la);
    s.fillEllipseSolid(hole, Colour(0xff030404));
    s.innerShadow(Shape::E(hole), rig.shadowOffset(2.0f), 1.3f, Al(BLACK, 0.9f));
}
} // namespace sw3d

// ---- rockers -----------------------------------------------------------------
enum class RockerStyle { Lamp, Classic, Slim, Neon };
inline constexpr const char* rockerNames[] = {"Lamp rocker", "Classic rocker", "Slim rocker", "Neon rocker"};

// A rocker is wider than tall when horizontal = true (I/O left-right), else vertical. `glow` turns
// on the bloom round lit lamps and tubes.
inline void drawRocker(Graphics& g, Rectangle<float> r, bool on, Colour lamp, RockerStyle style, bool horizontal = false, bool glow = true) {
    using namespace sw3d;
    g.saveState();
    const float ang = horizontal ? kHalfPi : 0.0f;
    if (horizontal) g.addTransform(juce::AffineTransform::rotation(kHalfPi, r.getCentreX(), r.getCentreY()));
    Sc s(g);
    const Rig rig = RigIn(ang); const float la = LitAngle(rig);
    const Rc b = horizontal ? BoxAt(r.getHeight(), r.getWidth(), r.getCentre()) : r;
    const float w = b.getWidth(), h = b.getHeight(), cx = b.getCentreX();

    // Bezel: a moulded frame standing just proud of the panel, chamfered into the well.
    const Rc bezel = Exp(b, 1.5f);
    dropShadow(s, Shape::R(bezel, 4.5f), rig, 2.0f, 0.55f);
    s.roundLinear(bezel, 4.5f, Stops{{0.0f, Colour(0xff4b4f54)}, {0.35f, Colour(0xff1d1f22)}, {1.0f, Colour(0xff060707)}}, la);
    s.roundLinear(Red(bezel, 1.0f, 1.0f), 3.8f, Stops{{0.0f, Colour(0xff232629)}, {1.0f, Colour(0xff111214)}}, la);
    const float t = std::max(std::min(w, h) * 0.085f, 2.0f);
    const Rc well = Red(b, t, t * 0.8f);
    s.roundLinear(Exp(well, 0.8f), 3.4f, Stops{{0.0f, Colour(0xff000000)}, {1.0f, Colour(0xff3a3d41)}}, la);
    s.solidRound(well, 2.8f, Colour(0xff020303));
    s.innerShadow(Shape::R(well, 2.8f), rig.shadowOffset(2.0f), 1.5f, Al(BLACK, 0.9f));

    // Paddle: a shallow V seesaw. Every surface is shaded from its normal, so which half is
    // bright follows the light.
    const Rc plate = Red(well, 1.3f, 1.5f);
    const float split = plate.getCentreY();
    const Rc top(plate.getX(), plate.getY(), plate.getWidth(), split - plate.getY());
    const Rc bottom(plate.getX(), split, plate.getWidth(), plate.getBottom() - split);
    const Colour body = style == RockerStyle::Classic ? Colour(0xff2f3134) : style == RockerStyle::Slim ? Colour(0xffc6c8c3) : style == RockerStyle::Neon ? Colour(0xff0e151d) : Colour(0xff34363a);
    const bool light = style == RockerStyle::Slim;
    const float gloss = light ? 0.05f : 0.13f;
    const float dir = on ? -1.0f : 1.0f;                      // from the pivot toward the raised end
    const float halfLen = plate.getHeight() * 0.5f, rise = 1.1f;
    const Rc raised = on ? Rc(plate.getX() - 0.5f, plate.getY() - rise, plate.getWidth() + 1.0f, halfLen + rise) : Rc(plate.getX() - 0.5f, split, plate.getWidth() + 1.0f, halfLen + rise);
    const Rc sunk = on ? Rc(plate.getX() + 0.9f, split, plate.getWidth() - 1.8f, halfLen - 1.7f) : Rc(plate.getX() + 0.9f, plate.getY() + 1.7f, plate.getWidth() - 1.8f, halfLen - 1.7f);
    const bool litSide = rig.screenDir().y * dir > 0.0f;
    auto along = [&](float toward, Colour col, const std::function<float(float)>& slope) {
        std::vector<std::pair<float, Colour>> st;
        for (int i = 0; i < 6; ++i) {
            const float p = float(i) / 5.0f, u = toward < 0 ? 1.0f - p : p;
            st.push_back({p, ShadeCol(rig, col, 0.0f, -slope(u) * toward * 1.9f, 1.0f, gloss)});
        }
        return Paint::L(Stops(st), kHalfPi);
    };
    // Pressed half first: it lies under everything else.
    s.fillShape(Shape::R(sunk, 1.6f), along(-dir, Dk(body, 0.10f), [](float u) { return -0.80f + 0.55f * u * u; }));
    s.roundLinear(sunk, 1.6f, Stops{{0.0f, Al(WHITE, 0.08f)}, {0.08f, Al(WHITE, 0.0f)}, {0.88f, Al(BLACK, 0.0f)}, {1.0f, Al(BLACK, 0.5f)}}, 0.0f);
    const float pv = litSide ? 0.62f : 0.34f;
    const Stops pvStops = on ? Stops{{0.0f, Al(BLACK, pv)}, {0.22f, Al(BLACK, pv * 0.55f)}, {0.55f, Al(BLACK, 0.0f)}}
                             : Stops{{0.45f, Al(BLACK, 0.0f)}, {0.78f, Al(BLACK, pv * 0.55f)}, {1.0f, Al(BLACK, pv)}};
    s.fillShape(Shape::R(sunk, 1.6f), Paint::L(pvStops, kHalfPi));
    const Rc sunkZone = on ? Rc(well.getX(), split, well.getWidth(), well.getBottom() - split) : Rc(well.getX(), well.getY(), well.getWidth(), split - well.getY());
    s.clipRound(sunkZone, 0.0f);
    s.innerShadow(Shape::R(well, 2.8f), rig.shadowOffset(5.0f), 2.6f, Al(BLACK, 0.85f));
    s.popClip();
    s.strokeRound(sunk, 1.6f, Al(BLACK, 0.55f), 0.6f);

    // Raised half: shadow onto the bezel, side walls, end wall, then the face.
    dropShadow(s, Shape::R(raised, 2.0f), rig, 3.5f, 0.6f);
    const float wallH = 2.6f;
    const Rc wall = on ? Rc(raised.getX(), raised.getY() - 0.2f, raised.getWidth(), wallH + 1.5f) : Rc(raised.getX(), raised.getBottom() - wallH - 1.3f, raised.getWidth(), wallH + 1.5f);
    const Colour wallBase = Br(body, light ? 0.0f : 0.10f);
    const Colour sideL = ShadeCol(rig, wallBase, -0.92f, 0.0f, 0.38f, 0.5f), sideR = ShadeCol(rig, wallBase, 0.92f, 0.0f, 0.38f, 0.5f);
    s.roundLinear(raised, 2.0f, Stops{{0.0f, sideL}, {0.12f, Mix(sideL, body, 0.5f)}, {0.88f, Mix(sideR, body, 0.5f)}, {1.0f, sideR}}, 0.0f);
    const Colour wallC = ShadeCol(rig, wallBase, 0.0f, dir * 0.92f, 0.38f, 0.9f);
    s.roundLinear(wall, 2.0f, Stops::two(Br(wallC, 0.10f), Dk(wallC, 0.25f)), kHalfPi);
    const float sw = 1.3f;
    const Rc face = on ? Rc(raised.getX() + sw, raised.getY() + wallH * 0.55f, raised.getWidth() - sw * 2, raised.getHeight() - wallH * 0.55f) : Rc(raised.getX() + sw, raised.getY(), raised.getWidth() - sw * 2, raised.getHeight() - wallH * 0.55f);
    s.fillShape(Shape::R(face, 1.8f), along(dir, body, [](float u) { return u < 0.30f ? 0.46f - 1.3f * u : u < 0.66f ? 0.07f : 0.07f - 2.6f * (u - 0.66f); }));
    const float edge = light ? 0.30f : 0.55f;
    s.roundLinear(face, 1.8f, Stops{{0.0f, Al(WHITE, 0.20f)}, {0.07f, Al(WHITE, 0.0f)}, {0.88f, Al(BLACK, 0.0f)}, {1.0f, Al(BLACK, edge)}}, 0.0f);
    const float sheenY = face.getY() + face.getHeight() * (on ? 0.52f : 0.48f);
    s.glowRound(BoxAt(face.getWidth() - 3.0f, face.getHeight() * 0.42f, {cx, sheenY}), face.getHeight() * 0.2f, Al(WHITE, light ? 0.20f : 0.085f), face.getHeight() * 0.2f, Blend::Screen);
    s.strokeRound(face, 1.8f, Al(BLACK, 0.5f), 0.6f);
    s.hline(split, plate.getX() - 0.3f, plate.getRight() + 0.3f, Al(BLACK, light ? 0.5f : 0.8f), 1.1f);
    s.hline(split + dir * 1.0f, face.getX() + 1.0f, face.getRight() - 1.0f, Al(WHITE, light ? 0.45f : 0.16f), 0.6f);
    const float turn = on ? face.getY() + 0.3f : face.getBottom() - 0.3f;
    s.hline(turn, face.getX() + 1.2f, face.getRight() - 1.2f, Al(WHITE, litSide ? (light ? 0.7f : 0.38f) : 0.10f), 0.6f);

    const Colour ink = light ? Colour(0xff222426) : Colour(0xfff2f2ee);
    const float mark = std::min(w, h) * 0.15f;
    const bool lamped = style == RockerStyle::Lamp || style == RockerStyle::Neon;
    const float topMarkY = top.getCentreY() + (lamped ? h * 0.05f : 0.0f);
    s.solidRound(BoxAt(mark * 0.45f, mark * 1.7f, {cx, topMarkY}), 0.8f, Al(ink, on ? 1.0f : 0.8f));
    s.strokeEllipse(BoxAt(mark * 1.5f, mark * 1.5f, {cx, bottom.getCentreY()}), Al(ink, on ? 0.8f : 1.0f), mark * 0.34f);
    if (lamped) {
        const Rc win = BoxAt(plate.getWidth() * 0.56f, std::max(h * 0.07f, 2.2f), {cx, top.getY() + h * 0.085f});
        s.solidRound(Exp(win, 0.7f), 1.8f, Colour(0xff020202));
        if (on) {
            const Colour hot = Br(lamp, 0.75f);
            s.roundRadial(win, 1.2f, Stops{{0.0f, hot}, {0.7f, lamp}, {1.0f, Dk(lamp, 0.25f)}}, win.getCentre());
            if (glow) {
                s.glowRound(Exp(win, h * 0.12f), h * 0.12f, Al(lamp, 0.13f), h * 0.14f, Blend::Over);
                s.glowRound(Exp(win, h * 0.22f), h * 0.22f, Al(lamp, 0.55f), h * 0.24f, Blend::Add);
            } else s.solidRound(Exp(win, 2.0f), 3.0f, Al(lamp, 0.25f));
            s.solidRound({win.getX() + 1.0f, win.getY() + win.getHeight() * 0.2f, win.getWidth() - 2.0f, win.getHeight() * 0.3f}, 0.6f, Al(WHITE, 0.55f));
        } else {
            s.roundLinear(win, 1.2f, Stops::two(Dk(lamp, 0.88f), Dk(lamp, 0.72f)), kHalfPi);
            s.hline(win.getY() + 0.6f, win.getX() + 1.0f, win.getRight() - 1.0f, Al(WHITE, 0.10f), 0.5f);
        }
    }
    if (style == RockerStyle::Neon) {
        if (on && glow) {
            s.glowRound(Exp(plate, 4.0f), 6.0f, Al(lamp, 0.10f), 7.0f, Blend::Over);
            s.glowRound(Exp(plate, 4.0f), 6.0f, Al(lamp, 0.16f), 7.0f, Blend::Add);
        }
        Tube(s, RoundPath(Red(plate, 0.2f, 0.2f), 2.2f), on ? lamp : Al(lamp, 0.35f), 1.0f, on ? 1.0f : 0.2f, glow);
    }
    g.restoreState();
}

// ---- toggles -----------------------------------------------------------------
enum class ToggleStyle { Bat, Lever, Mini };
inline constexpr const char* toggleNames[] = {"Bat toggle", "Lever toggle", "Mini toggle"};

// Toggle with the lever anywhere along its throw: pos +1 is fully up (on), -1 fully down, and 0
// points straight at the viewer, where the lever is foreshortened to its tip.
inline void drawToggleAt(Graphics& g, Rectangle<float> r, float pos, ToggleStyle style, Colour accent = Colour(0xfff2a31b)) {
    using namespace sw3d;
    Sc s(g); const Rig rig; const Pt ld = rig.screenDir();
    pos = std::isfinite(pos) ? juce::jlimit(-1.0f, 1.0f, pos) : 0.0f;
    const Pt c = r.getCentre(); const float sz = std::min(r.getWidth(), r.getHeight() * 0.8f);
    const float nutR = sz * (style == ToggleStyle::Mini ? 0.30f : 0.38f), rr = nutR * 1.12f;
    auto circ = [](Pt p, float rad) { return BoxAt(rad * 2, rad * 2, p); };

    // Washer on the panel.
    const Rc washer = circ(c, rr * 1.13f);
    dropShadow(s, Shape::E(washer), rig, 1.2f, 0.6f);
    s.ellipseConic(washer, ChromeConic(Colour(0xffd9dde0), Colour(0xff70787e)), ConicStart(rig));
    s.strokeEllipse(washer, Al(Colour(0xff1c2023), 0.8f), 0.6f);
    s.strokeEllipse(Red(washer, 0.7f, 0.7f), Al(WHITE, 0.35f), 0.5f);

    // Hex nut: chamfer facets shaded from their normals, flat top with a soft reflection.
    const Colour steel(0xffc3c9cd);
    auto corner = [&](int i, float k) { const float a = float(i) * kPi / 3.0f; return Pt(c.x + std::cos(a) * rr * k, c.y + std::sin(a) * rr * k); };
    std::vector<Pt> outer, inner; for (int i = 0; i < 6; ++i) { outer.push_back(corner(i, 1.0f)); inner.push_back(corner(i, 0.80f)); }
    dropShadow(s, Shape::E(circ(c, rr * 0.93f)), rig, 3.0f, 0.6f);
    s.fillPoly(outer, Colour(0xff8b9399), Colour(0xff8b9399));
    for (int i = 0; i < 6; ++i) {
        const float am = (float(i) + 0.5f) * kPi / 3.0f; const Colour col = ShadeCol(rig, steel, std::cos(am) * 0.9f, std::sin(am) * 0.9f, 0.45f, 1.3f); const int j = (i + 1) % 6;
        s.fillPoly({outer[i], outer[j], inner[j], inner[i]}, col, col);
    }
    for (int i = 0; i < 6; ++i) s.line(outer[i], inner[i], Al(Colour(0xff14171a), 0.35f), 0.5f);
    s.fillPoly(inner, Colour(0xfff4f6f7), Colour(0xff9aa2a8));
    s.strokePoly(inner, Al(WHITE, 0.45f), 0.5f, true);
    s.strokePoly(outer, Colour(0xff14171a), 0.7f, true);

    // Threaded bushing, seen end on.
    const float rb = nutR * 0.66f; const Rc bush = circ(c, rb);
    dropShadow(s, Shape::E(bush), rig, 1.5f, 0.5f);
    s.ellipseConic(bush, ChromeConic(Colour(0xffffffff), Colour(0xff596168)), ConicStart(rig));
    for (float k : {0.90f, 0.78f}) {
        s.strokeEllipse(circ(c, rb * k), Al(Colour(0xff0c0e10), 0.55f), rb * 0.05f);
        s.strokeEllipse(circ(c, rb * (k - 0.05f)), Al(WHITE, 0.30f), rb * 0.03f);
    }
    s.strokeEllipse(bush, Colour(0xff101214), 0.6f);
    const Rc socket = circ(c, rb * 0.64f);
    s.ellipseRadial(socket, Stops::two(Colour(0xff26292c), Colour(0xff040505)), c);
    s.innerShadow(Shape::E(socket), rig.shadowOffset(2.5f), 1.2f, Al(BLACK, 0.9f));

    // Lever: a rod leaning out of the panel.
    const bool bat = style == ToggleStyle::Bat, mini = style == ToggleStyle::Mini;
    const float len = r.getHeight() * (bat ? 0.33f : 0.42f), sinMax = 0.77f;
    const float cosT = std::sqrt(1.0f - (pos * sinMax) * (pos * sinMax));
    const float tipH = len / sinMax * cosT, nearK = 0.86f + 0.22f * cosT;
    const Pt tip(c.x, c.y - pos * len);
    const float w0 = sz * (bat ? 0.105f : mini ? 0.07f : 0.085f);
    const float w1 = sz * (bat ? 0.20f : mini ? 0.095f : 0.12f) * nearK;
    const bool black = bat;
    const Stops ramp = black ? Stops{{0.0f, Colour(0xff040405)}, {0.14f, Colour(0xff303438)}, {0.27f, Colour(0xffaab0b5)}, {0.40f, Colour(0xff2a2d30)}, {0.78f, Colour(0xff060607)}, {1.0f, Colour(0xff34383c)}}
                             : Stops{{0.0f, Colour(0xff4d555b)}, {0.12f, Colour(0xffb4bbc0)}, {0.28f, Colour(0xffffffff)}, {0.46f, Colour(0xffb0b7bc)}, {0.78f, Colour(0xff343a3f)}, {1.0f, Colour(0xff8b9399)}};
    // Cast shadow: the rod's shadow runs from the socket to where the tip's lands.
    const int n = 7;
    for (int i = 0; i < n; ++i) {
        const float t = (float(i) + 0.5f) / float(n), hgt = 2.0f + tipH * t;
        const Pt so = rig.shadowOffset(hgt); const Pt p(c.x + (tip.x - c.x) * t + so.x, c.y + (tip.y - c.y) * t + so.y);
        s.shadow(Shape::E(circ(p, (w0 + (w1 - w0) * t) * 0.95f)), rig.shadowSigma(hgt) * 0.55f, Al(BLACK, 0.24f + 0.14f * (1.0f - t)));
    }
    const Pt tsh = rig.shadowOffset(2.0f + tipH);
    s.shadow(Shape::E(circ({tip.x + tsh.x, tip.y + tsh.y}, w1)), rig.shadowSigma(tipH) * 0.55f, Al(BLACK, 0.38f));

    const float dx = tip.x - c.x, dy = tip.y - c.y, seen = std::sqrt(dx * dx + dy * dy);
    if (seen > 0.5f) {
        const Pt perp(-dy / seen, dx / seen);
        // The rod is a tapered cylinder: thin slices along its length, each a full gradient across its own width.
        const float ang = ld.x <= 0 ? 0.0f : kPi;
        const int slices = juce::jlimit(6, 120, int(std::ceil(seen / 0.5f)));
        for (int j = 0; j < slices; ++j) {
            const float t0 = float(j) / slices, t1 = float(j + 1) / slices, ya = c.y + dy * t0, yb = c.y + dy * t1, hw = w0 + (w1 - w0) * (t0 + t1) * 0.5f;
            s.fillShape(Shape::R({c.x - hw, std::min(ya, yb) - 0.06f, hw * 2, std::abs(ya - yb) + 0.12f}, 0.0f), Paint::L(ramp, ang));
        }
        const Colour nearC = Al(BLACK, 0.0f), farC = Al(BLACK, black ? 0.45f : 0.38f);
        const bool up = tip.y < c.y; const Colour g0 = up ? nearC : farC, g1 = up ? farC : nearC;
        auto q = [&](float u0, float u1) { return std::vector<Pt>{{c.x + perp.x * w0 * u0, c.y + perp.y * w0 * u0}, {tip.x + perp.x * w1 * u0, tip.y + perp.y * w1 * u0}, {tip.x + perp.x * w1 * u1, tip.y + perp.y * w1 * u1}, {c.x + perp.x * w0 * u1, c.y + perp.y * w0 * u1}}; };
        s.fillPoly(q(-1.0f, 1.0f), g0, g1);
        s.strokePoly({{c.x - perp.x * w0, c.y - perp.y * w0}, {tip.x - perp.x * w1, tip.y - perp.y * w1}}, Al(BLACK, 0.55f), 0.6f, false);
        s.strokePoly({{c.x + perp.x * w0, c.y + perp.y * w0}, {tip.x + perp.x * w1, tip.y + perp.y * w1}}, Al(BLACK, 0.55f), 0.6f, false);
        // Where the rod leaves the socket it is in the bushing's shade.
        s.clipRound(socket, rb);
        s.glowEllipse(socket, Al(BLACK, 0.55f), rb * 0.5f, Blend::Over);
        s.popClip();
    }

    // Rounded end of the bat, nearest the viewer.
    const Rc ball = circ(tip, w1); const Pt focus(tip.x + ld.x * w1 * 0.42f, tip.y + ld.y * w1 * 0.42f);
    if (black) s.ellipseRadial(ball, Stops{{0.0f, Colour(0xffb9bfc4)}, {0.16f, Colour(0xff4a4f54)}, {0.5f, Colour(0xff17191b)}, {1.0f, Colour(0xff030303)}}, focus);
    else s.ellipseRadial(ball, Stops{{0.0f, Colour(0xffffffff)}, {0.3f, Colour(0xffe3e7e9)}, {0.7f, Colour(0xff8f979d)}, {1.0f, Colour(0xff3c4247)}}, focus);
    s.strokeEllipse(ball, Al(BLACK, 0.6f), 0.6f);
    const float lang = std::atan2(ld.y, ld.x);
    s.strokeArc(tip, w1 - 0.7f, lang - 0.95f, lang + 0.95f, Al(WHITE, black ? 0.38f : 0.85f), 0.7f);
    s.strokeArc(tip, w1 - 0.8f, lang + kPi - 0.7f, lang + kPi + 0.7f, Al(WHITE, black ? 0.14f : 0.30f), 0.6f);
    if (style == ToggleStyle::Lever) {
        const Rc dot = circ(tip, w1 * 0.46f);
        s.fillEllipseSolid(Exp(dot, 0.6f), Colour(0xff0b0c0d));
        if (pos > 0.5f) {
            s.ellipseRadial(dot, Stops{{0.0f, Br(accent, 0.8f)}, {0.6f, accent}, {1.0f, Dk(accent, 0.3f)}}, tip);
            s.glowEllipse(circ(tip, w1 * 2.2f), Al(accent, 0.45f), w1 * 1.7f, Blend::Add);
        } else s.ellipseRadial(dot, Stops::two(Dk(accent, 0.6f), Dk(accent, 0.85f)), focus);
    }
}

inline void drawToggle(Graphics& g, Rectangle<float> r, bool on, ToggleStyle style, Colour accent = Colour(0xfff2a31b)) {
    drawToggleAt(g, r, on ? 1.0f : -1.0f, style, accent);
}

// ---- push buttons --------------------------------------------------------------
enum class PushStyle { Square, Round, Dome, Chamfer, Neon, Flat, Arcade, Pill };
inline constexpr const char* pushNames[] = {"Square key", "Round chrome", "Illuminated dome", "Chamfer pad", "Neon outline", "Flat label", "Arcade", "Pill"};
inline constexpr int numPushStyles = 8;

inline void drawPush(Graphics& g, Rectangle<float> r, const juce::String& text, PushStyle style, Colour colour, bool lit, bool down, bool glow = true) {
    using namespace sw3d;
    Sc s(g); const Rig rig; const float la = LitAngle(rig); const Pt ld = rig.screenDir();
    const float off = down ? 2.0f : 0.0f;
    const Rc face = r.translated(0, off);
    switch (style) {
        case PushStyle::Square: CapKey(s, r, text, 2.2f, Colour(0xffd0d3c3), lit ? Colour(0xff0d0f11) : Colour(0xff222a28), colour, lit, down, glow, 3.0f, true); break;
        case PushStyle::Flat: CapKey(s, r, text, 1.8f, Colour(0xff32373b), lit ? Colour(0xff0a0c0e) : Colour(0xffb8bfc2), colour, lit, down, glow, 1.6f, false); break;
        case PushStyle::Pill: CapKey(s, r, text, (r.getHeight() - 2.4f) * 0.5f, Colour(0xffc8ccca), Colour(0xff181c1e), colour, lit, down, glow, 2.4f, false); break;
        case PushStyle::Round: {
            const float d = std::min(r.getWidth(), r.getHeight()); const Rc cir = BoxAt(d, d, r.getCentre()), ring = Exp(cir, 2.0f);
            dropShadow(s, Shape::E(ring), rig, 2.5f, 0.6f);
            s.ellipseConic(ring, ChromeConic(Colour(0xffffffff), Colour(0xff5b6369)), ConicStart(rig));
            s.strokeEllipse(ring, Colour(0xff101214), 0.6f);
            s.ellipseLinear(Red(cir, d * 0.035f, d * 0.035f), Stops{{0.0f, Colour(0xff3d4348)}, {0.5f, Colour(0xff9aa2a8)}, {1.0f, Colour(0xffffffff)}}, la);
            const Rc hole = Red(cir, d * 0.085f, d * 0.085f);
            s.fillEllipseSolid(hole, Colour(0xff040505));
            if (lit) {
                s.strokeEllipse(Red(hole, d * 0.024f, d * 0.024f), colour, d * 0.05f);
                s.strokeEllipse(Red(hole, d * 0.024f, d * 0.024f), Br(colour, 0.6f), d * 0.018f);
                if (glow) {
                    s.glowEllipse(Exp(cir, d * 0.10f), Al(colour, 0.16f), d * 0.22f, Blend::Over);
                    s.glowEllipse(Exp(cir, d * 0.10f), Al(colour, 0.22f), d * 0.24f, Blend::Add);
                }
            } else s.innerShadow(Shape::E(hole), rig.shadowOffset(2.0f), 1.2f, Al(BLACK, 0.9f));
            const Rc cap = Red(cir, d * 0.15f, d * 0.15f).translated(0, off * 0.6f); const Pt cc = cap.getCentre();
            dropShadow(s, Shape::E(cap), rig, down ? 0.8f : 3.0f, down ? 0.45f : 0.65f);
            const float dim = down ? 0.14f : 0.0f;
            s.ellipseRadial(cap, Stops{{0.0f, Dk(WHITE, dim)}, {0.45f, Dk(Colour(0xffd0d5d8), dim)}, {0.85f, Dk(Colour(0xff838b91), dim)}, {1.0f, Dk(Colour(0xff4a5056), dim)}}, {cc.x + ld.x * cap.getWidth() * 0.24f, cc.y + ld.y * cap.getWidth() * 0.24f});
            s.strokeEllipse(cap, Al(BLACK, 0.55f), 0.6f);
            s.strokeEllipse(Red(cap, 0.9f, 0.9f), Al(WHITE, 0.45f), 0.6f);
            const Rc dish = Red(cap, cap.getWidth() * 0.15f, cap.getWidth() * 0.15f);
            s.ellipseLinear(dish, Stops{{0.0f, Dk(Colour(0xff8c949a), dim)}, {0.55f, Dk(Colour(0xffcfd4d7), dim)}, {1.0f, Dk(Colour(0xfff7f8f9), dim)}}, la);
            s.innerShadow(Shape::E(dish), rig.shadowOffset(1.2f), 0.9f, Al(BLACK, 0.35f));
            if (down) s.innerShadow(Shape::E(cap), rig.shadowOffset(3.0f), 1.5f, Al(BLACK, 0.5f));
            s.textC(cap, text, Colour(0xff1d2124), std::min(cap.getHeight() * 0.40f, 13.0f));
            break; }
        case PushStyle::Dome: {
            const float d = std::min(r.getWidth(), r.getHeight()); const Rc cir = BoxAt(d, d, r.getCentre()).translated(0, off * 0.5f);
            if (lit && glow) LitSurround(s, Shape::E(BoxAt(d + 6, d + 6, r.getCentre())));
            DarkBezel(s, BoxAt(d + 6, d + 6, r.getCentre()), BoxAt(d + 1.6f, d + 1.6f, r.getCentre()), rig);
            const Pt c = cir.getCentre();
            if (lit) s.ellipseRadial(cir, Stops{{0.0f, Br(colour, 0.92f)}, {0.30f, Br(colour, 0.5f)}, {0.72f, colour}, {1.0f, Dk(colour, 0.5f)}}, {c.x, c.y + d * 0.04f});
            else { const Colour base = Dk(colour, 0.68f); s.ellipseRadial(cir, Stops{{0.0f, Br(base, 0.30f)}, {0.5f, base}, {1.0f, Dk(base, 0.7f)}}, {c.x + ld.x * d * 0.2f, c.y + ld.y * d * 0.2f}); }
            s.strokeEllipse(cir, Al(BLACK, 0.6f), 0.7f);
            s.glowEllipse(BoxAt(d * 0.62f, d * 0.34f, {c.x - ld.x * d * 0.24f, c.y - ld.y * d * 0.24f}), Al(Br(colour, 0.55f), lit ? 0.40f : 0.16f), d * 0.15f, Blend::Screen);
            if (down) s.innerShadow(Shape::E(cir), rig.shadowOffset(3.0f), 1.6f, Al(BLACK, 0.5f));
            s.glowEllipse(BoxAt(d * 0.50f, d * 0.30f, {c.x + ld.x * d * 0.25f, c.y + ld.y * d * 0.25f}), Al(WHITE, lit ? 0.75f : 0.40f), d * 0.11f, Blend::Screen);
            if (lit && glow) {
                s.glowEllipse(Exp(cir, d * 0.34f), Al(colour, 0.17f), d * 0.44f, Blend::Over);
                s.glowEllipse(Exp(cir, d * 0.38f), Al(colour, 0.42f), d * 0.50f, Blend::Add);
            }
            s.textC(cir, text, lit ? Colour(0xff0f1113) : Colour(0xffeaeae4), std::min(cir.getHeight() * 0.42f, 13.0f));
            break; }
        case PushStyle::Chamfer: {
            const float ch = face.getHeight() * 0.28f; const Rc pad = Red(face, 1.0f, 1.0f); const Shape approx = Shape::R(pad, ch * 1.7f);
            if (lit && glow) LitSurround(s, Shape::R(Exp(r, 2.0f), ch * 1.7f + 2.0f));
            FacetWell(s, ChamferPts(Exp(r, 2.0f), r.getHeight() * 0.28f + 1.0f), rig);
            if (lit) s.glowRound(Exp(r, 1.5f), ch * 1.7f, Al(Br(colour, 0.3f), 0.45f), 2.4f, Blend::Add);
            dropShadow(s, approx, rig, down ? 1.0f : 4.5f, down ? 0.5f : 0.7f);
            dropShadow(s, approx, rig, 0.8f, 0.45f);
            const Colour base = lit ? Dk(colour, 0.22f) : Colour(0xff3a4047);
            const float bev = juce::jlimit(1.8f, 3.4f, face.getHeight() * 0.10f);
            const auto outer = ChamferPts(pad, ch); const Rc innerR = Red(pad, bev, bev); const float ich = std::max(ch - bev * 0.586f, 0.5f);
            const float k = down ? 0.02f : 0.14f;
            FacetPad(s, outer, ChamferPts(innerR, ich), base, Br(base, k + 0.06f), Dk(base, 0.24f), rig, 0.9f);
            const Shape top = Shape::R(Red(innerR, 0.4f, 0.4f), ich * 1.7f);
            s.fillShape(top, Paint::L(Stops{{0.0f, Al(WHITE, 0.10f)}, {0.45f, Al(WHITE, 0.0f)}, {1.0f, Al(BLACK, 0.16f)}}, la));
            if (lit) LampGlow(s, top, colour, glow);
            const float gh = innerR.getHeight() * 0.34f;
            s.glowRound({innerR.getX() + ich, innerR.getY() + 0.6f, innerR.getWidth() - ich * 2, gh}, gh * 0.5f, Al(WHITE, down ? 0.05f : lit ? 0.20f : 0.11f), gh * 0.45f, Blend::Screen);
            s.strokePoly(ChamferPts(Red(innerR, 1.3f, 1.3f), std::max(ich - 0.76f, 0.3f)), lit ? Al(Mix(colour, WHITE, 0.6f), 0.85f) : Al(colour, 0.42f), 0.8f, true);
            if (down) s.innerShadow(top, rig.shadowOffset(3.0f), 1.6f, Al(BLACK, 0.4f));
            s.textC(face, text, lit ? Colour(0xff090b0d) : Colour(0xffdbe0e2), KeySize(face));
            break; }
        case PushStyle::Neon: {
            // A gloss-black pad with shallow bevels, and a neon tube laid round its top.
            const float ch = face.getHeight() * 0.3f; const Rc pad = Red(face, 1.0f, 1.0f); const Shape approx = Shape::R(pad, ch * 1.7f);
            if (lit && glow) LitSurround(s, Shape::R(Exp(r, 2.0f), ch * 1.7f + 2.0f));
            dropShadow(s, approx, rig, down ? 0.8f : 3.5f, down ? 0.5f : 0.7f);
            dropShadow(s, approx, rig, 0.8f, 0.5f);
            const Colour base(0xff141b22);
            const float bev = juce::jlimit(1.3f, 2.4f, face.getHeight() * 0.07f);
            const auto outer = ChamferPts(pad, ch); const Rc innerR = Red(pad, bev, bev); const float ich = std::max(ch - bev * 0.586f, 0.5f);
            FacetPad(s, outer, ChamferPts(innerR, ich), base, Colour(0xff0b1016), Colour(0xff04070a), rig, 1.4f);
            const Shape top = Shape::R(Red(innerR, 0.4f, 0.4f), ich * 1.7f);
            s.fillShape(top, Paint::L(Stops{{0.0f, Al(WHITE, 0.10f)}, {0.30f, Al(WHITE, 0.03f)}, {0.42f, Al(WHITE, 0.0f)}, {1.0f, Al(WHITE, 0.0f)}}, la), 0.0f, Blend::Screen);
            const auto tubePts = ChamferPts(Red(innerR, 1.8f, 1.8f), std::max(ich - 1.05f, 0.3f));
            if (lit) {
                const Pt c = innerR.getCentre();
                s.fillShape(Shape::R(Red(innerR, 1.8f, 1.8f), ich * 1.7f), Paint::Rad(Stops{{0.0f, Al(colour, 0.10f)}, {0.6f, Al(colour, 0.20f)}, {1.0f, Al(colour, 0.42f)}}, c, innerR.getWidth() * 0.55f, innerR.getHeight() * 0.62f, c));
                if (glow) Spill(s, approx, colour, 0.75f);
            }
            Tube(s, Sc::poly(tubePts), lit ? colour : Al(colour, 0.4f), 1.4f, lit ? 1.0f : 0.15f, glow);
            if (down) s.innerShadow(top, rig.shadowOffset(3.0f), 1.6f, Al(BLACK, 0.45f));
            if (lit) HaloText(s, face, text, Br(colour, 0.85f), colour, KeySize(face));
            else s.textC(face, text, Mix(Dk(colour, 0.35f), Colour(0xff8a949c), 0.35f), KeySize(face));
            break; }
        case PushStyle::Arcade: {
            const float d = std::min(r.getWidth(), r.getHeight()); const Rc cir = BoxAt(d, d, r.getCentre());
            if (lit && glow) LitSurround(s, Shape::E(Exp(cir, 2.0f)));
            DarkBezel(s, Exp(cir, 2.0f), Red(cir, d * 0.08f, d * 0.08f), rig);
            const Rc cap = Red(cir, d * 0.12f, d * 0.12f).translated(0, off * 0.7f); const Pt cc = cap.getCentre();
            const Colour base = lit ? colour : Dk(colour, 0.55f);
            dropShadow(s, Shape::E(cap), rig, down ? 0.8f : 3.5f, down ? 0.5f : 0.7f);
            s.ellipseRadial(cap, Stops{{0.0f, Br(base, 0.70f)}, {0.35f, Br(base, 0.15f)}, {0.8f, Dk(base, 0.25f)}, {1.0f, Dk(base, 0.62f)}}, {cc.x + ld.x * cap.getWidth() * 0.30f, cc.y + ld.y * cap.getWidth() * 0.30f});
            s.strokeEllipse(cap, Al(BLACK, 0.55f), 0.6f);
            const Rc dish = Red(cap, cap.getWidth() * 0.13f, cap.getWidth() * 0.13f);
            s.ellipseLinear(dish, Stops{{0.0f, Dk(base, 0.42f)}, {0.5f, Dk(base, 0.08f)}, {1.0f, Br(base, 0.30f)}}, la);
            if (lit) LampGlow(s, Shape::E(dish), colour, glow);
            s.glowEllipse(BoxAt(cap.getWidth() * 0.42f, cap.getHeight() * 0.16f, {cc.x + ld.x * cap.getWidth() * 0.40f, cc.y + ld.y * cap.getWidth() * 0.40f}), Al(WHITE, 0.55f), cap.getHeight() * 0.07f, Blend::Screen);
            if (down) s.innerShadow(Shape::E(cap), rig.shadowOffset(3.0f), 1.5f, Al(BLACK, 0.5f));
            s.textC(cap, text, lit ? Colour(0xff0d0f11) : Al(Colour(0xffe6e2dc), 0.85f), std::min(cap.getHeight() * 0.42f, 13.0f));
            break; }
    }
}

// Row of latching keys in one frame; the selected key is lit and stays down. Unselected keys are
// drawn first so the lit key's bloom lies over its neighbours.
inline void drawKeyBank(Graphics& g, Rectangle<float> r, const juce::StringArray& names, int selected, PushStyle style, Colour colour, bool glow = true) {
    using namespace sw3d;
    const int n = names.size(); if (n == 0) return;
    const float gap = 4.0f, w = (r.getWidth() - gap * float(n - 1)) / float(n);
    auto cell = [&](int i) { return Rectangle<float>(r.getX() + float(i) * (w + gap), r.getY(), w, r.getHeight()); };
    if (style == PushStyle::Square || style == PushStyle::Flat) {
        Sc s(g); const Rig rig;
        dropShadow(s, Shape::R(Exp(r, 3.5f), 4.5f), rig, 1.5f, 0.5f);
        s.roundLinear(Exp(r, 3.5f), 4.5f, Stops{{0.0f, Colour(0xff4b4f54)}, {0.3f, Colour(0xff1c1e21)}, {1.0f, Colour(0xff060707)}}, LitAngle(rig));
        s.solidRound(Exp(r, 2.2f), 3.5f, Colour(0xff050606));
    }
    for (int i = 0; i < n; ++i) if (i != selected) drawPush(g, cell(i), names[i], style, colour, false, false, glow);
    for (int i = 0; i < n; ++i) if (i == selected) drawPush(g, cell(i), names[i], style, colour, true, true, glow);
}
} // namespace goodlookinui::juce_adapter::switches
