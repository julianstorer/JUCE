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

@interface NSEvent (DeviceDelta)
- (float)deviceDeltaX;
- (float)deviceDeltaY;
@end

//==============================================================================
namespace juce
{

using AppFocusChangeCallback = void (*)();
extern AppFocusChangeCallback appFocusChangeCallback;
using CheckEventBlockedByModalComps = bool (*) (NSEvent*);
extern CheckEventBlockedByModalComps isEventBlockedByModalComps;

//==============================================================================
static void resetTrackingArea (NSView* view)
{
    const auto trackingAreas = [view trackingAreas];

    jassert ([trackingAreas count] <= 1);

    for (NSTrackingArea* area in trackingAreas)
        [view removeTrackingArea: area];

    const auto options = NSTrackingMouseEnteredAndExited
                         | NSTrackingMouseMoved
                         | NSTrackingActiveAlways
                         | NSTrackingInVisibleRect;

    const NSUniquePtr<NSTrackingArea> trackingArea { [[NSTrackingArea alloc] initWithRect: [view bounds]
                                                                                  options: options
                                                                                    owner: view
                                                                                 userInfo: nil] };

    [view addTrackingArea: trackingArea.get()];
}

static constexpr int translateVirtualToAsciiKeyCode (int keyCode) noexcept
{
    switch (keyCode)
    {
        // The virtual keycodes are from HIToolbox/Events.h
        case 0x00: return 'A';
        case 0x01: return 'S';
        case 0x02: return 'D';
        case 0x03: return 'F';
        case 0x04: return 'H';
        case 0x05: return 'G';
        case 0x06: return 'Z';
        case 0x07: return 'X';
        case 0x08: return 'C';
        case 0x09: return 'V';
        case 0x0B: return 'B';
        case 0x0C: return 'Q';
        case 0x0D: return 'W';
        case 0x0E: return 'E';
        case 0x0F: return 'R';
        case 0x10: return 'Y';
        case 0x11: return 'T';
        case 0x12: return '1';
        case 0x13: return '2';
        case 0x14: return '3';
        case 0x15: return '4';
        case 0x16: return '6';
        case 0x17: return '5';
        case 0x18: return '=';  // kVK_ANSI_Equal
        case 0x19: return '9';
        case 0x1A: return '7';
        case 0x1B: return '-';  // kVK_ANSI_Minus
        case 0x1C: return '8';
        case 0x1D: return '0';
        case 0x1E: return ']';  // kVK_ANSI_RightBracket
        case 0x1F: return 'O';
        case 0x20: return 'U';
        case 0x21: return '[';  // kVK_ANSI_LeftBracket
        case 0x22: return 'I';
        case 0x23: return 'P';
        case 0x25: return 'L';
        case 0x26: return 'J';
        case 0x27: return '"';  // kVK_ANSI_Quote
        case 0x28: return 'K';
        case 0x29: return ';';  // kVK_ANSI_Semicolon
        case 0x2A: return '\\'; // kVK_ANSI_Backslash
        case 0x2B: return ',';  // kVK_ANSI_Comma
        case 0x2C: return '/';  // kVK_ANSI_Slash
        case 0x2D: return 'N';
        case 0x2E: return 'M';
        case 0x2F: return '.';  // kVK_ANSI_Period
        case 0x32: return '`';  // kVK_ANSI_Grave

        default:   return keyCode;
    }
}

constexpr int extendedKeyModifier = 0x30000;

//==============================================================================
struct JuceCALayerDelegateCallback
{
    virtual ~JuceCALayerDelegateCallback() = default;
    virtual void displayLayer (CALayer*) = 0;
};

class API_AVAILABLE (macos (10.12)) JuceCALayerDelegate final : public ObjCClass<NSObject<CALayerDelegate>>
{
public:
    using Callback = JuceCALayerDelegateCallback;

    static NSObject<CALayerDelegate>* construct (Callback* owner)
    {
        static JuceCALayerDelegate cls;
        auto* result = cls.createInstance();
        setOwner (result, owner);
        return result;
    }

private:
    JuceCALayerDelegate()
        : ObjCClass ("JuceCALayerDelegate_")
    {
        addIvar<Callback*> ("owner");

        addMethod (@selector (displayLayer:), [] (id self, SEL, CALayer* layer)
        {
            if (auto* owner = getOwner (self))
                owner->displayLayer (layer);
        });

        addProtocol (@protocol (CALayerDelegate));

        registerClass();
    }

    static Callback* getOwner (id self)
    {
        return getIvar<Callback*> (self, "owner");
    }

    static void setOwner (id self, Callback* newOwner)
    {
        object_setInstanceVariable (self, "owner", newOwner);
    }
};

//==============================================================================
class NSViewComponentPeer final : public ComponentPeer,
                                  private JuceCALayerDelegateCallback
{
public:
    NSViewComponentPeer (Component& comp, const int windowStyleFlags, Component* associated, NSView* viewToAttachTo)
        : ComponentPeer (comp, windowStyleFlags, associated),
          safeComponent (&comp),
          isSharedWindow (viewToAttachTo != nil),
          lastRepaintTime (Time::getMillisecondCounter())
    {
        UPDDManager::getInstance()->ensureConnected();
        
        appFocusChangeCallback = appFocusChanged;
        isEventBlockedByModalComps = checkEventBlockedByModalComps;

        auto r = makeCGRect (component.getLocalBounds());

        view = [createViewInstance() initWithFrame: r];
        setOwner (view, this);

        [view registerForDraggedTypes: getSupportedDragTypes()];

        resetTrackingArea (view);

        scopedObservers.emplace_back (view, frameChangedSelector, NSViewFrameDidChangeNotification, view);

        [view setPostsFrameChangedNotifications: YES];

      #if USE_COREGRAPHICS_RENDERING
        // Creating a metal renderer may fail on some systems.
        // We need to try creating the renderer before first creating a backing layer
        // so that we know whether to use a metal layer or the system default layer
        // (setWantsLayer: YES will call through to makeBackingLayer, where we check
        // whether metalRenderer is non-null).
        // The system overwrites the layer delegate set during makeBackingLayer,
        // so that must be set separately, after the layer has been created and
        // configured.
       #if JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS
        if (@available (macOS 10.14, *))
        {
            metalRenderer = CoreGraphicsMetalLayerRenderer::create();
            layerDelegate.reset (JuceCALayerDelegate::construct (this));
        }
       #endif

        if ((windowStyleFlags & ComponentPeer::windowRequiresSynchronousCoreGraphicsRendering) == 0)
        {
            [view setWantsLayer: YES];
            [view setLayerContentsRedrawPolicy: NSViewLayerContentsRedrawDuringViewResize];
            [view layer].drawsAsynchronously = YES;
        }

       #if JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS
        if (@available (macOS 10.14, *))
            if (metalRenderer != nullptr)
                view.layer.delegate = layerDelegate.get();
       #endif
      #endif

        if (isSharedWindow)
        {
            window = [viewToAttachTo window];
            [viewToAttachTo addSubview: view];
        }
        else
        {
            r.origin.x = (CGFloat) component.getX();
            r.origin.y = (CGFloat) component.getY();
            r = flippedScreenRect (r);

            window = [createWindowInstance() initWithContentRect: r
                                                       styleMask: getNSWindowStyleMask (windowStyleFlags)
                                                         backing: NSBackingStoreBuffered
                                                           defer: YES];
            [window setColorSpace: [NSColorSpace sRGBColorSpace]];
            setOwner (window, this);

            [window setAccessibilityElement: YES];

            [window orderOut: nil];
            [window setDelegate: (id<NSWindowDelegate>) window];

            [window setOpaque: component.isOpaque()];

            if (! [window isOpaque])
                [window setBackgroundColor: [NSColor clearColor]];

            [view setAppearance: [NSAppearance appearanceNamed: NSAppearanceNameAqua]];

            [window setHasShadow: ((windowStyleFlags & windowHasDropShadow) != 0)];

            if (component.isAlwaysOnTop())
                setAlwaysOnTop (true);

            [window setContentView: view];

            // We'll both retain and also release this on closing because plugin hosts can unexpectedly
            // close the window for us, and also tend to get cause trouble if setReleasedWhenClosed is NO.
            [window setReleasedWhenClosed: YES];
            [window retain];

            [window setExcludedFromWindowsMenu: (windowStyleFlags & windowIsTemporary) != 0];
            [window setIgnoresMouseEvents: (windowStyleFlags & windowIgnoresMouseClicks) != 0];

            setCollectionBehaviour (false);

            [window setRestorable: NO];

            if (@available (macOS 10.12, *))
                [window setTabbingMode: NSWindowTabbingModeDisallowed];

            scopedObservers.emplace_back (view, frameChangedSelector, NSWindowDidMoveNotification, window);
            scopedObservers.emplace_back (view, frameChangedSelector, NSWindowDidMiniaturizeNotification, window);
            scopedObservers.emplace_back (view, @selector (windowWillMiniaturize:), NSWindowWillMiniaturizeNotification, window);
            scopedObservers.emplace_back (view, @selector (windowDidDeminiaturize:), NSWindowDidDeminiaturizeNotification, window);
        }

        auto alpha = component.getAlpha();

        if (alpha < 1.0f)
            setAlpha (alpha);

        setTitle (component.getName());

        getNativeRealtimeModifiers = []
        {
            if ([NSEvent respondsToSelector: @selector (modifierFlags)])
                NSViewComponentPeer::updateModifiers ([NSEvent modifierFlags]);

            return ModifierKeys::currentModifiers;
        };
    }

    ~NSViewComponentPeer() override
    {
        scopedObservers.clear();

        setOwner (view, nullptr);

        if ([view superview] != nil)
        {
            redirectWillMoveToWindow (nullptr);
            [view removeFromSuperview];
        }

        if (! isSharedWindow)
        {
            detachFromParentWindow (window);

            NSArray<NSWindow*>* children = [[window childWindows] copy];

            for (NSWindow* child in children)
                [window removeChildWindow: child];

            [children release];

            setOwner (window, nullptr);
            [window setContentView: nil];
            [window close];
            [window release];
        }

        [view release];
    }

    //==============================================================================
    static void setOwner (id viewOrWindow, NSViewComponentPeer* newOwner)
    {
        object_setInstanceVariable (viewOrWindow, "owner", newOwner);
    }

    static NSViewComponentPeer* getOwner (id viewOrWindow)
    {
        return getIvar<NSViewComponentPeer*> (viewOrWindow, "owner");
    }

    static NSWindow* findParentWindow (const ComponentPeer& peer, NSWindow* ownWindow)
    {
        if (auto* associated = peer.getAssociatedComponent())
            if (auto* associatedPeer = associated->getPeer())
                if (auto* parent = [(NSView*) associatedPeer->getNativeHandle() window])
                    if (parent != ownWindow)
                        return parent;

        return nil;
    }

    static void detachFromParentWindow (NSWindow* window)
    {
        if (auto* parent = [window parentWindow])
            [parent removeChildWindow: window];
    }

    // A child ordered below its parent has to share the parent's level, since the window server
    // orders windows by level first
    static void addChildWindowKeepingLevel (NSWindow* parent, NSWindow* child, NSWindowOrderingMode ordered)
    {
        const auto level = [child level];
        [parent addChildWindow: child ordered: ordered];

        if (ordered == NSWindowAbove)
            [child setLevel: jmax (level, [parent level])];
    }

    // AppKit keeps child windows in the order they were attached, so changing the order means
    // reattaching every sibling in the new order.
    static void reattachChildWindows (NSWindow* parent, NSArray<NSWindow*>* newOrder)
    {
        for (NSWindow* child in newOrder)
        {
            if (auto* owner = getOwner (child))
            {
                [parent removeChildWindow: child];
                addChildWindowKeepingLevel (parent, child, owner->orderedBelowParent ? NSWindowBelow : NSWindowAbove);
            }
        }
    }

    // A window opened from another window is a child of that window, so that it stays above it,
    // moves with it, and is minimised with it. Ordering a child out drops the relationship, and
    // miniaturising a child on its own miniaturises its parent too, so the child is detached
    // around both and reattached afterwards.
    void attachToParentWindow()
    {
        if (isSharedWindow || ! component.isVisible())
            return;

        auto* parent = findParentWindow (*this, window);

        if ([window parentWindow] == parent)
            return;

        detachFromParentWindow (window);

        if (parent != nil)
            addChildWindowKeepingLevel (parent, window, orderedBelowParent ? NSWindowBelow : NSWindowAbove);

        updateOffsetFromParentWindow();
    }

    void associationChanged() override
    {
        attachToParentWindow();
    }

    //==============================================================================
    void* getNativeHandle() const override    { return view; }

    void setVisible (bool shouldBeVisible) override
    {
        if (isSharedWindow)
        {
            if (shouldBeVisible)
                [view setHidden: false];
            else if ([window firstResponder] != view || ([window firstResponder] == view && [window makeFirstResponder: nil]))
                [view setHidden: true];
        }
        else
        {
            if (shouldBeVisible)
            {
                ++insideToFrontCall;

                if (auto* parent = orderedBelowParent ? findParentWindow (*this, window) : nil)
                    [window orderWindow: NSWindowBelow relativeTo: [parent windowNumber]];
                else
                    [window orderFront: nil];

                --insideToFrontCall;
                attachToParentWindow();
                handleBroughtToFront();
            }
            else
            {
                detachFromParentWindow (window);
                [window orderOut: nil];
            }
        }
    }

    void setTitle (const String& title) override
    {
        JUCE_AUTORELEASEPOOL
        {
            if (! isSharedWindow)
                [window setTitle: juceStringToNS (title)];
        }
    }

    bool setDocumentEditedStatus (bool edited) override
    {
        if (! hasNativeTitleBar())
            return false;

        [window setDocumentEdited: edited];
        return true;
    }

    void setRepresentedFile (const File& file) override
    {
        if (! isSharedWindow)
        {
            [window setRepresentedFilename: juceStringToNS (file != File()
                                                                ? file.getFullPathName()
                                                                : String())];

            windowRepresentsFile = (file != File());
        }
    }

    void setBounds (const Rectangle<int>& newBounds, bool) override
    {
        auto r = makeCGRect (newBounds);
        auto oldViewSize = [view frame].size;

        if (isSharedWindow)
        {
            [view setFrame: r];
        }
        else
        {
            [window setFrame: [window frameRectForContentRect: flippedScreenRect (r)]
                     display: false];
        }

        if (! CGSizeEqualToSize (oldViewSize, r.size))
            [view setNeedsDisplay: true];
    }

    Rectangle<int> getBounds (const bool global) const
    {
        auto r = [view frame];
        NSWindow* viewWindow = [view window];

        if (global && viewWindow != nil)
        {
            r = [[view superview] convertRect: r toView: nil];
            r = [viewWindow convertRectToScreen: r];

            r = flippedScreenRect (r);
        }

        return convertToRectInt (r);
    }

    Rectangle<int> getBounds() const override
    {
        return getBounds (! isSharedWindow);
    }

    Point<float> localToGlobal (Point<float> relativePosition) override
    {
        return relativePosition + getBounds (true).getPosition().toFloat();
    }

    using ComponentPeer::localToGlobal;

    Point<float> globalToLocal (Point<float> screenPosition) override
    {
        return screenPosition - getBounds (true).getPosition().toFloat();
    }

    using ComponentPeer::globalToLocal;

    void setAlpha (float newAlpha) override
    {
        if (isSharedWindow)
            [view setAlphaValue: (CGFloat) newAlpha];
        else
            [window setAlphaValue: (CGFloat) newAlpha];
    }

    void setMinimised (bool shouldBeMinimised) override
    {
        if (! isSharedWindow)
        {
            if (shouldBeMinimised)
            {
                detachFromParentWindow (window);
                [window miniaturize: nil];
            }
            else
            {
                [window deminiaturize: nil];
                attachToParentWindow();
            }
        }
    }

    bool isMinimised() const override
    {
        return [window isMiniaturized];
    }

    bool isShowing() const override
    {
        return [window isVisible] && ! isMinimised();
    }

    NSWindowCollectionBehavior getCollectionBehavior (bool forceFullScreen) const
    {
        if (forceFullScreen)
            return NSWindowCollectionBehaviorFullScreenPrimary;

        // Some SDK versions don't define NSWindowCollectionBehaviorFullScreenAuxiliary
        constexpr auto fullScreenAux = (NSUInteger) (1 << 8);

        return (getStyleFlags() & (windowHasMaximiseButton | windowIsResizable)) == (windowHasMaximiseButton | windowIsResizable)
             ? NSWindowCollectionBehaviorFullScreenPrimary
             : fullScreenAux;
    }

    void setCollectionBehaviour (bool forceFullScreen) const
    {
        [window setCollectionBehavior: getCollectionBehavior (forceFullScreen)];
    }

    void setFullScreen (bool shouldBeFullScreen) override
    {
        if (isSharedWindow)
            return;

        if (shouldBeFullScreen)
            setCollectionBehaviour (true);

        if (isMinimised())
            setMinimised (false);

        if (shouldBeFullScreen != isFullScreen())
            [window toggleFullScreen: nil];
    }

    bool isFullScreen() const override
    {
        return ([window styleMask] & NSWindowStyleMaskFullScreen) != 0;
    }

    bool isKioskMode() const override
    {
        return isFullScreen() && ComponentPeer::isKioskMode();
    }

    static bool isWindowAtPoint (NSWindow* w, NSPoint screenPoint)
    {
        if ([NSWindow respondsToSelector: @selector (windowNumberAtPoint:belowWindowWithWindowNumber:)])
            return [NSWindow windowNumberAtPoint: screenPoint belowWindowWithWindowNumber: 0] == [w windowNumber];

        return true;
    }

    bool contains (Point<int> localPos, bool trueIfInAChildWindow) const override
    {
        NSRect viewFrame = [view frame];

        if (! (isPositiveAndBelow (localPos.getX(), viewFrame.size.width)
            && isPositiveAndBelow (localPos.getY(), viewFrame.size.height)))
            return false;

        if (! SystemStats::isRunningInAppExtensionSandbox())
        {
            if (NSWindow* const viewWindow = [view window])
            {
                NSRect windowFrame = [viewWindow frame];
                NSPoint windowPoint = [view convertPoint: NSMakePoint (localPos.x, localPos.y) toView: nil];
                NSPoint screenPoint = NSMakePoint (windowFrame.origin.x + windowPoint.x,
                                                   windowFrame.origin.y + windowPoint.y);

                if (! isWindowAtPoint (viewWindow, screenPoint))
                    return false;
            }
        }

        const auto pointInSuperview = std::invoke ([&]
        {
            const auto local = NSMakePoint (localPos.x, localPos.y);

            if (auto* superview = [view superview])
                return [view convertPoint: local toView: superview];

            return local;
        });

        NSView* v = [view hitTest: pointInSuperview];

        return trueIfInAChildWindow ? (v != nil)
                                    : (v == view);
    }

    OptionalBorderSize getFrameSizeIfPresent() const override
    {
        if (! isSharedWindow)
        {
            BorderSize<int> b;

            NSRect v = [view convertRect: [view frame] toView: nil];
            NSRect w = [window frame];

            b.setTop ((int) (w.size.height - (v.origin.y + v.size.height)));
            b.setBottom ((int) v.origin.y);
            b.setLeft ((int) v.origin.x);
            b.setRight ((int) (w.size.width - (v.origin.x + v.size.width)));

            return OptionalBorderSize { b };
        }

        return {};
    }

    BorderSize<int> getFrameSize() const override
    {
        if (const auto frameSize = getFrameSizeIfPresent())
            return *frameSize;

        return {};
    }

    bool hasNativeTitleBar() const
    {
        return (getStyleFlags() & windowHasTitleBar) != 0;
    }

    bool setAlwaysOnTop (bool alwaysOnTop) override
    {
        if (! isSharedWindow)
        {
            [window setLevel: alwaysOnTop ? ((getStyleFlags() & windowIsTemporary) != 0 ? NSPopUpMenuWindowLevel
                                                                                        : NSFloatingWindowLevel)
                                          : NSNormalWindowLevel];

            isAlwaysOnTop = alwaysOnTop;
        }

        return true;
    }

    void toFront (bool makeActiveWindow) override
    {
        if (isSharedWindow)
        {
            NSView* superview = [view superview];
            NSMutableArray* subviews = [NSMutableArray arrayWithArray: [superview subviews]];

            const auto isFrontmost = [[subviews lastObject] isEqual: view];

            if (! isFrontmost)
            {
                [view retain];
                [subviews removeObject: view];
                [subviews addObject: view];

                [superview setSubviews: subviews];
                [view release];
            }
        }

        if (window != nil && component.isVisible())
        {
            ++insideToFrontCall;
            orderedBelowParent = false;

            if (makeActiveWindow && ! inBecomeKeyWindow && [window canBecomeKeyWindow])
                [window makeKeyAndOrderFront: nil];
            else
                [window orderFront: nil];

            // Reattaching moves a child to the front of its siblings
            if ([window parentWindow] != nil)
            {
                detachFromParentWindow (window);
                attachToParentWindow();
            }

            if (insideToFrontCall <= 1)
            {
                Desktop::getInstance().getMainMouseSource().forceMouseCursorUpdate();
                handleBroughtToFront();
            }

            --insideToFrontCall;
        }
    }

    void toBehind (ComponentPeer* other) override
    {
        if (auto* otherPeer = dynamic_cast<NSViewComponentPeer*> (other))
        {
            if (isSharedWindow)
            {
                NSView* superview = [view superview];
                NSMutableArray* subviews = [NSMutableArray arrayWithArray: [superview subviews]];

                const auto otherViewIndex = [subviews indexOfObject: otherPeer->view];

                if (otherViewIndex == NSNotFound)
                    return;

                const auto isBehind = [subviews indexOfObject: view] < otherViewIndex;

                if (! isBehind)
                {
                    [view retain];
                    [subviews removeObject: view];
                    [subviews insertObject: view
                                   atIndex: otherViewIndex];

                    [superview setSubviews: subviews];
                    [view release];
                }
            }
            else if (otherPeer->window == findParentWindow (*this, window))
            {
                if (std::exchange (orderedBelowParent, true) && [window parentWindow] == otherPeer->window)
                    return;

                if (component.isVisible())
                {
                    detachFromParentWindow (window);
                    attachToParentWindow();
                }
            }
            else if (component.isVisible())
            {
                // Going behind a sibling means reattaching the children in the new order
                if (auto* parent = [window parentWindow])
                {
                    if ([otherPeer->window parentWindow] == parent)
                    {
                        if (NSArray<NSWindow*>* children = [parent childWindows])
                        {
                            NSMutableArray<NSWindow*>* order = [NSMutableArray arrayWithArray: children];
                            [order removeObject: window];
                            [order insertObject: window atIndex: [order indexOfObject: otherPeer->window]];
                            reattachChildWindows (parent, order);
                        }
                    }

                    return;
                }

                [window orderWindow: NSWindowBelow
                         relativeTo: [otherPeer->window windowNumber]];
            }
        }
        else
        {
            jassertfalse; // wrong type of window?
        }
    }

    void setIcon (const Image& newIcon) override
    {
        if (! isSharedWindow)
        {
            // need to set a dummy represented file here to show the file icon (which we then set to the new icon)
            if (! windowRepresentsFile)
                [window setRepresentedFilename: juceStringToNS (" ")]; // can't just use an empty string for some reason...

            auto img = NSUniquePtr<NSImage> { imageToNSImage (ScaledImage (newIcon)) };
            [[window standardWindowButton: NSWindowDocumentIconButton] setImage: img.get()];
        }
    }

    StringArray getAvailableRenderingEngines() override
    {
        StringArray s ("Software Renderer");

       #if USE_COREGRAPHICS_RENDERING
        s.add ("CoreGraphics Renderer");
       #endif

        return s;
    }

    int getCurrentRenderingEngine() const override
    {
        return usingCoreGraphics ? 1 : 0;
    }

    void setCurrentRenderingEngine ([[maybe_unused]] int index) override
    {
       #if USE_COREGRAPHICS_RENDERING
        if (usingCoreGraphics != (index > 0))
        {
            usingCoreGraphics = index > 0;
            component.repaint();
        }
       #endif
    }

    void redirectMouseDown (NSEvent* ev)
    {
        if (! Process::isForegroundProcess())
            Process::makeForegroundProcess();

        ModifierKeys::currentModifiers = ModifierKeys::getCurrentModifiers().withFlags (getModifierForButtonNumber ([ev buttonNumber]));
        sendMouseEvent (ev);
    }

    void redirectMouseUp (NSEvent* ev)
    {
        ModifierKeys::currentModifiers = ModifierKeys::getCurrentModifiers().withoutFlags (getModifierForButtonNumber ([ev buttonNumber]));
        sendMouseEvent (ev);
        showArrowCursorIfNeeded();
    }

    void redirectMouseDrag (NSEvent* ev)
    {
        // Very rarely we seem to receive mouseDragged messages in between draggingEntered and draggingExited
        // messages. If our drag target is in a viewport, it can cause it to scroll around, so we ignore these
        // stray mouseDragged messages.
        if (draggingActive)
            return;

        ModifierKeys::currentModifiers = ModifierKeys::getCurrentModifiers().withFlags (getModifierForButtonNumber ([ev buttonNumber]));
        sendMouseEvent (ev);
    }

    void redirectMouseMove (NSEvent* ev)
    {
        ModifierKeys::currentModifiers = ModifierKeys::getCurrentModifiers().withoutMouseButtons();

        NSPoint windowPos = [ev locationInWindow];
        NSPoint screenPos = [[ev window] convertRectToScreen: NSMakeRect (windowPos.x, windowPos.y, 1.0f, 1.0f)].origin;

        if (isWindowAtPoint ([ev window], screenPos))
        {
            if (contains (getMousePos (ev, view).roundToInt(), false))
                sendMouseEvent (ev);
        }
        else
            // moved into another window which overlaps this one, so trigger an exit
            handleMouseEvent (MouseInputSource::InputSourceType::mouse, MouseInputSource::offscreenMousePos, ModifierKeys::getCurrentModifiers(),
                              getMousePressure (ev), MouseInputSource::defaultOrientation, getMouseTime (ev));

        showArrowCursorIfNeeded();
    }

    void redirectMouseEnter (NSEvent* ev)
    {
        sendMouseEnterExit (ev);
    }

    void redirectMouseExit (NSEvent* ev)
    {
        sendMouseEnterExit (ev);
    }

    static float checkDeviceDeltaReturnValue (float v) noexcept
    {
        // (deviceDeltaX can fail and return NaN, so need to sanity-check the result)
        v *= 0.5f / 256.0f;
        return (v > -1000.0f && v < 1000.0f) ? v : 0.0f;
    }

    void redirectMouseWheel (NSEvent* ev)
    {
        updateModifiers (ev);

        MouseWheelDetails wheel;
        wheel.deltaX = 0;
        wheel.deltaY = 0;
        wheel.isReversed = false;
        wheel.isSmooth = false;
        wheel.isInertial = false;

        @try
        {
            if ([ev respondsToSelector: @selector (isDirectionInvertedFromDevice)])
                wheel.isReversed = [ev isDirectionInvertedFromDevice];

            wheel.isInertial = ([ev momentumPhase] != NSEventPhaseNone);

            if ([ev respondsToSelector: @selector (hasPreciseScrollingDeltas)])
            {
                if ([ev hasPreciseScrollingDeltas])
                {
                    const float scale = 0.5f / 256.0f;
                    wheel.deltaX = scale * (float) [ev scrollingDeltaX];
                    wheel.deltaY = scale * (float) [ev scrollingDeltaY];
                    wheel.isSmooth = true;
                }
            }
            else if ([ev respondsToSelector: @selector (deviceDeltaX)])
            {
                wheel.deltaX = checkDeviceDeltaReturnValue ([ev deviceDeltaX]);
                wheel.deltaY = checkDeviceDeltaReturnValue ([ev deviceDeltaY]);
            }
        }
        @catch (...)
        {}

        if (wheel.deltaX == 0.0f && wheel.deltaY == 0.0f)
        {
            const float scale = 10.0f / 256.0f;
            wheel.deltaX = scale * (float) [ev deltaX];
            wheel.deltaY = scale * (float) [ev deltaY];
        }

        handleMouseWheel (MouseInputSource::InputSourceType::mouse, getMousePos (ev, view), getMouseTime (ev), wheel);
    }

    void redirectMagnify (NSEvent* ev)
    {
        const float invScale = 1.0f - (float) [ev magnification];

        if (invScale > 0.0f)
            handleMagnifyGesture (MouseInputSource::InputSourceType::mouse, getMousePos (ev, view), getMouseTime (ev), 1.0f / invScale);
    }

    void redirectCopy      (NSObject*) { handleKeyPress (KeyPress ('c', ModifierKeys (ModifierKeys::commandModifier), 'c')); }
    void redirectPaste     (NSObject*) { handleKeyPress (KeyPress ('v', ModifierKeys (ModifierKeys::commandModifier), 'v')); }
    void redirectCut       (NSObject*) { handleKeyPress (KeyPress ('x', ModifierKeys (ModifierKeys::commandModifier), 'x')); }
    void redirectSelectAll (NSObject*) { handleKeyPress (KeyPress ('a', ModifierKeys (ModifierKeys::commandModifier), 'a')); }

    void redirectWillMoveToWindow (NSWindow* newWindow)
    {
        windowObservers.clear();

        if (isSharedWindow && [view window] == window && newWindow == nullptr)
        {
            if (auto* comp = safeComponent.get())
                comp->setVisible (false);
        }
    }

    void sendMouseEvent (NSEvent* ev)
    {
        updateModifiers (ev);
        handleMouseEvent (MouseInputSource::InputSourceType::mouse, getMousePos (ev, view), ModifierKeys::currentModifiers,
                          getMousePressure (ev), MouseInputSource::defaultOrientation, getMouseTime (ev));
    }

    bool handleKeyEvent (NSEvent* ev, bool isKeyDown)
    {
        auto unicode = nsStringToJuce ([ev characters]);
        auto keyCode = getKeyCodeFromEvent (ev);

       #if JUCE_DEBUG_KEYCODES
        DBG ("unicode: " + unicode + " " + String::toHexString ((int) unicode[0]));
        auto unmodified = nsStringToJuce ([ev charactersIgnoringModifiers]);
        DBG ("unmodified: " + unmodified + " " + String::toHexString ((int) unmodified[0]));
       #endif

        if (keyCode != 0 || unicode.isNotEmpty())
        {
            if (isKeyDown)
            {
                bool used = false;

                for (auto textCharacter : unicode)
                {
                    switch (keyCode)
                    {
                        case NSLeftArrowFunctionKey:
                        case NSRightArrowFunctionKey:
                        case NSUpArrowFunctionKey:
                        case NSDownArrowFunctionKey:
                        case NSPageUpFunctionKey:
                        case NSPageDownFunctionKey:
                        case NSEndFunctionKey:
                        case NSHomeFunctionKey:
                        case NSDeleteFunctionKey:
                            textCharacter = 0;
                            break; // (these all seem to generate unwanted garbage unicode strings)

                        default:
                            if (([ev modifierFlags] & NSEventModifierFlagCommand) != 0
                                 || (keyCode >= NSF1FunctionKey && keyCode <= NSF35FunctionKey))
                                textCharacter = 0;
                            break;
                    }

                    used |= handleKeyUpOrDown (true);
                    used |= handleKeyPress (keyCode, textCharacter);
                }

                return used;
            }

            if (handleKeyUpOrDown (false))
                return true;
        }

        return false;
    }

    bool redirectKeyDown (NSEvent* ev)
    {
        // (need to retain this in case a modal loop runs in handleKeyEvent and
        // our event object gets lost)
        const NSUniquePtr<NSEvent> r ([ev retain]);

        updateKeysDown (ev, true);
        bool used = handleKeyEvent (ev, true);

        if (([ev modifierFlags] & NSEventModifierFlagCommand) != 0)
        {
            // for command keys, the key-up event is thrown away, so simulate one
            updateKeysDown (ev, false);
            used = (isValidPeer (this) && handleKeyEvent (ev, false)) || used;
        }

        // If we're running modally, don't allow unused keystrokes to be passed
        // along to other blocked views.
        if (Component::getCurrentlyModalComponent() != nullptr)
            used = true;

        return used;
    }

    bool redirectKeyUp (NSEvent* ev)
    {
        updateKeysDown (ev, false);
        return handleKeyEvent (ev, false)
                || Component::getCurrentlyModalComponent() != nullptr;
    }

    void redirectModKeyChange (NSEvent* ev)
    {
        // (need to retain this in case a modal loop runs and our event object gets lost)
        const NSUniquePtr<NSEvent> r ([ev retain]);

        keysCurrentlyDown.clear();
        handleKeyUpOrDown (true);

        updateModifiers (ev);
        handleModifierKeysChange();
    }

    //==============================================================================
    void drawRect (NSRect dirty)
    {
        auto* cg = (CGContextRef) [[NSGraphicsContext currentContext] CGContext];

        deferredRepaints.subtract (Rectangle { dirty.origin.x,
                                               dirty.origin.y,
                                               dirty.size.width,
                                               dirty.size.height }.toFloat());

        drawRectWithContext (cg, dirty);
    }

    void drawRectWithContext (CGContextRef cg, NSRect r)
    {
        if (! component.isOpaque())
            CGContextClearRect (cg, CGContextGetClipBoundingBox (cg));

        float displayScale = 1.0f;
        NSScreen* screen = [[view window] screen];

        if ([screen respondsToSelector: @selector (backingScaleFactor)])
            displayScale = (float) screen.backingScaleFactor;

        auto invalidateTransparentWindowShadow = [this]
        {
            // transparent NSWindows with a drop-shadow need to redraw their shadow when the content
            // changes to avoid stale shadows being drawn behind the window
            if (! isSharedWindow && ! [window isOpaque] && [window hasShadow])
                [window invalidateShadow];
        };

       #if USE_COREGRAPHICS_RENDERING && JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS
        // This was a workaround for a CGContext not having a way of finding whether a rectangle
        // falls within its clip region. However, Apple removed the capability of
        // [view getRectsBeingDrawn: ...] sometime around 10.13, so on later versions of macOS
        // numRects will always be 1, and you'll need to use a CoreGraphicsMetalLayerRenderer
        // to avoid CoreGraphics consolidating disparate rects.
        if (usingCoreGraphics && metalRenderer == nullptr)
        {
            const NSRect* rects = nullptr;
            NSInteger numRects = 0;
            [view getRectsBeingDrawn: &rects count: &numRects];

            if (numRects > 1)
            {
                for (int i = 0; i < numRects; ++i)
                {
                    NSRect rect = rects[i];
                    CGContextSaveGState (cg);
                    CGContextClipToRect (cg, CGRectMake (rect.origin.x, rect.origin.y, rect.size.width, rect.size.height));
                    renderRect (cg, rect, displayScale);
                    CGContextRestoreGState (cg);
                }

                invalidateTransparentWindowShadow();
                return;
            }
        }
       #endif

        renderRect (cg, r, displayScale);
        invalidateTransparentWindowShadow();
    }

    void renderRect (CGContextRef cg, NSRect r, float displayScale)
    {
        CGContextSaveGState (cg);
        const ScopeGuard pop { [&] { CGContextRestoreGState (cg); } };

       #if USE_COREGRAPHICS_RENDERING
        if (usingCoreGraphics)
        {
            const auto height = getComponent().getHeight();
            CGContextConcatCTM (cg, CGAffineTransformMake (1, 0, 0, -1, 0, height));
            CoreGraphicsContext context (cg, (float) height);
            handlePaint (context);
        }
        else
       #endif
        {
            const Point<int> offset (-roundToInt (r.origin.x), -roundToInt (r.origin.y));
            auto clipW = (int) (r.size.width  + 0.5f);
            auto clipH = (int) (r.size.height + 0.5f);

            RectangleList<int> clip;
            getClipRects (clip, offset, clipW, clipH);

            if (clip.isEmpty())
                clip.add (Rectangle { r.size.width, r.size.height }.getSmallestIntegerContainer());

            if (! clip.isEmpty())
            {
                Image temp (component.isOpaque() ? Image::RGB : Image::ARGB,
                            roundToInt ((float) clipW * displayScale),
                            roundToInt ((float) clipH * displayScale),
                            ! component.isOpaque());

                {
                    auto intScale = roundToInt (displayScale);

                    if (intScale != 1)
                        clip.scaleAll (intScale);

                    auto context = component.getLookAndFeel()
                                            .createGraphicsContext (temp, offset * intScale, clip);

                    if (intScale != 1)
                        context->addTransform (AffineTransform::scale (displayScale));

                    handlePaint (*context);
                }

                detail::ColorSpacePtr colourSpace { CGColorSpaceCreateWithName (kCGColorSpaceSRGB) };
                CGImageRef image = juce_createCoreGraphicsImage (temp, colourSpace.get());
                CGContextConcatCTM (cg, CGAffineTransformMake (1, 0, 0, -1, r.origin.x, r.origin.y + clipH));
                CGContextDrawImage (cg, CGRectMake (0.0f, 0.0f, clipW, clipH), image);
                CGImageRelease (image);
            }
        }
    }

    void repaint (const Rectangle<int>& area) override
    {
        // In 10.11 changes were made to the way the OS handles repaint regions, and it seems that it can
        // no longer be trusted to coalesce all the regions, or to even remember them all without losing
        // a few when there's a lot of activity.
        // As a workaround for this, we use a RectangleList to do our own coalescing of regions before
        // asynchronously asking the OS to repaint them.
        deferredRepaints.add (area.toFloat());
        setNeedsDisplayRectangles();
    }

    static bool shouldThrottleRepaint()
    {
        return areAnyWindowsInLiveResize();
    }

    void onVBlank (double timestampSec)
    {
        callVBlankListeners (timestampSec);
        setNeedsDisplayRectangles();
    }

    void setNeedsDisplayRectangles()
    {
        if (deferredRepaints.isEmpty())
            return;

        auto now = Time::getMillisecondCounter();
        auto msSinceLastRepaint = (lastRepaintTime >= now) ? now - lastRepaintTime
                                                           : (std::numeric_limits<uint32>::max() - lastRepaintTime) + now;

        constexpr uint32 minimumRepaintInterval = 1000 / 30; // 30fps

        // When windows are being resized, artificially throttling high-frequency repaints helps
        // to stop the event queue getting clogged, and keeps everything working smoothly.
        // For some reason Logic also needs this throttling to record parameter events correctly.
        if (msSinceLastRepaint < minimumRepaintInterval && shouldThrottleRepaint())
            return;

        for (auto& i : deferredRepaints)
            [view setNeedsDisplayInRect: makeCGRect (i)];

        lastRepaintTime = Time::getMillisecondCounter();
    }

    void performAnyPendingRepaintsNow() override
    {
        [view displayIfNeeded];
    }

    static bool areAnyWindowsInLiveResize() noexcept
    {
        for (NSWindow* w in [NSApp windows])
            if ([w inLiveResize])
                return true;

        return false;
    }

    //==============================================================================
    bool isBlockedByModalComponent()
    {
        if (auto* modal = Component::getCurrentlyModalComponent())
        {
            if (insideToFrontCall == 0
                 && (! getComponent().isParentOf (modal))
                 && getComponent().isCurrentlyBlockedByAnotherModalComponent())
            {
                return true;
            }
        }

        return false;
    }

    enum class KeyWindowChanged { no, yes };

    void sendModalInputAttemptIfBlocked (KeyWindowChanged keyChanged)
    {
        if (! isBlockedByModalComponent())
            return;

        if (auto* modal = Component::getCurrentlyModalComponent())
        {
            if (auto* otherPeer = modal->getPeer())
            {
                const auto modalPeerIsTemporary = (otherPeer->getStyleFlags() & ComponentPeer::windowIsTemporary) != 0;

                if (! modalPeerIsTemporary)
                    return;

                // When a peer resigns key status, it might be because we just created a modal
                // component that is now key.
                // In this case, we should only dismiss the modal component if it isn't key,
                // implying that a third window has become key.
                const auto modalPeerIsKey = [NSApp keyWindow] == static_cast<NSViewComponentPeer*> (otherPeer)->window;

                if (keyChanged == KeyWindowChanged::yes && modalPeerIsKey)
                    return;

                modal->inputAttemptWhenModal();
            }
        }
    }

    bool canBecomeKeyWindow()
    {
        return component.isVisible() && (getStyleFlags() & ComponentPeer::windowIgnoresKeyPresses) == 0;
    }

    bool canBecomeMainWindow()
    {
        return component.isVisible() && dynamic_cast<ResizableWindow*> (&component) != nullptr;
    }

    bool worksWhenModal() const
    {
        // In plugins, the host could put our plugin window inside a modal window, so this
        // allows us to successfully open other popups. Feels like there could be edge-case
        // problems caused by this, so let us know if you spot any issues.
        return ! JUCEApplication::isStandaloneApp();
    }

    void becomeKeyWindow()
    {
        handleBroughtToFront();
        grabFocus();
    }

    void resignKeyWindow()
    {
        viewFocusLoss();
    }

    bool windowShouldClose()
    {
        if (! isValidPeer (this))
            return YES;

        handleUserClosingWindow();
        return NO;
    }

    void redirectMovedOrResized()
    {
        updateOffsetFromParentWindow();
        handleMovedOrResized();
        setNeedsDisplayRectangles();
    }

    // AppKit moves a child window along with its parent, and the clamping of each of those moves
    // to the screen would accumulate as the parent is moved across a screen edge, so the offset
    // of the child from its parent is recorded, and a move that keeps it is left unconstrained by
    // constrainFrameRect:toScreen:.
    void updateOffsetFromParentWindow()
    {
        offsetFromParentWindow = std::invoke ([&]() -> std::optional<Point<double>>
        {
            if (auto* parent = [window parentWindow])
                return Point<double> ([window frame].origin.x - [parent frame].origin.x,
                                      [window frame].origin.y - [parent frame].origin.y);

            return {};
        });
    }

    void viewMovedToWindow()
    {
        if (isSharedWindow)
        {
            auto newWindow = [view window];
            bool shouldSetVisible = (window == nullptr && newWindow != nullptr);

            window = newWindow;

            if (shouldSetVisible)
                getComponent().setVisible (true);
        }

        if (auto* currentWindow = [view window])
        {
            windowObservers.emplace_back (view, dismissModalsSelector, NSWindowWillMoveNotification, currentWindow);
            windowObservers.emplace_back (view, dismissModalsSelector, NSWindowWillMiniaturizeNotification, currentWindow);
            windowObservers.emplace_back (view, becomeKeySelector, NSWindowDidBecomeKeyNotification, currentWindow);
            windowObservers.emplace_back (view, resignKeySelector, NSWindowDidResignKeyNotification, currentWindow);
        }
    }

    void dismissModals()
    {
        if (hasNativeTitleBar() || isSharedWindow)
            sendModalInputAttemptIfBlocked (KeyWindowChanged::no);
    }

    void becomeKey()
    {
        component.repaint();
    }

    void resignKey()
    {
        viewFocusLoss();
        sendModalInputAttemptIfBlocked (KeyWindowChanged::yes);
    }

    void liveResizingStart()
    {
        if (constrainer == nullptr)
            return;

        constrainer->resizeStart();
        isFirstLiveResize = true;

        setFullScreenSizeConstraints (*constrainer);
    }

    void liveResizingEnd()
    {
        if (constrainer != nullptr)
            constrainer->resizeEnd();
    }

    NSRect constrainRect (const NSRect r)
    {
        if (constrainer == nullptr || isKioskMode() || isFullScreen())
            return r;

        const auto scale = getComponent().getDesktopScaleFactor();

        auto pos            = detail::ScalingHelpers::unscaledScreenPosToScaled (scale, convertToRectInt (flippedScreenRect (r)));
        const auto original = detail::ScalingHelpers::unscaledScreenPosToScaled (scale, convertToRectInt (flippedScreenRect ([window frame])));

        const auto screenBounds = Desktop::getInstance().getDisplays().getTotalBounds (true);

        const bool inLiveResize = [window inLiveResize];

        if (! inLiveResize || isFirstLiveResize)
        {
            isFirstLiveResize = false;

            isStretchingTop    = (pos.getY() != original.getY() && pos.getBottom() == original.getBottom());
            isStretchingLeft   = (pos.getX() != original.getX() && pos.getRight()  == original.getRight());
            isStretchingBottom = (pos.getY() == original.getY() && pos.getBottom() != original.getBottom());
            isStretchingRight  = (pos.getX() == original.getX() && pos.getRight()  != original.getRight());
        }

        constrainer->checkBounds (pos, original, screenBounds,
                                  isStretchingTop, isStretchingLeft, isStretchingBottom, isStretchingRight);

        return flippedScreenRect (makeCGRect (detail::ScalingHelpers::scaledScreenPosToUnscaled (scale, pos)));
    }

    static void showArrowCursorIfNeeded()
    {
        auto& desktop = Desktop::getInstance();
        auto mouse = desktop.getMainMouseSource();

        if (mouse.getComponentUnderMouse() == nullptr
             && desktop.findComponentAt (mouse.getScreenPosition().roundToInt()) == nullptr)
        {
            [[NSCursor arrowCursor] set];
        }
    }

    static void updateModifiers (NSEvent* e)
    {
        updateModifiers ([e modifierFlags]);
    }

    static void updateModifiers (const NSUInteger flags)
    {
        int m = 0;

        if ((flags & NSEventModifierFlagShift) != 0)        m |= ModifierKeys::shiftModifier;
        if ((flags & NSEventModifierFlagControl) != 0)      m |= ModifierKeys::ctrlModifier;
        if ((flags & NSEventModifierFlagOption) != 0)       m |= ModifierKeys::altModifier;
        if ((flags & NSEventModifierFlagCommand) != 0)      m |= ModifierKeys::commandModifier;

        ModifierKeys::currentModifiers = ModifierKeys::getCurrentModifiers().withOnlyMouseButtons().withFlags (m);
    }

    static void updateKeysDown (NSEvent* ev, bool isKeyDown)
    {
        updateModifiers (ev);

        if (auto keyCode = getKeyCodeFromEvent (ev))
        {
            if (isKeyDown)
                keysCurrentlyDown.insert (keyCode);
            else
                keysCurrentlyDown.erase (keyCode);
        }
    }

    static int getKeyCodeFromEvent (NSEvent* ev)
    {
        // Unfortunately, charactersIgnoringModifiers does not ignore the shift key.
        // Using [ev keyCode] is not a solution either as this will,
        // for example, return VK_KEY_Y if the key is pressed which
        // is typically located at the Y key position on a QWERTY
        // keyboard. However, on international keyboards this might not
        // be the key labeled Y (for example, on German keyboards this key
        // has a Z label). Therefore, we need to query the current keyboard
        // layout to figure out what character the key would have produced
        // if the shift key was not pressed
        String unmodified = nsStringToJuce ([ev charactersIgnoringModifiers]);
        auto keyCode = (int) unmodified[0];

        if (keyCode == 0x19) // (backwards-tab)
            keyCode = '\t';
        else if (keyCode == 0x03) // (enter)
            keyCode = '\r';
        else
            keyCode = (int) CharacterFunctions::toUpperCase ((juce_wchar) keyCode);

        // The purpose of the keyCode is to provide information about non-printing characters to facilitate
        // keyboard control over the application.
        //
        // So when keyCode is decoded as a printing character outside the ASCII range we need to replace it.
        // This holds when the keyCode is larger than 0xff and not part one of the two MacOS specific
        // non-printing ranges.
        if (keyCode > 0xff
            && ! (keyCode >= NSUpArrowFunctionKey && keyCode <= NSModeSwitchFunctionKey)
            && ! (keyCode >= extendedKeyModifier))
        {
            keyCode = translateVirtualToAsciiKeyCode ([ev keyCode]);
        }

        if (([ev modifierFlags] & NSEventModifierFlagNumericPad) != 0)
        {
            const int numPadConversions[] = { '0', KeyPress::numberPad0, '1', KeyPress::numberPad1,
                                              '2', KeyPress::numberPad2, '3', KeyPress::numberPad3,
                                              '4', KeyPress::numberPad4, '5', KeyPress::numberPad5,
                                              '6', KeyPress::numberPad6, '7', KeyPress::numberPad7,
                                              '8', KeyPress::numberPad8, '9', KeyPress::numberPad9,
                                              '+', KeyPress::numberPadAdd, '-', KeyPress::numberPadSubtract,
                                              '*', KeyPress::numberPadMultiply, '/', KeyPress::numberPadDivide,
                                              '.', KeyPress::numberPadDecimalPoint,
                                              ',', KeyPress::numberPadDecimalPoint, // (to deal with non-english kbds)
                                              '=', KeyPress::numberPadEquals };

            for (int i = 0; i < numElementsInArray (numPadConversions); i += 2)
                if (keyCode == numPadConversions [i])
                    keyCode = numPadConversions [i + 1];
        }

        return keyCode;
    }

    static int64 getMouseTime (NSEvent* e) noexcept
    {
        return (Time::currentTimeMillis() - Time::getMillisecondCounter())
                 + (int64) ([e timestamp] * 1000.0);
    }

    static float getMousePressure (NSEvent* e) noexcept
    {
        @try
        {
            if (e.type != NSEventTypeMouseEntered && e.type != NSEventTypeMouseExited)
                return (float) e.pressure;
        }
        @catch (NSException* e) {}
        @finally {}

        return 0.0f;
    }

    static Point<float> getMousePos (NSEvent* e, NSView* view)
    {
        NSPoint p = [view convertPoint: [e locationInWindow] fromView: nil];
        return { (float) p.x, (float) p.y };
    }

    static int getModifierForButtonNumber (const NSInteger num)
    {
        switch (num)
        {
            case 0: return ModifierKeys::leftButtonModifier;
            case 1: return ModifierKeys::rightButtonModifier;
            case 2: return ModifierKeys::middleButtonModifier;
            case 3: return ModifierKeys::backButtonModifier;
            case 4: return ModifierKeys::forwardButtonModifier;
        }

        return 0;
    }

    static unsigned int getNSWindowStyleMask (const int flags) noexcept
    {
        unsigned int style = (flags & windowHasTitleBar) != 0 ? NSWindowStyleMaskTitled
                                                              : NSWindowStyleMaskBorderless;

        if ((flags & windowHasMinimiseButton) != 0)  style |= NSWindowStyleMaskMiniaturizable;
        if ((flags & windowHasCloseButton) != 0)     style |= NSWindowStyleMaskClosable;
        if ((flags & windowIsResizable) != 0)        style |= NSWindowStyleMaskResizable;
        return style;
    }

    static NSArray* getSupportedDragTypes()
    {
        const auto type = []
        {
            if (@available (macOS 10.13, *))
                return NSPasteboardTypeFileURL;

            JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
            return (NSString*) kUTTypeFileURL;
            JUCE_END_IGNORE_DEPRECATION_WARNINGS
        }();

        return [NSArray arrayWithObjects: type, (NSString*) kPasteboardTypeFileURLPromise, NSPasteboardTypeString, nil];
    }

    BOOL sendDragCallback (bool (ComponentPeer::* callback) (const DragInfo&), id <NSDraggingInfo> sender)
    {
        if (callback == &NSViewComponentPeer::handleDragMove)
            draggingActive = true;

        if (callback == &NSViewComponentPeer::handleDragExit || callback == &NSViewComponentPeer::handleDragDrop)
            draggingActive = false;

        NSPasteboard* pasteboard = [sender draggingPasteboard];
        NSString* contentType = [pasteboard availableTypeFromArray: getSupportedDragTypes()];

        if (contentType == nil)
            return false;

        const auto p = localToGlobal (convertToPointFloat ([view convertPoint: [sender draggingLocation] fromView: nil]));

        ComponentPeer::DragInfo dragInfo;
        dragInfo.position = detail::ScalingHelpers::screenPosToLocalPos (component, p).roundToInt();

        if (contentType == NSPasteboardTypeString)
            dragInfo.text = nsStringToJuce ([pasteboard stringForType: NSPasteboardTypeString]);
        else
            dragInfo.files = getDroppedFiles (pasteboard, contentType);

        if (! dragInfo.isEmpty())
            return (this->*callback) (dragInfo);

        return false;
    }

    StringArray getDroppedFiles (NSPasteboard* pasteboard, NSString* contentType)
    {
        StringArray files;
        NSString* iTunesPasteboardType = nsStringLiteral ("CorePasteboardFlavorType 0x6974756E"); // 'itun'

        if ([contentType isEqualToString: (NSString*) kPasteboardTypeFileURLPromise]
             && [[pasteboard types] containsObject: iTunesPasteboardType])
        {
            id list = [pasteboard propertyListForType: iTunesPasteboardType];

            if ([list isKindOfClass: [NSDictionary class]])
            {
                NSDictionary* iTunesDictionary = (NSDictionary*) list;
                NSArray* tracks = [iTunesDictionary valueForKey: nsStringLiteral ("Tracks")];
                NSEnumerator* enumerator = [tracks objectEnumerator];
                NSDictionary* track;

                while ((track = [enumerator nextObject]) != nil)
                {
                    if (id value = [track valueForKey: nsStringLiteral ("Location")])
                    {
                        NSURL* url = [NSURL URLWithString: value];

                        if ([url isFileURL])
                            files.add (nsStringToJuce ([url path]));
                    }
                }
            }
        }
        else
        {
            NSArray* items = [pasteboard readObjectsForClasses:@[[NSURL class]] options: nil];

            for (unsigned int i = 0; i < [items count]; ++i)
            {
                NSURL* url = [items objectAtIndex: i];

                if ([url isFileURL])
                    files.add (nsStringToJuce ([url path]));
            }
        }

        return files;
    }

    //==============================================================================
    void viewFocusGain()
    {
        if (currentlyFocusedPeer != this)
        {
            if (ComponentPeer::isValidPeer (currentlyFocusedPeer))
                currentlyFocusedPeer->handleFocusLoss();

            currentlyFocusedPeer = this;
            handleFocusGain();
        }
    }

    void viewFocusLoss()
    {
        if (currentlyFocusedPeer == this)
        {
            currentlyFocusedPeer = nullptr;
            handleFocusLoss();
        }
    }

    bool isFocused() const override
    {
        return (isSharedWindow || ! JUCEApplication::isStandaloneApp())
                    ? this == currentlyFocusedPeer
                    : [window isKeyWindow];
    }

    void grabFocus() override
    {
        if (window != nil)
        {
            if (! inBecomeKeyWindow && [window canBecomeKeyWindow])
                [window makeKeyWindow];

            [window makeFirstResponder: view];

            viewFocusGain();
        }
    }

    void textInputRequired (Point<int>, TextInputTarget&) override {}

    void closeInputMethodContext() override
    {
        stringBeingComposed.clear();

        if (const auto* inputContext = [view inputContext])
            [inputContext discardMarkedText];
    }

    void resetWindowPresentation()
    {
        [window setStyleMask: (NSViewComponentPeer::getNSWindowStyleMask (getStyleFlags()))];

        if (hasNativeTitleBar())
            setTitle (getComponent().getName()); // required to force the OS to update the title

        [NSApp setPresentationOptions: NSApplicationPresentationDefault];
        setCollectionBehaviour (isFullScreen());
    }

    void setHasChangedSinceSaved (bool b) override
    {
        if (! isSharedWindow)
            [window setDocumentEdited: b];
    }

    bool sendEventToInputContextOrComponent (NSEvent* ev)
    {
        // In the case that an event was processed unsuccessfully in performKeyEquivalent and then
        // posted back to keyDown by the system, this check will ensure that we don't attempt to
        // process the same event a second time.
        const auto newEvent = KeyEventAttributes::make (ev);

        if (std::exchange (lastSeenKeyEvent, newEvent) == newEvent)
            return false;

        // We assume that the event will be handled by the IME.
        // Occasionally, the inputContext may be sent key events like cmd+Q, which it will turn
        // into a noop: call and forward to doCommandBySelector:.
        // In this case, the event will be extracted from keyEventBeingHandled and passed to the
        // focused component, and viewCannotHandleEvent will be set depending on whether the event
        // was handled by the component.
        // If the event was *not* handled by the component, and was also not consumed completely by
        // the IME, it's important to return the event to the system for further handling, so that
        // the main menu works as expected.
        viewCannotHandleEvent = false;
        keyEventBeingHandled.reset ([ev retain]);
        const WeakReference ref { this };
        // redirectKeyDown may delete this peer!
        const ScopeGuard scope { [&ref] { if (ref != nullptr) ref->keyEventBeingHandled = nullptr; } };

        const auto handled = [&]() -> bool
        {
            if (findCurrentTextInputTarget() != nullptr)
                if (const auto* inputContext = [view inputContext])
                    return [inputContext handleEvent: ev] && ! viewCannotHandleEvent;

            return false;
        }();

        if (handled)
            return true;

        stringBeingComposed.clear();
        return redirectKeyDown (ev);
    }

    //==============================================================================
    class KeyEventAttributes
    {
        auto tie() const
        {
            return std::tie (type,
                             modifierFlags,
                             timestamp,
                             windowNumber,
                             characters,
                             charactersIgnoringModifiers,
                             keyCode,
                             isRepeat);
        }

    public:
        static std::optional<KeyEventAttributes> make (NSEvent* event)
        {
            const auto type = [event type];

            if (type != NSEventTypeKeyDown && type != NSEventTypeKeyUp)
                return {};

            return KeyEventAttributes
            {
                type,
                [event modifierFlags],
                [event timestamp],
                [event windowNumber],
                nsStringToJuce ([event characters]),
                nsStringToJuce ([event charactersIgnoringModifiers]),
                [event keyCode],
                static_cast<bool> ([event isARepeat])
            };
        }

        bool operator== (const KeyEventAttributes& other) const { return tie() == other.tie(); }
        bool operator!= (const KeyEventAttributes& other) const { return tie() != other.tie(); }

    private:
        KeyEventAttributes (NSEventType typeIn,
                            NSEventModifierFlags flagsIn,
                            NSTimeInterval timestampIn,
                            NSInteger windowNumberIn,
                            String charactersIn,
                            String charactersIgnoringModifiersIn,
                            unsigned short keyCodeIn,
                            bool isRepeatIn)
            : type (typeIn),
              modifierFlags (flagsIn),
              timestamp (timestampIn),
              windowNumber (windowNumberIn),
              characters (charactersIn),
              charactersIgnoringModifiers (charactersIgnoringModifiersIn),
              keyCode (keyCodeIn),
              isRepeat (isRepeatIn)
        {}

        NSEventType type;
        NSEventModifierFlags modifierFlags;
        NSTimeInterval timestamp;
        NSInteger windowNumber;
        String characters;
        String charactersIgnoringModifiers;
        unsigned short keyCode;
        bool isRepeat;
    };

    NSWindow* window = nil;
    NSView* view = nil;
    WeakReference<Component> safeComponent;
    const bool isSharedWindow = false;
   #if USE_COREGRAPHICS_RENDERING
    bool usingCoreGraphics = true;
   #else
    bool usingCoreGraphics = false;
   #endif
    NSUniquePtr<NSEvent> keyEventBeingHandled;
    bool isFirstLiveResize = false, viewCannotHandleEvent = false;
    bool orderedBelowParent = false;
    bool isStretchingTop = false, isStretchingLeft = false, isStretchingBottom = false, isStretchingRight = false;
    bool windowRepresentsFile = false;
    bool isAlwaysOnTop = false, wasAlwaysOnTop = false, inBecomeKeyWindow = false;
    std::optional<Point<double>> offsetFromParentWindow;
    bool inPerformKeyEquivalent = false;
    bool draggingActive = false;
    String stringBeingComposed;
    int startOfMarkedTextInTextInputTarget = 0;

    Rectangle<float> lastSizeBeforeZoom;
    RectangleList<float> deferredRepaints;
    uint32 lastRepaintTime;

    std::optional<KeyEventAttributes> lastSeenKeyEvent;

    static inline ComponentPeer* currentlyFocusedPeer;
    static inline std::set<int> keysCurrentlyDown;
    static inline int insideToFrontCall;

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wundeclared-selector")
    static inline const auto dismissModalsSelector  = @selector (dismissModals);
    static inline const auto frameChangedSelector   = @selector (frameChanged:);
    static inline const auto asyncMouseDownSelector = @selector (asyncMouseDown:);
    static inline const auto asyncMouseUpSelector   = @selector (asyncMouseUp:);
    static inline const auto becomeKeySelector      = @selector (becomeKey:);
    static inline const auto resignKeySelector      = @selector (resignKey:);
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

   #if JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS
    std::unique_ptr<CoreGraphicsMetalLayerRenderer> metalRenderer;
    NSUniquePtr<NSObject<CALayerDelegate>> layerDelegate;
   #endif

private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (NSViewComponentPeer)

    // Note: the OpenGLContext also has a SharedResourcePointer<PerScreenDisplayLinks> to
    // avoid unnecessarily duplicating display-link threads.
    SharedResourcePointer<PerScreenDisplayLinks> sharedDisplayLinks;

    class AsyncRepainter final : private AsyncUpdater
    {
    public:
        explicit AsyncRepainter (NSViewComponentPeer& o) : owner (o) {}
        ~AsyncRepainter() override { cancelPendingUpdate(); }

        void markUpdated (const CGDirectDisplayID x, double timestampSec)
        {
            {
                const std::scoped_lock lock { mutex };

                if (const auto it = std::find_if (backgroundVBlankEvents.cbegin(),
                                                  backgroundVBlankEvents.cend(),
                                                  [&x] (const auto& event) { return event.display == x; });
                    it == backgroundVBlankEvents.cend())
                {
                    backgroundVBlankEvents.push_back ({ x, timestampSec });
                }
            }

            triggerAsyncUpdate();
        }

    private:
        void handleAsyncUpdate() override
        {
            {
                const std::scoped_lock lock { mutex };
                mainThreadVBlankEvents = backgroundVBlankEvents;
                backgroundVBlankEvents.clear();
            }

            for (const auto& event : mainThreadVBlankEvents)
            {
                if (auto* peerView = owner.view)
                    if (auto* peerWindow = [peerView window])
                        if (event.display == ScopedDisplayLink::getDisplayIdForScreen ([peerWindow screen]))
                            owner.onVBlank (event.timeSec);
            }
        }

        struct VBlankEvent
        {
            CGDirectDisplayID display{};
            double timeSec{};
        };

        NSViewComponentPeer& owner;
        std::mutex mutex;
        std::vector<VBlankEvent> backgroundVBlankEvents, mainThreadVBlankEvents;
    };

    AsyncRepainter asyncRepainter { *this };

    /*  Creates a function object that can be called from an arbitrary thread (probably a CVLink
        thread). When called, this function object will trigger a call to setNeedsDisplayRectangles
        as soon as possible on the main thread, for any peers currently on the provided NSScreen.
    */
    PerScreenDisplayLinks::Connection connection
    {
        sharedDisplayLinks->registerFactory ([this] (CGDirectDisplayID display)
        {
            return [this, display] (double timestampSec)
                   {
                       asyncRepainter.markUpdated (display, timestampSec);
                   };
        })
    };

    static NSView* createViewInstance();
    static NSWindow* createWindowInstance();

    void sendMouseEnterExit (NSEvent* ev)
    {
        if (auto* area = [ev trackingArea])
            if (! [[view trackingAreas] containsObject: area])
                return;

        if ([NSEvent pressedMouseButtons] == 0)
            sendMouseEvent (ev);
    }

    void getClipRects (RectangleList<int>& clip, Point<int> offset, int clipW, int clipH)
    {
        const NSRect* rects = nullptr;
        NSInteger numRects = 0;
        [view getRectsBeingDrawn: &rects count: &numRects];

        const Rectangle<int> clipBounds (clipW, clipH);

        clip.ensureStorageAllocated ((int) numRects);

        for (int i = 0; i < numRects; ++i)
            clip.addWithoutMerging (clipBounds.getIntersection (Rectangle<int> (roundToInt (rects[i].origin.x) + offset.x,
                                                                                roundToInt (rects[i].origin.y) + offset.y,
                                                                                roundToInt (rects[i].size.width),
                                                                                roundToInt (rects[i].size.height))));
    }

    static void appFocusChanged()
    {
        keysCurrentlyDown.clear();

        if (isValidPeer (currentlyFocusedPeer))
        {
            if (Process::isForegroundProcess())
            {
                currentlyFocusedPeer->handleFocusGain();
                ModalComponentManager::getInstance()->bringModalComponentsToFront();
            }
            else
            {
                currentlyFocusedPeer->handleFocusLoss();
            }
        }
    }

    static bool checkEventBlockedByModalComps (NSEvent* e)
    {
        if (Component::getNumCurrentlyModalComponents() == 0)
            return false;

        NSWindow* const w = [e window];

        if (w == nil || [w worksWhenModal])
            return false;

        bool isKey = false, isInputAttempt = false;

        switch ([e type])
        {
            case NSEventTypeKeyDown:
            case NSEventTypeKeyUp:
                isKey = isInputAttempt = true;
                break;

            case NSEventTypeLeftMouseDown:
            case NSEventTypeRightMouseDown:
            case NSEventTypeOtherMouseDown:
                isInputAttempt = true;
                break;

            case NSEventTypeLeftMouseDragged:
            case NSEventTypeRightMouseDragged:
            case NSEventTypeLeftMouseUp:
            case NSEventTypeRightMouseUp:
            case NSEventTypeOtherMouseUp:
            case NSEventTypeOtherMouseDragged:
           #if JUCE_MAC_API_VERSION_CAN_BE_BUILT (26, 0)
            case NSEventTypeMouseCancelled:
           #endif
                if (Desktop::getInstance().getDraggingMouseSource (0) != nullptr)
                    return false;
                break;

            case NSEventTypeMouseMoved:
            case NSEventTypeMouseEntered:
            case NSEventTypeMouseExited:
            case NSEventTypeCursorUpdate:
            case NSEventTypeScrollWheel:
            case NSEventTypeTabletPoint:
            case NSEventTypeTabletProximity:
                break;

            case NSEventTypeFlagsChanged:
            case NSEventTypeAppKitDefined:
            case NSEventTypeSystemDefined:
            case NSEventTypeApplicationDefined:
            case NSEventTypePeriodic:
            case NSEventTypeGesture:
            case NSEventTypeMagnify:
            case NSEventTypeSwipe:
            case NSEventTypeRotate:
            case NSEventTypeBeginGesture:
            case NSEventTypeEndGesture:
            case NSEventTypeQuickLook:
            case NSEventTypeSmartMagnify:
            case NSEventTypePressure:
            case NSEventTypeDirectTouch:
            case NSEventTypeChangeMode:
            default:
                return false;
        }

        for (int i = ComponentPeer::getNumPeers(); --i >= 0;)
        {
            if (auto* peer = dynamic_cast<NSViewComponentPeer*> (ComponentPeer::getPeer (i)))
            {
                if ([peer->view window] == w)
                {
                    if (isKey)
                    {
                        if (peer->view == [w firstResponder])
                            return false;
                    }
                    else
                    {
                        if (peer->isSharedWindow
                               ? NSPointInRect ([peer->view convertPoint: [e locationInWindow] fromView: nil], [peer->view bounds])
                               : NSPointInRect ([e locationInWindow], NSMakeRect (0, 0, [w frame].size.width, [w frame].size.height)))
                            return false;
                    }
                }
            }
        }

        if (isInputAttempt)
        {
            if (! [NSApp isActive])
                [NSApp activateIgnoringOtherApps: YES];

            if (auto* modal = Component::getCurrentlyModalComponent())
                modal->inputAttemptWhenModal();
        }

        return true;
    }

    void setFullScreenSizeConstraints (const ComponentBoundsConstrainer& c)
    {
        const auto minSize = NSMakeSize (static_cast<float> (c.getMinimumWidth()), 0.0f);
        [window setMinFullScreenContentSize: minSize];
        [window setMaxFullScreenContentSize: NSMakeSize (100000, 100000)];
    }

    void displayLayer ([[maybe_unused]] CALayer* layer) override
    {
       #if JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS
        if (metalRenderer == nullptr)
            return;

        const auto scale = [this]
        {
            if (auto* viewWindow = [view window])
                return (float) viewWindow.backingScaleFactor;

            return 1.0f;
        }();

        deferredRepaints = metalRenderer->drawRectangleList (static_cast<CAMetalLayer*> (layer),
                                                             scale,
                                                             [this] (auto&&... args) { drawRectWithContext (args...); },
                                                             std::move (deferredRepaints),
                                                             [view inLiveResize]);
       #endif
    }

    /*  Used to store and restore the values of the NSWindowStyleMaskClosable and
        NSWindowStyleMaskMiniaturizable flags.
    */
    struct StoredStyleFlags
    {
        StoredStyleFlags (NSWindowStyleMask m)
            : stored { m }
        {}

        static auto getStoredFlags()
        {
            return std::array<NSWindowStyleMask, 2> { NSWindowStyleMaskClosable,
                                                      NSWindowStyleMaskMiniaturizable };
        }

        auto withFlagsRestored (NSWindowStyleMask m) const
        {
            for (const auto& f : getStoredFlags())
                m = withFlagFromStored (m, f);

            return m;
        }

    private:
        NSWindowStyleMask withFlagFromStored (NSWindowStyleMask m, NSWindowStyleMask flag) const
        {
            return (m & ~flag) | (stored & flag);
        }

        NSWindowStyleMask stored;
    };

    void modalComponentManagerChanged()
    {
        const auto newStyleMask = std::invoke ([&]() -> std::optional<NSWindowStyleMask>
        {
            // We are only changing the style flags if we absolutely have to. Plugin windows generally
            // don't like to be modified. Windows created under plugin hosts running in an external
            // subprocess are particularly touchy, and may make the window invisible even if we call
            // [window setStyleMask [window setStyleMask]].
            if (isSharedWindow || ! hasNativeTitleBar())
                return {};

            const auto currentStyleMask = [window styleMask];

            if (ModalComponentManager::getInstance()->getNumModalComponents() > 0)
            {
                if (! storedFlags)
                    storedFlags.emplace (currentStyleMask);

                auto updatedMask = (storedFlags->withFlagsRestored (currentStyleMask)) & ~NSWindowStyleMaskMiniaturizable;

                if (component.isCurrentlyBlockedByAnotherModalComponent())
                    updatedMask &= ~NSWindowStyleMaskClosable;

                return updatedMask;
            }

            if (storedFlags)
            {
                const auto flagsToApply = storedFlags->withFlagsRestored (currentStyleMask);
                storedFlags.reset();
                return flagsToApply;
            }

            return {};
        });

        if (newStyleMask.has_value() && *newStyleMask != [window styleMask])
            [window setStyleMask: *newStyleMask];

        const auto newLevel = std::invoke ([&]() -> std::optional<NSWindowLevel>
        {
            if (isSharedWindow)
                return {};

            auto* manager = ModalComponentManager::getInstance();

            if (manager->getNumModalComponents() > 0)
            {
                auto* recent = manager->getModalComponent (0);

                if (recent == &component || component.isParentOf (recent))
                {
                    if (! storedLevel.has_value())
                        storedLevel = [window level];

                    return jmax (*storedLevel, NSModalPanelWindowLevel);
                }
            }

            return std::exchange (storedLevel, {});
        });

        if (newLevel.has_value() && *newLevel != [window level])
            [window setLevel: *newLevel];
    }

    //==============================================================================
    std::vector<ScopedNotificationCenterObserver> scopedObservers;
    std::vector<ScopedNotificationCenterObserver> windowObservers;

    std::optional<StoredStyleFlags> storedFlags;
    std::optional<NSWindowLevel> storedLevel;
    ErasedScopeGuard modalChangeListenerScope =
        detail::ComponentHelpers::ModalComponentManagerChangeNotifier::getInstance().addListener ([this]
                                                                                                  {
                                                                                                      modalComponentManagerChanged();
                                                                                                  });

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NSViewComponentPeer)
};

//==============================================================================
template <typename Base>
struct NSViewComponentPeerWrapper : public Base
{
    explicit NSViewComponentPeerWrapper (const char* baseName)
        : Base (baseName)
    {
        Base::template addIvar<NSViewComponentPeer*> ("owner");
    }

    static NSViewComponentPeer* getOwner (id self)
    {
        return NSViewComponentPeer::getOwner (self);
    }

    static id getAccessibleChild (id self)
    {
        if (auto* owner = getOwner (self))
            if (auto* handler = owner->getComponent().getAccessibilityHandler())
                return (id) handler->getNativeImplementation();

        return nil;
    }
};

//==============================================================================
struct JuceNSViewClass final : public NSViewComponentPeerWrapper<ObjCClass<NSView>>
{
    JuceNSViewClass()  : NSViewComponentPeerWrapper ("JUCEView_")
    {
        addMethod (@selector (isOpaque), [] (id self, SEL)
        {
            auto* owner = getOwner (self);
            return owner == nullptr || owner->getComponent().isOpaque();
        });

        addMethod (@selector (updateTrackingAreas), [] (id self, SEL)
        {
            sendSuperclassMessage<void> (self, @selector (updateTrackingAreas));

            resetTrackingArea (static_cast<NSView*> (self));
        });

        addMethod (@selector (becomeFirstResponder), [] (id self, SEL)
        {
            callOnOwner (self, &NSViewComponentPeer::viewFocusGain);
            return YES;
        });

        addMethod (@selector (resignFirstResponder), [] (id self, SEL)
        {
            callOnOwner (self, &NSViewComponentPeer::viewFocusLoss);
            return YES;
        });

        addMethod (NSViewComponentPeer::dismissModalsSelector,  [] (id self, SEL)                    { callOnOwner (self, &NSViewComponentPeer::dismissModals); });
        addMethod (NSViewComponentPeer::frameChangedSelector,   [] (id self, SEL, NSNotification*)   { callOnOwner (self, &NSViewComponentPeer::redirectMovedOrResized); });
        addMethod (NSViewComponentPeer::becomeKeySelector,      [] (id self, SEL)                    { callOnOwner (self, &NSViewComponentPeer::becomeKey); });
        addMethod (NSViewComponentPeer::resignKeySelector,      [] (id self, SEL)                    { callOnOwner (self, &NSViewComponentPeer::resignKey); });

        addMethod (@selector (paste:),                          [] (id self, SEL, NSObject* s)       { callOnOwner (self, &NSViewComponentPeer::redirectPaste,            s);  });
        addMethod (@selector (copy:),                           [] (id self, SEL, NSObject* s)       { callOnOwner (self, &NSViewComponentPeer::redirectCopy,             s);  });
        addMethod (@selector (cut:),                            [] (id self, SEL, NSObject* s)       { callOnOwner (self, &NSViewComponentPeer::redirectCut,              s);  });
        addMethod (@selector (selectAll:),                      [] (id self, SEL, NSObject* s)       { callOnOwner (self, &NSViewComponentPeer::redirectSelectAll,        s);  });

        addMethod (@selector (viewWillMoveToWindow:),           [] (id self, SEL, NSWindow* w)       { callOnOwner (self, &NSViewComponentPeer::redirectWillMoveToWindow, w); });

        addMethod (@selector (drawRect:),                       [] (id self, SEL, NSRect r)          { callOnOwner (self, &NSViewComponentPeer::drawRect, r); });
        addMethod (@selector (viewDidMoveToWindow),             [] (id self, SEL)                    { callOnOwner (self, &NSViewComponentPeer::viewMovedToWindow); });
        addMethod (@selector (flagsChanged:),                   [] (id self, SEL, NSEvent* ev)       { callOnOwner (self, &NSViewComponentPeer::redirectModKeyChange, ev); });
        addMethod (@selector (mouseMoved:),                     [] (id self, SEL, NSEvent* ev)       { callOnOwner (self, &NSViewComponentPeer::redirectMouseMove,    ev); });
        addMethod (@selector (mouseEntered:),                   [] (id self, SEL, NSEvent* ev)       { callOnOwner (self, &NSViewComponentPeer::redirectMouseEnter,   ev); });
        addMethod (@selector (mouseExited:),                    [] (id self, SEL, NSEvent* ev)       { callOnOwner (self, &NSViewComponentPeer::redirectMouseExit,    ev); });
        addMethod (@selector (scrollWheel:),                    [] (id self, SEL, NSEvent* ev)       { callOnOwner (self, &NSViewComponentPeer::redirectMouseWheel,   ev); });
        addMethod (@selector (magnifyWithEvent:),               [] (id self, SEL, NSEvent* ev)       { callOnOwner (self, &NSViewComponentPeer::redirectMagnify,      ev); });

        addMethod (@selector (mouseDragged:),                   mouseDragged);
        addMethod (@selector (rightMouseDragged:),              mouseDragged);
        addMethod (@selector (otherMouseDragged:),              mouseDragged);

        addMethod (NSViewComponentPeer::asyncMouseDownSelector, asyncMouseDown);
        addMethod (NSViewComponentPeer::asyncMouseUpSelector,   asyncMouseUp);

        addMethod (@selector (mouseDown:),                      mouseDown);
        addMethod (@selector (rightMouseDown:),                 mouseDown);
        addMethod (@selector (otherMouseDown:),                 mouseDown);

        addMethod (@selector (mouseUp:),                        mouseUp);
        addMethod (@selector (rightMouseUp:),                   mouseUp);
        addMethod (@selector (otherMouseUp:),                   mouseUp);

        addMethod (@selector (draggingEntered:),                draggingUpdated);
        addMethod (@selector (draggingUpdated:),                draggingUpdated);

        addMethod (@selector (draggingEnded:),                  draggingExited);
        addMethod (@selector (draggingExited:),                 draggingExited);

        addMethod (@selector (hitTest:), [] (id self, SEL, NSPoint pt) -> NSView*
        {
            auto* hit = sendSuperclassMessage<NSView*> (self, @selector (hitTest:), pt);

            if (hit == nil || hit == self)
                return hit;

            auto* owner = getOwner (self);

            if (owner == nullptr || ! owner->isBlockedByModalComponent())
                return hit;

            for (NSView* v = hit; v != nil && v != self; v = [v superview])
                if ([v isKindOfClass: [self class]])
                    return hit;

            return owner->view;
        });

        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wundeclared-selector")
        addMethod (@selector (clipsToBounds), [] (id, SEL) { return YES; });
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE

        addMethod (@selector (acceptsFirstMouse:), [] (id, SEL, NSEvent*) { return YES; });

       #if JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS
        addMethod (@selector (makeBackingLayer), [] (id self, SEL) -> CALayer*
        {
            if (auto* owner = getOwner (self))
            {
                if (owner->metalRenderer != nullptr)
                {
                    auto* layer = [CAMetalLayer layer];

                    layer.device = MTLCreateSystemDefaultDevice();
                    layer.framebufferOnly = NO;
                    layer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
                    layer.opaque = getOwner (self)->getComponent().isOpaque();
                    layer.needsDisplayOnBoundsChange = YES;
                    layer.drawsAsynchronously = YES;
                    layer.delegate = owner->layerDelegate.get();

                    if (@available (macOS 10.13, *))
                        layer.allowsNextDrawableTimeout = NO;

                    return layer;
                }
            }

            return sendSuperclassMessage<CALayer*> (self, @selector (makeBackingLayer));
        });
       #endif

        addMethod (@selector (windowWillMiniaturize:), [] (id self, SEL, NSNotification*)
        {
            if (auto* p = getOwner (self))
            {
                NSViewComponentPeer::detachFromParentWindow (p->window);

                if (p->isAlwaysOnTop)
                {
                    // there is a bug when restoring minimised always on top windows so we need
                    // to remove this behaviour before minimising and restore it afterwards
                    p->setAlwaysOnTop (false);
                    p->wasAlwaysOnTop = true;
                }
            }
        });

        addMethod (@selector (windowDidDeminiaturize:), [] (id self, SEL, NSNotification*)
        {
            if (auto* p = getOwner (self))
            {
                if (p->wasAlwaysOnTop)
                    p->setAlwaysOnTop (true);

                p->attachToParentWindow();
                p->redirectMovedOrResized();
            }
        });

        addMethod (@selector (wantsDefaultClipping), [] (id, SEL) { return YES; }); // (this is the default, but may want to customise it in future)

        addMethod (@selector (worksWhenModal), [] (id self, SEL)
        {
            if (auto* p = getOwner (self))
                return p->worksWhenModal();

            return false;
        });

        addMethod (@selector (viewWillDraw), [] (id self, SEL)
        {
            // Without setting contentsFormat macOS Big Sur will always set the invalid area
            // to be the entire frame.
            if (@available (macOS 10.12, *))
            {
                CALayer* layer = ((NSView*) self).layer;
                layer.contentsFormat = kCAContentsFormatRGBA8Uint;
            }

            sendSuperclassMessage<void> (self, @selector (viewWillDraw));
        });

        addMethod (@selector (keyDown:), [] (id self, SEL, NSEvent* ev)
        {
            const auto handled = [&]
            {
                if (auto* owner = getOwner (self))
                    return owner->sendEventToInputContextOrComponent (ev);

                return false;
            }();

            if (! handled)
                sendSuperclassMessage<void> (self, @selector (keyDown:), ev);
        });

        addMethod (@selector (keyUp:), [] (id self, SEL, NSEvent* ev)
        {
            auto* owner = getOwner (self);

            if (! owner->redirectKeyUp (ev))
                sendSuperclassMessage<void> (self, @selector (keyUp:), ev);
        });

        // See "The Path of Key Events" on this page:
        // https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/EventArchitecture/EventArchitecture.html
        // Normally, 'special' key presses (cursor keys, shortcuts, return, delete etc.) will be
        // sent down the view hierarchy to this function before any of the other keyboard handling
        // functions.
        // If any object returns YES from performKeyEquivalent, then the event is consumed.
        // If no object handles the key equivalent, then the event will be sent to the main menu.
        // If the menu is also unable to respond to the event, then the event will be sent
        // to keyDown:/keyUp: via sendEvent:, but this time the event will be sent to the first
        // responder and propagated back up the responder chain.
        // This architecture presents some issues in JUCE apps, which always expect the focused
        // Component to be sent all key presses *including* special keys.
        // There are also some slightly pathological cases that JUCE needs to support, for example
        // the situation where one of the cursor keys is bound to a main menu item. By default,
        // macOS would send the cursor event to performKeyEquivalent on each NSResponder, then send
        // the event to the main menu if no responder handled it. This would mean that the focused
        // Component would never see the event, which would break widgets like the TextEditor, which
        // expect to take precedence over menu items when they have focus.
        // Another layer of subtlety is that some IMEs require cursor key input. When long-pressing
        // the 'e' key to bring up the accent menu, the popup menu should receive cursor events
        // before the focused component.
        // To fulfil all of these requirements, we handle special keys ('key equivalents') like any
        // other key event, and send these events firstly to the NSTextInputContext (if there's an
        // active TextInputTarget), and then on to the focused Component in the case that the
        // input handler is unable to use the keypress. If the event still hasn't been used, then
        // it will be sent to the superclass's performKeyEquivalent: function, which will give the
        // OS a chance to handle events like cmd+Q, cmd+`, cmd+H etc.
        addMethod (@selector (performKeyEquivalent:), [] (id self, SEL s, NSEvent* ev) -> BOOL
        {
            if (auto* owner = getOwner (self))
            {
                const auto ref = owner->safeComponent;
                const auto prev = std::exchange (owner->inPerformKeyEquivalent, true);

                const ScopeGuard scope { [&ref, owner, prev]
                {
                    if (ref != nullptr)
                        owner->inPerformKeyEquivalent = prev;
                } };

                if (owner->sendEventToInputContextOrComponent (ev))
                {
                    if (ref == nullptr)
                        return YES;

                    const auto isFirstResponder = [&]
                    {
                        if (auto* v = owner->view)
                            if (auto* w = v.window)
                                return w.firstResponder == self;

                        return false;
                    }();

                    // If the view isn't the first responder, but the view has successfully
                    // performed the key equivalent, then the key event must have been passed down
                    // the view hierarchy to this point. In that case, the view won't be sent a
                    // matching keyUp event, so we simulate it here.
                    if (! isFirstResponder)
                        owner->redirectKeyUp (ev);

                    return YES;
                }
            }

            return sendSuperclassMessage<BOOL> (self, s, ev);
        });

        addMethod (@selector (insertText:replacementRange:), [] (id self, SEL, id aString, NSRange replacementRange)
        {
            // This commits multi-byte text when using an IME, or after every keypress for western keyboards
            if (auto* owner = getOwner (self))
            {
                if (auto* target = owner->findCurrentTextInputTarget())
                {
                    const auto newText = nsStringToJuce ([aString isKindOfClass: [NSAttributedString class]] ? [aString string] : aString);

                    if (newText.isNotEmpty())
                    {
                        target->setHighlightedRegion ([&]
                        {
                            // To test this, try long-pressing 'e' to bring up the accent popup,
                            // then select one of the accented options.
                            if (replacementRange.location != NSNotFound)
                                return nsRangeToJuce (replacementRange);

                            // To test this, try entering the characters 'a b <esc>' with the 2-Set
                            // Korean IME. The input client should receive three calls to setMarkedText:
                            // followed by a call to insertText:
                            // The final call to insertText should overwrite the currently-marked
                            // text, and reset the composition string.
                            if (owner->stringBeingComposed.isNotEmpty())
                                return Range<int>::withStartAndLength (owner->startOfMarkedTextInTextInputTarget,
                                                                       owner->stringBeingComposed.length());

                            return target->getHighlightedRegion();
                        }());

                        target->insertTextAtCaret (newText);
                        target->setTemporaryUnderlining ({});
                    }
                }
                else
                    jassertfalse; // The system should not attempt to insert text when there is no active TextInputTarget

                owner->stringBeingComposed.clear();
            }
        });

        addMethod (@selector (doCommandBySelector:), [] (id self, SEL, SEL sel)
        {
            const auto handled = [&]
            {
                // 'Special' keys, like backspace, return, tab, and escape, are converted to commands by the system.
                // Components still expect to receive these events as key presses, so we send the currently-processed
                // key event (if any).
                if (auto* owner = getOwner (self))
                {
                    owner->viewCannotHandleEvent = [&]
                    {
                        if (auto* e = owner->keyEventBeingHandled.get())
                        {
                            if ([e type] != NSEventTypeKeyDown && [e type] != NSEventTypeKeyUp)
                                return true;

                            return ! ([e type] == NSEventTypeKeyDown ? owner->redirectKeyDown (e)
                                                                     : owner->redirectKeyUp (e));
                        }

                        return true;
                    }();

                    return ! owner->viewCannotHandleEvent;
                }

                return false;
            }();

            if (! handled)
                sendSuperclassMessage<void> (self, @selector (doCommandBySelector:), sel);
        });

        addMethod (@selector (setMarkedText:selectedRange:replacementRange:), [] (id self,
                                                                                  SEL,
                                                                                  id aString,
                                                                                  const NSRange selectedRange,
                                                                                  const NSRange replacementRange)
        {
            if (auto* owner = getOwner (self))
            {
                if (auto* target = owner->findCurrentTextInputTarget())
                {
                    const auto toInsert = nsStringToJuce ([aString isKindOfClass: [NSAttributedString class]] ? [aString string] : aString);
                    const auto [initialHighlight, marked, finalHighlight] = [&]
                    {
                        if (owner->stringBeingComposed.isNotEmpty())
                        {
                            const auto toReplace = Range<int>::withStartAndLength (owner->startOfMarkedTextInTextInputTarget,
                                                                                   owner->stringBeingComposed.length());

                            return replacementRange.location != NSNotFound
                                 // There's a composition underway, so replacementRange is relative to the marked text,
                                 // and selectedRange is relative to the inserted string.
                                 ? std::tuple (toReplace,
                                               owner->stringBeingComposed.replaceSection (static_cast<int> (replacementRange.location),
                                                                                          static_cast<int> (replacementRange.length),
                                                                                          toInsert),
                                               nsRangeToJuce (selectedRange) + static_cast<int> (replacementRange.location))
                                 // The replacementRange is invalid, so replace all the marked text.
                                 : std::tuple (toReplace, toInsert, nsRangeToJuce (selectedRange));
                        }

                        if (replacementRange.location != NSNotFound)
                            // There's no string composition in progress, so replacementRange is relative to the start
                            // of the document.
                            return std::tuple (nsRangeToJuce (replacementRange), toInsert, nsRangeToJuce (selectedRange));

                        return std::tuple (target->getHighlightedRegion(), toInsert, nsRangeToJuce (selectedRange));
                    }();

                    owner->stringBeingComposed = marked;
                    owner->startOfMarkedTextInTextInputTarget = initialHighlight.getStart();

                    target->setHighlightedRegion (initialHighlight);
                    target->insertTextAtCaret (marked);
                    target->setTemporaryUnderlining ({ Range<int>::withStartAndLength (initialHighlight.getStart(), marked.length()) });
                    target->setHighlightedRegion (finalHighlight + owner->startOfMarkedTextInTextInputTarget);
                }
            }
        });

        addMethod (@selector (unmarkText), [] (id self, SEL)
        {
            if (auto* owner = getOwner (self))
            {
                if (owner->stringBeingComposed.isNotEmpty())
                {
                    if (auto* target = owner->findCurrentTextInputTarget())
                    {
                        target->insertTextAtCaret (owner->stringBeingComposed);
                        target->setTemporaryUnderlining ({});
                    }

                    owner->stringBeingComposed.clear();
                }
            }
        });

        addMethod (@selector (hasMarkedText), [] (id self, SEL)
        {
            auto* owner = getOwner (self);
            return owner != nullptr && owner->stringBeingComposed.isNotEmpty();
        });

        addMethod (@selector (attributedSubstringForProposedRange:actualRange:), [] (id self, SEL, NSRange theRange, NSRangePointer actualRange) -> NSAttributedString*
        {
            jassert (theRange.location != NSNotFound);

            if (auto* owner = getOwner (self))
            {
                if (auto* target = owner->findCurrentTextInputTarget())
                {
                    const auto clamped = Range<int> { 0, target->getTotalNumChars() }.constrainRange (nsRangeToJuce (theRange));

                    if (actualRange != nullptr)
                        *actualRange = juceRangeToNS (clamped);

                    return [[[NSAttributedString alloc] initWithString: juceStringToNS (target->getTextInRange (clamped))] autorelease];
                }
            }

            return nil;
        });

        addMethod (@selector (markedRange), [] (id self, SEL)
        {
            if (auto* owner = getOwner (self))
                if (owner->stringBeingComposed.isNotEmpty())
                    return NSMakeRange (static_cast<NSUInteger> (owner->startOfMarkedTextInTextInputTarget),
                                        static_cast<NSUInteger> (owner->stringBeingComposed.length()));

            return NSMakeRange (NSNotFound, 0);
        });

        addMethod (@selector (selectedRange), [] (id self, SEL)
        {
            if (auto* owner = getOwner (self))
            {
                if (auto* target = owner->findCurrentTextInputTarget())
                {
                    const auto highlight = target->getHighlightedRegion();

                    // The accent-selector popup does not show if the selectedRange location is NSNotFound!
                    return NSMakeRange ((NSUInteger) highlight.getStart(),
                                        (NSUInteger) highlight.getLength());
                }
            }

            return NSMakeRange (NSNotFound, 0);
        });

        addMethod (@selector (firstRectForCharacterRange:actualRange:), [] (id self, SEL, NSRange range, NSRangePointer actualRange)
        {
            if (auto* owner = getOwner (self))
            {
                if (auto* target = owner->findCurrentTextInputTarget())
                {
                    if (auto* comp = dynamic_cast<Component*> (target))
                    {
                        const auto codePointRange = range.location == NSNotFound ? Range<int>::emptyRange (target->getCaretPosition())
                                                                                 : nsRangeToJuce (range);
                        const auto clamped = Range<int> { 0, target->getTotalNumChars() }.constrainRange (codePointRange);

                        if (actualRange != nullptr)
                            *actualRange = juceRangeToNS (clamped);

                        const auto rect = codePointRange.isEmpty() ? target->getCaretRectangleForCharIndex (codePointRange.getStart())
                                                                   : target->getTextBounds (codePointRange).getRectangle (0);
                        const auto areaOnDesktop = comp->localAreaToGlobal (rect);

                        return flippedScreenRect (makeCGRect (detail::ScalingHelpers::scaledScreenPosToUnscaled (areaOnDesktop)));
                    }
                }
            }

            return NSZeroRect;
        });

        addMethod (@selector (characterIndexForPoint:), [] (id, SEL, NSPoint) { return NSNotFound; });

        addMethod (@selector (validAttributesForMarkedText), [] (id, SEL) { return [NSArray array]; });

        addMethod (@selector (acceptsFirstResponder), [] (id self, SEL)
        {
            auto* owner = getOwner (self);
            return owner != nullptr && owner->canBecomeKeyWindow();
        });

        addMethod (@selector (prepareForDragOperation:), [] (id, SEL, id<NSDraggingInfo>) { return YES; });

        addMethod (@selector (performDragOperation:), [] (id self, SEL, id<NSDraggingInfo> sender)
        {
            auto* owner = getOwner (self);
            return owner != nullptr && owner->sendDragCallback (&NSViewComponentPeer::handleDragDrop, sender);
        });

        addMethod (@selector (concludeDragOperation:), [] (id, SEL, id<NSDraggingInfo>) {});

        addMethod (@selector (isAccessibilityElement), [] (id, SEL) { return NO; });

        addMethod (@selector (accessibilityChildren), getAccessibilityChildren);

        addMethod (@selector (accessibilityHitTest:), [] (id self, SEL, NSPoint point)
        {
            return [getAccessibleChild (self) accessibilityHitTest: point];
        });

        addMethod (@selector (accessibilityFocusedUIElement), [] (id self, SEL)
        {
            return [getAccessibleChild (self) accessibilityFocusedUIElement];
        });

        // deprecated methods required for backwards compatibility
        addMethod (@selector (accessibilityIsIgnored), [] (id, SEL) { return YES; });

        addMethod (@selector (accessibilityAttributeValue:), [] (id self, SEL, NSString* attribute) -> id
        {
            if ([attribute isEqualToString: NSAccessibilityChildrenAttribute])
                return getAccessibilityChildren (self, {});

            return sendSuperclassMessage<id> (self, @selector (accessibilityAttributeValue:), attribute);
        });

        addMethod (@selector (isFlipped), [] (id, SEL) { return true; });

        addProtocol (@protocol (NSTextInputClient));

        registerClass();
    }

private:
    template <typename Func, typename... Args>
    static void callOnOwner (id self, Func&& func, Args&&... args)
    {
        if (auto* owner = getOwner (self))
            (owner->*func) (std::forward<Args> (args)...);
    }

    static void mouseDragged   (id self, SEL, NSEvent* ev)               { callOnOwner (self, &NSViewComponentPeer::redirectMouseDrag, ev); }
    static void asyncMouseDown (id self, SEL, NSEvent* ev)               { callOnOwner (self, &NSViewComponentPeer::redirectMouseDown, ev); }
    static void asyncMouseUp   (id self, SEL, NSEvent* ev)               { callOnOwner (self, &NSViewComponentPeer::redirectMouseUp,   ev); }
    static void draggingExited (id self, SEL, id<NSDraggingInfo> sender) { callOnOwner (self, &NSViewComponentPeer::sendDragCallback, &NSViewComponentPeer::handleDragExit, sender); }

    static void mouseDown (id self, SEL s, NSEvent* ev)
    {
        if (JUCEApplicationBase::isStandaloneApp())
        {
            asyncMouseDown (self, s, ev);
        }
        else
        {
            // In some host situations, the host will stop modal loops from working
            // correctly if they're called from a mouse event, so we'll trigger
            // the event asynchronously.
            [self performSelectorOnMainThread: NSViewComponentPeer::asyncMouseDownSelector
                                   withObject: ev
                                waitUntilDone: NO];
        }
    }

    static void mouseUp (id self, SEL s, NSEvent* ev)
    {
        if (JUCEApplicationBase::isStandaloneApp())
        {
            asyncMouseUp (self, s, ev);
        }
        else
        {
            // In some host situations, the host will stop modal loops from working
            // correctly if they're called from a mouse event, so we'll trigger
            // the event asynchronously.
            [self performSelectorOnMainThread: NSViewComponentPeer::asyncMouseUpSelector
                                   withObject: ev
                                waitUntilDone: NO];
        }
    }

    static NSDragOperation draggingUpdated (id self, SEL, id<NSDraggingInfo> sender)
    {
        if (auto* owner = getOwner (self))
            if (owner->sendDragCallback (&NSViewComponentPeer::handleDragMove, sender))
                return NSDragOperationGeneric;

        return NSDragOperationNone;
    }

    static NSArray* getAccessibilityChildren (id self, SEL)
    {
        return NSAccessibilityUnignoredChildrenForOnlyChild (getAccessibleChild (self));
    }
};

//==============================================================================
struct JuceNSWindowClass final : public NSViewComponentPeerWrapper<ObjCClass<NSWindow>>
{
    JuceNSWindowClass()  : NSViewComponentPeerWrapper ("JUCEWindow_")
    {
        addMethod (@selector (canBecomeKeyWindow), [] (id self, SEL)
        {
            auto* owner = getOwner (self);

            return owner != nullptr
                   && owner->canBecomeKeyWindow()
                   && ! owner->isBlockedByModalComponent();
        });

        addMethod (@selector (canBecomeMainWindow), [] (id self, SEL)
        {
            auto* owner = getOwner (self);

            return owner != nullptr
                   && owner->canBecomeMainWindow()
                   && ! owner->isBlockedByModalComponent();
        });

        addMethod (@selector (becomeKeyWindow), [] (id self, SEL)
        {
            sendSuperclassMessage<void> (self, @selector (becomeKeyWindow));

            if (auto* owner = getOwner (self))
            {
                jassert (! owner->inBecomeKeyWindow);

                const ScopedValueSetter scope { owner->inBecomeKeyWindow, true };

                if (owner->canBecomeKeyWindow())
                {
                    owner->becomeKeyWindow();
                    return;
                }

                // this fixes a bug causing hidden windows to sometimes become visible when the app regains focus
                if (! owner->getComponent().isVisible())
                    [(NSWindow*) self orderOut: nil];
            }
        });

        addMethod (@selector (resignKeyWindow), [] (id self, SEL)
        {
            sendSuperclassMessage<void> (self, @selector (resignKeyWindow));

            if (auto* owner = getOwner (self))
                owner->resignKeyWindow();
        });

        addMethod (@selector (windowShouldClose:), [] (id self, SEL, id /*window*/)
        {
            auto* owner = getOwner (self);
            return owner == nullptr || owner->windowShouldClose();
        });

        addMethod (@selector (constrainFrameRect:toScreen:), [] (id self, SEL, NSRect frameRect, NSScreen* screen)
        {
            if (auto* owner = getOwner (self))
            {
                // A move that keeps the offset of a child window from its parent is AppKit moving
                // the child along with its parent.
                if (auto* parent = [(NSWindow*) self parentWindow]; parent != nil && owner->offsetFromParentWindow.has_value())
                {
                    const auto proposedOffset = Point<double> (frameRect.origin.x - [parent frame].origin.x,
                                                               frameRect.origin.y - [parent frame].origin.y);

                    if (proposedOffset == *owner->offsetFromParentWindow && NSEqualSizes (frameRect.size, [(NSWindow*) self frame].size))
                        return frameRect;
                }

                frameRect = sendSuperclassMessage<NSRect, NSRect, NSScreen*> (self, @selector (constrainFrameRect:toScreen:),
                                                                              frameRect, screen);

                frameRect = owner->constrainRect (frameRect);
            }

            return frameRect;
        });

        addMethod (@selector (windowWillResize:toSize:), [] (id self, SEL, NSWindow*, NSSize proposedFrameSize)
        {
            auto* owner = getOwner (self);

            if (owner == nullptr)
                return proposedFrameSize;

            NSRect frameRect = flippedScreenRect ([(NSWindow*) self frame]);
            frameRect.size = proposedFrameSize;

            frameRect = owner->constrainRect (flippedScreenRect (frameRect));

            owner->dismissModals();

            return frameRect.size;
        });

        addMethod (@selector (windowDidExitFullScreen:), [] (id self, SEL, NSNotification*)
        {
            if (auto* owner = getOwner (self))
                owner->resetWindowPresentation();
        });

        addMethod (@selector (windowWillEnterFullScreen:), [] (id self, SEL, NSNotification*)
        {
            if (auto* owner = getOwner (self))
                if (owner->hasNativeTitleBar() && (owner->getStyleFlags() & ComponentPeer::windowIsResizable) == 0)
                    [owner->window setStyleMask: NSWindowStyleMaskBorderless];
        });

        addMethod (@selector (windowWillExitFullScreen:), [] (id self, SEL, NSNotification*)
        {
            // The exit-fullscreen animation looks bad on Monterey if the window isn't resizable...
            if (auto* owner = getOwner (self))
                if (auto* window = owner->window)
                    [window setStyleMask: [window styleMask] | NSWindowStyleMaskResizable];
        });

        addMethod (@selector (windowWillStartLiveResize:), [] (id self, SEL, NSNotification*)
        {
            if (auto* owner = getOwner (self))
                owner->liveResizingStart();
        });

        addMethod (@selector (windowDidEndLiveResize:), [] (id self, SEL, NSNotification*)
        {
            if (auto* owner = getOwner (self))
                owner->liveResizingEnd();
        });

        addMethod (@selector (window:shouldPopUpDocumentPathMenu:), [] (id self, SEL, id /*window*/, NSMenu*)
        {
            if (auto* owner = getOwner (self))
                return owner->windowRepresentsFile;

            return false;
        });

        addMethod (@selector (windowWillUseStandardFrame:defaultFrame:), [] (id self, SEL, NSWindow* window, NSRect r)
        {
            if (auto* owner = getOwner (self))
            {
                if (auto* constrainer = owner->getConstrainer())
                {
                    if (auto* screen = [window screen])
                    {
                        const auto safeScreenBounds = convertToRectFloat (flippedScreenRect (owner->hasNativeTitleBar() ? r : [screen visibleFrame]));
                        const auto originalBounds = owner->getFrameSize().addedTo (owner->getComponent().getScreenBounds()).toFloat();
                        const auto expanded = originalBounds.withWidth  ((float) constrainer->getMaximumWidth())
                                                            .withHeight ((float) constrainer->getMaximumHeight());
                        const auto constrained = expanded.constrainedWithin (safeScreenBounds);

                        return flippedScreenRect (makeCGRect ([&]
                                                              {
                                                                  if (constrained == owner->getBounds().toFloat())
                                                                      return owner->lastSizeBeforeZoom.toFloat();

                                                                  owner->lastSizeBeforeZoom = owner->getBounds().toFloat();
                                                                  return constrained;
                                                              }()));
                    }
                }
            }

            return r;
        });

        addMethod (@selector (windowShouldZoom:toFrame:), [] (id self, SEL, NSWindow*, NSRect)
        {
            if (auto* owner = getOwner (self))
                if (owner->hasNativeTitleBar() && (owner->getStyleFlags() & ComponentPeer::windowIsResizable) == 0)
                    return NO;

            return YES;
        });

        addMethod (@selector (accessibilityTitle), [] (id self, SEL) { return [self title]; });

        addMethod (@selector (accessibilityLabel), [] (id self, SEL) { return [getAccessibleChild (self) accessibilityLabel]; });

        addMethod (@selector (accessibilityRole), [] (id, SEL) { return NSAccessibilityWindowRole; });

        addMethod (@selector (accessibilitySubrole), [] (id self, SEL) -> NSAccessibilitySubrole
        {
            return [getAccessibleChild (self) accessibilitySubrole];
        });

        addMethod (@selector (window:shouldDragDocumentWithEvent:from:withPasteboard:), [] (id self, SEL, id /*window*/, NSEvent*, NSPoint, NSPasteboard*)
        {
            if (auto* owner = getOwner (self))
                return owner->windowRepresentsFile;

            return false;
        });

        addMethod (@selector (toggleFullScreen:), [] (id self, SEL name, id sender)
        {
            if (auto* owner = getOwner (self))
            {
                const auto isFullScreen = owner->isFullScreen();

                if (! isFullScreen)
                    owner->lastSizeBeforeZoom = owner->getBounds().toFloat();

                sendSuperclassMessage<void> (self, name, sender);

                if (isFullScreen)
                {
                    [NSApp setPresentationOptions: NSApplicationPresentationDefault];
                    owner->setBounds (owner->lastSizeBeforeZoom.toNearestInt(), false);
                }
            }
        });

        addMethod (@selector (accessibilityTopLevelUIElement),      getAccessibilityWindow);
        addMethod (@selector (accessibilityWindow),                 getAccessibilityWindow);

        addProtocol (@protocol (NSWindowDelegate));

        registerClass();
    }

private:
    //==============================================================================
    static id getAccessibilityWindow (id self, SEL) { return self; }
};

NSView* NSViewComponentPeer::createViewInstance()
{
    static JuceNSViewClass cls;
    return cls.createInstance();
}

NSWindow* NSViewComponentPeer::createWindowInstance()
{
    static JuceNSWindowClass cls;
    return cls.createInstance();
}


//==============================================================================
bool KeyPress::isKeyCurrentlyDown (int keyCode)
{
    const auto isDown = [] (int k)
    {
        return NSViewComponentPeer::keysCurrentlyDown.find (k) != NSViewComponentPeer::keysCurrentlyDown.cend();
    };

    if (isDown (keyCode))
        return true;

    if (keyCode >= 'A' && keyCode <= 'Z'
         && isDown ((int) CharacterFunctions::toLowerCase ((juce_wchar) keyCode)))
        return true;

    if (keyCode >= 'a' && keyCode <= 'z'
         && isDown ((int) CharacterFunctions::toUpperCase ((juce_wchar) keyCode)))
        return true;

    return false;
}

//==============================================================================
bool detail::MouseInputSourceList::addSource()
{
    auto numSources = sources.size();

    if (numSources == 0)
    {
        addSource (0, MouseInputSource::InputSourceType::mouse);
        
        if (UPDDManager::getInstance()->isUPDDAvailable()) {
            addSource (1, MouseInputSource::InputSourceType::touch);
        }
        
        return true;
    }

    return false;
}

bool detail::MouseInputSourceList::canUseTouch() const
{
    return UPDDManager::getInstance()->isUPDDAvailable();
}

//==============================================================================
void Desktop::setKioskComponent (Component* kioskComp, bool shouldBeEnabled, bool allowMenusAndBars)
{
    auto* peer = dynamic_cast<NSViewComponentPeer*> (kioskComp->getPeer());
    jassert (peer != nullptr); // (this should have been checked by the caller)

    if (peer->hasNativeTitleBar())
    {
        if (shouldBeEnabled && ! allowMenusAndBars)
            [NSApp setPresentationOptions: NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar];
        else if (! shouldBeEnabled)
            [NSApp setPresentationOptions: NSApplicationPresentationDefault];

        peer->setFullScreen (shouldBeEnabled);
    }
    else
    {
        if (shouldBeEnabled)
        {
            [NSApp setPresentationOptions: (allowMenusAndBars ? (NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar)
                                                              : (NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar))];

            kioskComp->setBounds (getDisplays().getDisplayForRect (kioskComp->getScreenBounds())->logicalBounds.getSmallestIntegerContainer());
            peer->becomeKeyWindow();
        }
        else
        {
            peer->resetWindowPresentation();
        }
    }
}

void Desktop::allowedOrientationsChanged() {}

//==============================================================================
ComponentPeer* Component::createNewPeer (const DesktopWindowOptions& options)
{
    return new NSViewComponentPeer (*this, options.getStyleFlags(), options.getAssociatedComponent(),
                                    (NSView*) options.getParentNSView());
}

//==============================================================================
const int KeyPress::spaceKey        = ' ';
const int KeyPress::returnKey       = 0x0d;
const int KeyPress::escapeKey       = 0x1b;
const int KeyPress::backspaceKey    = 0x7f;
const int KeyPress::leftKey         = NSLeftArrowFunctionKey;
const int KeyPress::rightKey        = NSRightArrowFunctionKey;
const int KeyPress::upKey           = NSUpArrowFunctionKey;
const int KeyPress::downKey         = NSDownArrowFunctionKey;
const int KeyPress::pageUpKey       = NSPageUpFunctionKey;
const int KeyPress::pageDownKey     = NSPageDownFunctionKey;
const int KeyPress::endKey          = NSEndFunctionKey;
const int KeyPress::homeKey         = NSHomeFunctionKey;
const int KeyPress::deleteKey       = NSDeleteFunctionKey;
const int KeyPress::insertKey       = -1;
const int KeyPress::tabKey          = 9;
const int KeyPress::F1Key           = NSF1FunctionKey;
const int KeyPress::F2Key           = NSF2FunctionKey;
const int KeyPress::F3Key           = NSF3FunctionKey;
const int KeyPress::F4Key           = NSF4FunctionKey;
const int KeyPress::F5Key           = NSF5FunctionKey;
const int KeyPress::F6Key           = NSF6FunctionKey;
const int KeyPress::F7Key           = NSF7FunctionKey;
const int KeyPress::F8Key           = NSF8FunctionKey;
const int KeyPress::F9Key           = NSF9FunctionKey;
const int KeyPress::F10Key          = NSF10FunctionKey;
const int KeyPress::F11Key          = NSF11FunctionKey;
const int KeyPress::F12Key          = NSF12FunctionKey;
const int KeyPress::F13Key          = NSF13FunctionKey;
const int KeyPress::F14Key          = NSF14FunctionKey;
const int KeyPress::F15Key          = NSF15FunctionKey;
const int KeyPress::F16Key          = NSF16FunctionKey;
const int KeyPress::F17Key          = NSF17FunctionKey;
const int KeyPress::F18Key          = NSF18FunctionKey;
const int KeyPress::F19Key          = NSF19FunctionKey;
const int KeyPress::F20Key          = NSF20FunctionKey;
const int KeyPress::F21Key          = NSF21FunctionKey;
const int KeyPress::F22Key          = NSF22FunctionKey;
const int KeyPress::F23Key          = NSF23FunctionKey;
const int KeyPress::F24Key          = NSF24FunctionKey;
const int KeyPress::F25Key          = NSF25FunctionKey;
const int KeyPress::F26Key          = NSF26FunctionKey;
const int KeyPress::F27Key          = NSF27FunctionKey;
const int KeyPress::F28Key          = NSF28FunctionKey;
const int KeyPress::F29Key          = NSF29FunctionKey;
const int KeyPress::F30Key          = NSF30FunctionKey;
const int KeyPress::F31Key          = NSF31FunctionKey;
const int KeyPress::F32Key          = NSF32FunctionKey;
const int KeyPress::F33Key          = NSF33FunctionKey;
const int KeyPress::F34Key          = NSF34FunctionKey;
const int KeyPress::F35Key          = NSF35FunctionKey;

const int KeyPress::numberPad0              = extendedKeyModifier + 0x20;
const int KeyPress::numberPad1              = extendedKeyModifier + 0x21;
const int KeyPress::numberPad2              = extendedKeyModifier + 0x22;
const int KeyPress::numberPad3              = extendedKeyModifier + 0x23;
const int KeyPress::numberPad4              = extendedKeyModifier + 0x24;
const int KeyPress::numberPad5              = extendedKeyModifier + 0x25;
const int KeyPress::numberPad6              = extendedKeyModifier + 0x26;
const int KeyPress::numberPad7              = extendedKeyModifier + 0x27;
const int KeyPress::numberPad8              = extendedKeyModifier + 0x28;
const int KeyPress::numberPad9              = extendedKeyModifier + 0x29;
const int KeyPress::numberPadAdd            = extendedKeyModifier + 0x2a;
const int KeyPress::numberPadSubtract       = extendedKeyModifier + 0x2b;
const int KeyPress::numberPadMultiply       = extendedKeyModifier + 0x2c;
const int KeyPress::numberPadDivide         = extendedKeyModifier + 0x2d;
const int KeyPress::numberPadSeparator      = extendedKeyModifier + 0x2e;
const int KeyPress::numberPadDecimalPoint   = extendedKeyModifier + 0x2f;
const int KeyPress::numberPadEquals         = extendedKeyModifier + 0x30;
const int KeyPress::numberPadDelete         = extendedKeyModifier + 0x31;
const int KeyPress::playKey                 = extendedKeyModifier + 0x00;
const int KeyPress::stopKey                 = extendedKeyModifier + 0x01;
const int KeyPress::fastForwardKey          = extendedKeyModifier + 0x02;
const int KeyPress::rewindKey               = extendedKeyModifier + 0x03;

} // namespace juce
