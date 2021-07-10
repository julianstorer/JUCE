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

#ifdef JUCE_AUDIO_FORMATS_H_INCLUDED
 /* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
 #error "Incorrect use of JUCE cpp file"
#endif

#define JUCE_CORE_INCLUDE_COM_SMART_PTR 1
#define JUCE_CORE_INCLUDE_JNI_HELPERS 1
#define JUCE_CORE_INCLUDE_NATIVE_HEADERS 1

#include "juce_audio_formats.h"

//==============================================================================
#if JUCE_MAC
 #include <AudioToolbox/AudioToolbox.h>

#elif JUCE_IOS
 #import <AudioToolbox/AudioToolbox.h>
 #import <AVFoundation/AVFoundation.h>

//==============================================================================
#elif JUCE_WINDOWS && JUCE_USE_WINDOWS_MEDIA_FORMAT
 #include <wmsdk.h>
#endif

//==============================================================================
#if ! DOXYGEN
namespace juce
{
    inline void setVorbisOrID3MetadataValue (StringPairArray& metadata, StringRef entry)
    {
        if (entry.isEmpty())
            return;

        const auto e = String (entry).trim();
        if (e.isEmpty())
            return;

        //NB: all standardised entries are supposed to be formatted as "TAG=TEXT".
        const auto tag = e.upToFirstOccurrenceOf ("=", false, true).trim();
        const auto value = e.fromFirstOccurrenceOf ("=", false, true).trim();

        if (VorbisComment::isVorbisComment (tag) || ID3Tags::isID3Tag (tag))
        {
            if (value.isNotEmpty())
            {
                if (metadata.containsKey (tag))
                    metadata.set (tag, metadata[tag] + " " + value); //NB: This bit appends the value in the event of duplicate entries being found.
                else
                    metadata.set (tag, value);
            }
        }
        else
        {
            if (tag != value)
                metadata.set (tag, value); //Mercifully set the provided tag even if it's an unofficial one.
        }
    }
}
#endif

//==============================================================================
#include "format/juce_AudioFormat.cpp"
#include "format/juce_AudioFormatManager.cpp"
#include "format/juce_AudioFormatReader.cpp"
#include "format/juce_AudioFormatReaderSource.cpp"
#include "format/juce_AudioFormatWriter.cpp"
#include "format/juce_AudioSubsectionReader.cpp"
#include "format/juce_BufferingAudioFormatReader.cpp"
#include "sampler/juce_Sampler.cpp"
#include "codecs/juce_AiffAudioFormat.cpp"
#include "codecs/juce_CoreAudioFormat.cpp"
#include "codecs/juce_FlacAudioFormat.cpp"
#include "codecs/juce_ID3Tags.cpp"
#include "codecs/juce_MP3AudioFormat.cpp"
#include "codecs/juce_OggVorbisAudioFormat.cpp"
#include "codecs/juce_WavAudioFormat.cpp"
#include "codecs/juce_LAMEEncoderAudioFormat.cpp"

#if JUCE_WINDOWS && JUCE_USE_WINDOWS_MEDIA_FORMAT
 #include "codecs/juce_WindowsMediaAudioFormat.cpp"
#endif
