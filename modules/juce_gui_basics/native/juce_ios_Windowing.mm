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
    extern bool isIOSAppActive;

    struct AppInactivityCallback // NB: careful, this declaration is duplicated in other modules
    {
        virtual ~AppInactivityCallback() = default;
        virtual void appBecomingInactive() = 0;
    };

    // This is an internal list of callbacks (but currently used between modules)
    Array<AppInactivityCallback*> appBecomingInactiveCallbacks;
}

#if JUCE_PUSH_NOTIFICATIONS && defined (__IPHONE_10_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
@interface JuceAppStartupDelegate : NSObject <UIApplicationDelegate, UNUserNotificationCenterDelegate>
#else
@interface JuceAppStartupDelegate : NSObject <UIApplicationDelegate>
#endif
{
    UIBackgroundTaskIdentifier appSuspendTask;
}

@property (strong, nonatomic) UIWindow *window;
- (id) init;
- (void) dealloc;
- (void) applicationDidFinishLaunching: (UIApplication*) application;
- (void) applicationWillTerminate: (UIApplication*) application;
- (void) applicationDidEnterBackground: (UIApplication*) application;
- (void) applicationWillEnterForeground: (UIApplication*) application;
- (void) applicationDidBecomeActive: (UIApplication*) application;
- (void) applicationWillResignActive: (UIApplication*) application;
- (void) application: (UIApplication*) application handleEventsForBackgroundURLSession: (NSString*) identifier
   completionHandler: (void (^)(void)) completionHandler;
- (void) applicationDidReceiveMemoryWarning: (UIApplication *) application;
#if JUCE_PUSH_NOTIFICATIONS
- (void) application: (UIApplication*) application didRegisterUserNotificationSettings: (UIUserNotificationSettings*) notificationSettings;
- (void) application: (UIApplication*) application didRegisterForRemoteNotificationsWithDeviceToken: (NSData*) deviceToken;
- (void) application: (UIApplication*) application didFailToRegisterForRemoteNotificationsWithError: (NSError*) error;
- (void) application: (UIApplication*) application didReceiveRemoteNotification: (NSDictionary*) userInfo;
- (void) application: (UIApplication*) application didReceiveRemoteNotification: (NSDictionary*) userInfo
  fetchCompletionHandler: (void (^)(UIBackgroundFetchResult result)) completionHandler;
- (void) application: (UIApplication*) application handleActionWithIdentifier: (NSString*) identifier
  forRemoteNotification: (NSDictionary*) userInfo withResponseInfo: (NSDictionary*) responseInfo
   completionHandler: (void(^)()) completionHandler;
- (void) application: (UIApplication*) application didReceiveLocalNotification: (UILocalNotification*) notification;
- (void) application: (UIApplication*) application handleActionWithIdentifier: (NSString*) identifier
  forLocalNotification: (UILocalNotification*) notification completionHandler: (void(^)()) completionHandler;
- (void) application: (UIApplication*) application handleActionWithIdentifier: (NSString*) identifier
  forLocalNotification: (UILocalNotification*) notification withResponseInfo: (NSDictionary*) responseInfo
   completionHandler: (void(^)()) completionHandler;
#if defined (__IPHONE_10_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
- (void) userNotificationCenter: (UNUserNotificationCenter*) center willPresentNotification: (UNNotification*) notification
          withCompletionHandler: (void (^)(UNNotificationPresentationOptions options)) completionHandler;
- (void) userNotificationCenter: (UNUserNotificationCenter*) center didReceiveNotificationResponse: (UNNotificationResponse*) response
          withCompletionHandler: (void(^)())completionHandler;
#endif
#endif

@end

@implementation JuceAppStartupDelegate

    NSObject* _pushNotificationsDelegate;

- (id) init
{
    self = [super init];
    appSuspendTask = UIBackgroundTaskInvalid;

   #if JUCE_PUSH_NOTIFICATIONS && defined (__IPHONE_10_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
    [UNUserNotificationCenter currentNotificationCenter].delegate = self;
   #endif

    return self;
}

- (void) dealloc
{
    [super dealloc];
}

- (BOOL) application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
    ignoreUnused (application);
    initialiseJuce_GUI();

    if (auto* app = JUCEApplicationBase::createInstance())
    {
        if (! app->initialiseApp())
        {
            exit(app->shutdownApp());
            return NO;
        }
    }
    else
    {
        jassertfalse; // you must supply an application object for an iOS app!
    }

        return YES;
}

- (BOOL) application: (UIApplication*) application willFinishLaunchingWithOptions: (NSDictionary<UIApplicationLaunchOptionsKey, id>*) launchOptions
{
    return YES;
}

-(BOOL) application: (UIApplication*) application continueUserActivity: (NSUserActivity*) userActivity restorationHandler: (void (^)(NSArray * _Nullable)) restorationHandler
{
    // Check whether the activity was from a web page redirect to the app.
    if ([userActivity.activityType isEqualToString: NSUserActivityTypeBrowsingWeb]) {
        // Get the URL from the UserActivty Object.
        NSURL *url = userActivity.webpageURL;

        if (auto* app = JUCEApplicationBase::getInstance())
        {
            auto commandLine (nsStringToJuce ([url absoluteString]));
            app->anotherInstanceStarted (commandLine);
            return YES;
        }
    }
    return NO;
}

- (BOOL) application: (UIApplication*) application openURL: (NSURL*) url sourceApplication: (NSString*) sourceApplication annotation: (id) annotation {

    if (auto* app = JUCEApplicationBase::getInstance())
    {
        auto commandLine (nsStringToJuce ([url absoluteString]));
        app->anotherInstanceStarted (commandLine);
        return YES;
    }

    return NO;
}


- (void) applicationWillTerminate: (UIApplication*) application
{
    ignoreUnused (application);
    JUCEApplicationBase::appWillTerminateByForce();
}

- (void) applicationDidEnterBackground: (UIApplication*) application
{
    if (auto* app = JUCEApplicationBase::getInstance())
    {
       #if JUCE_EXECUTE_APP_SUSPEND_ON_BACKGROUND_TASK
        appSuspendTask = [application beginBackgroundTaskWithName:@"JUCE Suspend Task" expirationHandler:^{
            if (appSuspendTask != UIBackgroundTaskInvalid)
            {
                [application endBackgroundTask:appSuspendTask];
                appSuspendTask = UIBackgroundTaskInvalid;
            }
        }];

        MessageManager::callAsync ([app] { app->suspended(); });
       #else
        ignoreUnused (application);
        app->suspended();
       #endif
    }
}

- (void) applicationWillEnterForeground: (UIApplication*) application
{
    ignoreUnused (application);

    if (auto* app = JUCEApplicationBase::getInstance())
        app->resumed();
}

- (void) applicationDidBecomeActive: (UIApplication*) application
{
    application.applicationIconBadgeNumber = 0;

    isIOSAppActive = true;
}

- (void) applicationWillResignActive: (UIApplication*) application
{
    ignoreUnused (application);
    isIOSAppActive = false;

    for (int i = appBecomingInactiveCallbacks.size(); --i >= 0;)
        appBecomingInactiveCallbacks.getReference(i)->appBecomingInactive();
}

- (void) application: (UIApplication*) application handleEventsForBackgroundURLSession: (NSString*)identifier
   completionHandler: (void (^)(void))completionHandler
{
    ignoreUnused (application);
    URL::DownloadTask::juce_iosURLSessionNotify (nsStringToJuce (identifier));
    completionHandler();
}

- (void) applicationDidReceiveMemoryWarning: (UIApplication*) application
{
    ignoreUnused (application);

    if (auto* app = JUCEApplicationBase::getInstance())
        app->memoryWarningReceived();
}

- (void) setPushNotificationsDelegateToUse: (NSObject*) delegate
{
    _pushNotificationsDelegate = delegate;
}

#if JUCE_PUSH_NOTIFICATIONS
- (void) application: (UIApplication*) application didRegisterUserNotificationSettings: (UIUserNotificationSettings*) notificationSettings
{
    ignoreUnused (application);

    SEL selector = @selector (application:didRegisterUserNotificationSettings:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application          atIndex:2];
        [invocation setArgument: &notificationSettings atIndex:3];

        [invocation invoke];
    }
}

- (void) application: (UIApplication*) application didRegisterForRemoteNotificationsWithDeviceToken: (NSData*) deviceToken
{
    ignoreUnused (application);

    SEL selector = @selector (application:didRegisterForRemoteNotificationsWithDeviceToken:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application atIndex:2];
        [invocation setArgument: &deviceToken atIndex:3];

        [invocation invoke];
    }
}

- (void) application: (UIApplication*) application didFailToRegisterForRemoteNotificationsWithError: (NSError*) error
{
    ignoreUnused (application);

    SEL selector = @selector (application:didFailToRegisterForRemoteNotificationsWithError:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application atIndex:2];
        [invocation setArgument: &error       atIndex:3];

        [invocation invoke];
    }
}

- (void) application: (UIApplication*) application didReceiveRemoteNotification: (NSDictionary*) userInfo
{
    ignoreUnused (application);

    SEL selector = @selector (application:didReceiveRemoteNotification:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application atIndex:2];
        [invocation setArgument: &userInfo    atIndex:3];

        [invocation invoke];
    }
}

- (void) application: (UIApplication*) application didReceiveRemoteNotification: (NSDictionary*) userInfo
  fetchCompletionHandler: (void (^)(UIBackgroundFetchResult result)) completionHandler
{
    ignoreUnused (application);

    SEL selector = @selector (application:didReceiveRemoteNotification:fetchCompletionHandler:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application       atIndex:2];
        [invocation setArgument: &userInfo          atIndex:3];
        [invocation setArgument: &completionHandler atIndex:4];

        [invocation invoke];
    }
}

- (void) application: (UIApplication*) application handleActionWithIdentifier: (NSString*) identifier
  forRemoteNotification: (NSDictionary*) userInfo withResponseInfo: (NSDictionary*) responseInfo
  completionHandler: (void(^)()) completionHandler
{
    ignoreUnused (application);

    SEL selector = @selector (application:handleActionWithIdentifier:forRemoteNotification:withResponseInfo:completionHandler:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application       atIndex:2];
        [invocation setArgument: &identifier        atIndex:3];
        [invocation setArgument: &userInfo          atIndex:4];
        [invocation setArgument: &responseInfo      atIndex:5];
        [invocation setArgument: &completionHandler atIndex:6];

        [invocation invoke];
    }
}

- (void) application: (UIApplication*) application didReceiveLocalNotification: (UILocalNotification*) notification
{
    ignoreUnused (application);

    SEL selector = @selector (application:didReceiveLocalNotification:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application  atIndex:2];
        [invocation setArgument: &notification atIndex:3];

        [invocation invoke];
    }
}

- (void) application: (UIApplication*) application handleActionWithIdentifier: (NSString*) identifier
  forLocalNotification: (UILocalNotification*) notification completionHandler: (void(^)()) completionHandler
{
    ignoreUnused (application);

    SEL selector = @selector (application:handleActionWithIdentifier:forLocalNotification:completionHandler:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application       atIndex:2];
        [invocation setArgument: &identifier        atIndex:3];
        [invocation setArgument: &notification      atIndex:4];
        [invocation setArgument: &completionHandler atIndex:5];

        [invocation invoke];
    }
}

- (void) application: (UIApplication*) application handleActionWithIdentifier: (NSString*) identifier
  forLocalNotification: (UILocalNotification*) notification withResponseInfo: (NSDictionary*) responseInfo
  completionHandler: (void(^)()) completionHandler
{
    ignoreUnused (application);

    SEL selector = @selector (application:handleActionWithIdentifier:forLocalNotification:withResponseInfo:completionHandler:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &application       atIndex:2];
        [invocation setArgument: &identifier        atIndex:3];
        [invocation setArgument: &notification      atIndex:4];
        [invocation setArgument: &responseInfo      atIndex:5];
        [invocation setArgument: &completionHandler atIndex:6];

        [invocation invoke];
    }
}

#if defined (__IPHONE_10_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
- (void) userNotificationCenter: (UNUserNotificationCenter*) center willPresentNotification: (UNNotification*) notification
         withCompletionHandler: (void (^)(UNNotificationPresentationOptions options)) completionHandler
{
    ignoreUnused (center);

    SEL selector = @selector (userNotificationCenter:willPresentNotification:withCompletionHandler:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &center            atIndex:2];
        [invocation setArgument: &notification      atIndex:3];
        [invocation setArgument: &completionHandler atIndex:4];

        [invocation invoke];
    }
}

- (void) userNotificationCenter: (UNUserNotificationCenter*) center didReceiveNotificationResponse: (UNNotificationResponse*) response
         withCompletionHandler: (void(^)()) completionHandler
{
    ignoreUnused (center);

    SEL selector = @selector (userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:);

    if (_pushNotificationsDelegate != nil && [_pushNotificationsDelegate respondsToSelector: selector])
    {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature: [_pushNotificationsDelegate methodSignatureForSelector: selector]];
        [invocation setSelector: selector];
        [invocation setTarget: _pushNotificationsDelegate];
        [invocation setArgument: &center            atIndex:2];
        [invocation setArgument: &response          atIndex:3];
        [invocation setArgument: &completionHandler atIndex:4];

        [invocation invoke];
    }
}
#endif
#endif

@end

namespace juce
{

int juce_iOSMain (int argc, const char* argv[], void* customDelegatePtr);
int juce_iOSMain (int argc, const char* argv[], void* customDelegatePtr)
{
    Class delegateClass = (customDelegatePtr != nullptr ? reinterpret_cast<Class> (customDelegatePtr) : [JuceAppStartupDelegate class]);

    return UIApplicationMain (argc, const_cast<char**> (argv), nil, NSStringFromClass (delegateClass));
}

//==============================================================================
void LookAndFeel::playAlertSound()
{
    // TODO
}

//==============================================================================
class iOSMessageBox
{
public:
    iOSMessageBox (const String& title, const String& message,
                   NSString* button1, NSString* button2, NSString* button3,
                   ModalComponentManager::Callback* cb, const bool async)
        : result (0), resultReceived (false), callback (cb), isAsync (async)
    {
        if (currentlyFocusedPeer != nullptr)
        {
            UIAlertController* alert = [UIAlertController alertControllerWithTitle: juceStringToNS (title)
                                                                           message: juceStringToNS (message)
                                                                    preferredStyle: UIAlertControllerStyleAlert];
            addButton (alert, button1, 0);
            addButton (alert, button2, 1);
            addButton (alert, button3, 2);

            [currentlyFocusedPeer->controller presentViewController: alert
                                                           animated: YES
                                                         completion: nil];
        }
        else
        {
            // Since iOS8, alert windows need to be associated with a window, so you need to
            // have at least one window on screen when you use this
            jassertfalse;
        }
    }

    int getResult()
    {
        jassert (callback == nullptr);

        JUCE_AUTORELEASEPOOL
        {
            while (! resultReceived)
                [[NSRunLoop mainRunLoop] runUntilDate: [NSDate dateWithTimeIntervalSinceNow: 0.01]];
        }

        return result;
    }

    void buttonClicked (const int buttonIndex) noexcept
    {
        result = buttonIndex;
        resultReceived = true;

        if (callback != nullptr)
            callback->modalStateFinished (result);

        if (isAsync)
            delete this;
    }

private:
    int result;
    bool resultReceived;
    std::unique_ptr<ModalComponentManager::Callback> callback;
    const bool isAsync;

    void addButton (UIAlertController* alert, NSString* text, int index)
    {
        if (text != nil)
            [alert addAction: [UIAlertAction actionWithTitle: text
                                                       style: UIAlertActionStyleDefault
                                                     handler: ^(UIAlertAction*) { this->buttonClicked (index); }]];
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (iOSMessageBox)
};

//==============================================================================
#if JUCE_MODAL_LOOPS_PERMITTED
void JUCE_CALLTYPE NativeMessageBox::showMessageBox (AlertWindow::AlertIconType /*iconType*/,
                                                     const String& title, const String& message,
                                                     Component* /*associatedComponent*/)
{
    JUCE_AUTORELEASEPOOL
    {
        iOSMessageBox mb (title, message, @"OK", nil, nil, nullptr, false);
        ignoreUnused (mb.getResult());
    }
}
#endif

void JUCE_CALLTYPE NativeMessageBox::showMessageBoxAsync (AlertWindow::AlertIconType /*iconType*/,
                                                          const String& title, const String& message,
                                                          Component* /*associatedComponent*/,
                                                          ModalComponentManager::Callback* callback)
{
    new iOSMessageBox (title, message, @"OK", nil, nil, callback, true);
}

bool JUCE_CALLTYPE NativeMessageBox::showOkCancelBox (AlertWindow::AlertIconType /*iconType*/,
                                                      const String& title, const String& message,
                                                      Component* /*associatedComponent*/,
                                                      ModalComponentManager::Callback* callback)
{
    std::unique_ptr<iOSMessageBox> mb (new iOSMessageBox (title, message, @"Cancel", @"OK",
                                                          nil, callback, callback != nullptr));

    if (callback == nullptr)
        return mb->getResult() == 1;

    mb.release();
    return false;
}

int JUCE_CALLTYPE NativeMessageBox::showYesNoCancelBox (AlertWindow::AlertIconType /*iconType*/,
                                                        const String& title, const String& message,
                                                        Component* /*associatedComponent*/,
                                                        ModalComponentManager::Callback* callback)
{
    std::unique_ptr<iOSMessageBox> mb (new iOSMessageBox (title, message, @"Cancel", @"Yes", @"No", callback, callback != nullptr));

    if (callback == nullptr)
        return mb->getResult();

    mb.release();
    return 0;
}

int JUCE_CALLTYPE NativeMessageBox::showYesNoBox (AlertWindow::AlertIconType /*iconType*/,
                                                  const String& title, const String& message,
                                                  Component* /*associatedComponent*/,
                                                  ModalComponentManager::Callback* callback)
{
    std::unique_ptr<iOSMessageBox> mb (new iOSMessageBox (title, message, @"No", @"Yes", nil, callback, callback != nullptr));

    if (callback == nullptr)
        return mb->getResult();

    mb.release();
    return 0;
}

//==============================================================================
bool DragAndDropContainer::performExternalDragDropOfFiles (const StringArray&, bool, Component*, std::function<void()>)
{
    jassertfalse;    // no such thing on iOS!
    return false;
}

bool DragAndDropContainer::performExternalDragDropOfText (const String&, Component*, std::function<void()>)
{
    jassertfalse;    // no such thing on iOS!
    return false;
}

//==============================================================================
void Desktop::setScreenSaverEnabled (const bool isEnabled)
{
    if (! SystemStats::isRunningInAppExtensionSandbox())
        [[UIApplication sharedApplication] setIdleTimerDisabled: ! isEnabled];
}

bool Desktop::isScreenSaverEnabled()
{
    if (SystemStats::isRunningInAppExtensionSandbox())
        return true;

    return ! [[UIApplication sharedApplication] isIdleTimerDisabled];
}

//==============================================================================
bool juce_areThereAnyAlwaysOnTopWindows()
{
    return false;
}

//==============================================================================
Image juce_createIconForFile (const File&)
{
    return Image();
}

//==============================================================================
void SystemClipboard::copyTextToClipboard (const String& text)
{
    [[UIPasteboard generalPasteboard] setValue: juceStringToNS (text)
                             forPasteboardType: @"public.text"];
}

String SystemClipboard::getTextFromClipboard()
{
    return nsStringToJuce ([[UIPasteboard generalPasteboard] valueForPasteboardType: @"public.text"]);
}

//==============================================================================
bool MouseInputSource::SourceList::addSource()
{
    addSource (sources.size(), MouseInputSource::InputSourceType::touch);
    return true;
}

bool MouseInputSource::SourceList::canUseTouch()
{
    return true;
}

bool Desktop::canUseSemiTransparentWindows() noexcept
{
    return true;
}

Point<float> MouseInputSource::getCurrentRawMousePosition()
{
    return juce_lastMousePos;
}

void MouseInputSource::setRawMousePosition (Point<float>)
{
}

double Desktop::getDefaultMasterScale()
{
    return 1.0;
}

Desktop::DisplayOrientation Desktop::getCurrentOrientation() const
{
    UIInterfaceOrientation orientation = SystemStats::isRunningInAppExtensionSandbox() ? UIInterfaceOrientationPortrait
                                                                                       : getWindowOrientation();

    return Orientations::convertToJuce (orientation);
}

// The most straightforward way of retrieving the screen area available to an iOS app
// seems to be to create a new window (which will take up all available space) and to
// query its frame.
struct TemporaryWindow
{
    UIWindow* window = [[UIWindow alloc] init];
    ~TemporaryWindow() noexcept { [window release]; }
};

static Rectangle<int> getRecommendedWindowBounds()
{
    return convertToRectInt (TemporaryWindow().window.frame);
}

static BorderSize<int> getSafeAreaInsets (float masterScale)
{
   #if defined (__IPHONE_11_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_11_0
    UIEdgeInsets safeInsets = TemporaryWindow().window.safeAreaInsets;

    auto getInset = [&] (CGFloat original) { return roundToInt (original / masterScale); };

    return { getInset (safeInsets.top),    getInset (safeInsets.left),
             getInset (safeInsets.bottom), getInset (safeInsets.right) };
   #else
    auto statusBarSize = [UIApplication sharedApplication].statusBarFrame.size;
    auto statusBarHeight = jmin (statusBarSize.width, statusBarSize.height);

    return { roundToInt (statusBarHeight / masterScale), 0, 0, 0 };
   #endif
}

void Displays::findDisplays (float masterScale)
{
    JUCE_AUTORELEASEPOOL
    {
        UIScreen* s = [UIScreen mainScreen];

        Display d;
        d.totalArea = convertToRectInt ([s bounds]) / masterScale;
        d.userArea = getRecommendedWindowBounds() / masterScale;
        d.safeAreaInsets = getSafeAreaInsets (masterScale);
        d.isMain = true;
        d.scale = masterScale * s.scale;
        d.dpi = 160 * d.scale;

        displays.add (d);
    }
}

} // namespace juce
