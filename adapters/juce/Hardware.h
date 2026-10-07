// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

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

// One rack ear: a slightly lighter plate with a slotted mounting hole.
inline void drawRackEar(Graphics& g, Rectangle<float> r, bool leftSide) {
    drawBrushedPanel(g, r, Colour(0xff2d2e31), Colour(0xff141516), leftSide ? 11u : 12u);
    g.setColour(Colour(0xff050506));
    g.drawVerticalLine(int(leftSide ? r.getRight() - 1 : r.getX()), r.getY(), r.getBottom());
    g.setColour(Colour(0x22ffffff));
    g.drawVerticalLine(int(leftSide ? r.getRight() - 2 : r.getX() + 1), r.getY() + 1, r.getBottom() - 1);
    const float sw = juce::jmin(r.getWidth() * 0.55f, 18.0f), sh = juce::jmin(r.getHeight() * 0.26f, 26.0f);
    auto slot = Rectangle<float>(sw, sh).withCentre({r.getCentreX(), r.getCentreY()});
    g.setColour(Colour(0xff000000)); g.fillRoundedRectangle(slot.expanded(1.5f), sw * 0.5f);
    g.setGradientFill(juce::ColourGradient(Colour(0xff020203), slot.getX(), slot.getY(), Colour(0xff17181a), slot.getX(), slot.getBottom(), false));
    g.fillRoundedRectangle(slot, sw * 0.5f);
    g.setColour(Colour(0x40ffffff)); g.drawRoundedRectangle(slot.expanded(1.5f), sw * 0.5f, 0.8f);
}

// Small rectangular key with a translucent LED cap. `lit` shows the lamp; text is drawn by the caller
// beside it, as the legends are printed on the panel rather than on the cap.
inline void drawLedKey(Graphics& g, Rectangle<float> r, Colour led, bool lit, bool down = false) {
    g.setColour(Colour(0xff030303)); g.fillRoundedRectangle(r.expanded(1.6f), 2.6f);
    g.setColour(Colour(0x30ffffff)); g.drawRoundedRectangle(r.expanded(1.6f), 2.6f, 0.7f);
    auto cap = r.reduced(0.6f).translated(0, down ? 0.8f : 0.0f);
    if (lit) {
        g.setColour(led.withAlpha(0.22f)); g.fillRoundedRectangle(cap.expanded(4.0f), 5.0f);
        g.setColour(led.withAlpha(0.20f)); g.fillRoundedRectangle(cap.expanded(2.0f), 3.5f);
    }
    const Colour hi = lit ? led.brighter(0.5f) : led.darker(0.78f), lo = lit ? led.darker(0.1f) : led.darker(0.95f);
    g.setGradientFill(juce::ColourGradient(hi, cap.getX(), cap.getY(), lo, cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, 2.0f);
    g.setColour(Colour(lit ? 0x55ffffff : 0x22ffffff));
    g.fillRoundedRectangle(cap.reduced(1.5f).removeFromTop(cap.getHeight() * 0.38f), 1.5f);
    g.setColour(Colour(0x66000000)); g.drawRoundedRectangle(cap, 2.0f, 0.8f);
}

// A white printed line with a gap for a title, as used to group controls on a panel.
inline void drawLegendLine(Graphics& g, Colour ink, float x0, float x1, float y, const juce::String& title, float fontSize = 9.5f) {
    juce::Font f(juce::FontOptions(fontSize, juce::Font::bold));
    const float tw = juce::GlyphArrangement::getStringWidth(f, title) + 10.0f, cx = 0.5f * (x0 + x1);
    g.setColour(ink);
    g.fillRect(x0, y, cx - tw * 0.5f - x0, 1.0f);
    g.fillRect(cx + tw * 0.5f, y, x1 - cx - tw * 0.5f, 1.0f);
    g.setFont(f);
    g.drawText(title, Rectangle<float>(cx - tw * 0.5f, y - 7.0f, tw, 14.0f), juce::Justification::centred, false);
}
} // namespace goodlookinui::juce_adapter::hardware
