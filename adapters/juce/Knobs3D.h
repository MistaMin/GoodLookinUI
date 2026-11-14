// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#pragma once
#include <goodlookinui/Knob3DModels.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <memory>
#include <mutex>

// "3D 2.0" lit knobs painted from baked, per-pixel shaded sprites (see include/goodlookinui/Knob3D.h).
// A sprite is cached per style, size, device scale, angle step, accent and panel colour, so repainting a
// knob that is not moving costs one image blit. Select the tier with the `lit_` prefix on a style
// ("lit_Mtl", "lit_satin") or globally with setKnobMode().
namespace goodlookinui::juce_adapter::knobs3d {

// Panel colour used for reflected and bounced light when the caller gives none (default mid grey).
inline juce::Colour& defaultFloor() { static juce::Colour c(0xff808080); return c; }
// Device pixels per logical unit; 0 (default) reads it from the graphics context.
inline float& scaleOverride() { static float s = 0.0f; return s; }

// Preset behind a catalogue code that has no model of its own (older shared look).
inline const char* litPresetFor(const std::string& code) {
    struct E { const char* c; const char* p; };
    static const E table[] = {
        {"Mtl", "spun"}, {"metal", "spun"}, {"Knl", "spun"}, {"Rck", "spun"}, {"Bk", "bakelite"}, {"Pie", "bakelite"},
        {"Chk", "chicken"}, {"Ptr", "chicken"}, {"Tab", "chicken"}, {"A", "wing"}, {"Brit", "strip"}, {"console", "strip"},
        {"N", "strip"}, {"FS", "strip"}, {"Rd", "gloss"}, {"Chr", "chrome"}, {"Slv", "chrome"}, {"Ivr", "ivory"},
        {"Snk", "phenolic"}, {"Skt", "phenolic"}, {"Tl", "phenolic"}, {"Fat", "phenolic"}, {"Mini", "phenolic"}, {"Dbl", "collet"}};
    for (auto& e : table) if (code == e.c) return e.p;
    return nullptr;
}

// Model of a lit code: a preset by its own name, else the catalogue style's model, else the preset
// that older versions mapped the style to. False for display and neon styles.
inline bool litModel(const std::string& code, knob3d::KnobModel& out) {
    if (knob3d::preset(code, out)) return true;
    if (knob3d::styleModel(code, out)) return true;
    if (auto* p = litPresetFor(code)) return knob3d::preset(p, out);
    return false;
}

inline std::shared_ptr<const knob3d::Sprite> litSprite(const std::string& code, float radiusPx, float angle, juce::Colour accent, juce::Colour floor) {
    knob3d::KnobModel model;
    if (!litModel(code, model)) return nullptr;
    return knob3d::KnobCache::global().get(model, knob3d::Rig::studio(), radiusPx, angle, accent.getARGB() | 0xff000000u, floor.getARGB() | 0xff000000u);
}

// Printed scale of a lit knob, as on the classic drawing of the style.
enum class LitScale { None, Dots, Ticks };
inline std::pair<LitScale, int> litScale(const std::string& code) {
    auto in = [&](std::initializer_list<const char*> l) { for (auto* c : l) if (code == c) return true; return false; };
    if (in({"Brit", "console", "Snk"})) return {LitScale::Ticks, 13};
    if (in({"N", "A", "Mic", "Slv", "ivory", "Chr", "Wnt"})) return {LitScale::Dots, 21};
    if (in({"FS", "Mtl", "metal", "Rd", "Ptr", "Dm", "Lvr", "Tab", "Skt", "Fat", "Chk", "Mini"}) || knob3d::isPreset(code)) return {LitScale::Dots, 11};
    return {LitScale::None, 0};
}

// Lift of the silhouette centre above the base centre and the silhouette radius, in knob radii.
inline std::pair<float, float> litFrame(const std::string& code) {
    static std::mutex mu; static std::map<std::string, std::pair<float, float>> frames;
    std::lock_guard<std::mutex> lk(mu);
    if (auto it = frames.find(code); it != frames.end()) return it->second;
    knob3d::KnobModel m; std::pair<float, float> f{0.0f, 1.0f};
    if (litModel(code, m)) f = m.visualFrame(knob3d::Rig::studio());
    frames[code] = f; return f;
}

// One weight for every style: fine dots or hairline ticks just outside the skirt, centred on where the
// knob appears to sit.
inline void drawLitScale(juce::Graphics& g, const std::string& code, juce::Point<float> c, float r, bool darkMarks) {
    auto [kind, n] = litScale(code);
    if (kind == LitScale::None) return;
    g.setColour(darkMarks ? juce::Colour(0xff252a2d) : juce::Colour(0xffc7ccce));
    auto [lift, reach] = litFrame(code);
    c.y -= lift * r; r *= std::max(reach, 1.0f);
    auto at = [&](float a, float d) { return juce::Point<float>(c.x + std::sin(a) * d, c.y - std::cos(a) * d); };
    auto sweep = [&](int i) { return (-0.8f + 1.6f * float(i) / float(n - 1)) * juce::MathConstants<float>::pi; };
    if (kind == LitScale::Dots) {
        const float d = std::max(r * 0.045f, 1.0f);
        for (int i = 0; i < n; ++i) { auto p = at(sweep(i), r * 1.12f); g.fillEllipse(p.x - d * 0.5f, p.y - d * 0.5f, d, d); }
    } else {
        const float w = juce::jlimit(0.6f, 1.1f, r * 0.025f);
        for (int i = 0; i < n; ++i) {
            const bool major = i % 3 == 0; const float a = sweep(i);
            g.drawLine(juce::Line<float>(at(a, r * 1.08f), at(a, r * (major ? 1.20f : 1.14f))), major ? w : w * 0.7f);
        }
    }
}

// Sprite pixels are premultiplied ARGB, which is also what juce::Image::ARGB stores. The cache keeps the
// sprite alive so its address cannot be reused while its image is cached.
struct ImageCache {
    struct Entry { std::shared_ptr<const knob3d::Sprite> sprite; juce::Image image; std::uint64_t tick; };
    std::mutex mu; std::map<const knob3d::Sprite*, Entry> entries; std::uint64_t tick = 0; size_t bytes = 0;
    static ImageCache& get() { static ImageCache c; return c; }
    juce::Image image(const std::shared_ptr<const knob3d::Sprite>& sprite) {
        std::lock_guard<std::mutex> lk(mu);
        ++tick;
        if (auto it = entries.find(sprite.get()); it != entries.end()) { it->second.tick = tick; return it->second.image; }
        juce::Image img(juce::Image::ARGB, sprite->w, sprite->h, true);
        {
            juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < sprite->h; ++y) {
                auto* line = reinterpret_cast<juce::PixelARGB*>(bd.getLinePointer(y));
                for (int x = 0; x < sprite->w; ++x) {
                    const std::uint32_t p = sprite->px[size_t(y) * size_t(sprite->w) + size_t(x)];
                    line[x].setARGB(juce::uint8(p >> 24), juce::uint8(p >> 16), juce::uint8(p >> 8), juce::uint8(p));
                }
            }
        }
        bytes += sprite->px.size() * 8;
        entries[sprite.get()] = {sprite, img, tick};
        while (entries.size() > 128 || (bytes > (size_t(32) << 20) && entries.size() > 1)) {
            auto old = entries.begin();
            for (auto it = entries.begin(); it != entries.end(); ++it) if (it->second.tick < old->second.tick) old = it;
            bytes -= old->second.sprite->px.size() * 8; entries.erase(old);
        }
        return img;
    }
};

// Draw one lit knob centred at `c` with base radius `r` (logical units). Returns false, drawing nothing,
// for styles without a lit model (the display and neon styles); the caller then falls back to classic.
inline bool drawLitKnob(juce::Graphics& g, const std::string& code, juce::Point<float> c, float r, float angle,
                        juce::Colour accent, juce::Colour floor, bool darkMarks) {
    float scale = scaleOverride() > 0 ? scaleOverride() : float(g.getInternalContext().getPhysicalPixelScaleFactor());
    if (!(scale > 0.0f)) scale = 1.0f;
    auto sprite = litSprite(code, r * scale, angle, accent, floor.isTransparent() ? defaultFloor() : floor);
    if (!sprite) return false;
    drawLitScale(g, code, c, r, darkMarks);
    auto image = ImageCache::get().image(sprite);
    g.drawImageTransformed(image, juce::AffineTransform::scale(1.0f / scale).translated(c.x - sprite->ox / scale, c.y - sprite->oy / scale));
    return true;
}
} // namespace goodlookinui::juce_adapter::knobs3d
