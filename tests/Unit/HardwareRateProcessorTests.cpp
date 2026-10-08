#include "AllocationGuard.h"
#include "TestSignals.h"

#include "core/FiveAProcessor.h"
#include "core/HardwareRateProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

using namespace fivea;
using fivea::test::PlanarBuffer;

namespace
{
constexpr double hardwareRate = 44100.0;

void processInBlocks(HardwareRateProcessor& processor, PlanarBuffer& buffer, const std::vector<int>& blockLengths)
{
    for (int start = 0, block = 0; start < buffer.getNumSamples(); ++block)
    {
        const int count = std::min(buffer.getNumSamples() - start,
                                   blockLengths[static_cast<std::size_t>(block) % blockLengths.size()]);
        processor.process(buffer.viewOf(start, count));
        start += count;
    }
}

// A signal well inside the band the conversion keeps: two sines per channel, different per channel.
void fillTones(PlanarBuffer& buffer, double sampleRate)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto& samples = buffer.channel(channel);
        const double first = channel == 0 ? 997.0 : 1499.0;
        const double second = channel == 0 ? 7001.0 : 11003.0;
        for (std::size_t n = 0; n < samples.size(); ++n)
        {
            const double t = static_cast<double>(n) / sampleRate;
            samples[n] = static_cast<float>(0.3 * std::sin(2.0 * std::numbers::pi * first * t) +
                                            0.2 * std::sin(2.0 * std::numbers::pi * second * t));
        }
    }
}

// The output's departure from the input delayed by `latency` samples, in dB relative to the
// input, over [start, end).
double alignedErrorDb(const std::vector<float>& output, const std::vector<float>& input, int latency, std::size_t start,
                      std::size_t end)
{
    double error = 0.0;
    double reference = 0.0;
    for (std::size_t n = start; n < end; ++n)
    {
        const double expected = input[n - static_cast<std::size_t>(latency)];
        error += (output[n] - expected) * (output[n] - expected);
        reference += expected * expected;
    }
    return 10.0 * std::log10(error / reference);
}

HardwareRateProcessor makeProcessor(double sampleRate, int numChannels, bool atHardwareRate, int oversampling = 1,
                                    const ParameterSnapshot& parameters = {})
{
    HardwareRateProcessor processor;
    processor.setParameters(parameters);
    processor.prepare({sampleRate, 512, numChannels, oversampling}, atHardwareRate);
    processor.setParameters(parameters);
    processor.reset();
    return processor;
}

ParameterSnapshot reverbAndDrive()
{
    ParameterSnapshot parameters;
    parameters.effectEnabled[static_cast<std::size_t>(EffectBlock::Drive)] = true;
    parameters.effectEnabled[static_cast<std::size_t>(EffectBlock::TimeEffect)] = true;
    parameters.drive = {.mode = 2, .drive = 8, .tone = 9, .level = 12};
    parameters.timeEffects = {.mode = 3, .mix = 8};
    return parameters;
}
} // namespace

TEST_CASE("At the host rate, or at a 44.1 kHz host, the hardware-rate processor is the engine itself")
{
    struct Case
    {
        double sampleRate;
        bool atHardwareRate;
    };
    for (const auto c : {Case{48000.0, false}, Case{96000.0, false}, Case{hardwareRate, true}})
    {
        const auto parameters = reverbAndDrive();
        auto wrapped = makeProcessor(c.sampleRate, 2, c.atHardwareRate, 4, parameters);
        FiveAProcessor engine;
        engine.setParameters(parameters);
        engine.prepare({c.sampleRate, 512, 2, 4});
        engine.setParameters(parameters);
        engine.reset();

        PlanarBuffer viaWrapper{2, 20000};
        viaWrapper.fillWithNoise(40);
        PlanarBuffer viaEngine = viaWrapper;
        processInBlocks(wrapped, viaWrapper, {512});
        for (int start = 0; start < 20000; start += 512)
            engine.process(viaEngine.viewOf(start, std::min(512, 20000 - start)));

        INFO(c.sampleRate << " Hz, hardware rate requested: " << c.atHardwareRate);
        CHECK_FALSE(wrapped.isConverting());
        CHECK(wrapped.getLatencySamples() == engine.getLatencySamples());
        CHECK(viaWrapper.data() == viaEngine.data());
    }
}

TEST_CASE("At the hardware rate, the output is the input delayed by exactly the reported latency")
{
    const double sampleRate = GENERATE(48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    const int oversampling = GENERATE(1, 4);
    const int numChannels = GENERATE(1, 2);

    // Neutral settings: the engine passes its input through, so all that remains is conversion.
    auto processor = makeProcessor(sampleRate, numChannels, true, oversampling);
    REQUIRE(processor.isConverting());

    const int length = static_cast<int>(sampleRate * 0.5);
    PlanarBuffer buffer{numChannels, length};
    fillTones(buffer, sampleRate);
    const auto input = buffer.data();
    processInBlocks(processor, buffer, {512});

    const int latency = processor.getLatencySamples();
    const auto start = static_cast<std::size_t>(latency + static_cast<int>(sampleRate * 0.01));
    const auto end = static_cast<std::size_t>(length);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto& out = buffer.data()[static_cast<std::size_t>(channel)];
        const auto& in = input[static_cast<std::size_t>(channel)];
        INFO(sampleRate << " Hz, drive oversampling " << oversampling << ", channel " << channel << " of "
                        << numChannels << ", latency " << latency);
        CHECK(alignedErrorDb(out, in, latency, start, end) < -90.0);
        // One sample either way is far off: the latency is exact, not merely close.
        CHECK(alignedErrorDb(out, in, latency - 1, start, end) > -30.0);
        CHECK(alignedErrorDb(out, in, latency + 1, start, end) > -30.0);
    }
}

TEST_CASE("At the hardware rate, the latency includes the drive's oversampling")
{
    // The drive's latency is counted at 44.1 kHz, so at the host rate it is longer: 76 samples at
    // 44.1 kHz are 82.7 at 48 kHz.
    const double sampleRate = GENERATE(48000.0, 96000.0, 192000.0);
    const auto plain = makeProcessor(sampleRate, 2, true, 1);
    const auto oversampled = makeProcessor(sampleRate, 2, true, 4);

    FiveAProcessor engine;
    engine.prepare({hardwareRate, 512, 2, 4});
    const double driveSamples = engine.getLatencySamples() * sampleRate / hardwareRate;

    INFO(sampleRate << " Hz");
    CHECK(std::abs(oversampled.getLatencySamples() - plain.getLatencySamples() - driveSamples) <= 1.0);
}

TEST_CASE("At the hardware rate, delay times stay as documented")
{
    // Delay, TIME 3 FINE 0: one echo 300 ms after the dry signal, at any host rate.
    const double sampleRate = GENERATE(48000.0, 96000.0);
    ParameterSnapshot parameters;
    parameters.effectEnabled[static_cast<std::size_t>(EffectBlock::TimeEffect)] = true;
    parameters.timeEffects = {.mode = 7, .time = 3, .fine = 0, .feedback = 0, .mix = 15};
    auto processor = makeProcessor(sampleRate, 1, true, 1, parameters);

    PlanarBuffer buffer{1, static_cast<int>(sampleRate * 0.5)};
    buffer.channel(0)[100] = 1.0f;
    processInBlocks(processor, buffer, {512});

    // The direct sound and the echo each peak on a whole sample: the round trip's delay is a
    // whole number of host samples.
    const auto& out = buffer.channel(0);
    const auto direct = static_cast<std::size_t>(100 + processor.getLatencySamples());
    const auto echoStart = direct + static_cast<std::size_t>(sampleRate * 0.1);
    const auto echo = static_cast<std::size_t>(
        std::max_element(out.begin() + static_cast<std::ptrdiff_t>(echoStart), out.end()) - out.begin());
    INFO(sampleRate << " Hz");
    CHECK(std::max_element(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(echoStart)) - out.begin() ==
          static_cast<std::ptrdiff_t>(direct));
    CHECK(echo == direct + static_cast<std::size_t>(sampleRate * 0.3));
}

TEST_CASE("At the hardware rate, the output does not depend on block size")
{
    const double sampleRate = GENERATE(48000.0, 192000.0);
    auto whole = makeProcessor(sampleRate, 2, true, 4, reverbAndDrive());
    auto pieces = makeProcessor(sampleRate, 2, true, 4, reverbAndDrive());

    PlanarBuffer a{2, 30000};
    a.fillWithNoise(41);
    PlanarBuffer b = a;
    processInBlocks(whole, a, {512});
    processInBlocks(pieces, b, {1, 2, 3, 500, 1, 512, 77});

    INFO(sampleRate << " Hz");
    CHECK(a.data() == b.data());
}

TEST_CASE("At the hardware rate, blocks longer than announced are processed whole")
{
    auto announced = makeProcessor(48000.0, 2, true, 1, reverbAndDrive());
    auto longer = makeProcessor(48000.0, 2, true, 1, reverbAndDrive());

    PlanarBuffer a{2, 12000};
    a.fillWithNoise(42);
    PlanarBuffer b = a;
    processInBlocks(announced, a, {512});
    processInBlocks(longer, b, {4000}); // announced 512
    CHECK(a.data() == b.data());
}

TEST_CASE("At the hardware rate, nothing drifts over a long render")
{
    // Ten minutes at 48 kHz, a minute at the other rates (Debug builds run these too). The input is
    // generated block by block, so nothing long is held in memory; the last second must still
    // line up with the input at the reported latency.
    struct Render
    {
        double sampleRate;
        double seconds;
    };
    const auto render = GENERATE(Render{48000.0, 600.0}, Render{88200.0, 60.0}, Render{96000.0, 60.0},
                                 Render{176400.0, 60.0}, Render{192000.0, 60.0});
    auto processor = makeProcessor(render.sampleRate, 2, true, 4);
    const int latency = processor.getLatencySamples();

    auto tone = [&](int channel, long long n)
    {
        const double t = static_cast<double>(n) / render.sampleRate;
        const double first = channel == 0 ? 997.0 : 1499.0;
        const double second = channel == 0 ? 7001.0 : 11003.0;
        return 0.3 * std::sin(2.0 * std::numbers::pi * first * t) + 0.2 * std::sin(2.0 * std::numbers::pi * second * t);
    };

    const auto length = static_cast<long long>(render.sampleRate * render.seconds);
    const auto checkedFrom = length - static_cast<long long>(render.sampleRate);
    const std::vector<int> blockLengths{480, 512, 1024, 37};
    PlanarBuffer block{2, 1024};
    std::array<double, 2> error{};
    std::array<double, 2> reference{};
    for (long long start = 0, index = 0; start < length; ++index)
    {
        const int count = static_cast<int>(
            std::min<long long>(length - start, blockLengths[static_cast<std::size_t>(index) % blockLengths.size()]));
        for (int channel = 0; channel < 2; ++channel)
            for (int n = 0; n < count; ++n)
                block.channel(channel)[static_cast<std::size_t>(n)] = static_cast<float>(tone(channel, start + n));
        processor.process(block.viewOf(0, count));

        for (int channel = 0; channel < 2; ++channel)
            for (int n = 0; n < count; ++n)
                if (start + n >= checkedFrom)
                {
                    const double expected = static_cast<float>(tone(channel, start + n - latency));
                    const double difference = block.channel(channel)[static_cast<std::size_t>(n)] - expected;
                    error[static_cast<std::size_t>(channel)] += difference * difference;
                    reference[static_cast<std::size_t>(channel)] += expected * expected;
                }
        start += count;
    }

    for (std::size_t channel = 0; channel < 2; ++channel)
    {
        INFO(render.sampleRate << " Hz, " << render.seconds << " s, channel " << channel);
        CHECK(10.0 * std::log10(error[channel] / reference[channel]) < -90.0);
    }
}

TEST_CASE("At the hardware rate, reset makes the output repeatable")
{
    auto processor = makeProcessor(96000.0, 2, true, 4, reverbAndDrive());
    PlanarBuffer first{2, 10000};
    first.fillWithNoise(43);
    PlanarBuffer second = first;
    processInBlocks(processor, first, {512});
    processor.reset();
    processInBlocks(processor, second, {512});
    CHECK(first.data() == second.data());
}

TEST_CASE("A host rate the resampler refuses falls back to the host rate")
{
    // 47993 Hz shares no factor with 44100: the conversion table would be too large.
    auto processor = makeProcessor(47993.0, 2, true, 4);
    FiveAProcessor engine;
    engine.prepare({47993.0, 512, 2, 4});
    CHECK_FALSE(processor.isConverting());
    CHECK(processor.getLatencySamples() == engine.getLatencySamples());
}

TEST_CASE("Settings may arrive before the hardware-rate processor is prepared")
{
    HardwareRateProcessor processor;
    processor.setParameters(reverbAndDrive());
    processor.reset();
    processor.prepare({48000.0, 512, 2, 4}, true);
    PlanarBuffer buffer{2, 2048};
    buffer.fillWithNoise(44);
    processInBlocks(processor, buffer, {512});
    CHECK(fivea::test::allFinite(buffer));
}

TEST_CASE("At the hardware rate, processing and changing settings do not allocate")
{
    auto processor = makeProcessor(48000.0, 2, true, 4, reverbAndDrive());
    PlanarBuffer buffer{2, 512};
    buffer.fillWithNoise(45);
    auto changed = reverbAndDrive();
    changed.timeEffects.mode = 7;

    std::size_t allocations = 0;
    {
        fivea::test::ScopedAllocationCounter counter;
        for (int block = 0; block < 40; ++block)
        {
            processor.setParameters(block % 2 == 0 ? changed : reverbAndDrive());
            processor.process(buffer.view());
        }
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}
