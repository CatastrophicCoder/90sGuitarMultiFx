#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

namespace fivea::dsp
{

// Runs a per-sample nonlinearity at 2× or 4× the sample rate, to keep the harmonics it creates
// from folding back as aliases (plan §9.2). Linear-phase FIR half-band filters, Kaiser-windowed
// sinc, about 100 dB stopband: one stage for 2× (139 taps), two for 4× (139 then 27 taps).
// Linear phase means a constant, whole-sample latency at the base rate, reported to the host.
//
// Sample-by-sample, so the caller needs no oversampled buffers. Memory is allocated in prepare().
class Oversampler
{
public:
    static constexpr int maximumChannels = 2;

    void prepare(int newFactor, int numChannels); // factor 1, 2 or 4; allocates
    void reset() noexcept;

    [[nodiscard]] int getFactor() const noexcept { return factor; }
    [[nodiscard]] int getLatencySamples() const noexcept;

    template <typename Function>
    [[nodiscard]] float processSample(int channel, float input, Function&& nonlinearity) noexcept
    {
        assert(channel >= 0 && channel < maximumChannels);
        auto& c = channels[static_cast<std::size_t>(channel)];

        if (factor == 1)
            return nonlinearity(input);

        float up[2];
        firstStage.upsample(c.first, input, up);

        if (factor == 2)
            return firstStage.downsample(c.first, nonlinearity(up[0]), nonlinearity(up[1]));

        // A one-sample delay at 2× makes the 4× latency a whole number of base-rate samples.
        const float delayed[2] = {c.alignment, up[0]};
        c.alignment = up[1];

        float down[2];
        for (int half = 0; half < 2; ++half)
        {
            float quarter[2];
            secondStage.upsample(c.second, delayed[half], quarter);
            down[half] = secondStage.downsample(c.second, nonlinearity(quarter[0]), nonlinearity(quarter[1]));
        }
        return firstStage.downsample(c.first, down[0], down[1]);
    }

private:
    // Newest-first history with a doubled buffer, so a contiguous read never wraps.
    struct History
    {
        std::vector<float> data;
        int length = 0;
        int position = 0;

        void resize(int newLength);
        void clear() noexcept;
        void push(float sample) noexcept
        {
            position = (position == 0 ? length : position) - 1;
            data[static_cast<std::size_t>(position)] = sample;
            data[static_cast<std::size_t>(position + length)] = sample;
        }
        [[nodiscard]] const float* newestFirst() const noexcept { return data.data() + position; }
    };

    struct StageState
    {
        History upHistory;
        History downHistory;
    };

    class HalfbandStage
    {
    public:
        void design(int numTaps, double kaiserBeta);
        void prepareState(StageState& state) const;
        [[nodiscard]] int getNumTaps() const noexcept { return static_cast<int>(taps.size()); }

        void upsample(StageState& state, float input, float output[2]) const noexcept;
        [[nodiscard]] float downsample(StageState& state, float first, float second) const noexcept;

    private:
        std::vector<double> taps;
        std::array<std::vector<double>, 2> phases; // even and odd taps, for the polyphase upsampler
    };

    struct ChannelState
    {
        StageState first;
        StageState second;
        float alignment = 0.0f;
    };

    int factor = 1;
    HalfbandStage firstStage;
    HalfbandStage secondStage;
    std::array<ChannelState, maximumChannels> channels;
};

} // namespace fivea::dsp
