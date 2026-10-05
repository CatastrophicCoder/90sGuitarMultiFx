#pragma once

#include "core/ModelProfile.h"

#include <array>
#include <vector>

namespace fivea::dsp
{

// A compact feedback delay network reverberator (plan §12.2), used for the five reverb voicings.
// A PLACEHOLDER, not the original unit's reverb (EV-115):
//
//   input → pre-delay → 4 series allpass diffusers → 8 delay lines, mixed back into each other by
//   an orthogonal 8×8 Hadamard matrix. Each line has an absorption filter (a one-pole low-pass
//   with gain) that sets the decay time at low frequencies and a shorter one at high frequencies:
//   J.-M. Jot and A. Chaigne, "Digital Delay Networks for Designing Artificial Reverberators",
//   AES 90th Convention, 1991, preprint 3030.
//   Left and right are taken from the lines with two different sign patterns, so they decorrelate.
//
// Memory is allocated in prepare() for the largest voicing; setVoicing() does not allocate and is
// safe on the audio thread, but should be followed by reset() (the engine switches voicings only
// on a cleared network).
class Reverb
{
public:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;

    void prepare(double newSampleRate, const ReverbProfile& reverbProfile); // allocates
    void setVoicing(const ReverbVoicing& voicing) noexcept;
    void reset() noexcept;

    void processSample(float input, float& left, float& right) noexcept;

private:
    struct Line
    {
        std::vector<float> buffer;
        int length = 1;
        int position = 0;

        [[nodiscard]] float readOldest() const noexcept { return buffer[static_cast<std::size_t>(position)]; }
        void writeAndAdvance(float sample) noexcept
        {
            buffer[static_cast<std::size_t>(position)] = sample;
            position = position + 1 == length ? 0 : position + 1;
        }
    };

    double sampleRate = 44100.0;
    Line preDelay;
    std::array<Line, numDiffusers> diffusers;
    std::array<Line, numLines> lines;
    std::array<float, numLines> absorptionGain{};
    std::array<float, numLines> absorptionPole{};
    std::array<float, numLines> absorptionState{};
    float diffusion = 0.6f;
};

} // namespace fivea::dsp
