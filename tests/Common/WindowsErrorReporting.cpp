// On Windows, the Debug C runtime reports a failed assert() or a checked-iterator error with a modal
// dialog box, and abort() opens another. On a CI runner nobody answers them, so the test run waits
// until the job is cancelled. This makes both print to stderr and end the process instead, so the
// message (and the failing test) appear in the log.

#if defined(_WIN32)
#include <crtdbg.h>
#include <cstdlib>

namespace
{
const bool reportErrorsToConsole = []
{
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    // A plain array: a braced list here would need <initializer_list>, which these headers do not
    // bring in.
    const int reportTypes[] = {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT};
    for (const int type : reportTypes)
    {
        _CrtSetReportMode(type, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
        _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
    }
    return true;
}();
} // namespace
#endif
