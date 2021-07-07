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

class Slider::Pimpl   : public AsyncUpdater, // this needs to be public otherwise it will cause an
                                             // error when JUCE_DLL_BUILD=1
                        private Value::Listener
{
public:
    Pimpl (Slider& s, SliderStyle sliderStyle, TextEntryBoxPosition textBoxPosition)
      : owner (s),
        style (sliderStyle),
        textBoxPos (textBoxPosition)
    {
        rotaryParams.startAngleRadians = MathConstants<float>::pi * 1.2f;
        rotaryParams.endAngleRadians   = MathConstants<float>::pi * 2.8f;
        rotaryParams.stopAtEnd = true;
    }

    ~Pimpl() override
    {
        currentValue.removeListener (this);
        valueMin.removeListener (this);
        valueMax.removeListener (this);
        popupDisplay.reset();
    }

    //==============================================================================
    void registerListeners()
    {
        currentValue.addListener (this);
        valueMin.addListener (this);
        valueMax.addListener (this);
    }

    bool isHorizontal() const noexcept
    {
        return style == LinearHorizontal
            || style == LinearBar
            || style == TwoValueHorizontal
            || style == ThreeValueHorizontal;
    }

    bool isVertical() const noexcept
    {
        return style == LinearVertical
            || style == LinearBarVertical
            || style == TwoValueVertical
            || style == ThreeValueVertical;
    }

    bool isRotary() const noexcept
    {
        return style == Rotary
            || style == RotaryHorizontalDrag
            || style == RotaryVerticalDrag
            || style == RotaryHorizontalVerticalDrag;
    }

    bool isBar() const noexcept
    {
        return style == LinearBar
            || style == LinearBarVertical;
    }

    bool isTwoValue() const noexcept
    {
        return style == TwoValueHorizontal
            || style == TwoValueVertical;
    }

    bool isThreeValue() const noexcept
    {
        return style == ThreeValueHorizontal
            || style == ThreeValueVertical;
    }

    bool incDecDragDirectionIsHorizontal() const noexcept
    {
        return incDecButtonMode == incDecButtonsDraggable_Horizontal
                || (incDecButtonMode == incDecButtonsDraggable_AutoDirection && incDecButtonsSideBySide);
    }

    float getPositionOfValue (double value) const
    {
        if (isHorizontal() || isVertical())
            return getLinearSliderPos (value);

        jassertfalse; // not a valid call on a slider that doesn't work linearly!
        return 0.0f;
    }

    void updateRange()
    {
        // figure out the number of DPs needed to display all values at this
        // interval setting.
        numDecimalPlaces = 7;

        if (normRange.interval != 0.0)
        {
            int v = std::abs (roundToInt (normRange.interval * 10000000));

            while ((v % 10) == 0 && numDecimalPlaces > 0)
            {
                --numDecimalPlaces;
                v /= 10;
            }
        }

        // keep the current values inside the new range..
        if (style != TwoValueHorizontal && style != TwoValueVertical)
        {
            setValue (getValue(), dontSendNotification);
        }
        else
        {
            setMinValue (getMinValue(), dontSendNotification, false);
            setMaxValue (getMaxValue(), dontSendNotification, false);
        }

        updateText();
    }

    void setRange (double newMin, double newMax, double newInt)
    {
        normRange = NormalisableRange<double> (newMin, newMax, newInt,
                                               normRange.skew, normRange.symmetricSkew);
        updateRange();
    }

    void setNormalisableRange (NormalisableRange<double> newRange)
    {
        normRange = newRange;
        updateRange();
    }

    double getValue() const
    {
        // for a two-value style slider, you should use the getMinValue() and getMaxValue()
        // methods to get the two values.
        jassert (style != TwoValueHorizontal && style != TwoValueVertical);

        return currentValue.getValue();
    }

    void setValue (double newValue, NotificationType notification)
    {
        // for a two-value style slider, you should use the setMinValue() and setMaxValue()
        // methods to set the two values.
        jassert (style != TwoValueHorizontal && style != TwoValueVertical);

        newValue = constrainedValue (newValue);

        if (style == ThreeValueHorizontal || style == ThreeValueVertical)
        {
            jassert (static_cast<double> (valueMin.getValue()) <= static_cast<double> (valueMax.getValue()));

            newValue = jlimit (static_cast<double> (valueMin.getValue()),
                               static_cast<double> (valueMax.getValue()),
                               newValue);
        }

        if (newValue != lastCurrentValue)
        {
            if (valueBox != nullptr)
                valueBox->hideEditor (true);

            lastCurrentValue = newValue;

            // (need to do this comparison because the Value will use equalsWithSameType to compare
            // the new and old values, so will generate unwanted change events if the type changes)
            if (currentValue != newValue)
                currentValue = newValue;

            updateText();
            owner.repaint();
            updatePopupDisplay (newValue);

            triggerChangeMessage (notification);
        }
    }

    void setMinValue (double newValue, NotificationType notification, bool allowNudgingOfOtherValues)
    {
        // The minimum value only applies to sliders that are in two- or three-value mode.
        jassert (style == TwoValueHorizontal || style == TwoValueVertical
                  || style == ThreeValueHorizontal || style == ThreeValueVertical);

        newValue = constrainedValue (newValue);

        if (style == TwoValueHorizontal || style == TwoValueVertical)
        {
            if (allowNudgingOfOtherValues && newValue > static_cast<double> (valueMax.getValue()))
                setMaxValue (newValue, notification, false);

            newValue = jmin (static_cast<double> (valueMax.getValue()), newValue);
        }
        else
        {
            if (allowNudgingOfOtherValues && newValue > lastCurrentValue)
                setValue (newValue, notification);

            newValue = jmin (lastCurrentValue, newValue);
        }

        if (lastValueMin != newValue)
        {
            lastValueMin = newValue;
            valueMin = newValue;
            owner.repaint();
            updatePopupDisplay (newValue);

            triggerChangeMessage (notification);
        }
    }

    void setMaxValue (double newValue, NotificationType notification, bool allowNudgingOfOtherValues)
    {
        // The maximum value only applies to sliders that are in two- or three-value mode.
        jassert (style == TwoValueHorizontal || style == TwoValueVertical
                  || style == ThreeValueHorizontal || style == ThreeValueVertical);

        newValue = constrainedValue (newValue);

        if (style == TwoValueHorizontal || style == TwoValueVertical)
        {
            if (allowNudgingOfOtherValues && newValue < static_cast<double> (valueMin.getValue()))
                setMinValue (newValue, notification, false);

            newValue = jmax (static_cast<double> (valueMin.getValue()), newValue);
        }
        else
        {
            if (allowNudgingOfOtherValues && newValue < lastCurrentValue)
                setValue (newValue, notification);

            newValue = jmax (lastCurrentValue, newValue);
        }

        if (lastValueMax != newValue)
        {
            lastValueMax = newValue;
            valueMax = newValue;
            owner.repaint();
            updatePopupDisplay (valueMax.getValue());

            triggerChangeMessage (notification);
        }
    }

    void setMinAndMaxValues (double newMinValue, double newMaxValue, NotificationType notification)
    {
        // The maximum value only applies to sliders that are in two- or three-value mode.
        jassert (style == TwoValueHorizontal || style == TwoValueVertical
                  || style == ThreeValueHorizontal || style == ThreeValueVertical);

        if (newMaxValue < newMinValue)
            std::swap (newMaxValue, newMinValue);

        newMinValue = constrainedValue (newMinValue);
        newMaxValue = constrainedValue (newMaxValue);

        if (lastValueMax != newMaxValue || lastValueMin != newMinValue)
        {
            lastValueMax = newMaxValue;
            lastValueMin = newMinValue;
            valueMin = newMinValue;
            valueMax = newMaxValue;
            owner.repaint();

            triggerChangeMessage (notification);
        }
    }

    double getMinValue() const
    {
        // The minimum value only applies to sliders that are in two- or three-value mode.
        jassert (style == TwoValueHorizontal || style == TwoValueVertical
                  || style == ThreeValueHorizontal || style == ThreeValueVertical);

        return valueMin.getValue();
    }

    double getMaxValue() const
    {
        // The maximum value only applies to sliders that are in two- or three-value mode.
        jassert (style == TwoValueHorizontal || style == TwoValueVertical
                  || style == ThreeValueHorizontal || style == ThreeValueVertical);

        return valueMax.getValue();
    }

    void triggerChangeMessage (NotificationType notification)
    {
        if (notification != dontSendNotification)
        {
            owner.valueChanged();

            if (notification == sendNotificationSync)
                handleAsyncUpdate();
            else
                triggerAsyncUpdate();
        }
    }

    void handleAsyncUpdate() override
    {
        cancelPendingUpdate();

        Component::BailOutChecker checker (&owner);
        listeners.callChecked (checker, [&] (Slider::Listener& l) { l.sliderValueChanged (&owner); });

        if (checker.shouldBailOut())
            return;

        if (owner.onValueChange != nullptr)
            owner.onValueChange();

        if (auto* handler = owner.getAccessibilityHandler())
            handler->notifyAccessibilityEvent (AccessibilityEvent::valueChanged);
    }

    void sendDragStart()
    {
        owner.startedDragging();

        Component::BailOutChecker checker (&owner);
        listeners.callChecked (checker, [&] (Slider::Listener& l) { l.sliderDragStarted (&owner); });

        if (checker.shouldBailOut())
            return;

        if (owner.onDragStart != nullptr)
            owner.onDragStart();
    }

    void sendDragEnd()
    {
        owner.stoppedDragging();
        sliderBeingDragged = -1;

        Component::BailOutChecker checker (&owner);
        listeners.callChecked (checker, [&] (Slider::Listener& l) { l.sliderDragEnded (&owner); });

        if (checker.shouldBailOut())
            return;

        if (owner.onDragEnd != nullptr)
            owner.onDragEnd();
    }

    struct DragInProgress
    {
        DragInProgress (Pimpl& p)  : owner (p)      { owner.sendDragStart(); }
        ~DragInProgress()                           { owner.sendDragEnd(); }

        Pimpl& owner;

        JUCE_DECLARE_NON_COPYABLE (DragInProgress)
    };

    void incrementOrDecrement (double delta)
    {
        if (style == IncDecButtons)
        {
            auto newValue = owner.snapValue (getValue() + delta, notDragging);

            if (currentDrag != nullptr)
            {
                setValue (newValue, sendNotificationSync);
            }
            else
            {
                DragInProgress drag (*this);
                setValue (newValue, sendNotificationSync);
            }
        }
    }

    void valueChanged (Value& value) override
    {
        if (value.refersToSameSourceAs (currentValue))
        {
            if (style != TwoValueHorizontal && style != TwoValueVertical)
                setValue (currentValue.getValue(), dontSendNotification);
        }
        else if (value.refersToSameSourceAs (valueMin))
            setMinValue (valueMin.getValue(), dontSendNotification, true);
        else if (value.refersToSameSourceAs (valueMax))
            setMaxValue (valueMax.getValue(), dontSendNotification, true);
    }

    void textChanged()
    {
        auto newValue = owner.snapValue (owner.getValueFromText (valueBox->getText()), notDragging);

        if (newValue != static_cast<double> (currentValue.getValue()))
        {
            DragInProgress drag (*this);
            setValue (newValue, sendNotificationSync);
        }

        updateText(); // force a clean-up of the text, needed in case setValue() hasn't done this.
    }

    void updateText()
    {
        if (valueBox != nullptr)
        {
            auto newValue = owner.getTextFromValue (currentValue.getValue());

            if (newValue != valueBox->getText())
                valueBox->setText (newValue, dontSendNotification);
        }
    }

    double constrainedValue (double value) const
    {
        return normRange.snapToLegalValue (value);
    }

    float getLinearSliderPos (double value) const
    {
        double pos;

        if (normRange.end <= normRange.start)
            pos = 0.5;
        else if (value < normRange.start)
            pos = 0.0;
        else if (value > normRange.end)
            pos = 1.0;
        else
            pos = owner.valueToProportionOfLength (value);

        if (isVertical() || style == IncDecButtons)
            pos = 1.0 - pos;

        jassert (pos >= 0 && pos <= 1.0);
        return (float) (sliderRegionStart + pos * sliderRegionSize);
    }

    void setSliderStyle (SliderStyle newStyle)
    {
        if (style != newStyle)
        {
            style = newStyle;

            owner.repaint();
            owner.lookAndFeelChanged();
            owner.invalidateAccessibilityHandler();
        }
    }

    void setVelocityModeParameters (double sensitivity, int threshold,
                                    double offset, bool userCanPressKeyToSwapMode,
                                    ModifierKeys::Flags newModifierToSwapModes)
    {
        velocityModeSensitivity = sensitivity;
        velocityModeOffset = offset;
        velocityModeThreshold = threshold;
        userKeyOverridesVelocity = userCanPressKeyToSwapMode;
        modifierToSwapModes = newModifierToSwapModes;
    }

    void setIncDecButtonsMode (IncDecButtonMode mode)
    {
        if (incDecButtonMode != mode)
        {
            incDecButtonMode = mode;
            owner.lookAndFeelChanged();
        }
    }

    void setTextBoxStyle (TextEntryBoxPosition newPosition,
                          bool isReadOnly,
                          int textEntryBoxWidth,
                          int textEntryBoxHeight)
    {
        if (textBoxPos != newPosition
             || editableText != (! isReadOnly)
             || textBoxWidth != textEntryBoxWidth
             || textBoxHeight != textEntryBoxHeight)
        {
            textBoxPos = newPosition;
            editableText = ! isReadOnly;
            textBoxWidth = textEntryBoxWidth;
            textBoxHeight = textEntryBoxHeight;

            owner.repaint();
            owner.lookAndFeelChanged();
        }
    }

    void setTextBoxIsEditable (bool shouldBeEditable)
    {
        editableText = shouldBeEditable;
        updateTextBoxEnablement();
    }

    void showTextBox()
    {
        jassert (editableText); // this should probably be avoided in read-only sliders.

        if (valueBox != nullptr)
            valueBox->showEditor();
    }

    void hideTextBox (bool discardCurrentEditorContents)
    {
        if (valueBox != nullptr)
        {
            valueBox->hideEditor (discardCurrentEditorContents);

            if (discardCurrentEditorContents)
                updateText();
        }
    }

    void setTextValueSuffix (const String& suffix)
    {
        if (textSuffix != suffix)
        {
            textSuffix = suffix;
            updateText();
        }
    }

    void updateTextBoxEnablement()
    {
        if (valueBox != nullptr)
        {
            bool shouldBeEditable = editableText && owner.isEnabled();

            if (valueBox->isEditable() != shouldBeEditable) // (to avoid changing the single/double click flags unless we need to)
                valueBox->setEditable (shouldBeEditable);
        }
    }

    void lookAndFeelChanged (LookAndFeel& lf)
    {
        if (textBoxPos != NoTextBox)
        {
            auto previousTextBoxContent = (valueBox != nullptr ? valueBox->getText()
                                                               : owner.getTextFromValue (currentValue.getValue()));

            valueBox.reset();
            valueBox.reset (lf.createSliderTextBox (owner));
            owner.addAndMakeVisible (valueBox.get());

            valueBox->setWantsKeyboardFocus (false);
            valueBox->setAccessible (false);
            valueBox->setText (previousTextBoxContent, dontSendNotification);
            valueBox->setTooltip (owner.getTooltip());
            updateTextBoxEnablement();
            valueBox->onTextChange = [this] { textChanged(); };

            if (style == LinearBar || style == LinearBarVertical)
            {
                valueBox->addMouseListener (&owner, false);
                valueBox->setMouseCursor (MouseCursor::ParentCursor);
            }
        }
        else
        {
            valueBox.reset();
        }

        if (style == IncDecButtons)
        {
            incButton.reset (lf.createSliderButton (owner, true));
            decButton.reset (lf.createSliderButton (owner, false));

            auto tooltip = owner.getTooltip();

            auto setupButton = [&] (Button& b, bool isIncrement)
            {
                owner.addAndMakeVisible (b);
                b.onClick = [this, isIncrement] { incrementOrDecrement (isIncrement ? normRange.interval : -normRange.interval); };

                if (incDecButtonMode != incDecButtonsNotDraggable)
                    b.addMouseListener (&owner, false);
                else
                    b.setRepeatSpeed (300, 100, 20);

                b.setTooltip (tooltip);
                b.setAccessible (false);
            };

            setupButton (*incButton, true);
            setupButton (*decButton, false);
        }
        else
        {
            incButton.reset();
            decButton.reset();
        }

        owner.setComponentEffect (lf.getSliderEffect (owner));

        owner.resized();
        owner.repaint();
    }

    void showPopupMenu()
    {
        PopupMenu m;
        m.setLookAndFeel (&owner.getLookAndFeel());
        m.addItem (1, TRANS ("Velocity-sensitive mode"), true, isVelocityBased);
        m.addSeparator();

        if (isRotary())
        {
            PopupMenu rotaryMenu;
            rotaryMenu.addItem (2, TRANS ("Use circular dragging"),           true, style == Rotary);
            rotaryMenu.addItem (3, TRANS ("Use left-right dragging"),         true, style == RotaryHorizontalDrag);
            rotaryMenu.addItem (4, TRANS ("Use up-down dragging"),            true, style == RotaryVerticalDrag);
            rotaryMenu.addItem (5, TRANS ("Use left-right/up-down dragging"), true, style == RotaryHorizontalVerticalDrag);

            m.addSubMenu (TRANS ("Rotary mode"), rotaryMenu);
        }

        m.showMenuAsync (PopupMenu::Options(),
                         ModalCallbackFunction::forComponent (sliderMenuCallback, &owner));
    }

    static void sliderMenuCallback (int result, Slider* slider)
    {
        if (slider != nullptr)
        {
            switch (result)
            {
                case 1:   slider->setVelocityBasedMode (! slider->getVelocityBasedMode()); break;
                case 2:   slider->setSliderStyle (Rotary); break;
                case 3:   slider->setSliderStyle (RotaryHorizontalDrag); break;
                case 4:   slider->setSliderStyle (RotaryVerticalDrag); break;
                case 5:   slider->setSliderStyle (RotaryHorizontalVerticalDrag); break;
                default:  break;
            }
        }
    }

    int getThumbIndexAt (const MouseEvent& e)
    {
        if (isTwoValue() || isThreeValue())
        {
            auto mousePos = isVertical() ? e.position.y : e.position.x;

            auto normalPosDistance = std::abs (getLinearSliderPos (currentValue.getValue()) - mousePos);
            auto minPosDistance    = std::abs (getLinearSliderPos (valueMin.getValue()) + (isVertical() ? 0.1f : -0.1f) - mousePos);
            auto maxPosDistance    = std::abs (getLinearSliderPos (valueMax.getValue()) + (isVertical() ? -0.1f : 0.1f) - mousePos);

            if (isTwoValue())
                return maxPosDistance <= minPosDistance ? 2 : 1;

            if (normalPosDistance >= minPosDistance && maxPosDistance >= minPosDistance)
                return 1;

            if (normalPosDistance >= maxPosDistance)
                return 2;
        }

        return 0;
    }

    //==============================================================================
    void handleRotaryDrag (const MouseEvent& e)
    {
        auto dx = e.position.x - (float) sliderRect.getCentreX();
        auto dy = e.position.y - (float) sliderRect.getCentreY();

        if (dx * dx + dy * dy > 25.0f)
        {
            auto angle = std::atan2 ((double) dx, (double) -dy);

            while (angle < 0.0)
                angle += MathConstants<double>::twoPi;

            if (rotaryParams.stopAtEnd && e.mouseWasDraggedSinceMouseDown())
            {
                if (std::abs (angle - lastAngle) > MathConstants<double>::pi)
                {
                    if (angle >= lastAngle)
                        angle -= MathConstants<double>::twoPi;
                    else
                        angle += MathConstants<double>::twoPi;
                }

                if (angle >= lastAngle)
                    angle = jmin (angle, (double) jmax (rotaryParams.startAngleRadians, rotaryParams.endAngleRadians));
                else
                    angle = jmax (angle, (double) jmin (rotaryParams.startAngleRadians, rotaryParams.endAngleRadians));
            }
            else
            {
                while (angle < rotaryParams.startAngleRadians)
                    angle += MathConstants<double>::twoPi;

                if (angle > rotaryParams.endAngleRadians)
                {
                    if (smallestAngleBetween (angle, rotaryParams.startAngleRadians)
                         <= smallestAngleBetween (angle, rotaryParams.endAngleRadians))
                        angle = rotaryParams.startAngleRadians;
                    else
                        angle = rotaryParams.endAngleRadians;
                }
            }

            auto proportion = (angle - rotaryParams.startAngleRadians) / (rotaryParams.endAngleRadians - rotaryParams.startAngleRadians);
            valueWhenLastDragged = owner.proportionOfLengthToValue (jlimit (0.0, 1.0, proportion));
            lastAngle = angle;
        }
    }

    void handleAbsoluteDrag (const MouseEvent& e)
    {
        auto mousePos = (isHorizontal() || style == RotaryHorizontalDrag) ? e.position.x : e.position.y;
        double newPos = 0;

        if (style == RotaryHorizontalDrag
            || style == RotaryVerticalDrag
            || style == IncDecButtons
            || ((style == LinearHorizontal || style == LinearVertical || style == LinearBar || style == LinearBarVertical)
                && ! snapsToMousePos))
        {
            auto mouseDiff = (style == RotaryHorizontalDrag
                                || style == LinearHorizontal
                                || style == LinearBar
                                || (style == IncDecButtons && incDecDragDirectionIsHorizontal()))
                              ? e.position.x - mouseDragStartPos.x
                              : mouseDragStartPos.y - e.position.y;

            newPos = owner.valueToProportionOfLength (valueOnMouseDown)
                       + mouseDiff * (1.0 / pixelsForFullDragExtent);

            if (style == IncDecButtons)
            {
                incButton->setState (mouseDiff < 0 ? Button::buttonNormal : Button::buttonDown);
                decButton->setState (mouseDiff > 0 ? Button::buttonNormal : Button::buttonDown);
            }
        }
        else if (style == RotaryHorizontalVerticalDrag)
        {
            auto mouseDiff = (e.position.x - mouseDragStartPos.x)
                               + (mouseDragStartPos.y - e.position.y);

            newPos = owner.valueToProportionOfLength (valueOnMouseDown)
                       + mouseDiff * (1.0 / pixelsForFullDragExtent);
        }
        else
        {
            newPos = (mousePos - (float) sliderRegionStart) / (double) sliderRegionSize;

            if (isVertical())
                newPos = 1.0 - newPos;
        }

        newPos = (isRotary() && ! rotaryParams.stopAtEnd) ? newPos - std::floor (newPos)
                                                          : jlimit (0.0, 1.0, newPos);
        valueWhenLastDragged = owner.proportionOfLengthToValue (newPos);
    }

    void handleVelocityDrag (const MouseEvent& e)
    {
        bool hasHorizontalStyle =
            (isHorizontal() ||  style == RotaryHorizontalDrag
                            || (style == IncDecButtons && incDecDragDirectionIsHorizontal()));

        auto mouseDiff = style == RotaryHorizontalVerticalDrag
                            ? (e.position.x - mousePosWhenLastDragged.x) + (mousePosWhenLastDragged.y - e.position.y)
                            : (hasHorizontalStyle ? e.position.x - mousePosWhenLastDragged.x
                                                  : e.position.y - mousePosWhenLastDragged.y);

        auto maxSpeed = jmax (200.0, (double) sliderRegionSize);
        auto speed = jlimit (0.0, maxSpeed, (double) std::abs (mouseDiff));

        if (speed != 0.0)
        {
            speed = 0.2 * velocityModeSensitivity
                      * (1.0 + std::sin (MathConstants<double>::pi * (1.5 + jmin (0.5, velocityModeOffset
                                                                                         + jmax (0.0, (double) (speed - velocityModeThreshold))
                                                                                            / maxSpeed))));

            if (mouseDiff < 0)
                speed = -speed;

            if (isVertical() || style == RotaryVerticalDrag
                 || (style == IncDecButtons && ! incDecDragDirectionIsHorizontal()))
                speed = -speed;

            auto newPos = owner.valueToProportionOfLength (valueWhenLastDragged) + speed;
            newPos = (isRotary() && ! rotaryParams.stopAtEnd) ? newPos - std::floor (newPos)
                                                              : jlimit (0.0, 1.0, newPos);
            valueWhenLastDragged = owner.proportionOfLengthToValue (newPos);

            e.source.enableUnboundedMouseMovement (true, false);
        }
    }

    void mouseDown (const MouseEvent& e)
    {
        incDecDragged = false;
        useDragEvents = false;
        mouseDragStartPos = mousePosWhenLastDragged = e.position;
        currentDrag.reset();
        popupDisplay.reset();

        if (owner.isEnabled())
        {
            if (e.mods.isPopupMenu() && menuEnabled)
            {
                showPopupMenu();
            }
            else if (canDoubleClickToValue()
                     && (singleClickModifiers != ModifierKeys() && e.mods.withoutMouseButtons() == singleClickModifiers))
            {
                mouseDoubleClick();
            }
            else if (normRange.end > normRange.start)
            {
                useDragEvents = true;

                if (valueBox != nullptr)
                    valueBox->hideEditor (true);

                sliderBeingDragged = getThumbIndexAt (e);

                minMaxDiff = static_cast<double> (valueMax.getValue()) - static_cast<double> (valueMin.getValue());

                if (! isTwoValue())
                    lastAngle = rotaryParams.startAngleRadians
                                    + (rotaryParams.endAngleRadians - rotaryParams.startAngleRadians)
                                         * owner.valueToProportionOfLength (currentValue.getValue());

                valueWhenLastDragged = (sliderBeingDragged == 2 ? valueMax
                                                                : (sliderBeingDragged == 1 ? valueMin
                                                                                           : currentValue)).getValue();
                valueOnMouseDown = valueWhenLastDragged;

                if (showPopupOnDrag || showPopupOnHover)
                {
                    showPopupDisplay();

                    if (popupDisplay != nullptr)
                        popupDisplay->stopTimer();
                }

                currentDrag.reset (new DragInProgress (*this));
                mouseDrag (e);
            }
        }
    }

    void mouseDrag (const MouseEvent& e)
    {
        if (useDragEvents && normRange.end > normRange.start
             && ! ((style == LinearBar || style == LinearBarVertical)
                    && e.mouseWasClicked() && valueBox != nullptr && valueBox->isEditable()))
        {
            DragMode dragMode = notDragging;

            if (style == Rotary)
            {
                handleRotaryDrag (e);
            }
            else
            {
                if (style == IncDecButtons && ! incDecDragged)
                {
                    if (e.getDistanceFromDragStart() < 10 || ! e.mouseWasDraggedSinceMouseDown())
                        return;

                    incDecDragged = true;
                    mouseDragStartPos = e.position;
                }

                if (isAbsoluteDragMode (e.mods) || (normRange.end - normRange.start) / sliderRegionSize < normRange.interval)
                {
                    dragMode = absoluteDrag;
                    handleAbsoluteDrag (e);
                }
                else
                {
                    dragMode = velocityDrag;
                    handleVelocityDrag (e);
                }
            }

            valueWhenLastDragged = jlimit (normRange.start, normRange.end, valueWhenLastDragged);

            if (sliderBeingDragged == 0)
            {
                setValue (owner.snapValue (valueWhenLastDragged, dragMode),
                          sendChangeOnlyOnRelease ? dontSendNotification : sendNotificationSync);
            }
            else if (sliderBeingDragged == 1)
            {
                setMinValue (owner.snapValue (valueWhenLastDragged, dragMode),
                             sendChangeOnlyOnRelease ? dontSendNotification : sendNotificationAsync, true);

                if (e.mods.isShiftDown())
                    setMaxValue (getMinValue() + minMaxDiff, dontSendNotification, true);
                else
                    minMaxDiff = static_cast<double> (valueMax.getValue()) - static_cast<double> (valueMin.getValue());
            }
            else if (sliderBeingDragged == 2)
            {
                setMaxValue (owner.snapValue (valueWhenLastDragged, dragMode),
                             sendChangeOnlyOnRelease ? dontSendNotification : sendNotificationAsync, true);

                if (e.mods.isShiftDown())
                    setMinValue (getMaxValue() - minMaxDiff, dontSendNotification, true);
                else
                    minMaxDiff = static_cast<double> (valueMax.getValue()) - static_cast<double> (valueMin.getValue());
            }

            mousePosWhenLastDragged = e.position;
        }
    }

    void mouseUp()
    {
        if (owner.isEnabled()
             && useDragEvents
             && (normRange.end > normRange.start)
             && (style != IncDecButtons || incDecDragged))
        {
            restoreMouseIfHidden();

            if (sendChangeOnlyOnRelease && valueOnMouseDown != static_cast<double> (currentValue.getValue()))
                triggerChangeMessage (sendNotificationAsync);

            currentDrag.reset();
            popupDisplay.reset();

            if (style == IncDecButtons)
            {
                incButton->setState (Button::buttonNormal);
                decButton->setState (Button::buttonNormal);
            }
        }
        else if (popupDisplay != nullptr)
        {
            popupDisplay->startTimer (200);
        }

        currentDrag.reset();
    }

    void mouseMove()
    {
        // this is a workaround for a bug where the popup display being dismissed triggers
        // a mouse move causing it to never be hidden
        auto shouldShowPopup = showPopupOnHover
                                && (Time::getMillisecondCounterHiRes() - lastPopupDismissal) > 250;

        if (shouldShowPopup
             && ! isTwoValue()
             && ! isThreeValue())
        {
            if (owner.isMouseOver (true))
            {
                if (popupDisplay == nullptr)
                    showPopupDisplay();

                if (popupDisplay != nullptr && popupHoverTimeout != -1)
                    popupDisplay->startTimer (popupHoverTimeout);
            }
        }
    }

    void mouseExit()
    {
        popupDisplay.reset();
    }

    void showPopupDisplay()
    {
        if (style == IncDecButtons)
            return;

        if (popupDisplay == nullptr)
        {
            popupDisplay.reset (new PopupDisplayComponent (owner, parentForPopupDisplay == nullptr));

            if (parentForPopupDisplay != nullptr)
                parentForPopupDisplay->addChildComponent (popupDisplay.get());
            else
                popupDisplay->addToDesktop (ComponentPeer::windowIsTemporary
                                            | ComponentPeer::windowIgnoresKeyPresses
                                            | ComponentPeer::windowIgnoresMouseClicks);

            if (style == SliderStyle::TwoValueHorizontal
                || style == SliderStyle::TwoValueVertical)
            {
                updatePopupDisplay (sliderBeingDragged == 2 ? getMaxValue()
                                                            : getMinValue());
            }
            else
            {
                updatePopupDisplay (getValue());
            }

            popupDisplay->setVisible (true);
        }
    }

    void updatePopupDisplay (double valueToShow)
    {
        if (popupDisplay != nullptr)
            popupDisplay->updatePosition (owner.getTextFromValue (valueToShow));
    }

    bool canDoubleClickToValue() const
    {
        return doubleClickToValue
                && style != IncDecButtons
                && normRange.start <= doubleClickReturnValue
                && normRange.end >= doubleClickReturnValue;
    }

    void mouseDoubleClick()
    {
        if (canDoubleClickToValue())
        {
            DragInProgress drag (*this);
            setValue (doubleClickReturnValue, sendNotificationSync);
        }
    }

    double getMouseWheelDelta (double value, double wheelAmount)
    {
        if (style == IncDecButtons)
            return normRange.interval * wheelAmount;

        auto proportionDelta = wheelAmount * 0.15;
        auto currentPos = owner.valueToProportionOfLength (value);
        auto newPos = currentPos + proportionDelta;
        newPos = (isRotary() && ! rotaryParams.stopAtEnd) ? newPos - std::floor (newPos)
                                                          : jlimit (0.0, 1.0, newPos);
        return owner.proportionOfLengthToValue (newPos) - value;
    }

    bool mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
    {
        if (scrollWheelEnabled
             && style != TwoValueHorizontal
             && style != TwoValueVertical)
        {
            // sometimes duplicate wheel events seem to be sent, so since we're going to
            // bump the value by a minimum of the interval, avoid doing this twice..
            if (e.eventTime != lastMouseWheelTime)
            {
                lastMouseWheelTime = e.eventTime;

                if (normRange.end > normRange.start && ! e.mods.isAnyMouseButtonDown())
                {
                    if (valueBox != nullptr)
                        valueBox->hideEditor (false);

                    auto value = static_cast<double> (currentValue.getValue());
                    auto delta = getMouseWheelDelta (value, (std::abs (wheel.deltaX) > std::abs (wheel.deltaY)
                                                                  ? -wheel.deltaX : wheel.deltaY)
                                                               * (wheel.isReversed ? -1.0f : 1.0f));
                    if (delta != 0.0)
                    {
                        auto newValue = value + jmax (normRange.interval, std::abs (delta)) * (delta < 0 ? -1.0 : 1.0);

                        DragInProgress drag (*this);
                        setValue (owner.snapValue (newValue, notDragging), sendNotificationSync);
                    }
                }
            }

            return true;
        }

        return false;
    }

    void modifierKeysChanged (const ModifierKeys& modifiers)
    {
        if (style != IncDecButtons && style != Rotary && isAbsoluteDragMode (modifiers))
            restoreMouseIfHidden();
    }

    bool isAbsoluteDragMode (ModifierKeys mods) const
    {
        return isVelocityBased == (userKeyOverridesVelocity && mods.testFlags (modifierToSwapModes));
    }

    void restoreMouseIfHidden()
    {
        for (auto& ms : Desktop::getInstance().getMouseSources())
        {
            if (ms.isUnboundedMouseMovementEnabled())
            {
                ms.enableUnboundedMouseMovement (false);

                auto pos = sliderBeingDragged == 2 ? getMaxValue()
                                                   : (sliderBeingDragged == 1 ? getMinValue()
                                                                              : static_cast<double> (currentValue.getValue()));
                Point<float> mousePos;

                if (isRotary())
                {
                    mousePos = ms.getLastMouseDownPosition();

                    auto delta = (float) (pixelsForFullDragExtent * (owner.valueToProportionOfLength (valueOnMouseDown)
                                                                       - owner.valueToProportionOfLength (pos)));

                    if (style == RotaryHorizontalDrag)      mousePos += Point<float> (-delta, 0.0f);
                    else if (style == RotaryVerticalDrag)   mousePos += Point<float> (0.0f, delta);
                    else                                    mousePos += Point<float> (delta / -2.0f, delta / 2.0f);

                    mousePos = owner.getScreenBounds().reduced (4).toFloat().getConstrainedPoint (mousePos);
                    mouseDragStartPos = mousePosWhenLastDragged = owner.getLocalPoint (nullptr, mousePos);
                    valueOnMouseDown = valueWhenLastDragged;
                }
                else
                {
                    auto pixelPos = (float) getLinearSliderPos (pos);

                    mousePos = owner.localPointToGlobal (Point<float> (isHorizontal() ? pixelPos : ((float) owner.getWidth()  / 2.0f),
                                                                       isVertical()   ? pixelPos : ((float) owner.getHeight() / 2.0f)));
                }

                const_cast <MouseInputSource&> (ms).setScreenPosition (mousePos);
            }
        }
    }

    //==============================================================================
    void paint (Graphics& g, LookAndFeel& lf)
    {
        if (style != IncDecButtons)
        {
            if (isRotary())
            {
                auto sliderPos = (float) owner.valueToProportionOfLength (lastCurrentValue);
                jassert (sliderPos >= 0 && sliderPos <= 1.0f);

                lf.drawRotarySlider (g,
                                     sliderRect.getX(), sliderRect.getY(),
                                     sliderRect.getWidth(), sliderRect.getHeight(),
                                     sliderPos, rotaryParams.startAngleRadians,
                                     rotaryParams.endAngleRadians, owner);
            }
            else
            {
                lf.drawLinearSlider (g,
                                     sliderRect.getX(), sliderRect.getY(),
                                     sliderRect.getWidth(), sliderRect.getHeight(),
                                     getLinearSliderPos (lastCurrentValue),
                                     getLinearSliderPos (lastValueMin),
                                     getLinearSliderPos (lastValueMax),
                                     style, owner);
            }

            if ((style == LinearBar || style == LinearBarVertical) && valueBox == nullptr)
            {
                g.setColour (owner.findColour (Slider::textBoxOutlineColourId));
                g.drawRect (0, 0, owner.getWidth(), owner.getHeight(), 1);
            }
        }
    }

    //==============================================================================
    void resized (LookAndFeel& lf)
    {
        auto layout = lf.getSliderLayout (owner);
        sliderRect = layout.sliderBounds;

        if (valueBox != nullptr)
            valueBox->setBounds (layout.textBoxBounds);

        if (isHorizontal())
        {
            sliderRegionStart = layout.sliderBounds.getX();
            sliderRegionSize = layout.sliderBounds.getWidth();
        }
        else if (isVertical())
        {
            sliderRegionStart = layout.sliderBounds.getY();
            sliderRegionSize = layout.sliderBounds.getHeight();
        }
        else if (style == IncDecButtons)
        {
            resizeIncDecButtons();
        }
    }

    //==============================================================================
    void resizeIncDecButtons()
    {
        auto buttonRect = sliderRect;

        if (textBoxPos == TextBoxLeft || textBoxPos == TextBoxRight)
            buttonRect.expand (-2, 0);
        else
            buttonRect.expand (0, -2);

        incDecButtonsSideBySide = buttonRect.getWidth() > buttonRect.getHeight();

        if (incDecButtonsSideBySide)
        {
            decButton->setBounds (buttonRect.removeFromLeft (buttonRect.getWidth() / 2));
            decButton->setConnectedEdges (Button::ConnectedOnRight);
            incButton->setConnectedEdges (Button::ConnectedOnLeft);
        }
        else
        {
            decButton->setBounds (buttonRect.removeFromBottom (buttonRect.getHeight() / 2));
            decButton->setConnectedEdges (Button::ConnectedOnTop);
            incButton->setConnectedEdges (Button::ConnectedOnBottom);
        }

        incButton->setBounds (buttonRect);
    }

    //==============================================================================
    Slider& owner;
    SliderStyle style;

    ListenerList<Slider::Listener> listeners;
    Value currentValue, valueMin, valueMax;
    double lastCurrentValue = 0, lastValueMin = 0, lastValueMax = 0;
    NormalisableRange<double> normRange { 0.0, 10.0 };
    double doubleClickReturnValue = 0;
    double valueWhenLastDragged = 0, valueOnMouseDown = 0, lastAngle = 0;
    double velocityModeSensitivity = 1.0, velocityModeOffset = 0, minMaxDiff = 0;
    int velocityModeThreshold = 1;
    RotaryParameters rotaryParams;
    Point<float> mouseDragStartPos, mousePosWhenLastDragged;
    int sliderRegionStart = 0, sliderRegionSize = 1;
    int sliderBeingDragged = -1;
    int pixelsForFullDragExtent = 250;
    Time lastMouseWheelTime;
    Rectangle<int> sliderRect;
    std::unique_ptr<DragInProgress> currentDrag;

    TextEntryBoxPosition textBoxPos;
    String textSuffix;
    int numDecimalPlaces = 7;
    int textBoxWidth = 80, textBoxHeight = 20;
    IncDecButtonMode incDecButtonMode = incDecButtonsNotDraggable;
    ModifierKeys::Flags modifierToSwapModes = ModifierKeys::ctrlAltCommandModifiers;

    bool editableText = true;
    bool doubleClickToValue = false;
    bool isVelocityBased = false;
    bool userKeyOverridesVelocity = true;
    bool incDecButtonsSideBySide = false;
    bool sendChangeOnlyOnRelease = false;
    bool showPopupOnDrag = false;
    bool showPopupOnHover = false;
    bool menuEnabled = false;
    bool useDragEvents = false;
    bool incDecDragged = false;
    bool scrollWheelEnabled = true;
    bool snapsToMousePos = true;

    int popupHoverTimeout = 2000;
    double lastPopupDismissal = 0.0;

    ModifierKeys singleClickModifiers;

    std::unique_ptr<Label> valueBox;
    std::unique_ptr<Button> incButton, decButton;

    //==============================================================================
    struct PopupDisplayComponent  : public BubbleComponent,
                                    public Timer
    {
        PopupDisplayComponent (Slider& s, bool isOnDesktop)
            : owner (s),
              font (s.getLookAndFeel().getSliderPopupFont (s))
        {
            if (isOnDesktop)
                setTransform (AffineTransform::scale (Component::getApproximateScaleFactorForComponent (&s)));

            setAlwaysOnTop (true);
            setAllowedPlacement (owner.getLookAndFeel().getSliderPopupPlacement (s));
            setLookAndFeel (&s.getLookAndFeel());
        }

        ~PopupDisplayComponent() override
        {
            if (owner.pimpl != nullptr)
                owner.pimpl->lastPopupDismissal = Time::getMillisecondCounterHiRes();
        }

        void paintContent (Graphics& g, int w, int h) override
        {
            g.setFont (font);
            g.setColour (owner.findColour (TooltipWindow::textColourId, true));
            g.drawFittedText (text, Rectangle<int> (w, h), Justification::centred, 1);
        }

        void getContentSize (int& w, int& h) override
        {
            w = font.getStringWidth (text) + 18;
            h = (int) (font.getHeight() * 1.6f);
        }

        void updatePosition (const String& newText)
        {
            text = newText;
            BubbleComponent::setPosition (&owner);
            repaint();
        }

        void timerCallback() override
        {
            stopTimer();
            owner.pimpl->popupDisplay.reset();
        }

    private:
        //==============================================================================
        Slider& owner;
        Font font;
        String text;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopupDisplayComponent)
    };

    std::unique_ptr<PopupDisplayComponent> popupDisplay;
    Component* parentForPopupDisplay = nullptr;

    //==============================================================================
    static double smallestAngleBetween (double a1, double a2) noexcept
    {
        return jmin (std::abs (a1 - a2),
                     std::abs (a1 + MathConstants<double>::twoPi - a2),
                     std::abs (a2 + MathConstants<double>::twoPi - a1));
    }
};


//==============================================================================
Slider::Slider()
{
    init (LinearHorizontal, TextBoxLeft);
}

Slider::Slider (const String& name)  : Component (name)
{
    init (LinearHorizontal, TextBoxLeft);
}

Slider::Slider (SliderStyle style, TextEntryBoxPosition textBoxPos)
{
    init (style, textBoxPos);
}

void Slider::init (SliderStyle style, TextEntryBoxPosition textBoxPos)
{
    setWantsKeyboardFocus (false);
    setRepaintsOnMouseActivity (true);

    pimpl.reset (new Pimpl (*this, style, textBoxPos));

    Slider::lookAndFeelChanged();
    updateText();

    pimpl->registerListeners();
}

Slider::~Slider() {}

//==============================================================================
void Slider::addListener (Listener* l)       { pimpl->listeners.add (l); }
void Slider::removeListener (Listener* l)    { pimpl->listeners.remove (l); }

//==============================================================================
Slider::SliderStyle Slider::getSliderStyle() const noexcept     { return pimpl->style; }
void Slider::setSliderStyle (SliderStyle newStyle)              { pimpl->setSliderStyle (newStyle); }

void Slider::setRotaryParameters (RotaryParameters p) noexcept
{
    // make sure the values are sensible..
    jassert (p.startAngleRadians >= 0 && p.endAngleRadians >= 0);
    jassert (p.startAngleRadians < MathConstants<float>::pi * 4.0f
              && p.endAngleRadians < MathConstants<float>::pi * 4.0f);

    pimpl->rotaryParams = p;
}

void Slider::setRotaryParameters (float startAngleRadians, float endAngleRadians, bool stopAtEnd) noexcept
{
    setRotaryParameters ({ startAngleRadians, endAngleRadians, stopAtEnd });
}

Slider::RotaryParameters Slider::getRotaryParameters() const noexcept
{
    return pimpl->rotaryParams;
}

void Slider::setVelocityBasedMode (bool vb)                 { pimpl->isVelocityBased = vb; }
bool Slider::getVelocityBasedMode() const noexcept          { return pimpl->isVelocityBased; }
bool Slider::getVelocityModeIsSwappable() const noexcept    { return pimpl->userKeyOverridesVelocity; }
int Slider::getVelocityThreshold() const noexcept           { return pimpl->velocityModeThreshold; }
double Slider::getVelocitySensitivity() const noexcept      { return pimpl->velocityModeSensitivity; }
double Slider::getVelocityOffset() const noexcept           { return pimpl->velocityModeOffset; }

void Slider::setVelocityModeParameters (double sensitivity, int threshold,
                                        double offset, bool userCanPressKeyToSwapMode,
                                        ModifierKeys::Flags modifierToSwapModes)
{
    jassert (threshold >= 0);
    jassert (sensitivity > 0);
    jassert (offset >= 0);

    pimpl->setVelocityModeParameters (sensitivity, threshold, offset,
                                      userCanPressKeyToSwapMode, modifierToSwapModes);
}

double Slider::getSkewFactor() const noexcept               { return pimpl->normRange.skew; }
bool Slider::isSymmetricSkew() const noexcept               { return pimpl->normRange.symmetricSkew; }

void Slider::setSkewFactor (double factor, bool symmetricSkew)
{
    pimpl->normRange.skew = factor;
    pimpl->normRange.symmetricSkew = symmetricSkew;
}

void Slider::setSkewFactorFromMidPoint (double sliderValueToShowAtMidPoint)
{
    pimpl->normRange.setSkewForCentre (sliderValueToShowAtMidPoint);
}

int Slider::getMouseDragSensitivity() const noexcept        { return pimpl->pixelsForFullDragExtent; }

void Slider::setMouseDragSensitivity (int distanceForFullScaleDrag)
{
    jassert (distanceForFullScaleDrag > 0);

    pimpl->pixelsForFullDragExtent = distanceForFullScaleDrag;
}

void Slider::setIncDecButtonsMode (IncDecButtonMode mode)                   { pimpl->setIncDecButtonsMode (mode); }

Slider::TextEntryBoxPosition Slider::getTextBoxPosition() const noexcept    { return pimpl->textBoxPos; }
int Slider::getTextBoxWidth() const noexcept                                { return pimpl->textBoxWidth; }
int Slider::getTextBoxHeight() const noexcept                               { return pimpl->textBoxHeight; }

void Slider::setTextBoxStyle (TextEntryBoxPosition newPosition, bool isReadOnly, int textEntryBoxWidth, int textEntryBoxHeight)
{
    pimpl->setTextBoxStyle (newPosition, isReadOnly, textEntryBoxWidth, textEntryBoxHeight);
}

bool Slider::isTextBoxEditable() const noexcept                     { return pimpl->editableText; }
void Slider::setTextBoxIsEditable (const bool shouldBeEditable)     { pimpl->setTextBoxIsEditable (shouldBeEditable); }
void Slider::showTextBox()                                          { pimpl->showTextBox(); }
void Slider::hideTextBox (bool discardCurrentEditorContents)        { pimpl->hideTextBox (discardCurrentEditorContents); }

void Slider::setChangeNotificationOnlyOnRelease (bool onlyNotifyOnRelease)
{
    pimpl->sendChangeOnlyOnRelease = onlyNotifyOnRelease;
}

bool Slider::getSliderSnapsToMousePosition() const noexcept           { return pimpl->snapsToMousePos; }
void Slider::setSliderSnapsToMousePosition (bool shouldSnapToMouse)   { pimpl->snapsToMousePos = shouldSnapToMouse; }

void Slider::setPopupDisplayEnabled (bool showOnDrag, bool showOnHover, Component* parent, int hoverTimeout)
{
    pimpl->showPopupOnDrag = showOnDrag;
    pimpl->showPopupOnHover = showOnHover;
    pimpl->parentForPopupDisplay = parent;
    pimpl->popupHoverTimeout = hoverTimeout;
}

Component* Slider::getCurrentPopupDisplay() const noexcept      { return pimpl->popupDisplay.get(); }

//==============================================================================
void Slider::colourChanged()        { lookAndFeelChanged(); }
void Slider::lookAndFeelChanged()   { pimpl->lookAndFeelChanged (getLookAndFeel()); }
void Slider::enablementChanged()    { repaint(); pimpl->updateTextBoxEnablement(); }

//==============================================================================
Range<double> Slider::getRange() const noexcept  { return { pimpl->normRange.start, pimpl->normRange.end }; }
double Slider::getMaximum() const noexcept       { return pimpl->normRange.end; }
double Slider::getMinimum() const noexcept       { return pimpl->normRange.start; }
double Slider::getInterval() const noexcept      { return pimpl->normRange.interval; }

void Slider::setRange (double newMin, double newMax, double newInt)      { pimpl->setRange (newMin, newMax, newInt); }
void Slider::setRange (Range<double> newRange, double newInt)            { pimpl->setRange (newRange.getStart(), newRange.getEnd(), newInt); }
void Slider::setNormalisableRange (NormalisableRange<double> newRange)   { pimpl->setNormalisableRange (newRange); }
NormalisableRange<double> Slider::getNormalisableRange() const noexcept  { return pimpl->normRange; }

double Slider::getValue() const                  { return pimpl->getValue(); }
Value& Slider::getValueObject() noexcept         { return pimpl->currentValue; }
Value& Slider::getMinValueObject() noexcept      { return pimpl->valueMin; }
Value& Slider::getMaxValueObject() noexcept      { return pimpl->valueMax; }

void Slider::setValue (double newValue, NotificationType notification)
{
    pimpl->setValue (newValue, notification);
}

double Slider::getMinValue() const      { return pimpl->getMinValue(); }
double Slider::getMaxValue() const      { return pimpl->getMaxValue(); }

void Slider::setMinValue (double newValue, NotificationType notification, bool allowNudgingOfOtherValues)
{
    pimpl->setMinValue (newValue, notification, allowNudgingOfOtherValues);
}

void Slider::setMaxValue (double newValue, NotificationType notification, bool allowNudgingOfOtherValues)
{
    pimpl->setMaxValue (newValue, notification, allowNudgingOfOtherValues);
}

void Slider::setMinAndMaxValues (double newMinValue, double newMaxValue, NotificationType notification)
{
    pimpl->setMinAndMaxValues (newMinValue, newMaxValue, notification);
}

void Slider::setDoubleClickReturnValue (bool isDoubleClickEnabled,  double valueToSetOnDoubleClick, ModifierKeys mods)
{
    pimpl->doubleClickToValue = isDoubleClickEnabled;
    pimpl->doubleClickReturnValue = valueToSetOnDoubleClick;
    pimpl->singleClickModifiers = mods;
}

double Slider::getDoubleClickReturnValue() const noexcept       { return pimpl->doubleClickReturnValue; }
bool Slider::isDoubleClickReturnEnabled() const noexcept        { return pimpl->doubleClickToValue; }

void Slider::updateText()
{
    pimpl->updateText();
}

void Slider::setTextValueSuffix (const String& suffix)
{
    pimpl->setTextValueSuffix (suffix);
}

String Slider::getTextValueSuffix() const
{
    return pimpl->textSuffix;
}

String Slider::getTextFromValue (double v)
{
    auto getText = [this] (double val)
    {
        if (textFromValueFunction != nullptr)
            return textFromValueFunction (val);

        if (getNumDecimalPlacesToDisplay() > 0)
            return String (val, getNumDecimalPlacesToDisplay());

        return String (roundToInt (val));
    };

    return getText (v) + getTextValueSuffix();
}

double Slider::getValueFromText (const String& text)
{
    auto t = text.trimStart();

    if (t.endsWith (getTextValueSuffix()))
        t = t.substring (0, t.length() - getTextValueSuffix().length());

    if (valueFromTextFunction != nullptr)
        return valueFromTextFunction (t);

    while (t.startsWithChar ('+'))
        t = t.substring (1).trimStart();

    return t.initialSectionContainingOnly ("0123456789.,-")
            .getDoubleValue();
}

double Slider::proportionOfLengthToValue (double proportion)
{
    if (proportionOfLengthToValueFunction)
        return proportionOfLengthToValueFunction (proportion);

    return pimpl->normRange.convertFrom0to1 (proportion);
}

double Slider::valueToProportionOfLength (double value)
{
    if (valueToProportionOfLengthFunction)
        valueToProportionOfLengthFunction (value);

    return pimpl->normRange.convertTo0to1 (value);
}

double Slider::snapValue (double attemptedValue, DragMode)
{
    return attemptedValue;
}

int Slider::getNumDecimalPlacesToDisplay() const noexcept   { return pimpl->numDecimalPlaces; }

void Slider::setNumDecimalPlacesToDisplay (int decimalPlacesToDisplay)
{
    pimpl->numDecimalPlaces = decimalPlacesToDisplay;
    updateText();
}

//==============================================================================
int Slider::getThumbBeingDragged() const noexcept           { return pimpl->sliderBeingDragged; }
void Slider::startedDragging() {}
void Slider::stoppedDragging() {}
void Slider::valueChanged() {}

//==============================================================================
void Slider::setPopupMenuEnabled (bool menuEnabled)         { pimpl->menuEnabled = menuEnabled; }
void Slider::setScrollWheelEnabled (bool enabled)           { pimpl->scrollWheelEnabled = enabled; }

bool Slider::isHorizontal() const noexcept                  { return pimpl->isHorizontal(); }
bool Slider::isVertical() const noexcept                    { return pimpl->isVertical(); }
bool Slider::isRotary() const noexcept                      { return pimpl->isRotary(); }
bool Slider::isBar() const noexcept                         { return pimpl->isBar(); }
bool Slider::isTwoValue() const noexcept                    { return pimpl->isTwoValue(); }
bool Slider::isThreeValue() const noexcept                  { return pimpl->isThreeValue(); }

float Slider::getPositionOfValue (double value) const       { return pimpl->getPositionOfValue (value); }

//==============================================================================
void Slider::paint (Graphics& g)        { pimpl->paint (g, getLookAndFeel()); }
void Slider::resized()                  { pimpl->resized (getLookAndFeel()); }

void Slider::focusOfChildComponentChanged (FocusChangeType)     { repaint(); }

void Slider::mouseDown (const MouseEvent& e)    { pimpl->mouseDown (e); }
void Slider::mouseUp   (const MouseEvent&)      { pimpl->mouseUp(); }
void Slider::mouseMove (const MouseEvent&)      { pimpl->mouseMove(); }
void Slider::mouseExit (const MouseEvent&)      { pimpl->mouseExit(); }

// If popup display is enabled and set to show on mouse hover, this makes sure
// it is shown when dragging the mouse over a slider and releasing
void Slider::mouseEnter (const MouseEvent&)     { pimpl->mouseMove(); }

void Slider::modifierKeysChanged (const ModifierKeys& modifiers)
{
    if (isEnabled())
        pimpl->modifierKeysChanged (modifiers);
}

void Slider::mouseDrag (const MouseEvent& e)
{
    if (isEnabled())
        pimpl->mouseDrag (e);
}

void Slider::mouseDoubleClick (const MouseEvent&)
{
    if (isEnabled())
        pimpl->mouseDoubleClick();
}

void Slider::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (! (isEnabled() && pimpl->mouseWheelMove (e, wheel)))
        Component::mouseWheelMove (e, wheel);
}

//==============================================================================
class SliderAccessibilityHandler  : public AccessibilityHandler
{
public:
    explicit SliderAccessibilityHandler (Slider& sliderToWrap)
        : AccessibilityHandler (sliderToWrap,
                                AccessibilityRole::slider,
                                AccessibilityActions{},
                                AccessibilityHandler::Interfaces { std::make_unique<ValueInterface> (sliderToWrap) }),
          slider (sliderToWrap)
    {
    }

    String getHelp() const override   { return slider.getTooltip(); }

private:
    class ValueInterface  : public AccessibilityValueInterface
    {
    public:
        explicit ValueInterface (Slider& sliderToWrap)
            : slider (sliderToWrap),
              valueToControl (slider.isTwoValue() ? slider.getMaxValueObject() : slider.getValueObject())
        {
        }

        bool isReadOnly() const override                         { return false; }

        double getCurrentValue() const override                  { return valueToControl.getValue(); }
        void setValue (double newValue) override                 { valueToControl = newValue; }

        String getCurrentValueAsString() const override          { return slider.getTextFromValue (getCurrentValue()); }
        void setValueAsString (const String& newValue) override  { setValue (slider.getValueFromText (newValue)); }

        AccessibleValueRange getRange() const override
        {
            return { { slider.getMinimum(), slider.getMaximum() },
                     getStepSize() };
        }

    private:
        double getStepSize() const
        {
            auto interval = slider.getInterval();

            return interval != 0.0 ? interval
                                   : slider.getRange().getLength() * 0.01;
        }

        Slider& slider;
        Value valueToControl;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueInterface)
    };

    Slider& slider;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliderAccessibilityHandler)
};

std::unique_ptr<AccessibilityHandler> Slider::createAccessibilityHandler()
{
    return std::make_unique<SliderAccessibilityHandler> (*this);
}

} // namespace juce
