// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#include <goodlookinui/Design.h>
#include <goodlookinui/Knob3DModels.h>
#include <cassert>
#include <iostream>
#include <set>
using namespace goodlookinui;
int main() {
    // style prefixes
    for (const auto& s : knobStyles) {
        for (auto [prefix, mode] : {std::pair{"", KnobMode::Classic}, {"flat_", KnobMode::Flat}, {"lit_", KnobMode::Lit}}) {
            const std::string name = std::string(prefix) + s.code;
            auto p = parseStyle(name);
            assert(p.mode == mode && p.code == s.code);
            assert(findStyle(name) && findStyle(name)->style == s.style);
            assert(isFlatStyle(name) == (mode == KnobMode::Flat));
        }
    }
    assert(parseStyle("flat").mode == KnobMode::Flat && parseStyle("flat").code == "Brit");
    for (auto* code : knob3d::PRESETS) assert(findStyle(std::string("lit_") + code));
    assert(!findStyle("lit_nope"));

    // every preset and physical style has a model; the 12 display styles have none
    knob3d::KnobModel m;
    for (auto* code : knob3d::PRESETS) assert(knob3d::preset(code, m));
    assert(!knob3d::preset("nope", m));
    std::set<std::string> physical(std::begin(knob3d::STYLE_CODES), std::end(knob3d::STYLE_CODES));
    assert(physical.size() == 35 && std::size(knobStyles) == 47);
    for (const auto& s : knobStyles) assert(knob3d::styleModel(s.code, m) == physical.count(s.code));

    // sprites: transparent corners, opaque centre, premultiplied, soft shadow, deterministic, angle matters
    for (auto* code : knob3d::PRESETS) {
        knob3d::preset(code, m);
        auto render = [&](float a) { return knob3d::render(m, knob3d::Rig::studio(), 14.0f, a, 0xffff9a1fu); };
        auto s = render(0.3f);
        assert(int(s.px.size()) == s.w * s.h);
        for (size_t i : {size_t(0), size_t(s.w - 1), size_t(s.h - 1) * size_t(s.w), s.px.size() - 1}) assert(s.px[i] == 0);
        assert((s.px[size_t(int(s.oy)) * size_t(s.w) + size_t(int(s.ox))] >> 24) == 0xff);
        bool soft = false;
        for (auto p : s.px) { const unsigned a = p >> 24; soft |= a > 0 && a < 255; assert(((p >> 16) & 255) <= a && ((p >> 8) & 255) <= a && (p & 255) <= a); }
        assert(soft);
        assert(render(0.3f).px == s.px);
        assert(render(-1.0f).px != render(1.0f).px);
    }

    // cache: same quantised request returns the same sprite, panel colour and angle change it
    knob3d::KnobCache cache(1u << 20);
    knob3d::preset("satin", m);
    const auto rig = knob3d::Rig::studio();
    auto a = cache.get(m, rig, 14.0f, 0.5f, 0xffff9a1fu, 0xff203060u);
    auto b = cache.get(m, rig, 14.1f, 0.5004f, 0xffff9a1fu, 0xff203060u);
    assert(a == b);
    assert(cache.get(m, rig, 14.0f, 0.5f, 0xffff9a1fu, 0xffc0c0c0u) != a);
    assert(cache.get(m, rig, 14.0f, -0.5f, 0xffff9a1fu, 0xff203060u) != a);
    assert(cache.stats().first == 3);
    std::cout << "knob3d ok\n";
}
