#pragma once

#include "core/AudioBufferView.h"
#include "core/ParameterSnapshot.h"
#include "dsp/Smoothing.h"

namespace a5
{

// Façade over the whole signal chain. Milestone 0: no effect blocks exist yet, so the chain is
// input trim → (documented five-block chain, all pass-through) → output level. Effect enable
// states are accepted and stored but change nothing until the blocks are implemented.
//
// prepare() and reset() run off the audio thread; setParameters() and process() are real-time
// safe (no allocation, locking or I/O).
class A5Processor
{
public:
    void prepare(const ProcessSpec& spec) noexcept;
    void reset() noexcept;

    void setParameters(const ParameterSnapshot& snapshot) noexcept;
    void process(AudioBufferView buffer) noexcept;

    [[nodiscard]] const ParameterSnapshot& getParameters() const noexcept { return parameters; }

    // Gain ramp length. A plugin design choice to avoid zipper noise, not a hardware property.
    static constexpr double gainRampSeconds = 0.02;

private:
    void applyGainTargets() noexcept;

    ParameterSnapshot parameters;
    dsp::LinearSmoother inputGain;
    dsp::LinearSmoother outputGain;
};

// Clamps to the plugin's range, snaps to its 0.1 dB step and replaces non-finite values with 0 dB,
// so a corrupt automation value or state can never reach the audio path.
[[nodiscard]] float sanitiseGainDb(float gainDb) noexcept;
[[nodiscard]] float decibelsToGain(float gainDb) noexcept;

} // namespace a5
