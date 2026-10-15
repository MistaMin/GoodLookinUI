// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
#pragma once
#include <goodlookinui/Design.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// Procedural knob classes (see goodlookinui::KnobStyle). Every knob is drawn
// into a square of diameter 2*r centred on c, pointing at angle a (radians,
// 0 = straight up, sweep +-0.8*pi). `accent` is the cap colour for capped
// styles and the indicator colour for material styles.
namespace goodlookinui::juce_adapter::knobs {
using juce::Colour;
using juce::Point;

inline constexpr float pi = juce::MathConstants<float>::pi;

struct Ctx {
    juce::Graphics& g;
    Point<float> c;
    float r, a;
    Colour accent;
    bool darkMarks = false;   // printed marks are drawn dark (for light plates) instead of light
};

// Printed marks must stay readable on any plate. A mark colour that is light is swapped for a dark one when the
// plate is light, and a dark one for a light one when the plate is dark; mid/coloured marks keep their hue.
inline Colour markColour(const Ctx& k, Colour c) {
    const float b = c.getPerceivedBrightness();
    if (k.darkMarks && b > 0.45f) return Colour(0xff252a2d).withAlpha(c.getFloatAlpha());
    if (!k.darkMarks && b < 0.30f) return Colour(0xffe6eaec).withAlpha(c.getFloatAlpha());
    return c;
}

inline Point<float> dir(float a) { return {std::sin(a), -std::cos(a)}; }
inline Point<float> at(const Ctx& k, float angle, float radius) { return k.c + dir(angle) * radius; }

inline void disc(const Ctx& k, float radius, Colour top, Colour bottom, Point<float> centre = {0, 0}, bool offset = false) {
    auto c = offset ? centre : k.c;
    k.g.setGradientFill(juce::ColourGradient(top, c.x - radius, c.y - radius, bottom, c.x + radius, c.y + radius, false));
    k.g.fillEllipse(c.x - radius, c.y - radius, radius * 2, radius * 2);
}
// Directional beveled rim: catches studio key-light on top-left (10:30), casts shadow on bottom-right (4:30).
inline void rim(const Ctx& k, float radius, Colour col, float w = 0.8f) {
    const float lx = k.c.x - radius * 0.707f, ly = k.c.y - radius * 0.707f;
    const float sx = k.c.x + radius * 0.707f, sy = k.c.y + radius * 0.707f;
    const auto hi = col.brighter(0.40f);
    const auto lo = col.darker(0.60f);
    k.g.setGradientFill(juce::ColourGradient(hi, lx, ly, lo, sx, sy, false));
    k.g.drawEllipse(k.c.x - radius, k.c.y - radius, radius * 2, radius * 2, w);
}
inline void rimFlat(const Ctx& k, float radius, Colour col, float w = 0.8f) {
    k.g.setColour(col);
    k.g.drawEllipse(k.c.x - radius, k.c.y - radius, radius * 2, radius * 2, w);
}
inline void bevelRim(const Ctx& k, float radius, Colour lightCol, Colour shadowCol, float w = 1.0f) {
    const float lx = k.c.x - radius * 0.707f, ly = k.c.y - radius * 0.707f;
    const float sx = k.c.x + radius * 0.707f, sy = k.c.y + radius * 0.707f;
    k.g.setGradientFill(juce::ColourGradient(lightCol, lx, ly, shadowCol, sx, sy, false));
    k.g.drawEllipse(k.c.x - radius, k.c.y - radius, radius * 2, radius * 2, w);
}
// Clean, grounded hardware shadow: contact ambient occlusion under the knob base plus a subtle directional contact blur
inline void shadow(const Ctx& k, float radius, float heightFactor = 1.0f) {
    auto& g = k.g;
    // 1. Contact ambient occlusion directly beneath the knob perimeter
    for (int n = 4; n > 0; --n) {
        g.setColour(Colour(0x38000000));
        const float e = radius + float(n) * 0.4f;
        g.fillEllipse(k.c.x - e, k.c.y - e + 0.5f, e * 2, e * 2);
    }
    // 2. Controlled directional drop shadow (offset slightly down-right, soft penumbra)
    const float dx = radius * 0.05f * heightFactor;
    const float dy = radius * 0.08f * heightFactor;
    for (int n = 6; n > 0; --n) {
        g.setColour(Colour(0x0e000000));
        const float sx = dx * (float(n) / 6.0f);
        const float sy = dy * (float(n) / 6.0f);
        const float e = radius + float(n) * 0.7f * heightFactor;
        g.fillEllipse(k.c.x - e + sx, k.c.y - e + sy, e * 2, e * 2);
    }
}

// Cylindrical side-wall bevel connecting skirt to elevated body
inline void barrelWall(const Ctx& k, float rBase, float rTop, float dy, Colour lightCol, Colour shadowCol) {
    juce::ignoreUnused(dy);
    // Beveled cylindrical shoulder
    k.g.setGradientFill(juce::ColourGradient(lightCol, k.c.x - rBase * 0.707f, k.c.y - rBase * 0.707f,
                                            shadowCol, k.c.x + rBase * 0.707f, k.c.y + rBase * 0.707f, false));
    k.g.fillEllipse(k.c.x - rBase, k.c.y - rBase, rBase * 2, rBase * 2);
    k.g.setColour(shadowCol.darker(0.3f));
    k.g.drawEllipse(k.c.x - rBase, k.c.y - rBase, rBase * 2, rBase * 2, 1.0f);
    juce::ignoreUnused(rTop);
}

// Machined grub/set screw on the skirt collar (disabled per user request)
inline void grubScrew(const Ctx& k, Point<float> pos, float rScrew, float slotAngle = 0.785f, Colour metal = Colour(0xff8a7542)) {
    juce::ignoreUnused(k, pos, rScrew, slotAngle, metal);
}

// Anisotropic radial reflection cones characteristic of lathe-turned aluminum / chrome
inline void anisotropicSheen(const Ctx& k, float radius, float strength = 0.20f) {
    for (float a : {-0.785f, 2.356f}) {
        auto p0 = at(k, a - 0.42f, radius);
        auto p1 = at(k, a + 0.42f, radius);
        juce::Path cone;
        cone.startNewSubPath(k.c);
        cone.lineTo(p0);
        cone.lineTo(p1);
        cone.closeSubPath();
        k.g.setGradientFill(juce::ColourGradient(Colour::fromFloatRGBA(1, 1, 1, strength), k.c.x, k.c.y,
                                                Colour::fromFloatRGBA(1, 1, 1, 0.0f), (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f, false));
        k.g.fillPath(cone);
    }
}
// Fixed printed marks over the knob's sweep.
inline void dots(const Ctx& k, float radiusFrac, int n, float size, Colour col) {
    k.g.setColour(markColour(k, col));
    for (int i = 0; i < n; ++i) {
        auto p = at(k, (-0.8f + 1.6f * float(i) / float(n - 1)) * pi, k.r * radiusFrac);
        k.g.fillEllipse(p.x - size * 0.5f, p.y - size * 0.5f, size, size);
    }
}
// Crisp, clean engraved indicator slot with subtle trench depth and enamel paint infill
inline void line(const Ctx& k, float from, float to, float w, Colour col, float angle, Point<float> offset = {0,0}) {
    auto& g = k.g;
    const auto p0 = at(k, angle, from) + offset;
    const auto p1 = at(k, angle, to) + offset;
    // Subtle milled groove shadow
    g.setColour(Colour(0x60000000));
    g.drawLine({p0 + Point<float>(0.35f, 0.5f), p1 + Point<float>(0.35f, 0.5f)}, w + 0.6f);
    // Clean enamel paint
    g.setColour(col);
    g.drawLine({p0, p1}, w);
}
inline void flutes(const Ctx& k, float r0, float r1, int n, Colour dark, Colour light, float w) {
    for (int i = 0; i < n; ++i) {
        const float t = k.a + float(i) * juce::MathConstants<float>::twoPi / float(n);
        const auto v = dir(t);
        const float l = juce::jlimit(0.0f, 1.0f, 0.5f - 0.38f * (v.x + v.y));
        const auto p0 = k.c + v * r0;
        const auto p1 = k.c + v * r1;
        // Deep shadow groove behind the tooth
        k.g.setColour(Colour(0xaa000000));
        k.g.drawLine({p0 + Point<float>(0.6f, 0.8f), p1 + Point<float>(0.6f, 0.8f)}, w + 0.6f);
        // Tooth body
        k.g.setColour(dark.interpolatedWith(light, l));
        k.g.drawLine({p0, p1}, w);
        // Crest specular highlight facing key light
        if (l > 0.38f && w > 1.0f) {
            k.g.setColour(Colour(0x60ffffff));
            k.g.drawLine({p0 - v * 0.25f, p1 - v * 0.25f}, w * 0.35f);
        }
    }
}
inline void gloss(const Ctx& k, float radius, float strength = 0.22f) {
    auto r = juce::Rectangle<float>(radius * 1.25f, radius * 0.68f).withCentre({k.c.x - radius * 0.22f, k.c.y - radius * 0.42f});
    k.g.setGradientFill(juce::ColourGradient(Colour::fromFloatRGBA(1, 1, 1, strength), r.getX(), r.getY(),
                                             Colour::fromFloatRGBA(1, 1, 1, 0), r.getX(), r.getBottom(), false));
    k.g.fillEllipse(r);
}
inline juce::Path wedge(const Ctx& k, float baseHalf, float tip, float tipHalf, float angle) {
    const auto v = dir(angle);
    const Point<float> n(-v.y, v.x);
    juce::Path p;
    p.startNewSubPath(k.c + n * baseHalf);
    p.lineTo(k.c + v * tip + n * tipHalf);
    p.lineTo(k.c + v * tip - n * tipHalf);
    p.lineTo(k.c - n * baseHalf);
    p.closeSubPath();
    return p;
}

// N: Neve/Marconi dual-collet knob: extruded skirt, fluted grip, brass collet nut, set screw, dished cap, enamel notch
inline void drawN(const Ctx& k) {
    dots(k, 0.94f, 21, k.r * 0.05f, Colour(0xffe9eef2));
    const float R = k.r * 0.82f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.15f);
    barrelWall(k, R, R * 0.95f, elev * 2.0f, Colour(0xff888e91), Colour(0xff181a1c));
    disc(k, R, Colour(0xfff6f8f8), Colour(0xff606669));
    bevelRim(k, R, Colour(0x88ffffff), Colour(0xaa000000), 1.1f);
    // Grub screw on the skirt
    grubScrew(k, k.c + Point<float>(R * 0.62f, R * 0.60f), k.r * 0.055f, 0.65f, Colour(0xffb89852));
    // Raised fluted body
    const float G = R * 0.78f;
    disc(k, G, Colour(0xff9a9ea1), Colour(0xff35393c));
    flutes(k, G * 0.68f, G * 0.98f, 32, Colour(0xff222527), Colour(0xffb8bcbe), 1.8f);
    bevelRim(k, G, Colour(0x66ffffff), Colour(0x88000000), 1.0f);
    // Brass collet nut ring
    const float Cnut = R * 0.52f;
    disc(k, Cnut, Colour(0xffd4b46a), Colour(0xff6a521a));
    bevelRim(k, Cnut, Colour(0x88ffffff), Colour(0x66000000), 0.8f);
    // Molded cap
    const float C = R * 0.44f;
    disc(k, C, k.accent.brighter(0.25f), k.accent.darker(0.45f));
    bevelRim(k, C, k.accent.brighter(0.60f), k.accent.darker(0.85f), 1.0f);
    gloss(k, C, 0.22f);
    // Single clean engraved pointer line on the outer chrome collar
    line(k, G * 1.01f, R * 0.99f, k.r * 0.055f, Colour(0xff0a0b0c), k.a);
}

// A: API-style console knob: flared silver crown with pointer lug and side ears, recessed matte cap
inline void drawA(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.97f, 21, k.r * 0.05f, k.accent.brighter(0.55f));
    const float R = k.r * 0.74f;
    shadow(k, R, 1.15f);
    auto body = wedge(k, R * 0.92f, k.r * 0.97f, k.r * 0.20f, k.a);
    const Point<float> lugA = at(k, k.a + 1.72f, R * 1.0f), lugB = at(k, k.a - 1.72f, R * 1.0f);
    // Beveled flared silver crown body
    g.setGradientFill(juce::ColourGradient(Colour(0xfff5f7f8), k.c.x - R * 0.707f, k.c.y - R * 0.707f,
                                           Colour(0xff757d82), k.c.x + R * 0.707f, k.c.y + R * 0.707f, false));
    g.fillPath(body);
    g.fillEllipse(k.c.x - R, k.c.y - R, R * 2, R * 2);
    for (auto l : {lugA, lugB}) g.fillEllipse(l.x - R * 0.17f, l.y - R * 0.17f, R * 0.34f, R * 0.34f);
    // Crisp beveled rim
    bevelRim(k, R, Colour(0x88ffffff), Colour(0x66000000), 1.0f);
    g.setColour(Colour(0xff222527));
    g.strokePath(body, juce::PathStrokeType(0.8f));
    for (auto l : {lugA, lugB}) g.drawEllipse(l.x - R * 0.17f, l.y - R * 0.17f, R * 0.34f, R * 0.34f, 0.7f);
    // Machined groove around cap
    g.setColour(Colour(0x55000000));
    g.drawEllipse(k.c.x - R * 0.90f, k.c.y - R * 0.90f, R * 1.80f, R * 1.80f, 1.2f);
    // Center cap with dished spherical normal shading
    const float C = R * 0.86f;
    disc(k, C, k.accent.brighter(0.18f), k.accent.darker(0.35f));
    bevelRim(k, C, k.accent.brighter(0.55f), k.accent.darker(0.70f), 0.9f);
    gloss(k, C, 0.16f);
    // Clean engraved pointer slot on the beak
    line(k, R * 0.78f, k.r * 0.94f, k.r * 0.065f, Colour(0xfff7f5ec), k.a);
}

// Mic: translucent crown pointer knob in the item colour with a faceted beak and white indicator.
inline void drawMic(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.97f, 21, k.r * 0.045f, Colour(0xffe9eef2));
    const float R = k.r * 0.70f;
    const auto v = dir(k.a);
    const Point<float> n(-v.y, v.x);
    auto body = wedge(k, R * 0.90f, k.r * 0.96f, k.r * 0.22f, k.a);
    shadow(k, R, 1.15f);
    g.setColour(Colour(0x35000000));
    g.fillPath(body, juce::AffineTransform::translation(0.5f, 0.8f));
    // Faceted pointer body
    juce::Path litHalf, shHalf;
    litHalf.startNewSubPath(k.c); litHalf.lineTo(k.c + v * k.r * 0.96f); litHalf.lineTo(k.c + v * k.r * 0.90f - n * k.r * 0.18f); litHalf.lineTo(k.c - n * R * 0.88f); litHalf.closeSubPath();
    shHalf.startNewSubPath(k.c); shHalf.lineTo(k.c + v * k.r * 0.96f); shHalf.lineTo(k.c + v * k.r * 0.90f + n * k.r * 0.18f); shHalf.lineTo(k.c + n * R * 0.88f); shHalf.closeSubPath();
    g.setGradientFill(juce::ColourGradient(k.accent.brighter(0.65f), k.c.x - R, k.c.y - R, k.accent.darker(0.35f), k.c.x + R, k.c.y + R, false));
    g.fillPath(litHalf);
    g.setGradientFill(juce::ColourGradient(k.accent.darker(0.15f), k.c.x - R, k.c.y - R, k.accent.darker(0.75f), k.c.x + R, k.c.y + R, false));
    g.fillPath(shHalf);
    disc(k, R, k.accent.brighter(0.40f), k.accent.darker(0.55f));
    bevelRim(k, R, k.accent.brighter(0.8f), k.accent.darker(0.85f), 1.0f);
    const float C = R * 0.62f;
    disc(k, C, k.accent.brighter(0.25f), k.accent.darker(0.5f));
    bevelRim(k, C, k.accent.brighter(0.6f), k.accent.darker(0.8f), 0.8f);
    gloss(k, R, 0.30f);
    line(k, R * 0.05f, k.r * 0.93f, k.r * 0.065f, Colour(0xfff8f4e6), k.a);
}

// FS: Focusrite ISA style: flared conical skirt with side wall, dark stepped collar, brass set screw, dished colored cap
inline void drawFS(const Ctx& k) {
    dots(k, 0.95f, 11, k.r * 0.04f, Colour(0xffdfe3e2));
    const float R = k.r * 0.86f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.2f);
    barrelWall(k, R, R * 0.95f, elev * 2.0f, Colour(0xffd5dcdb), Colour(0xff4a5250));
    disc(k, R, Colour(0xfff4f6f5), Colour(0xff7a8280));
    bevelRim(k, R, Colour(0x75ffffff), Colour(0x88000000), 1.1f);
    // Grub screw on the pale-grey skirt
    grubScrew(k, k.c + Point<float>(R * 0.64f, R * 0.58f), k.r * 0.052f, 0.65f, Colour(0xff2a2e30));
    // Dark stepped collar groove
    const float G = R * 0.74f;
    disc(k, G, Colour(0xff121516), Colour(0xff323639));
    bevelRim(k, G, Colour(0x28ffffff), Colour(0x77000000), 0.9f);
    // Dished coloured cap
    const float C = R * 0.66f;
    disc(k, C, k.accent.brighter(0.32f), k.accent.darker(0.42f));
    bevelRim(k, C, k.accent.brighter(0.65f), k.accent.darker(0.85f), 1.0f);
    gloss(k, C, 0.28f);
    // Engraved pointer trench
    line(k, R * 0.10f, R * 0.96f, k.r * 0.07f, Colour(0xff0a0c0d), k.a);
}

// Mtl: Precision turned aluminium: 3D diamond knurling, anisotropic lathe sheen, concentric micro-grooves, engraved index slot
inline void drawMtl(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.95f, 11, k.r * 0.035f, Colour(0xffc7ccce));
    const float R = k.r * 0.86f, T = R * 0.78f;
    const float elev = k.r * 0.032f;
    shadow(k, R, 1.15f);
    barrelWall(k, R, R * 0.96f, elev * 2.0f, Colour(0xff98a0a4), Colour(0xff222629));
    disc(k, R, Colour(0xffeef1f2), Colour(0xff5a6266));
    // Straight milled knurling teeth around the barrel rim
    flutes(k, R * 0.80f, R * 0.99f, 64, Colour(0xff32383c), Colour(0xfff2f5f6), 1.4f);
    bevelRim(k, R, Colour(0x99ffffff), Colour(0x77000000), 1.1f);
    // Spun aluminum face
    disc(k, T, Colour(0xfffafbfb), Colour(0xff8c9498));
    anisotropicSheen(k, T, 0.32f);
    // Concentric lathe micro-grooves
    g.setColour(Colour(0x18000000));
    for (int i = 1; i <= 7; ++i) {
        const float e = T * float(i) / 7.2f;
        g.drawEllipse(k.c.x - e, k.c.y - e, e * 2, e * 2, 0.55f);
    }
    bevelRim(k, T, Colour(0xaaffffff), Colour(0x66000000), 1.0f);
    gloss(k, T, 0.18f);
    // Grub screw on the skirt
    grubScrew(k, k.c + Point<float>(R * 0.62f, R * 0.60f), k.r * 0.048f, 0.785f, Colour(0xff3a3f44));
    // Engraved index trench filled with black enamel
    line(k, T * 0.10f, T * 0.98f, k.r * 0.065f, k.accent.darker(0.35f), k.a);
}

inline void woodKnob(const Ctx& k, Colour side1, Colour side2, Colour top1, Colour top2, Colour grain) {
    auto& g = k.g;
    const float R = k.r * 0.86f, T = R * 0.80f;
    shadow(k, R);
    disc(k, R, side1, side2);
    bevelRim(k, R, side1.brighter(0.35f), side2.darker(0.6f), 1.0f);
    disc(k, T, top1, top2);
    g.saveState();
    juce::Path clip;
    clip.addEllipse(k.c.x - T, k.c.y - T, T * 2, T * 2);
    g.reduceClipRegion(clip, {});
    g.addTransform(juce::AffineTransform::rotation(k.a, k.c.x, k.c.y));
    g.setColour(grain);
    for (int i = -5; i <= 5; ++i) {
        juce::Path grainPath;
        for (int s = 0; s <= 16; ++s) {
            const float x = k.c.x - T + T * 2 * float(s) / 16.0f;
            const float y = k.c.y + float(i) * T * 0.17f + std::sin(float(s) * 0.55f + float(i)) * T * 0.045f;
            s == 0 ? grainPath.startNewSubPath(x, y) : grainPath.lineTo(x, y);
        }
        g.strokePath(grainPath, juce::PathStrokeType(0.7f + 0.2f * float(i & 1)));
    }
    g.restoreState();
    bevelRim(k, T, Colour(0x77ffffff), Colour(0x44000000), 0.9f);
    gloss(k, T, 0.32f);
    line(k, T * 0.16f, T * 0.92f, k.r * 0.065f, k.accent.brighter(0.4f), k.a);
}

// Wd: turned wood with grain and an inlaid pointer.
inline void drawWd(const Ctx& k) { woodKnob(k, Colour(0xffa97444), Colour(0xff3e2412), Colour(0xffc08855), Colour(0xff6d4421), Colour(0x40301808)); }
// Mpl: light maple.
inline void drawMpl(const Ctx& k) { woodKnob(k, Colour(0xffe6c590), Colour(0xff9a7442), Colour(0xfff3dcb0), Colour(0xffc29a64), Colour(0x387a5428)); }

// Bk: Vintage molded Bakelite knob: fluted ribbed base skirt with a sleek phenolic teardrop pointer
inline void drawBk(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.88f, T = k.r * 0.60f;
    shadow(k, R, 1.15f);
    // Deep warm ebony-charcoal Bakelite base skirt
    disc(k, R, Colour(0xff34363a), Colour(0xff09090a));
    flutes(k, R * 0.86f, R * 0.99f, 36, Colour(0xff050506), Colour(0xff55585d), 1.4f);
    bevelRim(k, R, Colour(0x55ffffff), Colour(0xff000000), 1.0f);
    // Sleek raised teardrop pointer cap
    auto drop = wedge(k, T * 0.96f, k.r * 0.97f, k.r * 0.08f, k.a);
    g.setColour(Colour(0x35000000));
    g.fillPath(drop, juce::AffineTransform::translation(0.5f, 0.8f));
    g.setGradientFill(juce::ColourGradient(Colour(0xff4a4c50), k.c.x - T * 0.707f, k.c.y - T * 0.707f,
                                           Colour(0xff060607), k.c.x + T * 0.707f, k.c.y + T * 0.707f, false));
    g.fillPath(drop);
    g.fillEllipse(k.c.x - T, k.c.y - T, T * 2, T * 2);
    bevelRim(k, T, Colour(0x65ffffff), Colour(0xff000000), 0.9f);
    gloss(k, T, 0.28f);
    // Molded enamel pointer line
    line(k, T * 0.22f, k.r * 0.92f, k.r * 0.06f, k.accent, k.a);
}

// Pie: white serrated skirt around a black cap: each tooth modeled with 3D bevels, glossy cap
inline void drawPie(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.90f, inner = R * 0.86f;
    shadow(k, R, 1.15f);
    juce::Path skirt;
    const int teeth = 24;
    for (int i = 0; i < teeth * 2; ++i) {
        const float t = k.a + float(i) * pi / float(teeth);
        auto p = k.c + dir(t) * (i % 2 ? inner : R);
        i == 0 ? skirt.startNewSubPath(p) : skirt.lineTo(p);
    }
    skirt.closeSubPath();
    g.setGradientFill(juce::ColourGradient(Colour(0xffffffff), k.c.x - R, k.c.y - R, Colour(0xff949896), k.c.x + R, k.c.y + R, false));
    g.fillPath(skirt);
    g.setColour(Colour(0xff2d302f));
    g.strokePath(skirt, juce::PathStrokeType(0.8f));
    // Black bakelite center cap
    const float C = R * 0.68f;
    disc(k, C, Colour(0xff35373a), Colour(0xff060708));
    bevelRim(k, C, Colour(0x55ffffff), Colour(0xff000000), 1.0f);
    gloss(k, C, 0.32f);
    // Pointer line extending across the serrated skirt
    line(k, C * 0.12f, R * 0.98f, k.r * 0.065f, k.accent, k.a);
}

// Snk: matte black cylinder with a fixed tick ring, knurled barrel, grub screw
inline void drawSnk(const Ctx& k) {
    dots(k, 0.93f, 11, k.r * 0.05f, Colour(0xffb4b8ba));
    const float R = k.r * 0.80f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.2f);
    barrelWall(k, R, R * 0.95f, elev * 2.0f, Colour(0xff42464a), Colour(0xff0b0d0e));
    disc(k, R, Colour(0xff34363a), Colour(0xff0b0b0c));
    flutes(k, R * 0.82f, R * 0.99f, 48, Colour(0xff080809), Colour(0xff4a4e52), 1.3f);
    bevelRim(k, R, Colour(0x60ffffff), Colour(0xff000000), 1.1f);
    grubScrew(k, k.c + Point<float>(R * 0.62f, R * 0.60f), k.r * 0.05f, 0.785f, Colour(0xff42464a));
    const float C = R * 0.78f;
    disc(k, C, Colour(0xff25272a), Colour(0xff0e1011));
    bevelRim(k, C, Colour(0x35ffffff), Colour(0x66000000), 0.8f);
    gloss(k, C, 0.16f);
    line(k, R * 0.10f, R * 0.94f, k.r * 0.075f, k.accent.brighter(0.4f), k.a);
}

// Slv: silver cylinder on a dark skirt ring with dots (Fender/Marshall style)
inline void drawSlv(const Ctx& k) {
    const float R = k.r * 0.95f, T = k.r * 0.66f;
    shadow(k, R, 1.25f);
    disc(k, R, Colour(0xffeceff0), Colour(0xff727a7e));
    bevelRim(k, R, Colour(0x88ffffff), Colour(0x66000000), 1.0f);
    // Skirt calibration dots/ticks
    dots(k, 0.82f, 21, k.r * 0.045f, Colour(0xff15181a));
    // Raised knurled black barrel
    shadow(k, T, 1.05f);
    disc(k, T, Colour(0xff303437), Colour(0xff090b0c));
    flutes(k, T * 0.82f, T * 0.98f, 48, Colour(0xff060809), Colour(0xff555d62), 1.3f);
    bevelRim(k, T, Colour(0x40ffffff), Colour(0xff000000), 1.0f);
    // Spun aluminum bright center insert
    const float C = T * 0.74f;
    disc(k, C, Colour(0xffffffff), Colour(0xff98a0a4));
    anisotropicSheen(k, C, 0.28f);
    bevelRim(k, C, Colour(0x99ffffff), Colour(0x55000000), 0.8f);
    gloss(k, C, 0.20f);
    // Black index mark
    line(k, C * 0.12f, T * 0.80f, k.r * 0.065f, Colour(0xff0c0e10), k.a);
}

// Rd: round black body with a coloured cap and tick ring.
inline void drawRd(const Ctx& k) {
    dots(k, 0.95f, 11, k.r * 0.045f, k.accent.brighter(0.4f));
    const float R = k.r * 0.84f, C = R * 0.60f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.15f);
    barrelWall(k, R, R * 0.95f, elev * 2.0f, Colour(0xff45484c), Colour(0xff090a0c));
    disc(k, R, Colour(0xff3d3f43), Colour(0xff09090a));
    flutes(k, R * 0.74f, R * 0.98f, 36, Colour(0xff060607), Colour(0xff52555a), 1.5f);
    bevelRim(k, R, Colour(0x60ffffff), Colour(0xff000000), 1.0f);
    disc(k, C, k.accent.brighter(0.28f), k.accent.darker(0.45f));
    bevelRim(k, C, k.accent.brighter(0.65f), k.accent.darker(0.85f), 1.0f);
    gloss(k, C, 0.26f);
    line(k, R * 0.64f, R * 0.98f, k.r * 0.07f, Colour(0xfff1efe6), k.a);
}

// Ptr: Flat-top tapered silver pointer knob with a clean dark line
inline void drawPtr(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.95f, 11, k.r * 0.035f, Colour(0xffc7ccce));
    const float R = k.r * 0.60f;
    shadow(k, R, 1.15f);
    juce::Path p;
    p.addEllipse(k.c.x - R, k.c.y - R, R * 2, R * 2);
    p.addPath(wedge(k, R * 0.95f, k.r * 0.90f, k.r * 0.26f, k.a));
    // Subtle drop shadow under pointer
    g.setColour(Colour(0x35000000));
    g.fillPath(p, juce::AffineTransform::translation(0.5f, 0.8f));
    // Fully opaque solid metal pointer body
    g.setGradientFill(juce::ColourGradient(Colour(0xfff6f8f8), k.c.x - R * 0.707f, k.c.y - R * 0.707f,
                                           Colour(0xff757d82), k.c.x + R * 0.707f, k.c.y + R * 0.707f, false));
    g.fillPath(p);
    // Beveled highlight and dark rim
    g.setGradientFill(juce::ColourGradient(Colour(0x88ffffff), k.c.x - R * 0.707f, k.c.y - R * 0.707f,
                                           Colour(0x66000000), k.c.x + R * 0.707f, k.c.y + R * 0.707f, false));
    g.strokePath(p, juce::PathStrokeType(0.9f));
    g.setColour(Colour(0xff222528));
    g.strokePath(p, juce::PathStrokeType(0.6f));
    gloss(k, R, 0.18f);
    // Dark pointer line
    line(k, R * 0.05f, k.r * 0.86f, k.r * 0.07f, k.accent.darker(0.6f), k.a);
}

// Dm: chunky glossy domed cap.
inline void drawDm(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.95f, 11, k.r * 0.04f, Colour(0xff9aa3a6));
    const float R = k.r * 0.80f;
    shadow(k, R, 1.2f);
    disc(k, R, k.accent.brighter(0.55f), k.accent.darker(0.70f));
    bevelRim(k, R, k.accent.brighter(0.85f), k.accent.darker(0.90f), 1.1f);
    gloss(k, R, 0.38f);
    auto p = at(k, k.a, R * 0.68f);
    const float d = k.r * 0.15f;
    g.setColour(Colour(0x66000000));
    g.fillEllipse(p.x - d * 0.5f + 0.6f, p.y - d * 0.5f + 0.8f, d, d);
    g.setColour(Colour(0xfffaf7ea));
    g.fillEllipse(p.x - d * 0.5f, p.y - d * 0.5f, d, d);
}

// Sq: square rounded keycap with a dished top and a legend line.
inline void drawSq(const Ctx& k) {
    auto& g = k.g;
    const float S = k.r * 0.78f;
    shadow(k, S * 1.1f, 1.2f);
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(k.a, k.c.x, k.c.y));
    auto outer = juce::Rectangle<float>(S * 2, S * 2).withCentre(k.c);
    g.setGradientFill(juce::ColourGradient(Colour(0xff42454a), outer.getX(), outer.getY(), Colour(0xff090a0c), outer.getRight(), outer.getBottom(), false));
    g.fillRoundedRectangle(outer, S * 0.28f);
    g.setColour(Colour(0xff000000));
    g.drawRoundedRectangle(outer, S * 0.28f, 1.0f);
    auto top = outer.reduced(S * 0.20f);
    g.setGradientFill(juce::ColourGradient(Colour(0xff0e1012), top.getX(), top.getY(), Colour(0xff32353a), top.getRight(), top.getBottom(), false));
    g.fillRoundedRectangle(top, S * 0.2f);
    g.setColour(Colour(0x40ffffff));
    g.drawRoundedRectangle(top, S * 0.2f, 0.8f);
    line(k, S * 0.1f, S * 0.84f, k.r * 0.08f, k.accent, 0.0f);
    g.restoreState();
}

// Seg: segmented lit arc around a dark cylinder (bar-graph dash style).
inline void drawSeg(const Ctx& k) {
    auto& g = k.g;
    const int n = 21;
    const float prop = juce::jlimit(0.0f, 1.0f, (k.a / pi + 0.8f) / 1.6f);
    const float w = k.r * 0.11f;
    for (int i = 0; i < n; ++i) {
        const float f = (float(i) + 0.5f) / float(n);
        const float t = (-0.8f + 1.6f * f) * pi;
        const bool lit = f <= prop + 0.5f / float(n);
        const bool red = f > 0.88f;
        const Colour col = red ? Colour(0xffe4412c) : k.accent;
        if (lit) {
            g.setColour(col.withAlpha(0.28f));
            g.drawLine({at(k, t, k.r * 0.70f), at(k, t, k.r * 0.99f)}, w * 1.7f);
        }
        g.setColour(lit ? col : col.withAlpha(0.13f));
        g.drawLine({at(k, t, k.r * 0.74f), at(k, t, k.r * 0.96f)}, w);
    }
    const float R = k.r * 0.60f;
    shadow(k, R);
    disc(k, R, Colour(0xff2d2f33), Colour(0xff08090a));
    rim(k, R, Colour(0xff000000));
    rim(k, R * 0.88f, Colour(0x1affffff));
    gloss(k, R, 0.07f);
    line(k, R * 0.14f, R * 0.84f, k.r * 0.07f, k.accent.brighter(0.5f), k.a);
}

// Vfd: phosphor glass with a lit vector fan.
inline void drawVfd(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.92f;
    shadow(k, R);
    disc(k, R, Colour(0xff10221a), Colour(0xff020504));
    rim(k, R, Colour(0xff435248), 1.2f);
    const float prop = juce::jlimit(0.0f, 1.0f, (k.a / pi + 0.8f) / 1.6f);
    const int n = 17;
    for (int i = 0; i < n; ++i) {
        const float f = float(i) / float(n - 1);
        const float t = (-0.8f + 1.6f * f) * pi;
        const float outer = k.r * (0.40f + 0.46f * f);
        const bool lit = f <= prop + 0.5f / float(n);
        if (lit) {
            g.setColour(k.accent.withAlpha(0.25f));
            g.drawLine({at(k, t, k.r * 0.22f), at(k, t, outer)}, 3.0f);
        }
        g.setColour(lit ? k.accent.brighter(0.2f) : k.accent.withAlpha(0.16f));
        g.drawLine({at(k, t, k.r * 0.22f), at(k, t, outer)}, 1.1f);
    }
    g.setColour(k.accent.withAlpha(0.5f));
    g.drawEllipse(k.c.x - k.r * 0.16f, k.c.y - k.r * 0.16f, k.r * 0.32f, k.r * 0.32f, 1.0f);
    gloss(k, R, 0.10f);
    line(k, k.r * 0.10f, k.r * 0.30f, k.r * 0.07f, k.accent.brighter(0.7f), k.a);
}

// Cyb: chamfered dark frame with a glowing outline and a notched ring.
inline void drawCyb(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.90f, ch = R * 0.38f;
    juce::Path frame;
    frame.startNewSubPath(k.c.x - R + ch, k.c.y - R);
    frame.lineTo(k.c.x + R - ch, k.c.y - R);
    frame.lineTo(k.c.x + R, k.c.y - R + ch);
    frame.lineTo(k.c.x + R, k.c.y + R - ch);
    frame.lineTo(k.c.x + R - ch, k.c.y + R);
    frame.lineTo(k.c.x - R + ch, k.c.y + R);
    frame.lineTo(k.c.x - R, k.c.y + R - ch);
    frame.lineTo(k.c.x - R, k.c.y - R + ch);
    frame.closeSubPath();
    g.setColour(Colour(0x55000000));
    g.fillPath(frame, juce::AffineTransform::translation(1.5f, 3.0f));
    g.setGradientFill(juce::ColourGradient(Colour(0xff1b2024), k.c.x - R, k.c.y - R, Colour(0xff050607), k.c.x + R, k.c.y + R, false));
    g.fillPath(frame);
    g.setColour(k.accent.withAlpha(0.22f));
    g.strokePath(frame, juce::PathStrokeType(4.0f));
    g.setColour(k.accent);
    g.strokePath(frame, juce::PathStrokeType(1.4f));
    const float ring = R * 0.62f;
    rim(k, ring, k.accent.withAlpha(0.55f), 1.0f);
    g.setColour(k.accent.withAlpha(0.7f));
    for (int i = 0; i < 12; ++i) {
        const float t = k.a + float(i) * juce::MathConstants<float>::twoPi / 12.0f;
        g.drawLine({at(k, t, ring * 1.08f), at(k, t, ring * 1.22f)}, 1.1f);
    }
    line(k, ring * 0.18f, ring * 0.86f, k.r * 0.07f, k.accent.brighter(0.4f), k.a);
    auto tip = at(k, k.a, ring * 0.86f);
    g.fillEllipse(tip.x - k.r * 0.06f, tip.y - k.r * 0.06f, k.r * 0.12f, k.r * 0.12f);
}

inline void drawFat(const Ctx& k) {
    const float R = k.r * 0.90f;
    shadow(k, R, 1.2f);
    // Dark cylindrical base skirt with clean flutes
    disc(k, R, Colour(0xff3c3d40), Colour(0xff060708));
    flutes(k, R * 0.78f, R * 0.99f, 40, Colour(0xff050506), Colour(0xff55585d), 1.4f);
    bevelRim(k, R, Colour(0x60ffffff), Colour(0xff000000), 1.0f);
    const float T = R * 0.74f;
    disc(k, T, Colour(0xff2d2e30), Colour(0xff09090a));
    bevelRim(k, T, Colour(0x40ffffff), Colour(0x88000000), 0.9f);
    gloss(k, T, 0.25f);
    // Thick white/accent pointer running to edge
    line(k, R * 0.08f, R * 0.97f, k.r * 0.11f, k.accent, k.a);
}
inline void drawPag(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.88f;
    shadow(k, R, 1.2f);
    juce::Path oct;
    for (int i = 0; i < 8; ++i) {
        auto p = at(k, k.a + (float(i) + 0.5f) * pi / 4.0f, R);
        i == 0 ? oct.startNewSubPath(p) : oct.lineTo(p);
    }
    oct.closeSubPath();
    g.setGradientFill(juce::ColourGradient(Colour(0xff4a4c50), k.c.x - R, k.c.y - R, Colour(0xff070809), k.c.x + R, k.c.y + R, false));
    g.fillPath(oct);
    g.setColour(Colour(0x55ffffff));
    g.strokePath(oct, juce::PathStrokeType(1.0f));
    flutes(k, R * 0.62f, R * 0.84f, 32, Colour(0xff060708), Colour(0xff55585d), 1.6f);
    disc(k, R * 0.56f, Colour(0xff3a3b3e), Colour(0xff060607));
    bevelRim(k, R * 0.56f, Colour(0x40ffffff), Colour(0xff000000), 0.9f);
    gloss(k, R * 0.56f, 0.28f);
    line(k, R * 0.20f, R * 0.96f, k.r * 0.075f, k.accent, k.a);
}
inline void drawLvr(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.95f, 11, k.r * 0.04f, Colour(0xffc7ccce));
    shadow(k, k.r * 0.5f, 1.25f);
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(k.a, k.c.x, k.c.y));
    auto bar = juce::Rectangle<float>(k.r * 0.34f, k.r * 1.30f).withCentre({k.c.x, k.c.y - k.r * 0.28f});
    g.setGradientFill(juce::ColourGradient(Colour(0xff52555a), bar.getX(), 0, Colour(0xff08090a), bar.getRight(), 0, false));
    g.fillRoundedRectangle(bar, k.r * 0.08f);
    g.setColour(Colour(0x50ffffff));
    g.drawRoundedRectangle(bar, k.r * 0.08f, 0.8f);
    g.setColour(k.accent);
    g.fillRect(juce::Rectangle<float>(k.r * 0.09f, k.r * 0.34f).withCentre({k.c.x, k.c.y - k.r * 0.74f}));
    g.restoreState();
    disc(k, k.r * 0.32f, Colour(0xff45484c), Colour(0xff08090a));
    bevelRim(k, k.r * 0.32f, Colour(0x66ffffff), Colour(0x88000000), 1.0f);
}
inline void drawTab(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.95f, 11, k.r * 0.04f, Colour(0xffc7ccce));
    const float R = k.r * 0.52f;
    shadow(k, R, 1.15f);
    auto body = wedge(k, R * 0.5f, k.r * 0.94f, k.r * 0.12f, k.a);
    g.setGradientFill(juce::ColourGradient(Colour(0xff52555a), k.c.x - R, k.c.y - R, Colour(0xff08090a), k.c.x + R, k.c.y + R, false));
    g.fillPath(body);
    disc(k, R, Colour(0xff52555a), Colour(0xff08090a));
    bevelRim(k, R, Colour(0x55ffffff), Colour(0x88000000), 1.0f);
    line(k, R * 0.15f, k.r * 0.90f, k.r * 0.06f, k.accent, k.a);
}
inline void drawWh(const Ctx& k) {
    const float R = k.r * 0.86f, C = R * 0.64f;
    shadow(k, R, 1.2f);
    disc(k, R, Colour(0xff3f4145), Colour(0xff060708));
    flutes(k, R * 0.72f, R * 0.99f, 48, Colour(0xff050506), Colour(0xff606468), 1.4f);
    bevelRim(k, R, Colour(0x60ffffff), Colour(0xff000000), 1.0f);
    disc(k, C, Colour(0xfffdfdfc), Colour(0xff989da0));
    bevelRim(k, C, Colour(0x99ffffff), Colour(0x66000000), 0.9f);
    gloss(k, C, 0.18f);
    line(k, 0.0f, C * 0.96f, k.r * 0.07f, k.accent.darker(0.6f), k.a);
}
inline void drawKnl(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.86f, C = R * 0.70f;
    shadow(k, R, 1.2f);
    disc(k, R, Colour(0xff383a3e), Colour(0xff060708));
    flutes(k, R * 0.68f, R * 0.99f, 64, Colour(0xff050506), Colour(0xff65686d), 1.3f);
    bevelRim(k, R, Colour(0x60ffffff), Colour(0xff000000), 1.0f);
    grubScrew(k, k.c + Point<float>(R * 0.62f, R * 0.60f), k.r * 0.048f, 0.785f, Colour(0xff383d42));
    disc(k, C, Colour(0xffffffff), Colour(0xff8a9296));
    anisotropicSheen(k, C, 0.28f);
    g.setColour(Colour(0x18000000));
    for (int i = 1; i <= 6; ++i) {
        const float e = C * float(i) / 6.2f;
        g.drawEllipse(k.c.x - e, k.c.y - e, e * 2, e * 2, 0.5f);
    }
    bevelRim(k, C, Colour(0xaaffffff), Colour(0x66000000), 0.9f);
    line(k, C * 0.08f, C * 0.96f, k.r * 0.07f, k.accent.darker(0.4f), k.a);
}
inline void drawSkt(const Ctx& k) {
    dots(k, 0.95f, 11, k.r * 0.04f, Colour(0xffc7ccce));
    const float R = k.r * 0.88f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.25f);
    barrelWall(k, R, R * 0.95f, elev * 2.0f, Colour(0xff45484c), Colour(0xff08090a));
    disc(k, R, Colour(0xff3a3c3e), Colour(0xff060607));
    flutes(k, R * 0.84f, R * 0.99f, 48, Colour(0xff050506), Colour(0xff55585d), 1.2f);
    bevelRim(k, R, Colour(0x60ffffff), Colour(0xff000000), 1.1f);
    grubScrew(k, k.c + Point<float>(R * 0.62f, R * 0.60f), k.r * 0.048f, 0.785f, Colour(0xff42464a));
    const float C = R * 0.76f;
    disc(k, C, Colour(0xff2a2b2d), Colour(0xff0a0a0b));
    bevelRim(k, C, Colour(0x40ffffff), Colour(0x88000000), 0.9f);
    gloss(k, C, 0.20f);
    line(k, R * 0.25f, R * 0.99f, k.r * 0.075f, k.accent, k.a);
}
inline void drawRib(const Ctx& k) {
    const float R = k.r * 0.88f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.2f);
    barrelWall(k, R, R * 0.95f, elev * 2.0f, k.accent.brighter(0.2f), k.accent.darker(0.7f));
    disc(k, R, k.accent.brighter(0.25f), k.accent.darker(0.6f));
    flutes(k, R * 0.60f, R * 0.98f, 36, k.accent.darker(0.75f), k.accent.brighter(0.55f), 2.0f);
    bevelRim(k, R, k.accent.brighter(0.7f), k.accent.darker(0.85f), 1.1f);
    const float C = R * 0.58f;
    disc(k, C, k.accent.brighter(0.18f), k.accent.darker(0.5f));
    bevelRim(k, C, k.accent.brighter(0.6f), k.accent.darker(0.8f), 0.9f);
    gloss(k, C, 0.24f);
    line(k, R * 0.10f, R * 0.54f, k.r * 0.07f, Colour(0xfff7f4e8), k.a);
}
inline void drawChr(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.95f, 21, k.r * 0.04f, Colour(0xffe9eef2));
    const float R = k.r * 0.84f, C = R * 0.68f;
    shadow(k, R, 1.25f);
    disc(k, R, Colour(0xffffffff), Colour(0xff454a4e));
    g.setColour(Colour(0x55000000));
    g.drawEllipse(k.c.x - R * 0.9f, k.c.y - R * 0.9f, R * 1.8f, R * 1.8f, 1.2f);
    bevelRim(k, R, Colour(0xffffffff), Colour(0xff181a1c), 1.2f);
    disc(k, C, Colour(0xff55585d), Colour(0xff020203));
    bevelRim(k, C, Colour(0x77ffffff), Colour(0xff000000), 1.0f);
    gloss(k, C, 0.55f);
    line(k, C * 0.08f, R * 0.98f, k.r * 0.065f, Colour(0xfff5f2e6), k.a);
}
inline void arcStroke(const Ctx& k, float rad, float from, float to, float w, Colour col) {
    if (to <= from + 0.001f) return;
    juce::Path p;
    p.addCentredArc(k.c.x, k.c.y, rad, rad, 0.0f, from, to, true);
    k.g.setColour(col);
    k.g.strokePath(p, juce::PathStrokeType(w, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
}
inline void drawArc(const Ctx& k) {
    const float lo = -0.8f * pi, hi = 0.8f * pi, w = k.r * 0.17f, rad = k.r * 0.84f;
    const float split = lo + (hi - lo) * 0.86f;
    arcStroke(k, rad, lo, hi, w, k.accent.withAlpha(0.14f));
    arcStroke(k, rad, lo, juce::jmin(k.a, split), w * 1.6f, k.accent.withAlpha(0.22f));
    arcStroke(k, rad, lo, juce::jmin(k.a, split), w, k.accent);
    arcStroke(k, rad, split, k.a, w, Colour(0xffe4412c));
    const float R = k.r * 0.60f;
    shadow(k, R);
    disc(k, R, Colour(0xff2a2c30), Colour(0xff08090a));
    rim(k, R, Colour(0xff000000));
    gloss(k, R, 0.07f);
    line(k, R * 0.14f, R * 0.84f, k.r * 0.07f, k.accent.brighter(0.5f), k.a);
}
inline void drawBrg(const Ctx& k) {
    auto& g = k.g;
    const int n = 14;
    const float prop = juce::jlimit(0.0f, 1.0f, (k.a / pi + 0.8f) / 1.6f);
    for (int i = 0; i < n; ++i) {
        const float f = (float(i) + 0.5f) / float(n);
        const float t = (-0.8f + 1.6f * f) * pi;
        const float len = k.r * (0.10f + 0.26f * f);
        const bool lit = f <= prop + 0.5f / float(n);
        const Colour col = f > 0.88f ? Colour(0xffe4412c) : k.accent;
        g.setColour(lit ? col : col.withAlpha(0.14f));
        g.drawLine({at(k, t, k.r * 0.64f), at(k, t, k.r * 0.64f + len)}, k.r * 0.10f);
    }
    const float R = k.r * 0.54f;
    shadow(k, R);
    disc(k, R, Colour(0xff2a2c30), Colour(0xff08090a));
    rim(k, R, Colour(0xff000000));
    line(k, R * 0.14f, R * 0.84f, k.r * 0.07f, k.accent.brighter(0.5f), k.a);
}
inline void drawNeo(const Ctx& k) {
    auto& g = k.g;
    const float lo = -0.8f * pi, rad = k.r * 0.80f;
    disc(k, rad, Colour(0xff0a0f14), Colour(0xff020305));
    arcStroke(k, rad, lo, 0.8f * pi, k.r * 0.04f, k.accent.withAlpha(0.25f));
    arcStroke(k, rad, lo, k.a, k.r * 0.16f, k.accent.withAlpha(0.20f));
    arcStroke(k, rad, lo, k.a, k.r * 0.05f, k.accent);
    rim(k, rad * 0.62f, k.accent.withAlpha(0.35f), 0.8f);
    auto p = at(k, k.a, rad);
    g.setColour(k.accent.withAlpha(0.35f));
    g.fillEllipse(p.x - k.r * 0.13f, p.y - k.r * 0.13f, k.r * 0.26f, k.r * 0.26f);
    g.setColour(k.accent.brighter(0.7f));
    g.fillEllipse(p.x - k.r * 0.07f, p.y - k.r * 0.07f, k.r * 0.14f, k.r * 0.14f);
    line(k, rad * 0.20f, rad * 0.58f, k.r * 0.04f, k.accent.withAlpha(0.8f), k.a);
}
inline void drawHex(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.92f;
    juce::Path hx;
    for (int i = 0; i < 6; ++i) {
        auto p = at(k, float(i) * pi / 3.0f, R);
        i == 0 ? hx.startNewSubPath(p) : hx.lineTo(p);
    }
    hx.closeSubPath();
    g.setColour(Colour(0x55000000));
    g.fillPath(hx, juce::AffineTransform::translation(1.5f, 3.0f));
    g.setGradientFill(juce::ColourGradient(k.accent.darker(0.8f), k.c.x - R, k.c.y - R, Colour(0xff050607), k.c.x + R, k.c.y + R, false));
    g.fillPath(hx);
    g.setColour(k.accent.withAlpha(0.25f));
    g.strokePath(hx, juce::PathStrokeType(4.0f));
    g.setColour(k.accent);
    g.strokePath(hx, juce::PathStrokeType(1.3f));
    juce::Path inner;
    inner.addPolygon(k.c, 6, R * 0.62f, pi * 0.0f);
    g.setColour(k.accent.withAlpha(0.45f));
    g.strokePath(inner, juce::PathStrokeType(0.9f));
    line(k, R * 0.12f, R * 0.60f, k.r * 0.07f, k.accent.brighter(0.5f), k.a);
}
inline void drawYel(const Ctx& k) {
    auto& g = k.g;
    const float S = k.r * 0.90f;
    auto outer = juce::Rectangle<float>(S * 2, S * 2).withCentre(k.c);
    g.setColour(Colour(0x55000000));
    g.fillRoundedRectangle(outer.translated(1.5f, 3.0f), S * 0.30f);
    g.setGradientFill(juce::ColourGradient(k.accent.brighter(0.2f), outer.getX(), outer.getY(), k.accent.darker(0.5f), outer.getRight(), outer.getBottom(), false));
    g.fillRoundedRectangle(outer, S * 0.30f);
    g.setColour(k.accent.darker(0.8f));
    g.drawRoundedRectangle(outer, S * 0.30f, 1.0f);
    auto screen = outer.reduced(S * 0.20f);
    g.setColour(Colour(0xff06090b));
    g.fillRoundedRectangle(screen, S * 0.18f);
    g.setColour(Colour(0xff3ae6ee).withAlpha(0.35f));
    g.drawRoundedRectangle(screen, S * 0.18f, 1.0f);
    const float ring = screen.getWidth() * 0.34f;
    g.setColour(Colour(0xff3ae6ee).withAlpha(0.45f));
    g.drawEllipse(k.c.x - ring, k.c.y - ring, ring * 2, ring * 2, 0.9f);
    line(k, ring * 0.2f, ring, k.r * 0.07f, Colour(0xff6cf2f8), k.a);
}
inline void drawKey(const Ctx& k) {
    const float R = k.r * 0.80f;
    shadow(k, R);
    disc(k, R, Colour(0xff3a3c40), Colour(0xff070708));
    rim(k, R, Colour(0xff000000));
    disc(k, R * 0.76f, Colour(0xff0b0c0d), Colour(0xff2b2d31));
    rim(k, R * 0.76f, Colour(0x2affffff));
    line(k, R * 0.10f, R * 0.66f, k.r * 0.09f, k.accent, k.a);
}
inline void drawBrz(const Ctx& k) {
    const float R = k.r * 0.86f, T = R * 0.74f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.2f);
    barrelWall(k, R, R * 0.95f, elev * 2.0f, Colour(0xffd4aa55), Colour(0xff452d0a));
    disc(k, R, Colour(0xfff3d684), Colour(0xff6f4d14));
    flutes(k, R * 0.78f, R * 0.99f, 56, Colour(0xff4a320c), Colour(0xfffbe9a8), 1.2f);
    bevelRim(k, R, Colour(0x88ffffff), Colour(0x77000000), 1.1f);
    grubScrew(k, k.c + Point<float>(R * 0.62f, R * 0.60f), k.r * 0.048f, 0.785f, Colour(0xffc89e3a));
    disc(k, T, Colour(0xfffff0b4), Colour(0xff9a7122));
    anisotropicSheen(k, T, 0.30f);
    bevelRim(k, T, Colour(0x88ffffff), Colour(0x55000000), 0.9f);
    gloss(k, T, 0.22f);
    line(k, T * 0.08f, T * 0.96f, k.r * 0.065f, k.accent.darker(0.7f), k.a);
}
// Chk: Vintage chickenhead selector with 3D faceted ridge, brass grub screw, illuminated crest and enamel line
inline void drawChk(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.95f, 11, k.r * 0.04f, Colour(0xffc7ccce));
    const float R = k.r * 0.60f;
    const auto v = dir(k.a);
    const Point<float> n(-v.y, v.x);
    auto body = wedge(k, R * 0.96f, k.r * 0.95f, k.r * 0.20f, k.a);
    shadow(k, R, 1.15f);
    g.setColour(Colour(0x35000000));
    g.fillPath(body, juce::AffineTransform::translation(0.5f, 0.8f));
    // Faceted chickenhead beak: lit face + shadow face
    juce::Path litHalf, shHalf;
    litHalf.startNewSubPath(k.c);
    litHalf.lineTo(k.c + v * k.r * 0.95f);
    litHalf.lineTo(k.c + v * k.r * 0.90f - n * k.r * 0.18f);
    litHalf.lineTo(k.c - n * R * 0.94f);
    litHalf.closeSubPath();
    shHalf.startNewSubPath(k.c);
    shHalf.lineTo(k.c + v * k.r * 0.95f);
    shHalf.lineTo(k.c + v * k.r * 0.90f + n * k.r * 0.18f);
    shHalf.lineTo(k.c + n * R * 0.94f);
    shHalf.closeSubPath();
    g.setGradientFill(juce::ColourGradient(Colour(0xff4c5054), k.c.x - R, k.c.y - R, Colour(0xff121416), k.c.x + R, k.c.y + R, false));
    g.fillPath(litHalf);
    g.setGradientFill(juce::ColourGradient(Colour(0xff222528), k.c.x - R, k.c.y - R, Colour(0xff060708), k.c.x + R, k.c.y + R, false));
    g.fillPath(shHalf);
    disc(k, R, Colour(0xff3e4246), Colour(0xff08090a));
    bevelRim(k, R, Colour(0x55ffffff), Colour(0x88000000), 1.0f);
    gloss(k, R, 0.28f);
    // Grub screw on the skirt
    grubScrew(k, k.c + Point<float>(R * 0.65f, R * 0.58f), k.r * 0.052f, 0.785f, Colour(0xffbf9a4c));
    // White enamel line running along crest
    line(k, R * 0.05f, k.r * 0.92f, k.r * 0.065f, k.accent.getPerceivedBrightness() > 0.3f ? k.accent : Colour(0xfff7f5eb), k.a);
}
// Ivr: Vintage cream bakelite fluted knob with aged patina and dark pointer
inline void drawIvr(const Ctx& k) {
    const float R = k.r * 0.86f, T = R * 0.74f;
    shadow(k, R, 1.15f);
    disc(k, R, Colour(0xfff6efd8), Colour(0xffa89c76));
    flutes(k, R * 0.76f, R * 0.99f, 36, Colour(0xff857a58), Colour(0xfffffbea), 1.4f);
    bevelRim(k, R, Colour(0x88ffffff), Colour(0x4d4630), 1.0f);
    disc(k, T, Colour(0xfffaf4de), Colour(0xffc2b78f));
    bevelRim(k, T, Colour(0x77ffffff), Colour(0x55000000), 0.9f);
    gloss(k, T, 0.22f);
    line(k, T * 0.10f, T * 0.96f, k.r * 0.07f, k.accent.darker(0.7f), k.a);
}
inline void drawLed(const Ctx& k) {
    auto& g = k.g;
    const int n = 15;
    const float prop = juce::jlimit(0.0f, 1.0f, (k.a / pi + 0.8f) / 1.6f);
    for (int i = 0; i < n; ++i) {
        const float f = float(i) / float(n - 1);
        const bool lit = f <= prop + 0.5f / float(n);
        auto p = at(k, (-0.8f + 1.6f * f) * pi, k.r * 0.86f);
        const float d = k.r * 0.10f;
        if (lit) {
            g.setColour(k.accent.withAlpha(0.3f));
            g.fillEllipse(p.x - d, p.y - d, d * 2, d * 2);
        }
        g.setColour(lit ? k.accent.brighter(0.3f) : k.accent.withAlpha(0.16f));
        g.fillEllipse(p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }
    const float R = k.r * 0.66f;
    shadow(k, R, 1.15f);
    disc(k, R, Colour(0xff35373b), Colour(0xff060607));
    flutes(k, R * 0.84f, R * 0.99f, 40, Colour(0xff060607), Colour(0xff45484c), 1.1f);
    bevelRim(k, R, Colour(0x55ffffff), Colour(0xff000000), 1.0f);
    line(k, R * 0.10f, R * 0.90f, k.r * 0.075f, Colour(0xfff1efe6), k.a);
}
// Dbl: Dual-concentric stacked knob: fluted aluminium ring, upper elevated knob with single pointer line
inline void drawDbl(const Ctx& k) {
    const float R = k.r * 0.90f, I = R * 0.52f;
    shadow(k, R, 1.15f);
    // Lower knob: fluted aluminium ring
    disc(k, R, Colour(0xfff2f4f4), Colour(0xff5d6367));
    flutes(k, R * 0.78f, R * 0.99f, 48, Colour(0xff4b5155), Colour(0xffe9ecee), 1.2f);
    bevelRim(k, R, Colour(0x88ffffff), Colour(0x66000000), 1.0f);
    // Upper knob: elevated above lower knob with soft contact shadow
    shadow(k, I, 1.05f);
    disc(k, I, Colour(0xff3e4044), Colour(0xff050506));
    flutes(k, I * 0.72f, I * 0.98f, 28, Colour(0xff050506), Colour(0xff5a5c60), 1.1f);
    bevelRim(k, I, Colour(0x55ffffff), Colour(0xff000000), 0.9f);
    disc(k, I * 0.62f, k.accent.brighter(0.2f), k.accent.darker(0.5f));
    bevelRim(k, I * 0.62f, k.accent.brighter(0.6f), k.accent.darker(0.8f), 0.8f);
    gloss(k, I * 0.62f, 0.20f);
    // Single clean pointer line across the upper knob
    line(k, 0.0f, I * 0.58f, k.r * 0.055f, Colour(0xfff7f4e8), k.a);
}
inline void drawMini(const Ctx& k) {
    dots(k, 0.92f, 11, k.r * 0.05f, Colour(0xffc7ccce));
    const float R = k.r * 0.46f;
    shadow(k, R);
    disc(k, R, Colour(0xff414246), Colour(0xff060607));
    rim(k, R, Colour(0xff000000));
    gloss(k, R, 0.12f);
    line(k, R * 0.1f, R * 0.88f, k.r * 0.09f, k.accent.brighter(0.6f), k.a);
}
inline void drawTl(const Ctx& k) {
    const float R = k.r * 0.80f;
    shadow(k, R);
    disc(k, R, Colour(0xff3a3b3e), Colour(0xff060607));
    flutes(k, R * 0.74f, R * 0.99f, 60, Colour(0xff060607), Colour(0xff55575b), 1.1f);
    disc(k, R * 0.70f, Colour(0xff2c2d30), Colour(0xff09090a));
    rim(k, R * 0.70f, Colour(0x2affffff));
    line(k, R * 0.0f, R * 0.96f, k.r * 0.08f, k.accent.brighter(0.6f), k.a);
}

// ---- Batch 4: neon / retro dash ------------------------------------------
inline void drawGlw(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.70f;
    disc(k, R, Colour(0xff12181d), Colour(0xff020304));
    rim(k, R, k.accent.withAlpha(0.55f), 1.2f);
    const float lo = -0.8f * pi;
    juce::Path track; track.addCentredArc(k.c.x, k.c.y, k.r * 0.88f, k.r * 0.88f, 0, lo + juce::MathConstants<float>::halfPi * 0.0f, 0.8f * pi, true);
    g.setColour(k.accent.withAlpha(0.14f)); g.strokePath(track, juce::PathStrokeType(k.r * 0.07f));
    if (k.a > lo + 0.01f) {
        juce::Path val; val.addCentredArc(k.c.x, k.c.y, k.r * 0.88f, k.r * 0.88f, 0, lo, k.a, true);
        for (int n = 3; n >= 1; --n) { g.setColour(k.accent.withAlpha(0.07f * float(4 - n))); g.strokePath(val, juce::PathStrokeType(k.r * (0.07f + 0.07f * float(n)))); }
        g.setColour(k.accent); g.strokePath(val, juce::PathStrokeType(k.r * 0.07f));
        g.setColour(k.accent.brighter(0.8f)); g.strokePath(val, juce::PathStrokeType(k.r * 0.025f));
    }
    auto p = at(k, k.a, R * 0.86f);
    g.setColour(k.accent.withAlpha(0.35f)); g.fillEllipse(p.x - k.r * 0.12f, p.y - k.r * 0.12f, k.r * 0.24f, k.r * 0.24f);
    line(k, R * 0.30f, R * 0.88f, k.r * 0.06f, k.accent.brighter(0.7f), k.a);
    gloss(k, R, 0.07f);
}
inline void drawGrd(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.95f, 21, k.r * 0.04f, k.accent.withAlpha(0.8f));
    const float R = k.r * 0.80f;
    disc(k, R, Colour(0xff0a0f16), Colour(0xff020306));
    g.saveState();
    juce::Path clip; clip.addEllipse(k.c.x - R, k.c.y - R, R * 2, R * 2); g.reduceClipRegion(clip, {});
    g.setColour(k.accent.withAlpha(0.28f));
    for (int i = -4; i <= 4; ++i) g.drawLine(k.c.x + float(i) * R * 0.09f, k.c.y + R * 0.05f, k.c.x + float(i) * R * 0.5f, k.c.y + R, 0.8f);
    for (int j = 1; j <= 5; ++j) { const float y = k.c.y + R * 0.05f + std::pow(float(j) / 5.0f, 2.0f) * R * 0.95f; g.drawHorizontalLine(int(y), k.c.x - R, k.c.x + R); }
    g.setGradientFill(juce::ColourGradient(k.accent.withAlpha(0.35f), k.c.x, k.c.y + R * 0.05f, Colour(0x00000000), k.c.x, k.c.y - R * 0.6f, false));
    g.fillRect(k.c.x - R, k.c.y - R * 0.6f, R * 2, R * 0.65f);
    g.restoreState();
    rim(k, R, k.accent, 1.3f);
    line(k, R * 0.10f, R * 0.96f, k.r * 0.07f, k.accent.brighter(0.8f), k.a);
}
inline void drawDsh(const Ctx& k) {
    auto& g = k.g;
    const float prop = juce::jlimit(0.0f, 1.0f, (k.a / pi + 0.8f) / 1.6f);
    disc(k, k.r * 0.98f, Colour(0xff15110c), Colour(0xff050404));
    const int n = 16;
    for (int i = 0; i < n; ++i) {
        const float f = float(i) / float(n - 1), t = (-0.8f + 1.6f * f) * pi;
        const float len = k.r * (0.10f + 0.28f * f);
        const bool lit = f <= prop + 0.5f / float(n);
        const Colour col = f > 0.84f ? Colour(0xffff3b2a) : k.accent;
        g.setColour(lit ? col : col.withAlpha(0.12f));
        g.drawLine({at(k, t, k.r * 0.60f), at(k, t, k.r * 0.60f + len)}, k.r * 0.075f);
    }
    const float R = k.r * 0.50f;
    disc(k, R, Colour(0xff2a2c30), Colour(0xff070809));
    rim(k, R, Colour(0xff000000));
    line(k, R * 0.1f, R * 0.95f, k.r * 0.06f, Colour(0xffff8a26), k.a);
    disc(k, k.r * 0.10f, Colour(0xff9a9fa3), Colour(0xff2a2d30));
}

// Ebn: ebony wood with a brass cap and an inlaid pointer line.
inline void drawEbn(const Ctx& k) {
    auto& g = k.g;
    woodKnob(k, Colour(0xff3a2c22), Colour(0xff0b0806), Colour(0xff2b2018), Colour(0xff0e0a07), Colour(0x30c8a070));
    const float C = k.r * 0.86f * 0.80f * 0.46f;
    disc(k, C, Colour(0xfff1d98a), Colour(0xff8a6218)); rim(k, C, Colour(0xff4a3208), 1.0f);
    g.setColour(Colour(0x44000000)); for (int i = 1; i <= 3; ++i) { const float e = C * float(i) / 3.2f; g.drawEllipse(k.c.x - e, k.c.y - e, e * 2, e * 2, 0.5f); }
    gloss(k, C, 0.25f);
}
// Wnt: walnut top inside a chrome collar.
inline void drawWnt(const Ctx& k) {
    auto& g = k.g;
    dots(k, 0.96f, 21, k.r * 0.04f, Colour(0xffe9eef2));
    const float R = k.r * 0.86f, W = R * 0.74f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.2f);
    barrelWall(k, R, R * 0.95f, elev * 2.0f, Colour(0xffeef2f4), Colour(0xff454d52));
    disc(k, R, Colour(0xffffffff), Colour(0xff555c61));
    bevelRim(k, R, Colour(0xffffffff), Colour(0xff181a1c), 1.2f);
    grubScrew(k, k.c + Point<float>(R * 0.64f, R * 0.58f), k.r * 0.05f, 0.785f, Colour(0xff3a3f44));
    disc(k, W, Colour(0xff9a6a3e), Colour(0xff3a2210));
    g.saveState();
    juce::Path clip; clip.addEllipse(k.c.x - W, k.c.y - W, W * 2, W * 2); g.reduceClipRegion(clip, {});
    g.addTransform(juce::AffineTransform::rotation(k.a, k.c.x, k.c.y));
    g.setColour(Colour(0x40201008));
    for (int i = -5; i <= 5; ++i) {
        juce::Path grain;
        for (int s2 = 0; s2 <= 16; ++s2) {
            const float x = k.c.x - W + W * 2 * float(s2) / 16.0f;
            const float y = k.c.y + float(i) * W * 0.17f + std::sin(float(s2) * 0.6f + float(i)) * W * 0.05f;
            s2 == 0 ? grain.startNewSubPath(x, y) : grain.lineTo(x, y);
        }
        g.strokePath(grain, juce::PathStrokeType(0.8f));
    }
    g.restoreState();
    bevelRim(k, W, Colour(0x77ffffff), Colour(0x55000000), 0.9f);
    gloss(k, W, 0.28f);
    line(k, W * 0.10f, W * 0.94f, k.r * 0.065f, k.accent.brighter(0.4f), k.a);
}

// Rck: Stillwell "The Rocket" style heavy industrial cast-metal knob: deep knurling, cast shadow, satin lathe cap, grub screw, engraved notch
inline void drawRck(const Ctx& k) {
    auto& g = k.g;
    const float R = k.r * 0.88f;
    const float elev = k.r * 0.035f;
    shadow(k, R, 1.35f);
    barrelWall(k, R, R * 0.96f, elev * 2.2f, Colour(0xff45484c), Colour(0xff060708));
    disc(k, R, Colour(0xff323538), Colour(0xff060708));
    // Heavy knurling flutes around the skirt
    flutes(k, R * 0.76f, R * 0.995f, 64, Colour(0xff030304), Colour(0xff686d73), 1.6f);
    bevelRim(k, R, Colour(0x65ffffff), Colour(0xff000000), 1.1f);
    // Satin top: radial sheen plus concentric micro-lathe grooves
    const float T = R * 0.74f;
    disc(k, T, Colour(0xff45484c), Colour(0xff0d0e10));
    anisotropicSheen(k, T, 0.22f);
    for (int i = 1; i <= 7; ++i) {
        g.setColour(Colour(i % 2 ? 0x0effffff : 0x12000000));
        const float e = T * float(i) / 7.2f;
        g.drawEllipse(k.c.x - e, k.c.y - e, e * 2, e * 2, 0.6f);
    }
    bevelRim(k, T, Colour(0x88ffffff), Colour(0x66000000), 1.0f);
    gloss(k, T, 0.25f);
    // Machined set screw on skirt
    grubScrew(k, k.c + Point<float>(R * 0.62f, R * 0.60f), k.r * 0.052f, 0.85f, Colour(0xff4a5056));
    // Pointer: engraved bright white/tinted line with deep shadow cut
    const auto col = k.accent.getPerceivedBrightness() > 0.2f ? k.accent : Colour(0xfff6f6f2);
    line(k, R * 0.12f, R * 0.97f, k.r * 0.075f, col, k.a);
}

using Drawer = void (*)(const Ctx&);
// Order must match goodlookinui::KnobStyle.
inline constexpr Drawer drawers[] = {
    nullptr, drawN, drawA, drawFS, drawMtl, drawWd, drawBk, drawPie, drawSnk, drawSlv, drawRd,
    drawPtr, drawDm, drawSq, drawSeg, drawVfd, drawCyb,
    drawFat, drawPag, drawLvr, drawTab, drawWh, drawKnl, drawSkt, drawRib, drawChr, drawArc, drawBrg, drawNeo,
    drawHex, drawYel, drawKey, drawBrz, drawChk, drawIvr, drawMpl, drawLed, drawDbl, drawMini, drawTl,
    drawGlw, drawGrd, drawDsh, drawMic, drawEbn, drawWnt, drawRck};
static_assert(std::size(drawers) == std::size(goodlookinui::knobStyles), "one drawer per knob style");
inline void draw(goodlookinui::KnobStyle style, const Ctx& k) {
    if (auto* d = drawers[int(style)]) d(k);
}
} // namespace goodlookinui::juce_adapter::knobs
