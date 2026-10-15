// GameState.hpp - The original data segment (DGROUP), byte for byte
//
// The ASM addresses its globals and object records by offset ([si+12],
// offset tempindex-76 ...), so ported procs work on this 64K image through
// the generated label offsets (dgroup::m3::ox, dgroup::defobjs::plnplyobj ...)
// instead of on re-typed C++ structs. Little-endian, unaligned access, as on
// the 8086.

#pragma once
#include "DGroup.hpp"
#include <array>
#include <cstdint>
#include <cstring>

namespace corncob {

class GameState {
public:
    static constexpr size_t SIZE = 0x10000;

    GameState() { reset(); }

    // Restore the initialized data exactly as the program loader would
    void reset() {
        m_mem.fill(0);
        std::memcpy(m_mem.data(), dgroup::kImage, dgroup::kImageSize);
    }

    uint8_t b(uint32_t off) const { return m_mem[off & 0xFFFF]; }
    void setB(uint32_t off, uint8_t v) { m_mem[off & 0xFFFF] = v; }

    int16_t w(uint32_t off) const {
        return static_cast<int16_t>(m_mem[off & 0xFFFF] | (m_mem[(off + 1) & 0xFFFF] << 8));
    }
    uint16_t uw(uint32_t off) const { return static_cast<uint16_t>(w(off)); }
    void setW(uint32_t off, int16_t v) {
        m_mem[off & 0xFFFF] = static_cast<uint8_t>(v);
        m_mem[(off + 1) & 0xFFFF] = static_cast<uint8_t>(static_cast<uint16_t>(v) >> 8);
    }

    // 32-bit "dw lo,hi" pairs (e.g. ox, dticks)
    int32_t dw(uint32_t off) const {
        return static_cast<int32_t>(static_cast<uint32_t>(uw(off)) | (static_cast<uint32_t>(uw(off + 2)) << 16));
    }
    void setDw(uint32_t off, int32_t v) {
        setW(off, static_cast<int16_t>(v));
        setW(off + 2, static_cast<int16_t>(static_cast<uint32_t>(v) >> 16));
    }

    uint8_t* raw() { return m_mem.data(); }
    const uint8_t* raw() const { return m_mem.data(); }

private:
    std::array<uint8_t, SIZE> m_mem{};
};

} // namespace corncob
