#include "dsp/Compressor.h"

#include "core/StepMapping.h"

#include <algorithm>
#include <cmath>

namespace fivea::dsp
{

namespace
{
// Levels are floored here before taking the logarithm, so silence gives a finite level.
constexpr float levelFloorDb = -120.0f;
} // namespace

void Compressor::prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile) noexcept
{
    profile = modelProfile.compressor;
    levelProfile = modelProfile.level;
    sampleRate = newSampleRate;
    numChannels = std::max(1, newNumChannels);
    outputGain.prepare(sampleRate, modelProfile.switching.parameterRampSeconds);
    releaseCoefficient = coefficientFor(profile.releaseMs);

    setSettings({});
    reset();
}

void Compressor::reset() noexcept
{
    smoothedReductionDb = 0.0f;
    outputGain.setCurrentAndTarget(outputGain.getTarget());
}

void Compressor::setSettings(const Settings& settings) noexcept
{
    thresholdDb = mapping::compressorThresholdDb(settings.sens, profile);
    attackCoefficient = coefficientFor(mapping::compressorAttackMs(settings.attack, profile));
    outputGain.setTarget(mapping::levelGain(settings.level, levelProfile));
}

float Compressor::coefficientFor(float timeMs) const noexcept
{
    // One-pole smoothing coefficient for time constant τ: α = e^(−1 / (τ·fs)).
    return static_cast<float>(std::exp(-1.0 / (static_cast<double>(timeMs) * 0.001 * sampleRate)));
}

float Compressor::gainReductionDb(float inputDb) const noexcept
{
    // Static curve with a soft knee (Giannoulis et al. 2012, eq. 4); returns input − output ≥ 0.
    const float over = inputDb - thresholdDb;
    const float knee = profile.kneeDb;
    const float slope = 1.0f / profile.ratio - 1.0f;

    if (2.0f * over <= -knee)
        return 0.0f;
    if (2.0f * over >= knee)
        return -slope * over;

    const float inKnee = over + knee / 2.0f;
    return -slope * inKnee * inKnee / (2.0f * knee);
}

void Compressor::process(AudioBufferView buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int channels = std::min(buffer.getNumChannels(), numChannels);

    for (int n = 0; n < numSamples; ++n)
    {
        float peakLevel = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            peakLevel = std::max(peakLevel, std::abs(buffer.getChannel(channel)[n]));

        const float inputDb = peakLevel > 0.0f ? std::max(20.0f * std::log10(peakLevel), levelFloorDb) : levelFloorDb;
        const float targetReductionDb = gainReductionDb(inputDb);

        // Smooth branching peak detector (Giannoulis et al. 2012, eq. 17).
        const float coefficient = targetReductionDb > smoothedReductionDb ? attackCoefficient : releaseCoefficient;
        smoothedReductionDb = coefficient * smoothedReductionDb + (1.0f - coefficient) * targetReductionDb;

        const float gain = outputGain.getNextValue() *
                           (smoothedReductionDb == 0.0f ? 1.0f : std::pow(10.0f, -smoothedReductionDb / 20.0f));

        for (int channel = 0; channel < channels; ++channel)
            buffer.getChannel(channel)[n] *= gain;
    }
}

} // namespace fivea::dsp
