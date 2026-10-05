#pragma once

#include "core/AudioBufferView.h"
#include "core/ModelProfile.h"

namespace fivea::dsp
{

// The Utility page's noise reduction, NR LEVEL 0–15 (SRC-001 pp. 7, 12). Only its control is
// documented; this is a PLACEHOLDER downward expander (EV-117): a peak level linked across channels,
// a gain reduction of (ratio − 1) dB per dB below the threshold, smoothed so it opens quickly and
// closes slowly. NR LEVEL 0 is off; switching it off lets the gate open on its attack, after which
// the block passes audio through bit for bit.
class NoiseReduction
{
public:
    void prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile) noexcept;
    void reset() noexcept;
    void setLevel(int step) noexcept;
    void process(AudioBufferView buffer) noexcept;

private:
    [[nodiscard]] float coefficientFor(float timeMs) const noexcept;

    NoiseReductionProfile profile;
    double sampleRate = 44100.0;
    int numChannels = 1;
    bool enabled = false;
    float thresholdDb = -90.0f;
    float attackCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
    float smoothedReductionDb = 0.0f;
};

} // namespace fivea::dsp
