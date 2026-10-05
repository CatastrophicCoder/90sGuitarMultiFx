#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace fivea::dsp
{

// Fractional-delay interpolation, replaceable per plan §11.1. Which one the original unit uses is
// unknown (its "interpolation character" is a measurement target, plan §11.4); Cubic is the
// placeholder. Lagrange and allpass are added if a measurement calls for them.
enum class Interpolation
{
    Linear,
    Cubic // 4-point, 3rd-order Hermite (Catmull-Rom); reproduces straight lines exactly
};

// Circular delay buffer, one per channel, sized once in prepare(). Each sample, per channel:
// read() first, then write(). read(channel, d) returns the input from d samples before the one
// about to be written, so d = 1 is the previous input. Delays are clamped to
// [minimumDelay(), maximumDelay()], so a read can never overrun the buffer.
class DelayLine
{
public:
    static constexpr int maximumChannels = 2;

    void prepare(int maximumDelaySamples, int numChannels); // allocates
    void reset() noexcept;

    void setInterpolation(Interpolation newInterpolation) noexcept { interpolation = newInterpolation; }
    [[nodiscard]] float minimumDelay() const noexcept { return interpolation == Interpolation::Cubic ? 2.0f : 1.0f; }
    [[nodiscard]] float maximumDelay() const noexcept { return static_cast<float>(capacity - 3); }

    [[nodiscard]] float read(int channel, float delaySamples) const noexcept;
    void write(int channel, float sample) noexcept;

private:
    struct Channel
    {
        std::vector<float> buffer;
        int writeIndex = 0;

        [[nodiscard]] float samplesAgo(int age, int bufferSize) const noexcept
        {
            int index = writeIndex - age;
            if (index < 0)
                index += bufferSize;
            return buffer[static_cast<std::size_t>(index)];
        }
    };

    std::array<Channel, maximumChannels> channels;
    int capacity = 4;
    Interpolation interpolation = Interpolation::Cubic;
};

} // namespace fivea::dsp
