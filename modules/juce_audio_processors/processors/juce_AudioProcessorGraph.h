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
/**
    A type of AudioProcessor which plays back a graph of other AudioProcessors.

    Use one of these objects if you want to wire-up a set of AudioProcessors
    and play back the result.

    Processors can be added to the graph as "nodes" using addNode(), and once
    added, you can connect any of their input or output channels to other
    nodes using addConnection().

    To play back a graph through an audio device, you might want to use an
    AudioProcessorPlayer object.

    @tags{Audio}
*/
class JUCE_API  AudioProcessorGraph   : public AudioProcessor,
                                        public ChangeBroadcaster,
                                        private AsyncUpdater
{
public:
    //==============================================================================
    /** Creates an empty graph. */
    AudioProcessorGraph();

    /** Destructor.
        Any processor objects that have been added to the graph will also be deleted.
    */
    ~AudioProcessorGraph() override;

    /** Each node in the graph has a UID of this type. */
    struct NodeID
    {
        NodeID() {}
        explicit NodeID (uint32 i) : uid (i) {}

        uint32 uid = 0;

        bool operator== (const NodeID& other) const noexcept    { return uid == other.uid; }
        bool operator!= (const NodeID& other) const noexcept    { return uid != other.uid; }
        bool operator<  (const NodeID& other) const noexcept    { return uid <  other.uid; }
    };

    //==============================================================================
    /** A special index that represents the midi channel of a node.

        This is used as a channel index value if you want to refer to the midi input
        or output instead of an audio channel.
    */
    enum { midiChannelIndex = 0x1000 };

    //==============================================================================
    /**
        Represents an input or output channel of a node in an AudioProcessorGraph.
    */
    struct NodeAndChannel
    {
        NodeID nodeID;
        int channelIndex;

        bool isMIDI() const noexcept                                    { return channelIndex == midiChannelIndex; }

        bool operator== (const NodeAndChannel& other) const noexcept    { return nodeID == other.nodeID && channelIndex == other.channelIndex; }
        bool operator!= (const NodeAndChannel& other) const noexcept    { return ! operator== (other); }
    };

    //==============================================================================
    /** Represents one of the nodes, or processors, in an AudioProcessorGraph.

        To create a node, call AudioProcessorGraph::addNode().
    */
    class JUCE_API  Node   : public ReferenceCountedObject
    {
    public:
        //==============================================================================
        /** The ID number assigned to this node.
            This is assigned by the graph that owns it, and can't be changed.
        */
        const NodeID nodeID;

        /** The actual processor object that this node represents. */
        AudioProcessor* getProcessor() const noexcept           { return processor.get(); }

        /** A set of user-definable properties that are associated with this node.

            This can be used to attach values to the node for whatever purpose seems
            useful. For example, you might store an x and y position if your application
            is displaying the nodes on-screen.
        */
        NamedValueSet properties;

        //==============================================================================
        /** Returns if the node is bypassed or not. */
        bool isBypassed() const noexcept;

        /** Tell this node to bypass processing. */
        void setBypassed (bool shouldBeBypassed) noexcept;

        //==============================================================================
        /** A convenient typedef for referring to a pointer to a node object. */
        using Ptr = ReferenceCountedObjectPtr<Node>;

    private:
        //==============================================================================
        friend class AudioProcessorGraph;
        template <typename Float>
        friend struct GraphRenderSequence;

        struct Connection
        {
            Node* otherNode;
            int otherChannel, thisChannel;

            bool operator== (const Connection&) const noexcept;
        };

        const std::unique_ptr<AudioProcessor> processor;
        Array<Connection> inputs, outputs;
        bool isPrepared = false;
        std::atomic<bool> bypassed { false };

        Node (NodeID, std::unique_ptr<AudioProcessor>) noexcept;

        void setParentGraph (AudioProcessorGraph*) const;
        void prepare (double newSampleRate, int newBlockSize, AudioProcessorGraph*, ProcessingPrecision);
        void unprepare();

        template <typename Sample>
        void processBlock (AudioBuffer<Sample>& audio, MidiBuffer& midi)
        {
            const ScopedLock lock (processorLock);
            processor->processBlock (audio, midi);
        }

        template <typename Sample>
        void processBlockBypassed (AudioBuffer<Sample>& audio, MidiBuffer& midi)
        {
            const ScopedLock lock (processorLock);
            processor->processBlockBypassed (audio, midi);
        }

        CriticalSection processorLock;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Node)
    };

    //==============================================================================
    /** Represents a connection between two channels of two nodes in an AudioProcessorGraph.

        To create a connection, use AudioProcessorGraph::addConnection().
    */
    struct JUCE_API  Connection
    {
        //==============================================================================
        Connection() = default;
        Connection (NodeAndChannel source, NodeAndChannel destination) noexcept;

        Connection (const Connection&) = default;
        Connection& operator= (const Connection&) = default;

        bool operator== (const Connection&) const noexcept;
        bool operator!= (const Connection&) const noexcept;
        bool operator<  (const Connection&) const noexcept;

        //==============================================================================
        /** The channel and node which is the input source for this connection. */
        NodeAndChannel source { {}, 0 };

        /** The channel and node which is the input source for this connection. */
        NodeAndChannel destination { {}, 0 };
    };

    //==============================================================================
    /** Deletes all nodes and connections from this graph.
        Any processor objects in the graph will be deleted.
    */
    void clear();

    /** Returns the array of nodes in the graph. */
    const ReferenceCountedArray<Node>& getNodes() const noexcept    { return nodes; }

    /** Returns the number of nodes in the graph. */
    int getNumNodes() const noexcept                                { return nodes.size(); }

    /** Returns a pointer to one of the nodes in the graph.
        This will return nullptr if the index is out of range.
        @see getNodeForId
    */
    Node::Ptr getNode (int index) const noexcept                    { return nodes[index]; }

    /** Searches the graph for a node with the given ID number and returns it.
        If no such node was found, this returns nullptr.
        @see getNode
    */
    Node* getNodeForId (NodeID) const;

    /** Adds a node to the graph.

        This creates a new node in the graph, for the specified processor. Once you have
        added a processor to the graph, the graph owns it and will delete it later when
        it is no longer needed.

        The optional nodeId parameter lets you specify a unique ID to use for the node.
        If the value is already in use, this method will fail and return an empty node.

        If this succeeds, it returns a pointer to the newly-created node.
    */
    Node::Ptr addNode (std::unique_ptr<AudioProcessor> newProcessor, NodeID nodeId = {});

    /** Deletes a node within the graph which has the specified ID.
        This will also delete any connections that are attached to this node.
    */
    bool removeNode (NodeID);

    /** Deletes a node within the graph.
        This will also delete any connections that are attached to this node.
    */
    bool removeNode (Node*);

    /** Returns the list of connections in the graph. */
    std::vector<Connection> getConnections() const;

    /** Returns true if the given connection exists. */
    bool isConnected (const Connection&) const noexcept;

    /** Returns true if there is a direct connection between any of the channels of
        two specified nodes.
    */
    bool isConnected (NodeID possibleSourceNodeID, NodeID possibleDestNodeID) const noexcept;

    /** Does a recursive check to see if there's a direct or indirect series of connections
        between these two nodes.
    */
    bool isAnInputTo (Node& source, Node& destination) const noexcept;

    /** Returns true if it would be legal to connect the specified points. */
    bool canConnect (const Connection&) const;

    /** Attempts to connect two specified channels of two nodes.

        If this isn't allowed (e.g. because you're trying to connect a midi channel
        to an audio one or other such nonsense), then it'll return false.
    */
    bool addConnection (const Connection&);

    /** Deletes the given connection. */
    bool removeConnection (const Connection&);

    /** Removes all connections from the specified node. */
    bool disconnectNode (NodeID);

    /** Returns true if the given connection's channel numbers map on to valid
        channels at each end.
        Even if a connection is valid when created, its status could change if
        a node changes its channel config.
    */
    bool isConnectionLegal (const Connection&) const;

    /** Performs a sanity checks of all the connections.

        This might be useful if some of the processors are doing things like changing
        their channel counts, which could render some connections obsolete.
    */
    bool removeIllegalConnections();

    //==============================================================================
    /** A special type of AudioProcessor that can live inside an AudioProcessorGraph
        in order to use the audio that comes into and out of the graph itself.

        If you create an AudioGraphIOProcessor in "input" mode, it will act as a
        node in the graph which delivers the audio that is coming into the parent
        graph. This allows you to stream the data to other nodes and process the
        incoming audio.

        Likewise, one of these in "output" mode can be sent data which it will add to
        the sum of data being sent to the graph's output.

        @see AudioProcessorGraph
    */
    class JUCE_API  AudioGraphIOProcessor     : public AudioPluginInstance
    {
    public:
        /** Specifies the mode in which this processor will operate.
        */
        enum IODeviceType
        {
            audioInputNode,     /**< In this mode, the processor has output channels
                                     representing all the audio input channels that are
                                     coming into its parent audio graph. */
            audioOutputNode,    /**< In this mode, the processor has input channels
                                     representing all the audio output channels that are
                                     going out of its parent audio graph. */
            midiInputNode,      /**< In this mode, the processor has a midi output which
                                     delivers the same midi data that is arriving at its
                                     parent graph. */
            midiOutputNode      /**< In this mode, the processor has a midi input and
                                     any data sent to it will be passed out of the parent
                                     graph. */
        };

        //==============================================================================
        /** Returns the mode of this processor. */
        IODeviceType getType() const noexcept                       { return type; }

        /** Returns the parent graph to which this processor belongs, or nullptr if it
            hasn't yet been added to one. */
        AudioProcessorGraph* getParentGraph() const noexcept        { return graph; }

        /** True if this is an audio or midi input. */
        bool isInput() const noexcept;
        /** True if this is an audio or midi output. */
        bool isOutput() const noexcept;

        //==============================================================================
        AudioGraphIOProcessor (IODeviceType);
        ~AudioGraphIOProcessor() override;

        const String getName() const override;
        void fillInPluginDescription (PluginDescription&) const override;
        void prepareToPlay (double newSampleRate, int estimatedSamplesPerBlock) override;
        void releaseResources() override;
        void processBlock (AudioBuffer<float>& , MidiBuffer&) override;
        void processBlock (AudioBuffer<double>&, MidiBuffer&) override;
        bool supportsDoublePrecisionProcessing() const override;

        double getTailLengthSeconds() const override;
        bool acceptsMidi() const override;
        bool producesMidi() const override;

        bool hasEditor() const override;
        AudioProcessorEditor* createEditor() override;

        int getNumPrograms() override;
        int getCurrentProgram() override;
        void setCurrentProgram (int) override;
        String getProgramName (int) override;
        void changeProgramName (int, const String&) override;

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        /** @internal */
        void setParentGraph (AudioProcessorGraph*);

    private:
        const IODeviceType type;
        AudioProcessorGraph* graph = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphIOProcessor)
    };

    //==============================================================================
    const String getName() const override;
    void prepareToPlay (double, int) override;
    void releaseResources() override;
    void processBlock (AudioBuffer<float>&,  MidiBuffer&) override;
    void processBlock (AudioBuffer<double>&, MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override;

    void reset() override;
    void setNonRealtime (bool) noexcept override;

    double getTailLengthSeconds() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;

    bool hasEditor() const override                         { return false; }
    AudioProcessorEditor* createEditor() override           { return nullptr; }
    int getNumPrograms() override                           { return 0; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   { }
    String getProgramName (int) override              { return {}; }
    void changeProgramName (int, const String&) override    { }
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    ReferenceCountedArray<Node> nodes;
    NodeID lastNodeID = {};

    struct RenderSequenceFloat;
    struct RenderSequenceDouble;
    std::unique_ptr<RenderSequenceFloat> renderSequenceFloat;
    std::unique_ptr<RenderSequenceDouble> renderSequenceDouble;

    friend class AudioGraphIOProcessor;

    std::atomic<bool> isPrepared { false };

    void topologyChanged();
    void handleAsyncUpdate() override;
    void clearRenderingSequence();
    void buildRenderingSequence();
    bool anyNodesNeedPreparing() const noexcept;
    bool isConnected (Node* src, int sourceChannel, Node* dest, int destChannel) const noexcept;
    bool isAnInputTo (Node& src, Node& dst, int recursionCheck) const noexcept;
    bool canConnect (Node* src, int sourceChannel, Node* dest, int destChannel) const noexcept;
    bool isLegal (Node* src, int sourceChannel, Node* dest, int destChannel) const noexcept;
    static void getNodeConnections (Node&, std::vector<Connection>&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessorGraph)
};

} // namespace juce
