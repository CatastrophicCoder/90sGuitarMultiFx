#pragma once

#include "core/FactoryPrograms.h"
#include "core/ProgramBank.h"
#include "core/ProgramMode.h"

namespace fivea
{

// Everything about programs that a session saves: the 30 slots, which one is selected (and which
// bank the display shows), and the panel's mode. Message thread only; the audio thread sees a
// program through the parameters it is applied to.
struct ProgramState
{
    ProgramBank bank{factoryPrograms()};
    ProgramSelection selection;
    ProgramMode mode = ProgramMode::Program;
};

} // namespace fivea
