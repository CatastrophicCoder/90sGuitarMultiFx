#include "core/ProgramTransition.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fivea
{

namespace
{
// Raised cosine, 0 at position 0 and 1 at position length: it leaves and arrives with zero slope,
// so the fade has no corner (a linear fade's corners are audible as small clicks on a low note).
float fadeGain(int position, int length) noexcept
{
    return static_cast<float>(0.5 - 0.5 * std::cos(std::numbers::pi * position / length));
}

// The position, along a fade of `length`, at which fadeGain() reaches `gain`.
int positionForGain(double gain, int length) noexcept
{
    return static_cast<int>(std::lround(std::acos(1.0 - 2.0 * std::clamp(gain, 0.0, 1.0)) / std::numbers::pi * length));
}
} // namespace

void ProgramTransition::prepare(double sampleRate, double fadeOutSeconds, double fadeInSeconds) noexcept
{
    fadeOutLength = std::max(0, static_cast<int>(std::lround(sampleRate * fadeOutSeconds)));
    fadeInLength = std::max(0, static_cast<int>(std::lround(sampleRate * fadeInSeconds)));
    reset();
}

void ProgramTransition::reset() noexcept
{
    phase = Phase::Idle;
    position = 0;
    handled = requested.load(std::memory_order_acquire);
}

bool ProgramTransition::poll(bool fadeAudio) noexcept
{
    const auto newest = requested.load(std::memory_order_acquire);
    if (newest == handled)
        return false;
    handled = newest;

    switch (phase)
    {
    case Phase::Idle:
    case Phase::Holding:
        if (!fadeAudio)
        {
            phase = Phase::Holding;
            break;
        }
        phase = Phase::FadingOut;
        position = fadeOutLength;
        break;
    case Phase::FadingIn:
    {
        // Down from the gain reached, along the fade-out's curve.
        const double gain = fadeInLength > 0 ? fadeGain(position, fadeInLength) : 1.0;
        phase = Phase::FadingOut;
        position = positionForGain(gain, fadeOutLength);
        break;
    }
    case Phase::FadingOut:
    case Phase::Silent:
        break; // already on the way down, or there: the newer settings are switched to instead
    }

    if (phase == Phase::FadingOut && position <= 0)
        phase = Phase::Silent;
    return true;
}

void ProgramTransition::switched() noexcept
{
    if (phase == Phase::Holding)
    {
        phase = Phase::Idle;
        return;
    }
    phase = fadeInLength > 0 ? Phase::FadingIn : Phase::Idle;
    position = 0;
}

void ProgramTransition::applyGain(AudioBufferView buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    auto scale = [&](int sample, float gain)
    {
        for (int channel = 0; channel < numChannels; ++channel)
            buffer.getChannel(channel)[sample] *= gain;
    };

    for (int sample = 0; sample < numSamples; ++sample)
    {
        switch (phase)
        {
        case Phase::Idle:
        case Phase::Holding:
            return; // unity: leave the rest untouched, bit for bit
        case Phase::FadingOut:
            --position;
            scale(sample, fadeGain(position, fadeOutLength));
            if (position == 0)
                phase = Phase::Silent;
            break;
        case Phase::Silent:
            for (int channel = 0; channel < numChannels; ++channel)
                buffer.getChannel(channel)[sample] = 0.0f; // exactly, whatever was there
            break;
        case Phase::FadingIn:
            ++position;
            if (position >= fadeInLength)
            {
                phase = Phase::Idle;
                return; // the last fade-in sample is at unity: nothing to scale
            }
            scale(sample, fadeGain(position, fadeInLength));
            break;
        }
    }
}

} // namespace fivea
