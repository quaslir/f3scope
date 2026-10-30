#include "app/commands.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using app::commands::Mode;
using app::commands::Parser;
using app::commands::Result;
using app::commands::Settings;
using app::trigger::Edge;

namespace {

// Feeds a string into the parser one char at a time, like the UART does.
// Every char before the last one must return None; the result of the last
// char is returned.
Result feed_line(Parser& p, std::string_view line) {
    Result last = Result::None;
    for (std::size_t i = 0; i < line.size(); ++i) {
        last = p.feed(line[i]);
        if (i + 1 < line.size()) {
            EXPECT_EQ(last, Result::None) << "early result at char " << i << " of \"" << line << "\"";
        }
    }
    return last;
}

void expect_same(const Settings& a, const Settings& b) {
    EXPECT_EQ(a.sample_rate_hz, b.sample_rate_hz);
    EXPECT_EQ(a.level, b.level);
    EXPECT_EQ(a.edge, b.edge);
    EXPECT_EQ(a.mode, b.mode);
    EXPECT_EQ(a.single_armed, b.single_armed);
}

// fresh parser + settings for every test
class Commands : public ::testing::Test {
  protected:
    Settings settings{};
    Parser parser{settings};
    const Settings defaults{};
};

} // namespace

// ---- defaults ----------------------------------------------------------------

TEST_F(Commands, Defaults) {
    EXPECT_EQ(defaults.sample_rate_hz, 100'000u);
    EXPECT_EQ(defaults.level, 2048);
    EXPECT_EQ(defaults.edge, Edge::Rising);
    EXPECT_EQ(defaults.mode, Mode::Auto);
    EXPECT_FALSE(defaults.single_armed);
}

// ---- good commands change only their own field --------------------------------

TEST_F(Commands, Rate) {
    EXPECT_EQ(feed_line(parser, "RATE 500000\n"), Result::Ok);
    Settings expected = defaults;
    expected.sample_rate_hz = 500'000;
    expect_same(settings, expected);
}

TEST_F(Commands, Level) {
    EXPECT_EQ(feed_line(parser, "LEVEL 2482\n"), Result::Ok);
    Settings expected = defaults;
    expected.level = 2482;
    expect_same(settings, expected);
}

TEST_F(Commands, EdgeFalling) {
    EXPECT_EQ(feed_line(parser, "EDGE F\n"), Result::Ok);
    Settings expected = defaults;
    expected.edge = Edge::Falling;
    expect_same(settings, expected);
}

TEST_F(Commands, ModeNormal) {
    EXPECT_EQ(feed_line(parser, "MODE N\n"), Result::Ok);
    Settings expected = defaults;
    expected.mode = Mode::Normal;
    expect_same(settings, expected);
}

// ---- regressions found on the board -------------------------------------------

TEST_F(Commands, EdgeRisingAfterFalling) {
    // 'R' must map to Edge::Rising, not be cast from the char code
    ASSERT_EQ(feed_line(parser, "EDGE F\n"), Result::Ok);
    ASSERT_EQ(feed_line(parser, "EDGE R\n"), Result::Ok);
    EXPECT_EQ(settings.edge, Edge::Rising);
}

TEST_F(Commands, ModeSingleArms) {
    EXPECT_EQ(feed_line(parser, "MODE S\n"), Result::Ok);
    EXPECT_EQ(settings.mode, Mode::Single);
    EXPECT_TRUE(settings.single_armed);
}

TEST_F(Commands, ModeSingleRearmsAfterCapture) {
    ASSERT_EQ(feed_line(parser, "MODE S\n"), Result::Ok);
    settings.single_armed = false;   // main.cpp does this after sending a frame
    EXPECT_EQ(feed_line(parser, "MODE S\n"), Result::Ok);
    EXPECT_TRUE(settings.single_armed);
}

TEST_F(Commands, ModeAutoAndNormalDoNotArm) {
    EXPECT_EQ(feed_line(parser, "MODE A\n"), Result::Ok);
    EXPECT_FALSE(settings.single_armed);
    EXPECT_EQ(feed_line(parser, "MODE N\n"), Result::Ok);
    EXPECT_FALSE(settings.single_armed);
}

// ---- limits ------------------------------------------------------------------------

TEST_F(Commands, RateLimitsAreAccepted) {
    EXPECT_EQ(feed_line(parser, "RATE 1000\n"), Result::Ok);
    EXPECT_EQ(settings.sample_rate_hz, 1'000u);
    EXPECT_EQ(feed_line(parser, "RATE 1000000\n"), Result::Ok);
    EXPECT_EQ(settings.sample_rate_hz, 1'000'000u);
}

TEST_F(Commands, LevelLimitsAreAccepted) {
    EXPECT_EQ(feed_line(parser, "LEVEL 0\n"), Result::Ok);
    EXPECT_EQ(settings.level, 0);
    EXPECT_EQ(feed_line(parser, "LEVEL 4095\n"), Result::Ok);
    EXPECT_EQ(settings.level, 4095);
}

// ---- bad values: BadValue and settings untouched --------------------------------

class BadValue : public ::testing::TestWithParam<const char*> {};

TEST_P(BadValue, IsRejectedAndChangesNothing) {
    Settings settings{};
    Parser parser{settings};
    const std::string line = std::string(GetParam()) + "\n";
    EXPECT_EQ(feed_line(parser, line), Result::BadValue) << "line: " << GetParam();
    expect_same(settings, Settings{});
}

INSTANTIATE_TEST_SUITE_P(Commands, BadValue,
                         ::testing::Values("RATE 999",          // below RATE_MIN
                                           "RATE 1000001",      // above RATE_MAX
                                           "RATE",              // no argument
                                           "RATE ",             // empty argument
                                           "RATE 1e5",          // not an integer
                                           "LEVEL 4096",        // above 12 bits
                                           "LEVEL 70000",       // would overflow uint16_t
                                           "LEVEL -1",          // negative
                                           "LEVEL 12abc",       // trailing garbage
                                           "LEVEL  2000",       // two spaces
                                           "EDGE",              // no argument (was UB)
                                           "EDGE X",            // unknown edge
                                           "EDGE RF",           // more than one char
                                           "EDGE r",            // lower case
                                           "MODE",              // no argument
                                           "MODE X",            // unknown mode
                                           "MODE AN"));         // more than one char

// ---- other lines ---------------------------------------------------------------------

TEST_F(Commands, UnknownCommand) {
    EXPECT_EQ(feed_line(parser, "HELLO 5\n"), Result::UnknownCommand);
    EXPECT_EQ(feed_line(parser, "HELLO\n"), Result::UnknownCommand);
    expect_same(settings, defaults);
}

TEST_F(Commands, CommandNamesAreCaseSensitive) {
    EXPECT_EQ(feed_line(parser, "rate 1000\n"), Result::UnknownCommand);
    expect_same(settings, defaults);
}

TEST_F(Commands, EmptyLineIsNotAnError) {
    EXPECT_EQ(feed_line(parser, "\n"), Result::None);
    EXPECT_EQ(feed_line(parser, "\r\n"), Result::None);
}

TEST_F(Commands, CrLfWorksLikeLf) {
    EXPECT_EQ(feed_line(parser, "LEVEL 1000\r\n"), Result::Ok);
    EXPECT_EQ(settings.level, 1000);
}

TEST_F(Commands, SeveralCommandsInARow) {
    EXPECT_EQ(feed_line(parser, "RATE 10000\n"), Result::Ok);
    EXPECT_EQ(feed_line(parser, "LEVEL 1000\n"), Result::Ok);
    EXPECT_EQ(feed_line(parser, "EDGE F\n"), Result::Ok);
    EXPECT_EQ(feed_line(parser, "MODE N\n"), Result::Ok);
    EXPECT_EQ(settings.sample_rate_hz, 10'000u);
    EXPECT_EQ(settings.level, 1000);
    EXPECT_EQ(settings.edge, Edge::Falling);
    EXPECT_EQ(settings.mode, Mode::Normal);
}

TEST_F(Commands, BadLineDoesNotBreakTheNextOne) {
    EXPECT_EQ(feed_line(parser, "LEVEL 9999\n"), Result::BadValue);
    EXPECT_EQ(feed_line(parser, "LEVEL 1000\n"), Result::Ok);
    EXPECT_EQ(settings.level, 1000);
}

// ---- line length ---------------------------------------------------------------------

TEST_F(Commands, LineOfMaxLengthIsParsed) {
    // "RATE " + 27 digits = 32 chars = MAX_LINE_LENGTH
    const std::string line = "RATE " + std::string(21, '0') + "100000";
    ASSERT_EQ(line.size(), app::commands::MAX_LINE_LENGTH);
    EXPECT_EQ(feed_line(parser, line + "\n"), Result::Ok);
    EXPECT_EQ(settings.sample_rate_hz, 100'000u);
}

TEST_F(Commands, LineOneCharTooLong) {
    const std::string line = "RATE " + std::string(22, '0') + "100000";
    ASSERT_EQ(line.size(), app::commands::MAX_LINE_LENGTH + 1u);
    EXPECT_EQ(feed_line(parser, line + "\n"), Result::TooLong);
    expect_same(settings, defaults);
}

TEST_F(Commands, VeryLongLineGivesOneError) {
    // feed_line also checks that no error is reported before the '\n'
    EXPECT_EQ(feed_line(parser, std::string(500, 'X') + "\n"), Result::TooLong);
}

TEST_F(Commands, ParserRecoversAfterTooLong) {
    ASSERT_EQ(feed_line(parser, std::string(100, 'X') + "\n"), Result::TooLong);
    EXPECT_EQ(feed_line(parser, "LEVEL 1234\n"), Result::Ok);
    EXPECT_EQ(settings.level, 1234);
}

// ---- to_string -------------------------------------------------------------------------

TEST(CommandsToString, EveryResultHasAName) {
    using app::commands::to_string;
    EXPECT_STREQ(to_string(Result::Ok), "ok");
    EXPECT_STREQ(to_string(Result::UnknownCommand), "unknown command");
    EXPECT_STREQ(to_string(Result::BadValue), "bad value");
    EXPECT_STREQ(to_string(Result::TooLong), "line too long");
}
