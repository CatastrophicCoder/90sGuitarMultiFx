#pragma once

#include "core/ParameterSnapshot.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace fivea
{

// Program memory as the owner's manual documents it (docs/evidence-register.md, EV-009): 6 banks of
// 5 programs, bank 1 the user area, banks 2–6 presets (SRC-001 p. 4).
inline constexpr int numBanks = 6;
inline constexpr int programsPerBank = 5;
inline constexpr int numProgramSlots = numBanks * programsPerBank;
inline constexpr int userBank = 1;

// A program's name, stored inline so a Program copies without allocating. The original shows no
// names (a 2-digit display); they exist for the host's program list.
class ProgramName
{
public:
    static constexpr std::size_t maxLength = 31;

    constexpr ProgramName() = default;

    // Keeps printable ASCII only, and at most maxLength characters.
    static constexpr ProgramName from(std::string_view text) noexcept
    {
        ProgramName name;
        std::size_t length = 0;
        for (const char character : text)
        {
            if (length == maxLength)
                break;
            if (character >= ' ' && character <= '~')
                name.characters[length++] = character;
        }
        return name;
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept { return {characters.data()}; }

    bool operator==(const ProgramName&) const = default;

private:
    std::array<char, maxLength + 1> characters{};
};

// What one program stores: the five effects' on/off states and controls, and the Utility page's
// NR LEVEL and MASTER (SRC-001 pp. 5, 7, 10–12). Input trim, output level, global bypass and
// oversampling are not program data: on the original they are panel knobs and keys.
struct Program
{
    ProgramName name;
    std::array<bool, numEffectBlocks> effectEnabled{};
    dsp::Compressor::Settings compressor;
    dsp::Drive::Settings drive;
    dsp::ThreeBandEq::Settings equaliser;
    dsp::Modulation::Settings modulation;
    dsp::TimeEffects::Settings timeEffects;
    int noiseReductionLevel = 0;
    int master = 12;

    bool operator==(const Program&) const = default;
};

// The program's part of a snapshot.
[[nodiscard]] Program programFrom(const ParameterSnapshot& snapshot, ProgramName name = {}) noexcept;

// Sets the program's part of a snapshot; the rest (trim, output level, bypass) keeps its values.
void applyProgram(const Program& program, ParameterSnapshot& snapshot) noexcept;

// Every stepped control a program stores, for the display's dot in Edit mode: it shows when the
// value being turned equals the stored program's (SRC-001 p. 9).
enum class ProgramControl
{
    CompressorSens,
    CompressorAttack,
    CompressorLevel,
    DriveMode,
    DriveDrive,
    DriveTone,
    DriveLevel,
    EqualiserBass,
    EqualiserMidFrequency,
    EqualiserMid,
    EqualiserTreble,
    EqualiserTrim,
    ModulationMode,
    ModulationSpeed,
    ModulationDepth,
    ModulationFeedback,
    ModulationMix,
    TimeEffectMode,
    TimeEffectTime,
    TimeEffectFine,
    TimeEffectFeedback,
    TimeEffectMix,
    NoiseReductionLevel,
    Master
};

inline constexpr int numProgramControls = static_cast<int>(ProgramControl::Master) + 1;

// The host parameter that holds the control (core/ParameterIds.h); saved state names program
// values by it.
[[nodiscard]] const char* parameterIdFor(ProgramControl control) noexcept;

// The control's documented range (SRC-001 pp. 7, 10–12).
[[nodiscard]] documented::StepRange controlRange(ProgramControl control) noexcept;

[[nodiscard]] int controlValue(const Program& program, ProgramControl control) noexcept;
int& controlValue(Program& program, ProgramControl control) noexcept;

// The dot's three comparisons (SRC-001 p. 9): Edit mode, one control; Manual mode, the effects'
// on/off states; and the whole program.
[[nodiscard]] bool sameControl(const Program& a, const Program& b, ProgramControl control) noexcept;
[[nodiscard]] bool sameEffectSwitches(const Program& a, const Program& b) noexcept;
[[nodiscard]] bool sameSettings(const Program& a, const Program& b) noexcept; // ignores the name

} // namespace fivea
