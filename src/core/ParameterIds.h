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
