// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <goodlookinui/Spectrum.h>
#include <functional>
#include <vector>

namespace goodlookinui::juce_adapter {

// How the analyser looks and behaves. Saved with the project (see PluginEditor).
struct SpectrumSettings {
    enum class Style { Standard, Multicolour, Bars };
    bool on = true;
    int sizeIndex = 3;                    // index into spectrum::fftSizes (4096)
    Style style = Style::Standard;
    bool useOutput = true;                // false = analyse the input
    float tilt = 3.0f;                    // dB per octave around 1 kHz
    float fall = 24.0f;                   // dB per second
    float opacity = 0.85f;
    bool peakHold = true;
    std::uint32_t standard = 0xff8fe0c0;  // single colour for the Standard look
    std::uint32_t range[7] = {spectrum::defaultColours[0], spectrum::defaultColours[1], spectrum::defaultColours[2], spectrum::defaultColours[3],
                              spectrum::defaultColours[4], spectrum::defaultColours[5], spectrum::defaultColours[6]};

    static const char* styleCode(Style s) { return s == Style::Multicolour ? "multi" : s == Style::Bars ? "bars" : "standard"; }
    static Style styleFromCode(const juce::String& c) { return c == "multi" ? Style::Multicolour : c == "bars" ? Style::Bars : Style::Standard; }

    // Round trip through a compact string stored in the plugin state.
    juce::String toString() const {
        juce::String s;
        s << (on ? 1 : 0) << ',' << sizeIndex << ',' << styleCode(style) << ',' << (useOutput ? 1 : 0) << ',' << tilt << ',' << fall << ',' << opacity << ',' << (peakHold ? 1 : 0)
          << ',' << juce::String::toHexString(int(standard));
        for (auto c : range) s << ',' << juce::String::toHexString(int(c));
        return s;
    }
    static SpectrumSettings fromString(const juce::String& text) {
        SpectrumSettings d; auto t = juce::StringArray::fromTokens(text, ",", "");
        if (t.size() != 16) return d;
        d.on = t[0].getIntValue() != 0; d.sizeIndex = juce::jlimit(0, int(std::size(spectrum::fftSizes)) - 1, t[1].getIntValue());
        d.style = styleFromCode(t[2]); d.useOutput = t[3].getIntValue() != 0;
        d.tilt = juce::jlimit(-6.0f, 9.0f, t[4].getFloatValue()); d.fall = juce::jlimit(2.0f, 200.0f, t[5].getFloatValue());
        d.opacity = juce::jlimit(0.1f, 1.0f, t[6].getFloatValue()); d.peakHold = t[7].getIntValue() != 0;
        d.standard = 0xff000000u | std::uint32_t(t[8].getHexValue32());
        for (int i = 0; i < 7; ++i) d.range[i] = 0xff000000u | std::uint32_t(t[9 + i].getHexValue32());
        return d;
    }
};

// Draws the analyser behind the EQ curve, on the same log-frequency axis. The FFT runs on the
// UI thread only (the audio thread just copies samples into a ring), only while the spectrum is
// on and visible, and only when new audio has arrived.
class SpectrumRenderer {
public:
    SpectrumRenderer(const spectrum::SampleRing& inputRing, const spectrum::SampleRing& outputRing, std::function<double()> sampleRate)
        : in(inputRing), out(outputRing), getRate(std::move(sampleRate)) { apply(settings); }

    const SpectrumSettings& getSettings() const { return settings; }
    void setSettings(const SpectrumSettings& s) { settings = s; apply(s); }
    bool isOn() const { return settings.on; }
    const spectrum::Analyser& analyser() const { return analyser_; }
    float lastUpdateMs() const { return updateMs; }
    const std::vector<float>& columns() const { return cols; }

    // One UI tick (call about 30 times a second while visible).
    void update(float seconds) {
        if (!settings.on) return;
        const double rate = getRate();
        if (rate > 1000.0 && rate != analyser_.sampleRate()) analyser_.setSampleRate(rate);
        const auto t0 = juce::Time::getHighResolutionTicks();
        analyser_.analyse(settings.useOutput ? out : in);
        analyser_.decay(seconds);
        updateMs = float(juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0) * 1000.0);
    }

    // dB scale of the display: top .. bottom of the plot.
    static constexpr float topDb = 0.0f, bottomDb = -90.0f;

    void paint(juce::Graphics& g, juce::Rectangle<float> plot, float fMin = 20.0f, float fMax = 30000.0f) {
        if (!settings.on) return;
        const int n = juce::jlimit(8, 2048, int(plot.getWidth()));
        if (int(cols.size()) != n) { cols.assign(std::size_t(n), spectrum::floorDb); holdCols.assign(std::size_t(n), spectrum::floorDb); }
        spectrum::toColumns(analyser_.displayDb().data(), analyser_.numBins(), analyser_.sampleRate(), n, fMin, fMax, cols.data());
        spectrum::toColumns(analyser_.holdDb().data(), analyser_.numBins(), analyser_.sampleRate(), n, fMin, fMax, holdCols.data());
        const float step = plot.getWidth() / float(n), alpha = settings.opacity;
        // the analyser has nothing above Nyquist: stop drawing there instead of dropping to the floor
        const int nv = juce::jlimit(2, n, int(std::ceil(std::log(std::min(double(fMax), analyser_.sampleRate() * 0.5) / double(fMin)) / std::log(double(fMax) / double(fMin)) * double(n))));
        auto yFor = [&](float db) { return plot.getBottom() - juce::jlimit(0.0f, 1.0f, (db - bottomDb) / (topDb - bottomDb)) * plot.getHeight(); };
        auto colourFor = [&](int c) { return juce::Colour(spectrum::colourAt(fMin * std::pow(fMax / fMin, (float(c) + 0.5f) / float(n)), settings.range)); };
        g.saveState(); g.reduceClipRegion(plot.toNearestInt());

        if (settings.style == SpectrumSettings::Style::Bars) {
            const int group = 3; const float w = step * float(group);
            for (int c = 0; c + group <= nv; c += group) {
                float m = spectrum::floorDb, pk = spectrum::floorDb; for (int k = 0; k < group; ++k) { m = std::max(m, cols[std::size_t(c + k)]); pk = std::max(pk, holdCols[std::size_t(c + k)]); }
                if (m <= spectrum::floorDb + 1.0f) continue;
                const auto col = colourFor(c + group / 2); const float x = plot.getX() + float(c) * step, y = yFor(m);
                g.setGradientFill(juce::ColourGradient(col.withAlpha(alpha), x, y, col.darker(0.7f).withAlpha(alpha * 0.45f), x, plot.getBottom(), false));
                g.fillRect(x, y, w - 1.0f, plot.getBottom() - y);
                if (settings.peakHold) { g.setColour(col.brighter(0.6f).withAlpha(alpha)); g.fillRect(x, yFor(pk) - 1.0f, w - 1.0f, 1.5f); }
            }
        } else {
            line.clear(); fill.clear();
            for (int c = 0; c < nv; ++c) {
                const float x = plot.getX() + (float(c) + 0.5f) * step, y = yFor(cols[std::size_t(c)]);
                if (c == 0) { line.startNewSubPath(x, y); fill.startNewSubPath(x, plot.getBottom()); fill.lineTo(x, y); } else { line.lineTo(x, y); fill.lineTo(x, y); }
            }
            fill.lineTo(plot.getX() + (float(nv) - 0.5f) * step, plot.getBottom()); fill.closeSubPath();
            if (settings.style == SpectrumSettings::Style::Standard) {
                const auto c = juce::Colour(settings.standard);
                g.setGradientFill(juce::ColourGradient(c.withAlpha(alpha * 0.45f), 0, plot.getY(), c.withAlpha(alpha * 0.04f), 0, plot.getBottom(), false)); g.fillPath(fill);
                g.setColour(c.withAlpha(alpha)); g.strokePath(line, juce::PathStrokeType(1.3f));
            } else {
                // one translucent column per pixel in that frequency's colour, then a glowing multicolour top line
                for (int c = 0; c < nv; ++c) {
                    const float x = plot.getX() + float(c) * step, y = yFor(cols[std::size_t(c)]); if (y >= plot.getBottom() - 0.5f) continue;
                    const auto col = colourFor(c);
                    g.setGradientFill(juce::ColourGradient(col.withAlpha(alpha * 0.55f), x, y, col.withAlpha(alpha * 0.04f), x, plot.getBottom(), false));
                    g.fillRect(x, y, step + 0.5f, plot.getBottom() - y);
                }
                juce::ColourGradient grad(colourFor(0).withAlpha(alpha), plot.getX(), 0, colourFor(n - 1).withAlpha(alpha), plot.getRight(), 0, false);
                for (int i = 1; i < 24; ++i) grad.addColour(double(i) / 24.0, colourFor(juce::jmin(n - 1, i * n / 24)).withAlpha(alpha));
                g.setGradientFill(grad); g.strokePath(line, juce::PathStrokeType(5.0f)); // soft halo
                g.setGradientFill(grad); g.strokePath(line, juce::PathStrokeType(1.6f));
            }
            if (settings.peakHold) {
                hold.clear(); for (int c = 0; c < nv; ++c) { const float x = plot.getX() + (float(c) + 0.5f) * step, y = yFor(holdCols[std::size_t(c)]); c == 0 ? hold.startNewSubPath(x, y) : hold.lineTo(x, y); }
                g.setColour(juce::Colours::white.withAlpha(0.28f * alpha)); g.strokePath(hold, juce::PathStrokeType(0.8f));
            }
        }
        g.restoreState();
    }

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
    std::vector<float> cols, holdCols; juce::Path line, fill, hold; float updateMs = 0.0f;
};

} // namespace goodlookinui::juce_adapter
