/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 5 End-User License
   Agreement and JUCE 5 Privacy Policy (both updated and effective as of the
   27th April 2017).

   End User License Agreement: www.juce.com/juce-5-licence
   Privacy Policy: www.juce.com/juce-5-privacy-policy

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
AudioProcessorValueTreeState::Parameter::Parameter (const String& parameterID,
                                                    const String& parameterName,
                                                    const String& labelText,
                                                    NormalisableRange<float> valueRange,
                                                    float defaultParameterValue,
                                                    std::function<String(float)> valueToTextFunction,
                                                    std::function<float(const String&)> textToValueFunction,
                                                    bool isMetaParameter,
                                                    bool isAutomatableParameter,
                                                    bool isDiscrete,
                                                    AudioProcessorParameter::Category parameterCategory,
                                                    bool isBoolean)
    : AudioParameterFloat (parameterID,
                           parameterName,
                           valueRange,
                           defaultParameterValue,
                           labelText,
                           parameterCategory,
                           valueToTextFunction == nullptr ? std::function<String(float v, int)>()
                                                          : [valueToTextFunction](float v, int) { return valueToTextFunction (v); },
                           std::move (textToValueFunction)),
      unsnappedDefault (valueRange.convertTo0to1 (defaultParameterValue)),
      metaParameter (isMetaParameter),
      automatable (isAutomatableParameter),
      discrete (isDiscrete),
      boolean (isBoolean)
{
}

float AudioProcessorValueTreeState::Parameter::getDefaultValue() const  { return unsnappedDefault; }
int AudioProcessorValueTreeState::Parameter::getNumSteps() const        { return RangedAudioParameter::getNumSteps(); }

bool AudioProcessorValueTreeState::Parameter::isMetaParameter() const   { return metaParameter; }
bool AudioProcessorValueTreeState::Parameter::isAutomatable() const     { return automatable; }
bool AudioProcessorValueTreeState::Parameter::isDiscrete() const        { return discrete; }
bool AudioProcessorValueTreeState::Parameter::isBoolean() const         { return boolean; }

void AudioProcessorValueTreeState::Parameter::valueChanged (float newValue)
{
    if (lastValue == newValue)
        return;

    lastValue = newValue;

    if (onValueChanged != nullptr)
        onValueChanged();
}

//==============================================================================
class AudioProcessorValueTreeState::ParameterAdapter   : private AudioProcessorParameter::Listener
{
private:
    using Listener = AudioProcessorValueTreeState::Listener;

public:
    explicit ParameterAdapter (RangedAudioParameter& parameterIn)
        : parameter (parameterIn),
          // For legacy reasons, the unnormalised value should *not* be snapped on construction
          unnormalisedValue (getRange().convertFrom0to1 (parameter.getDefaultValue()))
    {
        parameter.addListener (this);

        if (auto* ptr = dynamic_cast<Parameter*> (&parameter))
            ptr->onValueChanged = [this] { parameterValueChanged ({}, {}); };
    }

    ~ParameterAdapter() override        { parameter.removeListener (this); }

    void addListener (Listener* l)      { listeners.add (l); }
    void removeListener (Listener* l)   { listeners.remove (l); }

    RangedAudioParameter& getParameter()                { return parameter; }
    const RangedAudioParameter& getParameter() const    { return parameter; }

    const NormalisableRange<float>& getRange() const    { return parameter.getNormalisableRange(); }

    float getDenormalisedDefaultValue() const    { return denormalise (parameter.getDefaultValue()); }

    void setDenormalisedValue (float value)
    {
        if (value == unnormalisedValue)
            return;

        setNormalisedValue (normalise (value));
    }

    float getDenormalisedValueForText (const String& text) const
    {
        return denormalise (parameter.getValueForText (text));
    }

    String getTextForDenormalisedValue (float value) const
    {
        return parameter.getText (normalise (value), 0);
    }

    float getDenormalisedValue() const                { return unnormalisedValue; }
    std::atomic<float>& getRawDenormalisedValue()     { return unnormalisedValue; }

    bool flushToTree (const Identifier& key, UndoManager* um)
    {
        auto needsUpdateTestValue = true;

        if (! needsUpdate.compare_exchange_strong (needsUpdateTestValue, false))
            return false;

        if (auto valueProperty = tree.getPropertyPointer (key))
        {
            if ((float) *valueProperty != unnormalisedValue)
            {
                ScopedValueSetter<bool> svs (ignoreParameterChangedCallbacks, true);
                tree.setProperty (key, unnormalisedValue.load(), um);
            }
        }
        else
        {
            tree.setProperty (key, unnormalisedValue.load(), nullptr);
        }

        return true;
    }

    ValueTree tree;

private:
    void parameterGestureChanged (int, bool) override {}

    void parameterValueChanged (int, float) override
    {
        const auto newValue = denormalise (parameter.getValue());

        if (unnormalisedValue == newValue && ! listenersNeedCalling)
            return;

        unnormalisedValue = newValue;
        listeners.call ([=](Listener& l) { l.parameterChanged (parameter.paramID, unnormalisedValue); });
        listenersNeedCalling = false;
        needsUpdate = true;
    }

    float denormalise (float normalised) const
    {
        return getParameter().convertFrom0to1 (normalised);
    }

    float normalise (float denormalised) const
    {
        return getParameter().convertTo0to1 (denormalised);
    }

    void setNormalisedValue (float value)
    {
        if (ignoreParameterChangedCallbacks)
            return;

        parameter.setValueNotifyingHost (value);
    }

    RangedAudioParameter& parameter;
    ListenerList<Listener> listeners;
    std::atomic<float> unnormalisedValue { 0.0f };
    std::atomic<bool> needsUpdate { true }, listenersNeedCalling { true };
    bool ignoreParameterChangedCallbacks { false };
};

//==============================================================================
AudioProcessorValueTreeState::AudioProcessorValueTreeState (AudioProcessor& processorToConnectTo,
                                                            UndoManager* undoManagerToUse,
                                                            const Identifier& valueTreeType,
                                                            ParameterLayout parameterLayout)
    : AudioProcessorValueTreeState (processorToConnectTo, undoManagerToUse)
{
    struct PushBackVisitor : ParameterLayout::Visitor
    {
        explicit PushBackVisitor (AudioProcessorValueTreeState& stateIn)
            : state (&stateIn) {}

        void visit (std::unique_ptr<RangedAudioParameter> param) const override
        {
            if (param == nullptr)
            {
                jassertfalse;
                return;
            }

            state->addParameterAdapter (*param);
            state->processor.addParameter (param.release());
        }

        void visit (std::unique_ptr<AudioProcessorParameterGroup> group) const override
        {
            if (group == nullptr)
            {
                jassertfalse;
                return;
            }

            for (const auto param : group->getParameters (true))
            {
                if (const auto rangedParam = dynamic_cast<RangedAudioParameter*> (param))
                {
                    state->addParameterAdapter (*rangedParam);
                }
                else
                {
                    // If you hit this assertion then you are attempting to add a parameter that is
                    // not derived from RangedAudioParameter to the AudioProcessorValueTreeState.
                    jassertfalse;
                }
            }

            state->processor.addParameterGroup (move (group));
        }

        AudioProcessorValueTreeState* state;
    };

    for (auto& item : parameterLayout.parameters)
        item->accept (PushBackVisitor (*this));

    state = ValueTree (valueTreeType);
}

AudioProcessorValueTreeState::AudioProcessorValueTreeState (AudioProcessor& p, UndoManager* um)
    : processor (p), undoManager (um)
{
    startTimerHz (10);
    state.addListener (this);
}

AudioProcessorValueTreeState::~AudioProcessorValueTreeState()
{
    stopTimer();
}

//==============================================================================
RangedAudioParameter* AudioProcessorValueTreeState::createAndAddParameter (const String& paramID,
                                                                           const String& paramName,
                                                                           const String& labelText,
                                                                           NormalisableRange<float> range,
                                                                           float defaultVal,
                                                                           std::function<String(float)> valueToTextFunction,
                                                                           std::function<float(const String&)> textToValueFunction,
                                                                           bool isMetaParameter,
                                                                           bool isAutomatableParameter,
                                                                           bool isDiscreteParameter,
                                                                           AudioProcessorParameter::Category category,
                                                                           bool isBooleanParameter)
{
    return createAndAddParameter (std::make_unique<Parameter> (paramID,
                                                               paramName,
                                                               labelText,
                                                               range,
                                                               defaultVal,
                                                               std::move (valueToTextFunction),
                                                               std::move (textToValueFunction),
                                                               isMetaParameter,
                                                               isAutomatableParameter,
                                                               isDiscreteParameter,
                                                               category,
                                                               isBooleanParameter));
}

RangedAudioParameter* AudioProcessorValueTreeState::createAndAddParameter (std::unique_ptr<RangedAudioParameter> param)
{
    if (param == nullptr)
        return nullptr;

    // All parameters must be created before giving this manager a ValueTree state!
    jassert (! state.isValid());

    if (getParameter (param->paramID) != nullptr)
        return nullptr;

    addParameterAdapter (*param);

    processor.addParameter (param.get());

    return param.release();
}

//==============================================================================
void AudioProcessorValueTreeState::addParameterAdapter (RangedAudioParameter& param)
{
    adapterTable.emplace (param.paramID, std::make_unique<ParameterAdapter> (param));
}

AudioProcessorValueTreeState::ParameterAdapter* AudioProcessorValueTreeState::getParameterAdapter (StringRef paramID) const
{
    auto it = adapterTable.find (paramID);
    return it == adapterTable.end() ? nullptr : it->second.get();
}

void AudioProcessorValueTreeState::addParameterListener (StringRef paramID, Listener* listener)
{
    if (auto* p = getParameterAdapter (paramID))
        p->addListener (listener);
}

void AudioProcessorValueTreeState::removeParameterListener (StringRef paramID, Listener* listener)
{
    if (auto* p = getParameterAdapter (paramID))
        p->removeListener (listener);
}

Value AudioProcessorValueTreeState::getParameterAsValue (StringRef paramID) const
{
    if (auto* adapter = getParameterAdapter (paramID))
        if (adapter->tree.isValid())
            return adapter->tree.getPropertyAsValue (valuePropertyID, undoManager);

    return {};
}

NormalisableRange<float> AudioProcessorValueTreeState::getParameterRange (StringRef paramID) const noexcept
{
    if (auto* p = getParameterAdapter (paramID))
        return p->getRange();

    return {};
}

RangedAudioParameter* AudioProcessorValueTreeState::getParameter (StringRef paramID) const noexcept
{
    if (auto adapter = getParameterAdapter (paramID))
        return &adapter->getParameter();

    return nullptr;
}

std::atomic<float>* AudioProcessorValueTreeState::getRawParameterValue (StringRef paramID) const noexcept
{
    if (auto* p = getParameterAdapter (paramID))
        return &p->getRawDenormalisedValue();

    return nullptr;
}

ValueTree AudioProcessorValueTreeState::copyState()
{
    ScopedLock lock (valueTreeChanging);
    flushParameterValuesToValueTree();
    return state.createCopy();
}

void AudioProcessorValueTreeState::replaceState (const ValueTree& newState)
{
    ScopedLock lock (valueTreeChanging);

    state = newState;

    if (undoManager != nullptr)
        undoManager->clearUndoHistory();
}

void AudioProcessorValueTreeState::setNewState (ValueTree vt)
{
    jassert (vt.getParent() == state);

    if (auto* p = getParameterAdapter (vt.getProperty (idPropertyID).toString()))
    {
        p->tree = vt;
        p->setDenormalisedValue (p->tree.getProperty (valuePropertyID, p->getDenormalisedDefaultValue()));
    }
}

void AudioProcessorValueTreeState::updateParameterConnectionsToChildTrees()
{
    ScopedLock lock (valueTreeChanging);

    for (auto& p : adapterTable)
        p.second->tree = ValueTree();

    for (const auto& child : state)
        setNewState (child);

    for (auto& p : adapterTable)
    {
        auto& adapter = *p.second;

        if (! adapter.tree.isValid())
        {
            adapter.tree = ValueTree (valueType);
            adapter.tree.setProperty (idPropertyID, adapter.getParameter().paramID, nullptr);
            state.appendChild (adapter.tree, nullptr);
        }
    }

    flushParameterValuesToValueTree();
}

void AudioProcessorValueTreeState::valueTreePropertyChanged (ValueTree& tree, const Identifier&)
{
    if (tree.hasType (valueType) && tree.getParent() == state)
        setNewState (tree);
}

void AudioProcessorValueTreeState::valueTreeChildAdded (ValueTree& parent, ValueTree& tree)
{
    if (parent == state && tree.hasType (valueType))
        setNewState (tree);
}

void AudioProcessorValueTreeState::valueTreeRedirected (ValueTree& v)
{
    if (v == state)
        updateParameterConnectionsToChildTrees();
}

bool AudioProcessorValueTreeState::flushParameterValuesToValueTree()
{
    ScopedLock lock (valueTreeChanging);

    bool anyUpdated = false;

    for (auto& p : adapterTable)
        anyUpdated |= p.second->flushToTree (valuePropertyID, undoManager);

    return anyUpdated;
}

void AudioProcessorValueTreeState::timerCallback()
{
    auto anythingUpdated = flushParameterValuesToValueTree();

    startTimer (anythingUpdated ? 1000 / 50
                                : jlimit (50, 500, getTimerInterval() + 20));
}

//==============================================================================
struct AttachedControlBase  : public AudioProcessorValueTreeState::Listener,
                              public AsyncUpdater
{
    AttachedControlBase (AudioProcessorValueTreeState& s, const String& p)
        : state (s), paramID (p), lastValue (0)
    {
        state.addParameterListener (paramID, this);
    }

    void removeListener()
    {
        state.removeParameterListener (paramID, this);
    }

    void setNewDenormalisedValue (float newDenormalisedValue)
    {
        if (auto* p = state.getParameter (paramID))
        {
            const float newValue = state.getParameterRange (paramID)
                                        .convertTo0to1 (newDenormalisedValue);

            if (p->getValue() != newValue)
                p->setValueNotifyingHost (newValue);
        }
    }

    void sendInitialUpdate()
    {
        if (auto* v = state.getRawParameterValue (paramID))
            parameterChanged (paramID, *v);
    }

    void parameterChanged (const String&, float newValue) override
    {
        lastValue = newValue;

        if (MessageManager::getInstance()->isThisTheMessageThread())
        {
            cancelPendingUpdate();
            setValue (newValue);
        }
        else
        {
            triggerAsyncUpdate();
        }
    }

    void beginParameterChange()
    {
        if (auto* p = state.getParameter (paramID))
        {
            if (state.undoManager != nullptr)
                state.undoManager->beginNewTransaction();

            p->beginChangeGesture();
        }
    }

    void endParameterChange()
    {
        if (AudioProcessorParameter* p = state.getParameter (paramID))
            p->endChangeGesture();
    }

    void handleAsyncUpdate() override
    {
        setValue (lastValue);
    }

    virtual void setValue (float) = 0;

    AudioProcessorValueTreeState& state;
    String paramID;
    float lastValue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AttachedControlBase)
};

//==============================================================================
struct AudioProcessorValueTreeState::SliderAttachment::Pimpl  : private AttachedControlBase,
                                                                private Slider::Listener
{
    Pimpl (AudioProcessorValueTreeState& s, const String& p, Slider& sl)
        : AttachedControlBase (s, p), slider (sl), ignoreCallbacks (false)
    {
        NormalisableRange<float> range (state.getParameterRange (paramID));

        if (auto* param = state.getParameterAdapter (paramID))
        {
            slider.valueFromTextFunction = [param](const String& text) { return (double) param->getDenormalisedValueForText (text); };
            slider.textFromValueFunction = [param](double value) { return param->getTextForDenormalisedValue ((float) value); };
            slider.setDoubleClickReturnValue (true, range.convertFrom0to1 (param->getParameter().getDefaultValue()));
        }

        auto convertFrom0To1Function = [range](double currentRangeStart,
                                               double currentRangeEnd,
                                               double normalisedValue) mutable
        {
            range.start = (float) currentRangeStart;
            range.end = (float) currentRangeEnd;
            return (double) range.convertFrom0to1 ((float) normalisedValue);
        };

        auto convertTo0To1Function = [range](double currentRangeStart,
                                             double currentRangeEnd,
                                             double mappedValue) mutable
        {
            range.start = (float) currentRangeStart;
            range.end = (float) currentRangeEnd;
            return (double) range.convertTo0to1 ((float) mappedValue);
        };

        auto snapToLegalValueFunction = [range](double currentRangeStart,
                                                double currentRangeEnd,
                                                double valueToSnap) mutable
        {
            range.start = (float) currentRangeStart;
            range.end = (float) currentRangeEnd;
            return (double) range.snapToLegalValue ((float) valueToSnap);
        };

        NormalisableRange<double> newRange { (double) range.start,
                                             (double) range.end,
                                             convertFrom0To1Function,
                                             convertTo0To1Function,
                                             snapToLegalValueFunction };
        newRange.interval = (double) range.interval;
        newRange.skew = (double) range.skew;

        slider.setNormalisableRange (newRange);

        sendInitialUpdate();
        slider.addListener (this);
    }

    ~Pimpl() override
    {
        slider.removeListener (this);
        removeListener();
    }

    void setValue (float newValue) override
    {
        const ScopedLock selfCallbackLock (selfCallbackMutex);

        {
            ScopedValueSetter<bool> svs (ignoreCallbacks, true);
            slider.setValue (newValue, sendNotificationSync);
        }
    }

    void sliderValueChanged (Slider* s) override
    {
        const ScopedLock selfCallbackLock (selfCallbackMutex);

        if ((! ignoreCallbacks) && (! ModifierKeys::currentModifiers.isRightButtonDown()))
            setNewDenormalisedValue ((float) s->getValue());
    }

    void sliderDragStarted (Slider*) override   { beginParameterChange(); }
    void sliderDragEnded   (Slider*) override   { endParameterChange(); }

    Slider& slider;
    bool ignoreCallbacks;
    CriticalSection selfCallbackMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pimpl)
};

AudioProcessorValueTreeState::SliderAttachment::SliderAttachment (AudioProcessorValueTreeState& s, const String& p, Slider& sl)
    : pimpl (new Pimpl (s, p, sl))
{
}

AudioProcessorValueTreeState::SliderAttachment::~SliderAttachment() {}

//==============================================================================
struct AudioProcessorValueTreeState::ComboBoxAttachment::Pimpl  : private AttachedControlBase,
                                                                  private ComboBox::Listener
{
    Pimpl (AudioProcessorValueTreeState& s, const String& p, ComboBox& c)
        : AttachedControlBase (s, p), combo (c), ignoreCallbacks (false)
    {
        sendInitialUpdate();
        combo.addListener (this);
    }

    ~Pimpl() override
    {
        combo.removeListener (this);
        removeListener();
    }

    void setValue (float newValue) override
    {
        const ScopedLock selfCallbackLock (selfCallbackMutex);

        if (state.getParameter (paramID) != nullptr)
        {
            auto normValue = state.getParameterRange (paramID)
                                  .convertTo0to1 (newValue);
            auto index = roundToInt (normValue * (combo.getNumItems() - 1));

            if (index != combo.getSelectedItemIndex())
            {
                ScopedValueSetter<bool> svs (ignoreCallbacks, true);
                combo.setSelectedItemIndex (index, sendNotificationSync);
            }
        }
    }

    void comboBoxChanged (ComboBox*) override
    {
        const ScopedLock selfCallbackLock (selfCallbackMutex);

        if (! ignoreCallbacks)
        {
            if (auto* p = state.getParameter (paramID))
            {
                auto newValue = (float) combo.getSelectedItemIndex() / (combo.getNumItems() - 1);

                if (p->getValue() != newValue)
                {
                    beginParameterChange();
                    p->setValueNotifyingHost (newValue);
                    endParameterChange();
                }
            }
        }
    }

    ComboBox& combo;
    bool ignoreCallbacks;
    CriticalSection selfCallbackMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pimpl)
};

AudioProcessorValueTreeState::ComboBoxAttachment::ComboBoxAttachment (AudioProcessorValueTreeState& s, const String& p, ComboBox& c)
    : pimpl (new Pimpl (s, p, c))
{
}

AudioProcessorValueTreeState::ComboBoxAttachment::~ComboBoxAttachment() {}

//==============================================================================
struct AudioProcessorValueTreeState::ButtonAttachment::Pimpl  : private AttachedControlBase,
                                                                private Button::Listener
{
    Pimpl (AudioProcessorValueTreeState& s, const String& p, Button& b)
        : AttachedControlBase (s, p), button (b), ignoreCallbacks (false)
    {
        sendInitialUpdate();
        button.addListener (this);
    }

    ~Pimpl() override
    {
        button.removeListener (this);
        removeListener();
    }

    void setValue (float newValue) override
    {
        const ScopedLock selfCallbackLock (selfCallbackMutex);

        {
            ScopedValueSetter<bool> svs (ignoreCallbacks, true);
            button.setToggleState (newValue >= 0.5f, sendNotificationSync);
        }
    }

    void buttonClicked (Button* b) override
    {
        const ScopedLock selfCallbackLock (selfCallbackMutex);

        if (! ignoreCallbacks)
        {
            beginParameterChange();
            setNewDenormalisedValue (b->getToggleState() ? 1.0f : 0.0f);
            endParameterChange();
        }
    }

    Button& button;
    bool ignoreCallbacks;
    CriticalSection selfCallbackMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pimpl)
};

AudioProcessorValueTreeState::ButtonAttachment::ButtonAttachment (AudioProcessorValueTreeState& s, const String& p, Button& b)
    : pimpl (new Pimpl (s, p, b))
{
}

AudioProcessorValueTreeState::ButtonAttachment::~ButtonAttachment() {}


//==============================================================================
//==============================================================================
#if JUCE_UNIT_TESTS

struct ParameterAdapterTests  : public UnitTest
{
    ParameterAdapterTests()
        : UnitTest ("Parameter Adapter", UnitTestCategories::audioProcessorParameters)
    {}

    void runTest() override
    {
        beginTest ("The default value is returned correctly");
        {
            const auto test = [&] (NormalisableRange<float> range, float value)
            {
                AudioParameterFloat param ({}, {}, range, value, {});

                AudioProcessorValueTreeState::ParameterAdapter adapter (param);

                expectEquals (adapter.getDenormalisedDefaultValue(), value);
            };

            test ({ -100, 100 }, 0);
            test ({ -2.5, 12.5 }, 10);
        }

        beginTest ("Denormalised parameter values can be retrieved");
        {
            const auto test = [&](NormalisableRange<float> range, float value)
            {
                AudioParameterFloat param ({}, {}, range, {}, {});
                AudioProcessorValueTreeState::ParameterAdapter adapter (param);

                adapter.setDenormalisedValue (value);

                expectEquals (adapter.getDenormalisedValue(), value);
                expectEquals (adapter.getRawDenormalisedValue().load(), value);
            };

            test ({ -20, -10 }, -15);
            test ({ 0, 7.5 }, 2.5);
        }

        beginTest ("Floats can be converted to text");
        {
            const auto test = [&](NormalisableRange<float> range, float value, String expected)
            {
                AudioParameterFloat param ({}, {}, range, {}, {});
                AudioProcessorValueTreeState::ParameterAdapter adapter (param);

                expectEquals (adapter.getTextForDenormalisedValue (value), expected);
            };

            test ({ -100, 100 }, 0, "0.0000000");
            test ({ -2.5, 12.5 }, 10, "10.0000000");
            test ({ -20, -10 }, -15, "-15.0000000");
            test ({ 0, 7.5 }, 2.5, "2.5000000");
        }

        beginTest ("Text can be converted to floats");
        {
            const auto test = [&](NormalisableRange<float> range, String text, float expected)
            {
                AudioParameterFloat param ({}, {}, range, {}, {});
                AudioProcessorValueTreeState::ParameterAdapter adapter (param);

                expectEquals (adapter.getDenormalisedValueForText (text), expected);
            };

            test ({ -100, 100 }, "0.0", 0);
            test ({ -2.5, 12.5 }, "10.0", 10);
            test ({ -20, -10 }, "-15.0", -15);
            test ({ 0, 7.5 }, "2.5", 2.5);
        }
    }
};

static ParameterAdapterTests parameterAdapterTests;

namespace
{
template <typename ValueType>
inline bool operator== (const NormalisableRange<ValueType>& a,
                        const NormalisableRange<ValueType>& b)
{
    return std::tie (a.start, a.end, a.interval, a.skew, a.symmetricSkew)
           == std::tie (b.start, b.end, b.interval, b.skew, b.symmetricSkew);
}

template <typename ValueType>
inline bool operator!= (const NormalisableRange<ValueType>& a,
                        const NormalisableRange<ValueType>& b)
{
    return ! (a == b);
}
} // namespace

class AudioProcessorValueTreeStateTests  : public UnitTest
{
private:
    using Parameter = AudioProcessorValueTreeState::Parameter;
    using ParameterGroup = AudioProcessorParameterGroup;
    using ParameterLayout = AudioProcessorValueTreeState::ParameterLayout;

    class TestAudioProcessor : public AudioProcessor
    {
    public:
        TestAudioProcessor() = default;

        explicit TestAudioProcessor (ParameterLayout layout)
            : state (*this, nullptr, "state", std::move (layout)) {}

        const String getName() const override { return {}; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (AudioBuffer<float>&, MidiBuffer&) override {}
        using AudioProcessor::processBlock;
        double getTailLengthSeconds() const override { return {}; }
        bool acceptsMidi() const override { return {}; }
        bool producesMidi() const override { return {}; }
        AudioProcessorEditor* createEditor() override { return {}; }
        bool hasEditor() const override { return {}; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return {}; }
        void setCurrentProgram (int) override {}
        String getProgramName (int) override { return {}; }
        void changeProgramName (int, const String&) override {}
        void getStateInformation (MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        AudioProcessorValueTreeState state { *this, nullptr };
    };

    struct Listener final : public AudioProcessorValueTreeState::Listener
    {
        void parameterChanged (const String& idIn, float valueIn) override
        {
            id = idIn;
            value = valueIn;
        }

        String id;
        float value{};
    };

public:
    AudioProcessorValueTreeStateTests()
        : UnitTest ("Audio Processor Value Tree State", UnitTestCategories::audioProcessorParameters)
    {}

    void runTest() override
    {
        ScopedJuceInitialiser_GUI scopedJuceInitialiser_gui;

        beginTest ("After calling createAndAddParameter, the number of parameters increases by one");
        {
            TestAudioProcessor proc;

            proc.state.createAndAddParameter (std::make_unique<Parameter> (String(), String(), String(), NormalisableRange<float>(),
                                                                           0.0f, nullptr, nullptr));

            expectEquals (proc.getParameters().size(), 1);
        }

        beginTest ("After creating a normal named parameter, we can later retrieve that parameter");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr));

            expect (proc.state.getParameter (key) == param);
        }

        beginTest ("After construction, the value tree has the expected format");
        {
            TestAudioProcessor proc ({
                std::make_unique<AudioProcessorParameterGroup> ("", "", "",
                    std::make_unique<AudioParameterBool> ("a", "", false),
                    std::make_unique<AudioParameterFloat> ("b", "", NormalisableRange<float>{}, 0.0f)),
                std::make_unique<AudioProcessorParameterGroup> ("", "", "",
                    std::make_unique<AudioParameterInt> ("c", "", 0, 1, 0),
                    std::make_unique<AudioParameterChoice> ("d", "", StringArray { "foo", "bar" }, 0)) });

            const auto valueTree = proc.state.copyState();

            expectEquals (valueTree.getNumChildren(), 4);

            for (auto child : valueTree)
            {
                expect (child.hasType ("PARAM"));
                expect (child.hasProperty ("id"));
                expect (child.hasProperty ("value"));
            }
        }

        beginTest ("Meta parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr, true));

            expect (param->isMetaParameter());
        }

        beginTest ("Automatable parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr, false, true));

            expect (param->isAutomatable());
        }

        beginTest ("Discrete parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr, false, false, true));

            expect (param->isDiscrete());
        }

        beginTest ("Custom category parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr, false, false, false,
                                                                                              AudioProcessorParameter::Category::inputMeter));

            expect (param->category == AudioProcessorParameter::Category::inputMeter);
        }

        beginTest ("Boolean parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr, false, false, false,
                                                                                              AudioProcessorParameter::Category::genericParameter, true));

            expect (param->isBoolean());
        }

        beginTest ("After creating a custom named parameter, we can later retrieve that parameter");
        {
            const auto key = "id";
            auto param = std::make_unique<AudioParameterBool> (key, "", false);
            const auto paramPtr = param.get();

            TestAudioProcessor proc (std::move (param));

            expect (proc.state.getParameter (key) == paramPtr);
        }

        beginTest ("After adding a normal parameter that already exists, the AudioProcessor parameters are unchanged");
        {
            TestAudioProcessor proc;
            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr));

            proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                           0.0f, nullptr, nullptr));

            expectEquals (proc.getParameters().size(), 1);
            expect (proc.getParameters().getFirst() == param);
        }

        beginTest ("After setting a parameter value, that value is reflected in the state");
        {
            TestAudioProcessor proc;
            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr));

            const auto value = 0.5f;
            param->setValueNotifyingHost (value);

            expectEquals (proc.state.getRawParameterValue (key)->load(), value);
        }

        beginTest ("After adding an APVTS::Parameter, its value is the default value");
        {
            TestAudioProcessor proc;
            const auto key = "id";
            const auto value = 5.0f;

            proc.state.createAndAddParameter (std::make_unique<Parameter> (
                key,
                String(),
                String(),
                NormalisableRange<float> (0.0f, 100.0f, 10.0f),
                value,
                nullptr,
                nullptr));

            expectEquals (proc.state.getRawParameterValue (key)->load(), value);
        }

        beginTest ("Listeners receive notifications when parameters change");
        {
            Listener listener;
            TestAudioProcessor proc;
            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                              0.0f, nullptr, nullptr));
            proc.state.addParameterListener (key, &listener);

            const auto value = 0.5f;
            param->setValueNotifyingHost (value);

            expectEquals (listener.id, String { key });
            expectEquals (listener.value, value);
        }

        beginTest ("Bool parameters have a range of 0-1");
        {
            const auto key = "id";

            TestAudioProcessor proc (std::make_unique<AudioParameterBool> (key, "", false));

            expect (proc.state.getParameterRange (key) == NormalisableRange<float> (0.0f, 1.0f, 1.0f));
        }

        beginTest ("Float parameters retain their specified range");
        {
            const auto key = "id";
            const auto range = NormalisableRange<float> { -100, 100, 0.7f, 0.2f, true };

            TestAudioProcessor proc (std::make_unique<AudioParameterFloat> (key, "", range, 0.0f));

            expect (proc.state.getParameterRange (key) == range);
        }

        beginTest ("Int parameters retain their specified range");
        {
            const auto key = "id";
            const auto min = -27;
            const auto max = 53;

            TestAudioProcessor proc (std::make_unique<AudioParameterInt> (key, "", min, max, 0));

            expect (proc.state.getParameterRange (key) == NormalisableRange<float> (float (min), float (max), 1.0f));
        }

        beginTest ("Choice parameters retain their specified range");
        {
            const auto key = "id";
            const auto choices = StringArray { "", "", "" };

            TestAudioProcessor proc (std::make_unique<AudioParameterChoice> (key, "", choices, 0));

            expect (proc.state.getParameterRange (key) == NormalisableRange<float> (0.0f, (float) (choices.size() - 1), 1.0f));
            expect (proc.state.getParameter (key)->getNumSteps() == choices.size());
        }

        beginTest ("When the parameter value is changed, normal parameter values are updated");
        {
            TestAudioProcessor proc;
            const auto key = "id";
            const auto initialValue = 0.2f;
            auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                                        initialValue, nullptr, nullptr));
            proc.state.state = ValueTree { "state" };

            auto value = proc.state.getParameterAsValue (key);
            expectEquals (float (value.getValue()), initialValue);

            const auto newValue = 0.75f;
            value = newValue;

            expectEquals (param->getValue(), newValue);
            expectEquals (proc.state.getRawParameterValue (key)->load(), newValue);
        }

        beginTest ("When the parameter value is changed, custom parameter values are updated");
        {
            const auto key = "id";
            const auto choices = StringArray ("foo", "bar", "baz");
            auto param = std::make_unique<AudioParameterChoice> (key, "", choices, 0);
            const auto paramPtr = param.get();
            TestAudioProcessor proc (std::move (param));

            const auto newValue = 2.0f;
            auto value = proc.state.getParameterAsValue (key);
            value = newValue;

            expectEquals (paramPtr->getCurrentChoiceName(), choices[int (newValue)]);
            expectEquals (proc.state.getRawParameterValue (key)->load(), newValue);
        }

        beginTest ("When the parameter value is changed, listeners are notified");
        {
            Listener listener;
            TestAudioProcessor proc;
            const auto key = "id";
            proc.state.createAndAddParameter (std::make_unique<Parameter> (key, String(), String(), NormalisableRange<float>(),
                                                                           0.0f, nullptr, nullptr));
            proc.state.addParameterListener (key, &listener);
            proc.state.state = ValueTree { "state" };

            const auto newValue = 0.75f;
            proc.state.getParameterAsValue (key) = newValue;

            expectEquals (listener.value, newValue);
            expectEquals (listener.id, String { key });
        }

        beginTest ("When the parameter value is changed, listeners are notified");
        {
            const auto key = "id";
            const auto choices = StringArray { "foo", "bar", "baz" };
            Listener listener;
            TestAudioProcessor proc (std::make_unique<AudioParameterChoice> (key, "", choices, 0));
            proc.state.addParameterListener (key, &listener);

            const auto newValue = 2.0f;
            proc.state.getParameterAsValue (key) = newValue;

            expectEquals (listener.value, newValue);
            expectEquals (listener.id, String (key));
        }
    }
};

static AudioProcessorValueTreeStateTests audioProcessorValueTreeStateTests;

#endif

} // namespace juce
