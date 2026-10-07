#include "TestSignals.h"

#include "core/FactoryPrograms.h"
#include "core/FiveAProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

using fivea::EffectBlock;
using fivea::Program;
using fivea::ProgramControl;
using fivea::ProgramLocation;
using fivea::test::PlanarBuffer;

namespace
{
const Program& slot(const fivea::ProgramBank::Slots& slots, ProgramLocation location)
{
    return slots[static_cast<std::size_t>(location.slotIndex())];
}

bool on(const Program& program, EffectBlock block)
{
    return program.effectEnabled[static_cast<std::size_t>(block)];
}

bool isDevelopmentPreset(const Program& program)
{
    return program.name.view().starts_with(fivea::developmentPresetPrefix);
}

std::vector<ProgramLocation> allLocations()
{
    std::vector<ProgramLocation> locations;
    for (int bank = 1; bank <= fivea::numBanks; ++bank)
        for (int program = 1; program <= fivea::programsPerBank; ++program)
            locations.push_back({bank, program});
    return locations;
}
} // namespace

TEST_CASE("2-1 is METAL 1 as the owner's manual shows it")
{
    // SRC-001 p. 6, typed in from the manual rather than taken from the model profile, so a
    // mistake in either shows up here.
    const auto metal = slot(fivea::factoryPrograms(), {2, 1});
    CHECK(metal.name.view() == "METAL 1");

    CHECK_FALSE(on(metal, EffectBlock::Compressor));
    CHECK(on(metal, EffectBlock::Drive));
    CHECK(on(metal, EffectBlock::Equaliser));
    CHECK_FALSE(on(metal, EffectBlock::Modulation));
    CHECK(on(metal, EffectBlock::TimeEffect));

    CHECK(metal.drive.mode == 1);
    CHECK(metal.drive.drive == 14);
    CHECK(metal.drive.tone == 15);
    CHECK(metal.drive.level == 11);
    CHECK(metal.equaliser.bass == 6);
    CHECK(metal.equaliser.midFrequency == 3);
    CHECK(metal.equaliser.mid == 5);
    CHECK(metal.equaliser.treble == 1);
    CHECK(metal.equaliser.trim == 12);
    CHECK(metal.timeEffects.mode == 4);
    CHECK(metal.timeEffects.mix == 5);

    // Not shown in the manual: defaults.
    const Program defaults;
    CHECK(metal.compressor == defaults.compressor);
    CHECK(metal.modulation == defaults.modulation);
    CHECK(metal.noiseReductionLevel == defaults.noiseReductionLevel);
    CHECK(metal.master == defaults.master);
}

TEST_CASE("Every other factory slot is a named development preset")
{
    const auto slots = fivea::factoryPrograms();
    std::set<std::string> names;
    for (const auto location : allLocations())
    {
        if (location.bank == fivea::userBank)
            continue;
        const auto& program = slot(slots, location);
        INFO(location.bank << "-" << location.program << " " << program.name.view());
        CHECK(isDevelopmentPreset(program) == !(location == ProgramLocation{2, 1}));
        CHECK(program.name.view().size() > std::string_view{fivea::developmentPresetPrefix}.size());
        CHECK(names.insert(std::string{program.name.view()}).second); // unique
    }
    CHECK(names.size() == 25);
}

TEST_CASE("Bank 1 starts as copies of five factory programs")
{
    const auto slots = fivea::factoryPrograms();
    std::set<std::string> names;
    for (int program = 1; program <= fivea::programsPerBank; ++program)
    {
        const auto source = fivea::userBankSources[static_cast<std::size_t>(program - 1)];
        INFO("1-" << program);
        CHECK(source.bank != fivea::userBank);
        CHECK(slot(slots, {1, program}) == slot(slots, source));
        names.insert(std::string{slot(slots, {1, program}).name.view()});
    }
    CHECK(names.size() == 5);
}

TEST_CASE("Every factory value is within its control's documented range")
{
    const auto slots = fivea::factoryPrograms();
    for (const auto location : allLocations())
        for (int index = 0; index < fivea::numProgramControls; ++index)
        {
            const auto control = static_cast<ProgramControl>(index);
            const auto range = fivea::controlRange(control);
            const int value = fivea::controlValue(slot(slots, location), control);
            INFO(location.bank << "-" << location.program << ", control " << index << " = " << value);
            CHECK(value >= range.minimum);
            CHECK(value <= range.maximum);
        }
}

TEST_CASE("Echoverb programs keep TIME within its documented 0 to 3")
{
    for (const auto& program : fivea::factoryPrograms())
        if (program.timeEffects.mode == static_cast<int>(fivea::TimeEffectMode::Echoverb))
        {
            INFO(program.name.view());
            CHECK(program.timeEffects.time <= fivea::documented::echoverbMaximumTimeStep);
        }
}

TEST_CASE("The factory programs use every mode of every effect")
{
    std::set<int> driveModes;
    std::set<int> modulationModes;
    std::set<int> timeEffectModes;
    std::array<bool, fivea::numEffectBlocks> used{};
    for (const auto& program : fivea::factoryPrograms())
    {
        for (std::size_t block = 0; block < fivea::numEffectBlocks; ++block)
            used[block] = used[block] || program.effectEnabled[block];
        if (on(program, EffectBlock::Drive))
            driveModes.insert(program.drive.mode);
        if (on(program, EffectBlock::Modulation))
            modulationModes.insert(program.modulation.mode);
        if (on(program, EffectBlock::TimeEffect))
            timeEffectModes.insert(program.timeEffects.mode);
    }
    CHECK(used == std::array<bool, fivea::numEffectBlocks>{true, true, true, true, true});
    CHECK(driveModes.size() == 2);
    CHECK(modulationModes.size() == 5);
    CHECK(timeEffectModes.size() == 7);
}

TEST_CASE("Every factory program plays through the chain with finite, audible output")
{
    const auto slots = fivea::factoryPrograms();
    for (const auto location : allLocations())
    {
        fivea::ParameterSnapshot snapshot;
        fivea::applyProgram(slot(slots, location), snapshot);

        fivea::FiveAProcessor processor;
        processor.prepare({48000.0, 512, 2, 2});
        processor.setParameters(snapshot);
        processor.reset();

        PlanarBuffer buffer{2, 24000};
        fivea::test::fillSine(buffer, 220.0, 48000.0, 0.1f);
        processor.process(buffer.view());

        INFO(location.bank << "-" << location.program << " " << slot(slots, location).name.view());
        CHECK(fivea::test::allFinite(buffer));
        CHECK(fivea::test::rms(buffer, 0, 12000, 12000) > 1.0e-3);
    }
}
