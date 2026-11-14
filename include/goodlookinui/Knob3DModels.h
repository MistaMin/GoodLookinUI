// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Quirino Deida
#pragma once
// Built-in lit knob models: 15 named presets and one model for each of the 35 physical catalogue styles.
// Ported from GoodLookinDri's knob3d presets and styles. The 12 display and neon styles have no model.
#include "Knob3D.h"

namespace goodlookinui::knob3d {

namespace presets_detail {
inline Material white() {
    return Material::plastic(0.93, 0.92, 0.88, 0.45).with_coat(0.5);
}

/// Satin-aluminium cylinder: nearly flat creamy top, a crisp dark line where the top
/// meets the polished wall, thin dark base ring.
inline KnobModel satin() {
    // half paint, half metal: keeps the soft form shading of a blasted face
    auto top = Material::plastic(0.95, 0.94, 0.91, 0.5).with_metallic(0.5).with_grain(0.012);
    auto gap = Material::plastic(0.03, 0.03, 0.035, 0.5);
    auto wall = Material::metal(0.93, 0.93, 0.94, 0.26);
    auto base = Material::metal(0.22, 0.22, 0.23, 0.30);
    return KnobModel::make({
        seg(Dome, 0.0, 0.80, 0.86, 0.855, top),
        seg(Dome, 0.80, 0.845, 0.855, 0.835, top),
        seg(Flat, 0.845, 0.862, 0.81, 0.81, gap),
        seg(Dome, 0.862, 0.91, 0.825, 0.76, wall),
        seg(Line, 0.91, 0.935, 0.76, 0.06, wall),
        seg(Flat, 0.935, 0.965, 0.05, 0.05, base),
        seg(Dome, 0.965, 0.985, 0.05, 0.02, base),
    })
    .with_edge(0.008)
    .with_pointer(Pointer::groove(0.50, 0.94, 0.10, 0.016, Material::ink()));
}

/// Black scalloped program-equaliser knob: eight-lobed grip with a flat top ring and a
/// recessed centre, on a wide flared skirt; the white index line is on the skirt and again
/// on the top ring, so it shows when the grip hides the skirt behind it.
inline KnobModel bakelite() {
    auto black = Material::black_polished();
    return KnobModel::make({
        seg(Dome, 0.0, 0.31, 0.885, 0.875, black),
        seg(Line, 0.31, 0.345, 0.875, 0.96, black),
        seg(Flat, 0.345, 0.52, 0.96, 0.96, black),
        seg(Dome, 0.52, 0.66, 0.96, 0.82, black),
        seg(Line, 0.66, 0.70, 0.82, 0.30, black),
    })
    .with_under({seg(Flat, 0.0, 0.60, 0.40, 0.40, black), seg(Line, 0.60, 0.965, 0.40, 0.09, black), seg(Dome, 0.965, 1.0, 0.09, 0.02, black)})
    .with_angular(Angular::scallops(8, 0.13, 0.50).r_out(0.9))
    .with_edge(0.02)
    .with_pointer(Pointer::line(0.72, 0.985, 0.075, white()))
    .with_mark(Pointer::line(0.36, 0.60, 0.08, white()));
}

/// Spun-aluminium cap on a knurled black skirt.
inline KnobModel spun() {
    auto cap = Material::spun_aluminium();
    auto rim = Material::metal(0.92, 0.92, 0.93, 0.10);
    auto skirt = Material::plastic(0.085, 0.085, 0.09, 0.36);
    return KnobModel::make({
        seg(Dome, 0.0, 0.60, 1.02, 1.00, cap),
        seg(Line, 0.60, 0.64, 1.00, 0.95, rim),
        seg(Line, 0.64, 0.66, 0.95, 0.66, rim),
        seg(Line, 0.66, 0.76, 0.66, 0.60, skirt),
        seg(Line, 0.76, 0.96, 0.60, 0.10, skirt),
        seg(Dome, 0.96, 1.0, 0.10, 0.04, skirt),
    })
    .with_angular(Angular::knurl(40, 0.045, 0.76))
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.18, 0.60, 0.06, 0.012, Material::ink()));
}

/// Glossy cap in the accent colour with a darker wall of the same hue.
inline KnobModel gloss() {
    auto cap = Material::plastic(0.5, 0.5, 0.5, 0.5).with_accent(1.0).with_coat(1.0).with_grain(0.02);
    auto wall = Material::plastic(0.5, 0.5, 0.5, 0.5).with_accent_shade(1.0, 0.7).with_coat(0.8);
    auto foot = Material::plastic(0.03, 0.03, 0.03, 0.6);
    return KnobModel::make({
        seg(Dome, 0.0, 0.74, 1.00, 0.95, cap),
        seg(Dome, 0.74, 0.86, 0.95, 0.84, cap),
        seg(Line, 0.86, 0.90, 0.84, 0.07, wall),
        seg(Flat, 0.90, 0.95, 0.06, 0.06, foot),
        seg(Dome, 0.95, 0.97, 0.06, 0.02, foot),
    })
    .with_edge(0.02)
    .with_pointer(Pointer::line(0.30, 0.86, 0.09, white()));
}

/// Console-strip knob: straight-sided tapered body with a foot ring, flat cap disc in the
/// accent colour and a white line that runs over the cap edge and down the wall.
inline KnobModel strip() {
    auto cap = Material::plastic(0.5, 0.5, 0.5, 0.45).with_accent(1.0).with_coat(0.55);
    auto body = Material::plastic(0.25, 0.25, 0.26, 0.48).with_sheen(0.2);
    return KnobModel::make({
        seg(Dome, 0.0, 0.50, 1.162, 1.15, cap),
        seg(Flat, 0.50, 0.52, 1.13, 1.13, body),
        seg(Flat, 0.52, 0.57, 1.15, 1.15, body),
        seg(Line, 0.57, 0.61, 1.15, 1.10, body),
        seg(Line, 0.61, 0.80, 1.10, 0.14, body),
        seg(Flat, 0.80, 0.86, 0.12, 0.12, body),
        seg(Line, 0.86, 0.885, 0.12, 0.02, body),
    })
    .with_angular(Angular::flutes(26, 0.018, 0.64).r_out(0.80))
    .with_edge(0.008)
    .with_pointer(Pointer::line(0.20, 0.80, 0.08, Material::white_paint()));
}

/// Grey ribbed cap with a flat insert inside a wide polished bezel that carries the index.
inline KnobModel collet() {
    auto cap = Material::plastic(0.42, 0.43, 0.45, 0.55);
    auto face = Material::plastic(0.56, 0.57, 0.59, 0.5).with_coat(0.3);
    auto bezel = Material::metal(0.96, 0.96, 0.97, 0.07);
    return KnobModel::make({
        seg(Flat, 0.0, 0.40, 0.97, 0.97, face),
        seg(Line, 0.40, 0.425, 0.97, 1.0, cap),
        seg(Dome, 0.425, 0.56, 1.0, 0.94, cap),
        seg(Line, 0.56, 0.60, 0.94, 0.26, cap),
    })
    .with_under({
        seg(Flat, 0.0, 0.62, 0.26, 0.26, bezel),
        seg(Line, 0.62, 0.68, 0.26, 0.32, bezel),
        seg(Dome, 0.68, 0.955, 0.32, 0.08, bezel),
        seg(Dome, 0.955, 0.985, 0.08, 0.02, bezel),
    })
    .with_angular(Angular::flutes(18, 0.07, 0.50))
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.66, 0.97, 0.10, 0.012, Material::ink()))
    .with_mark(Pointer::line(0.08, 0.39, 0.09, Material::ink()));
}

/// Black chicken-head pointer: a teardrop with a flat crest that falls away to the tip
/// and a white inlay line along it, on a short knurled skirt.
inline KnobModel chicken() {
    auto black = Material::black_polished();
    auto beak = Part::make(
        {
            seg(Flat, 0.0, 0.70, 0.96, 0.96, black),
            seg(Line, 0.70, 0.92, 0.96, 0.80, black),
            seg(Line, 0.92, 0.97, 0.80, 0.70, black),
            seg(Line, 0.97, 1.0, 0.70, 0.22, black),
        },
        Plan::bar(1.0, 0.0, 0.055, 0.50, 0.08)
    );
    return KnobModel::make({
        seg(Flat, 0.0, 0.60, 0.26, 0.26, black),
        seg(Line, 0.60, 0.64, 0.26, 0.22, black),
        seg(Line, 0.64, 0.68, 0.22, 0.03, black),
    })
    .with_under({seg(Flat, 0.0, 0.43, 0.96, 0.96, black), seg(Line, 0.43, 0.50, 0.96, 0.86, black), seg(Line, 0.50, 0.53, 0.86, 0.22, black)})
    .with_angular(Angular::knurl(38, 0.03, 0.60))
    .with_part(beak)
    .with_edge(0.016)
    .with_pointer(Pointer::line(0.0, 0.975, 0.05, white()));
}

/// Die-cast winged pointer knob: round body with a large recessed accent insert, a long
/// fin that stands proud of the rim from the edge of the insert out to the tip, and
/// three short lugs, on a thin flange.
inline KnobModel wing() {
    auto cast = Material::die_cast();
    auto disc = Material::plastic(0.5, 0.5, 0.5, 0.5).with_accent(1.0).with_coat(0.35).with_grain(0.03);
    auto fin = Part::make(
        {seg(Flat, 0.0, 0.93, 0.88, 0.88, cast), seg(Line, 0.93, 0.975, 0.88, 0.78, cast), seg(Line, 0.975, 1.0, 0.78, 0.05, cast)},
        Plan::bar(1.0, -0.43, 0.11, 0.27, 0.06)
    );
    auto lugs = Part::make(
        {seg(Flat, 0.0, 0.58, 0.56, 0.56, cast), seg(Line, 0.58, 0.63, 0.56, 0.48, cast), seg(Line, 0.63, 0.66, 0.48, 0.05, cast)},
        Plan::round()
    )
    .with_angular(Angular::teeth(4, 0.24, 0.20, 0.25));
    return KnobModel::make({
        seg(Flat, 0.0, 0.47, 0.78, 0.78, cast),
        seg(Line, 0.47, 0.50, 0.78, 0.75, cast),
        seg(Line, 0.50, 0.52, 0.75, 0.12, cast),
        seg(Flat, 0.52, 0.57, 0.10, 0.10, cast),
        seg(Line, 0.57, 0.595, 0.10, 0.02, cast),
    })
    .with_part(fin)
    .with_part(lugs)
    .with_insert(0.42, 0.04, disc)
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.50, 0.95, 0.045, 0.014, Material::ink()));
}

/// Cream fluted vintage knob on a flared skirt, dark pointer line.
inline KnobModel ivory() {
    auto cream = Material::cream();
    return KnobModel::make({seg(Dome, 0.0, 0.56, 0.96, 0.93, cream), seg(Dome, 0.56, 0.74, 0.93, 0.80, cream), seg(Line, 0.74, 0.79, 0.80, 0.12, cream)})
        .with_under({seg(Flat, 0.0, 0.80, 0.16, 0.16, cream), seg(Dome, 0.80, 0.92, 0.16, 0.03, cream)})
        .with_angular(Angular::flutes(18, 0.05, 0.62))
        .with_edge(0.02)
        .with_pointer(Pointer::line(0.16, 0.70, 0.055, Material::plastic(0.10, 0.07, 0.05, 0.6)));
}

/// Chrome skirted knob with a small cap.
inline KnobModel chrome() {
    auto cr = Material::chrome();
    return KnobModel::make({
        seg(Dome, 0.0, 0.40, 1.00, 0.97, cr),
        seg(Dome, 0.40, 0.50, 0.97, 0.88, cr),
        seg(Line, 0.50, 0.54, 0.88, 0.52, cr),
        seg(Fillet, 0.54, 0.93, 0.52, 0.10, cr),
        seg(Dome, 0.93, 0.99, 0.10, 0.03, cr),
    })
    .with_edge(0.022)
    .with_pointer(Pointer::groove(0.56, 0.95, 0.11, 0.012, Material::ink()))
    .with_mark(Pointer::groove(0.06, 0.38, 0.10, 0.012, Material::ink()));
}

/// Plain black skirted pointer knob: white line on the cap and again on the skirt.
inline KnobModel phenolic() {
    auto black = Material::black_polished();
    return KnobModel::make({
        seg(Dome, 0.0, 0.42, 0.84, 0.82, black),
        seg(Dome, 0.42, 0.52, 0.82, 0.74, black),
        seg(Line, 0.52, 0.58, 0.74, 0.46, black),
        seg(Line, 0.58, 0.93, 0.46, 0.12, black),
        seg(Dome, 0.93, 0.975, 0.12, 0.02, black),
    })
    .with_edge(0.018)
    .with_pointer(Pointer::line(0.08, 0.46, 0.06, white()))
    .with_mark(Pointer::line(0.64, 0.93, 0.06, white()));
}

/// Lacquered walnut knob: low crowned top with a turned shoulder, brass foot ring and a
/// pale inlaid pointer line.
inline KnobModel wood() {
    auto walnut = Material::walnut().with_coat(0.35).with_roughness(0.5);
    auto foot = Material::brass().with_roughness(0.25);
    return KnobModel::make({
        seg(Dome, 0.0, 0.70, 0.90, 0.87, walnut),
        seg(Dome, 0.70, 0.84, 0.87, 0.76, walnut),
        seg(Line, 0.84, 0.90, 0.76, 0.10, walnut),
        seg(Flat, 0.90, 0.945, 0.08, 0.08, foot),
        seg(Dome, 0.945, 0.97, 0.08, 0.02, foot),
    })
    .with_edge(0.016)
    .with_pointer(Pointer::line(0.30, 0.82, 0.065, Material::plastic(0.90, 0.84, 0.68, 0.5).with_coat(0.5)));
}

/// Knurled brass knob with a turned face.
inline KnobModel brass() {
    auto face = Material::brass().with_roughness(0.12).with_aniso(0.4).with_rings(0.6);
    auto side = Material::brass();
    return KnobModel::make({
        seg(Dome, 0.0, 0.70, 0.86, 0.85, face),
        seg(Line, 0.70, 0.76, 0.85, 0.80, side),
        seg(Line, 0.76, 0.80, 0.80, 0.80, side),
        seg(Line, 0.80, 0.90, 0.80, 0.14, side),
        seg(Flat, 0.90, 0.945, 0.12, 0.12, side),
        seg(Dome, 0.945, 0.97, 0.12, 0.02, side),
    })
    .with_angular(Angular::knurl(44, 0.04, 0.80).r_out(0.9))
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.22, 0.70, 0.055, 0.012, Material::ink()));
}

/// Moulded black dome knob: a smooth dome standing on a stepped skirt ring, with a clear
/// white dot as pointer.
inline KnobModel dome() {
    auto body = Material::plastic(0.09, 0.09, 0.10, 0.40).with_coat(0.7).with_grain(0.02);
    return KnobModel::make({
        seg(Dome, 0.0, 0.70, 0.86, 0.46, body),
        seg(Line, 0.70, 0.725, 0.46, 0.27, body),
        seg(Flat, 0.725, 0.76, 0.26, 0.26, body),
        seg(Dome, 0.76, 0.93, 0.27, 0.10, body),
        seg(Line, 0.93, 0.96, 0.10, 0.02, body),
    })
    .with_edge(0.02)
    .with_pointer(Pointer::dot(0.47, 0.17, 0.012, Material::white_paint()));
}

/// Two-tier concentric knob: wide knurled lower ring, smaller satin cap on top.
inline KnobModel dual() {
    auto lower = Material::anodised(0.16, 0.16, 0.17).with_roughness(0.38);
    auto upper = Material::satin_aluminium();
    return KnobModel::two_tier(0.40, 0.56, 0.98, lower, upper)
        .with_angular(Angular::knurl(48, 0.035, 0.90))
        .with_edge(0.012)
        .with_pointer(Pointer::groove(0.14, 0.56, 0.06, 0.012, Material::ink()))
        .with_mark(Pointer::line(0.66, 0.93, 0.045, Material::white_paint()));
}

} // namespace presets_detail

namespace styles_detail {
inline Material satin_black();
/// Matte paint in the accent colour, for cap discs.
inline Material accent_matte() {
    return Material::plastic(0.5, 0.5, 0.5, 0.55).with_accent(1.0).with_coat(0.15);
}
/// Glossy plastic in the accent colour, for moulded caps.
inline Material accent_gloss() {
    return Material::plastic(0.5, 0.5, 0.5, 0.45).with_accent(1.0).with_coat(0.8);
}
/// Paint in the accent colour for pointer lines.
inline Material accent_paint() {
    return Material::plastic(0.5, 0.5, 0.5, 0.6).with_accent(1.0);
}
inline Material white() {
    return Material::white_paint();
}
inline Material polished() {
    return Material::metal(0.93, 0.93, 0.94, 0.12);
}

/// Black fluted grip under a large flat cap disc in the accent colour; the white index
/// is milled into the cap.
inline KnobModel brit() {
    auto grip = Material::black_matte().with_grain(0.03);
    auto cap = accent_matte();
    return KnobModel::make({
        seg(Dome, 0.0, 0.56, 1.03, 1.015, cap),
        seg(Line, 0.56, 0.585, 1.015, 0.99, cap),
        seg(Flat, 0.585, 0.61, 0.955, 0.955, grip),
        seg(Dome, 0.61, 0.70, 0.99, 0.93, grip),
        seg(Line, 0.70, 0.90, 0.93, 0.10, grip),
        seg(Dome, 0.90, 0.93, 0.10, 0.02, grip),
    })
    .with_angular(Angular::flutes(24, 0.03, 0.64))
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.10, 0.56, 0.095, 0.014, white()));
}

/// Grey fluted grip with a flat accent cap, standing in a rounded chrome collar that
/// repeats the index.
inline KnobModel n() {
    auto cr = Material::metal(0.95, 0.95, 0.96, 0.20);
    auto grip = Material::plastic(0.40, 0.41, 0.43, 0.55);
    auto cap = accent_matte();
    return KnobModel::make({
        seg(Flat, 0.0, 0.43, 1.0, 1.0, cap),
        seg(Flat, 0.43, 0.455, 0.965, 0.965, grip),
        seg(Dome, 0.455, 0.56, 0.99, 0.92, grip),
        seg(Line, 0.56, 0.66, 0.92, 0.28, grip),
    })
    .with_under({seg(Flat, 0.0, 0.72, 0.30, 0.30, cr), seg(Dome, 0.72, 0.965, 0.30, 0.06, cr), seg(Line, 0.965, 0.985, 0.06, 0.02, cr)})
    .with_angular(Angular::flutes(16, 0.045, 0.50))
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.10, 0.43, 0.06, 0.012, Material::ink()))
    .with_mark(Pointer::line(0.74, 0.95, 0.05, Material::ink()));
}

/// Die-cast silver winged pointer, low and flared: a waisted crown with a large flat
/// matte cap in the accent colour let into the top, and a long fin that stands proud of
/// the rim from the edge of the cap out to the tip.
inline KnobModel a() {
    auto cast = Material::die_cast();
    auto fin = Part::make(
        {seg(Flat, 0.0, 0.93, 0.74, 0.74, cast), seg(Line, 0.93, 0.975, 0.74, 0.66, cast), seg(Line, 0.975, 1.0, 0.66, 0.05, cast)},
        Plan::bar(1.0, -0.43, 0.11, 0.27, 0.06)
    );
    return KnobModel::make({
        seg(Flat, 0.0, 0.47, 0.64, 0.64, cast),
        seg(Line, 0.47, 0.50, 0.64, 0.61, cast),
        seg(Line, 0.50, 0.525, 0.61, 0.28, cast),
        seg(Fillet, 0.525, 0.63, 0.28, 0.10, cast),
        seg(Dome, 0.63, 0.66, 0.10, 0.02, cast),
    })
    .with_part(fin)
    .with_insert(0.42, 0.035, accent_matte())
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.50, 0.95, 0.045, 0.014, Material::ink()));
}

/// Wide pale-grey skirt with a glossy cap in the accent colour; the black index runs
/// from the cap out over the skirt.
inline KnobModel fs() {
    auto skirt = Material::plastic(0.80, 0.81, 0.82, 0.5).with_coat(0.25);
    auto cap = accent_gloss();
    return KnobModel::make({
        seg(Dome, 0.0, 0.50, 0.80, 0.75, cap),
        seg(Dome, 0.50, 0.60, 0.75, 0.62, cap),
        seg(Line, 0.60, 0.62, 0.62, 0.38, cap),
        seg(Line, 0.62, 0.67, 0.38, 0.43, skirt),
        seg(Flat, 0.67, 0.85, 0.43, 0.43, skirt),
        seg(Dome, 0.85, 0.97, 0.43, 0.08, skirt),
        seg(Line, 0.97, 0.985, 0.08, 0.02, skirt),
    })
    .with_edge(0.02)
    .with_pointer(Pointer::line(0.12, 0.95, 0.06, Material::ink()));
}

/// Solid aluminium: knurled satin wall, polished chamfer and a turned top.
inline KnobModel mtl() {
    auto top = Material::spun_aluminium();
    auto wall = Material::satin_aluminium();
    return KnobModel::make({
        seg(Dome, 0.0, 0.74, 1.0, 0.99, top),
        seg(Line, 0.74, 0.80, 0.99, 0.93, polished()),
        seg(Line, 0.80, 0.84, 0.93, 0.91, wall),
        seg(Line, 0.84, 0.90, 0.91, 0.10, wall),
        seg(Flat, 0.90, 0.94, 0.08, 0.08, wall),
        seg(Dome, 0.94, 0.96, 0.08, 0.02, wall),
    })
    .with_angular(Angular::knurl(52, 0.03, 0.80).r_out(0.90))
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.20, 0.74, 0.055, 0.012, Material::ink()));
}

/// Turned mid-brown wooden knob: slightly dished flat top with a crisp edge, a lathe
/// groove round the wall, satin lacquer and a pale inlaid index.
inline KnobModel wd() {
    auto wood = Material::wood(0.60, 0.38, 0.20).with_coat(0.22).with_roughness(0.62);
    return KnobModel::make({
        seg(Dome, 0.0, 0.70, 0.69, 0.715, wood),
        seg(Line, 0.70, 0.78, 0.715, 0.66, wood),
        seg(Line, 0.78, 0.82, 0.66, 0.44, wood),
        seg(Flat, 0.82, 0.835, 0.40, 0.40, wood),
        seg(Dome, 0.835, 0.87, 0.44, 0.38, wood),
        seg(Line, 0.87, 0.92, 0.38, 0.08, wood),
        seg(Dome, 0.92, 0.95, 0.08, 0.02, wood),
    })
    .with_edge(0.012)
    .with_pointer(Pointer::line(0.14, 0.77, 0.08, Material::plastic(0.95, 0.90, 0.76, 0.5).with_coat(0.5)));
}

/// Black bakelite teardrop: a domed hub drawn out into a long pointer nose with a white
/// line along its crest, over a ribbed base ring.
inline KnobModel bk() {
    auto black = Material::black_gloss().with_grain(0.02);
    auto nose = Part::make(
        {seg(Flat, 0.0, 0.70, 0.84, 0.84, black), seg(Line, 0.70, 0.90, 0.84, 0.62, black), seg(Line, 0.90, 1.0, 0.62, 0.22, black)},
        Plan::bar(1.0, 0.0, 0.10, 0.42, 0.10)
    );
    return KnobModel::make({seg(Flat, 0.0, 0.78, 0.24, 0.24, black), seg(Dome, 0.78, 0.94, 0.24, 0.05, black), seg(Line, 0.94, 0.96, 0.05, 0.02, black)})
        .with_under({seg(Dome, 0.0, 0.32, 0.87, 0.84, black), seg(Dome, 0.32, 0.44, 0.84, 0.66, black), seg(Line, 0.44, 0.48, 0.66, 0.20, black)})
        .with_angular(Angular::flutes(28, 0.03, 0.60))
        .with_part(nose)
        .with_edge(0.018)
        .with_pointer(Pointer::line(0.34, 0.95, 0.06, white()));
}

/// White pie-crust skirt with rounded lobes under a glossy black cap.
inline KnobModel pie() {
    auto crust = Material::plastic(0.90, 0.90, 0.87, 0.45).with_coat(0.4);
    auto black = Material::black_gloss();
    return KnobModel::make({seg(Flat, 0.0, 0.76, 0.30, 0.30, crust), seg(Dome, 0.76, 0.97, 0.30, 0.06, crust), seg(Line, 0.97, 0.99, 0.06, 0.02, crust)})
        .with_under({seg(Dome, 0.0, 0.50, 0.84, 0.78, black), seg(Dome, 0.50, 0.62, 0.78, 0.62, black), seg(Line, 0.62, 0.65, 0.62, 0.28, black)})
        .with_angular(Angular::scallops(18, 0.08, 0.72))
        .with_edge(0.02)
        .with_pointer(Pointer::line(0.08, 0.61, 0.095, accent_paint()))
        .with_mark(Pointer::line(0.70, 0.90, 0.05, Material::ink()));
}

/// Plain matte black cylinder with a fine grip and a pointer line in the accent colour.
inline KnobModel snk() {
    auto body = Material::plastic(0.15, 0.15, 0.155, 0.40).with_sheen(0.3).with_grain(0.02);
    return KnobModel::make({
        seg(Dome, 0.0, 0.66, 1.0, 0.975, body),
        seg(Line, 0.66, 0.75, 0.975, 0.92, body),
        seg(Line, 0.75, 0.79, 0.92, 0.07, body),
        seg(Flat, 0.79, 0.83, 0.06, 0.06, body),
        seg(Dome, 0.83, 0.85, 0.06, 0.02, body),
    })
    .with_angular(Angular::knurl(40, 0.02, 0.73).r_out(0.79))
    .with_edge(0.016)
    .with_pointer(Pointer::line(0.24, 0.72, 0.06, accent_paint()));
}

/// Silver-grey cylinder on a dark ring inside a satin silver skirt, black index.
inline KnobModel slv() {
    auto top = Material::plastic(0.88, 0.89, 0.90, 0.5).with_metallic(0.5).with_grain(0.012);
    auto ring = Material::plastic(0.06, 0.065, 0.07, 0.5);
    auto skirt = Material::satin_aluminium();
    return KnobModel::make({
        seg(Dome, 0.0, 0.52, 0.92, 0.91, top),
        seg(Dome, 0.52, 0.58, 0.91, 0.86, top),
        seg(Line, 0.58, 0.61, 0.86, 0.20, polished()),
    })
    .with_under({
        seg(Flat, 0.0, 0.79, 0.20, 0.20, ring),
        seg(Line, 0.79, 0.82, 0.20, 0.28, skirt),
        seg(Flat, 0.82, 0.92, 0.28, 0.28, skirt),
        seg(Dome, 0.92, 0.99, 0.28, 0.03, skirt),
    })
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.08, 0.53, 0.095, 0.012, Material::ink()))
    .with_mark(Pointer::dot(0.70, 0.09, 0.0, white()));
}

/// Round black body with a ribbed shoulder, a glossy cap disc in the accent colour set
/// in the top and a white index on the shoulder.
inline KnobModel rd() {
    auto black = Material::black_gloss().with_grain(0.03f);
    return KnobModel::make({
        seg(Dome, 0.0, 0.49, 0.925, 0.905, accent_gloss()),
        seg(Line, 0.49, 0.51, 0.905, 0.885, accent_gloss()),
        seg(Flat, 0.51, 0.56, 0.90, 0.90, black),
        seg(Dome, 0.56, 0.84, 0.90, 0.66, black),
        seg(Line, 0.84, 0.90, 0.66, 0.08, black),
        seg(Dome, 0.90, 0.93, 0.08, 0.02, black),
    })
    .with_angular(Angular::knurl(36, 0.025, 0.62))
    .with_edge(0.02)
    .with_pointer(Pointer::line(0.56, 0.90, 0.085, white()));
}

/// Flat-topped silver pointer: a wide tapered bar on a low round hub, dark index.
inline KnobModel ptr() {
    auto alu = Material::satin_aluminium();
    auto bar = Part::make(
        {seg(Flat, 0.0, 0.93, 0.82, 0.82, alu), seg(Line, 0.93, 0.98, 0.82, 0.72, polished()), seg(Line, 0.98, 1.0, 0.72, 0.10, alu)},
        Plan::bar(1.0, 0.60, 0.15, 0.36, 0.06)
    );
    return KnobModel::make({seg(Flat, 0.0, 0.54, 0.30, 0.30, alu), seg(Line, 0.54, 0.61, 0.30, 0.22, alu), seg(Line, 0.61, 0.64, 0.22, 0.03, alu)})
        .with_part(bar)
        .with_edge(0.014)
        .with_pointer(Pointer::groove(0.25, 0.95, 0.045, 0.012, Material::ink()));
}

/// Chunky dome moulded in the item colour on a black base ring, with a white dimple.
inline KnobModel dm() {
    auto body = Material::plastic(0.5, 0.5, 0.5, 0.45).with_accent_shade(1.0, 0.85).with_coat(0.7).with_grain(0.02);
    auto base = Material::black_matte();
    return KnobModel::make({
        seg(Dome, 0.0, 0.80, 0.88, 0.50, body),
        seg(Line, 0.80, 0.84, 0.50, 0.12, body),
        seg(Flat, 0.84, 0.92, 0.10, 0.10, base),
        seg(Dome, 0.92, 0.95, 0.10, 0.02, base),
    })
    .with_edge(0.03)
    .with_pointer(Pointer::dot(0.52, 0.20, 0.014, white()));
}

/// Square keycap with rounded corners, sloped sides, a smoothly dished top and a legend
/// line.
inline KnobModel sq() {
    auto cap = Material::plastic(0.17, 0.18, 0.19, 0.55).with_sheen(0.3);
    return KnobModel::make({
        seg(Dome, 0.0, 0.70, 0.50, 0.585, cap),
        seg(Dome, 0.70, 0.78, 0.585, 0.53, cap),
        seg(Line, 0.78, 0.96, 0.53, 0.05, cap),
        seg(Dome, 0.96, 0.985, 0.05, 0.02, cap),
    })
    .with_plan(Plan::polygon(4, 0.35))
    .with_edge(0.022)
    .with_pointer(Pointer::line(0.10, 0.60, 0.07, white()));
}

/// Large black knob as on the classic drawing: a big smooth polished dome inside a broad
/// ribbed skirt, with a thick raised white pointer bar running from the dome out to the
/// very edge.
inline KnobModel fat() {
    auto dome = Material::black_polished();
    auto skirt = satin_black();
    return KnobModel::make({
        seg(Dome, 0.0, 0.58, 0.60, 0.50, dome),
        seg(Line, 0.58, 0.61, 0.50, 0.44, dome),
        seg(Flat, 0.61, 0.64, 0.42, 0.42, skirt),
        seg(Dome, 0.64, 0.72, 0.47, 0.44, skirt),
        seg(Line, 0.72, 0.95, 0.44, 0.26, skirt),
        seg(Line, 0.95, 0.98, 0.26, 0.10, skirt),
        seg(Dome, 0.98, 1.0, 0.10, 0.02, skirt),
    })
    .with_angular(Angular::knurl(44, 0.035, 0.70))
    .with_edge(0.014)
    .with_pointer(Pointer::ridge(0.16, 0.97, 0.12, 0.03, white()));
}

/// Black octagonal bakelite: eight flat sides with a bevelled top ring around a recessed
/// centre, standing on an octagonal flange.
inline KnobModel pag() {
    auto black = Material::black_polished();
    return KnobModel::make({
        seg(Dome, 0.0, 0.30, 0.50, 0.49, black),
        seg(Line, 0.30, 0.35, 0.49, 0.60, black),
        seg(Flat, 0.35, 0.58, 0.60, 0.60, black),
        seg(Line, 0.58, 0.68, 0.60, 0.50, black),
        seg(Line, 0.68, 0.72, 0.50, 0.17, black),
        seg(Line, 0.72, 0.94, 0.17, 0.10, black),
        seg(Dome, 0.94, 1.0, 0.10, 0.02, black),
    })
    .with_plan(Plan::polygon(8, 0.10))
    .with_edge(0.014)
    .with_pointer(Pointer::line(0.30, 0.68, 0.09, accent_paint()))
    .with_mark(Pointer::line(0.75, 0.94, 0.09, accent_paint()));
}

/// Lever: a tapered bar with rounded shoulders across a domed hub, on a knurled base
/// ring; the tip of the bar is painted in the accent colour.
inline KnobModel lvr() {
    auto black = Material::black_polished();
    auto ring = satin_black();
    auto bar = Part::make(
        {seg(Flat, 0.0, 0.89, 0.70, 0.70, black), seg(Dome, 0.89, 0.97, 0.70, 0.54, black), seg(Line, 0.97, 1.0, 0.54, 0.10, black)},
        Plan::bar(1.0, 0.60, 0.11, 0.19, 0.10)
    );
    return KnobModel::make({seg(Flat, 0.0, 0.70, 0.22, 0.22, ring), seg(Line, 0.70, 0.75, 0.22, 0.17, ring), seg(Line, 0.75, 0.79, 0.17, 0.02, ring)})
        .with_under({seg(Dome, 0.0, 0.38, 0.58, 0.55, black), seg(Dome, 0.38, 0.50, 0.55, 0.42, black), seg(Line, 0.50, 0.54, 0.42, 0.20, black)})
        .with_angular(Angular::knurl(40, 0.03, 0.70))
        .with_part(bar)
        .with_edge(0.014)
        .with_pointer(Pointer::line(0.50, 0.965, 0.12, accent_paint()));
}

/// Matte black cylinder with a knurled wall and a tab at the front that carries the
/// white index from the top face down to its tip.
inline KnobModel tab() {
    auto black = satin_black();
    auto tab = Part::make(
        {seg(Flat, 0.0, 0.90, 0.46, 0.46, black), seg(Line, 0.90, 0.96, 0.46, 0.38, black), seg(Line, 0.96, 1.0, 0.38, 0.04, black)},
        Plan::bar(1.0, 0.0, 0.11, 0.19, 0.06)
    );
    return KnobModel::make({
        seg(Dome, 0.0, 0.62, 0.74, 0.72, black),
        seg(Line, 0.62, 0.69, 0.72, 0.67, black),
        seg(Line, 0.69, 0.73, 0.67, 0.08, black),
        seg(Flat, 0.73, 0.77, 0.07, 0.07, black),
        seg(Dome, 0.77, 0.79, 0.07, 0.02, black),
    })
    .with_angular(Angular::knurl(34, 0.03, 0.67).r_out(0.73))
    .with_part(tab)
    .with_edge(0.014)
    .with_pointer(Pointer::line(0.78, 0.97, 0.085, white()))
    .with_mark(Pointer::line(0.16, 0.64, 0.075, white()));
}

/// Wide black knurled skirt around a white cap with a black index.
inline KnobModel wh() {
    auto cap = Material::plastic(0.93, 0.93, 0.91, 0.45).with_coat(0.5);
    auto skirt = Material::plastic(0.075, 0.075, 0.08, 0.38);
    return KnobModel::make({
        seg(Dome, 0.0, 0.60, 0.80, 0.775, cap),
        seg(Line, 0.60, 0.63, 0.775, 0.74, cap),
        seg(Flat, 0.63, 0.70, 0.72, 0.72, skirt),
        seg(Line, 0.70, 0.96, 0.72, 0.10, skirt),
        seg(Dome, 0.96, 1.0, 0.10, 0.02, skirt),
    })
    .with_angular(Angular::knurl(44, 0.035, 0.70))
    .with_edge(0.012)
    .with_pointer(Pointer::line(0.12, 0.58, 0.06, Material::ink()));
}

/// Dark anodised cylinder with a coarse straight knurl and a turned aluminium cap.
inline KnobModel knl() {
    auto cap = Material::spun_aluminium();
    auto body = Material::anodised(0.13, 0.13, 0.14);
    return KnobModel::make({
        seg(Dome, 0.0, 0.66, 1.0, 0.99, cap),
        seg(Line, 0.66, 0.70, 0.99, 0.95, polished()),
        seg(Line, 0.70, 0.84, 0.95, 0.90, body),
        seg(Line, 0.84, 0.88, 0.90, 0.08, body),
        seg(Flat, 0.88, 0.92, 0.07, 0.07, body),
        seg(Dome, 0.92, 0.94, 0.07, 0.02, body),
    })
    .with_angular(Angular::knurl(30, 0.04, 0.74).r_out(0.88))
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.14, 0.66, 0.09, 0.014, accent_paint()));
}

/// Tall narrow fluted crown on a thin flared skirt, glossy black; the bright index runs
/// down the skirt and is repeated on the crown.
inline KnobModel skt() {
    auto black = Material::black_polished();
    return KnobModel::make({
        seg(Dome, 0.0, 0.34, 1.0, 0.985, black),
        seg(Dome, 0.34, 0.43, 0.985, 0.92, black),
        seg(Line, 0.43, 0.49, 0.92, 0.30, black),
    })
    .with_under({
        seg(Flat, 0.0, 0.50, 0.34, 0.34, black),
        seg(Fillet, 0.50, 0.90, 0.34, 0.11, black),
        seg(Line, 0.90, 0.95, 0.11, 0.08, black),
        seg(Dome, 0.95, 0.985, 0.08, 0.02, black),
    })
    .with_angular(Angular::flutes(14, 0.07, 0.40).r_out(0.49))
    .with_edge(0.016)
    .with_pointer(Pointer::line(0.55, 0.94, 0.07, accent_paint()))
    .with_mark(Pointer::line(0.08, 0.36, 0.07, accent_paint()));
}

/// Large knob moulded in the item colour with broad ribs that run up onto the crown.
inline KnobModel rib() {
    auto body = Material::plastic(0.5, 0.5, 0.5, 0.5).with_accent_shade(1.0, 0.8).with_coat(0.35);
    return KnobModel::make({
        seg(Dome, 0.0, 0.50, 0.80, 0.74, body),
        seg(Dome, 0.50, 0.84, 0.74, 0.50, body),
        seg(Line, 0.84, 0.94, 0.50, 0.06, body),
        seg(Dome, 0.94, 0.97, 0.06, 0.02, body),
    })
    .with_angular(Angular::flutes(26, 0.045, 0.50))
    .with_edge(0.02)
    .with_pointer(Pointer::line(0.04, 0.58, 0.11, white()));
}

/// Glossy dark cap on a flared chrome skirt, white index.
inline KnobModel chr() {
    auto cr = Material::metal(0.95, 0.95, 0.96, 0.10);
    auto cap = Material::black_gloss();
    return KnobModel::make({seg(Dome, 0.0, 0.48, 0.95, 0.86, cap), seg(Dome, 0.48, 0.58, 0.86, 0.72, cap), seg(Line, 0.58, 0.61, 0.72, 0.36, cap)})
        .with_under({seg(Flat, 0.0, 0.60, 0.40, 0.40, cr), seg(Fillet, 0.60, 0.94, 0.40, 0.08, cr), seg(Dome, 0.94, 0.99, 0.08, 0.02, cr)})
        .with_edge(0.02)
        .with_pointer(Pointer::line(0.14, 0.56, 0.06, white()));
}

/// Round keycap: a raised satin rim around a deep, smoothly dished top that shades from
/// dark on the lit side to light on the far side, with a bold legend line in the accent
/// colour.
inline KnobModel key() {
    auto rim = Material::plastic(0.20, 0.20, 0.21, 0.42).with_sheen(0.3);
    auto dish = Material::plastic(0.15, 0.15, 0.16, 0.30);
    return KnobModel::make({
        seg(Dome, 0.0, 0.66, 0.36, 0.56, dish),
        seg(Line, 0.66, 0.68, 0.56, 0.585, rim),
        seg(Flat, 0.68, 0.78, 0.585, 0.585, rim),
        seg(Dome, 0.78, 0.85, 0.585, 0.52, rim),
        seg(Line, 0.85, 0.94, 0.52, 0.06, rim),
        seg(Dome, 0.94, 0.97, 0.06, 0.02, rim),
    })
    .with_edge(0.014)
    .with_pointer(Pointer::line(0.10, 0.60, 0.10, accent_paint()))
    .with_mark(Pointer::line(0.69, 0.77, 0.10, accent_paint()));
}

/// Polished brass: domed turned face inside a lathe groove, knurled wall, black index.
inline KnobModel brz() {
    auto face = Material::brass().with_roughness(0.14).with_aniso(0.3).with_rings(0.4);
    auto side = Material::brass();
    return KnobModel::make({
        seg(Dome, 0.0, 0.64, 0.78, 0.72, face),
        seg(Line, 0.64, 0.68, 0.72, 0.70, side),
        seg(Flat, 0.68, 0.72, 0.66, 0.66, side),
        seg(Dome, 0.72, 0.80, 0.70, 0.64, side),
        seg(Line, 0.80, 0.92, 0.64, 0.10, side),
        seg(Dome, 0.92, 0.95, 0.10, 0.02, side),
    })
    .with_angular(Angular::knurl(36, 0.04, 0.76))
    .with_edge(0.012)
    .with_pointer(Pointer::groove(0.15, 0.62, 0.06, 0.012, Material::ink()));
}

/// Black chicken-head pointer: slim tall beak with a rounded tail over a round skirt.
inline KnobModel chk() {
    auto black = Material::black_gloss().with_grain(0.03f);
    auto beak = Part::make(
        {seg(Flat, 0.0, 0.94, 0.94, 0.94, black), seg(Line, 0.94, 0.985, 0.94, 0.74, black), seg(Line, 0.985, 1.0, 0.74, 0.12, black)},
        Plan::bar(1.0, 0.62, 0.07, 0.26, 0.14)
    );
    return KnobModel::make({seg(Flat, 0.0, 0.58, 0.26, 0.26, black), seg(Dome, 0.58, 0.68, 0.26, 0.14, black), seg(Line, 0.68, 0.70, 0.14, 0.02, black)})
        .with_part(beak)
        .with_edge(0.016)
        .with_pointer(Pointer::line(0.20, 0.97, 0.035, white()));
}

/// Cream bakelite: deeply fluted grip with a domed crown on a rounded skirt, dark index.
inline KnobModel ivr() {
    auto cream = Material::cream();
    return KnobModel::make({seg(Dome, 0.0, 0.50, 0.92, 0.88, cream), seg(Dome, 0.50, 0.70, 0.88, 0.74, cream), seg(Line, 0.70, 0.76, 0.74, 0.14, cream)})
        .with_under({seg(Flat, 0.0, 0.78, 0.18, 0.18, cream), seg(Dome, 0.78, 0.94, 0.18, 0.03, cream)})
        .with_angular(Angular::flutes(16, 0.075, 0.50))
        .with_edge(0.025)
        .with_pointer(Pointer::line(0.14, 0.64, 0.055, Material::plastic(0.16, 0.10, 0.06, 0.6)));
}

/// Pale maple cylinder: slightly dished flat top with a crisp chamfer, straight wall and
/// a dark inlaid index.
inline KnobModel mpl() {
    auto maple = Material::maple().with_wood(0.9).with_coat(0.15).with_roughness(0.7);
    return KnobModel::make({
        seg(Dome, 0.0, 0.70, 0.72, 0.74, maple),
        seg(Line, 0.70, 0.77, 0.74, 0.69, maple),
        seg(Line, 0.77, 0.84, 0.69, 0.10, maple),
        seg(Flat, 0.84, 0.87, 0.08, 0.08, maple),
        seg(Dome, 0.87, 0.89, 0.08, 0.02, maple),
    })
    .with_edge(0.012)
    .with_pointer(Pointer::line(0.18, 0.76, 0.085, Material::plastic(0.13, 0.07, 0.04, 0.6)));
}

/// Stacked pair: knurled silver outer ring with a black inner knob on top.
inline KnobModel dbl() {
    return KnobModel::two_tier(0.34, 0.52, 0.92, Material::satin_aluminium(), Material::black_gloss())
        .with_angular(Angular::knurl(40, 0.03, 0.86))
        .with_edge(0.012)
        .with_pointer(Pointer::line(0.12, 0.50, 0.06, white()))
        .with_mark(Pointer::line(0.62, 0.90, 0.05, Material::ink()));
}

/// Small trimmer: a short black body with a white-filled slot from the centre to the edge on a thin
/// knurled flange, and a white dot on the flange that marks the direction.
inline KnobModel mini() {
    auto black = satin_black();
    return KnobModel::make({
        seg(Flat, 0.0, 0.46, 0.56, 0.56, black),
        seg(Line, 0.46, 0.52, 0.56, 0.50, black),
        seg(Line, 0.52, 0.55, 0.50, 0.16, black),
        seg(Flat, 0.55, 0.72, 0.14, 0.14, black),
        seg(Line, 0.72, 0.76, 0.14, 0.10, black),
        seg(Line, 0.76, 0.79, 0.10, 0.02, black),
    })
    .with_angular(Angular::knurl(32, 0.03, 0.74))
    .with_edge(0.014)
    .with_pointer(Pointer::groove(-0.10, 0.50, 0.12, 0.035, white()))
    .with_mark(Pointer::dot(0.64, 0.13, 0.0, white()));
}

/// Tall slim black knob with a straight knurl and a white index over the top edge.
inline KnobModel tl() {
    auto black = Material::plastic(0.08, 0.08, 0.085, 0.4).with_coat(0.2);
    return KnobModel::make({
        seg(Dome, 0.0, 0.52, 1.45, 1.43, black),
        seg(Dome, 0.52, 0.60, 1.43, 1.36, black),
        seg(Line, 0.60, 0.70, 1.36, 0.10, black),
        seg(Dome, 0.70, 0.73, 0.10, 0.02, black),
    })
    .with_angular(Angular::knurl(32, 0.03, 0.58).r_out(0.70))
    .with_edge(0.012)
    .with_pointer(Pointer::line(0.15, 0.69, 0.06, white()));
}

/// Translucent crown in the item colour drawn out into a pointer nose, white indicator.
inline KnobModel mic() {
    auto body = Material::translucent(0.5, 0.5, 0.5).with_accent(1.0);
    auto nose = Part::make(
        {seg(Flat, 0.0, 0.85, 0.56, 0.56, body), seg(Dome, 0.85, 1.0, 0.56, 0.10, body)},
        Plan::bar(0.98, 0.0, 0.10, 0.40, 0.08)
    );
    return KnobModel::make({seg(Dome, 0.0, 0.50, 0.80, 0.70, body), seg(Dome, 0.50, 0.62, 0.70, 0.50, body), seg(Line, 0.62, 0.66, 0.50, 0.03, body)})
        .with_part(nose)
        .with_edge(0.025)
        .with_pointer(Pointer::line(0.25, 0.94, 0.055, white()));
}

/// Lacquered ebony with a turned brass cap in the centre and a pale inlaid index.
inline KnobModel ebn() {
    auto ebony = Material::wood(0.10, 0.075, 0.06).with_wood(0.5);
    auto cap = Material::brass().with_roughness(0.14).with_rings(0.5);
    return KnobModel::make({
        seg(Dome, 0.0, 0.28, 0.93, 0.905, cap),
        seg(Line, 0.28, 0.31, 0.905, 0.86, cap),
        seg(Dome, 0.31, 0.84, 0.86, 0.58, ebony),
        seg(Line, 0.84, 0.90, 0.58, 0.06, ebony),
        seg(Dome, 0.90, 0.92, 0.06, 0.02, ebony),
    })
    .with_edge(0.016)
    .with_pointer(Pointer::line(0.36, 0.88, 0.075, Material::plastic(0.93, 0.85, 0.62, 0.5).with_coat(0.6)));
}

/// Walnut top let into a raised chrome collar, pale index on the wood.
inline KnobModel wnt() {
    auto walnut = Material::walnut().with_coat(0.4);
    auto cr = Material::metal(0.95, 0.95, 0.96, 0.14);
    return KnobModel::make({
        seg(Dome, 0.0, 0.62, 0.78, 0.76, walnut),
        seg(Flat, 0.62, 0.64, 0.74, 0.74, walnut),
        seg(Line, 0.64, 0.68, 0.76, 0.80, cr),
        seg(Flat, 0.68, 0.80, 0.80, 0.80, cr),
        seg(Dome, 0.80, 0.92, 0.80, 0.60, cr),
        seg(Line, 0.92, 0.96, 0.60, 0.05, cr),
        seg(Dome, 0.96, 0.98, 0.05, 0.02, cr),
    })
    .with_edge(0.014)
    .with_pointer(Pointer::line(0.18, 0.60, 0.06, Material::plastic(0.92, 0.86, 0.70, 0.5).with_coat(0.6)));
}

/// Small black rack knob: satin top over a fluted skirt, bright index down the side.
inline KnobModel rck() {
    auto top = Material::plastic(0.11, 0.11, 0.115, 0.62);
    auto skirt = Material::anodised(0.14, 0.14, 0.15);
    return KnobModel::make({
        seg(Dome, 0.0, 0.50, 0.84, 0.83, top),
        seg(Line, 0.50, 0.54, 0.83, 0.79, top),
        seg(Line, 0.54, 0.60, 0.79, 0.60, skirt),
        seg(Line, 0.60, 0.80, 0.60, 0.08, skirt),
        seg(Dome, 0.80, 0.83, 0.08, 0.02, skirt),
    })
    .with_angular(Angular::flutes(18, 0.035, 0.56))
    .with_edge(0.014)
    .with_pointer(Pointer::line(0.12, 0.78, 0.06, accent_paint()));
}

/// Satin black for moulded grips: dark grey, not black, with a soft sheen.
inline Material satin_black() {
    return Material::plastic(0.14, 0.14, 0.145, 0.42).with_sheen(0.25).with_grain(0.02);
}

} // namespace styles_detail

/// Codes accepted by preset().
inline constexpr const char* PRESETS[15] = {"satin", "bakelite", "spun", "gloss", "strip", "collet", "chicken", "wing", "ivory", "chrome", "phenolic", "wood", "brass", "dome", "dual"};
/// Codes of the styles that have a lit model, in catalogue order.
inline constexpr const char* STYLE_CODES[35] = {
    "Brit", "N", "A", "FS", "Mtl", "Wd", "Bk", "Pie", "Snk", "Slv", "Rd", "Ptr", "Dm", "Sq", "Fat", "Pag", "Lvr", "Tab", "Wh", "Knl", "Skt",
    "Rib", "Chr", "Key", "Brz", "Chk", "Ivr", "Mpl", "Dbl", "Mini", "Tl", "Mic", "Ebn", "Wnt", "Rck"};

inline bool isPreset(const std::string& code) { for (auto* p : PRESETS) if (code == p) return true; return false; }

/// Look up a built-in model by code, see PRESETS.
inline bool preset(const std::string& code, KnobModel& out) {
    namespace P = presets_detail;
    if (code == "satin") { out = P::satin(); return true; }
    if (code == "bakelite") { out = P::bakelite(); return true; }
    if (code == "spun") { out = P::spun(); return true; }
    if (code == "gloss") { out = P::gloss(); return true; }
    if (code == "strip") { out = P::strip(); return true; }
    if (code == "collet") { out = P::collet(); return true; }
    if (code == "chicken") { out = P::chicken(); return true; }
    if (code == "wing") { out = P::wing(); return true; }
    if (code == "ivory") { out = P::ivory(); return true; }
    if (code == "chrome") { out = P::chrome(); return true; }
    if (code == "phenolic") { out = P::phenolic(); return true; }
    if (code == "wood") { out = P::wood(); return true; }
    if (code == "brass") { out = P::brass(); return true; }
    if (code == "dome") { out = P::dome(); return true; }
    if (code == "dual") { out = P::dual(); return true; }
    return false;
}

/// Lit model of a catalogue style, by code or legacy alias. False for the display and neon styles,
/// which keep their vector drawing.
inline bool styleModel(const std::string& code, KnobModel& out) {
    namespace S = styles_detail;
    if (code == "Brit") { out = S::brit(); return true; }
    if (code == "console") { out = S::brit(); return true; }
    if (code == "N") { out = S::n(); return true; }
    if (code == "A") { out = S::a(); return true; }
    if (code == "FS") { out = S::fs(); return true; }
    if (code == "Mtl") { out = S::mtl(); return true; }
    if (code == "metal") { out = S::mtl(); return true; }
    if (code == "Wd") { out = S::wd(); return true; }
    if (code == "Bk") { out = S::bk(); return true; }
    if (code == "bakelite") { out = S::bk(); return true; }
    if (code == "Pie") { out = S::pie(); return true; }
    if (code == "Snk") { out = S::snk(); return true; }
    if (code == "Slv") { out = S::slv(); return true; }
    if (code == "ivory") { out = S::slv(); return true; }
    if (code == "Rd") { out = S::rd(); return true; }
    if (code == "Ptr") { out = S::ptr(); return true; }
    if (code == "Dm") { out = S::dm(); return true; }
    if (code == "Sq") { out = S::sq(); return true; }
    if (code == "Fat") { out = S::fat(); return true; }
    if (code == "Pag") { out = S::pag(); return true; }
    if (code == "Lvr") { out = S::lvr(); return true; }
    if (code == "Tab") { out = S::tab(); return true; }
    if (code == "Wh") { out = S::wh(); return true; }
    if (code == "Knl") { out = S::knl(); return true; }
    if (code == "Skt") { out = S::skt(); return true; }
    if (code == "Rib") { out = S::rib(); return true; }
    if (code == "Chr") { out = S::chr(); return true; }
    if (code == "Key") { out = S::key(); return true; }
    if (code == "Brz") { out = S::brz(); return true; }
    if (code == "Chk") { out = S::chk(); return true; }
    if (code == "Ivr") { out = S::ivr(); return true; }
    if (code == "Mpl") { out = S::mpl(); return true; }
    if (code == "Dbl") { out = S::dbl(); return true; }
    if (code == "Mini") { out = S::mini(); return true; }
    if (code == "Tl") { out = S::tl(); return true; }
    if (code == "Mic") { out = S::mic(); return true; }
    if (code == "Ebn") { out = S::ebn(); return true; }
    if (code == "Wnt") { out = S::wnt(); return true; }
    if (code == "Rck") { out = S::rck(); return true; }
    return false;
}

} // namespace goodlookinui::knob3d
