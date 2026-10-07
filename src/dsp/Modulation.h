#pragma once

#include "core/AudioBufferView.h"
#include "core/ModelProfile.h"
#include "dsp/DelayLine.h"
#include "dsp/Smoothing.h"

namespace fivea::dsp
{

// The Chorus/Flanger block: MODE 1–5 (Flanger 1, Flanger 2, Chorus 1, Chorus 2, Slapback) with the
// documented delay times 1.8, 4.0, 24, 32 and 75 ms, plus SPEED, DEPTH, F.BACK and MIX (SRC-001
// p. 11). The structure below is a PLACEHOLDER (plan §11, EV-114):
//
//   delay = base delay + DEPTH × sine LFO        (right channel's LFO leads by the stereo phase)
//   delayed = delay line read at that delay      (cubic interpolation)
//   delay line input = x + F.BACK × delayed
//   out = x + MIX × delayed                      (MIX 0 = dry only, MIX 15 = equal balance)
//
// SPEED changes keep the LFO's phase; DEPTH, F.BACK and MIX ramp. A MODE change ducks the effect
// signal and its feedback out and back in (the dry signal is untouched), switching the delay at the
// bottom, so the jump from one base delay to another is never heard.
class Modulation
{
public:
    struct Settings
    {
        int mode = 3; // Chorus 1
        int speed = 5;
        int depth = 8;
        int feedback = 0;
        int mix = 8;

        bool operator==(const Settings&) const = default;
    };

    void prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile);
    void reset() noexcept; // LFO back to phase 0, delay line cleared, ramps settled
    void setSettings(const Settings& settings) noexcept;
    void process(AudioBufferView buffer) noexcept;

private:
    void applyMode(ModulationMode mode) noexcept;
    [[nodiscard]] float depthInSamples(int depthStep, ModulationMode mode) const noexcept;

    ModulationProfile profile;
    double sampleRate = 44100.0;
    int numChannels = 1;
    DelayLine delayLine;

    ModulationMode activeMode = ModulationMode::Chorus1;
    ModulationMode requestedMode = ModulationMode::Chorus1;
    int depthStep = 8;
    float baseDelaySamples = 0.0f;
    double lfoPhase = 0.0;     // in cycles, [0, 1)
    double lfoIncrement = 0.0; // cycles per sample
    double stereoPhaseCycles = 0.25;

    LinearSmoother depthSamples;
    LinearSmoother feedback;
    LinearSmoother wetGain;
    LinearSmoother modeDuck;
};

} // namespace fivea::dsp
