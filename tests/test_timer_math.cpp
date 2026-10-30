#include "app/timer_math.hpp"

#include <gtest/gtest.h>

#include <cstdint>

using app::timer_math::calculate;
using app::timer_math::Result;

// calculate(timer clock, wanted rate): the first argument is the clock that
// feeds the timer (72 MHz for TIM6/TIM7), the second the wanted sample rate

namespace {

constexpr uint32_t CLK = 72'000'000;   // TIM6/TIM7 clock: APB1 36 MHz x2

struct Case {
    uint32_t clock_hz;
    uint32_t target_hz;
    uint16_t psc;
    uint16_t arr;
    uint32_t sample_rate;   // expected actual rate
};

// printed by gtest when a case fails
std::ostream& operator<<(std::ostream& os, const Case& c) {
    return os << c.clock_hz << " Hz / " << c.target_hz << " Hz";
}

uint64_t divider(const Result& cfg) {
    return (uint64_t{cfg.psc} + 1) * (uint64_t{cfg.arr} + 1);
}

} // namespace

// ---- table of known values -------------------------------------------------

class TimerMathTable : public ::testing::TestWithParam<Case> {};

TEST_P(TimerMathTable, MatchesHandCalculation) {
    const Case& c = GetParam();
    const auto cfg = calculate(c.clock_hz, c.target_hz);
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->psc, c.psc);
    EXPECT_EQ(cfg->arr, c.arr);
    EXPECT_EQ(cfg->sample_rate, c.sample_rate);
}

INSTANTIATE_TEST_SUITE_P(
    TimerMath, TimerMathTable,
    ::testing::Values(
        //   clock      target        psc   arr     actual rate
        Case{CLK, 100'000, 0, 719, 100'000},          // default rate, exact
        Case{CLK, 1'000'000, 0, 71, 1'000'000},       // RATE_MAX, exact
        Case{CLK, 1'000, 1, 35'999, 1'000},           // 72000 ticks: needs the prescaler
        Case{CLK, 700'000, 0, 101, 705'882},          // not a divisor: actual != target
        Case{CLK, 1, 1'098, 65'513, 1},               // slowest possible
        Case{CLK, CLK, 0, 0, CLK},                    // one tick per sample
        Case{65'536'000, 1'000, 0, 65'535, 1'000},    // 65536 ticks: ARR at its maximum
        Case{65'537'000, 1'000, 1, 32'767, 1'000}));  // 65537 ticks: first case with PSC = 1

// ---- invalid input -----------------------------------------------------------

TEST(TimerMath, ZeroRateIsRejected) {
    EXPECT_FALSE(calculate(CLK, 0).has_value());
}

TEST(TimerMath, RateAboveTimerClockIsRejected) {
    EXPECT_FALSE(calculate(CLK, CLK + 1).has_value());
}

// ---- properties over the whole RATE range --------------------------------------

TEST(TimerMath, WholeRateRangeIsValid) {
    // every rate the board accepts (RATE_MIN..RATE_MAX), in steps
    for (uint32_t hz = 1'000; hz <= 1'000'000; hz += 997) {
        SCOPED_TRACE(hz);
        const auto cfg = calculate(CLK, hz);
        ASSERT_TRUE(cfg.has_value());

        // PSC and ARR are 16-bit registers: Result stores uint32_t, so check
        EXPECT_LE(cfg->psc, 0xFFFFu);
        EXPECT_LE(cfg->arr, 0xFFFFu);

        // the reported rate really is what the registers produce
        EXPECT_EQ(cfg->sample_rate, CLK / divider(*cfg));

        // the divider never exceeds the ideal one, so the rate is never lower
        EXPECT_LE(divider(*cfg), CLK / hz);
        EXPECT_GE(cfg->sample_rate, hz);

        // and it is close: worst case near 1 MHz is one tick out of 72
        EXPECT_LE(cfg->sample_rate - hz, hz / 50) << "more than 2 % off";
    }
}

TEST(TimerMath, PrescalerOnlyWhenNeeded) {
    // up to 65536 ticks ARR alone is enough, so PSC must stay 0 (best resolution)
    for (uint32_t hz = 1'099; hz <= 1'000'000; hz += 1'009) {
        SCOPED_TRACE(hz);
        const auto cfg = calculate(CLK, hz);
        ASSERT_TRUE(cfg.has_value());
        if (CLK / hz <= 65'536) {
            EXPECT_EQ(cfg->psc, 0);
        } else {
            EXPECT_GT(cfg->psc, 0);
        }
    }
}
