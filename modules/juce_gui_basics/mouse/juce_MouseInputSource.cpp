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

class MouseInputSourceInternal   : private AsyncUpdater
{
public:
    MouseInputSourceInternal (int i, MouseInputSource::InputSourceType type)  : index (i), inputType (type)
    {
    }

    //==============================================================================
    bool isDragging() const noexcept
    {
        return buttonState.isAnyMouseButtonDown();
    }

    Component* getComponentUnderMouse() const noexcept
    {
        return componentUnderMouse.get();
    }

    ModifierKeys getCurrentModifiers() const noexcept
    {
        return ModifierKeys::currentModifiers.withoutMouseButtons().withFlags (buttonState.getRawFlags());
    }

    ComponentPeer* getPeer() noexcept
    {
        if (! ComponentPeer::isValidPeer (lastPeer))
            lastPeer = nullptr;

        return lastPeer;
    }

    static Point<float> screenPosToLocalPos (Component& comp, Point<float> pos)
    {
        if (auto* peer = comp.getPeer())
        {
            pos = peer->globalToLocal (pos);
            auto& peerComp = peer->getComponent();
            return comp.getLocalPoint (&peerComp, ScalingHelpers::unscaledScreenPosToScaled (peerComp, pos));
        }

        return comp.getLocalPoint (nullptr, ScalingHelpers::unscaledScreenPosToScaled (comp, pos));
    }

    Component* findComponentAt (Point<float> screenPos)
    {
        if (auto* peer = getPeer())
        {
            auto relativePos = ScalingHelpers::unscaledScreenPosToScaled (peer->getComponent(),
                                                                          peer->globalToLocal (screenPos));
            auto& comp = peer->getComponent();

            // (the contains() call is needed to test for overlapping desktop windows)
            if (comp.containsInternal (relativePos))
                return comp.getComponentAtInternal (relativePos);
        }

        return nullptr;
    }

    Point<float> getScreenPosition() const noexcept
    {
        // This needs to return the live position if possible, but it mustn't update the lastScreenPos
        // value, because that can cause continuity problems.
        return ScalingHelpers::unscaledScreenPosToScaled (getRawScreenPosition());
    }

    Point<float> getRawScreenPosition() const noexcept
    {
        return unboundedMouseOffset + (inputType != MouseInputSource::InputSourceType::touch ? MouseInputSource::getCurrentRawMousePosition()
                                                                                             : lastScreenPos);
    }

    void setScreenPosition (Point<float> p)
    {
        MouseInputSource::setRawMousePosition (ScalingHelpers::scaledScreenPosToUnscaled (p));
    }

    bool isPressureValid()    const noexcept     { return pressure >= 0.0f && pressure <= 1.0f; }
    bool isOrientationValid() const noexcept     { return orientation >= 0.0f && orientation <= MathConstants<float>::twoPi; }
    bool isRotationValid() const noexcept        { return rotation >= 0.0f && rotation <= MathConstants<float>::twoPi; }
    bool isTiltValid (bool isX) const noexcept   { return isX ? (tiltX >= -1.0f && tiltX <= 1.0f) : (tiltY >= -1.0f && tiltY <= 1.0f); }

    //==============================================================================
   #if JUCE_DUMP_MOUSE_EVENTS
    #define JUCE_MOUSE_EVENT_DBG(desc)   DBG ("Mouse " << desc << " #" << index \
                                                << ": " << screenPosToLocalPos (comp, screenPos).toString() \
                                                << " - Comp: " << String::toHexString ((pointer_sized_int) &comp));
   #else
    #define JUCE_MOUSE_EVENT_DBG(desc)
   #endif

    void sendMouseEnter (Component& comp, Point<float> screenPos, Time time)
    {
        JUCE_MOUSE_EVENT_DBG ("enter")
        comp.internalMouseEnter (MouseInputSource (this), screenPosToLocalPos (comp, screenPos), time);
    }

    void sendMouseExit (Component& comp, Point<float> screenPos, Time time)
    {
        JUCE_MOUSE_EVENT_DBG ("exit")
        comp.internalMouseExit (MouseInputSource (this), screenPosToLocalPos (comp, screenPos), time);
    }

    void sendMouseMove (Component& comp, Point<float> screenPos, Time time)
    {
        JUCE_MOUSE_EVENT_DBG ("move")
        comp.internalMouseMove (MouseInputSource (this), screenPosToLocalPos (comp, screenPos), time);
    }

    void sendMouseDown (Component& comp, Point<float> screenPos, Time time)
    {
        JUCE_MOUSE_EVENT_DBG ("down")
        comp.internalMouseDown (MouseInputSource (this), screenPosToLocalPos (comp, screenPos), time, pressure, orientation, rotation, tiltX, tiltY);
    }

    void sendMouseDrag (Component& comp, Point<float> screenPos, Time time)
    {
        JUCE_MOUSE_EVENT_DBG ("drag")
        comp.internalMouseDrag (MouseInputSource (this), screenPosToLocalPos (comp, screenPos), time, pressure, orientation, rotation, tiltX, tiltY);
    }

    void sendMouseUp (Component& comp, Point<float> screenPos, Time time, ModifierKeys oldMods)
    {
        JUCE_MOUSE_EVENT_DBG ("up")
            comp.internalMouseUp (MouseInputSource (this), screenPosToLocalPos (comp, screenPos), time, oldMods, pressure, orientation, rotation, tiltX, tiltY);
    }

    void sendMouseWheel (Component& comp, Point<float> screenPos, Time time, const MouseWheelDetails& wheel)
    {
        JUCE_MOUSE_EVENT_DBG ("wheel")
        comp.internalMouseWheel (MouseInputSource (this), screenPosToLocalPos (comp, screenPos), time, wheel);
    }

    void sendMagnifyGesture (Component& comp, Point<float> screenPos, Time time, float amount)
    {
        JUCE_MOUSE_EVENT_DBG ("magnify")
        comp.internalMagnifyGesture (MouseInputSource (this), screenPosToLocalPos (comp, screenPos), time, amount);
    }

    //==============================================================================
    // (returns true if the button change caused a modal event loop)
    bool setButtons (Point<float> screenPos, Time time, ModifierKeys newButtonState)
    {
        if (buttonState == newButtonState)
            return false;

        // (avoid sending a spurious mouse-drag when we receive a mouse-up)
        if (! (isDragging() && ! newButtonState.isAnyMouseButtonDown()))
            setScreenPos (screenPos, time, false);

        // (ignore secondary clicks when there's already a button down)
        if (buttonState.isAnyMouseButtonDown() == newButtonState.isAnyMouseButtonDown())
        {
            buttonState = newButtonState;
            return false;
        }

        auto lastCounter = mouseEventCounter;

        if (buttonState.isAnyMouseButtonDown())
        {
            if (auto* current = getComponentUnderMouse())
            {
                auto oldMods = getCurrentModifiers();
                buttonState = newButtonState; // must change this before calling sendMouseUp, in case it runs a modal loop

                sendMouseUp (*current, screenPos + unboundedMouseOffset, time, oldMods);

                if (lastCounter != mouseEventCounter)
                    return true; // if a modal loop happened, then newButtonState is no longer valid.
            }

            enableUnboundedMouseMovement (false, false);
        }

        buttonState = newButtonState;

        if (buttonState.isAnyMouseButtonDown())
        {
            Desktop::getInstance().incrementMouseClickCounter();

            if (auto* current = getComponentUnderMouse())
            {
                registerMouseDown (screenPos, time, *current, buttonState,
                                   inputType == MouseInputSource::InputSourceType::touch);
                sendMouseDown (*current, screenPos, time);
            }
        }

        return lastCounter != mouseEventCounter;
    }

    void setComponentUnderMouse (Component* newComponent, Point<float> screenPos, Time time)
    {
        auto* current = getComponentUnderMouse();

        if (newComponent != current)
        {
            WeakReference<Component> safeNewComp (newComponent);
            auto originalButtonState = buttonState;

            if (current != nullptr)
            {
                WeakReference<Component> safeOldComp (current);
                setButtons (screenPos, time, ModifierKeys());

                if (auto oldComp = safeOldComp.get())
                {
                    componentUnderMouse = safeNewComp;
                    sendMouseExit (*oldComp, screenPos, time);
                }

                buttonState = originalButtonState;
            }

            componentUnderMouse = safeNewComp.get();
            current = safeNewComp.get();

            if (current != nullptr)
                sendMouseEnter (*current, screenPos, time);

            revealCursor (false);
            setButtons (screenPos, time, originalButtonState);
        }
    }

    void setPeer (ComponentPeer& newPeer, Point<float> screenPos, Time time)
    {
        if (&newPeer != lastPeer)
        {
            setComponentUnderMouse (nullptr, screenPos, time);
            lastPeer = &newPeer;
            setComponentUnderMouse (findComponentAt (screenPos), screenPos, time);
        }
    }

    void setScreenPos (Point<float> newScreenPos, Time time, bool forceUpdate)
    {
        if (! isDragging())
            setComponentUnderMouse (findComponentAt (newScreenPos), newScreenPos, time);
        else if (lastScreenPos == MouseInputSource::offscreenMousePos) {
          auto componentScreenBounds =
              ScalingHelpers::scaledScreenPosToUnscaled(
                  getComponentUnderMouse()
                      ->getParentMonitorArea()
                      .reduced(2, 2)
                      .toFloat());
            if ( !componentScreenBounds.contains(newScreenPos) ) {
                // this case means handleUnboundedDrag has been fired, but OS didn't updated new newScreenPos
                // ignore drags as long as Mouse pos hasn't been updated by OS (X11
                // is affected...)
                return;
            }
        }

        if (newScreenPos != lastScreenPos || forceUpdate)
        {
            cancelPendingUpdate();

            if (newScreenPos != MouseInputSource::offscreenMousePos)
                lastScreenPos = newScreenPos;

            if (auto* current = getComponentUnderMouse())
            {
                if (isDragging())
                {
                    registerMouseDrag (newScreenPos);
                    sendMouseDrag (*current, newScreenPos + unboundedMouseOffset, time);

                    if (isUnboundedMouseModeOn)
                        handleUnboundedDrag (*current);
                }
                else
                {
                    sendMouseMove (*current, newScreenPos, time);
                }
            }

            revealCursor (false);
        }
    }

    //==============================================================================
    void handleEvent (ComponentPeer& newPeer, Point<float> positionWithinPeer, Time time,
                      const ModifierKeys newMods, float newPressure, float newOrientation, PenDetails pen)
    {
        lastTime = time;

        const bool pressureChanged = (pressure != newPressure);
        pressure = newPressure;

        const bool orientationChanged = (orientation != newOrientation);
        orientation = newOrientation;

        const bool rotationChanged = (rotation != pen.rotation);
        rotation = pen.rotation;

        const bool tiltChanged = (tiltX != pen.tiltX || tiltY != pen.tiltY);
        tiltX = pen.tiltX;
        tiltY = pen.tiltY;

        const bool shouldUpdate = (pressureChanged || orientationChanged || rotationChanged || tiltChanged);

        ++mouseEventCounter;

        auto screenPos = newPeer.localToGlobal (positionWithinPeer);

        if (isDragging() && newMods.isAnyMouseButtonDown())
        {
            setScreenPos (screenPos, time, shouldUpdate);
        }
        else
        {
            setPeer (newPeer, screenPos, time);

            if (auto* peer = getPeer())
            {
                if (setButtons (screenPos, time, newMods))
                    return; // some modal events have been dispatched, so the current event is now out-of-date

                peer = getPeer();

                if (peer != nullptr)
                    setScreenPos (screenPos, time, shouldUpdate);
            }
        }
    }

    Component* getTargetForGesture (ComponentPeer& peer, Point<float> positionWithinPeer,
                                    Time time, Point<float>& screenPos)
    {
        lastTime = time;
        ++mouseEventCounter;

        screenPos = peer.localToGlobal (positionWithinPeer);
        setPeer (peer, screenPos, time);
        setScreenPos (screenPos, time, false);
        triggerFakeMove();

        return getComponentUnderMouse();
    }

    void handleWheel (ComponentPeer& peer, Point<float> positionWithinPeer,
                      Time time, const MouseWheelDetails& wheel)
    {
        Desktop::getInstance().incrementMouseWheelCounter();
        Point<float> screenPos;

        // This will make sure that when the wheel spins in its inertial phase, any events
        // continue to be sent to the last component that the mouse was over when it was being
        // actively controlled by the user. This avoids confusion when scrolling through nested
        // scrollable components.
        if (lastNonInertialWheelTarget == nullptr || ! wheel.isInertial)
            lastNonInertialWheelTarget = getTargetForGesture (peer, positionWithinPeer, time, screenPos);
        else
            screenPos = peer.localToGlobal (positionWithinPeer);

        if (auto target = lastNonInertialWheelTarget.get())
            sendMouseWheel (*target, screenPos, time, wheel);
    }

    void handleMagnifyGesture (ComponentPeer& peer, Point<float> positionWithinPeer,
                               Time time, const float scaleFactor)
    {
        Point<float> screenPos;

        if (auto* current = getTargetForGesture (peer, positionWithinPeer, time, screenPos))
            sendMagnifyGesture (*current, screenPos, time, scaleFactor);
    }

    //==============================================================================
    Time getLastMouseDownTime() const noexcept              { return mouseDowns[0].time; }
    Point<float> getLastMouseDownPosition() const noexcept  { return ScalingHelpers::unscaledScreenPosToScaled (mouseDowns[0].position); }

    int getNumberOfMultipleClicks() const noexcept
    {
        int numClicks = 1;

        if (! isLongPressOrDrag())
        {
            for (int i = 1; i < numElementsInArray (mouseDowns); ++i)
            {
                if (mouseDowns[0].canBePartOfMultipleClickWith (mouseDowns[i], MouseEvent::getDoubleClickTimeout() * jmin (i, 2)))
                    ++numClicks;
                else
                    break;
            }
        }

        return numClicks;
    }

    bool isLongPressOrDrag() const noexcept
    {
        return movedSignificantly || lastTime > mouseDowns[0].time + RelativeTime::milliseconds (300);
    }

    bool hasMovedSignificantlySincePressed() const noexcept
    {
        return movedSignificantly;
    }

    // Deprecated method
    bool hasMouseMovedSignificantlySincePressed() const noexcept
    {
        return isLongPressOrDrag();
    }

    //==============================================================================
    void triggerFakeMove()
    {
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        setScreenPos (lastScreenPos, jmax (lastTime, Time::getCurrentTime()), true);
    }

    //==============================================================================
    void enableUnboundedMouseMovement (bool enable, bool keepCursorVisibleUntilOffscreen)
    {
        enable = enable && isDragging();
        isCursorVisibleUntilOffscreen = keepCursorVisibleUntilOffscreen;

        if (enable != isUnboundedMouseModeOn)
        {
            if ((! enable) && ((! isCursorVisibleUntilOffscreen) || ! unboundedMouseOffset.isOrigin()))
            {
                // when released, return the mouse to within the component's bounds
                if (auto* current = getComponentUnderMouse())
                    setScreenPosition (current->getScreenBounds().toFloat()
                                          .getConstrainedPoint (ScalingHelpers::unscaledScreenPosToScaled (lastScreenPos)));
            }

            isUnboundedMouseModeOn = enable;
            unboundedMouseOffset = {};

            revealCursor (true);
        }
    }

    void handleUnboundedDrag (Component& current)
    {
        
        auto componentScreenBounds = ScalingHelpers::scaledScreenPosToUnscaled (current.getParentMonitorArea().reduced (2, 2).toFloat());

        if (! componentScreenBounds.contains (lastScreenPos))
        {
            if(lastScreenPos == MouseInputSource::offscreenMousePos){
                // OS didn't process last call to setScreenPosition
                jassertfalse;
                return;
            }
            auto componentCentre = current.getScreenBounds().toFloat().getCentre();
            if(!componentScreenBounds.contains(componentCentre)){
                // new position will not be in margins, avoid it
                jassertfalse;
                return;
            }
            unboundedMouseOffset += (lastScreenPos - ScalingHelpers::scaledScreenPosToUnscaled (componentCentre));
            setScreenPosition (componentCentre);
            lastScreenPos = MouseInputSource::offscreenMousePos;
        }
        else if (isCursorVisibleUntilOffscreen
                  && (! unboundedMouseOffset.isOrigin())
                  && componentScreenBounds.contains (lastScreenPos + unboundedMouseOffset))
        {
            MouseInputSource::setRawMousePosition (lastScreenPos + unboundedMouseOffset);
            unboundedMouseOffset = {};
        }
    }

    //==============================================================================
    void showMouseCursor (MouseCursor cursor, bool forcedUpdate)
    {
        if (isUnboundedMouseModeOn && ((! unboundedMouseOffset.isOrigin()) || ! isCursorVisibleUntilOffscreen))
        {
            cursor = MouseCursor::NoCursor;
            forcedUpdate = true;
        }

        if (forcedUpdate || cursor.getHandle() != currentCursorHandle)
        {
            currentCursorHandle = cursor.getHandle();
            cursor.showInWindow (getPeer());
        }
    }

    void hideCursor()
    {
        showMouseCursor (MouseCursor::NoCursor, true);
    }

    void revealCursor (bool forcedUpdate)
    {
        MouseCursor mc (MouseCursor::NormalCursor);

        if (auto* current = getComponentUnderMouse())
            mc = current->getLookAndFeel().getMouseCursorFor (*current);

        showMouseCursor (mc, forcedUpdate);
    }

    //==============================================================================
    const int index;
    const MouseInputSource::InputSourceType inputType;
    Point<float> lastScreenPos, unboundedMouseOffset; // NB: these are unscaled coords
    ModifierKeys buttonState;
    float pressure = 0;
    float orientation = 0;
    float rotation = 0;
    float tiltX = 0;
    float tiltY = 0;

    bool isUnboundedMouseModeOn = false, isCursorVisibleUntilOffscreen = false;

private:
    WeakReference<Component> componentUnderMouse, lastNonInertialWheelTarget;
    ComponentPeer* lastPeer = nullptr;

    void* currentCursorHandle = nullptr;
    int mouseEventCounter = 0;

    struct RecentMouseDown
    {
        RecentMouseDown() = default;

        Point<float> position;
        Time time;
        ModifierKeys buttons;
        uint32 peerID = 0;
        bool isTouch = false;

        bool canBePartOfMultipleClickWith (const RecentMouseDown& other, int maxTimeBetweenMs) const noexcept
        {
            return time - other.time < RelativeTime::milliseconds (maxTimeBetweenMs)
                    && std::abs (position.x - other.position.x) < (float) getPositionToleranceForInputType()
                    && std::abs (position.y - other.position.y) < (float) getPositionToleranceForInputType()
                    && buttons == other.buttons
                    && peerID == other.peerID;
        }

        int getPositionToleranceForInputType() const noexcept    { return isTouch ? 25 : 8;  }
    };

    RecentMouseDown mouseDowns[4];
    Time lastTime;
    bool movedSignificantly = false;

    void registerMouseDown (Point<float> screenPos, Time time, Component& component,
                            const ModifierKeys modifiers, bool isTouchSource) noexcept
    {
        for (int i = numElementsInArray (mouseDowns); --i > 0;)
            mouseDowns[i] = mouseDowns[i - 1];

        mouseDowns[0].position = screenPos;
        mouseDowns[0].time = time;
        mouseDowns[0].buttons = modifiers.withOnlyMouseButtons();
        mouseDowns[0].isTouch = isTouchSource;

        if (auto* peer = component.getPeer())
            mouseDowns[0].peerID = peer->getUniqueID();
        else
            mouseDowns[0].peerID = 0;

        movedSignificantly = false;
        lastNonInertialWheelTarget = nullptr;
    }

    void registerMouseDrag (Point<float> screenPos) noexcept
    {
        movedSignificantly = movedSignificantly || mouseDowns[0].position.getDistanceFrom (screenPos) >= 4;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MouseInputSourceInternal)
};

//==============================================================================
MouseInputSource::MouseInputSource (MouseInputSourceInternal* s) noexcept   : pimpl (s)  {}
MouseInputSource::MouseInputSource (const MouseInputSource& other) noexcept : pimpl (other.pimpl)  {}
MouseInputSource::~MouseInputSource() noexcept {}

MouseInputSource& MouseInputSource::operator= (const MouseInputSource& other) noexcept
{
    pimpl = other.pimpl;
    return *this;
}

MouseInputSource::InputSourceType MouseInputSource::getType() const noexcept    { return pimpl->inputType; }
bool MouseInputSource::isMouse() const noexcept                                 { return (getType() == MouseInputSource::InputSourceType::mouse); }
bool MouseInputSource::isTouch() const noexcept                                 { return (getType() == MouseInputSource::InputSourceType::touch); }
bool MouseInputSource::isPen() const noexcept                                   { return (getType() == MouseInputSource::InputSourceType::pen); }
bool MouseInputSource::canHover() const noexcept                                { return ! isTouch(); }
bool MouseInputSource::hasMouseWheel() const noexcept                           { return ! isTouch(); }
int MouseInputSource::getIndex() const noexcept                                 { return pimpl->index; }
bool MouseInputSource::isDragging() const noexcept                              { return pimpl->isDragging(); }
Point<float> MouseInputSource::getScreenPosition() const noexcept               { return pimpl->getScreenPosition(); }
Point<float> MouseInputSource::getRawScreenPosition() const noexcept            { return pimpl->getRawScreenPosition();  }
ModifierKeys MouseInputSource::getCurrentModifiers() const noexcept             { return pimpl->getCurrentModifiers(); }
float MouseInputSource::getCurrentPressure() const noexcept                     { return pimpl->pressure; }
bool MouseInputSource::isPressureValid() const noexcept                         { return pimpl->isPressureValid(); }
float MouseInputSource::getCurrentOrientation() const noexcept                  { return pimpl->orientation; }
bool MouseInputSource::isOrientationValid() const noexcept                      { return pimpl->isOrientationValid(); }
float MouseInputSource::getCurrentRotation() const noexcept                     { return pimpl->rotation; }
bool MouseInputSource::isRotationValid() const noexcept                         { return pimpl->isRotationValid(); }
float MouseInputSource::getCurrentTilt (bool tiltX) const noexcept              { return tiltX ? pimpl->tiltX : pimpl->tiltY; }
bool MouseInputSource::isTiltValid (bool isX) const noexcept                    { return pimpl->isTiltValid (isX); }
Component* MouseInputSource::getComponentUnderMouse() const                     { return pimpl->getComponentUnderMouse(); }
void MouseInputSource::triggerFakeMove() const                                  { pimpl->triggerFakeMove(); }
int MouseInputSource::getNumberOfMultipleClicks() const noexcept                { return pimpl->getNumberOfMultipleClicks(); }
Time MouseInputSource::getLastMouseDownTime() const noexcept                    { return pimpl->getLastMouseDownTime(); }
Point<float> MouseInputSource::getLastMouseDownPosition() const noexcept        { return pimpl->getLastMouseDownPosition(); }
bool MouseInputSource::isLongPressOrDrag() const noexcept                       { return pimpl->isLongPressOrDrag(); }
bool MouseInputSource::hasMovedSignificantlySincePressed() const noexcept       { return pimpl->hasMovedSignificantlySincePressed(); }
bool MouseInputSource::canDoUnboundedMovement() const noexcept                  { return ! isTouch(); }
void MouseInputSource::enableUnboundedMouseMovement (bool isEnabled, bool keepCursorVisibleUntilOffscreen) const
                                                                         { pimpl->enableUnboundedMouseMovement (isEnabled, keepCursorVisibleUntilOffscreen); }
bool MouseInputSource::isUnboundedMouseMovementEnabled() const           { return pimpl->isUnboundedMouseModeOn; }
bool MouseInputSource::hasMouseCursor() const noexcept                   { return ! isTouch(); }
void MouseInputSource::showMouseCursor (const MouseCursor& cursor)       { pimpl->showMouseCursor (cursor, false); }
void MouseInputSource::hideCursor()                                      { pimpl->hideCursor(); }
void MouseInputSource::revealCursor()                                    { pimpl->revealCursor (false); }
void MouseInputSource::forceMouseCursorUpdate()                          { pimpl->revealCursor (true); }
void MouseInputSource::setScreenPosition (Point<float> p)                { pimpl->setScreenPosition (p); }

void MouseInputSource::handleEvent (ComponentPeer& peer, Point<float> pos, int64 time, ModifierKeys mods,
                                    float pressure, float orientation, const PenDetails& penDetails)
{
    pimpl->handleEvent (peer, pos, Time (time), mods.withOnlyMouseButtons(), pressure, orientation, penDetails);
}

void MouseInputSource::handleWheel (ComponentPeer& peer, Point<float> pos, int64 time, const MouseWheelDetails& wheel)
{
    pimpl->handleWheel (peer, pos, Time (time), wheel);
}

void MouseInputSource::handleMagnifyGesture (ComponentPeer& peer, Point<float> pos, int64 time, float scaleFactor)
{
    pimpl->handleMagnifyGesture (peer, pos, Time (time), scaleFactor);
}

const float MouseInputSource::invalidPressure = 0.0f;
const float MouseInputSource::invalidOrientation = 0.0f;
const float MouseInputSource::invalidRotation = 0.0f;

const float MouseInputSource::invalidTiltX = 0.0f;
const float MouseInputSource::invalidTiltY = 0.0f;

const Point<float> MouseInputSource::offscreenMousePos { -10.0f, -10.0f };

// Deprecated method
bool MouseInputSource::hasMouseMovedSignificantlySincePressed() const noexcept  { return pimpl->hasMouseMovedSignificantlySincePressed(); }

//==============================================================================
struct MouseInputSource::SourceList  : public Timer
{
    SourceList()
    {
       #if JUCE_ANDROID || JUCE_IOS
        auto mainMouseInputType = MouseInputSource::InputSourceType::touch;
       #else
        auto mainMouseInputType = MouseInputSource::InputSourceType::mouse;
       #endif

        addSource (0, mainMouseInputType);
    }

    bool addSource();
    bool canUseTouch();

    MouseInputSource* addSource (int index, MouseInputSource::InputSourceType type)
    {
        auto* s = new MouseInputSourceInternal (index, type);
        sources.add (s);
        sourceArray.add (MouseInputSource (s));

        return &sourceArray.getReference (sourceArray.size() - 1);
    }

    MouseInputSource* getMouseSource (int index) noexcept
    {
        return isPositiveAndBelow (index, sourceArray.size()) ? &sourceArray.getReference (index)
                                                              : nullptr;
    }

    MouseInputSource* getOrCreateMouseInputSource (MouseInputSource::InputSourceType type, int touchIndex = 0)
    {
        if (type == MouseInputSource::InputSourceType::mouse || type == MouseInputSource::InputSourceType::pen)
        {
            for (auto& m : sourceArray)
                if (type == m.getType())
                    return &m;

            addSource (0, type);
        }
        else if (type == MouseInputSource::InputSourceType::touch)
        {
            jassert (touchIndex >= 0 && touchIndex < 100); // sanity-check on number of fingers

            for (auto& m : sourceArray)
                if (type == m.getType() && touchIndex == m.getIndex())
                    return &m;

            if (canUseTouch())
                return addSource (touchIndex, type);
        }

        return nullptr;
    }

    int getNumDraggingMouseSources() const noexcept
    {
        int num = 0;

        for (auto* s : sources)
            if (s->isDragging())
                ++num;

        return num;
    }

    MouseInputSource* getDraggingMouseSource (int index) noexcept
    {
        int num = 0;

        for (auto& s : sourceArray)
        {
            if (s.isDragging())
            {
                if (index == num)
                    return &s;

                ++num;
            }
        }

        return nullptr;
    }

    void beginDragAutoRepeat (int interval)
    {
        if (interval > 0)
        {
            if (getTimerInterval() != interval)
                startTimer (interval);
        }
        else
        {
            stopTimer();
        }
    }

    void timerCallback() override
    {
        bool anyDragging = false;

        for (auto* s : sources)
        {
            // NB: when doing auto-repeat, we need to force an update of the current position and button state,
            // because on some OSes the queue can get overloaded with messages so that mouse-events don't get through..
            if (s->isDragging() && ComponentPeer::getCurrentModifiersRealtime().isAnyMouseButtonDown())
            {
                s->lastScreenPos = s->getRawScreenPosition() - s->unboundedMouseOffset;
                s->triggerFakeMove();
                anyDragging = true;
            }
        }

        if (! anyDragging)
            stopTimer();
    }

    OwnedArray<MouseInputSourceInternal> sources;
    Array<MouseInputSource> sourceArray;
};

} // namespace juce
