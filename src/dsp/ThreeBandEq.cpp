#include "dsp/ThreeBandEq.h"

#include "core/StepMapping.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace fivea::dsp
{

void ThreeBandEq::prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile) noexcept
{
    assert(newNumChannels >= 1 && newNumChannels <= Biquad::maximumChannels);
    profile = modelProfile.eq;
    sampleRate = newSampleRate;
    numChannels = std::clamp(newNumChannels, 1, Biquad::maximumChannels);

    const double rampSeconds = modelProfile.switching.parameterRampSeconds;
    for (auto* smoother : {&trimGain, &midLog2Frequency, &bass.gainDb, &mid.gainDb, &treble.gainDb})
        smoother->prepare(sampleRate, rampSeconds);

    setSettings({});
    reset();
}

void ThreeBandEq::reset() noexcept
{
    for (auto* smoother : {&trimGain, &midLog2Frequency, &bass.gainDb, &mid.gainDb, &treble.gainDb})
        smoother->setCurrentAndTarget(smoother->getTarget());

    for (auto* band : {&bass, &mid, &treble})
        band->filter.reset();

    samplesUntilUpdate = 0;
    coefficientsDirty = true;
}

void ThreeBandEq::setSettings(const Settings& settings) noexcept
{
    trimGain.setTarget(mapping::eqTrimGain(settings.trim, profile));
    midLog2Frequency.setTarget(std::log2(mapping::eqMidFrequencyHz(settings.midFrequency)));
    bass.gainDb.setTarget(mapping::eqBandGainDb(settings.bass, profile));
    mid.gainDb.setTarget(mapping::eqBandGainDb(settings.mid, profile));
    treble.gainDb.setTarget(mapping::eqBandGainDb(settings.treble, profile));
    coefficientsDirty = true;
}

bool ThreeBandEq::anyRamping() const noexcept
{
    return midLog2Frequency.isSmoothing() || bass.gainDb.isSmoothing() || mid.gainDb.isSmoothing() ||
           treble.gainDb.isSmoothing();
}

void ThreeBandEq::updateCoefficients() noexcept
{
    const double slope = profile.shelfSlope;
    bass.filter.setCoefficients(
        design::lowShelf(sampleRate, mapping::eqBassFrequencyHz(), bass.gainDb.getCurrent(), slope));
    mid.filter.setCoefficients(
        design::peaking(sampleRate, std::exp2(midLog2Frequency.getCurrent()), profile.midQ, mid.gainDb.getCurrent()));
    treble.filter.setCoefficients(
        design::highShelf(sampleRate, mapping::eqTrebleFrequencyHz(), treble.gainDb.getCurrent(), slope));
}

void ThreeBandEq::advanceRamps(int numSamples) noexcept
{
    for (auto* smoother : {&midLog2Frequency, &bass.gainDb, &mid.gainDb, &treble.gainDb})
        smoother->skip(numSamples);
}

void ThreeBandEq::process(AudioBufferView buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int channels = std::min(buffer.getNumChannels(), numChannels);

    int position = 0;
    while (position < numSamples)
    {
        // Coefficients change only on a fixed grid of absolute sample positions, so the output
        // does not depend on how the host splits blocks.
        if (samplesUntilUpdate == 0)
        {
            if (coefficientsDirty)
                updateCoefficients();

            // Still dirty if the ramps move during this interval: the next update must catch up,
            // including the value they land on.
            coefficientsDirty = anyRamping();
            advanceRamps(coefficientInterval);
            samplesUntilUpdate = coefficientInterval;
        }

        const int length = std::min(samplesUntilUpdate, numSamples - position);

        for (int n = position; n < position + length; ++n)
        {
            const float trim = trimGain.getNextValue();

            for (int channel = 0; channel < channels; ++channel)
            {
                float sample = buffer.getChannel(channel)[n] * trim;
                sample = bass.filter.processSample(channel, sample);
                sample = mid.filter.processSample(channel, sample);
                sample = treble.filter.processSample(channel, sample);
                buffer.getChannel(channel)[n] = sample;
            }
        }

        position += length;
        samplesUntilUpdate -= length;
    }
}

} // namespace fivea::dsp
