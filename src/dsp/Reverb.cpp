#include "dsp/Reverb.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fivea::dsp
{

namespace
{
// Delay lengths at size 1, in milliseconds: spread out and with no simple ratios between them,
// so the network's echoes do not pile up at the same times.
constexpr std::array<double, Reverb::numLines> lineMs{31.3, 37.9, 41.7, 45.1, 53.9, 59.3, 67.1, 73.7};
constexpr std::array<double, Reverb::numDiffusers> diffuserMs{4.77, 3.59, 12.73, 9.30};

// Output sign patterns: orthogonal to each other, so left and right get differently mixed tails.
constexpr std::array<float, Reverb::numLines> leftSigns{1, -1, 1, -1, 1, -1, 1, -1};
constexpr std::array<float, Reverb::numLines> rightSigns{1, 1, -1, -1, 1, 1, -1, -1};

// The largest voicing size and pre-delay the buffers are allocated for.
double largestSize(const ReverbProfile& profile)
{
    double largest = 1.0;
    for (const auto& voicing : profile.voicings)
        largest = std::max(largest, static_cast<double>(voicing.size));
    return largest;
}

double largestPreDelayMs(const ReverbProfile& profile)
{
    double largest = 0.0;
    for (const auto& voicing : profile.voicings)
        largest = std::max(largest, static_cast<double>(voicing.preDelayMs));
    return largest;
}

int toSamples(double ms, double sampleRate)
{
    return std::max(1, static_cast<int>(std::lround(ms * 0.001 * sampleRate)));
}

// In-place fast Walsh–Hadamard transform, scaled by 1/√8 so the matrix is orthogonal (energy
// preserving): the decay is then set by the absorption filters alone.
void hadamard8(std::array<float, Reverb::numLines>& v) noexcept
{
    for (int span = 1; span < Reverb::numLines; span *= 2)
        for (int start = 0; start < Reverb::numLines; start += 2 * span)
            for (int i = start; i < start + span; ++i)
            {
                const float a = v[static_cast<std::size_t>(i)];
                const float b = v[static_cast<std::size_t>(i + span)];
                v[static_cast<std::size_t>(i)] = a + b;
                v[static_cast<std::size_t>(i + span)] = a - b;
            }

    constexpr float scale = 0.35355339059327373f; // 1/√8
    for (auto& value : v)
        value *= scale;
}
} // namespace

void Reverb::prepare(double newSampleRate, const ReverbProfile& reverbProfile)
{
    sampleRate = newSampleRate;
    const double size = largestSize(reverbProfile);

    preDelay.buffer.assign(static_cast<std::size_t>(toSamples(largestPreDelayMs(reverbProfile), sampleRate) + 1), 0.0f);
    for (std::size_t i = 0; i < diffusers.size(); ++i)
        diffusers[i].buffer.assign(static_cast<std::size_t>(toSamples(diffuserMs[i] * std::sqrt(size), sampleRate) + 1),
                                   0.0f);
    for (std::size_t i = 0; i < lines.size(); ++i)
        lines[i].buffer.assign(static_cast<std::size_t>(toSamples(lineMs[i] * size, sampleRate) + 1), 0.0f);

    setVoicing(reverbProfile.voicings[0]);
    reset();
}

void Reverb::setVoicing(const ReverbVoicing& voicing) noexcept
{
    // Safe before prepare(), when the buffers are still empty: settings may arrive first (the plugin
    // passes them before preparing), and std::clamp with an upper bound below its lower one is
    // undefined (MSVC's Debug runtime stops on it). The engine sets the voicing again once prepared.
    auto fit = [](Line& line, int samples)
    {
        const int capacity = static_cast<int>(line.buffer.size());
        line.length = capacity > 0 ? std::clamp(samples, 1, capacity) : 1;
        line.position = 0;
    };

    fit(preDelay, voicing.preDelayMs > 0.0f ? toSamples(voicing.preDelayMs, sampleRate) : 1);
    for (std::size_t i = 0; i < diffusers.size(); ++i)
        fit(diffusers[i], toSamples(diffuserMs[i] * std::sqrt(static_cast<double>(voicing.size)), sampleRate));

    diffusion = voicing.diffusion;
    const double decay = std::max(0.05, static_cast<double>(voicing.decaySeconds));
    const double ratio = std::clamp(static_cast<double>(voicing.highFrequencyRatio), 0.05, 1.0);

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        fit(lines[i], toSamples(lineMs[i] * voicing.size, sampleRate));

        // Jot & Chaigne 1991: gain for a 60 dB decay over `decay` seconds at this line length, and
        // a one-pole low-pass that shortens the decay towards the top of the band by `ratio`.
        const double gain = std::pow(10.0, -3.0 * lines[i].length / (sampleRate * decay));
        const double pole = std::log(10.0) / 4.0 * std::log10(gain) * (1.0 - 1.0 / (ratio * ratio));
        absorptionGain[i] = static_cast<float>(gain);
        absorptionPole[i] = static_cast<float>(std::clamp(pole, 0.0, 0.99));
    }
}

void Reverb::reset() noexcept
{
    auto clear = [](Line& line)
    {
        std::fill(line.buffer.begin(), line.buffer.end(), 0.0f);
        line.position = 0;
    };
    clear(preDelay);
    for (auto& diffuser : diffusers)
        clear(diffuser);
    for (auto& line : lines)
        clear(line);
    absorptionState.fill(0.0f);
}

void Reverb::processSample(float input, float& left, float& right) noexcept
{
    // Pre-delay (a delay of `length` samples: read the oldest, then overwrite it).
    float x = preDelay.readOldest();
    preDelay.writeAndAdvance(input);

    // Schroeder allpass diffusers: smear the input into a dense burst before the network.
    for (auto& diffuser : diffusers)
    {
        const float delayed = diffuser.readOldest();
        const float w = x - diffusion * delayed;
        diffuser.writeAndAdvance(w);
        x = delayed + diffusion * w;
    }

    std::array<float, numLines> filtered{};
    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        // Absorption: y = g(1 − b)·x + b·y₋₁ (Jot & Chaigne 1991).
        auto& state = absorptionState[i];
        state = absorptionGain[i] * (1.0f - absorptionPole[i]) * lines[i].readOldest() + absorptionPole[i] * state;
        filtered[i] = state;
        sumLeft += leftSigns[i] * state;
        sumRight += rightSigns[i] * state;
    }

    hadamard8(filtered);
    for (std::size_t i = 0; i < lines.size(); ++i)
        lines[i].writeAndAdvance(filtered[i] + x);

    left = 0.5f * sumLeft;
    right = 0.5f * sumRight;
}

} // namespace fivea::dsp
