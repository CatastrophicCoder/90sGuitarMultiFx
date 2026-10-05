#pragma once

#include "dsp/Waveshapers.h"

#include <array>

// Every hardware-specific value in one place (plan §14). Two kinds, kept apart on purpose:
//
//   documented::  values stated in the original unit's owner's manual (SRC-001). CONFIRMED; change
//                 only if the manual was misread.
//   *Profile      how a documented control step maps to an algorithm value (dB, Hz, ms). The
//                 manual does not say, so these are PLACEHOLDER design choices until measured
//                 (evidence register EV-011). A measured profile replaces them without code changes.

namespace fivea
{

enum class DriveMode
{
    Distortion = 1,
    Overdrive = 2
};

enum class ModulationMode
{
    Flanger1 = 1,
    Flanger2,
    Chorus1,
    Chorus2,
    Slapback
};

enum class TimeEffectMode
{
    HallReverb = 1,
    EnsembleHallReverb,
    RoomReverb,
    PlateReverb,
    LiveStageReverb,
    Echoverb,
    Delay
};

namespace documented
{

struct StepRange
{
    int minimum;
    int maximum;
};

// Control ranges (SRC-001 pp. 7, 10–12).
inline constexpr StepRange compressorSens{0, 15};
inline constexpr StepRange compressorAttack{0, 7};
inline constexpr StepRange driveMode{1, 2};
inline constexpr StepRange eqBandGain{-7, 7};
inline constexpr StepRange eqMidFrequency{1, 8};
inline constexpr StepRange modulationMode{1, 5};
inline constexpr StepRange timeEffectMode{1, 7};
inline constexpr StepRange delayFine{0, 9};
inline constexpr StepRange zeroToFifteen{0,
                                         15}; // LEVEL, DRIVE, TONE, TRIM, SPEED, DEPTH, F.BACK, MIX, NR LEVEL, MASTER

inline constexpr double internalSampleRate = 44100.0; // SRC-001 p. 14

// 3 Band EQ (SRC-001 p. 11).
inline constexpr float eqBassFrequencyHz = 100.0f;
inline constexpr float eqTrebleFrequencyHz = 3000.0f;
inline constexpr std::array<float, 8> eqMidFrequenciesHz{200.0f,  550.0f,  800.0f,  1000.0f,
                                                         1250.0f, 2000.0f, 4000.0f, 5000.0f};

// Chorus/Flanger delay times per mode, Flanger 1 … Slapback (SRC-001 p. 11).
inline constexpr std::array<float, 5> modulationBaseDelaysMs{1.8f, 4.0f, 24.0f, 32.0f, 75.0f};

// Reverb/Delay: delay time = TIME × 100 ms + FINE × 10 ms; TIME 0–3 in Echoverb, 0–4 in Delay;
// no delay controls in the reverb modes (SRC-001 p. 12).
inline constexpr float delayTimeStepMs = 100.0f;
inline constexpr float delayFineStepMs = 10.0f;
inline constexpr int echoverbMaximumTimeStep = 3;
inline constexpr int delayMaximumTimeStep = 4;

// MIX: 15 = effect and direct sound 50/50, 0 = direct only (SRC-001 pp. 11–12).
inline constexpr int mixEqualBalanceStep = 15;

} // namespace documented

// --- Placeholder step mappings (all PLACEHOLDER; see the file comment) -------------------------

struct CompressorProfile
{
    float thresholdAtSensZeroDb = -6.0f;
    float thresholdDbPerSensStep = -3.0f;
    float ratio = 8.0f; // "a limiter effect" (SRC-001 p. 10)
    float kneeDb = 6.0f;
    std::array<float, 8> attackMs{50.0f, 30.0f, 20.0f, 12.0f, 8.0f, 5.0f, 3.0f, 1.0f}; // ATTACK 0–7
    float releaseMs = 250.0f;                                                          // no release control on the unit
};

// One per DIST/OD mode. The documented descriptions are "a high gain 'fuzz' type effect"
// (Distortion) and "a mild saturation effect" (Overdrive), SRC-001 p. 10; everything here is a
// placeholder pipeline in their spirit (plan §9.1):
//   input high-pass → pre-emphasis bell → DRIVE gain → waveshaper (oversampled) → DC blocker →
//   TONE low-pass → output trim.
struct DriveProfile
{
    float preGainAtZeroDb;
    float preGainDbPerStep;
    float toneMinimumHz;
    float toneMaximumHz;
    dsp::WaveshaperType waveshaper;
    float inputHighPassHz;
    float emphasisHz;
    float emphasisGainDb;
    float emphasisQ;
    float outputTrimDb;
    float dcBlockerHz = 10.0f; // removes the offset an asymmetric curve creates
};

struct EqProfile
{
    float dbPerStep = 1.5f;
    float midQ = 0.7f;
    float shelfSlope = 1.0f;    // RBJ shelf slope S; 1 = steepest without overshoot
    float trimDbPerStep = 1.5f; // TRIM 15 = unity; lower steps attenuate ("lower the TRIM" to avoid clipping)
};

// LEVEL (compressor, drive) and MASTER: step 0 silent, otherwise dB relative to a unity step.
struct LevelProfile
{
    int unityStep = 12;
    float dbPerStep = 2.0f;
};

struct ModulationProfile
{
    float speedMinimumHz = 0.1f;
    float speedMaximumHz = 10.0f;
    std::array<float, 5> maximumDepthMs{1.6f, 3.6f, 6.0f, 6.0f, 1.0f}; // per mode, Flanger 1 … Slapback
    float feedbackMaximum = 0.9f;
    float stereoPhaseDegrees = 90.0f; // right channel's LFO leads the left (EV-005: where stereo begins is unknown)
};

struct DelayProfile
{
    float feedbackMaximum = 0.85f;
    float feedbackToneHz = 5000.0f; // low-pass in the repeat path: each repeat a little darker
};

// One fixed reverb type. The original unit's reverb modes have no decay, size or tone controls
// (SRC-001 p. 12), so each is a fixed voicing; all values are placeholders (EV-115).
struct ReverbVoicing
{
    float decaySeconds;       // T60 at low frequencies
    float highFrequencyRatio; // T60 at the top of the band ÷ T60 at low frequencies (< 1: darker tail)
    float size;               // scales the network's delay lengths
    float preDelayMs;
    float diffusion; // input allpass gain
};

struct ReverbProfile
{
    // Hall, Ensemble Hall, Room, Plate, Live Stage (SRC-001 p. 12, modes 1–5).
    std::array<ReverbVoicing, 5> voicings{{
        {.decaySeconds = 2.4f, .highFrequencyRatio = 0.5f, .size = 1.0f, .preDelayMs = 20.0f, .diffusion = 0.6f},
        {.decaySeconds = 3.0f, .highFrequencyRatio = 0.45f, .size = 1.2f, .preDelayMs = 30.0f, .diffusion = 0.65f},
        {.decaySeconds = 0.8f, .highFrequencyRatio = 0.6f, .size = 0.4f, .preDelayMs = 5.0f, .diffusion = 0.6f},
        {.decaySeconds = 1.8f, .highFrequencyRatio = 0.75f, .size = 0.6f, .preDelayMs = 0.0f, .diffusion = 0.7f},
        {.decaySeconds = 1.4f, .highFrequencyRatio = 0.55f, .size = 0.8f, .preDelayMs = 12.0f, .diffusion = 0.6f},
    }};
    float echoverbDelayGain = 0.7f;  // Echoverb: echoes …
    float echoverbReverbGain = 0.7f; // … plus a reverb of the dry signal and the echoes
};

struct NoiseReductionProfile
{
    float thresholdAtStepOneDb = -90.0f;
    float thresholdDbPerStep = 3.0f;
};

// Plugin design choice (plan §15): how fast an effect fades in or out when switched.
struct SwitchingProfile
{
    double effectCrossfadeSeconds = 0.01;
    double parameterRampSeconds = 0.02; // stepped controls ramp, so a step does not click
    double tailCrossfadeSeconds = 0.1;  // Reverb/Delay mode change: old tail fades as the new mode starts (plan §15)
};

struct FiveAModelProfile
{
    double internalSampleRate = documented::internalSampleRate;
    CompressorProfile compressor;
    DriveProfile overdrive{.preGainAtZeroDb = 0.0f,
                           .preGainDbPerStep = 2.0f,
                           .toneMinimumHz = 1000.0f,
                           .toneMaximumHz = 8000.0f,
                           .waveshaper = dsp::WaveshaperType::CubicSoft,
                           .inputHighPassHz = 150.0f,
                           .emphasisHz = 800.0f,
                           .emphasisGainDb = 6.0f,
                           .emphasisQ = 0.7f,
                           .outputTrimDb = 0.0f};
    DriveProfile distortion{.preGainAtZeroDb = 10.0f,
                            .preGainDbPerStep = 3.0f,
                            .toneMinimumHz = 1000.0f,
                            .toneMaximumHz = 8000.0f,
                            .waveshaper = dsp::WaveshaperType::AsymmetricPiecewise,
                            .inputHighPassHz = 100.0f,
                            .emphasisHz = 1200.0f,
                            .emphasisGainDb = 9.0f,
                            .emphasisQ = 0.7f,
                            .outputTrimDb = -3.0f};
    EqProfile eq;
    LevelProfile level;
    ModulationProfile modulation;
    DelayProfile delay;
    ReverbProfile reverb;
    NoiseReductionProfile noiseReduction;
    SwitchingProfile switching;
};

// The only profile until a physical unit is measured (plan §14: later MeasuredUnitProfile).
inline constexpr FiveAModelProfile functionalPlaceholderProfile{};

} // namespace fivea
