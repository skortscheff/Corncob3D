// PlayerObjects.cpp - Player plane data transcribed from DEFOBJS.ASM (lines ~128-148, 922-939, 1592-2157)
// Constant names and expressions match the ASM so entries can be checked line by line.

#include "PlayerObjects.hpp"

namespace corncob {
namespace {

// --- F3DEQU.INC globals ---
constexpr int l = 60;
constexpr int lo2 = l / 2;
constexpr int l4 = l * 4;
constexpr int p = 4 * l / 2;
constexpr int pl = 4 * l;

// --- DEFOBJS.ASM plane constants ---
constexpr int lp = 180;
constexpr int lpo2 = lp * 2 / 3;
constexpr int lo3 = lo2 * 3 / 2;
constexpr int lpo6 = lp / 6;
constexpr int lp2 = lp * 2;
constexpr int lp4 = lp * 4;
constexpr int l4rear = lp * 2;
constexpr int ldip = lp * 2;
constexpr int l8 = lp * 8;
constexpr int l10 = lp * 10;
constexpr int lcanp = lpo2 * 4 / 3;
constexpr int lelv = l10 - lp - lo3 / 2;
constexpr int lwelv = lp * 3;
constexpr int ltail = lelv - lpo2;
constexpr int lwtail = lp * 5 / 2;
constexpr int clr8 = 0;
constexpr int clr6 = 12;
constexpr int clr9 = 14;

// Indirect color bytes (initial values from the data segment)
constexpr uint8_t wingclr = 1;

// --- propeller constants ---
constexpr int pwid = lpo6 / 2;
constexpr int pwids = lpo6 / 4;
constexpr int ppit = lpo6 / 4;
constexpr int plen = 3 * lpo2;
constexpr int ptip = plen + pwid;
constexpr int psml = lpo2;
constexpr int poff = lp4 + lpo6 / 2;

// ASM axes: x forward, y span, z up -> port axes: x right, y up, z forward.
// Assumes the ASM frame is right-handed with +y to the left.
P3 A(int x, int y, int z) { return {static_cast<float>(-y), static_cast<float>(z), static_cast<float>(x)}; }

Poly poly(uint8_t color, std::initializer_list<P3> pts) { return Poly{color, pts, true}; }
Poly wire(uint8_t color, std::initializer_list<P3> pts) { return Poly{color, pts, false}; }

// ---------------- plane polygons ----------------
const Poly plnply1 = poly(7, {A(lp4,-lpo6,-lpo2), A(lp4,lpo6,-lpo2), A(lp4,lpo2,-lpo6), A(lp4,lpo2,lpo6),
                              A(lp4,lpo6,lpo2), A(lp4,-lpo6,lpo2), A(lp4,-lpo2,lpo6), A(lp4,-lpo2,-lpo6)});
// NOTE: third vertex uses l4 (=240), not lp4, exactly as in the original source.
const Poly plnply2 = poly(7, {A(-l4rear,lpo2,lpo6), A(lp4,lpo2,lpo6), A(l4,lpo2,-lpo6), A(-l4rear,lpo2,-lpo6)});
const Poly plnply2top = poly(7, {A(-l4rear,lpo2,lpo6), A(lp4,lpo2,lpo6), A(lp4,lpo6,lpo2), A(-l4rear,lpo6,lpo2)});
const Poly plnply2bot = poly(7, {A(-l4rear,lpo2,-lpo6), A(lp4,lpo2,-lpo6), A(lp4,lpo6,-lpo2), A(-l4rear,lpo6,-lpo2)});
const Poly plnply2tri = poly(7, {A(-l4rear,lpo2,lpo6), A(-l4rear,lpo2,-lpo6), A(-l10,0,0)});
const Poly plnply2toptri = poly(7, {A(-l4rear,lpo2,lpo6), A(-l4rear,lpo6,lcanp), A(-l10,0,0)});
const Poly plnply2bottri = poly(7, {A(-l4rear,lpo2,-lpo6), A(-l4rear,lpo6,-lpo2), A(-l10,0,0)});
const Poly plnply3 = poly(7, {A(-l4rear,-lpo2,lpo6), A(lp4,-lpo2,lpo6), A(lp4,-lpo2,-lpo6), A(-l4rear,-lpo2,-lpo6)});
const Poly plnply3top = poly(7, {A(-l4rear,-lpo2,lpo6), A(lp4,-lpo2,lpo6), A(lp4,-lpo6,lpo2), A(-l4rear,-lpo6,lpo2)});
const Poly plnply3bot = poly(7, {A(-l4rear,-lpo2,-lpo6), A(lp4,-lpo2,-lpo6), A(lp4,-lpo6,-lpo2), A(-l4rear,-lpo6,-lpo2)});
const Poly plnply3tri = poly(7, {A(-l4rear,-lpo2,lpo6), A(-l4rear,-lpo2,-lpo6), A(-l10,0,0)});
const Poly plnply3toptri = poly(7, {A(-l4rear,-lpo2,lpo6), A(-l4rear,-lpo6,lcanp), A(-l10,0,0)});
const Poly plnply3bottri = poly(7, {A(-l4rear,-lpo2,-lpo6), A(-l4rear,-lpo6,-lpo2), A(-l10,0,0)});
const Poly plnply4 = poly(7, {A(-l4rear,-lpo6,lpo2), A(lp4,-lpo6,lpo2), A(lp4,lpo6,lpo2), A(-l4rear,lpo6,lpo2)});
const Poly plnply4tri = poly(7, {A(-l4rear,-lpo6,lcanp), A(-l4rear,lpo6,lcanp), A(-l10,0,0)});
const Poly plnply5 = poly(7, {A(-l4rear,-lpo6,-lpo2), A(lp4,-lpo6,-lpo2), A(lp4,lpo6,-lpo2), A(-l4rear,lpo6,-lpo2)});
const Poly plnply5tri = poly(7, {A(-l4rear,-lpo6,-lpo2), A(-l4rear,lpo6,-lpo2), A(-l10,0,0)});
const Poly plnplyw2 = poly(wingclr, {A(lp2,lpo2,0), A(-lp2,lpo2,0), A(-lp,ldip,-lpo2), A(lp2,ldip,-lpo2)});
const Poly plnplyw3 = poly(wingclr, {A(lp2,-lpo2,0), A(-lp2,-lpo2,0), A(-lp,-ldip,-lpo2), A(lp2,-ldip,-lpo2)});
const Poly plnplyw2tip = poly(wingclr, {A(lp2,ldip,-lpo2), A(-lp,ldip,-lpo2), A(-lp+lpo2,l8,0), A(lp2,l8,0)});
const Poly plnplyw3tip = poly(wingclr, {A(lp2,-ldip,-lpo2), A(-lp,-ldip,-lpo2), A(-lp+lpo2,-l8,0), A(lp2,-l8,0)});
const Poly plnplyelv2 = poly(wingclr, {A(-lelv,0,0), A(-lelv-lpo2,lwelv,0), A(-l10+lo3/2,lwelv,0), A(-l10+lo3/4,0,0)});
const Poly plnplyelv3 = poly(wingclr, {A(-lelv,0,0), A(-lelv-lpo2,-lwelv,0), A(-l10+lo3/2,-lwelv,0), A(-l10+lo3/4,0,0)});
const Poly plnplytail = poly(7, {A(-ltail,0,0), A(-ltail-lpo2/2,0,lwtail*5/6), A(-ltail-lpo2,0,lwtail),
                                 A(-l10+3*lpo2/2,0,lwtail), A(-l10+lpo2,0,lwtail*4/5), A(-l10+lpo2,0,0)});
// plnfr1 -> plnwr1: nose ring outline drawn in color 0
const Poly plnwr1 = wire(0, {A(lp4,-lpo6,-lpo2), A(lp4,lpo6,-lpo2), A(lp4,lpo2,-lpo6), A(lp4,lpo2,lpo6),
                             A(lp4,lpo6,lpo2), A(lp4,-lpo6,lpo2), A(lp4,-lpo2,lpo6), A(lp4,-lpo2,-lpo6),
                             A(lp4,-lpo6,-lpo2)});

// medium LOD fuselage
const Poly splnply1 = poly(7, {A(lp4,0,lpo2), A(-l4rear,0,lpo2), A(-l10,0,0), A(-l4rear,0,-lpo2)});
const Poly splnply2 = poly(7, {A(lp4,lpo2,0), A(-l4rear,lpo2,0), A(-l10,0,0), A(-l4rear,-lpo2,0)});

// sdsph: smoke column (first sphere is the fire, then rising puffs)
std::vector<Sphere> sdsph() {
    std::vector<Sphere> s;
    s.push_back({A(0, 0, 0), 2 * l, clr9});
    for (int i = 1; i <= 15; i++) s.push_back({A(0, 0, i * p), 2 * pl, static_cast<uint8_t>(i == 1 ? clr6 : clr8)});
    return s;
}

// ---------------- propeller ----------------
const Poly propply1s = poly(15, {A(poff,-pwids,0), A(poff,-pwid,psml), A(poff,pwid,psml), A(poff,pwids,0)});
const Poly propply2s = poly(15, {A(poff,-pwids,0), A(poff,-pwid,-psml), A(poff,pwid,-psml), A(poff,pwids,0)});
const Poly propply3s = poly(15, {A(poff,0,-pwids), A(poff,-psml,-pwid), A(poff,-psml,pwid), A(poff,0,pwids)});
const Poly propply4s = poly(15, {A(poff,0,-pwids), A(poff,psml,-pwid), A(poff,psml,pwid), A(poff,0,pwids)});
const Poly propply1tip = poly(14, {A(poff,0,ptip), A(poff,-pwid,plen), A(poff,pwid,plen)});
const Poly propply2tip = poly(14, {A(poff,0,-ptip), A(poff,-pwid,-plen), A(poff,pwid,-plen)});
const Poly propply3tip = poly(14, {A(poff,-ptip,0), A(poff,-plen,-pwid), A(poff,-plen,pwid)});
const Poly propply4tip = poly(14, {A(poff,ptip,0), A(poff,plen,-pwid), A(poff,plen,pwid)});
const Poly propply1 = poly(0, {A(poff-ppit,-pwid,psml), A(poff-ppit,-pwid,plen), A(ppit+poff,pwid,plen), A(ppit+poff,pwid,psml)});
const Poly propply2 = poly(0, {A(ppit+poff,-pwid,-psml), A(ppit+poff,-pwid,-plen), A(-ppit+poff,pwid,-plen), A(-ppit+poff,pwid,-psml)});
const Poly propply3 = poly(0, {A(-ppit+poff,-psml,-pwid), A(-ppit+poff,-plen,-pwid), A(ppit+poff,-plen,pwid), A(ppit+poff,-psml,pwid)});
const Poly propply4 = poly(0, {A(ppit+poff,psml,-pwid), A(ppit+poff,plen,-pwid), A(-ppit+poff,plen,pwid), A(-ppit+poff,psml,pwid)});
const Sphere propsph = {A(poff, 0, 0), 5 * pwid, 0};

const Poly spropply1 = poly(0, {A(poff-ppit,-pwid,0), A(poff-ppit,-pwid,ptip), A(ppit+poff,pwid,ptip), A(ppit+poff,pwid,0)});
const Poly spropply2 = poly(0, {A(ppit+poff,-pwid,0), A(ppit+poff,-pwid,-ptip), A(-ppit+poff,pwid,-ptip), A(-ppit+poff,pwid,0)});
const Poly spropply3 = poly(0, {A(-ppit+poff,0,-pwid), A(-ppit+poff,-ptip,-pwid), A(ppit+poff,-ptip,pwid), A(ppit+poff,0,pwid)});
const Poly spropply4 = poly(0, {A(ppit+poff,0,-pwid), A(ppit+poff,ptip,-pwid), A(-ppit+poff,ptip,pwid), A(-ppit+poff,0,pwid)});

const Poly sspropwr1 = wire(0, {A(poff,-ptip,0), A(poff,ptip,0)});
const Poly sspropwr2 = wire(0, {A(poff,0,-ptip), A(poff,0,ptip)});

Model buildPlane() {
    Model m;
    m.polys = {plnplyw2, plnplyw2tip, plnplyelv2, plnplyw3, plnplyw3tip, plnplyelv3, plnplytail,
               plnply1, plnply2, plnply2top, plnply2bot, plnply2tri, plnply2toptri, plnply2bottri,
               plnply3, plnply3top, plnply3bot, plnply3tri, plnply3toptri, plnply3bottri,
               plnply4, plnply4tri, plnply5, plnply5tri, plnwr1};
    return m;
}

} // namespace

const Model& playerPlaneModel() {
    static const Model m = buildPlane();
    return m;
}

const Model& playerPlaneSmokeModel() {
    static const Model m = [] { Model s = buildPlane(); s.spheres = sdsph(); return s; }();
    return m;
}

const Model& playerPlaneMedModel() {
    static const Model m = [] {
        Model s;
        s.polys = {plnplyw2, plnplyw2tip, splnply2, splnply1, plnplyw3, plnplyw3tip, plnplytail};
        return s;
    }();
    return m;
}

const Model& propellerModel() {
    static const Model m = [] {
        Model s;
        s.polys = {propply1tip, propply2tip, propply3tip, propply4tip, propply1s, propply2s, propply3s,
                   propply4s, propply1, propply2, propply3, propply4};
        s.spheres = {propsph};
        return s;
    }();
    return m;
}

const Model& propellerMedModel() {
    static const Model m = [] {
        Model s;
        s.polys = {spropply1, spropply2, spropply3, spropply4};
        s.spheres = {propsph};
        return s;
    }();
    return m;
}

const Model& propellerFarModel() {
    static const Model m = [] {
        Model s;
        s.polys = {sspropwr1, sspropwr2};
        s.spheres = {propsph};
        return s;
    }();
    return m;
}

} // namespace corncob
