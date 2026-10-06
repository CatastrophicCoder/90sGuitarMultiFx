#pragma once

#include "core/FiveAProcessor.h"
#include "core/ParameterSnapshot.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace fivea
{

// Host-facing adapter: owns the parameters and state, and feeds the engine an allocation-free
// parameter snapshot each block. All signal processing lives in FiveAProcessor.
class PluginProcessor final : public juce::AudioProcessor, private juce::Timer
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

    // The longest reverb voicing rings about 3 s; Delay at maximum F.BACK needs about 20 s to fall
    // 60 dB. Hosts keep processing this long after the input stops.
    double getTailLengthSeconds() const override { return 20.0; }

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

    // The parameters as the engine receives them; for tests and the editor.
    [[nodiscard]] ParameterSnapshot readParameterSnapshot() const noexcept;

    // Message thread: re-prepares the engine if the oversampling setting changed since the last
    // prepare, and reports the new latency. Called by a timer; public so tests can call it.
    void applyOversamplingSetting();

    // The input peak since the last call, after the input trim: the PEAK LED's source. Written by
    // the audio thread, taken (and cleared) by the editor's timer.
    [[nodiscard]] float takeInputPeak() noexcept { return inputPeak.exchange(0.0f, std::memory_order_relaxed); }

    // Which row the panel's slide switch selects (1–6, SRC-001 p. 2). Editor state only: it lives
    // here so it survives the editor being closed and reopened.
    std::atomic<int> selectedRow{1};

private:
    void timerCallback() override { applyOversamplingSetting(); }
    [[nodiscard]] int requestedOversampling() const noexcept;
    [[nodiscard]] static int readStep(const std::atomic<float>* value) noexcept;

    juce::AudioProcessorValueTreeState parameterState;

    std::atomic<float>* inputTrimDb = nullptr;
    std::atomic<float>* outputLevelDb = nullptr;
    std::atomic<float>* globalBypass = nullptr;
    std::array<std::atomic<float>*, numEffectBlocks> effectEnabled{};

    struct DocumentedControls
    {
        std::atomic<float>* compressorSens = nullptr;
        std::atomic<float>* compressorAttack = nullptr;
        std::atomic<float>* compressorLevel = nullptr;
        std::atomic<float>* driveMode = nullptr;
        std::atomic<float>* driveDrive = nullptr;
        std::atomic<float>* driveTone = nullptr;
        std::atomic<float>* driveLevel = nullptr;
        std::atomic<float>* eqBass = nullptr;
        std::atomic<float>* eqMidFrequency = nullptr;
        std::atomic<float>* eqMid = nullptr;
        std::atomic<float>* eqTreble = nullptr;
        std::atomic<float>* eqTrim = nullptr;
        std::atomic<float>* modulationMode = nullptr;
        std::atomic<float>* modulationSpeed = nullptr;
        std::atomic<float>* modulationDepth = nullptr;
        std::atomic<float>* modulationFeedback = nullptr;
        std::atomic<float>* modulationMix = nullptr;
        std::atomic<float>* timeEffectMode = nullptr;
        std::atomic<float>* timeEffectTime = nullptr;
        std::atomic<float>* timeEffectFine = nullptr;
        std::atomic<float>* timeEffectFeedback = nullptr;
        std::atomic<float>* timeEffectMix = nullptr;
        std::atomic<float>* noiseReductionLevel = nullptr;
        std::atomic<float>* master = nullptr;
    } controls;
    std::atomic<float>* driveOversampling = nullptr;

    FiveAProcessor engine;
    std::atomic<float> inputPeak{0.0f};
    double preparedSampleRate = 0.0;
    int preparedBlockSize = 0;
    int preparedOversampling = 1;
    int lastLoadedSchemaVersion = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace fivea
