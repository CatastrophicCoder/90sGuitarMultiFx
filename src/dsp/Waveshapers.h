#pragma once

#include <algorithm>
#include <cmath>

// Candidate clipping curves for the Distortion/Overdrive block (plan §9.1). Interchangeable: a
// DriveProfile names one per mode. None of them is the original unit's curve, which is unknown
// until measured (plan §9.3); tanh is deliberately not offered as a permanent model.
//
// Every curve maps 0 to 0, never decreases, and stays within [−1, 1].

namespace fivea::dsp
{

enum class WaveshaperType
{
    CubicSoft,           // smooth, gentle onset; 1.5x − 0.5x³, flat beyond |x| = 1
    RationalSoft,        // Padé approximant of tanh, x(27 + x²)/(27 + 9x²), flat beyond |x| = 3
    PiecewiseLinear,     // linear to 0.5, half slope to 1.5, then flat: a "harder" knee
    AsymmetricPiecewise, // piecewise-linear, with the negative side clipping at −0.7
    Hard                 // clamp to ±1
};

namespace waveshapers
{

[[nodiscard]] inline float cubicSoft(float x) noexcept
{
    if (x >= 1.0f)
        return 1.0f;
    if (x <= -1.0f)
        return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

[[nodiscard]] inline float rationalSoft(float x) noexcept
{
    const float clamped = std::clamp(x, -3.0f, 3.0f);
    const float squared = clamped * clamped;
    return clamped * (27.0f + squared) / (27.0f + 9.0f * squared);
}

[[nodiscard]] inline float piecewiseLinear(float x) noexcept
{
    const float magnitude = std::abs(x);
    const float shaped = magnitude <= 0.5f ? magnitude : (magnitude <= 1.5f ? 0.5f + 0.5f * (magnitude - 0.5f) : 1.0f);
    return std::copysign(shaped, x);
}

inline constexpr float asymmetricNegativeLimit = 0.7f;

[[nodiscard]] inline float asymmetricPiecewise(float x) noexcept
{
    if (x >= 0.0f)
        return piecewiseLinear(x);
    return asymmetricNegativeLimit * piecewiseLinear(x / asymmetricNegativeLimit);
}

[[nodiscard]] inline float hard(float x) noexcept
{
    return std::clamp(x, -1.0f, 1.0f);
}

[[nodiscard]] inline float shape(WaveshaperType type, float x) noexcept
{
    switch (type)
    {
    case WaveshaperType::CubicSoft:
        return cubicSoft(x);
    case WaveshaperType::RationalSoft:
        return rationalSoft(x);
    case WaveshaperType::PiecewiseLinear:
        return piecewiseLinear(x);
    case WaveshaperType::AsymmetricPiecewise:
        return asymmetricPiecewise(x);
    case WaveshaperType::Hard:
        return hard(x);
    }
    return x;
}

} // namespace waveshapers

} // namespace fivea::dsp
