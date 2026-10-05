#pragma once

#include <cassert>
#include <cstddef>

namespace fivea
{

struct ProcessSpec
{
    double sampleRate = 44100.0;
    int maximumBlockSize = 0;
    int numChannels = 0;
};

// Non-owning view of planar audio, so the engine never depends on a host's buffer class. The
// caller keeps the channel pointers valid for the duration of one process() call.
class AudioBufferView
{
public:
    AudioBufferView(float* const* channels, int numChannels, int numSamples) noexcept
        : channelPointers(channels)
        , channelCount(numChannels)
        , sampleCount(numSamples)
    {
        assert(numChannels >= 0 && numSamples >= 0);
        assert(channels != nullptr || numChannels == 0);
    }

    [[nodiscard]] int getNumChannels() const noexcept { return channelCount; }
    [[nodiscard]] int getNumSamples() const noexcept { return sampleCount; }

    [[nodiscard]] float* getChannel(int channel) const noexcept
    {
        assert(channel >= 0 && channel < channelCount);
        return channelPointers[static_cast<std::size_t>(channel)];
    }

private:
    float* const* channelPointers;
    int channelCount;
    int sampleCount;
};

} // namespace fivea
