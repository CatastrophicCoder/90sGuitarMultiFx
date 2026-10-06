#pragma once

#include "core/ParameterIds.h"

#include <array>

// What the panel shows and where, in one place: the parameter grid as printed on the original
// unit (SRC-001 p. 2, SRC-003; docs/panel-specification.md) and the panel's design coordinates.
//
// Design space: the original's front panel in units measured from the reference photograph,
// 1905 × 883 across the full face (upper panel and footswitch deck). The editor scales this space
// to its window as a whole, so every position below is a measurement, not a guess per window size.

namespace fivea::ui::panel
{

inline constexpr float designWidth = 1905.0f;
inline constexpr float designHeight = 883.0f;

// One cell of the grid: the printed label and range, and the parameter knob A–E controls there.
struct Cell
{
    const char* label = nullptr; // nullptr: an empty cell (the knob does nothing in this row)
    const char* range = nullptr;
    const char* parameterId = nullptr;
};

struct Row
{
    const char* name;
    std::array<Cell, 5> cells; // knobs A–E
};

// SRC-001 p. 2 (the panel drawing) and pp. 10–12. Text is UTF-8 ("\xc3\x97" is ×), spelled as bytes so
// every compiler reads it the same.
inline const std::array<Row, 6> rows{{
    {"COMP",
     {{{"SENS", "0~15", ParameterIds::compressorSens},
       {"ATTACK", "0~7", ParameterIds::compressorAttack},
       {},
       {},
       {"LEVEL", "0~15", ParameterIds::compressorLevel}}}},
    {"DIST/OD",
     {{{"MODE", "1~2", ParameterIds::driveMode},
       {"DRIVE", "0~15", ParameterIds::driveDrive},
       {"TONE", "0~15", ParameterIds::driveTone},
       {},
       {"LEVEL", "0~15", ParameterIds::driveLevel}}}},
    {"3 BAND EQ",
     {{{"BASS", "-7~7", ParameterIds::equaliserBass},
       {"MID FREQ", "1~8", ParameterIds::equaliserMidFrequency},
       {"MID", "-7~7", ParameterIds::equaliserMid},
       {"TREBLE", "-7~7", ParameterIds::equaliserTreble},
       {"TRIM", "0~15", ParameterIds::equaliserTrim}}}},
    {"CHORUS/FL",
     {{{"MODE", "1~5", ParameterIds::modulationMode},
       {"SPEED", "0~15", ParameterIds::modulationSpeed},
       {"DEPTH", "0~15", ParameterIds::modulationDepth},
       {"F. BACK", "0~15", ParameterIds::modulationFeedback},
       {"MIX", "0~15", ParameterIds::modulationMix}}}},
    {"REV/DELAY",
     {{{"MODE", "1~7", ParameterIds::timeEffectMode},
       {"TIME",
        "\xc3\x97"
        "100ms",
        ParameterIds::timeEffectTime},
       {"FINE",
        "\xc3\x97"
        "10ms",
        ParameterIds::timeEffectFine},
       {"F. BACK", "0~15", ParameterIds::timeEffectFeedback},
       {"MIX", "0~15", ParameterIds::timeEffectMix}}}},
    {"UTILITY",
     {{{}, {}, {}, {"NR LEVEL", "0~15", ParameterIds::noiseReductionLevel}, {"MASTER", "0~15", ParameterIds::master}}}},
}};

// The effect names on the red stripe, above footswitches 1–5.
inline constexpr std::array<const char*, 5> stripeNames{"COMPRESSOR", "DISTORTION/OVERDRIVE", "3 BAND EQ",
                                                        "CHORUS/FLANGER", "REVERB/DELAY"};

// --- Design coordinates (from SRC-003) -----------------------------------------------------------

struct Point
{
    float x;
    float y;
};

inline constexpr float upperPanelTop = 83.0f;     // the recessed face starts below the housing's lip
inline constexpr float upperPanelBottom = 393.0f; // the red stripe's bottom edge
inline constexpr float stripeTop = 358.0f;
inline constexpr float deckTop = 423.0f;

inline constexpr Point wordmark{55.0f, 110.0f};
inline constexpr Point peakLed{157.0f, 191.0f};
inline constexpr Point inputKnob{155.0f, 288.0f};
inline constexpr float smallKnobRadius = 27.0f;

inline constexpr float slideLeft = 248.0f;
inline constexpr float slideRight = 375.0f;
inline constexpr float slideTop = 141.0f;
inline constexpr float slideBottom = 286.0f;

inline constexpr float gridLeft = 385.0f;
inline constexpr float gridRight = 1190.0f;
inline constexpr float gridTop = 145.0f;
inline constexpr float gridRowHeight = 23.0f;
inline constexpr float rowNameLeft = 405.0f;
inline constexpr float rowNameRight = 498.0f;
inline constexpr std::array<float, 6> columnEdges{503.0f, 615.0f, 725.0f, 838.0f, 948.0f, 1060.0f};

inline constexpr std::array<float, 5> parameterKnobX{535.0f, 640.0f, 755.0f, 870.0f, 980.0f};
inline constexpr float parameterKnobY = 325.0f;
inline constexpr float parameterKnobRadius = 22.0f;

inline constexpr float displayLeft = 1200.0f;
inline constexpr float displayRight = 1620.0f;
inline constexpr float displayTop = 150.0f;
inline constexpr float displayBottom = 283.0f;
inline constexpr float digitsLeft = 1445.0f;
inline constexpr float digitsRight = 1552.0f;
inline constexpr float digitsTop = 178.0f;
inline constexpr float digitsBottom = 228.0f;

inline constexpr float modeListTop = 293.0f;
inline constexpr Point outputKnob{1745.0f, 195.0f};
inline constexpr Point writeKey{1705.0f, 290.0f};
inline constexpr Point bypassKey{1782.0f, 290.0f};
inline constexpr float keyRadius = 22.0f;

inline constexpr std::array<float, 6> footswitchX{162.0f, 478.0f, 798.0f, 1120.0f, 1438.0f, 1745.0f};
inline constexpr float footswitchWidth = 95.0f;
inline constexpr float footswitchTop = 503.0f;
inline constexpr float footswitchBottom = 793.0f;
inline constexpr float footswitchLedY = 468.0f;

} // namespace fivea::ui::panel
