#include <gtest/gtest.h>
#include <tick_clock.hpp>
using kuki::TickClock;
TEST(TickClock, OwesNothingUntilATickHasPassed) {
  TickClock clock;
  EXPECT_EQ(clock.Advance(.01), 0);
  EXPECT_EQ(clock.Advance(.01), 1);
  EXPECT_NEAR(clock.Alpha(), .2f, 1e-4f);
}
TEST(TickClock, OwesSeveralAfterASlowFrame) {
  TickClock clock;
  EXPECT_EQ(clock.Advance(.05), 3);
}
TEST(TickClock, CountsTheSameTicksAtAnyFrameRate) {
  for (const auto frames : {30, 50, 144, 1000}) {
    TickClock clock;
    auto ticks = 0;
    for (auto frame = 0; frame < frames * 10; ++frame)
      ticks += clock.Advance(1. / frames);
    EXPECT_NEAR(ticks, 600, 1) << frames << " frames a second";
  }
}
TEST(TickClock, ForgetsABacklogRatherThanRepayingIt) {
  TickClock clock;
  EXPECT_EQ(clock.Advance(5.), TickClock::MAX_TICKS_PER_FRAME);
  EXPECT_EQ(clock.Advance(0.), 0);
  EXPECT_LT(clock.Alpha(), 1.f);
}
TEST(TickClock, NumbersTheTickBeingRun) {
  TickClock clock;
  EXPECT_EQ(clock.Index(), 0u);
  clock.Complete();
  EXPECT_EQ(clock.Index(), 1u);
}
TEST(TickClock, FollowsTheRate) {
  TickClock clock;
  clock.SetRate(120);
  EXPECT_EQ(clock.Advance(.05), 6);
  clock.SetRate(0);
  EXPECT_EQ(clock.Rate(), 1);
}
