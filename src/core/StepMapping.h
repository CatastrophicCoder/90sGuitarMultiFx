#pragma once

#include "core/ModelProfile.h"

// The middle layer of plan §7.3: DocumentedParameter (a step on the original unit's control) →
// AlgorithmParameter (dB, Hz, ms, gain). Every function clamps its step to the documented range,
// so a corrupt value can never produce an out-of-range or non-finite result.

namespace fivea::mapping
{

[[nodiscard]] int clampStep(int step, documented::StepRange range) noexcept;

[[nodiscard]] DriveMode toDriveMode(int step) noexcept;
[[nodiscard]] ModulationMode toModulationMode(int step) noexcept;
[[nodiscard]] TimeEffectMode toTimeEffectMode(int step) noexcept;

// Compressor
[[nodiscard]] float compressorThresholdDb(int sens, const CompressorProfile& profile) noexcept;
[[nodiscard]] float compressorAttackMs(int attack, const CompressorProfile& profile) noexcept;

// LEVEL and MASTER: linear gain, 0 at step 0.
[[nodiscard]] float levelGain(int step, const LevelProfile& profile) noexcept;

// Distortion/Overdrive
[[nodiscard]] float drivePreGainDb(int drive, const DriveProfile& profile) noexcept;
[[nodiscard]] float driveToneCutoffHz(int tone, const DriveProfile& profile) noexcept;

// 3 Band EQ
[[nodiscard]] float eqBassFrequencyHz() noexcept;
[[nodiscard]] float eqTrebleFrequencyHz() noexcept;
[[nodiscard]] float eqMidFrequencyHz(int midFrequencyStep) noexcept;
[[nodiscard]] float eqBandGainDb(int step, const EqProfile& profile) noexcept;
[[nodiscard]] float eqTrimGain(int trim, const EqProfile& profile) noexcept;

// Chorus/Flanger
[[nodiscard]] float modulationBaseDelayMs(ModulationMode mode) noexcept;
[[nodiscard]] float modulationSpeedHz(int speed, const ModulationProfile& profile) noexcept;
[[nodiscard]] float modulationDepthMs(int depth, ModulationMode mode, const ModulationProfile& profile) noexcept;

// F.BACK (Chorus/Flanger and Reverb/Delay): 0 at step 0, `maximum` at step 15.
[[nodiscard]] float feedbackAmount(int step, float maximum) noexcept;

// MIX: the direct sound stays at unity; the effect rises from 0 (step 0) to equal balance
// (step 15). End points documented; the curve between them is a placeholder (linear).
[[nodiscard]] float mixDryGain() noexcept;
[[nodiscard]] float mixWetGain(int mix) noexcept;

// Reverb/Delay
[[nodiscard]] bool hasDelayControls(TimeEffectMode mode) noexcept;
[[nodiscard]] int maximumTimeStep(TimeEffectMode mode) noexcept;
[[nodiscard]] float delayTimeMs(TimeEffectMode mode, int time, int fine) noexcept; // 0 in reverb modes
[[nodiscard]] float maximumDelayTimeMs() noexcept;

// Noise reduction (Utility): NR LEVEL 0 is off.
[[nodiscard]] bool noiseReductionEnabled(int step) noexcept;
[[nodiscard]] float noiseReductionThresholdDb(int step, const NoiseReductionProfile& profile) noexcept;

} // namespace fivea::mapping
