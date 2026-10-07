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
// Each voicing's output is levelled so its wet signal is as loud as its input on a steady
// broadband signal: MIX 15 is then an equal balance (EV-125). The level comes from the voicing's
// impulse response energy (for white noise, the output power gain), measured in prepare().
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
    void setVoicing(std::size_t voicingIndex) noexcept;                     // an index into the profile's voicings
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

    void configure(const ReverbVoicing& voicing) noexcept;
    [[nodiscard]] double impulseResponseEnergy(const ReverbVoicing& voicing) noexcept;

    double sampleRate = 44100.0;
    std::array<ReverbVoicing, ReverbProfile{}.voicings.size()> voicings = ReverbProfile{}.voicings;
    std::array<float, ReverbProfile{}.voicings.size()> levelGains = []
    {
        std::array<float, ReverbProfile{}.voicings.size()> gains{};
        gains.fill(0.5f);
        return gains;
    }();
    float outputGain = 0.5f;
    Line preDelay;
    std::array<Line, numDiffusers> diffusers;
    std::array<Line, numLines> lines;
    std::array<float, numLines> absorptionGain{};
    std::array<float, numLines> absorptionPole{};
    std::array<float, numLines> absorptionState{};
    float diffusion = 0.6f;
};

} // namespace fivea::dsp
