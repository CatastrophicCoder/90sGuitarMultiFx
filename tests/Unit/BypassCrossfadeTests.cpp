#include "dsp/BypassCrossfade.h"

#include <catch2/catch_test_macros.hpp>

using fivea::dsp::BypassCrossfade;

namespace
{
int samplesUntilSettled(BypassCrossfade& crossfade)
{
    int count = 0;
    while (crossfade.isRamping())
    {
        (void)crossfade.getNextWetGain();
        ++count;
    }
    return count;
}
} // namespace

TEST_CASE("BypassCrossfade starts settled in the state it was reset to")
{
    BypassCrossfade crossfade;
    crossfade.prepare(48000.0, 0.01);

    crossfade.reset(false);
    CHECK(crossfade.isBypassed());
    CHECK_FALSE(crossfade.isFullyEnabled());
    CHECK(crossfade.getNextWetGain() == 0.0f);

    crossfade.reset(true);
    CHECK(crossfade.isFullyEnabled());
    CHECK_FALSE(crossfade.isBypassed());
    CHECK(crossfade.getNextWetGain() == 1.0f);
}

TEST_CASE("BypassCrossfade ramps over its length and lands exactly on 0 or 1")
{
    BypassCrossfade crossfade;
    crossfade.prepare(48000.0, 0.01); // 480 samples
    crossfade.reset(false);

    crossfade.setEnabled(true);
    CHECK_FALSE(crossfade.isBypassed());
    CHECK(samplesUntilSettled(crossfade) == 480);
    CHECK(crossfade.isFullyEnabled());
    CHECK(crossfade.getNextWetGain() == 1.0f);

    crossfade.setEnabled(false);
    CHECK_FALSE(crossfade.isBypassed()); // still fading out: the block must keep running
    CHECK(samplesUntilSettled(crossfade) == 480);
    CHECK(crossfade.isBypassed());
    CHECK(crossfade.getNextWetGain() == 0.0f);
}

TEST_CASE("BypassCrossfade gain moves monotonically without overshoot")
{
    BypassCrossfade crossfade;
    crossfade.prepare(44100.0, 0.01);
    crossfade.reset(false);
    crossfade.setEnabled(true);

    float previous = 0.0f;
    while (crossfade.isRamping())
    {
        const float gain = crossfade.getNextWetGain();
        REQUIRE(gain >= previous);
        REQUIRE(gain <= 1.0f);
        previous = gain;
    }
}

TEST_CASE("Repeating the current state does not restart the ramp")
{
    BypassCrossfade crossfade;
    crossfade.prepare(44100.0, 0.01);
    crossfade.reset(true);

    crossfade.setEnabled(true);
    CHECK_FALSE(crossfade.isRamping());
    CHECK(crossfade.isFullyEnabled());
}
