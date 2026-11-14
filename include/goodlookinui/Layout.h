// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
namespace goodlookinui::layout {
struct Rect { float x, y, w, h; };
inline bool valid(Rect r) { return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h) && r.w >= 0 && r.h >= 0; }
// Equal-height cards, with equal outer/inter-card gaps reduced to fit small panels.
inline std::optional<Rect> readoutCard(Rect panel, std::size_t index, std::size_t count, float gap) {
    if (!valid(panel) || !count || index >= count || !std::isfinite(gap) || gap < 0) return {};
    const float n = float(count), g = std::min({gap, panel.h / (n + 1), panel.w * 0.5f});
    const float h = std::max(0.0f, (panel.h - g * (n + 1)) / n);
    return Rect{panel.x + g, panel.y + g + float(index) * (h + g), std::max(0.0f, panel.w - 2 * g), h};
}
inline std::optional<std::pair<Rect, Rect>> captionSwitch(Rect row, float switchWidth, float gap) {
    if (!valid(row) || !std::isfinite(switchWidth) || !std::isfinite(gap) || switchWidth < 0 || gap < 0) return {};
    const float w = std::min(switchWidth, row.w);
    return std::pair{Rect{row.x, row.y, std::max(0.0f, row.w - w - gap), row.h}, Rect{row.x + row.w - w, row.y, w, row.h}};
}
}
