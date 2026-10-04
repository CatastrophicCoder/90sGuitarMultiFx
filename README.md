# Nineties Multi-FX (working title)

A guitar multi-effect plugin whose architecture and workflow are inspired by the red Korg A5
Guitar performance signal processor of the early 1990s. It is an independent project, not
affiliated with or endorsed by Korg.

**Status: Milestone 0.** The project builds VST3, Standalone and (on macOS) Audio Unit targets
that pass audio through with input trim, output level and bypass. **There are no effects yet**, and
nothing in this build reproduces the sound of the original unit. See `docs/A5_implementation_plan.md`
for the roadmap and `docs/evidence-register.md` for what is known versus assumed.

## Requirements

- CMake 3.25 or newer, and Ninja (recommended)
- A C++20 compiler: Xcode 15+ / Apple Clang, Visual Studio 2022, or GCC 11+ / Clang 15+
- Internet access on the first configure (JUCE and Catch2 are downloaded and checksum-verified)
- Linux only: the JUCE system packages listed in `.github/workflows/ci.yml`

## Build and test

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Built products land in `build/A5Plugin_artefacts/<config>/`:

| Format | Path |
|--------|------|
| Standalone | `Standalone/Nineties Multi-FX.app` (macOS), `.exe` (Windows), binary (Linux) |
| VST3 | `VST3/Nineties Multi-FX.vst3` |
| AU (macOS) | `AU/Nineties Multi-FX.component` |

To have the build copy the plugins into your user plugin folders, configure with
`-DA5_COPY_PLUGIN=ON`. To build without tests, `-DA5_BUILD_TESTS=OFF`.

To use a local JUCE checkout instead of downloading:

```sh
cmake -B build -G Ninja -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
```

### Formatting

CI checks formatting with clang-format 18.1.8:

```sh
pipx install clang-format==18.1.8
clang-format -i $(git ls-files 'src/*.h' 'src/*.cpp' 'tests/*.h' 'tests/*.cpp')
```

## Layout

| Path | Contents |
|------|----------|
| `src/core` | Engine façade (`A5Processor`, pure C++), parameter IDs, parameter layout and state (JUCE) |
| `src/dsp` | DSP building blocks (only gain smoothing so far) |
| `src/plugin` | JUCE `AudioProcessor` and editor |
| `src/ui` | Editor components |
| `tests/Unit` | Engine tests, no JUCE |
| `tests/Integration` | Plugin tests through the `AudioProcessor` API |
| `docs` | Architecture, evidence and source registers, parameter specification, validation plan |

## Licence

Not yet chosen. JUCE 9 is available under AGPLv3 or a commercial licence, which constrains the
choice; see `THIRD_PARTY_NOTICES.md`.
