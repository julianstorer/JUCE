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

//==============================================================================
/**
    A base class for top-level windows that can be dragged around and resized.

    To add content to the window, use its setContentOwned() or setContentNonOwned() methods
    to give it a component that will remain positioned inside it (leaving a gap around
    the edges for a border).

    It's not advisable to add child components directly to a ResizableWindow: put them
    inside your content component instead. And overriding methods like resized(), moved(), etc
    is also not recommended - instead override these methods for your content component.
    (If for some obscure reason you do need to override these methods, always remember to
    call the super-class's resized() method too, otherwise it'll fail to lay out the window
    decorations correctly).

    By default resizing isn't enabled - use the setResizable() method to enable it and
    to choose the style of resizing to use.

    @see TopLevelWindow

    @tags{GUI}
*/
class JUCE_API  ResizableWindow  : public TopLevelWindow
{
public:
    //==============================================================================
    /** Creates a ResizableWindow.

        This constructor doesn't specify a background colour, so the LookAndFeel's default
        background colour will be used.

        @param name                 the name to give the component
        @param addToDesktop         if true, the window will be automatically added to the
                                    desktop; if false, you can use it as a child component
    */
    ResizableWindow (const String& name,
                     bool addToDesktop);

    /** Creates a ResizableWindow.

        @param name                 the name to give the component
        @param backgroundColour     the colour to use for filling the window's background.
        @param addToDesktop         if true, the window will be automatically added to the
                                    desktop; if false, you can use it as a child component
    */
    ResizableWindow (const String& name,
                     Colour backgroundColour,
                     bool addToDesktop);

    /** Destructor.
        If a content component has been set with setContentOwned(), it will be deleted.
    */
    ~ResizableWindow() override;

    //==============================================================================
    /** Returns the colour currently being used for the window's background.

        As a convenience the window will fill itself with this colour, but you
        can override the paint() method if you need more customised behaviour.

        This method is the same as retrieving the colour for ResizableWindow::backgroundColourId.

        @see setBackgroundColour
    */
    Colour getBackgroundColour() const noexcept;

    /** Changes the colour currently being used for the window's background.

        As a convenience the window will fill itself with this colour, but you
        can override the paint() method if you need more customised behaviour.

        Note that the opaque state of this window is altered by this call to reflect
        the opacity of the colour passed-in. On window systems which can't support
        semi-transparent windows this might cause problems, (though it's unlikely you'll
        be using this class as a base for a semi-transparent component anyway).

        You can also use the ResizableWindow::backgroundColourId colour id to set
        this colour.

        @see getBackgroundColour
    */
    void setBackgroundColour (Colour newColour);

    //==============================================================================
    /** Make the window resizable or fixed.

        @param shouldBeResizable            whether it's resizable at all
        @param useBottomRightCornerResizer  if true, it'll add a ResizableCornerComponent at the
                                            bottom-right; if false, it'll use a ResizableBorderComponent
                                            around the edge
        @see setResizeLimits, isResizable
    */
    void setResizable (bool shouldBeResizable,
                       bool useBottomRightCornerResizer);

    /** Returns true if resizing is enabled.
        @see setResizable
    */
    bool isResizable() const noexcept;

    /** This sets the maximum and minimum sizes for the window.

        If the window's current size is outside these limits, it will be resized to
        make sure it's within them.

        A direct call to setBounds() will bypass any constraint checks, but when the
        window is dragged by the user or resized by other indirect means, the constrainer
        will limit the numbers involved.

        @see setResizable, setFixedAspectRatio
    */
    void setResizeLimits (int newMinimumWidth,
                          int newMinimumHeight,
                          int newMaximumWidth,
                          int newMaximumHeight) noexcept;

    /** Can be used to enable or disable user-dragging of the window. */
    void setDraggable (bool shouldBeDraggable) noexcept;

    /** Returns true if the window can be dragged around by the user. */
    bool isDraggable() const noexcept                               { return canDrag; }

    /** Returns the bounds constrainer object that this window is using.
        You can access this to change its properties.
    */
    ComponentBoundsConstrainer* getConstrainer() noexcept           { return constrainer; }

    /** Sets the bounds-constrainer object to use for resizing and dragging this window.

        A pointer to the object you pass in will be kept, but it won't be deleted
        by this object, so it's the caller's responsibility to manage it.

        If you pass a nullptr, then no constraints will be placed on the positioning of the window.
    */
    void setConstrainer (ComponentBoundsConstrainer* newConstrainer);

    /** Calls the window's setBounds method, after first checking these bounds
        with the current constrainer.
        @see setConstrainer
    */
    void setBoundsConstrained (const Rectangle<int>& newBounds);


    //==============================================================================
    /** Returns true if the window is currently in full-screen mode.
        @see setFullScreen
    */
    bool isFullScreen() const;

    /** Puts the window into full-screen mode, or restores it to its normal size.

        If true, the window will become full-screen; if false, it will return to the
        last size it was before being made full-screen.

        @see isFullScreen
    */
    void setFullScreen (bool shouldBeFullScreen);

    /** Returns true if the window is currently minimised.
        @see setMinimised
    */
    bool isMinimised() const;

    /** Minimises the window, or restores it to its previous position and size.

        When being un-minimised, it'll return to the last position and size it
        was in before being minimised.

        @see isMinimised
    */
    void setMinimised (bool shouldMinimise);

    /** Returns true if the window has been placed in kiosk-mode.
        @see Desktop::setKioskComponent
    */
    bool isKioskMode() const;

    //==============================================================================
    /** Returns a string which encodes the window's current size and position.

        This string will encapsulate the window's size, position, whether it's
        in full-screen mode and wether it's visible. It's intended for letting your
        application save and restore a window's position.

        Use the restoreWindowStateFromString() to restore from a saved state.

        @see restoreWindowStateFromString
    */
    String getWindowStateAsString();

    /** Restores the window to a previously-saved size and position.

        This restores the window's size, position, full-screen and visibility status from a
        string that was previously created with the getWindowStateAsString()
        method.
     
        @param forceVisible         if true, then the visibility status from the provided previous
                                    state will be ignored and forced to true.

        @returns false if the string wasn't a valid window state
        @see getWindowStateAsString
    */
    bool restoreWindowStateFromString (const String& previousState, bool forceVisible = false);


    //==============================================================================
    /** Returns the current content component.

        This will be the component set by setContentOwned() or setContentNonOwned, or
        nullptr if none has yet been specified.

        @see setContentOwned, setContentNonOwned
    */
    Component* getContentComponent() const noexcept                 { return contentComponent; }

    /** Changes the current content component.

        This sets a component that will be placed in the centre of the ResizableWindow,
        (leaving a space around the edge for the border).

        You should never add components directly to a ResizableWindow (or any of its subclasses)
        with addChildComponent(). Instead, add them to the content component.

        @param newContentComponent  the new component to use - this component will be deleted when it's
                                    no longer needed (i.e. when the window is deleted or a new content
                                    component is set for it). To set a component that this window will not
                                    delete, call setContentNonOwned() instead.
        @param resizeToFitWhenContentChangesSize  if true, then the ResizableWindow will maintain its size
                                    such that it always fits around the size of the content component. If false,
                                    the new content will be resized to fit the current space available.
    */
    void setContentOwned (Component* newContentComponent,
                          bool resizeToFitWhenContentChangesSize);

    /** Changes the current content component.

        This sets a component that will be placed in the centre of the ResizableWindow,
        (leaving a space around the edge for the border).

        You should never add components directly to a ResizableWindow (or any of its subclasses)
        with addChildComponent(). Instead, add them to the content component.

        @param newContentComponent  the new component to use - this component will NOT be deleted by this
                                    component, so it's the caller's responsibility to manage its lifetime (it's
                                    ok to delete it while this window is still using it). To set a content
                                    component that the window will delete, call setContentOwned() instead.
        @param resizeToFitWhenContentChangesSize  if true, then the ResizableWindow will maintain its size
                                    such that it always fits around the size of the content component. If false,
                                    the new content will be resized to fit the current space available.
    */
    void setContentNonOwned (Component* newContentComponent,
                             bool resizeToFitWhenContentChangesSize);

    /** Removes the current content component.
        If the previous content component was added with setContentOwned(), it will also be deleted. If
        it was added with setContentNonOwned(), it will simply be removed from this component.
    */
    void clearContentComponent();

    /** Changes the window so that the content component ends up with the specified size.

        This is basically a setSize call on the window, but which adds on the borders,
        so you can specify the content component's target size.
    */
    void setContentComponentSize (int width, int height);

    /** Returns the width of the frame to use around the window.
        @see getContentComponentBorder
    */
    virtual BorderSize<int> getBorderThickness();

    /** Returns the insets to use when positioning the content component.
        @see getBorderThickness
    */
    virtual BorderSize<int> getContentComponentBorder();

    //==============================================================================
    /** A set of colour IDs to use to change the colour of various aspects of the window.

        These constants can be used either via the Component::setColour(), or LookAndFeel::setColour()
        methods.

        @see Component::setColour, Component::findColour, LookAndFeel::setColour, LookAndFeel::findColour
    */
    enum ColourIds
    {
        backgroundColourId          = 0x1005700,  /**< A colour to use to fill the window's background. */
    };

    //==============================================================================
    // Deprecated: use setContentOwned() and setContentNonOwned() instead.
    JUCE_DEPRECATED (void setContentComponent (Component* newContentComponent,
                                               bool deleteOldOne = true,
                                               bool resizeToFit = false));
    using TopLevelWindow::addToDesktop;

    //==============================================================================
    /** This abstract base class is implemented by LookAndFeel classes to provide
        window drawing functionality.
    */
    struct JUCE_API  LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        //==============================================================================
        virtual void drawCornerResizer (Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) = 0;
        virtual void drawResizableFrame (Graphics&, int w, int h, const BorderSize<int>&) = 0;

        virtual void fillResizableWindowBackground (Graphics&, int w, int h, const BorderSize<int>&, ResizableWindow&) = 0;
        virtual void drawResizableWindowBorder (Graphics&, int w, int h, const BorderSize<int>& border, ResizableWindow&) = 0;
    };

protected:
    /** @internal */
    void paint (Graphics&) override;
    /** (if overriding this, make sure you call ResizableWindow::moved() in your subclass) */
    void moved() override;
    /** (if overriding this, make sure you call ResizableWindow::resized() in your subclass) */
    void resized() override;
    /** @internal */
    void mouseDown (const MouseEvent&) override;
    /** @internal */
    void mouseDrag (const MouseEvent&) override;
    /** @internal */
    void mouseUp (const MouseEvent&) override;
    /** @internal */
    void lookAndFeelChanged() override;
    /** @internal */
    void childBoundsChanged (Component*) override;
    /** @internal */
    void parentSizeChanged() override;
    /** @internal */
    void visibilityChanged() override;
    /** @internal */
    void activeWindowStatusChanged() override;
    /** @internal */
    int getDesktopWindowStyleFlags() const override;

   #if JUCE_DEBUG
    /** Overridden to warn people about adding components directly to this component
        instead of using setContentOwned().

        If you know what you're doing and are sure you really want to add a component, specify
        a base-class method call to Component::addAndMakeVisible(), to side-step this warning.
    */
    void addChildComponent (Component*, int zOrder = -1);
    /** Overridden to warn people about adding components directly to this component
        instead of using setContentOwned().

        If you know what you're doing and are sure you really want to add a component, specify
        a base-class method call to Component::addAndMakeVisible(), to side-step this warning.
    */
    void addAndMakeVisible (Component*, int zOrder = -1);
   #endif

    std::unique_ptr<ResizableCornerComponent> resizableCorner;
    std::unique_ptr<ResizableBorderComponent> resizableBorder;

private:
    //==============================================================================
    Component::SafePointer<Component> contentComponent, splashScreen;
    bool ownsContentComponent = false, resizeToFitContent = false, fullscreen = false, canDrag = true, dragStarted = false;
    ComponentDragger dragger;
    Rectangle<int> lastNonFullScreenPos;
    ComponentBoundsConstrainer defaultConstrainer;
    ComponentBoundsConstrainer* constrainer = nullptr;
   #if JUCE_DEBUG
    bool hasBeenResized = false;
   #endif

    void initialise (bool addToDesktop);
    void updateLastPosIfNotFullScreen();
    void updateLastPosIfShowing();
    void setContent (Component*, bool takeOwnership, bool resizeToFit);
    void updatePeerConstrainer();

   #if JUCE_CATCH_DEPRECATED_CODE_MISUSE
    // The parameters for these methods have changed - please update your code!
    JUCE_DEPRECATED (void getBorderThickness (int& left, int& top, int& right, int& bottom));
    JUCE_DEPRECATED (void getContentComponentBorder (int& left, int& top, int& right, int& bottom));
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResizableWindow)
};

} // namespace juce
