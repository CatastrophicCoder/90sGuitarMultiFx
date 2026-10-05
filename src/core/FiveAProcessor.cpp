#include "core/FiveAProcessor.h"

#include "core/ParameterIds.h"

#include <algorithm>
#include <cmath>

namespace fivea
{

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

void FiveAProcessor::prepare(const ProcessSpec& spec) noexcept
{
    inputGain.prepare(spec.sampleRate, gainRampSeconds);
    outputGain.prepare(spec.sampleRate, gainRampSeconds);
    reset();
}

void FiveAProcessor::reset() noexcept
{
    applyGainTargets();
    inputGain.setCurrentAndTarget(inputGain.getTarget());
    outputGain.setCurrentAndTarget(outputGain.getTarget());
}

void FiveAProcessor::setParameters(const ParameterSnapshot& snapshot) noexcept
{
    parameters = snapshot;
    parameters.inputTrimDb = sanitiseGainDb(snapshot.inputTrimDb);
    parameters.outputLevelDb = sanitiseGainDb(snapshot.outputLevelDb);
    applyGainTargets();
}

void FiveAProcessor::applyGainTargets() noexcept
{
    // Bypass ramps both gains to unity rather than switching, so engaging it while trim is
    // non-zero does not click. Once the ramp ends the output is the input, bit for bit.
    const bool bypassed = parameters.globalBypass;
    inputGain.setTarget(bypassed ? 1.0f : decibelsToGain(parameters.inputTrimDb));
    outputGain.setTarget(bypassed ? 1.0f : decibelsToGain(parameters.outputLevelDb));
}

void FiveAProcessor::process(AudioBufferView buffer) noexcept
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // The documented chain (compressor → drive → EQ → modulation → reverb/delay) slots in between
    // the two gain stages in Milestone 1. Gains are computed per sample and shared by all
    // channels so stereo channels stay matched.
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float gain = inputGain.getNextValue() * outputGain.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.getChannel(channel)[sample] *= gain;
    }
}

} // namespace fivea
