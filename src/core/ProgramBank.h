#pragma once

#include "core/Program.h"

#include <array>

namespace fivea
{

// A program slot, as the original numbers it: bank 1–6, program 1–5 ("2-1" is bank 2, program 1).
struct ProgramLocation
{
    int bank = userBank;
    int program = 1;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] ProgramLocation clamped() const noexcept;
    [[nodiscard]] int slotIndex() const noexcept; // 0–29, of the clamped location

    bool operator==(const ProgramLocation&) const = default;
};

// The 30 program slots. Bank 1 is the user area; banks 2–6 cannot be written (SRC-001 pp. 4, 8).
class ProgramBank
{
public:
    using Slots = std::array<Program, numProgramSlots>;

    ProgramBank() = default;
    explicit ProgramBank(const Slots& initialSlots) noexcept : slots(initialSlots) {}

    [[nodiscard]] const Program& at(ProgramLocation location) const noexcept;

    // Program Write: stores into the user bank, replacing that slot. Refused (returns false, changes
    // nothing) for any other bank or an invalid location.
    bool write(ProgramLocation location, const Program& program) noexcept;

    [[nodiscard]] const Slots& allSlots() const noexcept { return slots; }

private:
    Slots slots{};
};

// Which program is playing, and which bank the display shows. They differ while a bank is pending:
// moving the slide switch to another bank in Program mode shows that bank but selects nothing; the
// bank is entered when a footswitch picks one of its programs (SRC-001 p. 4, p. 7 note 2).
class ProgramSelection
{
public:
    ProgramSelection() = default;
    explicit ProgramSelection(ProgramLocation location) noexcept { select(location); }

    [[nodiscard]] ProgramLocation selected() const noexcept { return current; }
    [[nodiscard]] int shownBank() const noexcept { return shown; }

    // The display's dot in Program mode: shown only when the bank shown is the selected program's.
    [[nodiscard]] bool isBankPending() const noexcept { return shown != current.bank; }

    void showBank(int bank) noexcept;                    // the slide switch moved to a bank
    ProgramLocation selectProgram(int program) noexcept; // a footswitch, in the shown bank
    void select(ProgramLocation location) noexcept;      // directly (host, saved state)

private:
    ProgramLocation current{};
    int shown = userBank;
};

} // namespace fivea
