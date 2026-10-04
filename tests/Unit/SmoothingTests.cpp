#include "dsp/Smoothing.h"

#include <catch2/catch_test_macros.hpp>

using a5::dsp::LinearSmoother;

TEST_CASE("LinearSmoother lands exactly on its target when the ramp ends")
{
    LinearSmoother smoother;
    smoother.prepare(48000.0, 0.01); // 480 samples
    smoother.setCurrentAndTarget(1.0f);
    smoother.setTarget(0.3f);

    float value = 1.0f;
    for (int sample = 0; sample < 480; ++sample)
        value = smoother.getNextValue();

    CHECK(value == 0.3f);
    CHECK_FALSE(smoother.isSmoothing());
    CHECK(smoother.getNextValue() == 0.3f);
}

TEST_CASE("LinearSmoother moves monotonically towards the target")
{
    LinearSmoother smoother;
    smoother.prepare(44100.0, 0.02);
    smoother.setCurrentAndTarget(0.0f);
    smoother.setTarget(1.0f);

    float previous = 0.0f;
    while (smoother.isSmoothing())
    {
        const float value = smoother.getNextValue();
        REQUIRE(value >= previous);
        REQUIRE(value <= 1.0f);
        previous = value;
    }

    CHECK(previous == 1.0f);
}

TEST_CASE("LinearSmoother with a zero ramp jumps immediately")
{
    LinearSmoother smoother;
    smoother.prepare(44100.0, 0.0);
    smoother.setCurrentAndTarget(0.0f);
    smoother.setTarget(0.5f);

    CHECK_FALSE(smoother.isSmoothing());
    CHECK(smoother.getNextValue() == 0.5f);
}
