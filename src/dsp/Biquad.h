#pragma once

#include <array>
#include <cassert>
#include <complex>
#include <cstddef>

namespace fivea::dsp
{

// Normalised second-order section, a0 = 1:
//   H(z) = (b0 + b1 z⁻¹ + b2 z⁻²) / (1 + a1 z⁻¹ + a2 z⁻²)
struct BiquadCoefficients
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

// Robert Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (the Audio
// EQ Cookbook). Shelves reach `gainDb` on their plateau and gainDb/2 at `frequency`; the peaking
// filter reaches `gainDb` exactly at `frequency`. Boost and cut are exact mirrors in dB.
namespace design
{
[[nodiscard]] BiquadCoefficients lowShelf(double sampleRate, double frequency, double gainDb, double slope) noexcept;
[[nodiscard]] BiquadCoefficients highShelf(double sampleRate, double frequency, double gainDb, double slope) noexcept;
[[nodiscard]] BiquadCoefficients peaking(double sampleRate, double frequency, double q, double gainDb) noexcept;
} // namespace design

// The design's own frequency response, H(e^jω), for tests and displays.
[[nodiscard]] std::complex<double> response(const BiquadCoefficients& coefficients, double frequency,
                                            double sampleRate) noexcept;
[[nodiscard]] double magnitudeDb(const BiquadCoefficients& coefficients, double frequency, double sampleRate) noexcept;

// Transposed direct form II, one state per channel, shared coefficients. Double precision: a
// 100 Hz shelf at 192 kHz has poles close to z = 1, where float coefficients and state lose
// accuracy.
class Biquad
{
public:
    static constexpr int maximumChannels = 2;

    void setCoefficients(const BiquadCoefficients& newCoefficients) noexcept { coefficients = newCoefficients; }
    void reset() noexcept { state = {}; }

    [[nodiscard]] float processSample(int channel, float input) noexcept
    {
        assert(channel >= 0 && channel < maximumChannels);
        auto& s = state[static_cast<std::size_t>(channel)];
        const double x = input;
        const double y = coefficients.b0 * x + s.s1;
        s.s1 = coefficients.b1 * x - coefficients.a1 * y + s.s2;
        s.s2 = coefficients.b2 * x - coefficients.a2 * y;
        return static_cast<float>(y);
    }

private:
    struct State
    {
        double s1 = 0.0;
        double s2 = 0.0;
    };

    BiquadCoefficients coefficients;
    std::array<State, maximumChannels> state{};
};

} // namespace fivea::dsp
