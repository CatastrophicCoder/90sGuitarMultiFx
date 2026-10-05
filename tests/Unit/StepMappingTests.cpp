#include "core/ModelProfile.h"
#include "core/StepMapping.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace fivea;
using namespace fivea::mapping;

namespace
{
const auto& profile = functionalPlaceholderProfile;
} // namespace

// --- Documented values (SRC-001): must come out exactly ---------------------------------------

TEST_CASE("EQ frequencies are the documented ones")
{
    CHECK(eqBassFrequencyHz() == 100.0f);
    CHECK(eqTrebleFrequencyHz() == 3000.0f);

    const float expectedMid[] = {200.0f, 550.0f, 800.0f, 1000.0f, 1250.0f, 2000.0f, 4000.0f, 5000.0f};
    for (int step = 1; step <= 8; ++step)
        CHECK(eqMidFrequencyHz(step) == expectedMid[step - 1]);

    CHECK(eqMidFrequencyHz(0) == 200.0f);
    CHECK(eqMidFrequencyHz(99) == 5000.0f);
}

TEST_CASE("Chorus/flanger base delays are the documented ones")
{
    CHECK(modulationBaseDelayMs(ModulationMode::Flanger1) == 1.8f);
    CHECK(modulationBaseDelayMs(ModulationMode::Flanger2) == 4.0f);
    CHECK(modulationBaseDelayMs(ModulationMode::Chorus1) == 24.0f);
    CHECK(modulationBaseDelayMs(ModulationMode::Chorus2) == 32.0f);
    CHECK(modulationBaseDelayMs(ModulationMode::Slapback) == 75.0f);
}

TEST_CASE("Delay time is TIME x 100 ms + FINE x 10 ms, with the documented maxima")
{
    CHECK(delayTimeMs(TimeEffectMode::Delay, 3, 5) == 350.0f);
    CHECK(delayTimeMs(TimeEffectMode::Delay, 4, 9) == 490.0f);
    CHECK(delayTimeMs(TimeEffectMode::Echoverb, 3, 9) == 390.0f);

    // TIME beyond a mode's documented range is clamped to it.
    CHECK(delayTimeMs(TimeEffectMode::Echoverb, 4, 9) == 390.0f);
    CHECK(delayTimeMs(TimeEffectMode::Delay, 9, 99) == 490.0f);

    CHECK(maximumDelayTimeMs() == 490.0f);
}

TEST_CASE("Reverb modes have no delay controls")
{
    for (int mode = 1; mode <= 5; ++mode)
    {
        const auto timeEffect = toTimeEffectMode(mode);
        CHECK_FALSE(hasDelayControls(timeEffect));
        CHECK(delayTimeMs(timeEffect, 4, 9) == 0.0f);
    }

    CHECK(hasDelayControls(TimeEffectMode::Echoverb));
    CHECK(hasDelayControls(TimeEffectMode::Delay));
    CHECK(maximumTimeStep(TimeEffectMode::Echoverb) == 3);
    CHECK(maximumTimeStep(TimeEffectMode::Delay) == 4);
}

TEST_CASE("MIX 0 is dry only and MIX 15 is effect and direct in equal balance")
{
    CHECK(mixWetGain(0) == 0.0f);
    CHECK(mixWetGain(15) == 1.0f);
    CHECK(mixDryGain() == 1.0f);
}

TEST_CASE("Out-of-range modes are clamped to the documented ones")
{
    CHECK(toDriveMode(0) == DriveMode::Distortion);
    CHECK(toDriveMode(3) == DriveMode::Overdrive);
    CHECK(toModulationMode(-4) == ModulationMode::Flanger1);
    CHECK(toModulationMode(9) == ModulationMode::Slapback);
    CHECK(toTimeEffectMode(0) == TimeEffectMode::HallReverb);
    CHECK(toTimeEffectMode(8) == TimeEffectMode::Delay);
}

// --- Placeholder mappings: shape only (direction, bounds, finiteness) -------------------------

TEST_CASE("Placeholder mappings move in the documented direction")
{
    for (int step = 0; step < 15; ++step)
    {
        INFO("step " << step);

        // SENS: "the larger this value, the greater the sustain" -> lower threshold.
        CHECK(compressorThresholdDb(step + 1, profile.compressor) < compressorThresholdDb(step, profile.compressor));

        // TONE: "the larger this value, the brighter".
        CHECK(driveToneCutoffHz(step + 1, profile.overdrive) > driveToneCutoffHz(step, profile.overdrive));
        CHECK(driveToneCutoffHz(step + 1, profile.distortion) > driveToneCutoffHz(step, profile.distortion));

        // DRIVE: "amount of distortion or gain".
        CHECK(drivePreGainDb(step + 1, profile.overdrive) > drivePreGainDb(step, profile.overdrive));
        CHECK(drivePreGainDb(step + 1, profile.distortion) > drivePreGainDb(step, profile.distortion));

        // SPEED and DEPTH increase; MIX moves from dry towards the effect.
        CHECK(modulationSpeedHz(step + 1, profile.modulation) > modulationSpeedHz(step, profile.modulation));
        CHECK(modulationDepthMs(step + 1, ModulationMode::Chorus1, profile.modulation) >
              modulationDepthMs(step, ModulationMode::Chorus1, profile.modulation));
        CHECK(mixWetGain(step + 1) > mixWetGain(step));

        // LEVEL and MASTER: louder with the step; TRIM likewise.
        CHECK(levelGain(step + 1, profile.level) > levelGain(step, profile.level));
        CHECK(eqTrimGain(step + 1, profile.eq) > eqTrimGain(step, profile.eq));
    }

    // ATTACK: "the higher this value, the faster the attack".
    for (int step = 0; step < 7; ++step)
        CHECK(compressorAttackMs(step + 1, profile.compressor) < compressorAttackMs(step, profile.compressor));
}

TEST_CASE("Placeholder EQ gains are symmetric around the centre detent")
{
    CHECK(eqBandGainDb(0, profile.eq) == 0.0f);
    for (int step = 1; step <= 7; ++step)
    {
        CHECK(eqBandGainDb(step, profile.eq) > 0.0f);
        CHECK(eqBandGainDb(-step, profile.eq) == -eqBandGainDb(step, profile.eq));
    }
    CHECK(eqBandGainDb(12, profile.eq) == eqBandGainDb(7, profile.eq));
}

TEST_CASE("LEVEL 0 is silent, and TRIM at its top is unity")
{
    CHECK(levelGain(0, profile.level) == 0.0f);
    CHECK(eqTrimGain(15, profile.eq) == 1.0f);
}

TEST_CASE("NR LEVEL 0 switches noise reduction off; higher steps raise the threshold")
{
    CHECK_FALSE(noiseReductionEnabled(0));
    for (int step = 1; step < 15; ++step)
    {
        CHECK(noiseReductionEnabled(step));
        CHECK(noiseReductionThresholdDb(step + 1, profile.noiseReduction) >
              noiseReductionThresholdDb(step, profile.noiseReduction));
    }
}

TEST_CASE("Feedback stays below one at every step")
{
    for (int step = 0; step <= 15; ++step)
    {
        CHECK(feedbackAmount(step, profile.modulation.feedbackMaximum) < 1.0f);
        CHECK(feedbackAmount(step, profile.delay.feedbackMaximum) < 1.0f);
    }
    CHECK(feedbackAmount(0, profile.delay.feedbackMaximum) == 0.0f);
}

TEST_CASE("Every placeholder mapping is finite over its whole documented range and beyond")
{
    for (int step = -20; step <= 40; ++step)
    {
        INFO("step " << step);
        CHECK(std::isfinite(compressorThresholdDb(step, profile.compressor)));
        CHECK(std::isfinite(compressorAttackMs(step, profile.compressor)));
        CHECK(std::isfinite(drivePreGainDb(step, profile.distortion)));
        CHECK(std::isfinite(driveToneCutoffHz(step, profile.overdrive)));
        CHECK(std::isfinite(eqBandGainDb(step, profile.eq)));
        CHECK(std::isfinite(eqTrimGain(step, profile.eq)));
        CHECK(std::isfinite(modulationSpeedHz(step, profile.modulation)));
        CHECK(std::isfinite(modulationDepthMs(step, ModulationMode::Flanger1, profile.modulation)));
        CHECK(std::isfinite(levelGain(step, profile.level)));
        CHECK(std::isfinite(mixWetGain(step)));
    }
}
