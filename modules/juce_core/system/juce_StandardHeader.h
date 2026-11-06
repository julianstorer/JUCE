/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#pragma once

//==============================================================================
/** Current JUCE version number.

    See also SystemStats::getJUCEVersion() for a string version.
*/
#define JUCE_MAJOR_VERSION      9
#define JUCE_MINOR_VERSION      0
#define JUCE_BUILDNUMBER        6

/** Current JUCE version number.

    Bits 16 to 32 = major version.
    Bits 8 to 16 = minor version.
    Bits 0 to 8 = point release.

    See also SystemStats::getJUCEVersion() for a string version.
*/
#define JUCE_VERSION   ((JUCE_MAJOR_VERSION << 16) + (JUCE_MINOR_VERSION << 8) + JUCE_BUILDNUMBER)

/** @cond */
#define JUCE_VERSION_ID \
    [[maybe_unused]] volatile auto juceVersionId = "juce_version_" JUCE_STRINGIFY(JUCE_MAJOR_VERSION) "_" JUCE_STRINGIFY(JUCE_MINOR_VERSION) "_" JUCE_STRINGIFY(JUCE_BUILDNUMBER);
/** @endcond */

//==============================================================================
#include "juce_CompilerSupport.h"
#include "juce_CompilerWarnings.h"

//==============================================================================
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstddef>

// There's a template specialisation to std::wstring_convert in
// wstring_convert.h that triggers a warning on Xcode 26.4.1 when used by the
// VST3 SDK. It cannot be suppressed at the call site. This file is included
// in <locale>, and would transitively be included by <functional>.
#if defined (__apple_build_version__)
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
#include <locale>
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
#endif

#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string_view>
#include <thread>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

//==============================================================================
#include "juce_PlatformDefs.h"

//==============================================================================
// Now we'll include some common OS headers.
JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4514 4245 4100)

#if JUCE_MSVC
 #include <intrin.h>
#endif


#if JUCE_MAC || JUCE_IOS
 #include <libkern/OSAtomic.h>
 #include <libkern/OSByteOrder.h>
 #include <xlocale.h>
 #include <signal.h>
#endif

#if JUCE_LINUX || JUCE_BSD
 #include <cstring>
 #include <signal.h>

 #if __INTEL_COMPILER
  #if __ia64__
   #include <ia64intrin.h>
  #else
   #include <ia32intrin.h>
  #endif
 #endif
#endif

#if JUCE_MSVC && JUCE_DEBUG
 #include <crtdbg.h>
#endif

JUCE_END_IGNORE_WARNINGS_MSVC

#if JUCE_ANDROID
 #include <cstring>
 #include <byteswap.h>
#endif

// undef symbols that are sometimes set by misguided 3rd-party headers
#undef TYPE_BOOL
#undef max
#undef min
#undef major
#undef minor
#undef KeyPress

//==============================================================================
// DLL building settings on Windows
#if JUCE_WINDOWS
 #ifdef JUCE_DLL_BUILD
  #define JUCE_API __declspec (dllexport)
  #ifdef _MSC_VER
   #pragma warning (disable: 4251)
  #endif
 #elif defined (JUCE_DLL)
  #define JUCE_API __declspec (dllimport)
  #ifdef _MSC_VER
   #pragma warning (disable: 4251)
  #endif
 #endif
 #if JUCE_MSVC && defined(__INTEL_COMPILER)
  #pragma warning (disable: 1125) // (virtual override warning)
 #endif
#elif defined (JUCE_DLL) || defined (JUCE_DLL_BUILD)
 #define JUCE_API __attribute__ ((visibility ("default")))
#endif

//==============================================================================
#ifndef JUCE_API
 /** This macro is added to all JUCE public class declarations. */
 #define JUCE_API
#endif

#if JUCE_WINDOWS && JUCE_DLL_BUILD
 #define JUCE_PUBLIC_IN_DLL_BUILD(declaration)  public: declaration; private:
#else
 #define JUCE_PUBLIC_IN_DLL_BUILD(declaration)  declaration;
#endif

/** This macro is added to all JUCE public function declarations. */
#define JUCE_PUBLIC_FUNCTION        JUCE_API JUCE_CALLTYPE

/** @cond */
#define JUCE_NAMESPACE juce  // This old macro is deprecated: you should just use the juce namespace directly.
/** @endcond */
