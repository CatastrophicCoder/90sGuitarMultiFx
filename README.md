# Five-A MultiFX Processor

A guitar multi-effect plugin whose architecture and workflow are modelled on an early-1990s
guitar multi-effect floor unit, referred to here as "the original unit".

**Status: Milestone 2 done.** VST3 and (on macOS) Audio Unit plugins with the original
unit's five-effect chain (compressor, distortion/overdrive, 3-band EQ, chorus/flanger,
reverb/delay), noise reduction and master volume, each control with its original steps. 30
programs in six banks: bank 1 is yours to write; banks 2–6 hold development presets ("DEV …")
and the one factory program the original's manual documents. **Every algorithm is a
placeholder:** the controls and their documented values match the original, the sound does not
claim to. The editor is a replica of the original's front panel, in both its Program and
Manual/Edit modes. See `docs/implementation-plan.md` for the
roadmap and `docs/evidence-register.md` for what is known versus assumed.

## Requirements

- CMake 3.25 or newer, and Ninja (recommended)
- A C++20 compiler: Xcode 15+ / Apple Clang, Visual Studio 2022, or GCC 11+ / Clang 15+
- Linux only: the JUCE system packages listed in `.github/workflows/build.yml`

## Build and test

JUCE 9.0.2 and Catch2 v3.9.1 are git submodules in `external/`, pinned to their release tags.
Clone with them:

```sh
git clone --recurse-submodules https://github.com/CatastrophicCoder/90sGuitarMultiFx.git
# or, in an existing clone:
git submodule update --init
```

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Built products land in `build/FiveAPlugin_artefacts/<config>/`:

| Format | Path |
|--------|------|
| VST3 | `VST3/Five-A MultiFX Processor.vst3` |
| AU (macOS) | `AU/Five-A MultiFX Processor.component` |

A Release build also installs the AU and VST3 into your user plugin folders
(`~/Library/Audio/Plug-Ins` on macOS); a Debug build does not. Override with
`-DFIVEA_COPY_PLUGIN=ON` or `OFF`. To build without tests, `-DFIVEA_BUILD_TESTS=OFF`.

### Validating the plugin (macOS)

```sh
auval -v aufx Nmf1 Ctcd     # the installed AU, i.e. the last Release build
/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
    --validate "build/FiveAPlugin_artefacts/Release/VST3/Five-A MultiFX Processor.vst3"
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
| `external` | JUCE and Catch2 submodules, pinned to release tags |
| `src/core` | Engine façade (`FiveAProcessor`, pure C++), parameter IDs, parameter layout and state (JUCE) |
| `src/dsp` | DSP building blocks (only gain smoothing so far) |
| `src/plugin` | JUCE `AudioProcessor` and editor |
| `src/ui` | The replica panel: layout, look-and-feel, controls, LED display |
| `resources/fonts` | Barlow Condensed (SIL OFL), embedded in the plugin, with its licence |
| `tests/Unit` | Engine tests, no JUCE |
| `tests/Integration` | Plugin tests through the `AudioProcessor` API |
| `docs` | Architecture, evidence and source registers, parameter specification, validation plan |

## Licence

GNU Affero General Public License v3.0; see `LICENSE`. Third-party components and their licences
are listed in `THIRD_PARTY_NOTICES.md`.
