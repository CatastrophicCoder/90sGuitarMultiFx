#pragma once

#include "core/AudioBufferView.h"
#include "core/ModelProfile.h"
#include "core/ParameterSnapshot.h"
#include "dsp/BypassCrossfade.h"
#include "dsp/Compressor.h"
#include "dsp/DelayLine.h"
#include "dsp/Drive.h"
#include "dsp/Modulation.h"
#include "dsp/NoiseReduction.h"
#include "dsp/Smoothing.h"
#include "dsp/ThreeBandEq.h"
#include "dsp/TimeEffects.h"

#include <array>
#include <vector>

namespace fivea
{

// The whole signal chain, in the documented order (EV-001), with the Utility page's noise
// reduction and master volume placed as placeholders (EV-014, EV-015):
//
//   input trim → NR → Compressor → Distortion/Overdrive → 3 Band EQ → Chorus/Flanger →
//   Reverb/Delay → MASTER → output level
//
// Each effect fades in and out over a short crossfade when switched (plan §15). A switched-off
// effect is not run, and is reset once its fade-out ends so it starts clean when switched on again.
// Global bypass crossfades to the dry input. Oversampling in the drive adds latency; the drive's
// bypass path and the global bypass path are delayed to match, so the latency the host compensates
// never changes while playing.
//
// prepare() and reset() run off the audio thread (prepare allocates); setParameters() and process()
// are real-time safe. process() accepts blocks of any length, including longer than announced.
class FiveAProcessor
{
public:
    void prepare(const ProcessSpec& spec);
    void reset() noexcept;

    void setParameters(const ParameterSnapshot& snapshot) noexcept;
    void process(AudioBufferView buffer) noexcept;

    [[nodiscard]] const ParameterSnapshot& getParameters() const noexcept { return parameters; }
    [[nodiscard]] int getLatencySamples() const noexcept { return drive.getLatencySamples(); }

    // Gain ramp length. A plugin design choice to avoid zipper noise, not a hardware property.
    static constexpr double gainRampSeconds = 0.02;

private:
    // Delays a signal by a whole number of samples, to keep bypassed paths in time with the
    // oversampled drive. Zero latency is a straight copy.
    class LatencyCompensation
    {
    public:
        void prepare(int latencySamples, int numChannels);
        void reset() noexcept { line.reset(); }
        void process(AudioBufferView buffer) noexcept;

    private:
        dsp::DelayLine line;
        int latency = 0;
    };

    template <typename Block>
    void runSwitchable(Block& block, dsp::BypassCrossfade& fade, bool& resetPending, AudioBufferView chunk,
                       LatencyCompensation* compensation, bool hasMemory) noexcept;
    void processChunk(AudioBufferView chunk) noexcept;
    [[nodiscard]] AudioBufferView scratchView(std::vector<std::vector<float>>& storage, std::vector<float*>& pointers,
                                              int numSamples) noexcept;

    ProcessSpec preparedSpec;
    ParameterSnapshot parameters;

    dsp::NoiseReduction noiseReduction;
    dsp::Compressor compressor;
    dsp::Drive drive;
    dsp::ThreeBandEq equaliser;
    dsp::Modulation modulation;
    dsp::TimeEffects timeEffects;

    std::array<dsp::BypassCrossfade, numEffectBlocks> blockFades;
    std::array<bool, numEffectBlocks> resetPending{};
    dsp::BypassCrossfade processingFade; // global bypass: enabled = processing
    bool chainResetPending = false;
    LatencyCompensation driveBypassDelay;
    LatencyCompensation globalDryDelay;

    dsp::LinearSmoother inputGain;
    dsp::LinearSmoother outputGain;
    dsp::LinearSmoother masterGain;

    std::vector<std::vector<float>> blockDry;
    std::vector<float*> blockDryPointers;
    std::vector<float> fadeGains; // one chunk of a switching block's crossfade
    std::vector<std::vector<float>> globalDry;
    std::vector<float*> globalDryPointers;
    int chunkSize = 1;
};

// Clamps to the plugin's range, snaps to its 0.1 dB step and replaces non-finite values with 0 dB,
// so a corrupt automation value or state can never reach the audio path.
[[nodiscard]] float sanitiseGainDb(float gainDb) noexcept;
[[nodiscard]] float decibelsToGain(float gainDb) noexcept;

} // namespace fivea
