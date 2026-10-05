#include "plugin/PluginProcessor.h"

#include "core/ParameterIds.h"
#include "core/ParameterLayout.h"
#include "core/PresetState.h"
#include "plugin/PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace fivea
{

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameterState(*this, nullptr, "Parameters", createParameterLayout())
{
    inputTrimDb = parameterState.getRawParameterValue(ParameterIds::inputTrim);
    outputLevelDb = parameterState.getRawParameterValue(ParameterIds::outputLevel);
    globalBypass = parameterState.getRawParameterValue(ParameterIds::globalBypass);

    for (std::size_t block = 0; block < numEffectBlocks; ++block)
        effectEnabled[block] = parameterState.getRawParameterValue(ParameterIds::effectEnabled[block]);

    auto raw = [this](const char* id)
    {
        return parameterState.getRawParameterValue(id);
    };
    using namespace ParameterIds;
    controls = {raw(compressorSens),     raw(compressorAttack),    raw(compressorLevel),
                raw(driveMode),          raw(driveDrive),          raw(driveTone),
                raw(driveLevel),         raw(equaliserBass),       raw(equaliserMidFrequency),
                raw(equaliserMid),       raw(equaliserTreble),     raw(equaliserTrim),
                raw(modulationMode),     raw(modulationSpeed),     raw(modulationDepth),
                raw(modulationFeedback), raw(modulationMix),       raw(timeEffectMode),
                raw(timeEffectTime),     raw(timeEffectFine),      raw(timeEffectFeedback),
                raw(timeEffectMix),      raw(noiseReductionLevel), raw(master)};
    driveOversampling = raw(ParameterIds::driveOversampling);

    startTimerHz(10);
}

int PluginProcessor::readStep(const std::atomic<float>* value) noexcept
{
    return static_cast<int>(std::lround(value->load(std::memory_order_relaxed)));
}

int PluginProcessor::requestedOversampling() const noexcept
{
    constexpr int factors[] = {1, 2, 4}; // choices Off, 2x, 4x
    return factors[std::clamp(readStep(driveOversampling), 0, 2)];
}

void PluginProcessor::applyOversamplingSetting()
{
    if (preparedSampleRate <= 0.0 || requestedOversampling() == preparedOversampling)
        return;

    // Allocates, so not on the audio thread; processing is held off while it happens.
    suspendProcessing(true);
    prepareToPlay(preparedSampleRate, preparedBlockSize);
    suspendProcessing(false);
    updateHostDisplay(ChangeDetails{}.withLatencyChanged(true));
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Mono in → stereo out is the usual guitar-plugin case. The original's outputs are L/mono and R
    // (evidence register EV-004); its input configuration is not yet recorded, and where its signal
    // becomes stereo is unknown (EV-005).
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output == juce::AudioChannelSet::mono())
        return input == juce::AudioChannelSet::mono();

    if (output == juce::AudioChannelSet::stereo())
        return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();

    return false;
}

void PluginProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    preparedSampleRate = sampleRate;
    preparedBlockSize = maximumExpectedSamplesPerBlock;
    preparedOversampling = requestedOversampling();

    engine.setParameters(readParameterSnapshot());
    engine.prepare({sampleRate, maximumExpectedSamplesPerBlock, getTotalNumOutputChannels(), preparedOversampling});
    engine.setParameters(readParameterSnapshot());
    engine.reset();
    setLatencySamples(engine.getLatencySamples());
}

void PluginProcessor::reset()
{
    engine.reset();
}

ParameterSnapshot PluginProcessor::readParameterSnapshot() const noexcept
{
    ParameterSnapshot snapshot;
    snapshot.inputTrimDb = inputTrimDb->load(std::memory_order_relaxed);
    snapshot.outputLevelDb = outputLevelDb->load(std::memory_order_relaxed);
    snapshot.globalBypass = globalBypass->load(std::memory_order_relaxed) >= 0.5f;

    for (std::size_t block = 0; block < numEffectBlocks; ++block)
        snapshot.effectEnabled[block] = effectEnabled[block]->load(std::memory_order_relaxed) >= 0.5f;

    // Choices hold an index from 0; the documented modes count from 1.
    const auto& c = controls;
    snapshot.compressor = {readStep(c.compressorSens), readStep(c.compressorAttack), readStep(c.compressorLevel)};
    snapshot.drive = {readStep(c.driveMode) + 1, readStep(c.driveDrive), readStep(c.driveTone), readStep(c.driveLevel)};
    snapshot.equaliser = {readStep(c.eqBass), readStep(c.eqMidFrequency), readStep(c.eqMid), readStep(c.eqTreble),
                          readStep(c.eqTrim)};
    snapshot.modulation = {readStep(c.modulationMode) + 1, readStep(c.modulationSpeed), readStep(c.modulationDepth),
                           readStep(c.modulationFeedback), readStep(c.modulationMix)};
    snapshot.timeEffects = {readStep(c.timeEffectMode) + 1, readStep(c.timeEffectTime), readStep(c.timeEffectFine),
                            readStep(c.timeEffectFeedback), readStep(c.timeEffectMix)};
    snapshot.noiseReductionLevel = readStep(c.noiseReductionLevel);
    snapshot.master = readStep(c.master);
    return snapshot;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Output-only channels hold garbage on entry. A mono input feeds every output so mono in
    // produces a valid stereo pair; the stereo-placement of later blocks is a placeholder
    // (evidence register EV-005).
    for (int channel = inputChannels; channel < outputChannels; ++channel)
    {
        if (inputChannels == 1)
            buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
        else
            buffer.clear(channel, 0, numSamples);
    }

    engine.setParameters(readParameterSnapshot());
    engine.process({buffer.getArrayOfWritePointers(), outputChannels, numSamples});
}

juce::AudioProcessorParameter* PluginProcessor::getBypassParameter() const
{
    return parameterState.getParameter(ParameterIds::globalBypass);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = state::toXml(*this))
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Corrupt or foreign data leaves the current settings untouched.
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto result = state::fromXml(*xml, *this);
        if (result.loaded)
            lastLoadedSchemaVersion = result.schemaVersion;
    }
}

} // namespace fivea

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fivea::PluginProcessor();
}
