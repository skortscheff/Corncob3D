// Keyboard.cpp - Port of new9 (3MACS.INC), setkbit/getkbit/readkey (3.ASM), scalekey (3.ASM)

#include <map>
#include <set>
#include <vector>
#include "Keyboard.hpp"
#include "DGroup.hpp"

namespace corncob {

namespace d = corncob::dgroup;

const uint32_t DosKeyboard::SCNCODE = d::m3::scncode;
const uint32_t DosKeyboard::SCNFLG = d::m3::scnflg;
const uint32_t DosKeyboard::KEYTABLE = d::m3::keytable;
const uint32_t DosKeyboard::KBDTBL = d::m3::kbdtbl;

namespace {
std::map<int, uint8_t> g_override;   // a key the player chose -> the DOS code it gives
std::set<uint8_t> g_rebound;         // the DOS codes that have been given another key
}

void DosKeyboard::setBindings(const std::vector<std::pair<int, uint8_t>>& bindings) {
    g_override.clear();
    g_rebound.clear();
    for (const auto& b : bindings) {
        g_override[b.first] = b.second;
        g_rebound.insert(b.second);
    }
}

uint8_t DosKeyboard::dosScanCode(SDL_Scancode sc) {
    const auto it = g_override.find(static_cast<int>(sc));
    if (it != g_override.end()) return it->second;
    const uint8_t code = defaultScanCode(sc);
    return g_rebound.count(code) ? 0 : code;   // the old key of a rebound action does nothing any more
}

uint8_t DosKeyboard::defaultScanCode(SDL_Scancode sc) {
    switch (sc) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_1: return 0x02;
    case SDL_SCANCODE_2: return 0x03;
    case SDL_SCANCODE_3: return 0x04;
    case SDL_SCANCODE_4: return 0x05;
    case SDL_SCANCODE_5: return 0x06;
    case SDL_SCANCODE_6: return 0x07;
    case SDL_SCANCODE_7: return 0x08;
    case SDL_SCANCODE_8: return 0x09;
    case SDL_SCANCODE_9: return 0x0A;
    case SDL_SCANCODE_0: return 0x0B;
    case SDL_SCANCODE_MINUS: return 0x0C;
    case SDL_SCANCODE_EQUALS: return 0x0D;
    case SDL_SCANCODE_BACKSPACE: return 0x0E;
    case SDL_SCANCODE_TAB: return 0x0F;
    case SDL_SCANCODE_Q: return 0x10;
    case SDL_SCANCODE_W: return 0x11;
    case SDL_SCANCODE_E: return 0x12;
    case SDL_SCANCODE_R: return 0x13;
    case SDL_SCANCODE_T: return 0x14;
    case SDL_SCANCODE_Y: return 0x15;
    case SDL_SCANCODE_U: return 0x16;
    case SDL_SCANCODE_I: return 0x17;
    case SDL_SCANCODE_O: return 0x18;
    case SDL_SCANCODE_P: return 0x19;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1A;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_RETURN: return 0x1C;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL: return 0x1D;
    case SDL_SCANCODE_A: return 0x1E;
    case SDL_SCANCODE_S: return 0x1F;
    case SDL_SCANCODE_D: return 0x20;
    case SDL_SCANCODE_F: return 0x21;
    case SDL_SCANCODE_G: return 0x22;
    case SDL_SCANCODE_H: return 0x23;
    case SDL_SCANCODE_J: return 0x24;
    case SDL_SCANCODE_K: return 0x25;
    case SDL_SCANCODE_L: return 0x26;
    case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_LSHIFT: return 0x2A;
    case SDL_SCANCODE_BACKSLASH: return 0x2B;
    case SDL_SCANCODE_Z: return 0x2C;
    case SDL_SCANCODE_X: return 0x2D;
    case SDL_SCANCODE_C: return 0x2E;
    case SDL_SCANCODE_V: return 0x2F;
    case SDL_SCANCODE_B: return 0x30;
    case SDL_SCANCODE_N: return 0x31;
    case SDL_SCANCODE_M: return 0x32;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT: return 0x38;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_CAPSLOCK: return 0x3A;
    case SDL_SCANCODE_F1: return 0x3B;
    case SDL_SCANCODE_F2: return 0x3C;
    case SDL_SCANCODE_F3: return 0x3D;
    case SDL_SCANCODE_F4: return 0x3E;
    case SDL_SCANCODE_F5: return 0x3F;
    case SDL_SCANCODE_F6: return 0x40;
    case SDL_SCANCODE_F7: return 0x41;
    case SDL_SCANCODE_F8: return 0x42;
    case SDL_SCANCODE_F9: return 0x43;
    case SDL_SCANCODE_F10: return 0x44;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x45;
    case SDL_SCANCODE_SCROLLLOCK: return 0x46;
    // The arrow keys and the numeric keypad share scan codes on a PC/XT keyboard
    case SDL_SCANCODE_KP_7:
    case SDL_SCANCODE_HOME: return 0x47;
    case SDL_SCANCODE_KP_8:
    case SDL_SCANCODE_UP: return 0x48;
    case SDL_SCANCODE_KP_9:
    case SDL_SCANCODE_PAGEUP: return 0x49;
    case SDL_SCANCODE_KP_MINUS: return 0x4A;
    case SDL_SCANCODE_KP_4:
    case SDL_SCANCODE_LEFT: return 0x4B;
    case SDL_SCANCODE_KP_5: return 0x4C;
    case SDL_SCANCODE_KP_6:
    case SDL_SCANCODE_RIGHT: return 0x4D;
    case SDL_SCANCODE_KP_PLUS: return 0x4E;
    case SDL_SCANCODE_KP_1:
    case SDL_SCANCODE_END: return 0x4F;
    case SDL_SCANCODE_KP_2:
    case SDL_SCANCODE_DOWN: return 0x50;
    case SDL_SCANCODE_KP_3:
    case SDL_SCANCODE_PAGEDOWN: return 0x51;
    case SDL_SCANCODE_KP_0:
    case SDL_SCANCODE_INSERT: return 0x52;
    case SDL_SCANCODE_KP_PERIOD:
    case SDL_SCANCODE_DELETE: return 0x53;
    case SDL_SCANCODE_F11: return 0x57;
    case SDL_SCANCODE_F12: return 0x58;
    default: return 0;
    }
}

// new9
void DosKeyboard::key(uint8_t dosCode, bool down, uint16_t ticksAgo) {
    if (dosCode == 0) return;
    const uint8_t code = static_cast<uint8_t>(dosCode & 0x7F);
    const uint16_t now = static_cast<uint16_t>(m_clock.oticks - ticksAgo);
    m_g.setB(SCNFLG, 0xFF);
    m_g.setB(SCNCODE, static_cast<uint8_t>(down ? code : (code | 0x80)));
    const uint32_t e = KEYTABLE + 8u * code;
    if (!down) {
        if (m_g.w(e) != 0) {   // was the key down?
            m_g.setW(e + 4, static_cast<int16_t>(m_g.uw(e + 4) + static_cast<uint16_t>(now - m_g.uw(e + 2))));
            m_g.setW(e, 0);
        }
    } else if (m_g.w(e) == 0) {
        m_g.setW(e + 2, static_cast<int16_t>(now));
        m_g.setW(e, -1);
    }
    // setkbit
    const uint32_t byteAddr = KBDTBL + (code >> 3);
    const uint8_t bit = static_cast<uint8_t>(1u << (code & 7));
    m_g.setB(byteAddr, static_cast<uint8_t>(down ? (m_g.b(byteAddr) | bit) : (m_g.b(byteAddr) & ~bit)));
}

// getkbit
uint8_t DosKeyboard::getkbit(uint8_t code) const {
    return (m_g.b(KBDTBL + (code >> 3)) >> (code & 7)) & 1;
}

// readkey
DosKeyboard::Read DosKeyboard::readkey(uint8_t code) {
    const uint32_t si = KEYTABLE + 8u * code;
    const uint16_t now = m_clock.oticks;
    Read r;
    if (m_g.w(si) != 0) {   // down: count up to now, and restart the timer
        r.down = static_cast<uint16_t>(now - m_g.uw(si + 2) + m_g.uw(si + 4));
        m_g.setW(si + 4, 0);
        m_g.setW(si + 2, static_cast<int16_t>(now));
    } else {                // up: what new9 left since the last read
        r.down = m_g.uw(si + 4);
        m_g.setW(si + 4, 0);
    }
    r.elapsed = static_cast<int16_t>(now - m_g.uw(si + 6));
    m_g.setW(si + 6, static_cast<int16_t>(now));
    return r;
}

// scalekey
uint16_t DosKeyboard::scalekey(uint8_t code, uint16_t cx) {
    const Read r = readkey(code);
    if (r.elapsed <= 0) return 0;   // measurement time must be positive
    const uint32_t prod = static_cast<uint32_t>(r.down) * cx;
    if ((prod >> 16) >= static_cast<uint16_t>(r.elapsed)) return cx;   // would overflow
    const uint16_t q = static_cast<uint16_t>(prod / static_cast<uint16_t>(r.elapsed));
    return q < cx ? q : cx;
}

void DosKeyboard::clear() {
    for (int i = 0; i < 16; i++) m_g.setB(KBDTBL + i, 0);
}

} // namespace corncob
