#include "core/FactoryPrograms.h"

namespace fivea
{

namespace
{
constexpr std::size_t index(EffectBlock block)
{
    return static_cast<std::size_t>(block);
}

// Builds a program effect by effect; an effect not mentioned stays off with default controls.
class Builder
{
public:
    explicit Builder(const char* name) { program.name = ProgramName::from(name); }

    Builder& compressor(int sens, int attack, int level)
    {
        program.effectEnabled[index(EffectBlock::Compressor)] = true;
        program.compressor = {.sens = sens, .attack = attack, .level = level};
        return *this;
    }

    Builder& drive(DriveMode mode, int drive, int tone, int level)
    {
        program.effectEnabled[index(EffectBlock::Drive)] = true;
        program.drive = {.mode = static_cast<int>(mode), .drive = drive, .tone = tone, .level = level};
        return *this;
    }

    Builder& equaliser(int bass, int midFrequency, int mid, int treble, int trim)
    {
        program.effectEnabled[index(EffectBlock::Equaliser)] = true;
        program.equaliser = {.bass = bass, .midFrequency = midFrequency, .mid = mid, .treble = treble, .trim = trim};
        return *this;
    }

    Builder& modulation(ModulationMode mode, int speed, int depth, int feedback, int mix)
    {
        program.effectEnabled[index(EffectBlock::Modulation)] = true;
        program.modulation = {
            .mode = static_cast<int>(mode), .speed = speed, .depth = depth, .feedback = feedback, .mix = mix};
        return *this;
    }

    // The reverb modes have no TIME, FINE or F.BACK (SRC-001 p. 12); those keep their defaults.
    Builder& reverb(TimeEffectMode mode, int mix)
    {
        program.effectEnabled[index(EffectBlock::TimeEffect)] = true;
        program.timeEffects.mode = static_cast<int>(mode);
        program.timeEffects.mix = mix;
        return *this;
    }

    Builder& delay(TimeEffectMode mode, int time, int fine, int feedback, int mix)
    {
        program.effectEnabled[index(EffectBlock::TimeEffect)] = true;
        program.timeEffects = {
            .mode = static_cast<int>(mode), .time = time, .fine = fine, .feedback = feedback, .mix = mix};
        return *this;
    }

    Builder& noiseReduction(int level)
    {
        program.noiseReductionLevel = level;
        return *this;
    }

    operator Program() const { return program; }

private:
    Program program;
};

Program metal1()
{
    using M = documented::Metal1;
    Program program = Builder{M::name}
                          .drive(static_cast<DriveMode>(M::driveMode), M::driveDrive, M::driveTone, M::driveLevel)
                          .equaliser(M::eqBass, M::eqMidFrequency, M::eqMid, M::eqTreble, M::eqTrim)
                          .reverb(static_cast<TimeEffectMode>(M::timeEffectMode), M::timeEffectMix);
    return program;
}
} // namespace

ProgramBank::Slots factoryPrograms() noexcept
{
    using enum DriveMode;
    using enum ModulationMode;
    using enum TimeEffectMode;

    ProgramBank::Slots slots{};
    auto set = [&slots](int bank, int program, const Program& content)
    {
        slots[static_cast<std::size_t>(ProgramLocation{bank, program}.slotIndex())] = content;
    };

    // Bank 2: drive.
    set(2, 1, metal1());
    set(2, 2, Builder{"DEV Soft Drive"}.drive(Overdrive, 6, 9, 12));
    set(2, 3, Builder{"DEV Hard Drive"}.drive(Distortion, 11, 8, 11).equaliser(2, 4, 3, 1, 13).reverb(RoomReverb, 4));
    set(2, 4, Builder{"DEV Crunch"}.compressor(6, 4, 12).drive(Overdrive, 10, 10, 12));
    set(2, 5,
        Builder{"DEV Lead Drive"}
            .drive(Distortion, 13, 11, 11)
            .equaliser(0, 5, 4, 0, 13)
            .delay(Delay, 3, 5, 5, 6)
            .noiseReduction(4));

    // Bank 3: clean and compressed.
    set(3, 1, Builder{"DEV Clean Compression"}.compressor(8, 4, 12));
    set(3, 2, Builder{"DEV Squeeze"}.compressor(13, 6, 13));
    set(3, 3, Builder{"DEV Clean Bright"}.compressor(5, 4, 12).equaliser(-1, 3, 0, 3, 15));
    set(3, 4, Builder{"DEV Clean Warm"}.equaliser(3, 2, 2, -2, 15));
    set(3, 5,
        Builder{"DEV Clean Slap"}.compressor(10, 7, 12).equaliser(0, 3, 0, 2, 15).modulation(Slapback, 0, 0, 0, 8));

    // Bank 4: chorus, flanger, slapback.
    set(4, 1, Builder{"DEV Wide Chorus"}.modulation(Chorus2, 4, 10, 0, 15));
    set(4, 2, Builder{"DEV Light Chorus"}.modulation(Chorus1, 6, 5, 0, 9));
    set(4, 3, Builder{"DEV Jet Flanger"}.modulation(Flanger1, 2, 12, 11, 15));
    set(4, 4, Builder{"DEV Slow Flanger"}.modulation(Flanger2, 1, 9, 7, 12));
    set(4, 5, Builder{"DEV Slapback"}.modulation(Slapback, 0, 0, 3, 10));

    // Bank 5: reverb and delay.
    set(5, 1, Builder{"DEV Short Ambience"}.reverb(RoomReverb, 6));
    set(5, 2, Builder{"DEV Big Hall"}.reverb(HallReverb, 9));
    set(5, 3, Builder{"DEV Plate"}.reverb(PlateReverb, 8));
    set(5, 4, Builder{"DEV Echo Delay"}.delay(Delay, 3, 8, 6, 9));
    set(5, 5, Builder{"DEV Echoverb"}.delay(Echoverb, 2, 5, 4, 9));

    // Bank 6: combinations.
    set(6, 1,
        Builder{"DEV Drive Chorus Hall"}
            .drive(Overdrive, 9, 9, 12)
            .modulation(Chorus1, 5, 6, 0, 8)
            .reverb(HallReverb, 6));
    set(6, 2,
        Builder{"DEV Ensemble Lead"}
            .compressor(7, 3, 12)
            .drive(Distortion, 12, 10, 11)
            .equaliser(1, 5, 3, 1, 13)
            .reverb(EnsembleHallReverb, 7)
            .noiseReduction(3));
    set(6, 3,
        Builder{"DEV Flange Drive"}
            .drive(Distortion, 10, 9, 11)
            .modulation(Flanger1, 3, 10, 9, 12)
            .reverb(RoomReverb, 5));
    set(6, 4, Builder{"DEV Live Stage"}.compressor(6, 4, 12).drive(Overdrive, 7, 8, 12).reverb(LiveStageReverb, 7));
    set(6, 5,
        Builder{"DEV Full Chain"}
            .compressor(8, 4, 12)
            .drive(Overdrive, 8, 9, 12)
            .equaliser(1, 3, 1, 1, 14)
            .modulation(Chorus2, 3, 7, 0, 8)
            .delay(Echoverb, 2, 0, 3, 7)
            .noiseReduction(2));

    for (int program = 1; program <= programsPerBank; ++program)
        set(userBank, program,
            slots[static_cast<std::size_t>(userBankSources[static_cast<std::size_t>(program - 1)].slotIndex())]);

    return slots;
}

} // namespace fivea
