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

ResizableWindow::ResizableWindow (const String& name, bool shouldAddToDesktop)
    : TopLevelWindow (name, shouldAddToDesktop)
{
    initialise (shouldAddToDesktop);
}

ResizableWindow::ResizableWindow (const String& name, Colour bkgnd, bool shouldAddToDesktop)
    : TopLevelWindow (name, shouldAddToDesktop)
{
    setBackgroundColour (bkgnd);
    initialise (shouldAddToDesktop);
}

ResizableWindow::~ResizableWindow()
{
    splashScreen.deleteAndZero();

    // Don't delete or remove the resizer components yourself! They're managed by the
    // ResizableWindow, and you should leave them alone! You may have deleted them
    // accidentally by careless use of deleteAllChildren()..?
    jassert (resizableCorner == nullptr || getIndexOfChildComponent (resizableCorner.get()) >= 0);
    jassert (resizableBorder == nullptr || getIndexOfChildComponent (resizableBorder.get()) >= 0);

    resizableCorner.reset();
    resizableBorder.reset();
    clearContentComponent();

    // have you been adding your own components directly to this window..? tut tut tut.
    // Read the instructions for using a ResizableWindow!
    jassert (getNumChildComponents() == 0);
}

void ResizableWindow::initialise (const bool shouldAddToDesktop)
{
    /*
      ==========================================================================

       In accordance with the terms of the JUCE 6 End-Use License Agreement, the
       JUCE Code in SECTION A cannot be removed, changed or otherwise rendered
       ineffective unless you have a JUCE Indie or Pro license, or are using
       JUCE under the GPL v3 license.

       End User License Agreement: www.juce.com/juce-6-licence

      ==========================================================================
    */

    // BEGIN SECTION A

   #if ! JucePlugin_Build_Standalone
    splashScreen = new JUCESplashScreen (*this);
   #endif

    // END SECTION A

    defaultConstrainer.setMinimumOnscreenAmounts (0x10000, 16, 24, 16);

    lastNonFullScreenPos.setBounds (50, 50, 256, 256);

    if (shouldAddToDesktop)
        addToDesktop();
}

int ResizableWindow::getDesktopWindowStyleFlags() const
{
    int styleFlags = TopLevelWindow::getDesktopWindowStyleFlags();

    if (isResizable() && (styleFlags & ComponentPeer::windowHasTitleBar) != 0)
        styleFlags |= ComponentPeer::windowIsResizable;

    return styleFlags;
}

//==============================================================================
void ResizableWindow::clearContentComponent()
{
    if (ownsContentComponent)
    {
        contentComponent.deleteAndZero();
    }
    else
    {
        removeChildComponent (contentComponent);
        contentComponent = nullptr;
    }
}

void ResizableWindow::setContent (Component* newContentComponent,
                                  bool takeOwnership,
                                  bool resizeToFitWhenContentChangesSize)
{
    if (newContentComponent != contentComponent)
    {
        clearContentComponent();

        contentComponent = newContentComponent;
        Component::addAndMakeVisible (contentComponent);
    }

    ownsContentComponent = takeOwnership;
    resizeToFitContent = resizeToFitWhenContentChangesSize;

    if (resizeToFitWhenContentChangesSize)
        childBoundsChanged (contentComponent);

    resized(); // must always be called to position the new content comp
}

void ResizableWindow::setContentOwned (Component* newContentComponent, const bool resizeToFitWhenContentChangesSize)
{
    setContent (newContentComponent, true, resizeToFitWhenContentChangesSize);
}

void ResizableWindow::setContentNonOwned (Component* newContentComponent, const bool resizeToFitWhenContentChangesSize)
{
    setContent (newContentComponent, false, resizeToFitWhenContentChangesSize);
}

void ResizableWindow::setContentComponent (Component* const newContentComponent,
                                           const bool deleteOldOne,
                                           const bool resizeToFitWhenContentChangesSize)
{
    if (newContentComponent != contentComponent)
    {
        if (deleteOldOne)
        {
            contentComponent.deleteAndZero();
        }
        else
        {
            removeChildComponent (contentComponent);
            contentComponent = nullptr;
        }
    }

    setContent (newContentComponent, true, resizeToFitWhenContentChangesSize);
}

void ResizableWindow::setContentComponentSize (int width, int height)
{
    jassert (width > 0 && height > 0); // not a great idea to give it a zero size..

    auto border = getContentComponentBorder();

    setSize (width + border.getLeftAndRight(),
             height + border.getTopAndBottom());
}

BorderSize<int> ResizableWindow::getBorderThickness()
{
    if (isUsingNativeTitleBar() || isKioskMode())
        return {};

    return BorderSize<int> ((resizableBorder != nullptr && ! isFullScreen()) ? 4 : 1);
}

BorderSize<int> ResizableWindow::getContentComponentBorder()
{
    return getBorderThickness();
}

void ResizableWindow::moved()
{
    updateLastPosIfShowing();
}

void ResizableWindow::visibilityChanged()
{
    TopLevelWindow::visibilityChanged();
    updateLastPosIfShowing();
}

void ResizableWindow::resized()
{
    const bool resizerHidden = isFullScreen() || isKioskMode() || isUsingNativeTitleBar();

    if (resizableBorder != nullptr)
    {
        resizableBorder->setVisible (! resizerHidden);
        resizableBorder->setBorderThickness (getBorderThickness());
        resizableBorder->setSize (getWidth(), getHeight());
        resizableBorder->toBack();
    }

    if (resizableCorner != nullptr)
    {
        resizableCorner->setVisible (! resizerHidden);

        const int resizerSize = 18;
        resizableCorner->setBounds (getWidth() - resizerSize,
                                    getHeight() - resizerSize,
                                    resizerSize, resizerSize);
    }

    if (contentComponent != nullptr)
    {
        // The window expects to be able to be able to manage the size and position
        // of its content component, so you can't arbitrarily add a transform to it!
        jassert (! contentComponent->isTransformed());

        contentComponent->setBoundsInset (getContentComponentBorder());
    }

    updateLastPosIfShowing();

   #if JUCE_DEBUG
    hasBeenResized = true;
   #endif
}

void ResizableWindow::childBoundsChanged (Component* child)
{
    if ((child == contentComponent) && (child != nullptr) && resizeToFitContent)
    {
        // not going to look very good if this component has a zero size..
        jassert (child->getWidth() > 0);
        jassert (child->getHeight() > 0);

        auto borders = getContentComponentBorder();

        setSize (child->getWidth() + borders.getLeftAndRight(),
                 child->getHeight() + borders.getTopAndBottom());
    }
}


//==============================================================================
void ResizableWindow::activeWindowStatusChanged()
{
    auto border = getContentComponentBorder();
    auto area = getLocalBounds();

    repaint (area.removeFromTop (border.getTop()));
    repaint (area.removeFromLeft (border.getLeft()));
    repaint (area.removeFromRight (border.getRight()));
    repaint (area.removeFromBottom (border.getBottom()));
}

//==============================================================================
void ResizableWindow::setResizable (const bool shouldBeResizable,
                                    const bool useBottomRightCornerResizer)
{
    if (shouldBeResizable)
    {
        if (useBottomRightCornerResizer)
        {
            resizableBorder.reset();

            if (resizableCorner == nullptr)
            {
                resizableCorner.reset (new ResizableCornerComponent (this, constrainer));
                Component::addChildComponent (resizableCorner.get());
                resizableCorner->setAlwaysOnTop (true);
            }
        }
        else
        {
            resizableCorner.reset();

            if (resizableBorder == nullptr)
            {
                resizableBorder.reset (new ResizableBorderComponent (this, constrainer));
                Component::addChildComponent (resizableBorder.get());
            }
        }
    }
    else
    {
        resizableCorner.reset();
        resizableBorder.reset();
    }

    if (isUsingNativeTitleBar())
        recreateDesktopWindow();

    childBoundsChanged (contentComponent);
    resized();
}

bool ResizableWindow::isResizable() const noexcept
{
    return resizableCorner != nullptr
        || resizableBorder != nullptr;
}

void ResizableWindow::setResizeLimits (int newMinimumWidth,
                                       int newMinimumHeight,
                                       int newMaximumWidth,
                                       int newMaximumHeight) noexcept
{
    // if you've set up a custom constrainer then these settings won't have any effect..
    jassert (constrainer == &defaultConstrainer || constrainer == nullptr);

    if (constrainer == nullptr)
        setConstrainer (&defaultConstrainer);

    defaultConstrainer.setSizeLimits (newMinimumWidth, newMinimumHeight,
                                      newMaximumWidth, newMaximumHeight);

    setBoundsConstrained (getBounds());
}

void ResizableWindow::setDraggable (bool shouldBeDraggable) noexcept
{
    canDrag = shouldBeDraggable;
}

void ResizableWindow::setConstrainer (ComponentBoundsConstrainer* newConstrainer)
{
    if (constrainer != newConstrainer)
    {
        constrainer = newConstrainer;

        bool useBottomRightCornerResizer = resizableCorner != nullptr;
        bool shouldBeResizable = useBottomRightCornerResizer || resizableBorder != nullptr;

        resizableCorner.reset();
        resizableBorder.reset();

        setResizable (shouldBeResizable, useBottomRightCornerResizer);
        updatePeerConstrainer();
    }
}

void ResizableWindow::setBoundsConstrained (const Rectangle<int>& newBounds)
{
    if (constrainer != nullptr)
        constrainer->setBoundsForComponent (this, newBounds, false, false, false, false);
    else
        setBounds (newBounds);
}

//==============================================================================
void ResizableWindow::paint (Graphics& g)
{
    auto& lf = getLookAndFeel();

    lf.fillResizableWindowBackground (g, getWidth(), getHeight(),
                                      getBorderThickness(), *this);

    if (! isFullScreen())
        lf.drawResizableWindowBorder (g, getWidth(), getHeight(),
                                      getBorderThickness(), *this);

   #if JUCE_DEBUG
    /* If this fails, then you've probably written a subclass with a resized()
       callback but forgotten to make it call its parent class's resized() method.

       It's important when you override methods like resized(), moved(),
       etc., that you make sure the base class methods also get called.

       Of course you shouldn't really be overriding ResizableWindow::resized() anyway,
       because your content should all be inside the content component - and it's the
       content component's resized() method that you should be using to do your
       layout.
    */
    jassert (hasBeenResized || (getWidth() == 0 && getHeight() == 0));
   #endif
}

void ResizableWindow::lookAndFeelChanged()
{
    resized();

    if (isOnDesktop())
    {
        Component::addToDesktop (getDesktopWindowStyleFlags());
        updatePeerConstrainer();
    }
}

Colour ResizableWindow::getBackgroundColour() const noexcept
{
    return findColour (backgroundColourId, false);
}

void ResizableWindow::setBackgroundColour (Colour newColour)
{
    auto backgroundColour = newColour;

    if (! Desktop::canUseSemiTransparentWindows())
        backgroundColour = newColour.withAlpha (1.0f);

    setColour (backgroundColourId, backgroundColour);
    setOpaque (backgroundColour.isOpaque());
    repaint();
}

//==============================================================================
bool ResizableWindow::isFullScreen() const
{
    if (isOnDesktop())
    {
        auto* peer = getPeer();
        return peer != nullptr && peer->isFullScreen();
    }

    return fullscreen;
}

void ResizableWindow::setFullScreen (const bool shouldBeFullScreen)
{
    if (shouldBeFullScreen != isFullScreen())
    {
        updateLastPosIfShowing();
        fullscreen = shouldBeFullScreen;

        if (isOnDesktop())
        {
            if (auto* peer = getPeer())
            {
                // keep a copy of this intact in case the real one gets messed-up while we're un-maximising
                auto lastPos = lastNonFullScreenPos;

                peer->setFullScreen (shouldBeFullScreen);

                if ((! shouldBeFullScreen) && ! lastPos.isEmpty())
                    setBounds (lastPos);
            }
            else
            {
                jassertfalse;
            }
        }
        else
        {
            if (shouldBeFullScreen)
                setBounds (0, 0, getParentWidth(), getParentHeight());
            else
                setBounds (lastNonFullScreenPos);
        }

        resized();
    }
}

bool ResizableWindow::isMinimised() const
{
    if (auto* peer = getPeer())
        return peer->isMinimised();

    return false;
}

void ResizableWindow::setMinimised (const bool shouldMinimise)
{
    if (shouldMinimise != isMinimised())
    {
        if (auto* peer = getPeer())
        {
            updateLastPosIfShowing();
            peer->setMinimised (shouldMinimise);
        }
        else
        {
            jassertfalse;
        }
    }
}

bool ResizableWindow::isKioskMode() const
{
    if (isOnDesktop())
        if (auto* peer = getPeer())
            return peer->isKioskMode();

    return Desktop::getInstance().getKioskModeComponent() == this;
}

void ResizableWindow::updateLastPosIfShowing()
{
    if (isShowing())
    {
        updateLastPosIfNotFullScreen();
        updatePeerConstrainer();
    }
}

void ResizableWindow::updateLastPosIfNotFullScreen()
{
    if (! (isFullScreen() || isMinimised() || isKioskMode()))
        lastNonFullScreenPos = getBounds();
}

void ResizableWindow::updatePeerConstrainer()
{
    if (isOnDesktop())
        if (auto* peer = getPeer())
            peer->setConstrainer (constrainer);
}

void ResizableWindow::parentSizeChanged()
{
    if (isFullScreen() && getParentComponent() != nullptr)
        setBounds (getParentComponent()->getLocalBounds());
}

//==============================================================================
String ResizableWindow::getWindowStateAsString()
{
    updateLastPosIfShowing();
    return String (isVisible() ? "" : "h ") + String (isFullScreen() && ! isKioskMode() ? "fs " : "")
                                            + lastNonFullScreenPos.toString();
}

bool ResizableWindow::restoreWindowStateFromString (const String& s, bool forceVisible)
{
    StringArray tokens;
    tokens.addTokens (s, false);
    tokens.removeEmptyStrings();
    tokens.trim();

    const bool hidden = tokens[0].startsWithIgnoreCase ("h");
    const int fsToken = (hidden ? 1 : 0);
    const bool fs = tokens[fsToken].startsWithIgnoreCase ("fs");
    const int firstCoord = fsToken + (fs ? 1 : 0);

    if (tokens.size() != firstCoord + 4)
        return false;

    Rectangle<int> newPos (tokens[firstCoord].getIntValue(),
                           tokens[firstCoord + 1].getIntValue(),
                           tokens[firstCoord + 2].getIntValue(),
                           tokens[firstCoord + 3].getIntValue());

    if (newPos.isEmpty())
        return false;

    auto* peer = isOnDesktop() ? getPeer() : nullptr;

    if (peer != nullptr)
        peer->getFrameSize().addTo (newPos);

    {
        auto& desktop = Desktop::getInstance();
        auto allMonitors = desktop.getDisplays().getRectangleList (true);
        allMonitors.clipTo (newPos);
        auto onScreenArea = allMonitors.getBounds();

        if (onScreenArea.getWidth() * onScreenArea.getHeight() < 32 * 32)
        {
            auto screen = desktop.getDisplays().getDisplayForRect (newPos)->userArea;

            newPos.setSize (jmin (newPos.getWidth(),  screen.getWidth()),
                            jmin (newPos.getHeight(), screen.getHeight()));

            newPos.setPosition (jlimit (screen.getX(), screen.getRight()  - newPos.getWidth(),  newPos.getX()),
                                jlimit (screen.getY(), screen.getBottom() - newPos.getHeight(), newPos.getY()));
        }
    }

    if (peer != nullptr)
    {
        peer->getFrameSize().subtractFrom (newPos);
        peer->setNonFullScreenBounds (newPos);
    }

    updateLastPosIfNotFullScreen();

    if (fs)
        setBoundsConstrained (newPos);

    setFullScreen (fs);

    if (! fs)
        setBoundsConstrained (newPos);
    
    setVisible (forceVisible ? true : ! hidden);

    return true;
}

//==============================================================================
void ResizableWindow::mouseDown (const MouseEvent& e)
{
    if (canDrag && ! isFullScreen())
    {
        dragStarted = true;
        dragger.startDraggingComponent (this, e);
    }
}

void ResizableWindow::mouseDrag (const MouseEvent& e)
{
    if (dragStarted)
        dragger.dragComponent (this, e, constrainer);
}

void ResizableWindow::mouseUp (const MouseEvent&)
{
    dragStarted = false;
}

//==============================================================================
#if JUCE_DEBUG
void ResizableWindow::addChildComponent (Component* const child, int zOrder)
{
    /* Agh! You shouldn't add components directly to a ResizableWindow - this class
       manages its child components automatically, and if you add your own it'll cause
       trouble. Instead, use setContentComponent() to give it a component which
       will be automatically resized and kept in the right place - then you can add
       subcomponents to the content comp. See the notes for the ResizableWindow class
       for more info.

       If you really know what you're doing and want to avoid this assertion, just call
       Component::addChildComponent directly.
    */
    jassertfalse;

    Component::addChildComponent (child, zOrder);
}

void ResizableWindow::addAndMakeVisible (Component* const child, int zOrder)
{
    /* Agh! You shouldn't add components directly to a ResizableWindow - this class
       manages its child components automatically, and if you add your own it'll cause
       trouble. Instead, use setContentComponent() to give it a component which
       will be automatically resized and kept in the right place - then you can add
       subcomponents to the content comp. See the notes for the ResizableWindow class
       for more info.

       If you really know what you're doing and want to avoid this assertion, just call
       Component::addAndMakeVisible directly.
    */
    jassertfalse;

    Component::addAndMakeVisible (child, zOrder);
}
#endif

} // namespace juce
