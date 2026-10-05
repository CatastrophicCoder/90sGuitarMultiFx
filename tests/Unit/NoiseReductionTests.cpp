#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/ModelProfile.h"
#include "dsp/NoiseReduction.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

using namespace fivea;
using namespace fivea::dsp;
using Catch::Matchers::WithinAbs;
using fivea::test::PlanarBuffer;

namespace
{
NoiseReduction makeNoiseReduction(double sampleRate, int numChannels, int level)
{
    NoiseReduction noiseReduction;
    noiseReduction.prepare(sampleRate, numChannels, functionalPlaceholderProfile);
    noiseReduction.setLevel(level);
    noiseReduction.reset();
    return noiseReduction;
}

void scale(PlanarBuffer& buffer, float factor)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (auto& sample : buffer.channel(channel))
            sample *= factor;
}
} // namespace

TEST_CASE("NR LEVEL 0 leaves audio untouched")
{
    auto noiseReduction = makeNoiseReduction(48000.0, 2, 0);
    PlanarBuffer buffer{2, 4800};
    buffer.fillWithNoise(41);
    scale(buffer, 1.0e-4f); // quiet enough to be gated if NR were on
    const auto input = buffer.data();
    noiseReduction.process(buffer.view());
    CHECK(buffer.data() == input);
}

TEST_CASE("Noise below the threshold is strongly reduced; a played note passes")
{
    const double sampleRate = 48000.0;
    auto noiseReduction = makeNoiseReduction(sampleRate, 1, 15); // threshold −48 dBFS

    PlanarBuffer hiss{1, 48000};
    hiss.fillWithNoise(42);
    scale(hiss, 1.0e-4f); // −80 dBFS peaks
    noiseReduction.process(hiss.view());
    double hissIn = 1.0e-4 / std::sqrt(3.0); // RMS of uniform noise with that peak
    CHECK(20.0 * std::log10(fivea::test::rms(hiss, 0, 24000, 24000) / hissIn) < -40.0);

    PlanarBuffer note{1, 48000};
    fivea::test::fillSine(note, 220.0, sampleRate, 0.25f); // −12 dBFS
    const double noteIn = fivea::test::rms(note, 0, 24000, 24000);
    noiseReduction.process(note.view());
    CHECK_THAT(20.0 * std::log10(fivea::test::rms(note, 0, 24000, 24000) / noteIn), WithinAbs(0.0, 0.1));
}

TEST_CASE("Higher NR LEVEL gates louder noise")
{
    double previous = 1.0;
    for (const int level : {3, 8, 15})
    {
        auto noiseReduction = makeNoiseReduction(48000.0, 1, level);
        PlanarBuffer hiss{1, 48000};
        hiss.fillWithNoise(43);
        scale(hiss, 0.003f); // about −50 dBFS
        noiseReduction.process(hiss.view());
        const double level_ = fivea::test::rms(hiss, 0, 24000, 24000);
        CHECK(level_ <= previous);
        previous = level_;
    }
}

TEST_CASE("The gate opens on the attack and closes on the release")
{
    const double sampleRate = 48000.0;
    const auto& profile = functionalPlaceholderProfile.noiseReduction;
    auto noiseReduction = makeNoiseReduction(sampleRate, 1, 15);

    // Closed on silence-level hiss, then a loud constant: the reduction falls as e^(−t/τ_attack).
    PlanarBuffer quiet{1, 48000};
    quiet.fill(1.0e-5f);
    noiseReduction.process(quiet.view());
    const double closedDb = 20.0 * std::log10(quiet.data()[0].back() / 1.0e-5);
    REQUIRE(closedDb < -60.0);

    PlanarBuffer loud{1, 4800};
    loud.fill(0.25f);
    noiseReduction.process(loud.view());
    const auto& opened = loud.data()[0];
    const auto reached = std::find_if(opened.begin(), opened.end(),
                                      [&](float sample)
                                      {
                                          return 20.0 * std::log10(sample / 0.25) >= closedDb * std::exp(-1.0);
                                      });
    const double openMs = 1000.0 * static_cast<double>(reached - opened.begin()) / sampleRate;
    CHECK_THAT(openMs, WithinAbs(static_cast<double>(profile.attackMs), 0.1));

    // Then back to quiet: the reduction rises as 1 − e^(−t/τ_release).
    PlanarBuffer fading{1, 48000};
    fading.fill(1.0e-5f);
    noiseReduction.process(fading.view());
    const auto& closing = fading.data()[0];
    const auto closed = std::find_if(closing.begin(), closing.end(),
                                     [&](float sample)
                                     {
                                         return 20.0 * std::log10(sample / 1.0e-5) <= closedDb * (1.0 - std::exp(-1.0));
                                     });
    const double closeMs = 1000.0 * static_cast<double>(closed - closing.begin()) / sampleRate;
    CHECK_THAT(closeMs, WithinAbs(static_cast<double>(profile.releaseMs), 0.02 * profile.releaseMs));
}

TEST_CASE("Switching NR off while it is closed opens smoothly, then becomes bit-exact")
{
    auto noiseReduction = makeNoiseReduction(48000.0, 1, 15);
    PlanarBuffer quiet{1, 48000};
    quiet.fill(1.0e-5f);
    noiseReduction.process(quiet.view());

    noiseReduction.setLevel(0);
    PlanarBuffer after{1, 48000};
    after.fill(1.0e-5f);
    noiseReduction.process(after.view());

    const auto& y = after.data()[0];
    CHECK(y[1] < 1.0e-5f * 0.5f); // still opening a sample later: no jump
    CHECK(std::all_of(y.begin() + 4800, y.end(),
                      [](float s)
                      {
                          return s == 1.0e-5f;
                      })); // then exact
}

TEST_CASE("Noise reduction stays finite, and does not allocate")
{
    auto noiseReduction = makeNoiseReduction(192000.0, 2, 99);
    PlanarBuffer buffer{2, 19200};
    buffer.fillWithNoise(44);
    std::size_t allocations = 0;
    {
        const fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 50; ++block)
        {
            noiseReduction.setLevel(block % 16);
            noiseReduction.process(buffer.viewOf(0, 384));
        }
        allocations = counter.count();
    }
    noiseReduction.process(buffer.viewOf(0, 0));
    CHECK(allocations == 0);
    CHECK(fivea::test::allFinite(buffer));
}
