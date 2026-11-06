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


/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.md file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 juce_osc
  vendor:             juce
  version:            9.0.6
  name:               JUCE OSC classes
  description:        Open Sound Control implementation.
  website:            http://www.juce.com/juce
  license:            AGPLv3/Commercial
  minimumCppStandard: 17

  dependencies:       juce_events

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/


#pragma once
#define JUCE_OSC_H_INCLUDED

/** Config: JUCE_ALLOW_SPECIAL_CHARS_IN_ADDRESS
	Enables the use of characters in adress that are not allowed by the OSC specifications (like spaces), but that are used
	by some applications anyway (e.g. /my spaced/address)
*/
#ifndef JUCE_ALLOW_SPECIAL_CHARS_IN_ADDRESS 
#define JUCE_ALLOW_SPECIAL_CHARS_IN_ADDRESS 0 
#endif 

/** Config: JUCE_ENABLE_BROADCAST_BY_DEFAULT
	Automatically enables broadcast on bound port in OSCReceiver
*/
#ifndef JUCE_ENABLE_BROADCAST_BY_DEFAULT 
#define JUCE_ENABLE_BROADCAST_BY_DEFAULT 0 
#endif 

/** Config: JUCE_EXCLUSIVE_BINDING_BY_DEFAULT
	If enabled, this will make the binding of this port exclusive, so no other process can bind it.
*/
#ifndef JUCE_EXCLUSIVE_BINDING_BY_DEFAULT 
#define JUCE_EXCLUSIVE_BINDING_BY_DEFAULT 0 
#endif 

/** Config: JUCE_IP_AND_PORT_DETECTION
	If enabled, this will add remoteIP and remotePort variables to osc packets, corresponding to the sender's ip and port when receiving messages.
*/
#ifndef JUCE_IP_AND_PORT_DETECTION 
#define JUCE_IP_AND_PORT_DETECTION 0 
#endif 

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

//==============================================================================
#include "osc/juce_OSCTypes.h"
#include "osc/juce_OSCTimeTag.h"
#include "osc/juce_OSCArgument.h"
#include "osc/juce_OSCAddress.h"
#include "osc/juce_OSCMessage.h"
#include "osc/juce_OSCBundle.h"
#include "osc/juce_OSCReceiver.h"
#include "osc/juce_OSCSender.h"
