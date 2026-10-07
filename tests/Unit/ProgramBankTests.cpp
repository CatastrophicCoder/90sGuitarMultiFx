#include "AllocationGuard.h"

#include "core/ProgramBank.h"
#include "core/ProgramMode.h"

#include <catch2/catch_test_macros.hpp>

using fivea::Program;
using fivea::ProgramBank;
using fivea::ProgramControl;
using fivea::ProgramLocation;
using fivea::ProgramName;
using fivea::ProgramSelection;
using fivea::test::ScopedAllocationCounter;

namespace
{
constexpr auto allControls()
{
    std::array<ProgramControl, fivea::numProgramControls> controls{};
    for (int index = 0; index < fivea::numProgramControls; ++index)
        controls[static_cast<std::size_t>(index)] = static_cast<ProgramControl>(index);
    return controls;
}

// Every control a different value, and an uneven on/off pattern, so a field copied into the wrong
// place cannot go unnoticed.
Program distinctProgram(int offset = 0)
{
    Program program;
    program.name = ProgramName::from("Distinct");
    program.effectEnabled = {true, false, true, true, false};
    for (const auto control : allControls())
        fivea::controlValue(program, control) = 100 + 10 * offset + static_cast<int>(control);
    return program;
}
} // namespace

TEST_CASE("Program locations number banks 1 to 6 and programs 1 to 5")
{
    CHECK(fivea::numProgramSlots == 30);
    CHECK(ProgramLocation{1, 1}.slotIndex() == 0);
    CHECK(ProgramLocation{2, 1}.slotIndex() == 5);
    CHECK(ProgramLocation{6, 5}.slotIndex() == 29);

    CHECK(ProgramLocation{6, 5}.isValid());
    CHECK_FALSE(ProgramLocation{0, 1}.isValid());
    CHECK_FALSE(ProgramLocation{7, 1}.isValid());
    CHECK_FALSE(ProgramLocation{1, 6}.isValid());
    CHECK(ProgramLocation{9, -3}.clamped() == ProgramLocation{6, 1});
}

TEST_CASE("Every control reads and writes its own field")
{
    const auto program = distinctProgram();
    for (const auto control : allControls())
    {
        INFO("control " << static_cast<int>(control));
        CHECK(fivea::controlValue(program, control) == 100 + static_cast<int>(control));
    }
    CHECK(program.compressor.sens == 100);
    CHECK(program.equaliser.trim == 100 + static_cast<int>(ProgramControl::EqualiserTrim));
    CHECK(program.master == 100 + static_cast<int>(ProgramControl::Master));
}

TEST_CASE("A program round-trips through a snapshot, leaving the panel's own controls alone")
{
    const auto program = distinctProgram();

    fivea::ParameterSnapshot snapshot;
    snapshot.inputTrimDb = -6.5f;
    snapshot.outputLevelDb = 3.0f;
    snapshot.globalBypass = true;
    fivea::applyProgram(program, snapshot);

    CHECK(snapshot.inputTrimDb == -6.5f);
    CHECK(snapshot.outputLevelDb == 3.0f);
    CHECK(snapshot.globalBypass);
    CHECK(snapshot.effectEnabled == program.effectEnabled);
    CHECK(snapshot.timeEffects == program.timeEffects);
    CHECK(snapshot.noiseReductionLevel == program.noiseReductionLevel);

    CHECK(fivea::programFrom(snapshot, program.name) == program);
}

TEST_CASE("Names keep printable ASCII and are cut at the maximum length")
{
    CHECK(ProgramName::from("METAL 1").view() == "METAL 1");
    CHECK(ProgramName::from("Tab\there").view() == "Tabhere");
    CHECK(ProgramName::from(std::string(40, 'x')).view().size() == ProgramName::maxLength);
    CHECK(ProgramName{}.view().empty());
}

TEST_CASE("Only the user bank can be written")
{
    ProgramBank bank;
    const auto program = distinctProgram();

    for (int bankNumber = 2; bankNumber <= fivea::numBanks; ++bankNumber)
        for (int programNumber = 1; programNumber <= fivea::programsPerBank; ++programNumber)
        {
            INFO(bankNumber << "-" << programNumber);
            CHECK_FALSE(bank.write({bankNumber, programNumber}, program));
            CHECK(bank.at({bankNumber, programNumber}) == Program{});
        }
    CHECK_FALSE(bank.write({1, 0}, program));
    CHECK_FALSE(bank.write({1, 6}, program));

    CHECK(bank.write({1, 3}, program));
    for (int slot = 0; slot < fivea::numProgramSlots; ++slot)
    {
        INFO("slot " << slot);
        CHECK((bank.allSlots()[static_cast<std::size_t>(slot)] == program) == (slot == 2));
    }
}

TEST_CASE("Writing replaces the slot's previous program")
{
    ProgramBank bank;
    REQUIRE(bank.write({1, 5}, distinctProgram(1)));
    REQUIRE(bank.write({1, 5}, distinctProgram(2)));
    CHECK(bank.at({1, 5}) == distinctProgram(2));
}

TEST_CASE("A bank shown on the selector is not entered until a program is picked")
{
    ProgramSelection selection{{2, 1}};
    CHECK(selection.selected() == ProgramLocation{2, 1});
    CHECK(selection.shownBank() == 2);
    CHECK_FALSE(selection.isBankPending());

    selection.showBank(4);
    CHECK(selection.selected() == ProgramLocation{2, 1}); // still playing 2-1
    CHECK(selection.shownBank() == 4);
    CHECK(selection.isBankPending()); // no dot

    CHECK(selection.selectProgram(3) == ProgramLocation{4, 3});
    CHECK(selection.selected() == ProgramLocation{4, 3});
    CHECK_FALSE(selection.isBankPending());

    selection.showBank(4); // moving to the bank already playing keeps it
    CHECK_FALSE(selection.isBankPending());

    selection.showBank(5);
    selection.showBank(4); // and back again, without picking: nothing pending
    CHECK_FALSE(selection.isBankPending());
}

TEST_CASE("Selecting directly shows the selected bank, and out-of-range numbers are clamped")
{
    ProgramSelection selection;
    CHECK(selection.selected() == ProgramLocation{1, 1});
    selection.showBank(3);
    selection.select({5, 2});
    CHECK(selection.shownBank() == 5);
    CHECK_FALSE(selection.isBankPending());

    selection.showBank(12);
    CHECK(selection.shownBank() == fivea::numBanks);
    CHECK(selection.selectProgram(0) == ProgramLocation{6, 1});
}

TEST_CASE("The dot's comparisons each notice a single change")
{
    const auto stored = distinctProgram();
    CHECK(fivea::sameSettings(stored, stored));
    CHECK(fivea::sameEffectSwitches(stored, stored));

    for (const auto changed : allControls())
    {
        auto edited = stored;
        fivea::controlValue(edited, changed) += 1;
        INFO("changed control " << static_cast<int>(changed));
        CHECK_FALSE(fivea::sameSettings(edited, stored));
        CHECK(fivea::sameEffectSwitches(edited, stored));
        for (const auto control : allControls())
            CHECK(fivea::sameControl(edited, stored, control) == (control != changed));
    }

    for (std::size_t block = 0; block < fivea::numEffectBlocks; ++block)
    {
        auto edited = stored;
        edited.effectEnabled[block] = !edited.effectEnabled[block];
        INFO("switched effect " << block);
        CHECK_FALSE(fivea::sameEffectSwitches(edited, stored));
        CHECK_FALSE(fivea::sameSettings(edited, stored));
    }

    auto renamed = stored;
    renamed.name = ProgramName::from("Another name");
    CHECK(fivea::sameSettings(renamed, stored)); // a name is not a setting
    CHECK_FALSE(renamed == stored);
}

TEST_CASE("A new instance starts in Program mode")
{
    CHECK(fivea::ProgramMode{} == fivea::ProgramMode::Program);
}

TEST_CASE("Copying, applying and writing programs do not allocate")
{
    ProgramBank bank;
    ProgramSelection selection;
    fivea::ParameterSnapshot snapshot;
    const auto program = distinctProgram();

    std::size_t allocations = 1;
    {
        const ScopedAllocationCounter counter;
        Program copy = program;
        fivea::applyProgram(copy, snapshot);
        copy = fivea::programFrom(snapshot, ProgramName::from("Copy"));
        bank.write({1, 2}, copy);
        selection.showBank(2);
        selection.selectProgram(4);
        ScopedAllocationCounter::keepAlive(&bank.at(selection.selected()));
        allocations = counter.count();
    }
    CHECK(allocations == 0);
}
