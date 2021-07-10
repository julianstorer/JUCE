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
    namespace VorbisComment
    {
        const StringArray& getAll()
        {
            static const char* c[] =
            {
                VorbisComment::title,
                VorbisComment::version,
                VorbisComment::album,
                VorbisComment::trackNumber,
                VorbisComment::artist,
                VorbisComment::artist,
                VorbisComment::performer,
                VorbisComment::copyright,
                VorbisComment::license,
                VorbisComment::organisation,
                VorbisComment::description,
                VorbisComment::genre,
                VorbisComment::location,
                VorbisComment::contact,
                VorbisComment::isrc,
                nullptr
            };

            static const StringArray comments (c);
            return comments;
        }

        bool isVorbisComment (StringRef tag)
        {
            return tag.isNotEmpty() && getAll().contains (tag);
        }
    }

    namespace ID3Tags
    {
        //==============================================================================
        bool isID3Tag (StringRef tag)
        {
            if (tag.isEmpty())
                return false;

            return getV1().contains (tag)
                || getV1Enhanced().contains (tag)
                || getV2dot2().contains (tag)
                || getV2dot3().contains (tag)
                || getV2dot4().contains (tag);
        }

        //==============================================================================
        const StringArray& getV1()
        {
            static const char* const t[] =
            {
                v1::title,
                v1::artist,
                v1::album,
                v1::year,
                v1::comment,
                v1::track,
                v1::genre,
                nullptr
            };

            static const StringArray tags (t);
            return tags;
        }

        //==============================================================================
        const StringArray& getV1Enhanced()
        {
            static const char* const t[] =
            {
                v1Enhanced::title2,
                v1Enhanced::artist2,
                v1Enhanced::album2,
                v1Enhanced::speed,
                v1Enhanced::startTime,
                v1Enhanced::endTime,
                nullptr
            };

            static const StringArray tags (t);
            return tags;
        }

        //==============================================================================
        const StringArray& getV2dot2()
        {
            static const char* const t[] =
            {
                v22::album,
                v22::albumArtistSortOrder,
                v22::albumSortOrder,
                v22::artist,
                v22::artistURL,
                v22::band,
                v22::beatsPerMinute,
                v22::comment,
                v22::commercialURL,
                v22::compilation,
                v22::composer,
                v22::composerSortOrder,
                v22::conductor,
                v22::copyright,
                v22::copyrightURL,
                v22::date,
                v22::encodedBy,
                v22::encoderSettings,
                v22::fileType,
                v22::fileURL,
                v22::genre,
                v22::grouping,
                v22::initialKey,
                v22::interpretedBy,
                v22::involvedPeople,
                v22::ISRC,
                v22::iTunesU,
                v22::language,
                v22::length,
                v22::lyricist,
                v22::lyrics,
                v22::media,
                v22::originalAlbum,
                v22::originalArtist,
                v22::originalFileName,
                v22::originalLyricist,
                v22::originalReleaseYear,
                v22::partOfSet,
                v22::performerSortOrder,
                v22::picture,
                v22::pictureDescription,
                v22::pictureFormat,
                v22::pictureType,
                v22::playCounter,
                v22::playlistDelay,
                v22::podcast,
                v22::popularimeter,
                v22::publisher,
                v22::publisherURL,
                v22::recordingDates,
                v22::relativeVolumeAdjustment,
                v22::size,
                v22::sourceURL,
                v22::subtitle,
                v22::synLyrics,
                v22::synchronisedLyricsDescription,
                v22::synchronisedLyricsText,
                v22::synchronisedLyricsType,
                v22::time,
                v22::title,
                v22::titleSortOrder,
                v22::track,
                v22::userDefinedText,
                v22::userDefinedURL,
                v22::year,
                nullptr
            };

            static const StringArray tags (t);
            return tags;
        }

        //==============================================================================
        const StringArray& getV2dot3()
        {
            static const char* const t[] =
            {
                v23::album,
                v23::albumArtistSortOrder,
                v23::albumSortOrder,
                v23::artist,
                v23::artistURL,
                v23::band,
                v23::beatsPerMinute,
                v23::comment,
                v23::commercialURL,
                v23::compilation,
                v23::composer,
                v23::composerSortOrder,
                v23::conductor,
                v23::copyright,
                v23::copyrightURL,
                v23::date,
                v23::encodedBy,
                v23::encoderSettings,
                v23::fileOwner,
                v23::fileType,
                v23::fileURL,
                v23::genre,
                v23::grouping,
                v23::initialKey,
                v23::internetRadioStationName,
                v23::internetRadioStationOwner,
                v23::internetRadioStationURL,
                v23::interpretedBy,
                v23::involvedPeople,
                v23::ISRC,
                v23::iTunesU,
                v23::language,
                v23::length,
                v23::lyricist,
                v23::lyrics,
                v23::media,
                v23::musicCDIdentifier,
                v23::olympusDSS,
                v23::originalAlbum,
                v23::originalArtist,
                v23::originalFileName,
                v23::originalLyricist,
                v23::originalReleaseTime,
                v23::originalReleaseYear,
                v23::ownership,
                v23::partOfSet,
                v23::paymentURL,
                v23::performerSortOrder,
                v23::picture,
                v23::pictureDescription,
                v23::pictureMIMEType,
                v23::pictureType,
                v23::playCounter,
                v23::playlistDelay,
                v23::podcast,
                v23::podcastCategory,
                v23::podcastDescription,
                v23::podcastID,
                v23::podcastKeywords,
                v23::podcastURL,
                v23::popularimeter,
                v23::privateInfo,
                v23::publisher,
                v23::publisherURL,
                v23::recordingDates,
                v23::size,
                v23::sourceURL,
                v23::subtitle,
                v23::synLyrics,
                v23::termsOfUse,
                v23::time,
                v23::title,
                v23::titleSortOrder,
                v23::track,
                v23::userDefinedText,
                v23::userDefinedURL,
                v23::year,
                nullptr
            };

            static const StringArray tags (t);
            return tags;
        }

        //==============================================================================
        const StringArray& getV2dot4()
        {
            static const char* const t[] =
            {
                v24::encodingTime,
                v24::involvedPeople,
                v24::mood,
                v24::musicianCredits,
                v24::olympusDSS,
                v24::performerSortOrder,
                v24::producedNotice,
                v24::recordingTime,
                v24::relativeVolumeAdjustment,
                v24::releaseTime,
                v24::setSubtitle,
                v24::taggingTime,
                nullptr
            };

            static const StringArray tags (t);
            return tags;
        }
    }
}
