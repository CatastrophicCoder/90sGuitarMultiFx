#pragma once

#include "core/ProgramBank.h"

namespace fivea
{

// The 30 slots a new instance starts with.
//
//   2-1      "METAL 1", from the owner's manual (documented::Metal1). Controls the manual does not
//            show keep their defaults (PLACEHOLDER).
//   2-2…6-5  Development presets: this project's own programs, not the original's factory
//            presets (whose values are not documented). Their names start with "DEV ".
//   bank 1   Copies of five of the above, as the original's factory filled its user bank
//            (SRC-001 p. 8); which five it chose is not documented.
[[nodiscard]] ProgramBank::Slots factoryPrograms() noexcept;

inline constexpr const char* developmentPresetPrefix = "DEV ";

// The bank 1 slots' sources, in order 1-1…1-5.
inline constexpr std::array<ProgramLocation, programsPerBank> userBankSources{{{2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}}};

} // namespace fivea
