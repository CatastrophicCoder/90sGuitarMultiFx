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
        , offsetPointers(static_cast<std::size_t>(numChannels)) // sized here, so viewOf() never allocates
    {
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

    // The returned view shares storage with the previous one: use one at a time. Pointers are
    // taken from this buffer's own samples every time, so a copied buffer views its own data.
    [[nodiscard]] AudioBufferView viewOf(int startSample, int numSamples)
    {
        for (std::size_t channel = 0; channel < samples.size(); ++channel)
            offsetPointers[channel] = samples[channel].data() + startSample;

        return {offsetPointers.data(), static_cast<int>(samples.size()), numSamples};
    }

    [[nodiscard]] int getNumChannels() const { return static_cast<int>(samples.size()); }
    [[nodiscard]] int getNumSamples() const { return static_cast<int>(samples.front().size()); }
    [[nodiscard]] const std::vector<std::vector<float>>& data() const { return samples; }
    [[nodiscard]] std::vector<float>& channel(int index) { return samples[static_cast<std::size_t>(index)]; }

private:
    std::vector<std::vector<float>> samples;
    std::vector<float*> offsetPointers;
};

// A plucked open A string (Karplus-Strong, 110 Hz), the same in every channel: the nominal guitar
// input of the level tests, peaking near −10 dBFS at the default amplitude (about −27 dB RMS over
// its first 1.5 s), like a DI guitar at a healthy interface level.
inline void fillPluck(PlanarBuffer& buffer, double sampleRate, float amplitude = 0.3f)
{
    const int period = static_cast<int>(sampleRate / 110.0);
    std::vector<float> string(static_cast<std::size_t>(period));
    std::mt19937 generator{1};
    std::uniform_real_distribution<float> distribution{-amplitude, amplitude};
    for (auto& sample : string)
        sample = distribution(generator);

    std::vector<float> pluck(buffer.channel(0).size());
    for (std::size_t n = 0, position = 0; n < pluck.size(); ++n)
    {
        const std::size_t next = (position + 1) % string.size();
        pluck[n] = string[position];
        string[position] = 0.4985f * (string[position] + string[next]);
        position = next;
    }
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        buffer.channel(channel) = pluck;
}

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

// Amplitude of the component at `frequency` in samples[start, start + length), by the Goertzel
// algorithm, with a Hann window so neighbouring components leak little. A full-scale sine of
// amplitude A at exactly that frequency reads ≈ A.
inline double toneAmplitude(const std::vector<float>& samples, double frequency, double sampleRate, std::size_t start,
                            std::size_t length)
{
    const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const double coefficient = 2.0 * std::cos(omega);
    double previous = 0.0;
    double beforePrevious = 0.0;
    double windowSum = 0.0;
    for (std::size_t n = 0; n < length; ++n)
    {
        const double window =
            0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(n) / static_cast<double>(length - 1));
        windowSum += window;
        const double current = window * samples[start + n] + coefficient * previous - beforePrevious;
        beforePrevious = previous;
        previous = current;
    }
    const double power =
        previous * previous + beforePrevious * beforePrevious - coefficient * previous * beforePrevious;
    return 2.0 * std::sqrt(std::max(power, 0.0)) / windowSum;
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
