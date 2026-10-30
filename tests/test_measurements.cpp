#include "app/measure.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

using app::measure::measure;
using app::measure::Measurements;

namespace {

constexpr uint32_t RATE = 100'000;   // default sample rate, Hz
constexpr uint16_t N = 2000;         // capture size in main.cpp

// offset + amplitude * sin(2*pi*i/period), rounded like the ADC would see it
std::vector<uint16_t> sine(uint16_t n, double period, double offset = 2048,
                           double amplitude = 1700) {
    std::vector<uint16_t> v(n);
    const double pi = std::acos(-1.0);
    for (uint16_t i = 0; i < n; ++i) {
        v[i] = static_cast<uint16_t>(
            std::lround(offset + amplitude * std::sin(2.0 * pi * i / period)));
    }
    return v;
}

// starts high: `high_samples` at `hi`, then the rest of the period at `lo`
std::vector<uint16_t> square(uint16_t n, uint16_t period, uint16_t high_samples,
                             uint16_t lo = 1000, uint16_t hi = 3000) {
    std::vector<uint16_t> v(n);
    for (uint16_t i = 0; i < n; ++i) {
        v[i] = (i % period) < high_samples ? hi : lo;
    }
    return v;
}

Measurements run(const std::vector<uint16_t>& v, uint32_t rate = RATE) {
    return measure(v.data(), static_cast<uint16_t>(v.size()), rate);
}

double freq_hz(const Measurements& m) {
    return m.freq_mhz / 1000.0;
}

} // namespace

// ---- degenerate input --------------------------------------------------------

TEST(Measure, EmptyInputGivesZeros) {
    const uint16_t dummy = 1234;
    const auto m = measure(&dummy, 0, RATE);   // must not read or divide by 0
    EXPECT_EQ(m.min, 0);
    EXPECT_EQ(m.max, 0);
    EXPECT_EQ(m.mean, 0);
    EXPECT_EQ(m.rms, 0);
    EXPECT_FALSE(m.periodic);
    EXPECT_EQ(m.freq_mhz, 0u);
}

TEST(Measure, SingleSample) {
    const uint16_t one = 1500;
    const auto m = measure(&one, 1, RATE);
    EXPECT_EQ(m.min, 1500);
    EXPECT_EQ(m.max, 1500);
    EXPECT_EQ(m.mean, 1500);
    EXPECT_EQ(m.rms, 1500);
    EXPECT_FALSE(m.periodic);
}

TEST(Measure, FlatSignal) {
    const auto m = run(std::vector<uint16_t>(N, 1000));
    EXPECT_EQ(m.min, 1000);
    EXPECT_EQ(m.max, 1000);
    EXPECT_EQ(m.mean, 1000);
    EXPECT_EQ(m.rms, 1000);   // pure DC: RMS equals the level
    EXPECT_FALSE(m.periodic);
    EXPECT_EQ(m.freq_mhz, 0u);
}

TEST(Measure, SmallNoiseIsNotPeriodic) {
    // +-10 counts around mid-scale: span 20, below the "is there a signal" limit
    std::vector<uint16_t> v(N);
    for (uint16_t i = 0; i < N; ++i) {
        v[i] = (i % 2) ? 2058 : 2038;
    }
    const auto m = run(v);
    EXPECT_EQ(m.min, 2038);
    EXPECT_EQ(m.max, 2058);
    EXPECT_FALSE(m.periodic);
}

TEST(Measure, LessThanTwoEdgesIsNotPeriodic) {
    // one period only: a single rising crossing, no full period between two
    const auto m = run(sine(100, 100));
    EXPECT_FALSE(m.periodic);
}

// ---- DAC test signal: 1 kHz sine, 2048 +- 1700, 100 kS/s ------------------------

TEST(Measure, SineAmplitudeAndLevels) {
    const auto m = run(sine(N, 100));
    EXPECT_EQ(m.min, 348);
    EXPECT_EQ(m.max, 3748);
    EXPECT_NEAR(m.mean, 2048, 1);
    // RMS with DC: sqrt(2048^2 + 1700^2 / 2) = 2374.7
    EXPECT_NEAR(m.rms, 2375, 2);
}

TEST(Measure, SineFrequencyAndDuty) {
    const auto m = run(sine(N, 100));
    ASSERT_TRUE(m.periodic);
    EXPECT_NEAR(freq_hz(m), 1000.0, 1.0);    // 0.1 %
    EXPECT_NEAR(m.duty_permille, 500, 10);   // 50 % +- 1 %
}

TEST(Measure, FrequencyAveragesOverAllPeriods) {
    // 3 kHz at 100 kS/s: 33.33 samples per period, not a whole number.
    // Two neighbouring crossings alone would give 3030 or 2941 Hz.
    const auto m = run(sine(N, 100'000.0 / 3'000.0));
    ASSERT_TRUE(m.periodic);
    EXPECT_NEAR(freq_hz(m), 3000.0, 3.0);    // 0.1 %
}

TEST(Measure, FrequencyScalesWithSampleRate) {
    // the same samples mean 10x the frequency at 10x the sample rate
    const auto m = run(sine(N, 100), 1'000'000);
    ASSERT_TRUE(m.periodic);
    EXPECT_NEAR(freq_hz(m), 10'000.0, 10.0);
}

TEST(Measure, LowFrequencyIsNotMilliHertzRounded) {
    // 1 kS/s, period 400 samples: 2.5 Hz must not become 2 or 3
    const auto m = run(sine(N, 400), 1'000);
    ASSERT_TRUE(m.periodic);
    EXPECT_NEAR(freq_hz(m), 2.5, 0.01);
}

TEST(Measure, IndependentOfOffset) {
    // the period is measured at 50 % of the signal, not at a fixed level
    const auto low = run(sine(N, 100, 800, 500));
    const auto high = run(sine(N, 100, 3200, 500));
    ASSERT_TRUE(low.periodic);
    ASSERT_TRUE(high.periodic);
    EXPECT_NEAR(freq_hz(low), 1000.0, 1.0);
    EXPECT_NEAR(freq_hz(high), 1000.0, 1.0);
}

TEST(Measure, NoiseDoesNotAddEdges) {
    // +-150 counts of noise on every sample: steeper than the sine itself near
    // the crossing (~107 counts per sample), so without hysteresis every
    // crossing would be counted several times. 300 pp is still below 10 % of
    // the span (~370), so hysteresis must filter it out.
    auto v = sine(N, 100);
    for (uint16_t i = 0; i < N; ++i) {
        v[i] = static_cast<uint16_t>(v[i] + ((i % 2) ? 150 : -150));
    }
    const auto m = run(v);
    ASSERT_TRUE(m.periodic);
    EXPECT_NEAR(freq_hz(m), 1000.0, 1.0);
}

// ---- square waves (PWM from the Pico) ---------------------------------------------

TEST(Measure, SquareQuarterDuty) {
    // period 100 samples, 25 high: 1 kHz, 25 %
    const auto m = run(square(N, 100, 25));
    EXPECT_EQ(m.min, 1000);
    EXPECT_EQ(m.max, 3000);
    EXPECT_EQ(m.mean, 1500);                  // 0.25 * 3000 + 0.75 * 1000
    EXPECT_NEAR(m.rms, 1732, 1);              // sqrt(0.25 * 3000^2 + 0.75 * 1000^2)
    ASSERT_TRUE(m.periodic);
    EXPECT_EQ(m.freq_mhz, 1'000'000u);        // exactly 1 kHz
    EXPECT_EQ(m.duty_permille, 250);
}

TEST(Measure, SquareDutyRange) {
    for (uint16_t high : {10, 50, 90}) {
        SCOPED_TRACE(high);
        const auto m = run(square(N, 100, high));
        ASSERT_TRUE(m.periodic);
        EXPECT_EQ(m.freq_mhz, 1'000'000u);
        EXPECT_EQ(m.duty_permille, high * 10);
    }
}

TEST(Measure, FastestSignal) {
    // alternating 0 / 4095 at 1 MS/s: period of 2 samples = 500 kHz (Nyquist)
    std::vector<uint16_t> v(N);
    for (uint16_t i = 0; i < N; ++i) {
        v[i] = (i % 2) ? 0 : 4095;
    }
    const auto m = run(v, 1'000'000);
    ASSERT_TRUE(m.periodic);
    EXPECT_EQ(m.freq_mhz, 500'000'000u);      // still fits in uint32_t
    EXPECT_EQ(m.duty_permille, 500);
}

// ---- no overflow -------------------------------------------------------------------

TEST(Measure, FullScaleDoesNotOverflow) {
    // 2000 * 4095^2 = 3.4e10: needs 64-bit sums
    const auto m = run(std::vector<uint16_t>(N, 4095));
    EXPECT_EQ(m.mean, 4095);
    EXPECT_EQ(m.rms, 4095);
}

TEST(Measure, MaxLengthBuffer) {
    // n is uint16_t: the largest buffer must work too
    const auto v = sine(65'535, 100);
    const auto m = run(v);
    EXPECT_NEAR(m.mean, 2048, 1);
    EXPECT_NEAR(m.rms, 2375, 2);
    ASSERT_TRUE(m.periodic);
    EXPECT_NEAR(freq_hz(m), 1000.0, 1.0);
}
