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
    A component that displays a text string, and can optionally become a text
    editor when clicked.

    @tags{GUI}
*/
class JUCE_API  Label  : public Component,
                         public SettableTooltipClient,
                         protected TextEditor::Listener,
                         private ComponentListener,
                         private Value::Listener
{
public:
    //==============================================================================
    /** Creates a Label.

        @param componentName    the name to give the component
        @param labelText        the text to show in the label
    */
    Label (const String& componentName = String(),
           const String& labelText = String());

    /** Destructor. */
    ~Label() override;

    //==============================================================================
    /** Changes the label text.

        The NotificationType parameter indicates whether to send a change message to
        any Label::Listener objects if the new text is different.
    */
    void setText (const String& newText,
                  NotificationType notification);

    /** Returns the label's current text.

        @param returnActiveEditorContents   if this is true and the label is currently
                                            being edited, then this method will return the
                                            text as it's being shown in the editor. If false,
                                            then the value returned here won't be updated until
                                            the user has finished typing and pressed the return
                                            key.
    */
    String getText (bool returnActiveEditorContents = false) const;

    /** Returns the text content as a Value object.
        You can call Value::referTo() on this object to make the label read and control
        a Value object that you supply.
    */
    Value& getTextValue() noexcept                          { return textValue; }

    //==============================================================================
    /** Changes the font to use to draw the text.
        @see getFont
    */
    void setFont (const Font& newFont);

    /** Returns the font currently being used.
        This may be the one set by setFont(), unless it has been overridden by the current LookAndFeel
        @see setFont
    */
    Font getFont() const noexcept;

    //==============================================================================
    /** A set of colour IDs to use to change the colour of various aspects of the label.

        These constants can be used either via the Component::setColour(), or LookAndFeel::setColour()
        methods.

        Note that you can also use the constants from TextEditor::ColourIds to change the
        colour of the text editor that is opened when a label is editable.

        @see Component::setColour, Component::findColour, LookAndFeel::setColour, LookAndFeel::findColour
    */
    enum ColourIds
    {
        backgroundColourId                = 0x1000280, /**< The background colour to fill the label with. */
        textColourId                      = 0x1000281, /**< The colour for the text. */
        outlineColourId                   = 0x1000282, /**< An optional colour to use to draw a border around the label.
                                                         Leave this transparent to not have an outline. */
        backgroundWhenEditingColourId     = 0x1000283, /**< The background colour when the label is being edited. */
        textWhenEditingColourId           = 0x1000284, /**< The colour for the text when the label is being edited. */
        outlineWhenEditingColourId        = 0x1000285, /**< An optional border colour when the label is being edited. */
        highlightWhenEditingColourId      = 0x1000286, /**< The highlight colour when the label is being edited. */
        highlightTextWhenEditingColourId  = 0x1000287, /**<  The highlighted text colour when the label is being edited. */
        focusedOutlineWhenEditingColourId = 0x1000288, /**< Colour for drawing a box around the edge of the component when it has focus when the label is being edited. */
        shadowColourIdWhenEditingColourId = 0x1000289, /**< Colour for drawing an inner shadow around the edge of the editor when the label is being edited. */
    };

    //==============================================================================
    /** Sets the style of justification to be used for positioning the text.
        (The default is Justification::centredLeft)
    */
    void setJustificationType (Justification justification);

    /** Returns the type of justification, as set in setJustificationType(). */
    Justification getJustificationType() const noexcept                         { return justification; }

    /** Changes the border that is left between the edge of the component and the text.
        By default there's a small gap left at the sides of the component to allow for
        the drawing of the border, but you can change this if necessary.
    */
    void setBorderSize (BorderSize<int> newBorderSize);

    /** Returns the size of the border to be left around the text. */
    BorderSize<int> getBorderSize() const noexcept                              { return border; }

    /** Makes this label "stick to" another component.

        This will cause the label to follow another component around, staying
        either to its left or above it.

        @param owner    the component to follow
        @param onLeft   if true, the label will stay on the left of its component; if
                        false, it will stay above it.
    */
    void attachToComponent (Component* owner, bool onLeft);

    /** If this label has been attached to another component using attachToComponent, this
        returns the other component.

        Returns nullptr if the label is not attached.
    */
    Component* getAttachedComponent() const;

    /** If the label is attached to the left of another component, this returns true.

        Returns false if the label is above the other component. This is only relevant if
        attachToComponent() has been called.
    */
    bool isAttachedOnLeft() const noexcept                                      { return leftOfOwnerComp; }

    /** Specifies the minimum amount that the font can be squashed horizontally before it starts
        using ellipsis. Use a value of 0 for a default value.

        @see Graphics::drawFittedText
    */
    void setMinimumHorizontalScale (float newScale);

    /** Specifies the amount that the font can be squashed horizontally. */
    float getMinimumHorizontalScale() const noexcept                            { return minimumHorizontalScale; }

    /** Set a keyboard type for use when the text editor is shown. */
    void setKeyboardType (TextInputTarget::VirtualKeyboardType type) noexcept   { keyboardType = type; }

    //==============================================================================
    /**
        A class for receiving events from a Label.

        You can register a Label::Listener with a Label using the Label::addListener()
        method, and it will be called when the text of the label changes, either because
        of a call to Label::setText() or by the user editing the text (if the label is
        editable).

        @see Label::addListener, Label::removeListener
    */
    class JUCE_API  Listener
    {
    public:
        /** Destructor. */
        virtual ~Listener() = default;

        /** Called when a Label's text has changed. */
        virtual void labelTextChanged (Label* labelThatHasChanged) = 0;

        /** Called when a Label goes into editing mode and displays a TextEditor. */
        virtual void editorShown (Label*, TextEditor&) {}

        /** Called when a Label is about to delete its TextEditor and exit editing mode. */
        virtual void editorHidden (Label*, TextEditor&) {}
    };

    /** Registers a listener that will be called when the label's text changes. */
    void addListener (Listener* listener);

    /** Deregisters a previously-registered listener. */
    void removeListener (Listener* listener);

    //==============================================================================
    /** You can assign a lambda to this callback object to have it called when the label text is changed. */
    std::function<void()> onTextChange;

    /** You can assign a lambda to this callback object to have it called when the label's editor is shown. */
    std::function<void()> onEditorShow;

    /** You can assign a lambda to this callback object to have it called when the label's editor is hidden. */
    std::function<void()> onEditorHide;

    //==============================================================================
    /** Makes the label turn into a TextEditor when clicked.

        By default this is turned off.

        If turned on, then single- or double-clicking will turn the label into
        an editor. If the user then changes the text, then the ChangeBroadcaster
        base class will be used to send change messages to any listeners that
        have registered.

        If the user changes the text, the textWasEdited() method will be called
        afterwards, and subclasses can override this if they need to do anything
        special.

        @param editOnSingleClick            if true, just clicking once on the label will start editing the text
        @param editOnDoubleClick            if true, a double-click is needed to start editing
        @param lossOfFocusDiscardsChanges   if true, clicking somewhere else while the text is being
                                            edited will discard any changes; if false, then this will
                                            commit the changes.
        @see showEditor, setEditorColours, TextEditor
    */
    void setEditable (bool editOnSingleClick,
                      bool editOnDoubleClick = false,
                      bool lossOfFocusDiscardsChanges = false);

    /** Returns true if this option was set using setEditable(). */
    bool isEditableOnSingleClick() const noexcept                       { return editSingleClick; }

    /** Returns true if this option was set using setEditable(). */
    bool isEditableOnDoubleClick() const noexcept                       { return editDoubleClick; }

    /** Returns true if this option has been set in a call to setEditable(). */
    bool doesLossOfFocusDiscardChanges() const noexcept                 { return lossOfFocusDiscardsChanges; }

    /** Returns true if the user can edit this label's text. */
    bool isEditable() const noexcept                                    { return editSingleClick || editDoubleClick; }

    /** Makes the editor appear as if the label had been clicked by the user.
        @see textWasEdited, setEditable
    */
    void showEditor();

    /** Hides the editor if it was being shown.

        @param discardCurrentEditorContents     if true, the label's text will be
                                                reset to whatever it was before the editor
                                                was shown; if false, the current contents of the
                                                editor will be used to set the label's text
                                                before it is hidden.
    */
    void hideEditor (bool discardCurrentEditorContents);

    /** Returns true if the editor is currently focused and active. */
    bool isBeingEdited() const noexcept;

    /** Returns the currently-visible text editor, or nullptr if none is open. */
    TextEditor* getCurrentTextEditor() const noexcept;

    //==============================================================================
    /** This abstract base class is implemented by LookAndFeel classes to provide
        label drawing functionality.
    */
    struct JUCE_API  LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawLabel (Graphics&, Label&) = 0;
        virtual Font getLabelFont (Label&) = 0;
        virtual BorderSize<int> getLabelBorderSize (Label&) = 0;
    };

protected:
    //==============================================================================
    /** Creates the TextEditor component that will be used when the user has clicked on the label.
        Subclasses can override this if they need to customise this component in some way.
    */
    virtual TextEditor* createEditorComponent();

    /** Called after the user changes the text. */
    virtual void textWasEdited();

    /** Called when the text has been altered. */
    virtual void textWasChanged();

    /** Called when the text editor has just appeared, due to a user click or other focus change. */
    virtual void editorShown (TextEditor*);

    /** Called when the text editor is going to be deleted, after editing has finished. */
    virtual void editorAboutToBeHidden (TextEditor*);

    //==============================================================================
    /** @internal */
    void paint (Graphics&) override;
    /** @internal */
    void resized() override;
    /** @internal */
    void mouseUp (const MouseEvent&) override;
    /** @internal */
    void mouseDoubleClick (const MouseEvent&) override;
    /** @internal */
    void componentMovedOrResized (Component&, bool wasMoved, bool wasResized) override;
    /** @internal */
    void componentParentHierarchyChanged (Component&) override;
    /** @internal */
    void componentVisibilityChanged (Component&) override;
    /** @internal */
    void inputAttemptWhenModal() override;
    /** @internal */
    void focusGained (FocusChangeType) override;
    /** @internal */
    void enablementChanged() override;
    /** @internal */
    std::unique_ptr<ComponentTraverser> createKeyboardFocusTraverser() override;
    /** @internal */
    void textEditorTextChanged (TextEditor&) override;
    /** @internal */
    void textEditorReturnKeyPressed (TextEditor&) override;
    /** @internal */
    void textEditorEscapeKeyPressed (TextEditor&) override;
    /** @internal */
    void textEditorFocusLost (TextEditor&) override;
    /** @internal */
    void colourChanged() override;
    /** @internal */
    void valueChanged (Value&) override;
    /** @internal */
    void callChangeListeners();
    /** @internal */
    std::unique_ptr<AccessibilityHandler> createAccessibilityHandler() override;

private:
    //==============================================================================
    Value textValue;
    String lastTextValue;
    Font font { 15.0f };
    Justification justification = Justification::centredLeft;
    std::unique_ptr<TextEditor> editor;
    ListenerList<Listener> listeners;
    WeakReference<Component> ownerComponent;
    BorderSize<int> border { 1, 5, 1, 5 };
    float minimumHorizontalScale = 0;
    TextInputTarget::VirtualKeyboardType keyboardType = TextInputTarget::textKeyboard;
    bool editSingleClick = false;
    bool editDoubleClick = false;
    bool lossOfFocusDiscardsChanges = false;
    bool leftOfOwnerComp = false;

    bool updateFromTextEditorContents (TextEditor&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Label)
};


} // namespace juce
