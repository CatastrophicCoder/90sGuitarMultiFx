#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/ProgramTransition.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using fivea::ProgramTransition;
using fivea::test::PlanarBuffer;
using fivea::test::ScopedAllocationCounter;

namespace
{
constexpr double sampleRate = 1000.0; // 10 ms = 10 samples: easy to count

// The raised-cosine fade: 0 at position 0, 1 at the fade's length.
double cosineGain(int position, int length)
{
    return 0.5 - 0.5 * std::cos(std::numbers::pi * position / length);
}

// The gains the transition applies, sample by sample, to a buffer of ones.
std::vector<float> gains(ProgramTransition& transition, int numSamples)
{
    PlanarBuffer ones{1, numSamples};
    ones.fill(1.0f);
    transition.applyGain(ones.view());
    return ones.data()[0];
}

struct Prepared
{
    Prepared() { transition.prepare(sampleRate, 0.01, 0.01); }
    ProgramTransition transition;
};
} // namespace

TEST_CASE("With no program change the transition leaves the signal bit for bit")
{
    Prepared prepared;
    auto& transition = prepared.transition;
    PlanarBuffer noise{2, 256};
    noise.fillWithNoise(3);
    const auto before = noise.data();
    CHECK_FALSE(transition.poll(true));
    transition.applyGain(noise.view());
    CHECK(noise.data() == before);
    CHECK(transition.isIdle());
}

TEST_CASE("A program change fades out, waits silent for the new settings, then fades in")
{
    Prepared prepared;
    auto& transition = prepared.transition;
    const auto ticket = transition.post();
    CHECK(transition.poll(true));
    CHECK(transition.holdsSettings());
    CHECK(transition.samplesUntilSilent() == 10);

    const auto down = gains(transition, 10);
    for (int n = 0; n < 10; ++n)
        CHECK_THAT(down[static_cast<std::size_t>(n)], Catch::Matchers::WithinAbs(cosineGain(9 - n, 10), 1.0e-6));
    CHECK(down.back() == 0.0f);
    CHECK(transition.isSilent());

    // The message thread has not finished setting the parameters: stay silent and keep holding.
    CHECK_FALSE(transition.readyToSwitch());
    CHECK(gains(transition, 50) == std::vector<float>(50, 0.0f));
    CHECK(transition.holdsSettings());

    transition.complete(ticket);
    CHECK(transition.readyToSwitch());
    transition.switched();
    CHECK_FALSE(transition.holdsSettings());

    const auto up = gains(transition, 20);
    for (int n = 0; n < 9; ++n)
        CHECK_THAT(up[static_cast<std::size_t>(n)], Catch::Matchers::WithinAbs(cosineGain(n + 1, 10), 1.0e-6));
    for (int n = 9; n < 20; ++n)
        CHECK(up[static_cast<std::size_t>(n)] == 1.0f);
    CHECK(transition.isIdle());
}

TEST_CASE("Silence is written as exact zeros, whatever the input holds")
{
    Prepared prepared;
    auto& transition = prepared.transition;
    transition.complete(transition.post());
    transition.poll(true);
    (void)gains(transition, 10);
    PlanarBuffer odd{1, 4};
    odd.fill(std::numeric_limits<float>::quiet_NaN());
    transition.applyGain(odd.view());
    CHECK(odd.data()[0] == std::vector<float>(4, 0.0f));
}

TEST_CASE("A change while fading in fades out from the gain reached, without a jump")
{
    Prepared prepared;
    auto& transition = prepared.transition;
    transition.complete(transition.post());
    transition.poll(true);
    (void)gains(transition, 10);
    transition.switched();
    const auto up = gains(transition, 4);
    CHECK_THAT(up.back(), Catch::Matchers::WithinAbs(cosineGain(4, 10), 1.0e-6));

    const auto second = transition.post();
    CHECK(transition.poll(true));
    CHECK(transition.samplesUntilSilent() == 4); // the same point on the way down
    const auto down = gains(transition, 4);
    CHECK_THAT(down[0], Catch::Matchers::WithinAbs(cosineGain(3, 10), 1.0e-6));
    CHECK(down.back() == 0.0f);

    transition.complete(second);
    CHECK(transition.readyToSwitch());
}

TEST_CASE("A change while fading out continues the same fade, and waits for the newest settings")
{
    Prepared prepared;
    auto& transition = prepared.transition;
    const auto first = transition.post();
    transition.poll(true);
    (void)gains(transition, 3);
    CHECK(transition.samplesUntilSilent() == 7);

    const auto second = transition.post();
    CHECK(transition.poll(true));
    CHECK(transition.samplesUntilSilent() == 7); // not restarted from unity
    (void)gains(transition, 7);

    transition.complete(first);
    CHECK_FALSE(transition.readyToSwitch()); // the first change's settings are already stale
    transition.complete(second);
    CHECK(transition.readyToSwitch());
}

TEST_CASE("Without a fade, the old settings are held at full gain until the switch")
{
    Prepared prepared;
    auto& transition = prepared.transition;
    const auto ticket = transition.post();
    CHECK(transition.poll(false));
    CHECK(transition.holdsSettings());
    CHECK_FALSE(transition.isSilent());
    CHECK(gains(transition, 30) == std::vector<float>(30, 1.0f));
    CHECK_FALSE(transition.readyToSwitch());

    transition.complete(ticket);
    CHECK(transition.readyToSwitch());
    transition.switched();
    CHECK(transition.isIdle());
    CHECK(gains(transition, 5) == std::vector<float>(5, 1.0f));
}

TEST_CASE("Reset abandons a transition and forgets the requests posted so far")
{
    Prepared prepared;
    auto& transition = prepared.transition;
    (void)transition.post();
    transition.reset();
    CHECK_FALSE(transition.poll(true));
    CHECK(transition.isIdle());
}

TEST_CASE("The fades last the same time at every sample rate")
{
    for (const double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0})
    {
        ProgramTransition transition;
        transition.prepare(rate, 0.01, 0.01);
        (void)transition.post();
        transition.poll(true);
        INFO(rate);
        CHECK(transition.samplesUntilSilent() == static_cast<int>(std::lround(rate * 0.01)));
    }
}

TEST_CASE("The transition does not allocate")
{
    Prepared prepared;
    auto& transition = prepared.transition;
    PlanarBuffer buffer{2, 64};
    buffer.fill(0.5f);
    std::size_t allocations = 1;
    {
        const ScopedAllocationCounter counter;
        const auto ticket = transition.post();
        transition.poll(true);
        transition.applyGain(buffer.view());
        transition.complete(ticket);
        if (transition.readyToSwitch())
            transition.switched();
        transition.applyGain(buffer.view());
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}
