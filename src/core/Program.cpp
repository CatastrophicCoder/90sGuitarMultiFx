#include "core/Program.h"

namespace fivea
{

Program programFrom(const ParameterSnapshot& snapshot, ProgramName name) noexcept
{
    Program program;
    program.name = name;
    program.effectEnabled = snapshot.effectEnabled;
    program.compressor = snapshot.compressor;
    program.drive = snapshot.drive;
    program.equaliser = snapshot.equaliser;
    program.modulation = snapshot.modulation;
    program.timeEffects = snapshot.timeEffects;
    program.noiseReductionLevel = snapshot.noiseReductionLevel;
    program.master = snapshot.master;
    return program;
}

void applyProgram(const Program& program, ParameterSnapshot& snapshot) noexcept
{
    snapshot.effectEnabled = program.effectEnabled;
    snapshot.compressor = program.compressor;
    snapshot.drive = program.drive;
    snapshot.equaliser = program.equaliser;
    snapshot.modulation = program.modulation;
    snapshot.timeEffects = program.timeEffects;
    snapshot.noiseReductionLevel = program.noiseReductionLevel;
    snapshot.master = program.master;
}

namespace
{
// One table for the const and non-const accessors: ProgramType is Program or const Program.
template <typename ProgramType> auto& valueOf(ProgramType& program, ProgramControl control) noexcept
{
    switch (control)
    {
    case ProgramControl::CompressorSens:
        return program.compressor.sens;
    case ProgramControl::CompressorAttack:
        return program.compressor.attack;
    case ProgramControl::CompressorLevel:
        return program.compressor.level;
    case ProgramControl::DriveMode:
        return program.drive.mode;
    case ProgramControl::DriveDrive:
        return program.drive.drive;
    case ProgramControl::DriveTone:
        return program.drive.tone;
    case ProgramControl::DriveLevel:
        return program.drive.level;
    case ProgramControl::EqualiserBass:
        return program.equaliser.bass;
    case ProgramControl::EqualiserMidFrequency:
        return program.equaliser.midFrequency;
    case ProgramControl::EqualiserMid:
        return program.equaliser.mid;
    case ProgramControl::EqualiserTreble:
        return program.equaliser.treble;
    case ProgramControl::EqualiserTrim:
        return program.equaliser.trim;
    case ProgramControl::ModulationMode:
        return program.modulation.mode;
    case ProgramControl::ModulationSpeed:
        return program.modulation.speed;
    case ProgramControl::ModulationDepth:
        return program.modulation.depth;
    case ProgramControl::ModulationFeedback:
        return program.modulation.feedback;
    case ProgramControl::ModulationMix:
        return program.modulation.mix;
    case ProgramControl::TimeEffectMode:
        return program.timeEffects.mode;
    case ProgramControl::TimeEffectTime:
        return program.timeEffects.time;
    case ProgramControl::TimeEffectFine:
        return program.timeEffects.fine;
    case ProgramControl::TimeEffectFeedback:
        return program.timeEffects.feedback;
    case ProgramControl::TimeEffectMix:
        return program.timeEffects.mix;
    case ProgramControl::NoiseReductionLevel:
        return program.noiseReductionLevel;
    case ProgramControl::Master:
        return program.master;
    }
    return program.master; // unreachable: every enumerator is handled above
}
} // namespace

int& controlValue(Program& program, ProgramControl control) noexcept
{
    return valueOf(program, control);
}

int controlValue(const Program& program, ProgramControl control) noexcept
{
    return valueOf(program, control);
}

documented::StepRange controlRange(ProgramControl control) noexcept
{
    switch (control)
    {
    case ProgramControl::CompressorSens:
        return documented::compressorSens;
    case ProgramControl::CompressorAttack:
        return documented::compressorAttack;
    case ProgramControl::DriveMode:
        return documented::driveMode;
    case ProgramControl::EqualiserBass:
    case ProgramControl::EqualiserMid:
    case ProgramControl::EqualiserTreble:
        return documented::eqBandGain;
    case ProgramControl::EqualiserMidFrequency:
        return documented::eqMidFrequency;
    case ProgramControl::ModulationMode:
        return documented::modulationMode;
    case ProgramControl::TimeEffectMode:
        return documented::timeEffectMode;
    case ProgramControl::TimeEffectTime:
        return {0, documented::delayMaximumTimeStep};
    case ProgramControl::TimeEffectFine:
        return documented::delayFine;
    default:
        return documented::zeroToFifteen;
    }
}

bool sameControl(const Program& a, const Program& b, ProgramControl control) noexcept
{
    return controlValue(a, control) == controlValue(b, control);
}

bool sameEffectSwitches(const Program& a, const Program& b) noexcept
{
    return a.effectEnabled == b.effectEnabled;
}

bool sameSettings(const Program& a, const Program& b) noexcept
{
    Program renamed = b;
    renamed.name = a.name;
    return a == renamed;
}

} // namespace fivea
