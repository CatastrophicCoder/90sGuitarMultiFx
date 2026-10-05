#pragma once

#include <cmath>
#include <functional>

namespace fivea::dsp
{

// Linear ramp towards a target over a fixed duration. Lands exactly on the target when the ramp
// ends, so a gain smoothed back to 1.0 becomes bit-transparent rather than approaching it forever.
class LinearSmoother
{
public:
    void prepare(double sampleRate, double rampSeconds) noexcept
    {
        rampLengthSamples = rampSeconds > 0.0 ? static_cast<int>(std::lround(sampleRate * rampSeconds)) : 0;
        setCurrentAndTarget(target);
    }

    void setCurrentAndTarget(float value) noexcept
    {
        current = value;
        target = value;
        step = 0.0f;
        samplesRemaining = 0;
    }

    void setTarget(float newTarget) noexcept
    {
        // Exact comparison on purpose: hosts resend unchanged values every block, and those must not
        // restart the ramp.
        if (std::equal_to<float>{}(newTarget, target))
            return;

        target = newTarget;

        if (rampLengthSamples <= 0)
        {
            setCurrentAndTarget(newTarget);
            return;
        }

        samplesRemaining = rampLengthSamples;
        step = (target - current) / static_cast<float>(rampLengthSamples);
    }

    [[nodiscard]] float getNextValue() noexcept
    {
        if (samplesRemaining <= 0)
            return target;

        if (--samplesRemaining == 0)
            current = target;
        else
            current += step;

        return current;
    }

    // Advances by numSamples and returns the value reached; lands exactly on the target like
    // getNextValue().
    float skip(int numSamples) noexcept
    {
        float value = getCurrent();
        for (int n = 0; n < numSamples && isSmoothing(); ++n)
            value = getNextValue();
        return value;
    }

    [[nodiscard]] float getCurrent() const noexcept { return isSmoothing() ? current : target; }
    [[nodiscard]] bool isSmoothing() const noexcept { return samplesRemaining > 0; }
    [[nodiscard]] float getTarget() const noexcept { return target; }

private:
    float current = 0.0f;
    float target = 0.0f;
    float step = 0.0f;
    int rampLengthSamples = 0;
    int samplesRemaining = 0;
};

} // namespace fivea::dsp
