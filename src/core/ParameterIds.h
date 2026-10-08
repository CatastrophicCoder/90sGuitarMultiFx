#pragma once

// Stable identifiers for every host-visible parameter. These strings are saved in plugin state
// and used by hosts for automation, so once released they must never change; rename the display
// name instead. docs/parameter-specification.md lists each one with its range and evidence status.

namespace fivea::ParameterIds
{

inline constexpr const char* inputTrim = "inputTrim";
inline constexpr const char* outputLevel = "outputLevel";
inline constexpr const char* globalBypass = "globalBypass";

inline constexpr const char* compressorEnabled = "compressorEnabled";
inline constexpr const char* driveEnabled = "driveEnabled";
inline constexpr const char* equaliserEnabled = "equaliserEnabled";
inline constexpr const char* modulationEnabled = "modulationEnabled";
inline constexpr const char* timeEffectEnabled = "timeEffectEnabled";

// Indexed by fivea::EffectBlock.
inline constexpr const char* effectEnabled[] = {compressorEnabled, driveEnabled, equaliserEnabled, modulationEnabled,
                                                timeEffectEnabled};

// The original unit's documented controls (SRC-001 pp. 7, 10–12), one parameter each, holding the
// control's own steps. Ranges and choices are in ParameterLayout.cpp and parameter-specification.md.
inline constexpr const char* compressorSens = "compSens";
inline constexpr const char* compressorAttack = "compAttack";
inline constexpr const char* compressorLevel = "compLevel";

inline constexpr const char* driveMode = "driveMode";
inline constexpr const char* driveDrive = "driveDrive";
inline constexpr const char* driveTone = "driveTone";
inline constexpr const char* driveLevel = "driveLevel";

inline constexpr const char* equaliserBass = "eqBass";
inline constexpr const char* equaliserMidFrequency = "eqMidFreq";
inline constexpr const char* equaliserMid = "eqMid";
inline constexpr const char* equaliserTreble = "eqTreble";
inline constexpr const char* equaliserTrim = "eqTrim";

inline constexpr const char* modulationMode = "modMode";
inline constexpr const char* modulationSpeed = "modSpeed";
inline constexpr const char* modulationDepth = "modDepth";
inline constexpr const char* modulationFeedback = "modFeedback";
inline constexpr const char* modulationMix = "modMix";

inline constexpr const char* timeEffectMode = "revMode";
inline constexpr const char* timeEffectTime = "revTime";
inline constexpr const char* timeEffectFine = "revFine";
inline constexpr const char* timeEffectFeedback = "revFeedback";
inline constexpr const char* timeEffectMix = "revMix";

inline constexpr const char* noiseReductionLevel = "nrLevel";
inline constexpr const char* master = "master";

// Plugin setting, not a control of the original: drive oversampling Off / 2× / 4×. Not
// automatable, since changing it changes the latency.
inline constexpr const char* driveOversampling = "driveOversampling";

// Plugin setting: the engine at the host rate, or at the original's 44.1 kHz behind rate converters
// (plan §6.2, EV-126). Not automatable, for the same reason.
inline constexpr const char* processingRate = "processingRate";

// Passed to juce::ParameterID. Audio Units order parameters by it; a parameter added in a later
// release takes that release's number so existing Logic sessions keep their mapping.
inline constexpr int firstVersionHint = 1;

} // namespace fivea::ParameterIds

namespace fivea::ParameterRanges
{

// A plugin design choice, not a claim about the original hardware (plan §7.2).
inline constexpr float minimumGainDb = -24.0f;
inline constexpr float maximumGainDb = 24.0f;
inline constexpr float gainStepsPerDb = 10.0f;

} // namespace fivea::ParameterRanges
