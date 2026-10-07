#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace fivea::dsp
{

// Converts a stream between two sample rates whose ratio reduces to whole numbers, up / down
// (147/160 from 48 kHz to 44.1 kHz). Conceptually: insert up − 1 zeros between samples, low-pass
// at the common rate (input rate × up), keep every down-th sample; computed as a polyphase filter
// that only evaluates the kept outputs (J. O. Smith and P. Gossett, "A Flexible Sampling-Rate
// Conversion Method", ICASSP 1984; J. O. Smith, "Digital Audio Resampling", ccrma.stanford.edu).
//
// The low-pass is a linear-phase Kaiser-windowed sinc: flat to 20 kHz (or 0.9 × the lower Nyquist,
// if that is lower), at least 100 dB down from the lower rate's Nyquist. Linear phase gives a
// constant delay, a whole number of common-rate samples, which an optional extra delay lengthens.
// These are design choices (EV-126), not properties of the original unit.
//
// Positions are counted in whole numbers, so the output never drifts: after n inputs exactly
// ceil(n × up / down) outputs have been produced. Equal rates are a straight copy with no delay.
// Memory is allocated in prepare(); process() and reset() are real-time safe.
class RationalResampler
{
public:
    static constexpr int maximumChannels = 2;

    // Rates must be whole numbers of Hz, and the polyphase table no larger than about a million
    // coefficients; otherwise false, and the resampler stays unprepared. An approximated ratio
    // would drift, so none is used. Allocates.
    [[nodiscard]] bool prepare(double inputRate, double outputRate, int numChannels, int extraDelaySamples = 0);
    void reset() noexcept;

    // Reads numInput samples per channel, writes the outputs that become due (at most
    // maximumOutputFor(numInput)) and returns how many.
    int process(const float* const* input, int numInput, float* const* output) noexcept;
    [[nodiscard]] int maximumOutputFor(int numInput) const noexcept;

    [[nodiscard]] int getUpFactor() const noexcept { return up; }
    [[nodiscard]] int getDownFactor() const noexcept { return down; }
    // The filter's delay, at the common rate (input rate × up factor).
    [[nodiscard]] int getDelayInCommonRateSamples() const noexcept { return delay; }
    [[nodiscard]] double getDelaySeconds() const noexcept;

private:
    double inputRate = 0.0;
    int numChannels = 0;
    int up = 1;
    int down = 1;
    int delay = 0;
    int tapsPerPhase = 0;
    std::vector<float> table; // phase-major: table[phase * tapsPerPhase + j] = h[phase + j * up]

    // Per channel, twice the taps long, so the newest tapsPerPhase inputs are always contiguous;
    // every channel is written at the same position.
    std::array<std::vector<float>, maximumChannels> history;
    int historyPosition = 0;

    int phase = 0;          // (output index × down) mod up
    int inputsUntilDue = 0; // inputs still to arrive before the next output is due
};

} // namespace fivea::dsp
