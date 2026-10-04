#pragma once

#include "core/A5Processor.h"
#include "core/ParameterSnapshot.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace a5
{

// Host-facing adapter: owns the parameters and state, and feeds the engine an allocation-free
// parameter snapshot each block. All signal processing lives in A5Processor.
class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    // No effect exists yet, so nothing rings on. Revisit when delay and reverb arrive.
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Hosts that bypass through this parameter keep automation of it, and the engine ramps it.
    juce::AudioProcessorParameter* getBypassParameter() const override;

    [[nodiscard]] juce::AudioProcessorValueTreeState& getParameterState() noexcept { return parameterState; }
    [[nodiscard]] int getLastLoadedSchemaVersion() const noexcept { return lastLoadedSchemaVersion; }

private:
    [[nodiscard]] ParameterSnapshot readParameterSnapshot() const noexcept;

    juce::AudioProcessorValueTreeState parameterState;

    std::atomic<float>* inputTrimDb = nullptr;
    std::atomic<float>* outputLevelDb = nullptr;
    std::atomic<float>* globalBypass = nullptr;
    std::array<std::atomic<float>*, numEffectBlocks> effectEnabled{};

    A5Processor engine;
    int lastLoadedSchemaVersion = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace a5
