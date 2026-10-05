#include "TestSignals.h"

#include "core/FiveAProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

using namespace fivea;

namespace
{

using TestBuffer = fivea::test::PlanarBuffer;
using fivea::test::allFinite;

void processInBlocks(FiveAProcessor& processor, TestBuffer& buffer, int blockSize)
{
    for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        processor.process(buffer.viewOf(start, std::min(blockSize, buffer.getNumSamples() - start)));
}

} // namespace

TEST_CASE("Default settings pass audio through bit for bit")
{
    const double sampleRate = GENERATE(44100.0, 48000.0, 88200.0, 96000.0, 192000.0);
    const int numChannels = GENERATE(1, 2);
    const int blockSize = GENERATE(1, 64, 512);

    FiveAProcessor processor;
    processor.prepare({sampleRate, blockSize, numChannels});

    TestBuffer buffer{numChannels, 4096};
    buffer.fillWithNoise(1);
    const auto input = buffer.data();

    processInBlocks(processor, buffer, blockSize);

    CHECK(buffer.data() == input);
}

TEST_CASE("Silence in gives finite silence out, even with gain applied")
{
    FiveAProcessor processor;
    processor.prepare({48000.0, 256, 2});

    ParameterSnapshot parameters;
    parameters.inputTrimDb = 24.0f;
    parameters.outputLevelDb = 24.0f;
    processor.setParameters(parameters);

    TestBuffer buffer{2, 48000};
    processInBlocks(processor, buffer, 256);

    REQUIRE(allFinite(buffer));
    for (const auto& channel : buffer.data())
        for (const float sample : channel)
            REQUIRE(sample == 0.0f);
}

TEST_CASE("Zero-length blocks are accepted")
{
    FiveAProcessor processor;
    processor.prepare({44100.0, 512, 2});

    TestBuffer buffer{2, 1};
    processor.process(buffer.viewOf(0, 0));
    processor.process({nullptr, 0, 0});

    CHECK(buffer.data()[0][0] == 0.0f);
}

TEST_CASE("Reset makes processing deterministic")
{
    FiveAProcessor processor;
    processor.prepare({44100.0, 128, 2});

    ParameterSnapshot parameters;
    parameters.inputTrimDb = -6.0f;
    parameters.outputLevelDb = 3.0f;

    // The host's order: parameters first, then reset settles every ramp on them.
    auto render = [&]
    {
        processor.setParameters(parameters);
        processor.reset();
        TestBuffer buffer{2, 2048};
        buffer.fillWithNoise(7);
        processInBlocks(processor, buffer, 128);
        return buffer.data();
    };

    // Leave the processor mid-ramp before the first render, so reset has something to undo.
    ParameterSnapshot other;
    other.inputTrimDb = 12.0f;
    processor.setParameters(other);
    TestBuffer warmup{2, 100};
    warmup.fillWithNoise(3);
    processor.process(warmup.view());

    const auto first = render();
    const auto second = render();

    CHECK(first == second);
}

TEST_CASE("Gain settles at the requested level after the ramp")
{
    const double sampleRate = 48000.0;
    FiveAProcessor processor;
    processor.prepare({sampleRate, 512, 1});

    ParameterSnapshot parameters;
    parameters.inputTrimDb = 6.0f;
    parameters.outputLevelDb = -12.0f;
    processor.setParameters(parameters);

    const int rampSamples = static_cast<int>(sampleRate * FiveAProcessor::gainRampSeconds);
    TestBuffer buffer{1, rampSamples + 100};
    buffer.fill(1.0f);

    processInBlocks(processor, buffer, 512);

    const float expected = std::pow(10.0f, 6.0f / 20.0f) * std::pow(10.0f, -12.0f / 20.0f);
    CHECK_THAT(buffer.data()[0].back(), Catch::Matchers::WithinRel(expected, 1.0e-6f));
}

TEST_CASE("Global bypass returns to exact pass-through once its ramp ends")
{
    FiveAProcessor processor;
    processor.prepare({44100.0, 64, 2});

    ParameterSnapshot parameters;
    parameters.inputTrimDb = 9.0f;
    parameters.outputLevelDb = -3.0f;
    processor.setParameters(parameters);
    processor.reset();

    parameters.globalBypass = true;
    processor.setParameters(parameters);

    const int rampSamples = static_cast<int>(44100.0 * FiveAProcessor::gainRampSeconds);
    TestBuffer ramp{2, rampSamples};
    processInBlocks(processor, ramp, 64);

    TestBuffer buffer{2, 1024};
    buffer.fillWithNoise(11);
    const auto input = buffer.data();
    processInBlocks(processor, buffer, 64);

    CHECK(buffer.data() == input);
}

TEST_CASE("Output does not depend on how the host splits blocks")
{
    ParameterSnapshot parameters;
    parameters.inputTrimDb = -9.0f;
    parameters.outputLevelDb = 4.5f;

    auto render = [&](int blockSize)
    {
        FiveAProcessor processor;
        processor.prepare({96000.0, blockSize, 2});
        processor.setParameters(parameters);
        TestBuffer buffer{2, 8192};
        buffer.fillWithNoise(5);
        processInBlocks(processor, buffer, blockSize);
        return buffer.data();
    };

    const auto reference = render(1);
    CHECK(render(17) == reference);
    CHECK(render(512) == reference);
}

TEST_CASE("Out-of-range and non-finite gains are sanitised")
{
    CHECK(sanitiseGainDb(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    CHECK(sanitiseGainDb(std::numeric_limits<float>::infinity()) == 0.0f);
    CHECK(sanitiseGainDb(100.0f) == 24.0f);
    CHECK(sanitiseGainDb(-100.0f) == -24.0f);
    CHECK(sanitiseGainDb(3.6e-7f) == 0.0f);
    CHECK(sanitiseGainDb(-2.96f) == -3.0f);

    FiveAProcessor processor;
    processor.prepare({44100.0, 32, 1});

    ParameterSnapshot parameters;
    parameters.inputTrimDb = std::numeric_limits<float>::quiet_NaN();
    parameters.outputLevelDb = -std::numeric_limits<float>::infinity();
    processor.setParameters(parameters);

    TestBuffer buffer{1, 2048};
    buffer.fillWithNoise(2);
    const auto input = buffer.data();
    processInBlocks(processor, buffer, 32);

    CHECK(allFinite(buffer));
    CHECK(buffer.data() == input);
}
