#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/ModelProfile.h"
#include "core/StepMapping.h"
#include "dsp/Biquad.h"
#include "dsp/ThreeBandEq.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using namespace fivea;
using namespace fivea::dsp;
using Catch::Matchers::WithinAbs;
using fivea::test::PlanarBuffer;

namespace
{
const EqProfile& eqProfile = functionalPlaceholderProfile.eq;

ThreeBandEq makeEq(double sampleRate, int numChannels, const ThreeBandEq::Settings& settings)
{
    ThreeBandEq eq;
    eq.prepare(sampleRate, numChannels, functionalPlaceholderProfile);
    eq.setSettings(settings);
    eq.reset(); // settle every ramp on these settings
    return eq;
}

// Steady-state gain of the EQ for a sine at `frequency`, in dB, from the RMS after settling.
double measuredGainDb(ThreeBandEq& eq, double sampleRate, double frequency)
{
    const int settle = static_cast<int>(sampleRate * 0.5);
    const int measure = static_cast<int>(sampleRate * 0.5);

    PlanarBuffer buffer{1, settle + measure};
    fivea::test::fillSine(buffer, frequency, sampleRate, 0.25f);
    const double inputRms = fivea::test::rms(buffer, 0, settle, measure);

    eq.process(buffer.view());
    return 20.0 * std::log10(fivea::test::rms(buffer, 0, settle, measure) / inputRms);
}

double designGainDb(const ThreeBandEq::Settings& settings, double sampleRate, double frequency)
{
    using namespace fivea::mapping;
    double gainDb = 20.0 * std::log10(eqTrimGain(settings.trim, eqProfile));
    gainDb += magnitudeDb(
        design::lowShelf(sampleRate, eqBassFrequencyHz(), eqBandGainDb(settings.bass, eqProfile), eqProfile.shelfSlope),
        frequency, sampleRate);
    gainDb += magnitudeDb(design::peaking(sampleRate, eqMidFrequencyHz(settings.midFrequency), eqProfile.midQ,
                                          eqBandGainDb(settings.mid, eqProfile)),
                          frequency, sampleRate);
    gainDb += magnitudeDb(design::highShelf(sampleRate, eqTrebleFrequencyHz(), eqBandGainDb(settings.treble, eqProfile),
                                            eqProfile.shelfSlope),
                          frequency, sampleRate);
    return gainDb;
}

constexpr double sampleRates[] = {44100.0, 48000.0, 88200.0, 96000.0, 192000.0};
} // namespace

// --- The filter designs (RBJ Audio EQ Cookbook) ------------------------------------------------

TEST_CASE("Shelves reach their gain on the plateau and half of it at the corner")
{
    const double sampleRate = GENERATE(44100.0, 48000.0, 96000.0, 192000.0);
    const double gainDb = GENERATE(-10.5, -3.0, 4.5, 10.5);

    const auto low = design::lowShelf(sampleRate, 100.0, gainDb, 1.0);
    CHECK_THAT(magnitudeDb(low, 0.0, sampleRate), WithinAbs(gainDb, 1.0e-6));
    CHECK_THAT(magnitudeDb(low, 100.0, sampleRate), WithinAbs(gainDb / 2.0, 1.0e-6));
    CHECK_THAT(magnitudeDb(low, 20000.0, sampleRate), WithinAbs(0.0, 0.01));

    const auto high = design::highShelf(sampleRate, 3000.0, gainDb, 1.0);
    CHECK_THAT(magnitudeDb(high, sampleRate / 2.0, sampleRate), WithinAbs(gainDb, 1.0e-6));
    CHECK_THAT(magnitudeDb(high, 3000.0, sampleRate), WithinAbs(gainDb / 2.0, 1.0e-6));
    CHECK_THAT(magnitudeDb(high, 20.0, sampleRate), WithinAbs(0.0, 0.01));
}

TEST_CASE("The peaking filter reaches its gain exactly at the centre frequency")
{
    const double sampleRate = GENERATE(44100.0, 48000.0, 96000.0, 192000.0);
    for (const float centre : documented::eqMidFrequenciesHz)
    {
        const auto peak = design::peaking(sampleRate, centre, 0.7, 10.5);
        CHECK_THAT(magnitudeDb(peak, centre, sampleRate), WithinAbs(10.5, 1.0e-6));
        CHECK(magnitudeDb(peak, centre * 0.5, sampleRate) < 10.5);
        CHECK(magnitudeDb(peak, centre * 2.0, sampleRate) < 10.5);
    }
}

namespace
{
// The cookbook's analog prototypes, written out independently of the digital formulas, evaluated
// at the bilinear transform's warped frequency Ω = tan(πf/fs) / tan(πf0/fs). A digital design equals
// its prototype there exactly, so this checks every coefficient, including Q and the shelf slope.
using Complex = std::complex<double>;

double warped(double frequency, double centre, double sampleRate)
{
    return std::tan(std::numbers::pi * frequency / sampleRate) / std::tan(std::numbers::pi * centre / sampleRate);
}

double analogPeakingDb(double omega, double q, double gainDb)
{
    const double a = std::pow(10.0, gainDb / 40.0);
    const Complex s{0.0, omega};
    return 20.0 * std::log10(std::abs((s * s + s * (a / q) + 1.0) / (s * s + s / (a * q) + 1.0)));
}

double shelfInverseQ(double a, double slope)
{
    return std::sqrt((a + 1.0 / a) * (1.0 / slope - 1.0) + 2.0);
}

double analogLowShelfDb(double omega, double slope, double gainDb)
{
    const double a = std::pow(10.0, gainDb / 40.0);
    const double k = std::sqrt(a) * shelfInverseQ(a, slope);
    const Complex s{0.0, omega};
    return 20.0 * std::log10(std::abs(a * (s * s + k * s + a) / (a * s * s + k * s + 1.0)));
}

double analogHighShelfDb(double omega, double slope, double gainDb)
{
    const double a = std::pow(10.0, gainDb / 40.0);
    const double k = std::sqrt(a) * shelfInverseQ(a, slope);
    const Complex s{0.0, omega};
    return 20.0 * std::log10(std::abs(a * (a * s * s + k * s + 1.0) / (s * s + k * s + a)));
}
} // namespace

TEST_CASE("Designs equal the cookbook's analog prototypes under the bilinear transform")
{
    const double sampleRate = GENERATE(44100.0, 96000.0, 192000.0);
    const double gainDb = GENERATE(-10.5, 6.0);
    const double slope = GENERATE(0.5, 1.0);
    const double q = GENERATE(0.5, 0.7, 2.0);

    for (double frequency = 20.0; frequency < sampleRate * 0.45; frequency *= 1.3)
    {
        INFO("fs " << sampleRate << ", f " << frequency);
        CHECK_THAT(magnitudeDb(design::peaking(sampleRate, 800.0, q, gainDb), frequency, sampleRate),
                   WithinAbs(analogPeakingDb(warped(frequency, 800.0, sampleRate), q, gainDb), 1.0e-6));
        CHECK_THAT(magnitudeDb(design::lowShelf(sampleRate, 100.0, gainDb, slope), frequency, sampleRate),
                   WithinAbs(analogLowShelfDb(warped(frequency, 100.0, sampleRate), slope, gainDb), 1.0e-6));
        CHECK_THAT(magnitudeDb(design::highShelf(sampleRate, 3000.0, gainDb, slope), frequency, sampleRate),
                   WithinAbs(analogHighShelfDb(warped(frequency, 3000.0, sampleRate), slope, gainDb), 1.0e-6));
    }
}

TEST_CASE("Cut mirrors boost at every frequency")
{
    const double sampleRate = 48000.0;
    for (double frequency = 20.0; frequency < 20000.0; frequency *= 1.25)
    {
        CHECK_THAT(magnitudeDb(design::lowShelf(sampleRate, 100.0, 9.0, 1.0), frequency, sampleRate) +
                       magnitudeDb(design::lowShelf(sampleRate, 100.0, -9.0, 1.0), frequency, sampleRate),
                   WithinAbs(0.0, 1.0e-6));
        CHECK_THAT(magnitudeDb(design::peaking(sampleRate, 800.0, 0.7, 9.0), frequency, sampleRate) +
                       magnitudeDb(design::peaking(sampleRate, 800.0, 0.7, -9.0), frequency, sampleRate),
                   WithinAbs(0.0, 1.0e-6));
        CHECK_THAT(magnitudeDb(design::highShelf(sampleRate, 3000.0, 9.0, 1.0), frequency, sampleRate) +
                       magnitudeDb(design::highShelf(sampleRate, 3000.0, -9.0, 1.0), frequency, sampleRate),
                   WithinAbs(0.0, 1.0e-6));
    }
}

// --- The EQ block --------------------------------------------------------------------------------

TEST_CASE("Measured EQ response matches its design within 0.1 dB at every sample rate")
{
    const double sampleRate = GENERATE(from_range(sampleRates));
    const ThreeBandEq::Settings settings{.bass = 6, .midFrequency = 3, .mid = 5, .treble = 1, .trim = 12};

    for (const double frequency : {50.0, 100.0, 250.0, 800.0, 1500.0, 3000.0, 8000.0})
    {
        INFO("sample rate " << sampleRate << ", frequency " << frequency);
        auto eq = makeEq(sampleRate, 1, settings);
        CHECK_THAT(measuredGainDb(eq, sampleRate, frequency),
                   WithinAbs(designGainDb(settings, sampleRate, frequency), 0.1));
    }
}

TEST_CASE("MID FREQ moves the middle band to each documented frequency")
{
    const double sampleRate = 48000.0;
    for (int step = 1; step <= 8; ++step)
    {
        const ThreeBandEq::Settings settings{.midFrequency = step, .mid = 7};
        const float centre = mapping::eqMidFrequencyHz(step);
        auto eq = makeEq(sampleRate, 1, settings);
        INFO("MID FREQ " << step << " = " << centre << " Hz");
        CHECK_THAT(measuredGainDb(eq, sampleRate, centre), WithinAbs(mapping::eqBandGainDb(7, eqProfile), 0.1));
    }
}

TEST_CASE("TRIM is an input gain, unity at its top step")
{
    const double sampleRate = 48000.0;
    auto atTop = makeEq(sampleRate, 1, {.trim = 15});
    CHECK_THAT(measuredGainDb(atTop, sampleRate, 1000.0), WithinAbs(0.0, 0.01));

    auto lowered = makeEq(sampleRate, 1, {.trim = 11});
    CHECK_THAT(measuredGainDb(lowered, sampleRate, 1000.0),
               WithinAbs(20.0 * std::log10(mapping::eqTrimGain(11, eqProfile)), 0.01));
}

TEST_CASE("An EQ flat since reset, with TRIM at its top, passes audio through bit for bit")
{
    const double sampleRate = GENERATE(from_range(sampleRates));
    const int numChannels = GENERATE(1, 2);
    auto eq = makeEq(sampleRate, numChannels, {.trim = 15});

    PlanarBuffer buffer{numChannels, 4096};
    buffer.fillWithNoise(1);
    const auto input = buffer.data();

    for (int start = 0; start < buffer.getNumSamples(); start += 64)
        eq.process(buffer.viewOf(start, 64));

    CHECK(buffer.data() == input);
}

namespace
{
// Click bound on the largest second difference, for a 0.25-amplitude 50 Hz sine with the changes
// at sine peaks (a change at a zero crossing hides a gain jump). Measured 2026-10-05 with the
// stepped changes below: 0.049 with the 20 ms ramp, 0.61 with the ramp removed. Do not raise it
// without asking.
constexpr double clickBound = 0.15;

// 50 Hz at 48 kHz peaks every 960 samples, at 240 + k × 960.
constexpr int firstPeakAfterQuarter = 12240;
constexpr int firstPeakAfterHalf = 24240;
} // namespace

TEST_CASE("Stepped changes do not click")
{
    const double sampleRate = 48000.0;
    auto eq = makeEq(sampleRate, 1, {.bass = -7, .midFrequency = 1, .mid = -7, .treble = -7});

    PlanarBuffer buffer{1, static_cast<int>(sampleRate)};
    fivea::test::fillSine(buffer, 50.0, sampleRate, 0.25f);

    // Jump every control from one end to the other, twice, mid-stream, at sine peaks.
    eq.process(buffer.viewOf(0, firstPeakAfterQuarter));
    eq.setSettings({.bass = 7, .midFrequency = 8, .mid = 7, .treble = 7, .trim = 9});
    eq.process(buffer.viewOf(firstPeakAfterQuarter, firstPeakAfterHalf - firstPeakAfterQuarter));
    eq.setSettings({.bass = -7, .midFrequency = 1, .mid = -7, .treble = -7, .trim = 15});
    eq.process(buffer.viewOf(firstPeakAfterHalf, buffer.getNumSamples() - firstPeakAfterHalf));

    CHECK(fivea::test::largestSecondDifference(buffer, 0) < clickBound);
    CHECK(fivea::test::allFinite(buffer));
}

TEST_CASE("Returning to flat ends without a click, and the EQ is then transparent")
{
    const double sampleRate = 48000.0;
    auto eq = makeEq(sampleRate, 1, {.bass = 7, .mid = 7, .treble = 7});

    PlanarBuffer buffer{1, static_cast<int>(sampleRate * 2)};
    fivea::test::fillSine(buffer, 50.0, sampleRate, 0.25f);
    const auto input = buffer.data();

    const int change = 48240; // a sine peak
    eq.process(buffer.viewOf(0, change));
    eq.setSettings({});
    eq.process(buffer.viewOf(change, buffer.getNumSamples() - change));

    CHECK(fivea::test::largestSecondDifference(buffer, 0) < clickBound);

    // Flat is transparent to far below audibility. Not bit-exact once the filters have been used:
    // rounding leaves a residue around 1e-14 in their state. Bit-exact transparency is the job of
    // the bypass (plan §10.2), which does not run the block at all.
    const auto& output = buffer.data()[0];
    double largestDifference = 0.0;
    for (std::size_t n = output.size() * 3 / 4; n < output.size(); ++n)
        largestDifference = std::max(largestDifference, static_cast<double>(std::abs(output[n] - input[0][n])));
    CHECK(largestDifference < 1.0e-9);
}

TEST_CASE("After a change, the response settles on the new design")
{
    const double sampleRate = 44100.0;
    auto eq = makeEq(sampleRate, 1, {});
    const ThreeBandEq::Settings target{.bass = -4, .midFrequency = 6, .mid = 7, .treble = 3, .trim = 13};
    eq.setSettings(target);

    PlanarBuffer warmup{1, static_cast<int>(sampleRate * 0.1)};
    eq.process(warmup.view()); // longer than the 20 ms ramp

    CHECK_THAT(measuredGainDb(eq, sampleRate, 2000.0), WithinAbs(designGainDb(target, sampleRate, 2000.0), 0.1));
}

TEST_CASE("EQ output does not depend on block size, and reset makes it repeatable")
{
    const ThreeBandEq::Settings start{.bass = 3, .midFrequency = 2, .mid = -5, .treble = 6, .trim = 14};
    const ThreeBandEq::Settings changed{.bass = -6, .midFrequency = 7, .mid = 4, .treble = -2, .trim = 10};

    auto render = [&](int blockSize)
    {
        auto eq = makeEq(96000.0, 2, start);
        PlanarBuffer buffer{2, 8192};
        buffer.fillWithNoise(3);
        for (int begin = 0; begin < buffer.getNumSamples(); begin += blockSize)
        {
            if (begin >= 2048 && begin < 2048 + blockSize)
                eq.setSettings(changed);
            eq.process(buffer.viewOf(begin, std::min(blockSize, buffer.getNumSamples() - begin)));
        }
        return buffer.data();
    };

    // Settings change at the first block boundary at or after sample 2048; with these sizes that
    // is exactly 2048 for all three.
    const auto reference = render(1);
    CHECK(render(16) == reference);
    CHECK(render(512) == reference);
}

TEST_CASE("EQ stays finite and silent on silence, at extreme settings and the highest rate")
{
    auto eq = makeEq(192000.0, 2, {.bass = 99, .midFrequency = -3, .mid = 99, .treble = 99, .trim = 99});
    PlanarBuffer buffer{2, 192000};
    eq.process(buffer.view());

    CHECK(fivea::test::allFinite(buffer));
    CHECK(fivea::test::peak(buffer) == 0.0f);

    eq.process(buffer.viewOf(0, 0)); // zero-length block
    CHECK(fivea::test::allFinite(buffer));
}

TEST_CASE("EQ does not allocate while processing or changing settings")
{
    auto eq = makeEq(48000.0, 2, {});
    PlanarBuffer buffer{2, 256};
    buffer.fillWithNoise(4);

    std::size_t allocations = 0;
    {
        const fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 100; ++block)
        {
            eq.setSettings({.bass = block % 15 - 7, .midFrequency = block % 8 + 1, .mid = 7 - block % 15});
            eq.process(buffer.view());
        }
        eq.reset();
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}
