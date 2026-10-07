#pragma once

#include <algorithm>
#include <cmath>

namespace fivea::dsp
{

// The Kaiser window, for windowed-sinc FIR design (J. F. Kaiser, "Nonrecursive Digital Filter
// Design Using the I0-sinh Window Function", Proc. IEEE ISCAS, 1974).

// Modified Bessel function of the first kind, order zero, by its power series.
inline double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 50; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1.0e-12 * sum)
            break;
    }
    return sum;
}

// The window at `offset` from the centre of a window reaching `halfLength` either side.
inline double kaiserWindow(double offset, double halfLength, double beta)
{
    const double ratio = offset / halfLength;
    return besselI0(beta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / besselI0(beta);
}

// β for a stopband attenuation of `decibels` (above 50 dB).
inline double kaiserBetaFor(double decibels)
{
    return 0.1102 * (decibels - 8.7);
}

} // namespace fivea::dsp
