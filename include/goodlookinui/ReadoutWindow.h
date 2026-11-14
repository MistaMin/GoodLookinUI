// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace goodlookinui {
// Display aggregation of pre-measured LUFS/crest, not a loudness detector.
// Single-thread owned. Publish results through host-owned atomics for GUI use.
class ReadoutWindow {
    struct Bin { double energy = 0; std::uint64_t heard = 0; float crest = -100; std::uint64_t samples = 0; };
    std::array<Bin, 40> bins {};
    std::size_t current = 0;
    std::uint64_t binSamples;
public:
    // Forty rounded 50 ms bins retain 1.95–2 seconds at typical rates.
    explicit ReadoutWindow(double sampleRate = 48000) noexcept {
        const auto rate = std::isfinite(sampleRate) && sampleRate > 0 ? sampleRate : 48000;
        binSamples = std::uint64_t(std::max(1.0, std::round(std::clamp(rate, 1.0, 768000.0) * 0.05)));
    }
    void reset() noexcept { bins.fill({}); current = 0; }
    void add(float lufs, float crestDb, std::uint64_t samples) noexcept {
        const bool valid = std::isfinite(lufs) && lufs > -70 && lufs <= 100;
        const double energy = valid ? std::pow(10.0, double(lufs) * 0.1) : 0;
        while (samples > 0) {
            auto& b = bins[current];
            const auto take = std::min(samples, binSamples - b.samples);
            if (valid) { b.energy += energy * double(take); b.heard += take; }
            if (std::isfinite(crestDb)) b.crest = std::max(b.crest, crestDb);
            b.samples += take; samples -= take;
            if (b.samples == binSamples) { current = (current + 1) % bins.size(); bins[current] = {}; }
        }
    }
    // Energy mean excluding <= -70 LUFS, or -200 when empty.
    float loudness() const noexcept {
        double energy = 0; std::uint64_t heard = 0;
        for (const auto& b : bins) { energy += b.energy; heard += b.heard; }
        return heard ? float(10 * std::log10(energy / double(heard))) : -200;
    }
    float crest() const noexcept {
        float value = -100; for (const auto& b : bins) value = std::max(value, b.crest); return value;
    }
};
}
