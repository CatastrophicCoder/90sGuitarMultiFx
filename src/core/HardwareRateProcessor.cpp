#include "core/HardwareRateProcessor.h"

#include "core/ModelProfile.h"

#include <algorithm>

namespace fivea
{

void HardwareRateProcessor::prepare(const ProcessSpec& spec, bool atHardwareRate)
{
    numChannels = std::clamp(spec.numChannels, 1, maximumChannels);
    chunkSize = std::max(spec.maximumBlockSize, 64);
    const double internalRate = documented::internalSampleRate;

    converting = atHardwareRate && spec.sampleRate != internalRate &&
                 toHardwareRate.prepare(spec.sampleRate, internalRate, numChannels) &&
                 toHostRate.prepare(internalRate, spec.sampleRate, numChannels);

    if (!converting)
    {
        engine.prepare(spec);
        latencySamples = engine.getLatencySamples();
        for (auto* storage : {&internal, &queue})
            for (auto& channel : *storage)
                channel.clear();
        silence.clear();
        discard.clear();
        queued = 0;
        return;
    }

    const int internalBlock = toHardwareRate.maximumOutputFor(chunkSize);
    engine.prepare({internalRate, internalBlock, numChannels, spec.driveOversampling});

    // Everything in common-rate samples: a host sample is `up` of them, a 44.1 kHz sample `down`.
    const int hostSample = toHardwareRate.getUpFactor();
    const int hardwareSample = toHardwareRate.getDownFactor();
    const long long delay = static_cast<long long>(toHardwareRate.getDelayInCommonRateSamples()) +
                            static_cast<long long>(engine.getLatencySamples()) * hardwareSample +
                            toHostRate.getDelayInCommonRateSamples();
    const auto padding = static_cast<int>((hostSample - delay % hostSample) % hostSample);
    if (padding > 0)
        (void)toHostRate.prepare(internalRate, spec.sampleRate, numChannels, padding); // same ratio: accepted
    latencySamples = static_cast<int>((delay + padding) / hostSample);

    // The up converter can run ahead of the host by up to ceil(down / up) samples (in its own
    // factors) at the end of a block; room for that on top of a whole block's output.
    const int ahead = (hardwareSample + hostSample - 1) / hostSample + 1;
    const int queueLength = toHostRate.maximumOutputFor(internalBlock) + ahead;
    for (int channel = 0; channel < maximumChannels; ++channel)
    {
        const bool used = channel < numChannels;
        internal[static_cast<std::size_t>(channel)].assign(used ? static_cast<std::size_t>(internalBlock) : 0, 0.0f);
        queue[static_cast<std::size_t>(channel)].assign(used ? static_cast<std::size_t>(queueLength) : 0, 0.0f);
    }
    silence.assign(static_cast<std::size_t>(chunkSize), 0.0f);
    discard.assign(static_cast<std::size_t>(chunkSize), 0.0f);
    reset();
}

void HardwareRateProcessor::reset() noexcept
{
    engine.reset();
    toHardwareRate.reset();
    toHostRate.reset();
    queued = 0;
}

void HardwareRateProcessor::process(AudioBufferView buffer) noexcept
{
    if (!converting)
    {
        engine.process(buffer);
        return;
    }

    const int channels = std::min(buffer.getNumChannels(), numChannels);
    if (channels == 0)
        return;

    std::array<float*, maximumChannels> pointers{};
    for (int start = 0; start < buffer.getNumSamples(); start += chunkSize)
    {
        const int length = std::min(chunkSize, buffer.getNumSamples() - start);
        for (int channel = 0; channel < channels; ++channel)
            pointers[static_cast<std::size_t>(channel)] = buffer.getChannel(channel) + start;
        processChunk({pointers.data(), channels, length});
    }
}

void HardwareRateProcessor::processChunk(AudioBufferView chunk) noexcept
{
    const int length = chunk.getNumSamples();
    const int present = chunk.getNumChannels();

    std::array<const float*, maximumChannels> hostIn{};
    std::array<float*, maximumChannels> internalPointers{};
    std::array<float*, maximumChannels> queueTail{};
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto index = static_cast<std::size_t>(channel);
        hostIn[index] = channel < present ? chunk.getChannel(channel) : silence.data();
        internalPointers[index] = internal[index].data();
        queueTail[index] = queue[index].data() + queued;
    }

    const int internalLength = toHardwareRate.process(hostIn.data(), length, internalPointers.data());
    engine.process({internalPointers.data(), numChannels, internalLength});

    std::array<const float*, maximumChannels> internalIn{};
    for (int channel = 0; channel < numChannels; ++channel)
        internalIn[static_cast<std::size_t>(channel)] = internal[static_cast<std::size_t>(channel)].data();
    queued += toHostRate.process(internalIn.data(), internalLength, queueTail.data());

    // Hand out this block's samples; the few produced ahead move to the front of the queue.
    const int handedOut = std::min(length, queued);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto& samples = queue[static_cast<std::size_t>(channel)];
        float* destination = channel < present ? chunk.getChannel(channel) : discard.data();
        std::copy(samples.begin(), samples.begin() + handedOut, destination);
        std::fill(destination + handedOut, destination + length, 0.0f); // never reached: counts are exact
        std::copy(samples.begin() + handedOut, samples.begin() + queued, samples.begin());
    }
    queued -= handedOut;
}

} // namespace fivea
