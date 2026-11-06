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

namespace juce
{

//==============================================================================
/** The type used for OSC type tags. */
using OSCType = char;


/** The type used for OSC type tag strings. */
using OSCTypeList = Array<OSCType>;

//==============================================================================
/** The definitions of supported OSC types and their associated OSC type tags,
    as defined in the OpenSoundControl 1.0 specification.

    Note: This implementation does not support any additional type tags that
    are not part of the specification.

    @tags{OSC}
*/
class JUCE_API  OSCTypes
{
public:
    static const OSCType int32;
    static const OSCType int64;
    static const OSCType float32;
    static const OSCType double64;
    static const OSCType string;
    static const OSCType blob;
    static const OSCType colour;
    static const OSCType nil;
    static const OSCType impulse;
    static const OSCType T;
    static const OSCType F;

    static bool isSupportedType (OSCType type) noexcept
    {
        return type == OSCTypes::int32
            || type == OSCTypes::int64
            || type == OSCTypes::float32
            || type == OSCTypes::double64
            || type == OSCTypes::string
            || type == OSCTypes::blob
            || type == OSCTypes::colour
            || type == OSCTypes::nil
            || type == OSCTypes::impulse
            || type == OSCTypes::T
            || type == OSCTypes::F;
    }
};


//==============================================================================
/**
    Holds a 32-bit RGBA colour for passing to and from an OSCArgument.
    @see OSCArgument, OSCTypes::colour
    @tags{OSC}
*/
struct OSCColour
{
    uint8 red, green, blue, alpha;

    uint32 toInt32() const;
    static OSCColour fromInt32 (uint32);
};


//==============================================================================
/** Base class for exceptions that can be thrown by methods in the OSC module.

    @tags{OSC}
*/
struct OSCException  : public std::exception
{
    OSCException (const String& desc)
        : description (desc)
    {
       #if ! JUCE_UNIT_TESTS
        DBG ("OSCFormatError: " + description);
       #endif
    }

    String description;
};

//==============================================================================
/** Exception type thrown when the OSC module fails to parse something because
    of a data format not compatible with the OpenSoundControl 1.0 specification.

    @tags{OSC}
*/
struct OSCFormatError : public OSCException
{
    OSCFormatError (const String& desc) : OSCException (desc) {}
};

//==============================================================================
/** Exception type thrown in cases of unexpected errors in the OSC module.

    Note: This should never happen, and all the places where this is thrown
    should have a preceding jassertfalse to facilitate debugging.

    @tags{OSC}
*/
struct OSCInternalError : public OSCException
{
    OSCInternalError (const String& desc) : OSCException (desc) {}
};

} // namespace juce
