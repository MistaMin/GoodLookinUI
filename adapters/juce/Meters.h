// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <vector>

// Procedural meters, readouts, lamps, keys and faders. Everything is drawn at the
// component's current scale (no bitmaps). Levels are passed in as plain numbers so
// the same drawing works for any plugin.
namespace goodlookinui::juce_adapter::meters {
using juce::Colour;
using juce::Graphics;
using juce::Point;
using juce::Rectangle;

inline constexpr float pi = juce::MathConstants<float>::pi;

// ---- scales ---------------------------------------------------------------
// VU needle position (0..1) for an amplitude relative to the 0 VU reference.
// Deflection is proportional to voltage, with +3 VU at full scale.
inline float vuPosition(float amplitudeOverReference) {
    return juce::jlimit(0.0f, 1.0f, amplitudeOverReference * 0.7079f);
}
inline float vuPositionForDb(float vu) { return vuPosition(std::pow(10.0f, vu / 20.0f)); }
// 0..1 position on a plain dB scale.
inline float dbPosition(float db, float floorDb = -48.0f, float ceilDb = 0.0f) {
    return juce::jlimit(0.0f, 1.0f, (db - floorDb) / (ceilDb - floorDb));
}
inline Colour ladderColour(float db) {
    return db > -3.0f ? Colour(0xffe5382a) : db > -12.0f ? Colour(0xfff0b323) : Colour(0xff3ccf5a);
}


// ---- lit rendering helpers --------------------------------------------------
// A small stand-in for the Rust scene commands: multi-stop gradients, feathered glows,
// soft shadows and inner shadows, all expressed with plain juce::Graphics calls. Shadows and
// feathers are built from a few stacked translucent layers, which is cheap and needs no images.
// Add/Screen blends have no JUCE equivalent; they are drawn as ordinary translucent fills.
namespace lit {
using Stop = std::pair<float, Colour>;
using Stops = std::vector<Stop>;

// Colour maths with the same meaning as GoodLookinDri's Color (JUCE's own darker() differs).
inline Colour dk(Colour c, float a) { a = juce::jlimit(0.0f, 1.0f, a); return Colour::fromFloatRGBA(c.getFloatRed() * (1 - a), c.getFloatGreen() * (1 - a), c.getFloatBlue() * (1 - a), c.getFloatAlpha()); }
inline Colour br(Colour c, float a) { a = juce::jmax(0.0f, a); auto f = [&](float v) { return juce::jlimit(0.0f, 1.0f, v + a * (1 - v)); }; return Colour::fromFloatRGBA(f(c.getFloatRed()), f(c.getFloatGreen()), f(c.getFloatBlue()), c.getFloatAlpha()); }
inline Colour mix(Colour a, Colour b, float t) { return a.interpolatedWith(b, juce::jlimit(0.0f, 1.0f, t)); }
inline Colour wh(float a) { return Colour::fromFloatRGBA(1, 1, 1, a); }
inline Colour bk(float a) { return Colour::fromFloatRGBA(0, 0, 0, a); }
inline Colour opaque(Colour c) { return c.withAlpha(1.0f); }
inline Colour sampleStops(const Stops& s, float t) {
    if (t <= s.front().first) return s.front().second;
    for (size_t i = 1; i < s.size(); ++i)
        if (t <= s[i].first) { const float k = (t - s[i - 1].first) / juce::jmax(1e-6f, s[i].first - s[i - 1].first); return s[i - 1].second.interpolatedWith(s[i].second, k); }
    return s.back().second;
}

// The single studio light shared by every part (key 20 degrees left of up, 65 degrees elevation).
struct Rig {
    float key[3] = {-0.14452f, -0.39707f, 0.90631f};
    float keyCol[3] = {1.0f, 0.97f, 0.92f}, fillCol[3] = {0.78f, 0.84f, 0.95f};
    float softness = 0.27f, ambient = 0.32f;
    Point<float> shadowOffset(float h) const { const float k = h / juce::jmax(0.05f, key[2]); return {-key[0] * k, -key[1] * k}; }
    float shadowSigma(float h) const { return juce::jmax(0.5f, h * softness); }
    void shade(const float n[3], float out[3]) const {
        const float d = juce::jmax(0.0f, n[0] * key[0] + n[1] * key[1] + n[2] * key[2]) * (1 - ambient);
        for (int i = 0; i < 3; ++i) out[i] = fillCol[i] * ambient + keyCol[i] * d;
    }
    float specular(const float n[3], float shin) const {
        const float h[3] = {key[0], key[1], key[2] + 1.0f};
        const float l = juce::jmax(1e-6f, std::sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]));
        return std::pow(juce::jmax(0.0f, (n[0] * h[0] + n[1] * h[1] + n[2] * h[2]) / l), shin);
    }
};
inline const Rig& rig() { static const Rig r; return r; }
// Gradient angle that starts on the lit side and runs away from the key (screen space, radians).
inline float lightAngle() { return 1.2217305f; }

// `c` lit by the rig on a facet with normal n, relative to a flat facet.
inline Colour facet(Colour c, float nx, float ny, float nz, float gain) {
    const float l = juce::jmax(1e-6f, std::sqrt(nx * nx + ny * ny + nz * nz));
    const float n[3] = {nx / l, ny / l, nz / l}, flat[3] = {0, 0, 1};
    float s[3], f[3]; rig().shade(n, s); rig().shade(flat, f);
    const float spec = rig().specular(n, 24.0f) * 0.35f * gain;
    auto ch = [&](float v, int i) { return juce::jlimit(0.0f, 1.0f, v * std::pow(juce::jmax(0.0f, s[i] / f[i]), gain) + spec); };
    return Colour::fromFloatRGBA(ch(c.getFloatRed(), 0), ch(c.getFloatGreen(), 1), ch(c.getFloatBlue(), 2), c.getFloatAlpha());
}

// A rounded rectangle or ellipse, with the operations the scene commands need.
struct Shp {
    bool ellipse = false; Rectangle<float> r; float rad = 0;
    static Shp round(Rectangle<float> r, float rad) { return {false, r, rad}; }
    static Shp oval(Rectangle<float> r) { return {true, r, 0}; }
    Shp translated(Point<float> d) const { return {ellipse, r.translated(d.x, d.y), rad}; }
    Shp expanded(float d) const { return {ellipse, r.expanded(d), ellipse ? 0.0f : juce::jmax(0.0f, rad + d)}; }
    bool empty() const { return r.getWidth() <= 0.01f || r.getHeight() <= 0.01f; }
    juce::Path path() const { juce::Path p; if (ellipse) p.addEllipse(r); else p.addRoundedRectangle(r, juce::jmin(rad, 0.5f * juce::jmin(r.getWidth(), r.getHeight()))); return p; }
};
inline Rectangle<float> boxAt(float w, float h, Point<float> c) { return Rectangle<float>(w, h).withCentre(c); }
inline Rectangle<float> ellAt(Point<float> c, float rx, float ry) { return Rectangle<float>(rx * 2, ry * 2).withCentre(c); }

inline void fillShape(Graphics& g, const Shp& s, Colour c) { if (s.empty() || c.isTransparent()) return; g.setColour(c); g.fillPath(s.path()); }

// Linear gradient across the shape's bounds along `angle` (0 left to right, pi/2 top to bottom).
inline juce::FillType linearFill(Rectangle<float> b, const Stops& st, float angle) {
    const float dx = std::cos(angle), dy = std::sin(angle);
    const float len = std::abs(b.getWidth() * dx) + std::abs(b.getHeight() * dy);
    const auto c = b.getCentre();
    const Point<float> p0(c.x - dx * len * 0.5f, c.y - dy * len * 0.5f), p1(c.x + dx * len * 0.5f, c.y + dy * len * 0.5f);
    juce::ColourGradient cg(st.front().second, p0, st.back().second, p1, false);
    for (size_t i = 1; i + 1 < st.size(); ++i) cg.addColour(double(juce::jlimit(0.0f, 1.0f, st[i].first)), st[i].second);
    return juce::FillType(cg);
}
inline void fillLinear(Graphics& g, const Shp& s, const Stops& st, float angle) {
    if (s.empty()) return;
    g.setFillType(linearFill(s.r, st, angle)); g.fillPath(s.path());
}
// Radial gradient: t = 0 at `focus`, t = 1 on the ellipse (centre, rx, ry).
inline void fillRadial(Graphics& g, const Shp& s, const Stops& st, Point<float> centre, float rx, float ry, Point<float> focus) {
    if (s.empty()) return;
    const float ex = rx + std::abs(focus.x - centre.x), ey = ry + std::abs(focus.y - centre.y);
    const float k = 0.5f * (rx / ex + ry / ey);
    juce::ColourGradient cg(st.front().second, 0.0f, 0.0f, st.back().second, 1.0f, 0.0f, true);
    for (size_t i = 1; i < st.size(); ++i) cg.addColour(double(juce::jlimit(0.0f, 1.0f, st[i].first * k)), st[i].second);
    cg.addColour(1.0, st.back().second);
    juce::FillType ft(cg);
    ft.transform = juce::AffineTransform::scale(ex, ey).translated(focus.x, focus.y);
    g.setFillType(ft); g.fillPath(s.path());
}
inline void fillRadial(Graphics& g, const Shp& s, const Stops& st, Point<float> focus) { fillRadial(g, s, st, s.r.getCentre(), s.r.getWidth() * 0.5f, s.r.getHeight() * 0.5f, focus); }

// Feathered fill: fully painted `feather` inside the outline, fading to nothing on it.
inline void glow(Graphics& g, const Shp& s, Colour c, float feather) {
    if (c.isTransparent()) return;
    if (feather < 0.4f) { fillShape(g, s, c); return; }
    const int n = 6; const float a = c.getFloatAlpha(), p = 1.0f - std::pow(1.0f - a, 1.0f / float(n));
    for (int i = 0; i < n; ++i) {
        const Shp l = s.expanded(-feather * (float(i) + 0.5f) / float(n));
        if (!l.empty()) fillShape(g, l, c.withAlpha(p));
    }
}
// Soft drop shadow: coverage about half on the outline, ~1 inside, ~0 beyond 2 sigma outside.
inline void shadow(Graphics& g, const Shp& s, float sigma, Colour c) {
    const int n = 6; const float a = c.getFloatAlpha(), p = 1.0f - std::pow(1.0f - a, 1.0f / float(n));
    for (int i = 0; i < n; ++i) {
        const Shp l = s.expanded(2.0f * sigma - 4.0f * sigma * (float(i) + 0.5f) / float(n));
        if (!l.empty()) fillShape(g, l, c.withAlpha(p));
    }
}
inline void dropShadow(Graphics& g, const Shp& s, float height, float strength) {
    shadow(g, s.translated(rig().shadowOffset(height)), rig().shadowSigma(height), bk(strength));
}
// Shadow inside `s` as if cast by its own rim onto a recess `depth` deep.
inline void innerShadow(Graphics& g, const Shp& s, Point<float> off, float sigma, Colour c) {
    g.saveState();
    g.reduceClipRegion(s.path());
    const int n = 6; const float a = c.getFloatAlpha(), p = 1.0f - std::pow(1.0f - a, 1.0f / float(n));
    const auto big = s.r.expanded(sigma * 4.0f + 8.0f + std::abs(off.x) + std::abs(off.y));
    for (int i = 0; i < n; ++i) {
        const Shp shrunk = s.translated(off).expanded(2.0f * sigma - 4.0f * sigma * (float(i) + 0.5f) / float(n));
        juce::Path pa; pa.addRectangle(big);
        if (!shrunk.empty()) pa.addPath(shrunk.path());
        pa.setUsingNonZeroWinding(false);
        g.setColour(c.withAlpha(p)); g.fillPath(pa);
    }
    g.restoreState();
}
inline void recessShadow(Graphics& g, const Shp& s, float depth, float strength) { innerShadow(g, s, rig().shadowOffset(depth), rig().shadowSigma(depth), bk(strength)); }

inline void txt(Graphics& g, Point<float> c, const juce::String& t, Colour col, float size) {
    juce::Font f(juce::FontOptions(size, juce::Font::bold));
    const float w = juce::GlyphArrangement::getStringWidth(f, t) + 4.0f;
    g.setColour(col); g.setFont(f);
    g.drawText(t, boxAt(w, size * 1.4f, c), juce::Justification::centred, false);
}
inline void line(Graphics& g, Point<float> a, Point<float> b, Colour c, float w) { g.setColour(c); g.drawLine({a, b}, w); }
inline void poly(Graphics& g, const std::vector<Point<float>>& pts, Colour a, Colour b, bool closed = true) {
    if (pts.size() < 2) return;
    juce::Path p; p.startNewSubPath(pts[0]); for (size_t i = 1; i < pts.size(); ++i) p.lineTo(pts[i]); if (closed) p.closeSubPath();
    if (a == b) g.setColour(a);
    else { auto bb = p.getBounds(); g.setGradientFill(juce::ColourGradient(a, bb.getX(), bb.getY(), b, bb.getRight(), bb.getBottom(), false)); }
    g.fillPath(p);
}
inline void strokePoly(Graphics& g, const std::vector<Point<float>>& pts, Colour c, float w, bool closed) {
    if (pts.size() < 2) return;
    juce::Path p; p.startNewSubPath(pts[0]); for (size_t i = 1; i < pts.size(); ++i) p.lineTo(pts[i]); if (closed) p.closeSubPath();
    g.setColour(c); g.strokePath(p, juce::PathStrokeType(w));
}
inline void strokeShape(Graphics& g, const Shp& s, Colour c, float w) { g.setColour(c); g.strokePath(s.path(), juce::PathStrokeType(w)); }
inline void clipTo(Graphics& g, const Shp& s) { g.saveState(); g.reduceClipRegion(s.path()); }
inline void popClip(Graphics& g) { g.restoreState(); }
} // namespace lit

// ---- moving-coil meters ---------------------------------------------------
enum class VuFace { Cream, Black };

namespace detail {
using namespace lit;

// Deep black bezel raised off the panel with a chamfer down into the window. Returns window and its radius.
inline Rectangle<float> meterBezel(Graphics& g, Rectangle<float> r, float& wr) {
    const float la = lightAngle(), m = juce::jmin(r.getHeight(), r.getWidth());
    const float rad = juce::jlimit(2.5f, 6.0f, m * 0.035f), b = juce::jlimit(4.0f, 10.0f, m * 0.055f);
    dropShadow(g, Shp::round(r, rad), 3.0f, 0.6f);
    fillLinear(g, Shp::round(r, rad), {{0.0f, Colour(0xff5c5f62)}, {0.3f, Colour(0xff1d1e20)}, {1.0f, Colour(0xff020202)}}, la);
    fillLinear(g, Shp::round(r.reduced(1.1f), rad - 1.1f), {{0.0f, Colour(0xff27282b)}, {0.5f, Colour(0xff131415)}, {1.0f, Colour(0xff0a0a0b)}}, la);
    auto win = r.reduced(b);
    wr = juce::jmax(1.5f, rad - b * 0.5f);
    const float e = b * 0.45f;
    fillLinear(g, Shp::round(win.expanded(e), wr + e), {{0.0f, Colour(0xff010101)}, {0.55f, Colour(0xff0b0b0c)}, {1.0f, Colour(0xff3a3c3f)}}, la);
    fillShape(g, Shp::round(win.expanded(0.7f), wr + 0.7f), Colour(0xff000000));
    return win;
}

struct DialMark { float pos; const char* label; bool red; };
struct DialSpec {
    Stops litStops; Colour paper, glow, ink, red, needle;
    const DialMark* majors; int numMajors;
    std::vector<std::pair<float, bool>> minors;
    float redFrom;      // < 0: no red zone
    bool percent, signs; const char* title; bool titleLeft; float numSize, backlight;
};

inline std::vector<Point<float>> arcBand(Point<float> pivot, float a0, float a1, float outer, const std::function<float(float)>& inner, int n) {
    auto pt = [&](float a, float rad) { return Point<float>(pivot.x + std::sin(a) * rad, pivot.y - std::cos(a) * rad); };
    std::vector<Point<float>> pts;
    for (int i = 0; i <= n; ++i) pts.push_back(pt(a0 + (a1 - a0) * float(i) / float(n), outer));
    for (int i = n; i >= 0; --i) { const float t = float(i) / float(n); pts.push_back(pt(a0 + (a1 - a0) * t, inner(t))); }
    return pts;
}
inline std::vector<Point<float>> quadPts(Point<float> p0, Point<float> c, Point<float> p1, int n) {
    std::vector<Point<float>> v;
    for (int i = 0; i <= n; ++i) { const float t = float(i) / float(n), u = 1 - t; v.push_back({u * u * p0.x + 2 * u * t * c.x + t * t * p1.x, u * u * p0.y + 2 * u * t * c.y + t * t * p1.y}); }
    return v;
}

inline void drawDial(Graphics& g, Rectangle<float> r, float needle, const DialSpec& s) {
    float fr = 0;
    const auto f = meterBezel(g, r, fr);
    const float w = f.getWidth(), h = f.getHeight(), cx = f.getCentreX();
    const float bl = juce::jlimit(0.0f, 1.0f, s.backlight);
    const Shp shape = Shp::round(f, fr);

    clipTo(g, shape);
    // Face: paper lit from behind, a warm hotspot fading toward the corners.
    Stops st; for (auto& p : s.litStops) st.push_back({p.first, mix(s.paper, p.second, bl)});
    const Point<float> hot(cx, f.getY() + h * 0.56f);
    fillRadial(g, shape, st, hot, w * 0.66f, h * 0.98f, {cx, f.getY() + h * 0.48f});
    if (bl > 0.0f) glow(g, Shp::oval(boxAt(w * 0.62f, h * 0.78f, hot)), s.glow.withAlpha(0.20f * bl), h * 0.36f);

    // Arc geometry: the scale is an arc about a pivot below the window.
    const float half = w * 0.40f, sag = h * 0.22f;
    float radius = (half * half + sag * sag) / (2 * sag);
    float span = std::asin(juce::jmin(1.0f, half / radius));
    if (span > 0.72f) { span = 0.72f; radius = half / std::sin(span); }
    const Point<float> pivot(cx, f.getY() + h * 0.36f + radius);
    auto ang = [&](float p) { return -span + 2 * span * p; };
    auto pt = [&](float a, float rad) { return Point<float>(pivot.x + std::sin(a) * rad, pivot.y - std::cos(a) * rad); };
    const float lineW = juce::jmax(0.8f, h * 0.008f);
    const float redFrom = s.redFrom >= 0 ? s.redFrom : 1.0f;
    auto arc = [&](float from, float to, float rad, Colour c, float width) {
        juce::Path p; p.addCentredArc(pivot.x, pivot.y, rad, rad, 0.0f, from, to, true);
        g.setColour(c); g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };
    arc(ang(0.0f), ang(redFrom), radius, s.ink, lineW);
    if (s.redFrom >= 0) {
        const float thick = h * 0.05f;
        auto band = arcBand(pivot, ang(redFrom), ang(1.0f), radius + lineW * 0.5f, [&](float t) { return radius - lineW * 0.5f - thick * std::pow(t, 0.8f); }, 18);
        poly(g, band, s.red, s.red);
    }
    const float tickMajor = h * 0.075f, tickMinor = h * 0.042f;
    for (auto& m : s.minors) { const float a = ang(m.first); line(g, pt(a, radius), pt(a, radius + tickMinor), m.second ? s.red : s.ink, lineW * 0.9f); }
    const float ns = h * s.numSize;
    for (int i = 0; i < s.numMajors; ++i) {
        auto& m = s.majors[i];
        const float a = ang(m.pos); const Colour c = m.red ? s.red : s.ink;
        line(g, pt(a, radius - lineW), pt(a, radius + tickMajor), c, lineW * 1.35f);
        txt(g, pt(a, radius + tickMajor + ns * 0.72f), m.label, c, ns);
    }
    if (s.signs) {
        const float sign = ns * 0.95f;
        txt(g, pt(ang(0.0f) - 0.07f, radius + h * 0.01f), "-", s.ink, sign * 1.3f);
        txt(g, pt(ang(1.0f) + 0.07f, radius + h * 0.01f), "+", s.red, sign);
    }
    if (s.percent) {
        const float r2 = radius - h * 0.10f;
        static const char* labs[] = {"0", "20", "40", "60", "80", "100"};
        for (int i = 0; i < 6; ++i) {
            const float a = ang(juce::jmax(0.012f, vuPosition(float(i) * 0.2f))), dot = h * 0.011f;
            g.setColour(s.ink); g.fillEllipse(ellAt(pt(a, r2), dot, dot));
            txt(g, pt(a, r2 - h * 0.065f), labs[i], s.ink.withAlpha(0.85f), h * 0.058f);
        }
    }

    // Moulded cover over the movement.
    const float below = pivot.y - f.getBottom(), coverR = below + h * 0.13f;
    const float titleY = f.getBottom() - h * 0.22f, titleX = s.titleLeft ? f.getX() + w * 0.15f : f.getRight() - w * 0.14f;
    txt(g, {titleX, titleY}, s.title, s.ink, h * 0.125f);

    // Tapered needle with a soft shadow whose offset grows toward the tip (parallax).
    const float na = ang(juce::jlimit(0.0f, 1.0f, needle));
    const Point<float> d(std::sin(na), -std::cos(na)), n(std::cos(na), std::sin(na));
    const float r0 = coverR - 2.0f, r1 = radius + tickMajor + h * 0.02f;
    const auto base = pivot + d * r0, tip = pivot + d * r1;
    const float wb = juce::jmax(1.3f, h * 0.018f), wt = juce::jmax(0.45f, h * 0.0045f);
    auto needlePoly = [&](Point<float> b, Point<float> t, float kb, float kt) { return std::vector<Point<float>>{b + n * (wb * kb), t + n * (wt * kt), t - n * (wt * kt), b - n * (wb * kb)}; };
    const auto o0 = rig().shadowOffset(2.0f), o1 = rig().shadowOffset(h * 0.07f);
    const auto o0s = o0 * 1.4f, o1s = o1 * 1.5f;
    poly(g, needlePoly(base + o0s, tip + o1s, 2.6f, 5.5f), bk(0.035f), bk(0.035f));
    poly(g, needlePoly(base + o0s * 0.85f, tip + o1s * 0.85f, 1.5f, 3.0f), bk(0.05f), bk(0.05f));
    poly(g, needlePoly(base + o0, tip + o1, 0.7f, 1.3f), bk(0.18f), bk(0.18f));
    poly(g, needlePoly(base, tip, 0.5f, 0.5f), s.needle, s.needle);

    // Cover: shadow onto the face, then the black dome with a lit rim.
    const auto cr = ellAt(pivot, coverR, coverR);
    dropShadow(g, Shp::oval(cr), 3.5f, 0.55f);
    fillRadial(g, Shp::oval(cr), {{0.0f, Colour(0xff3a3c3f)}, {0.1f, Colour(0xff26272a)}, {0.5f, Colour(0xff141516)}, {1.0f, Colour(0xff0a0a0b)}}, {pivot.x - coverR * 0.18f, pivot.y - coverR * 0.9f});
    const float rimA = std::asin(juce::jmin(1.0f, f.getWidth() * 0.5f / coverR));
    arc(-rimA, rimA * 0.4f, coverR - 0.6f, wh(0.22f), 0.7f);

    // The bezel's shadow on everything behind the glass.
    recessShadow(g, shape, 5.0f, 0.85f);
    recessShadow(g, shape, 11.0f, 0.35f);

    // Glass, two layers: a soft diagonal streak, a sharp lit edge; and a faint inner reflection.
    fillLinear(g, shape, {{0.0f, wh(0)}, {0.16f, wh(0)}, {0.25f, wh(0.13f)}, {0.33f, wh(0.04f)}, {0.42f, wh(0)}, {1.0f, wh(0)}}, 0.62f);
    fillLinear(g, shape, {{0.0f, wh(0)}, {0.58f, wh(0)}, {0.66f, wh(0.06f)}, {0.74f, wh(0)}, {1.0f, wh(0)}}, 0.62f);
    auto refl = quadPts({f.getX() + 2, f.getY() + h * 0.30f}, {cx, f.getY() + h * 0.05f}, {f.getRight() - 2, f.getY() + h * 0.30f}, 10);
    refl.push_back({f.getRight() - 2, f.getY() + 2}); refl.push_back({f.getX() + 2, f.getY() + 2});
    poly(g, refl, wh(0.05f), wh(0.0f));
    const float inset = juce::jmax(2.0f, h * 0.035f), ra = s.paper.getPerceivedBrightness() > 0.5f ? 1.0f : 0.45f;
    strokePoly(g, {{f.getX() + inset, f.getBottom() - h * 0.14f}, {f.getX() + inset, f.getBottom() - inset}, {f.getX() + w * 0.22f, f.getBottom() - inset}}, wh(0.12f * ra), 0.6f, false);
    strokePoly(g, {{f.getRight() - w * 0.22f, f.getBottom() - inset}, {f.getRight() - inset, f.getBottom() - inset}, {f.getRight() - inset, f.getBottom() - h * 0.14f}}, wh(0.08f * ra), 0.6f, false);
    popClip(g);
    // Sharp glass edge catching the key along the top and left.
    line(g, {f.getX() + fr + 1, f.getY() + 0.6f}, {f.getRight() - fr - 1, f.getY() + 0.6f}, wh(0.30f), 0.6f);
    line(g, {f.getX() + 0.6f, f.getY() + fr + 1}, {f.getX() + 0.6f, f.getBottom() - fr - 1}, wh(0.12f), 0.6f);
}
} // namespace detail

// VU meter. `needle` is the 0..1 movement position (see vuPosition); `backlight` 0 (lamp off) .. 1.
inline void drawVuMeterLit(Graphics& g, Rectangle<float> r, float needle, VuFace face, float backlight) {
    using detail::DialMark; using detail::DialSpec;
    static const DialMark majors[] = {{0.0708f, "20", false}, {0.2239f, "10", false}, {0.3162f, "7", false}, {0.3981f, "5", false}, {0.5012f, "3", false}, {0.5623f, "2", false},
                                      {0.6310f, "1", false}, {0.7079f, "0", false}, {0.7943f, "1", true}, {0.8913f, "2", true}, {1.0f, "3", true}};
    static const float minorsDb[] = {-15, -6, -4, -2.5f, -1.5f, -0.5f, 0.5f, 1.5f, 2.5f};
    DialSpec s;
    for (float d : minorsDb) s.minors.push_back({vuPositionForDb(d), d > 0});
    s.majors = majors; s.numMajors = 11; s.redFrom = vuPositionForDb(0.0f); s.percent = true; s.signs = true;
    s.title = "VU"; s.titleLeft = false; s.numSize = 0.10f; s.backlight = backlight;
    if (face == VuFace::Cream) {
        s.litStops = {{0.0f, Colour(0xfffff6dc)}, {0.42f, Colour(0xfff8e2ac)}, {0.78f, Colour(0xffe2b470)}, {1.0f, Colour(0xffa8692e)}};
        s.paper = Colour(0xffcfc3a4); s.glow = Colour(0xffffd8a0); s.ink = Colour(0xff1d1914); s.red = Colour(0xffd0261c); s.needle = Colour(0xff15130f);
    } else {
        s.litStops = {{0.0f, Colour(0xff3b2d1c)}, {0.45f, Colour(0xff261d12)}, {0.8f, Colour(0xff140f0a)}, {1.0f, Colour(0xff070605)}};
        s.paper = Colour(0xff121110); s.glow = Colour(0xffff9a3a); s.ink = Colour(0xffffc27a); s.red = Colour(0xffff5a32); s.needle = Colour(0xffffb04e);
    }
    detail::drawDial(g, r, needle, s);
}
inline void drawVuMeter(Graphics& g, Rectangle<float> r, float needle, VuFace face) { drawVuMeterLit(g, r, needle, face, 1.0f); }

// ---- PPM needle meter (broadcast-style 1..7 scale, 4 = reference) -------------
inline void drawPpmLit(Graphics& g, Rectangle<float> r, float level01, float backlight) {
    using detail::DialMark; using detail::DialSpec;
    static const DialMark majors[] = {{0.0f, "1", false}, {1.0f / 6, "2", false}, {2.0f / 6, "3", false}, {3.0f / 6, "4", false}, {4.0f / 6, "5", false}, {5.0f / 6, "6", false}, {1.0f, "7", true}};
    DialSpec s;
    for (int i = 0; i < 6; ++i) s.minors.push_back({(float(i) + 0.5f) / 6.0f, i == 5});
    s.litStops = {{0.0f, Colour(0xfffffffb)}, {0.5f, Colour(0xfff1f1ea)}, {0.82f, Colour(0xffd9dad0)}, {1.0f, Colour(0xffa9aba2)}};
    s.paper = Colour(0xffc4c5bd); s.glow = Colour(0xfffff0d8); s.ink = Colour(0xff1b1d1c); s.red = Colour(0xffc8261c); s.needle = Colour(0xff121211);
    s.majors = majors; s.numMajors = 7; s.redFrom = 5.0f / 6.0f; s.percent = false; s.signs = false;
    s.title = "PPM"; s.titleLeft = true; s.numSize = 0.15f; s.backlight = backlight;
    detail::drawDial(g, r, level01, s);
}
inline void drawPpm(Graphics& g, Rectangle<float> r, float level01) { drawPpmLit(g, r, level01, 1.0f); }

// ---- LED ladder -----------------------------------------------------------
// One rectangular LED lens at `brightness` 0..1, moulded like a low barrel. Bloom is separate: drawLedBloom.
inline void drawLed(Graphics& g, Rectangle<float> s, Colour colour, float brightness) {
    using namespace lit;
    const float k = juce::jlimit(0.0f, 1.0f, brightness), down = juce::MathConstants<float>::halfPi;
    const Colour c = opaque(colour);
    const float thin = juce::jmin(s.getWidth(), s.getHeight()), rr = juce::jlimit(0.6f, 2.2f, thin * 0.22f);
    const Shp shape = Shp::round(s, rr);
    const float gp = juce::jlimit(0.45f, 0.9f, thin * 0.10f);
    strokeShape(g, shape.expanded(gp * 0.5f), Colour(0xff010202), gp + 0.6f);
    fillLinear(g, shape, {{0.0f, dk(c, 0.60f)}, {0.30f, dk(c, 0.74f)}, {0.70f, dk(c, 0.82f)}, {0.92f, dk(c, 0.66f)}, {1.0f, dk(c, 0.86f)}}, down);
    if (k > 0.0f) {
        fillLinear(g, shape, {{0.0f, dk(c, 0.22f).withAlpha(k)}, {0.20f, mix(c, Colour(0xffffffff), 0.30f).withAlpha(k)}, {0.46f, mix(c, Colour(0xffffffff), 0.06f).withAlpha(k)},
                              {0.80f, dk(c, 0.22f).withAlpha(k)}, {1.0f, dk(c, 0.58f).withAlpha(k)}}, down);
        auto core = s.reduced(s.getWidth() * 0.14f, s.getHeight() * 0.16f);
        glow(g, Shp::round(core, rr), mix(c, Colour(0xfffff6e0), 0.55f).withAlpha(0.42f * k), juce::jmin(core.getWidth(), core.getHeight()) * 0.48f);
    }
    const float e0 = k > 0 ? 0.30f : 0.42f, e1 = k > 0 ? 0.38f : 0.50f;
    fillLinear(g, shape, {{0.0f, bk(e0)}, {0.14f, bk(0)}, {0.86f, bk(0)}, {1.0f, bk(e1)}}, s.getWidth() >= s.getHeight() ? 0.0f : down);
    const float sh = juce::jlimit(0.5f, 1.6f, s.getHeight() * 0.17f);
    Rectangle<float> spec(s.getX() + rr * 0.9f + s.getWidth() * 0.05f, s.getY() + s.getHeight() * 0.13f, juce::jmax(0.5f, s.getWidth() - rr * 1.8f - s.getWidth() * 0.10f), sh);
    glow(g, Shp::round(spec, sh * 0.5f), wh(k > 0 ? 0.62f : 0.24f), sh * 0.42f);
}

// Additive bloom around a lit LED: light spilling onto what surrounds it.
inline void drawLedBloom(Graphics& g, Rectangle<float> s, Colour colour, float brightness) {
    using namespace lit;
    const float k = juce::jlimit(0.0f, 1.0f, brightness); if (k <= 0.0f) return;
    const float e = juce::jlimit(2.0f, 5.0f, juce::jmin(s.getWidth(), s.getHeight()) * 0.35f);
    glow(g, Shp::round(s.expanded(e), 2.0f + e), colour.withAlpha(0.10f * k), e * 1.6f);
    glow(g, Shp::round(s.expanded(e), 2.0f + e), colour.withAlpha(0.20f * k), e * 1.6f);
}

// levelDb / peakDb on a -48..0 dB scale. Lit segments use green / amber / red.
inline void drawLedLadder(Graphics& g, Rectangle<float> r, bool horizontal, float levelDb, float peakDb, int segments = 16) {
    using namespace lit;
    segments = juce::jmax(1, segments);
    const float span = horizontal ? r.getWidth() : r.getHeight();
    const float gap = juce::jlimit(1.5f, 3.0f, span / float(segments) * 0.17f);
    const float segLen = (span - gap * float(segments - 1)) / float(segments);
    const float peakIdx = std::floor(dbPosition(peakDb) * float(segments - 1) + 0.5f);
    const Shp ch = Shp::round(r.expanded(1.0f), 2.0f);
    fillShape(g, ch, Colour(0xff040505));
    struct Bloom { Rectangle<float> s; Colour c; float k; };
    std::vector<Bloom> blooms;
    for (int i = 0; i < segments; ++i) {
        const float db = -48.0f + 48.0f * float(i + 1) / float(segments);
        const bool on = levelDb >= db - 48.0f / float(segments) * 0.5f || float(i) == peakIdx;
        const Colour c = ladderColour(db);
        Rectangle<float> s = horizontal ? Rectangle<float>(r.getX() + float(i) * (segLen + gap), r.getY(), segLen, r.getHeight())
                                        : Rectangle<float>(r.getX(), r.getBottom() - float(i + 1) * segLen - float(i) * gap, r.getWidth(), segLen);
        const float k = !on ? 0.0f : (levelDb >= db - 1.5f || float(i) == peakIdx) ? 1.0f : 0.72f;
        drawLed(g, s, c, k);
        if (k > 0) blooms.push_back({s, c, k});
    }
    recessShadow(g, ch, 1.8f, 0.8f);
    for (auto& b : blooms) drawLedBloom(g, b.s, b.c, b.k);
}

// Two tall ladders with a shared dB scale between them (L left, R right).
inline void drawStereoLadder(Graphics& g, Rectangle<float> r, float dbL, float dbR, float peakDb) {
    using namespace lit;
    const float la = lightAngle();
    dropShadow(g, Shp::round(r, 4), 2.0f, 0.5f);
    fillLinear(g, Shp::round(r, 4), {{0.0f, Colour(0xff3a3f42)}, {0.35f, Colour(0xff15181a)}, {1.0f, Colour(0xff050606)}}, la);
    auto face = r.reduced(1.2f);
    fillLinear(g, Shp::round(face, 3), {{0.0f, Colour(0xff131617)}, {1.0f, Colour(0xff0a0b0c)}}, juce::MathConstants<float>::halfPi);
    recessShadow(g, Shp::round(face, 3), 2.0f, 0.6f);
    auto b = r.reduced(10, 8); const float barW = juce::jmin(30.0f, b.getWidth() * 0.26f);
    auto lb = Rectangle<float>(b.getX() + b.getWidth() * 0.5f - barW - 16, b.getY(), barW, b.getHeight());
    auto rb = Rectangle<float>(b.getX() + b.getWidth() * 0.5f + 16, b.getY(), barW, b.getHeight());
    drawLedLadder(g, lb, false, dbL, peakDb, 24);
    drawLedLadder(g, rb, false, dbR, peakDb, 24);
    const Colour ink(0xffa3abaf);
    for (int d : {0, -6, -12, -24, -36, -48}) {
        const float y = b.getBottom() - dbPosition(float(d)) * b.getHeight();
        txt(g, {b.getCentreX(), juce::jlimit(b.getY() + 5, b.getBottom() - 5, y)}, juce::String(d), ink, 7.0f);
    }
    txt(g, lb.withY(r.getBottom() - 11).withHeight(10).getCentre(), "L", ink, 8.0f);
    txt(g, rb.withY(r.getBottom() - 11).withHeight(10).getCentre(), "R", ink, 8.0f);
}

// ---- digital meter faces --------------------------------------------------
namespace detail {
using namespace lit;
// Dark display window let into the panel: lip shaded by the rig, a graded floor.
inline void screenWindow(Graphics& g, Rectangle<float> r, float radius, Colour top, Colour bottom) {
    fillLinear(g, Shp::round(r.expanded(1.1f), radius + 1.1f), {{0.0f, bk(0.80f)}, {0.5f, bk(0.32f)}, {1.0f, wh(0.20f)}}, lightAngle());
    fillLinear(g, Shp::round(r, radius), {{0.0f, top}, {1.0f, bottom}}, juce::MathConstants<float>::halfPi);
}
// Glass over a screenWindow: the panel's shadow on the contents, a reflection, a dark edge.
inline void screenGlass(Graphics& g, Rectangle<float> r, float radius) {
    recessShadow(g, Shp::round(r, radius), 3.2f, 0.85f);
    Rectangle<float> gl(r.getX() + 1, r.getY() + 1, juce::jmax(0.0f, r.getWidth() - 2), r.getHeight() * 0.55f);
    fillLinear(g, Shp::round(gl, juce::jmax(0.0f, radius - 1)), {{0.0f, wh(0.11f)}, {0.4f, wh(0.035f)}, {1.0f, wh(0)}}, juce::MathConstants<float>::halfPi);
    strokeShape(g, Shp::round(r, radius), bk(0.9f), 1.0f);
}
} // namespace detail

// Arc of LED wedges that grow toward full scale, in a channel cut to their outline. level01 on 0..1.
inline void drawSegmentArc(Graphics& g, Rectangle<float> r, float level01, Colour litColour) {
    using namespace lit;
    const int n = 32;
    const float rad = r.getHeight() * 1.05f, half = std::asin(juce::jmin(0.98f, (r.getWidth() * 0.46f) / rad));
    const Point<float> centre(r.getCentreX(), r.getBottom() + rad * 0.18f);
    auto at = [&](float a, float d) { return Point<float>(centre.x + std::sin(a) * d, centre.y - std::cos(a) * d); };
    const float rIn = rad * 0.70f; auto rOut = [&](float f) { return rad * (0.82f + 0.16f * f); };
    const float da = half / float(n);
    const Colour red(0xffe4412c), lc = opaque(litColour);

    auto band = [&](float grow) {
        const int m = 24; std::vector<Point<float>> pts;
        const float a0 = -half - grow / rad, a1 = half + grow / rad;
        for (int i = 0; i <= m; ++i) { const float t = float(i) / float(m); pts.push_back(at(a0 + (a1 - a0) * t, rOut(t) + grow)); }
        for (int i = m; i >= 0; --i) pts.push_back(at(a0 + (a1 - a0) * float(i) / float(m), rIn - grow));
        return pts;
    };
    const auto o = rig().shadowOffset(1.2f);
    { auto b = band(3.6f); for (auto& p : b) p += o; poly(g, b, wh(0.13f), wh(0.13f)); }
    poly(g, band(3.4f), Colour(0xff020303), Colour(0xff0b0d0e));
    strokePoly(g, band(3.4f), bk(0.85f), 0.8f, true);

    auto wedge = [&](int i, float& f) {
        f = (float(i) + 0.5f) / float(n);
        const float a = (-1 + 2 * f) * half, w = da * 0.62f;
        return std::vector<Point<float>>{at(a - w, rIn), at(a - w, rOut(f)), at(a + w, rOut(f)), at(a + w, rIn)};
    };
    auto colAt = [&](float f) { return f > 0.86f ? red : f > 0.72f ? mix(lc, Colour(0xffffc530), 0.75f) : lc; };
    auto on = [&](float f) { return f <= level01 + 0.5f / float(n); };
    float f;
    for (int i = 0; i < n; ++i) { auto pts = wedge(i, f); if (on(f)) continue; const Colour c = colAt(f); poly(g, pts, dk(c, 0.70f), dk(c, 0.84f)); }
    for (int i = 0; i < n; ++i) {
        wedge(i, f); if (!on(f)) continue;
        const float a = (-1 + 2 * f) * half, len = rOut(f) - rIn;
        glow(g, Shp::oval(boxAt(len * 0.9f + 8, len * 0.9f + 8, at(a, (rIn + rOut(f)) * 0.5f))), colAt(f).withAlpha(0.085f), len * 0.45f + 4.0f);
    }
    for (int i = 0; i < n; ++i) {
        auto pts = wedge(i, f); if (!on(f)) continue;
        const Colour c = colAt(f); const float a = (-1 + 2 * f) * half;
        poly(g, pts, mix(c, Colour(0xffffffff), 0.22f), dk(c, 0.20f));
        line(g, at(a, rIn + 1.2f), at(a, rOut(f) - 1.2f), mix(c, Colour(0xfffff6e0), 0.6f).withAlpha(0.75f), juce::jmax(0.5f, rad * da * 0.34f));
        line(g, at(a - da * 0.3f, rOut(f) - 1.0f), at(a + da * 0.3f, rOut(f) - 1.0f), wh(0.5f), 0.6f);
    }
    for (int i = 0; i < n; ++i) strokePoly(g, wedge(i, f), bk(0.55f), 0.5f, true);
}

// Fan of vector strokes on a phosphor screen behind glass.
inline void drawVectorFan(Graphics& g, Rectangle<float> r, float level01, Colour litColour) {
    using namespace lit;
    const Colour c = opaque(litColour);
    detail::screenWindow(g, r, 4.0f, mix(Colour(0xff04120b), c, 0.03f), Colour(0xff010503));
    clipTo(g, Shp::round(r, 4.0f));
    auto b = r.reduced(8);
    glow(g, Shp::oval({b.getX() - b.getWidth() * 0.1f, b.getY() + b.getHeight() * 0.35f, b.getWidth() * 1.2f, b.getHeight()}), c.withAlpha(0.07f), b.getHeight() * 0.4f);
    for (int i = 1; i < 4; ++i) { const float y = std::round(b.getBottom() - b.getHeight() * float(i) / 4.0f) + 0.25f; line(g, {r.getX(), y}, {r.getRight(), y}, c.withAlpha(0.07f), 0.5f); }
    const int n = 26;
    auto ln = [&](int i, float& x, float& hgt, float& f) { f = float(i) / float(n - 1); x = b.getX() + f * b.getWidth(); hgt = b.getHeight() * (0.28f + 0.72f * f); };
    auto on = [&](float f) { return f <= level01 + 0.5f / float(n); };
    float x, hgt, f;
    for (int i = 0; i < n; ++i) { ln(i, x, hgt, f); if (!on(f)) line(g, {x, b.getBottom()}, {x, b.getBottom() - hgt}, c.withAlpha(0.13f), 1.1f); }
    for (int i = 0; i < n; ++i) {
        ln(i, x, hgt, f); if (!on(f)) continue;
        Rectangle<float> bar(x - 0.6f, b.getBottom() - hgt, 1.2f, hgt);
        glow(g, Shp::round(bar.expanded(4.5f), 5.0f), c.withAlpha(0.085f), 4.5f);
        glow(g, Shp::round(bar.expanded(1.6f), 2.2f), c.withAlpha(0.16f), 1.6f);
    }
    for (int i = 0; i < n; ++i) {
        ln(i, x, hgt, f); if (!on(f)) continue;
        Rectangle<float> bar(x - 0.65f, b.getBottom() - hgt, 1.3f, hgt);
        fillLinear(g, Shp::round(bar, 0.65f), {{0.0f, mix(c, Colour(0xffffffff), 0.75f)}, {0.12f, mix(c, Colour(0xffffffff), 0.35f)}, {1.0f, dk(c, 0.12f)}}, juce::MathConstants<float>::halfPi);
        glow(g, Shp::oval(boxAt(4.4f, 4.4f, {x, b.getBottom() - hgt})), mix(c, Colour(0xffffffff), 0.6f).withAlpha(0.55f), 1.8f);
    }
    line(g, {b.getX() - 2, b.getBottom() + 1.5f}, {b.getRight() + 2, b.getBottom() + 1.5f}, c.withAlpha(0.45f), 0.7f);
    popClip(g);
    detail::screenGlass(g, r, 4.0f);
    strokeShape(g, Shp::round(r.reduced(0.5f), 3.5f), c.withAlpha(0.16f), 1.0f);
}

// ---- 7-segment readout ----------------------------------------------------
// Draws text made of digits, '-', '+', ' ' and '.' (a dot attaches to the digit before it). Pointed, slanted
// segments with a hot core and bloom; with `background` it sits in a recessed window behind glass and shows the
// unlit segments as ghosts. `lit`'s alpha dims the whole display.
namespace detail {
struct SegPoly { std::vector<Point<float>> pts; Point<float> a, b; };
inline SegPoly segPoly(char s, float x0, float x1, float y0, float y1, float th) {
    const float ym = (y0 + y1) * 0.5f, gp = th * 0.16f, h = th * 0.5f;
    auto skew = [&](Point<float> p) { return Point<float>(p.x + (ym - p.y) * 0.09f, p.y); };
    auto bar = [&](Point<float> a, Point<float> b) {
        const bool horiz = std::abs(a.y - b.y) < 1e-3f;
        std::vector<Point<float>> pts;
        if (horiz) pts = {a, {a.x + h, a.y - h}, {b.x - h, b.y - h}, b, {b.x - h, b.y + h}, {a.x + h, a.y + h}};
        else       pts = {a, {a.x + h, a.y + h}, {b.x + h, b.y - h}, b, {b.x - h, b.y - h}, {a.x - h, a.y + h}};
        for (auto& p : pts) p = skew(p);
        return SegPoly{pts, skew(a), skew(b)};
    };
    const float xl = x0 + h, xr = x1 - h, yt = y0 + h, yb = y1 - h;
    switch (s) {
        case 'a': return bar({xl + gp, yt}, {xr - gp, yt});   case 'b': return bar({xr, yt + gp}, {xr, ym - gp});
        case 'c': return bar({xr, ym + gp}, {xr, yb - gp});   case 'd': return bar({xl + gp, yb}, {xr - gp, yb});
        case 'e': return bar({xl, ym + gp}, {xl, yb - gp});   case 'f': return bar({xl, yt + gp}, {xl, ym - gp});
        default:  return bar({xl + gp, ym}, {xr - gp, ym});
    }
}
} // namespace detail

inline void drawSevenSegment(Graphics& g, Rectangle<float> r, const juce::String& text, Colour litColour, bool background = true) {
    using namespace lit;
    static const char* segs[10] = {"abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg", "abcdfg"};
    juce::String cells; std::vector<bool> dots;
    for (auto ch : text) {
        if (ch == '.' && !dots.empty()) { dots.back() = true; continue; }
        cells += ch; dots.push_back(false);
    }
    const int n = cells.length(); if (n == 0) return;
    const float dim = litColour.getFloatAlpha(); const Colour c = opaque(litColour);
    if (background) {
        detail::screenWindow(g, r, 3.0f, mix(Colour(0xff0c0a08), c, 0.035f), Colour(0xff040303));
        glow(g, Shp::oval(r.reduced(r.getWidth() * 0.06f, r.getHeight() * 0.12f)), c.withAlpha(0.05f * dim), r.getHeight() * 0.35f);
    }
    auto b = r.reduced(r.getHeight() * 0.16f);
    const float ch = b.getHeight(), cw = juce::jmin(b.getWidth() / float(n), ch * 0.88f);
    b = Rectangle<float>(b.getCentreX() - cw * float(n) * 0.5f, b.getY(), cw * float(n), b.getHeight());
    const float th = juce::jmax(0.8f, juce::jmin(cw * 0.17f, ch * 0.115f));
    std::vector<detail::SegPoly> onSegs; std::vector<Point<float>> dotAt;
    for (int i = 0; i < n; ++i) {
        const float dw = cw * 0.76f, xc = b.getX() + (float(i) + 0.5f) * cw, x0 = xc - dw * 0.5f, x1 = xc + dw * 0.5f;
        const auto cc = cells[i];
        juce::String on = (cc >= '0' && cc <= '9') ? segs[cc - '0'] : (cc == '-' || cc == '+') ? "g" : "";
        for (char s : {'a', 'b', 'c', 'd', 'e', 'f', 'g'}) {
            auto seg = detail::segPoly(s, x0, x1, b.getY(), b.getBottom(), th);
            if (on.containsChar(s)) onSegs.push_back(seg);
            else if (background) poly(g, seg.pts, c.withAlpha(0.07f * dim), c.withAlpha(0.07f * dim));
        }
        const Point<float> dp(x1 + th * 0.75f - ch * 0.045f, b.getBottom() - th * 0.5f);
        if (dots[size_t(i)]) dotAt.push_back(dp);
        else if (background) { g.setColour(c.withAlpha(0.07f * dim)); g.fillEllipse(boxAt(th, th, dp)); }
    }
    const float halo = juce::jlimit(1.6f, 7.0f, th * 2.2f);
    for (auto& s : onSegs) {
        Rectangle<float> bb(std::min(s.a.x, s.b.x), std::min(s.a.y, s.b.y), std::abs(s.a.x - s.b.x), std::abs(s.a.y - s.b.y));
        glow(g, Shp::round(bb.expanded(halo + th * 0.3f), halo + th * 0.3f), c.withAlpha(0.15f * dim), halo);
        glow(g, Shp::round(bb.expanded(th * 1.1f), th * 1.1f), c.withAlpha(0.20f * dim), th * 0.9f);
    }
    for (auto& p : dotAt) glow(g, Shp::oval(boxAt(th * 4.6f, th * 4.6f, p)), c.withAlpha(0.30f * dim), th * 1.8f);
    const Colour core = mix(c, Colour(0xfffff4e0), 0.62f);
    for (auto& s : onSegs) {
        poly(g, s.pts, mix(c, Colour(0xffffffff), 0.16f).withAlpha(dim), dk(c, 0.10f).withAlpha(dim));
        const auto d = s.b - s.a; const float inset = th * 0.45f / juce::jmax(1e-3f, std::sqrt(d.x * d.x + d.y * d.y));
        line(g, s.a + d * inset, s.b - d * inset, core.withAlpha(0.80f * dim), juce::jmax(0.45f, th * 0.32f));
    }
    for (auto& p : dotAt) {
        g.setColour(mix(c, Colour(0xffffffff), 0.16f).withAlpha(dim)); g.fillEllipse(boxAt(th * 1.15f, th * 1.15f, p));
        g.setColour(core.withAlpha(0.8f * dim)); g.fillEllipse(boxAt(th * 0.5f, th * 0.5f, p));
    }
    if (background) detail::screenGlass(g, r, 3.0f);
}

// ---- lamp and lit key -------------------------------------------------------
enum class LampBezel { Chrome, Black, Flush };

// Jewel lamp: a lens of `radius` at `c` with radial shading and a rim, a hot core and bloom when lit
// (`brightness` 0..1), a dull translucent lens when off. The bezel adds about 0.36 x radius around the lens.
inline void drawJewel(Graphics& g, Point<float> c, float radius, Colour colour, float brightness, LampBezel bezel = LampBezel::Chrome) {
    using namespace lit;
    const float la = lightAngle(), k = juce::jlimit(0.0f, 1.0f, brightness), r = juce::jmax(1.0f, radius);
    const Colour col = opaque(colour), white = Colour(0xffffffff);
    if (k > 0.0f) {
        glow(g, Shp::oval(ellAt(c, r * 3.2f, r * 3.2f)), col.withAlpha(0.16f * k), r * 2.3f);
        glow(g, Shp::oval(ellAt(c, r * 3.6f, r * 3.6f)), col.withAlpha(0.16f * k), r * 2.8f);
    }
    const float ring = bezel == LampBezel::Flush ? r * 1.08f : r * 1.38f;
    dropShadow(g, Shp::oval(ellAt(c, ring, ring)), juce::jlimit(1.0f, 4.0f, r * 0.3f), 0.65f);
    if (bezel == LampBezel::Chrome) {
        const auto rr = ellAt(c, ring, ring);
        const Stops chrome = {{0.0f, Colour(0xffe2e6e8)}, {0.18f, Colour(0xff6c7276)}, {0.40f, Colour(0xfff6f8f9)}, {0.62f, Colour(0xff474c50)}, {0.84f, Colour(0xffc6cbce)}, {1.0f, Colour(0xffe2e6e8)}};
        // conic sweep from -2.3 rad (0 = 3 o'clock, clockwise), drawn as thin wedges
        const int wedges = 40; const float start = -2.3f + juce::MathConstants<float>::halfPi, step = juce::MathConstants<float>::twoPi / float(wedges);
        for (int i = 0; i < wedges; ++i) {
            juce::Path p; p.addPieSegment(rr, start + step * float(i) - 0.01f, start + step * float(i + 1) + 0.01f, 0.0f);
            g.setColour(sampleStops(chrome, (float(i) + 0.5f) / float(wedges))); g.fillPath(p);
        }
        fillLinear(g, Shp::oval(rr), {{0.0f, wh(0.30f)}, {0.5f, wh(0)}, {1.0f, bk(0.45f)}}, la);
        strokeShape(g, Shp::oval(rr), bk(0.45f), 0.6f);
        fillLinear(g, Shp::oval(ellAt(c, r * 1.14f, r * 1.14f)), {{0.0f, Colour(0xff2a2d30)}, {0.6f, Colour(0xff6a7074)}, {1.0f, Colour(0xffd5d9db)}}, la);
    } else if (bezel == LampBezel::Black) {
        fillLinear(g, Shp::oval(ellAt(c, ring, ring)), {{0.0f, Colour(0xff4a4d50)}, {0.35f, Colour(0xff1a1b1d)}, {1.0f, Colour(0xff060606)}}, la);
        fillLinear(g, Shp::oval(ellAt(c, r * 1.14f, r * 1.14f)), {{0.0f, Colour(0xff050505)}, {1.0f, Colour(0xff2e3033)}}, la);
    }
    const auto lens = Shp::oval(ellAt(c, r, r));
    fillShape(g, Shp::oval(ellAt(c, r * 1.04f, r * 1.04f)), Colour(0xff060606));
    if (k < 1.0f)
        fillRadial(g, lens, {{0.0f, mix(dk(col, 0.45f), Colour(0xff3a3a3a), 0.15f)}, {0.45f, dk(col, 0.68f)}, {1.0f, dk(col, 0.88f)}}, {c.x + r * 0.28f, c.y + r * 0.34f});
    if (k > 0.0f) {
        const Colour hot = mix(col, Colour(0xfffff2dc), 0.82f);
        fillRadial(g, lens, {{0.0f, hot.withAlpha(k)}, {0.2f, mix(col, hot, 0.4f).withAlpha(k)}, {0.48f, col.withAlpha(k)}, {0.8f, dk(col, 0.32f).withAlpha(k)}, {1.0f, dk(col, 0.6f).withAlpha(k)}},
                   {c.x + r * 0.04f, c.y + r * 0.06f});
    }
    strokeShape(g, Shp::oval(ellAt(c, r * 0.68f, r * 0.68f)), k > 0 ? mix(col, white, 0.5f).withAlpha(0.22f * k) : wh(0.06f), juce::jmax(0.5f, r * 0.05f));
    strokeShape(g, Shp::oval(ellAt(c, r * 0.96f, r * 0.96f)), dk(col, 0.75f).withAlpha(0.7f), juce::jmax(0.6f, r * 0.08f));
    if (k > 0.0f) glow(g, Shp::oval(ellAt(c, r * 0.42f, r * 0.42f)), mix(col, Colour(0xfffff2dc), 0.7f).withAlpha(0.55f * k), r * 0.4f);
    glow(g, Shp::oval(ellAt({c.x - r * 0.12f, c.y - r * 0.38f}, r * 0.62f, r * 0.40f)), wh(0.16f), r * 0.35f);
    glow(g, Shp::oval(ellAt({c.x - r * 0.33f, c.y - r * 0.44f}, r * 0.30f, r * 0.17f)), wh(0.85f), r * 0.12f);
    glow(g, Shp::oval(ellAt({c.x + r * 0.42f, c.y + r * 0.48f}, r * 0.22f, r * 0.13f)), wh(0.18f), r * 0.1f);
    if (k > 0.0f) glow(g, Shp::oval(ellAt(c, r * 2.0f, r * 2.0f)), col.withAlpha(0.30f * k), r * 1.3f);
}
inline void drawLamp(Graphics& g, Point<float> c, float radius, Colour colour, bool on) { drawJewel(g, c, radius, colour, on ? 1.0f : 0.0f, LampBezel::Chrome); }

// Lit key: a translucent cap lit from inside, light spilling out at its edges and into the well around it.
// Unlit, a milky cap tinted by the lamp colour.
inline void drawLitKey(Graphics& g, Rectangle<float> r, const juce::String& text, bool isLit, Colour lampIn, bool down = false) {
    using namespace lit;
    const float la = lightAngle(), downAng = juce::MathConstants<float>::halfPi;
    const Colour lamp = opaque(lampIn), white = Colour(0xffffffff);
    auto well = r.expanded(1.5f); const float wr = 3.2f;
    fillLinear(g, Shp::round(well.expanded(0.8f), wr + 0.8f), {{0.0f, Colour(0xff050606)}, {1.0f, Colour(0xff3a3f42)}}, la);
    fillShape(g, Shp::round(well, wr), Colour(0xff0b0d0e));
    if (isLit) glow(g, Shp::round(well, wr), lamp.withAlpha(0.55f), 3.0f);
    recessShadow(g, Shp::round(well, wr), 2.5f, 0.8f);
    const float height = down ? 0.8f : 2.6f;
    auto face = r.reduced(0.6f).translated(0, down ? 0.8f : 0.0f);
    const float fr = 2.2f; const Shp cap = Shp::round(face, fr);
    dropShadow(g, cap, height, isLit ? 0.35f : 0.7f);
    if (isLit) {
        fillShape(g, cap, dk(lamp, 0.38f));
        const Colour hot = mix(lamp, Colour(0xfffff6e6), 0.7f);
        // feathered radial: approximate by the radial fill clipped to the cap, edges kept hot
        g.saveState(); g.reduceClipRegion(cap.expanded(-1.0f).path());
        fillRadial(g, cap.expanded(-1.0f), {{0.0f, hot}, {0.35f, mix(lamp, hot, 0.35f)}, {0.75f, lamp}, {1.0f, dk(lamp, 0.1f)}}, face.getCentre(), face.getWidth() * 0.6f, face.getHeight() * 0.75f,
                   {face.getCentreX(), face.getY() + face.getHeight() * 0.42f});
        g.restoreState();
        strokeShape(g, Shp::round(face.reduced(0.4f), fr), mix(lamp, white, 0.75f).withAlpha(0.9f), 0.8f);
    } else {
        const Colour base = mix(dk(lamp, 0.5f), Colour(0xff767a78), 0.5f);
        fillLinear(g, cap, {{0.0f, br(base, 0.18f)}, {0.5f, base}, {1.0f, dk(base, 0.35f)}}, downAng);
        Rectangle<float> low(face.getX() + 1.5f, face.getY() + face.getHeight() * 0.5f, face.getWidth() - 3.0f, face.getHeight() * 0.45f);
        glow(g, Shp::round(low, fr), lamp.withAlpha(0.14f), low.getHeight() * 0.45f);
        strokeShape(g, Shp::round(face.reduced(0.4f), fr), wh(0.10f), 0.7f);
    }
    Rectangle<float> gloss(face.getX() + 1.0f, face.getY() + 0.8f, face.getWidth() - 2.0f, face.getHeight() * 0.45f);
    fillLinear(g, Shp::round(gloss, fr), {{0.0f, wh(isLit ? 0.38f : 0.22f)}, {1.0f, wh(0)}}, downAng);
    line(g, {face.getX() + fr, face.getY() + 0.7f}, {face.getRight() - fr, face.getY() + 0.7f}, wh(0.55f), 0.6f);
    strokeShape(g, cap, bk(0.45f), 0.6f);
    txt(g, face.getCentre(), text, isLit ? dk(lamp, 0.78f) : Colour(0xffe2e6e3).withAlpha(0.85f), face.getHeight() * 0.40f);
    if (isLit) {
        glow(g, Shp::round(face.expanded(5.0f), fr + 5.0f), lamp.withAlpha(0.10f), 7.0f);
        glow(g, Shp::round(face.expanded(5.0f), fr + 5.0f), lamp.withAlpha(0.22f), 7.0f);
    }
}

// ---- faders ------------------------------------------------------------------
enum class FaderCap { Oval, Console };

// Fader with an explicit orientation. prop01 1 is the top (vertical) or the right end. `scale` prints ticks on both
// sides of the slot with numerals on one side. `panel` is the colour behind it: the scale is printed in dark ink on
// a pale panel and pale ink on a dark one.
inline void drawFaderOn(Graphics& g, Rectangle<float> r, float prop01, FaderCap cap, Colour capColour, bool vertical, bool scale, Colour panel) {
    using namespace lit;
    const bool pale = panel.getPerceivedBrightness() > 0.5f;
    const float len = vertical ? r.getHeight() : r.getWidth(), wid = vertical ? r.getWidth() : r.getHeight();
    auto rect = [&](float a0, float c0, float la, float lc) { return vertical ? Rectangle<float>(r.getX() + c0, r.getY() + a0, lc, la) : Rectangle<float>(r.getX() + a0, r.getY() + c0, la, lc); };
    auto pnt = [&](float a, float c) { return vertical ? Point<float>(r.getX() + c, r.getY() + a) : Point<float>(r.getX() + a, r.getY() + c); };
    float capAlong, capAcross;
    if (cap == FaderCap::Console) { capAlong = juce::jmax(8.0f, wid * 0.62f); capAcross = wid * 0.86f; }
    else { capAlong = juce::jmax(7.0f, wid * 0.52f); capAcross = wid * 0.74f; }
    const float t0 = capAlong * 0.5f + 1.0f, t1 = juce::jmax(t0, len - capAlong * 0.5f - 1.0f);
    auto along = [&](float p) { p = juce::jlimit(0.0f, 1.0f, p); return vertical ? t1 - (t1 - t0) * p : t0 + (t1 - t0) * p; };
    const float mid = wid * 0.5f;

    const float sw = juce::jlimit(3.0f, 6.0f, wid * 0.16f);
    const auto slot = rect(t0 - sw * 0.5f, mid - sw * 0.5f, (t1 - t0) + sw, sw);
    const float sr = sw * 0.5f;
    strokeShape(g, Shp::round(slot.translated(0.4f, 0.8f), sr), wh(pale ? 0.75f : 0.26f), 0.9f);
    if (pale) strokeShape(g, Shp::round(slot.translated(-0.3f, -0.6f), sr), bk(0.30f), 0.8f);
    fillShape(g, Shp::round(slot, sr), Colour(0xff030404));
    recessShadow(g, Shp::round(slot, sr), 2.5f, 0.9f);
    if (vertical) line(g, {slot.getCentreX(), slot.getY() + sr}, {slot.getCentreX(), slot.getBottom() - sr}, wh(0.06f), 0.7f);
    else line(g, {slot.getX() + sr, slot.getCentreY()}, {slot.getRight() - sr, slot.getCentreY()}, wh(0.06f), 0.7f);

    if (scale) {
        const Colour ink = pale ? Colour(0xff16191b).withAlpha(0.88f) : Colour(0xffd3dade).withAlpha(0.86f);
        const float tw = pale ? 0.8f : 0.75f, gp = sw * 0.5f + wid * 0.06f, maj = wid * 0.15f, mn = wid * 0.08f, ns = juce::jlimit(5.0f, 8.0f, wid * 0.19f);
        for (int i = 0; i <= 10; ++i) {
            const float a = along(float(i) / 10.0f), l = i % 2 == 0 ? maj : mn;
            line(g, pnt(a, mid + gp), pnt(a, mid + gp + l), ink, tw);
            line(g, pnt(a, mid - gp), pnt(a, mid - gp - l), ink, tw);
            if (i % 2 == 0 && wid >= 20.0f) txt(g, pnt(a, mid - gp - maj - ns * 0.62f), juce::String(i), ink, ns);
        }
    }

    const float pos = along(prop01);
    const auto cr = rect(pos - capAlong * 0.5f, mid - capAcross * 0.5f, capAlong, capAcross);
    const Colour base = opaque(capColour);
    const float nLo[3] = {vertical ? 0.0f : -0.75f, vertical ? -0.75f : 0.0f, 0.66f}, nHi[3] = {vertical ? 0.0f : 0.75f, vertical ? 0.75f : 0.0f, 0.66f};
    const float nL[3] = {vertical ? -0.75f : 0.0f, vertical ? 0.0f : -0.75f, 0.66f}, nR[3] = {vertical ? 0.75f : 0.0f, vertical ? 0.0f : 0.75f, 0.66f};
    const float alongAng = vertical ? juce::MathConstants<float>::halfPi : 0.0f, acrossAng = vertical ? 0.0f : juce::MathConstants<float>::halfPi;
    const Point<float> off = vertical ? Point<float>(0, 0.6f) : Point<float>(0.6f, 0);
    if (cap == FaderCap::Console) {
        const float rad = juce::jlimit(1.2f, 3.0f, juce::jmin(capAcross, capAlong) * 0.12f);
        dropShadow(g, Shp::round(cr, rad), 7.0f, 0.55f);
        dropShadow(g, Shp::round(cr, rad), 1.8f, 0.45f);
        const Colour lo = facet(base, nLo[0], nLo[1], nLo[2], 2.2f), hi = facet(base, nHi[0], nHi[1], nHi[2], 2.2f), top = base;
        fillLinear(g, Shp::round(cr, rad), {{0.0f, lo}, {0.22f, mix(lo, top, 0.3f)}, {0.25f, br(top, 0.04f)}, {0.75f, dk(top, 0.08f)}, {0.78f, mix(hi, top, 0.3f)}, {1.0f, hi}}, alongAng);
        const Colour sl = facet(base, nL[0], nL[1], nL[2], 2.2f), sr2 = facet(base, nR[0], nR[1], nR[2], 2.2f);
        fillLinear(g, Shp::round(cr, rad), {{0.0f, sl}, {0.07f, sl.withAlpha(0.0f)}, {0.93f, sr2.withAlpha(0.0f)}, {1.0f, sr2}}, acrossAng);
        const float spanA = capAlong * 0.5f, rf = capAcross * 0.14f, rt = capAcross * 0.86f;
        for (int j : {-3, -2, -1, 1, 2, 3}) {
            const float a = pos + spanA * 0.15f * float(j);
            const auto p0 = pnt(a, mid - capAcross * 0.5f + rf), p1 = pnt(a, mid - capAcross * 0.5f + rt);
            line(g, p0, p1, bk(0.28f), 0.6f); line(g, p0 + off, p1 + off, wh(0.30f), 0.5f);
        }
        const auto p0 = pnt(pos, mid - capAcross * 0.5f + capAcross * 0.06f), p1 = pnt(pos, mid + capAcross * 0.5f - capAcross * 0.06f);
        const Point<float> o2 = vertical ? Point<float>(0, 0.7f) : Point<float>(0.7f, 0);
        line(g, p0 + o2, p1 + o2, bk(0.45f), 1.2f);
        line(g, p0, p1, base.getPerceivedBrightness() > 0.6f ? Colour(0xff1a1c1d) : Colour(0xffffffff), 1.1f);
        strokeShape(g, Shp::round(cr.reduced(0.5f), rad), wh(0.18f), 0.6f);
        strokeShape(g, Shp::round(cr, rad), bk(0.55f), 0.6f);
    } else {
        const float rad = juce::jmin(capAlong, capAcross) * 0.5f;
        dropShadow(g, Shp::round(cr, rad), 6.0f, 0.55f);
        dropShadow(g, Shp::round(cr, rad), 1.6f, 0.45f);
        fillRadial(g, Shp::round(cr, rad), {{0.0f, mix(base, Colour(0xffffffff), 0.55f)}, {0.35f, br(base, 0.08f)}, {0.8f, dk(base, 0.28f)}, {1.0f, dk(base, 0.5f)}},
                   {cr.getX() + cr.getWidth() * 0.38f, cr.getY() + cr.getHeight() * 0.32f});
        const auto p0 = pnt(pos, mid - capAcross * 0.5f + rad * 0.7f), p1 = pnt(pos, mid + capAcross * 0.5f - rad * 0.7f);
        line(g, p0 + off, p1 + off, wh(0.4f), 0.8f);
        line(g, p0, p1, base.getPerceivedBrightness() > 0.6f ? Colour(0xff2a2c2d) : Colour(0xffffffff), 1.0f);
        Rectangle<float> hl(cr.getX() + cr.getWidth() * 0.18f, cr.getY() + cr.getHeight() * 0.12f, cr.getWidth() * 0.38f, cr.getHeight() * 0.26f);
        glow(g, Shp::round(hl, hl.getHeight() * 0.5f), wh(0.55f), hl.getHeight() * 0.45f);
        strokeShape(g, Shp::round(cr, rad), bk(0.45f), 0.6f);
    }
}
inline void drawFader(Graphics& g, Rectangle<float> r, float prop01, FaderCap cap, Colour capColour, bool vertical, bool scale) {
    drawFaderOn(g, r, prop01, cap, capColour, vertical, scale, Colour(0xff14181b));
}
// Fader in a slot; vertical when `r` is taller than wide, with a printed 0..10 scale.
inline void drawSlotFader(Graphics& g, Rectangle<float> r, float prop01, FaderCap cap, Colour capColour = Colour(0xfff0f0ee)) {
    drawFader(g, r, prop01, cap, capColour, r.getHeight() > r.getWidth(), true);
}
} // namespace goodlookinui::juce_adapter::meters
