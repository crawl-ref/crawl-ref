/**
 * @file
 * @brief Header file for MSVC compiles
**/

#ifndef __msvc_h
#define __msvc_h

#if defined(TARGET_COMPILER_VC)

#define __STDC_FORMAT_MACROS
//now included via tweaking include directories
//#include "MSVC/inttypes.h"

#include <io.h>
#include <math.h>
#include <chrono>
#include <thread>

#define fileno _fileno
#define snprintf _snprintf
#define strdup _strdup
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strnicmp _strnicmp
#define ftruncate _chsize
#define putenv _putenv
#define strtoll _strtoi64

// These are not defined in MSVC version of stat.h
#define        S_IWUSR        S_IWRITE
#define        S_IRUSR        S_IREAD

#pragma warning(disable : 4290)
#pragma warning(disable : 4351)
// bool -> int
#pragma warning(disable : 4800)

// struct vs class XXX: fix these some day!
#pragma warning(disable : 4099)

// truncating conversions XXX: fix these too!
#pragma warning(disable : 4244)

// POSIX deprecation warnings
#pragma warning(disable : 4996)

static inline double round(double x)
{
    if (x >= 0.0)
        return floor(x + 0.5);
    else
        return ceil(x - 0.5);
}

static inline double sqrt(int x)
{
    return sqrt((double)x);
}

static inline double atan2(int x, int y)
{
    return atan2((double)x, (double)y);
}

static inline double log(int x)
{
    return log((double)x);
}

static inline double log2(double n)
{
    return log(n) / log(2.0);
}

static inline void usleep(unsigned int usec)
{
	std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

//this is targeting for struct member name in store.h, nothing else gets affected as of 0.9.0
#define _int64 var_int64

//missing in sys/types.h
#define mode_t unsigned short

typedef ptrdiff_t ssize_t;

#endif /* defined(TARGET_COMPILER_VC) */

#endif
