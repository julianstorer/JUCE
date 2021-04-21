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

Component* Component::currentlyFocusedComponent = nullptr;


//==============================================================================
class Component::MouseListenerList
{
public:
    MouseListenerList() noexcept {}

    void addListener (MouseListener* newListener, bool wantsEventsForAllNestedChildComponents)
    {
        if (! listeners.contains (newListener))
        {
            if (wantsEventsForAllNestedChildComponents)
            {
                listeners.insert (0, newListener);
                ++numDeepMouseListeners;
            }
            else
            {
                listeners.add (newListener);
            }
        }
    }

    void removeListener (MouseListener* listenerToRemove)
    {
        auto index = listeners.indexOf (listenerToRemove);

        if (index >= 0)
        {
            if (index < numDeepMouseListeners)
                --numDeepMouseListeners;

            listeners.remove (index);
        }
    }

    // g++ 4.8 cannot deduce the parameter pack inside the function pointer when it has more than one element
   #if defined(__GNUC__) && __GNUC__ < 5 && ! defined(__clang__)
    template <typename... Params>
    static void sendMouseEvent (Component& comp, Component::BailOutChecker& checker,
                                void (MouseListener::*eventMethod) (const MouseEvent&),
                                Params... params)
    {
        sendMouseEvent <decltype (eventMethod), Params...> (comp, checker, eventMethod, params...);
    }

    template <typename... Params>
    static void sendMouseEvent (Component& comp, Component::BailOutChecker& checker,
                                void (MouseListener::*eventMethod) (const MouseEvent&, const MouseWheelDetails&),
                                Params... params)
    {
        sendMouseEvent <decltype (eventMethod), Params...> (comp, checker, eventMethod, params...);
    }

    template <typename... Params>
    static void sendMouseEvent (Component& comp, Component::BailOutChecker& checker,
                                void (MouseListener::*eventMethod) (const MouseEvent&, float),
                                Params... params)
    {
        sendMouseEvent <decltype (eventMethod), Params...> (comp, checker, eventMethod, params...);
    }

    template <typename EventMethod, typename... Params>
    static void sendMouseEvent (Component& comp, Component::BailOutChecker& checker,
                                EventMethod eventMethod,
                                Params... params)
   #else
    template <typename... Params>
    static void sendMouseEvent (Component& comp, Component::BailOutChecker& checker,
                                void (MouseListener::*eventMethod) (Params...),
                                Params... params)
   #endif
    {
        if (checker.shouldBailOut())
            return;

        if (auto* list = comp.mouseListeners.get())
        {
            for (int i = list->listeners.size(); --i >= 0;)
            {
                (list->listeners.getUnchecked(i)->*eventMethod) (params...);

                if (checker.shouldBailOut())
                    return;

                i = jmin (i, list->listeners.size());
            }
        }

        for (Component* p = comp.parentComponent; p != nullptr; p = p->parentComponent)
        {
            if (auto* list = p->mouseListeners.get())
            {
                if (list->numDeepMouseListeners > 0)
                {
                    BailOutChecker2 checker2 (checker, p);

                    for (int i = list->numDeepMouseListeners; --i >= 0;)
                    {
                        (list->listeners.getUnchecked(i)->*eventMethod) (params...);

                        if (checker2.shouldBailOut())
                            return;

                        i = jmin (i, list->numDeepMouseListeners);
                    }
                }
            }
        }
    }

private:
    Array<MouseListener*> listeners;
    int numDeepMouseListeners = 0;

    struct BailOutChecker2
    {
        BailOutChecker2 (Component::BailOutChecker& boc, Component* comp)
            : checker (boc), safePointer (comp)
        {
        }

        bool shouldBailOut() const noexcept
        {
            return checker.shouldBailOut() || safePointer == nullptr;
        }

    private:
        Component::BailOutChecker& checker;
        const WeakReference<Component> safePointer;

        JUCE_DECLARE_NON_COPYABLE (BailOutChecker2)
    };

    JUCE_DECLARE_NON_COPYABLE (MouseListenerList)
};

//==============================================================================
struct FocusRestorer
{
    FocusRestorer()  : lastFocus (Component::getCurrentlyFocusedComponent()) {}

    ~FocusRestorer()
    {
        if (lastFocus != nullptr
             && lastFocus->isShowing()
             && ! lastFocus->isCurrentlyBlockedByAnotherModalComponent())
            lastFocus->grabKeyboardFocus();
    }

    WeakReference<Component> lastFocus;

    JUCE_DECLARE_NON_COPYABLE (FocusRestorer)
};

//==============================================================================
struct ScalingHelpers
{
    template <typename PointOrRect>
    static PointOrRect unscaledScreenPosToScaled (float scale, PointOrRect pos) noexcept
    {
        return scale != 1.0f ? pos / scale : pos;
    }

    template <typename PointOrRect>
    static PointOrRect scaledScreenPosToUnscaled (float scale, PointOrRect pos) noexcept
    {
        return scale != 1.0f ? pos * scale : pos;
    }

    // For these, we need to avoid getSmallestIntegerContainer being used, which causes
    // judder when moving windows
    static Rectangle<int> unscaledScreenPosToScaled (float scale, Rectangle<int> pos) noexcept
    {
        return scale != 1.0f ? Rectangle<int> (roundToInt ((float) pos.getX() / scale),
                                               roundToInt ((float) pos.getY() / scale),
                                               roundToInt ((float) pos.getWidth() / scale),
                                               roundToInt ((float) pos.getHeight() / scale)) : pos;
    }

    static Rectangle<int> scaledScreenPosToUnscaled (float scale, Rectangle<int> pos) noexcept
    {
        return scale != 1.0f ? Rectangle<int> (roundToInt ((float) pos.getX() * scale),
                                               roundToInt ((float) pos.getY() * scale),
                                               roundToInt ((float) pos.getWidth() * scale),
                                               roundToInt ((float) pos.getHeight() * scale)) : pos;
    }

    static Rectangle<float> unscaledScreenPosToScaled (float scale, Rectangle<float> pos) noexcept
    {
        return scale != 1.0f ? Rectangle<float> (pos.getX() / scale,
                                                 pos.getY() / scale,
                                                 pos.getWidth() / scale,
                                                 pos.getHeight() / scale) : pos;
    }

    static Rectangle<float> scaledScreenPosToUnscaled (float scale, Rectangle<float> pos) noexcept
    {
        return scale != 1.0f ? Rectangle<float> (pos.getX() * scale,
                                                 pos.getY() * scale,
                                                 pos.getWidth() * scale,
                                                 pos.getHeight() * scale) : pos;
    }

    template <typename PointOrRect>
    static PointOrRect unscaledScreenPosToScaled (PointOrRect pos) noexcept
    {
        return unscaledScreenPosToScaled (Desktop::getInstance().getGlobalScaleFactor(), pos);
    }

    template <typename PointOrRect>
    static PointOrRect scaledScreenPosToUnscaled (PointOrRect pos) noexcept
    {
        return scaledScreenPosToUnscaled (Desktop::getInstance().getGlobalScaleFactor(), pos);
    }

    template <typename PointOrRect>
    static PointOrRect unscaledScreenPosToScaled (const Component& comp, PointOrRect pos) noexcept
    {
        return unscaledScreenPosToScaled (comp.getDesktopScaleFactor(), pos);
    }

    template <typename PointOrRect>
    static PointOrRect scaledScreenPosToUnscaled (const Component& comp, PointOrRect pos) noexcept
    {
        return scaledScreenPosToUnscaled (comp.getDesktopScaleFactor(), pos);
    }

    static Point<int>       addPosition      (Point<int> p,       const Component& c) noexcept  { return p + c.getPosition(); }
    static Rectangle<int>   addPosition      (Rectangle<int> p,   const Component& c) noexcept  { return p + c.getPosition(); }
    static Point<float>     addPosition      (Point<float> p,     const Component& c) noexcept  { return p + c.getPosition().toFloat(); }
    static Rectangle<float> addPosition      (Rectangle<float> p, const Component& c) noexcept  { return p + c.getPosition().toFloat(); }
    static Point<int>       subtractPosition (Point<int> p,       const Component& c) noexcept  { return p - c.getPosition(); }
    static Rectangle<int>   subtractPosition (Rectangle<int> p,   const Component& c) noexcept  { return p - c.getPosition(); }
    static Point<float>     subtractPosition (Point<float> p,     const Component& c) noexcept  { return p - c.getPosition().toFloat(); }
    static Rectangle<float> subtractPosition (Rectangle<float> p, const Component& c) noexcept  { return p - c.getPosition().toFloat(); }
};

static const char colourPropertyPrefix[] = "jcclr_";

//==============================================================================
struct Component::ComponentHelpers
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    static void* runModalLoopCallback (void* userData)
    {
        return (void*) (pointer_sized_int) static_cast<Component*> (userData)->runModalLoop();
    }
   #endif

    static Identifier getColourPropertyID (int colourID)
    {
        char buffer[32];
        auto* end = buffer + numElementsInArray (buffer) - 1;
        auto* t = end;
        *t = 0;

        for (auto v = (uint32) colourID;;)
        {
            *--t = "0123456789abcdef" [v & 15];
            v >>= 4;

            if (v == 0)
                break;
        }

        for (int i = (int) sizeof (colourPropertyPrefix) - 1; --i >= 0;)
            *--t = colourPropertyPrefix[i];

        return t;
    }

    //==============================================================================
    static bool hitTest (Component& comp, Point<int> localPoint)
    {
        return isPositiveAndBelow (localPoint.x, comp.getWidth())
            && isPositiveAndBelow (localPoint.y, comp.getHeight())
            && comp.hitTest (localPoint.x, localPoint.y);
    }

    // converts an unscaled position within a peer to the local position within that peer's component
    template <typename PointOrRect>
    static PointOrRect rawPeerPositionToLocal (const Component& comp, PointOrRect pos) noexcept
    {
        if (comp.isTransformed())
            pos = pos.transformedBy (comp.getTransform().inverted());

        return ScalingHelpers::unscaledScreenPosToScaled (comp, pos);
    }

    // converts a position within a peer's component to the unscaled position within the peer
    template <typename PointOrRect>
    static PointOrRect localPositionToRawPeerPos (const Component& comp, PointOrRect pos) noexcept
    {
        if (comp.isTransformed())
            pos = pos.transformedBy (comp.getTransform());

        return ScalingHelpers::scaledScreenPosToUnscaled (comp, pos);
    }

    template <typename PointOrRect>
    static PointOrRect convertFromParentSpace (const Component& comp, PointOrRect pointInParentSpace)
    {
        if (comp.affineTransform != nullptr)
            pointInParentSpace = pointInParentSpace.transformedBy (comp.affineTransform->inverted());

        if (comp.isOnDesktop())
        {
            if (auto* peer = comp.getPeer())
                pointInParentSpace = ScalingHelpers::unscaledScreenPosToScaled
                                        (comp, peer->globalToLocal (ScalingHelpers::scaledScreenPosToUnscaled (pointInParentSpace)));
            else
                jassertfalse;
        }
        else
        {
            pointInParentSpace = ScalingHelpers::subtractPosition (pointInParentSpace, comp);
        }

        return pointInParentSpace;
    }

    template <typename PointOrRect>
    static PointOrRect convertToParentSpace (const Component& comp, PointOrRect pointInLocalSpace)
    {
        if (comp.isOnDesktop())
        {
            if (auto* peer = comp.getPeer())
                pointInLocalSpace = ScalingHelpers::unscaledScreenPosToScaled
                                        (peer->localToGlobal (ScalingHelpers::scaledScreenPosToUnscaled (comp, pointInLocalSpace)));
            else
                jassertfalse;
        }
        else
        {
            pointInLocalSpace = ScalingHelpers::addPosition (pointInLocalSpace, comp);
        }

        if (comp.affineTransform != nullptr)
            pointInLocalSpace = pointInLocalSpace.transformedBy (*comp.affineTransform);

        return pointInLocalSpace;
    }

    template <typename PointOrRect>
    static PointOrRect convertFromDistantParentSpace (const Component* parent, const Component& target, PointOrRect coordInParent)
    {
        auto* directParent = target.getParentComponent();
        jassert (directParent != nullptr);

        if (directParent == parent)
            return convertFromParentSpace (target, coordInParent);

        return convertFromParentSpace (target, convertFromDistantParentSpace (parent, *directParent, coordInParent));
    }

    template <typename PointOrRect>
    static PointOrRect convertCoordinate (const Component* target, const Component* source, PointOrRect p)
    {
        while (source != nullptr)
        {
            if (source == target)
                return p;

            if (source->isParentOf (target))
                return convertFromDistantParentSpace (source, *target, p);

            p = convertToParentSpace (*source, p);
            source = source->getParentComponent();
        }

        jassert (source == nullptr);
        if (target == nullptr)
            return p;

        auto* topLevelComp = target->getTopLevelComponent();

        p = convertFromParentSpace (*topLevelComp, p);

        if (topLevelComp == target)
            return p;

        return convertFromDistantParentSpace (topLevelComp, *target, p);
    }

    static bool clipObscuredRegions (const Component& comp, Graphics& g,
                                     const Rectangle<int> clipRect, Point<int> delta)
    {
        bool wasClipped = false;

        for (int i = comp.childComponentList.size(); --i >= 0;)
        {
            auto& child = *comp.childComponentList.getUnchecked(i);

            if (child.isVisible() && ! child.isTransformed())
            {
                auto newClip = clipRect.getIntersection (child.boundsRelativeToParent);

                if (! newClip.isEmpty())
                {
                    if (child.isOpaque() && child.componentTransparency == 0)
                    {
                        g.excludeClipRegion (newClip + delta);
                        wasClipped = true;
                    }
                    else
                    {
                        auto childPos = child.getPosition();

                        if (clipObscuredRegions (child, g, newClip - childPos, childPos + delta))
                            wasClipped = true;
                    }
                }
            }
        }

        return wasClipped;
    }

    static Rectangle<int> getParentOrMainMonitorBounds (const Component& comp)
    {
        if (auto* p = comp.getParentComponent())
            return p->getLocalBounds();

        return Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
    }

    static void releaseAllCachedImageResources (Component& c)
    {
        if (auto* cached = c.getCachedComponentImage())
            cached->releaseResources();

        for (auto* child : c.childComponentList)
            releaseAllCachedImageResources (*child);
    }
};

//==============================================================================
Component::Component() noexcept
  : componentFlags (0)
{
}

Component::Component (const String& name) noexcept
  : componentName (name), componentFlags (0)
{
}

Component::~Component()
{
    static_assert (sizeof (flags) <= sizeof (componentFlags), "componentFlags has too many bits!");

    componentListeners.call ([this] (ComponentListener& l) { l.componentBeingDeleted (*this); });

    masterReference.clear();

    while (childComponentList.size() > 0)
        removeChildComponent (childComponentList.size() - 1, false, true);

    if (parentComponent != nullptr)
        parentComponent->removeChildComponent (parentComponent->childComponentList.indexOf (this), true, false);
    else if (hasKeyboardFocus (true))
        giveAwayFocus (currentlyFocusedComponent != this);

    if (flags.hasHeavyweightPeerFlag)
        removeFromDesktop();

    // Something has added some children to this component during its destructor! Not a smart idea!
    jassert (childComponentList.size() == 0);
}

//==============================================================================
void Component::setName (const String& name)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN

    if (componentName != name)
    {
        componentName = name;

        if (flags.hasHeavyweightPeerFlag)
            if (auto* peer = getPeer())
                peer->setTitle (name);

        BailOutChecker checker (this);
        componentListeners.callChecked (checker, [this] (ComponentListener& l) { l.componentNameChanged (*this); });
    }
}

void Component::setComponentID (const String& newID)
{
    componentID = newID;
}

void Component::setVisible (bool shouldBeVisible)
{
    if (flags.visibleFlag != shouldBeVisible)
    {
        // if component methods are being called from threads other than the message
        // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
        JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN

        const WeakReference<Component> safePointer (this);
        flags.visibleFlag = shouldBeVisible;

        if (shouldBeVisible)
            repaint();
        else
            repaintParent();

        sendFakeMouseMove();

        if (! shouldBeVisible)
        {
            ComponentHelpers::releaseAllCachedImageResources (*this);

            if (hasKeyboardFocus (true))
            {
                if (parentComponent != nullptr)
                    parentComponent->grabKeyboardFocus();

                if (hasKeyboardFocus (true))
                    giveAwayFocus (true);
            }
        }

        if (safePointer != nullptr)
        {
            sendVisibilityChangeMessage();

            if (safePointer != nullptr && flags.hasHeavyweightPeerFlag)
            {
                if (auto* peer = getPeer())
                {
                    peer->setVisible (shouldBeVisible);
                    internalHierarchyChanged();
                }
            }
        }
    }
}

void Component::visibilityChanged() {}

void Component::sendVisibilityChangeMessage()
{
    BailOutChecker checker (this);
    visibilityChanged();

    if (! checker.shouldBailOut())
        componentListeners.callChecked (checker, [this] (ComponentListener& l) { l.componentVisibilityChanged (*this); });
}

bool Component::isShowing() const
{
    if (! flags.visibleFlag)
        return false;

    if (parentComponent != nullptr)
        return parentComponent->isShowing();

    if (auto* peer = getPeer())
        return ! peer->isMinimised();

    return false;
}

//==============================================================================
void* Component::getWindowHandle() const
{
    if (auto* peer = getPeer())
        return peer->getNativeHandle();

    return nullptr;
}

//==============================================================================
void Component::addToDesktop (int styleWanted, void* nativeWindowToAttachTo)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED

    if (isOpaque())
        styleWanted &= ~ComponentPeer::windowIsSemiTransparent;
    else
        styleWanted |= ComponentPeer::windowIsSemiTransparent;

    // don't use getPeer(), so that we only get the peer that's specifically
    // for this comp, and not for one of its parents.
    auto* peer = ComponentPeer::getPeerFor (this);

    if (peer == nullptr || styleWanted != peer->getStyleFlags())
    {
        const WeakReference<Component> safePointer (this);

       #if JUCE_LINUX
        // it's wise to give the component a non-zero size before
        // putting it on the desktop, as X windows get confused by this, and
        // a (1, 1) minimum size is enforced here.
        setSize (jmax (1, getWidth()),
                 jmax (1, getHeight()));
       #endif

        auto topLeft = getScreenPosition();

        bool wasFullscreen = false;
        bool wasMinimised = false;
        ComponentBoundsConstrainer* currentConstrainer = nullptr;
        Rectangle<int> oldNonFullScreenBounds;
        int oldRenderingEngine = -1;

        if (peer != nullptr)
        {
            std::unique_ptr<ComponentPeer> oldPeerToDelete (peer);

            wasFullscreen = peer->isFullScreen();
            wasMinimised = peer->isMinimised();
            currentConstrainer = peer->getConstrainer();
            oldNonFullScreenBounds = peer->getNonFullScreenBounds();
            oldRenderingEngine = peer->getCurrentRenderingEngine();

            flags.hasHeavyweightPeerFlag = false;
            Desktop::getInstance().removeDesktopComponent (this);
            internalHierarchyChanged(); // give comps a chance to react to the peer change before the old peer is deleted.

            if (safePointer == nullptr)
                return;

            setTopLeftPosition (topLeft);
        }

        if (parentComponent != nullptr)
            parentComponent->removeChildComponent (this);

        if (safePointer != nullptr)
        {
            flags.hasHeavyweightPeerFlag = true;

            peer = createNewPeer (styleWanted, nativeWindowToAttachTo);

            Desktop::getInstance().addDesktopComponent (this);

            boundsRelativeToParent.setPosition (topLeft);
            peer->updateBounds();

            if (oldRenderingEngine >= 0)
                peer->setCurrentRenderingEngine (oldRenderingEngine);

            peer->setVisible (isVisible());

            peer = ComponentPeer::getPeerFor (this);

            if (peer == nullptr)
                return;

            if (wasFullscreen)
            {
                peer->setFullScreen (true);
                peer->setNonFullScreenBounds (oldNonFullScreenBounds);
            }

            if (wasMinimised)
                peer->setMinimised (true);

           #if JUCE_WINDOWS
            if (isAlwaysOnTop())
                peer->setAlwaysOnTop (true);
           #endif

            peer->setConstrainer (currentConstrainer);

            repaint();
            internalHierarchyChanged();
        }
    }
}

void Component::removeFromDesktop()
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN

    if (flags.hasHeavyweightPeerFlag)
    {
        ComponentHelpers::releaseAllCachedImageResources (*this);

        auto* peer = ComponentPeer::getPeerFor (this);
        jassert (peer != nullptr);

        flags.hasHeavyweightPeerFlag = false;
        delete peer;

        Desktop::getInstance().removeDesktopComponent (this);
    }
}

bool Component::isOnDesktop() const noexcept
{
    return flags.hasHeavyweightPeerFlag;
}

ComponentPeer* Component::getPeer() const
{
    if (flags.hasHeavyweightPeerFlag)
        return ComponentPeer::getPeerFor (this);

    if (parentComponent == nullptr)
        return nullptr;

    return parentComponent->getPeer();
}

void Component::userTriedToCloseWindow()
{
    /* This means that the user's trying to get rid of your window with the 'close window' system
       menu option (on windows) or possibly the task manager - you should really handle this
       and delete or hide your component in an appropriate way.

       If you want to ignore the event and don't want to trigger this assertion, just override
       this method and do nothing.
    */
    jassertfalse;
}

void Component::minimisationStateChanged (bool) {}

float Component::getDesktopScaleFactor() const  { return Desktop::getInstance().getGlobalScaleFactor(); }

//==============================================================================
void Component::setOpaque (bool shouldBeOpaque)
{
    if (shouldBeOpaque != flags.opaqueFlag)
    {
        flags.opaqueFlag = shouldBeOpaque;

        if (flags.hasHeavyweightPeerFlag)
            if (auto* peer = ComponentPeer::getPeerFor (this))
                addToDesktop (peer->getStyleFlags());  // recreates the heavyweight window

        repaint();
    }
}

bool Component::isOpaque() const noexcept
{
    return flags.opaqueFlag;
}

//==============================================================================
struct StandardCachedComponentImage  : public CachedComponentImage
{
    StandardCachedComponentImage (Component& c) noexcept : owner (c)  {}

    void paint (Graphics& g) override
    {
        scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        auto compBounds = owner.getLocalBounds();
        auto imageBounds = compBounds * scale;

        if (image.isNull() || image.getBounds() != imageBounds)
        {
            image = Image (owner.isOpaque() ? Image::RGB
                                            : Image::ARGB,
                           jmax (1, imageBounds.getWidth()),
                           jmax (1, imageBounds.getHeight()),
                           ! owner.isOpaque());

            validArea.clear();
        }

        if (! validArea.containsRectangle (compBounds))
        {
            Graphics imG (image);
            auto& lg = imG.getInternalContext();

            lg.addTransform (AffineTransform::scale (scale));

            for (auto& i : validArea)
                lg.excludeClipRectangle (i);

            if (! owner.isOpaque())
            {
                lg.setFill (Colours::transparentBlack);
                lg.fillRect (compBounds, true);
                lg.setFill (Colours::black);
            }

            owner.paintEntireComponent (imG, true);
        }

        validArea = compBounds;

        g.setColour (Colours::black.withAlpha (owner.getAlpha()));
        g.drawImageTransformed (image, AffineTransform::scale ((float) compBounds.getWidth()  / (float) imageBounds.getWidth(),
                                                               (float) compBounds.getHeight() / (float) imageBounds.getHeight()), false);
    }

    bool invalidateAll() override                            { validArea.clear(); return true; }
    bool invalidate (const Rectangle<int>& area) override    { validArea.subtract (area); return true; }
    void releaseResources() override                         { image = Image(); }

private:
    Image image;
    RectangleList<int> validArea;
    Component& owner;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandardCachedComponentImage)
};

void Component::setCachedComponentImage (CachedComponentImage* newCachedImage)
{
    if (cachedImage.get() != newCachedImage)
    {
        cachedImage.reset (newCachedImage);
        repaint();
    }
}

void Component::setBufferedToImage (bool shouldBeBuffered)
{
    // This assertion means that this component is already using a custom CachedComponentImage,
    // so by calling setBufferedToImage, you'll be deleting the custom one - this is almost certainly
    // not what you wanted to happen... If you really do know what you're doing here, and want to
    // avoid this assertion, just call setCachedComponentImage (nullptr) before setBufferedToImage().
    jassert (cachedImage == nullptr || dynamic_cast<StandardCachedComponentImage*> (cachedImage.get()) != nullptr);

    if (shouldBeBuffered)
    {
        if (cachedImage == nullptr)
            cachedImage.reset (new StandardCachedComponentImage (*this));
    }
    else
    {
        cachedImage.reset();
    }
}

//==============================================================================
void Component::reorderChildInternal (int sourceIndex, int destIndex)
{
    if (sourceIndex != destIndex)
    {
        auto* c = childComponentList.getUnchecked (sourceIndex);
        jassert (c != nullptr);
        c->repaintParent();

        childComponentList.move (sourceIndex, destIndex);

        sendFakeMouseMove();
        internalChildrenChanged();
    }
}

void Component::toFront (bool setAsForeground)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN

    if (flags.hasHeavyweightPeerFlag)
    {
        if (auto* peer = getPeer())
        {
            peer->toFront (setAsForeground);

            if (setAsForeground && ! hasKeyboardFocus (true))
                grabKeyboardFocus();
        }
    }
    else if (parentComponent != nullptr)
    {
        auto& childList = parentComponent->childComponentList;

        if (childList.getLast() != this)
        {
            auto index = childList.indexOf (this);

            if (index >= 0)
            {
                int insertIndex = -1;

                if (! flags.alwaysOnTopFlag)
                {
                    insertIndex = childList.size() - 1;

                    while (insertIndex > 0 && childList.getUnchecked (insertIndex)->isAlwaysOnTop())
                        --insertIndex;
                }

                parentComponent->reorderChildInternal (index, insertIndex);
            }
        }

        if (setAsForeground)
        {
            internalBroughtToFront();

            if (isShowing())
                grabKeyboardFocus();
        }
    }
}

void Component::toBehind (Component* other)
{
    if (other != nullptr && other != this)
    {
        // the two components must belong to the same parent..
        jassert (parentComponent == other->parentComponent);

        if (parentComponent != nullptr)
        {
            auto& childList = parentComponent->childComponentList;
            auto index = childList.indexOf (this);

            if (index >= 0 && childList [index + 1] != other)
            {
                auto otherIndex = childList.indexOf (other);

                if (otherIndex >= 0)
                {
                    if (index < otherIndex)
                        --otherIndex;

                    parentComponent->reorderChildInternal (index, otherIndex);
                }
            }
        }
        else if (isOnDesktop())
        {
            jassert (other->isOnDesktop());

            if (other->isOnDesktop())
            {
                auto* us = getPeer();
                auto* them = other->getPeer();
                jassert (us != nullptr && them != nullptr);

                if (us != nullptr && them != nullptr)
                    us->toBehind (them);
            }
        }
    }
}

void Component::toBack()
{
    if (isOnDesktop())
    {
        jassertfalse; //xxx need to add this to native window
    }
    else if (parentComponent != nullptr)
    {
        auto& childList = parentComponent->childComponentList;

        if (childList.getFirst() != this)
        {
            auto index = childList.indexOf (this);

            if (index > 0)
            {
                int insertIndex = 0;

                if (flags.alwaysOnTopFlag)
                    while (insertIndex < childList.size() && ! childList.getUnchecked (insertIndex)->isAlwaysOnTop())
                        ++insertIndex;

                parentComponent->reorderChildInternal (index, insertIndex);
            }
        }
    }
}

void Component::setAlwaysOnTop (bool shouldStayOnTop)
{
    if (shouldStayOnTop != flags.alwaysOnTopFlag)
    {
        BailOutChecker checker (this);

        flags.alwaysOnTopFlag = shouldStayOnTop;

        if (isOnDesktop())
        {
            if (auto* peer = getPeer())
            {
                if (! peer->setAlwaysOnTop (shouldStayOnTop))
                {
                    // some kinds of peer can't change their always-on-top status, so
                    // for these, we'll need to create a new window
                    auto oldFlags = peer->getStyleFlags();
                    removeFromDesktop();
                    addToDesktop (oldFlags);
                }
            }
        }

        if (shouldStayOnTop && ! checker.shouldBailOut())
            toFront (false);

        if (! checker.shouldBailOut())
            internalHierarchyChanged();
    }
}

bool Component::isAlwaysOnTop() const noexcept
{
    return flags.alwaysOnTopFlag;
}

//==============================================================================
int Component::proportionOfWidth  (float proportion) const noexcept   { return roundToInt (proportion * (float) boundsRelativeToParent.getWidth()); }
int Component::proportionOfHeight (float proportion) const noexcept   { return roundToInt (proportion * (float) boundsRelativeToParent.getHeight()); }

int Component::getParentWidth() const noexcept
{
    return parentComponent != nullptr ? parentComponent->getWidth()
                                      : getParentMonitorArea().getWidth();
}

int Component::getParentHeight() const noexcept
{
    return parentComponent != nullptr ? parentComponent->getHeight()
                                      : getParentMonitorArea().getHeight();
}

Rectangle<int> Component::getParentMonitorArea() const
{
    return Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())->userArea;
}

int Component::getScreenX() const                       { return getScreenPosition().x; }
int Component::getScreenY() const                       { return getScreenPosition().y; }
Point<int>     Component::getScreenPosition() const     { return localPointToGlobal (Point<int>()); }
Rectangle<int> Component::getScreenBounds() const       { return localAreaToGlobal (getLocalBounds()); }

Point<int>       Component::getLocalPoint (const Component* source, Point<int> point) const       { return ComponentHelpers::convertCoordinate (this, source, point); }
Point<float>     Component::getLocalPoint (const Component* source, Point<float> point) const     { return ComponentHelpers::convertCoordinate (this, source, point); }
Rectangle<int>   Component::getLocalArea  (const Component* source, Rectangle<int> area) const    { return ComponentHelpers::convertCoordinate (this, source, area); }
Rectangle<float> Component::getLocalArea  (const Component* source, Rectangle<float> area) const  { return ComponentHelpers::convertCoordinate (this, source, area); }

Point<int>       Component::localPointToGlobal (Point<int> point) const       { return ComponentHelpers::convertCoordinate (nullptr, this, point); }
Point<float>     Component::localPointToGlobal (Point<float> point) const     { return ComponentHelpers::convertCoordinate (nullptr, this, point); }
Rectangle<int>   Component::localAreaToGlobal  (Rectangle<int> area) const    { return ComponentHelpers::convertCoordinate (nullptr, this, area); }
Rectangle<float> Component::localAreaToGlobal  (Rectangle<float> area) const  { return ComponentHelpers::convertCoordinate (nullptr, this, area); }

//==============================================================================
void Component::setBounds (int x, int y, int w, int h)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN

    if (w < 0) w = 0;
    if (h < 0) h = 0;

    const bool wasResized  = (getWidth() != w || getHeight() != h);
    const bool wasMoved    = (getX() != x || getY() != y);

   #if JUCE_DEBUG
    // It's a very bad idea to try to resize a window during its paint() method!
    jassert (! (flags.isInsidePaintCall && wasResized && isOnDesktop()));
   #endif

    if (wasMoved || wasResized)
    {
        const bool showing = isShowing();

        if (showing)
        {
            // send a fake mouse move to trigger enter/exit messages if needed..
            sendFakeMouseMove();

            if (! flags.hasHeavyweightPeerFlag)
                repaintParent();
        }

        boundsRelativeToParent.setBounds (x, y, w, h);

        if (showing)
        {
            if (wasResized)
                repaint();
            else if (! flags.hasHeavyweightPeerFlag)
                repaintParent();
        }
        else if (cachedImage != nullptr)
        {
            cachedImage->invalidateAll();
        }

        flags.isMoveCallbackPending = wasMoved;
        flags.isResizeCallbackPending = wasResized;

        if (flags.hasHeavyweightPeerFlag)
            if (auto* peer = getPeer())
                peer->updateBounds();

        sendMovedResizedMessagesIfPending();
    }
}

void Component::sendMovedResizedMessagesIfPending()
{
    const bool wasMoved   = flags.isMoveCallbackPending;
    const bool wasResized = flags.isResizeCallbackPending;

    if (wasMoved || wasResized)
    {
        flags.isMoveCallbackPending = false;
        flags.isResizeCallbackPending = false;

        sendMovedResizedMessages (wasMoved, wasResized);
    }
}

void Component::sendMovedResizedMessages (bool wasMoved, bool wasResized)
{
    BailOutChecker checker (this);

    if (wasMoved)
    {
        moved();

        if (checker.shouldBailOut())
            return;
    }

    if (wasResized)
    {
        resized();

        if (checker.shouldBailOut())
            return;

        for (int i = childComponentList.size(); --i >= 0;)
        {
            childComponentList.getUnchecked(i)->parentSizeChanged();

            if (checker.shouldBailOut())
                return;

            i = jmin (i, childComponentList.size());
        }
    }

    if (parentComponent != nullptr)
        parentComponent->childBoundsChanged (this);

    if (! checker.shouldBailOut())
    {
        componentListeners.callChecked (checker, [this, wasMoved, wasResized] (ComponentListener& l)
        {
            l.componentMovedOrResized (*this, wasMoved, wasResized);
        });
    }
}

void Component::setSize (int w, int h)                  { setBounds (getX(), getY(), w, h); }

void Component::setTopLeftPosition (int x, int y)       { setTopLeftPosition ({ x, y }); }
void Component::setTopLeftPosition (Point<int> pos)     { setBounds (pos.x, pos.y, getWidth(), getHeight()); }

void Component::setTopRightPosition (int x, int y)      { setTopLeftPosition (x - getWidth(), y); }
void Component::setBounds (Rectangle<int> r)            { setBounds (r.getX(), r.getY(), r.getWidth(), r.getHeight()); }

void Component::setCentrePosition (Point<int> p)        { setBounds (getBounds().withCentre (p.transformedBy (getTransform().inverted()))); }
void Component::setCentrePosition (int x, int y)        { setCentrePosition ({ x, y }); }

void Component::setCentreRelative (float x, float y)
{
    setCentrePosition (roundToInt ((float) getParentWidth()  * x),
                       roundToInt ((float) getParentHeight() * y));
}

void Component::setBoundsRelative (Rectangle<float> target)
{
    setBounds ((target * Point<float> ((float) getParentWidth(),
                                       (float) getParentHeight())).toNearestInt());
}

void Component::setBoundsRelative (float x, float y, float w, float h)
{
    setBoundsRelative ({ x, y, w, h });
}

void Component::centreWithSize (int width, int height)
{
    auto parentArea = ComponentHelpers::getParentOrMainMonitorBounds (*this)
                          .transformedBy (getTransform().inverted());

    setBounds (parentArea.getCentreX() - width / 2,
               parentArea.getCentreY() - height / 2,
               width, height);
}

void Component::setBoundsInset (BorderSize<int> borders)
{
    setBounds (borders.subtractedFrom (ComponentHelpers::getParentOrMainMonitorBounds (*this)));
}

void Component::setBoundsToFit (Rectangle<int> targetArea, Justification justification, bool onlyReduceInSize)
{
    if (getLocalBounds().isEmpty() || targetArea.isEmpty())
    {
        // it's no good calling this method unless both the component and
        // target rectangle have a finite size.
        jassertfalse;
        return;
    }

    auto sourceArea = targetArea.withZeroOrigin();

    if (onlyReduceInSize
         && getWidth() <= targetArea.getWidth()
         && getHeight() <= targetArea.getHeight())
    {
        sourceArea = getLocalBounds();
    }
    else
    {
        auto sourceRatio = getHeight() / (double) getWidth();
        auto targetRatio = targetArea.getHeight() / (double) targetArea.getWidth();

        if (sourceRatio <= targetRatio)
            sourceArea.setHeight (jmin (targetArea.getHeight(),
                                        roundToInt (targetArea.getWidth() * sourceRatio)));
        else
            sourceArea.setWidth (jmin (targetArea.getWidth(),
                                       roundToInt (targetArea.getHeight() / sourceRatio)));
    }

    if (! sourceArea.isEmpty())
        setBounds (justification.appliedToRectangle (sourceArea, targetArea));
}

//==============================================================================
void Component::setTransform (const AffineTransform& newTransform)
{
    // If you pass in a transform with no inverse, the component will have no dimensions,
    // and there will be all sorts of maths errors when converting coordinates.
    jassert (! newTransform.isSingularity());

    if (newTransform.isIdentity())
    {
        if (affineTransform != nullptr)
        {
            repaint();
            affineTransform.reset();
            repaint();
            sendMovedResizedMessages (false, false);
        }
    }
    else if (affineTransform == nullptr)
    {
        repaint();
        affineTransform.reset (new AffineTransform (newTransform));
        repaint();
        sendMovedResizedMessages (false, false);
    }
    else if (*affineTransform != newTransform)
    {
        repaint();
        *affineTransform = newTransform;
        repaint();
        sendMovedResizedMessages (false, false);
    }
}

bool Component::isTransformed() const noexcept
{
    return affineTransform != nullptr;
}

AffineTransform Component::getTransform() const
{
    return affineTransform != nullptr ? *affineTransform : AffineTransform();
}

float Component::getApproximateScaleFactorForComponent (Component* targetComponent)
{
    AffineTransform transform;

    for (auto* target = targetComponent; target != nullptr; target = target->getParentComponent())
    {
        transform = transform.followedBy (target->getTransform());

        if (target->isOnDesktop())
            transform = transform.scaled (target->getDesktopScaleFactor());
    }

    auto transformScale = std::sqrt (std::abs (transform.getDeterminant()));
    return transformScale / Desktop::getInstance().getGlobalScaleFactor();
}

//==============================================================================
bool Component::hitTest (int x, int y)
{
    if (! flags.ignoresMouseClicksFlag)
        return true;

    if (flags.allowChildMouseClicksFlag)
    {
        for (int i = childComponentList.size(); --i >= 0;)
        {
            auto& child = *childComponentList.getUnchecked (i);

            if (child.isVisible()
                 && ComponentHelpers::hitTest (child, ComponentHelpers::convertFromParentSpace (child, Point<int> (x, y))))
                return true;
        }
    }

    return false;
}

void Component::setInterceptsMouseClicks (bool allowClicks,
                                          bool allowClicksOnChildComponents) noexcept
{
    flags.ignoresMouseClicksFlag = ! allowClicks;
    flags.allowChildMouseClicksFlag = allowClicksOnChildComponents;
}

void Component::getInterceptsMouseClicks (bool& allowsClicksOnThisComponent,
                                          bool& allowsClicksOnChildComponents) const noexcept
{
    allowsClicksOnThisComponent = ! flags.ignoresMouseClicksFlag;
    allowsClicksOnChildComponents = flags.allowChildMouseClicksFlag;
}

bool Component::contains (Point<int> point)
{
    if (ComponentHelpers::hitTest (*this, point))
    {
        if (parentComponent != nullptr)
            return parentComponent->contains (ComponentHelpers::convertToParentSpace (*this, point));

        if (flags.hasHeavyweightPeerFlag)
            if (auto* peer = getPeer())
                return peer->contains (ComponentHelpers::localPositionToRawPeerPos (*this, point), true);
    }

    return false;
}

bool Component::reallyContains (Point<int> point, bool returnTrueIfWithinAChild)
{
    if (! contains (point))
        return false;

    auto* top = getTopLevelComponent();
    auto* compAtPosition = top->getComponentAt (top->getLocalPoint (this, point));

    return (compAtPosition == this) || (returnTrueIfWithinAChild && isParentOf (compAtPosition));
}

Component* Component::getComponentAt (Point<int> position)
{
    if (flags.visibleFlag && ComponentHelpers::hitTest (*this, position))
    {
        for (int i = childComponentList.size(); --i >= 0;)
        {
            auto* child = childComponentList.getUnchecked(i);

            child = child->getComponentAt (ComponentHelpers::convertFromParentSpace (*child, position));

            if (child != nullptr)
                return child;
        }

        return this;
    }

    return nullptr;
}

Component* Component::getComponentAt (int x, int y)
{
    return getComponentAt ({ x, y });
}

//==============================================================================
void Component::addChildComponent (Component& child, int zOrder)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN

    jassert (this != &child); // adding a component to itself!?

    if (child.parentComponent != this)
    {
        if (child.parentComponent != nullptr)
            child.parentComponent->removeChildComponent (&child);
        else
            child.removeFromDesktop();

        child.parentComponent = this;

        if (child.isVisible())
            child.repaintParent();

        if (! child.isAlwaysOnTop())
        {
            if (zOrder < 0 || zOrder > childComponentList.size())
                zOrder = childComponentList.size();

            while (zOrder > 0)
            {
                if (! childComponentList.getUnchecked (zOrder - 1)->isAlwaysOnTop())
                    break;

                --zOrder;
            }
        }

        childComponentList.insert (zOrder, &child);

        child.internalHierarchyChanged();
        internalChildrenChanged();
      
        child.lookAndFeelChanged();
    }
}

void Component::addAndMakeVisible (Component& child, int zOrder)
{
    child.setVisible (true);
    addChildComponent (child, zOrder);
}

void Component::addChildComponent (Component* child, int zOrder)
{
    if (child != nullptr)
        addChildComponent (*child, zOrder);
}

void Component::addAndMakeVisible (Component* child, int zOrder)
{
    if (child != nullptr)
        addAndMakeVisible (*child, zOrder);
}

void Component::addChildAndSetID (Component* child, const String& childID)
{
    if (child != nullptr)
    {
        child->setComponentID (childID);
        addAndMakeVisible (child);
    }
}

void Component::removeChildComponent (Component* child)
{
    removeChildComponent (childComponentList.indexOf (child), true, true);
}

Component* Component::removeChildComponent (int index)
{
    return removeChildComponent (index, true, true);
}

Component* Component::removeChildComponent (int index, bool sendParentEvents, bool sendChildEvents)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN

    auto* child = childComponentList [index];

    if (child != nullptr)
    {
        sendParentEvents = sendParentEvents && child->isShowing();

        if (sendParentEvents)
        {
            sendFakeMouseMove();

            if (child->isVisible())
                child->repaintParent();
        }

        childComponentList.remove (index);
        child->parentComponent = nullptr;

        ComponentHelpers::releaseAllCachedImageResources (*child);

        // (NB: there are obscure situations where child->isShowing() = false, but it still has the focus)
        if (currentlyFocusedComponent == child || child->isParentOf (currentlyFocusedComponent))
        {
            if (sendParentEvents)
            {
                const WeakReference<Component> thisPointer (this);

                giveAwayFocus (sendChildEvents || currentlyFocusedComponent != child);

                if (thisPointer == nullptr)
                    return child;

                grabKeyboardFocus();
            }
            else
            {
                giveAwayFocus (sendChildEvents || currentlyFocusedComponent != child);
            }
        }

        if (sendChildEvents)
            child->internalHierarchyChanged();

        if (sendParentEvents)
            internalChildrenChanged();
    }

    return child;
}

//==============================================================================
void Component::removeAllChildren()
{
    while (! childComponentList.isEmpty())
        removeChildComponent (childComponentList.size() - 1);
}

void Component::deleteAllChildren()
{
    while (! childComponentList.isEmpty())
        delete (removeChildComponent (childComponentList.size() - 1));
}

int Component::getNumChildComponents() const noexcept
{
    return childComponentList.size();
}

Component* Component::getChildComponent (int index) const noexcept
{
    return childComponentList[index];
}

int Component::getIndexOfChildComponent (const Component* child) const noexcept
{
    return childComponentList.indexOf (const_cast<Component*> (child));
}

Component* Component::findChildWithID (StringRef targetID) const noexcept
{
    for (auto* c : childComponentList)
        if (c->componentID == targetID)
            return c;

    return nullptr;
}

Component* Component::getTopLevelComponent() const noexcept
{
    auto* comp = this;

    while (comp->parentComponent != nullptr)
        comp = comp->parentComponent;

    return const_cast<Component*> (comp);
}

bool Component::isParentOf (const Component* possibleChild) const noexcept
{
    while (possibleChild != nullptr)
    {
        possibleChild = possibleChild->parentComponent;

        if (possibleChild == this)
            return true;
    }

    return false;
}

//==============================================================================
void Component::parentHierarchyChanged() {}
void Component::childrenChanged() {}

void Component::internalChildrenChanged()
{
    if (componentListeners.isEmpty())
    {
        childrenChanged();
    }
    else
    {
        BailOutChecker checker (this);

        childrenChanged();

        if (! checker.shouldBailOut())
            componentListeners.callChecked (checker, [this] (ComponentListener& l) { l.componentChildrenChanged (*this); });
    }
}

void Component::internalHierarchyChanged()
{
    BailOutChecker checker (this);

    parentHierarchyChanged();

    if (checker.shouldBailOut())
        return;

    componentListeners.callChecked (checker, [this] (ComponentListener& l) { l.componentParentHierarchyChanged (*this); });

    if (checker.shouldBailOut())
        return;

    for (int i = childComponentList.size(); --i >= 0;)
    {
        childComponentList.getUnchecked (i)->internalHierarchyChanged();

        if (checker.shouldBailOut())
        {
            // you really shouldn't delete the parent component during a callback telling you
            // that it's changed..
            jassertfalse;
            return;
        }

        i = jmin (i, childComponentList.size());
    }
}

//==============================================================================
#if JUCE_MODAL_LOOPS_PERMITTED
int Component::runModalLoop()
{
    if (! MessageManager::getInstance()->isThisTheMessageThread())
    {
        // use a callback so this can be called from non-gui threads
        return (int) (pointer_sized_int) MessageManager::getInstance()
                                           ->callFunctionOnMessageThread (&ComponentHelpers::runModalLoopCallback, this);
    }

    if (! isCurrentlyModal (false))
        enterModalState (true);

    return ModalComponentManager::getInstance()->runEventLoopForCurrentComponent();
}
#endif

//==============================================================================
void Component::enterModalState (bool shouldTakeKeyboardFocus,
                                 ModalComponentManager::Callback* callback,
                                 bool deleteWhenDismissed)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED

    if (! isCurrentlyModal (false))
    {
        auto& mcm = *ModalComponentManager::getInstance();
        mcm.startModal (this, deleteWhenDismissed);
        mcm.attachCallback (this, callback);

        setVisible (true);

        if (shouldTakeKeyboardFocus)
            grabKeyboardFocus();
    }
    else
    {
        // Probably a bad idea to try to make a component modal twice!
        jassertfalse;
    }
}

void Component::exitModalState (int returnValue)
{
    if (isCurrentlyModal (false))
    {
        if (MessageManager::getInstance()->isThisTheMessageThread())
        {
            auto& mcm = *ModalComponentManager::getInstance();
            mcm.endModal (this, returnValue);
            mcm.bringModalComponentsToFront();

            // If any of the mouse sources are over another Component when we exit the modal state then send a mouse enter event
            for (auto& ms : Desktop::getInstance().getMouseSources())
                if (auto* c = ms.getComponentUnderMouse())
                    c->internalMouseEnter (ms, ms.getScreenPosition(), Time::getCurrentTime());
        }
        else
        {
            WeakReference<Component> target (this);

            MessageManager::callAsync ([=]
            {
                if (auto* c = target.get())
                    c->exitModalState (returnValue);
            });
        }
    }
}

bool Component::isCurrentlyModal (bool onlyConsiderForemostModalComponent) const noexcept
{
    auto& mcm = *ModalComponentManager::getInstance();

    return onlyConsiderForemostModalComponent ? mcm.isFrontModalComponent (this)
                                              : mcm.isModal (this);
}

bool Component::isCurrentlyBlockedByAnotherModalComponent() const
{
    auto* mc = getCurrentlyModalComponent();

    return ! (mc == nullptr || mc == this || mc->isParentOf (this)
               || mc->canModalEventBeSentToComponent (this));
}

int JUCE_CALLTYPE Component::getNumCurrentlyModalComponents() noexcept
{
    return ModalComponentManager::getInstance()->getNumModalComponents();
}

Component* JUCE_CALLTYPE Component::getCurrentlyModalComponent (int index) noexcept
{
    return ModalComponentManager::getInstance()->getModalComponent (index);
}

//==============================================================================
void Component::setBroughtToFrontOnMouseClick (bool shouldBeBroughtToFront) noexcept
{
    flags.bringToFrontOnClickFlag = shouldBeBroughtToFront;
}

bool Component::isBroughtToFrontOnMouseClick() const noexcept
{
    return flags.bringToFrontOnClickFlag;
}

//==============================================================================
void Component::setMouseCursor (const MouseCursor& newCursor)
{
    if (cursor != newCursor)
    {
        cursor = newCursor;

        if (flags.visibleFlag)
            updateMouseCursor();
    }
}

MouseCursor Component::getMouseCursor()
{
    return cursor;
}

void Component::updateMouseCursor() const
{
    Desktop::getInstance().getMainMouseSource().forceMouseCursorUpdate();
}

//==============================================================================
void Component::setRepaintsOnMouseActivity (bool shouldRepaint) noexcept
{
    flags.repaintOnMouseActivityFlag = shouldRepaint;
}

//==============================================================================
float Component::getAlpha() const noexcept
{
    return (255 - componentTransparency) / 255.0f;
}

void Component::setAlpha (float newAlpha)
{
    auto newIntAlpha = (uint8) (255 - jlimit (0, 255, roundToInt (newAlpha * 255.0)));

    if (componentTransparency != newIntAlpha)
    {
        componentTransparency = newIntAlpha;
        alphaChanged();
    }
}

void Component::alphaChanged()
{
    if (flags.hasHeavyweightPeerFlag)
    {
        if (auto* peer = getPeer())
            peer->setAlpha (getAlpha());
    }
    else
    {
        repaint();
    }
}

//==============================================================================
void Component::repaint()
{
    internalRepaintUnchecked (getLocalBounds(), true);
}

void Component::repaint (int x, int y, int w, int h)
{
    internalRepaint ({ x, y, w, h });
}

void Component::repaint (Rectangle<int> area)
{
    internalRepaint (area);
}

void Component::repaintParent()
{
    if (parentComponent != nullptr)
        parentComponent->internalRepaint (ComponentHelpers::convertToParentSpace (*this, getLocalBounds()));
}

void Component::internalRepaint (Rectangle<int> area)
{
    area = area.getIntersection (getLocalBounds());

    if (! area.isEmpty())
        internalRepaintUnchecked (area, false);
}

void Component::internalRepaintUnchecked (Rectangle<int> area, bool isEntireComponent)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED

    if (flags.visibleFlag)
    {
        if (cachedImage != nullptr)
            if (! (isEntireComponent ? cachedImage->invalidateAll()
                                     : cachedImage->invalidate (area)))
                return;

        if (area.isEmpty())
            return;

        if (flags.hasHeavyweightPeerFlag)
        {
            if (auto* peer = getPeer())
            {
                // Tweak the scaling so that the component's integer size exactly aligns with the peer's scaled size
                auto peerBounds = peer->getBounds();
                auto scaled = area * Point<float> ((float) peerBounds.getWidth()  / (float) getWidth(),
                                                   (float) peerBounds.getHeight() / (float) getHeight());

                peer->repaint (affineTransform != nullptr ? scaled.transformedBy (*affineTransform) : scaled);
            }
        }
        else
        {
            if (parentComponent != nullptr)
                parentComponent->internalRepaint (ComponentHelpers::convertToParentSpace (*this, area));
        }
    }
}

//==============================================================================
void Component::paint (Graphics&)
{
    // if your component is marked as opaque, you must implement a paint
    // method and ensure that its entire area is completely painted.
    jassert (getBounds().isEmpty() || ! isOpaque());
}

void Component::paintOverChildren (Graphics&)
{
    // all painting is done in the subclasses
}

//==============================================================================
void Component::paintWithinParentContext (Graphics& g)
{
    g.setOrigin (getPosition());

    if (cachedImage != nullptr)
        cachedImage->paint (g);
    else
        paintEntireComponent (g, false);
}

void Component::paintComponentAndChildren (Graphics& g)
{
    auto clipBounds = g.getClipBounds();

    if (flags.dontClipGraphicsFlag)
    {
        paint (g);
    }
    else
    {
        Graphics::ScopedSaveState ss (g);

        if (! (ComponentHelpers::clipObscuredRegions (*this, g, clipBounds, {}) && g.isClipEmpty()))
            paint (g);
    }

    for (int i = 0; i < childComponentList.size(); ++i)
    {
        auto& child = *childComponentList.getUnchecked (i);

        if (child.isVisible())
        {
            if (child.affineTransform != nullptr)
            {
                Graphics::ScopedSaveState ss (g);

                g.addTransform (*child.affineTransform);

                if ((child.flags.dontClipGraphicsFlag && ! g.isClipEmpty()) || g.reduceClipRegion (child.getBounds()))
                    child.paintWithinParentContext (g);
            }
            else if (clipBounds.intersects (child.getBounds()))
            {
                Graphics::ScopedSaveState ss (g);

                if (child.flags.dontClipGraphicsFlag)
                {
                    child.paintWithinParentContext (g);
                }
                else if (g.reduceClipRegion (child.getBounds()))
                {
                    bool nothingClipped = true;

                    for (int j = i + 1; j < childComponentList.size(); ++j)
                    {
                        auto& sibling = *childComponentList.getUnchecked (j);

                        if (sibling.flags.opaqueFlag && sibling.isVisible() && sibling.affineTransform == nullptr)
                        {
                            nothingClipped = false;
                            g.excludeClipRegion (sibling.getBounds());
                        }
                    }

                    if (nothingClipped || ! g.isClipEmpty())
                        child.paintWithinParentContext (g);
                }
            }
        }
    }

    Graphics::ScopedSaveState ss (g);
    paintOverChildren (g);
}

void Component::paintEntireComponent (Graphics& g, bool ignoreAlphaLevel)
{
    // If sizing a top-level-window and the OS paint message is delivered synchronously
    // before resized() is called, then we'll invoke the callback here, to make sure
    // the components inside have had a chance to sort their sizes out..
   #if JUCE_DEBUG
    if (! flags.isInsidePaintCall) // (avoids an assertion in plugins hosted in WaveLab)
   #endif
        sendMovedResizedMessagesIfPending();

   #if JUCE_DEBUG
    flags.isInsidePaintCall = true;
   #endif

    if (effect != nullptr)
    {
        auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();

        auto scaledBounds = getLocalBounds() * scale;

        Image effectImage (flags.opaqueFlag ? Image::RGB : Image::ARGB,
                           scaledBounds.getWidth(), scaledBounds.getHeight(), ! flags.opaqueFlag);
        {
            Graphics g2 (effectImage);
            g2.addTransform (AffineTransform::scale ((float) scaledBounds.getWidth()  / (float) getWidth(),
                                                     (float) scaledBounds.getHeight() / (float) getHeight()));
            paintComponentAndChildren (g2);
        }

        Graphics::ScopedSaveState ss (g);

        g.addTransform (AffineTransform::scale (1.0f / scale));
        effect->applyEffect (effectImage, g, scale, ignoreAlphaLevel ? 1.0f : getAlpha());
    }
    else if (componentTransparency > 0 && ! ignoreAlphaLevel)
    {
        if (componentTransparency < 255)
        {
            g.beginTransparencyLayer (getAlpha());
            paintComponentAndChildren (g);
            g.endTransparencyLayer();
        }
    }
    else
    {
        paintComponentAndChildren (g);
    }

   #if JUCE_DEBUG
    flags.isInsidePaintCall = false;
   #endif
}

void Component::setPaintingIsUnclipped (bool shouldPaintWithoutClipping) noexcept
{
    flags.dontClipGraphicsFlag = shouldPaintWithoutClipping;
}

bool Component::isPaintingUnclipped() const noexcept
{
    return flags.dontClipGraphicsFlag;
}

//==============================================================================
Image Component::createComponentSnapshot (Rectangle<int> areaToGrab,
                                          bool clipImageToComponentBounds, float scaleFactor)
{
    auto r = areaToGrab;

    if (clipImageToComponentBounds)
        r = r.getIntersection (getLocalBounds());

    if (r.isEmpty())
        return {};

    auto w = roundToInt (scaleFactor * (float) r.getWidth());
    auto h = roundToInt (scaleFactor * (float) r.getHeight());

    Image image (flags.opaqueFlag ? Image::RGB : Image::ARGB, w, h, true);

    Graphics g (image);

    if (w != getWidth() || h != getHeight())
        g.addTransform (AffineTransform::scale ((float) w / (float) r.getWidth(),
                                                (float) h / (float) r.getHeight()));
    g.setOrigin (-r.getPosition());

    paintEntireComponent (g, true);

    return image;
}

void Component::setComponentEffect (ImageEffectFilter* newEffect)
{
    if (effect != newEffect)
    {
        effect = newEffect;
        repaint();
    }
}

//==============================================================================
LookAndFeel& Component::getLookAndFeel() const noexcept
{
    for (auto* c = this; c != nullptr; c = c->parentComponent)
        if (auto lf = c->lookAndFeel.get())
            return *lf;

    return LookAndFeel::getDefaultLookAndFeel();
}

void Component::setLookAndFeel (LookAndFeel* newLookAndFeel)
{
    if (lookAndFeel != newLookAndFeel)
    {
        lookAndFeel = newLookAndFeel;
        sendLookAndFeelChange();
    }
}

void Component::lookAndFeelChanged() {}
void Component::colourChanged() {}

void Component::sendLookAndFeelChange()
{
    const WeakReference<Component> safePointer (this);
    repaint();
    lookAndFeelChanged();

    if (safePointer != nullptr)
    {
        colourChanged();

        if (safePointer != nullptr)
        {
            for (int i = childComponentList.size(); --i >= 0;)
            {
                childComponentList.getUnchecked (i)->sendLookAndFeelChange();

                if (safePointer == nullptr)
                    return;

                i = jmin (i, childComponentList.size());
            }
        }
    }
}

Colour Component::findColour (int colourID, bool inheritFromParent) const
{
    if (auto* v = properties.getVarPointer (ComponentHelpers::getColourPropertyID (colourID)))
        return Colour ((uint32) static_cast<int> (*v));

    if (inheritFromParent && parentComponent != nullptr
         && (lookAndFeel == nullptr || ! lookAndFeel->isColourSpecified (colourID)))
        return parentComponent->findColour (colourID, true);

    return getLookAndFeel().findColour (colourID);
}

bool Component::isColourSpecified (int colourID) const
{
    return properties.contains (ComponentHelpers::getColourPropertyID (colourID));
}

void Component::removeColour (int colourID)
{
    if (properties.remove (ComponentHelpers::getColourPropertyID (colourID)))
        colourChanged();
}

void Component::setColour (int colourID, Colour colour)
{
    if (properties.set (ComponentHelpers::getColourPropertyID (colourID), (int) colour.getARGB()))
        colourChanged();
}

void Component::copyAllExplicitColoursTo (Component& target) const
{
    bool changed = false;

    for (int i = properties.size(); --i >= 0;)
    {
        auto name = properties.getName(i);

        if (name.toString().startsWith (colourPropertyPrefix))
            if (target.properties.set (name, properties [name]))
                changed = true;
    }

    if (changed)
        target.colourChanged();
}

//==============================================================================
Component::Positioner::Positioner (Component& c) noexcept  : component (c)
{
}

Component::Positioner* Component::getPositioner() const noexcept
{
    return positioner.get();
}

void Component::setPositioner (Positioner* newPositioner)
{
    // You can only assign a positioner to the component that it was created for!
    jassert (newPositioner == nullptr || this == &(newPositioner->getComponent()));
    positioner.reset (newPositioner);
}

//==============================================================================
Rectangle<int> Component::getLocalBounds() const noexcept
{
    return boundsRelativeToParent.withZeroOrigin();
}

Rectangle<int> Component::getBoundsInParent() const noexcept
{
    return affineTransform == nullptr ? boundsRelativeToParent
                                      : boundsRelativeToParent.transformedBy (*affineTransform);
}

//==============================================================================
void Component::mouseEnter (const MouseEvent&)          {}
void Component::mouseExit  (const MouseEvent&)          {}
void Component::mouseDown  (const MouseEvent&)          {}
void Component::mouseUp    (const MouseEvent&)          {}
void Component::mouseDrag  (const MouseEvent&)          {}
void Component::mouseMove  (const MouseEvent&)          {}
void Component::mouseDoubleClick (const MouseEvent&)    {}

void Component::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    // the base class just passes this event up to its parent..
    if (parentComponent != nullptr)
        parentComponent->mouseWheelMove (e.getEventRelativeTo (parentComponent), wheel);
}

void Component::mouseMagnify (const MouseEvent& e, float magnifyAmount)
{
    // the base class just passes this event up to its parent..
    if (parentComponent != nullptr)
        parentComponent->mouseMagnify (e.getEventRelativeTo (parentComponent), magnifyAmount);
}

//==============================================================================
void Component::resized()                       {}
void Component::moved()                         {}
void Component::childBoundsChanged (Component*) {}
void Component::parentSizeChanged()             {}

void Component::addComponentListener (ComponentListener* newListener)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
   #if JUCE_DEBUG || JUCE_LOG_ASSERTIONS
    if (getParentComponent() != nullptr)
    {
        JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED
    }
   #endif

    componentListeners.add (newListener);
}

void Component::removeComponentListener (ComponentListener* listenerToRemove)
{
    componentListeners.remove (listenerToRemove);
}

//==============================================================================
void Component::inputAttemptWhenModal()
{
    ModalComponentManager::getInstance()->bringModalComponentsToFront();
    getLookAndFeel().playAlertSound();
}

bool Component::canModalEventBeSentToComponent (const Component*)
{
    return false;
}

void Component::internalModalInputAttempt()
{
    if (auto* current = getCurrentlyModalComponent())
        current->inputAttemptWhenModal();
}

//==============================================================================
void Component::postCommandMessage (int commandID)
{
    WeakReference<Component> target (this);

    MessageManager::callAsync ([=]
    {
        if (auto* c = target.get())
            c->handleCommandMessage (commandID);
    });
}

void Component::handleCommandMessage (int)
{
    // used by subclasses
}

//==============================================================================
void Component::addMouseListener (MouseListener* newListener,
                                  bool wantsEventsForAllNestedChildComponents)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED

    // If you register a component as a mouselistener for itself, it'll receive all the events
    // twice - once via the direct callback that all components get anyway, and then again as a listener!
    jassert ((newListener != this) || wantsEventsForAllNestedChildComponents);

    if (mouseListeners == nullptr)
        mouseListeners.reset (new MouseListenerList());

    mouseListeners->addListener (newListener, wantsEventsForAllNestedChildComponents);
}

void Component::removeMouseListener (MouseListener* listenerToRemove)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED

    if (mouseListeners != nullptr)
        mouseListeners->removeListener (listenerToRemove);
}

//==============================================================================
void Component::internalMouseEnter (MouseInputSource source, Point<float> relativePos, Time time)
{
    if (isCurrentlyBlockedByAnotherModalComponent())
    {
        // if something else is modal, always just show a normal mouse cursor
        source.showMouseCursor (MouseCursor::NormalCursor);
        return;
    }

    if (flags.repaintOnMouseActivityFlag)
        repaint();

    BailOutChecker checker (this);

    const MouseEvent me (source, relativePos, source.getCurrentModifiers(), MouseInputSource::invalidPressure,
                         MouseInputSource::invalidOrientation, MouseInputSource::invalidRotation,
                         MouseInputSource::invalidTiltX, MouseInputSource::invalidTiltY,
                         this, this, time, relativePos, time, 0, false);
    mouseEnter (me);

    if (checker.shouldBailOut())
        return;

    Desktop::getInstance().getMouseListeners().callChecked (checker, [&] (MouseListener& l) { l.mouseEnter (me); });

    MouseListenerList::template sendMouseEvent<const MouseEvent&> (*this, checker, &MouseListener::mouseEnter, me);
}

void Component::internalMouseExit (MouseInputSource source, Point<float> relativePos, Time time)
{
    if (isCurrentlyBlockedByAnotherModalComponent())
    {
        // if something else is modal, always just show a normal mouse cursor
        source.showMouseCursor (MouseCursor::NormalCursor);
        return;
    }

    if (flags.repaintOnMouseActivityFlag)
        repaint();

    BailOutChecker checker (this);

    const MouseEvent me (source, relativePos, source.getCurrentModifiers(), MouseInputSource::invalidPressure,
                         MouseInputSource::invalidOrientation, MouseInputSource::invalidRotation,
                         MouseInputSource::invalidTiltX, MouseInputSource::invalidTiltY,
                         this, this, time, relativePos, time, 0, false);

    mouseExit (me);

    if (checker.shouldBailOut())
        return;

    Desktop::getInstance().getMouseListeners().callChecked (checker, [&] (MouseListener& l) { l.mouseExit (me); });

    MouseListenerList::template sendMouseEvent<const MouseEvent&> (*this, checker, &MouseListener::mouseExit, me);
}

void Component::internalMouseDown (MouseInputSource source, Point<float> relativePos, Time time,
                                   float pressure, float orientation, float rotation, float tiltX, float tiltY)
{
    auto& desktop = Desktop::getInstance();
    BailOutChecker checker (this);

    if (isCurrentlyBlockedByAnotherModalComponent())
    {
        flags.mouseDownWasBlocked = true;
        internalModalInputAttempt();

        if (checker.shouldBailOut())
            return;

        // If processing the input attempt has exited the modal loop, we'll allow the event
        // to be delivered..
        if (isCurrentlyBlockedByAnotherModalComponent())
        {
            // allow blocked mouse-events to go to global listeners..
            const MouseEvent me (source, relativePos, source.getCurrentModifiers(), pressure,
                                 orientation, rotation, tiltX, tiltY, this, this, time, relativePos,
                                 time, source.getNumberOfMultipleClicks(), false);

            desktop.getMouseListeners().callChecked (checker, [&] (MouseListener& l) { l.mouseDown (me); });
            return;
        }
    }

    flags.mouseDownWasBlocked = false;

    for (auto* c = this; c != nullptr; c = c->parentComponent)
    {
        if (c->isBroughtToFrontOnMouseClick())
        {
            c->toFront (true);

            if (checker.shouldBailOut())
                return;
        }
    }

    if (! flags.dontFocusOnMouseClickFlag)
    {
        grabFocusInternal (focusChangedByMouseClick, true);

        if (checker.shouldBailOut())
            return;
    }

    if (flags.repaintOnMouseActivityFlag)
        repaint();

    const MouseEvent me (source, relativePos, source.getCurrentModifiers(), pressure,
                         orientation, rotation, tiltX, tiltY, this, this, time, relativePos,
                         time, source.getNumberOfMultipleClicks(), false);
    mouseDown (me);

    if (checker.shouldBailOut())
        return;

    desktop.getMouseListeners().callChecked (checker, [&] (MouseListener& l) { l.mouseDown (me); });

    MouseListenerList::template sendMouseEvent<const MouseEvent&> (*this, checker, &MouseListener::mouseDown, me);
}

void Component::internalMouseUp (MouseInputSource source, Point<float> relativePos, Time time,
                                 const ModifierKeys oldModifiers, float pressure, float orientation, float rotation, float tiltX, float tiltY)
{
    if (flags.mouseDownWasBlocked && isCurrentlyBlockedByAnotherModalComponent())
        return;

    BailOutChecker checker (this);

    if (flags.repaintOnMouseActivityFlag)
        repaint();

    const MouseEvent me (source, relativePos, oldModifiers, pressure, orientation,
                         rotation, tiltX, tiltY, this, this, time,
                         getLocalPoint (nullptr, source.getLastMouseDownPosition()),
                         source.getLastMouseDownTime(),
                         source.getNumberOfMultipleClicks(),
                         source.isLongPressOrDrag());
    mouseUp (me);

    if (checker.shouldBailOut())
        return;

    auto& desktop = Desktop::getInstance();
    desktop.getMouseListeners().callChecked (checker, [&] (MouseListener& l) { l.mouseUp (me); });

    MouseListenerList::template sendMouseEvent<const MouseEvent&> (*this, checker, &MouseListener::mouseUp, me);

    if (checker.shouldBailOut())
        return;

    // check for double-click
    if (me.getNumberOfClicks() >= 2)
    {
        mouseDoubleClick (me);

        if (checker.shouldBailOut())
            return;

        desktop.mouseListeners.callChecked (checker, [&] (MouseListener& l) { l.mouseDoubleClick (me); });
        MouseListenerList::template sendMouseEvent<const MouseEvent&> (*this, checker, &MouseListener::mouseDoubleClick, me);
    }
}

void Component::internalMouseDrag (MouseInputSource source, Point<float> relativePos, Time time,
                                   float pressure, float orientation, float rotation, float tiltX, float tiltY)
{
    if (! isCurrentlyBlockedByAnotherModalComponent())
    {
        BailOutChecker checker (this);

        const MouseEvent me (source, relativePos, source.getCurrentModifiers(),
                             pressure, orientation, rotation, tiltX, tiltY, this, this, time,
                             getLocalPoint (nullptr, source.getLastMouseDownPosition()),
                             source.getLastMouseDownTime(),
                             source.getNumberOfMultipleClicks(),
                             source.isLongPressOrDrag());
        mouseDrag (me);

        if (checker.shouldBailOut())
            return;

        Desktop::getInstance().getMouseListeners().callChecked (checker, [&] (MouseListener& l) { l.mouseDrag (me); });

        MouseListenerList::template sendMouseEvent<const MouseEvent&> (*this, checker, &MouseListener::mouseDrag, me);
    }
}

void Component::internalMouseMove (MouseInputSource source, Point<float> relativePos, Time time)
{
    auto& desktop = Desktop::getInstance();

    if (isCurrentlyBlockedByAnotherModalComponent())
    {
        // allow blocked mouse-events to go to global listeners..
        desktop.sendMouseMove();
    }
    else
    {
        BailOutChecker checker (this);

        const MouseEvent me (source, relativePos, source.getCurrentModifiers(), MouseInputSource::invalidPressure,
                             MouseInputSource::invalidOrientation, MouseInputSource::invalidRotation,
                             MouseInputSource::invalidTiltX, MouseInputSource::invalidTiltY,
                             this, this, time, relativePos, time, 0, false);
        mouseMove (me);

        if (checker.shouldBailOut())
            return;

        desktop.getMouseListeners().callChecked (checker, [&] (MouseListener& l) { l.mouseMove (me); });

        MouseListenerList::template sendMouseEvent<const MouseEvent&> (*this, checker, &MouseListener::mouseMove, me);
    }
}

void Component::internalMouseWheel (MouseInputSource source, Point<float> relativePos,
                                    Time time, const MouseWheelDetails& wheel)
{
    auto& desktop = Desktop::getInstance();
    BailOutChecker checker (this);

    const MouseEvent me (source, relativePos, source.getCurrentModifiers(), MouseInputSource::invalidPressure,
                         MouseInputSource::invalidOrientation, MouseInputSource::invalidRotation,
                         MouseInputSource::invalidTiltX, MouseInputSource::invalidTiltY,
                         this, this, time, relativePos, time, 0, false);

    if (isCurrentlyBlockedByAnotherModalComponent())
    {
        // allow blocked mouse-events to go to global listeners..
        desktop.mouseListeners.callChecked (checker, [&] (MouseListener& l) { l.mouseWheelMove (me, wheel); });
    }
    else
    {
        mouseWheelMove (me, wheel);

        if (checker.shouldBailOut())
            return;

        desktop.mouseListeners.callChecked (checker, [&] (MouseListener& l) { l.mouseWheelMove (me, wheel); });

        if (! checker.shouldBailOut())
            MouseListenerList::template sendMouseEvent<const MouseEvent&, const MouseWheelDetails&> (*this, checker, &MouseListener::mouseWheelMove, me, wheel);
    }
}

void Component::internalMagnifyGesture (MouseInputSource source, Point<float> relativePos,
                                        Time time, float amount)
{
    auto& desktop = Desktop::getInstance();
    BailOutChecker checker (this);

    const MouseEvent me (source, relativePos, source.getCurrentModifiers(), MouseInputSource::invalidPressure,
                         MouseInputSource::invalidOrientation, MouseInputSource::invalidRotation,
                         MouseInputSource::invalidTiltX, MouseInputSource::invalidTiltY,
                         this, this, time, relativePos, time, 0, false);

    if (isCurrentlyBlockedByAnotherModalComponent())
    {
        // allow blocked mouse-events to go to global listeners..
        desktop.mouseListeners.callChecked (checker, [&] (MouseListener& l) { l.mouseMagnify (me, amount); });
    }
    else
    {
        mouseMagnify (me, amount);

        if (checker.shouldBailOut())
            return;

        desktop.mouseListeners.callChecked (checker, [&] (MouseListener& l) { l.mouseMagnify (me, amount); });

        if (! checker.shouldBailOut())
            MouseListenerList::template sendMouseEvent<const MouseEvent&, float> (*this, checker, &MouseListener::mouseMagnify, me, amount);
    }
}

void Component::sendFakeMouseMove() const
{
    if (flags.ignoresMouseClicksFlag && ! flags.allowChildMouseClicksFlag)
        return;

    auto mainMouse = Desktop::getInstance().getMainMouseSource();

    if (! mainMouse.isDragging())
        mainMouse.triggerFakeMove();
}

void JUCE_CALLTYPE Component::beginDragAutoRepeat (int interval)
{
    Desktop::getInstance().beginDragAutoRepeat (interval);
}

//==============================================================================
void Component::broughtToFront()
{
}

void Component::internalBroughtToFront()
{
    if (flags.hasHeavyweightPeerFlag)
        Desktop::getInstance().componentBroughtToFront (this);

    BailOutChecker checker (this);
    broughtToFront();

    if (checker.shouldBailOut())
        return;

    componentListeners.callChecked (checker, [this] (ComponentListener& l) { l.componentBroughtToFront (*this); });

    if (checker.shouldBailOut())
        return;

    // When brought to the front and there's a modal component blocking this one,
    // we need to bring the modal one to the front instead..
    if (auto* cm = getCurrentlyModalComponent())
        if (cm->getTopLevelComponent() != getTopLevelComponent())
            ModalComponentManager::getInstance()->bringModalComponentsToFront (false); // very important that this is false, otherwise in Windows,
                                                                                       // non-front components can't get focus when another modal comp is
                                                                                       // active, and therefore can't receive mouse-clicks
}

//==============================================================================
void Component::focusGained (FocusChangeType)   {}
void Component::focusLost (FocusChangeType)     {}
void Component::focusOfChildComponentChanged (FocusChangeType) {}

void Component::internalFocusGain (FocusChangeType cause)
{
    internalFocusGain (cause, WeakReference<Component> (this));
}

void Component::internalFocusGain (FocusChangeType cause, const WeakReference<Component>& safePointer)
{
    focusGained (cause);

    if (safePointer != nullptr)
        internalChildFocusChange (cause, safePointer);
}

void Component::internalFocusLoss (FocusChangeType cause)
{
    const WeakReference<Component> safePointer (this);

    focusLost (cause);

    if (safePointer != nullptr)
        internalChildFocusChange (cause, safePointer);
}

void Component::internalChildFocusChange (FocusChangeType cause, const WeakReference<Component>& safePointer)
{
    const bool childIsNowFocused = hasKeyboardFocus (true);

    if (flags.childCompFocusedFlag != childIsNowFocused)
    {
        flags.childCompFocusedFlag = childIsNowFocused;

        focusOfChildComponentChanged (cause);

        if (safePointer == nullptr)
            return;
    }

    if (parentComponent != nullptr)
        parentComponent->internalChildFocusChange (cause, WeakReference<Component> (parentComponent));
}

void Component::setWantsKeyboardFocus (bool wantsFocus) noexcept
{
    flags.wantsFocusFlag = wantsFocus;
}

void Component::setMouseClickGrabsKeyboardFocus (bool shouldGrabFocus)
{
    flags.dontFocusOnMouseClickFlag = ! shouldGrabFocus;
}

bool Component::getMouseClickGrabsKeyboardFocus() const noexcept
{
    return ! flags.dontFocusOnMouseClickFlag;
}

bool Component::getWantsKeyboardFocus() const noexcept
{
    return flags.wantsFocusFlag && ! flags.isDisabledFlag;
}

void Component::setFocusContainer (bool shouldBeFocusContainer) noexcept
{
    flags.isFocusContainerFlag = shouldBeFocusContainer;
}

bool Component::isFocusContainer() const noexcept
{
    return flags.isFocusContainerFlag;
}

static const Identifier juce_explicitFocusOrderId ("_jexfo");

int Component::getExplicitFocusOrder() const
{
    return properties [juce_explicitFocusOrderId];
}

void Component::setExplicitFocusOrder (int newFocusOrderIndex)
{
    properties.set (juce_explicitFocusOrderId, newFocusOrderIndex);
}

KeyboardFocusTraverser* Component::createFocusTraverser()
{
    if (flags.isFocusContainerFlag || parentComponent == nullptr)
        return new KeyboardFocusTraverser();

    return parentComponent->createFocusTraverser();
}

void Component::takeKeyboardFocus (FocusChangeType cause)
{
    // give the focus to this component
    if (currentlyFocusedComponent != this)
    {
        // get the focus onto our desktop window
        if (auto* peer = getPeer())
        {
            const WeakReference<Component> safePointer (this);
            peer->grabFocus();

            if (peer->isFocused() && currentlyFocusedComponent != this)
            {
                WeakReference<Component> componentLosingFocus (currentlyFocusedComponent);
                currentlyFocusedComponent = this;

                Desktop::getInstance().triggerFocusCallback();

                // call this after setting currentlyFocusedComponent so that the one that's
                // losing it has a chance to see where focus is going
                if (componentLosingFocus != nullptr)
                    componentLosingFocus->internalFocusLoss (cause);

                if (currentlyFocusedComponent == this)
                    internalFocusGain (cause, safePointer);
            }
        }
    }
}

void Component::grabFocusInternal (FocusChangeType cause, bool canTryParent)
{
    if (isShowing())
    {
        if (flags.wantsFocusFlag && (isEnabled() || parentComponent == nullptr))
        {
            takeKeyboardFocus (cause);
        }
        else
        {
            if (isParentOf (currentlyFocusedComponent)
                 && currentlyFocusedComponent->isShowing())
            {
                // do nothing if the focused component is actually a child of ours..
            }
            else
            {
                // find the default child component..
                std::unique_ptr<KeyboardFocusTraverser> traverser (createFocusTraverser());

                if (traverser != nullptr)
                {
                    auto* defaultComp = traverser->getDefaultComponent (this);
                    traverser.reset();

                    if (defaultComp != nullptr)
                    {
                        defaultComp->grabFocusInternal (cause, false);
                        return;
                    }
                }

                if (canTryParent && parentComponent != nullptr)
                {
                    // if no children want it and we're allowed to try our parent comp,
                    // then pass up to parent, which will try our siblings.
                    parentComponent->grabFocusInternal (cause, true);
                }
            }
        }
    }
}

void Component::grabKeyboardFocus()
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED

    grabFocusInternal (focusChangedDirectly, true);

    // A component can only be focused when it's actually on the screen!
    // If this fails then you're probably trying to grab the focus before you've
    // added the component to a parent or made it visible. Or maybe one of its parent
    // components isn't yet visible.
    jassert (isShowing() || isOnDesktop());
}

void Component::moveKeyboardFocusToSibling (bool moveToNext)
{
    // if component methods are being called from threads other than the message
    // thread, you'll need to use a MessageManagerLock object to make sure it's thread-safe.
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED

    if (parentComponent != nullptr)
    {
        std::unique_ptr<KeyboardFocusTraverser> traverser (createFocusTraverser());

        if (traverser != nullptr)
        {
            auto* nextComp = moveToNext ? traverser->getNextComponent (this)
                                        : traverser->getPreviousComponent (this);
            traverser.reset();

            if (nextComp != nullptr)
            {
                if (nextComp->isCurrentlyBlockedByAnotherModalComponent())
                {
                    const WeakReference<Component> nextCompPointer (nextComp);
                    internalModalInputAttempt();

                    if (nextCompPointer == nullptr || nextComp->isCurrentlyBlockedByAnotherModalComponent())
                        return;
                }

                nextComp->grabFocusInternal (focusChangedByTabKey, true);
                return;
            }
        }

        parentComponent->moveKeyboardFocusToSibling (moveToNext);
    }
}

bool Component::hasKeyboardFocus (bool trueIfChildIsFocused) const
{
    return (currentlyFocusedComponent == this)
            || (trueIfChildIsFocused && isParentOf (currentlyFocusedComponent));
}

Component* JUCE_CALLTYPE Component::getCurrentlyFocusedComponent() noexcept
{
    return currentlyFocusedComponent;
}

void JUCE_CALLTYPE Component::unfocusAllComponents()
{
    if (auto* c = getCurrentlyFocusedComponent())
        c->giveAwayFocus (true);
}

void Component::giveAwayFocus (bool sendFocusLossEvent)
{
    auto* componentLosingFocus = currentlyFocusedComponent;
    currentlyFocusedComponent = nullptr;

    if (sendFocusLossEvent && componentLosingFocus != nullptr)
        componentLosingFocus->internalFocusLoss (focusChangedDirectly);

    Desktop::getInstance().triggerFocusCallback();
}

//==============================================================================
bool Component::isEnabled() const noexcept
{
    return (! flags.isDisabledFlag)
            && (parentComponent == nullptr || parentComponent->isEnabled());
}

void Component::setEnabled (bool shouldBeEnabled)
{
    if (flags.isDisabledFlag == shouldBeEnabled)
    {
        flags.isDisabledFlag = ! shouldBeEnabled;

        // if any parent components are disabled, setting our flag won't make a difference,
        // so no need to send a change message
        if (parentComponent == nullptr || parentComponent->isEnabled())
            sendEnablementChangeMessage();

        BailOutChecker checker (this);
        componentListeners.callChecked (checker, [this] (ComponentListener& l) { l.componentEnablementChanged (*this); });
    }
}

void Component::enablementChanged() {}

void Component::sendEnablementChangeMessage()
{
    const WeakReference<Component> safePointer (this);

    enablementChanged();

    if (safePointer == nullptr)
        return;

    for (int i = getNumChildComponents(); --i >= 0;)
    {
        if (auto* c = getChildComponent (i))
        {
            c->sendEnablementChangeMessage();

            if (safePointer == nullptr)
                return;
        }
    }
}

//==============================================================================
bool Component::isMouseOver (bool includeChildren) const
{
    for (auto& ms : Desktop::getInstance().getMouseSources())
    {
        auto* c = ms.getComponentUnderMouse();

        if (c == this || (includeChildren && isParentOf (c)))
            if (ms.isDragging() || ! (ms.isTouch() || ms.isPen()))
                if (c->reallyContains (c->getLocalPoint (nullptr, ms.getScreenPosition()).roundToInt(), false))
                    return true;
    }

    return false;
}

bool Component::isMouseButtonDown (bool includeChildren) const
{
    for (auto& ms : Desktop::getInstance().getMouseSources())
    {
        auto* c = ms.getComponentUnderMouse();

        if (c == this || (includeChildren && isParentOf (c)))
            if (ms.isDragging())
                return true;
    }

    return false;
}

bool Component::isMouseOverOrDragging (bool includeChildren) const
{
    for (auto& ms : Desktop::getInstance().getMouseSources())
    {
        auto* c = ms.getComponentUnderMouse();

        if (c == this || (includeChildren && isParentOf (c)))
            if (ms.isDragging() || ! ms.isTouch())
                return true;
    }

    return false;
}

bool JUCE_CALLTYPE Component::isMouseButtonDownAnywhere() noexcept
{
    return ModifierKeys::currentModifiers.isAnyMouseButtonDown();
}

Point<int> Component::getMouseXYRelative() const
{
    return getLocalPoint (nullptr, Desktop::getMousePosition());
}

//==============================================================================
void Component::addKeyListener (KeyListener* newListener)
{
    if (keyListeners == nullptr)
        keyListeners.reset (new Array<KeyListener*>());

    keyListeners->addIfNotAlreadyThere (newListener);
}

void Component::removeKeyListener (KeyListener* listenerToRemove)
{
    if (keyListeners != nullptr)
        keyListeners->removeFirstMatchingValue (listenerToRemove);
}

bool Component::keyPressed (const KeyPress&)            { return false; }
bool Component::keyStateChanged (bool /*isKeyDown*/)    { return false; }

void Component::modifierKeysChanged (const ModifierKeys& modifiers)
{
    if (parentComponent != nullptr)
        parentComponent->modifierKeysChanged (modifiers);
}

void Component::internalModifierKeysChanged()
{
    sendFakeMouseMove();
    modifierKeysChanged (ModifierKeys::currentModifiers);
}

//==============================================================================
Component::BailOutChecker::BailOutChecker (Component* component)
    : safePointer (component)
{
    jassert (component != nullptr);
}

bool Component::BailOutChecker::shouldBailOut() const noexcept
{
    return safePointer == nullptr;
}

} // namespace juce
