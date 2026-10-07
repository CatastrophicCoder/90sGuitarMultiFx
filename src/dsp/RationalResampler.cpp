#include "dsp/RationalResampler.h"

#include "dsp/Kaiser.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

namespace fivea::dsp
{

namespace
{
constexpr double passbandEdgeHz = 20000.0;
// Kaiser's estimates fall slightly short at the band edge (99.85 dB measured when designed for
// 100): designed for 103 dB, to measure at least 100.
constexpr double stopbandAttenuationDb = 103.0;
constexpr std::size_t maximumTableSize = std::size_t{1} << 20;

bool isWholeHz(double rate)
{
    return rate >= 1.0 && rate <= 1.0e7 && std::abs(rate - std::round(rate)) < 1.0e-9;
}
} // namespace

bool RationalResampler::prepare(double newInputRate, double outputRate, int newNumChannels, int extraDelaySamples)
{
    table.clear();
    numChannels = 0;
    if (!isWholeHz(newInputRate) || !isWholeHz(outputRate))
        return false;

    const auto from = static_cast<long long>(std::llround(newInputRate));
    const auto to = static_cast<long long>(std::llround(outputRate));
    const auto divisor = std::gcd(from, to);
    const auto newUp = to / divisor;
    const auto newDown = from / divisor;
    const int channels = std::clamp(newNumChannels, 1, maximumChannels);
    const int extraDelay = std::max(0, extraDelaySamples);

    if (newUp == 1 && newDown == 1)
    {
        inputRate = newInputRate;
        up = down = 1;
        delay = 0;
        tapsPerPhase = 0;
        numChannels = channels;
        reset();
        return true;
    }

    // Kaiser's estimate of the length for the attenuation over the transition band, at the
    // common rate (Kaiser 1974): N - 1 = (A - 7.95) / (2.285 dw).
    const double commonRate = newInputRate * static_cast<double>(newUp);
    const double stopbandEdge = 0.5 * std::min(newInputRate, outputRate);
    const double passbandEdge = std::min(passbandEdgeHz, 0.9 * stopbandEdge);
    const double transition = 2.0 * std::numbers::pi * (stopbandEdge - passbandEdge) / commonRate;
    auto length = static_cast<long long>(std::ceil((stopbandAttenuationDb - 7.95) / (2.285 * transition))) + 1;
    length += 1 - length % 2; // odd: the centre falls on a sample, so the delay is whole

    const auto newTapsPerPhase = (length + extraDelay + newUp - 1) / newUp;
    if (static_cast<std::size_t>(newUp * newTapsPerPhase) > maximumTableSize)
        return false;

    inputRate = newInputRate;
    up = static_cast<int>(newUp);
    down = static_cast<int>(newDown);
    tapsPerPhase = static_cast<int>(newTapsPerPhase);
    const auto centre = (length - 1) / 2;
    delay = static_cast<int>(centre) + extraDelay;

    // Windowed sinc with its cutoff mid-transition, scaled by `up` to make up for the inserted
    // zeros, and shifted by the extra delay.
    const double cutoff = 0.5 * (passbandEdge + stopbandEdge) / commonRate; // cycles per sample
    const double beta = kaiserBetaFor(stopbandAttenuationDb);
    table.assign(static_cast<std::size_t>(up) * static_cast<std::size_t>(tapsPerPhase), 0.0f);
    for (long long n = 0; n < length; ++n)
    {
        const double offset = static_cast<double>(n - centre);
        const double sinc = offset == 0.0 ? 1.0
                                          : std::sin(2.0 * std::numbers::pi * cutoff * offset) /
                                                (2.0 * std::numbers::pi * cutoff * offset);
        const double tap =
            static_cast<double>(up) * 2.0 * cutoff * sinc * kaiserWindow(offset, static_cast<double>(centre), beta);
        const auto index = n + extraDelay; // index into the prototype: phase + j * up
        const auto tablePhase = index % up;
        const auto j = index / up;
        table[static_cast<std::size_t>(tablePhase * tapsPerPhase + j)] = static_cast<float>(tap);
    }

    for (int channel = 0; channel < channels; ++channel)
        history[static_cast<std::size_t>(channel)].assign(static_cast<std::size_t>(2 * tapsPerPhase), 0.0f);
    numChannels = channels;
    reset();
    return true;
}

void RationalResampler::reset() noexcept
{
    for (auto& channel : history)
        std::fill(channel.begin(), channel.end(), 0.0f);
    historyPosition = 0;
    phase = 0;
    inputsUntilDue = 0;
}

int RationalResampler::maximumOutputFor(int numInput) const noexcept
{
    const auto inputs = static_cast<long long>(std::max(0, numInput));
    return static_cast<int>((inputs * up + down - 1) / down) + 1;
}

double RationalResampler::getDelaySeconds() const noexcept
{
    return inputRate > 0.0 ? static_cast<double>(delay) / (inputRate * static_cast<double>(up)) : 0.0;
}

int RationalResampler::process(const float* const* input, int numInput, float* const* output) noexcept
{
    if (numChannels == 0)
        return 0;

    if (tapsPerPhase == 0) // equal rates
    {
        for (int channel = 0; channel < numChannels; ++channel)
            std::copy(input[channel], input[channel] + numInput, output[channel]);
        return numInput;
    }

    int produced = 0;
    for (int n = 0; n < numInput; ++n)
    {
        // Newest first: history[position + j] is the input j samples ago, for j < tapsPerPhase.
        historyPosition = historyPosition == 0 ? tapsPerPhase - 1 : historyPosition - 1;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& samples = history[static_cast<std::size_t>(channel)];
            samples[static_cast<std::size_t>(historyPosition)] = input[channel][n];
            samples[static_cast<std::size_t>(historyPosition + tapsPerPhase)] = input[channel][n];
        }

        // y[k] = sum_j x[floor(k down / up) - j] h[phase + j up], phase = k down mod up.
        while (inputsUntilDue == 0)
        {
            const float* taps = table.data() + static_cast<std::size_t>(phase) * static_cast<std::size_t>(tapsPerPhase);
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float* x = history[static_cast<std::size_t>(channel)].data() + historyPosition;
                float sum = 0.0f;
                for (int j = 0; j < tapsPerPhase; ++j)
                    sum += x[j] * taps[j];
                output[channel][produced] = sum;
            }
            ++produced;
            phase += down;
            inputsUntilDue += phase / up;
            phase %= up;
        }
        --inputsUntilDue;
    }
    return produced;
}

} // namespace fivea::dsp
