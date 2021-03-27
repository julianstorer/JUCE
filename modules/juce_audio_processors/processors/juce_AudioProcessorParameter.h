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
/** An abstract base class for parameter objects that can be added to an
    AudioProcessor.

    @see AudioProcessor::addParameter

    @tags{Audio}
*/
class JUCE_API  AudioProcessorParameter
{
public:
    enum Orientation
    {
        fillFromLeft,
        fillFromRight,
        spreadFromMiddle,
        spreadFromMiddleClockwise,
        spreadFromMiddleAntiClockwise
    };

    AudioProcessorParameter() noexcept;

    /** Destructor. */
    virtual ~AudioProcessorParameter();

    /** Called by the host to find out the value of this parameter.

        Hosts will expect the value returned to be between 0 and 1.0.

        This could be called quite frequently, so try to make your code efficient.
        It's also likely to be called by non-UI threads, so the code in here should
        be thread-aware.
    */
    virtual float getValue() const = 0;

    /** The host will call this method to change the value of a parameter.

        The host may call this at any time, including during the audio processing
        callback, so your implementation has to process this very efficiently and
        avoid any kind of locking.

        If you want to set the value of a parameter internally, e.g. from your
        editor component, then don't call this directly - instead, use the
        setValueNotifyingHost() method, which will also send a message to
        the host telling it about the change. If the message isn't sent, the host
        won't be able to automate your parameters properly.

        The value passed will be between 0 and 1.0.
    */
    virtual void setValue (float newValue) = 0;

    /** A processor should call this when it needs to change one of its parameters.

        This could happen when the editor or some other internal operation changes
        a parameter. This method will call the setValue() method to change the
        value, and will then send a message to the host telling it about the change.

        Note that to make sure the host correctly handles automation, you should call
        the beginChangeGesture() and endChangeGesture() methods to tell the host when
        the user has started and stopped changing the parameter.
    */
    void setValueNotifyingHost (float newValue);

    /** Sends a signal to the host to tell it that the user is about to start changing this
        parameter.
        This allows the host to know when a parameter is actively being held by the user, and
        it may use this information to help it record automation.
        If you call this, it must be matched by a later call to endChangeGesture().
    */
    void beginChangeGesture();

    /** Tells the host that the user has finished changing this parameter.
        This allows the host to know when a parameter is actively being held by the user,
        and it may use this information to help it record automation.
        A call to this method must follow a call to beginChangeGesture().
    */
    void endChangeGesture();

    /** This should return the default value for this parameter. */
    virtual float getDefaultValue() const = 0;

    /** Returns the name to display for this parameter, which should be made
        to fit within the given string length.
    */
    virtual String getName (int maximumStringLength) const = 0;

    /** Some parameters may be able to return a label string for
        their units. For example "Hz" or "%".
    */
    virtual String getLabel() const = 0;

    /** Returns the number of steps that this parameter's range should be quantised into.

        If you want a continuous range of values, don't override this method, and allow
        the default implementation to return AudioProcessor::getDefaultNumParameterSteps().

        If your parameter is boolean, then you may want to make this return 2.

        The value that is returned may or may not be used, depending on the host. If you
        want the host to display stepped automation values, rather than a continuous
        interpolation between successive values, you should override isDiscrete to return true.

        @see isDiscrete
    */
    virtual int getNumSteps() const;

    /** Returns whether the parameter uses discrete values, based on the result of
        getNumSteps, or allows the host to select values continuously.

        This information may or may not be used, depending on the host. If you
        want the host to display stepped automation values, rather than a continuous
        interpolation between successive values, override this method to return true.

        @see getNumSteps
    */
    virtual bool isDiscrete() const;

    /** Returns whether the parameter represents a boolean switch, typically with
        "On" and "Off" states.

        This information may or may not be used, depending on the host. If you
        want the host to display a switch, rather than a two item dropdown menu,
        override this method to return true. You also need to override
        isDiscrete() to return `true` and getNumSteps() to return `2`.

        @see isDiscrete getNumSteps
    */
    virtual bool isBoolean() const;

    /** Returns a textual version of the supplied normalised parameter value.
        The default implementation just returns the floating point value
        as a string, but this could do anything you need for a custom type
        of value.
    */
    virtual String getText (float normalisedValue, int /*maximumStringLength*/) const;

    /** Should parse a string and return the appropriate value for it. */
    virtual float getValueForText (const String& text) const = 0;

    /** This can be overridden to tell the host that this parameter operates in the
        reverse direction.
        (Not all plugin formats or hosts will actually use this information).
    */
    virtual bool isOrientationInverted() const;

    /** This can be overridden to tell the host a specific orientation to use
        when displaying parameter controls on external control surfaces.
        (Not all plugin formats or hosts will actually use this information).
    */
    virtual Orientation getOrientation() const;

    /** Returns true if the host can automate this parameter.
        By default, this returns true.
    */
    virtual bool isAutomatable() const;

    /** Should return true if this parameter is a "meta" parameter.
        A meta-parameter is a parameter that changes other params. It is used
        by some hosts (e.g. AudioUnit hosts).
        By default this returns false.
    */
    virtual bool isMetaParameter() const;

    enum Category
    {
        genericParameter = (0 << 16) | 0,        /** If your parameter is not a meter then you should use this category */

        inputGain        = (1 << 16) | 0,        /** Currently not used */
        outputGain       = (1 << 16) | 1,

        /** The following categories tell the host that this parameter is a meter level value
            and therefore read-only. Most hosts will display these type of parameters as
            a meter in the generic view of your plug-in. Pro-Tools will also show the meter
            in the mixer view.
        */
        inputMeter                          = (2 << 16) | 0,
        outputMeter                         = (2 << 16) | 1,
        compressorLimiterGainReductionMeter = (2 << 16) | 2,
        expanderGateGainReductionMeter      = (2 << 16) | 3,
        analysisMeter                       = (2 << 16) | 4,
        otherMeter                          = (2 << 16) | 5
    };

    /** Returns the parameter's category. */
    virtual Category getCategory() const;

    /** Returns the index of this parameter in its parent processor's parameter list. */
    int getParameterIndex() const noexcept              { return parameterIndex; }

    //==============================================================================
    /** Returns the current value of the parameter as a String.

        This function can be called when you are hosting plug-ins to get a
        more specialised textual representation of the current value from the
        plug-in, for example "On" rather than "1.0".

        If you are implementing a plug-in then you should ignore this function
        and instead override getText.
    */
    virtual String getCurrentValueAsText() const;

    /** Returns the set of strings which represent the possible states a parameter
        can be in.

        If you are hosting a plug-in you can use the result of this function to
        populate a ComboBox listing the allowed values.

        If you are implementing a plug-in then you do not need to override this.
    */
    virtual StringArray getAllValueStrings() const;

    //==============================================================================
    /**
        A base class for listeners that want to know about changes to an
        AudioProcessorParameter.

        Use AudioProcessorParameter::addListener() to register your listener with
        an AudioProcessorParameter.

        This Listener replaces most of the functionality in the
        AudioProcessorListener class, which will be deprecated and removed.
    */
    class JUCE_API  Listener
    {
    public:
        /** Destructor. */
        virtual ~Listener()  = default;

        /** Receives a callback when a parameter has been changed.

            IMPORTANT NOTE: This will be called synchronously when a parameter changes, and
            many audio processors will change their parameter during their audio callback.
            This means that not only has your handler code got to be completely thread-safe,
            but it's also got to be VERY fast, and avoid blocking. If you need to handle
            this event on your message thread, use this callback to trigger an AsyncUpdater
            or ChangeBroadcaster which you can respond to on the message thread.
        */
        virtual void parameterValueChanged (int parameterIndex, float newValue) = 0;

        /** Indicates that a parameter change gesture has started.

            E.g. if the user is dragging a slider, this would be called with gestureIsStarting
            being true when they first press the mouse button, and it will be called again with
            gestureIsStarting being false when they release it.

            IMPORTANT NOTE: This will be called synchronously, and many audio processors will
            call it during their audio callback. This means that not only has your handler code
            got to be completely thread-safe, but it's also got to be VERY fast, and avoid
            blocking. If you need to handle this event on your message thread, use this callback
            to trigger an AsyncUpdater or ChangeBroadcaster which you can respond to later on the
            message thread.
        */
        virtual void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) = 0;
    };

    /** Registers a listener to receive events when the parameter's state changes.
        If the listener is already registered, this will not register it again.

        @see removeListener
    */
    void addListener (Listener* newListener);

    /** Removes a previously registered parameter listener

        @see addListener
    */
    void removeListener (Listener* listener);

    //==============================================================================
    /** @internal */
    void sendValueChangedMessageToListeners (float newValue);

private:
    //==============================================================================
    friend class AudioProcessor;
    friend class LegacyAudioParameter;
    AudioProcessor* processor = nullptr;
    int parameterIndex = -1;
    CriticalSection listenerLock;
    Array<Listener*> listeners;
    mutable StringArray valueStrings;

   #if JUCE_DEBUG
    bool isPerformingGesture = false;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessorParameter)
};

} // namespace juce
