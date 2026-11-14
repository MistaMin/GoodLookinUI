// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <goodlookinui/Spectrum.h>
#include <functional>
#include <vector>
#include <tuple>
#include <algorithm>

namespace goodlookinui::juce_adapter {

// How the analyser looks and behaves. Saved with the host project (the host stores toString()).
struct SpectrumSettings {
    enum class Style { Standard, Multicolour, Bars, Spectrogram };
    enum class Palette { Accent, Amber, Cyan, Range };
    // spectrogramMap: 0 inferno (heat), 1 per-range shade, 2 magma, 3 ice
    bool on = true;
    int sizeIndex = 3;                    // index into spectrum::fftSizes (4096)
    Style style = Style::Standard;
    bool useOutput = true;                // false = analyse the input
    float tilt = 3.0f;                    // dB per octave around 1 kHz
    float fall = 24.0f;                   // dB per second
    float opacity = 0.85f;
    bool peakHold = true;
    int smoothing = 0;                    // 0 off, else octave denominator: 1, 3 (1/3 octave), 6, 12, 24
    int spectrogramMap = 0;               // 0 inferno, 1 palette shade, 2 magma, 3 ice
    std::uint32_t standard = 0xff8fe0c0;  // single colour for the Standard look
    std::uint32_t range[7] = {spectrum::defaultColours[0], spectrum::defaultColours[1], spectrum::defaultColours[2], spectrum::defaultColours[3],
                              spectrum::defaultColours[4], spectrum::defaultColours[5], spectrum::defaultColours[6]};
    Palette palette = Palette::Accent;    // colour source for the multicolour looks
    bool frame = true;                    // recessed screen with bevel, vignette and glass
    bool grid = true;                     // frequency / dB graticule
    bool labels = true;                   // graticule labels (needs grid)
    float bloom = 1.0f;                   // phosphor bloom strength, 0..2

    static const char* paletteCode(Palette p) { return p == Palette::Amber ? "amber" : p == Palette::Cyan ? "cyan" : p == Palette::Range ? "range" : "accent"; }
    static Palette paletteFromCode(const juce::String& c) { return c == "amber" ? Palette::Amber : c == "cyan" ? Palette::Cyan : c == "range" ? Palette::Range : Palette::Accent; }

    static const char* styleCode(Style s) { return s == Style::Multicolour ? "multi" : s == Style::Bars ? "bars" : s == Style::Spectrogram ? "gram" : "standard"; }
    static Style styleFromCode(const juce::String& c) { return c == "multi" ? Style::Multicolour : c == "bars" ? Style::Bars : c == "gram" ? Style::Spectrogram : Style::Standard; }
    static constexpr int smoothingChoices[6] = {0, 1, 3, 6, 12, 24};

    // The single trace colour of the Standard look.
    juce::Colour traceColour() const {
        return palette == Palette::Amber ? juce::Colour(0xffffb347) : palette == Palette::Cyan ? juce::Colour(0xff4fd8ff) : juce::Colour(0xff000000u | standard);
    }
    // Palette colour at t = 0 (lowest frequency) .. 1 (highest) for the multicolour looks; freq only matters for Range.
    juce::Colour rampColour(float t, float freq) const {
        static constexpr std::uint32_t amber[4] = {0xffe8452c, 0xffff8a1f, 0xffffc247, 0xffffe9a3}, cyan[4] = {0xff6a5cf5, 0xff2f9bff, 0xff35d6f0, 0xff9af5dc};
        auto four = [&](const std::uint32_t* c) { const float u = juce::jlimit(0.0f, 1.0f, t) * 3.0f; const int i = juce::jmin(int(u), 2);
            return juce::Colour(c[i]).interpolatedWith(juce::Colour(c[i + 1]), u - float(i)); };
        if (palette == Palette::Amber) return four(amber);
        if (palette == Palette::Cyan) return four(cyan);
        if (palette == Palette::Range) return juce::Colour(spectrum::colourAt(freq, range));
        float h, s, v; juce::Colour(standard).withAlpha(1.0f).getHSB(h, s, v);
        t = juce::jlimit(0.0f, 1.0f, t);
        float hh = h + 0.13f - 0.24f * t; hh -= std::floor(hh);
        return juce::Colour::fromHSV(hh, juce::jmin(1.0f, s * 1.2f + 0.22f - 0.12f * t), juce::jmin(1.0f, v * 1.04f), 1.0f);
    }

    // Round trip through a compact string stored in the plugin state.
    juce::String toString() const {
        juce::String s;
        s << (on ? 1 : 0) << ',' << sizeIndex << ',' << styleCode(style) << ',' << (useOutput ? 1 : 0) << ',' << tilt << ',' << fall << ',' << opacity << ',' << (peakHold ? 1 : 0)
          << ',' << juce::String::toHexString(int(standard));
        for (auto c : range) s << ',' << juce::String::toHexString(int(c));
        s << ',' << smoothing << ',' << spectrogramMap;
        const SpectrumSettings d;
        if (palette != d.palette || frame != d.frame || grid != d.grid || labels != d.labels || bloom != d.bloom)
            s << ',' << paletteCode(palette) << ',' << ((frame ? 1 : 0) | (grid ? 2 : 0) | (labels ? 4 : 0)) << ',' << bloom;
        return s;
    }
    static SpectrumSettings fromString(const juce::String& text) {
        SpectrumSettings d; auto t = juce::StringArray::fromTokens(text, ",", "");
        if (t.size() != 16 && t.size() != 18 && t.size() != 21) return d;      // 16 = saved before smoothing / spectrogram existed
        d.on = t[0].getIntValue() != 0; d.sizeIndex = juce::jlimit(0, int(std::size(spectrum::fftSizes)) - 1, t[1].getIntValue());
        d.style = styleFromCode(t[2]); d.useOutput = t[3].getIntValue() != 0;
        d.tilt = juce::jlimit(-6.0f, 9.0f, t[4].getFloatValue()); d.fall = juce::jlimit(2.0f, 200.0f, t[5].getFloatValue());
        d.opacity = juce::jlimit(0.1f, 1.0f, t[6].getFloatValue()); d.peakHold = t[7].getIntValue() != 0;
        d.standard = 0xff000000u | std::uint32_t(t[8].getHexValue32());
        for (int i = 0; i < 7; ++i) d.range[i] = 0xff000000u | std::uint32_t(t[9 + i].getHexValue32());
        if (t.size() >= 18) { d.smoothing = juce::jlimit(0, 48, t[16].getIntValue()); d.spectrogramMap = juce::jlimit(0, 3, t[17].getIntValue()); }
        if (t.size() == 21) {
            d.palette = paletteFromCode(t[18]); const int fl = t[19].getIntValue();
            d.frame = (fl & 1) != 0; d.grid = (fl & 2) != 0; d.labels = (fl & 4) != 0; d.bloom = juce::jlimit(0.0f, 2.0f, t[20].getFloatValue());
        } else {
            for (int i = 0; i < 7; ++i) if (d.range[i] != spectrum::defaultColours[i]) { d.palette = Palette::Range; break; }
        }
        return d;
    }
};

// Draws the analyser behind the EQ curve, on the same log-frequency axis. The FFT runs on the
// UI thread only (the audio thread just copies samples into a ring), only while the spectrum is
// on and visible, and only when new audio has arrived.
// Drawing helpers for the 3D 2.0 analyser look: recessed screen, graticule, bloomed traces, bars and a
// perceptual spectrogram. All stateless; SpectrumRenderer::paint() composes them.
namespace spectrum_draw {
using juce::Colour; using juce::Graphics; using juce::Rectangle; using juce::Point;
constexpr float dbTop = 0.0f, dbBottom = -90.0f, screenRadius = 4.0f;
inline const Colour gridInk{0xffb8cdd6};
inline constexpr float gridFreqs[10] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};

inline float yNorm(float db) { return juce::jlimit(0.0f, 1.0f, (db - dbBottom) / (dbTop - dbBottom)); }
inline float freqX(Rectangle<float> p, float f, float fMin, float fMax) { return p.getX() + std::log(f / fMin) / std::log(fMax / fMin) * p.getWidth(); }
inline int visibleColumns(int n, float fMin, float fMax, double sampleRate) {
    if (sampleRate <= 0.0) return n;
    const double nyq = std::min(double(fMax), sampleRate * 0.5);
    const auto nv = (long long) std::ceil(std::log(nyq / double(fMin)) / std::log(double(fMax / fMin)) * double(n));
    return int(std::clamp<long long>(nv, std::min(2, n), n));
}

// Perceptual ramps: map 0 inferno, 2 magma, 3 ice (anything else inferno). v is the level 0..255. Opaque 0xAARRGGBB.
inline std::uint32_t colourMap(int map, std::uint8_t v) {
    static constexpr std::uint8_t inferno[9][3] = {{0,0,4},{31,12,72},{85,15,109},{136,34,106},{186,54,85},{227,89,51},{249,140,10},{249,201,50},{252,255,164}};
    static constexpr std::uint8_t magma[9][3] = {{0,0,4},{28,16,68},{79,18,123},{129,37,129},{181,54,122},{229,80,100},{251,135,97},{254,194,135},{252,253,191}};
    static constexpr std::uint8_t ice[9][3] = {{1,3,8},{6,18,44},{10,38,84},{14,66,120},{20,100,152},{34,140,178},{62,184,200},{140,228,222},{236,255,246}};
    const auto& tab = map == 2 ? magma : map == 3 ? ice : inferno;
    const float u = float(v) / 255.0f * 8.0f; const int i = std::min(int(u), 7); const float f = u - float(i);
    auto ch = [&](int k) { return std::uint32_t(std::lround(float(tab[i][k]) + (float(tab[i + 1][k]) - float(tab[i][k])) * f)); };
    return 0xff000000u | (ch(0) << 16) | (ch(1) << 8) | ch(2);
}
// Contrast curve for spectrogram levels: mild gamma lift plus an S-curve, 0 and 255 fixed.
inline std::uint8_t levelCurve(std::uint8_t v) {
    const float u = std::pow(float(v) / 255.0f, 0.82f), t = std::clamp((u - 0.04f) / 0.92f, 0.0f, 1.0f), sc = t * t * (3.0f - 2.0f * t);
    return std::uint8_t(std::lround(std::clamp(u + (sc - u) * 0.7f, 0.0f, 1.0f) * 255.0f));
}

// Spectrogram picture: rows is row-major levels, `columns` wide, age 0 at the top. Columns above Nyquist are
// transparent (sampleRate 0 keeps every column). One texel per cell; drawing it scaled smooths it.
inline juce::Image spectrogramImage(const SpectrumSettings& st, const std::uint8_t* rows, int columns, int nrows, float fMin, float fMax, double sampleRate) {
    if (columns <= 0 || nrows <= 0 || fMin <= 0 || fMax <= fMin) return {};
    juce::Image img(juce::Image::ARGB, columns, nrows, true);
    juce::Image::BitmapData px(img, juce::Image::BitmapData::writeOnly);
    const int nv = visibleColumns(columns, fMin, fMax, sampleRate);
    const bool shade = st.spectrogramMap == 1;
    std::uint8_t curve[256]; for (int v = 0; v < 256; ++v) curve[v] = levelCurve(std::uint8_t(v));
    std::uint32_t lut[256] = {};
    if (!shade) for (int v = 0; v < 256; ++v) lut[v] = colourMap(st.spectrogramMap, curve[v]);
    std::vector<std::uint32_t> base;
    if (shade) { base.resize(std::size_t(columns)); for (int c = 0; c < columns; ++c) { const float t = (float(c) + 0.5f) / float(columns); base[std::size_t(c)] = st.rampColour(t, fMin * std::pow(fMax / fMin, t)).getARGB(); } }
    for (int r = 0; r < nrows; ++r) {
        auto* dst = reinterpret_cast<std::uint32_t*>(px.getLinePointer(r)); const std::uint8_t* src = rows + std::size_t(r) * std::size_t(columns);
        for (int c = 0; c < columns; ++c) dst[c] = c < nv ? (shade ? spectrum::shadeColour(base[std::size_t(c)], curve[src[c]]) : lut[src[c]]) : 0u;
    }
    return img;
}

// ---- screen ----
inline void fillRadial(Graphics& g, Point<float> c, float rx, float ry, std::initializer_list<std::pair<float, Colour>> stops, Rectangle<float> shape, float radius) {
    juce::ColourGradient grad(stops.begin()->second, 0, 0, (stops.end() - 1)->second, 1, 0, true);
    for (auto& s : stops) grad.addColour(s.first, s.second);
    g.setFillType(juce::FillType(grad).transformed(juce::AffineTransform::scale(rx, ry).translated(c.x, c.y)));
    g.fillRoundedRectangle(shape, radius);
}
inline void vGradient(Graphics& g, Rectangle<float> r, float radius, Colour top, Colour bottom) {
    g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false)); g.fillRoundedRectangle(r, radius);
}
inline void screenBack(Graphics& g, Rectangle<float> plot, const SpectrumSettings& st) {
    const float r = screenRadius; const Colour tint = st.traceColour().withAlpha(1.0f);
    const auto lip = plot.expanded(1.25f);
    vGradient(g, lip, r + 1.25f, Colour(0xd8000000), Colour(0x8c000000));
    { juce::ColourGradient c(Colour(0x00ffffff), 0, lip.getY(), Colour(0x38ffffff), 0, lip.getBottom(), false); c.addColour(0.9, Colour(0x00ffffff));
      g.setGradientFill(c); g.fillRoundedRectangle(lip, r + 1.25f); }
    vGradient(g, plot, r, Colour(0xff0c1215).interpolatedWith(tint, 0.035f), Colour(0xff05080a).interpolatedWith(tint, 0.015f));
    const auto c = plot.getCentre(); const Point<float> hc(c.x, plot.getY() + plot.getHeight() * 0.62f);
    g.saveState(); g.reduceClipRegion(plot.toNearestInt());
    fillRadial(g, hc, plot.getWidth() * 0.62f, plot.getHeight() * 0.75f, {{0.0f, tint.withAlpha(0.055f)}, {0.6f, tint.withAlpha(0.02f)}, {1.0f, tint.withAlpha(0.0f)}}, plot, r);
    const float strength = 0.42f;
    fillRadial(g, c, plot.getWidth() * 0.74f, plot.getHeight() * 0.86f, {{0.0f, Colour(0x00000000)}, {0.55f, Colour(0x00000000)}, {0.85f, Colour::fromFloatRGBA(0,0,0,strength * 0.45f)}, {1.0f, Colour::fromFloatRGBA(0,0,0,strength)}}, plot, r);
    g.restoreState();
}
inline void screenFront(Graphics& g, Rectangle<float> plot) {
    const float r = screenRadius;
    g.saveState(); g.reduceClipRegion(plot.toNearestInt());
    juce::Path clip; clip.addRoundedRectangle(plot, r); g.reduceClipRegion(clip);
    // glass sheen from the top-left
    { juce::ColourGradient c(Colour(0x0effffff), plot.getX(), plot.getY(), Colour(0x00ffffff), plot.getX() + plot.getWidth() * 0.5f, plot.getY() + plot.getHeight() * 0.55f, false);
      c.addColour(0.4, Colour(0x06ffffff)); g.setGradientFill(c); g.fillRect(plot); }
    // recess: wide soft shade from the top lip, tight contact shadow all round
    auto edge = [&](float depth, Colour col, float dy) {
        const float x0 = plot.getX(), x1 = plot.getRight(), y0 = plot.getY() + dy, y1 = plot.getBottom() + dy;
        g.setGradientFill(juce::ColourGradient(col, 0, y0, col.withAlpha(0.0f), 0, y0 + depth, false)); g.fillRect(plot.withTop(plot.getY()).withHeight(depth + dy));
        g.setGradientFill(juce::ColourGradient(col.withMultipliedAlpha(0.55f), x0, 0, col.withAlpha(0.0f), x0 + depth, 0, false)); g.fillRect(plot.withWidth(depth));
        g.setGradientFill(juce::ColourGradient(col.withMultipliedAlpha(0.55f), x1, 0, col.withAlpha(0.0f), x1 - depth, 0, false)); g.fillRect(plot.withLeft(x1 - depth));
        g.setGradientFill(juce::ColourGradient(col.withMultipliedAlpha(0.35f), 0, y1 - dy, col.withAlpha(0.0f), 0, y1 - dy - depth, false)); g.fillRect(plot.withTop(plot.getBottom() - depth));
    };
    edge(6.0f, Colour(0xc8000000), 3.0f);
    edge(1.6f, Colour(0x99000000), 0.6f);
    g.restoreState();
    g.setColour(Colour(0x70000000)); g.drawRoundedRectangle(plot.reduced(0.25f), r, 0.5f);
}

// ---- graticule ----
inline float dbStep(Rectangle<float> plot) {
    const float perDb = plot.getHeight() / (dbTop - dbBottom);
    for (float s : {10.0f, 20.0f, 30.0f}) if (s * perDb >= 24.0f) return s;
    return 45.0f;
}
inline void gridLines(Graphics& g, Rectangle<float> plot, float fMin, float fMax, bool db, float gain) {
    const float top = plot.getY(), bottom = plot.getBottom(), decade = plot.getWidth() / std::log10(fMax / fMin);
    if (decade >= 90.0f && db) {
        g.setColour(gridInk.withAlpha(0.028f * gain));
        for (float d = 10.0f; d < fMax; d *= 10.0f)
            for (float m : {3.0f, 4.0f, 6.0f, 7.0f, 8.0f, 9.0f}) { const float x = freqX(plot, d * m, fMin, fMax); if (x > plot.getX() + 1 && x < plot.getRight() - 1) g.drawLine(x, top, x, bottom, 0.5f); }
    }
    for (float f : gridFreqs) {
        const float x = freqX(plot, f, fMin, fMax); if (x <= plot.getX() + 1 || x >= plot.getRight() - 1) continue;
        const bool dec = f == 100.0f || f == 1000.0f || f == 10000.0f;
        g.setColour(gridInk.withAlpha((dec ? 0.105f : 0.065f) * gain)); g.drawLine(x, top, x, bottom, 0.5f);
    }
    if (db) {
        const float step = dbStep(plot); g.setColour(gridInk.withAlpha(0.065f * gain));
        for (float v = dbTop - step; v > dbBottom + 0.5f; v -= step) { const float y = std::round(plot.getBottom() - yNorm(v) * plot.getHeight()) + 0.25f; g.drawLine(plot.getX(), y, plot.getRight(), y, 0.5f); }
    }
}
inline void gridLabels(Graphics& g, Rectangle<float> plot, float fMin, float fMax, bool db, float inkAlpha) {
    if (plot.getWidth() < 120.0f || plot.getHeight() < 48.0f) return;
    const float size = plot.getHeight() >= 260.0f && plot.getWidth() >= 520.0f ? 8.5f : 7.0f;
    g.setFont(juce::Font(juce::FontOptions(size)));
    const float y = plot.getBottom() - size * 0.5f - 2.5f; float lastEnd = -1e9f;
    auto text = [&](const juce::String& s, float x, float yc, float w, bool right, Colour ink) {
        g.setColour(ink); g.drawText(s, Rectangle<float>(x, yc - size * 0.6f, w + 2.0f, size * 1.2f), right ? juce::Justification::centredRight : juce::Justification::centredLeft, false); };
    for (float f : gridFreqs) {
        const float x = freqX(plot, f, fMin, fMax); if (x < plot.getX() - 0.5f || x > plot.getRight()) continue;
        const juce::String s = f >= 1000.0f ? juce::String(int(f / 1000.0f)) + "k" : juce::String(int(f));
        const float w = float(s.length()) * size * 0.56f, at = std::max(x, plot.getX() + 1.0f) + 2.5f;
        if (at < lastEnd + 3.0f || at + w > plot.getRight() - 3.0f) continue;
        lastEnd = at + w; text(s, at, y, w, false, gridInk.withAlpha(inkAlpha));
    }
    if (db) {
        const float step = dbStep(plot);
        for (float v = dbTop - step; v > dbBottom + step * 1.25f; v -= step) {
            const float ly = std::round(plot.getBottom() - yNorm(v) * plot.getHeight()) - size * 0.5f - 1.0f;
            if (ly > plot.getY() + size && ly < y - size) {
                const juce::String s{int(v)}; const float w = float(s.length()) * size * 0.58f, ax = plot.getRight() - size * 1.35f - 2.0f;
                g.setColour(Colour(0x73050809)); g.fillRoundedRectangle(ax - float(s.length()) * size * 0.29f - 2.0f, ly - size * 0.55f - 0.5f, w + 4.0f, size * 1.1f + 1.0f, 2.0f);
                text(s, ax - w * 0.5f, ly, w, true, gridInk.withAlpha(std::max(inkAlpha * 0.9f, 0.42f)));
            }
        }
    }
}

// ---- traces ----
// Monotone cubic through equally spaced values: rounds joints without overshooting a peak.
struct Curve {
    const float* v; int n; std::vector<float> m;
    Curve(const float* values, int count) : v(values), n(count), m(std::size_t(count), 0.0f) {
        for (int k = 0; k < n; ++k) {
            const float d0 = k > 0 ? v[k] - v[k - 1] : 0.0f, d1 = k + 1 < n ? v[k + 1] - v[k] : 0.0f;
            m[std::size_t(k)] = k == 0 ? d1 : k + 1 == n ? d0 : d0 * d1 <= 0.0f ? 0.0f : 2.0f * d0 * d1 / (d0 + d1);
        }
    }
    float at(float u) const {
        if (n == 1 || !(u > 0.0f)) return v[0];
        if (u >= float(n - 1)) return v[n - 1];
        const int k = int(u); const float t = u - float(k), t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * v[k] + (t3 - 2 * t2 + t) * m[std::size_t(k)] + (-2 * t3 + 3 * t2) * v[k + 1] + (t3 - t2) * m[std::size_t(k + 1)];
    }
};

inline void paintTrace(Graphics& g, Rectangle<float> plot, const SpectrumSettings& st, const float* columns, const float* hold, int n, int nv, float fMin, float fMax) {
    const float alpha = juce::jlimit(0.0f, 1.0f, st.opacity), step = plot.getWidth() / float(n);
    const bool multi = st.style == SpectrumSettings::Style::Multicolour;
    const float xEnd = nv >= n ? plot.getWidth() : (float(nv) - 0.5f) * step;
    if (!(xEnd > 0.0f)) return;
    Curve curve(columns, nv);
    auto colourAtX = [&](float x) { if (!multi) return st.traceColour().withAlpha(1.0f); const float t = juce::jlimit(0.0f, 1.0f, x / plot.getWidth()); return st.rampColour(t, fMin * std::pow(fMax / fMin, t)); };

    // fill + bloom image (two texels per unit while cheap)
    const float area = plot.getWidth() * plot.getHeight();
    const float res = area * 4.0f <= 800000.0f ? 2.0f : juce::jlimit(0.5f, 2.0f, std::sqrt(800000.0f / area));
    const int w = std::max(2, int(std::ceil(plot.getWidth() * res))), h = std::max(2, int(std::ceil(plot.getHeight() * res)));
    const float sx = float(w) / plot.getWidth(), sy = float(h) / plot.getHeight();
    const int last = std::min(int(std::ceil(xEnd * sx)), w);
    if (last >= 1) {
        std::vector<float> yc(static_cast<std::size_t>(last));
        for (int i = 0; i < last; ++i) yc[std::size_t(i)] = (1.0f - yNorm(curve.at((float(i) + 0.5f) / sx / step - 0.5f))) * float(h);
        struct S { float p; float a; };
        const S fillStops[4] = {multi ? S{0.0f, 0xa6 / 255.f} : S{0.0f, 0x8c / 255.f}, multi ? S{0.3f, 0x70 / 255.f} : S{0.3f, 0x5c / 255.f}, multi ? S{0.65f, 0x30 / 255.f} : S{0.65f, 0x26 / 255.f}, S{1.0f, 0.0f}};
        auto stopAt = [&](float t) { t = juce::jlimit(0.0f, 1.0f, t); for (int k = 0; k < 3; ++k) if (t <= fillStops[k + 1].p) { const float u = (t - fillStops[k].p) / (fillStops[k + 1].p - fillStops[k].p); return fillStops[k].a + (fillStops[k + 1].a - fillStops[k].a) * u; } return 0.0f; };
        float crest = float(h); for (float y : yc) crest = std::min(crest, y); crest = std::max(crest, 0.0f);
        const float span = std::max(float(h) - crest, 1.0f), body = 0.5f + 0.5f * std::sqrt(span / float(h));
        std::vector<float> rowFill(static_cast<std::size_t>(h)); for (int j = 0; j < h; ++j) rowFill[std::size_t(j)] = body * stopAt((float(j) + 0.5f - crest) / span);
        const float bloom = juce::jlimit(0.0f, 2.0f, st.bloom), sigma = 2.1f * sy, skirt = 10.0f * sy, reach = std::max(std::ceil(sigma * 3.4f), 1.0f);
        const int win = int(reach);
        auto mid = [&](int a, int b) { return (yc[std::size_t(a)] + yc[std::size_t(b)]) * 0.5f; };
        std::vector<float> lo(static_cast<std::size_t>(last)), hi(static_cast<std::size_t>(last));
        for (int i = 0; i < last; ++i) { const int p = std::max(i - 1, 0), q = std::min(i + 1, last - 1);
            lo[std::size_t(i)] = std::min({yc[std::size_t(i)], mid(i, p), mid(i, q)}); hi[std::size_t(i)] = std::max({yc[std::size_t(i)], mid(i, p), mid(i, q)}); }
        juce::Image img(juce::Image::ARGB, w, h, true);
        { juce::Image::BitmapData px(img, juce::Image::BitmapData::writeOnly);
          for (int i = 0; i < std::min(last + win, w); ++i) {
            const bool inside = i < last; const Colour col = colourAtX((float(i) + 0.5f) / sx), hot = col.brighter(0.65f);
            const float cr = col.getRed(), cg = col.getGreen(), cb = col.getBlue(), hr = hot.getRed(), hg = hot.getGreen(), hb = hot.getBlue();
            const float y = yc[std::size_t(std::min(i, last - 1))], lift = juce::jlimit(0.0f, 1.0f, (float(h) - y) / (5.0f * sy));
            const float edge = !inside ? 0.0f : nv >= n ? 1.0f : juce::jlimit(0.0f, 1.0f, (xEnd * sx - float(i)) / (8.0f * sx));
            const int k0 = std::max(i - win, 0), k1 = std::min(i + win, last - 1);
            float top = 1e30f, bot = -1e30f;
            if (bloom > 0.0f && lift > 0.0f && k0 <= k1) for (int k = k0; k <= k1; ++k) { top = std::min(top, lo[std::size_t(k)]); bot = std::max(bot, hi[std::size_t(k)]); }
            const float g0 = std::max(std::floor(top - reach), 0.0f), g1 = bot + reach, f0 = inside ? std::max(std::floor(y - 0.5f), 0.0f) : float(h);
            const int j0 = int(std::min({g0, f0, float(h)}));
            for (int j = j0; j < h; ++j) {
                const float yy = float(j) + 0.5f, below = yy - y, cov = juce::jlimit(0.0f, 1.0f, below + 0.5f);
                const float af = inside ? cov * alpha * edge * (rowFill[std::size_t(j)] + 0.20f * std::exp(-std::max(below, 0.0f) / skirt) * lift) : 0.0f;
                float core = 0.0f, gl = 0.0f;
                if (yy >= g0 && yy <= g1) {
                    float best = reach * reach;
                    for (int k = k0; k <= k1; ++k) { const float dx = float(k - i), dx2 = dx * dx; if (dx2 >= best) continue;
                        const float dy = std::max({lo[std::size_t(k)] - yy, yy - hi[std::size_t(k)], 0.0f}), d2 = dx2 + dy * dy; if (d2 < best) best = d2; }
                    if (best < reach * reach) { const float d = std::sqrt(best), fade = 1.0f - d / reach; core = std::exp(-(d / sigma) * (d / sigma));
                        gl = lift * bloom * alpha * (0.44f * core + 0.20f * std::exp(-d / (1.6f * sigma)) * fade * fade); }
                } else if (!inside && yy > g1) break;
                if (af <= 0.002f && gl <= 0.002f) continue;
                const float k = 0.55f * core;
                auto ch = [&](float c, float hc) { return std::uint32_t(std::min(std::round(c * af + (c + (hc - c) * k) * gl), 255.0f)); };
                const auto a = std::uint32_t(std::min(std::round(af * 255.0f), 255.0f));
                reinterpret_cast<std::uint32_t*>(px.getLinePointer(j))[i] = (a << 24) | (ch(cr, hr) << 16) | (ch(cg, hg) << 8) | ch(cb, hb);
            }
          } }
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.setOpacity(1.0f);
        g.drawImage(img, plot, juce::RectanglePlacement::stretchToFit);
    }

    // vector traces: peak hold first, then the line with a hot core
    const int count = std::max(int(std::ceil(xEnd)), 2);
    auto xs = [&](int k) { return xEnd * float(k) / float(count); };
    auto poly = [&](const Curve& c, std::vector<Point<float>>& pts) { pts.clear(); for (int k = 0; k <= count; ++k) pts.emplace_back(plot.getX() + xs(k), plot.getBottom() - yNorm(c.at(xs(k) / step - 0.5f)) * plot.getHeight()); };
    auto strokeRange = [&](const std::vector<Point<float>>& pts, int a, int b, Colour c, float width) {
        juce::Path p; p.startNewSubPath(pts[std::size_t(a)]); for (int i = a + 1; i <= b; ++i) p.lineTo(pts[std::size_t(i)]);
        g.setColour(c); g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); };
    std::vector<Point<float>> pts;
    if (st.peakHold && hold) {
        const int m = nv;
        if (m >= 2) { Curve hc(hold, m); poly(hc, pts);
            const Colour ink = multi ? Colour(0xffffffff) : st.traceColour().withAlpha(1.0f).brighter(0.55f);
            strokeRange(pts, 0, int(pts.size()) - 1, ink.withAlpha(0.28f * alpha), 0.7f); }
    }
    poly(curve, pts);
    if (multi) {
        const int run = 6;
        for (int i = 0; i + 1 < int(pts.size());) { const int end = std::min(i + run, int(pts.size()) - 1);
            const Colour c = colourAtX(pts[std::size_t((i + end) / 2)].x - plot.getX());
            strokeRange(pts, i, end, c.withAlpha(alpha), 1.5f); strokeRange(pts, i, end, c.brighter(0.7f).withAlpha(alpha * 0.8f), 0.6f); i = end; }
    } else {
        const Colour c = st.traceColour().withAlpha(1.0f);
        strokeRange(pts, 0, int(pts.size()) - 1, c.withAlpha(alpha), 1.5f); strokeRange(pts, 0, int(pts.size()) - 1, c.brighter(0.7f).withAlpha(alpha * 0.8f), 0.6f);
    }
}

// soft halo approximated by stacked translucent rounded rectangles
inline void glowRound(Graphics& g, Rectangle<float> r, float radius, Colour c, float reach) {
    for (int i = 4; i >= 1; --i) { const float k = float(i) / 4.0f; g.setColour(c.withMultipliedAlpha(0.28f * (1.0f - k * 0.55f))); g.fillRoundedRectangle(r.expanded(reach * k * 0.5f), radius + reach * k * 0.5f); }
}
inline void paintBars(Graphics& g, Rectangle<float> plot, const SpectrumSettings& st, const float* columns, const float* hold, int n, int nv, float fMin, float fMax) {
    const float alpha = juce::jlimit(0.0f, 1.0f, st.opacity), step = plot.getWidth() / float(n);
    const int group = std::max(1, int(std::ceil(6.5f / step)));
    const float pitch = step * float(group), gap = juce::jlimit(1.0f, 2.5f, pitch * 0.2f), bw = std::max(pitch - gap, 0.5f), radius = std::min(bw * 0.22f, 1.5f), bloom = juce::jlimit(0.0f, 2.0f, st.bloom);
    for (int c = 0; c + group <= nv; c += group) {
        float m = spectrum::floorDb, pk = spectrum::floorDb;
        for (int k = 0; k < group; ++k) { m = std::max(m, columns[c + k]); if (hold) pk = std::max(pk, hold[c + k]); }
        const float t = (float(c) + float(group) * 0.5f) / float(n); const Colour col = st.rampColour(t, fMin * std::pow(fMax / fMin, t));
        const float x = plot.getX() + float(c) * step + gap * 0.5f;
        g.setGradientFill(juce::ColourGradient(col.withAlpha(0.035f), 0, plot.getY(), col.withAlpha(0.06f), 0, plot.getBottom(), false)); g.fillRect(x, plot.getY(), bw, plot.getHeight());
        const float level = yNorm(m);
        if (m > spectrum::floorDb + 1.0f && level > 0.0f) {
            const float y = plot.getBottom() - level * plot.getHeight(); const Rectangle<float> bar(x, y, bw, plot.getBottom() - y + radius);
            if (bloom > 0.0f) { const float reach = 5.0f + 6.0f * level; glowRound(g, bar.withHeight(reach * 2.0f).expanded(0.0f), reach * 0.5f, col.withAlpha((0.10f + 0.34f * level) * std::min(bloom, 1.5f) * alpha), reach); }
            juce::ColourGradient grad(col.brighter(0.45f).withAlpha(alpha), 0, bar.getY(), col.darker(0.6f).withAlpha(alpha * 0.4f), 0, bar.getBottom(), false);
            grad.addColour(0.08, col.withAlpha(alpha)); grad.addColour(0.55, col.darker(0.25f).withAlpha(alpha * 0.8f));
            g.setGradientFill(grad); g.fillRoundedRectangle(bar, radius);
            g.setGradientFill(juce::ColourGradient(Colour::fromFloatRGBA(1, 1, 1, 0.16f * alpha), x, 0, Colour::fromFloatRGBA(1, 1, 1, 0.0f), x + std::max(bw * 0.18f, 0.5f), 0, false));
            g.fillRect(x, y + radius, std::max(bw * 0.18f, 0.5f), std::max(plot.getBottom() - y - radius, 0.0f));
        }
        if (st.peakHold && hold && pk > spectrum::floorDb + 1.0f && yNorm(pk) > 0.0f) {
            const float y = plot.getBottom() - yNorm(pk) * plot.getHeight(); const Colour cap = col.brighter(0.5f);
            if (bloom > 0.0f) glowRound(g, Rectangle<float>(x, y - 1.5f, bw, 3.0f), 1.5f, cap.withAlpha(0.22f * std::min(bloom, 1.5f) * alpha), 6.0f);
            g.setColour(cap.withAlpha(alpha)); g.fillRect(x, y - 0.75f, bw, 1.25f);
        }
    }
}

// The whole display from already-reduced columns. hold may be null. gramRows is row-major levels, n wide,
// gramRowCount tall (age 0 at the top); or pass a ready gramImage. sampleRate 0 draws every column.
inline void paintCore(Graphics& g, Rectangle<float> plot, const SpectrumSettings& st, const float* columns, const float* hold, int n,
                      const std::uint8_t* gramRows, int gramRowCount, const juce::Image* gramImage, float fMin, float fMax, double sampleRate) {
    if (!st.on || n <= 0 || fMin <= 0 || fMax <= fMin || !(plot.getWidth() > 0 && plot.getHeight() > 0)) return;
    const float alpha = juce::jlimit(0.0f, 1.0f, st.opacity); const int nv = visibleColumns(n, fMin, fMax, sampleRate);
    const bool isGram = st.style == SpectrumSettings::Style::Spectrogram;
    if (st.frame) screenBack(g, plot, st);
    g.saveState();
    { juce::Path clip; clip.addRoundedRectangle(plot, st.frame ? screenRadius : 0.0f); g.reduceClipRegion(clip); }
    if (st.grid && !isGram) gridLines(g, plot, fMin, fMax, true, 1.0f);
    if (isGram) {
        juce::Image built; const juce::Image* img = gramImage;
        if (!img && gramRows) { built = spectrogramImage(st, gramRows, n, gramRowCount, fMin, fMax, sampleRate); img = &built; }
        if (img && img->isValid()) {
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality); g.setOpacity(alpha);
            g.drawImage(*img, plot, juce::RectanglePlacement::stretchToFit);
            if (st.grid) { gridLines(g, plot, fMin, fMax, false, 0.9f);
                if (st.labels) { g.setGradientFill(juce::ColourGradient(Colour(0x00000000), 0, plot.getBottom() - 16.0f, Colour(0x8c000000), 0, plot.getBottom(), false)); g.fillRect(plot.getX(), plot.getBottom() - 16.0f, plot.getWidth(), 16.0f); } }
        }
    } else if (st.style == SpectrumSettings::Style::Bars) paintBars(g, plot, st, columns, hold, n, nv, fMin, fMax);
    else paintTrace(g, plot, st, columns, hold, n, nv, fMin, fMax);
    if (st.grid && st.labels) {
        const bool bars = st.style == SpectrumSettings::Style::Bars;
        if (bars) { juce::ColourGradient c(Colour(0x00000000), 0, plot.getBottom() - 17.0f, Colour(0xd2030506), 0, plot.getBottom(), false); c.addColour(0.45, Colour(0xa0030506));
            g.setGradientFill(c); g.fillRect(plot.getX(), plot.getBottom() - 17.0f, plot.getWidth(), 17.0f); }
        gridLabels(g, plot, fMin, fMax, !isGram, bars || isGram ? 0.82f : 0.5f);
    }
    g.restoreState();
    if (st.frame) screenFront(g, plot);
}

// Only the recessed screen and graticule, for hosts that draw their own trace on top. Follow with paintScreenFront.
inline void paintScreenBack(Graphics& g, Rectangle<float> plot, const SpectrumSettings& st, float fMin, float fMax) {
    if (fMin <= 0 || fMax <= fMin) return;
    if (st.frame) screenBack(g, plot, st);
    if (st.grid) { g.saveState(); juce::Path clip; clip.addRoundedRectangle(plot, st.frame ? screenRadius : 0.0f); g.reduceClipRegion(clip);
        gridLines(g, plot, fMin, fMax, true, 1.0f); if (st.labels) gridLabels(g, plot, fMin, fMax, true, 0.5f); g.restoreState(); }
}
inline void paintScreenFront(Graphics& g, Rectangle<float> plot, const SpectrumSettings& st) { if (st.frame) screenFront(g, plot); }
} // namespace spectrum_draw

// Draws the analyser behind the EQ curve, on the same log-frequency axis. The FFT runs on the
// UI thread only (the audio thread just copies samples into a ring), only while the spectrum is
// on and visible, and only when new audio has arrived.
class SpectrumRenderer {
public:
    SpectrumRenderer(const spectrum::SampleRing& inputRing, const spectrum::SampleRing& outputRing, std::function<double()> sampleRate)
        : in(inputRing), out(outputRing), getRate(std::move(sampleRate)) { apply(settings); }

    const SpectrumSettings& getSettings() const { return settings; }
    void setSettings(const SpectrumSettings& s) { settings = s; ++settingsGen; apply(s); }
    bool isOn() const { return settings.on; }
    const spectrum::Analyser& analyser() const { return analyser_; }
    float lastUpdateMs() const { return updateMs; }
    const std::vector<float>& columns() const { return cols; }
    std::uint64_t updateTick() const { return tick; }

    // One UI tick (call about 30 times a second while visible).
    void update(float seconds) {
        if (!settings.on) return;
        const double rate = getRate();
        if (rate > 1000.0 && rate != analyser_.sampleRate()) analyser_.setSampleRate(rate);
        const auto t0 = juce::Time::getHighResolutionTicks();
        analyser_.analyse(settings.useOutput ? out : in);
        analyser_.decay(seconds);
        ++tick;
        if (settings.style == SpectrumSettings::Style::Spectrogram && lastN > 0 && hist.columns() == lastN) { computeColumns(lastN, lastFMin, lastFMax); hist.push(cols.data(), gramFloorDb, gramCeilDb); }
        updateMs = float(juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0) * 1000.0);
    }

    // Reduces the analyser to `n` display columns (with optional smoothing) into cols / holdCols.
    void computeColumns(int n, float fMin, float fMax) {
        if (int(cols.size()) != n) { cols.assign(std::size_t(n), spectrum::floorDb); holdCols.assign(std::size_t(n), spectrum::floorDb); }
        spectrum::toColumns(analyser_.displayDb().data(), analyser_.numBins(), analyser_.sampleRate(), n, fMin, fMax, cols.data());
        spectrum::toColumns(analyser_.holdDb().data(), analyser_.numBins(), analyser_.sampleRate(), n, fMin, fMax, holdCols.data());
        if (settings.smoothing > 0) {
            if (int(scratch.size()) != n) { scratch.assign(std::size_t(n), 0.0f); prefix.assign(std::size_t(n) + 1, 0.0); smoothed.assign(std::size_t(n), 0.0f); }
            const float octaves = float(std::log2(double(fMax) / double(fMin)));
            spectrum::smoothColumns(cols.data(), smoothed.data(), n, octaves, settings.smoothing, scratch.data(), prefix.data()); cols = smoothed;
            spectrum::smoothColumns(holdCols.data(), smoothed.data(), n, octaves, settings.smoothing, scratch.data(), prefix.data()); holdCols = smoothed;
        }
    }

    // dB scale of the display: top .. bottom of the plot.
    static constexpr float topDb = 0.0f, bottomDb = -90.0f;

    void paint(juce::Graphics& g, juce::Rectangle<float> plot, float fMin = 20.0f, float fMax = 30000.0f) {
        if (!settings.on || plot.getWidth() < 8.0f) return;
        const int n = juce::jlimit(8, 2048, int(plot.getWidth()));
        computeColumns(n, fMin, fMax);
        lastN = n; lastFMin = fMin; lastFMax = fMax;
        const double sr = analyser_.sampleRate();
        if (settings.style == SpectrumSettings::Style::Spectrogram) {
            const int rows = juce::jlimit(8, 256, int(plot.getHeight()));
            if (hist.columns() != n || hist.rows() != rows) { hist.configure(n, rows); gramValid = false; }
            const auto key = std::make_tuple(tick, n, rows, settingsGen, fMin, fMax, sr);
            if (!gramValid || key != gramKey) {
                const int filled = std::min(hist.filled(), rows);
                flat.assign(std::size_t(n) * std::size_t(rows), 0);
                for (int r = 0; r < filled; ++r) std::copy_n(hist.row(r), n, flat.begin() + std::ptrdiff_t(r) * n);
                gram = spectrum_draw::spectrogramImage(settings, flat.data(), n, rows, fMin, fMax, sr); gramKey = key; gramValid = true;
            }
            spectrum_draw::paintCore(g, plot, settings, cols.data(), nullptr, n, nullptr, 0, &gram, fMin, fMax, sr);
        } else {
            spectrum_draw::paintCore(g, plot, settings, cols.data(), holdCols.data(), n, nullptr, 0, nullptr, fMin, fMax, sr);
        }
    }

    // Stateless entry points for hosts that reduce the columns themselves.
    static void paintColumns(juce::Graphics& g, juce::Rectangle<float> plot, const SpectrumSettings& s, const std::vector<float>& columns, const std::vector<float>* hold,
                             const std::uint8_t* gramRows, int gramRowCount, float fMin, float fMax, double sampleRate) {
        spectrum_draw::paintCore(g, plot, s, columns.data(), hold ? hold->data() : nullptr, int(columns.size()), gramRows, gramRowCount, nullptr, fMin, fMax, sampleRate);
    }
    static void paintScreenBack(juce::Graphics& g, juce::Rectangle<float> plot, const SpectrumSettings& s, float fMin, float fMax) { spectrum_draw::paintScreenBack(g, plot, s, fMin, fMax); }
    static void paintScreenFront(juce::Graphics& g, juce::Rectangle<float> plot, const SpectrumSettings& s) { spectrum_draw::paintScreenFront(g, plot, s); }

private:
    void apply(const SpectrumSettings& s) {
        const int size = spectrum::fftSizes[juce::jlimit(0, int(std::size(spectrum::fftSizes)) - 1, s.sizeIndex)];
        const double rate = getRate ? getRate() : 48000.0;
        if (size != analyser_.fftSize() || (rate > 1000.0 && rate != analyser_.sampleRate())) analyser_.configure(size, rate > 1000.0 ? rate : 48000.0);
        analyser_.setTilt(s.tilt); analyser_.setFall(s.fall);
    }
    const spectrum::SampleRing& in; const spectrum::SampleRing& out;
    std::function<double()> getRate;
    SpectrumSettings settings; spectrum::Analyser analyser_;
    std::vector<float> cols, holdCols, scratch, smoothed; std::vector<double> prefix; float updateMs = 0.0f;
    spectrum::SpectrogramHistory hist; juce::Image gram; std::vector<std::uint8_t> flat; std::uint64_t tick = 0, settingsGen = 0; int lastN = 0; float lastFMin = 20.0f, lastFMax = 30000.0f;
    std::tuple<std::uint64_t, int, int, std::uint64_t, float, float, double> gramKey; bool gramValid = false;
    static constexpr float gramFloorDb = -90.0f, gramCeilDb = -10.0f;
};

} // namespace goodlookinui::juce_adapter
