#include "core/StepMapping.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace fivea::mapping
{

namespace
{
constexpr int maximumStep = documented::zeroToFifteen.maximum;

float stepFraction(int step) noexcept
{
    return static_cast<float>(clampStep(step, documented::zeroToFifteen)) / static_cast<float>(maximumStep);
}

// Evenly spaced on a log axis, which is how frequency and rate controls are usually felt.
float geometric(float fraction, float minimum, float maximum) noexcept
{
    return minimum * std::pow(maximum / minimum, fraction);
}

float decibelsToGain(float decibels) noexcept
{
    return std::pow(10.0f, decibels / 20.0f);
}

std::size_t modeIndex(ModulationMode mode) noexcept
{
    return static_cast<std::size_t>(mode) - 1;
}
} // namespace

int clampStep(int step, documented::StepRange range) noexcept
{
    return std::clamp(step, range.minimum, range.maximum);
}

DriveMode toDriveMode(int step) noexcept
{
    return static_cast<DriveMode>(clampStep(step, documented::driveMode));
}

ModulationMode toModulationMode(int step) noexcept
{
    return static_cast<ModulationMode>(clampStep(step, documented::modulationMode));
}

TimeEffectMode toTimeEffectMode(int step) noexcept
{
    return static_cast<TimeEffectMode>(clampStep(step, documented::timeEffectMode));
}

float compressorThresholdDb(int sens, const CompressorProfile& profile) noexcept
{
    const int step = clampStep(sens, documented::compressorSens);
    return profile.thresholdAtSensZeroDb + profile.thresholdDbPerSensStep * static_cast<float>(step);
}

float compressorAttackMs(int attack, const CompressorProfile& profile) noexcept
{
    return profile.attackMs[static_cast<std::size_t>(clampStep(attack, documented::compressorAttack))];
}

float levelGain(int step, const LevelProfile& profile) noexcept
{
    const int clamped = clampStep(step, documented::zeroToFifteen);
    if (clamped == 0)
        return 0.0f;

    return decibelsToGain(profile.dbPerStep * static_cast<float>(clamped - profile.unityStep));
}

float drivePreGainDb(int drive, const DriveProfile& profile) noexcept
{
    const int step = clampStep(drive, documented::zeroToFifteen);
    return profile.preGainAtZeroDb + profile.preGainDbPerStep * static_cast<float>(step);
}

float driveToneCutoffHz(int tone, const DriveProfile& profile) noexcept
{
    return geometric(stepFraction(tone), profile.toneMinimumHz, profile.toneMaximumHz);
}

float eqBassFrequencyHz() noexcept
{
    return documented::eqBassFrequencyHz;
}

float eqTrebleFrequencyHz() noexcept
{
    return documented::eqTrebleFrequencyHz;
}

float eqMidFrequencyHz(int midFrequencyStep) noexcept
{
    const int step = clampStep(midFrequencyStep, documented::eqMidFrequency);
    return documented::eqMidFrequenciesHz[static_cast<std::size_t>(step - documented::eqMidFrequency.minimum)];
}

float eqBandGainDb(int step, const EqProfile& profile) noexcept
{
    return profile.dbPerStep * static_cast<float>(clampStep(step, documented::eqBandGain));
}

float eqTrimGain(int trim, const EqProfile& profile) noexcept
{
    const int step = clampStep(trim, documented::zeroToFifteen);
    if (step == maximumStep)
        return 1.0f; // exact, so TRIM at its top is transparent

    return decibelsToGain(profile.trimDbPerStep * static_cast<float>(step - maximumStep));
}

float modulationBaseDelayMs(ModulationMode mode) noexcept
{
    return documented::modulationBaseDelaysMs[modeIndex(mode)];
}

float modulationSpeedHz(int speed, const ModulationProfile& profile) noexcept
{
    return geometric(stepFraction(speed), profile.speedMinimumHz, profile.speedMaximumHz);
}

float modulationDepthMs(int depth, ModulationMode mode, const ModulationProfile& profile) noexcept
{
    return stepFraction(depth) * profile.maximumDepthMs[modeIndex(mode)];
}

float feedbackAmount(int step, float maximum) noexcept
{
    return stepFraction(step) * std::min(maximum, 0.99f);
}

float mixDryGain() noexcept
{
    return 1.0f;
}

float mixWetGain(int mix) noexcept
{
    return static_cast<float>(clampStep(mix, documented::zeroToFifteen)) /
           static_cast<float>(documented::mixEqualBalanceStep);
}

bool hasDelayControls(TimeEffectMode mode) noexcept
{
    return mode == TimeEffectMode::Echoverb || mode == TimeEffectMode::Delay;
}

int maximumTimeStep(TimeEffectMode mode) noexcept
{
    switch (mode)
    {
    case TimeEffectMode::Echoverb:
        return documented::echoverbMaximumTimeStep;
    case TimeEffectMode::Delay:
        return documented::delayMaximumTimeStep;
    case TimeEffectMode::HallReverb:
    case TimeEffectMode::EnsembleHallReverb:
    case TimeEffectMode::RoomReverb:
    case TimeEffectMode::PlateReverb:
    case TimeEffectMode::LiveStageReverb:
        break;
    }
    return 0;
}

float delayTimeMs(TimeEffectMode mode, int time, int fine) noexcept
{
    if (!hasDelayControls(mode))
        return 0.0f;

    const int timeStep = std::clamp(time, 0, maximumTimeStep(mode));
    const int fineStep = clampStep(fine, documented::delayFine);
    return documented::delayTimeStepMs * static_cast<float>(timeStep) +
           documented::delayFineStepMs * static_cast<float>(fineStep);
}

float maximumDelayTimeMs() noexcept
{
    return delayTimeMs(TimeEffectMode::Delay, documented::delayMaximumTimeStep, documented::delayFine.maximum);
}

bool noiseReductionEnabled(int step) noexcept
{
    return clampStep(step, documented::zeroToFifteen) > 0;
}

float noiseReductionThresholdDb(int step, const NoiseReductionProfile& profile) noexcept
{
    const int clamped = std::max(1, clampStep(step, documented::zeroToFifteen));
    return profile.thresholdAtStepOneDb + profile.thresholdDbPerStep * static_cast<float>(clamped - 1);
}

} // namespace fivea::mapping
