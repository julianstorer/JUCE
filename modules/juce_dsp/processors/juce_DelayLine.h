/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

namespace juce::dsp
{

//==============================================================================
/**
    A collection of structs to pass as the template argument when setting the
    interpolation type for the DelayLine class.
*/
namespace DelayLineInterpolationTypes
{
    /**
        No interpolation between successive samples in the delay line will be
        performed. This is useful when the delay is a constant integer or to
        create lo-fi audio effects.

        @tags{DSP}
    */
    struct None {};

    /**
        Successive samples in the delay line will be linearly interpolated. This
        type of interpolation has a low computational cost where the delay can be
        modulated in real time, but it also introduces a low-pass filtering effect
        into your audio signal.

        @tags{DSP}
    */
    struct Linear {};

    /**
        Successive samples in the delay line will be interpolated using a 3rd order
        Lagrange interpolator. This method incurs more computational overhead than
        linear interpolation but reduces the low-pass filtering effect whilst
        remaining amenable to real time delay modulation.

        @tags{DSP}
    */
    struct Lagrange3rd {};

    /**
        Successive samples in the delay line will be interpolated using 1st order
        Thiran interpolation. This method is very efficient, and features a flat
        amplitude frequency response in exchange for less accuracy in the phase
        response. This interpolation method is stateful so is unsuitable for
        applications requiring fast delay modulation.

        @tags{DSP}
    */
    struct Thiran {};
}

//==============================================================================
/**
    A delay line processor featuring several algorithms for the fractional delay
    calculation, block processing, and sample-by-sample processing useful when
    modulating the delay in real time or creating a standard delay effect with
    feedback.

    Note: If you intend to change the delay in real time, you may want to smooth
    changes to the delay systematically using either a ramp or a low-pass filter.

    @see SmoothedValue, FirstOrderTPTFilter

    @tags{DSP}
*/
template <typename SampleType, typename InterpolationType = DelayLineInterpolationTypes::Linear>
class DelayLine
{
public:
    //==============================================================================
    /** Default constructor. */
    DelayLine();

    /** Constructor. */
    explicit DelayLine (int maximumDelayInSamples);

    //==============================================================================
    /** Sets the delay in samples. */
    void setDelay (SampleType newDelayInSamples);

    /** Returns the current delay in samples. */
    SampleType getDelay() const;

    //==============================================================================
    /** Initialises the processor. */
    void prepare (const ProcessSpec& spec);

    /** Sets a new maximum delay in samples.

        Also clears the delay line.

        This may allocate internally, so you should never call it from the audio thread.
    */
    void setMaximumDelayInSamples (int maxDelayInSamples);

    /** Gets the maximum possible delay in samples.

        For very short delay times, the result of getMaximumDelayInSamples() may
        differ from the last value passed to setMaximumDelayInSamples().
    */
    int getMaximumDelayInSamples() const noexcept       { return totalSize - 2; }

    /** Resets the internal state variables of the processor. */
    void reset();

    //==============================================================================
    /** Pushes a single sample into one channel of the delay line.

        Use this function and popSample instead of process if you need to modulate
        the delay in real time instead of using a fixed delay value, or if you want
        to code a delay effect with a feedback loop.

        @see setDelay, popSample, process
    */
    void pushSample (int channel, SampleType sample);

    /** Pops a single sample from one channel of the delay line.

        Use this function to modulate the delay in real time or implement standard
        delay effects with feedback.

        @param channel              the target channel for the delay line.

        @param delayInSamples       sets the wanted fractional delay in samples, or -1
                                    to use the value being used before or set with
                                    setDelay function.

        @param updateReadPointer    should be set to true if you use the function
                                    once for each sample, or false if you need
                                    multi-tap delay capabilities.

        @see setDelay, pushSample, process
    */
    SampleType popSample (int channel, SampleType delayInSamples = -1, bool updateReadPointer = true);

    //==============================================================================
    /** Processes the input and output samples supplied in the processing context.

        Can be used for block processing when the delay is not going to change
        during processing. The delay must first be set by calling setDelay.

        @see setDelay
    */
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        const auto& inputBlock = context.getInputBlock();
        auto& outputBlock      = context.getOutputBlock();
        const auto numChannels = outputBlock.getNumChannels();
        const auto numSamples  = outputBlock.getNumSamples();

        jassert (inputBlock.getNumChannels() == numChannels);
        jassert (inputBlock.getNumChannels() == writePos.size());
        jassert (inputBlock.getNumSamples()  == numSamples);

        if (context.isBypassed)
        {
            outputBlock.copyFrom (inputBlock);
            return;
        }

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* inputSamples = inputBlock.getChannelPointer (channel);
            auto* outputSamples = outputBlock.getChannelPointer (channel);

            for (size_t i = 0; i < numSamples; ++i)
            {
                pushSample ((int) channel, inputSamples[i]);
                outputSamples[i] = popSample ((int) channel);
            }
        }
    }

private:
    //==============================================================================
    SampleType interpolateSample (int channel)
    {
        if constexpr (std::is_same_v<InterpolationType, DelayLineInterpolationTypes::None>)
        {
            auto index = (readPos[(size_t) channel] + delayInt) % totalSize;
            return bufferData.getSample (channel, index);
        }
        else if constexpr (std::is_same_v<InterpolationType, DelayLineInterpolationTypes::Linear>)
        {
            auto index1 = readPos[(size_t) channel] + delayInt;
            auto index2 = index1 + 1;

            if (index2 >= totalSize)
            {
                index1 %= totalSize;
                index2 %= totalSize;
            }

            auto value1 = bufferData.getSample (channel, index1);
            auto value2 = bufferData.getSample (channel, index2);

            return value1 + delayFrac * (value2 - value1);
        }
        else if constexpr (std::is_same_v<InterpolationType, DelayLineInterpolationTypes::Lagrange3rd>)
        {
            auto index1 = readPos[(size_t) channel] + delayInt;
            auto index2 = index1 + 1;
            auto index3 = index2 + 1;
            auto index4 = index3 + 1;

            if (index4 >= totalSize)
            {
                index1 %= totalSize;
                index2 %= totalSize;
                index3 %= totalSize;
                index4 %= totalSize;
            }

            auto* samples = bufferData.getReadPointer (channel);

            auto value1 = samples[index1];
            auto value2 = samples[index2];
            auto value3 = samples[index3];
            auto value4 = samples[index4];

            auto d0 = delayFrac;
            auto d1 = delayFrac - 1.f;
            auto d2 = delayFrac - 2.f;
            auto d3 = delayFrac - 3.f;

            auto a = d1 * d2 / 6.f;
            auto b = d3 * d0 * 0.5f;

            return a * (d0 * value4 - d3 * value1)
                 + b * (d2 * value2 - d1 * value3);
        }
        else if constexpr (std::is_same_v<InterpolationType, DelayLineInterpolationTypes::Thiran>)
        {
            auto index1 = readPos[(size_t) channel] + delayInt;
            auto index2 = index1 + 1;

            if (index2 >= totalSize)
            {
                index1 %= totalSize;
                index2 %= totalSize;
            }

            auto value1 = bufferData.getSample (channel, index1);
            auto value2 = bufferData.getSample (channel, index2);

            auto output = approximatelyEqual (delayFrac, (SampleType) 0) ? value1 : value2 + alpha * (value1 - v[(size_t) channel]);
            v[(size_t) channel] = output;

            return output;
        }
    }

    //==============================================================================
    void updateInternalVariables()
    {
        if constexpr (std::is_same_v<InterpolationType, DelayLineInterpolationTypes::Lagrange3rd>)
        {
            if (delayFrac < (SampleType) 2.0 && delayInt >= 1)
            {
                delayFrac++;
                delayInt--;
            }
        }
        else if constexpr (std::is_same_v<InterpolationType, DelayLineInterpolationTypes::Thiran>)
        {
            if (delayFrac < (SampleType) 0.618 && delayInt >= 1)
            {
                delayFrac++;
                delayInt--;
            }

            alpha = (1 - delayFrac) / (1 + delayFrac);
        }
    }

    //==============================================================================
    double sampleRate;

    //==============================================================================
    AudioBuffer<SampleType> bufferData;
    std::vector<SampleType> v;
    std::vector<int> writePos, readPos;
    SampleType delay = 0.0, delayFrac = 0.0;
    int delayInt = 0, totalSize = 4;
    SampleType alpha = 0.0;
};

} // namespace juce::dsp
