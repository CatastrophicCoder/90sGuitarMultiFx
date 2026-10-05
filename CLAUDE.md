# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Five-A MultiFX Processor by Catastrophic Audio: a guitar multi-effect plugin (AU, VST3)
whose architecture and workflow are modelled on an early-1990s guitar multi-effect
floor unit, called "the original unit" throughout the repository. Built with JUCE and C++20 in CLion. Public repo, licensed AGPLv3.

**Intended use: in front of an amp (or amp sim), as the original was used.** Guitar → this plugin
→ amp → cabinet. The plugin is the effects unit only: it contains no amp and no cabinet, is not
meant to be heard on its own, and therefore has no standalone target. Design the effects for a
guitar's instrument-level signal, with an output level suited to an amp's input: the drive block
works like a pedal, driving the amp. A single amp is mono, so the L/Mono output matters
(plan §3.3). The placement is a design decision (evidence register EV-107); that the original was
used this way is confirmed by the owner's manual (EV-012).

Read these at the start of every session:

- `docs/implementation-plan.md`: the specification. Milestones and their acceptance criteria
  are in §21.
- `docs/PROGRESS.md`: milestone status, owner's decisions, session log.
- `docs/evidence-register.md`: what is known about the original unit versus assumed.
- `docs/architecture.md`: how the code is put together, and where it differs from the plan's layout.

**Current state:** Milestone 1 in progress (see `docs/PROGRESS.md`): the full chain and every
documented control work; the replica panel (step 8) and docs (step 9) remain.

## How to work in this repo

- Work only on the milestone named in my prompt. Do not start the next one.
- Before editing, propose a plan: files to add or change, tests to write, commands you will run.
  Wait for my OK.
- Items under "Open decisions" below are mine to make. Do not pick one silently: lay out the
  options with their trade-offs and ask.
- Do not add third-party dependencies without asking.
- When a milestone step is done: build, run all tests, run auval and pluginval, update
  `docs/PROGRESS.md` (status and a short session log entry) and any document the change affects,
  then summarise what changed.
- Small, focused commits. Message format: `M<n>: <what changed>` (e.g. `M1: add compressor static
  curve test`). Work that belongs to no milestone uses `Docs:`, `CI:` or `Build:`.

## Evidence rules (plan §2)

This project's central rule: never present an inferred or placeholder behaviour as a property of
the original hardware.

- Every hardware-related behaviour gets a row in `docs/evidence-register.md`: CONFIRMED, INFERRED,
  PLACEHOLDER or MEASURED, with source, consequence, confidence and open questions.
- CONFIRMED needs a source catalogued in `docs/source-register.md`: SRC-001 is the original
  unit's owner's manual (cite printed pages), SRC-002 its service manual (cite PDF pages). Both are
  local copies kept outside the repository, and are never committed. Where they and the plan
  differ, the documents win; record the difference.
- The documented parameter set is in `docs/parameter-specification.md`. Labels, ranges and
  discrete values are CONFIRMED; how a step maps to dB, Hz or ms is PLACEHOLDER unless the manual
  states it.
- Do not invent DSP algorithms and call them the original unit's. Placeholder algorithms are named as
  placeholders in code, UI and docs.
- Do not derive hardware values from indirect evidence (e.g. maximum delay time from the 512 kbit
  DRAM size, plan §12.1; a 17 kHz low-pass or bit-crushing from the spec sheet, plan §13).
- Hardware-specific constants live in one model profile (plan §14), never scattered across
  processors.
- Plugin design choices (ranges, ramp times) are recorded as PLACEHOLDER (design choice).

## Build, test and validate

```bash
git submodule update --init                    # once, after cloning

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R "bypass"             # one test, or a pattern

# Release: also installs the AU and VST3 into ~/Library/Audio/Plug-Ins
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# auval tests the installed AU, which is only ever a Release build: build build-release first.
killall -9 AudioComponentRegistrar 2>/dev/null   # if a fresh build does not show up in auval -a
auval -v aufx Nmf1 Ctcd
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
    --validate "build-release/FiveAPlugin_artefacts/Release/VST3/Five-A MultiFX Processor.vst3"
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
    --validate "$HOME/Library/Audio/Plug-Ins/Components/Five-A MultiFX Processor.component"

# memory and undefined-behaviour check of the engine tests (no plugin build needed)
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan --target fivea_engine_tests && ASAN_OPTIONS=detect_leaks=0 build-asan/tests/fivea_engine_tests

# formatting, as CI checks it (clang-format 18.1.8)
clang-format -i $(git ls-files 'src/*.h' 'src/*.cpp' 'tests/*.h' 'tests/*.cpp')
```

Run `ctest` after any DSP change and the validators after any change to the processor shell. They
cover different things: the validators catch threading and state bugs a DAW hides; `ctest` catches
wrong DSP, which the validators never look at.

- Plugin identity: product `Five-A MultiFX Processor`, company `Catastrophic Audio`, CMake target
  `FiveAPlugin`, manufacturer code `Ctcd`, plugin code `Nmf1`, bundle ID
  `com.catastrophicaudio.fivea`. **Do not change the codes or the bundle
  ID** once released: hosts use them to recall saved sessions. The state root tag `FiveAState`
  also stays, so older sessions load.
- `FIVEA_COPY_PLUGIN` is on for Release and off otherwise: Debug and Release install to the same
  place, and whichever built last would be what Logic loads. `FIVEA_BUILD_TESTS` (on) builds the tests.
- JUCE 9.0.2 and Catch2 v3.9.1 are submodules in `external/`, pinned to release tags (same
  commits as ampsim). Plugin formats: VST3, and AU on macOS. No standalone (see Project).
- CI (`.github/workflows/build.yml`) checks formatting, then builds Debug and Release on macOS,
  Windows and Linux, runs the tests, validates (auval and pluginval on macOS, pluginval on
  Windows) and uploads the Release artefacts. Linux needs the apt packages listed there; CI is
  the only Linux and Windows build anyone runs, so push a branch and use "Run workflow" to check
  them. An Ubuntu 24.04 Docker container reproduces the Linux job locally.
- Macs are arm64, macOS 11+. No signing or notarisation yet.
- You cannot listen to audio. Numerical tests are the only way to check DSP changes; I do
  listening checks myself.

## Architecture in brief

```
Host → PluginProcessor (src/plugin, JUCE: buses, APVTS, state, editor)
         │ ParameterSnapshot by value, AudioBufferView
         ▼
       FiveAProcessor (src/core, pure C++20, no JUCE)
         input trim → Compressor → Drive → 3-band EQ → Chorus/Flanger → Reverb/Delay → output
```

- `fivea_engine` must stay free of JUCE so it can be tested without a host and wrapped for CLAP later.
- Parameter IDs live in `src/core/ParameterIds.h`. Never change an existing ID or its version
  hint; add new parameters with the version hint of the release that adds them.
- Parameter pointers are cached in the constructor, not looked up by string per block.
- `setStateInformation` touches only parameters; it runs on the message thread. State changes go
  through `src/core/PresetState.cpp`: bump `currentSchemaVersion` and add a migration step for any
  non-additive change.

## Hard rules: real-time audio thread

Inside `processBlock`, `FiveAProcessor::setParameters`/`process` and anything they call:

- No memory allocation, no `new`/`delete`, no container growth.
- No locks, no `std::mutex`, no waiting on other threads.
- No file I/O, logging, `DBG` or console output.
- No object construction that allocates, and no parsing of host state.

Also: every user-facing parameter that could click is smoothed; anything expensive happens off the
audio thread and is swapped in atomically.

## Testing rules

- Every behaviour change comes with tests that check it numerically. Write the test first.
- A test is only worth committing if it fails when the behaviour is broken: check by reverting
  the fix, not by assuming.
- Engine tests go in `tests/Unit` and link only `fivea_engine`; tests through the `AudioProcessor`
  go in `tests/Integration`.
- Sample-rate-dependent behaviour is tested at 44.1, 48, 88.2, 96 and 192 kHz (plan §6.2), with
  mono and stereo layouts and block sizes including 1.
- Use fixed seeds and phases; free-running modulation is compared sample-for-sample only in a
  fixed test mode (plan §19.3).
- Exact float comparisons are fine where the behaviour is specified as bit-exact (unity gain,
  bypass); otherwise state a tolerance.

## Code conventions

- C++20, JUCE naming style: `camelCase` functions and variables, `PascalCase` classes, descriptive
  names rather than abbreviations.
- DSP in `src/dsp/`, engine in `src/core/`, JUCE adapter in `src/plugin/`, UI in `src/ui/`, tests in
  `tests/`, measurement tools in `tools/`.
- RAII, no raw owning pointers, const correctness, no hidden mutable global state.
- Comments explain why, not what.
- Introduce abstractions only where the plan requires replacement or calibration (plan §23).

## Do not

- Edit anything under `external/`.
- Commit firmware, ROM contents, factory-preset data, or captures without clear redistribution
  rights (plan §1, §16, §18).
- Put a trademarked brand or model name (the original unit's maker or model, or anyone else's)
  anywhere in the repository or the UI: docs, code identifiers, file names, commit messages, UI
  text. Use our own names; refer to the original as "the original unit". Chip part numbers in
  the evidence register are technical facts and may stay.
- Use another company's logo or wordmark, ship or commit product photography, or trace or cut
  artwork from photos or scans. The panel is a close visual replica by my decision
  (`docs/panel-specification.md`), but all artwork is drawn from scratch.
- Add disclaimers about, or commentary on, the original unit's maker.
- Copy code from GPL/AGPL projects. Reading them for ideas is fine; implement from published
  papers and formulas and cite the source in a comment.
- Describe or advertise AI-assisted development in the README, the user-facing docs or
  `docs/PROGRESS.md` (same as the other Catastrophic Audio projects). Commit trailers and this file
  are fine. The plan's §22 heading predates this rule and stays as written.

## Open decisions (ask before assuming)

Record each decision in `docs/PROGRESS.md` once I make it, then move it out of this list.

- The factory preset values: only 2-1 "METAL 1" is documented. The rest are in an "Effect
  Parameter List" sheet that is not in SRC-001.
