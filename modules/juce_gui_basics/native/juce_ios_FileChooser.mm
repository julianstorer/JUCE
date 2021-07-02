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

namespace juce
{

#if ! (defined (__IPHONE_16_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_16_0)
 JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
 #define JUCE_DEPRECATION_IGNORED 1
#endif

class FileChooser::Native  : public FileChooser::Pimpl,
                             public Component,
                             private AsyncUpdater
{
public:
    Native (FileChooser& fileChooser, int flags)
        : owner (fileChooser)
    {
        static FileChooserDelegateClass delegateClass;
        delegate.reset ([delegateClass.createInstance() init]);
        FileChooserDelegateClass::setOwner (delegate.get(), this);

        static FileChooserControllerClass controllerClass;
        auto* controllerClassInstance = controllerClass.createInstance();

        String firstFileExtension;
        auto utTypeArray = createNSArrayFromStringArray (getUTTypesForWildcards (owner.filters, firstFileExtension));

        if ((flags & FileBrowserComponent::saveMode) != 0)
        {
            auto currentFileOrDirectory = owner.startingFile;

            UIDocumentPickerMode pickerMode = currentFileOrDirectory.existsAsFile()
                                                ? UIDocumentPickerModeExportToService
                                                : UIDocumentPickerModeMoveToService;

            if (! currentFileOrDirectory.existsAsFile())
            {
                auto filename = getFilename (currentFileOrDirectory, firstFileExtension);
                auto tmpDirectory = File::createTempFile ("JUCE-filepath");

                if (tmpDirectory.createDirectory().wasOk())
                {
                    currentFileOrDirectory = tmpDirectory.getChildFile (filename);
                    currentFileOrDirectory.replaceWithText ("");
                }
                else
                {
                    // Temporary directory creation failed! You need to specify a
                    // path you have write access to. Saving will not work for
                    // current path.
                    jassertfalse;
                }
            }

            auto url = [[NSURL alloc] initFileURLWithPath: juceStringToNS (currentFileOrDirectory.getFullPathName())];

            controller.reset ([controllerClassInstance initWithURL: url
                                                            inMode: pickerMode]);

            [url release];
        }
        else
        {
            controller.reset ([controllerClassInstance initWithDocumentTypes: utTypeArray
                                                                      inMode: UIDocumentPickerModeOpen]);
            if (@available(iOS 11, *))
            {
                if ((flags & FileBrowserComponent::canSelectMultipleItems) != 0)
                {
                    controller.get().allowsMultipleSelection = YES;
                }
            }
        }

        FileChooserControllerClass::setOwner (controller.get(), this);

        [controller.get() setDelegate: delegate.get()];
        [controller.get() setModalTransitionStyle: UIModalTransitionStyleCrossDissolve];

        setOpaque (false);

        if (fileChooser.parent != nullptr)
        {
            [controller.get() setModalPresentationStyle: UIModalPresentationFullScreen];

            auto chooserBounds = fileChooser.parent->getBounds();
            setBounds (chooserBounds);

            setAlwaysOnTop (true);
            fileChooser.parent->addAndMakeVisible (this);
        }
        else
        {
            if (SystemStats::isRunningInAppExtensionSandbox())
            {
                // Opening a native top-level window in an AUv3 is not allowed (sandboxing). You need to specify a
                // parent component (for example your editor) to parent the native file chooser window. To do this
                // specify a parent component in the FileChooser's constructor!
                jassertfalse;
                return;
            }

            auto chooserBounds = Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            setBounds (chooserBounds);

            setAlwaysOnTop (true);
            setVisible (true);
            addToDesktop (0);
        }
    }

    ~Native() override
    {
        exitModalState (0);
    }

    void launch() override
    {
        enterModalState (true, nullptr, true);
    }

    void runModally() override
    {
       #if JUCE_MODAL_LOOPS_PERMITTED
        runModalLoop();
       #else
        jassertfalse;
       #endif
    }

    void parentHierarchyChanged() override
    {
        auto* newPeer = dynamic_cast<UIViewComponentPeer*> (getPeer());

        if (peer != newPeer)
        {
            peer = newPeer;

            if (peer != nullptr)
            {
                if (auto* parentController = peer->controller)
                    [parentController showViewController: controller.get() sender: parentController];

                peer->toFront (false);
            }
        }
    }

private:
    //==============================================================================
    void handleAsyncUpdate() override
    {
        pickerWasCancelled();
    }

    //==============================================================================
    static StringArray getUTTypesForWildcards (const String& filterWildcards, String& firstExtension)
    {
        auto filters = StringArray::fromTokens (filterWildcards, ";", "");
        StringArray result;

        firstExtension = {};

        if (! filters.contains ("*") && filters.size() > 0)
        {
            for (auto filter : filters)
            {
                if (filter.isEmpty())
                    continue;

                // iOS only supports file extension wild cards
                jassert (filter.upToLastOccurrenceOf (".", true, false) == "*.");

                auto fileExtension = filter.fromLastOccurrenceOf (".", false, false);
                CFUniquePtr<CFStringRef> fileExtensionCF (fileExtension.toCFString());

                if (firstExtension.isEmpty())
                    firstExtension = fileExtension;

                if (auto tag = CFUniquePtr<CFStringRef> (UTTypeCreatePreferredIdentifierForTag (kUTTagClassFilenameExtension, fileExtensionCF.get(), nullptr)))
                    result.add (String::fromCFString (tag.get()));
            }
        }
        else
        {
            result.add ("public.data");
        }

        return result;
    }

    static String getFilename (const File& path, const String& fallbackExtension)
    {
        auto filename  = path.getFileNameWithoutExtension();
        auto extension = path.getFileExtension().substring (1);

        if (filename.isEmpty())
            filename = "Untitled";

        if (extension.isEmpty())
            extension = fallbackExtension;

        if (extension.isNotEmpty())
            filename += "." + extension;

        return filename;
    }

    //==============================================================================
    void didPickDocumentsAtURLs (NSArray<NSURL*>* urls)
    {
        cancelPendingUpdate();

        bool isWriting = controller.get().documentPickerMode == UIDocumentPickerModeExportToService
                       | controller.get().documentPickerMode == UIDocumentPickerModeMoveToService;

        NSUInteger accessOptions = isWriting ? 0 : NSFileCoordinatorReadingWithoutChanges;

        NSMutableArray<NSFileAccessIntent*>* intents = [NSMutableArray array];

        for (NSURL* url in urls)
        {
            auto* fileAccessIntent = isWriting
                                     ? [NSFileAccessIntent writingIntentWithURL: url options: accessOptions]
                                     : [NSFileAccessIntent readingIntentWithURL: url options: accessOptions];
            [intents addObject:fileAccessIntent];
        }

        auto fileCoordinator = [[[NSFileCoordinator alloc] initWithFilePresenter: nil] autorelease];

        [fileCoordinator coordinateAccessWithIntents: intents queue: [NSOperationQueue mainQueue] byAccessor: ^(NSError* err)
        {
            Array<URL> chooserResults;

            if (err == nil)
            {
                for (NSURL* url in urls)
                {
                    [url startAccessingSecurityScopedResource];

                    NSError* error = nil;

                    NSData* bookmark = [url bookmarkDataWithOptions: 0
                                     includingResourceValuesForKeys: nil
                                                      relativeToURL: nil
                                                              error: &error];

                    [bookmark retain];

                    [url stopAccessingSecurityScopedResource];

                    URL juceUrl (nsStringToJuce ([url absoluteString]));

                    if (error == nil)
                    {
                        setURLBookmark (juceUrl, (void*) bookmark);
                    }
                    else
                    {
                        auto desc = [error localizedDescription];
                        ignoreUnused (desc);
                        jassertfalse;
                    }

                    chooserResults.add (juceUrl);
                }
            }
            else
            {
                auto desc = [err localizedDescription];
                ignoreUnused (desc);
                jassertfalse;
            }

            owner.finished (chooserResults);
        }];
    }

    void pickerWasCancelled()
    {
        cancelPendingUpdate();

        owner.finished ({});
        exitModalState (0);
    }

    //==============================================================================
    struct FileChooserDelegateClass  : public ObjCClass<NSObject<UIDocumentPickerDelegate>>
    {
        FileChooserDelegateClass()  : ObjCClass<NSObject<UIDocumentPickerDelegate>> ("FileChooserDelegate_")
        {
            addIvar<Native*> ("owner");

            if (@available(iOS 11, *))
            {
                addMethod (@selector (documentPicker:didPickDocumentsAtURLs:), didPickDocumentsAtURLs, "v@:@@");
            }
            else
            {
                addMethod (@selector (documentPicker:didPickDocumentAtURL:),   didPickDocumentAtURL,   "v@:@@");
            }

            addMethod (@selector (documentPickerWasCancelled:),          documentPickerWasCancelled, "v@:@");

            addProtocol (@protocol (UIDocumentPickerDelegate));

            registerClass();
        }

        static void setOwner (id self, Native* owner)   { object_setInstanceVariable (self, "owner", owner); }
        static Native* getOwner (id self)               { return getIvar<Native*> (self, "owner"); }

        //==============================================================================
        static void didPickDocumentsAtURLs (id self, SEL, UIDocumentPickerViewController*, NSArray<NSURL*>* urls)
        {
            if (auto* picker = getOwner (self))
                picker->didPickDocumentsAtURLs (urls);
        }

        static void didPickDocumentAtURL (id self, SEL, UIDocumentPickerViewController*, NSURL* url)
        {
            if (auto* picker = getOwner (self))
                picker->didPickDocumentsAtURLs (@[url]);
        }

        static void documentPickerWasCancelled (id self, SEL, UIDocumentPickerViewController*)
        {
            if (auto* picker = getOwner (self))
                picker->pickerWasCancelled();
        }
    };

    struct FileChooserControllerClass  : public ObjCClass<UIDocumentPickerViewController>
    {
        FileChooserControllerClass()  : ObjCClass<UIDocumentPickerViewController> ("FileChooserController_")
        {
            addIvar<Native*> ("owner");
            addMethod (@selector (viewDidDisappear:), viewDidDisappear, "v@:@c");

            registerClass();
        }

        static void setOwner (id self, Native* owner)   { object_setInstanceVariable (self, "owner", owner); }
        static Native* getOwner (id self)               { return getIvar<Native*> (self, "owner"); }

        //==============================================================================
        static void viewDidDisappear (id self, SEL, BOOL animated)
        {
            sendSuperclassMessage<void> (self, @selector (viewDidDisappear:), animated);

            if (auto* picker = getOwner (self))
                picker->triggerAsyncUpdate();
        }
    };

    //==============================================================================
    FileChooser& owner;
    NSUniquePtr<NSObject<UIDocumentPickerDelegate>> delegate;
    NSUniquePtr<UIDocumentPickerViewController> controller;
    UIViewComponentPeer* peer = nullptr;

    static FileChooserDelegateClass fileChooserDelegateClass;
    static FileChooserControllerClass fileChooserControllerClass;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Native)
};

//==============================================================================
bool FileChooser::isPlatformDialogAvailable()
{
   #if JUCE_DISABLE_NATIVE_FILECHOOSERS
    return false;
   #else
    return true;
   #endif
}

std::shared_ptr<FileChooser::Pimpl> FileChooser::showPlatformDialog (FileChooser& owner, int flags,
                                                                     FilePreviewComponent*)
{
    return std::make_shared<FileChooser::Native> (owner, flags);
}

#if JUCE_DEPRECATION_IGNORED
 JUCE_END_IGNORE_WARNINGS_GCC_LIKE
#endif

#undef JUCE_DEPRECATION_IGNORED

} // namespace juce
