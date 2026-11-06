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

/**
    Performs a fast fourier transform.

    This is only a simple low-footprint implementation and isn't tuned for speed - it may
    be useful for simple applications where one of the more complex FFT libraries would be
    overkill. (But in the future it may end up becoming optimised of course...)

    The FFT class itself contains lookup tables, so there's some overhead in creating
    one, you should create and cache an FFT object for each size/direction of transform
    that you need, and re-use them to perform the actual operation.

    @tags{DSP}
*/
class JUCE_API  FFT
{
public:
    //==============================================================================
    /** Initialises an object for performing forward and inverse FFT with the given size.
        The number of points the FFT will operate on will be 2 ^ order.
    */
    FFT (int order);

    /** Move constructor. */
    FFT (FFT&&) noexcept;

    /** Move assignment operator. */
    FFT& operator= (FFT&&) noexcept;

    /** Destructor. */
    ~FFT();

    //==============================================================================
    /** Performs an out-of-place FFT, either forward or inverse.
        The arrays must contain at least getSize() elements.
    */
    void perform (const Complex<float>* input, Complex<float>* output, bool inverse) const noexcept;

    /** Performs an in-place forward transform on a block of real data.

        As the coefficients of the negative frequencies (frequencies higher than
        N/2 or pi) are the complex conjugate of their positive counterparts,
        it is usually unnecessary to calculate them.
        You can use onlyCalculateNonNegativeFrequencies to let the FFT
        engine know that you do not plan on using them.

        The array size must be large enough for the outputs,
        which are of size (2 * getSize()) by default or (2 + getSize()) if enabling onlyCalculateNonNegativeFrequencies.
        The first getSize() elements should contain your raw input sample data.

        On return, if the array will contain complex real + imaginary parts data interleaved.
        If onlyCalculateNonNegativeFrequencies is true, the array will contain
        (size / 2) + 1 complex numbers. The outputs can be passed to
        performRealOnlyInverseTransform() in order to convert it back to reals.
    */
    void performRealOnlyForwardTransform (float* inputOutputData,
                                          bool onlyCalculateNonNegativeFrequencies = false) const noexcept;

    /** Performs a reverse operation to data created in performRealOnlyForwardTransform().

        The size of the array passed in must still be getSize() + 2,
        which contains the ((size / 2) + 1) complex numbers representing the non-negative frequencies of the DFT.
        On return, the first getSize() elements of the array will contain the reconstituted samples.
    */
    void performRealOnlyInverseTransform (float* inputOutputData) const noexcept;

    /** Takes an array and simply transforms it to the magnitude frequency response
        spectrum. This may be handy for things like frequency displays or analysis.
        The size of the array passed in must be 2 * getSize().

        On return, if onlyCalculateNonNegativeFrequencies is false, the array will contain size
        magnitude values. If onlyCalculateNonNegativeFrequencies is true, the array will contain
        at least size / 2 + 1 magnitude values.
    */
    void performFrequencyOnlyForwardTransform (float* inputOutputData,
                                               bool onlyCalculateNonNegativeFrequencies = false) const noexcept;

    /** Returns the number of data points that this FFT was created to work with. */
    int getSize() const noexcept            { return size; }

    //==============================================================================
    /** @internal */
    /** @cond */
    struct Instance;
    template <typename> struct EngineImpl;
    /** @endcond */

private:
    //==============================================================================
    struct Engine;

    std::unique_ptr<Instance> engine;
    int size;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFT)
};

} // namespace juce::dsp
