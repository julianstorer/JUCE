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

/** @cond */
//==============================================================================
/** The contents of this namespace are used to implement ProcessorChain and should
    not be used elsewhere. Their interfaces (and existence) are liable to change!
*/
namespace detail
{
    template <typename Fn, typename Tuple, size_t... Ix>
    constexpr void forEachInTuple (Fn&& fn, Tuple&& tuple, std::index_sequence<Ix...>)
    {
        (fn (std::get<Ix> (tuple), std::integral_constant<size_t, Ix>()), ...);
    }

    template <typename T>
    using TupleIndexSequence = std::make_index_sequence<std::tuple_size_v<std::remove_cv_t<std::remove_reference_t<T>>>>;

    template <typename Fn, typename Tuple>
    constexpr void forEachInTuple (Fn&& fn, Tuple&& tuple)
    {
        forEachInTuple (std::forward<Fn> (fn), std::forward<Tuple> (tuple), TupleIndexSequence<Tuple>{});
    }

    template <typename Context, size_t Ix>
    inline constexpr auto useContextDirectly = ! Context::usesSeparateInputAndOutputBlocks() || Ix == 0;
}
/** @endcond */

/** This variadically-templated class lets you join together any number of processor
    classes into a single processor which will call process() on them all in sequence.

    @tags{DSP}
*/
template <typename... Processors>
class ProcessorChain
{
public:
    /** Get a reference to the processor at index `Index`. */
    template <int Index>       auto& get()       noexcept { return std::get<Index> (processors); }

    /** Get a reference to the processor at index `Index`. */
    template <int Index> const auto& get() const noexcept { return std::get<Index> (processors); }

    /** Set the processor at index `Index` to be bypassed or enabled. */
    template <int Index>
    void setBypassed (bool b) noexcept  { bypassed[(size_t) Index] = b; }

    /** Query whether the processor at index `Index` is bypassed. */
    template <int Index>
    bool isBypassed() const noexcept    { return bypassed[(size_t) Index]; }

    /** Prepare all inner processors with the provided `ProcessSpec`. */
    void prepare (const ProcessSpec& spec)
    {
        detail::forEachInTuple ([&] (auto& proc, auto) { proc.prepare (spec); }, processors);
    }

    /** Reset all inner processors. */
    void reset()
    {
        detail::forEachInTuple ([] (auto& proc, auto) { proc.reset(); }, processors);
    }

    /** Process `context` through all inner processors in sequence. */
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        detail::forEachInTuple ([this, &context] (auto& proc, auto index) noexcept { this->processOne (context, proc, index); },
                                processors);
    }

    /** Call the given function on each processor */
    template <typename Fn>
    void forEach(Fn &&fn) {
      detail::forEachInTuple([&](auto &proc, auto) { fn(proc); }, processors);
    }

private:
    template <typename Context, typename Proc, size_t Ix>
    void processOne (const Context& context, Proc& proc, std::integral_constant<size_t, Ix>) noexcept
    {
        if constexpr (detail::useContextDirectly<Context, Ix>)
        {
            auto contextCopy = context;
            contextCopy.isBypassed = (bypassed[Ix] || context.isBypassed);

            proc.process (contextCopy);
        }
        else
        {
            jassert (context.getOutputBlock().getNumChannels() == context.getInputBlock().getNumChannels());
            ProcessContextReplacing<typename Context::SampleType> replacingContext (context.getOutputBlock());
            replacingContext.isBypassed = (bypassed[Ix] || context.isBypassed);

            proc.process (replacingContext);
        }
    }

    std::tuple<Processors...> processors;
    std::array<bool, sizeof... (Processors)> bypassed { {} };
};

/** Non-member equivalent of ProcessorChain::get which avoids awkward
    member template syntax.
*/
template <int Index, typename... Processors>
inline auto& get (ProcessorChain<Processors...>& chain) noexcept
{
    return chain.template get<Index>();
}

/** Non-member equivalent of ProcessorChain::get which avoids awkward
    member template syntax.
*/
template <int Index, typename... Processors>
inline auto& get (const ProcessorChain<Processors...>& chain) noexcept
{
    return chain.template get<Index>();
}

/** Non-member equivalent of ProcessorChain::setBypassed which avoids awkward
    member template syntax.
*/
template <int Index, typename... Processors>
inline void setBypassed (ProcessorChain<Processors...>& chain, bool bypassed) noexcept
{
    chain.template setBypassed<Index> (bypassed);
}

/** Non-member equivalent of ProcessorChain::isBypassed which avoids awkward
    member template syntax.
*/
template <int Index, typename... Processors>
inline bool isBypassed (const ProcessorChain<Processors...>& chain) noexcept
{
    return chain.template isBypassed<Index>();
}

} // namespace juce::dsp

/** @cond */
namespace std
{

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wmismatched-tags")

/** Adds support for C++17 structured bindings. */
template <typename... Processors>
struct tuple_size<::juce::dsp::ProcessorChain<Processors...>> : integral_constant<size_t, sizeof... (Processors)> {};

/** Adds support for C++17 structured bindings. */
template <size_t I, typename... Processors>
struct tuple_element<I, ::juce::dsp::ProcessorChain<Processors...>> : tuple_element<I, tuple<Processors...>> {};

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

} // namespace std
/** @endcond */
