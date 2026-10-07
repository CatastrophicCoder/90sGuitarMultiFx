#include "core/ProgramBank.h"

#include <algorithm>

namespace fivea
{

bool ProgramLocation::isValid() const noexcept
{
    return bank >= 1 && bank <= numBanks && program >= 1 && program <= programsPerBank;
}

ProgramLocation ProgramLocation::clamped() const noexcept
{
    return {std::clamp(bank, 1, numBanks), std::clamp(program, 1, programsPerBank)};
}

int ProgramLocation::slotIndex() const noexcept
{
    const auto location = clamped();
    return (location.bank - 1) * programsPerBank + (location.program - 1);
}

const Program& ProgramBank::at(ProgramLocation location) const noexcept
{
    return slots[static_cast<std::size_t>(location.slotIndex())];
}

bool ProgramBank::write(ProgramLocation location, const Program& program) noexcept
{
    if (!location.isValid() || location.bank != userBank)
        return false;
    slots[static_cast<std::size_t>(location.slotIndex())] = program;
    return true;
}

void ProgramSelection::showBank(int bank) noexcept
{
    shown = std::clamp(bank, 1, numBanks);
}

ProgramLocation ProgramSelection::selectProgram(int program) noexcept
{
    current = ProgramLocation{shown, program}.clamped();
    return current;
}

void ProgramSelection::select(ProgramLocation location) noexcept
{
    current = location.clamped();
    shown = current.bank;
}

} // namespace fivea
