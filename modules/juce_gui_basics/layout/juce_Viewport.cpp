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

Viewport::Viewport (const String& name)  : Component (name)
{
    // content holder is used to clip the contents so they don't overlap the scrollbars
    addAndMakeVisible (contentHolder);
    contentHolder.setInterceptsMouseClicks (false, true);

    scrollBarThickness = getLookAndFeel().getDefaultScrollbarWidth();

    setInterceptsMouseClicks (false, true);
    setWantsKeyboardFocus (true);
    setScrollOnDragEnabled (Desktop::getInstance().getMainMouseSource().isTouch());

    recreateScrollbars();
}

Viewport::~Viewport()
{
    setScrollOnDragEnabled (false);
    deleteOrRemoveContentComp();
}

//==============================================================================
void Viewport::visibleAreaChanged (const Rectangle<int>&) {}
void Viewport::viewedComponentChanged (Component*) {}

//==============================================================================
void Viewport::deleteOrRemoveContentComp()
{
    if (contentComp != nullptr)
    {
        contentComp->removeComponentListener (this);

        if (deleteContent)
        {
            // This sets the content comp to a null pointer before deleting the old one, in case
            // anything tries to use the old one while it's in mid-deletion..
            std::unique_ptr<Component> oldCompDeleter (contentComp.get());
            contentComp = nullptr;
        }
        else
        {
            contentHolder.removeChildComponent (contentComp);
            contentComp = nullptr;
        }
    }
}

void Viewport::setViewedComponent (Component* const newViewedComponent, const bool deleteComponentWhenNoLongerNeeded)
{
    if (contentComp.get() != newViewedComponent)
    {
        deleteOrRemoveContentComp();
        contentComp = newViewedComponent;
        deleteContent = deleteComponentWhenNoLongerNeeded;

        if (contentComp != nullptr)
        {
            contentHolder.addAndMakeVisible (contentComp);
            setViewPosition (Point<int>());
            contentComp->addComponentListener (this);
        }

        viewedComponentChanged (contentComp);
        updateVisibleArea();
    }
}

void Viewport::recreateScrollbars()
{
    verticalScrollBar.reset();
    horizontalScrollBar.reset();

    verticalScrollBar  .reset (createScrollBarComponent (true));
    horizontalScrollBar.reset (createScrollBarComponent (false));

    addChildComponent (verticalScrollBar.get());
    addChildComponent (horizontalScrollBar.get());

    getVerticalScrollBar().addListener (this);
    getHorizontalScrollBar().addListener (this);

    resized();
}

int Viewport::getMaximumVisibleWidth() const            { return contentHolder.getWidth(); }
int Viewport::getMaximumVisibleHeight() const           { return contentHolder.getHeight(); }

bool Viewport::canScrollVertically() const noexcept     { return contentComp->getY() < 0 || contentComp->getBottom() > getHeight(); }
bool Viewport::canScrollHorizontally() const noexcept   { return contentComp->getX() < 0 || contentComp->getRight()  > getWidth(); }

Point<int> Viewport::viewportPosToCompPos (Point<int> pos) const
{
    jassert (contentComp != nullptr);

    auto contentBounds = contentHolder.getLocalArea (contentComp.get(), contentComp->getLocalBounds());

    Point<int> p (jmax (jmin (0, contentHolder.getWidth()  - contentBounds.getWidth()),  jmin (0, -(pos.x))),
                  jmax (jmin (0, contentHolder.getHeight() - contentBounds.getHeight()), jmin (0, -(pos.y))));

    return p.transformedBy (contentComp->getTransform().inverted());
}

void Viewport::setViewPosition (const int xPixelsOffset, const int yPixelsOffset)
{
    setViewPosition ({ xPixelsOffset, yPixelsOffset });
}

void Viewport::setViewPosition (Point<int> newPosition)
{
    if (contentComp != nullptr)
        contentComp->setTopLeftPosition (viewportPosToCompPos (newPosition));
}

void Viewport::setViewPositionProportionately (const double x, const double y)
{
    if (contentComp != nullptr)
        setViewPosition (jmax (0, roundToInt (x * (contentComp->getWidth()  - getWidth()))),
                         jmax (0, roundToInt (y * (contentComp->getHeight() - getHeight()))));
}

bool Viewport::autoScroll (const int mouseX, const int mouseY, const int activeBorderThickness, const int maximumSpeed)
{
    if (contentComp != nullptr)
    {
        int dx = 0, dy = 0;

        if (getHorizontalScrollBar().isVisible() || canScrollHorizontally())
        {
            if (mouseX < activeBorderThickness)
                dx = activeBorderThickness - mouseX;
            else if (mouseX >= contentHolder.getWidth() - activeBorderThickness)
                dx = (contentHolder.getWidth() - activeBorderThickness) - mouseX;

            if (dx < 0)
                dx = jmax (dx, -maximumSpeed, contentHolder.getWidth() - contentComp->getRight());
            else
                dx = jmin (dx, maximumSpeed, -contentComp->getX());
        }

        if (getVerticalScrollBar().isVisible() || canScrollVertically())
        {
            if (mouseY < activeBorderThickness)
                dy = activeBorderThickness - mouseY;
            else if (mouseY >= contentHolder.getHeight() - activeBorderThickness)
                dy = (contentHolder.getHeight() - activeBorderThickness) - mouseY;

            if (dy < 0)
                dy = jmax (dy, -maximumSpeed, contentHolder.getHeight() - contentComp->getBottom());
            else
                dy = jmin (dy, maximumSpeed, -contentComp->getY());
        }

        if (dx != 0 || dy != 0)
        {
            contentComp->setTopLeftPosition (contentComp->getX() + dx,
                                             contentComp->getY() + dy);

            return true;
        }
    }

    return false;
}

void Viewport::componentMovedOrResized (Component&, bool, bool)
{
    updateVisibleArea();
}

//==============================================================================
typedef AnimatedPosition<AnimatedPositionBehaviours::ContinuousWithMomentum> ViewportDragPosition;

struct Viewport::DragToScrollListener   : private MouseListener,
                                          private ViewportDragPosition::Listener
{
    DragToScrollListener (Viewport& v)  : viewport (v)
    {
        viewport.contentHolder.addMouseListener (this, true);
        offsetX.addListener (this);
        offsetY.addListener (this);
        offsetX.behaviour.setMinimumVelocity (60);
        offsetY.behaviour.setMinimumVelocity (60);
    }

    ~DragToScrollListener() override
    {
        viewport.contentHolder.removeMouseListener (this);
        Desktop::getInstance().removeGlobalMouseListener (this);
    }

    void positionChanged (ViewportDragPosition&, double) override
    {
        viewport.setViewPosition (originalViewPos - Point<int> ((int) offsetX.getPosition(),
                                                                (int) offsetY.getPosition()));
    }

    void mouseDown (const MouseEvent&) override
    {
        if (! isGlobalMouseListener)
        {
            offsetX.setPosition (offsetX.getPosition());
            offsetY.setPosition (offsetY.getPosition());

            // switch to a global mouse listener so we still receive mouseUp events
            // if the original event component is deleted
            viewport.contentHolder.removeMouseListener (this);
            Desktop::getInstance().addGlobalMouseListener (this);

            isGlobalMouseListener = true;
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (Desktop::getInstance().getNumDraggingMouseSources() == 1 && ! doesMouseEventComponentBlockViewportDrag (e.eventComponent))
        {
            auto totalOffset = e.getOffsetFromDragStart().toFloat();

            if (! isDragging && totalOffset.getDistanceFromOrigin() > 8.0f)
            {
                isDragging = true;

                originalViewPos = viewport.getViewPosition();
                offsetX.setPosition (0.0);
                offsetX.beginDrag();
                offsetY.setPosition (0.0);
                offsetY.beginDrag();
            }

            if (isDragging)
            {
                offsetX.drag (totalOffset.x);
                offsetY.drag (totalOffset.y);
            }
        }
    }

    void mouseUp (const MouseEvent&) override
    {
        if (isGlobalMouseListener && Desktop::getInstance().getNumDraggingMouseSources() == 0)
            endDragAndClearGlobalMouseListener();
    }

    void endDragAndClearGlobalMouseListener()
    {
        offsetX.endDrag();
        offsetY.endDrag();
        isDragging = false;

        viewport.contentHolder.addMouseListener (this, true);
        Desktop::getInstance().removeGlobalMouseListener (this);

        isGlobalMouseListener = false;
    }

    bool doesMouseEventComponentBlockViewportDrag (const Component* eventComp)
    {
        for (auto c = eventComp; c != nullptr && c != &viewport; c = c->getParentComponent())
            if (c->getViewportIgnoreDragFlag())
                return true;

        return false;
    }

    Viewport& viewport;
    ViewportDragPosition offsetX, offsetY;
    Point<int> originalViewPos;
    bool isDragging = false;
    bool isGlobalMouseListener = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragToScrollListener)
};

void Viewport::setScrollOnDragEnabled (bool shouldScrollOnDrag)
{
    if (isScrollOnDragEnabled() != shouldScrollOnDrag)
    {
        if (shouldScrollOnDrag)
            dragToScrollListener.reset (new DragToScrollListener (*this));
        else
            dragToScrollListener.reset();
    }
}

bool Viewport::isScrollOnDragEnabled() const noexcept
{
    return dragToScrollListener != nullptr;
}

bool Viewport::isCurrentlyScrollingOnDrag() const noexcept
{
    return dragToScrollListener != nullptr && dragToScrollListener->isDragging;
}
    
void Viewport::setFloatingScrollbarEnabled(bool floating)
{
    if (floatingScrollbars != floating)
    {
        floatingScrollbars = floating;
        updateVisibleArea();
    }
}
    
bool Viewport::isFloatingScrollbarEnabled() const noexcept
{
    return floatingScrollbars;
}

//==============================================================================
void Viewport::lookAndFeelChanged()
{
    if (! customScrollBarThickness)
    {
        scrollBarThickness = getLookAndFeel().getDefaultScrollbarWidth();
        resized();
    }
}

void Viewport::resized()
{
    updateVisibleArea();
}

//==============================================================================
void Viewport::updateVisibleArea()
{
    auto scrollbarWidth = getScrollBarThickness();
    const bool canShowAnyBars = getWidth() > scrollbarWidth && getHeight() > scrollbarWidth;
    const bool canShowHBar = showHScrollbar && canShowAnyBars;
    const bool canShowVBar = showVScrollbar && canShowAnyBars;

    bool hBarVisible = false, vBarVisible = false;
    Rectangle<int> contentArea;

    for (int i = 3; --i >= 0;)
    {
        hBarVisible = canShowHBar && ! getHorizontalScrollBar().autoHides();
        vBarVisible = canShowVBar && ! getVerticalScrollBar().autoHides();
        contentArea = getLocalBounds();

        if (contentComp != nullptr && ! contentArea.contains (contentComp->getBounds()))
        {
            hBarVisible = canShowHBar && (hBarVisible || contentComp->getX() < 0 || contentComp->getRight() > contentArea.getWidth());
            vBarVisible = canShowVBar && (vBarVisible || contentComp->getY() < 0 || contentComp->getBottom() > contentArea.getHeight());

            if (vBarVisible)
                contentArea.setWidth (getWidth() - scrollbarWidth);

            if (hBarVisible)
                contentArea.setHeight (getHeight() - scrollbarWidth);

            if (! contentArea.contains (contentComp->getBounds()))
            {
                hBarVisible = canShowHBar && (hBarVisible || contentComp->getRight() > contentArea.getWidth());
                vBarVisible = canShowVBar && (vBarVisible || contentComp->getBottom() > contentArea.getHeight());
            }
        }

        if (vBarVisible)  contentArea.setWidth  (getWidth()  - scrollbarWidth);
        if (hBarVisible)  contentArea.setHeight (getHeight() - scrollbarWidth);

        if (! vScrollbarRight  && vBarVisible)
            contentArea.setX (scrollbarWidth);

        if (! hScrollbarBottom && hBarVisible)
            contentArea.setY (scrollbarWidth);

        if (contentComp == nullptr)
        {
            contentHolder.setBounds (floatingScrollbars ? getLocalBounds() : contentArea);
            break;
        }

        auto oldContentBounds = contentComp->getBounds();
        contentHolder.setBounds (floatingScrollbars ? getLocalBounds() : contentArea);

        // If the content has changed its size, that might affect our scrollbars, so go round again and re-calculate..
        if (oldContentBounds == contentComp->getBounds())
            break;
    }

    Rectangle<int> contentBounds;

    if (auto cc = contentComp.get())
        contentBounds = contentHolder.getLocalArea (cc, cc->getLocalBounds());

    auto visibleOrigin = -contentBounds.getPosition();

    auto& hbar = getHorizontalScrollBar();
    auto& vbar = getVerticalScrollBar();

    hbar.setBounds (contentArea.getX(), hScrollbarBottom ? contentArea.getHeight() : 0, contentArea.getWidth(), scrollbarWidth);
    hbar.setRangeLimits (0.0, contentBounds.getWidth());
    hbar.setCurrentRange (visibleOrigin.x, contentArea.getWidth());
    hbar.setSingleStepSize (singleStepX);

    if (canShowHBar && ! hBarVisible)
        visibleOrigin.setX (0);

    vbar.setBounds (vScrollbarRight ? contentArea.getWidth() : 0, contentArea.getY(), scrollbarWidth, contentArea.getHeight());
    vbar.setRangeLimits (0.0, contentBounds.getHeight());
    vbar.setCurrentRange (visibleOrigin.y, contentArea.getHeight());
    vbar.setSingleStepSize (singleStepY);

    if (canShowVBar && ! vBarVisible)
        visibleOrigin.setY (0);

    // Force the visibility *after* setting the ranges to avoid flicker caused by edge conditions in the numbers.
    hbar.setVisible (hBarVisible);
    vbar.setVisible (vBarVisible);

    if (contentComp != nullptr)
    {
        auto newContentCompPos = viewportPosToCompPos (visibleOrigin);

        if (contentComp->getBounds().getPosition() != newContentCompPos)
        {
            contentComp->setTopLeftPosition (newContentCompPos);  // (this will re-entrantly call updateVisibleArea again)
            return;
        }
    }

    const Rectangle<int> visibleArea (visibleOrigin.x, visibleOrigin.y,
                                      jmin (contentBounds.getWidth()  - visibleOrigin.x, contentArea.getWidth()),
                                      jmin (contentBounds.getHeight() - visibleOrigin.y, contentArea.getHeight()));

    if (lastVisibleArea != visibleArea)
    {
        lastVisibleArea = visibleArea;
        visibleAreaChanged (visibleArea);
    }

    hbar.handleUpdateNowIfNeeded();
    vbar.handleUpdateNowIfNeeded();
}

//==============================================================================
void Viewport::setSingleStepSizes (const int stepX, const int stepY)
{
    if (singleStepX != stepX || singleStepY != stepY)
    {
        singleStepX = stepX;
        singleStepY = stepY;
        updateVisibleArea();
    }
}

void Viewport::setScrollBarsShown (const bool showVerticalScrollbarIfNeeded,
                                   const bool showHorizontalScrollbarIfNeeded,
                                   const bool allowVerticalScrollingWithoutScrollbar,
                                   const bool allowHorizontalScrollingWithoutScrollbar)
{
    allowScrollingWithoutScrollbarV = allowVerticalScrollingWithoutScrollbar;
    allowScrollingWithoutScrollbarH = allowHorizontalScrollingWithoutScrollbar;

    if (showVScrollbar != showVerticalScrollbarIfNeeded
         || showHScrollbar != showHorizontalScrollbarIfNeeded)
    {
        showVScrollbar = showVerticalScrollbarIfNeeded;
        showHScrollbar = showHorizontalScrollbarIfNeeded;
        updateVisibleArea();
    }
}

void Viewport::setScrollBarThickness (const int thickness)
{
    int newThickness;

    // To stay compatible with the previous code: use the
    // default thickness if thickness parameter is zero
    // or negative
    if (thickness <= 0)
    {
        customScrollBarThickness = false;
        newThickness = getLookAndFeel().getDefaultScrollbarWidth();
    }
    else
    {
        customScrollBarThickness = true;
        newThickness = thickness;
    }

    if (scrollBarThickness != newThickness)
    {
        scrollBarThickness = newThickness;
        updateVisibleArea();
    }
}

int Viewport::getScrollBarThickness() const
{
    return scrollBarThickness;
}

void Viewport::scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    auto newRangeStartInt = roundToInt (newRangeStart);

    if (scrollBarThatHasMoved == horizontalScrollBar.get())
    {
        setViewPosition (newRangeStartInt, getViewPositionY());
    }
    else if (scrollBarThatHasMoved == verticalScrollBar.get())
    {
        setViewPosition (getViewPositionX(), newRangeStartInt);
    }
}

void Viewport::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (! useMouseWheelMoveIfNeeded (e, wheel))
        Component::mouseWheelMove (e, wheel);
}

static int rescaleMouseWheelDistance (float distance, int singleStepSize) noexcept
{
    if (distance == 0.0f)
        return 0;

    distance *= 14.0f * (float) singleStepSize;

    return roundToInt (distance < 0 ? jmin (distance, -1.0f)
                                    : jmax (distance,  1.0f));
}

bool Viewport::useMouseWheelMoveIfNeeded (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (! (e.mods.isAltDown() || e.mods.isCtrlDown() || e.mods.isCommandDown()))
    {
        const bool canScrollVert = (allowScrollingWithoutScrollbarV || getVerticalScrollBar().isVisible());
        const bool canScrollHorz = (allowScrollingWithoutScrollbarH || getHorizontalScrollBar().isVisible());

        if (canScrollHorz || canScrollVert)
        {
            auto deltaX = rescaleMouseWheelDistance (wheel.deltaX, singleStepX);
            auto deltaY = rescaleMouseWheelDistance (wheel.deltaY, singleStepY);

            auto pos = getViewPosition();

            if (deltaX != 0 && deltaY != 0 && canScrollHorz && canScrollVert)
            {
                pos.x -= deltaX;
                pos.y -= deltaY;
            }
            else if (canScrollHorz && (deltaX != 0 || e.mods.isShiftDown() || ! canScrollVert))
            {
                pos.x -= deltaX != 0 ? deltaX : deltaY;
            }
            else if (canScrollVert && deltaY != 0)
            {
                pos.y -= deltaY;
            }

            if (pos != getViewPosition())
            {
                setViewPosition (pos);
                return true;
            }
        }
    }

    return false;
}

static bool isUpDownKeyPress (const KeyPress& key)
{
    return key == KeyPress::upKey
        || key == KeyPress::downKey
        || key == KeyPress::pageUpKey
        || key == KeyPress::pageDownKey
        || key == KeyPress::homeKey
        || key == KeyPress::endKey;
}

static bool isLeftRightKeyPress (const KeyPress& key)
{
    return key == KeyPress::leftKey
        || key == KeyPress::rightKey;
}

bool Viewport::keyPressed (const KeyPress& key)
{
    const bool isUpDownKey = isUpDownKeyPress (key);

    if (getVerticalScrollBar().isVisible() && isUpDownKey)
        return getVerticalScrollBar().keyPressed (key);

    const bool isLeftRightKey = isLeftRightKeyPress (key);

    if (getHorizontalScrollBar().isVisible() && (isUpDownKey || isLeftRightKey))
        return getHorizontalScrollBar().keyPressed (key);

    return false;
}

bool Viewport::respondsToKey (const KeyPress& key)
{
    return isUpDownKeyPress (key) || isLeftRightKeyPress (key);
}

ScrollBar* Viewport::createScrollBarComponent (bool isVertical)
{
    return new ScrollBar (isVertical);
}

void Viewport::setScrollBarPosition (bool verticalScrollbarOnRight,
                                     bool horizontalScrollbarAtBottom)
{
    vScrollbarRight  = verticalScrollbarOnRight;
    hScrollbarBottom = horizontalScrollbarAtBottom;

    resized();
}

} // namespace juce
