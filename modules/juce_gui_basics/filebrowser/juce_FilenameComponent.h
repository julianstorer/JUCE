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

//==============================================================================
/**
    Listens for events happening to a FilenameComponent.

    Use FilenameComponent::addListener() and FilenameComponent::removeListener() to
    register one of these objects for event callbacks when the filename is changed.

    @see FilenameComponent

    @tags{GUI}
*/
class JUCE_API  FilenameComponentListener
{
public:
    /** Destructor. */
    virtual ~FilenameComponentListener() = default;

    /** This method is called after the FilenameComponent's file has been changed. */
    virtual void filenameComponentChanged (FilenameComponent* fileComponentThatHasChanged) = 0;
};


//==============================================================================
/**
    Shows a filename as an editable text box, with a 'browse' button and a
    drop-down list for recently selected files.

    A handy component for dialogue boxes where you want the user to be able to
    select a file or directory.

    Attach an FilenameComponentListener using the addListener() method, and it will
    get called each time the user changes the filename, either by browsing for a file
    and clicking 'ok', or by typing a new filename into the box and pressing return.

    @see FileChooser, ComboBox

    @tags{GUI}
*/
class JUCE_API  FilenameComponent  : public Component,
                                     public SettableTooltipClient,
                                     public FileDragAndDropTarget,
                                     private AsyncUpdater
{
public:
    //==============================================================================
    /** Creates a FilenameComponent.

        @param name             the name for this component.
        @param currentFile      the file to initially show in the box
        @param canEditFilename  if true, the user can manually edit the filename; if false,
                                they can only change it by browsing for a new file
        @param isDirectory      if true, the file will be treated as a directory, and
                                an appropriate directory browser used
        @param isForSaving      if true, the file browser will allow non-existent files to
                                be picked, as the file is assumed to be used for saving rather
                                than loading
        @param fileBrowserWildcard  a wildcard pattern to use in the file browser - e.g. "*.txt;*.foo".
                                If an empty string is passed in, then the pattern is assumed to be "*"
        @param enforcedSuffix   if this is non-empty, it is treated as a suffix that will be added
                                to any filenames that are entered or chosen
        @param textWhenNothingSelected  the message to display in the box before any filename is entered. (This
                                will only appear if the initial file isn't valid)
    */
    FilenameComponent (const String& name,
                       const File& currentFile,
                       bool canEditFilename,
                       bool isDirectory,
                       bool isForSaving,
                       const String& fileBrowserWildcard,
                       const String& enforcedSuffix,
                       const String& textWhenNothingSelected);

    /** Destructor. */
    ~FilenameComponent() override;

    //==============================================================================
    /** Returns the currently displayed filename. */
    File getCurrentFile() const;

    /** Returns the raw text that the user has entered. */
    String getCurrentFileText() const;

    /** Changes the current filename.

        @param newFile                the new filename to use
        @param addToRecentlyUsedList  if true, the filename will also be added to the
                                      drop-down list of recent files.
        @param notification           whether to send a notification of the change to listeners.
                                      A notification will only be sent if the filename has changed.
    */
    void setCurrentFile (File newFile,
                         bool addToRecentlyUsedList,
                         NotificationType notification = sendNotificationAsync);

    /** Changes whether the use can type into the filename box.
    */
    void setFilenameIsEditable (bool shouldBeEditable);

    /** Sets a file or directory to be the default starting point for the browser to show.

        This is only used if the current file hasn't been set.
    */
    void setDefaultBrowseTarget (const File& newDefaultDirectory);

    /** This can be overridden to return a custom location that you want the dialog box
        to show when the browse button is pushed.
        The default implementation of this method will return either the current file
        (if one has been chosen) or the location that was set by setDefaultBrowseTarget().
    */
    virtual File getLocationToBrowse();

    /** Returns all the entries on the recent files list.

        This can be used in conjunction with setRecentlyUsedFilenames() for saving the
        state of this list.

        @see setRecentlyUsedFilenames
    */
    StringArray getRecentlyUsedFilenames() const;

    /** Sets all the entries on the recent files list.

        This can be used in conjunction with getRecentlyUsedFilenames() for saving the
        state of this list.

        @see getRecentlyUsedFilenames, addRecentlyUsedFile
    */
    void setRecentlyUsedFilenames (const StringArray& filenames);

    /** Adds an entry to the recently-used files dropdown list.

        If the file is already in the list, it will be moved to the top. A limit
        is also placed on the number of items that are kept in the list.

        @see getRecentlyUsedFilenames, setRecentlyUsedFilenames, setMaxNumberOfRecentFiles
    */
    void addRecentlyUsedFile (const File& file);

    /** Changes the limit for the number of files that will be stored in the recent-file list.
    */
    void setMaxNumberOfRecentFiles (int newMaximum);

    /** Changes the text shown on the 'browse' button.

        By default this button just says "..." but you can change it. The button itself
        can be changed using the look-and-feel classes, so it might not actually have any
        text on it.
    */
    void setBrowseButtonText (const String& browseButtonText);

    /** Changes the wildcard pattern to use in the file browser - e.g. "*.txt;*.foo".
        If an empty string is passed in, then the pattern is assumed to be "*".

        @param wildcard     the new wildcard to use.
    */
    void setFileBrowserWildcard (const String& wildcard) noexcept;

    //==============================================================================
    /** Adds a listener that will be called when the selected file is changed. */
    void addListener (FilenameComponentListener* listener);

    /** Removes a previously-registered listener. */
    void removeListener (FilenameComponentListener* listener);

    /** Gives the component a tooltip. */
    void setTooltip (const String& newTooltip) override;

    //==============================================================================
    /** This abstract base class is implemented by LookAndFeel classes. */
    struct JUCE_API  LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual Button* createFilenameComponentBrowseButton (const String& text) = 0;
        virtual void layoutFilenameComponent (FilenameComponent&, ComboBox* filenameBox, Button* browseButton) =  0;
    };

    //==============================================================================
    /** @internal */
    void paintOverChildren (Graphics&) override;
    /** @internal */
    void resized() override;
    /** @internal */
    void lookAndFeelChanged() override;
    /** @internal */
    bool isInterestedInFileDrag (const StringArray&) override;
    /** @internal */
    void filesDropped (const StringArray&, int, int) override;
    /** @internal */
    void fileDragEnter (const StringArray&, int, int) override;
    /** @internal */
    void fileDragExit (const StringArray&) override;
    /** @internal */
    KeyboardFocusTraverser* createFocusTraverser() override;

private:
    //==============================================================================
    ComboBox filenameBox;
    String lastFilename;
    std::unique_ptr<Button> browseButton;
    int maxRecentFiles = 30;
    bool isDir, isSaving, isFileDragOver = false;
    String wildcard, enforcedSuffix, browseButtonText;
    ListenerList <FilenameComponentListener> listeners;
    File defaultBrowseFile;

    void showChooser();
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilenameComponent)
};

} // namespace juce
