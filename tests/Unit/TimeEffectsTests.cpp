#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/ModelProfile.h"
#include "core/StepMapping.h"
#include "dsp/Biquad.h"
#include "dsp/TimeEffects.h"

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
const ReverbProfile& reverbProfile = functionalPlaceholderProfile.reverb;

TimeEffects makeTimeEffects(double sampleRate, int numChannels, const TimeEffects::Settings& settings)
{
    TimeEffects effects;
    effects.prepare(sampleRate, numChannels, functionalPlaceholderProfile);
    effects.setSettings(settings);
    effects.reset();
    return effects;
}

// The effect's impulse response (dry removed), per channel.
PlanarBuffer impulseResponse(TimeEffects& effects, int numChannels, int length)
{
    PlanarBuffer buffer{numChannels, length};
    for (int channel = 0; channel < numChannels; ++channel)
        buffer.channel(channel)[0] = 1.0f;
    effects.process(buffer.view());
    for (int channel = 0; channel < numChannels; ++channel)
        buffer.channel(channel)[0] -= 1.0f;
    return buffer;
}

// T60 in a band: Schroeder's backward-integrated energy decay of the impulse response, filtered,
// fitted between −5 and −25 dB and extrapolated to 60 dB.
double decaySeconds(std::vector<float> response, double sampleRate, const BiquadCoefficients& band)
{
    Biquad filter;
    filter.setCoefficients(band);
    for (auto& sample : response)
        sample = filter.processSample(0, sample);

    std::vector<double> energy(response.size());
    double remaining = 0.0;
    for (std::size_t n = response.size(); n-- > 0;)
    {
        remaining += static_cast<double>(response[n]) * response[n];
        energy[n] = remaining;
    }

    std::size_t at5 = 0;
    std::size_t at25 = 0;
    for (std::size_t n = 0; n < energy.size(); ++n)
    {
        const double db = 10.0 * std::log10(energy[n] / energy[0]);
        if (at5 == 0 && db <= -5.0)
            at5 = n;
        if (db <= -25.0)
        {
            at25 = n;
            break;
        }
    }
    return 3.0 * static_cast<double>(at25 - at5) / sampleRate;
}

double lowFrequencyDecaySeconds(const std::vector<float>& response, double sampleRate)
{
    return decaySeconds(response, sampleRate, design::lowPass(sampleRate, 300.0, 0.7071067811865476));
}

double highFrequencyDecaySeconds(const std::vector<float>& response, double sampleRate)
{
    return decaySeconds(response, sampleRate, design::highPass(sampleRate, 4000.0, 0.7071067811865476));
}

// Same click measure as the other blocks, on a 43 Hz sine: no documented delay step is a whole
// number of its periods, so a delay change cannot hide. Measured 2026-10-05 at 48 kHz: ramped
// changes 3.5 to 6.1 (MODE), 25 to 34 (TIME, FINE), 13.5 (F.BACK), 38.8 (MIX); unramped 98
// (F.BACK) to 20 400. The margins are narrow (about 1.5× each side). Do not raise the bound
// without asking.
constexpr double clickRatioBound = 60.0;

double clickRatio(const TimeEffects::Settings& before, const TimeEffects::Settings& after)
{
    const double sampleRate = 48000.0;
    auto effects = makeTimeEffects(sampleRate, 1, before);
    PlanarBuffer buffer{1, 4 * 48000};
    fivea::test::fillSine(buffer, 43.0, sampleRate, 0.05f);

    const int change = 96279; // a sine peak
    effects.process(buffer.viewOf(0, change));
    effects.setSettings(after);
    effects.process(buffer.viewOf(change, buffer.getNumSamples() - change));

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
    const int end = buffer.getNumSamples();
    return largestBetween(change, change + 24000) /
           std::max(largestBetween(change - 24000, change), largestBetween(end - 24000, end));
}
} // namespace

// --- Delay ---------------------------------------------------------------------------------------

TEST_CASE("Delay echoes at TIME x 100 ms + FINE x 10 ms, up to the documented 490 ms")
{
    const double sampleRate = 48000.0;
    struct Case
    {
        int time;
        int fine;
        double ms;
    };
    for (const auto& c : {Case{1, 0, 100.0}, Case{3, 5, 350.0}, Case{4, 9, 490.0}, Case{0, 4, 40.0}})
    {
        auto effects =
            makeTimeEffects(sampleRate, 1, {.mode = 7, .time = c.time, .fine = c.fine, .feedback = 0, .mix = 15});
        const auto response = impulseResponse(effects, 1, 30000);
        const auto echo = static_cast<std::size_t>(c.ms * 48.0);

        INFO("TIME " << c.time << ", FINE " << c.fine);
        CHECK_THAT(response.data()[0][echo], WithinAbs(1.0, 1.0e-6)); // MIX 15: the echo as loud as the dry
        CHECK(std::abs(response.data()[0][echo - 1]) < 1.0e-6);
    }
}

TEST_CASE("F.BACK repeats decay by the feedback amount, through the repeat-path tone filter")
{
    // A 200 Hz burst, well below the 5 kHz repeat-path filter: each repeat is F.BACK × the last.
    const double sampleRate = 48000.0;
    const int feedbackStep = 10;
    auto effects =
        makeTimeEffects(sampleRate, 1, {.mode = 7, .time = 1, .fine = 0, .feedback = feedbackStep, .mix = 15});

    PlanarBuffer buffer{1, 48000};
    for (int n = 0; n < 960; ++n) // 20 ms, four whole periods
        buffer.channel(0)[static_cast<std::size_t>(n)] =
            static_cast<float>(std::sin(2.0 * std::numbers::pi * 200.0 * n / sampleRate));
    effects.process(buffer.view());

    const auto& y = buffer.data()[0];
    auto burstPeak = [&](std::size_t start)
    {
        float peak = 0.0f;
        for (std::size_t n = start + 240; n < start + 720; ++n) // the burst's middle, away from its edges
            peak = std::max(peak, std::abs(y[n]));
        return static_cast<double>(peak);
    };
    const double amount = mapping::feedbackAmount(feedbackStep, functionalPlaceholderProfile.delay.feedbackMaximum);
    CHECK_THAT(burstPeak(9600) / burstPeak(4800), WithinRel(amount, 0.02));
    CHECK_THAT(burstPeak(14400) / burstPeak(9600), WithinRel(amount, 0.02));
}

TEST_CASE("TIME, FINE and F.BACK do nothing in the reverb modes")
{
    for (int mode = 1; mode <= 5; ++mode)
    {
        auto a = makeTimeEffects(48000.0, 2, {.mode = mode, .time = 0, .fine = 0, .feedback = 0, .mix = 12});
        auto b = makeTimeEffects(48000.0, 2, {.mode = mode, .time = 4, .fine = 9, .feedback = 15, .mix = 12});
        PlanarBuffer x{2, 24000};
        PlanarBuffer y{2, 24000};
        x.fillWithNoise(31);
        y.fillWithNoise(31);
        a.process(x.view());
        b.process(y.view());
        INFO("MODE " << mode);
        CHECK(x.data() == y.data());
    }
}

// --- Reverb --------------------------------------------------------------------------------------

TEST_CASE("Each reverb voicing decays in its set time")
{
    const double sampleRate = GENERATE(44100.0, 48000.0, 96000.0);
    for (int mode = 1; mode <= 5; ++mode)
    {
        auto effects = makeTimeEffects(sampleRate, 1, {.mode = mode, .mix = 15});
        const auto response = impulseResponse(effects, 1, static_cast<int>(sampleRate * 8.0));
        const double target = reverbProfile.voicings[static_cast<std::size_t>(mode - 1)].decaySeconds;

        INFO("fs " << sampleRate << ", MODE " << mode);
        CHECK_THAT(lowFrequencyDecaySeconds(response.data()[0], sampleRate), WithinRel(target, 0.05));
    }
}

TEST_CASE("Reverb tails darken: treble dies away faster than bass")
{
    // T60 above 4 kHz ÷ T60 below 300 Hz, measured 2026-10-05 at 48 kHz: 0.68–0.84 with the
    // absorption filters, 0.99–1.03 with the high-frequency damping removed.
    for (int mode = 1; mode <= 5; ++mode)
    {
        auto effects = makeTimeEffects(48000.0, 1, {.mode = mode, .mix = 15});
        const auto response = impulseResponse(effects, 1, 48000 * 8);
        const auto& y = response.data()[0];
        INFO("MODE " << mode);
        CHECK(highFrequencyDecaySeconds(y, 48000.0) < 0.92 * lowFrequencyDecaySeconds(y, 48000.0));
    }
}

TEST_CASE("Reverb tails are decorrelated between left and right")
{
    for (int mode = 1; mode <= 5; ++mode)
    {
        auto effects = makeTimeEffects(48000.0, 2, {.mode = mode, .mix = 15});
        const auto response = impulseResponse(effects, 2, 96000);
        const auto& left = response.data()[0];
        const auto& right = response.data()[1];

        double ll = 0.0;
        double rr = 0.0;
        double lr = 0.0;
        for (std::size_t n = 2400; n < left.size(); ++n)
        {
            ll += static_cast<double>(left[n]) * left[n];
            rr += static_cast<double>(right[n]) * right[n];
            lr += static_cast<double>(left[n]) * right[n];
        }
        INFO("MODE " << mode);
        CHECK(std::abs(lr / std::sqrt(ll * rr)) < 0.2); // measured 0.05–0.09
    }
}

TEST_CASE("Tails die away instead of building up")
{
    const int mode = GENERATE(1, 2, 3, 4, 5, 6, 7);
    auto effects = makeTimeEffects(48000.0, 2, {.mode = mode, .time = 4, .fine = 9, .feedback = 15, .mix = 15});
    PlanarBuffer burst{2, 48000};
    burst.fillWithNoise(32);
    effects.process(burst.view());

    // Peak of each following second of silence. The slowest case is Delay at maximum F.BACK
    // (0.85 per 490 ms repeat, about 1.4 dB each): 20 seconds bring it down by roughly 57 dB.
    std::vector<float> peaks;
    PlanarBuffer silence{2, 48000};
    for (int second = 0; second < 30; ++second)
    {
        silence.fill(0.0f); // fresh silence each second: the buffer holds the previous output
        effects.process(silence.view());
        CHECK(fivea::test::allFinite(silence));
        peaks.push_back(fivea::test::peak(silence));
    }
    CHECK(peaks[29] < 0.01f * peaks[9]);
    CHECK(peaks[29] < 1.0e-3f);
}

TEST_CASE("Echoverb is echoes plus reverb")
{
    const double sampleRate = 48000.0;
    auto echoverb = makeTimeEffects(sampleRate, 1, {.mode = 6, .time = 2, .fine = 0, .feedback = 0, .mix = 15});
    const auto response = impulseResponse(echoverb, 1, 48000);
    const auto& y = response.data()[0];

    // An echo at 200 ms, at the profile's echo gain …
    CHECK_THAT(y[9600], WithinAbs(reverbProfile.echoverbDelayGain, 0.05));
    // … and reverb energy in between, where a plain delay would be silent.
    double between = 0.0;
    for (std::size_t n = 2400; n < 9000; ++n)
        between += static_cast<double>(y[n]) * y[n];
    CHECK(between > 1.0e-3);
}

// --- The block -----------------------------------------------------------------------------------

TEST_CASE("Reverb/Delay MIX 0 is the dry signal, bit for bit")
{
    const int mode = GENERATE(1, 3, 6, 7);
    auto effects = makeTimeEffects(48000.0, 2, {.mode = mode, .time = 2, .feedback = 10, .mix = 0});
    PlanarBuffer buffer{2, 9600};
    buffer.fillWithNoise(33);
    const auto input = buffer.data();
    effects.process(buffer.view());
    CHECK(buffer.data() == input);
}

TEST_CASE("Changing MODE, TIME, FINE, F.BACK or MIX does not click")
{
    CHECK(clickRatio({.mode = 1, .mix = 12}, {.mode = 3, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 1, .time = 3, .feedback = 6, .mix = 12},
                     {.mode = 7, .time = 3, .feedback = 6, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 7, .time = 2, .feedback = 6, .mix = 12},
                     {.mode = 6, .time = 2, .feedback = 6, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 7, .time = 3, .feedback = 6, .mix = 12},
                     {.mode = 7, .time = 1, .feedback = 6, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 7, .time = 2, .fine = 0, .feedback = 6, .mix = 12},
                     {.mode = 7, .time = 2, .fine = 9, .feedback = 6, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 7, .time = 2, .feedback = 0, .mix = 12},
                     {.mode = 7, .time = 2, .feedback = 15, .mix = 12}) < clickRatioBound);
    CHECK(clickRatio({.mode = 1, .mix = 0}, {.mode = 1, .mix = 15}) < clickRatioBound);
}

TEST_CASE("Rapid MODE changes do not cut a tail")
{
    // A change during a mode crossfade waits for it to finish instead of cutting the fading tail.
    auto effects = makeTimeEffects(48000.0, 1, {.mode = 1, .mix = 12});
    PlanarBuffer buffer{1, 4 * 48000};
    fivea::test::fillSine(buffer, 43.0, 48000.0, 0.05f);

    int position = 96279;
    effects.process(buffer.viewOf(0, position));
    for (const int mode : {3, 7, 2})
    {
        effects.setSettings({.mode = mode, .time = 2, .feedback = 6, .mix = 12});
        effects.process(buffer.viewOf(position, 960)); // 20 ms, well inside the 100 ms crossfade
        position += 960;
    }
    effects.process(buffer.viewOf(position, buffer.getNumSamples() - position));

    CHECK(fivea::test::allFinite(buffer));
    CHECK(fivea::test::largestStep(buffer, 0) < 0.02);
}

TEST_CASE("Silence stays silent; extreme settings and every rate stay finite and bounded")
{
    const double sampleRate = GENERATE(44100.0, 48000.0, 88200.0, 96000.0, 192000.0);
    const int mode = GENERATE(1, 6, 7);
    auto effects = makeTimeEffects(sampleRate, 2, {.mode = mode, .time = 99, .fine = 99, .feedback = 99, .mix = 99});

    PlanarBuffer silence{2, 9600};
    effects.process(silence.view());
    CHECK(fivea::test::peak(silence) == 0.0f);

    PlanarBuffer noise{2, static_cast<int>(sampleRate)};
    noise.fillWithNoise(34);
    effects.process(noise.view());
    effects.process(noise.viewOf(0, 0));
    CHECK(fivea::test::allFinite(noise));
    CHECK(fivea::test::peak(noise) < 20.0f); // feedback ≤ 0.85 gains at most 1/(1 − 0.85) ≈ 6.7, plus dry
}

TEST_CASE("Reverb/Delay output does not depend on block size, and reset makes it repeatable")
{
    auto render = [](int blockSize)
    {
        auto effects = makeTimeEffects(96000.0, 2, {.mode = 6, .time = 1, .fine = 3, .feedback = 7, .mix = 13});
        PlanarBuffer buffer{2, 38400};
        buffer.fillWithNoise(35);
        for (int begin = 0; begin < buffer.getNumSamples(); begin += blockSize)
        {
            if (begin == 9600)
                effects.setSettings({.mode = 6, .time = 3, .fine = 0, .feedback = 4, .mix = 13});
            if (begin == 19200)
                effects.setSettings({.mode = 2, .mix = 10});
            effects.process(buffer.viewOf(begin, std::min(blockSize, buffer.getNumSamples() - begin)));
        }
        return buffer.data();
    };

    const auto reference = render(1);
    CHECK(render(64) == reference);
    CHECK(render(480) == reference);
}

TEST_CASE("Reverb/Delay does not allocate while processing or changing settings")
{
    auto effects = makeTimeEffects(48000.0, 2, {});
    PlanarBuffer buffer{2, 256};
    buffer.fillWithNoise(36);

    std::size_t allocations = 0;
    {
        const fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 400; ++block)
        {
            effects.setSettings({.mode = 1 + (block / 20) % 7,
                                 .time = block % 5,
                                 .fine = block % 10,
                                 .feedback = block % 16,
                                 .mix = block % 16});
            effects.process(buffer.view());
        }
        effects.reset();
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}
