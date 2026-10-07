#pragma once

namespace fivea
{

// The mode select switch (footswitch 6) toggles between these. The original starts in Program mode
// when powered on (SRC-001 p. 4); a new plugin instance does the same.
enum class ProgramMode
{
    Program,
    ManualEdit
};

} // namespace fivea
