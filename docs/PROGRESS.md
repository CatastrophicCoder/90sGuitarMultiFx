# Progress

Current status, decisions made, and a short log of each working session. The plan and milestone
definitions are in `docs/A5_implementation_plan.md` (§21); what is known about the original unit
versus assumed is in `docs/evidence-register.md`.

## Milestone status

| Milestone | Status | Notes |
|-----------|--------|-------|
| 0 Evidence and skeleton | Done (2026-10-04) | Pass-through plugin, versioned state, evidence register, CI on three platforms. Standalone not yet run with an audio device. |
| 1 Complete functional chain | Not started | |
| 2 Programs and workflow | Not started | |
| 3 Hardware-rate mode | Not started | |
| 4 Measurement tooling | Not started | |
| 5 Measurement-driven calibration | Not started | Needs access to a physical unit. |
| 6 Release hardening | Not started | Packaging (`.pkg`/`.dmg`) and `CHANGELOG.md` arrive here, as in the other Catastrophic Audio projects. |

## Decisions

Owner's decisions, with dates. Engineering decisions made while implementing are recorded in
`docs/architecture.md`, and hardware-related ones in `docs/evidence-register.md`.

- 2026-10-04: Company **Catastrophic Audio**; manufacturer code `Ctcd`, shared with the other
  plugins; bundle ID `com.catastrophicaudio.ninetiesmultifx`; plugin code `Nmf1`. Do not change
  the codes: hosts use them to recall saved sessions.
- 2026-10-04: Public GitHub repository `CatastrophicCoder/90sGuitarMultiFx`, default branch `main`.
- 2026-10-04: Development practice follows the other Catastrophic Audio projects (ampsim, eq):
  AGPLv3 licence; JUCE and Catch2 as pinned submodules in `external/` (same commits as ampsim);
  only Release builds install into the plugin folders; static MSVC runtime on Windows; auval and
  pluginval locally and in CI; `CLAUDE.md`; this file; `M<n>:` commit messages.
- Product name: "Nineties Multi-FX" is a working title (open decision, see `CLAUDE.md`).

## Session log

### 2026-10-04

- M0 implemented: CMake project, JUCE plugin (VST3, AU, Standalone), JUCE-free `A5Processor`
  engine with input trim, output level and global bypass, versioned XML state, editor with five
  disabled effect sections, 22 Catch2 tests, architecture/evidence/source/parameter/validation
  documents.
- Found while testing: 0 dB reloads from saved state as 3.6×10⁻⁷ dB after JUCE's range
  normalisation, which broke bit-exact pass-through. Gains are now snapped to the 0.1 dB step.
- Published to GitHub. First CI run: macOS and Windows passed; Linux failed on a missing
  `libxi-dev`, then (found in an Ubuntu 24.04 container) on the engine library not being built
  position-independent. Both fixed.
- Aligned with ampsim/eq practice (see Decisions). auval and pluginval (strictness 10, VST3 and
  AU) pass locally.
