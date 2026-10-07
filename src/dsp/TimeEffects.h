#pragma once

#include "core/AudioBufferView.h"
#include "core/ModelProfile.h"
#include "dsp/Biquad.h"
#include "dsp/DelayLine.h"
#include "dsp/Reverb.h"
#include "dsp/Smoothing.h"

#include <array>

namespace fivea::dsp
{

// One complete Reverb/Delay mode: the delay, the reverb, or both (Echoverb). Produces the effect
// signal only; the block mixes it with the dry signal. Driven one frame (all channels) at a time.
class TimeEffectEngine
{
public:
    void prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile); // allocates

    // `immediate`: jump to the new delay time (only on a cleared engine); otherwise crossfade to it.
    void configure(TimeEffectMode newMode, int time, int fine, int feedbackStep, bool immediate) noexcept;
    void reset() noexcept;
    void processFrame(const float* input, float* wet, int channels) noexcept;

private:
    [[nodiscard]] float delayInSamples(float ms) const noexcept;
    float readTap(int channel) const noexcept;

    TimeEffectMode mode = TimeEffectMode::HallReverb;
    DelayProfile delayProfile;
    ReverbProfile reverbProfile;
    double sampleRate = 44100.0;
    int numChannels = 1;

    DelayLine delayLine;
    Biquad feedbackTone; // one design, a state per channel
    LinearSmoother feedback;

    // A TIME/FINE change crossfades from the old read position (tapA) to the new one (tapB), so the
    // pitch never bends; a change during a crossfade waits for it to finish.
    float tapA = 2.0f;
    float tapB = 2.0f;
    float pendingTap = -1.0f;
    LinearSmoother tapFade;

    Reverb reverb;
};

// The Reverb/Delay block: MODE 1–7 (Hall, Ensemble Hall, Room, Plate, Live Stage, Echoverb, Delay),
// TIME, FINE, F.BACK and MIX (SRC-001 p. 12). TIME, FINE and F.BACK act only in Echoverb and Delay,
// as on the unit. A MODE change crossfades from the old mode's tail to the new mode (plan §15's
// Crossfade tail policy), using two engines; a further MODE change during that crossfade is applied
// when it ends, so no tail is ever cut. MIX ramps; MIX 0 is the dry signal bit for bit.
class TimeEffects
{
public:
    struct Settings
    {
        int mode = 1; // Hall
        int time = 3;
        int fine = 0;
        int feedback = 4;
        int mix = 8;

        bool operator==(const Settings&) const = default;
    };

    void prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile); // allocates
    void reset() noexcept;
    void setSettings(const Settings& settings) noexcept;
    void process(AudioBufferView buffer) noexcept;

private:
    void startModeChange(const Settings& settings) noexcept;

    std::array<TimeEffectEngine, 2> engines;
    int activeEngine = 0;
    int fadingEngine = -1;
    TimeEffectMode activeMode = TimeEffectMode::HallReverb;
    Settings current;
    bool modeChangePending = false; // a MODE change during a crossfade waits for it to end
    LinearSmoother engineFade;      // weight of the active engine
    LinearSmoother wetGain;
    int numChannels = 1;
};

} // namespace fivea::dsp
