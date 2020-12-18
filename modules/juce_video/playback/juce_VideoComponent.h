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

#ifndef JUCE_VIDEOCOMPONENT_H_INCLUDED
#define JUCE_VIDEOCOMPONENT_H_INCLUDED

namespace juce
{

//==============================================================================
/**
    A component that can play a movie.

    Use the load() method to open a video once you've added this component to
    a parent (or put it on the desktop).

    @tags{Video}
*/
class JUCE_API  VideoComponent  : public Component,
                                  private Timer
{
public:
    //==============================================================================
    /** Creates an empty VideoComponent.

        Use the loadAsync() or load() method to open a video once you've added
        this component to a parent (or put it on the desktop).

        If useNativeControlsIfAvailable is enabled and a target OS has a video view with
        dedicated controls for transport etc, that view will be used. In opposite
        case a bare video view without any controls will be presented, allowing you to
        tailor your own UI. Currently this flag is used on iOS and 64bit macOS.
        Android, Windows and 32bit macOS will always use plain video views without
        dedicated controls.
    */
    VideoComponent (bool useNativeControlsIfAvailable);

    /** Destructor. */
    ~VideoComponent() override;

    //==============================================================================
    /** Tries to load a video from a local file.

        This function is supported on macOS and Windows. For iOS and Android, use
        loadAsync() instead.

        @returns an error if the file failed to be loaded correctly

        @see loadAsync
    */
    Result load (const File& file);

    /** Tries to load a video from a URL.

        This function is supported on macOS and Windows. For iOS and Android, use
        loadAsync() instead.

        @returns an error if the file failed to be loaded correctly

        @see loadAsync
    */
    Result load (const URL& url);

    /** Tries to load a video from a URL asynchronously. When finished, invokes the
        callback supplied to the function on the message thread.

        This is the preferred way of loading content, since it works not only on
        macOS and Windows, but also on iOS and Android. On Windows, it will internally
        call load().

        @see load
     */
    void loadAsync (const URL& url, std::function<void (const URL&, Result)> loadFinishedCallback);

    /** Closes the video and resets the component. */
    void closeVideo();

    /** Returns true if a video is currently open. */
    bool isVideoOpen() const;

    /** Returns the last file that was loaded.
        If nothing is open, or if it was a URL rather than a file, this will return File().
    */
    File getCurrentVideoFile() const;

    /** Returns the last URL that was loaded.
        If nothing is open, or if it was a file rather than a URL, this will return URL().
    */
    URL getCurrentVideoURL() const;

    //==============================================================================
    /** Returns the length of the video, in seconds. */
    double getVideoDuration() const;

    /** Returns the video's natural size, in pixels.
        If no video is loaded, an empty rectangle will be returned.
    */
    Rectangle<int> getVideoNativeSize() const;

    /** Starts the video playing. */
    void play();

    /** Stops the video playing. */
    void stop();

    /** Returns true if the video is currently playing. */
    bool isPlaying() const;

    /** Sets the video's position to a given time. */
    void setPlayPosition (double newPositionSeconds);

    /** Returns the current play position of the video. */
    double getPlayPosition() const;

    /** Changes the video playback rate.
        A value of 1.0 is normal speed, greater values will play faster, smaller
        values play more slowly.
    */
    void setPlaySpeed (double newSpeed);

    /** Returns the current play speed of the video. */
    double getPlaySpeed() const;

    /** Changes the video's playback volume.
        @param newVolume    the volume in the range 0 (silent) to 1.0 (full)
    */
    void setAudioVolume (float newVolume);

    /** Returns the video's playback volume.
        @returns the volume in the range 0 (silent) to 1.0 (full)
    */
    float getAudioVolume() const;

   #ifdef JUCE_SYNC_VIDEO_VOLUME_WITH_OS_MEDIA_VOLUME
    /** Set this callback to be notified whenever OS global media volume changes.
        Currently used on Android only.
     */
    std::function<void()> onGlobalMediaVolumeChanged;
   #endif

    /** Set this callback to be notified whenever the playback starts. */
    std::function<void()> onPlaybackStarted;

    /** Set this callback to be notified whenever the playback stops. */
    std::function<void()> onPlaybackStopped;

    /** Set this callback to be notified whenever an error occurs. Upon error, you
        may need to load the video again. */
    std::function<void (const String& /*error*/)> onErrorOccurred;

private:
    //==============================================================================
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    void resized() override;
    void timerCallback() override;

    template<class FileOrURL>
    Result loadInternal (const FileOrURL&, bool);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VideoComponent)
};


#endif

} // namespace juce
