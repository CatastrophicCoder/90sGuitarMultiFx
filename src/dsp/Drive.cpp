#include "dsp/Drive.h"

#include "core/StepMapping.h"
#include "dsp/Waveshapers.h"

#include <algorithm>
#include <cmath>

namespace fivea::dsp
{

namespace
{
constexpr double butterworthQ = 0.7071067811865476;

float decibelsToGain(float decibels) noexcept
{
    return std::pow(10.0f, decibels / 20.0f);
}
} // namespace

// --- DriveModel ----------------------------------------------------------------------------------

void DriveModel::prepare(double newSampleRate, int numChannels, const DriveProfile& driveProfile,
                         int oversamplingFactor, double rampSeconds)
{
    profile = driveProfile;
    sampleRate = newSampleRate;
    oversampler.prepare(oversamplingFactor, numChannels);

    inputHighPass.setCoefficients(design::highPass(sampleRate, profile.inputHighPassHz, butterworthQ));
    emphasis.setCoefficients(
        design::peaking(sampleRate, profile.emphasisHz, profile.emphasisQ, profile.emphasisGainDb));
    dcBlocker.setCoefficients(design::highPass(sampleRate, profile.dcBlockerHz, butterworthQ));
    preGainDb.prepare(sampleRate, rampSeconds);
    outputTrimDb.prepare(sampleRate, rampSeconds);
    toneLog2Hertz.prepare(sampleRate, rampSeconds);
    setDrive(0);
    setTone(0);
    reset();
}

void DriveModel::reset() noexcept
{
    preGainDb.setCurrentAndTarget(preGainDb.getTarget());
    outputTrimDb.setCurrentAndTarget(outputTrimDb.getTarget());
    toneLog2Hertz.setCurrentAndTarget(toneLog2Hertz.getTarget());
    currentPreGain = decibelsToGain(preGainDb.getCurrent());
    currentOutputTrim = decibelsToGain(outputTrimDb.getCurrent());

    for (auto* filter : {&inputHighPass, &emphasis, &dcBlocker, &toneLowPass})
        filter->reset();
    oversampler.reset();

    samplesUntilUpdate = 0;
    toneDirty = true;
}

void DriveModel::setDrive(int step) noexcept
{
    preGainDb.setTarget(mapping::drivePreGainDb(step, profile));
    outputTrimDb.setTarget(mapping::driveOutputTrimDb(step, profile));
    if (!preGainDb.isSmoothing() && !outputTrimDb.isSmoothing()) // no ramp (or no change): current now
    {
        currentPreGain = decibelsToGain(preGainDb.getCurrent());
        currentOutputTrim = decibelsToGain(outputTrimDb.getCurrent());
    }
}

void DriveModel::setTone(int step) noexcept
{
    toneLog2Hertz.setTarget(std::log2(mapping::driveToneCutoffHz(step, profile)));
    toneDirty = true;
}

void DriveModel::beginSample() noexcept
{
    if (preGainDb.isSmoothing() || outputTrimDb.isSmoothing()) // otherwise the gains are already current
    {
        currentPreGain = decibelsToGain(preGainDb.getNextValue());
        currentOutputTrim = decibelsToGain(outputTrimDb.getNextValue());
    }

    // The tone filter is redesigned on a fixed grid while its cutoff ramps, like the EQ, so the
    // output does not depend on block size.
    if (samplesUntilUpdate == 0)
    {
        if (toneDirty)
            toneLowPass.setCoefficients(
                design::lowPass(sampleRate, std::exp2(toneLog2Hertz.getCurrent()), butterworthQ));
        toneDirty = toneLog2Hertz.isSmoothing();
        (void)toneLog2Hertz.skip(coefficientInterval);
        samplesUntilUpdate = coefficientInterval;
    }
    --samplesUntilUpdate;
}

float DriveModel::processSample(int channel, float input) noexcept
{
    float sample = inputHighPass.processSample(channel, input);
    sample = emphasis.processSample(channel, sample) * currentPreGain;

    const WaveshaperType curve = profile.waveshaper;
    sample = oversampler.processSample(channel, sample,
                                       [curve](float x)
                                       {
                                           return waveshapers::shape(curve, x);
                                       });

    sample = dcBlocker.processSample(channel, sample);
    sample = toneLowPass.processSample(channel, sample);
    return sample * currentOutputTrim;
}

// --- Drive ---------------------------------------------------------------------------------------

int Drive::modelIndex(int modeStep) noexcept
{
    return mapping::toDriveMode(modeStep) == DriveMode::Distortion ? 0 : 1;
}

void Drive::prepare(double newSampleRate, int newNumChannels, const FiveAModelProfile& modelProfile,
                    int oversamplingFactor)
{
    numChannels = std::clamp(newNumChannels, 1, Oversampler::maximumChannels);
    levelProfile = modelProfile.level;
    const double rampSeconds = modelProfile.switching.parameterRampSeconds;

    models[0].prepare(newSampleRate, numChannels, modelProfile.distortion, oversamplingFactor, rampSeconds);
    models[1].prepare(newSampleRate, numChannels, modelProfile.overdrive, oversamplingFactor, rampSeconds);
    modeFade.prepare(newSampleRate, rampSeconds);
    outputGain.prepare(newSampleRate, rampSeconds);

    setSettings({});
    reset();
}

void Drive::reset() noexcept
{
    for (auto& model : models)
        model.reset();
    fadingModel = -1;
    modeFade.setCurrentAndTarget(1.0f);
    outputGain.setCurrentAndTarget(outputGain.getTarget());
}

void Drive::setSettings(const Settings& settings) noexcept
{
    for (auto& model : models)
    {
        model.setDrive(settings.drive);
        model.setTone(settings.tone);
    }
    outputGain.setTarget(mapping::levelGain(settings.level, levelProfile));

    const int requested = modelIndex(settings.mode);
    if (requested == activeModel)
        return;

    if (requested == fadingModel)
    {
        // Switched back during a fade: reverse it from where it is, without a jump.
        fadingModel = activeModel;
        activeModel = requested;
        modeFade.setCurrentAndTarget(1.0f - modeFade.getCurrent());
        modeFade.setTarget(1.0f);
        return;
    }

    // The incoming model starts from clean state; it is faded in from silence, so that is inaudible.
    models[static_cast<std::size_t>(requested)].reset();
    fadingModel = activeModel;
    activeModel = requested;
    modeFade.setCurrentAndTarget(0.0f);
    modeFade.setTarget(1.0f);
}

void Drive::process(AudioBufferView buffer) noexcept
{
    const int channels = std::min(buffer.getNumChannels(), numChannels);
    auto& active = models[static_cast<std::size_t>(activeModel)];

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const float gain = outputGain.getNextValue();
        const bool fading = fadingModel >= 0;
        const float weight = modeFade.getNextValue();

        active.beginSample();
        if (fading)
            models[static_cast<std::size_t>(fadingModel)].beginSample();

        for (int channel = 0; channel < channels; ++channel)
        {
            float* sample = buffer.getChannel(channel) + n;
            float output = active.processSample(channel, *sample);
            if (fading)
                output =
                    weight * output +
                    (1.0f - weight) * models[static_cast<std::size_t>(fadingModel)].processSample(channel, *sample);
            *sample = output * gain;
        }

        if (fading && !modeFade.isSmoothing())
            fadingModel = -1;
    }
}

} // namespace fivea::dsp
