/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 5 End-User License
   Agreement and JUCE 5 Privacy Policy (both updated and effective as of the
   22nd April 2020).

   End User License Agreement: www.juce.com/juce-5-licence
   Privacy Policy: www.juce.com/juce-5-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{
    /** Below is a list of officially supported metadata keys
        for Vorbis codecs like Ogg and FLAC.

        @see https://www.xiph.org/vorbis/doc/v-comment.html

        @see OggVorbisAudioFormat, FlacAudioFormat
    */
    namespace VorbisComment
    {
       #if ! DOXYGEN
        /** Creates a case-sensitive Vorbis Comment. */
        #define JUCE_DEFINE_VORBIS_COMMENT(name, s) \
            const char* const name = s;
       #endif

        JUCE_DEFINE_VORBIS_COMMENT (vendor, "VENDOR")
        JUCE_DEFINE_VORBIS_COMMENT (encoder, "ENCODER")
        JUCE_DEFINE_VORBIS_COMMENT (title, "TITLE")
        JUCE_DEFINE_VORBIS_COMMENT (version, "VERSION")
        JUCE_DEFINE_VORBIS_COMMENT (album, "ALBUM")
        JUCE_DEFINE_VORBIS_COMMENT (trackNumber, "TRACKNUMBER")
        JUCE_DEFINE_VORBIS_COMMENT (artist, "ARTIST")
        JUCE_DEFINE_VORBIS_COMMENT (performer, "PERFORMER")
        JUCE_DEFINE_VORBIS_COMMENT (copyright, "COPYRIGHT")
        JUCE_DEFINE_VORBIS_COMMENT (license, "LICENSE")
        JUCE_DEFINE_VORBIS_COMMENT (organisation, "ORGANIZATION")
        JUCE_DEFINE_VORBIS_COMMENT (description, "DESCRIPTION")
        JUCE_DEFINE_VORBIS_COMMENT (genre, "GENRE")
        JUCE_DEFINE_VORBIS_COMMENT (date, "DATE")
        JUCE_DEFINE_VORBIS_COMMENT (location, "LOCATION")
        JUCE_DEFINE_VORBIS_COMMENT (contact, "CONTACT")
        JUCE_DEFINE_VORBIS_COMMENT (isrc, "ISRC")

        #undef JUCE_DEFINE_VORBIS_COMMENT

        /** @returns the list of all supported Vorbis Comments. */
        const StringArray& getAll();

        /** @returns true if the given tag is a Vorbis Comment. */
        bool isVorbisComment (StringRef tag);
    }
}
