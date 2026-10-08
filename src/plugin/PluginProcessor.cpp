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
    processingRate = raw(ParameterIds::processingRate);

    // Like the original at power-on (EV-029), a new instance plays the selected program, 1-1.
    // Nothing is processing yet, so no dip is needed. A host restoring a session overrides it.
    loadIntoParameters(getStoredProgram());

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

bool PluginProcessor::requestedHardwareRate() const noexcept
{
    return readStep(processingRate) == 1; // choices Host rate, 44.1 kHz
}

void PluginProcessor::applyLatencySettings()
{
    if (preparedSampleRate <= 0.0 ||
        (requestedOversampling() == preparedOversampling && requestedHardwareRate() == preparedAtHardwareRate))
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
    preparedAtHardwareRate = requestedHardwareRate();

    engine.setParameters(readParameterSnapshot());
    engine.prepare({sampleRate, maximumExpectedSamplesPerBlock, getTotalNumOutputChannels(), preparedOversampling},
                   preparedAtHardwareRate);
    engine.setParameters(readParameterSnapshot());
    engine.reset();
    lastApplied = readParameterSnapshot();
    const auto& switching = functionalPlaceholderProfile.switching;
    programTransition.prepare(sampleRate, switching.programFadeOutSeconds, switching.programFadeInSeconds);
    setLatencySamples(engine.getLatencySamples());
}

void PluginProcessor::reset()
{
    // Settle on the parameters as they are now, not on those of the last processed block.
    lastApplied = readParameterSnapshot();
    engine.setParameters(lastApplied);
    engine.reset();
    programTransition.reset();
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

    const auto snapshot = readParameterSnapshot();

    // PEAK LED: after the input trim, before the effects, like the original's (it follows the
    // INPUT level control and gain stage, ahead of the A/D: SRC-002 PDF p. 10).
    float peak = 0.0f;
    for (int channel = 0; channel < outputChannels; ++channel)
        peak = std::max(peak, buffer.getMagnitude(channel, 0, numSamples));
    peak *= decibelsToGain(sanitiseGainDb(snapshot.inputTrimDb));
    float previous = inputPeak.load(std::memory_order_relaxed);
    while (peak > previous && !inputPeak.compare_exchange_weak(previous, peak, std::memory_order_relaxed))
    {
    }

    // A program change: hold the old program while fading out, switch once silent and the new
    // parameters are all set, then fade in (ProgramTransition). Without one, this is a single pass.
    if (const bool wasHolding = programTransition.holdsSettings();
        programTransition.poll(!snapshot.globalBypass) && !wasHolding)
        heldProgram = programFrom(lastApplied);

    auto live = snapshot;
    std::array<float*, 2> pointers{};
    const int channels = std::min(outputChannels, static_cast<int>(pointers.size()));
    for (int start = 0; start < numSamples;)
    {
        if (programTransition.readyToSwitch())
        {
            const bool cutTails = programTransition.isSilent();
            programTransition.switched();
            live = readParameterSnapshot();
            engine.setParameters(live);
            if (cutTails)
                engine.reset(); // settles every ramp on the new program, and clears the old tails
        }

        int length = numSamples - start;
        if (programTransition.isFadingOut())
            length = std::min(length, programTransition.samplesUntilSilent());

        auto settings = live;
        if (programTransition.holdsSettings())
            applyProgram(heldProgram, settings);
        engine.setParameters(settings);
        lastApplied = settings;

        for (int channel = 0; channel < channels; ++channel)
            pointers[static_cast<std::size_t>(channel)] = buffer.getWritePointer(channel, start);
        const AudioBufferView part{pointers.data(), channels, length};
        if (programTransition.isFadingIn())
        {
            programTransition.applyGain(part); // fades the input in (see ProgramTransition)
            engine.process(part);
        }
        else
        {
            if (!programTransition.isSilent())
                engine.process(part);
            programTransition.applyGain(part);
        }
        start += length;
    }
}

void PluginProcessor::loadIntoParameters(const Program& program)
{
    auto set = [this](const char* id, int value)
    {
        auto* parameter = parameterState.getParameter(id);
        // Choices (the MODEs) hold an index from 0; programs hold the documented step from 1.
        const bool isChoice = dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr;
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(isChoice ? value - 1 : value)));
    };

    for (std::size_t block = 0; block < numEffectBlocks; ++block)
        set(ParameterIds::effectEnabled[block], program.effectEnabled[block] ? 1 : 0);
    for (int index = 0; index < numProgramControls; ++index)
    {
        const auto control = static_cast<ProgramControl>(index);
        set(parameterIdFor(control), controlValue(program, control));
    }
}

void PluginProcessor::selectProgram(ProgramLocation location)
{
    programState.selection.select(location);
    const auto ticket = programTransition.post();
    loadIntoParameters(programState.bank.at(programState.selection.selected()));
    programTransition.complete(ticket);
}

bool PluginProcessor::writeProgram(ProgramLocation destination)
{
    const auto program = programFrom(readParameterSnapshot(), getStoredProgram().name);
    if (!programState.bank.write(destination, program))
        return false;
    programState.selection.select(destination);
    return true;
}

const Program& PluginProcessor::getStoredProgram() const noexcept
{
    return programState.bank.at(programState.selection.selected());
}

Program PluginProcessor::getEditedProgram() const noexcept
{
    return programFrom(readParameterSnapshot(), getStoredProgram().name);
}

namespace
{
ProgramLocation locationOfSlot(int index)
{
    const int slot = std::clamp(index, 0, numProgramSlots - 1);
    return {slot / programsPerBank + 1, slot % programsPerBank + 1};
}
} // namespace

void PluginProcessor::setCurrentProgram(int index)
{
    selectProgram(locationOfSlot(index));
}

const juce::String PluginProcessor::getProgramName(int index)
{
    const auto location = locationOfSlot(index);
    return juce::String(location.bank) + "-" + juce::String(location.program) + " " +
           juce::String{std::string{programState.bank.at(location).name.view()}};
}

void PluginProcessor::changeProgramName(int index, const juce::String& newName)
{
    const auto location = locationOfSlot(index);
    auto program = programState.bank.at(location);
    program.name = ProgramName::from(newName.toStdString());
    programState.bank.write(location, program); // refused outside bank 1
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
    if (const auto xml = state::toXml(*this, programState))
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Corrupt or foreign data leaves the current settings untouched.
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto result = state::fromXml(*xml, *this, programState);
        if (result.loaded)
            lastLoadedSchemaVersion = result.schemaVersion;
    }
}

} // namespace fivea

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fivea::PluginProcessor();
}
