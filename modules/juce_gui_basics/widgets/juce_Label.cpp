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

Label::Label (const String& name, const String& labelText)
    : Component (name),
      textValue (labelText),
      lastTextValue (labelText)
{
    setColour (TextEditor::textColourId, Colours::black);
    setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);

    textValue.addListener (this);
}

Label::~Label()
{
    textValue.removeListener (this);

    if (ownerComponent != nullptr)
        ownerComponent->removeComponentListener (this);

    editor.reset();
}

//==============================================================================
void Label::setText (const String& newText, NotificationType notification)
{
    hideEditor (true);

    if (lastTextValue != newText)
    {
        lastTextValue = newText;
        textValue = newText;
        repaint();

        textWasChanged();

        if (ownerComponent != nullptr)
            componentMovedOrResized (*ownerComponent, true, true);

        if (notification != dontSendNotification)
            callChangeListeners();
    }
}

String Label::getText (bool returnActiveEditorContents) const
{
    return (returnActiveEditorContents && isBeingEdited())
                ? editor->getText()
                : textValue.toString();
}

void Label::valueChanged (Value&)
{
    if (lastTextValue != textValue.toString())
        setText (textValue.toString(), sendNotification);
}

//==============================================================================
void Label::setFont (const Font& newFont)
{
    if (font != newFont)
    {
        font = newFont;
        repaint();
    }
}

Font Label::getFont() const noexcept
{
    return font;
}

void Label::setEditable (bool editOnSingleClick,
                         bool editOnDoubleClick,
                         bool lossOfFocusDiscards)
{
    editSingleClick = editOnSingleClick;
    editDoubleClick = editOnDoubleClick;
    lossOfFocusDiscardsChanges = lossOfFocusDiscards;

    const auto isKeybordFocusable = (editOnSingleClick || editOnDoubleClick);

    setWantsKeyboardFocus (isKeybordFocusable);
    setFocusContainerType (isKeybordFocusable ? FocusContainerType::keyboardFocusContainer
                                              : FocusContainerType::none);

    invalidateAccessibilityHandler();
}

void Label::setJustificationType (Justification newJustification)
{
    if (justification != newJustification)
    {
        justification = newJustification;
        repaint();
    }
}

void Label::setBorderSize (BorderSize<int> newBorder)
{
    if (border != newBorder)
    {
        border = newBorder;
        repaint();
    }
}

//==============================================================================
Component* Label::getAttachedComponent() const
{
    return ownerComponent.get();
}

void Label::attachToComponent (Component* owner, bool onLeft)
{
    jassert (owner != this); // Not a great idea to try to attach it to itself!

    if (ownerComponent != nullptr)
        ownerComponent->removeComponentListener (this);

    ownerComponent = owner;
    leftOfOwnerComp = onLeft;

    if (ownerComponent != nullptr)
    {
        setVisible (ownerComponent->isVisible());
        ownerComponent->addComponentListener (this);
        componentParentHierarchyChanged (*ownerComponent);
        componentMovedOrResized (*ownerComponent, true, true);
    }
}

void Label::componentMovedOrResized (Component& component, bool /*wasMoved*/, bool /*wasResized*/)
{
    auto& lf = getLookAndFeel();
    auto f = lf.getLabelFont (*this);
    auto borderSize = lf.getLabelBorderSize (*this);

    if (leftOfOwnerComp)
    {
        auto width = jmin (roundToInt (f.getStringWidthFloat (textValue.toString()) + 0.5f)
                             + borderSize.getLeftAndRight(),
                           component.getX());

        setBounds (component.getX() - width, component.getY(), width, component.getHeight());
    }
    else
    {
        auto height = borderSize.getTopAndBottom() + 6 + roundToInt (f.getHeight() + 0.5f);

        setBounds (component.getX(), component.getY() - height, component.getWidth(), height);
    }
}

void Label::componentParentHierarchyChanged (Component& component)
{
    if (auto* parent = component.getParentComponent())
        parent->addChildComponent (this);
}

void Label::componentVisibilityChanged (Component& component)
{
    setVisible (component.isVisible());
}

//==============================================================================
void Label::textWasEdited() {}
void Label::textWasChanged() {}

void Label::editorShown (TextEditor* textEditor)
{
    Component::BailOutChecker checker (this);
    listeners.callChecked (checker, [this, textEditor] (Label::Listener& l) { l.editorShown (this, *textEditor); });

    if (checker.shouldBailOut())
        return;

    if (onEditorShow != nullptr)
        onEditorShow();
}

void Label::editorAboutToBeHidden (TextEditor* textEditor)
{
    if (auto* peer = getPeer())
        peer->dismissPendingTextInput();

    Component::BailOutChecker checker (this);
    listeners.callChecked (checker, [this, textEditor] (Label::Listener& l) { l.editorHidden (this, *textEditor); });

    if (checker.shouldBailOut())
        return;

    if (onEditorHide != nullptr)
        onEditorHide();
}

void Label::showEditor()
{
    if (editor == nullptr)
    {
        editor.reset (createEditorComponent());
        editor->setSize (10, 10);
        addAndMakeVisible (editor.get());
        editor->setText (getText(), false);
        editor->setKeyboardType (keyboardType);
        editor->addListener (this);
        editor->grabKeyboardFocus();

        if (editor == nullptr) // may be deleted by a callback
            return;

        editor->setHighlightedRegion (Range<int> (0, textValue.toString().length()));

        resized();
        repaint();

        editorShown (editor.get());

        enterModalState (false);
        editor->grabKeyboardFocus();
    }
}

bool Label::updateFromTextEditorContents (TextEditor& ed)
{
    auto newText = ed.getText();

    if (textValue.toString() != newText)
    {
        lastTextValue = newText;
        textValue = newText;
        repaint();

        textWasChanged();

        if (ownerComponent != nullptr)
            componentMovedOrResized (*ownerComponent, true, true);

        return true;
    }

    return false;
}

void Label::hideEditor (bool discardCurrentEditorContents)
{
    if (editor != nullptr)
    {
        WeakReference<Component> deletionChecker (this);
        std::unique_ptr<TextEditor> outgoingEditor;
        std::swap (outgoingEditor, editor);

        editorAboutToBeHidden (outgoingEditor.get());

        const bool changed = (! discardCurrentEditorContents)
                               && updateFromTextEditorContents (*outgoingEditor);
        outgoingEditor.reset();

        if (deletionChecker != nullptr)
            repaint();

        if (changed)
            textWasEdited();

        if (deletionChecker != nullptr)
            exitModalState (0);

        if (changed && deletionChecker != nullptr)
            callChangeListeners();
    }
}

void Label::inputAttemptWhenModal()
{
    if (editor != nullptr)
    {
        if (lossOfFocusDiscardsChanges)
            textEditorEscapeKeyPressed (*editor);
        else
            textEditorReturnKeyPressed (*editor);
    }
}

bool Label::isBeingEdited() const noexcept
{
    return editor != nullptr;
}

static void copyColourIfSpecified (Label& l, TextEditor& ed, int colourID, int targetColourID)
{
    if (l.isColourSpecified (colourID) || l.getLookAndFeel().isColourSpecified (colourID))
        ed.setColour (targetColourID, l.findColour (colourID));
}

TextEditor* Label::createEditorComponent()
{
    auto* ed = new TextEditor (getName());
    ed->applyFontToAllText (getLookAndFeel().getLabelFont (*this));
    copyAllExplicitColoursTo (*ed);

    copyColourIfSpecified (*this, *ed, textWhenEditingColourId, TextEditor::textColourId);
    copyColourIfSpecified (*this, *ed, backgroundWhenEditingColourId, TextEditor::backgroundColourId);
    copyColourIfSpecified (*this, *ed, outlineWhenEditingColourId, TextEditor::focusedOutlineColourId);
    copyColourIfSpecified (*this, *ed, highlightWhenEditingColourId, TextEditor::highlightColourId);
    copyColourIfSpecified (*this, *ed, highlightTextWhenEditingColourId, TextEditor::highlightedTextColourId);
    copyColourIfSpecified (*this, *ed, focusedOutlineWhenEditingColourId, TextEditor::focusedOutlineColourId);
    copyColourIfSpecified (*this, *ed, shadowColourIdWhenEditingColourId, TextEditor::shadowColourId);

    return ed;
}

TextEditor* Label::getCurrentTextEditor() const noexcept
{
    return editor.get();
}

//==============================================================================
void Label::paint (Graphics& g)
{
    getLookAndFeel().drawLabel (g, *this);
}

void Label::mouseUp (const MouseEvent& e)
{
    if (editSingleClick
         && isEnabled()
         && contains (e.getPosition())
         && ! (e.mouseWasDraggedSinceMouseDown() || e.mods.isPopupMenu()))
    {
        showEditor();
    }
}

void Label::mouseDoubleClick (const MouseEvent& e)
{
    if (editDoubleClick
         && isEnabled()
         && ! e.mods.isPopupMenu())
    {
        showEditor();
    }
}

void Label::resized()
{
    if (editor != nullptr)
        editor->setBounds (getLocalBounds());
}

void Label::focusGained (FocusChangeType cause)
{
    if (editSingleClick
         && isEnabled()
         && cause == focusChangedByTabKey)
    {
        showEditor();
    }
}

void Label::enablementChanged()
{
    repaint();
}

void Label::colourChanged()
{
    repaint();
}

void Label::setMinimumHorizontalScale (const float newScale)
{
    if (minimumHorizontalScale != newScale)
    {
        minimumHorizontalScale = newScale;
        repaint();
    }
}

//==============================================================================
// We'll use a custom focus traverser here to make sure focus goes from the
// text editor to another component rather than back to the label itself.
class LabelKeyboardFocusTraverser   : public KeyboardFocusTraverser
{
public:
    explicit LabelKeyboardFocusTraverser (Label& l)  : owner (l)  {}

    Component* getDefaultComponent (Component* parent) override
    {
        auto getContainer = [&]
        {
            if (owner.getCurrentTextEditor() != nullptr && parent == &owner)
                return owner.findKeyboardFocusContainer();

            return parent;
        };

        if (auto* container = getContainer())
            KeyboardFocusTraverser::getDefaultComponent (container);

        return nullptr;
    }

    Component* getNextComponent     (Component* c) override  { return KeyboardFocusTraverser::getNextComponent     (getComp (c)); }
    Component* getPreviousComponent (Component* c) override  { return KeyboardFocusTraverser::getPreviousComponent (getComp (c)); }

private:
    Component* getComp (Component* current) const
    {
        if (auto* ed = owner.getCurrentTextEditor())
            if (current == ed)
                return current->getParentComponent();

        return current;
    }

    Label& owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabelKeyboardFocusTraverser)
};

std::unique_ptr<ComponentTraverser> Label::createKeyboardFocusTraverser()
{
    return std::make_unique<LabelKeyboardFocusTraverser> (*this);
}

//==============================================================================
void Label::addListener    (Label::Listener* l)     { listeners.add (l); }
void Label::removeListener (Label::Listener* l)     { listeners.remove (l); }

void Label::callChangeListeners()
{
    Component::BailOutChecker checker (this);
    listeners.callChecked (checker, [this] (Listener& l) { l.labelTextChanged (this); });

    if (checker.shouldBailOut())
        return;

    if (onTextChange != nullptr)
        onTextChange();
}

//==============================================================================
void Label::textEditorTextChanged (TextEditor& ed)
{
    if (editor != nullptr)
    {
        jassert (&ed == editor.get());

        if (! (hasKeyboardFocus (true) || isCurrentlyBlockedByAnotherModalComponent()))
        {
            if (lossOfFocusDiscardsChanges)
                textEditorEscapeKeyPressed (ed);
            else
                textEditorReturnKeyPressed (ed);
        }
    }
}

void Label::textEditorReturnKeyPressed (TextEditor& ed)
{
    if (editor != nullptr)
    {
        jassert (&ed == editor.get());

        WeakReference<Component> deletionChecker (this);
        bool changed = updateFromTextEditorContents (ed);
        hideEditor (true);

        if (changed && deletionChecker != nullptr)
        {
            textWasEdited();

            if (deletionChecker != nullptr)
                callChangeListeners();
        }
    }
}

void Label::textEditorEscapeKeyPressed (TextEditor& ed)
{
    if (editor != nullptr)
    {
        jassert (&ed == editor.get());
        ignoreUnused (ed);

        editor->setText (textValue.toString(), false);
        hideEditor (true);
    }
}

void Label::textEditorFocusLost (TextEditor& ed)
{
    textEditorTextChanged (ed);
}

//==============================================================================
class LabelAccessibilityHandler  : public AccessibilityHandler
{
public:
    explicit LabelAccessibilityHandler (Label& labelToWrap)
        : AccessibilityHandler (labelToWrap,
                                labelToWrap.isEditable() ? AccessibilityRole::editableText : AccessibilityRole::label,
                                getAccessibilityActions (labelToWrap),
                                { std::make_unique<LabelValueInterface> (labelToWrap) }),
          label (labelToWrap)
    {
    }

    String getTitle() const override  { return label.getText(); }
    String getHelp() const override   { return label.getTooltip(); }

    AccessibleState getCurrentState() const override
    {
        if (label.isBeingEdited())
            return {}; // allow focus to pass through to the TextEditor

        return AccessibilityHandler::getCurrentState();
    }

private:
    class LabelValueInterface  : public AccessibilityTextValueInterface
    {
    public:
        explicit LabelValueInterface (Label& labelToWrap)
            : label (labelToWrap)
        {
        }

        bool isReadOnly() const override                 { return true; }
        String getCurrentValueAsString() const override  { return label.getText(); }
        void setValueAsString (const String&) override   {}

    private:
        Label& label;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabelValueInterface)
    };

    static AccessibilityActions getAccessibilityActions (Label& label)
    {
        if (label.isEditable())
            return AccessibilityActions().addAction (AccessibilityActionType::press, [&label] { label.showEditor(); });

        return {};
    }

    Label& label;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabelAccessibilityHandler)
};

std::unique_ptr<AccessibilityHandler> Label::createAccessibilityHandler()
{
    return std::make_unique<LabelAccessibilityHandler> (*this);
}

} // namespace juce
