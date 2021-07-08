/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/


/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.md file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 juce_osc
  vendor:             juce
  version:            6.0.8
  name:               JUCE OSC classes
  description:        Open Sound Control implementation.
  website:            http://www.juce.com/juce
  license:            GPL/Commercial
  minimumCppStandard: 14

  dependencies:       juce_events

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/


#pragma once
#define JUCE_OSC_H_INCLUDED

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

/** Config: JUCE_ALLOW_SPECIAL_CHARS_IN_ADDRESS
	Enables the use of characters in adress that are not allowed by the OSC specifications (like spaces), but that are used
	by some applications anyway (e.g. /my spaced/address)
*/
#ifndef JUCE_ALLOW_SPECIAL_CHARS_IN_ADDRESS
#define JUCE_ALLOW_SPECIAL_CHARS_IN_ADDRESS 0
#endif

//==============================================================================
#include "osc/juce_OSCTypes.h"
#include "osc/juce_OSCTimeTag.h"
#include "osc/juce_OSCArgument.h"
#include "osc/juce_OSCAddress.h"
#include "osc/juce_OSCMessage.h"
#include "osc/juce_OSCBundle.h"
#include "osc/juce_OSCReceiver.h"
#include "osc/juce_OSCSender.h"
