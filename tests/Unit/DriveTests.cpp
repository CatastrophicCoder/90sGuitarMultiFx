#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/FactoryPrograms.h"
#include "core/ModelProfile.h"
#include "core/StepMapping.h"
#include "dsp/Drive.h"
#include "dsp/Waveshapers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace fivea;
using namespace fivea::dsp;
using fivea::test::PlanarBuffer;

namespace
{
Drive makeDrive(double sampleRate, int numChannels, const Drive::Settings& settings, int oversampling = 1)
{
    Drive drive;
    drive.prepare(sampleRate, numChannels, functionalPlaceholderProfile, oversampling);
    drive.setSettings(settings);
    drive.reset();
    return drive;
}

// Harmonic amplitudes 1…count of a sine at `fundamental` after the drive (index 0 = fundamental).
std::vector<double> harmonics(Drive& drive, double sampleRate, double fundamental, int count)
{
    PlanarBuffer buffer{1, 65536};
    fivea::test::fillSine(buffer, fundamental, sampleRate, 0.25f);
    drive.process(buffer.view());

    std::vector<double> amplitudes;
    for (int k = 1; k <= count; ++k)
        amplitudes.push_back(fivea::test::toneAmplitude(buffer.data()[0], k * fundamental, sampleRate, 8192, 57344));
    return amplitudes;
}

double distortionRatio(const std::vector<double>& amplitudes)
{
    double sum = 0.0;
    for (std::size_t k = 1; k < amplitudes.size(); ++k)
        sum += amplitudes[k] * amplitudes[k];
    return std::sqrt(sum) / amplitudes[0];
}

// A click is a transition far sharper than the signal on either side of it: the largest second
// difference in the 100 ms after a change, against the largest in the settled signal before and
// after. Measured 2026-10-05 (50 Hz, 0.05 amplitude, 48 kHz): ramped changes 1.5 (MODE), 4.8
// (DRIVE), 22 (TONE), 24 (LEVEL; the corners of a linear ramp); the same changes unramped 334 to
// 22 064. Do not raise the bound without asking.
constexpr double clickRatioBound = 100.0;

double clickRatio(const Drive::Settings& before, const Drive::Settings& after)
{
    const double sampleRate = 48000.0;
    auto drive = makeDrive(sampleRate, 1, before);
    PlanarBuffer buffer{1, 48000};
    fivea::test::fillSine(buffer, 50.0, sampleRate, 0.05f);

    const int change = 24240; // a sine peak
    drive.process(buffer.viewOf(0, change));
    drive.setSettings(after);
    drive.process(buffer.viewOf(change, buffer.getNumSamples() - change));

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
    const double transition = largestBetween(change, change + 4800);
    const double settled = std::max(largestBetween(change - 9600, change), largestBetween(48000 - 9600, 48000));
    return transition / settled;
}
} // namespace

// --- Waveshapers ---------------------------------------------------------------------------------

TEST_CASE("Every waveshaper is bounded, passes zero and never decreases")
{
    for (const auto type : {WaveshaperType::CubicSoft, WaveshaperType::RationalSoft, WaveshaperType::PiecewiseLinear,
                            WaveshaperType::AsymmetricPiecewise, WaveshaperType::Hard})
    {
        INFO("waveshaper " << static_cast<int>(type));
        CHECK(waveshapers::shape(type, 0.0f) == 0.0f);

        float previous = waveshapers::shape(type, -100.0f);
        for (float x = -100.0f; x <= 100.0f; x += 0.01f)
        {
            const float y = waveshapers::shape(type, x);
            REQUIRE(std::abs(y) <= 1.0f);
            REQUIRE(y >= previous);
            REQUIRE(y - previous < 0.02f); // continuous: no jumps
            previous = y;
        }
    }
}

TEST_CASE("The asymmetric waveshaper clips lower on its negative side")
{
    CHECK(waveshapers::asymmetricPiecewise(100.0f) == 1.0f);
    CHECK(waveshapers::asymmetricPiecewise(-100.0f) == -waveshapers::asymmetricNegativeLimit);
    CHECK(waveshapers::cubicSoft(-0.3f) == -waveshapers::cubicSoft(0.3f));
}

// --- The block -----------------------------------------------------------------------------------

TEST_CASE("Drive latency is the oversampler's: none without oversampling")
{
    CHECK(makeDrive(48000.0, 1, {}, 1).getLatencySamples() == 0);
    CHECK(makeDrive(48000.0, 1, {}, 2).getLatencySamples() == 69);
    CHECK(makeDrive(48000.0, 1, {}, 4).getLatencySamples() == 76);
}

TEST_CASE("Silence in gives silence out, in both modes and at every oversampling factor")
{
    const int mode = GENERATE(1, 2);
    const int factor = GENERATE(1, 2, 4);
    auto drive = makeDrive(44100.0, 2, {.mode = mode, .drive = 15, .tone = 15, .level = 15}, factor);
    PlanarBuffer buffer{2, 4410};
    drive.process(buffer.view());
    CHECK(fivea::test::peak(buffer) == 0.0f);
}

TEST_CASE("Output stays bounded however hard the drive is pushed")
{
    const int mode = GENERATE(1, 2);
    auto drive = makeDrive(48000.0, 2, {.mode = mode, .drive = 15, .tone = 15, .level = 15}, 2);
    PlanarBuffer buffer{2, 48000};
    buffer.fillWithNoise(11);
    for (int channel = 0; channel < 2; ++channel)
        for (auto& sample : buffer.channel(channel))
            sample *= 10.0f;
    drive.process(buffer.view());

    // The curves stop at ±1; filters after them may overshoot a little. Then trim and LEVEL 15.
    const auto& profile = mode == 1 ? functionalPlaceholderProfile.distortion : functionalPlaceholderProfile.overdrive;
    const double ceiling = 1.5 * std::pow(10.0, mapping::driveOutputTrimDb(15, profile) / 20.0) *
                           mapping::levelGain(15, functionalPlaceholderProfile.level);
    CHECK(fivea::test::allFinite(buffer));
    CHECK(fivea::test::peak(buffer) < ceiling);
}

TEST_CASE("More DRIVE means more distortion")
{
    const int mode = GENERATE(1, 2);
    double previous = 0.0;
    for (const int step : {0, 5, 10, 15})
    {
        auto drive = makeDrive(44100.0, 1, {.mode = mode, .drive = step, .tone = 15}, 2);
        const double ratio = distortionRatio(harmonics(drive, 44100.0, 220.0, 10));
        INFO("mode " << mode << ", DRIVE " << step << ": harmonics/fundamental " << ratio);
        CHECK(ratio > previous);
        previous = ratio;
    }
}

TEST_CASE("Distortion is asymmetric (even harmonics); overdrive is symmetric")
{
    auto distortion = makeDrive(44100.0, 1, {.mode = 1, .drive = 8, .tone = 15}, 2);
    auto overdrive = makeDrive(44100.0, 1, {.mode = 2, .drive = 8, .tone = 15}, 2);
    const auto d = harmonics(distortion, 44100.0, 220.0, 3);
    const auto o = harmonics(overdrive, 44100.0, 220.0, 3);

    INFO("2nd/1st: distortion " << d[1] / d[0] << ", overdrive " << o[1] / o[0]);
    CHECK(d[1] / d[0] > 0.01);
    CHECK(o[1] / o[0] < 0.001);
}

TEST_CASE("No DC offset reaches the output, even from the asymmetric curve")
{
    auto drive = makeDrive(44100.0, 1, {.mode = 1, .drive = 15, .tone = 15, .level = 12}, 2);
    PlanarBuffer buffer{1, 88200};
    fivea::test::fillSine(buffer, 220.0, 44100.0, 0.25f);
    drive.process(buffer.view());

    // Mean over the second second, a whole number of 220 Hz periods: the asymmetric clipper's
    // offset is a sizeable fraction of its output unless the DC blocker removes it.
    const auto& y = buffer.data()[0];
    double sum = 0.0;
    for (std::size_t n = 44100; n < 88200; ++n)
        sum += y[n];
    CHECK(std::abs(sum / 44100.0) < 1.0e-3);
}

TEST_CASE("TONE brightens")
{
    const int mode = GENERATE(1, 2);
    auto dark = makeDrive(44100.0, 1, {.mode = mode, .drive = 12, .tone = 0}, 2);
    auto bright = makeDrive(44100.0, 1, {.mode = mode, .drive = 12, .tone = 15}, 2);
    const auto d = harmonics(dark, 44100.0, 220.0, 30);
    const auto b = harmonics(bright, 44100.0, 220.0, 30);

    double darkHigh = 0.0;
    double brightHigh = 0.0;
    for (std::size_t k = 14; k < 30; ++k) // harmonics 15–30: 3.3–6.6 kHz
    {
        darkHigh += d[k] * d[k];
        brightHigh += b[k] * b[k];
    }
    CHECK(10.0 * std::log10(brightHigh / darkHigh) > 6.0);
}

TEST_CASE("Oversampling reduces aliasing")
{
    // A 3101 Hz tone at 44.1 kHz, heavily distorted: harmonics above 22.05 kHz fold back to
    // frequencies that are not harmonics. Measured 2026-10-05: alias sum 0.286 without
    // oversampling, 0.092 at 2×, 1.2e-5 at 4×.
    const double sampleRate = 44100.0;
    const double fundamental = 3101.0;

    auto aliasSum = [&](int factor)
    {
        auto drive = makeDrive(sampleRate, 1, {.mode = 1, .drive = 15, .tone = 15}, factor);
        PlanarBuffer buffer{1, 65536};
        fivea::test::fillSine(buffer, fundamental, sampleRate, 0.25f);
        drive.process(buffer.view());

        double sum = 0.0;
        for (int k = 8; k <= 40; ++k)
        {
            double folded = std::fmod(k * fundamental, sampleRate);
            if (folded > sampleRate / 2.0)
                folded = sampleRate - folded;
            constexpr int inBandHarmonics[] = {1, 2, 3, 4, 5, 6, 7};
            const bool nearHarmonic = std::any_of(std::begin(inBandHarmonics), std::end(inBandHarmonics),
                                                  [&](int j)
                                                  {
                                                      return std::abs(folded - j * fundamental) < 50.0;
                                                  });
            if (!nearHarmonic)
                sum += fivea::test::toneAmplitude(buffer.data()[0], folded, sampleRate, 8192, 57344);
        }
        return sum;
    };

    const double without = aliasSum(1);
    INFO("alias sum: 1× " << without << ", 2× " << aliasSum(2) << ", 4× " << aliasSum(4));
    CHECK(aliasSum(2) < 0.5 * without);
    CHECK(aliasSum(4) < 1.0e-3 * without);
}

TEST_CASE("Changing MODE, DRIVE, TONE or LEVEL does not click")
{
    CHECK(clickRatio({.mode = 2, .drive = 8, .tone = 8}, {.mode = 1, .drive = 8, .tone = 8}) < clickRatioBound);
    CHECK(clickRatio({.mode = 1, .drive = 8, .tone = 8}, {.mode = 2, .drive = 8, .tone = 8}) < clickRatioBound);
    CHECK(clickRatio({.mode = 2, .drive = 0, .tone = 8}, {.mode = 2, .drive = 15, .tone = 8}) < clickRatioBound);
    CHECK(clickRatio({.mode = 1, .drive = 8, .tone = 0}, {.mode = 1, .drive = 8, .tone = 15}) < clickRatioBound);
    CHECK(clickRatio({.mode = 2, .drive = 4, .tone = 8, .level = 1}, {.mode = 2, .drive = 4, .tone = 8, .level = 15}) <
          clickRatioBound);
}

TEST_CASE("Switching MODE back during a fade does not jump")
{
    auto drive = makeDrive(48000.0, 1, {.mode = 2});
    PlanarBuffer buffer{1, 9600};
    fivea::test::fillSine(buffer, 50.0, 48000.0, 0.05f);

    drive.process(buffer.viewOf(0, 4000));
    drive.setSettings({.mode = 1});
    drive.process(buffer.viewOf(4000, 300)); // part-way through the 20 ms fade
    drive.setSettings({.mode = 2});
    drive.process(buffer.viewOf(4300, 9600 - 4300));

    CHECK(fivea::test::allFinite(buffer));
    CHECK(fivea::test::largestStep(buffer, 0) < 0.01);
}

TEST_CASE("Drive runs at every sample rate, and stays finite at extreme settings")
{
    const double sampleRate = GENERATE(44100.0, 48000.0, 88200.0, 96000.0, 192000.0);
    const int factor = GENERATE(1, 4);
    auto drive = makeDrive(sampleRate, 2, {.mode = 99, .drive = 99, .tone = -9, .level = 99}, factor);
    PlanarBuffer buffer{2, 8192};
    buffer.fillWithNoise(12);
    drive.process(buffer.view());
    drive.process(buffer.viewOf(0, 0));
    CHECK(fivea::test::allFinite(buffer));
}

TEST_CASE("Drive output does not depend on block size, and reset makes it repeatable")
{
    auto render = [](int blockSize)
    {
        auto drive = makeDrive(96000.0, 2, {.mode = 2, .drive = 6, .tone = 4, .level = 11}, 2);
        PlanarBuffer buffer{2, 9600};
        buffer.fillWithNoise(13);
        for (int begin = 0; begin < buffer.getNumSamples(); begin += blockSize)
        {
            if (begin == 4800)
                drive.setSettings({.mode = 1, .drive = 12, .tone = 13, .level = 12});
            drive.process(buffer.viewOf(begin, std::min(blockSize, buffer.getNumSamples() - begin)));
        }
        return buffer.data();
    };

    const auto reference = render(1);
    CHECK(render(64) == reference);
    CHECK(render(480) == reference);
}

TEST_CASE("Drive does not allocate while processing or changing settings")
{
    auto drive = makeDrive(48000.0, 2, {}, 4);
    PlanarBuffer buffer{2, 256};
    buffer.fillWithNoise(14);

    std::size_t allocations = 0;
    {
        const fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 100; ++block)
        {
            drive.setSettings({.mode = 1 + block % 2, .drive = block % 16, .tone = 15 - block % 16, .level = 12});
            drive.process(buffer.view());
        }
        drive.reset();
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}

TEST_CASE("At LEVEL 12 the drive is about as loud as the dry guitar, at every DRIVE step")
{
    // The trim follows DRIVE (DriveProfile::outputTrimDbPerDrive), so that in front of an amp the
    // block behaves like a pedal at unity rather than a +25 dB boost (found 2026-10-07: the first
    // placeholder trims made the drive programs 17 to 25 dB louder than the guitar).
    const double sampleRate = GENERATE(44100.0, 96000.0);
    const int factor = GENERATE(1, 4);
    for (const int mode : {1, 2})
        for (int step = 0; step <= 15; ++step)
        {
            PlanarBuffer dry{1, static_cast<int>(sampleRate)};
            fivea::test::fillPluck(dry, sampleRate);
            auto wet = dry;
            auto drive = makeDrive(sampleRate, 1, {.mode = mode, .drive = step, .tone = 8, .level = 12}, factor);
            drive.process(wet.view());

            const int length = dry.getNumSamples();
            const double differenceDb =
                20.0 * std::log10(fivea::test::rms(wet, 0, 0, length) / fivea::test::rms(dry, 0, 0, length));
            INFO(sampleRate << " Hz, " << factor << "x, mode " << mode << ", DRIVE " << step << ": " << differenceDb
                            << " dB");
            CHECK(std::abs(differenceDb) < 3.0);
        }
}

namespace
{
// Aliasing, measured exactly: a sine of `cycles` cycles in one N-sample period, so the settled
// output repeats every N samples and its DFT needs no window. Every harmonic falls on a multiple of
// `cycles`; an odd `cycles` and a power-of-two N keep every folded (aliased) harmonic off those
// bins. Returns the energy off the harmonic bins, relative to all of it, in dB.
double aliasingDb(Drive& drive, int cycles)
{
    constexpr int period = 4096;
    PlanarBuffer buffer{1, 4 * period};
    auto& samples = buffer.channel(0);
    for (std::size_t n = 0; n < samples.size(); ++n)
        samples[n] = 0.1f * static_cast<float>(
                                std::sin(2.0 * std::numbers::pi * cycles * static_cast<double>(n % period) / period));
    drive.process(buffer.view());

    std::vector<double> cosines(period);
    std::vector<double> sines(period);
    for (int n = 0; n < period; ++n)
    {
        cosines[static_cast<std::size_t>(n)] = std::cos(2.0 * std::numbers::pi * n / period);
        sines[static_cast<std::size_t>(n)] = std::sin(2.0 * std::numbers::pi * n / period);
    }
    const auto* settled = samples.data() + samples.size() - period;
    double total = 0.0;
    double harmonic = 0.0;
    for (int bin = 1; bin < period / 2; ++bin)
    {
        double real = 0.0;
        double imaginary = 0.0;
        for (int n = 0; n < period; ++n)
        {
            const auto phase = static_cast<std::size_t>((static_cast<long long>(bin) * n) % period);
            real += settled[n] * cosines[phase];
            imaginary -= settled[n] * sines[phase];
        }
        const double power = real * real + imaginary * imaginary;
        total += power;
        if (bin % cycles == 0)
            harmonic += power;
    }
    return 10.0 * std::log10((total - harmonic) / total);
}
} // namespace

TEST_CASE("Oversampling keeps the Distortion programs' aliasing down")
{
    // 113 cycles in 4096 samples: about 1324 Hz at 48 kHz, a high note on the B string. Measured
    // 2026-10-07 for 2-1, 2-3, 2-5, 6-2, 6-3: −23 to −30 dB without oversampling (heard in front of
    // an amp sim as a whistling, feedback-like tone), −39 to −60 dB at 4×, the default since then.
    // METAL 1 (2-1) is the worst at −39.4. Do not loosen the bound without asking.
    constexpr double aliasingBoundDb = -37.0;
    const auto slots = fivea::factoryPrograms();
    for (const int slot : {5, 7, 9, 26, 27})
    {
        const auto& program = slots[static_cast<std::size_t>(slot)];
        REQUIRE(program.drive.mode == 1);
        auto off = makeDrive(48000.0, 1, program.drive, 1);
        auto oversampled = makeDrive(48000.0, 1, program.drive, 4);
        const double withoutDb = aliasingDb(off, 113);
        const double withDb = aliasingDb(oversampled, 113);
        INFO(program.name.view() << ": " << withoutDb << " dB off, " << withDb << " dB at 4x");
        CHECK(withDb < aliasingBoundDb);
        CHECK(withDb < withoutDb - 10.0);
    }
}
