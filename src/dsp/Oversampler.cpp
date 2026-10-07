#include "dsp/Oversampler.h"

#include "dsp/Kaiser.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fivea::dsp
{

namespace
{
// Lengths of the form 4k + 3 keep the half-band centre tap where the polyphase split and the
// whole-sample latency need it. Kaiser β ≈ 10 gives about 100 dB of stopband attenuation
// (β = 0.1102 (A − 8.7), J. F. Kaiser 1974).
constexpr int firstStageTaps = 139; // transition 20 → 24.1 kHz at a 44.1 kHz base rate
constexpr int secondStageTaps = 27; // the second stage only has to reject images above 2× rate
constexpr double kaiserBeta = 10.06;
} // namespace

void Oversampler::History::resize(int newLength)
{
    length = newLength;
    data.assign(static_cast<std::size_t>(2 * newLength), 0.0f);
    position = 0;
}

void Oversampler::History::clear() noexcept
{
    std::fill(data.begin(), data.end(), 0.0f);
    position = 0;
}

void Oversampler::HalfbandStage::design(int numTaps, double beta)
{
    // Ideal half-band low-pass (cutoff at a quarter of the rate it runs at) under a Kaiser window,
    // normalised to unity gain at DC. Every second tap away from the centre is exactly zero.
    taps.assign(static_cast<std::size_t>(numTaps), 0.0);
    const double centre = (numTaps - 1) / 2.0;
    const double windowNorm = besselI0(beta);
    double sum = 0.0;

    for (int n = 0; n < numTaps; ++n)
    {
        const double offset = n - centre;
        const double sinc =
            offset == 0.0 ? 1.0 : std::sin(std::numbers::pi * offset / 2.0) / (std::numbers::pi * offset / 2.0);
        const double ratio = offset / centre;
        const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / windowNorm;
        taps[static_cast<std::size_t>(n)] = 0.5 * sinc * window;
        sum += taps[static_cast<std::size_t>(n)];
    }

    for (auto& tap : taps)
        tap /= sum;

    for (int phase = 0; phase < 2; ++phase)
    {
        auto& phaseTaps = phases[static_cast<std::size_t>(phase)];
        phaseTaps.clear();
        for (int n = phase; n < numTaps; n += 2)
            phaseTaps.push_back(2.0 * taps[static_cast<std::size_t>(n)]); // ×2: zero-stuffing halves the level
    }
}

void Oversampler::HalfbandStage::prepareState(StageState& state) const
{
    state.upHistory.resize(static_cast<int>(phases[0].size()));
    state.downHistory.resize(getNumTaps());
}

void Oversampler::HalfbandStage::upsample(StageState& state, float input, float output[2]) const noexcept
{
    // Zero-stuffing then filtering, done as two polyphase branches: output 2m uses the even taps,
    // output 2m + 1 the odd ones, both over the same input history.
    state.upHistory.push(input);
    const float* history = state.upHistory.newestFirst();

    for (int phase = 0; phase < 2; ++phase)
    {
        const auto& phaseTaps = phases[static_cast<std::size_t>(phase)];
        double sum = 0.0;
        for (std::size_t j = 0; j < phaseTaps.size(); ++j)
            sum += phaseTaps[j] * history[j];
        output[phase] = static_cast<float>(sum);
    }
}

float Oversampler::HalfbandStage::downsample(StageState& state, float first, float second) const noexcept
{
    // Filter at the high rate and keep every second output: the one aligned with `first`. Keeping
    // the one aligned with `second` would put the result half a base-rate sample off, and the
    // latency would no longer be a whole number of samples.
    state.downHistory.push(first);
    const float* history = state.downHistory.newestFirst();

    double sum = 0.0;
    for (std::size_t n = 0; n < taps.size(); ++n)
        sum += taps[n] * history[n];

    state.downHistory.push(second);
    return static_cast<float>(sum);
}

void Oversampler::prepare(int newFactor, int numChannels)
{
    factor = newFactor >= 4 ? 4 : (newFactor >= 2 ? 2 : 1);
    firstStage.design(firstStageTaps, kaiserBeta);
    secondStage.design(secondStageTaps, kaiserBeta);

    const int count = std::clamp(numChannels, 1, maximumChannels);
    for (int channel = 0; channel < count; ++channel)
    {
        auto& c = channels[static_cast<std::size_t>(channel)];
        firstStage.prepareState(c.first);
        secondStage.prepareState(c.second);
    }
    reset();
}

void Oversampler::reset() noexcept
{
    for (auto& c : channels)
    {
        for (auto* history : {&c.first.upHistory, &c.first.downHistory, &c.second.upHistory, &c.second.downHistory})
            history->clear();
        c.alignment = 0.0f;
    }
}

int Oversampler::getLatencySamples() const noexcept
{
    // Each stage's up- and down-sampling filters together delay by (taps − 1) samples at that
    // stage's output rate; the 4× alignment adds one sample at 2×.
    const int first = (firstStage.getNumTaps() - 1) / 2;
    if (factor == 1)
        return 0;
    if (factor == 2)
        return first;
    return first + (secondStage.getNumTaps() - 1 + 2) / 4;
}

} // namespace fivea::dsp
