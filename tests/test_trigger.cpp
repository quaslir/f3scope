#include "app/trigger.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

using app::trigger::Edge;
using app::trigger::find;

namespace {

constexpr uint16_t LEVEL = 2048;
constexpr uint16_t HYST = 100;
// rising:  arms at <= 1948, fires at >= 2048
// falling: arms at >= 2148, fires at <= 2048

template <std::size_t N>
std::optional<uint16_t> run(const std::array<uint16_t, N>& data, Edge edge,
                            uint16_t level = LEVEL, uint16_t hyst = HYST) {
    return find(data.data(), static_cast<uint16_t>(data.size()), level, hyst, edge);
}

std::vector<uint16_t> sine(std::size_t n, std::size_t period) {
    std::vector<uint16_t> v(n);
    const double pi = std::acos(-1.0);
    for (std::size_t i = 0; i < n; ++i) {
        v[i] = static_cast<uint16_t>(
            std::lround(2048.0 + 1000.0 * std::sin(2.0 * pi * double(i) / double(period))));
    }
    return v;
}

} // namespace

// ---- basic -------------------------------------------------------------------

TEST(Trigger, RisingFindsCrossingAfterArming) {
    const std::array<uint16_t, 5> d{3000, 1000, 1500, 2100, 3000};
    const auto r = run(d, Edge::Rising);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 3);
}

TEST(Trigger, FallingFindsCrossingAfterArming) {
    const std::array<uint16_t, 5> d{1000, 3000, 2500, 2000, 1000};
    const auto r = run(d, Edge::Falling);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 3);
}

TEST(Trigger, RisingNeedsToArmFirst) {
    // already above the level, never goes below the arming threshold
    const std::array<uint16_t, 3> d{2500, 2600, 2700};
    EXPECT_FALSE(run(d, Edge::Rising).has_value());
}

TEST(Trigger, FallingNeedsToArmFirst) {
    // already below the level, never goes above the arming threshold
    const std::array<uint16_t, 3> d{1500, 1400, 1300};
    EXPECT_FALSE(run(d, Edge::Falling).has_value());
}

TEST(Trigger, ThresholdsAreInclusive) {
    const std::array<uint16_t, 2> rising{1948, 2048};
    const auto r = run(rising, Edge::Rising);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 1);

    const std::array<uint16_t, 2> falling{2148, 2048};
    const auto f = run(falling, Edge::Falling);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(*f, 1);
}

// ---- noise and hysteresis --------------------------------------------------

TEST(Trigger, RisingIgnoresNoiseSmallerThanHysteresis) {
    const std::array<uint16_t, 5> d{2000, 2060, 2000, 2060, 2000};
    EXPECT_FALSE(run(d, Edge::Rising).has_value());
}

TEST(Trigger, FallingIgnoresNoiseSmallerThanHysteresis) {
    const std::array<uint16_t, 4> d{2100, 2040, 2100, 2040};
    EXPECT_FALSE(run(d, Edge::Falling).has_value());
}

TEST(Trigger, ZeroHysteresis) {
    const std::array<uint16_t, 2> d{1000, 2048};
    const auto r = run(d, Edge::Rising, LEVEL, 0);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 1);
}

// ---- empty and flat input ----------------------------------------------------

TEST(Trigger, EmptyInput) {
    const uint16_t dummy = 0;
    EXPECT_FALSE(find(&dummy, 0, LEVEL, HYST, Edge::Rising).has_value());
    EXPECT_FALSE(find(&dummy, 0, LEVEL, HYST, Edge::Falling).has_value());
}

TEST(Trigger, FlatSignalNeverTriggers) {
    const std::vector<uint16_t> d(1000, 2048);
    const auto n = static_cast<uint16_t>(d.size());
    EXPECT_FALSE(find(d.data(), n, LEVEL, HYST, Edge::Rising).has_value());
    EXPECT_FALSE(find(d.data(), n, LEVEL, HYST, Edge::Falling).has_value());
}

// ---- real sine -----------------------------------------------------------------

TEST(Trigger, SineRisingReturnsFirstRealEdge) {
    // i = 0 is exactly 2048 but not armed yet; the first real rising edge
    // after the trough is at the start of the second period
    const auto d = sine(200, 100);
    const auto r = find(d.data(), static_cast<uint16_t>(d.size()), LEVEL, HYST, Edge::Rising);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 100);
}

TEST(Trigger, SineFallingReturnsFirstRealEdge) {
    // arms on the way up (i = 2), fires at the half period
    const auto d = sine(200, 100);
    const auto r = find(d.data(), static_cast<uint16_t>(d.size()), LEVEL, HYST, Edge::Falling);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 50);
}

// ---- ends of the ADC range (documents current behaviour) --------------------

TEST(Trigger, RisingNearBottomCannotArm) {
    // level 10, hysteresis 50: the arming threshold would be -40
    const std::array<uint16_t, 3> d{0, 5, 20};
    EXPECT_FALSE(run(d, Edge::Rising, 10, 50).has_value());
}

TEST(Trigger, FallingNearTopCannotArm) {
    // level 4090, hysteresis 50: the arming threshold would be 4140 > 4095
    const std::array<uint16_t, 3> d{4095, 4000, 4095};
    EXPECT_FALSE(run(d, Edge::Falling, 4090, 50).has_value());
}
