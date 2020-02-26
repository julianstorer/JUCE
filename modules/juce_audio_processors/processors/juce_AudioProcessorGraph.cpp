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

static void updateOnMessageThread (AsyncUpdater& updater)
{
    if (MessageManager::getInstance()->isThisTheMessageThread())
        updater.handleAsyncUpdate();
    else
        updater.triggerAsyncUpdate();
}

template <typename FloatType>
struct GraphRenderSequence
{
    GraphRenderSequence() {}

    struct Context
    {
        FloatType** audioBuffers;
        MidiBuffer* midiBuffers;
        AudioPlayHead* audioPlayHead;
        int numSamples;
    };

    void perform (AudioBuffer<FloatType>& buffer, MidiBuffer& midiMessages, AudioPlayHead* audioPlayHead)
    {
        auto numSamples = buffer.getNumSamples();
        auto maxSamples = renderingBuffer.getNumSamples();

        if (numSamples > maxSamples)
        {
            // Being asked to render more samples than our buffers have, so divide the buffer into chunks
            int chunkStartSample = 0;
            while (chunkStartSample < numSamples)
            {
                auto chunkSize = jmin (maxSamples, numSamples - chunkStartSample);

                AudioBuffer<FloatType> audioChunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), chunkStartSample, chunkSize);
                midiChunk.clear();
                midiChunk.addEvents (midiMessages, chunkStartSample, chunkSize, -chunkStartSample);

                perform (audioChunk, midiChunk, audioPlayHead);

                chunkStartSample += maxSamples;
            }

            return;
        }

        currentAudioInputBuffer = &buffer;
        currentAudioOutputBuffer.setSize (jmax (1, buffer.getNumChannels()), numSamples);
        currentAudioOutputBuffer.clear();
        currentMidiInputBuffer = &midiMessages;
        currentMidiOutputBuffer.clear();

        {
            const Context context { renderingBuffer.getArrayOfWritePointers(), midiBuffers.begin(), audioPlayHead, numSamples };

            for (auto* op : renderOps)
                op->perform (context);
        }

        for (int i = 0; i < buffer.getNumChannels(); ++i)
            buffer.copyFrom (i, 0, currentAudioOutputBuffer, i, 0, numSamples);

        midiMessages.clear();
        midiMessages.addEvents (currentMidiOutputBuffer, 0, buffer.getNumSamples(), 0);
        currentAudioInputBuffer = nullptr;
    }

    void addClearChannelOp (int index)
    {
        createOp ([=] (const Context& c)    { FloatVectorOperations::clear (c.audioBuffers[index], c.numSamples); });
    }

    void addCopyChannelOp (int srcIndex, int dstIndex)
    {
        createOp ([=] (const Context& c)    { FloatVectorOperations::copy (c.audioBuffers[dstIndex],
                                                                           c.audioBuffers[srcIndex],
                                                                           c.numSamples); });
    }

    void addAddChannelOp (int srcIndex, int dstIndex)
    {
        createOp ([=] (const Context& c)    { FloatVectorOperations::add (c.audioBuffers[dstIndex],
                                                                          c.audioBuffers[srcIndex],
                                                                          c.numSamples); });
    }

    void addClearMidiBufferOp (int index)
    {
        createOp ([=] (const Context& c)    { c.midiBuffers[index].clear(); });
    }

    void addCopyMidiBufferOp (int srcIndex, int dstIndex)
    {
        createOp ([=] (const Context& c)    { c.midiBuffers[dstIndex] = c.midiBuffers[srcIndex]; });
    }

    void addAddMidiBufferOp (int srcIndex, int dstIndex)
    {
        createOp ([=] (const Context& c)    { c.midiBuffers[dstIndex].addEvents (c.midiBuffers[srcIndex],
                                                                                 0, c.numSamples, 0); });
    }

    void addDelayChannelOp (int chan, int delaySize)
    {
        renderOps.add (new DelayChannelOp (chan, delaySize));
    }

    void addProcessOp (const AudioProcessorGraph::Node::Ptr& node,
                       const Array<int>& audioChannelsUsed, int totalNumChans, int midiBuffer)
    {
        renderOps.add (new ProcessOp (node, audioChannelsUsed, totalNumChans, midiBuffer));
    }

    void prepareBuffers (int blockSize)
    {
        renderingBuffer.setSize (numBuffersNeeded + 1, blockSize);
        renderingBuffer.clear();
        currentAudioOutputBuffer.setSize (numBuffersNeeded + 1, blockSize);
        currentAudioOutputBuffer.clear();

        currentAudioInputBuffer = nullptr;
        currentMidiInputBuffer = nullptr;
        currentMidiOutputBuffer.clear();

        midiBuffers.clearQuick();
        midiBuffers.resize (numMidiBuffersNeeded);

        const int defaultMIDIBufferSize = 512;

        midiChunk.ensureSize (defaultMIDIBufferSize);

        for (auto&& m : midiBuffers)
            m.ensureSize (defaultMIDIBufferSize);
    }

    void releaseBuffers()
    {
        renderingBuffer.setSize (1, 1);
        currentAudioOutputBuffer.setSize (1, 1);
        currentAudioInputBuffer = nullptr;
        currentMidiInputBuffer = nullptr;
        currentMidiOutputBuffer.clear();
        midiBuffers.clear();
    }

    int numBuffersNeeded = 0, numMidiBuffersNeeded = 0;

    AudioBuffer<FloatType> renderingBuffer, currentAudioOutputBuffer;
    AudioBuffer<FloatType>* currentAudioInputBuffer = nullptr;

    MidiBuffer* currentMidiInputBuffer = nullptr;
    MidiBuffer currentMidiOutputBuffer;

    Array<MidiBuffer> midiBuffers;
    MidiBuffer midiChunk;

private:
    //==============================================================================
    struct RenderingOp
    {
        RenderingOp() noexcept {}
        virtual ~RenderingOp() {}
        virtual void perform (const Context&) = 0;

        JUCE_LEAK_DETECTOR (RenderingOp)
    };

    OwnedArray<RenderingOp> renderOps;

    //==============================================================================
    template <typename LambdaType>
    void createOp (LambdaType&& fn)
    {
        struct LambdaOp  : public RenderingOp
        {
            LambdaOp (LambdaType&& f) : function (std::move (f)) {}
            void perform (const Context& c) override    { function (c); }

            LambdaType function;
        };

        renderOps.add (new LambdaOp (std::move (fn)));
    }

    //==============================================================================
    struct DelayChannelOp  : public RenderingOp
    {
        DelayChannelOp (int chan, int delaySize)
            : channel (chan),
              bufferSize (delaySize + 1),
              writeIndex (delaySize)
        {
            buffer.calloc ((size_t) bufferSize);
        }

        void perform (const Context& c) override
        {
            auto* data = c.audioBuffers[channel];

            for (int i = c.numSamples; --i >= 0;)
            {
                buffer[writeIndex] = *data;
                *data++ = buffer[readIndex];

                if (++readIndex  >= bufferSize) readIndex = 0;
                if (++writeIndex >= bufferSize) writeIndex = 0;
            }
        }

        HeapBlock<FloatType> buffer;
        const int channel, bufferSize;
        int readIndex = 0, writeIndex;

        JUCE_DECLARE_NON_COPYABLE (DelayChannelOp)
    };

    //==============================================================================
    struct ProcessOp   : public RenderingOp
    {
        ProcessOp (const AudioProcessorGraph::Node::Ptr& n,
                   const Array<int>& audioChannelsUsed,
                   int totalNumChans, int midiBuffer)
            : node (n),
              processor (*n->getProcessor()),
              audioChannelsToUse (audioChannelsUsed),
              totalChans (jmax (1, totalNumChans)),
              midiBufferToUse (midiBuffer)
        {
            audioChannels.calloc ((size_t) totalChans);

            while (audioChannelsToUse.size() < totalChans)
                audioChannelsToUse.add (0);
        }

        void perform (const Context& c) override
        {
            processor.setPlayHead (c.audioPlayHead);

            for (int i = 0; i < totalChans; ++i)
                audioChannels[i] = c.audioBuffers[audioChannelsToUse.getUnchecked (i)];

            AudioBuffer<FloatType> buffer (audioChannels, totalChans, c.numSamples);

            if (processor.isSuspended())
                buffer.clear();
            else
                callProcess (buffer, c.midiBuffers[midiBufferToUse]);
        }

        void callProcess (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
        {
            if (processor.isUsingDoublePrecision())
            {
                tempBufferDouble.makeCopyOf (buffer, true);

                if (node->isBypassed())
                    node->processBlockBypassed (tempBufferDouble, midiMessages);
                else
                    node->processBlock (tempBufferDouble, midiMessages);

                buffer.makeCopyOf (tempBufferDouble, true);
            }
            else
            {
                if (node->isBypassed())
                    node->processBlockBypassed (buffer, midiMessages);
                else
                    node->processBlock (buffer, midiMessages);
            }
        }

        void callProcess (AudioBuffer<double>& buffer, MidiBuffer& midiMessages)
        {
            if (processor.isUsingDoublePrecision())
            {
                if (node->isBypassed())
                    node->processBlockBypassed (buffer, midiMessages);
                else
                    node->processBlock (buffer, midiMessages);
            }
            else
            {
                tempBufferFloat.makeCopyOf (buffer, true);

                if (node->isBypassed())
                    node->processBlockBypassed (tempBufferFloat, midiMessages);
                else
                    node->processBlock (tempBufferFloat, midiMessages);

                buffer.makeCopyOf (tempBufferFloat, true);
            }
        }

        const AudioProcessorGraph::Node::Ptr node;
        AudioProcessor& processor;

        Array<int> audioChannelsToUse;
        HeapBlock<FloatType*> audioChannels;
        AudioBuffer<float> tempBufferFloat, tempBufferDouble;
        const int totalChans, midiBufferToUse;

        JUCE_DECLARE_NON_COPYABLE (ProcessOp)
    };
};

//==============================================================================
//==============================================================================
template <typename RenderSequence>
struct RenderSequenceBuilder
{
    RenderSequenceBuilder (AudioProcessorGraph& g, RenderSequence& s)
        : graph (g), sequence (s)
    {
        createOrderedNodeList();

        audioBuffers.add (AssignedBuffer::createReadOnlyEmpty()); // first buffer is read-only zeros
        midiBuffers .add (AssignedBuffer::createReadOnlyEmpty());

        for (int i = 0; i < orderedNodes.size(); ++i)
        {
            createRenderingOpsForNode (*orderedNodes.getUnchecked(i), i);
            markAnyUnusedBuffersAsFree (audioBuffers, i);
            markAnyUnusedBuffersAsFree (midiBuffers, i);
        }

        graph.setLatencySamples (totalLatency);

        s.numBuffersNeeded = audioBuffers.size();
        s.numMidiBuffersNeeded = midiBuffers.size();
    }

    //==============================================================================
    using NodeID = AudioProcessorGraph::NodeID;

    AudioProcessorGraph& graph;
    RenderSequence& sequence;

    Array<AudioProcessorGraph::Node*> orderedNodes;

    struct AssignedBuffer
    {
        AudioProcessorGraph::NodeAndChannel channel;

        static AssignedBuffer createReadOnlyEmpty() noexcept    { return { { zeroNodeID(), 0 } }; }
        static AssignedBuffer createFree() noexcept             { return { { freeNodeID(), 0 } }; }

        bool isReadOnlyEmpty() const noexcept                   { return channel.nodeID == zeroNodeID(); }
        bool isFree() const noexcept                            { return channel.nodeID == freeNodeID(); }
        bool isAssigned() const noexcept                        { return ! (isReadOnlyEmpty() || isFree()); }

        void setFree() noexcept                                 { channel = { freeNodeID(), 0 }; }
        void setAssignedToNonExistentNode() noexcept            { channel = { anonNodeID(), 0 }; }

    private:
        static NodeID anonNodeID() { return NodeID (0x7ffffffd); }
        static NodeID zeroNodeID() { return NodeID (0x7ffffffe); }
        static NodeID freeNodeID() { return NodeID (0x7fffffff); }
    };

    Array<AssignedBuffer> audioBuffers, midiBuffers;

    enum { readOnlyEmptyBufferIndex = 0 };

    struct Delay
    {
        NodeID nodeID;
        int delay;
    };

    HashMap<uint32, int> delays;
    int totalLatency = 0;

    int getNodeDelay (NodeID nodeID) const noexcept
    {
        return delays[nodeID.uid];
    }

    int getInputLatencyForNode (NodeID nodeID) const
    {
        int maxLatency = 0;

        for (auto&& c : graph.getConnections())
            if (c.destination.nodeID == nodeID)
                maxLatency = jmax (maxLatency, getNodeDelay (c.source.nodeID));

        return maxLatency;
    }

    //==============================================================================
    void createOrderedNodeList()
    {
        for (auto* node : graph.getNodes())
        {
            int j = 0;

            for (; j < orderedNodes.size(); ++j)
                if (graph.isAnInputTo (*node, *orderedNodes.getUnchecked(j)))
                  break;

            orderedNodes.insert (j, node);
        }
    }

    int findBufferForInputAudioChannel (AudioProcessorGraph::Node& node, const int inputChan,
                                        const int ourRenderingIndex, const int maxLatency)
    {
        auto& processor = *node.getProcessor();
        auto numOuts = processor.getTotalNumOutputChannels();

        auto sources = getSourcesForChannel (node, inputChan);

        // Handle an unconnected input channel...
        if (sources.isEmpty())
        {
            if (inputChan >= numOuts)
                return readOnlyEmptyBufferIndex;

            auto index = getFreeBuffer (audioBuffers);
            sequence.addClearChannelOp (index);
            return index;
        }

        // Handle an input from a single source..
        if (sources.size() == 1)
        {
            // channel with a straightforward single input..
            auto src = sources.getUnchecked(0);

            int bufIndex = getBufferContaining (src);

            if (bufIndex < 0)
            {
                // if not found, this is probably a feedback loop
                bufIndex = readOnlyEmptyBufferIndex;
                jassert (bufIndex >= 0);
            }

            if (inputChan < numOuts
                 && isBufferNeededLater (ourRenderingIndex, inputChan, src))
            {
                // can't mess up this channel because it's needed later by another node,
                // so we need to use a copy of it..
                auto newFreeBuffer = getFreeBuffer (audioBuffers);
                sequence.addCopyChannelOp (bufIndex, newFreeBuffer);
                bufIndex = newFreeBuffer;
            }

            auto nodeDelay = getNodeDelay (src.nodeID);

            if (nodeDelay < maxLatency)
                sequence.addDelayChannelOp (bufIndex, maxLatency - nodeDelay);

            return bufIndex;
        }

        // Handle a mix of several outputs coming into this input..
        int reusableInputIndex = -1;
        int bufIndex = -1;

        for (int i = 0; i < sources.size(); ++i)
        {
            auto src = sources.getReference(i);
            auto sourceBufIndex = getBufferContaining (src);

            if (sourceBufIndex >= 0 && ! isBufferNeededLater (ourRenderingIndex, inputChan, src))
            {
                // we've found one of our input chans that can be re-used..
                reusableInputIndex = i;
                bufIndex = sourceBufIndex;

                auto nodeDelay = getNodeDelay (src.nodeID);

                if (nodeDelay < maxLatency)
                    sequence.addDelayChannelOp (bufIndex, maxLatency - nodeDelay);

                break;
            }
        }

        if (reusableInputIndex < 0)
        {
            // can't re-use any of our input chans, so get a new one and copy everything into it..
            bufIndex = getFreeBuffer (audioBuffers);
            jassert (bufIndex != 0);

            audioBuffers.getReference (bufIndex).setAssignedToNonExistentNode();

            auto srcIndex = getBufferContaining (sources.getFirst());

            if (srcIndex < 0)
                sequence.addClearChannelOp (bufIndex);  // if not found, this is probably a feedback loop
            else
                sequence.addCopyChannelOp (srcIndex, bufIndex);

            reusableInputIndex = 0;
            auto nodeDelay = getNodeDelay (sources.getFirst().nodeID);

            if (nodeDelay < maxLatency)
                sequence.addDelayChannelOp (bufIndex, maxLatency - nodeDelay);
        }

        for (int i = 0; i < sources.size(); ++i)
        {
            if (i != reusableInputIndex)
            {
                auto src = sources.getReference(i);
                int srcIndex = getBufferContaining (src);

                if (srcIndex >= 0)
                {
                    auto nodeDelay = getNodeDelay (src.nodeID);

                    if (nodeDelay < maxLatency)
                    {
                        if (! isBufferNeededLater (ourRenderingIndex, inputChan, src))
                        {
                            sequence.addDelayChannelOp (srcIndex, maxLatency - nodeDelay);
                        }
                        else // buffer is reused elsewhere, can't be delayed
                        {
                            auto bufferToDelay = getFreeBuffer (audioBuffers);
                            sequence.addCopyChannelOp (srcIndex, bufferToDelay);
                            sequence.addDelayChannelOp (bufferToDelay, maxLatency - nodeDelay);
                            srcIndex = bufferToDelay;
                        }
                    }

                    sequence.addAddChannelOp (srcIndex, bufIndex);
                }
            }
        }

        return bufIndex;
    }

    int findBufferForInputMidiChannel (AudioProcessorGraph::Node& node, int ourRenderingIndex)
    {
        auto& processor = *node.getProcessor();
        auto sources = getSourcesForChannel (node, AudioProcessorGraph::midiChannelIndex);

        // No midi inputs..
        if (sources.isEmpty())
        {
            auto midiBufferToUse = getFreeBuffer (midiBuffers); // need to pick a buffer even if the processor doesn't use midi

            if (processor.acceptsMidi() || processor.producesMidi())
                sequence.addClearMidiBufferOp (midiBufferToUse);

            return midiBufferToUse;
        }

        // One midi input..
        if (sources.size() == 1)
        {
            auto src = sources.getReference (0);
            auto midiBufferToUse = getBufferContaining (src);

            if (midiBufferToUse >= 0)
            {
                if (isBufferNeededLater (ourRenderingIndex, AudioProcessorGraph::midiChannelIndex, src))
                {
                    // can't mess up this channel because it's needed later by another node, so we
                    // need to use a copy of it..
                    auto newFreeBuffer = getFreeBuffer (midiBuffers);
                    sequence.addCopyMidiBufferOp (midiBufferToUse, newFreeBuffer);
                    midiBufferToUse = newFreeBuffer;
                }
            }
            else
            {
                // probably a feedback loop, so just use an empty one..
                midiBufferToUse = getFreeBuffer (midiBuffers); // need to pick a buffer even if the processor doesn't use midi
            }

            return midiBufferToUse;
        }

        // Multiple midi inputs..
        int midiBufferToUse = -1;
        int reusableInputIndex = -1;

        for (int i = 0; i < sources.size(); ++i)
        {
            auto src = sources.getReference (i);
            auto sourceBufIndex = getBufferContaining (src);

            if (sourceBufIndex >= 0
                 && ! isBufferNeededLater (ourRenderingIndex, AudioProcessorGraph::midiChannelIndex, src))
            {
                // we've found one of our input buffers that can be re-used..
                reusableInputIndex = i;
                midiBufferToUse = sourceBufIndex;
                break;
            }
        }

        if (reusableInputIndex < 0)
        {
            // can't re-use any of our input buffers, so get a new one and copy everything into it..
            midiBufferToUse = getFreeBuffer (midiBuffers);
            jassert (midiBufferToUse >= 0);

            auto srcIndex = getBufferContaining (sources.getUnchecked(0));

            if (srcIndex >= 0)
                sequence.addCopyMidiBufferOp (srcIndex, midiBufferToUse);
            else
                sequence.addClearMidiBufferOp (midiBufferToUse);

            reusableInputIndex = 0;
        }

        for (int i = 0; i < sources.size(); ++i)
        {
            if (i != reusableInputIndex)
            {
                auto srcIndex = getBufferContaining (sources.getUnchecked(i));

                if (srcIndex >= 0)
                    sequence.addAddMidiBufferOp (srcIndex, midiBufferToUse);
            }
        }

        return midiBufferToUse;
    }

    void createRenderingOpsForNode (AudioProcessorGraph::Node& node, const int ourRenderingIndex)
    {
        auto& processor = *node.getProcessor();
        auto numIns  = processor.getTotalNumInputChannels();
        auto numOuts = processor.getTotalNumOutputChannels();
        auto totalChans = jmax (numIns, numOuts);

        Array<int> audioChannelsToUse;
        auto maxLatency = getInputLatencyForNode (node.nodeID);

        for (int inputChan = 0; inputChan < numIns; ++inputChan)
        {
            // get a list of all the inputs to this node
            auto index = findBufferForInputAudioChannel (node, inputChan, ourRenderingIndex, maxLatency);
            jassert (index >= 0);

            audioChannelsToUse.add (index);

            if (inputChan < numOuts)
                audioBuffers.getReference (index).channel = { node.nodeID, inputChan };
        }

        for (int outputChan = numIns; outputChan < numOuts; ++outputChan)
        {
            auto index = getFreeBuffer (audioBuffers);
            jassert (index != 0);
            audioChannelsToUse.add (index);

            audioBuffers.getReference (index).channel = { node.nodeID, outputChan };
        }

        auto midiBufferToUse = findBufferForInputMidiChannel (node, ourRenderingIndex);

        if (processor.producesMidi())
            midiBuffers.getReference (midiBufferToUse).channel = { node.nodeID, AudioProcessorGraph::midiChannelIndex };

        delays.set (node.nodeID.uid, maxLatency + processor.getLatencySamples());

        if (numOuts == 0)
            totalLatency = maxLatency;

        sequence.addProcessOp (node, audioChannelsToUse, totalChans, midiBufferToUse);
    }

    //==============================================================================
    Array<AudioProcessorGraph::NodeAndChannel> getSourcesForChannel (AudioProcessorGraph::Node& node, int inputChannelIndex)
    {
        Array<AudioProcessorGraph::NodeAndChannel> results;
        AudioProcessorGraph::NodeAndChannel nc { node.nodeID, inputChannelIndex };

        for (auto&& c : graph.getConnections())
            if (c.destination == nc)
                results.add (c.source);

        return results;
    }

    static int getFreeBuffer (Array<AssignedBuffer>& buffers)
    {
        for (int i = 1; i < buffers.size(); ++i)
            if (buffers.getReference (i).isFree())
                return i;

        buffers.add (AssignedBuffer::createFree());
        return buffers.size() - 1;
    }

    int getBufferContaining (AudioProcessorGraph::NodeAndChannel output) const noexcept
    {
        int i = 0;

        for (auto& b : output.isMIDI() ? midiBuffers : audioBuffers)
        {
            if (b.channel == output)
                return i;

            ++i;
        }

        return -1;
    }

    void markAnyUnusedBuffersAsFree (Array<AssignedBuffer>& buffers, const int stepIndex)
    {
        for (auto& b : buffers)
            if (b.isAssigned() && ! isBufferNeededLater (stepIndex, -1, b.channel))
                b.setFree();
    }

    bool isBufferNeededLater (int stepIndexToSearchFrom,
                              int inputChannelOfIndexToIgnore,
                              AudioProcessorGraph::NodeAndChannel output) const
    {
        while (stepIndexToSearchFrom < orderedNodes.size())
        {
            auto* node = orderedNodes.getUnchecked (stepIndexToSearchFrom);

            if (output.isMIDI())
            {
                if (inputChannelOfIndexToIgnore != AudioProcessorGraph::midiChannelIndex
                     && graph.isConnected ({ { output.nodeID, AudioProcessorGraph::midiChannelIndex },
                                             { node->nodeID,  AudioProcessorGraph::midiChannelIndex } }))
                    return true;
            }
            else
            {
                for (int i = 0; i < node->getProcessor()->getTotalNumInputChannels(); ++i)
                    if (i != inputChannelOfIndexToIgnore && graph.isConnected ({ output, { node->nodeID, i } }))
                        return true;
            }

            inputChannelOfIndexToIgnore = -1;
            ++stepIndexToSearchFrom;
        }

        return false;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderSequenceBuilder)
};

//==============================================================================
AudioProcessorGraph::Connection::Connection (NodeAndChannel src, NodeAndChannel dst) noexcept
    : source (src), destination (dst)
{
}

bool AudioProcessorGraph::Connection::operator== (const Connection& other) const noexcept
{
    return source == other.source && destination == other.destination;
}

bool AudioProcessorGraph::Connection::operator!= (const Connection& c) const noexcept
{
    return ! operator== (c);
}

bool AudioProcessorGraph::Connection::operator< (const Connection& other) const noexcept
{
    if (source.nodeID != other.source.nodeID)
        return source.nodeID < other.source.nodeID;

    if (destination.nodeID != other.destination.nodeID)
        return destination.nodeID < other.destination.nodeID;

    if (source.channelIndex != other.source.channelIndex)
        return source.channelIndex < other.source.channelIndex;

    return destination.channelIndex < other.destination.channelIndex;
}

//==============================================================================
AudioProcessorGraph::Node::Node (NodeID n, std::unique_ptr<AudioProcessor> p) noexcept
    : nodeID (n), processor (std::move (p))
{
    jassert (processor != nullptr);
}

void AudioProcessorGraph::Node::prepare (double newSampleRate, int newBlockSize,
                                         AudioProcessorGraph* graph, ProcessingPrecision precision)
{
    const ScopedLock lock (processorLock);

    if (! isPrepared)
    {
        setParentGraph (graph);

        // try to align the precision of the processor and the graph
        processor->setProcessingPrecision (processor->supportsDoublePrecisionProcessing() ? precision
                                                                                          : singlePrecision);

        processor->setRateAndBufferSizeDetails (newSampleRate, newBlockSize);
        processor->prepareToPlay (newSampleRate, newBlockSize);

        // This may be checked from other threads that haven't taken the processorLock,
        // so we need to leave it until the processor has been completely prepared
        isPrepared = true;
    }
}

void AudioProcessorGraph::Node::unprepare()
{
    const ScopedLock lock (processorLock);

    if (isPrepared)
    {
        isPrepared = false;
        processor->releaseResources();
    }
}

void AudioProcessorGraph::Node::setParentGraph (AudioProcessorGraph* const graph) const
{
    const ScopedLock lock (processorLock);

    if (auto* ioProc = dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (processor.get()))
        ioProc->setParentGraph (graph);
}

bool AudioProcessorGraph::Node::Connection::operator== (const Connection& other) const noexcept
{
    return otherNode == other.otherNode
        && thisChannel == other.thisChannel
        && otherChannel == other.otherChannel;
}

//==============================================================================
bool AudioProcessorGraph::Node::isBypassed() const noexcept
{
    if (processor != nullptr)
    {
        if (auto* bypassParam = processor->getBypassParameter())
            return (bypassParam->getValue() != 0.0f);
    }

    return bypassed;
}

void AudioProcessorGraph::Node::setBypassed (bool shouldBeBypassed) noexcept
{
    if (processor != nullptr)
    {
        if (auto* bypassParam = processor->getBypassParameter())
            bypassParam->setValueNotifyingHost (shouldBeBypassed ? 1.0f : 0.0f);
    }

    bypassed = shouldBeBypassed;
}

//==============================================================================
struct AudioProcessorGraph::RenderSequenceFloat   : public GraphRenderSequence<float> {};
struct AudioProcessorGraph::RenderSequenceDouble  : public GraphRenderSequence<double> {};

//==============================================================================
AudioProcessorGraph::AudioProcessorGraph()
{
}

AudioProcessorGraph::~AudioProcessorGraph()
{
    cancelPendingUpdate();
    clearRenderingSequence();
    clear();
}

const String AudioProcessorGraph::getName() const
{
    return "Audio Graph";
}

//==============================================================================
void AudioProcessorGraph::topologyChanged()
{
    sendChangeMessage();

    if (isPrepared)
        updateOnMessageThread (*this);
}

void AudioProcessorGraph::clear()
{
    const ScopedLock sl (getCallbackLock());

    if (nodes.isEmpty())
        return;

    nodes.clear();
    topologyChanged();
}

AudioProcessorGraph::Node* AudioProcessorGraph::getNodeForId (NodeID nodeID) const
{
    for (auto* n : nodes)
        if (n->nodeID == nodeID)
            return n;

    return {};
}

AudioProcessorGraph::Node::Ptr AudioProcessorGraph::addNode (std::unique_ptr<AudioProcessor> newProcessor, NodeID nodeID)
{
    if (newProcessor == nullptr || newProcessor.get() == this)
    {
        jassertfalse;
        return {};
    }

    if (nodeID == NodeID())
        nodeID.uid = ++(lastNodeID.uid);

    for (auto* n : nodes)
    {
        if (n->getProcessor() == newProcessor.get() || n->nodeID == nodeID)
        {
            jassertfalse; // Cannot add two copies of the same processor, or duplicate node IDs!
            return {};
        }
    }

    if (lastNodeID < nodeID)
        lastNodeID = nodeID;

    newProcessor->setPlayHead (getPlayHead());

    Node::Ptr n (new Node (nodeID, std::move (newProcessor)));

    {
        const ScopedLock sl (getCallbackLock());
        nodes.add (n.get());
    }

    n->setParentGraph (this);
    topologyChanged();
    return n;
}

bool AudioProcessorGraph::removeNode (NodeID nodeId)
{
    const ScopedLock sl (getCallbackLock());

    for (int i = nodes.size(); --i >= 0;)
    {
        if (nodes.getUnchecked(i)->nodeID == nodeId)
        {
            disconnectNode (nodeId);
            nodes.remove (i);
            topologyChanged();
            return true;
        }
    }

    return false;
}

bool AudioProcessorGraph::removeNode (Node* node)
{
    if (node != nullptr)
        return removeNode (node->nodeID);

    jassertfalse;
    return false;
}

//==============================================================================
void AudioProcessorGraph::getNodeConnections (Node& node, std::vector<Connection>& connections)
{
    for (auto& i : node.inputs)
        connections.push_back ({ { i.otherNode->nodeID, i.otherChannel }, { node.nodeID, i.thisChannel } });

    for (auto& o : node.outputs)
        connections.push_back ({ { node.nodeID, o.thisChannel }, { o.otherNode->nodeID, o.otherChannel } });
}

std::vector<AudioProcessorGraph::Connection> AudioProcessorGraph::getConnections() const
{
    std::vector<Connection> connections;

    for (auto& n : nodes)
        getNodeConnections (*n, connections);

    std::sort (connections.begin(), connections.end());
    auto last = std::unique (connections.begin(), connections.end());
    connections.erase (last, connections.end());

    return connections;
}

bool AudioProcessorGraph::isConnected (Node* source, int sourceChannel, Node* dest, int destChannel) const noexcept
{
    for (auto& o : source->outputs)
        if (o.otherNode == dest && o.thisChannel == sourceChannel && o.otherChannel == destChannel)
            return true;

    return false;
}

bool AudioProcessorGraph::isConnected (const Connection& c) const noexcept
{
    if (auto* source = getNodeForId (c.source.nodeID))
        if (auto* dest = getNodeForId (c.destination.nodeID))
            return isConnected (source, c.source.channelIndex,
                                dest, c.destination.channelIndex);

    return false;
}

bool AudioProcessorGraph::isConnected (NodeID srcID, NodeID destID) const noexcept
{
    if (auto* source = getNodeForId (srcID))
        if (auto* dest = getNodeForId (destID))
            for (auto& out : source->outputs)
                if (out.otherNode == dest)
                    return true;

    return false;
}

bool AudioProcessorGraph::isAnInputTo (Node& src, Node& dst) const noexcept
{
    jassert (nodes.contains (&src));
    jassert (nodes.contains (&dst));

    return isAnInputTo (src, dst, nodes.size());
}

bool AudioProcessorGraph::isAnInputTo (Node& src, Node& dst, int recursionCheck) const noexcept
{
    for (auto&& i : dst.inputs)
        if (i.otherNode == &src)
            return true;

    if (recursionCheck > 0)
        for (auto&& i : dst.inputs)
            if (isAnInputTo (src, *i.otherNode, recursionCheck - 1))
                return true;

    return false;
}

bool AudioProcessorGraph::canConnect (Node* source, int sourceChannel, Node* dest, int destChannel) const noexcept
{
    bool sourceIsMIDI = sourceChannel == midiChannelIndex;
    bool destIsMIDI   = destChannel == midiChannelIndex;

    if (sourceChannel < 0
         || destChannel < 0
         || source == dest
         || sourceIsMIDI != destIsMIDI)
        return false;

    if (source == nullptr
         || (! sourceIsMIDI && sourceChannel >= source->processor->getTotalNumOutputChannels())
         || (sourceIsMIDI && ! source->processor->producesMidi()))
        return false;

    if (dest == nullptr
         || (! destIsMIDI && destChannel >= dest->processor->getTotalNumInputChannels())
         || (destIsMIDI && ! dest->processor->acceptsMidi()))
        return false;

    return ! isConnected (source, sourceChannel, dest, destChannel);
}

bool AudioProcessorGraph::canConnect (const Connection& c) const
{
    if (auto* source = getNodeForId (c.source.nodeID))
        if (auto* dest = getNodeForId (c.destination.nodeID))
            return canConnect (source, c.source.channelIndex,
                               dest, c.destination.channelIndex);

    return false;
}

bool AudioProcessorGraph::addConnection (const Connection& c)
{
    if (auto* source = getNodeForId (c.source.nodeID))
    {
        if (auto* dest = getNodeForId (c.destination.nodeID))
        {
            auto sourceChan = c.source.channelIndex;
            auto destChan = c.destination.channelIndex;

            if (canConnect (source, sourceChan, dest, destChan))
            {
                source->outputs.add ({ dest, destChan, sourceChan });
                dest->inputs.add ({ source, sourceChan, destChan });
                jassert (isConnected (c));
                topologyChanged();
                return true;
            }
        }
    }

    return false;
}

bool AudioProcessorGraph::removeConnection (const Connection& c)
{
    if (auto* source = getNodeForId (c.source.nodeID))
    {
        if (auto* dest = getNodeForId (c.destination.nodeID))
        {
            auto sourceChan = c.source.channelIndex;
            auto destChan = c.destination.channelIndex;

            if (isConnected (source, sourceChan, dest, destChan))
            {
                source->outputs.removeAllInstancesOf ({ dest, destChan, sourceChan });
                dest->inputs.removeAllInstancesOf ({ source, sourceChan, destChan });
                topologyChanged();
                return true;
            }
        }
    }

    return false;
}

bool AudioProcessorGraph::disconnectNode (NodeID nodeID)
{
    if (auto* node = getNodeForId (nodeID))
    {
        std::vector<Connection> connections;
        getNodeConnections (*node, connections);

        if (! connections.empty())
        {
            for (auto c : connections)
                removeConnection (c);

            return true;
        }
    }

    return false;
}

bool AudioProcessorGraph::isLegal (Node* source, int sourceChannel, Node* dest, int destChannel) const noexcept
{
    return (sourceChannel == midiChannelIndex ? source->processor->producesMidi()
                                              : isPositiveAndBelow (sourceChannel, source->processor->getTotalNumOutputChannels()))
        && (destChannel == midiChannelIndex ? dest->processor->acceptsMidi()
                                            : isPositiveAndBelow (destChannel, dest->processor->getTotalNumInputChannels()));
}

bool AudioProcessorGraph::isConnectionLegal (const Connection& c) const
{
    if (auto* source = getNodeForId (c.source.nodeID))
        if (auto* dest = getNodeForId (c.destination.nodeID))
            return isLegal (source, c.source.channelIndex, dest, c.destination.channelIndex);

    return false;
}

bool AudioProcessorGraph::removeIllegalConnections()
{
    bool anyRemoved = false;

    for (auto* node : nodes)
    {
        std::vector<Connection> connections;
        getNodeConnections (*node, connections);

        for (auto c : connections)
            if (! isConnectionLegal (c))
                anyRemoved = removeConnection (c) || anyRemoved;
    }

    return anyRemoved;
}

//==============================================================================
void AudioProcessorGraph::clearRenderingSequence()
{
    std::unique_ptr<RenderSequenceFloat> oldSequenceF;
    std::unique_ptr<RenderSequenceDouble> oldSequenceD;

    {
        const ScopedLock sl (getCallbackLock());
        std::swap (renderSequenceFloat, oldSequenceF);
        std::swap (renderSequenceDouble, oldSequenceD);
    }
}

bool AudioProcessorGraph::anyNodesNeedPreparing() const noexcept
{
    for (auto* node : nodes)
        if (! node->isPrepared)
            return true;

    return false;
}

void AudioProcessorGraph::buildRenderingSequence()
{
    auto newSequenceF = std::make_unique<RenderSequenceFloat>();
    auto newSequenceD = std::make_unique<RenderSequenceDouble>();

    RenderSequenceBuilder<RenderSequenceFloat>  builderF (*this, *newSequenceF);
    RenderSequenceBuilder<RenderSequenceDouble> builderD (*this, *newSequenceD);

    const ScopedLock sl (getCallbackLock());

    const auto currentBlockSize = getBlockSize();
    newSequenceF->prepareBuffers (currentBlockSize);
    newSequenceD->prepareBuffers (currentBlockSize);

    if (anyNodesNeedPreparing())
    {
        renderSequenceFloat.reset();
        renderSequenceDouble.reset();

        for (auto* node : nodes)
            node->prepare (getSampleRate(), currentBlockSize, this, getProcessingPrecision());
    }

    isPrepared = 1;

    std::swap (renderSequenceFloat,  newSequenceF);
    std::swap (renderSequenceDouble, newSequenceD);
}

void AudioProcessorGraph::handleAsyncUpdate()
{
    buildRenderingSequence();
}

//==============================================================================
void AudioProcessorGraph::prepareToPlay (double sampleRate, int estimatedSamplesPerBlock)
{
    {
        const ScopedLock sl (getCallbackLock());
        setRateAndBufferSizeDetails (sampleRate, estimatedSamplesPerBlock);
    }

    clearRenderingSequence();

    updateOnMessageThread (*this);
}

bool AudioProcessorGraph::supportsDoublePrecisionProcessing() const
{
    return true;
}

void AudioProcessorGraph::releaseResources()
{
    const ScopedLock sl (getCallbackLock());

    cancelPendingUpdate();

    isPrepared = 0;

    for (auto* n : nodes)
        n->unprepare();

    if (renderSequenceFloat != nullptr)
        renderSequenceFloat->releaseBuffers();

    if (renderSequenceDouble != nullptr)
        renderSequenceDouble->releaseBuffers();
}

void AudioProcessorGraph::reset()
{
    const ScopedLock sl (getCallbackLock());

    for (auto* n : nodes)
        n->getProcessor()->reset();
}

void AudioProcessorGraph::setNonRealtime (bool isProcessingNonRealtime) noexcept
{
    const ScopedLock sl (getCallbackLock());

    AudioProcessor::setNonRealtime (isProcessingNonRealtime);

    for (auto* n : nodes)
        n->getProcessor()->setNonRealtime (isProcessingNonRealtime);
}

double AudioProcessorGraph::getTailLengthSeconds() const            { return 0; }
bool AudioProcessorGraph::acceptsMidi() const                       { return true; }
bool AudioProcessorGraph::producesMidi() const                      { return true; }
void AudioProcessorGraph::getStateInformation (juce::MemoryBlock&)  {}
void AudioProcessorGraph::setStateInformation (const void*, int)    {}

template <typename FloatType, typename SequenceType>
static void processBlockForBuffer (AudioBuffer<FloatType>& buffer, MidiBuffer& midiMessages,
                                   AudioProcessorGraph& graph,
                                   std::unique_ptr<SequenceType>& renderSequence,
                                   std::atomic<bool>& isPrepared)
{
    if (graph.isNonRealtime())
    {
        while (! isPrepared)
            Thread::sleep (1);

        const ScopedLock sl (graph.getCallbackLock());

        if (renderSequence != nullptr)
            renderSequence->perform (buffer, midiMessages, graph.getPlayHead());
    }
    else
    {
        const ScopedLock sl (graph.getCallbackLock());

        if (isPrepared)
        {
            if (renderSequence != nullptr)
                renderSequence->perform (buffer, midiMessages, graph.getPlayHead());
        }
        else
        {
            buffer.clear();
            midiMessages.clear();
        }
    }
}

void AudioProcessorGraph::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    if ((! isPrepared) && MessageManager::getInstance()->isThisTheMessageThread())
        handleAsyncUpdate();

    processBlockForBuffer<float> (buffer, midiMessages, *this, renderSequenceFloat, isPrepared);
}

void AudioProcessorGraph::processBlock (AudioBuffer<double>& buffer, MidiBuffer& midiMessages)
{
    if ((! isPrepared) && MessageManager::getInstance()->isThisTheMessageThread())
        handleAsyncUpdate();

    processBlockForBuffer<double> (buffer, midiMessages, *this, renderSequenceDouble, isPrepared);
}

//==============================================================================
AudioProcessorGraph::AudioGraphIOProcessor::AudioGraphIOProcessor (const IODeviceType deviceType)
    : type (deviceType)
{
}

AudioProcessorGraph::AudioGraphIOProcessor::~AudioGraphIOProcessor()
{
}

const String AudioProcessorGraph::AudioGraphIOProcessor::getName() const
{
    switch (type)
    {
        case audioOutputNode:   return "Audio Output";
        case audioInputNode:    return "Audio Input";
        case midiOutputNode:    return "Midi Output";
        case midiInputNode:     return "Midi Input";
        default:                break;
    }

    return {};
}

void AudioProcessorGraph::AudioGraphIOProcessor::fillInPluginDescription (PluginDescription& d) const
{
    d.name = getName();
    d.uid = d.name.hashCode();
    d.category = "I/O devices";
    d.pluginFormatName = "Internal";
    d.manufacturerName = "JUCE";
    d.version = "1.0";
    d.isInstrument = false;

    d.numInputChannels = getTotalNumInputChannels();

    if (type == audioOutputNode && graph != nullptr)
        d.numInputChannels = graph->getTotalNumInputChannels();

    d.numOutputChannels = getTotalNumOutputChannels();

    if (type == audioInputNode && graph != nullptr)
        d.numOutputChannels = graph->getTotalNumOutputChannels();
}

void AudioProcessorGraph::AudioGraphIOProcessor::prepareToPlay (double, int)
{
    jassert (graph != nullptr);
}

void AudioProcessorGraph::AudioGraphIOProcessor::releaseResources()
{
}

bool AudioProcessorGraph::AudioGraphIOProcessor::supportsDoublePrecisionProcessing() const
{
    return true;
}

template <typename FloatType, typename SequenceType>
static void processIOBlock (AudioProcessorGraph::AudioGraphIOProcessor& io, SequenceType& sequence,
                            AudioBuffer<FloatType>& buffer, MidiBuffer& midiMessages)
{
    switch (io.getType())
    {
        case AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode:
        {
            auto&& currentAudioOutputBuffer = sequence.currentAudioOutputBuffer;

            for (int i = jmin (currentAudioOutputBuffer.getNumChannels(), buffer.getNumChannels()); --i >= 0;)
                currentAudioOutputBuffer.addFrom (i, 0, buffer, i, 0, buffer.getNumSamples());

            break;
        }

        case AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode:
        {
            auto* currentInputBuffer = sequence.currentAudioInputBuffer;

            for (int i = jmin (currentInputBuffer->getNumChannels(), buffer.getNumChannels()); --i >= 0;)
                buffer.copyFrom (i, 0, *currentInputBuffer, i, 0, buffer.getNumSamples());

            break;
        }

        case AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode:
            sequence.currentMidiOutputBuffer.addEvents (midiMessages, 0, buffer.getNumSamples(), 0);
            break;

        case AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode:
            midiMessages.addEvents (*sequence.currentMidiInputBuffer, 0, buffer.getNumSamples(), 0);
            break;

        default:
            break;
    }
}

void AudioProcessorGraph::AudioGraphIOProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    jassert (graph != nullptr);
    processIOBlock (*this, *graph->renderSequenceFloat, buffer, midiMessages);
}

void AudioProcessorGraph::AudioGraphIOProcessor::processBlock (AudioBuffer<double>& buffer, MidiBuffer& midiMessages)
{
    jassert (graph != nullptr);
    processIOBlock (*this, *graph->renderSequenceDouble, buffer, midiMessages);
}

double AudioProcessorGraph::AudioGraphIOProcessor::getTailLengthSeconds() const
{
    return 0;
}

bool AudioProcessorGraph::AudioGraphIOProcessor::acceptsMidi() const
{
    return type == midiOutputNode;
}

bool AudioProcessorGraph::AudioGraphIOProcessor::producesMidi() const
{
    return type == midiInputNode;
}

bool AudioProcessorGraph::AudioGraphIOProcessor::isInput() const noexcept           { return type == audioInputNode  || type == midiInputNode; }
bool AudioProcessorGraph::AudioGraphIOProcessor::isOutput() const noexcept          { return type == audioOutputNode || type == midiOutputNode; }

bool AudioProcessorGraph::AudioGraphIOProcessor::hasEditor() const                  { return false; }
AudioProcessorEditor* AudioProcessorGraph::AudioGraphIOProcessor::createEditor()    { return nullptr; }

int AudioProcessorGraph::AudioGraphIOProcessor::getNumPrograms()                    { return 0; }
int AudioProcessorGraph::AudioGraphIOProcessor::getCurrentProgram()                 { return 0; }
void AudioProcessorGraph::AudioGraphIOProcessor::setCurrentProgram (int)            { }

String AudioProcessorGraph::AudioGraphIOProcessor::getProgramName (int)       { return {}; }
void AudioProcessorGraph::AudioGraphIOProcessor::changeProgramName (int, const String&) {}

void AudioProcessorGraph::AudioGraphIOProcessor::getStateInformation (juce::MemoryBlock&) {}
void AudioProcessorGraph::AudioGraphIOProcessor::setStateInformation (const void*, int) {}

void AudioProcessorGraph::AudioGraphIOProcessor::setParentGraph (AudioProcessorGraph* const newGraph)
{
    graph = newGraph;

    if (graph != nullptr)
    {
        setPlayConfigDetails (type == audioOutputNode ? graph->getTotalNumOutputChannels() : 0,
                              type == audioInputNode  ? graph->getTotalNumInputChannels()  : 0,
                              getSampleRate(),
                              getBlockSize());

        updateHostDisplay();
    }
}

} // namespace juce
