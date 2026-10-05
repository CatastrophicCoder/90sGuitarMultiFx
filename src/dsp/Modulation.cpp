#include "dsp/Modulation.h"

#include "core/StepMapping.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fivea::dsp
{

void Modulation::prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile)
{
    profile = modelProfile.modulation;
    sampleRate = newSampleRate;
    numChannels = std::clamp(newNumChannels, 1, DelayLine::maximumChannels);
    stereoPhaseCycles = static_cast<double>(profile.stereoPhaseDegrees) / 360.0;

    // Room for the longest base delay plus its largest sweep, plus a millisecond to spare.
    float longestMs = 0.0f;
    for (int mode = documented::modulationMode.minimum; mode <= documented::modulationMode.maximum; ++mode)
    {
        const auto m = static_cast<ModulationMode>(mode);
        longestMs = std::max(longestMs, mapping::modulationBaseDelayMs(m) + mapping::modulationDepthMs(15, m, profile));
    }
    delayLine.prepare(static_cast<int>(std::ceil((longestMs + 1.0f) * 0.001f * static_cast<float>(sampleRate))),
                      numChannels);
    delayLine.setInterpolation(Interpolation::Cubic);

    const double rampSeconds = modelProfile.switching.parameterRampSeconds;
    for (auto* smoother : {&depthSamples, &feedback, &wetGain, &modeDuck})
        smoother->prepare(sampleRate, rampSeconds);

    setSettings({});
    reset();
}

float Modulation::depthInSamples(int step, ModulationMode mode) const noexcept
{
    return mapping::modulationDepthMs(step, mode, profile) * 0.001f * static_cast<float>(sampleRate);
}

void Modulation::applyMode(ModulationMode mode) noexcept
{
    activeMode = mode;
    // In double: 75 ms at 48 kHz must come out as exactly 3600 samples, not 3600.0002.
    baseDelaySamples =
        static_cast<float>(static_cast<double>(mapping::modulationBaseDelayMs(mode)) * 0.001 * sampleRate);
    depthSamples.setCurrentAndTarget(depthInSamples(depthStep, mode));
}

void Modulation::reset() noexcept
{
    applyMode(requestedMode);
    lfoPhase = 0.0;
    delayLine.reset();
    for (auto* smoother : {&feedback, &wetGain})
        smoother->setCurrentAndTarget(smoother->getTarget());
    modeDuck.setCurrentAndTarget(1.0f);
}

void Modulation::setSettings(const Settings& settings) noexcept
{
    depthStep = settings.depth;
    requestedMode = mapping::toModulationMode(settings.mode);
    lfoIncrement = static_cast<double>(mapping::modulationSpeedHz(settings.speed, profile)) / sampleRate;
    feedback.setTarget(mapping::feedbackAmount(settings.feedback, profile.feedbackMaximum));
    wetGain.setTarget(mapping::mixWetGain(settings.mix));

    if (requestedMode == activeMode)
    {
        depthSamples.setTarget(depthInSamples(depthStep, activeMode));
        modeDuck.setTarget(1.0f); // cancels a duck if the mode was switched away and back
    }
    else
        modeDuck.setTarget(0.0f); // the switch itself happens at the bottom of the duck
}

void Modulation::process(AudioBufferView buffer) noexcept
{
    const int channels = std::min(buffer.getNumChannels(), numChannels);

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const float duck = modeDuck.getNextValue();
        if (requestedMode != activeMode && duck == 0.0f)
        {
            // Silent here: switch the delay and come back up. The line keeps its history: it is
            // continuous audio, so reading it at the new delay is continuous too. Clearing it would
            // make the effect start with a step one base delay later, after the fade-in is over.
            applyMode(requestedMode);
            modeDuck.setTarget(1.0f);
        }

        const float depth = depthSamples.getNextValue();
        // The duck also scales the feedback: at the switch the delayed signal jumps, and written
        // back into the line that jump would come out as a click one delay later.
        const float feedbackAmount = feedback.getNextValue() * duck;
        const float wet = wetGain.getNextValue() * duck;

        for (int channel = 0; channel < channels; ++channel)
        {
            const double phase = lfoPhase + (channel == 1 ? stereoPhaseCycles : 0.0);
            const auto lfo = static_cast<float>(std::sin(2.0 * std::numbers::pi * phase));

            float* sample = buffer.getChannel(channel) + n;
            const float input = *sample;
            const float delayed = delayLine.read(channel, baseDelaySamples + depth * lfo);
            delayLine.write(channel, input + feedbackAmount * delayed);
            *sample = input + wet * delayed;
        }

        lfoPhase += lfoIncrement;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;
    }
}

} // namespace fivea::dsp
