#pragma once

#include "core/AudioBufferView.h"
#include "core/ModelProfile.h"
#include "dsp/Smoothing.h"

namespace fivea::dsp
{

// The Compressor block. The original unit documents it as a limiter/sustainer with SENS, ATTACK
// and LEVEL (SRC-001 p. 10, EV-022); how it works inside is not documented, so this is a
// PLACEHOLDER (plan §8.1):
//
//   Feed-forward, no lookahead. A peak level, linked across channels, goes through a gain computer
//   in dB (threshold from SENS, fixed ratio and soft knee); the resulting gain reduction is then
//   smoothed by a "smooth branching" peak detector with the ATTACK time constant rising and the
//   fixed release time constant falling. This is the log-domain design recommended in
//   D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial
//   and Analysis", J. Audio Eng. Soc. 60(6), 2012 (eqs. 4 and 17).
//
// LEVEL is output gain, ramped so a step does not click. SENS and ATTACK need no ramp: they change
// the target gain reduction, which the detector already smooths.
class Compressor
{
public:
    struct Settings
    {
        int sens = 8;
        int attack = 4;
        int level = 12; // unity
    };

    void prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile) noexcept;
    void reset() noexcept; // clears the detector and settles LEVEL on its target
    void setSettings(const Settings& settings) noexcept;
    void process(AudioBufferView buffer) noexcept;

private:
    [[nodiscard]] float gainReductionDb(float inputDb) const noexcept;
    [[nodiscard]] float coefficientFor(float timeMs) const noexcept;

    CompressorProfile profile;
    LevelProfile levelProfile;
    double sampleRate = 44100.0;
    int numChannels = 1;

    float thresholdDb = 0.0f;
    float attackCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
    float smoothedReductionDb = 0.0f;
    LinearSmoother outputGain;
};

} // namespace fivea::dsp
