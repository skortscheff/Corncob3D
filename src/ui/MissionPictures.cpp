// MissionPictures.cpp - Which pictures a mission's end shows (PICT.ASM pict), as a pure function

#include "MissionEnd.hpp"

namespace corncob {

// PICT.ASM pict, label by label (the status bits are listed in the header)
std::vector<int> missionPictures(uint16_t ax, uint16_t nfdest, uint16_t deathcode) {
    std::vector<int> out;
    if (ax & 0x4000) return out;   // ctrl-c: nothing
    if (ax & 0x0400) {             // lost in space
        out.push_back(14);
        return out;
    }
    enum { PICT1, PICT3, PICT5, PICT6, PICT7, PICT8, SHOWCORPSE, DONE } next;

    if (!(ax & 0x0800) && (ax & 0x08)) next = PICT1;            // captured: no crash-land screen
    else if (!(ax & 0x0800) && (ax & 0x02)) {                   // killed (pict2)
        if (ax & 0x20) {
            next = SHOWCORPSE;                                   // ejected: the guy hit the ground too hard
        } else {
            out.push_back(4);
            return out;                                          // a flyby
        }
    } else if (ax & 0x40) {                                      // crash-landed, not home
        if ((ax & 0x100) || (ax & 0x10)) out.push_back(2);       // rescued, or got home: show the crash-landing
        next = PICT1;
    } else {                                                     // pict0
        if (ax & 0x200) out.push_back(3);                        // crash-landed at home
        next = PICT1;
    }

    for (;;) {
        switch (next) {
        case SHOWCORPSE:
            out.push_back(7);
            return out;
        case PICT1:
            if (ax & 0x08) {   // captured
                out.push_back(8);
                return out;
            }
            next = PICT3;
            break;
        case PICT3:
            if (ax & 0x100) {   // rescued
                out.push_back(1);
                next = PICT8;
            } else if (ax & 0x0800) {   // killed by an object: the corpse, by what killed it
                int n = 7;
                if (deathcode != 0) n = deathcode == 22 ? 10 : deathcode == 24 ? 11 : deathcode == 32 ? 12 : 13;
                out.push_back(n);
                return out;
            } else {
                next = PICT5;
            }
            break;
        case PICT5:   // home and landed home, and didn't crash-land: a happy landing
            if ((ax & (16 + 4)) != (16 + 4) || (ax & (0x40 + 0x200))) {
                next = PICT6;
                break;
            }
            if (!(ax & 1)) out.push_back(0);
            next = PICT8;
            break;
        case PICT6:   // not at home: an unknown fate
            if (ax & 0x10) {
                next = PICT7;
                break;
            }
            if (!(ax & 1)) out.push_back(9);
            next = PICT8;
            break;
        case PICT7:   // still in our plane at home: a flyby
            if (!(ax & (0x20 + 0x200)) && !(ax & 1)) out.push_back(5);
            next = PICT8;
            break;
        case PICT8:   // friendlies lost: the brig
            if (nfdest != 0) out.push_back(6);
            next = DONE;
            break;
        case DONE:
        default:
            return out;
        }
    }
}

} // namespace corncob
