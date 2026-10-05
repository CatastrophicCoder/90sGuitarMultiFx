#include "dsp/DelayLine.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace fivea::dsp
{

void DelayLine::prepare(int maximumDelaySamples, int numChannels)
{
    capacity = std::max(maximumDelaySamples, 2) + 4; // room for the interpolator's neighbours
    const int count = std::clamp(numChannels, 1, maximumChannels);
    for (int channel = 0; channel < count; ++channel)
        channels[static_cast<std::size_t>(channel)].buffer.assign(static_cast<std::size_t>(capacity), 0.0f);
    reset();
}

void DelayLine::reset() noexcept
{
    for (auto& channel : channels)
    {
        std::fill(channel.buffer.begin(), channel.buffer.end(), 0.0f);
        channel.writeIndex = 0;
    }
}

float DelayLine::read(int channel, float delaySamples) const noexcept
{
    assert(channel >= 0 && channel < maximumChannels);
    const auto& c = channels[static_cast<std::size_t>(channel)];

    const float delay = std::clamp(delaySamples, minimumDelay(), maximumDelay());
    const int whole = static_cast<int>(delay);
    const float fraction = delay - static_cast<float>(whole);

    const float p1 = c.samplesAgo(whole, capacity);
    const float p2 = c.samplesAgo(whole + 1, capacity);

    if (interpolation == Interpolation::Linear)
        return p1 + fraction * (p2 - p1);

    // Catmull-Rom (4-point Hermite) between p1 and p2; p0 is the newer neighbour, p3 the older.
    const float p0 = c.samplesAgo(whole - 1, capacity);
    const float p3 = c.samplesAgo(whole + 2, capacity);
    const float c1 = 0.5f * (p2 - p0);
    const float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    const float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
    return ((c3 * fraction + c2) * fraction + c1) * fraction + p1;
}

void DelayLine::write(int channel, float sample) noexcept
{
    assert(channel >= 0 && channel < maximumChannels);
    auto& c = channels[static_cast<std::size_t>(channel)];
    c.buffer[static_cast<std::size_t>(c.writeIndex)] = sample;
    c.writeIndex = c.writeIndex + 1 == capacity ? 0 : c.writeIndex + 1;
}

} // namespace fivea::dsp
