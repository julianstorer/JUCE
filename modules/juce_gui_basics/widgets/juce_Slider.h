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
    A slider control for changing a value.

    The slider can be horizontal, vertical, or rotary, and can optionally have
    a text-box inside it to show an editable display of the current value.

    To use it, create a Slider object and use the setSliderStyle() method
    to set up the type you want. To set up the text-entry box, use setTextBoxStyle().

    To define the values that it can be set to, see the setRange() and setValue() methods.

    There are also lots of custom tweaks you can do by subclassing and overriding
    some of the virtual methods, such as changing the scaling, changing the format of
    the text display, custom ways of limiting the values, etc.

    You can register Slider::Listener objects with a slider, and they'll be called when
    the value changes.

    @see Slider::Listener

    @tags{GUI}
*/
class JUCE_API  Slider  : public Component,
                          public SettableTooltipClient
{
public:
    //==============================================================================
    /** The types of slider available.

        @see setSliderStyle, setRotaryParameters
    */
    enum SliderStyle
    {
        LinearHorizontal,               /**< A traditional horizontal slider. */
        LinearVertical,                 /**< A traditional vertical slider. */
        LinearBar,                      /**< A horizontal bar slider with the text label drawn on top of it. */
        LinearBarVertical,              /**< A vertical bar slider with the text label drawn on top of it. */
        Rotary,                         /**< A rotary control that you move by dragging the mouse in a circular motion, like a knob.
                                             @see setRotaryParameters */
        RotaryHorizontalDrag,           /**< A rotary control that you move by dragging the mouse left-to-right.
                                             @see setRotaryParameters */
        RotaryVerticalDrag,             /**< A rotary control that you move by dragging the mouse up-and-down.
                                             @see setRotaryParameters */
        RotaryHorizontalVerticalDrag,   /**< A rotary control that you move by dragging the mouse up-and-down or left-to-right.
                                             @see setRotaryParameters */
        IncDecButtons,                  /**< A pair of buttons that increment or decrement the slider's value by the increment set in setRange(). */

        TwoValueHorizontal,             /**< A horizontal slider that has two thumbs instead of one, so it can show a minimum and maximum value.
                                             @see setMinValue, setMaxValue */
        TwoValueVertical,               /**< A vertical slider that has two thumbs instead of one, so it can show a minimum and maximum value.
                                             @see setMinValue, setMaxValue */

        ThreeValueHorizontal,           /**< A horizontal slider that has three thumbs instead of one, so it can show a minimum and maximum
                                             value, with the current value being somewhere between them.
                                             @see setMinValue, setMaxValue */
        ThreeValueVertical              /**< A vertical slider that has three thumbs instead of one, so it can show a minimum and maximum
                                             value, with the current value being somewhere between them.
                                             @see setMinValue, setMaxValue */
    };

    /** The position of the slider's text-entry box.
        @see setTextBoxStyle
    */
    enum TextEntryBoxPosition
    {
        NoTextBox,              /**< Doesn't display a text box.  */
        TextBoxLeft,            /**< Puts the text box to the left of the slider, vertically centred.  */
        TextBoxRight,           /**< Puts the text box to the right of the slider, vertically centred.  */
        TextBoxAbove,           /**< Puts the text box above the slider, horizontally centred.  */
        TextBoxBelow            /**< Puts the text box below the slider, horizontally centred.  */
    };

    /** Describes the type of mouse-dragging that is happening when a value is being changed.
        @see snapValue
     */
    enum DragMode
    {
        notDragging,            /**< Dragging is not active.  */
        absoluteDrag,           /**< The dragging corresponds directly to the value that is displayed.  */
        velocityDrag            /**< The dragging value change is relative to the velocity of the mouse movement.  */
    };

    //==============================================================================
    /** Creates a slider.
        When created, you can set up the slider's style and range with setSliderStyle(), setRange(), etc.
    */
    Slider();

    /** Creates a slider.
        When created, you can set up the slider's style and range with setSliderStyle(), setRange(), etc.
    */
    explicit Slider (const String& componentName);

    /** Creates a slider with some explicit options. */
    Slider (SliderStyle style, TextEntryBoxPosition textBoxPosition);

    /** Destructor. */
    ~Slider() override;

    //==============================================================================
    /** Changes the type of slider interface being used.

        @param newStyle         the type of interface
        @see setRotaryParameters, setVelocityBasedMode
    */
    void setSliderStyle (SliderStyle newStyle);

    /** Returns the slider's current style.
        @see setSliderStyle
    */
    SliderStyle getSliderStyle() const noexcept;

    //==============================================================================
    /** Structure defining rotary parameters for a slider */
    struct RotaryParameters
    {
        /** The angle (in radians, clockwise from the top) at which
            the slider's minimum value is represented.
        */
        float startAngleRadians;

        /** The angle (in radians, clockwise from the top) at which
            the slider's maximum value is represented. This must be
            greater than startAngleRadians.
        */
        float endAngleRadians;

        /** Determines what happens when a circular drag action rotates beyond
            the minimum or maximum angle. If true, the value will stop changing
            until the mouse moves back the way it came; if false, the value
            will snap back to the value nearest to the mouse. Note that this has
            no effect if the drag mode is vertical or horizontal.
        */
        bool stopAtEnd;
    };

    /** Changes the properties of a rotary slider. */
    void setRotaryParameters (RotaryParameters newParameters) noexcept;

    /** Changes the properties of a rotary slider. */
    void setRotaryParameters (float startAngleRadians,
                              float endAngleRadians,
                              bool stopAtEnd) noexcept;

    /** Changes the properties of a rotary slider. */
    RotaryParameters getRotaryParameters() const noexcept;

    /** Sets the distance the mouse has to move to drag the slider across
        the full extent of its range.

        This only applies when in modes like RotaryHorizontalDrag, where it's using
        relative mouse movements to adjust the slider.
    */
    void setMouseDragSensitivity (int distanceForFullScaleDrag);

    /** Returns the current sensitivity value set by setMouseDragSensitivity(). */
    int getMouseDragSensitivity() const noexcept;

    //==============================================================================
    /** Changes the way the mouse is used when dragging the slider.

        If true, this will turn on velocity-sensitive dragging, so that
        the faster the mouse moves, the bigger the movement to the slider. This
        helps when making accurate adjustments if the slider's range is quite large.

        If false, the slider will just try to snap to wherever the mouse is.
    */
    void setVelocityBasedMode (bool isVelocityBased);

    /** Returns true if velocity-based mode is active.
        @see setVelocityBasedMode
    */
    bool getVelocityBasedMode() const noexcept;

    /** Changes aspects of the scaling used when in velocity-sensitive mode.

        These apply when you've used setVelocityBasedMode() to turn on velocity mode,
        or if you're holding down ctrl.

        @param sensitivity      higher values than 1.0 increase the range of acceleration used
        @param threshold        the minimum number of pixels that the mouse needs to move for it
                                to be treated as a movement
        @param offset           values greater than 0.0 increase the minimum speed that will be used when
                                the threshold is reached
        @param userCanPressKeyToSwapMode    if true, then the user can hold down the ctrl or command
                                key to toggle velocity-sensitive mode
        @param modifiersToSwapModes  this is a set of modifier flags which will be tested when determining
                                whether to enable/disable velocity-sensitive mode
    */
    void setVelocityModeParameters (double sensitivity = 1.0,
                                    int threshold = 1,
                                    double offset = 0.0,
                                    bool userCanPressKeyToSwapMode = true,
                                    ModifierKeys::Flags modifiersToSwapModes = ModifierKeys::ctrlAltCommandModifiers);

    /** Returns the velocity sensitivity setting.
        @see setVelocityModeParameters
    */
    double getVelocitySensitivity() const noexcept;

    /** Returns the velocity threshold setting.
        @see setVelocityModeParameters
    */
    int getVelocityThreshold() const noexcept;

    /** Returns the velocity offset setting.
        @see setVelocityModeParameters
    */
    double getVelocityOffset() const noexcept;

    /** Returns the velocity user key setting.
        @see setVelocityModeParameters
    */
    bool getVelocityModeIsSwappable() const noexcept;

    //==============================================================================
    /** Sets up a skew factor to alter the way values are distributed.

        You may want to use a range of values on the slider where more accuracy
        is required towards one end of the range, so this will logarithmically
        spread the values across the length of the slider.

        If the factor is < 1.0, the lower end of the range will fill more of the
        slider's length; if the factor is > 1.0, the upper end of the range
        will be expanded instead. A factor of 1.0 doesn't skew it at all.

        If symmetricSkew is true, the skew factor applies from the middle of the slider
        to each of its ends.

        To set the skew position by using a mid-point, use the setSkewFactorFromMidPoint()
        method instead.

        @see getSkewFactor, setSkewFactorFromMidPoint, isSymmetricSkew
    */
    void setSkewFactor (double factor, bool symmetricSkew = false);

    /** Sets up a skew factor to alter the way values are distributed.

        This allows you to specify the slider value that should appear in the
        centre of the slider's visible range.

        @see setSkewFactor, getSkewFactor, isSymmetricSkew
     */
    void setSkewFactorFromMidPoint (double sliderValueToShowAtMidPoint);

    /** Returns the current skew factor.
        See setSkewFactor for more info.
        @see setSkewFactor, setSkewFactorFromMidPoint, isSymmetricSkew
    */
    double getSkewFactor() const noexcept;

    /** Returns the whether the skew is symmetric from the midpoint to both sides.
        See setSkewFactor for more info.
        @see getSkewFactor, setSkewFactor, setSkewFactorFromMidPoint
     */
    bool isSymmetricSkew() const noexcept;

    //==============================================================================
    /** Used by setIncDecButtonsMode().
    */
    enum IncDecButtonMode
    {
        incDecButtonsNotDraggable,
        incDecButtonsDraggable_AutoDirection,
        incDecButtonsDraggable_Horizontal,
        incDecButtonsDraggable_Vertical
    };

    /** When the style is IncDecButtons, this lets you turn on a mode where the mouse
        can be dragged on the buttons to drag the values.

        By default this is turned off. When enabled, clicking on the buttons still works
        them as normal, but by holding down the mouse on a button and dragging it a little
        distance, it flips into a mode where the value can be dragged. The drag direction can
        either be set explicitly to be vertical or horizontal, or can be set to
        incDecButtonsDraggable_AutoDirection so that it depends on whether the buttons
        are side-by-side or above each other.
    */
    void setIncDecButtonsMode (IncDecButtonMode mode);

    //==============================================================================
    /** Changes the location and properties of the text-entry box.

        @param newPosition          where it should go (or NoTextBox to not have one at all)
        @param isReadOnly           if true, it's a read-only display
        @param textEntryBoxWidth    the width of the text-box in pixels. Make sure this leaves enough
                                    room for the slider as well!
        @param textEntryBoxHeight   the height of the text-box in pixels. Make sure this leaves enough
                                    room for the slider as well!

        @see setTextBoxIsEditable, getValueFromText, getTextFromValue
    */
    void setTextBoxStyle (TextEntryBoxPosition newPosition,
                          bool isReadOnly,
                          int textEntryBoxWidth,
                          int textEntryBoxHeight);

    /** Returns the status of the text-box.
        @see setTextBoxStyle
    */
    TextEntryBoxPosition getTextBoxPosition() const noexcept;

    /** Returns the width used for the text-box.
        @see setTextBoxStyle
    */
    int getTextBoxWidth() const noexcept;

    /** Returns the height used for the text-box.
        @see setTextBoxStyle
    */
    int getTextBoxHeight() const noexcept;

    /** Makes the text-box editable.

        By default this is true, and the user can enter values into the textbox,
        but it can be turned off if that's not suitable.

        @see setTextBoxStyle, getValueFromText, getTextFromValue
    */
    void setTextBoxIsEditable (bool shouldBeEditable);

    /** Returns true if the text-box is read-only.
        @see setTextBoxStyle
    */
    bool isTextBoxEditable() const noexcept;

    /** If the text-box is editable, this will give it the focus so that the user can
        type directly into it.
        This is basically the effect as the user clicking on it.
    */
    void showTextBox();

    /** If the text-box currently has focus and is being edited, this resets it and takes keyboard
        focus away from it.

        @param discardCurrentEditorContents     if true, the slider's value will be left
                                                unchanged; if false, the current contents of the
                                                text editor will be used to set the slider position
                                                before it is hidden.
    */
    void hideTextBox (bool discardCurrentEditorContents);


    //==============================================================================
    /** Changes the slider's current value.

        This will trigger a callback to Slider::Listener::sliderValueChanged() for any listeners
        that are registered, and will synchronously call the valueChanged() method in case subclasses
        want to handle it.

        @param newValue         the new value to set - this will be restricted by the
                                minimum and maximum range, and will be snapped to the
                                nearest interval if one has been set
        @param notification     can be one of the NotificationType values, to request
                                a synchronous or asynchronous call to the valueChanged() method
                                of any Slider::Listeners that are registered. A notification will
                                only be sent if the Slider's value has changed.
    */
    void setValue (double newValue, NotificationType notification = sendNotificationAsync);

    /** Returns the slider's current value. */
    double getValue() const;

    /** Returns the Value object that represents the slider's current position.
        You can use this Value object to connect the slider's position to external values or setters,
        either by taking a copy of the Value, or by using Value::referTo() to make it point to
        your own Value object.
        @see Value, getMaxValue, getMinValueObject
    */
    Value& getValueObject() noexcept;

    //==============================================================================
    /** Sets the limits that the slider's value can take.

        @param newMinimum   the lowest value allowed
        @param newMaximum   the highest value allowed
        @param newInterval  the steps in which the value is allowed to increase - if this
                            is not zero, the value will always be (newMinimum + (newInterval * an integer)).
    */
    void setRange (double newMinimum,
                   double newMaximum,
                   double newInterval = 0);

    /** Sets the limits that the slider's value can take.

        @param newRange     the range to allow
        @param newInterval  the steps in which the value is allowed to increase - if this
                            is not zero, the value will always be (newMinimum + (newInterval * an integer)).
    */
    void setRange (Range<double> newRange, double newInterval);

    /** Sets a NormalisableRange to use for the Slider values.

        @param newNormalisableRange     the NormalisableRange to use
    */
    void setNormalisableRange (NormalisableRange<double> newNormalisableRange);

    /** Returns the NormalisableRange used for the Slider values.

        @see setNormalisableRange
     */
    NormalisableRange<double> getNormalisableRange() const noexcept;

    /** Returns the slider's range. */
    Range<double> getRange() const noexcept;

    /** Returns the current maximum value.
        @see setRange, getRange
    */
    double getMaximum() const noexcept;

    /** Returns the current minimum value.
        @see setRange, getRange
    */
    double getMinimum() const noexcept;

    /** Returns the current step-size for values.
        @see setRange, getRange
    */
    double getInterval() const noexcept;

    //==============================================================================
    /** For a slider with two or three thumbs, this returns the lower of its values.

        For a two-value slider, the values are controlled with getMinValue() and getMaxValue().
        A slider with three values also uses the normal getValue() and setValue() methods to
        control the middle value.

        @see setMinValue, getMaxValue, TwoValueHorizontal, TwoValueVertical, ThreeValueHorizontal, ThreeValueVertical
    */
    double getMinValue() const;

    /** For a slider with two or three thumbs, this returns the lower of its values.
        You can use this Value object to connect the slider's position to external values or setters,
        either by taking a copy of the Value, or by using Value::referTo() to make it point to
        your own Value object.
        @see Value, getMinValue, getMaxValueObject
    */
    Value& getMinValueObject() noexcept;

    /** For a slider with two or three thumbs, this sets the lower of its values.

        This will trigger a callback to Slider::Listener::sliderValueChanged() for any listeners
        that are registered, and will synchronously call the valueChanged() method in case subclasses
        want to handle it.

        @param newValue         the new value to set - this will be restricted by the
                                minimum and maximum range, and will be snapped to the nearest
                                interval if one has been set.
        @param notification     can be one of the NotificationType values, to request
                                a synchronous or asynchronous call to the valueChanged() method
                                of any Slider::Listeners that are registered. A notification will
                                only be sent if this value has changed.
        @param allowNudgingOfOtherValues  if false, this value will be restricted to being below the
                                        max value (in a two-value slider) or the mid value (in a three-value
                                        slider). If true, then if this value goes beyond those values,
                                        it will push them along with it.
        @see getMinValue, setMaxValue, setValue
    */
    void setMinValue (double newValue,
                      NotificationType notification = sendNotificationAsync,
                      bool allowNudgingOfOtherValues = false);

    /** For a slider with two or three thumbs, this returns the higher of its values.

        For a two-value slider, the values are controlled with getMinValue() and getMaxValue().
        A slider with three values also uses the normal getValue() and setValue() methods to
        control the middle value.

        @see getMinValue, TwoValueHorizontal, TwoValueVertical, ThreeValueHorizontal, ThreeValueVertical
    */
    double getMaxValue() const;

    /** For a slider with two or three thumbs, this returns the higher of its values.
        You can use this Value object to connect the slider's position to external values or setters,
        either by taking a copy of the Value, or by using Value::referTo() to make it point to
        your own Value object.
        @see Value, getMaxValue, getMinValueObject
    */
    Value& getMaxValueObject() noexcept;

    /** For a slider with two or three thumbs, this sets the lower of its values.

        This will trigger a callback to Slider::Listener::sliderValueChanged() for any listeners
        that are registered, and will synchronously call the valueChanged() method in case subclasses
        want to handle it.

        @param newValue         the new value to set - this will be restricted by the
                                minimum and maximum range, and will be snapped to the nearest
                                interval if one has been set.
        @param notification     can be one of the NotificationType values, to request
                                a synchronous or asynchronous call to the valueChanged() method
                                of any Slider::Listeners that are registered. A notification will
                                only be sent if this value has changed.
        @param allowNudgingOfOtherValues  if false, this value will be restricted to being above the
                                        min value (in a two-value slider) or the mid value (in a three-value
                                        slider). If true, then if this value goes beyond those values,
                                        it will push them along with it.
        @see getMaxValue, setMinValue, setValue
    */
    void setMaxValue (double newValue,
                      NotificationType notification = sendNotificationAsync,
                      bool allowNudgingOfOtherValues = false);

    /** For a slider with two or three thumbs, this sets the minimum and maximum thumb positions.

        This will trigger a callback to Slider::Listener::sliderValueChanged() for any listeners
        that are registered, and will synchronously call the valueChanged() method in case subclasses
        want to handle it.

        @param newMinValue      the new minimum value to set - this will be snapped to the
                                nearest interval if one has been set.
        @param newMaxValue      the new minimum value to set - this will be snapped to the
                                nearest interval if one has been set.
        @param notification     can be one of the NotificationType values, to request
                                a synchronous or asynchronous call to the valueChanged() method
                                of any Slider::Listeners that are registered. A notification will
                                only be sent if one or more of the values has changed.
        @see setMaxValue, setMinValue, setValue
    */
    void setMinAndMaxValues (double newMinValue, double newMaxValue,
                             NotificationType notification = sendNotificationAsync);

    //==============================================================================
    /** A class for receiving callbacks from a Slider.

        To be told when a slider's value changes, you can register a Slider::Listener
        object using Slider::addListener().

        @see Slider::addListener, Slider::removeListener
    */
    class JUCE_API  Listener
    {
    public:
        //==============================================================================
        /** Destructor. */
        virtual ~Listener() = default;

        //==============================================================================
        /** Called when the slider's value is changed.

            This may be caused by dragging it, or by typing in its text entry box,
            or by a call to Slider::setValue().

            You can find out the new value using Slider::getValue().

            @see Slider::valueChanged
        */
        virtual void sliderValueChanged (Slider* slider) = 0;

        //==============================================================================
        /** Called when the slider is about to be dragged.

            This is called when a drag begins, then it's followed by multiple calls
            to sliderValueChanged(), and then sliderDragEnded() is called after the
            user lets go.

            @see sliderDragEnded, Slider::startedDragging
        */
        virtual void sliderDragStarted (Slider*) {}

        /** Called after a drag operation has finished.
            @see sliderDragStarted, Slider::stoppedDragging
        */
        virtual void sliderDragEnded (Slider*) {}
    };

    /** Adds a listener to be called when this slider's value changes. */
    void addListener (Listener* listener);

    /** Removes a previously-registered listener. */
    void removeListener (Listener* listener);

    //==============================================================================
    /** You can assign a lambda to this callback object to have it called when the slider value is changed. */
    std::function<void()> onValueChange;

    /** You can assign a lambda to this callback object to have it called when the slider's drag begins. */
    std::function<void()> onDragStart;

    /** You can assign a lambda to this callback object to have it called when the slider's drag ends. */
    std::function<void()> onDragEnd;

    /** You can assign a lambda that will be used to convert textual values to the slider's normalised position. */
    std::function<double (const String&)> valueFromTextFunction;

    /** You can assign a lambda that will be used to convert the slider's normalised position to a textual value. */
    std::function<String (double)> textFromValueFunction;

    /** You can assign a lambda that will be used to convert the slider's proportion of the thumb to the value. */
    std::function<double (double)> proportionOfLengthToValueFunction;

    /** You can assign a lambda that will be used to convert the slider's value to the proportion place of the thumb. */
    std::function<double (double)> valueToProportionOfLengthFunction;

    //==============================================================================
    /** This lets you choose whether double-clicking or single-clicking with a specified
        key modifier moves the slider to a given position.

        By default this is turned off, but it's handy if you want either of these actions
        to act as a quick way of resetting a slider. Just pass in the value you want it to
        go to when double-clicked. By default the key modifier is the alt key but you can
        pass in another key modifier, or none to disable this behaviour.

        @see getDoubleClickReturnValue
    */
    void setDoubleClickReturnValue (bool shouldDoubleClickBeEnabled,
                                    double valueToSetOnDoubleClick,
                                    ModifierKeys singleClickModifiers = ModifierKeys::altModifier);

    /** Returns the values last set by setDoubleClickReturnValue() method.
        @see setDoubleClickReturnValue
    */
    double getDoubleClickReturnValue() const noexcept;

    /** Returns true if double-clicking to reset to a default value is enabled.
        @see setDoubleClickReturnValue
    */
    bool isDoubleClickReturnEnabled() const noexcept;

    //==============================================================================
    /** Tells the slider whether to keep sending change messages while the user
        is dragging the slider.

        If set to true, a change message will only be sent when the user has
        dragged the slider and let go. If set to false (the default), then messages
        will be continuously sent as they drag it while the mouse button is still
        held down.
    */
    void setChangeNotificationOnlyOnRelease (bool onlyNotifyOnRelease);

    /** This lets you change whether the slider thumb jumps to the mouse position
        when you click.

        By default, this is true. If it's false, then the slider moves with relative
        motion when you drag it.

        This only applies to linear bars, and won't affect two- or three- value
        sliders.
    */
    void setSliderSnapsToMousePosition (bool shouldSnapToMouse);

    /** Returns true if setSliderSnapsToMousePosition() has been enabled. */
    bool getSliderSnapsToMousePosition() const noexcept;

    /** If enabled, this gives the slider a pop-up bubble which appears while the
        slider is being dragged or hovered-over.

        This can be handy if your slider doesn't have a text-box, so that users can
        see the value just when they're changing it.

        If you pass a component as the parentComponentToUse parameter, the pop-up
        bubble will be added as a child of that component when it's needed. If you
        pass nullptr, the pop-up will be placed on the desktop instead (note that it's a
        transparent window, so if you're using an OS that can't do transparent windows
        you'll have to add it to a parent component instead).

        By default the popup display shown when hovering will remain visible for 2 seconds,
        but it is possible to change this by passing a different hoverTimeout value. A
        value of -1 will cause the popup to remain until a mouseExit() occurs on the slider.
    */
    void setPopupDisplayEnabled (bool shouldShowOnMouseDrag,
                                 bool shouldShowOnMouseHover,
                                 Component* parentComponentToUse,
                                 int hoverTimeout = 2000);

    /** If a popup display is enabled and is currently visible, this returns the component
        that is being shown, or nullptr if none is currently in use.
        @see setPopupDisplayEnabled
    */
    Component* getCurrentPopupDisplay() const noexcept;

    /** If this is set to true, then right-clicking on the slider will pop-up
        a menu to let the user change the way it works.

        By default this is turned off, but when turned on, the menu will include
        things like velocity sensitivity, and for rotary sliders, whether they
        use a linear or rotary mouse-drag to move them.
    */
    void setPopupMenuEnabled (bool menuEnabled);

    /** This can be used to stop the mouse scroll-wheel from moving the slider.
        By default it's enabled.
    */
    void setScrollWheelEnabled (bool enabled);

    /** Returns a number to indicate which thumb is currently being dragged by the mouse.

        This will return 0 for the main thumb, 1 for the minimum-value thumb, 2 for
        the maximum-value thumb, or -1 if none is currently down.
    */
    int getThumbBeingDragged() const noexcept;

    //==============================================================================
    /** Callback to indicate that the user is about to start dragging the slider.
        @see Slider::Listener::sliderDragStarted
    */
    virtual void startedDragging();

    /** Callback to indicate that the user has just stopped dragging the slider.
        @see Slider::Listener::sliderDragEnded
    */
    virtual void stoppedDragging();

    /** Callback to indicate that the user has just moved the slider.
        @see Slider::Listener::sliderValueChanged
    */
    virtual void valueChanged();

    //==============================================================================
    /** Subclasses can override this to convert a text string to a value.

        When the user enters something into the text-entry box, this method is
        called to convert it to a value.
        The default implementation just tries to convert it to a double.

        @see getTextFromValue
    */
    virtual double getValueFromText (const String& text);

    /** Turns the slider's current value into a text string.

        Subclasses can override this to customise the formatting of the text-entry box.

        The default implementation just turns the value into a string, using
        a number of decimal places based on the range interval. If a suffix string
        has been set using setTextValueSuffix(), this will be appended to the text.

        @see getValueFromText
    */
    virtual String getTextFromValue (double value);

    /** Sets a suffix to append to the end of the numeric value when it's displayed as
        a string.

        This is used by the default implementation of getTextFromValue(), and is just
        appended to the numeric value. For more advanced formatting, you can override
        getTextFromValue() and do something else.
    */
    void setTextValueSuffix (const String& suffix);

    /** Returns the suffix that was set by setTextValueSuffix(). */
    String getTextValueSuffix() const;

    /** Returns the best number of decimal places to use when displaying this
        slider's value.
        It calculates the fewest decimal places needed to represent numbers with
        the slider's interval setting.

        @see setNumDecimalPlacesToDisplay
    */
    int getNumDecimalPlacesToDisplay() const noexcept;

    /** Modifies the best number of decimal places to use when displaying this
        slider's value.

        @see getNumDecimalPlacesToDisplay
    */
    void setNumDecimalPlacesToDisplay (int decimalPlacesToDisplay);

    //==============================================================================
    /** Allows a user-defined mapping of distance along the slider to its value.

        The default implementation for this performs the skewing operation that
        can be set up in the setSkewFactor() method. Override it if you need
        some kind of custom mapping instead, but make sure you also implement the
        inverse function in valueToProportionOfLength().

        @param proportion       a value 0 to 1.0, indicating a distance along the slider
        @returns                the slider value that is represented by this position
        @see valueToProportionOfLength
    */
    virtual double proportionOfLengthToValue (double proportion);

    /** Allows a user-defined mapping of value to the position of the slider along its length.

        The default implementation for this performs the skewing operation that
        can be set up in the setSkewFactor() method. Override it if you need
        some kind of custom mapping instead, but make sure you also implement the
        inverse function in proportionOfLengthToValue().

        @param value            a valid slider value, between the range of values specified in
                                setRange()
        @returns                a value 0 to 1.0 indicating the distance along the slider that
                                represents this value
        @see proportionOfLengthToValue
    */
    virtual double valueToProportionOfLength (double value);

    /** Returns the X or Y coordinate of a value along the slider's length.

        If the slider is horizontal, this will be the X coordinate of the given
        value, relative to the left of the slider. If it's vertical, then this will
        be the Y coordinate, relative to the top of the slider.

        If the slider is rotary, this will throw an assertion and return 0. If the
        value is out-of-range, it will be constrained to the length of the slider.
    */
    float getPositionOfValue (double value) const;

    //==============================================================================
    /** This can be overridden to allow the slider to snap to user-definable values.

        If overridden, it will be called when the user tries to move the slider to
        a given position, and allows a subclass to sanity-check this value, possibly
        returning a different value to use instead.

        @param attemptedValue   the value the user is trying to enter
        @param dragMode         indicates whether the user is dragging with
                                the mouse; notDragging if they are entering the value
                                using the text box or other non-dragging interaction
        @returns                the value to use instead
    */
    virtual double snapValue (double attemptedValue, DragMode dragMode);


    //==============================================================================
    /** This can be called to force the text box to update its contents.
        (Not normally needed, as this is done automatically).
    */
    void updateText();

    /** True if the slider moves horizontally. */
    bool isHorizontal() const noexcept;
    /** True if the slider moves vertically. */
    bool isVertical() const noexcept;
    /** True if the slider is in a rotary mode. */
    bool isRotary() const noexcept;
    /** True if the slider is in a linear bar mode. */
    bool isBar() const noexcept;
    /** True if the slider has two thumbs. */
    bool isTwoValue() const noexcept;
    /** True if the slider has three thumbs. */
    bool isThreeValue() const noexcept;

    //==============================================================================
    /** A set of colour IDs to use to change the colour of various aspects of the slider.

        These constants can be used either via the Component::setColour(), or LookAndFeel::setColour()
        methods.

        @see Component::setColour, Component::findColour, LookAndFeel::setColour, LookAndFeel::findColour
    */
    enum ColourIds
    {
        backgroundColourId          = 0x1001200,  /**< A colour to use to fill the slider's background. */
        thumbColourId               = 0x1001300,  /**< The colour to draw the thumb with. It's up to the look
                                                       and feel class how this is used. */
        trackColourId               = 0x1001310,  /**< The colour to draw the groove that the thumb moves along. */
        rotarySliderFillColourId    = 0x1001311,  /**< For rotary sliders, this colour fills the outer curve. */
        rotarySliderOutlineColourId = 0x1001312,  /**< For rotary sliders, this colour is used to draw the outer curve's outline. */

        textBoxTextColourId         = 0x1001400,  /**< The colour for the text in the text-editor box used for editing the value. */
        textBoxBackgroundColourId   = 0x1001500,  /**< The background colour for the text-editor box. */
        textBoxHighlightColourId    = 0x1001600,  /**< The text highlight colour for the text-editor box. */
        textBoxOutlineColourId      = 0x1001700   /**< The colour to use for a border around the text-editor box. */
    };

    //==============================================================================
    /** A struct defining the placement of the slider area and the text box area
        relative to the bounds of the whole Slider component.
     */
    struct SliderLayout
    {
        Rectangle<int> sliderBounds;
        Rectangle<int> textBoxBounds;
    };

    //==============================================================================
    /** This abstract base class is implemented by LookAndFeel classes to provide
        slider drawing functionality.
    */
    struct JUCE_API  LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        //==============================================================================
        virtual void drawLinearSlider (Graphics&,
                                       int x, int y, int width, int height,
                                       float sliderPos,
                                       float minSliderPos,
                                       float maxSliderPos,
                                       const Slider::SliderStyle,
                                       Slider&) = 0;

        virtual void drawLinearSliderBackground (Graphics&,
                                                 int x, int y, int width, int height,
                                                 float sliderPos,
                                                 float minSliderPos,
                                                 float maxSliderPos,
                                                 const Slider::SliderStyle style,
                                                 Slider&) = 0;

        virtual void drawLinearSliderThumb (Graphics&,
                                            int x, int y, int width, int height,
                                            float sliderPos,
                                            float minSliderPos,
                                            float maxSliderPos,
                                            const Slider::SliderStyle,
                                            Slider&) = 0;

        virtual int getSliderThumbRadius (Slider&) = 0;

        virtual void drawRotarySlider (Graphics&,
                                       int x, int y, int width, int height,
                                       float sliderPosProportional,
                                       float rotaryStartAngle,
                                       float rotaryEndAngle,
                                       Slider&) = 0;

        virtual Button* createSliderButton (Slider&, bool isIncrement) = 0;
        virtual Label* createSliderTextBox (Slider&) = 0;

        virtual ImageEffectFilter* getSliderEffect (Slider&) = 0;

        virtual Font getSliderPopupFont (Slider&) = 0;
        virtual int getSliderPopupPlacement (Slider&) = 0;

        virtual SliderLayout getSliderLayout (Slider&) = 0;
    };

    //==============================================================================
    /** @internal */
    void paint (Graphics&) override;
    /** @internal */
    void resized() override;
    /** @internal */
    void mouseDown (const MouseEvent&) override;
    /** @internal */
    void mouseUp (const MouseEvent&) override;
    /** @internal */
    void mouseDrag (const MouseEvent&) override;
    /** @internal */
    void mouseDoubleClick (const MouseEvent&) override;
    /** @internal */
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails&) override;
    /** @internal */
    void modifierKeysChanged (const ModifierKeys&) override;
    /** @internal */
    void lookAndFeelChanged() override;
    /** @internal */
    void enablementChanged() override;
    /** @internal */
    void focusOfChildComponentChanged (FocusChangeType) override;
    /** @internal */
    void colourChanged() override;
    /** @internal */
    void mouseMove (const MouseEvent&) override;
    /** @internal */
    void mouseExit (const MouseEvent&) override;
    /** @internal */
    void mouseEnter (const MouseEvent&) override;
    /** @internal */
    std::unique_ptr<AccessibilityHandler> createAccessibilityHandler() override;

private:
    //==============================================================================
    JUCE_PUBLIC_IN_DLL_BUILD (class Pimpl)
    std::unique_ptr<Pimpl> pimpl;

    void init (SliderStyle, TextEntryBoxPosition);

   #if JUCE_CATCH_DEPRECATED_CODE_MISUSE
    // These methods' bool parameters have changed: see the new method signature.
    JUCE_DEPRECATED (void setValue (double, bool));
    JUCE_DEPRECATED (void setValue (double, bool, bool));
    JUCE_DEPRECATED (void setMinValue (double, bool, bool, bool));
    JUCE_DEPRECATED (void setMinValue (double, bool, bool));
    JUCE_DEPRECATED (void setMinValue (double, bool));
    JUCE_DEPRECATED (void setMaxValue (double, bool, bool, bool));
    JUCE_DEPRECATED (void setMaxValue (double, bool, bool));
    JUCE_DEPRECATED (void setMaxValue (double, bool));
    JUCE_DEPRECATED (void setMinAndMaxValues (double, double, bool, bool));
    JUCE_DEPRECATED (void setMinAndMaxValues (double, double, bool));
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Slider)
};


} // namespace juce
