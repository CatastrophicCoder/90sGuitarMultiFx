#include "../Unit/AllocationGuard.h"

#include "core/FactoryPrograms.h"
#include "core/ParameterIds.h"
#include "plugin/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <functional>
#include <numbers>
#include <random>

using fivea::PluginProcessor;
using fivea::Program;
using fivea::ProgramLocation;
using fivea::test::ScopedAllocationCounter;
namespace ParameterIds = fivea::ParameterIds;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int fadeSamples = 480;        // 10 ms at 48 kHz (SwitchingProfile)
constexpr ProgramLocation allOff{1, 5}; // the tests write a program with every effect off here

struct Setup
{
    juce::ScopedJuceInitialiser_GUI juce;
    PluginProcessor processor;

    Setup()
    {
        REQUIRE(processor.getProgramState().bank.write(allOff, Program{}));
        setPlain(ParameterIds::driveOversampling, 0.0f); // no latency: these tests compare sample by sample
        processor.setPlayConfigDetails(2, 2, sampleRate, blockSize);
        processor.prepareToPlay(sampleRate, blockSize);
        processor.selectProgram(allOff);
        processor.reset(); // start settled on it, without the dip
    }

    float plain(const char* id)
    {
        auto* parameter = processor.getParameterState().getParameter(id);
        return parameter->convertFrom0to1(parameter->getValue());
    }

    void setPlain(const char* id, float value)
    {
        auto* parameter = processor.getParameterState().getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }

    // Runs `input` through processBlock in blocks; `beforeBlock(index)` runs before each block, as
    // the message thread would between callbacks.
    juce::AudioBuffer<float> run(const juce::AudioBuffer<float>& input, const std::function<void(int)>& beforeBlock)
    {
        juce::AudioBuffer<float> output{input};
        juce::MidiBuffer midi;
        for (int start = 0, index = 0; start < output.getNumSamples(); start += blockSize, ++index)
        {
            beforeBlock(index);
            const int length = std::min(blockSize, output.getNumSamples() - start);
            juce::AudioBuffer<float> block{output.getArrayOfWritePointers(), 2, start, length};
            processor.processBlock(block, midi);
        }
        return output;
    }
};

juce::AudioBuffer<float> sine(int numSamples, double frequency, float amplitude, double phase = 0.0)
{
    juce::AudioBuffer<float> buffer{2, numSamples};
    for (int n = 0; n < numSamples; ++n)
    {
        const auto value =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * n / sampleRate + phase));
        buffer.setSample(0, n, value);
        buffer.setSample(1, n, value);
    }
    return buffer;
}

juce::AudioBuffer<float> noise(int numSamples, unsigned int seed, float amplitude)
{
    juce::AudioBuffer<float> buffer{2, numSamples};
    std::mt19937 generator{seed};
    std::uniform_real_distribution<float> distribution{-amplitude, amplitude};
    for (int channel = 0; channel < 2; ++channel)
        for (int n = 0; n < numSamples; ++n)
            buffer.setSample(channel, n, distribution(generator));
    return buffer;
}

double largestSecondDifference(const juce::AudioBuffer<float>& buffer, int from, int to)
{
    double largest = 0.0;
    for (int n = std::max(from, 2); n < to; ++n)
        largest =
            std::max(largest, static_cast<double>(std::abs(buffer.getSample(0, n) - 2.0f * buffer.getSample(0, n - 1) +
                                                           buffer.getSample(0, n - 2))));
    return largest;
}

// As in Milestone 1's switching tests: a 43 Hz sine, changed at a sine peak; the largest second
// difference across the change against the larger of the two settled states'.
double programChangeClickRatio(ProgramLocation from, ProgramLocation to)
{
    Setup setup;
    setup.processor.selectProgram(from);
    setup.processor.reset();

    const int changeBlock = 375; // 96 000 samples in: 2 s
    const int change = changeBlock * blockSize;
    const double peakPhase = std::numbers::pi / 2.0 - 2.0 * std::numbers::pi * 43.0 * change / sampleRate;
    const auto output = setup.run(sine(4 * static_cast<int>(sampleRate), 43.0, 0.05f, peakPhase),
                                  [&](int index)
                                  {
                                      if (index == changeBlock)
                                          setup.processor.selectProgram(to);
                                  });
    const int end = output.getNumSamples();
    return largestSecondDifference(output, change, change + 24000) /
           std::max(largestSecondDifference(output, change - 24000, change),
                    largestSecondDifference(output, end - 24000, end));
}
} // namespace

TEST_CASE("The host sees the 30 programs, named by slot")
{
    Setup setup;
    auto& processor = setup.processor;
    CHECK(processor.getNumPrograms() == 30);
    CHECK(processor.getProgramName(5) == "2-1 METAL 1");
    CHECK(processor.getProgramName(29).startsWith("6-5 DEV "));
    CHECK(processor.getProgramName(4) == "1-5 "); // the tests' unnamed all-off program

    processor.setCurrentProgram(5);
    CHECK(processor.getCurrentProgram() == 5);
    CHECK(processor.getProgramState().selection.selected() == ProgramLocation{2, 1});
    CHECK(setup.plain(ParameterIds::driveDrive) == 14.0f);
    CHECK(setup.plain(ParameterIds::driveMode) == 0.0f);      // choice index 0: Distortion, mode 1
    CHECK(setup.plain(ParameterIds::timeEffectMode) == 3.0f); // Plate, mode 4
    CHECK(setup.plain(ParameterIds::equaliserBass) == 6.0f);
    CHECK(setup.plain(ParameterIds::compressorEnabled) == 0.0f);
    CHECK(setup.plain(ParameterIds::driveEnabled) == 1.0f);
}

TEST_CASE("Only bank 1 programs can be renamed by the host")
{
    Setup setup;
    auto& processor = setup.processor;
    processor.changeProgramName(1, "My Clean");
    processor.changeProgramName(5, "Not METAL");
    CHECK(processor.getProgramName(1) == "1-2 My Clean");
    CHECK(processor.getProgramName(5) == "2-1 METAL 1");
}

TEST_CASE("A new instance plays program 1-1, as the original does at power-on")
{
    juce::ScopedJuceInitialiser_GUI juce;
    PluginProcessor processor;
    CHECK(processor.getProgramState().selection.selected() == ProgramLocation{1, 1});
    CHECK(fivea::sameSettings(processor.getEditedProgram(), fivea::factoryPrograms()[0]));
}

TEST_CASE("Selecting a program discards unwritten edits")
{
    Setup setup;
    auto& processor = setup.processor;
    processor.selectProgram({2, 1});
    setup.setPlain(ParameterIds::driveDrive, 3.0f);
    setup.setPlain(ParameterIds::modulationEnabled, 1.0f);
    CHECK_FALSE(fivea::sameSettings(processor.getEditedProgram(), processor.getStoredProgram()));

    processor.selectProgram({3, 1});
    processor.selectProgram({2, 1});
    CHECK(setup.plain(ParameterIds::driveDrive) == 14.0f);
    CHECK(setup.plain(ParameterIds::modulationEnabled) == 0.0f);
    CHECK(fivea::sameSettings(processor.getEditedProgram(), processor.getStoredProgram()));
    CHECK(processor.getStoredProgram() == fivea::factoryPrograms()[5]); // the stored program is untouched
}

TEST_CASE("Program Write stores the edits in bank 1 and selects the written slot")
{
    Setup setup;
    auto& processor = setup.processor;
    processor.selectProgram({2, 1});
    setup.setPlain(ParameterIds::driveDrive, 3.0f);
    setup.setPlain(ParameterIds::inputTrim, -6.0f); // not program data

    CHECK(processor.writeProgram({1, 3}));
    const auto& written = processor.getProgramState().bank.at({1, 3});
    CHECK(written.name.view() == "METAL 1");
    CHECK(written.drive.drive == 3);
    CHECK(written.equaliser.bass == 6);
    CHECK(processor.getProgramState().selection.selected() == ProgramLocation{1, 3});
    CHECK(fivea::sameSettings(processor.getEditedProgram(), processor.getStoredProgram()));
    CHECK(processor.getProgramState().bank.at({2, 1}) == fivea::factoryPrograms()[5]);

    const auto before = processor.getProgramState().bank.allSlots();
    CHECK_FALSE(processor.writeProgram({2, 2}));
    CHECK(processor.getProgramState().bank.allSlots() == before);
    CHECK(processor.getProgramState().selection.selected() == ProgramLocation{1, 3});
}

TEST_CASE("While fading out, the old program plays: the new one is not heard before the dip")
{
    Setup setup;
    const auto input = sine(8 * blockSize, 220.0, 0.2f);
    const auto output = setup.run(input,
                                  [&](int index)
                                  {
                                      if (index == 2)
                                          setup.processor.selectProgram({2, 1}); // METAL 1
                                  });

    // Blocks 0–1: all off, so the input itself. Then the fade-out, still all off: input × gain.
    const int change = 2 * blockSize;
    for (int n = 0; n < change; ++n)
        REQUIRE(output.getSample(0, n) == input.getSample(0, n));
    for (int n = 0; n < fadeSamples; ++n)
    {
        const auto gain = 0.5 - 0.5 * std::cos(std::numbers::pi * (fadeSamples - 1 - n) / fadeSamples);
        INFO("fade-out sample " << n);
        REQUIRE_THAT(output.getSample(0, change + n),
                     Catch::Matchers::WithinAbs(input.getSample(0, change + n) * gain, 1.0e-6));
    }
    CHECK(output.getSample(0, change + fadeSamples - 1) == 0.0f); // the silent point

    // The new program fades in from the very next sample, not from the next block: the gap stays
    // one sample long whatever the host's block size (here the fade ends mid-block).
    REQUIRE((change + fadeSamples) % blockSize != 0);
    CHECK(output.getMagnitude(0, change + fadeSamples, 8) > 0.0f);

    // After the fade-in, METAL 1's distortion: far from the clean input.
    double difference = 0.0;
    for (int n = change + 2 * fadeSamples; n < output.getNumSamples(); ++n)
        difference =
            std::max(difference, static_cast<double>(std::abs(output.getSample(0, n) - input.getSample(0, n))));
    CHECK(difference > 0.05);
}

TEST_CASE("A program change cuts the old program's tails")
{
    Setup setup;
    setup.processor.selectProgram({5, 2}); // Big Hall
    setup.processor.reset();

    const int loud = 1 * static_cast<int>(sampleRate);
    auto input = noise(loud + 2 * static_cast<int>(sampleRate), 8, 0.3f);
    for (int channel = 0; channel < 2; ++channel)
        input.clear(channel, loud, input.getNumSamples() - loud); // then silence

    const int changeBlock = loud / blockSize + 4; // a few blocks into the silence: the hall still rings
    const auto output = setup.run(input,
                                  [&](int index)
                                  {
                                      if (index == changeBlock)
                                          setup.processor.selectProgram(allOff);
                                  });

    const int change = changeBlock * blockSize;
    CHECK(output.getMagnitude(0, change - blockSize, blockSize) > 1.0e-3f); // the tail was there
    CHECK(output.getMagnitude(0, change + fadeSamples, output.getNumSamples() - change - fadeSamples) == 0.0f);
}

TEST_CASE("Program changes do not click")
{
    // Measured 2026-10-07, into and out of each factory program from one with every effect off:
    // 0.04 to 8.6 with the raised-cosine dip and the fade-in applied to the effects' input. With a
    // linear dip on the output, the chorus, reverb and delay programs measured up to 22 164 (a
    // restarted delay line records the cut-in signal as a step and plays it back after the fade);
    // linear with the input fade-in, up to 164 (the fade's corners). Do not raise the bound without
    // asking.
    constexpr double programChangeClickBound = 20.0;
    for (int slot = 5; slot < fivea::numProgramSlots; ++slot)
    {
        const ProgramLocation location{slot / 5 + 1, slot % 5 + 1};
        INFO(location.bank << "-" << location.program);
        CHECK(programChangeClickRatio(allOff, location) < programChangeClickBound);
        CHECK(programChangeClickRatio(location, allOff) < programChangeClickBound);
    }
}

TEST_CASE("With global bypass on, a program change leaves the dry signal untouched")
{
    Setup setup;
    setup.setPlain(ParameterIds::globalBypass, 1.0f);
    setup.processor.reset();
    const auto input = noise(16 * blockSize, 5, 0.5f);
    const auto output = setup.run(input,
                                  [&](int index)
                                  {
                                      if (index == 3)
                                          setup.processor.selectProgram({2, 1});
                                      if (index == 4)
                                          setup.processor.selectProgram({6, 5});
                                  });
    for (int n = 0; n < output.getNumSamples(); ++n)
        REQUIRE(output.getSample(0, n) == input.getSample(0, n));
    CHECK(setup.plain(ParameterIds::driveDrive) == 8.0f); // 6-5's settings are in place
}

TEST_CASE("A second program change during the dip ends on the second program")
{
    Setup setup;
    const auto input = sine(40 * blockSize, 220.0, 0.2f);
    const auto output = setup.run(input,
                                  [&](int index)
                                  {
                                      if (index == 2)
                                          setup.processor.selectProgram({2, 1});
                                      if (index == 3) // mid fade-out
                                          setup.processor.selectProgram({4, 1});
                                  });
    CHECK(setup.processor.getProgramState().selection.selected() == ProgramLocation{4, 1});
    CHECK(fivea::sameSettings(setup.processor.getEditedProgram(), fivea::factoryPrograms()[15]));
    for (int n = 0; n < output.getNumSamples(); ++n)
        REQUIRE(std::isfinite(output.getSample(0, n)));
}

TEST_CASE("processBlock does not allocate across a program change")
{
    Setup setup;
    juce::AudioBuffer<float> buffer{2, blockSize};
    juce::MidiBuffer midi;
    std::size_t allocations = 1;
    setup.processor.selectProgram({6, 5});
    {
        const ScopedAllocationCounter counter;
        for (int block = 0; block < 12; ++block) // fade-out, switch, fade-in
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int n = 0; n < blockSize; ++n)
                    buffer.setSample(channel, n, 0.1f * std::sin(0.05f * static_cast<float>(n)));
            setup.processor.processBlock(buffer, midi);
        }
        allocations = counter.count();
    }
    CHECK(allocations == 0);
    CHECK(setup.processor.getProgramState().selection.selected() == ProgramLocation{6, 5});
}
