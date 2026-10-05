# Third-party notices

| Component | Version | Used in | Licence | Source |
|-----------|---------|---------|---------|--------|
| JUCE | 9.0.2 | Plugins (distributed) | Dual: AGPLv3 or the commercial JUCE 9 licence | https://github.com/juce-framework/JUCE |
| Catch2 | 3.9.1 | Tests only (not distributed) | Boost Software License 1.0 | https://github.com/catchorg/Catch2 |

Both are git submodules in `external/`, pinned to their release tags. Their code is not copied
into this repository's history.

**JUCE licensing note.** Any binary built from this project links JUCE, which this project uses
under the AGPLv3. The project itself is licensed AGPLv3 (`LICENSE`), so its source is available
under the same terms as any binary built from it.

This project contains no firmware, ROM data, factory-preset data, logos, panel artwork or product
photography from the original hardware.
