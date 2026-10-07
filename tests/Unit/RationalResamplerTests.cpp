#include "AllocationGuard.h"

#include "dsp/RationalResampler.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

using fivea::dsp::RationalResampler;

namespace
{
constexpr double internalRate = 44100.0;

// Every rate is converted to and from the internal 44.1 kHz.
struct Conversion
{
    double from;
    double to;
};

Conversion conversionUnderTest()
{
    const double hostRate = GENERATE(48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    const bool down = GENERATE(true, false);
    return down ? Conversion{hostRate, internalRate} : Conversion{internalRate, hostRate};
}

// Runs a whole signal through, in blocks of the given lengths (cycled), one channel or two.
std::vector<std::vector<float>> run(RationalResampler& resampler, const std::vector<std::vector<float>>& input,
                                    const std::vector<int>& blockLengths = {512})
{
    const auto channels = input.size();
    const auto length = static_cast<int>(input[0].size());
    std::vector<std::vector<float>> output(channels);
    std::vector<std::vector<float>> scratch(
        channels, std::vector<float>(static_cast<std::size_t>(resampler.maximumOutputFor(length) + 1)));
    std::vector<const float*> in(channels);
    std::vector<float*> out(channels);

    for (int start = 0, block = 0; start < length; ++block)
    {
        const int count = std::min(length - start, blockLengths[static_cast<std::size_t>(block) % blockLengths.size()]);
        for (std::size_t c = 0; c < channels; ++c)
        {
            in[c] = input[c].data() + start;
            out[c] = scratch[c].data();
        }
        const int produced = resampler.process(in.data(), count, out.data());
        for (std::size_t c = 0; c < channels; ++c)
            output[c].insert(output[c].end(), scratch[c].begin(), scratch[c].begin() + produced);
        start += count;
    }
    return output;
}

std::vector<float> sine(double frequency, double sampleRate, std::size_t length, double amplitude)
{
    std::vector<float> samples(length);
    for (std::size_t n = 0; n < length; ++n)
        samples[n] = static_cast<float>(
            amplitude * std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(n) / sampleRate));
    return samples;
}

// The output's departure from the ideal: the same sine, at the output rate, delayed by the
// resampler's stated delay. Covers gain, phase and the exactness of the delay at once; anything
// else in the output (aliases, images, filter error) adds to it. In dB relative to the sine.
double errorDb(const std::vector<float>& output, double frequency, double amplitude, const RationalResampler& resampler,
               double outputRate)
{
    const double delaySeconds = resampler.getDelaySeconds();
    const auto settled = static_cast<std::size_t>(std::ceil(delaySeconds * outputRate * 2.0)) + 64;
    double error = 0.0;
    double reference = 0.0;
    for (std::size_t k = settled; k < output.size(); ++k)
    {
        const double ideal = amplitude * std::sin(2.0 * std::numbers::pi * frequency *
                                                  (static_cast<double>(k) / outputRate - delaySeconds));
        error += (output[k] - ideal) * (output[k] - ideal);
        reference += ideal * ideal;
    }
    return 10.0 * std::log10(error / reference);
}

double rmsDb(const std::vector<float>& samples, std::size_t start)
{
    double sum = 0.0;
    for (std::size_t n = start; n < samples.size(); ++n)
        sum += static_cast<double>(samples[n]) * samples[n];
    return 10.0 * std::log10(sum / static_cast<double>(samples.size() - start) + 1.0e-30);
}
} // namespace

TEST_CASE("The resampler reduces each rate's ratio to 44.1 kHz to whole numbers")
{
    struct Expected
    {
        double hostRate;
        int up;
        int down;
    };
    for (const auto expected : {Expected{48000.0, 147, 160}, Expected{88200.0, 1, 2}, Expected{96000.0, 147, 320},
                                Expected{176400.0, 1, 4}, Expected{192000.0, 147, 640}, Expected{44100.0, 1, 1}})
    {
        RationalResampler down;
        REQUIRE(down.prepare(expected.hostRate, internalRate, 1));
        CHECK(down.getUpFactor() == expected.up);
        CHECK(down.getDownFactor() == expected.down);

        RationalResampler up;
        REQUIRE(up.prepare(internalRate, expected.hostRate, 1));
        CHECK(up.getUpFactor() == expected.down);
        CHECK(up.getDownFactor() == expected.up);
    }
}

TEST_CASE("At equal rates the resampler is a straight copy")
{
    RationalResampler resampler;
    REQUIRE(resampler.prepare(internalRate, internalRate, 2));
    CHECK(resampler.getDelaySeconds() == 0.0);

    std::vector<std::vector<float>> input(2, std::vector<float>(1000));
    std::mt19937 generator{5};
    std::uniform_real_distribution<float> distribution{-1.0f, 1.0f};
    for (auto& channel : input)
        for (auto& sample : channel)
            sample = distribution(generator);
    CHECK(run(resampler, input, {1, 7, 300}) == input);
}

TEST_CASE("The resampler passes everything up to 20 kHz, in time with its stated delay")
{
    const auto conversion = conversionUnderTest();
    const auto frequency = GENERATE(50.0, 1000.0, 5000.0, 10000.0, 15000.0, 19000.0, 20000.0);

    RationalResampler resampler;
    REQUIRE(resampler.prepare(conversion.from, conversion.to, 1));
    const auto output = run(resampler, {sine(frequency, conversion.from, 24000, 0.5)});

    // Kaiser's length estimate is approximate: the response starts to fall just short of the
    // passband edge, measured -0.007 dB at 20 kHz (2026-10-07). The specification there is
    // +-0.01 dB, an error of -58.8 dB; below 19.5 kHz the error is under -105 dB.
    const double bound = frequency < 19500.0 ? -90.0 : 20.0 * std::log10(1.0 - std::pow(10.0, -0.01 / 20.0));
    INFO(conversion.from << " -> " << conversion.to << " Hz, " << frequency << " Hz");
    CHECK(errorDb(output[0], frequency, 0.5, resampler, conversion.to) < bound);
}

TEST_CASE("The resampler adds a requested delay exactly")
{
    const auto conversion = conversionUnderTest();
    RationalResampler plain;
    REQUIRE(plain.prepare(conversion.from, conversion.to, 1));
    RationalResampler delayed;
    REQUIRE(delayed.prepare(conversion.from, conversion.to, 1, 37));

    const double commonRate = conversion.from * delayed.getUpFactor();
    INFO(conversion.from << " -> " << conversion.to << " Hz");
    CHECK(std::abs(delayed.getDelaySeconds() - plain.getDelaySeconds() - 37.0 / commonRate) < 1.0e-12);
    CHECK(delayed.getDelayInCommonRateSamples() == plain.getDelayInCommonRateSamples() + 37);

    const auto output = run(delayed, {sine(3000.0, conversion.from, 24000, 0.5)});
    CHECK(errorDb(output[0], 3000.0, 0.5, delayed, conversion.to) < -90.0);
}

TEST_CASE("Converting down, nothing above the lower rate's Nyquist folds back")
{
    const double hostRate = GENERATE(48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    for (const double frequency : {22100.0, 23000.0, 0.49 * hostRate})
    {
        RationalResampler resampler;
        REQUIRE(resampler.prepare(hostRate, internalRate, 1));
        const auto output = run(resampler, {sine(frequency, hostRate, 48000, 0.5)});

        INFO(hostRate << " Hz, a sine at " << frequency << " Hz");
        // Relative to the input sine (0.5 amplitude: -9 dB RMS re full scale).
        CHECK(rmsDb(output[0], 2048) - 10.0 * std::log10(0.125) < -100.0);
    }
}

TEST_CASE("Converting up, the images above 22.05 kHz are removed")
{
    const double hostRate = GENERATE(48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    // Up to 19 kHz, where the passband is flat to well below this bound; its closest image is at
    // 25.1 kHz. The images nearest the stopband edge are checked converting down.
    for (const double frequency : {1000.0, 10000.0, 19000.0})
    {
        RationalResampler resampler;
        REQUIRE(resampler.prepare(internalRate, hostRate, 1));
        const auto output = run(resampler, {sine(frequency, internalRate, 24000, 0.5)});

        // Images of f sit at 44.1 kHz ± f and its multiples; whatever remains besides the sine
        // itself is the error.
        INFO(hostRate << " Hz, a sine at " << frequency << " Hz");
        CHECK(errorDb(output[0], frequency, 0.5, resampler, hostRate) < -100.0);
    }
}

TEST_CASE("The resampler's output does not depend on block size")
{
    const auto conversion = conversionUnderTest();
    std::vector<float> noise(20000);
    std::mt19937 generator{6};
    std::uniform_real_distribution<float> distribution{-1.0f, 1.0f};
    for (auto& sample : noise)
        sample = distribution(generator);

    RationalResampler whole;
    REQUIRE(whole.prepare(conversion.from, conversion.to, 1));
    RationalResampler pieces;
    REQUIRE(pieces.prepare(conversion.from, conversion.to, 1));

    INFO(conversion.from << " -> " << conversion.to << " Hz");
    CHECK(run(pieces, {noise}, {1, 2, 3, 64, 1, 511, 17}) == run(whole, {noise}, {20000}));
}

TEST_CASE("The resampler produces exactly the ratio's share of samples, block after block")
{
    // Output k is due once input floor(k * down / up) has arrived, so after n inputs exactly
    // ceil(n * up / down) outputs exist. Counted in whole numbers: nothing to drift.
    const auto conversion = conversionUnderTest();
    RationalResampler resampler;
    REQUIRE(resampler.prepare(conversion.from, conversion.to, 1));
    const auto up = static_cast<long long>(resampler.getUpFactor());
    const auto down = static_cast<long long>(resampler.getDownFactor());

    std::vector<float> input(4096, 0.0f);
    std::vector<float> output(static_cast<std::size_t>(resampler.maximumOutputFor(4096)));
    std::mt19937 generator{7};
    std::uniform_int_distribution<int> lengths{1, 4096};

    long long inputs = 0;
    long long outputs = 0;
    bool countsMatch = true;
    bool withinMaximum = true;
    while (inputs < static_cast<long long>(conversion.from) * 30) // 30 seconds
    {
        const int count = lengths(generator);
        const float* in = input.data();
        float* out = output.data();
        const int produced = resampler.process(&in, count, &out);
        withinMaximum = withinMaximum && produced <= resampler.maximumOutputFor(count);
        inputs += count;
        outputs += produced;
        countsMatch = countsMatch && outputs == (inputs * up + down - 1) / down;
    }
    INFO(conversion.from << " -> " << conversion.to << " Hz");
    CHECK(countsMatch);
    CHECK(withinMaximum);
}

TEST_CASE("The resampler keeps stereo channels in step")
{
    const auto conversion = conversionUnderTest();
    const auto left = sine(440.0, conversion.from, 8000, 0.5);
    const auto right = sine(7000.0, conversion.from, 8000, 0.25);

    RationalResampler stereo;
    REQUIRE(stereo.prepare(conversion.from, conversion.to, 2));
    RationalResampler mono;
    REQUIRE(mono.prepare(conversion.from, conversion.to, 1));
    const auto both = run(stereo, {left, right}, {1, 300, 77});

    INFO(conversion.from << " -> " << conversion.to << " Hz");
    CHECK(both[0] == run(mono, {left})[0]);
    mono.reset();
    CHECK(both[1] == run(mono, {right})[0]);
}

TEST_CASE("Reset makes the resampler repeatable")
{
    RationalResampler resampler;
    REQUIRE(resampler.prepare(48000.0, internalRate, 1));
    const auto signal = sine(1000.0, 48000.0, 5000, 0.5);
    const auto first = run(resampler, {signal}, {333});
    resampler.reset();
    CHECK(run(resampler, {signal}, {333}) == first);
}

TEST_CASE("The resampler does not allocate while processing or resetting")
{
    const auto conversion = conversionUnderTest();
    RationalResampler resampler;
    REQUIRE(resampler.prepare(conversion.from, conversion.to, 2));

    std::vector<std::vector<float>> input(2, std::vector<float>(512, 0.25f));
    std::vector<std::vector<float>> output(
        2, std::vector<float>(static_cast<std::size_t>(resampler.maximumOutputFor(512))));
    const float* in[] = {input[0].data(), input[1].data()};
    float* out[] = {output[0].data(), output[1].data()};

    std::size_t allocations = 0;
    {
        fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 50; ++block)
            (void)resampler.process(in, 512, out);
        resampler.reset();
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}

TEST_CASE("A ratio too fine to tabulate is refused, not approximated")
{
    // 44100 / 47993 does not reduce: its table would need 44100 phases. Approximating the ratio
    // would drift, so prepare() says no.
    RationalResampler resampler;
    CHECK_FALSE(resampler.prepare(47993.0, internalRate, 1));
    CHECK_FALSE(resampler.prepare(48000.5, internalRate, 1)); // not a whole number of Hz
}
