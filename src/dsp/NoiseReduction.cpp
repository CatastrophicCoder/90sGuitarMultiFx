#include "dsp/NoiseReduction.h"

#include "core/StepMapping.h"

#include <algorithm>
#include <cmath>

namespace fivea::dsp
{

namespace
{
constexpr float levelFloorDb = -140.0f;
// Below this the gate counts as fully open and the block steps aside, bit-exact.
constexpr float openEnoughDb = 1.0e-6f;
} // namespace

void NoiseReduction::prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile) noexcept
{
    profile = modelProfile.noiseReduction;
    sampleRate = newSampleRate;
    numChannels = std::max(1, newNumChannels);
    attackCoefficient = coefficientFor(profile.attackMs);
    releaseCoefficient = coefficientFor(profile.releaseMs);
    setLevel(0);
    reset();
}

void NoiseReduction::reset() noexcept
{
    smoothedReductionDb = 0.0f;
}

float NoiseReduction::coefficientFor(float timeMs) const noexcept
{
    return static_cast<float>(std::exp(-1.0 / (static_cast<double>(timeMs) * 0.001 * sampleRate)));
}

void NoiseReduction::setLevel(int step) noexcept
{
    enabled = mapping::noiseReductionEnabled(step);
    thresholdDb = mapping::noiseReductionThresholdDb(step, profile);
}

void NoiseReduction::process(AudioBufferView buffer) noexcept
{
    if (!enabled && smoothedReductionDb == 0.0f)
        return; // off and fully open: untouched

    const int channels = std::min(buffer.getNumChannels(), numChannels);
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        float peakLevel = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            peakLevel = std::max(peakLevel, std::abs(buffer.getChannel(channel)[n]));
        const float levelDb = peakLevel > 0.0f ? std::max(20.0f * std::log10(peakLevel), levelFloorDb) : levelFloorDb;

        const float below = enabled ? std::max(0.0f, thresholdDb - levelDb) : 0.0f;
        const float target = std::min((profile.expansionRatio - 1.0f) * below, profile.maximumReductionDb);

        // Opening (less reduction) uses the attack, closing the release.
        const float coefficient = target < smoothedReductionDb ? attackCoefficient : releaseCoefficient;
        smoothedReductionDb = coefficient * smoothedReductionDb + (1.0f - coefficient) * target;
        if (!enabled && smoothedReductionDb < openEnoughDb)
            smoothedReductionDb = 0.0f;

        const float gain = smoothedReductionDb == 0.0f ? 1.0f : std::pow(10.0f, -smoothedReductionDb / 20.0f);
        for (int channel = 0; channel < channels; ++channel)
            buffer.getChannel(channel)[n] *= gain;
    }
}

} // namespace fivea::dsp
