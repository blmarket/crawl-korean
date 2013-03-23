/**
 * @file
 * @brief Version (and revision) functionality.
**/

#include "AppHdr.h"

#include "version.h"
#include "build.h"
#include "compflag.h"
#include "libutil.h"

namespace Version
{
    const char* Major          = CRAWL_VERSION_MAJOR;
    const char* Short          = CRAWL_VERSION_SHORT;
    const char* Long           = CRAWL_VERSION_LONG;
    const rel_type ReleaseType = CRAWL_VERSION_RELEASE;
}

#if defined(__GNUC__) && defined(__VERSION__)
 #define COMPILER "GCC " __VERSION__
#elif defined(__GNUC__)
 #define COMPILER "GCC (unknown version)"
#elif defined(TARGET_COMPILER_MINGW)
 #define COMPILER "MINGW"
#elif defined(TARGET_COMPILER_CYGWIN)
 #define COMPILER "CYGWIN"
#elif defined(TARGET_COMPILER_VC)
 #define COMPILER "Visual C++"
#elif defined(TARGET_COMPILER_ICC)
 #define COMPILER "Intel C++"
#else
 #define COMPILER "Unknown compiler"
#endif

const char* compilation_info =
    "Compiled with " COMPILER " on " __DATE__ " at " __TIME__ "\n"
    "Build platform: " CRAWL_HOST "\n"
    "Platform: " CRAWL_ARCH "\n"
    "CFLAGS: " CRAWL_CFLAGS "\n"
    "LDFLAGS: " CRAWL_LDFLAGS "\n";
