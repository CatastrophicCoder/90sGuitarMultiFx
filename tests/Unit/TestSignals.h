#pragma once

#include "core/AudioBufferView.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <vector>

// Shared test signals and measurements for the engine tests. Everything is deterministic: noise
// uses a fixed seed, so every run sees the same input.

namespace fivea::test
{

class PlanarBuffer
{
public:
    PlanarBuffer(int numChannels, int numSamples)
        : samples(static_cast<std::size_t>(numChannels), std::vector<float>(static_cast<std::size_t>(numSamples)))
        , pointers(static_cast<std::size_t>(numChannels))
        , offsetPointers(static_cast<std::size_t>(numChannels)) // sized here, so viewOf() never allocates
    {
        for (std::size_t channel = 0; channel < samples.size(); ++channel)
            pointers[channel] = samples[channel].data();
    }

    void fillWithNoise(unsigned int seed)
    {
        std::mt19937 generator{seed};
        std::uniform_real_distribution<float> distribution{-1.0f, 1.0f};

        for (auto& channel : samples)
            for (auto& sample : channel)
                sample = distribution(generator);
    }

    void fill(float value)
    {
        for (auto& channel : samples)
            std::fill(channel.begin(), channel.end(), value);
    }

    [[nodiscard]] AudioBufferView view() { return viewOf(0, getNumSamples()); }

    // The returned view shares storage with the previous one: use one at a time.
    [[nodiscard]] AudioBufferView viewOf(int startSample, int numSamples)
    {
        for (std::size_t channel = 0; channel < pointers.size(); ++channel)
            offsetPointers[channel] = pointers[channel] + startSample;

        return {offsetPointers.data(), static_cast<int>(pointers.size()), numSamples};
    }

    [[nodiscard]] int getNumChannels() const { return static_cast<int>(samples.size()); }
    [[nodiscard]] int getNumSamples() const { return static_cast<int>(samples.front().size()); }
    [[nodiscard]] const std::vector<std::vector<float>>& data() const { return samples; }
    [[nodiscard]] std::vector<float>& channel(int index) { return samples[static_cast<std::size_t>(index)]; }

private:
    std::vector<std::vector<float>> samples;
    std::vector<float*> pointers;
    std::vector<float*> offsetPointers;
};

inline void fillSine(PlanarBuffer& buffer, double frequency, double sampleRate, float amplitude)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto& samples = buffer.channel(channel);
        for (std::size_t n = 0; n < samples.size(); ++n)
            samples[n] =
                amplitude *
                static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(n) / sampleRate));
    }
}

inline double rms(const PlanarBuffer& buffer, int channel, int start, int length)
{
    const auto& samples = buffer.data()[static_cast<std::size_t>(channel)];
    double sum = 0.0;
    for (int n = start; n < start + length; ++n)
    {
        const double value = samples[static_cast<std::size_t>(n)];
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(length));
}

inline float peak(const PlanarBuffer& buffer)
{
    float largest = 0.0f;
    for (const auto& channel : buffer.data())
        for (const float sample : channel)
            largest = std::max(largest, std::abs(sample));
    return largest;
}

// Largest sample-to-sample change: a click shows up as a step far beyond the signal's own slope.
inline double largestStep(const PlanarBuffer& buffer, int channel)
{
    const auto& samples = buffer.data()[static_cast<std::size_t>(channel)];
    double largest = 0.0;
    for (std::size_t n = 1; n < samples.size(); ++n)
        largest = std::max(largest, static_cast<double>(std::abs(samples[n] - samples[n - 1])));
    return largest;
}

// Largest second difference |x[n] − 2x[n−1] + x[n−2]|. A low-frequency sine has almost none, so a
// click (a jump in value or slope) stands out far more clearly than in the first difference.
inline double largestSecondDifference(const PlanarBuffer& buffer, int channel)
{
    const auto& samples = buffer.data()[static_cast<std::size_t>(channel)];
    double largest = 0.0;
    for (std::size_t n = 2; n < samples.size(); ++n)
        largest = std::max(largest, static_cast<double>(std::abs(samples[n] - 2.0f * samples[n - 1] + samples[n - 2])));
    return largest;
}

inline bool allFinite(const PlanarBuffer& buffer)
{
    for (const auto& channel : buffer.data())
        for (const float sample : channel)
            if (!std::isfinite(sample))
                return false;
    return true;
}

} // namespace fivea::test
