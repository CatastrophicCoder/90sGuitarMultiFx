#pragma once

#include "core/AudioBufferView.h"
#include "core/ModelProfile.h"
#include "dsp/Biquad.h"
#include "dsp/Smoothing.h"

namespace fivea::dsp
{

// The 3 Band EQ block: TRIM (input gain) → BASS (low shelf, 100 Hz) → MID (peaking, frequency from
// the MID FREQ table) → TREBLE (high shelf, 3 kHz). Controls are the original unit's steps
// (SRC-001 p. 11); dB per step, Q and the filter shapes are placeholders from the profile (EV-021,
// EV-109).
//
// Gains ramp in dB and MID FREQ in log-frequency; coefficients are recomputed every
// `coefficientInterval` samples while anything ramps. Every intermediate set is a complete design
// for an in-between setting, so it is always stable (plan §10.2). At exactly 0 dB the cookbook
// designs reduce to b = a bit for bit, an exact identity, so a flat EQ is bit-transparent without
// any special case.
class ThreeBandEq
{
public:
    struct Settings
    {
        int bass = 0;
        int midFrequency = 3; // 800 Hz
        int mid = 0;
        int treble = 0;
        int trim = 15; // unity

        bool operator==(const Settings&) const = default;
    };

    static constexpr int coefficientInterval = 16;

    void prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile) noexcept;
    void reset() noexcept; // settles every ramp on the current settings and clears the filters
    void setSettings(const Settings& settings) noexcept;
    void process(AudioBufferView buffer) noexcept;

private:
    struct Band
    {
        LinearSmoother gainDb;
        Biquad filter;
    };

    void updateCoefficients() noexcept;
    void advanceRamps(int numSamples) noexcept;
    [[nodiscard]] bool anyRamping() const noexcept;

    EqProfile profile;
    double sampleRate = 44100.0;
    int numChannels = 1;

    LinearSmoother trimGain;
    LinearSmoother midLog2Frequency;
    Band bass;
    Band mid;
    Band treble;
    int samplesUntilUpdate = 0;
    bool coefficientsDirty = true;
};

} // namespace fivea::dsp
