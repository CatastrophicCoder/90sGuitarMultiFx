#include "dsp/TimeEffects.h"

#include "core/StepMapping.h"

#include <algorithm>
#include <cmath>

namespace fivea::dsp
{

namespace
{
constexpr double butterworthQ = 0.7071067811865476;
} // namespace

// --- TimeEffectEngine ----------------------------------------------------------------------------

void TimeEffectEngine::prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile)
{
    sampleRate = newSampleRate;
    numChannels = std::clamp(newNumChannels, 1, DelayLine::maximumChannels);
    delayProfile = modelProfile.delay;
    reverbProfile = modelProfile.reverb;

    // The documented maximum (490 ms) plus room for the interpolator.
    delayLine.prepare(static_cast<int>(std::ceil(mapping::maximumDelayTimeMs() * 0.001 * sampleRate)) + 4, numChannels);
    delayLine.setInterpolation(Interpolation::Cubic);
    feedbackTone.setCoefficients(design::lowPass(sampleRate, delayProfile.feedbackToneHz, butterworthQ));

    const double rampSeconds = modelProfile.switching.parameterRampSeconds;
    feedback.prepare(sampleRate, rampSeconds);
    tapFade.prepare(sampleRate, rampSeconds);

    reverb.prepare(sampleRate, reverbProfile);
    configure(TimeEffectMode::HallReverb, 0, 0, 0, true);
    reset();
}

float TimeEffectEngine::delayInSamples(float ms) const noexcept
{
    // In double, so documented times land on whole samples where the rate allows (100 ms at 48 kHz
    // is exactly 4800). 0 ms is not representable in a read-before-write line; the shortest is used.
    return std::max(delayLine.minimumDelay(), static_cast<float>(static_cast<double>(ms) * 0.001 * sampleRate));
}

void TimeEffectEngine::configure(TimeEffectMode newMode, int time, int fine, int feedbackStep, bool immediate) noexcept
{
    const bool voicingChanged = immediate || newMode != mode;
    mode = newMode;

    if (mapping::hasDelayControls(mode))
    {
        const float tap = delayInSamples(mapping::delayTimeMs(mode, time, fine));
        feedback.setTarget(mapping::feedbackAmount(feedbackStep, delayProfile.feedbackMaximum));

        if (immediate)
        {
            tapA = tapB = tap;
            pendingTap = -1.0f;
            tapFade.setCurrentAndTarget(1.0f);
        }
        else if (tapFade.isSmoothing())
            pendingTap = tap; // started when the current crossfade ends
        else if (tap != tapA)
        {
            tapB = tap;
            tapFade.setCurrentAndTarget(0.0f);
            tapFade.setTarget(1.0f);
        }
    }
    else
        feedback.setTarget(0.0f); // reverb modes have no F.BACK (SRC-001 p. 12)

    if (voicingChanged && mode != TimeEffectMode::Delay)
    {
        const auto voicingIndex = mode == TimeEffectMode::Echoverb
                                      ? std::size_t{0} // Echoverb reverberates with the Hall voicing (placeholder)
                                      : static_cast<std::size_t>(mode) - 1;
        reverb.setVoicing(voicingIndex);
    }

    if (immediate)
        feedback.setCurrentAndTarget(feedback.getTarget());
}

void TimeEffectEngine::reset() noexcept
{
    delayLine.reset();
    feedbackTone.reset();
    reverb.reset();
    feedback.setCurrentAndTarget(feedback.getTarget());
    if (pendingTap >= 0.0f)
        tapA = tapB = pendingTap;
    else
        tapA = tapB;
    pendingTap = -1.0f;
    tapFade.setCurrentAndTarget(1.0f);
}

float TimeEffectEngine::readTap(int channel) const noexcept
{
    // At the end of a crossfade the weight is exactly 1 for one sample before tapA takes tapB's
    // value: that sample must already be the new position, or it repeats the old one (a spike).
    const float fade = tapFade.getCurrent();
    if (fade >= 1.0f)
        return delayLine.read(channel, tapB);
    return (1.0f - fade) * delayLine.read(channel, tapA) + fade * delayLine.read(channel, tapB);
}

void TimeEffectEngine::processFrame(const float* input, float* wet, int channels) noexcept
{
    const bool usesDelay = mapping::hasDelayControls(mode);
    const bool usesReverb = mode != TimeEffectMode::Delay;

    float delayed[DelayLine::maximumChannels]{};
    if (usesDelay)
    {
        (void)tapFade.getNextValue();
        const float feedbackAmount = feedback.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
        {
            delayed[channel] = readTap(channel);
            // The tone filter is in the repeat path only: the first echo is unfiltered.
            const float repeat = feedbackTone.processSample(channel, delayed[channel]);
            delayLine.write(channel, input[channel] + feedbackAmount * repeat);
        }

        if (!tapFade.isSmoothing() && tapB != tapA)
        {
            tapA = tapB;
            if (pendingTap >= 0.0f && pendingTap != tapA)
            {
                tapB = pendingTap;
                tapFade.setCurrentAndTarget(0.0f);
                tapFade.setTarget(1.0f);
            }
            pendingTap = -1.0f;
        }
    }

    float reverbLeft = 0.0f;
    float reverbRight = 0.0f;
    if (usesReverb)
    {
        // The original is mono up to its DSP (EV-005): the reverb takes the channels' average.
        float sum = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            sum += input[channel] + (usesDelay ? delayed[channel] : 0.0f);
        reverb.processSample(sum / static_cast<float>(channels), reverbLeft, reverbRight);
    }

    for (int channel = 0; channel < channels; ++channel)
    {
        const float reverbOut = channel == 0 ? reverbLeft : reverbRight;
        if (mode == TimeEffectMode::Delay)
            wet[channel] = delayed[channel];
        else if (mode == TimeEffectMode::Echoverb)
            wet[channel] =
                reverbProfile.echoverbDelayGain * delayed[channel] + reverbProfile.echoverbReverbGain * reverbOut;
        else
            wet[channel] = reverbOut;
    }
}

// --- TimeEffects ---------------------------------------------------------------------------------

void TimeEffects::prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile)
{
    numChannels = std::clamp(newNumChannels, 1, DelayLine::maximumChannels);
    for (auto& engine : engines)
        engine.prepare(newSampleRate, numChannels, modelProfile);
    engineFade.prepare(newSampleRate, modelProfile.switching.tailCrossfadeSeconds);
    wetGain.prepare(newSampleRate, modelProfile.switching.parameterRampSeconds);

    current = {};
    activeMode = mapping::toTimeEffectMode(current.mode);
    engines[0].configure(activeMode, current.time, current.fine, current.feedback, true);
    wetGain.setTarget(mapping::mixWetGain(current.mix));
    reset();
}

void TimeEffects::reset() noexcept
{
    for (auto& engine : engines)
        engine.reset();
    fadingEngine = -1;
    modeChangePending = false;
    engineFade.setCurrentAndTarget(1.0f);
    wetGain.setCurrentAndTarget(wetGain.getTarget());
}

void TimeEffects::setSettings(const Settings& settings) noexcept
{
    current = settings;
    wetGain.setTarget(mapping::mixWetGain(settings.mix));
    const auto requested = mapping::toTimeEffectMode(settings.mode);

    if (requested == activeMode)
    {
        modeChangePending = false;
        engines[static_cast<std::size_t>(activeEngine)].configure(requested, settings.time, settings.fine,
                                                                  settings.feedback, false);
        return;
    }

    if (fadingEngine >= 0)
    {
        modeChangePending = true; // the engine needed is still fading out
        return;
    }

    startModeChange(settings);
}

void TimeEffects::startModeChange(const Settings& settings) noexcept
{
    // The new mode starts on the other engine, cleared, while the old one's tail fades out.
    const int next = 1 - activeEngine;
    auto& engine = engines[static_cast<std::size_t>(next)];
    activeMode = mapping::toTimeEffectMode(settings.mode);
    engine.configure(activeMode, settings.time, settings.fine, settings.feedback, true);
    engine.reset();

    fadingEngine = activeEngine;
    activeEngine = next;
    modeChangePending = false;
    engineFade.setCurrentAndTarget(0.0f);
    engineFade.setTarget(1.0f);
}

void TimeEffects::process(AudioBufferView buffer) noexcept
{
    const int channels = std::min(buffer.getNumChannels(), numChannels);

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        auto& active = engines[static_cast<std::size_t>(activeEngine)];
        float input[DelayLine::maximumChannels]{};
        for (int channel = 0; channel < channels; ++channel)
            input[channel] = buffer.getChannel(channel)[n];

        // During a mode change the incoming engine's input fades in too: a cleared delay or reverb
        // that is suddenly fed mid-signal sees a step, and plays it back after the fade is over.
        const float weight = engineFade.getNextValue();
        float activeInput[DelayLine::maximumChannels]{};
        for (int channel = 0; channel < channels; ++channel)
            activeInput[channel] = fadingEngine >= 0 ? weight * input[channel] : input[channel];

        float wet[DelayLine::maximumChannels]{};
        active.processFrame(activeInput, wet, channels);

        if (fadingEngine >= 0)
        {
            float fadingWet[DelayLine::maximumChannels]{};
            engines[static_cast<std::size_t>(fadingEngine)].processFrame(input, fadingWet, channels);
            for (int channel = 0; channel < channels; ++channel)
                wet[channel] = weight * wet[channel] + (1.0f - weight) * fadingWet[channel];
            if (!engineFade.isSmoothing())
            {
                fadingEngine = -1;
                if (modeChangePending)
                    startModeChange(current);
            }
        }

        const float mix = wetGain.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
            buffer.getChannel(channel)[n] = input[channel] + mix * wet[channel];
    }
}

} // namespace fivea::dsp
