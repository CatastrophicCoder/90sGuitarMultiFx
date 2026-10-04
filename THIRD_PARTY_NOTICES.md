# Third-party notices

| Component | Version | Used in | Licence | Source |
|-----------|---------|---------|---------|--------|
| JUCE | 9.0.2 | Plugin and standalone (distributed) | Dual: AGPLv3 or the commercial JUCE 9 licence | https://github.com/juce-framework/JUCE |
| Catch2 | 3.8.1 | Tests only (not distributed) | Boost Software License 1.0 | https://github.com/catchorg/Catch2 |

Both are downloaded at configure time (`cmake/Dependencies.cmake`); neither is stored in this
repository.

**JUCE licensing note.** Any binary built from this project links JUCE. Distributing it requires
either complying with the AGPLv3 (including releasing this project's source under compatible
terms) or holding a commercial JUCE licence. The project licence has not been chosen yet.

This project contains no firmware, ROM data, factory-preset data, logos, panel artwork or product
photography from the original hardware.
