// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#pragma once
#include "Meters.h"
#include <vector>
#include <initializer_list>

// Retro-futuristic meters, sliders, lamps and frames: 80s car-dash displays and
// cyberpunk HUD parts. Everything is generated in code. `t` is a time in seconds used
// for gentle animation (flicker, sweeps); pass 0 for a still image.
//
// Lit parts bloom through feathered glow layers (a tight core plus a wider, fainter halo).
// Display windows are recesses under glass, and raised parts take their bevel and soft shadow
// from one studio light, also when a slider is drawn rotated. Matches GoodLookinDri retro.rs.
namespace goodlookinui::juce_adapter::retro {
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Point; using juce::Rectangle; using juce::Path;

// Global look switch: glow + scanlines + flicker on (neon) or off (clean, flat). Both are first-class.
inline bool glowEnabled = true;

namespace lit {
using Pt = Point<float>;
using Rf = Rectangle<float>;
constexpr float PI = juce::MathConstants<float>::pi, HALF_PI = juce::MathConstants<float>::halfPi;

inline float lim(float v, float lo, float hi) { return juce::jlimit(lo, hi, v); }
// Same channel maths as GoodLookinDri: brighter moves toward white, darker scales toward black.
inline Colour brt(Colour c, float a) {
    a = std::max(0.0f, a);
    auto ch = [a](juce::uint8 v) { return juce::uint8(lim(std::round(float(v) + a * (255.0f - float(v))), 0.0f, 255.0f)); };
    return Colour(ch(c.getRed()), ch(c.getGreen()), ch(c.getBlue()), c.getAlpha());
}
inline Colour drk(Colour c, float a) {
    a = lim(a, 0.0f, 1.0f);
    auto ch = [a](juce::uint8 v) { return juce::uint8(lim(std::round(float(v) * (1.0f - a)), 0.0f, 255.0f)); };
    return Colour(ch(c.getRed()), ch(c.getGreen()), ch(c.getBlue()), c.getAlpha());
}
inline Colour mix(Colour a, Colour b, float t) { return a.interpolatedWith(b, lim(t, 0.0f, 1.0f)); }
inline Colour wa(Colour c, float a) { return c.withAlpha(lim(a, 0.0f, 1.0f)); }
inline Rf boxAt(float w, float h, Pt c) { return Rf(c.x - w * 0.5f, c.y - h * 0.5f, w, h); }
inline Rf withRight(Rf r, float right) { return Rf(r.getX(), r.getY(), std::max(0.0f, right - r.getX()), r.getHeight()); }
inline Rf withTop(Rf r, float top) { return Rf(r.getX(), top, r.getWidth(), std::max(0.0f, r.getBottom() - top)); }
inline Rf trimBottom(Rf r, float d) { return Rf(r.getX(), r.getY(), r.getWidth(), std::max(0.0f, r.getHeight() - d)); }
inline Rf trimTop(Rf r, float d) { return Rf(r.getX(), r.getY() + d, r.getWidth(), std::max(0.0f, r.getHeight() - d)); }
inline Rf cutBottom(Rf& r, float h) { h = lim(h, 0.0f, r.getHeight()); r.setHeight(r.getHeight() - h); return Rf(r.getX(), r.getBottom(), r.getWidth(), h); }
inline Rf cutLeft(Rf& r, float w) { w = lim(w, 0.0f, r.getWidth()); Rf out(r.getX(), r.getY(), w, r.getHeight()); r.setX(r.getX() + w); r.setWidth(r.getWidth() - w); return out; }

struct Stop { float p; Colour c; };
using Stops = std::vector<Stop>;

// Linear gradient across `r` along `angle` (0 = left to right, pi/2 = top to bottom).
inline juce::ColourGradient linearGrad(Rf r, const Stops& s, float angle) {
    const float dx = std::cos(angle), dy = std::sin(angle), half = (std::abs(dx) * r.getWidth() + std::abs(dy) * r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    juce::ColourGradient gr(s.front().c, c.x - dx * half, c.y - dy * half, s.back().c, c.x + dx * half, c.y + dy * half, false);
    for (size_t i = 1; i + 1 < s.size(); ++i) gr.addColour(double(lim(s[i].p, 0.0f, 1.0f)), s[i].c);
    return gr;
}
inline void fillRoundLinear(Graphics& g, Rf r, float radius, const Stops& s, float angle) {
    if (r.getWidth() <= 0 || r.getHeight() <= 0) return;
    g.setGradientFill(linearGrad(r, s, angle));
    if (radius > 0.3f) g.fillRoundedRectangle(r, radius); else g.fillRect(r);
}
inline void fillRound(Graphics& g, Rf r, float radius, Colour top, Colour bottom) { fillRoundLinear(g, r, radius, {{0, top}, {1, bottom}}, HALF_PI); }
inline void solidRound(Graphics& g, Rf r, float radius, Colour c) { if (r.getWidth() <= 0 || r.getHeight() <= 0) return; g.setColour(c); if (radius > 0.3f) g.fillRoundedRectangle(r, radius); else g.fillRect(r); }
inline void solidRect(Graphics& g, Rf r, Colour c) { solidRound(g, r, 0, c); }
inline void strokeRound(Graphics& g, Rf r, float radius, Colour c, float w) { g.setColour(c); g.drawRoundedRectangle(r, radius, w); }
inline void hline(Graphics& g, float y, float x0, float x1, Colour c, float w) { g.setColour(c); g.drawLine(x0, y, x1, y, w); }
inline void vline(Graphics& g, float x, float y0, float y1, Colour c, float w) { g.setColour(c); g.drawLine(x, y0, x, y1, w); }
inline void line(Graphics& g, Pt a, Pt b, Colour c, float w) { g.setColour(c); g.drawLine(a.x, a.y, b.x, b.y, w); }
inline Path polyPath(const std::vector<Pt>& p, bool closed) {
    Path path; if (p.empty()) return path;
    path.startNewSubPath(p[0]); for (size_t i = 1; i < p.size(); ++i) path.lineTo(p[i]);
    if (closed) path.closeSubPath();
    return path;
}
inline void strokePoly(Graphics& g, const std::vector<Pt>& p, Colour c, float w, bool closed) {
    g.setColour(c); g.strokePath(polyPath(p, closed), juce::PathStrokeType(w, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
}
inline void fillPoly(Graphics& g, const std::vector<Pt>& p, Colour top, Colour bottom) {
    if (p.size() < 3) return;
    float y0 = p[0].y, y1 = p[0].y; for (auto& q : p) { y0 = std::min(y0, q.y); y1 = std::max(y1, q.y); }
    g.setGradientFill(juce::ColourGradient(top, 0, y0, bottom, 0, y1 + 0.001f, false));
    g.fillPath(polyPath(p, true));
}
inline void fillEllipse(Graphics& g, Rf r, Colour top, Colour bottom) { g.setGradientFill(juce::ColourGradient(top, 0, r.getY(), bottom, 0, r.getBottom(), false)); g.fillEllipse(r); }
inline void solidEllipse(Graphics& g, Rf r, Colour c) { g.setColour(c); g.fillEllipse(r); }
inline Path roundPath(Rf r, float radius) { Path p; p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius); return p; }
inline void pushClipRound(Graphics& g, Rf r, float radius) { g.saveState(); g.reduceClipRegion(roundPath(r, radius), juce::AffineTransform()); }
inline void popClip(Graphics& g) { g.restoreState(); }

// Per-layer alpha so that N stacked layers reach a total of `a`.
inline float layerAlpha(float a, int n) { return 1.0f - std::pow(1.0f - lim(a, 0.0f, 0.999f), 1.0f / float(n)); }

// Soft drop shadow built from stacked fills that run from +1.6 sigma outside to -1.6 sigma inside.
inline void shadowRound(Graphics& g, Rf r, float radius, float sigma, Colour col, bool ellipse = false) {
    const int N = 7; const float la = layerAlpha(col.getFloatAlpha(), N);
    g.setColour(col.withAlpha(la));
    for (int k = 0; k < N; ++k) {
        const float e = sigma * (1.6f - 3.2f * (float(k) + 0.5f) / float(N));
        auto q = r.expanded(e); if (q.getWidth() <= 0.5f || q.getHeight() <= 0.5f) continue;
        if (ellipse) g.fillEllipse(q); else g.fillRoundedRectangle(q, std::max(0.0f, radius + e));
    }
}
inline void shadowEllipse(Graphics& g, Rf r, float sigma, Colour col) { shadowRound(g, r, 0, sigma, col, true); }

// Shade inside a round rect, thrown from its walls by `off`; the same stacked-layer softness.
inline void innerShadow(Graphics& g, Rf r, float radius, Pt off, float sigma, Colour col) {
    const int N = 7; const float la = layerAlpha(col.getFloatAlpha(), N);
    g.saveState(); g.reduceClipRegion(roundPath(r, radius), juce::AffineTransform());
    const auto big = r.expanded(sigma * 4.0f + 24.0f + std::abs(off.x) + std::abs(off.y));
    g.setColour(col.withAlpha(la));
    for (int k = 0; k < N; ++k) {
        const float d = sigma * (1.6f - 3.2f * (float(k) + 0.5f) / float(N));
        auto hole = r.translated(off.x, off.y).expanded(d);
        Path p; p.setUsingNonZeroWinding(false); p.addRectangle(big);
        if (hole.getWidth() > 0.5f && hole.getHeight() > 0.5f) p.addRoundedRectangle(hole.getX(), hole.getY(), hole.getWidth(), hole.getHeight(), std::max(0.0f, radius + d));
        g.fillPath(p);
    }
    g.restoreState();
}

// Additive-style glow ellipse: full colour in the middle, fading over `feather` toward the edge.
inline void glowEllipse(Graphics& g, Rf r, Colour col, float feather) {
    const float rad = std::min(r.getWidth(), r.getHeight()) * 0.5f; if (rad <= 0.1f || col.getAlpha() == 0) return;
    const float f = lim(feather / rad, 0.02f, 1.0f); const auto c = r.getCentre();
    juce::ColourGradient gr(col, c.x, c.y, col.withAlpha(0.0f), c.x + rad, c.y, true);
    gr.addColour(double(1.0f - f), col);
    g.setGradientFill(gr); g.fillEllipse(r);
}
// Glow around `r`: full strength on its outline, gone `spread` outside it.
inline void bloomRect(Graphics& g, Rf r, float radius, Colour c, float spread, float alpha) {
    if (alpha < 0.004f || r.getWidth() <= 0 || r.getHeight() <= 0) return;
    const int N = 5; g.setColour(c.withAlpha(layerAlpha(alpha, N)));
    for (int k = N; k >= 1; --k) { const float e = spread * float(k) / float(N); g.fillRoundedRectangle(r.expanded(e), radius + e); }
}
// The two bloom layers every lit block shares: a tight core and a wider faint halo.
inline void bloomBlock(Graphics& g, Rf r, float radius, Colour c, float k) {
    c = wa(c, 1.0f);
    const float thin = std::min(r.getWidth(), r.getHeight()), halo = lim(thin * 0.9f, 2.6f, 6.0f);
    k *= lim(thin / 3.0f, 0.3f, 1.0f);
    bloomRect(g, r, radius, c, halo, 0.17f * k);
    bloomRect(g, r, radius, c, 1.6f, 0.15f * k);
}
// Halo along a path: feathered discs stamped closer than their radius. A single near
// axis-aligned segment is one glow round rect instead.
inline void bloomPath(Graphics& g, const std::vector<Pt>& pts, bool closed, Colour c, float w, float spread, float alpha) {
    if (pts.size() < 2 || alpha < 0.004f) return;
    c = wa(c, 1.0f);
    if (pts.size() == 2) {
        const Pt a = pts[0], b = pts[1]; Rf bb(std::min(a.x, b.x), std::min(a.y, b.y), std::abs(a.x - b.x), std::abs(a.y - b.y));
        if (std::min(bb.getWidth(), bb.getHeight()) <= 4.0f) { bloomRect(g, bb.expanded(w * 0.5f), w * 0.5f, c, spread, alpha); return; }
    }
    float total = 0; const size_t n = closed ? pts.size() : pts.size() - 1;
    for (size_t i = 0; i < n; ++i) { auto d = pts[(i + 1) % pts.size()] - pts[i]; total += std::sqrt(d.x * d.x + d.y * d.y); }
    if (total <= 0) return;
    const float rad = w * 0.5f + spread, step = std::min(rad, std::max(spread * 0.5f, total / 120.0f));
    const auto col = wa(c, std::min(alpha, alpha * step / (2.0f * rad - spread)));
    float carry = 0;
    for (size_t i = 0; i < n; ++i) {
        const Pt a = pts[i], b = pts[(i + 1) % pts.size()], d = b - a; const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len <= 0) continue;
        float at = carry;
        while (at <= len) { glowEllipse(g, boxAt(rad * 2, rad * 2, a + d * (at / len)), col, spread); at += step; }
        carry = at - len;
    }
}
inline std::vector<Pt> roundPts(Rf r, float radius) {
    auto rectPts = [&] { return std::vector<Pt>{{r.getX(), r.getY()}, {r.getRight(), r.getY()}, {r.getRight(), r.getBottom()}, {r.getX(), r.getBottom()}}; };
    const float k = std::max(0.0f, std::min({radius, r.getWidth() * 0.5f, r.getHeight() * 0.5f}));
    if (k < 0.5f) return rectPts();
    std::vector<Pt> out;
    const float cs[4][3] = {{r.getRight() - k, r.getY() + k, -HALF_PI}, {r.getRight() - k, r.getBottom() - k, 0}, {r.getX() + k, r.getBottom() - k, HALF_PI}, {r.getX() + k, r.getY() + k, PI}};
    for (auto& c : cs) for (int j = 0; j <= 4; ++j) { const float a = c[2] + HALF_PI * float(j) / 4.0f; out.push_back({c[0] + std::cos(a) * k, c[1] + std::sin(a) * k}); }
    return out;
}

constexpr float TUBE_OFF = 0.2f;   // below this much light a tube is drawn switched off
inline float coreAlpha(float k) { return 0.9f * std::sqrt(lim(k, 0.0f, 1.0f)); }
inline void tubeOff(Colour c, Colour& body, Colour& hi) {
    c = wa(c, 1.0f); body = mix(drk(c, 0.66f), Colour(0xff3a4046), 0.30f); hi = wa(mix(c, Colours::white, 0.45f), 0.22f);
}
// Neon tube look for a polyline: halo, a soft sleeve, the tube and a hot centre. Under 0.2 of full
// light the tube is drawn switched off: dark tinted glass with a faint highlight, no core, no bloom.
inline void glowStrokePts(Graphics& g, const std::vector<Pt>& pts, Colour c, float w, float strength, bool glow, bool closed) {
    if (pts.size() < 2) return;
    const float k = strength * c.getFloatAlpha();
    if (k < TUBE_OFF) {
        Colour body, hi; tubeOff(c, body, hi);
        strokePoly(g, pts, Colour(0x80000000), w + 1, closed); strokePoly(g, pts, body, w, closed); strokePoly(g, pts, hi, w * 0.3f, closed); return;
    }
    if (!glow) { strokePoly(g, pts, c, w, closed); return; }
    bloomPath(g, pts, closed, c, w, 3.5f + w * 1.6f, 0.20f * k);
    strokePoly(g, pts, wa(c, 0.30f * k), w + 2.2f, closed);
    strokePoly(g, pts, c, w, closed);
    strokePoly(g, pts, wa(brt(c, 0.7f), coreAlpha(k)), w * 0.35f, closed);
}
inline void glowRoundRect(Graphics& g, Rf r, float radius, Colour c, float w, float strength, bool glow) {
    const float k = strength * c.getFloatAlpha();
    if (k < TUBE_OFF) {
        Colour body, hi; tubeOff(c, body, hi);
        strokeRound(g, r, radius, Colour(0x80000000), w + 1); strokeRound(g, r, radius, body, w); strokeRound(g, r, radius, hi, w * 0.3f); return;
    }
    if (!glow) { strokeRound(g, r, radius, c, w); return; }
    bloomPath(g, roundPts(r, radius), true, c, w, 3.5f + w * 1.6f, 0.20f * k);
    strokeRound(g, r, radius, wa(c, 0.30f * k), w + 2.2f); strokeRound(g, r, radius, c, w); strokeRound(g, r, radius, wa(brt(c, 0.7f), coreAlpha(k)), w * 0.35f);
}
// JUCE arc convention: angle 0 at 12 o'clock, clockwise.
inline void juceArc(Graphics& g, Pt c, float rad, float from, float to, Colour col, float w) {
    Path p; p.addCentredArc(c.x, c.y, rad, rad, 0, from, to, true);
    g.setColour(col); g.strokePath(p, juce::PathStrokeType(w, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
}
inline std::vector<Pt> juceArcPts(Pt c, float rad, float from, float to) {
    const int n = juce::jlimit(2, 96, int(std::ceil(std::abs(to - from) * rad / 4.0f)));
    std::vector<Pt> out;
    for (int i = 0; i <= n; ++i) { const float a = from + (to - from) * float(i) / float(n) - HALF_PI; out.push_back({c.x + std::cos(a) * rad, c.y + std::sin(a) * rad}); }
    return out;
}
inline void glowJuceArc(Graphics& g, Pt c, float rad, float from, float to, Colour col, float w, float strength, bool glow) {
    if (!glow) { juceArc(g, c, rad, from, to, col, w); return; }
    const float k = strength * col.getFloatAlpha();
    bloomPath(g, juceArcPts(c, rad, from, to), false, col, w, 3.0f + w * 0.8f, 0.20f * k);
    juceArc(g, c, rad, from, to, wa(col, 0.28f * k), w + 2.4f);
    juceArc(g, c, rad, from, to, col, w);
    juceArc(g, c, rad, from, to, wa(brt(col, 0.7f), coreAlpha(k)), w * 0.35f);
}

inline void textC(Graphics& g, Rf r, const juce::String& text, Colour c, float size) {
    g.setColour(c); g.setFont(juce::FontOptions(size, juce::Font::bold)); g.drawText(text, r, juce::Justification::centred, false);
}
// Soft halo behind a centred text run, then the run stamped one pixel out in four directions.
inline void textBloom(Graphics& g, Rf r, const juce::String& text, Colour c, float size, bool glow) {
    if (!glow || text.trim().isEmpty()) return;
    const float a = c.getFloatAlpha(); const Colour hue = wa(c, 1.0f);
    const auto box = boxAt(size * 0.52f * float(text.length()), size * 0.5f, r.getCentre());
    bloomRect(g, box, size * 0.25f, hue, size * 0.75f, 0.13f * a);
    bloomRect(g, box.reduced(0, size * 0.1f), size * 0.2f, hue, size * 0.3f, 0.10f * a);
    const float d = 1.0f;
    for (auto o : {Pt(-d, 0), Pt(d, 0), Pt(0, -d), Pt(0, d)}) textC(g, r.translated(o.x, o.y), text, wa(hue, 0.17f * a), size);
}
inline void litText(Graphics& g, Rf r, const juce::String& text, Colour c, float size, bool glow) {
    textBloom(g, r, text, c, size, glow); textC(g, r, text, glow ? brt(c, 0.22f) : c, size);
}

// The one studio light: key 20 deg left of straight up, 65 deg above the panel. `turn` is the
// rotation a vertical control is drawn with, so shadows and bevels stay put on screen.
struct Lt {
    float turn = 0;
    static constexpr float kx = -0.14455f, ky = -0.39713f, kz = 0.90631f;
    Pt v(Pt p) const { const float s = std::sin(-turn), c = std::cos(-turn); return {c * p.x - s * p.y, s * p.x + c * p.y}; }
    Pt offset(float h) const { return v({-kx * h / kz, -ky * h / kz}); }
    float sigma(float h) const { return std::max(0.5f, h * 0.27f); }
    float away() const { const Pt d = v({kx / 0.42262f, ky / 0.42262f}); return std::atan2(-d.y, -d.x); }
    Pt toward() const { return v({kx / 0.42262f, ky / 0.42262f}); }
};

// A cut-out in the panel with a dark floor: the lip nearest the light is in shade, the far lip catches it.
inline void recess(Graphics& g, Rf r, float radius, Colour top, Colour bottom, Lt lt) {
    fillRoundLinear(g, r.expanded(1.1f), radius + 1.1f, {{0, Colour(0xc8000000)}, {0.5f, Colour(0x50000000)}, {1, Colour(0x30ffffff)}}, lt.away());
    fillRound(g, r, radius, top, bottom);
}
inline void recessShade(Graphics& g, Rf r, float radius, float depth, Lt lt) { innerShadow(g, r, radius, lt.offset(depth), std::max(1.0f, depth * 0.9f), Colour(0xd8000000)); }
inline void windowGround(Graphics& g, Rf r, float radius, Colour top, Colour bottom) { recess(g, r, radius, top, bottom, Lt{}); }
// What sits in front of a display: panel shadow, a faint reflection of the room and a dark edge.
inline void windowGlass(Graphics& g, Rf r, float radius) {
    recessShade(g, r, radius, 3.2f, Lt{});
    Rf s(r.getX() + 1, r.getY() + 1, std::max(0.0f, r.getWidth() - 2), std::max(0.0f, r.getHeight() * 0.6f));
    fillRoundLinear(g, s, std::max(0.0f, radius - 1), {{0, Colour(0x1effffff)}, {0.4f, Colour(0x09ffffff)}, {1, Colour(0x00ffffff)}}, HALF_PI);
    strokeRound(g, r, radius, Colour(0xe6000000), 1.0f);
}

struct Seg { Rf r; Colour c; bool on; };
// Display segments: unlit stay as 10% ghosts, lit get bloom underneath and a slight vertical gradient.
inline void segments(Graphics& g, const std::vector<Seg>& segs, float radius, float t, bool glow) {
    for (auto& s : segs) if (!s.on) solidRound(g, s.r, radius, wa(s.c, 0.10f));
    if (glow) for (size_t i = 0; i < segs.size(); ++i) if (segs[i].on) bloomBlock(g, segs[i].r, radius, segs[i].c, glowEnabled ? 0.94f + 0.06f * std::sin((t + float(i) * 0.37f) * 31.0f) * std::sin((t + float(i) * 0.37f) * 7.3f) : 1.0f);
    for (size_t i = 0; i < segs.size(); ++i) if (segs[i].on) {
        const auto& s = segs[i]; const float tt = t + float(i) * 1.1f;
        const float a = glow ? 0.94f + 0.06f * std::sin(tt * 31.0f) * std::sin(tt * 7.3f) : 1.0f;
        fillRoundLinear(g, s.r, radius, {{0, wa(brt(s.c, 0.5f), a)}, {0.22f, wa(brt(s.c, 0.14f), a)}, {1, wa(drk(s.c, 0.16f), a)}}, HALF_PI);
    }
}
inline void gapDark(Graphics& g, const std::vector<Seg>& segs, Colour dark, float alpha) {
    for (size_t i = 0; i + 1 < segs.size(); ++i) {
        const auto &a = segs[i], &c = segs[i + 1]; if (!(a.on && c.on)) continue;
        Rf gap(a.r.getRight() + 0.25f, c.r.getY(), std::max(0.0f, c.r.getX() - a.r.getRight() - 0.5f), c.r.getHeight());
        if (gap.getWidth() > 0.3f) solidRect(g, gap, wa(dark, alpha));
    }
}
} // namespace lit

// ---- glow primitives ------------------------------------------------------
namespace detail {
inline std::vector<Point<float>> flatten(const Path& p, bool& closed) {
    std::vector<Point<float>> pts; closed = false;
    for (juce::PathFlatteningIterator it(p, juce::AffineTransform(), 0.4f); it.next();) {
        if (pts.empty()) pts.push_back({it.x1, it.y1});
        pts.push_back({it.x2, it.y2});
        if (it.isLastInSubpath()) { closed = it.closesSubPath; break; }
    }
    if (closed && pts.size() > 1 && pts.front().getDistanceFrom(pts.back()) < 0.01f) pts.pop_back();
    return pts;
}
}
inline void glowStroke(Graphics& g, const Path& p, Colour c, float w, float strength = 1.0f) {
    bool closed; auto pts = detail::flatten(p, closed);
    lit::glowStrokePts(g, pts, c, w, strength, glowEnabled, closed);
}
inline void glowLine(Graphics& g, Point<float> a, Point<float> b, Colour c, float w, float strength = 1.0f) { lit::glowStrokePts(g, {a, b}, c, w, strength, glowEnabled, false); }
inline void glowDot(Graphics& g, Point<float> c, float r, Colour col) {
    if (!glowEnabled) { g.setColour(col); g.fillEllipse(c.x - r, c.y - r, r * 2, r * 2); return; }
    lit::glowEllipse(g, lit::boxAt(r * 9, r * 9, c), lit::wa(col, 0.30f), r * 4.5f);
    lit::glowEllipse(g, lit::boxAt(r * 4.4f, r * 4.4f, c), lit::wa(col, 0.55f), r * 1.6f);
    lit::solidEllipse(g, lit::boxAt(r * 1.5f, r * 1.5f, c), lit::brt(col, 0.8f));
}
// Round-rect outline in the same neon tube look.
inline void glowRound(Graphics& g, Rectangle<float> r, float radius, Colour c, float w, float strength = 1.0f) { lit::glowRoundRect(g, r, radius, c, w, strength, glowEnabled); }

inline Path chamfer(Rectangle<float> r, float ch, bool topRightOnly = false) {
    Path p;
    if (topRightOnly) { p.startNewSubPath(r.getX(), r.getY()); p.lineTo(r.getRight() - ch, r.getY()); p.lineTo(r.getRight(), r.getY() + ch);
        p.lineTo(r.getRight(), r.getBottom()); p.lineTo(r.getX() + ch, r.getBottom()); p.lineTo(r.getX(), r.getBottom() - ch); }
    else { p.startNewSubPath(r.getX() + ch, r.getY()); p.lineTo(r.getRight() - ch, r.getY()); p.lineTo(r.getRight(), r.getY() + ch);
        p.lineTo(r.getRight(), r.getBottom() - ch); p.lineTo(r.getRight() - ch, r.getBottom()); p.lineTo(r.getX() + ch, r.getBottom());
        p.lineTo(r.getX(), r.getBottom() - ch); p.lineTo(r.getX(), r.getY() + ch); }
    p.closeSubPath(); return p;
}
namespace detail {
inline std::vector<Point<float>> chamferPts(Rectangle<float> r, float ch, bool trOnly) {
    bool c; return flatten(chamfer(r, ch, trOnly), c);
}
}
inline void scanlines(Graphics& g, Rectangle<float> r, float t, float alpha = 0.18f) {
    if (!glowEnabled) return;
    g.setColour(Colour::fromFloatRGBA(0, 0, 0, alpha));
    for (float y = r.getY(); y < r.getBottom(); y += 2.5f) g.drawHorizontalLine(int(y), r.getX(), r.getRight());
    // The refresh sweep: a thin bright row where the beam is, with a short fading afterglow above it.
    const float sweep = r.getY() + std::fmod(std::fmod(t * 0.35f, 1.0f) + 1.0f, 1.0f) * r.getHeight();
    const float tail = lit::lim(r.getHeight() * 0.22f, 6.0f, 22.0f), y0 = std::max(sweep - tail, r.getY());
    if (sweep - y0 > 1.0f) {
        const float f0 = (y0 - (sweep - tail)) / tail;
        auto w = [](float a) { return Colour(0xffffffff).withAlpha(lit::lim(a, 0.0f, 1.0f)); };
        lit::fillRoundLinear(g, {r.getX(), y0, r.getWidth(), sweep - y0}, 0, {{0, w(0.03f * f0 * f0)}, {0.75f, w(0.006f + 0.01f * f0)}, {1, w(0.03f)}}, lit::HALF_PI);
    }
    if (sweep > r.getY() + 0.5f && sweep < r.getBottom() - 0.5f) { g.setColour(Colour(0xffffffff).withAlpha(0.085f)); g.fillRect(r.getX(), sweep - 0.5f, r.getWidth(), 0.7f); }
}
inline float flicker(float t) { if (!glowEnabled) return 1.0f; return 0.94f + 0.06f * std::sin(t * 31.0f) * std::sin(t * 7.3f); }
inline Colour heat(float f, Colour lo, Colour hi) { return lo.interpolatedWith(hi, juce::jlimit(0.0f, 1.0f, f)); }
inline Colour neon(int i) { static const Colour c[] = {Colour(0xff39f2ff), Colour(0xffff4fd8), Colour(0xff9dff3a), Colour(0xffffb02e), Colour(0xffff3b4a), Colour(0xff7a5cff)}; return c[((i % 6) + 6) % 6]; }

// ---- faces: all take level01 (0..1) ---------------------------------------
// 1. Bar-graph wedge (speedo / tach ramp): bars climb toward the red end.
inline void drawBarWedge(Graphics& g, Rectangle<float> r, float level, Colour litC, float t) {
    using namespace lit; const bool glow = glowEnabled;
    windowGround(g, r, 5, Colour(0xff0d0b06), Colour(0xff040302));
    auto b = r.reduced(8, 6); b.setHeight(std::max(0.0f, b.getHeight() - 10));
    const int n = 26; std::vector<Seg> segs;
    for (int i = 0; i < n; ++i) {
        const float f = float(i) / float(n - 1), x = b.getX() + f * (b.getWidth() - 4);
        const float h = b.getHeight() * (0.14f + 0.86f * std::pow(f, 1.35f)), w = b.getWidth() / float(n) * 0.60f;
        const bool on = f <= level + 0.5f / float(n);
        const Colour c = f > 0.84f ? Colour(0xffff3b2a) : f > 0.68f ? mix(litC, Colour(0xffff6a1a), 0.55f) : litC;
        segs.push_back({Rf(x, b.getBottom() - h, w, h), c, on});
    }
    segments(g, segs, 1.0f, t, glow);
    gapDark(g, segs, Colour(0xff040302), 0.62f);
    auto q = r.reduced(8, 2); auto labels = cutBottom(q, 10);
    for (int i = 0; i <= 5; ++i) litText(g, Rf(b.getX() + b.getWidth() * float(i) / 5.0f - 10, labels.getY(), 20, labels.getHeight()), juce::String(i * 12), wa(litC, 0.7f), 7.5f, glow);
    scanlines(g, r, t, 0.14f);
    windowGlass(g, r, 5);
    strokeRound(g, r.reduced(0.5f), 4.5f, wa(litC, 0.20f), 1.0f);
}

// 2. Vector mountains: mirrored green fans (two channels) with a centre scale.
inline void drawMountains(Graphics& g, Rectangle<float> r, float left, float right, Colour litC, float t) {
    using namespace lit; const bool glow = glowEnabled;
    windowGround(g, r, 5, Colour(0xff030b07), Colour(0xff010402));
    const auto b = r.reduced(8, 7); const float cx = b.getCentreX(), half = b.getWidth() * 0.5f - 12; const int n = 22;
    struct L { Pt base, top; bool on; float f; }; std::vector<L> lines;
    for (int side = 0; side < 2; ++side) {
        const float level = side == 0 ? left : right, dirn = side == 0 ? -1.0f : 1.0f;
        for (int i = 0; i < n; ++i) {
            const float f = float(i) / float(n - 1), x = cx + dirn * (10 + f * half), h = b.getHeight() * (0.92f - 0.80f * f);
            lines.push_back({{x, b.getBottom()}, {x - dirn * (b.getWidth() * 0.02f), b.getBottom() - h}, (1.0f - f) <= level + 0.5f / float(n), f});
        }
    }
    for (auto& l : lines) if (!l.on) line(g, l.base, l.top, wa(litC, 0.11f), 1.2f);
    if (glow) for (auto& l : lines) if (l.on) { const float k = flicker(t + l.f); bloomPath(g, {l.base, l.top}, false, litC, 1.2f, 3.2f, 0.09f * k); bloomPath(g, {l.base, l.top}, false, litC, 1.2f, 1.2f, 0.08f * k); }
    for (auto& l : lines) if (l.on) { line(g, l.base, l.top, brt(litC, 0.12f), 1.2f); line(g, l.top, l.top + (l.base - l.top) * 0.3f, wa(brt(litC, 0.55f), 0.8f), 0.7f); }
    for (int i = 0; i < 6; ++i) {
        const float y = b.getBottom() - b.getHeight() * float(i) / 5.0f;
        litText(g, boxAt(20, 9, {cx, y - 3}), juce::String(i * 10), wa(litC, 0.8f), 7.5f, glow);
        hline(g, std::floor(y), cx - 8, cx - 5, wa(litC, 0.8f), 1.0f); hline(g, std::floor(y), cx + 5, cx + 8, wa(litC, 0.8f), 1.0f);
    }
    hline(g, std::floor(b.getBottom() + 1), b.getX(), b.getRight(), wa(litC, 0.6f), 1.0f);
    scanlines(g, r, t, 0.2f);
    windowGlass(g, r, 5);
    strokeRound(g, r.reduced(0.5f), 4.5f, wa(litC, 0.18f), 1.0f);
}

// 3. Twin seven-segment LCD readouts.
inline void drawLcdPanel(Graphics& g, Rectangle<float> r, const juce::String& a, const juce::String& aLabel, const juce::String& b, const juce::String& bLabel, Colour litC, float t) {
    using namespace lit; const bool glow = glowEnabled;
    windowGround(g, r, 6, Colour(0xff140c05), Colour(0xff060301));
    const auto inn = r.reduced(7); const float half = inn.getWidth() * 0.5f - 3;
    for (int i = 0; i < 2; ++i) {
        Rf cell(inn.getX() + float(i) * (half + 6), inn.getY(), half, inn.getHeight());
        auto lab = cutBottom(cell, 12); const float k = flicker(t + float(i));
        fillRound(g, cell, 4, Colour(0xff0a0603), Colour(0xff040201));
        if (glow) bloomRect(g, cell.reduced(cell.getWidth() * 0.3f, cell.getHeight() * 0.34f), 3, wa(litC, 1.0f), cell.getHeight() * 0.42f, 0.07f * k);
        recessShade(g, cell, 4, 1.6f, Lt{});
        strokeRound(g, cell, 4, wa(litC, 0.26f), 1.0f);
        meters::drawSevenSegment(g, cell.reduced(4, 3), i == 0 ? a : b, litC.withAlpha(k), false);
        litText(g, lab, i == 0 ? aLabel : bLabel, wa(litC, 0.85f), 8.5f, glow);
    }
    scanlines(g, r, t, 0.12f);
    windowGlass(g, r, 6);
    strokeRound(g, r.reduced(0.5f), 5.5f, wa(litC, 0.32f), 1.0f);
}

// 4. Half-ring arc gauge of segmented wedges.
inline void drawArcGauge(Graphics& g, Rectangle<float> r, float level, Colour litC, float t) {
    using namespace lit; const bool glow = glowEnabled;
    windowGround(g, r, 6, Colour(0xff100d09), Colour(0xff040303));
    const float rad = std::min((r.getWidth() * 0.5f - 6) / 1.1f, (r.getHeight() - 13) / 1.1f);
    const Pt c(r.getCentreX(), r.getBottom() - 6); const Colour red(0xffff3b2a);
    const float w = rad * 0.20f; const int n = 30; const float gap = 0.14f / float(n), litTo = lim(level, 0.0f, 1.0f);
    auto ang = [](float f) { return (-0.5f + f) * PI; };
    auto wedge = [&](float f0, float f1, float r0, float r1) {
        auto at = [&](float f, float rr) { const float a = ang(f); return Pt(c.x + std::sin(a) * rr, c.y - std::cos(a) * rr); };
        return std::vector<Pt>{at(f0, r1), at((f0 + f1) * 0.5f, r1), at(f1, r1), at(f1, r0), at((f0 + f1) * 0.5f, r0), at(f0, r0)};
    };
    const float r0 = rad - w * 0.5f, r1 = rad + w * 0.5f;
    auto cellC = [&](int i) { return float(i) / float(n) >= 0.86f ? red : litC; };
    auto on = [&](int i) { return (float(i) + 0.5f) / float(n) <= litTo; };
    for (int i = 0; i < n; ++i) if (!on(i)) { auto col = wa(cellC(i), 0.10f); fillPoly(g, wedge(float(i) / float(n) + gap, float(i + 1) / float(n) - gap, r0, r1), col, col); }
    if (glow && litTo > 0) {
        const float k = flicker(t);
        for (auto [f0, f1, col] : {std::tuple<float, float, Colour>{0.0f, std::min(litTo, 0.86f), litC}, {0.86f, litTo, red}})
            if (f1 > f0) { bloomPath(g, juceArcPts(c, rad, ang(f0), ang(f1)), false, col, w, 6.0f, 0.22f * k); bloomPath(g, juceArcPts(c, rad, ang(f0), ang(f1)), false, col, w, 1.8f, 0.26f * k); }
    }
    for (int i = 0; i < n; ++i) if (on(i)) {
        const float f0 = float(i) / float(n) + gap, f1 = float(i + 1) / float(n) - gap; const Colour col = cellC(i);
        fillPoly(g, wedge(f0, f1, r0, r1), col, drk(col, 0.14f));
        auto hi = wa(brt(col, 0.45f), 0.8f); fillPoly(g, wedge(f0, f1, r1 - w * 0.36f, r1 - w * 0.08f), hi, hi);
    }
    juceArc(g, c, rad * 0.78f, ang(0.86f), ang(1.0f), wa(red, 0.85f), rad * 0.06f);
    for (int i = 0; i <= 9; ++i) {
        const float f = float(i) / 9.0f, a = (-0.5f + f) * PI;
        const Pt t0(c.x + std::sin(a) * rad * 0.82f, c.y - std::cos(a) * rad * 0.82f), t1(c.x + std::sin(a) * rad * 0.74f, c.y - std::cos(a) * rad * 0.74f);
        const Colour col = f > 0.86f ? Colour(0xffff6a4a) : wa(litC, 0.9f);
        line(g, t0, t1, col, 1.3f);
        litText(g, boxAt(14, 10, {c.x + std::sin(a) * rad * 0.64f, c.y - std::cos(a) * rad * 0.64f}), juce::String(i), col, lim(rad * 0.16f, 7.0f, 10.0f), glow);
    }
    meters::drawSevenSegment(g, boxAt(rad * 0.55f, rad * 0.30f, {c.x, c.y - rad * 0.22f}), juce::String(int(level * 99.0f)), litC, false);
    scanlines(g, r, t, 0.12f);
    windowGlass(g, r, 6);
    strokeRound(g, r.reduced(0.5f), 5.5f, wa(litC, 0.22f), 1.0f);
}

// 5. Two rows of green / amber / red lenses with L / R tags.
inline void drawColourColumns(Graphics& g, Rectangle<float> r, float l, float rr, float peakL, float peakR, float t) {
    using namespace lit; const bool glow = glowEnabled;
    windowGround(g, r, 5, Colour(0xff070a08), Colour(0xff020403));
    auto b = r.reduced(10, 7); cutLeft(b, 12);
    const int n = 22; const float seg = b.getWidth() / float(n), rowH = (b.getHeight() - 6) * 0.5f; std::vector<Seg> segs;
    for (int c = 0; c < 2; ++c) {
        const float lv = c == 0 ? l : rr, pk = c == 0 ? peakL : peakR;
        Rf row(b.getX(), b.getY() + float(c) * (rowH + 6), b.getWidth(), rowH);
        textC(g, boxAt(10, rowH, {r.getX() + 10, row.getCentreY()}), c == 0 ? "L" : "R", Colour(0xff8b9498), 8.0f);
        for (int i = 0; i < n; ++i) {
            const float f = float(i) / float(n - 1);
            const Colour cc = f > 0.85f ? Colour(0xffff3b2a) : f > 0.62f ? Colour(0xffffcf2a) : Colour(0xff35e04a);
            const bool on = f <= lv + 0.5f / float(n) || (pk > 0 && std::abs(f - pk) < 0.5f / float(n));
            segs.push_back({Rf(row.getX() + float(i) * seg + 1, row.getY(), std::max(0.6f, seg - 2), row.getHeight()), cc, on});
        }
    }
    segments(g, segs, 0.4f, t, glow);
    scanlines(g, r, t, 0.10f);
    windowGlass(g, r, 5);
    strokeRound(g, r.reduced(0.5f), 4.5f, Colour(0x22ffffff), 1.0f);
}

// A chamfered window's glass: contact shade all round, extra from walls facing the light, a sheen.
inline void chamferGlass(Graphics& g, Rectangle<float> r, float ch, bool trOnly) {
    using namespace lit; const Pt toward = Lt{}.toward(); auto black = [](float a) { return Colour(0xff000000).withAlpha(lim(a, 0.0f, 1.0f)); };
    const int n = 9; const float step = 0.75f;
    for (int j = 0; j < n; ++j) {
        const float d = 0.35f + float(j) * step, fall = std::pow(1.0f - float(j) / float(n), 1.8f);
        auto path = detail::chamferPts(r.reduced(d), std::max(0.0f, ch - d * 0.586f), trOnly);
        if (j < 4) strokePoly(g, path, black(0.34f * fall), step + 0.25f, true);
        for (size_t i = 0; i < path.size(); ++i) {
            const Pt p = path[i], q = path[(i + 1) % path.size()], e = q - p; const float len = std::sqrt(e.x * e.x + e.y * e.y);
            if (len < 0.5f) continue;
            const float facing = (e.y * toward.x - e.x * toward.y) / len;
            if (facing > 0.05f) line(g, p, q, black(0.52f * facing * fall), step + 0.35f);
        }
    }
    const float nearV = trOnly ? 0.0f : ch, yb = r.getY() + r.getHeight() * 0.55f;
    fillPoly(g, {{r.getX() + nearV, r.getY()}, {r.getRight() - ch, r.getY()}, {r.getRight(), r.getY() + ch}, {r.getRight(), yb}, {r.getX(), yb}, {r.getX(), r.getY() + nearV}}, Colour(0x16ffffff), Colour(0x00ffffff));
    auto path = detail::chamferPts(r.reduced(0.6f), std::max(0.0f, ch - 0.35f), trOnly);
    for (size_t i = 0; i < path.size(); ++i) {
        const Pt p = path[i], q = path[(i + 1) % path.size()], e = q - p; const float len = std::sqrt(e.x * e.x + e.y * e.y);
        if (len < 0.5f) continue;
        const float facing = (e.y * toward.x - e.x * toward.y) / len;
        if (facing < -0.05f) line(g, p, q, Colour(0xffffffff).withAlpha(lim(0.10f * -facing, 0.0f, 1.0f)), 0.7f);
    }
}

// 6. Cyberpunk neon bars in a chamfered frame.
inline void drawNeonBars(Graphics& g, Rectangle<float> r, float level, float peak, Colour lo, Colour hi, float t) {
    using namespace lit; const bool glow = glowEnabled;
    auto frame = detail::chamferPts(r.reduced(1.5f), 10, false);
    fillPoly(g, frame, Colour(0xff051217), Colour(0xff010507));
    pushClipRound(g, r.reduced(1.5f), 17);
    const auto grid = wa(lo, 0.06f);
    for (float x = r.getX(); x < r.getRight(); x += 12) vline(g, std::floor(x), r.getY(), r.getBottom(), grid, 1.0f);
    for (float y = r.getY(); y < r.getBottom(); y += 12) hline(g, std::floor(y), r.getX(), r.getRight(), grid, 1.0f);
    const auto b = r.reduced(14, 12); const int n = 24; std::vector<Seg> segs;
    for (int i = 0; i < n; ++i) {
        const float f = float(i) / float(n - 1), x = b.getX() + f * b.getWidth(), w = b.getWidth() / float(n) * 0.55f;
        const float h = b.getHeight() * (0.45f + 0.55f * std::sin(f * PI * 0.5f + 0.4f)), fill = lim((level - f * 0.6f) / 0.4f, 0.0f, 1.0f);
        const Colour c = heat(f, lo, hi); Rf bar(x, b.getBottom() - h, w, h);
        segs.push_back({bar, c, false});
        if (fill > 0) segs.push_back({withTop(bar, bar.getBottom() - h * fill), c, true});
    }
    segments(g, segs, 0, t, glow);
    for (auto& s : segs) if (s.on) solidRect(g, Rf(s.r.getX(), s.r.getY(), s.r.getWidth(), 1.6f), brt(s.c, 0.9f));
    const float px = b.getX() + lim(peak, 0.0f, 1.0f) * b.getWidth();
    lit::glowStrokePts(g, {{px, b.getY()}, {px, b.getBottom()}}, hi, 1.2f, 0.8f, glow, false);
    scanlines(g, r, t, 0.16f);
    chamferGlass(g, r.reduced(1.5f), 10, false);
    popClip(g);
    lit::glowStrokePts(g, frame, lo, 1.4f, 1.0f, glow, true);
    lit::glowStrokePts(g, {{r.getX() + 14, r.getY() + 1}, {r.getX() + 44, r.getY() + 1}}, hi, 2.2f, 1.0f, glow, false);
}

// 7. Hex ring gauge.
inline void drawHexRing(Graphics& g, Rectangle<float> r, float level, Colour lo, Colour hi, float t) {
    using namespace lit; const bool glow = glowEnabled;
    const Pt c = r.getCentre(); const float radius = (std::min(r.getWidth(), r.getHeight()) * 0.5f - 5) / 1.12f;
    std::vector<Pt> hx;
    for (int i = 0; i < 6; ++i) { const float a = float(i) * PI / 3.0f + PI / 6.0f; hx.push_back({c.x + std::cos(a) * radius * 1.12f, c.y + std::sin(a) * radius * 1.12f}); }
    windowGround(g, r, 6, Colour(0xff040a10), Colour(0xff010305));
    pushClipRound(g, r, 6);
    fillPoly(g, hx, drk(lo, 0.9f), Colour(0xff020406));
    glowStrokePts(g, hx, wa(lo, 0.7f), 1.2f, 0.8f, glow, true);
    const float a0 = -PI * 1.25f, a1 = PI * 0.25f, ring = radius * 0.80f; const Colour litC = heat(level, lo, hi);
    juceArc(g, c, ring, a0 + HALF_PI, a1 + HALF_PI, wa(lo, 0.12f), radius * 0.10f);
    if (level > 0) glowJuceArc(g, c, ring, a0 + HALF_PI, a0 + (a1 - a0) * lim(level, 0.0f, 1.0f) + HALF_PI, litC, radius * 0.10f, 1.0f, glow);
    for (int i = 0; i < 24; ++i) {
        const float a = a0 + (a1 - a0) * float(i) / 23.0f + HALF_PI; const bool on = float(i) / 23.0f <= level;
        line(g, {c.x + std::cos(a) * ring * 0.80f, c.y + std::sin(a) * ring * 0.80f}, {c.x + std::cos(a) * ring * 0.70f, c.y + std::sin(a) * ring * 0.70f}, on ? wa(Colours::white, 0.7f) : wa(lo, 0.3f), 1.0f);
    }
    meters::drawSevenSegment(g, boxAt(radius * 0.9f, radius * 0.46f, {c.x, c.y + radius * 0.05f}), juce::String(int(level * 100.0f)), litC.withAlpha(flicker(t)), false);
    scanlines(g, r, t, 0.12f);
    popClip(g);
    windowGlass(g, r, 6);
}

// 8. Oscilloscope with a CLIP warning band.
inline void drawScope(Graphics& g, Rectangle<float> r, const std::vector<float>& hist, bool clip, Colour litC, float t) {
    using namespace lit; const bool glow = glowEnabled;
    auto frame = detail::chamferPts(r.reduced(1.5f), 8, true);
    fillPoly(g, frame, Colour(0xff031013), Colour(0xff010608));
    const auto fr = r.reduced(1.5f); const float k = 13.6f;
    pushClipRound(g, fr, 0); pushClipRound(g, {fr.getX() - k, fr.getY(), fr.getWidth() + k, fr.getHeight() + k}, k); pushClipRound(g, {fr.getX(), fr.getY() - k, fr.getWidth() + k, fr.getHeight() + k}, k);
    const auto grid = wa(litC, 0.08f);
    for (float x = r.getX(); x < r.getRight(); x += 14) vline(g, std::floor(x), r.getY(), r.getBottom(), grid, 1.0f);
    float y = std::floor(r.getCentreY()); while (y > r.getY()) y -= 14;
    for (; y < r.getBottom(); y += 14) hline(g, y, r.getX(), r.getRight(), std::abs(y - std::floor(r.getCentreY())) < 1.0f ? wa(litC, 0.16f) : grid, 1.0f);
    if (hist.size() >= 2) {
        const float cy = r.getCentreY(), amp = r.getHeight() * 0.44f; const size_t N = hist.size();
        std::vector<Pt> top, bot, fill;
        for (size_t i = 0; i < N; ++i) {
            const float x = r.getX() + r.getWidth() * float(i) / float(N - 1), v = hist[i] * amp;
            const float jitter = hist[i] > 0.02f ? 0.5f * std::sin(float(i) * 1.7f + t * 30.0f) : 0.0f;
            top.push_back({x, cy - v - (i == 0 ? 0.0f : jitter)}); bot.push_back({x, cy + v + (i == 0 ? 0.0f : jitter)});
        }
        fill = top; fill.push_back({r.getRight(), cy});
        for (size_t i = N; i-- > 0;) fill.push_back({r.getX() + r.getWidth() * float(i) / float(N - 1), cy + hist[i] * amp});
        fillPoly(g, fill, wa(litC, 0.22f), wa(litC, 0.04f));
        std::vector<Pt> ghost; for (auto& p : top) ghost.push_back({p.x + 1.5f, p.y});
        strokePoly(g, ghost, Colour(0x30ff2a6a), 1.0f, false);
        glowStrokePts(g, bot, wa(litC, 0.6f), 1.0f, 0.6f, glow, false);
        glowStrokePts(g, top, litC, 1.2f, 0.9f, glow, false);
        glowDot(g, {r.getRight() - 3, cy - hist.back() * amp}, 2.0f, litC);
    }
    if (clip && std::fmod(std::fmod(t, 0.8f) + 0.8f, 0.8f) < 0.5f) {
        Rf band(r.getX(), r.getY() + 3, r.getWidth(), 14);
        if (glow) bloomRect(g, band, 0, Colour(0xffff2a3a), 6, 0.25f);
        solidRect(g, band, Colour(0xaaff2a3a));
        litText(g, band, "! ATTENTION  CLIP", Colours::white, 9.0f, glow);
    }
    scanlines(g, r, t, 0.2f);
    chamferGlass(g, r.reduced(1.5f), 8, true);
    popClip(g); popClip(g); popClip(g);
    glowStrokePts(g, frame, wa(litC, 0.8f), 1.2f, 0.7f, glow, true);
}

// 9. Dashboard chassis with a bevelled body, screws and a bar window.
inline void drawChassis(Graphics& g, Rectangle<float> r, float level, Colour body, Colour litC, float t) {
    using namespace lit; const bool glow = glowEnabled; Lt lt;
    const Pt o = lt.offset(5);
    shadowRound(g, r.translated(o.x, o.y), 9, lt.sigma(9), Colour(0x99000000));
    shadowRound(g, r.translated(o.x * 0.25f, o.y * 0.25f), 9, 1.0f, Colour(0x66000000));
    fillRoundLinear(g, r, 9, {{0, brt(body, 0.75f)}, {0.35f, brt(body, 0.2f)}, {0.7f, drk(body, 0.3f)}, {1, drk(body, 0.62f)}}, lt.away());
    const auto face = r.reduced(1.8f);
    fillRoundLinear(g, face, 7.4f, {{0, brt(body, 0.26f)}, {0.45f, brt(body, 0.04f)}, {1, drk(body, 0.32f)}}, lt.away() * 0.25f + HALF_PI * 0.75f);
    fillRoundLinear(g, {face.getX(), face.getY(), face.getWidth(), face.getHeight() * 0.4f}, 7.4f, {{0, Colour(0x30ffffff)}, {1, Colour(0x00ffffff)}}, HALF_PI);
    strokeRound(g, r, 9, wa(drk(body, 0.82f), 0.9f), 0.8f);
    const Pt toward = lt.toward();
    for (Pt p : {Pt(r.getX() + 7, r.getY() + 7), Pt(r.getRight() - 7, r.getY() + 7), Pt(r.getX() + 7, r.getBottom() - 7), Pt(r.getRight() - 7, r.getBottom() - 7)}) {
        auto head = boxAt(5, 5, p); const Pt so = lt.offset(1.2f);
        shadowEllipse(g, head.translated(so.x, so.y), 0.8f, Colour(0x90000000));
        const Pt f = p + toward * 1.2f;
        juce::ColourGradient rg(brt(body, 0.55f), f.x, f.y, drk(body, 0.72f), p.x + 2.5f, p.y, true);
        rg.addColour(0.6, drk(body, 0.35f)); g.setGradientFill(rg); g.fillEllipse(head);
        line(g, {p.x - 1.5f, p.y + 1}, {p.x + 1.5f, p.y - 1}, drk(body, 0.85f), 0.7f);
    }
    auto s = r.reduced(13, 11); const auto win = s;
    windowGround(g, win, 5, Colour(0xff050a0c), Colour(0xff020405));
    const auto bars = trimTop(trimBottom(s.reduced(8, 0), 14), 8); const int n = 28; std::vector<Seg> segs;
    for (int i = 0; i < n; ++i) {
        const float f = float(i) / float(n - 1), x = bars.getX() + f * bars.getWidth(); const bool on = f <= level + 0.5f / float(n);
        const Colour c = f > 0.85f ? Colour(0xffff4a5a) : litC; const float inset = i % 4 == 0 ? 0.0f : 5.0f;
        segs.push_back({Rf(x, bars.getY() + inset, bars.getWidth() / float(n) * 0.62f, bars.getHeight() - inset), c, on});
    }
    segments(g, segs, 0.3f, t, glow);
    auto foot = cutBottom(s, 13);
    litText(g, Rf(foot.getX() + 8, foot.getY(), foot.getWidth() - 8, foot.getHeight()), "OUTPUT  //  LEVEL", wa(litC, 0.8f), 8.0f, glow);
    scanlines(g, win, t, 0.14f);
    windowGlass(g, win, 5);
    strokeRound(g, win.reduced(0.5f), 4.5f, wa(litC, 0.22f), 1.0f);
}

// 10. Plain two-row bar meter: no bloom or scanlines. Matt lenses in a shallow recess, each lit one
// graded with a thin highlight, the peak segment hotter, a printed dB scale below (dropped when short).
inline void drawFlatBars(Graphics& g, Rectangle<float> r, float l, float rr, float peakL, float peakR, Colour lo) {
    using namespace lit; Lt lt;
    recess(g, r, 4, Colour(0xff111416), Colour(0xff0a0c0d), lt);
    const bool scale = r.getHeight() >= 50; const float padY = lim(r.getHeight() * 0.16f, 5.0f, 9.0f);
    auto b = r.reduced(12, padY); if (scale) b.setHeight(std::max(0.0f, b.getHeight() - 10));
    const float gap = lim(b.getHeight() * 0.2f, 3.0f, 8.0f), h = (b.getHeight() - gap) * 0.5f; const int n = 30;
    const float seg = b.getWidth() / float(n), rad = std::min(h * 0.12f, 1.0f);
    for (int c = 0; c < 2; ++c) {
        const float lv = c == 0 ? l : rr, pk = c == 0 ? peakL : peakR;
        Rf row(b.getX(), b.getY() + float(c) * (h + gap), b.getWidth(), h);
        solidRound(g, row.expanded(1.2f), 1.6f, Colour(0xff040506));
        hline(g, row.getBottom() + 1.5f, row.getX(), row.getRight(), Colour(0x16ffffff), 0.6f);
        for (int i = 0; i < n; ++i) {
            const float f = float(i) / float(n - 1);
            Colour cc = f > 0.88f ? Colour(0xffe5382a) : f > 0.70f ? Colour(0xffe8b923) : lo;
            const bool peak = std::abs(f - pk) < 0.5f / float(n), on = f <= lv + 0.5f / float(n) || peak;
            Rf cell(row.getX() + float(i) * seg + 0.6f, row.getY(), std::max(0.5f, seg - 1.2f), row.getHeight());
            if (on) {
                if (peak) cc = brt(cc, 0.35f);
                fillRoundLinear(g, cell, rad, {{0, brt(cc, 0.30f)}, {0.25f, brt(cc, 0.08f)}, {1, drk(cc, 0.24f)}}, HALF_PI);
                hline(g, cell.getY() + 0.5f, cell.getX() + 0.4f, cell.getRight() - 0.4f, Colour(0x70ffffff), 0.5f);
            } else {
                fillRoundLinear(g, cell, rad, {{0, drk(cc, 0.80f)}, {1, drk(cc, 0.88f)}}, HALF_PI);
                hline(g, cell.getY() + 0.5f, cell.getX() + 0.4f, cell.getRight() - 0.4f, Colour(0x12ffffff), 0.5f);
            }
        }
    }
    if (scale) for (int i = 0; i <= 6; ++i) {
        const float x = b.getX() + b.getWidth() * float(i) / 6.0f;
        vline(g, x, b.getBottom() + 2, b.getBottom() + 4, wa(Colour(0xff8b9498), 0.7f), 0.6f);
        textC(g, boxAt(24, 9, {x, r.getBottom() - padY * 0.5f - 2.5f}), juce::String(-48 + i * 8), Colour(0xff9aa3a7), 7.5f);
    }
    recessShade(g, r, 4, 2.2f, lt);
    strokeRound(g, r, 4, Colour(0xe6000000), 1.0f);
    strokeRound(g, r.reduced(0.5f), 3.5f, Colour(0x14ffffff), 0.6f);
}

// ---- icon lamps -------------------------------------------------------------
enum class Icon { Battery, Warning, ArrowLeft, ArrowRight, Beam, Temp };
namespace detail {
// Icon stroke on a lamp lens: a ghost when off, a lit filament with a soft sleeve when on.
inline void lampPoly(Graphics& g, const std::vector<Point<float>>& pts, Colour c, float w, bool closed, bool on) {
    using namespace lit;
    if (!on) { strokePoly(g, pts, wa(c, 0.12f), w, closed); return; }
    strokePoly(g, pts, wa(c, 0.26f), w + 2.4f, closed); strokePoly(g, pts, c, w, closed); strokePoly(g, pts, wa(brt(c, 0.65f), 0.85f), w * 0.4f, closed);
}
inline std::vector<Point<float>> rectPts(Rectangle<float> r) { return {{r.getX(), r.getY()}, {r.getRight(), r.getY()}, {r.getRight(), r.getBottom()}, {r.getX(), r.getBottom()}}; }
}
inline void drawIconLamp(Graphics& g, Rectangle<float> r, Icon icon, Colour c, bool on) {
    using namespace lit; const Colour hue = wa(c, 1.0f);
    if (on) bloomRect(g, r, 3, hue, 7, 0.13f);
    windowGround(g, r, 3, on ? drk(hue, 0.90f) : Colour(0xff070808), on ? drk(hue, 0.96f) : Colour(0xff030404));
    if (on) {
        glowEllipse(g, r.expanded(r.getWidth() * 0.1f), wa(hue, 0.20f), r.getWidth() * 0.6f);
        pushClipRound(g, r, 3); glowEllipse(g, r.reduced(r.getWidth() * 0.12f), wa(hue, 0.16f), r.getWidth() * 0.38f); popClip(g);
    }
    const Colour col = on ? c : wa(c, 0.12f), hot = on ? brt(c, 0.35f) : col; const auto a = r.reduced(r.getWidth() * 0.24f);
    auto glyph = [&](Rf box, const juce::String& s, float size) { if (on) litText(g, box, s, hot, size, true); else textC(g, box, s, hot, size); };
    switch (icon) {
        case Icon::Battery: {
            detail::lampPoly(g, detail::rectPts(trimTop(a, a.getHeight() * 0.2f)), c, 1.4f, true, on);
            detail::lampPoly(g, detail::rectPts(boxAt(a.getWidth() * 0.3f, a.getHeight() * 0.16f, {a.getCentreX(), a.getY() + a.getHeight() * 0.08f})), c, 1.4f, true, on);
            glyph(trimTop(a, a.getHeight() * 0.2f), "+", a.getHeight() * 0.55f); break; }
        case Icon::Warning:
            detail::lampPoly(g, {{a.getCentreX(), a.getY()}, {a.getRight(), a.getBottom()}, {a.getX(), a.getBottom()}}, c, 1.5f, true, on);
            glyph(trimTop(a, a.getHeight() * 0.25f), "!", a.getHeight() * 0.62f); break;
        case Icon::ArrowLeft: case Icon::ArrowRight: {
            const float tip = icon == Icon::ArrowLeft ? a.getX() : a.getRight(), back = icon == Icon::ArrowLeft ? a.getRight() : a.getX();
            std::vector<Point<float>> tri{{tip, a.getCentreY()}, {back, a.getY()}, {back, a.getBottom()}};
            if (on) strokePoly(g, tri, wa(c, 0.26f), 2.6f, true);
            fillPoly(g, tri, on ? brt(c, 0.4f) : col, on ? drk(c, 0.12f) : col); break; }
        case Icon::Beam: {
            for (int i = 0; i < 4; ++i) { const float y = a.getY() + a.getHeight() * (0.15f + 0.23f * float(i)); detail::lampPoly(g, {{a.getX() + a.getWidth() * 0.35f, y}, {a.getRight(), y}}, c, 1.6f, false, on); }
            Rf lens(a.getX(), a.getCentreY() - a.getHeight() * 0.3f, a.getWidth() * 0.4f, a.getHeight() * 0.6f);
            if (on) solidEllipse(g, lens.expanded(1.2f), wa(c, 0.26f));
            fillEllipse(g, lens, hot, col); break; }
        case Icon::Temp: {
            detail::lampPoly(g, {{a.getCentreX(), a.getY()}, {a.getCentreX(), a.getBottom() - a.getWidth() * 0.3f}}, c, 2.4f, false, on);
            Rf bulb(a.getCentreX() - a.getWidth() * 0.28f, a.getBottom() - a.getWidth() * 0.56f, a.getWidth() * 0.56f, a.getWidth() * 0.56f);
            if (on) solidEllipse(g, bulb.expanded(1.2f), wa(c, 0.26f));
            fillEllipse(g, bulb, hot, col); break; }
    }
    windowGlass(g, r, 3);
    strokeRound(g, r.reduced(0.5f), 2.5f, on ? wa(c, 0.40f) : Colour(0xff24292b), 1.0f);
}

// ---- sliders ---------------------------------------------------------------
enum class SliderStyle { Neon, Bar, Chamfer, Wedge, Flat };
namespace detail {
// Raised slider cap: soft shadow thrown away from the light, a bevelled rim and a flatter face.
inline void cap(Graphics& g, Rectangle<float> r, float radius, Colour face, lit::Lt lt) {
    using namespace lit; const Pt o = lt.offset(3.5f);
    shadowRound(g, r.translated(o.x, o.y), radius, lt.sigma(8), Colour(0x9c000000));
    fillRoundLinear(g, r, radius, {{0, brt(face, 0.9f)}, {0.5f, drk(face, 0.14f)}, {1, drk(face, 0.55f)}}, lt.away());
    fillRoundLinear(g, r.reduced(1), std::max(0.0f, radius - 1), {{0, brt(face, 0.25f)}, {1, drk(face, 0.24f)}}, lt.away());
    strokeRound(g, r, radius, Colour(0xb0000000), 0.6f);
}
// The lit part of a track: bloom below, then a bar brightest along its upper third.
inline void litBar(Graphics& g, Rectangle<float> r, float radius, Colour c, float k, bool glow) {
    using namespace lit; if (r.getWidth() <= 0 || r.getHeight() <= 0) return;
    if (glow) bloomBlock(g, r, radius, c, 2.0f * k);
    fillRoundLinear(g, r, radius, {{0, brt(c, 0.2f)}, {0.3f, brt(c, 0.6f)}, {0.6f, c}, {1, drk(c, 0.25f)}}, HALF_PI);
}
inline void sliderH(Graphics& g, Rectangle<float> r, float prop, SliderStyle st, Colour c, float t, bool glow, lit::Lt lt) {
    using namespace lit; using Pt = lit::Pt;
    prop = lim(prop, 0.0f, 1.0f); const float cy = r.getCentreY(), x = r.getX() + prop * r.getWidth(), k = glow ? flicker(t) : 1.0f;
    switch (st) {
        case SliderStyle::Neon: {
            auto groove = boxAt(r.getWidth() + 8, 5, {r.getCentreX(), cy});
            recess(g, groove, 2.5f, Colour(0xff05090b), Colour(0xff020405), lt); recessShade(g, groove, 2.5f, 1.4f, lt);
            hline(g, cy, r.getX(), r.getRight(), wa(c, 0.22f), 1.0f);
            for (int i = 0; i <= 10; ++i) { const float tx = r.getX() + r.getWidth() * float(i) / 10.0f; line(g, {tx, cy + 6}, {tx, cy + (i % 5 == 0 ? 12.0f : 9.0f)}, wa(c, 0.35f), 0.9f); }
            litBar(g, Rf(r.getX(), cy - 1.2f, std::max(0.0f, x - r.getX()), 2.4f), 1.2f, c, k, glow);
            const Pt o = lt.offset(3);
            shadowEllipse(g, boxAt(11, 19, {x + o.x, cy + o.y}), 2.2f, Colour(0xa0000000));
            std::vector<Pt> d{{x, cy - 10}, {x + 6, cy}, {x, cy + 10}, {x - 6, cy}};
            fillPoly(g, d, drk(c, 0.72f), Colour(0xff04090c));
            glowStrokePts(g, d, brt(c, 0.2f), 1.3f, 0.9f, glow, true);
            line(g, {x, cy - 10}, {x, cy + 10}, wa(brt(c, 0.2f), 0.55f), 0.9f);
            if (glow) { glowEllipse(g, boxAt(14.4f, 14.4f, {x, cy}), wa(Colours::white, 0.30f), 3.2f); glowEllipse(g, boxAt(7.0f, 7.0f, {x, cy}), wa(Colours::white, 0.55f), 2.6f); }
            solidEllipse(g, boxAt(2.4f, 2.4f, {x, cy}), brt(Colours::white, 0.8f)); break; }
        case SliderStyle::Bar: {
            auto win = boxAt(r.getWidth() + 10, 30, {r.getCentreX(), cy});
            recess(g, win, 3, Colour(0xff080a0b), Colour(0xff030405), lt);
            const int n = 30; const float seg = r.getWidth() / float(n); std::vector<Seg> segs;
            for (int i = 0; i < n; ++i) {
                const float f = float(i) / float(n - 1); const bool on = f <= prop + 0.5f / float(n);
                const Colour cc = f > 0.85f ? Colour(0xffff3b2a) : f > 0.66f ? Colour(0xffffcf2a) : c;
                segs.push_back({boxAt(seg - 1.8f, 12.0f + 10.0f * f, {r.getX() + float(i) * seg + 0.8f + (seg - 1.8f) * 0.5f, cy}), cc, on});
            }
            segments(g, segs, 0.5f, t, glow);
            recessShade(g, win, 3, 2.6f, lt);
            cap(g, boxAt(5, 34, {x, cy}), 1.5f, Colour(0xffe9e9e2), lt); break; }
        case SliderStyle::Chamfer: {
            auto track = boxAt(r.getWidth(), 12, {r.getCentreX(), cy}); auto frame = chamferPts(track, 4, false); const auto litR = withRight(track, x);
            if (glow && litR.getWidth() > 0) bloomRect(g, litR, 3, wa(c, 1.0f), 6, 0.16f * k);
            fillPoly(g, frame, Colour(0xff04080b), Colour(0xff020405));
            pushClipRound(g, track.reduced(1), 3);
            fillRoundLinear(g, litR, 0, {{0, drk(c, 0.1f)}, {0.28f, brt(c, 0.5f)}, {0.6f, drk(c, 0.05f)}, {1, drk(c, 0.4f)}}, HALF_PI);
            popClip(g);
            recessShade(g, track, 4, 2.0f, lt);
            glowStrokePts(g, frame, c, 1.2f, 0.7f, glow, true);
            const Pt o = lt.offset(3.5f);
            shadowRound(g, boxAt(16, 22, {x + o.x, cy + o.y}), 5, lt.sigma(8), Colour(0xa0000000));
            auto hex = chamferPts(boxAt(16, 22, {x, cy}), 5, false);
            fillPoly(g, hex, drk(c, 0.62f), Colour(0xff070d12));
            glowStrokePts(g, hex, brt(c, 0.3f), 1.4f, 1.0f, glow, true);
            litBar(g, boxAt(2, 10, {x, cy}), 1.0f, brt(c, 0.6f), 0.6f * k, glow); break; }
        case SliderStyle::Flat: {
            auto track = boxAt(r.getWidth(), 6, {r.getCentreX(), cy});
            recess(g, track, 3, Colour(0xff0b0d0e), Colour(0xff050607), lt);
            litBar(g, withRight(track, x), 3, c, 0.8f * k, glow);
            recessShade(g, track, 3, 1.3f, lt);
            cap(g, boxAt(8, 20, {x, cy}), 2, Colour(0xffe6e6e0), lt); break; }
        case SliderStyle::Wedge: {
            // A rising ramp of display bars in a recessed window; the top of the ramp runs through orange to red.
            Rf win(r.getX() - 6, cy - 15, r.getWidth() + 12, 29);
            recess(g, win, 3, mix(Colour(0xff0a0d0e), c, 0.035f), Colour(0xff030405), lt);
            const int n = juce::jlimit(10, 56, int(r.getWidth() / 7.0f)); const float pitch = r.getWidth() / float(n), base = cy + 10;
            auto height = [](float f) { return 3.0f + 19.0f * std::pow(f, 1.15f); };
            std::vector<Seg> segs;
            for (int i = 0; i < n; ++i) {
                const float f = (float(i) + 0.5f) / float(n);
                const Colour cc = f > 0.86f ? Colour(0xffff3b2a) : f > 0.70f ? mix(c, Colour(0xffff8a1f), 0.65f) : c; const float h = height(f);
                segs.push_back({Rf(r.getX() + float(i) * pitch + pitch * 0.2f, base - h, pitch * 0.6f, h), cc, f <= prop});
            }
            if (glow && prop > 0) bloomRect(g, Rf(r.getX(), base - height(prop) * 0.6f, std::max(0.0f, x - r.getX()), height(prop) * 0.6f), 2, wa(c, 1.0f), 7, 0.07f * k);
            segments(g, segs, 0.6f, t, glow);
            gapDark(g, segs, Colour(0xff030405), 0.6f);
            for (auto& sg : segs) if (sg.on) solidRect(g, Rf(sg.r.getX(), sg.r.getY(), sg.r.getWidth(), 1.0f), brt(sg.c, 0.75f));
            line(g, {r.getX(), base - height(0) - 2}, {r.getRight(), base - height(1) - 2}, wa(c, 0.30f), 0.6f);
            hline(g, base + 1.6f, r.getX(), r.getRight(), wa(c, 0.40f), 0.7f);
            for (int i = 0; i <= 4; ++i) { const float tx = r.getX() + r.getWidth() * float(i) / 4.0f; vline(g, tx, base + 1.6f, base + 3.6f, wa(c, 0.40f), 0.7f); }
            recessShade(g, win, 3, 2.6f, lt);
            strokeRound(g, win.reduced(0.5f), 2.5f, wa(c, 0.16f), 1.0f);
            cap(g, boxAt(5, 33, {x, cy - 0.5f}), 1.5f, Colour(0xffeeeeea), lt); break; }
    }
}
}
// Draws a horizontal slider; for vertical sliders see drawSlider(), which rotates it.
inline void drawSliderH(Graphics& g, Rectangle<float> r, float prop, SliderStyle st, Colour c, float t) { detail::sliderH(g, r, prop, st, c, t, glowEnabled, lit::Lt{}); }
inline void drawSlider(Graphics& g, Rectangle<float> r, float prop, SliderStyle st, Colour c, bool vertical, float t) {
    if (!vertical) { drawSliderH(g, r, prop, st, c, t); return; }
    const float turn = -juce::MathConstants<float>::halfPi;
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(turn, r.getCentreX(), r.getCentreY()));
    detail::sliderH(g, Rectangle<float>(r.getHeight(), r.getWidth()).withCentre(r.getCentre()), prop, st, c, t, glowEnabled, lit::Lt{turn});
    g.restoreState();
}
} // namespace goodlookinui::juce_adapter::retro
