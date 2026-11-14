// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <algorithm>
#include <map>
#include <mutex>
#include <memory>
#include <vector>

// Rack-hardware surfaces: a brushed anodised front panel, rack ears with slots, and the small
// rectangular LED keys used on 19-inch units. Everything is procedural and stateless.
namespace goodlookinui::juce_adapter::hardware {
using juce::Colour;
using juce::Graphics;
using juce::Point;
using juce::Rectangle;

inline unsigned hash(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

// Fine horizontal brushing over a vertical gradient, with a lit top edge and a dark bottom edge.
inline void drawBrushedPanel(Graphics& g, Rectangle<float> r, Colour top = Colour(0xff26272a), Colour bottom = Colour(0xff101112), unsigned seed = 7) {
    g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRect(r);
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    for (int y = int(r.getY()); y < int(r.getBottom()); ++y) {
        const unsigned h = hash(unsigned(y) * 2654435761u + seed);
        const float a = float(h & 255) / 255.0f;
        g.setColour((h & 256) ? Colour::fromFloatRGBA(1, 1, 1, 0.008f + 0.02f * a) : Colour::fromFloatRGBA(0, 0, 0, 0.03f + 0.06f * a));
        // each scratch covers a random stretch, so the brushing is not a uniform stripe
        const float x0 = r.getX() + float((h >> 9) & 511) / 511.0f * r.getWidth() * 0.4f;
        const float x1 = x0 + r.getWidth() * (0.3f + 0.7f * float((h >> 18) & 255) / 255.0f);
        g.drawHorizontalLine(y, x0 - r.getWidth() * 0.2f, x1);
    }
    g.setGradientFill(juce::ColourGradient(Colour(0x14ffffff), r.getX(), r.getY(), Colour(0x00ffffff), r.getX(), r.getY() + r.getHeight() * 0.18f, false));
    g.fillRect(r.withHeight(r.getHeight() * 0.18f));
    g.setColour(Colour(0x40ffffff)); g.drawHorizontalLine(int(r.getY()), r.getX(), r.getRight());
    g.setColour(Colour(0xa0000000)); g.drawHorizontalLine(int(r.getBottom()) - 1, r.getX(), r.getRight());
    g.restoreState();
}


// ---------------------------------------------------------------------------------------------
// Lit hardware (3D 2.0): one studio light for every plate, bevel, screw and shadow, matching
// GoodLookinDri's hardware.rs. Surface grain comes from small seamless hash textures.
// ---------------------------------------------------------------------------------------------

inline constexpr int kFinishFlat = 0, kFinishBrushed = 1, kFinishWood = 2, kFinishScanlines = 3, kFinishHazard = 4,
                     kFinishGrid = 5, kFinishNeonEdge = 6, kFinishHorizon = 7, kFinishAnodised = 8, kFinishPowder = 9,
                     kFinishHammertone = 10, kFinishSatinBlack = 11, kFinishCount = 12;

// Light rig: key 20 degrees left of up, 65 degrees above the panel.
struct LightRig {
    float kx, ky, kz, softness = 0.27f;
    static LightRig studio() {
        const float az = 20.0f * juce::MathConstants<float>::pi / 180.0f, el = 65.0f * juce::MathConstants<float>::pi / 180.0f;
        return {-std::sin(az) * std::cos(el), -std::cos(az) * std::cos(el), std::sin(el)};
    }
    Point<float> screenDir() const {
        const float l = std::sqrt(kx * kx + ky * ky);
        return l < 1e-6f ? Point<float>(0, 0) : Point<float>(kx / l, ky / l);
    }
    Point<float> shadowOffset(float h) const { const float k = h / std::max(kz, 0.05f); return {-kx * k, -ky * k}; }
    float shadowSigma(float h) const { return std::max(h * softness, 0.5f); }
};

namespace detail {
inline float& deviceScaleRef() { static float s = 2.0f; return s; }
inline float unit(unsigned h) { return float(h >> 8) / 16777216.0f; }
inline float snorm(unsigned h) { return unit(h) * 2.0f - 1.0f; }
inline unsigned hash2(unsigned x, unsigned y, unsigned seed) { return hash(x * 0x9e3779b1u ^ hash(y + seed * 0x85ebca77u)); }
inline float tri(unsigned x, unsigned y, unsigned seed) { return unit(hash2(x, y, seed)) + unit(hash2(x, y, seed ^ 0x5bd1e995u)) - 1.0f; }
inline unsigned posSeed(float x, float y) { return hash(unsigned(int(std::round(x * 8.0f))) ^ (hash(unsigned(int(std::round(y * 8.0f)))) + 0x27d4eb2fu)); }
inline float valueNoise(float x, float y, int period, unsigned seed) {
    const float xi = std::floor(x), yi = std::floor(y), fx = x - xi, fy = y - yi;
    auto s = [](float t) { return t * t * (3.0f - 2.0f * t); };
    const float sx = s(fx), sy = s(fy);
    auto md = [&](int i) { return unsigned(((i % period) + period) % period); };
    auto p = [&](int i, int j) { return snorm(hash2(md(i), md(j), seed)); };
    const int i = int(xi), j = int(yi);
    const float a = p(i, j) + (p(i + 1, j) - p(i, j)) * sx, b = p(i, j + 1) + (p(i + 1, j + 1) - p(i, j + 1)) * sx;
    return a + (b - a) * sy;
}
inline juce::uint32 signedPx(float v) {
    const juce::uint32 a = juce::uint32(std::round(std::min(std::abs(v), 1.0f) * 255.0f));
    return v >= 0 ? ((a << 24) | (a << 16) | (a << 8) | a) : (a << 24);
}
inline std::vector<float> blurWrapNorm(const std::vector<float>& src, int len) {
    const int n = int(src.size());
    std::vector<float> out(size_t(n), 0.0f);
    len = std::max(1, std::min(len, n));
    float acc = 0; for (int k = 0; k < len; ++k) acc += src[size_t(k % n)];
    for (int x = 0; x < n; ++x) { out[size_t((x + len / 2) % n)] = acc / float(len); acc += src[size_t((x + len) % n)] - src[size_t(x)]; }
    float var = 0; for (float v : out) var += v * v;
    var /= float(n);
    const float k = 1.0f / std::max(std::sqrt(var), 1e-6f);
    for (float& v : out) v *= k;
    return out;
}
inline Colour mixc(Colour a, Colour b, float t) { return a.interpolatedWith(b, juce::jlimit(0.0f, 1.0f, t)); }
inline Colour darkerBy(Colour c, float amt) { return Colour::fromFloatRGBA(c.getFloatRed() * (1 - amt), c.getFloatGreen() * (1 - amt), c.getFloatBlue() * (1 - amt), c.getFloatAlpha()); }
inline Colour brighterBy(Colour c, float amt) {
    auto f = [&](float v) { return v + amt * (1 - v); };
    return Colour::fromFloatRGBA(f(c.getFloatRed()), f(c.getFloatGreen()), f(c.getFloatBlue()), c.getFloatAlpha());
}
inline Colour white(float a) { return Colour::fromFloatRGBA(1, 1, 1, juce::jlimit(0.0f, 1.0f, a)); }
inline Colour black(float a) { return Colour::fromFloatRGBA(0, 0, 0, juce::jlimit(0.0f, 1.0f, a)); }
inline float lum(Colour c) {
    const float r = c.getFloatRed(), gr = c.getFloatGreen(), b = c.getFloatBlue();
    return std::sqrt(r * r * 0.241f + gr * gr * 0.691f + b * b * 0.068f);
}

enum class Tex { Grain = 0, Brushed, Speckle, Hammer, Wood };

// Seamless tile at `scale` texels per unit, built once per (kind, scale).
inline juce::Image buildTexture(Tex t, float scale) {
    const auto l = LightRig::studio().screenDir();
    const float k = scale / 2.0f, fine = std::min(0.72f + 0.28f * k, 1.12f);
    auto dim = [&](float u) { return std::max(8, int(std::round(u * scale))); };
    int w = dim(128), h = dim(128);
    if (t == Tex::Brushed) { w = dim(256); h = dim(128); }
    if (t == Tex::Wood) { w = dim(256); h = dim(128); }
    juce::Image img(juce::Image::ARGB, w, h, true);
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    auto put = [&](int x, int y, juce::uint32 v) { *reinterpret_cast<juce::uint32*>(bd.getPixelPointer(x, y)) = v; };
    const unsigned uw = unsigned(w), uh = unsigned(h);
    switch (t) {
    case Tex::Grain: {
        const float ux = 256.0f / float(w), uy = 256.0f / float(h);
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
            put(x, y, signedPx((0.8f * fine * tri(unsigned(x), unsigned(y), 11) + 0.35f * valueNoise(float(x) * ux / 4.0f, float(y) * uy / 4.0f, 64, 12)) * 0.9f));
        break;
    }
    case Tex::Brushed: {
        const int longN = std::max(3, int(std::round(72.0f * k))), shortN = std::max(2, int(std::round(9.0f * k)));
        for (int y = 0; y < h; ++y) {
            std::vector<float> white_((size_t)(w));
            for (int x = 0; x < w; ++x) white_[size_t(x)] = snorm(hash2(unsigned(x), unsigned(y), 21));
            auto lg = blurWrapNorm(blurWrapNorm(white_, longN), longN), sh = blurWrapNorm(white_, shortN);
            const unsigned band = unsigned(float(y) / std::max(3.0f * k, 1.0f));
            const float row = snorm(hash(unsigned(y) + 0x3c6ef372u)) * 0.9f * fine + 0.35f * snorm(hash(band + 77u));
            for (int x = 0; x < w; ++x)
                put(x, y, signedPx((0.30f * row + 0.40f * lg[size_t(x)] + 0.22f * sh[size_t(x)] + 0.12f * fine * white_[size_t(x)]) * 0.75f));
        }
        break;
    }
    case Tex::Speckle: {
        const float ux = 256.0f / float(w), uy = 256.0f / float(h), p = std::min(0.0070f / (k * k), 0.03f);
        auto height = [](float x, float y) { return valueNoise(x / 4.0f, y / 4.0f, 64, 31) + 0.5f * valueNoise(x / 2.0f, y / 2.0f, 128, 32); };
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            const float fx = float(x) * ux, fy = float(y) * uy;
            const float peel = (height(fx - l.x, fy - l.y) - height(fx + l.x, fy + l.y)) * 0.55f;
            const float s = unit(hash2(unsigned(x), unsigned(y), 33));
            const float speck = s > 1.0f - p ? 0.85f * fine : (s < p * 1.15f ? -0.9f * fine : 0.0f);
            put(x, y, signedPx(0.6f * peel + speck + 0.18f * fine * tri(unsigned(x), unsigned(y), 34)));
        }
        break;
    }
    case Tex::Hammer: {
        const int gN = 14; const float cell = float(w) / float(gN);
        auto feat = [&](int i, int j) {
            const unsigned wi = unsigned(((i % gN) + gN) % gN), wj = unsigned(((j % gN) + gN) % gN);
            return Point<float>(float(i) + 0.15f + 0.7f * unit(hash2(wi, wj, 41)), float(j) + 0.15f + 0.7f * unit(hash2(wi, wj, 42)));
        };
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            const float u = (float(x) + 0.5f) / cell, v = (float(y) + 0.5f) / cell;
            const int ci = int(std::floor(u)), cj = int(std::floor(v));
            float d1 = 1e9f, d2 = 1e9f; Point<float> best;
            for (int dj = -1; dj <= 1; ++dj) for (int di = -1; di <= 1; ++di) {
                const auto f = feat(ci + di, cj + dj);
                const float d = std::hypot(u - f.x, v - f.y);
                if (d < d1) { d2 = d1; d1 = d; best = {u - f.x, v - f.y}; } else if (d < d2) d2 = d;
            }
            const float rr = 0.62f;
            const float slope = d1 < rr ? 2.0f * (1.0f - d1 / rr) * std::sqrt(d1 / rr) : 0.0f;
            const float lit = -slope * (best.x * l.x + best.y * l.y) / std::max(d1, 1e-4f);
            const float ridge = (1.0f - std::min((d2 - d1) / 0.08f, 1.0f)) * 0.12f;
            const float mottle = valueNoise(float(x) * 16.0f / float(w), float(y) * 16.0f / float(h), 16, 43);
            put(x, y, signedPx(0.7f * lit + ridge + 0.22f * mottle + 0.16f * fine * tri(unsigned(x), unsigned(y), 44)));
        }
        break;
    }
    case Tex::Wood: {
        const float ux = 512.0f / float(w), uy = 256.0f / float(h);
        const int poreN = std::max(2, int(std::round(14.0f * k)));
        for (int y = 0; y < h; ++y) {
            std::vector<float> white_((size_t)(w));
            for (int x = 0; x < w; ++x) white_[size_t(x)] = snorm(hash2(unsigned(x), unsigned(y), 51));
            auto pores = blurWrapNorm(white_, poreN);
            for (int x = 0; x < w; ++x) {
                const float fx = float(x) * ux, fy = float(y) * uy;
                const float drift = 0.55f * valueNoise(fx / 256.0f, fy / 128.0f, 2, 52) + 0.08f * valueNoise(fx / 64.0f, fy / 32.0f, 8, 53);
                const float tt = fy / 256.0f * 16.0f + 1.6f * valueNoise(0.0f, fy / 16.0f, 16, 55) + drift;
                const float f = tt - std::floor(tt);
                const float strength = 0.5f + 0.5f * unit(hash(unsigned(((int(std::floor(tt)) % 16) + 16) % 16) ^ 0x5a5au));
                const float late = juce::jlimit(0.0f, 1.0f, (f - 0.62f) / 0.22f);
                const float ring = (late * late * (3.0f - 2.0f * late) - juce::jlimit(0.0f, 1.0f, (f - 0.9f) / 0.1f)) * strength;
                const float figure = 0.18f * valueNoise(fx / 64.0f, fy / 4.0f, 8, 54);
                put(x, y, signedPx(-0.5f * ring - 0.28f * std::max(pores[size_t(x)], 0.0f) + figure + 0.1f + 0.05f * fine * tri(unsigned(x), unsigned(y), 56)));
            }
        }
        break;
    }
    }
    juce::ignoreUnused(uw, uh);
    return img;
}
inline const juce::Image& texture(Tex t, float scale) {
    static std::mutex m; static std::map<std::pair<int, int>, juce::Image> cache;
    std::lock_guard<std::mutex> lock(m);
    const std::pair<int, int> key{int(t), int(std::round(scale * 4.0f))};
    auto it = cache.find(key);
    if (it == cache.end()) it = cache.emplace(key, buildTexture(t, float(key.second) / 4.0f)).first;
    return it->second;
}

// Tile `t` over `area` one texel per device pixel at `opacity`.
inline void tileTexture(Graphics& g, Tex t, Rectangle<float> area, float opacity, unsigned seed) {
    if (opacity <= 0 || area.isEmpty()) return;
    const float s = deviceScaleRef();
    const auto& img = texture(t, s);
    g.saveState();
    g.reduceClipRegion(area.getSmallestIntegerContainer());
    g.addTransform(juce::AffineTransform::scale(1.0f / s));
    g.setTiledImageFill(img, int(std::floor(area.getX() * s)) - int(hash(seed) % unsigned(img.getWidth())), int(std::floor(area.getY() * s)), opacity);
    g.fillRect(area.getSmallestIntegerContainer().toFloat() * s);
    g.restoreState();
}

// Gradient along `angle` (radians; 0 = left to right, pi/2 = top to bottom) across `r`.
inline juce::ColourGradient linearGrad(Rectangle<float> r, float angle, std::initializer_list<std::pair<float, Colour>> stops) {
    const float ca = std::cos(angle), sa = std::sin(angle), len = std::abs(r.getWidth() * ca) + std::abs(r.getHeight() * sa);
    const auto c = r.getCentre();
    const Point<float> p0(c.x - ca * len * 0.5f, c.y - sa * len * 0.5f), p1(c.x + ca * len * 0.5f, c.y + sa * len * 0.5f);
    juce::ColourGradient gr(stops.begin()->second, p0, (stops.end() - 1)->second, p1, false);
    for (auto& s : stops) gr.addColour(double(s.first), s.second);
    return gr;
}
inline void fillLinear(Graphics& g, Rectangle<float> r, float radius, float angle, std::initializer_list<std::pair<float, Colour>> stops) {
    g.setGradientFill(linearGrad(r, angle, stops));
    if (radius > 0) g.fillRoundedRectangle(r, radius); else g.fillRect(r);
}
// Elliptical radial gradient (centre `c`, radii rx, ry), clipped to `shape`.
inline void fillRadial(Graphics& g, const juce::Path& shape, Point<float> c, float rx, float ry, std::initializer_list<std::pair<float, Colour>> stops) {
    g.saveState();
    g.reduceClipRegion(shape.getBounds().getSmallestIntegerContainer());
    g.addTransform(juce::AffineTransform::translation(-c.x, -c.y).scaled(1.0f, ry / std::max(rx, 0.001f)).translated(c.x, c.y));
    juce::ColourGradient gr(stops.begin()->second, c, (stops.end() - 1)->second, {c.x + rx, c.y}, true);
    for (auto& s : stops) gr.addColour(double(s.first), s.second);
    g.setGradientFill(gr);
    juce::Path p(shape);
    p.applyTransform(juce::AffineTransform::translation(-c.x, -c.y).scaled(1.0f, std::max(rx, 0.001f) / std::max(ry, 0.001f)).translated(c.x, c.y));
    g.fillPath(p);
    g.restoreState();
}
inline juce::Path roundPath(Rectangle<float> r, float radius) { juce::Path p; p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius); return p; }

// Soft shadow: stacked translucent shapes approximating a gaussian of `sigma`.
inline void softShadow(Graphics& g, Rectangle<float> r, float radius, float sigma, Colour col, bool ellipse = false) {
    const int n = 6;
    for (int i = 0; i < n; ++i) {
        const float t = float(i) / float(n - 1), grow = sigma * (1.6f - 3.0f * t);
        auto rr = r.expanded(grow);
        if (rr.isEmpty()) continue;
        g.setColour(col.withMultipliedAlpha(0.28f));
        if (ellipse) g.fillEllipse(rr); else g.fillRoundedRectangle(rr, radius + std::max(grow, 0.0f));
    }
}
// Light cast inward from the edge of a rounded shape (inner shadow).
inline void innerShadow(Graphics& g, Rectangle<float> r, float radius, Point<float> off, float sigma, Colour col, bool ellipse = false) {
    g.saveState();
    juce::Path p; if (ellipse) p.addEllipse(r); else p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius);
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    g.setColour(col.withMultipliedAlpha(0.3f));
    for (int i = 0; i < 4; ++i) {
        const float w = sigma * (0.5f + 0.9f * float(i)) + 0.5f;
        juce::Path ring; ring.addRectangle(r.expanded(w * 3 + 8)); ring.addPath(p); ring.setUsingNonZeroWinding(false);
        ring.applyTransform(juce::AffineTransform::translation(off.x * (1.0f - 0.2f * float(i)), off.y * (1.0f - 0.2f * float(i))));
        g.fillPath(ring);
        juce::Path inner = p; // erode: stroke the edge with growing width
        g.setColour(col.withMultipliedAlpha(0.3f * (1.0f - 0.22f * float(i))));
        g.strokePath(p, juce::PathStrokeType(w), juce::AffineTransform::translation(off.x, off.y));
    }
    g.restoreState();
}
inline float awayAngle(const LightRig& rig) { auto d = rig.screenDir(); return (d.x == 0 && d.y == 0) ? juce::MathConstants<float>::halfPi : std::atan2(-d.y, -d.x); }
inline Colour edgeShade(Colour face, float f) { return f >= 0 ? mixc(face, white(1), 0.06f + 0.30f * f) : darkerBy(face, 0.08f + 0.50f * -f); }
inline float facing(const LightRig& rig, float nx, float ny) { auto d = rig.screenDir(); return nx * d.x + ny * d.y; }
} // namespace detail

inline void setDeviceScale(float scale) { detail::deviceScaleRef() = std::isfinite(scale) ? std::round(juce::jlimit(1.0f, 4.0f, scale) * 4.0f) / 4.0f : 2.0f; }
inline float deviceScale() { return detail::deviceScaleRef(); }

namespace detail {
// Four-sided chamfer between `r` and `r` inset by `b`, each side shaded by its facing.
inline void chamfer(Graphics& g, Rectangle<float> r, float radius, float b, Colour top, Colour bottom, const LightRig& rig) {
    const Colour mid = mixc(top, bottom, 0.5f);
    const float x0 = r.getX(), y0 = r.getY(), x1 = r.getRight(), y1 = r.getBottom();
    const Colour t = edgeShade(top, facing(rig, 0, -1)), bt = edgeShade(bottom, facing(rig, 0, 1)),
                 l = edgeShade(mid, facing(rig, -1, 0)), rt = edgeShade(mid, facing(rig, 1, 0));
    fillLinear(g, r, radius, juce::MathConstants<float>::halfPi, {{0.0f, t}, {0.5f, mixc(l, rt, 0.5f)}, {1.0f, bt}});
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    juce::Path clip = roundPath(r, radius);
    g.reduceClipRegion(clip);
    auto poly = [&](std::initializer_list<Point<float>> pts, Colour c0, Colour c1, bool vertical) {
        juce::Path p; bool first = true; for (auto& q : pts) { if (first) p.startNewSubPath(q); else p.lineTo(q); first = false; }
        p.closeSubPath();
        auto bb = p.getBounds();
        g.setGradientFill(juce::ColourGradient(c0, vertical ? bb.getX() : bb.getX(), bb.getY(), c1, vertical ? bb.getX() : bb.getX(), vertical ? bb.getBottom() : bb.getY() + 0.01f, false));
        g.fillPath(p);
    };
    poly({{x0, y0}, {x1, y0}, {x1 - b, y0 + b}, {x0 + b, y0 + b}}, mixc(t, top, 0.25f), t, true);
    poly({{x0 + b, y1 - b}, {x1 - b, y1 - b}, {x1, y1}, {x0, y1}}, bt, darkerBy(bt, 0.25f), true);
    poly({{x0, y0}, {x0 + b, y0 + b}, {x0 + b, y1 - b}, {x0, y1}}, mixc(l, t, 0.35f), darkerBy(l, 0.18f), true);
    poly({{x1 - b, y0 + b}, {x1, y0}, {x1, y1}, {x1 - b, y1 - b}}, mixc(rt, t, 0.35f), darkerBy(rt, 0.18f), true);
    g.setColour(Colour(0x66000000)); g.drawLine(x0 + b, y0 + b, x1 - b, y0 + b, 0.5f);
    g.restoreState();
}

inline void brushedStreaks(Graphics& g, Rectangle<float> r, unsigned seed, float strength) {
    const int n = int(juce::jlimit(16.0f, 900.0f, (r.getWidth() * r.getHeight()) / 70.0f));
    for (int i = 0; i < n; ++i) {
        const unsigned h = hash2(unsigned(i), seed, 61);
        const float y = r.getY() + unit(h) * r.getHeight(), x = r.getX() - 6.0f + unit(hash(h ^ 0x68e31da4u)) * (r.getWidth() + 12.0f);
        const float lr = unit(hash(h ^ 0x1b56c4e9u)), len = 4.0f + lr * lr * std::min(r.getWidth() * 0.35f, 160.0f);
        const float a = (0.02f + 0.05f * unit(hash(h ^ 0x2545f491u))) * strength;
        g.setColour((h & 1) == 0 ? white(a * 1.2f) : black(a * 0.6f));
        g.drawLine(x, y, x + len, y, 0.45f + 0.4f * unit(hash(h ^ 0x9e3779b9u)));
    }
}
inline bool isMetal(int f) { return f == kFinishBrushed || f == kFinishAnodised || f == kFinishHammertone; }
inline bool isScreen(int f) { return f == kFinishScanlines || f == kFinishGrid || f == kFinishNeonEdge || f == kFinishHorizon; }

// The finish over `face`. The caller has clipped to the face.
inline void finishLayer(Graphics& g, Rectangle<float> face, int finish, Colour base, unsigned seed, const LightRig& rig) {
    const float x0 = face.getX(), x1 = face.getRight(), y0 = face.getY(), y1 = face.getBottom(), w = face.getWidth(), h = face.getHeight();
    const bool lightBase = lum(base) > 0.55f;
    const float pi = juce::MathConstants<float>::pi;
    auto hl = [&](float y, float a, float b, Colour c, float wd) { g.setColour(c); g.drawLine(a, y, b, y, wd); };
    auto vl = [&](float x, float a, float b, Colour c, float wd) { g.setColour(c); g.drawLine(x, a, x, b, wd); };
    auto glow = [&](Rectangle<float> r, float radius, Colour c, float feather) {
        for (int i = 0; i < 4; ++i) { g.setColour(c.withMultipliedAlpha(0.28f)); g.fillRoundedRectangle(r.expanded(feather * (1.0f - 0.3f * float(i)) * 0.5f), radius + feather * 0.3f); }
    };
    switch (finish) {
    case kFinishFlat: tileTexture(g, Tex::Grain, face, lightBase ? 0.05f : 0.07f, seed); break;
    case kFinishBrushed:
        tileTexture(g, Tex::Brushed, face, lightBase ? 0.20f : 0.13f, seed);
        brushedStreaks(g, face, seed, lightBase ? 1.0f : 0.8f);
        break;
    case kFinishWood: {
        tileTexture(g, Tex::Wood, face, 0.62f, 0);
        float y = y0 + 4.0f; unsigned k = 0;
        while (y < y1) {
            const float ph = unit(hash2(k, seed, 71)) * 6.0f;
            juce::Path p;
            for (int i = 0; i <= int(std::ceil((x1 - x0) / 6.0f)); ++i) {
                const float x = x0 + float(i) * 6.0f, yy = y + std::sin(x * 0.031f + ph) * 1.6f + std::sin(x * 0.011f + y * 0.2f) * 1.2f;
                if (i == 0) p.startNewSubPath(x, yy); else p.lineTo(x, yy);
            }
            g.setColour(black(0.06f + 0.06f * unit(hash2(k, seed, 72)))); g.strokePath(p, juce::PathStrokeType(0.7f));
            y += 4.0f + 5.0f * unit(hash2(k, seed, 73)); ++k;
        }
        tileTexture(g, Tex::Grain, face, 0.05f, seed);
        break;
    }
    case kFinishScanlines: {
        const auto tint = base.withSaturation(juce::jmin(base.getSaturation() * 0.9f, 0.85f)).withBrightness(1.0f);
        const auto c = face.getCentre();
        juce::Path fp; fp.addRectangle(face);
        fillRadial(g, fp, {c.x, y0 + h * 0.46f}, w * 0.62f, h * 0.70f, {{0.0f, tint.withAlpha(0.20f)}, {0.5f, tint.withAlpha(0.08f)}, {1.0f, tint.withAlpha(0.0f)}});
        tileTexture(g, Tex::Grain, face, 0.05f, seed);
        const float s = deviceScale(), pitch = std::max(2.0f, std::round(3.0f * s)), gap = std::max(1.0f, std::round(pitch / 3.0f));
        for (float py = std::floor(y0 * s); py / s < y1; py += pitch) {
            const float y = py / s;
            g.setColour(black(0.34f)); g.fillRect(x0, y, w, gap / s);
            g.setColour(tint.withAlpha(0.085f)); g.fillRect(x0, y + gap / s, w, 1.0f / s);
        }
        const float beam = y0 + h * (0.30f + 0.4f * unit(hash(seed ^ 0x51edu)));
        fillLinear(g, {x0, beam - 9.0f, w, 9.0f}, 0, pi * 0.5f, {{0.0f, tint.withAlpha(0.0f)}, {0.7f, tint.withAlpha(0.02f)}, {1.0f, tint.withAlpha(0.08f)}});
        glow({x0 - 4.0f, beam - 2.2f, w + 8.0f, 3.2f}, 1.6f, tint.withAlpha(0.11f), 3.0f);
        fillLinear(g, {x0, y0, w, h * 0.45f}, 0, pi * 0.5f, {{0.0f, Colour(0x14ffffff)}, {1.0f, Colour(0x00ffffff)}});
        g.setColour(white(0.045f)); g.fillEllipse(x0 - w * 0.2f, y0 - h * 0.5f, w * 0.85f, h * 0.85f);
        innerShadow(g, face, 3.0f, {0, 0}, juce::jlimit(3.0f, 14.0f, std::min(w, h) * 0.10f), black(0.75f));
        break;
    }
    case kFinishHazard: {
        tileTexture(g, Tex::Grain, face, 0.06f, seed);
        const float bh = 6.0f;
        for (float yb : {y0 + 1.0f, y1 - 1.0f - bh}) {
            Rectangle<float> band(x0, yb, x1 - x0, bh);
            fillLinear(g, band, 0, pi * 0.5f, {{0.0f, Colour(0xff1a1a18)}, {1.0f, Colour(0xff121210)}});
            g.saveState(); g.reduceClipRegion(band.getSmallestIntegerContainer());
            for (float x = x0 - 8.0f; x < x1; x += 12.0f) {
                juce::Path p; p.startNewSubPath(x, yb + bh); p.lineTo(x + 6, yb + bh); p.lineTo(x + 12, yb); p.lineTo(x + 6, yb); p.closeSubPath();
                g.setGradientFill(juce::ColourGradient(Colour(0xfff0d438), x, yb, Colour(0xffcfae14), x, yb + bh, false)); g.fillPath(p);
            }
            tileTexture(g, Tex::Speckle, band, 0.25f, seed ^ 5);
            g.restoreState();
            auto o = rig.shadowOffset(0.8f);
            innerShadow(g, band, 0, o, 0.6f, Colour(0x99000000));
        }
        break;
    }
    case kFinishNeonEdge: {
        const Colour cyan(0xff39f2ff), mag(0xffff4fd8);
        const float inset = juce::jlimit(2.5f, 5.0f, std::min(w, h) * 0.035f);
        auto tube = face.reduced(inset);
        const float tr = std::min(inset, 3.5f), tw = juce::jlimit(1.1f, 1.8f, inset * 0.42f);
        fillLinear(g, face, 0, awayAngle(rig), {{0.0f, white(0.07f)}, {0.22f, white(0.015f)}, {0.48f, white(0.05f)}, {0.56f, white(0.0f)}, {1.0f, white(0.0f)}});
        for (int k = 1; k <= 11; ++k) {
            const float d = float(k) * 0.95f, a = 0.115f * std::exp(-float(k) / 3.4f);
            g.setColour(cyan.withAlpha(a));
            g.drawRoundedRectangle(tube.reduced(d), std::max(tr - d, 0.5f), 1.3f);
            if (d < inset + 1.0f) g.drawRoundedRectangle(tube.expanded(d), tr + d, 1.3f);
        }
        const float f = juce::jlimit(4.0f, 9.0f, inset * 1.9f);
        for (auto band : {Rectangle<float>(tube.getX(), tube.getY() - 0.5f, tube.getWidth(), 1.0f), Rectangle<float>(tube.getX(), tube.getBottom() - 0.5f, tube.getWidth(), 1.0f),
                          Rectangle<float>(tube.getX() - 0.5f, tube.getY(), 1.0f, tube.getHeight()), Rectangle<float>(tube.getRight() - 0.5f, tube.getY(), 1.0f, tube.getHeight())})
            glow(band.expanded(f * 0.5f), f * 0.5f, cyan.withAlpha(0.20f), f);
        g.setColour(cyan.withAlpha(0.035f)); g.drawRoundedRectangle(tube.reduced(inset * 1.9f), 1.0f, 0.8f);
        g.setColour(darkerBy(cyan, 0.25f).withAlpha(0.55f)); g.drawRoundedRectangle(tube, tr, tw + 1.4f);
        g.setColour(cyan); g.drawRoundedRectangle(tube, tr, tw);
        g.setColour(brighterBy(cyan, 0.8f)); g.drawRoundedRectangle(tube, tr, tw * 0.42f);
        const float ml = juce::jlimit(12.0f, 60.0f, w * 0.13f);
        for (auto m : {Point<float>(tube.getX() + tr + 3.0f, tube.getY()), Point<float>(tube.getRight() - tr - 3.0f - ml, tube.getBottom())}) {
            Rectangle<float> seg(m.x, m.y - tw * 0.5f, ml, tw);
            g.setColour(Colour(0xff04060c)); g.fillRoundedRectangle(seg.expanded(1.6f), 1.6f);
            glow(seg.expanded(f * 0.5f), f * 0.5f, mag.withAlpha(0.30f), f * 1.1f);
            g.setColour(mag); g.fillRoundedRectangle(seg.expanded(0.4f), tw);
            g.setColour(brighterBy(mag, 0.8f)); g.fillRoundedRectangle(seg.reduced(1.0f, tw * 0.29f), tw * 0.3f);
        }
        break;
    }
    case kFinishHorizon: {
        const float hy = y0 + h * 0.58f, cx = x0 + w * 0.5f;
        const Colour mag(0xffff4fd8), cyan(0xff35f2ff);
        fillLinear(g, {x0, y0, w, hy - y0}, 0, pi * 0.5f, {{0.0f, Colour(0x00ff4fd8)}, {1.0f, Colour(0x55ff4fd8)}});
        for (int i = 0; i < 3; ++i) { g.setColour(mag.withAlpha(0.12f)); g.fillEllipse(cx - w * 0.5f, hy - 14.0f + float(i) * 3.0f, w, 28.0f - float(i) * 6.0f); }
        g.setColour(Colour(0xffffc9f0).withAlpha(0.30f)); g.fillEllipse(cx - w * 0.22f, hy - 7.0f, w * 0.44f, 14.0f);
        fillLinear(g, {x0, hy, w, (y1 - hy) * 0.6f}, 0, pi * 0.5f, {{0.0f, mag.withAlpha(0.16f)}, {1.0f, mag.withAlpha(0.0f)}});
        for (int i = -8; i <= 8; ++i) {
            g.setColour(cyan.withAlpha(0.14f)); g.drawLine(cx + float(i) * 3.0f, hy, cx + float(i) * w * 0.16f, y1, 2.2f);
            g.setColour(cyan.withAlpha(0.62f)); g.drawLine(cx + float(i) * 3.0f, hy, cx + float(i) * w * 0.16f, y1, 0.7f);
        }
        for (int j = 1; j <= 6; ++j) {
            const float f = float(j) / 6.0f, y = hy + f * f * (y1 - hy);
            hl(y, x0, x1, cyan.withAlpha(0.14f), 1.0f + 1.6f * f); hl(y, x0, x1, cyan.withAlpha(0.62f), 0.6f + 0.4f * f);
        }
        hl(hy, x0, x1, cyan.withAlpha(0.25f), 2.4f); hl(hy, x0, x1, Colour(0xffd8fbff).withAlpha(0.85f), 0.8f);
        break;
    }
    case kFinishAnodised: tileTexture(g, Tex::Grain, face, lightBase ? 0.10f : 0.13f, seed); break;
    case kFinishPowder: tileTexture(g, Tex::Speckle, face, lightBase ? 0.16f : 0.2f, seed); break;
    case kFinishHammertone:
        tileTexture(g, Tex::Hammer, face, lightBase ? 0.2f : 0.26f, seed);
        tileTexture(g, Tex::Grain, face, 0.06f, seed ^ 9);
        break;
    case kFinishSatinBlack: tileTexture(g, Tex::Grain, face, 0.07f, seed); break;
    default:
        for (float x = x0 + 14.0f; x < x1; x += 14.0f) vl(std::trunc(x) + 0.5f, y0, y1, Colour(0x12ffffff), 1.0f);
        for (float y = y0 + 14.0f; y < y1; y += 14.0f) hl(std::trunc(y) + 0.5f, x0, x1, Colour(0x12ffffff), 1.0f);
        for (auto c : {Point<float>(x0, y0), Point<float>(x1, y0), Point<float>(x0, y1), Point<float>(x1, y1)}) {
            const float sx = c.x == x0 ? 1.0f : -1.0f, sy = c.y == y0 ? 1.0f : -1.0f;
            c = {c.x + sx * 1.5f, c.y + sy * 1.5f};
            g.setColour(Colour(0x77ffffff));
            g.drawLine(c.x, c.y + sy * 10.0f, c.x, c.y, 1.2f); g.drawLine(c.x, c.y, c.x + sx * 10.0f, c.y, 1.2f);
        }
    }
}

inline void plateLight(Graphics& g, Rectangle<float> face, float fr, int finish, unsigned seed, const LightRig& rig) {
    const auto d = rig.screenDir(), c = face.getCentre();
    const float sheen = isScreen(finish) ? 0.05f : finish == kFinishSatinBlack ? 0.16f : isMetal(finish) ? 0.13f : 0.10f;
    juce::Path shape = roundPath(face, fr);
    g.saveState(); g.reduceClipRegion(shape);
    const Point<float> focus(c.x + d.x * face.getWidth() * 0.30f, c.y + d.y * face.getHeight() * 0.45f);
    fillRadial(g, shape, focus, face.getWidth() * 0.85f, face.getHeight() * 1.15f, {{0.0f, white(sheen)}, {0.45f, white(sheen * 0.35f)}, {1.0f, white(0.0f)}});
    if (finish == kFinishBrushed || finish == kFinishAnodised) {
        auto j = [&](unsigned k) { return (unit(hash(seed + k)) - 0.5f) * 0.12f; };
        const float k = finish == kFinishBrushed ? 1.0f : 0.55f;
        auto hi = [&](float a) { return white(a * k); };
        auto lo = [&](float a) { return black(a * k); };
        fillLinear(g, face, 0, 0.04f, {{0.0f, hi(0.0f)}, {0.18f + j(1), hi(0.10f)}, {0.34f + j(2), hi(0.0f)}, {0.58f + j(3), hi(0.16f)}, {0.78f + j(4), hi(0.02f)}, {1.0f, hi(0.07f)}});
        fillLinear(g, face, 0, 0.04f, {{0.0f, lo(0.10f)}, {0.12f, lo(0.0f)}, {0.42f + j(5), lo(0.07f)}, {0.62f, lo(0.0f)}, {0.88f + j(6), lo(0.08f)}, {1.0f, lo(0.13f)}});
    }
    if (finish == kFinishSatinBlack)
        fillLinear(g, face, 0, juce::MathConstants<float>::halfPi, {{0.0f, white(0.07f)}, {0.3f, white(0.02f)}, {0.6f, white(0.0f)}});
    fillLinear(g, face, 0, awayAngle(rig), {{0.0f, black(0.0f)}, {0.5f, black(0.05f)}, {1.0f, black(0.17f)}});
    fillRadial(g, shape, c, face.getWidth() * 0.56f, face.getHeight() * 0.62f, {{0.0f, black(0.0f)}, {0.72f, black(0.0f)}, {1.0f, black(0.13f)}});
    g.restoreState();
}
} // namespace detail

// A lit faceplate: soft drop shadow, chamfered edge shaded per side, sheen at the light's angle,
// the finish texture, falloff and vignette. `finish` is 0..11 (kFinish*).
inline void drawPanelLit(Graphics& g, Rectangle<float> r, float radius, Colour top, Colour bottom, int finish, const LightRig& rig = LightRig::studio()) {
    using namespace detail;
    if (finish == kFinishSatinBlack) { top = mixc(top, Colour(0xff232427), 0.85f); bottom = mixc(bottom, Colour(0xff131415), 0.85f); }
    radius = std::max(radius, 0.0f);
    auto soft = rig.shadowOffset(5.0f);
    softShadow(g, r.translated(soft.x, soft.y), radius + 2.0f, rig.shadowSigma(5.0f) * 2.4f, Colour(0x8c000000));
    auto near_ = rig.shadowOffset(1.2f);
    softShadow(g, r.translated(near_.x, near_.y), radius, rig.shadowSigma(1.2f) + 0.4f, Colour(0x99000000));
    const float b = juce::jlimit(0.6f, 2.0f, std::min(r.getWidth(), r.getHeight()) * 0.06f);
    chamfer(g, r, radius, b, top, bottom, rig);
    auto face = r.reduced(b);
    const float fr = std::max(radius - b * 0.6f, 0.5f);
    fillLinear(g, face, fr, awayAngle(rig), {{0.0f, mixc(top, white(1), 0.04f)}, {0.35f, top}, {1.0f, bottom}});
    g.saveState();
    g.reduceClipRegion(detail::roundPath(face, fr));
    const unsigned seed = posSeed(r.getX(), r.getY());
    finishLayer(g, face, finish, mixc(top, bottom, 0.5f), seed, rig);
    g.restoreState();
    plateLight(g, face, fr, finish, seed, rig);
    g.setColour(Colour(0x1a000000)); g.drawRoundedRectangle(face, fr, 0.6f);
    g.setColour(Colour(0x8c06080a)); g.drawRoundedRectangle(r.expanded(0.25f), radius + 0.25f, 0.6f);
}
inline void drawPanelLit(Graphics& g, Rectangle<float> r, Colour top, Colour bottom, int finish) { drawPanelLit(g, r, 2.5f, top, bottom, finish); }

// The finish alone over `r` (clipped to it).
inline void drawFinishLit(Graphics& g, Rectangle<float> r, int finish) {
    if (finish <= 0) return;
    g.saveState(); g.reduceClipRegion(detail::roundPath(r, 2.0f));
    detail::finishLayer(g, r, finish, Colour(0xff808080), detail::posSeed(r.getX(), r.getY()), LightRig::studio());
    g.restoreState();
}

// ---------------------------------------------------------------------------------------------
// Screws
// ---------------------------------------------------------------------------------------------
enum class ScrewHead { Slot, Cross, Hex };

inline float screwRadiusFor(Rectangle<float> plate) { return juce::jlimit(2.5f, 7.0f, std::min(plate.getWidth(), plate.getHeight()) * 0.035f); }

// Domed screw in a countersink. Rotation is hashed from the position, so a plate's screws differ but redraw the same.
inline void drawScrewHead(Graphics& g, Point<float> c, float radius, ScrewHead head, const LightRig& rig = LightRig::studio()) {
    using namespace detail;
    const float r = std::max(radius, 1.0f), pi = juce::MathConstants<float>::pi;
    const auto d = rig.screenDir();
    const float ang = awayAngle(rig), rot = unit(posSeed(c.x, c.y)) * pi;
    auto cs = Rectangle<float>(r * 2.48f, r * 2.48f).withCentre(c);
    g.setGradientFill(linearGrad(cs, ang, {{0.0f, black(0.55f)}, {0.55f, black(0.25f)}, {1.0f, black(0.05f)}}));
    g.fillEllipse(cs);
    g.setGradientFill(linearGrad(cs, ang, {{0.0f, white(0.0f)}, {0.6f, white(0.0f)}, {1.0f, white(0.35f)}}));
    g.fillEllipse(cs);
    innerShadow(g, cs, 0, rig.shadowOffset(r * 0.18f), r * 0.12f + 0.3f, Colour(0xb0000000), true);
    auto hr = Rectangle<float>(r * 2, r * 2).withCentre(c);
    auto so = rig.shadowOffset(r * 0.25f);
    softShadow(g, hr.translated(so.x, so.y), 0, rig.shadowSigma(r * 0.25f) + 0.25f, Colour(0xcc000000), true);
    const Point<float> focus(c.x + d.x * r * 0.42f, c.y + d.y * r * 0.42f);
    juce::ColourGradient dome(Colour(0xfff4f5f4), focus, Colour(0xff30363a), {focus.x + r * 1.45f, focus.y}, true);
    dome.addColour(0.22, Colour(0xffd3d7d8)); dome.addColour(0.55, Colour(0xff989fa3)); dome.addColour(0.85, Colour(0xff586065));
    g.setGradientFill(dome); g.fillEllipse(hr);
    g.saveState();
    juce::Path hp; hp.addEllipse(hr); g.reduceClipRegion(hp);
    const Point<float> lit(-d.x * r * 0.09f, -d.y * r * 0.09f), dark(d.x * r * 0.03f, d.y * r * 0.03f);
    auto recess = [&](const juce::Path& p) {
        g.setColour(Colour(0x99e8ecee)); g.fillPath(p, juce::AffineTransform::translation(lit.x, lit.y));
        g.setColour(Colour(0xff1c2124)); g.fillPath(p, juce::AffineTransform::translation(dark.x, dark.y));
    };
    auto bar = [&](float halfLen, float halfW, float a) {
        juce::Path p; p.addRectangle(-halfLen, -halfW, halfLen * 2, halfW * 2);
        p.applyTransform(juce::AffineTransform::rotation(a).translated(c.x, c.y)); return p;
    };
    switch (head) {
    case ScrewHead::Slot: recess(bar(r * 1.1f, r * 0.15f, rot)); break;
    case ScrewHead::Cross:
        recess(bar(r * 0.62f, r * 0.13f, rot)); recess(bar(r * 0.62f, r * 0.13f, rot + pi * 0.5f));
        g.setColour(Colour(0xff1a1e20)); g.fillEllipse(Rectangle<float>(r * 0.44f, r * 0.44f).withCentre(c));
        break;
    case ScrewHead::Hex: {
        juce::Path p;
        for (int k = 0; k < 6; ++k) { const float a = rot + float(k) * pi / 3.0f; Point<float> q(c.x + std::cos(a) * r * 0.46f, c.y + std::sin(a) * r * 0.46f); if (k == 0) p.startNewSubPath(q); else p.lineTo(q); }
        p.closeSubPath(); recess(p); break;
    }
    }
    g.restoreState();
    g.setColour(Colour(0x55000000)); g.drawEllipse(hr, 0.5f);
    const Point<float> sp(c.x + d.x * r * 0.45f, c.y + d.y * r * 0.45f);
    g.setColour(Colour(0x44ffffff)); g.fillEllipse(Rectangle<float>(r * 0.84f, r * 0.64f).withCentre(sp));
    g.setColour(Colour(0xeeffffff)); g.fillEllipse(Rectangle<float>(std::max(r * 0.26f, 1.0f), std::max(r * 0.26f, 1.0f)).withCentre(sp));
}
inline void drawScrewLit(Graphics& g, float x, float y) { drawScrewHead(g, {x, y}, 4.25f, ScrewHead::Slot); }

// Four screws inset in the corners of `plate`.
inline void drawCornerScrews(Graphics& g, Rectangle<float> plate, ScrewHead head = ScrewHead::Slot) {
    const float rad = screwRadiusFor(plate), inset = rad * 2.6f;
    for (auto p : {Point<float>(plate.getX() + inset, plate.getY() + inset), Point<float>(plate.getRight() - inset, plate.getY() + inset),
                   Point<float>(plate.getX() + inset, plate.getBottom() - inset), Point<float>(plate.getRight() - inset, plate.getBottom() - inset)})
        drawScrewHead(g, p, rad, head);
}

// ---------------------------------------------------------------------------------------------
// Keys, rack ears
// ---------------------------------------------------------------------------------------------
// Square key with a recessed well, cast shadow and a lit top edge.
inline void drawKey(Graphics& g, Rectangle<float> r, Colour base, bool down = false) {
    using namespace detail;
    const auto rig = LightRig::studio();
    const float pi = juce::MathConstants<float>::pi;
    auto well = r.expanded(2.5f);
    fillLinear(g, well, 3.5f, pi * 0.5f, {{0.0f, Colour(0xff050607)}, {1.0f, Colour(0xff0d0f11)}});
    innerShadow(g, well, 3.5f, rig.shadowOffset(1.5f), 1.0f, Colour(0xcc000000));
    g.setColour(Colour(0x30ffffff)); g.drawRoundedRectangle(well.expanded(0.4f), 3.9f, 0.7f);
    auto face = r.translated(0, down ? 1.6f : 0.0f);
    const float hgt = down ? 0.6f : 3.0f;
    auto o = rig.shadowOffset(hgt);
    softShadow(g, face.translated(o.x, o.y), 2.0f, rig.shadowSigma(hgt) + 0.4f, Colour(0xcc000000));
    const Colour hi = brighterBy(base, down ? 0.06f : 0.26f), lo = darkerBy(base, down ? 0.45f : 0.25f);
    fillLinear(g, face, 2.0f, awayAngle(rig), {{0.0f, hi}, {0.45f, mixc(base, hi, 0.3f)}, {1.0f, lo}});
    auto rim = face.reduced(1.0f);
    fillLinear(g, Rectangle<float>(rim.getX(), rim.getY(), rim.getWidth(), rim.getHeight() * 0.42f), 1.5f, pi * 0.5f, {{0.0f, white(down ? 0.08f : 0.24f)}, {1.0f, white(0.0f)}});
    fillLinear(g, Rectangle<float>(face.getX() + 2.0f, face.getBottom() - 1.4f, face.getWidth() - 4.0f, 1.2f), 0, pi * 0.5f, {{0.0f, Colour(0x00000000)}, {1.0f, Colour(0x60000000)}});
    g.setColour(Colour(0x55000000)); g.drawRoundedRectangle(face, 2.0f, 0.8f);
}

namespace detail {
inline void rackSlot(Graphics& g, Rectangle<float> slot, const LightRig& rig) {
    const float ang = awayAngle(rig), k = slot.getHeight() * 0.5f, e = juce::jlimit(0.8f, 1.6f, slot.getHeight() * 0.14f), pi = juce::MathConstants<float>::pi;
    auto lip = slot.expanded(e);
    fillLinear(g, lip, k + e, ang, {{0.0f, black(0.55f)}, {0.45f, black(0.2f)}, {0.7f, black(0.0f)}});
    fillLinear(g, lip, k + e, ang, {{0.0f, white(0.0f)}, {0.5f, white(0.0f)}, {1.0f, white(0.55f)}});
    fillLinear(g, slot, k, pi * 0.5f, {{0.0f, Colour(0xff1a1b1e)}, {0.6f, Colour(0xff3a3d42)}, {1.0f, Colour(0xff2a2c30)}});
    innerShadow(g, slot, k, rig.shadowOffset(slot.getHeight() * 0.5f), slot.getHeight() * 0.2f + 0.4f, Colour(0xff000000));
    g.setColour(Colour(0x66000000)); g.drawRoundedRectangle(slot, k, 0.6f);
}
} // namespace detail

// Rack ear in any metal colour: cast shadow, chamfered edges, brushed face, bent flange on the
// inner edge, and slots with a pressed lip. `leftSide` puts the bend on the right edge.
inline void drawRackEar(Graphics& g, Rectangle<float> r, bool leftSide, Colour top, Colour bottom, const LightRig& rig) {
    using namespace detail;
    const float pi = juce::MathConstants<float>::pi;
    auto o = rig.shadowOffset(3.0f);
    softShadow(g, r.translated(o.x, o.y), 1.5f, rig.shadowSigma(3.0f) * 1.8f, Colour(0xa0000000));
    auto o2 = rig.shadowOffset(0.8f);
    softShadow(g, r.translated(o2.x, o2.y), 1.0f, 0.6f, Colour(0x80000000));
    const float b = juce::jlimit(0.6f, 1.2f, r.getWidth() * 0.05f);
    chamfer(g, r, 1.0f, b, top, bottom, rig);
    auto face = r.reduced(b);
    drawBrushedPanel(g, face, top, bottom, leftSide ? 11u : 12u);
    const float fw = juce::jlimit(2.0f, 5.0f, r.getWidth() * 0.16f);
    const bool lit = facing(rig, leftSide ? 1.0f : -1.0f, 0.0f) > 0;
    Rectangle<float> band = leftSide ? Rectangle<float>(face.getRight() - fw, face.getY(), fw, face.getHeight()) : Rectangle<float>(face.getX(), face.getY(), fw, face.getHeight());
    const float ang = leftSide ? 0.0f : pi, catchA = lit ? 0.24f : 0.10f;
    fillLinear(g, band, 0, ang, {{0.0f, white(0.0f)}, {0.45f, white(catchA)}, {0.7f, white(catchA * 0.4f)}, {1.0f, white(0.0f)}});
    fillLinear(g, band, 0, ang, {{0.0f, black(0.0f)}, {0.6f, black(0.0f)}, {1.0f, black(lit ? 0.45f : 0.7f)}});
    Rectangle<float> body = leftSide ? Rectangle<float>(face.getX(), face.getY(), face.getWidth() - fw, face.getHeight()) : Rectangle<float>(face.getX() + fw, face.getY(), face.getWidth() - fw, face.getHeight());
    const float sw = juce::jlimit(4.0f, 22.0f, body.getWidth() * 0.6f), sh = std::max(sw * 0.55f, 3.0f);
    std::vector<float> ys;
    if (body.getHeight() > sh * 5.0f) { ys.push_back(body.getY() + body.getHeight() * 0.17f); ys.push_back(body.getBottom() - body.getHeight() * 0.17f); }
    else ys.push_back(body.getCentreY());
    for (float cy : ys) rackSlot(g, Rectangle<float>(body.getCentreX() - sw * 0.5f, cy - sh * 0.5f, sw, sh), rig);
}
inline void drawRackEar(Graphics& g, Rectangle<float> r, bool leftSide) {
    drawRackEar(g, r, leftSide, Colour(0xff2d2e31), Colour(0xff141516), LightRig::studio());
}

// Small rectangular key with a translucent LED cap. `lit` shows the lamp; text is drawn by the
// caller beside it, as the legends are printed on the panel rather than on the cap.
inline void drawLedKey(Graphics& g, Rectangle<float> r, Colour led, bool lit, bool down = false) {
    using namespace detail;
    const auto rig = LightRig::studio();
    const float pi = juce::MathConstants<float>::pi;
    auto well = r.expanded(1.6f);
    fillLinear(g, well, 2.6f, pi * 0.5f, {{0.0f, Colour(0xff030303)}, {1.0f, Colour(0xff0a0b0c)}});
    innerShadow(g, well, 2.6f, rig.shadowOffset(1.0f), 0.8f, Colour(0xcc000000));
    g.setColour(Colour(0x30ffffff)); g.drawRoundedRectangle(well.expanded(0.4f), 3.0f, 0.7f);
    auto cap = r.reduced(0.6f).translated(0, down ? 0.8f : 0.0f);
    if (lit) {
        g.setColour(led.withAlpha(0.14f)); g.fillRoundedRectangle(cap.expanded(6.0f), 8.0f);
        g.setColour(led.withAlpha(0.18f)); g.fillRoundedRectangle(cap.expanded(3.5f), 5.5f);
        g.setColour(led.withAlpha(0.20f)); g.fillRoundedRectangle(cap.expanded(1.8f), 3.5f);
    }
    const Colour hi = lit ? brighterBy(led, 0.5f) : darkerBy(led, 0.78f), lo = lit ? darkerBy(led, 0.1f) : darkerBy(led, 0.95f);
    fillLinear(g, cap, 2.0f, pi * 0.5f, {{0.0f, hi}, {1.0f, lo}});
    if (lit) { g.setColour(brighterBy(led, 0.7f).withAlpha(0.35f)); g.fillEllipse(cap.reduced(cap.getWidth() * 0.08f, cap.getHeight() * 0.12f)); }
    auto sheen = cap.reduced(1.5f);
    fillLinear(g, Rectangle<float>(sheen.getX(), sheen.getY(), sheen.getWidth(), cap.getHeight() * 0.4f), 1.5f, pi * 0.5f, {{0.0f, white(lit ? 0.4f : 0.16f)}, {1.0f, white(0.0f)}});
    g.setColour(Colour(0x66000000)); g.drawRoundedRectangle(cap, 2.0f, 0.8f);
}

// ---------------------------------------------------------------------------------------------
// Legends: silk-screen ink or engraving lit from the studio light.
// ---------------------------------------------------------------------------------------------
struct Legend {
    enum Kind { Silk, Engraved } kind = Silk;
    Colour colour = Colour(0xffefebe0);
    static Legend silk(Colour ink) { return {Silk, ink}; }
    static Legend engraved(Colour fill) { return {Engraved, fill}; }
};

namespace detail {
inline Colour silkInk(Colour c) {
    if (lum(c) > 0.85f) c = mixc(c, Colour(0xfff1ede2).withAlpha(c.getFloatAlpha()), 0.65f);
    return c.withAlpha(c.getFloatAlpha() * 0.94f);
}
inline void engraveOffsets(float size, Point<float>& dark, Point<float>& light) {
    const auto d = LightRig::studio().screenDir();
    const float s = deviceScale(), px = std::max(std::round(size * 0.045f * s), 1.0f);
    float ox = std::round(d.x * px), oy = std::round(d.y * px);
    if (ox == 0 && oy == 0) { if (std::abs(d.x) > std::abs(d.y)) ox = d.x < 0 ? -1.0f : 1.0f; else oy = d.y == 0 ? -1.0f : (d.y < 0 ? -1.0f : 1.0f); }
    dark = {ox / s, oy / s}; light = {-ox / s, -oy / s};
}
inline void engraveAlphas(float plateLum, float& shaded, float& lit) { const float l = juce::jlimit(0.0f, 1.0f, plateLum); shaded = 0.92f - 0.30f * l; lit = 0.34f + 0.56f * l; }
inline float guessPlateLum(Colour fill) { return lum(fill) > 0.6f ? 0.38f : 0.72f; }
// `at` is the horizontal centre/left and the vertical centre of the text.
inline void textAt(Graphics& g, Colour c, Point<float> at, const juce::String& text, float size, bool centre) {
    g.setColour(c);
    g.setFont(juce::Font(juce::FontOptions(size, juce::Font::bold)));
    Rectangle<float> box(600.0f, size * 1.6f);
    box = centre ? box.withCentre(at) : box.withPosition(at.x, at.y - size * 0.8f);
    g.drawText(text, box, centre ? juce::Justification::centred : juce::Justification::centredLeft, false);
}
inline void engravedText(Graphics& g, Colour fill, float plateLum, Point<float> at, const juce::String& text, float size, bool centre) {
    Point<float> dark, light; engraveOffsets(size, dark, light);
    float ad, al; engraveAlphas(plateLum, ad, al);
    const bool unfilled = std::abs(lum(fill) - plateLum) < 0.28f;
    const float low = juce::jlimit(0.0f, 1.0f, 2.0f - deviceScale());
    if (unfilled) {
        ad = std::min(ad + 0.12f + 0.25f * low, 1.0f); al = std::min(al + 0.12f + 0.30f * low, 1.0f);
        fill = darkerBy(fill, 0.10f + 0.22f * low);
        if (low > 0) textAt(g, black(0.30f * low), at + dark * 2.0f, text, size, centre);
    }
    textAt(g, white(al), at + light, text, size, centre);
    textAt(g, black(ad), at + dark, text, size, centre);
    if (plateLum >= 0.25f && plateLum < 0.75f && size >= 9.0f) textAt(g, black(ad * 0.28f), at + dark * 2.0f, text, size, centre);
    textAt(g, fill, at, text, size, centre);
}
} // namespace detail

inline void legendText(Graphics& g, Legend lg, Point<float> at, const juce::String& text, float size, bool centre = false) {
    using namespace detail;
    if (lg.kind == Legend::Silk) {
        const Colour ink = silkInk(lg.colour), bleed = ink.withAlpha(ink.getFloatAlpha() * 0.16f);
        textAt(g, bleed, {at.x + 0.45f, at.y + 0.25f}, text, size, centre);
        textAt(g, bleed, {at.x - 0.3f, at.y - 0.2f}, text, size, centre);
        textAt(g, ink, at, text, size, centre);
    } else engravedText(g, lg.colour, guessPlateLum(lg.colour), at, text, size, centre);
}
// Same, with the plate colour known so an engraving's contrast suits it.
inline void legendTextOn(Graphics& g, Legend lg, Colour plate, Point<float> at, const juce::String& text, float size, bool centre = false) {
    if (lg.kind == Legend::Engraved) detail::engravedText(g, lg.colour, detail::lum(plate), at, text, size, centre);
    else legendText(g, lg, at, text, size, centre);
}
inline void legendLine(Graphics& g, Legend lg, Point<float> a, Point<float> b, float width) {
    using namespace detail;
    if (lg.kind == Legend::Silk) {
        const Colour ink = silkInk(lg.colour);
        g.setColour(ink.withAlpha(ink.getFloatAlpha() * 0.18f)); g.drawLine({a, b}, width + 0.7f);
        g.setColour(ink); g.drawLine({a, b}, width);
    } else {
        Point<float> dark, light; engraveOffsets(width * 10.0f, dark, light);
        float ad, al; engraveAlphas(guessPlateLum(lg.colour), ad, al);
        g.setColour(white(al)); g.drawLine({a + light, b + light}, width);
        g.setColour(black(ad * 0.85f)); g.drawLine({a + dark, b + dark}, width);
        g.setColour(lg.colour); g.drawLine({a, b}, width * 0.85f);
    }
}
inline void legendDot(Graphics& g, Legend lg, Point<float> c, float radius) {
    using namespace detail;
    auto dot = [&](Point<float> p, float rad) { g.fillEllipse(Rectangle<float>(rad * 2, rad * 2).withCentre(p)); };
    if (lg.kind == Legend::Silk) {
        const Colour ink = silkInk(lg.colour);
        g.setColour(ink.withAlpha(ink.getFloatAlpha() * 0.35f)); dot(c, radius + 0.45f);
        g.setColour(ink); dot(c, radius);
    } else {
        Point<float> dark, light; engraveOffsets(radius * 8.0f, dark, light);
        float ad, al; engraveAlphas(guessPlateLum(lg.colour), ad, al);
        g.setColour(white(al)); dot(c + light, radius);
        g.setColour(black(ad * 0.85f)); dot(c + dark, radius);
        g.setColour(lg.colour); dot(c, radius * 0.9f);
    }
}

// A white printed line with a gap for a title, as used to group controls on a panel (silk ink).
inline void drawLegendLine(Graphics& g, Colour ink, float x0, float x1, float y, const juce::String& title, float fontSize = 9.5f) {
    juce::Font f(juce::FontOptions(fontSize, juce::Font::bold));
    const float tw = juce::GlyphArrangement::getStringWidth(f, title) + 10.0f, cx = 0.5f * (x0 + x1);
    const auto lg = Legend::silk(ink);
    const float w = juce::jlimit(0.7f, 1.4f, fontSize * 0.08f);
    if (cx - tw * 0.5f > x0) {
        legendLine(g, lg, {x0, y}, {cx - tw * 0.5f, y}, w);
        legendLine(g, lg, {cx + tw * 0.5f, y}, {x1, y}, w);
    }
    legendText(g, lg, {cx, y}, title, fontSize, true);
}

enum class ScaleMarks { Ticks, Dots };
// A printed knob scale. Angles run from straight up, clockwise, from -sweep to +sweep.
struct ScaleRing {
    Point<float> centre; float radius, sweep = 0.8f * juce::MathConstants<float>::pi;
    int nMajor, minorPerMajor; juce::StringArray labels;
    Legend legend = Legend::silk(Colour(0xffefebe0)); ScaleMarks marks = ScaleMarks::Ticks;
    float fontSize, tick;
    ScaleRing(Point<float> c, float rad, int major, int minor, juce::StringArray lab = {})
        : centre(c), radius(rad), nMajor(major), minorPerMajor(minor), labels(std::move(lab)),
          fontSize(juce::jlimit(7.0f, 13.0f, rad * 0.3f)), tick(juce::jlimit(2.5f, 7.0f, rad * 0.16f)) {}
    Point<float> at(float angle, float dist) const { return {centre.x + std::sin(angle) * dist, centre.y - std::cos(angle) * dist}; }
};
inline void drawScaleRing(Graphics& g, const ScaleRing& s) {
    const int n = std::max(s.nMajor, 2);
    auto angle = [&](float t) { return -s.sweep + 2.0f * s.sweep * t; };
    const int steps = (n - 1) * (s.minorPerMajor + 1);
    const float w = juce::jlimit(0.7f, 1.5f, s.tick * 0.22f);
    for (int i = 0; i <= steps; ++i) {
        const bool major = i % (s.minorPerMajor + 1) == 0;
        const float a = angle(float(i) / float(steps));
        if (s.marks == ScaleMarks::Ticks) {
            const float len = major ? s.tick : s.tick * 0.55f;
            legendLine(g, s.legend, s.at(a, s.radius), s.at(a, s.radius + len), major ? w : w * 0.75f);
        } else legendDot(g, s.legend, s.at(a, s.radius + s.tick * 0.5f), major ? std::max(s.tick * 0.26f, 0.9f) : std::max(s.tick * 0.15f, 0.6f));
    }
    const int m = s.labels.size();
    if (m == 0) return;
    const float dist = s.radius + s.tick + s.fontSize * 0.85f;
    for (int k = 0; k < m; ++k) {
        const float t = m == n ? float(k) / float(n - 1) : (m == 1 ? 0.5f : float(k) / float(m - 1));
        const float a = angle(t);
        legendText(g, s.legend, s.at(a, dist + std::abs(std::sin(a)) * s.fontSize * 0.28f * float(std::min(s.labels[k].length(), 4))), s.labels[k], s.fontSize, true);
    }
}
} // namespace goodlookinui::juce_adapter::hardware
