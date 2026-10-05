#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/ModelProfile.h"
#include "core/StepMapping.h"
#include "dsp/Compressor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace fivea;
using namespace fivea::dsp;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using fivea::test::PlanarBuffer;

namespace
{
const CompressorProfile& compressorProfile = functionalPlaceholderProfile.compressor;

Compressor makeCompressor(double sampleRate, int numChannels, const Compressor::Settings& settings)
{
    Compressor compressor;
    compressor.prepare(sampleRate, numChannels, functionalPlaceholderProfile);
    compressor.setSettings(settings);
    compressor.reset();
    return compressor;
}

double toDb(double linear)
{
    return 20.0 * std::log10(linear);
}

// The static curve the gain computer should implement (Giannoulis et al. 2012, eq. 4), as output
// level in dB for an input level in dB.
double staticCurveDb(double inputDb, double thresholdDb, double ratio, double kneeDb)
{
    const double over = inputDb - thresholdDb;
    if (2.0 * over < -kneeDb)
        return inputDb;
    if (2.0 * over > kneeDb)
        return thresholdDb + over / ratio;
    const double inKnee = over + kneeDb / 2.0;
    return inputDb + (1.0 / ratio - 1.0) * inKnee * inKnee / (2.0 * kneeDb);
}

// Feeds a constant level until the detector has settled; returns the output level in dB.
double settledOutputDb(Compressor& compressor, double sampleRate, double inputDb)
{
    PlanarBuffer buffer{1, static_cast<int>(sampleRate * 2.0)};
    buffer.fill(static_cast<float>(std::pow(10.0, inputDb / 20.0)));
    compressor.process(buffer.view());
    return toDb(buffer.data()[0].back());
}

constexpr double sampleRates[] = {44100.0, 48000.0, 88200.0, 96000.0, 192000.0};
} // namespace

TEST_CASE("Below the threshold the compressor is unity, apart from LEVEL")
{
    const double sampleRate = GENERATE(44100.0, 96000.0);
    auto compressor = makeCompressor(sampleRate, 2, {.sens = 4, .attack = 3, .level = 12});

    // −60 dBFS peaks, far below SENS 4's threshold.
    PlanarBuffer buffer{2, 8192};
    buffer.fillWithNoise(5);
    for (int channel = 0; channel < 2; ++channel)
        for (auto& sample : buffer.channel(channel))
            sample *= 0.001f;
    const auto input = buffer.data();

    compressor.process(buffer.view());
    CHECK(buffer.data() == input); // LEVEL 12 is unity, and no gain reduction is computed

    auto quieter = makeCompressor(sampleRate, 1, {.sens = 4, .attack = 3, .level = 9});
    CHECK_THAT(settledOutputDb(quieter, sampleRate, -60.0),
               WithinAbs(-60.0 + toDb(mapping::levelGain(9, functionalPlaceholderProfile.level)), 1.0e-4));
}

TEST_CASE("Settled output follows the static compression curve")
{
    const double sampleRate = 48000.0;
    const int sens = GENERATE(0, 5, 10, 15);
    const double thresholdDb = mapping::compressorThresholdDb(sens, compressorProfile);

    for (const double overDb : {-12.0, -3.0, -1.0, 0.0, 1.0, 3.0, 6.0, 12.0, 24.0})
    {
        const double inputDb = std::min(thresholdDb + overDb, 0.0);
        INFO("SENS " << sens << ", input " << inputDb << " dBFS");
        auto compressor = makeCompressor(sampleRate, 1, {.sens = sens, .attack = 7, .level = 12});
        CHECK_THAT(
            settledOutputDb(compressor, sampleRate, inputDb),
            WithinAbs(staticCurveDb(inputDb, thresholdDb, compressorProfile.ratio, compressorProfile.kneeDb), 0.01));
    }
}

TEST_CASE("Well above the threshold, the ratio is the profile's")
{
    const double sampleRate = 48000.0;
    auto compressor = makeCompressor(sampleRate, 1, {.sens = 15, .attack = 7, .level = 12});
    const double outA = settledOutputDb(compressor, sampleRate, -30.0);
    const double outB = settledOutputDb(compressor, sampleRate, -10.0);
    CHECK_THAT(20.0 / (outB - outA), WithinRel(static_cast<double>(compressorProfile.ratio), 1.0e-3));
}

TEST_CASE("Gain reduction rises with each ATTACK step's time constant")
{
    const double sampleRate = GENERATE(from_range(sampleRates));
    const int attack = GENERATE(0, 3, 7);
    const double attackMs = mapping::compressorAttackMs(attack, compressorProfile);

    // From silence to a loud constant level: the reduction in dB approaches its final value as
    // 1 − e^(−t/τ), so it reaches 63.2 % after one time constant.
    auto compressor = makeCompressor(sampleRate, 1, {.sens = 12, .attack = attack, .level = 12});
    const double inputDb = -6.0;
    const float input = static_cast<float>(std::pow(10.0, inputDb / 20.0));

    PlanarBuffer buffer{1, static_cast<int>(sampleRate * 1.0)};
    buffer.fill(input);
    compressor.process(buffer.view());

    const auto& out = buffer.data()[0];
    const double finalReductionDb = inputDb - toDb(out.back());
    REQUIRE(finalReductionDb > 10.0);

    const auto reached = std::find_if(out.begin(), out.end(),
                                      [&](float sample)
                                      {
                                          return inputDb - toDb(sample) >= finalReductionDb * (1.0 - std::exp(-1.0));
                                      });
    const double reachedMs = 1000.0 * static_cast<double>(reached - out.begin()) / sampleRate;

    INFO("fs " << sampleRate << ", ATTACK " << attack << ": τ " << attackMs << " ms, reached " << reachedMs << " ms");
    CHECK_THAT(reachedMs, WithinAbs(attackMs, std::max(0.02 * attackMs, 2000.0 / sampleRate)));
}

TEST_CASE("Gain reduction falls back with the release time constant")
{
    const double sampleRate = GENERATE(44100.0, 96000.0, 192000.0);
    auto compressor = makeCompressor(sampleRate, 1, {.sens = 12, .attack = 7, .level = 12});

    // Settle on a loud level, then drop far below the threshold: the reduction decays as e^(−t/τ).
    PlanarBuffer loud{1, static_cast<int>(sampleRate)};
    loud.fill(0.5f);
    compressor.process(loud.view());
    const double startReductionDb = toDb(0.5) - toDb(loud.data()[0].back());

    const float quietLevel = 0.0001f;
    PlanarBuffer quiet{1, static_cast<int>(sampleRate)};
    quiet.fill(quietLevel);
    compressor.process(quiet.view());

    const auto& out = quiet.data()[0];
    const auto reached = std::find_if(out.begin(), out.end(),
                                      [&](float sample)
                                      {
                                          return toDb(quietLevel) - toDb(sample) <= startReductionDb * std::exp(-1.0);
                                      });
    const double reachedMs = 1000.0 * static_cast<double>(reached - out.begin()) / sampleRate;
    CHECK_THAT(reachedMs,
               WithinAbs(static_cast<double>(compressorProfile.releaseMs), 0.02 * compressorProfile.releaseMs));
}

TEST_CASE("Both channels get the same gain")
{
    auto compressor = makeCompressor(48000.0, 2, {.sens = 12, .attack = 5, .level = 12});
    PlanarBuffer buffer{2, 48000};
    buffer.channel(0).assign(48000, 0.5f);   // loud: drives the reduction
    buffer.channel(1).assign(48000, 0.001f); // quiet: must be reduced by the same amount
    compressor.process(buffer.view());

    const double gainLeft = buffer.data()[0].back() / 0.5;
    const double gainRight = buffer.data()[1].back() / 0.001;
    CHECK_THAT(gainRight, WithinRel(gainLeft, 1.0e-5));
    CHECK(gainLeft < 0.5);
}

TEST_CASE("A LEVEL step does not click")
{
    const double sampleRate = 48000.0;
    auto compressor = makeCompressor(sampleRate, 1, {.sens = 0, .attack = 4, .level = 1});
    PlanarBuffer buffer{1, static_cast<int>(sampleRate)};
    fivea::test::fillSine(buffer, 50.0, sampleRate, 0.05f);

    // At a sine peak: a gain jump at a zero crossing would leave no trace.
    const int change = 24240;
    compressor.process(buffer.viewOf(0, change));
    compressor.setSettings({.sens = 0, .attack = 4, .level = 15});
    compressor.process(buffer.viewOf(change, buffer.getNumSamples() - change));

    // Largest second difference, measured 2026-10-05: 0.0001 with the 20 ms LEVEL ramp, 0.096
    // without it. Do not raise the bound without asking.
    CHECK(fivea::test::largestSecondDifference(buffer, 0) < 0.005);
}

TEST_CASE("Silence, extreme levels and extreme settings stay finite")
{
    auto compressor = makeCompressor(192000.0, 2, {.sens = 99, .attack = -5, .level = 99});

    PlanarBuffer silence{2, 19200};
    compressor.process(silence.view());
    CHECK(fivea::test::peak(silence) == 0.0f);

    PlanarBuffer extreme{2, 19200};
    extreme.fill(1.0e6f);
    extreme.channel(1)[100] = std::numeric_limits<float>::denorm_min();
    compressor.process(extreme.view());
    CHECK(fivea::test::allFinite(extreme));

    compressor.process(extreme.viewOf(0, 0));
    CHECK(fivea::test::allFinite(extreme));
}

TEST_CASE("Compressor output does not depend on block size, and reset makes it repeatable")
{
    auto render = [](int blockSize)
    {
        auto compressor = makeCompressor(96000.0, 2, {.sens = 10, .attack = 2, .level = 11});
        PlanarBuffer buffer{2, 9600};
        buffer.fillWithNoise(8);
        for (int begin = 0; begin < buffer.getNumSamples(); begin += blockSize)
        {
            if (begin == 4800)
                compressor.setSettings({.sens = 14, .attack = 6, .level = 13});
            compressor.process(buffer.viewOf(begin, std::min(blockSize, buffer.getNumSamples() - begin)));
        }
        return buffer.data();
    };

    const auto reference = render(1);
    CHECK(render(64) == reference);
    CHECK(render(480) == reference);
}

TEST_CASE("Compressor does not allocate while processing or changing settings")
{
    auto compressor = makeCompressor(48000.0, 2, {});
    PlanarBuffer buffer{2, 256};
    buffer.fillWithNoise(9);

    std::size_t allocations = 0;
    {
        const fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 100; ++block)
        {
            compressor.setSettings({.sens = block % 16, .attack = block % 8, .level = 15 - block % 16});
            compressor.process(buffer.view());
        }
        compressor.reset();
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}
