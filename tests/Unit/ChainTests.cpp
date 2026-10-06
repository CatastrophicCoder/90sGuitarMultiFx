#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/FiveAProcessor.h"
#include "core/ModelProfile.h"
#include "core/StepMapping.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using namespace fivea;
using fivea::test::PlanarBuffer;

namespace
{
// Every control away from its default, so each block audibly does something.
ParameterSnapshot busySettings(unsigned int enabledMask)
{
    ParameterSnapshot snapshot;
    for (std::size_t block = 0; block < numEffectBlocks; ++block)
        snapshot.effectEnabled[block] = ((enabledMask >> block) & 1u) != 0;
    snapshot.compressor = {.sens = 10, .attack = 5, .level = 13};
    snapshot.drive = {.mode = 1, .drive = 9, .tone = 11, .level = 10};
    snapshot.equaliser = {.bass = 4, .midFrequency = 6, .mid = -3, .treble = 2, .trim = 13};
    snapshot.modulation = {.mode = 3, .speed = 7, .depth = 9, .feedback = 3, .mix = 11};
    snapshot.timeEffects = {.mode = 6, .time = 1, .fine = 5, .feedback = 6, .mix = 10};
    snapshot.noiseReductionLevel = 4;
    snapshot.master = 11;
    return snapshot;
}

FiveAProcessor makeChain(double sampleRate, int numChannels, const ParameterSnapshot& snapshot, int oversampling = 1)
{
    FiveAProcessor processor;
    processor.setParameters(snapshot);
    processor.prepare({sampleRate, 512, numChannels, oversampling});
    processor.setParameters(snapshot);
    processor.reset();
    return processor;
}

// The documented order, written out by hand from the individual blocks (EV-001, EV-014, EV-015).
std::vector<std::vector<float>> composeByHand(const ParameterSnapshot& snapshot, double sampleRate, int numChannels,
                                              PlanarBuffer buffer)
{
    const auto& profile = functionalPlaceholderProfile;
    dsp::NoiseReduction noiseReduction;
    dsp::Compressor compressor;
    dsp::Drive drive;
    dsp::ThreeBandEq equaliser;
    dsp::Modulation modulation;
    dsp::TimeEffects timeEffects;

    noiseReduction.prepare(sampleRate, numChannels, profile);
    noiseReduction.setLevel(snapshot.noiseReductionLevel);
    noiseReduction.reset();
    noiseReduction.process(buffer.view());

    if (snapshot.isEnabled(EffectBlock::Compressor))
    {
        compressor.prepare(sampleRate, numChannels, profile);
        compressor.setSettings(snapshot.compressor);
        compressor.reset();
        compressor.process(buffer.view());
    }
    if (snapshot.isEnabled(EffectBlock::Drive))
    {
        drive.prepare(sampleRate, numChannels, profile, 1);
        drive.setSettings(snapshot.drive);
        drive.reset();
        drive.process(buffer.view());
    }
    if (snapshot.isEnabled(EffectBlock::Equaliser))
    {
        equaliser.prepare(sampleRate, numChannels, profile);
        equaliser.setSettings(snapshot.equaliser);
        equaliser.reset();
        equaliser.process(buffer.view());
    }
    if (snapshot.isEnabled(EffectBlock::Modulation))
    {
        modulation.prepare(sampleRate, numChannels, profile);
        modulation.setSettings(snapshot.modulation);
        modulation.reset();
        modulation.process(buffer.view());
    }
    if (snapshot.isEnabled(EffectBlock::TimeEffect))
    {
        timeEffects.prepare(sampleRate, numChannels, profile);
        timeEffects.setSettings(snapshot.timeEffects);
        timeEffects.reset();
        timeEffects.process(buffer.view());
    }

    const float master = mapping::levelGain(snapshot.master, profile.level);
    for (int channel = 0; channel < numChannels; ++channel)
        for (auto& sample : buffer.channel(channel))
            sample *= master;
    return buffer.data();
}

double clickRatio(FiveAProcessor& processor, ParameterSnapshot before, ParameterSnapshot after)
{
    // As in the block tests: the transition against both settled states, 43 Hz at a sine peak.
    PlanarBuffer buffer{1, 4 * 48000};
    fivea::test::fillSine(buffer, 43.0, 48000.0, 0.05f);
    processor.setParameters(before);
    processor.reset();

    const int change = 96279;
    processor.process(buffer.viewOf(0, change));
    processor.setParameters(after);
    processor.process(buffer.viewOf(change, buffer.getNumSamples() - change));

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

TEST_CASE("The chain is the documented order, for all 32 on/off combinations")
{
    const double sampleRate = 48000.0;
    for (unsigned int mask = 0; mask < 32; ++mask)
    {
        const auto snapshot = busySettings(mask);
        auto processor = makeChain(sampleRate, 2, snapshot);

        PlanarBuffer input{2, 9600};
        input.fillWithNoise(51);
        for (int channel = 0; channel < 2; ++channel)
            for (auto& sample : input.channel(channel))
                sample *= 0.3f;

        const auto expected = composeByHand(snapshot, sampleRate, 2, input);
        processor.process(input.view());

        INFO("blocks on (bit 0 = compressor … bit 4 = reverb/delay): " << mask);
        CHECK(input.data() == expected);
    }
}

TEST_CASE("Switching an effect on or off does not click")
{
    // Measured 2026-10-05 (same measure as the block tests): with the 10 ms switching crossfade,
    // 3.2 to 65.8; without it, 584 to 17 100. The memory blocks (chorus/flanger, reverb/delay) first
    // measured 10 667 and 2 193 switching on, because a cleared block fed mid-signal records a step
    // and plays it back after the fade; their input now fades in too. Do not raise the bound
    // without asking.
    constexpr double switchClickBound = 150.0;
    auto processor = makeChain(48000.0, 1, busySettings(0));
    for (std::size_t block = 0; block < numEffectBlocks; ++block)
    {
        auto off = busySettings(0);
        auto on = off;
        on.effectEnabled[block] = true;
        INFO("block " << block);
        CHECK(clickRatio(processor, off, on) < switchClickBound);
        CHECK(clickRatio(processor, on, off) < switchClickBound);
    }
}

TEST_CASE("An effect switched off and on again starts clean, without its old tail")
{
    ParameterSnapshot on;
    on.effectEnabled[4] = true;
    on.timeEffects = {.mode = 2, .mix = 15}; // Ensemble Hall: the longest tail
    auto off = on;
    off.effectEnabled[4] = false;
    auto processor = makeChain(48000.0, 2, on);

    PlanarBuffer loud{2, 24000};
    loud.fillWithNoise(57);
    processor.process(loud.view()); // fill the reverb

    processor.setParameters(off);
    PlanarBuffer gap{2, 4800}; // well past the 10 ms fade-out
    processor.process(gap.view());

    processor.setParameters(on);
    PlanarBuffer silence{2, 9600};
    processor.process(silence.view());
    CHECK(fivea::test::peak(silence) == 0.0f); // nothing left over from before
}

TEST_CASE("With oversampling, the latency is the same whether the drive is on or off")
{
    // The oversampled chain must equal the same chain without oversampling, delayed by exactly the
    // reported latency. Quiet broadband noise keeps the drive nearly linear and gives the
    // cross-correlation one sharp peak.
    const int factor = GENERATE(2, 4);
    for (const bool driveOn : {false, true})
    {
        ParameterSnapshot snapshot;
        snapshot.effectEnabled[1] = driveOn;
        snapshot.drive = {.mode = 2, .drive = 0, .tone = 15, .level = 12};
        auto oversampled = makeChain(48000.0, 1, snapshot, factor);
        auto plain = makeChain(48000.0, 1, snapshot, 1);
        const int latency = oversampled.getLatencySamples();
        CHECK(latency == (factor == 2 ? 69 : 76));
        CHECK(plain.getLatencySamples() == 0);

        PlanarBuffer a{1, 9600};
        a.fillWithNoise(56);
        for (auto& sample : a.channel(0))
            sample *= 0.01f;
        PlanarBuffer b = a;
        oversampled.process(a.view());
        plain.process(b.view());

        int bestLag = 0;
        double best = -1.0;
        for (int lag = 0; lag < 200; ++lag)
        {
            double sum = 0.0;
            for (std::size_t n = 2400; n < 9600; ++n)
                sum += static_cast<double>(b.data()[0][n - static_cast<std::size_t>(lag)]) * a.data()[0][n];
            if (sum > best)
            {
                best = sum;
                bestLag = lag;
            }
        }
        INFO("factor " << factor << ", drive " << (driveOn ? "on" : "off"));
        CHECK(bestLag == latency);
    }
}

TEST_CASE("With oversampling, global bypass passes the input delayed by exactly the latency")
{
    ParameterSnapshot snapshot = busySettings(31);
    snapshot.globalBypass = true;
    auto processor = makeChain(48000.0, 2, snapshot, 4);

    PlanarBuffer buffer{2, 4800};
    buffer.fillWithNoise(52);
    const auto input = buffer.data();
    processor.process(buffer.view());

    const auto latency = static_cast<std::size_t>(processor.getLatencySamples());
    for (std::size_t channel = 0; channel < 2; ++channel)
    {
        CHECK(std::all_of(buffer.data()[channel].begin(), buffer.data()[channel].begin() + static_cast<long>(latency),
                          [](float s)
                          {
                              return s == 0.0f;
                          }));
        CHECK(std::equal(input[channel].begin(), input[channel].end() - static_cast<long>(latency),
                         buffer.data()[channel].begin() + static_cast<long>(latency)));
    }
}

TEST_CASE("Blocks longer than announced are processed the same as announced ones")
{
    const auto snapshot = busySettings(31);
    auto render = [&](int blockSize)
    {
        auto processor = makeChain(48000.0, 2, snapshot); // announced 512
        PlanarBuffer buffer{2, 12000};
        buffer.fillWithNoise(53);
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
            processor.process(buffer.viewOf(start, std::min(blockSize, buffer.getNumSamples() - start)));
        return buffer.data();
    };
    CHECK(render(4000) == render(512));
    CHECK(render(1) == render(512));
}

TEST_CASE("The full chain stays finite on long silence after loud input, at every rate")
{
    const double sampleRate = GENERATE(44100.0, 96000.0, 192000.0);
    auto snapshot = busySettings(31);
    snapshot.timeEffects = {.mode = 2, .mix = 15};
    auto processor = makeChain(sampleRate, 2, snapshot, 4);

    PlanarBuffer loud{2, static_cast<int>(sampleRate)};
    loud.fillWithNoise(54);
    processor.process(loud.view());
    CHECK(fivea::test::allFinite(loud));

    PlanarBuffer silence{2, static_cast<int>(sampleRate)};
    for (int second = 0; second < 10; ++second)
    {
        silence.fill(0.0f);
        processor.process(silence.view());
        CHECK(fivea::test::allFinite(silence));
    }
}

TEST_CASE("The full chain does not allocate, even while switching everything")
{
    auto processor = makeChain(48000.0, 2, busySettings(31), 4);
    PlanarBuffer buffer{2, 256};
    buffer.fillWithNoise(55);

    std::size_t allocations = 0;
    {
        const fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 400; ++block)
        {
            auto snapshot = busySettings(static_cast<unsigned int>(block * 7) % 32);
            snapshot.globalBypass = (block / 40) % 2 == 1;
            snapshot.drive.mode = 1 + (block / 13) % 2;
            snapshot.modulation.mode = 1 + (block / 17) % 5;
            snapshot.timeEffects.mode = 1 + (block / 19) % 7;
            processor.setParameters(snapshot);
            processor.process(buffer.view());
        }
        processor.reset();
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}

TEST_CASE("Re-preparing at another sample rate and block size keeps working")
{
    // A host may re-prepare at any time with new settings (plan §19.2). Each time, every effect must
    // run, and with everything off the chain must still be bit-exact.
    FiveAProcessor processor;
    struct Setting
    {
        double sampleRate;
        int blockSize;
        int oversampling;
    };
    for (const auto& setting : {Setting{44100.0, 512, 1}, Setting{192000.0, 64, 4}, Setting{48000.0, 4096, 2},
                                Setting{88200.0, 1, 1}, Setting{96000.0, 256, 4}})
    {
        INFO("fs " << setting.sampleRate << ", block " << setting.blockSize << ", oversampling "
                   << setting.oversampling);
        processor.setParameters(busySettings(31));
        processor.prepare({setting.sampleRate, setting.blockSize, 2, setting.oversampling});
        processor.reset();

        PlanarBuffer busy{2, 8192};
        busy.fillWithNoise(58);
        processor.process(busy.view());
        CHECK(fivea::test::allFinite(busy));
        CHECK(fivea::test::peak(busy) > 0.0f);

        ParameterSnapshot allOff;
        processor.setParameters(allOff);
        processor.reset();
        PlanarBuffer quiet{2, 4096};
        quiet.fillWithNoise(59);
        const auto input = quiet.data();
        processor.process(quiet.view());
        const auto latency = static_cast<long>(processor.getLatencySamples());
        for (std::size_t channel = 0; channel < 2; ++channel)
            CHECK(std::equal(input[channel].begin(), input[channel].end() - latency,
                             quiet.data()[channel].begin() + latency));
    }
}

// Not a check: prints the full chain's CPU use. Only a Release build's figures mean anything.
//   build-release/tests/fivea_engine_tests "[.benchmark]" -s
TEST_CASE("Benchmark the full chain", "[.benchmark]")
{
    for (const int oversampling : {1, 2, 4})
    {
        auto processor = makeChain(48000.0, 2, busySettings(31), oversampling);
        PlanarBuffer buffer{2, 256};
        buffer.fillWithNoise(60);

        const int blocks = 48000 * 10 / 256; // ten seconds of audio
        double worstBlock = 0.0;
        const auto start = std::chrono::steady_clock::now();
        for (int block = 0; block < blocks; ++block)
        {
            const auto before = std::chrono::steady_clock::now();
            processor.process(buffer.view());
            worstBlock =
                std::max(worstBlock, std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count());
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const double blockSeconds = 256.0 / 48000.0;
        WARN("all effects on, stereo, 48 kHz, oversampling " << oversampling << "x: " << 100.0 * seconds / 10.0
                                                             << " % of one core on average, worst block "
                                                             << 100.0 * worstBlock / blockSeconds << " %");
    }
}
