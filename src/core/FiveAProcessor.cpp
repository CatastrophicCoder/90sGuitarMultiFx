#include "core/FiveAProcessor.h"

#include "core/ParameterIds.h"
#include "core/StepMapping.h"
#include "dsp/DenormalGuard.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fivea
{

namespace
{
const FiveAModelProfile& modelProfile = functionalPlaceholderProfile;

void copyInto(AudioBufferView destination, AudioBufferView source) noexcept
{
    for (int channel = 0; channel < source.getNumChannels(); ++channel)
        std::memcpy(destination.getChannel(channel), source.getChannel(channel),
                    sizeof(float) * static_cast<std::size_t>(source.getNumSamples()));
}
} // namespace

float sanitiseGainDb(float gainDb) noexcept
{
    if (!std::isfinite(gainDb))
        return 0.0f;

    // Snapped to the parameter's step grid. A host's normalised value does not map back exactly
    // (0 dB returns from a saved state as 3.6e-7 dB), and only an exact 0 dB is bit-transparent.
    const float clamped = std::clamp(gainDb, ParameterRanges::minimumGainDb, ParameterRanges::maximumGainDb);
    return std::round(clamped * ParameterRanges::gainStepsPerDb) / ParameterRanges::gainStepsPerDb;
}

float decibelsToGain(float gainDb) noexcept
{
    // Exactly 1 at 0 dB, so the default settings are bit-transparent.
    return gainDb == 0.0f ? 1.0f : std::pow(10.0f, gainDb / 20.0f);
}

// --- LatencyCompensation -------------------------------------------------------------------------

void FiveAProcessor::LatencyCompensation::prepare(int latencySamples, int numChannels)
{
    latency = std::max(0, latencySamples);
    if (latency > 0)
    {
        line.prepare(latency + 4, numChannels);
        line.setInterpolation(dsp::Interpolation::Linear); // whole-sample reads are exact
    }
}

void FiveAProcessor::LatencyCompensation::process(AudioBufferView buffer) noexcept
{
    if (latency == 0)
        return;

    const auto delay = static_cast<float>(latency);
    for (int n = 0; n < buffer.getNumSamples(); ++n)
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float& sample = buffer.getChannel(channel)[n];
            const float delayed = line.read(channel, delay);
            line.write(channel, sample);
            sample = delayed;
        }
}

// --- FiveAProcessor ------------------------------------------------------------------------------

void FiveAProcessor::prepare(const ProcessSpec& spec)
{
    preparedSpec = spec;
    const int channels = std::clamp(spec.numChannels, 1, dsp::DelayLine::maximumChannels);
    preparedSpec.numChannels = channels;
    chunkSize = std::max(spec.maximumBlockSize, 64);

    for (auto* storage : {&blockDry, &globalDry})
        storage->assign(static_cast<std::size_t>(channels), std::vector<float>(static_cast<std::size_t>(chunkSize)));
    blockDryPointers.assign(static_cast<std::size_t>(channels), nullptr);
    fadeGains.assign(static_cast<std::size_t>(chunkSize), 0.0f);
    globalDryPointers.assign(static_cast<std::size_t>(channels), nullptr);

    noiseReduction.prepare(spec.sampleRate, channels, modelProfile);
    compressor.prepare(spec.sampleRate, channels, modelProfile);
    drive.prepare(spec.sampleRate, channels, modelProfile, spec.driveOversampling);
    equaliser.prepare(spec.sampleRate, channels, modelProfile);
    modulation.prepare(spec.sampleRate, channels, modelProfile);
    timeEffects.prepare(spec.sampleRate, channels, modelProfile);

    driveBypassDelay.prepare(drive.getLatencySamples(), channels);
    globalDryDelay.prepare(drive.getLatencySamples(), channels);

    for (auto& fade : blockFades)
        fade.prepare(spec.sampleRate, modelProfile.switching.effectCrossfadeSeconds);
    processingFade.prepare(spec.sampleRate, modelProfile.switching.effectCrossfadeSeconds);

    inputGain.prepare(spec.sampleRate, gainRampSeconds);
    outputGain.prepare(spec.sampleRate, gainRampSeconds);
    masterGain.prepare(spec.sampleRate, modelProfile.switching.parameterRampSeconds);

    setParameters(parameters);
    reset();
}

void FiveAProcessor::reset() noexcept
{
    noiseReduction.reset();
    compressor.reset();
    drive.reset();
    equaliser.reset();
    modulation.reset();
    timeEffects.reset();
    driveBypassDelay.reset();
    globalDryDelay.reset();

    for (std::size_t block = 0; block < numEffectBlocks; ++block)
    {
        blockFades[block].reset(parameters.effectEnabled[block]);
        resetPending[block] = false;
    }
    processingFade.reset(!parameters.globalBypass);
    chainResetPending = false;

    for (auto* smoother : {&inputGain, &outputGain, &masterGain})
        smoother->setCurrentAndTarget(smoother->getTarget());
}

void FiveAProcessor::setParameters(const ParameterSnapshot& snapshot) noexcept
{
    parameters = snapshot;
    parameters.inputTrimDb = sanitiseGainDb(snapshot.inputTrimDb);
    parameters.outputLevelDb = sanitiseGainDb(snapshot.outputLevelDb);

    inputGain.setTarget(decibelsToGain(parameters.inputTrimDb));
    outputGain.setTarget(decibelsToGain(parameters.outputLevelDb));
    masterGain.setTarget(mapping::levelGain(parameters.master, modelProfile.level));

    noiseReduction.setLevel(parameters.noiseReductionLevel);
    compressor.setSettings(parameters.compressor);
    drive.setSettings(parameters.drive);
    equaliser.setSettings(parameters.equaliser);
    modulation.setSettings(parameters.modulation);
    timeEffects.setSettings(parameters.timeEffects);

    for (std::size_t block = 0; block < numEffectBlocks; ++block)
        blockFades[block].setEnabled(parameters.effectEnabled[block]);
    processingFade.setEnabled(!parameters.globalBypass);
}

AudioBufferView FiveAProcessor::scratchView(std::vector<std::vector<float>>& storage, std::vector<float*>& pointers,
                                            int numSamples) noexcept
{
    for (std::size_t channel = 0; channel < storage.size(); ++channel)
        pointers[channel] = storage[channel].data();
    return {pointers.data(), static_cast<int>(storage.size()), numSamples};
}

template <typename Block>
void FiveAProcessor::runSwitchable(Block& block, dsp::BypassCrossfade& fade, bool& pending, AudioBufferView chunk,
                                   LatencyCompensation* compensation, bool hasMemory) noexcept
{
    const int numSamples = chunk.getNumSamples();
    const auto dry = scratchView(blockDry, blockDryPointers, numSamples);

    // The latency-compensated dry path is fed every sample, so its history is ready whenever the
    // block is switched off.
    if (compensation != nullptr)
    {
        copyInto(dry, chunk);
        compensation->process(dry);
    }

    if (fade.isBypassed())
    {
        if (pending)
        {
            block.reset(); // out of the signal: start clean next time
            pending = false;
        }
        if (compensation != nullptr)
            copyInto(chunk, dry);
        return;
    }

    if (fade.isFullyEnabled())
    {
        block.process(chunk);
        return;
    }

    if (compensation == nullptr)
        copyInto(dry, chunk);

    for (int n = 0; n < numSamples; ++n)
        fadeGains[static_cast<std::size_t>(n)] = fade.getNextWetGain();

    // Switching on, a block with memory (a delay, a reverb) also gets its input faded in: fed
    // suddenly mid-signal it would record a step and play it back after the fade. Blocks without
    // such memory are left alone; fading them twice only steepens the fade.
    if (hasMemory && fade.isEnabled())
        for (int n = 0; n < numSamples; ++n)
            for (int channel = 0; channel < chunk.getNumChannels(); ++channel)
                chunk.getChannel(channel)[n] *= fadeGains[static_cast<std::size_t>(n)];

    block.process(chunk);

    for (int n = 0; n < numSamples; ++n)
    {
        const float wet = fadeGains[static_cast<std::size_t>(n)];
        for (int channel = 0; channel < chunk.getNumChannels(); ++channel)
        {
            float& sample = chunk.getChannel(channel)[n];
            sample = dry.getChannel(channel)[n] * (1.0f - wet) + sample * wet;
        }
    }

    if (fade.isBypassed())
        pending = true;
}

void FiveAProcessor::processChunk(AudioBufferView chunk) noexcept
{
    const int numSamples = chunk.getNumSamples();
    const int channels = chunk.getNumChannels();

    // Global bypass's dry path, delayed to match the drive's latency, fed every sample.
    const auto dry = scratchView(globalDry, globalDryPointers, numSamples);
    copyInto(dry, chunk);
    globalDryDelay.process(dry);

    if (processingFade.isBypassed())
    {
        if (chainResetPending)
        {
            // Bypassed: every effect starts clean when the chain comes back.
            noiseReduction.reset();
            compressor.reset();
            drive.reset();
            equaliser.reset();
            modulation.reset();
            timeEffects.reset();
            driveBypassDelay.reset();
            chainResetPending = false;
        }
        copyInto(chunk, dry);
        return;
    }

    for (int n = 0; n < numSamples; ++n)
    {
        const float gain = inputGain.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
            chunk.getChannel(channel)[n] *= gain;
    }

    noiseReduction.process(chunk);
    runSwitchable(compressor, blockFades[0], resetPending[0], chunk, nullptr, false);
    runSwitchable(drive, blockFades[1], resetPending[1], chunk, &driveBypassDelay, false);
    runSwitchable(equaliser, blockFades[2], resetPending[2], chunk, nullptr, false);
    runSwitchable(modulation, blockFades[3], resetPending[3], chunk, nullptr, true);
    runSwitchable(timeEffects, blockFades[4], resetPending[4], chunk, nullptr, true);

    for (int n = 0; n < numSamples; ++n)
    {
        const float gain = masterGain.getNextValue() * outputGain.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
            chunk.getChannel(channel)[n] *= gain;
    }

    if (processingFade.isFullyEnabled())
        return;

    for (int n = 0; n < numSamples; ++n)
    {
        const float processed = processingFade.getNextWetGain();
        for (int channel = 0; channel < channels; ++channel)
        {
            float& sample = chunk.getChannel(channel)[n];
            sample = dry.getChannel(channel)[n] * (1.0f - processed) + sample * processed;
        }
    }

    if (processingFade.isBypassed())
        chainResetPending = true;
}

void FiveAProcessor::process(AudioBufferView buffer) noexcept
{
    const dsp::DenormalGuard denormalGuard;
    const int channels = std::min(buffer.getNumChannels(), preparedSpec.numChannels);
    if (channels == 0)
        return;

    std::array<float*, dsp::DelayLine::maximumChannels> pointers{};
    for (int start = 0; start < buffer.getNumSamples(); start += chunkSize)
    {
        const int length = std::min(chunkSize, buffer.getNumSamples() - start);
        for (int channel = 0; channel < channels; ++channel)
            pointers[static_cast<std::size_t>(channel)] = buffer.getChannel(channel) + start;
        processChunk({pointers.data(), channels, length});
    }
}

} // namespace fivea
