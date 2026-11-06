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

#if defined(_M_ARM64EC)
  #error JUCE_ARCH arm64ec
#elif defined(_M_ARM64) || defined(__aarch64__) || defined(__ARM64__)
  #error JUCE_ARCH aarch64
#elif (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM == 8) || defined(__ARMv8__) || defined(__ARMv8_A__)
  #error JUCE_ARCH armv8l
#elif (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM == 7) || defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) || defined(_ARM_ARCH_7) || defined(__CORE_CORTEXA__)
  #error JUCE_ARCH armv7l
#elif (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM == 6) || defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6T2__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6M__)
  #error JUCE_ARCH armv6l
#elif (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM == 5) || defined(__ARM_ARCH_5TEJ__)
  #error JUCE_ARCH armv5l
#elif defined(__arm__) || defined(_M_ARM)
  #error JUCE_ARCH arm

#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)

  #error JUCE_ARCH i386

#elif defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)

  #error JUCE_ARCH x86_64

#elif defined(__ia64) || defined(__ia64__) || defined(_M_IA64)

  #error JUCE_ARCH ia64

#elif defined(__mips) || defined(__mips__) || defined(_M_MRX000)

  #if defined(_MIPS_ARCH_MIPS64) || defined(__mips64)
    #error JUCE_ARCH mips64
  #else
    #error JUCE_ARCH mips
  #endif

#elif defined(__ppc__) || defined(__ppc) || defined(__powerpc__) || defined(_ARCH_COM) || defined(_ARCH_PWR) || defined(_ARCH_PPC) || defined(_M_MPPC) || defined(_M_PPC)

  #if defined(__ppc64__) || defined(__powerpc64__) || defined(__64BIT__)
    #ifdef __LITTLE_ENDIAN__
      #error JUCE_ARCH ppc64le
    #else
      #error JUCE_ARCH ppc64
    #endif
  #else
    #error JUCE_ARCH ppc
  #endif

#elif defined(__riscv)

  #if __riscv_xlen == 64
    #error JUCE_ARCH riscv64
  #else
    #error JUCE_ARCH riscv
  #endif

#else

  #error JUCE_ARCH unknown

#endif
