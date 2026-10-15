// test_asmmath.cpp - Bit-exactness tests for src/core/AsmMath.hpp
// Expected values are worked out by hand from the 8086 instruction semantics.

#include "core/AsmMath.hpp"
#include <cstdio>

using namespace corncob::asmx;

static int failures = 0;
#define CHECK_EQ(actual, expected)                                                        \
    do {                                                                                  \
        long long a_ = (actual), e_ = (expected);                                         \
        if (a_ != e_) {                                                                   \
            std::printf("%s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__, #actual, \
                        a_, e_);                                                          \
            failures++;                                                                   \
        }                                                                                 \
    } while (0)

int main() {
    // hmul: high word of (a*b) << 1, i.e. a 1.15 fractional multiply
    CHECK_EQ(hmul(16384, 16384), 8192);     // 0.5 * 0.5 = 0.25
    CHECK_EQ(hmul(32767, 32767), 32766);    // ~1 * ~1
    CHECK_EQ(hmul(-16384, 16384), -8192);
    CHECK_EQ(hmul(-32768, -32768), -32768); // 0x40000000 << 1 = 0x80000000: wraps negative
    CHECK_EQ(hmul(1, 1), 0);
    CHECK_EQ(hmul(-1, 1), -1);              // arithmetic high word of -2

    // madd: half-scale add, clamp, rol
    CHECK_EQ(madd(1000, 2000), 3000);
    CHECK_EQ(madd(20000, 20000), static_cast<int16_t>(0x7FFE)); // clamp 16383, rol -> 32766
    CHECK_EQ(madd(-1000, -2000), -3000 + 1);  // rol of a negative sets bit 0
    CHECK_EQ(madd(-32768, -1), -32765);  // -16385: cmp doesn't wrap yet -> clamp -16383, rol
    CHECK_EQ(madd(-32768, -4), static_cast<int16_t>(0x7FFE)); // -16386: cmp wraps -> +16383 (quirk)
    CHECK_EQ(madd(-32768, 0), static_cast<int16_t>(0x8003));  // -16384 -> clamp -16383 (0xC001) rol -> 0x8003
    CHECK_EQ(madd(-30000, -10000), static_cast<int16_t>(0x7FFE)); // -20000: clamps to +16383 (quirk)

    // MAT.ASM madd: saturating 32-bit add
    CHECK_EQ(madd32(30000, 30000), 32767);
    CHECK_EQ(madd32(-30000, -30000), -32767);
    CHECK_EQ(madd32(100, -50), 50);

    // bndck: proper signed clamp
    CHECK_EQ(bndck(50, 35, 2), 35);
    CHECK_EQ(bndck(0, 35, 2), 2);
    CHECK_EQ(bndck(10, 35, 2), 10);

    // dbndck: sign-flag clamp (quirk for far-out values)
    CHECK_EQ(dbndck(100, 500, -300), 100);
    CHECK_EQ(dbndck(600, 500, -300), 500);
    CHECK_EQ(dbndck(-400, 500, -300), -300);
    CHECK_EQ(dbndck(-32000, 500, -300), -300); // no wrap: normal low clamp
    CHECK_EQ(dbndck(-32600, 500, -300), 500);  // -32600-500 wraps positive -> high (quirk)

    // wtadj/uwtadj with idiv truncation toward zero
    CHECK_EQ(wtadj(100, 3, 10), 30);
    CHECK_EQ(wtadj(-7, 1, 2), -3);
    CHECK_EQ(uwtadj(30, 3, 10), 100);
    CHECK_EQ(idiv32(100000, 2).overflow, 1);
    CHECK_EQ(idiv32(-7, 2).r, -1);

    if (failures == 0) std::printf("test_asmmath: all passed\n");
    return failures == 0 ? 0 : 1;
}
