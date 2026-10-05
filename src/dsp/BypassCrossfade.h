#pragma once

#include "dsp/Smoothing.h"

namespace fivea::dsp
{

// Fades one effect block in or out when it is switched (plan §15), instead of cutting it. The
// caller mixes `dry × (1 − g) + wet × g` with g from getNextWetGain(). Linear rather than
// equal-power: a block's output is strongly correlated with its input, so a linear fade keeps the
// level steady. While isBypassed() the block need not run at all; while ramping it must, or the
// fade would be of stale output.
class BypassCrossfade
{
public:
    void prepare(double sampleRate, double fadeSeconds) noexcept
    {
        wetGain.prepare(sampleRate, fadeSeconds);
        reset(enabled);
    }

    void reset(bool shouldBeEnabled) noexcept
    {
        enabled = shouldBeEnabled;
        wetGain.setCurrentAndTarget(enabled ? 1.0f : 0.0f);
    }

    void setEnabled(bool shouldBeEnabled) noexcept
    {
        enabled = shouldBeEnabled;
        wetGain.setTarget(enabled ? 1.0f : 0.0f);
    }

    [[nodiscard]] float getNextWetGain() noexcept { return wetGain.getNextValue(); }

    [[nodiscard]] bool isRamping() const noexcept { return wetGain.isSmoothing(); }
    [[nodiscard]] bool isEnabled() const noexcept { return enabled; } // the state it is heading for
    [[nodiscard]] bool isBypassed() const noexcept { return !enabled && !isRamping(); }
    [[nodiscard]] bool isFullyEnabled() const noexcept { return enabled && !isRamping(); }

private:
    LinearSmoother wetGain;
    bool enabled = false;
};

} // namespace fivea::dsp
