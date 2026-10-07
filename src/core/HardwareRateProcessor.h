#pragma once

#include "core/AudioBufferView.h"
#include "core/FiveAProcessor.h"
#include "core/ParameterSnapshot.h"
#include "dsp/RationalResampler.h"

#include <array>
#include <vector>

namespace fivea
{

// The engine, run either at the host's rate (native mode) or at the original unit's documented
// 44.1 kHz (EV-003) behind a pair of rate converters (plan §6.2, hardware-rate mode):
//
//   host rate → RationalResampler → FiveAProcessor at 44.1 kHz → RationalResampler → host rate
//
// Running at 44.1 kHz does not recreate the original's converters or its DSP arithmetic; it only
// puts the engine at the same sample rate.
//
// The two converters share one common rate, so the round trip's delay is known exactly: the down
// converter's filter delay, the engine's latency and the up converter's, with the up converter's
// filter lengthened just enough to make the sum a whole number of host samples. Output samples
// the up converter produces ahead of the host block wait in a short queue; counted in whole
// numbers, there are always enough, so the queue adds no latency.
//
// At a 44.1 kHz host, in native mode, or at a rate the converter refuses (one whose ratio to
// 44.1 kHz does not reduce to a practical table), the engine runs at the host rate directly.
//
// prepare() and reset() run off the audio thread (prepare allocates); setParameters() and process()
// are real-time safe. process() accepts blocks of any length, including longer than announced.
class HardwareRateProcessor
{
public:
    void prepare(const ProcessSpec& spec, bool atHardwareRate);
    void reset() noexcept;

    void setParameters(const ParameterSnapshot& snapshot) noexcept { engine.setParameters(snapshot); }
    void process(AudioBufferView buffer) noexcept;

    [[nodiscard]] const ParameterSnapshot& getParameters() const noexcept { return engine.getParameters(); }
    [[nodiscard]] int getLatencySamples() const noexcept { return latencySamples; }
    // True when the engine runs at 44.1 kHz behind the converters.
    [[nodiscard]] bool isConverting() const noexcept { return converting; }

private:
    static constexpr int maximumChannels = dsp::RationalResampler::maximumChannels;

    void processChunk(AudioBufferView chunk) noexcept;

    FiveAProcessor engine;
    dsp::RationalResampler toHardwareRate;
    dsp::RationalResampler toHostRate;
    bool converting = false;
    int latencySamples = 0;
    int numChannels = 0;
    int chunkSize = 0;

    std::array<std::vector<float>, maximumChannels> internal; // the engine's block at 44.1 kHz
    std::array<std::vector<float>, maximumChannels> queue;    // host-rate output not yet handed out
    std::vector<float> silence;                               // input for a channel the host left out
    std::vector<float> discard;                               // output for a channel the host left out
    int queued = 0;
};

} // namespace fivea
