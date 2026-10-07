#pragma once

#include "core/AudioBufferView.h"
#include "core/ModelProfile.h"
#include "dsp/Biquad.h"
#include "dsp/Oversampler.h"
#include "dsp/Smoothing.h"

#include <array>

namespace fivea::dsp
{

// One DIST/OD mode's pipeline, configured entirely by a DriveProfile (plan §9.1's interchangeable
// model): input high-pass → pre-emphasis → DRIVE gain → waveshaper (oversampled) → DC blocker →
// TONE low-pass → output trim. A measured model with a different structure (e.g. the transfer-
// curve tables of plan §9.3) would replace this class behind the same calls.
//
// Driven sample by sample: beginSample() once per sample, then processSample() per channel.
class DriveModel
{
public:
    void prepare(double newSampleRate, int numChannels, const DriveProfile& driveProfile, int oversamplingFactor,
                 double rampSeconds);
    void reset() noexcept; // settles ramps on their targets and clears all state
    void setDrive(int step) noexcept;
    void setTone(int step) noexcept;

    void beginSample() noexcept;
    [[nodiscard]] float processSample(int channel, float input) noexcept;
    [[nodiscard]] int getLatencySamples() const noexcept { return oversampler.getLatencySamples(); }

    static constexpr int coefficientInterval = 16;

private:
    DriveProfile profile{};
    double sampleRate = 44100.0;

    Biquad inputHighPass;
    Biquad emphasis;
    Biquad dcBlocker;
    Biquad toneLowPass;
    Oversampler oversampler;

    LinearSmoother preGain;       // linear gain
    LinearSmoother toneLog2Hertz; // cutoff, ramped on a log axis
    float currentPreGain = 1.0f;
    float outputTrim = 1.0f;
    int samplesUntilUpdate = 0;
    bool toneDirty = true;
};

// The Distortion/Overdrive block: MODE 1 = Distortion, 2 = Overdrive, DRIVE, TONE, LEVEL
// (SRC-001 p. 10). Each mode has its own DriveModel; a MODE change crossfades from one to the
// other, so it does not click. LEVEL is output gain, ramped.
class Drive
{
public:
    struct Settings
    {
        int mode = 2; // Overdrive
        int drive = 8;
        int tone = 8;
        int level = 12; // unity

        bool operator==(const Settings&) const = default;
    };

    // The oversampling factor (1, 2 or 4) is fixed here: changing it changes the latency, which
    // only happens when the host re-prepares (plan §9.2).
    void prepare(double newSampleRate, int numChannels, const FiveAModelProfile& modelProfile, int oversamplingFactor);
    void reset() noexcept;
    void setSettings(const Settings& settings) noexcept;
    void process(AudioBufferView buffer) noexcept;
    [[nodiscard]] int getLatencySamples() const noexcept { return models[0].getLatencySamples(); }

private:
    static int modelIndex(int modeStep) noexcept;

    std::array<DriveModel, 2> models; // [0] Distortion, [1] Overdrive
    int activeModel = 1;
    int fadingModel = -1;    // the model fading out, or −1
    LinearSmoother modeFade; // weight of the active model, 0 → 1 during a switch
    LinearSmoother outputGain;
    LevelProfile levelProfile;
    int numChannels = 1;
};

} // namespace fivea::dsp
