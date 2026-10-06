# Third-party notices

| Component | Version | Used in | Licence | Source |
|-----------|---------|---------|---------|--------|
| JUCE | 9.0.2 | Plugins (distributed) | Dual: AGPLv3 or the commercial JUCE 9 licence | https://github.com/juce-framework/JUCE |
| Catch2 | 3.9.1 | Tests only (not distributed) | Boost Software License 1.0 | https://github.com/catchorg/Catch2 |
| Barlow Condensed (Regular, SemiBold) | Google Fonts repository, fetched 2026-10-06 | Panel lettering, embedded in the plugin | SIL Open Font License 1.1; © 2017 The Barlow Project Authors | https://github.com/jpt/barlow; licence text in `resources/fonts/BarlowCondensed-OFL.txt` |

JUCE and Catch2 are git submodules in `external/`, pinned to their release tags; their code is not
copied into this repository's history. The font files are in `resources/fonts/` with their licence,
which must ship with any distributed build.

**JUCE licensing note.** Any binary built from this project links JUCE, which this project uses
under the AGPLv3. The project itself is licensed AGPLv3 (`LICENSE`), so its source is available
under the same terms as any binary built from it.

This project contains no firmware, ROM data, factory-preset data, logos, panel artwork or product
photography from the original hardware.
