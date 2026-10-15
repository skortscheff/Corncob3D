// PlayerObjects.hpp - Player plane models decoded from DEFOBJS.ASM
//
// Original object layout (DEFOBJS.ASM):
//   obj   dw 0,0,0          ; header
//         dw N              ; parts per view
//         8 x N (type,ptr)  ; one list per viewing octant, ordered back-to-front
// Part types: 8 = polygon (nverts, color, verts...), 6 = sphere list
// (count, then x,y,z,radius,color each), 2 = frame (count, wire ptrs; a wire is
// nsegs, color, nsegs+1 points), 0 = null. A color word is either a literal EGA
// index or the address of a color byte (e.g. wingclr) that can change at runtime.
//
// Since this port draws without painter ordering, each Model holds the union of
// the parts across all 8 view lists.
//
// Superseded: every object (this one included) is now generated from the ASM into
// build/generated/DGroup.* (tools/asmdata). This hand transcription stays only as a
// cross-check (tests/test_dgroup.cpp) until the Phase 1 engine draws from DGROUP.

#pragma once
#include "../render/Renderer.hpp"

namespace corncob {

// Units are the original object units: the plane is ~2500 long (nose at +poff).
const Model& playerPlaneModel();       // plnplyobj  - near
const Model& playerPlaneMedModel();    // splnplyobj - medium distance
const Model& playerPlaneSmokeModel();  // plnplyobjsm - near, with smoke column (sdsph)
const Model& propellerModel();         // propobj    - near
const Model& propellerMedModel();      // spropobj
const Model& propellerFarModel();      // sspropobj

// Distance (original units) beyond which the medium LOD is used. The real
// threshold lives in the object-type table; this is an approximation.
constexpr float PLANE_MED_LOD_DIST = 40000.0f;

} // namespace corncob
