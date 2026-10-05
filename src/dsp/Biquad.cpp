#include "dsp/Biquad.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fivea::dsp
{

namespace
{
BiquadCoefficients normalise(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
{
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

struct ShelfTerms
{
    double amplitude; // A = 10^(gain/40)
    double cosine;
    double twoRootAAlpha;
};

ShelfTerms shelfTerms(double sampleRate, double frequency, double gainDb, double slope) noexcept
{
    const double amplitude = std::pow(10.0, gainDb / 40.0);
    const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const double alpha = std::sin(omega) / 2.0 * std::sqrt((amplitude + 1.0 / amplitude) * (1.0 / slope - 1.0) + 2.0);
    return {amplitude, std::cos(omega), 2.0 * std::sqrt(amplitude) * alpha};
}
} // namespace

namespace design
{

BiquadCoefficients lowShelf(double sampleRate, double frequency, double gainDb, double slope) noexcept
{
    const auto [a, c, k] = shelfTerms(sampleRate, frequency, gainDb, slope);
    return normalise(a * ((a + 1.0) - (a - 1.0) * c + k), 2.0 * a * ((a - 1.0) - (a + 1.0) * c),
                     a * ((a + 1.0) - (a - 1.0) * c - k), (a + 1.0) + (a - 1.0) * c + k,
                     -2.0 * ((a - 1.0) + (a + 1.0) * c), (a + 1.0) + (a - 1.0) * c - k);
}

BiquadCoefficients highShelf(double sampleRate, double frequency, double gainDb, double slope) noexcept
{
    const auto [a, c, k] = shelfTerms(sampleRate, frequency, gainDb, slope);
    return normalise(a * ((a + 1.0) + (a - 1.0) * c + k), -2.0 * a * ((a - 1.0) + (a + 1.0) * c),
                     a * ((a + 1.0) + (a - 1.0) * c - k), (a + 1.0) - (a - 1.0) * c + k,
                     2.0 * ((a - 1.0) - (a + 1.0) * c), (a + 1.0) - (a - 1.0) * c - k);
}

BiquadCoefficients peaking(double sampleRate, double frequency, double q, double gainDb) noexcept
{
    const double amplitude = std::pow(10.0, gainDb / 40.0);
    const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const double alpha = std::sin(omega) / (2.0 * q);
    const double c = std::cos(omega);
    return normalise(1.0 + alpha * amplitude, -2.0 * c, 1.0 - alpha * amplitude, 1.0 + alpha / amplitude, -2.0 * c,
                     1.0 - alpha / amplitude);
}

} // namespace design

std::complex<double> response(const BiquadCoefficients& coefficients, double frequency, double sampleRate) noexcept
{
    const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const std::complex<double> zInverse = std::polar(1.0, -omega);
    const std::complex<double> zInverse2 = zInverse * zInverse;
    return (coefficients.b0 + coefficients.b1 * zInverse + coefficients.b2 * zInverse2) /
           (1.0 + coefficients.a1 * zInverse + coefficients.a2 * zInverse2);
}

double magnitudeDb(const BiquadCoefficients& coefficients, double frequency, double sampleRate) noexcept
{
    return 20.0 * std::log10(std::abs(response(coefficients, frequency, sampleRate)));
}

} // namespace fivea::dsp
