#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/ModelProfile.h"
#include "core/StepMapping.h"
#include "dsp/DelayLine.h"
#include "dsp/Modulation.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace fivea;
using namespace fivea::dsp;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using fivea::test::PlanarBuffer;

namespace
{
const ModulationProfile& modulationProfile = functionalPlaceholderProfile.modulation;

Modulation makeModulation(double sampleRate, int numChannels, const Modulation::Settings& settings)
{
    Modulation modulation;
    modulation.prepare(sampleRate, numChannels, functionalPlaceholderProfile);
    modulation.setSettings(settings);
    modulation.reset();
    return modulation;
}

// With MIX 15 (effect equal to dry) and no feedback, a ramp input x[n] = k·n comes out as
// y[n] = k·n + k·(n − d[n]), because the cubic interpolator reproduces a straight line exactly.
// So the delay actually used at every sample is d[n] = 2n − y[n]/k.
std::vector<std::vector<double>> recoverDelays(Modulation& modulation, int numChannels, int length)
{
    constexpr double slope = 1.0e-5;
    PlanarBuffer buffer{numChannels, length};
    for (int channel = 0; channel < numChannels; ++channel)
        for (int n = 0; n < length; ++n)
            buffer.channel(channel)[static_cast<std::size_t>(n)] = static_cast<float>(slope * n);

    modulation.process(buffer.view());

    std::vector<std::vector<double>> delays(static_cast<std::size_t>(numChannels));
    for (int channel = 0; channel < numChannels; ++channel)
        for (int n = 0; n < length; ++n)
            delays[static_cast<std::size_t>(channel)].push_back(
                2.0 * n - buffer.data()[static_cast<std::size_t>(channel)][static_cast<std::size_t>(n)] / slope);
    return delays;
}

double millisecondsToSamples(double ms, double sampleRate)
{
    return ms * 0.001 * sampleRate;
}

// Same click measure as the drive's: the transition against both settled states. Measured
// 2026-10-05 (50 Hz, 0.05 amplitude, 48 kHz): ramped changes 1.8 (MIX), 3.5 (F.BACK), 9.5–10.9
// (MODE), 17.8 (DEPTH), 8.6 (SPEED, which needs no ramp: the LFO keeps its phase); the same changes
// unramped 1 370 to 11 300. Do not raise the bound without asking.
constexpr double clickRatioBound = 100.0;

double clickRatio(const Modulation::Settings& before, const Modulation::Settings& after)
{
    const double sampleRate = 48000.0;
    auto modulation = makeModulation(sampleRate, 1, before);
    PlanarBuffer buffer{1, 96000};
    fivea::test::fillSine(buffer, 50.0, sampleRate, 0.05f);

    const int change = 48240; // a sine peak
    modulation.process(buffer.viewOf(0, change));
    modulation.setSettings(after);
    modulation.process(buffer.viewOf(change, buffer.getNumSamples() - change));

    const auto& y = buffer.data()[0];
    auto largestBetween = [&](int from, int to)
    {
        double largest = 0.0;
        for (int n = from; n < to; ++n)
        {
            const auto i = static_cast<std::size_t>(n);
            largest = std::max(largest, static_cast<double>(std::abs(y[i] - 2.0f * y[i - 1] + y[i - 2])));
        }
        return largest;
    };
    return largestBetween(change, change + 9600) /
           std::max(largestBetween(change - 19200, change), largestBetween(96000 - 19200, 96000));
}
} // namespace

// --- Delay line ----------------------------------------------------------------------------------

TEST_CASE("Delay line: whole-sample delays are exact")
{
    const auto interpolation = GENERATE(Interpolation::Linear, Interpolation::Cubic);
    DelayLine line;
    line.prepare(100, 1);
    line.setInterpolation(interpolation);

    std::vector<float> out;
    for (int n = 0; n < 60; ++n)
    {
        out.push_back(line.read(0, 37.0f));
        line.write(0, n == 5 ? 1.0f : 0.0f);
    }
    for (int n = 0; n < 60; ++n)
        CHECK(out[static_cast<std::size_t>(n)] == (n == 5 + 37 ? 1.0f : 0.0f));
}

TEST_CASE("Delay line: fractional delays reproduce a straight line exactly and a sine closely")
{
    DelayLine line;
    line.prepare(100, 1);

    double largestRampError = 0.0;
    double largestSineError = 0.0;
    for (int n = 0; n < 400; ++n)
    {
        const float delayed = line.read(0, 10.25f);
        if (n > 20)
            largestRampError = std::max(largestRampError, std::abs(delayed - 0.001 * (n - 10.25)));
        line.write(0, static_cast<float>(0.001 * n));
    }

    line.reset();
    const double omega = 2.0 * std::numbers::pi * 1000.0 / 48000.0;
    for (int n = 0; n < 400; ++n)
    {
        const float delayed = line.read(0, 10.5f);
        if (n > 20)
            largestSineError = std::max(largestSineError, std::abs(delayed - std::sin(omega * (n - 10.5))));
        line.write(0, static_cast<float>(std::sin(omega * n)));
    }

    CHECK(largestRampError < 1.0e-6);
    CHECK(largestSineError < 1.0e-3);
}

TEST_CASE("Delay line: requested delays are clamped to what the buffer holds")
{
    DelayLine line;
    line.prepare(50, 2);
    for (int n = 0; n < 200; ++n)
    {
        CHECK(std::isfinite(line.read(0, 1.0e9f)));
        CHECK(std::isfinite(line.read(1, -5.0f)));
        line.write(0, 1.0f);
        line.write(1, -1.0f);
    }
    CHECK(line.maximumDelay() >= 50.0f);
}

// --- The block -----------------------------------------------------------------------------------

TEST_CASE("Each MODE's delay is the documented one")
{
    const double sampleRate = GENERATE(44100.0, 48000.0, 96000.0);
    const double documentedMs[] = {1.8, 4.0, 24.0, 32.0, 75.0}; // SRC-001 p. 11, written out on purpose
    for (int mode = 1; mode <= 5; ++mode)
    {
        auto modulation = makeModulation(sampleRate, 1, {.mode = mode, .depth = 0, .feedback = 0, .mix = 15});
        const auto delays = recoverDelays(modulation, 1, 16384);
        const double expected = millisecondsToSamples(documentedMs[mode - 1], sampleRate);

        INFO("fs " << sampleRate << ", MODE " << mode);
        for (std::size_t n = 8192; n < delays[0].size(); n += 997)
            CHECK_THAT(delays[0][n], WithinAbs(expected, 0.02));
    }
}

TEST_CASE("The effect path delays a high-frequency sine accurately (cubic interpolation)")
{
    // Flanger 1 at 44.1 kHz is a fractional delay (79.38 samples). Largest error against the ideal
    // delayed sine, relative to full scale, measured 2026-10-05 at 4 kHz: cubic 0.28 %, linear
    // 3.8 %. Do not raise the bound without asking.
    const double sampleRate = 44100.0;
    auto modulation = makeModulation(sampleRate, 1, {.mode = 1, .depth = 0, .feedback = 0, .mix = 15});
    PlanarBuffer buffer{1, 8192};
    fivea::test::fillSine(buffer, 4000.0, sampleRate, 1.0f);
    const auto input = buffer.data()[0];
    modulation.process(buffer.view());

    const double delay = millisecondsToSamples(1.8, sampleRate);
    const double omega = 2.0 * std::numbers::pi * 4000.0 / sampleRate;
    double largestError = 0.0;
    for (std::size_t n = 1024; n < input.size(); ++n)
    {
        const double wet = buffer.data()[0][n] - input[n];
        largestError = std::max(largestError, std::abs(wet - std::sin(omega * (static_cast<double>(n) - delay))));
    }
    CHECK(largestError < 0.01);
}

TEST_CASE("DEPTH sets the sweep's range and SPEED its rate")
{
    const double sampleRate = 48000.0;
    const int speed = GENERATE(8, 12); // about 1.2 and 4 Hz: several cycles in four seconds
    auto modulation = makeModulation(sampleRate, 1, {.mode = 3, .speed = speed, .depth = 15, .mix = 15});
    const auto delays = recoverDelays(modulation, 1, 192000)[0];

    const double base = millisecondsToSamples(24.0, sampleRate);
    const double depth =
        millisecondsToSamples(mapping::modulationDepthMs(15, ModulationMode::Chorus1, modulationProfile), sampleRate);
    const auto [lowest, highest] = std::minmax_element(delays.begin() + 4800, delays.end());
    CHECK_THAT(*lowest, WithinAbs(base - depth, 0.05));
    CHECK_THAT(*highest, WithinAbs(base + depth, 0.05));

    // Rate from upward crossings of the base delay.
    std::vector<double> crossings;
    for (std::size_t n = 4801; n < delays.size(); ++n)
        if (delays[n - 1] < base && delays[n] >= base)
            crossings.push_back(static_cast<double>(n));
    REQUIRE(crossings.size() >= 3);
    const double period = (crossings.back() - crossings.front()) / static_cast<double>(crossings.size() - 1);
    CHECK_THAT(sampleRate / period,
               WithinRel(static_cast<double>(mapping::modulationSpeedHz(speed, modulationProfile)), 0.01));
}

TEST_CASE("The right channel's sweep leads the left by the stereo phase")
{
    const double sampleRate = 48000.0;
    auto modulation = makeModulation(sampleRate, 2, {.mode = 4, .speed = 10, .depth = 15, .mix = 15});
    const auto delays = recoverDelays(modulation, 2, 96000);
    const double base = millisecondsToSamples(32.0, sampleRate);
    const double depth =
        millisecondsToSamples(mapping::modulationDepthMs(15, ModulationMode::Chorus2, modulationProfile), sampleRate);

    // Where the left sweep crosses its centre going up (LFO phase 0), the right one, a quarter cycle
    // ahead, is at its top.
    int checked = 0;
    for (std::size_t n = 2401; n < delays[0].size(); ++n)
        if (delays[0][n - 1] < base && delays[0][n] >= base)
        {
            CHECK_THAT(delays[1][n], WithinAbs(base + depth, 0.02 * depth));
            ++checked;
        }
    CHECK(checked >= 3);
}

TEST_CASE("A mono input comes out as stereo once the sweep is on")
{
    auto swept = makeModulation(48000.0, 2, {.mode = 3, .depth = 8, .mix = 12});
    auto still = makeModulation(48000.0, 2, {.mode = 3, .depth = 0, .mix = 12});

    for (auto* modulation : {&swept, &still})
    {
        PlanarBuffer buffer{2, 4800};
        buffer.fillWithNoise(21);
        buffer.channel(1) = buffer.channel(0); // the same guitar in both channels
        modulation->process(buffer.view());
        if (modulation == &swept)
            CHECK(buffer.data()[0] != buffer.data()[1]);
        else
            CHECK(buffer.data()[0] == buffer.data()[1]);
    }
}

TEST_CASE("MIX 0 is the dry signal, bit for bit")
{
    auto modulation = makeModulation(48000.0, 2, {.mode = 1, .speed = 12, .depth = 15, .feedback = 15, .mix = 0});
    PlanarBuffer buffer{2, 9600};
    buffer.fillWithNoise(22);
    const auto input = buffer.data();
    modulation.process(buffer.view());
    CHECK(buffer.data() == input);
}

TEST_CASE("F.BACK makes decaying repeats at the delay time")
{
    const double sampleRate = 48000.0;
    const int feedbackStep = 2; // "a typical rockabilly sound" with Slapback (SRC-001 p. 11)
    auto modulation = makeModulation(sampleRate, 1, {.mode = 5, .depth = 0, .feedback = feedbackStep, .mix = 15});

    PlanarBuffer buffer{1, 12000};
    buffer.channel(0)[0] = 1.0f;
    modulation.process(buffer.view());

    const auto& y = buffer.data()[0];
    const auto delay = static_cast<std::size_t>(millisecondsToSamples(75.0, sampleRate)); // 3600, a whole number
    const double amount = mapping::feedbackAmount(feedbackStep, modulationProfile.feedbackMaximum);
    CHECK_THAT(y[delay], WithinAbs(1.0, 1.0e-6));
    CHECK_THAT(y[2 * delay], WithinAbs(amount, 1.0e-6));
    CHECK_THAT(y[3 * delay], WithinAbs(amount * amount, 1.0e-6));
}

TEST_CASE("Maximum feedback stays bounded in every mode")
{
    const int mode = GENERATE(1, 2, 3, 4, 5);
    auto modulation = makeModulation(44100.0, 2, {.mode = mode, .speed = 15, .depth = 15, .feedback = 15, .mix = 15});
    PlanarBuffer buffer{2, 88200};
    buffer.fillWithNoise(23);
    for (int channel = 0; channel < 2; ++channel)
        for (auto& sample : buffer.channel(channel))
            sample *= 0.5f;
    modulation.process(buffer.view());

    // Feedback g ≤ 0.9 gains at most 1/(1 − g) = 10; plus the dry path, then margin.
    CHECK(fivea::test::allFinite(buffer));
    CHECK(fivea::test::peak(buffer) < 0.5f * (1.0f + 10.0f) * 1.5f);
}

TEST_CASE("Changing MODE, SPEED, DEPTH, F.BACK or MIX does not click")
{
    CHECK(clickRatio({.mode = 3, .depth = 8, .feedback = 4, .mix = 12},
                     {.mode = 1, .depth = 8, .feedback = 4, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 1, .depth = 8, .feedback = 4, .mix = 12},
                     {.mode = 5, .depth = 8, .feedback = 4, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 3, .speed = 0, .depth = 15, .feedback = 4, .mix = 12},
                     {.mode = 3, .speed = 15, .depth = 15, .feedback = 4, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 3, .speed = 8, .depth = 0, .feedback = 4, .mix = 12},
                     {.mode = 3, .speed = 8, .depth = 15, .feedback = 4, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 1, .depth = 8, .feedback = 0, .mix = 12},
                     {.mode = 1, .depth = 8, .feedback = 15, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 3, .depth = 8, .feedback = 4, .mix = 0},
                     {.mode = 3, .depth = 8, .feedback = 4, .mix = 15}) < clickRatioBound);
}

TEST_CASE("Silence stays silent; extreme settings and every rate stay finite")
{
    const double sampleRate = GENERATE(44100.0, 48000.0, 88200.0, 96000.0, 192000.0);
    auto modulation = makeModulation(sampleRate, 2, {.mode = 99, .speed = 99, .depth = 99, .feedback = 99, .mix = 99});

    PlanarBuffer silence{2, 9600};
    modulation.process(silence.view());
    CHECK(fivea::test::peak(silence) == 0.0f);

    PlanarBuffer noise{2, 9600};
    noise.fillWithNoise(24);
    modulation.process(noise.view());
    modulation.process(noise.viewOf(0, 0));
    CHECK(fivea::test::allFinite(noise));
}

TEST_CASE("Modulation output does not depend on block size, and reset makes it repeatable")
{
    auto render = [](int blockSize)
    {
        auto modulation = makeModulation(96000.0, 2, {.mode = 2, .speed = 9, .depth = 11, .feedback = 7, .mix = 13});
        PlanarBuffer buffer{2, 19200};
        buffer.fillWithNoise(25);
        for (int begin = 0; begin < buffer.getNumSamples(); begin += blockSize)
        {
            if (begin == 9600)
                modulation.setSettings({.mode = 4, .speed = 3, .depth = 6, .feedback = 2, .mix = 9});
            modulation.process(buffer.viewOf(begin, std::min(blockSize, buffer.getNumSamples() - begin)));
        }
        return buffer.data();
    };

    const auto reference = render(1);
    CHECK(render(64) == reference);
    CHECK(render(480) == reference);
}

TEST_CASE("Modulation does not allocate while processing or changing settings")
{
    auto modulation = makeModulation(48000.0, 2, {});
    PlanarBuffer buffer{2, 256};
    buffer.fillWithNoise(26);

    std::size_t allocations = 0;
    {
        const fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 200; ++block)
        {
            modulation.setSettings({.mode = 1 + block % 5,
                                    .speed = block % 16,
                                    .depth = 15 - block % 16,
                                    .feedback = block % 16,
                                    .mix = block % 16});
            modulation.process(buffer.view());
        }
        modulation.reset();
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}
