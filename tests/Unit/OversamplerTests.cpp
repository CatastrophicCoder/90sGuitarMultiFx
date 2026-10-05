#include "AllocationGuard.h"
#include "TestSignals.h"

#include "dsp/Oversampler.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using fivea::dsp::Oversampler;

namespace
{
std::vector<float> runThrough(Oversampler& oversampler, const std::vector<float>& input)
{
    std::vector<float> output(input.size());
    for (std::size_t n = 0; n < input.size(); ++n)
        output[n] = oversampler.processSample(0, input[n],
                                              [](float x)
                                              {
                                                  return x;
                                              });
    return output;
}

std::vector<float> sine(double frequency, double sampleRate, std::size_t length, float amplitude)
{
    std::vector<float> samples(length);
    for (std::size_t n = 0; n < length; ++n)
        samples[n] =
            amplitude *
            static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(n) / sampleRate));
    return samples;
}
} // namespace

TEST_CASE("Factor 1 is a straight pass-through with no latency")
{
    Oversampler oversampler;
    oversampler.prepare(1, 1);
    CHECK(oversampler.getLatencySamples() == 0);

    const auto input = sine(1000.0, 48000.0, 512, 0.5f);
    CHECK(runThrough(oversampler, input) == input);
}

TEST_CASE("Reported latency is where an impulse comes out")
{
    const int factor = GENERATE(2, 4);
    Oversampler oversampler;
    oversampler.prepare(factor, 1);

    std::vector<float> impulse(512, 0.0f);
    impulse[10] = 1.0f;
    const auto output = runThrough(oversampler, impulse);

    const auto peak = std::max_element(output.begin(), output.end(),
                                       [](float a, float b)
                                       {
                                           return std::abs(a) < std::abs(b);
                                       });
    INFO("factor " << factor << ": reported " << oversampler.getLatencySamples() << ", peak at "
                   << (peak - output.begin()) - 10);
    CHECK((peak - output.begin()) - 10 == oversampler.getLatencySamples());
    CHECK(*peak > 0.9f);
}

TEST_CASE("Audio passes through, delayed by exactly the latency, within the passband")
{
    const int factor = GENERATE(2, 4);
    const double sampleRate = GENERATE(44100.0, 96000.0);
    Oversampler oversampler;
    oversampler.prepare(factor, 1);
    const auto latency = static_cast<std::size_t>(oversampler.getLatencySamples());

    for (const double frequency : {100.0, 1000.0, 10000.0, 18000.0})
    {
        oversampler.reset();
        const auto input = sine(frequency, sampleRate, 8192, 0.5f);
        const auto output = runThrough(oversampler, input);

        double largestError = 0.0;
        for (std::size_t n = 1024; n < input.size(); ++n)
            largestError = std::max(largestError, static_cast<double>(std::abs(output[n] - input[n - latency])));

        INFO("factor " << factor << ", fs " << sampleRate << ", f " << frequency);
        CHECK(largestError < 0.5 * 0.002); // passband ripple below about 0.02 dB
    }
}

TEST_CASE("Components above half the base rate do not fold back")
{
    // A nonlinearity that multiplies by a sine at the oversampled rate's Nyquist region would be
    // complicated; instead, square the signal: a 15 kHz input makes 30 kHz, which is above
    // 22.05 kHz and would alias to 14.1 kHz without oversampling.
    const double sampleRate = 44100.0;
    const auto input = sine(15000.0, sampleRate, 32768, 0.5f);

    auto aliasLevel = [&](int factor)
    {
        Oversampler oversampler;
        oversampler.prepare(factor, 1);
        std::vector<float> output(input.size());
        for (std::size_t n = 0; n < input.size(); ++n)
            output[n] = oversampler.processSample(0, input[n],
                                                  [](float x)
                                                  {
                                                      return x * x;
                                                  });
        return fivea::test::toneAmplitude(output, sampleRate - 30000.0, sampleRate, 4096, 28672);
    };

    const double without = aliasLevel(1);
    const double with2 = aliasLevel(2);
    const double with4 = aliasLevel(4);
    INFO("alias at 14.1 kHz: 1× " << without << ", 2× " << with2 << ", 4× " << with4);
    CHECK(without > 0.1);            // 0.125 expected: the squared sine's 30 kHz part, folded
    CHECK(with2 < without * 1.0e-4); // at least 80 dB lower
    CHECK(with4 < without * 1.0e-4);
}

TEST_CASE("Oversampler does not allocate while processing, and channels are independent")
{
    Oversampler oversampler;
    oversampler.prepare(4, 2);
    std::size_t allocations = 0;
    float left = 0.0f;
    float right = 0.0f;
    {
        const fivea::test::ScopedAllocationCounter counter;
        for (int n = 0; n < 4096; ++n)
        {
            left = oversampler.processSample(0, n == 100 ? 1.0f : 0.0f,
                                             [](float x)
                                             {
                                                 return x;
                                             });
            right = oversampler.processSample(1, 0.0f,
                                              [](float x)
                                              {
                                                  return x;
                                              });
        }
        oversampler.reset();
        allocations = counter.count();
    }
    CHECK(allocations == 0);
    CHECK(right == 0.0f);
    CHECK(std::isfinite(left));
}
