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
    /** Below is a list of officially supported ID3 tags
        which various codecs support, like OggVorbis and FLAC.

        @see https://sno.phy.queensu.ca/~phil/exiftool/TagNames/ID3.html

        @see OggVorbisAudioFormat, FlacAudioFormat
    */
    namespace ID3Tags
    {
        /** @returns a list of supported ID3v1 tags. */
        const StringArray& getV1();

        /** @returns a list of supported ID3v1 Enhanced tags. */
        const StringArray& getV1Enhanced();

        /** @returns a list of supported ID3v2.2 tags. */
        const StringArray& getV2dot2();

        /** @returns a list of supported ID3v2.3 tags. */
        const StringArray& getV2dot3();

        /** @returns a list of supported ID3v2.4 tags. */
        const StringArray& getV2dot4();

        /** @returns true if the given tag is a supported ID3 tag. */
        bool isID3Tag (StringRef tag);

        //==============================================================================
       #if ! DOXYGEN
        /** Creates a case-sensitive ID3 vX.Y tag. */
        #define JUCE_DEFINE_ID3_TAG(name, s) \
            const char* const name = s;
       #endif

        namespace v1
        {
            JUCE_DEFINE_ID3_TAG (title, "Title")
            JUCE_DEFINE_ID3_TAG (artist, "Artist")
            JUCE_DEFINE_ID3_TAG (album, "Album")
            JUCE_DEFINE_ID3_TAG (year, "Year")
            JUCE_DEFINE_ID3_TAG (comment, "Comment")
            JUCE_DEFINE_ID3_TAG (track, "Track")
            JUCE_DEFINE_ID3_TAG (genre, "Genre")
        }

        //==============================================================================
        namespace v1Enhanced
        {
            JUCE_DEFINE_ID3_TAG (title2, "Title2")
            JUCE_DEFINE_ID3_TAG (artist2, "Artist2")
            JUCE_DEFINE_ID3_TAG (album2, "Album2")
            JUCE_DEFINE_ID3_TAG (speed, "Speed")
            JUCE_DEFINE_ID3_TAG (startTime, "StartTime")
            JUCE_DEFINE_ID3_TAG (endTime, "EndTime")
        }

        //==============================================================================
        namespace v22
        {
            JUCE_DEFINE_ID3_TAG (album, "TAL")
            JUCE_DEFINE_ID3_TAG (albumArtistSortOrder, "TS2")
            JUCE_DEFINE_ID3_TAG (albumSortOrder, "TSA")
            JUCE_DEFINE_ID3_TAG (artist, "TP1")
            JUCE_DEFINE_ID3_TAG (artistURL, "WAR")
            JUCE_DEFINE_ID3_TAG (band, "TP2")
            JUCE_DEFINE_ID3_TAG (beatsPerMinute, "TBP")
            JUCE_DEFINE_ID3_TAG (comment, "COM")
            JUCE_DEFINE_ID3_TAG (commercialURL, "WCM")
            JUCE_DEFINE_ID3_TAG (compilation, "TCP")
            JUCE_DEFINE_ID3_TAG (composer, "TCM")
            JUCE_DEFINE_ID3_TAG (composerSortOrder, "TSC")
            JUCE_DEFINE_ID3_TAG (conductor, "TP3")
            JUCE_DEFINE_ID3_TAG (copyright, "TCR")
            JUCE_DEFINE_ID3_TAG (copyrightURL, "WCP")
            JUCE_DEFINE_ID3_TAG (date, "TDA")
            JUCE_DEFINE_ID3_TAG (encodedBy, "TEN")
            JUCE_DEFINE_ID3_TAG (encoderSettings, "TSS")
            JUCE_DEFINE_ID3_TAG (fileType, "TFT")
            JUCE_DEFINE_ID3_TAG (fileURL, "WAF")
            JUCE_DEFINE_ID3_TAG (genre, "TCO")
            JUCE_DEFINE_ID3_TAG (grouping, "TT1")
            JUCE_DEFINE_ID3_TAG (initialKey, "TKE")
            JUCE_DEFINE_ID3_TAG (interpretedBy, "TP4")
            JUCE_DEFINE_ID3_TAG (involvedPeople, "IPL")
            JUCE_DEFINE_ID3_TAG (ISRC, "TRC")
            JUCE_DEFINE_ID3_TAG (iTunesU, "ITU")
            JUCE_DEFINE_ID3_TAG (language, "TLA")
            JUCE_DEFINE_ID3_TAG (length, "TLE")
            JUCE_DEFINE_ID3_TAG (lyricist, "TXT")
            JUCE_DEFINE_ID3_TAG (lyrics, "ULT")
            JUCE_DEFINE_ID3_TAG (media, "TMT")
            JUCE_DEFINE_ID3_TAG (originalAlbum, "TOT")
            JUCE_DEFINE_ID3_TAG (originalArtist, "TOA")
            JUCE_DEFINE_ID3_TAG (originalFileName, "TOF")
            JUCE_DEFINE_ID3_TAG (originalLyricist, "TOL")
            JUCE_DEFINE_ID3_TAG (originalReleaseYear, "TOR")
            JUCE_DEFINE_ID3_TAG (partOfSet, "TPA")
            JUCE_DEFINE_ID3_TAG (performerSortOrder, "TSP")
            JUCE_DEFINE_ID3_TAG (picture, "PIC")
            JUCE_DEFINE_ID3_TAG (pictureDescription, "PIC-3")
            JUCE_DEFINE_ID3_TAG (pictureFormat, "PIC-1")
            JUCE_DEFINE_ID3_TAG (pictureType, "PIC-2")
            JUCE_DEFINE_ID3_TAG (playCounter, "CNT")
            JUCE_DEFINE_ID3_TAG (playlistDelay, "TDY")
            JUCE_DEFINE_ID3_TAG (podcast, "PCS")
            JUCE_DEFINE_ID3_TAG (popularimeter, "POP")
            JUCE_DEFINE_ID3_TAG (publisher, "TPB")
            JUCE_DEFINE_ID3_TAG (publisherURL, "WPB")
            JUCE_DEFINE_ID3_TAG (recordingDates, "TRD")
            JUCE_DEFINE_ID3_TAG (relativeVolumeAdjustment, "RVA")
            JUCE_DEFINE_ID3_TAG (size, "TSI")
            JUCE_DEFINE_ID3_TAG (sourceURL, "WAS")
            JUCE_DEFINE_ID3_TAG (subtitle, "TT3")
            JUCE_DEFINE_ID3_TAG (synLyrics, "SLT")
            JUCE_DEFINE_ID3_TAG (synchronisedLyricsDescription, "SynchronizedLyricsDescription")
            JUCE_DEFINE_ID3_TAG (synchronisedLyricsText, "SynchronizedLyricsText")
            JUCE_DEFINE_ID3_TAG (synchronisedLyricsType, "SynchronizedLyricsType")
            JUCE_DEFINE_ID3_TAG (time, "TIM")
            JUCE_DEFINE_ID3_TAG (title, "TT2")
            JUCE_DEFINE_ID3_TAG (titleSortOrder, "TST")
            JUCE_DEFINE_ID3_TAG (track, "TRK")
            JUCE_DEFINE_ID3_TAG (userDefinedText, "TXX")
            JUCE_DEFINE_ID3_TAG (userDefinedURL, "WXX")
            JUCE_DEFINE_ID3_TAG (year, "TYE")
        }

        //==============================================================================
        namespace v23
        {
            JUCE_DEFINE_ID3_TAG (album, "TALB")
            JUCE_DEFINE_ID3_TAG (albumArtistSortOrder, "TSO2")
            JUCE_DEFINE_ID3_TAG (albumSortOrder, "XSOA")
            JUCE_DEFINE_ID3_TAG (artist, "TPE1")
            JUCE_DEFINE_ID3_TAG (artistURL, "WOAR")
            JUCE_DEFINE_ID3_TAG (band, "TPE2")
            JUCE_DEFINE_ID3_TAG (beatsPerMinute, "TBPM")
            JUCE_DEFINE_ID3_TAG (comment, "COMM")
            JUCE_DEFINE_ID3_TAG (commercialURL, "WCOM")
            JUCE_DEFINE_ID3_TAG (compilation, "TCMP")
            JUCE_DEFINE_ID3_TAG (composer, "TCOM")
            JUCE_DEFINE_ID3_TAG (composerSortOrder, "TSOC")
            JUCE_DEFINE_ID3_TAG (conductor, "TPE3")
            JUCE_DEFINE_ID3_TAG (copyright, "TCOP")
            JUCE_DEFINE_ID3_TAG (copyrightURL, "WCOP")
            JUCE_DEFINE_ID3_TAG (date, "TDAT")
            JUCE_DEFINE_ID3_TAG (encodedBy, "TENC")
            JUCE_DEFINE_ID3_TAG (encoderSettings, "TSSE")
            JUCE_DEFINE_ID3_TAG (fileOwner, "TOWN")
            JUCE_DEFINE_ID3_TAG (fileType, "TFLT")
            JUCE_DEFINE_ID3_TAG (fileURL, "WOAF")
            JUCE_DEFINE_ID3_TAG (genre, "TCON")
            JUCE_DEFINE_ID3_TAG (grouping, "TIT1")
            JUCE_DEFINE_ID3_TAG (initialKey, "TKEY")
            JUCE_DEFINE_ID3_TAG (internetRadioStationName, "TRSN")
            JUCE_DEFINE_ID3_TAG (internetRadioStationOwner, "TRSO")
            JUCE_DEFINE_ID3_TAG (internetRadioStationURL, "WORS")
            JUCE_DEFINE_ID3_TAG (interpretedBy, "TPE4")
            JUCE_DEFINE_ID3_TAG (involvedPeople, "IPLS")
            JUCE_DEFINE_ID3_TAG (ISRC, "TSRC")
            JUCE_DEFINE_ID3_TAG (iTunesU, "ITNU")
            JUCE_DEFINE_ID3_TAG (language, "TLAN")
            JUCE_DEFINE_ID3_TAG (length, "TLEN")
            JUCE_DEFINE_ID3_TAG (lyricist, "TEXT")
            JUCE_DEFINE_ID3_TAG (lyrics, "USLT")
            JUCE_DEFINE_ID3_TAG (media, "TMED")
            JUCE_DEFINE_ID3_TAG (musicCDIdentifier, "MCDI")
            JUCE_DEFINE_ID3_TAG (olympusDSS, "XOLY")
            JUCE_DEFINE_ID3_TAG (originalAlbum, "TOAL")
            JUCE_DEFINE_ID3_TAG (originalArtist, "TOPE")
            JUCE_DEFINE_ID3_TAG (originalFileName, "TOFN")
            JUCE_DEFINE_ID3_TAG (originalLyricist, "TOLY")
            JUCE_DEFINE_ID3_TAG (originalReleaseTime, "XDOR")
            JUCE_DEFINE_ID3_TAG (originalReleaseYear, "TORY")
            JUCE_DEFINE_ID3_TAG (ownership, "OWNE")
            JUCE_DEFINE_ID3_TAG (partOfSet, "TPOS")
            JUCE_DEFINE_ID3_TAG (paymentURL, "WPAY")
            JUCE_DEFINE_ID3_TAG (performerSortOrder, "XSOP")
            JUCE_DEFINE_ID3_TAG (picture, "APIC")
            JUCE_DEFINE_ID3_TAG (pictureDescription, "APIC-3")
            JUCE_DEFINE_ID3_TAG (pictureMIMEType, "APIC-1")
            JUCE_DEFINE_ID3_TAG (pictureType, "APIC-2")
            JUCE_DEFINE_ID3_TAG (playCounter, "PCNT")
            JUCE_DEFINE_ID3_TAG (playlistDelay, "TDLY")
            JUCE_DEFINE_ID3_TAG (podcast, "PCST")
            JUCE_DEFINE_ID3_TAG (podcastCategory, "TCAT")
            JUCE_DEFINE_ID3_TAG (podcastDescription, "TDES")
            JUCE_DEFINE_ID3_TAG (podcastID, "TGID")
            JUCE_DEFINE_ID3_TAG (podcastKeywords, "TKWD")
            JUCE_DEFINE_ID3_TAG (podcastURL, "WFED")
            JUCE_DEFINE_ID3_TAG (popularimeter, "POPM")
            JUCE_DEFINE_ID3_TAG (privateInfo, "PRIV")
            JUCE_DEFINE_ID3_TAG (publisher, "TPUB")
            JUCE_DEFINE_ID3_TAG (publisherURL, "WPUB")
            JUCE_DEFINE_ID3_TAG (recordingDates, "TRDA")
            JUCE_DEFINE_ID3_TAG (size, "TSIZ")
            JUCE_DEFINE_ID3_TAG (sourceURL, "WOAS")
            JUCE_DEFINE_ID3_TAG (subtitle, "TIT3")
            JUCE_DEFINE_ID3_TAG (synLyrics, "SYLT")
            JUCE_DEFINE_ID3_TAG (termsOfUse, "USER")
            JUCE_DEFINE_ID3_TAG (time, "TIME")
            JUCE_DEFINE_ID3_TAG (title, "TIT2")
            JUCE_DEFINE_ID3_TAG (titleSortOrder, "XSOT")
            JUCE_DEFINE_ID3_TAG (track, "TRCK")
            JUCE_DEFINE_ID3_TAG (userDefinedText, "TXXX")
            JUCE_DEFINE_ID3_TAG (userDefinedURL, "WXXX")
            JUCE_DEFINE_ID3_TAG (year, "TYER")
        }

        //==============================================================================
        namespace v24
        {
	        JUCE_DEFINE_ID3_TAG (albumSortOrder, "TSOA")
	        JUCE_DEFINE_ID3_TAG (encodingTime, "TDEN")
	        JUCE_DEFINE_ID3_TAG (involvedPeople, "TIPL")
	        JUCE_DEFINE_ID3_TAG (mood, "TMOO")
	        JUCE_DEFINE_ID3_TAG (musicianCredits, "TMCL")
	        JUCE_DEFINE_ID3_TAG (olympusDSS, "XOLY")
	        JUCE_DEFINE_ID3_TAG (originalReleaseTime, "TDOR")
	        JUCE_DEFINE_ID3_TAG (performerSortOrder, "TSOP")
	        JUCE_DEFINE_ID3_TAG (producedNotice, "TPRO")
	        JUCE_DEFINE_ID3_TAG (recordingTime, "TDRC")
	        JUCE_DEFINE_ID3_TAG (relativeVolumeAdjustment, "RVA2")
	        JUCE_DEFINE_ID3_TAG (releaseTime, "TDRL")
	        JUCE_DEFINE_ID3_TAG (setSubtitle, "TSST")
	        JUCE_DEFINE_ID3_TAG (taggingTime, "TDTG")
	        JUCE_DEFINE_ID3_TAG (titleSortOrder, "TSOT")
        }

        #undef JUCE_DEFINE_ID3_TAG
    }
}
