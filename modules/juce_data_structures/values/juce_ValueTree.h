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

namespace juce
{

//==============================================================================
/**
    A powerful tree structure that can be used to hold free-form data, and which can
    handle its own undo and redo behaviour.

    A ValueTree contains a list of named properties as var objects, and also holds
    any number of sub-trees.

    Create ValueTree objects on the stack, and don't be afraid to copy them around, as
    they're simply a lightweight reference to a shared data container. Creating a copy
    of another ValueTree simply creates a new reference to the same underlying object - to
    make a separate, deep copy of a tree you should explicitly call createCopy().

    Each ValueTree has a type name, in much the same way as an XmlElement has a tag name,
    and much of the structure of a ValueTree is similar to an XmlElement tree.
    You can convert a ValueTree to and from an XmlElement, and as long as the XML doesn't
    contain text elements, the conversion works well and makes a good serialisation
    format. They can also be serialised to a binary format, which is very fast and compact.

    All the methods that change data take an optional UndoManager, which will be used
    to track any changes to the object. For this to work, you have to be careful to
    consistently always use the same UndoManager for all operations to any sub-tree inside
    the tree.

    A ValueTree can only be a child of one parent at a time, so if you're moving one from
    one tree to another, be careful to always remove it first, before adding it. This
    could also mess up your undo/redo chain, so be wary! In a debug build you should hit
    assertions if you try to do anything dangerous, but there are still plenty of ways it
    could go wrong.

    Note that although the children in a tree have a fixed order, the properties are not
    guaranteed to be stored in any particular order, so don't expect that a property's index
    will correspond to the order in which the property was added, or that it will remain
    constant when other properties are added or removed.

    Listeners can be added to a ValueTree to be told when properties change and when
    sub-trees are added or removed.

    @see var, XmlElement

    @tags{DataStructures}
*/
class JUCE_API  ValueTree  final
{
    JUCE_PUBLIC_IN_DLL_BUILD (class SharedObject)

public:
    //==============================================================================
    /** Creates an empty, invalid ValueTree.

        A ValueTree that is created with this constructor can't actually be used for anything,
        it's just a default 'null' ValueTree that can be returned to indicate some sort of failure.
        To create a real one, use the constructor that takes a string.
    */
    ValueTree() noexcept;

    /** Creates an empty ValueTree with the given type name.

        Like an XmlElement, each ValueTree has a type, which you can access with
        getType() and hasType().
    */
    explicit ValueTree (const Identifier& type);

    /** Creates a value tree from nested lists of properties and ValueTrees.

        This code,

        @code
        ValueTree groups
        { "ParameterGroups", {},
          {
            { "Group", {{ "name", "Tone Controls" }},
              {
                { "Parameter", {{ "id", "distortion" }, { "value", 0.5 }}},
                { "Parameter", {{ "id", "reverb" },     { "value", 0.5 }}}
              }
            },
            { "Group", {{ "name", "Other Controls" }},
              {
                { "Parameter", {{ "id", "drywet" }, { "value", 0.5 }}},
                { "Parameter", {{ "id", "gain" },   { "value", 0.5 }}}
              }
            }
          }
        };
        @endcode

        produces this tree:

        @verbatim
        <ParameterGroups>
          <Group name="Tone Controls">
            <Parameter id="distortion" value="0.5"/>
            <Parameter id="reverb" value="0.5"/>
          </Group>
          <Group name="Other Controls">
            <Parameter id="drywet" value="0.5"/>
            <Parameter id="gain" value="0.5"/>
          </Group>
        </ParameterGroups>
        @endverbatim
    */
    ValueTree (const Identifier& type,
               std::initializer_list<NamedValueSet::NamedValue> properties,
               std::initializer_list<ValueTree> subTrees = {});

    /** Creates a reference to another ValueTree. */
    ValueTree (const ValueTree&) noexcept;

    /** Move constructor */
    ValueTree (ValueTree&&) noexcept;

    /** Changes this object to be a reference to the given tree.
        Note that calling this just points this at the new object and invokes the
        Listener::valueTreeRedirected callback, but it's not an undoable operation. If
        you're trying to replace an entire tree in an undoable way, you probably want
        to use copyPropertiesAndChildrenFrom() instead.
    */
    ValueTree& operator= (const ValueTree&);

    /** Destructor. */
    ~ValueTree();

    /** Returns true if both this and the other tree refer to the same underlying structure.
        Note that this isn't a value comparison - two independently-created trees which
        contain identical data are NOT considered equal.
    */
    bool operator== (const ValueTree&) const noexcept;

    /** Returns true if this and the other tree refer to different underlying structures.
        Note that this isn't a value comparison - two independently-created trees which
        contain identical data are not considered equal.
    */
    bool operator!= (const ValueTree&) const noexcept;

    /** Performs a deep comparison between the properties and children of two trees.
        If all the properties and children of the two trees are the same (recursively), this
        returns true.
        The normal operator==() only checks whether two trees refer to the same shared data
        structure, so use this method if you need to do a proper value comparison.
    */
    bool isEquivalentTo (const ValueTree&) const;

    //==============================================================================
    /** Returns true if this tree refers to some valid data.
        An invalid tree is one that was created with the default constructor.
    */
    bool isValid() const noexcept                           { return object != nullptr; }

    /** Returns a deep copy of this tree and all its sub-trees. */
    ValueTree createCopy() const;

    /** Overwrites all the properties in this tree with the properties of the source tree.
        Any properties that already exist will be updated; and new ones will be added, and
        any that are not present in the source tree will be removed.
        @see copyPropertiesAndChildrenFrom
    */
    void copyPropertiesFrom (const ValueTree& source, UndoManager* undoManager);

    /** Replaces all children and properties of this object with copies of those from
        the source object.
        @see copyPropertiesFrom
    */
    void copyPropertiesAndChildrenFrom (const ValueTree& source, UndoManager* undoManager);

    //==============================================================================
    /** Returns the type of this tree.
        The type is specified when the ValueTree is created.
        @see hasType
    */
    Identifier getType() const noexcept;

    /** Returns true if the tree has this type.
        The comparison is case-sensitive.
        @see getType
    */
    bool hasType (const Identifier& typeName) const noexcept;

    //==============================================================================
    /** Returns the value of a named property.
        If no such property has been set, this will return a void variant.
        You can also use operator[] to get a property.
        @see var, setProperty, getPropertyPointer, hasProperty
    */
    const var& getProperty (const Identifier& name) const noexcept;

    /** Returns the value of a named property, or the value of defaultReturnValue
        if the property doesn't exist.
        You can also use operator[] and getProperty to get a property.
        @see var, getProperty, getPropertyPointer, setProperty, hasProperty
    */
    var getProperty (const Identifier& name, const var& defaultReturnValue) const;

    /** Returns a pointer to the value of a named property, or nullptr if the property
        doesn't exist.
        @see var, getProperty, setProperty, hasProperty
    */
    const var* getPropertyPointer (const Identifier& name) const noexcept;

    /** Returns the value of a named property.
        If no such property has been set, this will return a void variant. This is the same as
        calling getProperty().
        @see getProperty
    */
    const var& operator[] (const Identifier& name) const noexcept;

    /** Changes a named property of the tree.
        The name identifier must not be an empty string.
        If the undoManager parameter is not nullptr, its UndoManager::perform() method will be used,
        so that this change can be undone. Be very careful not to mix undoable and non-undoable changes!
        @see var, getProperty, removeProperty
        @returns a reference to the value tree, so that you can daisy-chain calls to this method.
    */
    ValueTree& setProperty (const Identifier& name, const var& newValue, UndoManager* undoManager);

    /** Returns true if the tree contains a named property. */
    bool hasProperty (const Identifier& name) const noexcept;

    /** Removes a property from the tree.
        If the undoManager parameter is not nullptr, its UndoManager::perform() method will be used,
        so that this change can be undone. Be very careful not to mix undoable and non-undoable changes!
    */
    void removeProperty (const Identifier& name, UndoManager* undoManager);

    /** Removes all properties from the tree.
        If the undoManager parameter is not nullptr, its UndoManager::perform() method will be used,
        so that this change can be undone. Be very careful not to mix undoable and non-undoable changes!
    */
    void removeAllProperties (UndoManager* undoManager);

    /** Returns the total number of properties that the tree contains.
        @see getProperty.
    */
    int getNumProperties() const noexcept;

    /** Returns the identifier of the property with a given index.
        Note that properties are not guaranteed to be stored in any particular order, so don't
        expect that the index will correspond to the order in which the property was added, or
        that it will remain constant when other properties are added or removed.
        @see getNumProperties
    */
    Identifier getPropertyName (int index) const noexcept;

    /** Returns a Value object that can be used to control and respond to one of the tree's properties.

        The Value object will maintain a reference to this tree, and will use the undo manager when
        it needs to change the value. Attaching a Value::Listener to the value object will provide
        callbacks whenever the property changes.
        If shouldUpdateSynchronously is true the Value::Listener will be updated synchronously.
        @see ValueSource::sendChangeMessage (bool)
    */
    Value getPropertyAsValue (const Identifier& name, UndoManager* undoManager,
                              bool shouldUpdateSynchronously = false);

    //==============================================================================
    /** Returns the number of child trees inside this one.
        @see getChild
    */
    int getNumChildren() const noexcept;

    /** Returns one of this tree's sub-trees.
        If the index is out of range, it'll return an invalid tree. (You can use isValid() to
        check whether a tree is valid)
    */
    ValueTree getChild (int index) const;

    /** Returns the first sub-tree with the specified type name.
        If no such child tree exists, it'll return an invalid tree. (You can use isValid() to
        check whether a tree is valid)
        @see getOrCreateChildWithName
    */
    ValueTree getChildWithName (const Identifier& type) const;

    /** Returns the first sub-tree with the specified type name, creating and adding
        a child with this name if there wasn't already one there.
        The only time this will return an invalid object is when the object that you're calling
        the method on is itself invalid.
        @see getChildWithName
    */
    ValueTree getOrCreateChildWithName (const Identifier& type, UndoManager* undoManager);

    /** Looks for the first sub-tree that has the specified property value.
        This will scan the child trees in order, until it finds one that has property that matches
        the specified value.
        If no such tree is found, it'll return an invalid object. (You can use isValid() to
        check whether a tree is valid)
    */
    ValueTree getChildWithProperty (const Identifier& propertyName, const var& propertyValue) const;

    /** Adds a child to this tree.
        Make sure that the child being added has first been removed from any former parent before
        calling this, or else you'll hit an assertion.
        If the index is < 0 or greater than the current number of sub-trees, the new one will be
        added at the end of the list.
        If the undoManager parameter is not nullptr, its UndoManager::perform() method will be used,
        so that this change can be undone. Be very careful not to mix undoable and non-undoable changes!
        @see appendChild, removeChild
        @returns a reference to the value tree, so that you can daisy-chain calls to this method.
    */
    ValueTree& addChild (const ValueTree& child, int index, UndoManager* undoManager);

    /** Appends a new child sub-tree to this tree.
        This is equivalent to calling addChild() with an index of -1. See addChild() for more details.
        @see addChild, removeChild
        @returns a reference to the value tree, so that you can daisy-chain calls to this method.
    */
    ValueTree& appendChild (const ValueTree& child, UndoManager* undoManager);

    /** Removes the specified child from this tree's child-list.
        If the undoManager parameter is not nullptr, its UndoManager::perform() method will be used,
        so that this change can be undone. Be very careful not to mix undoable and non-undoable changes!
    */
    void removeChild (const ValueTree& child, UndoManager* undoManager);

    /** Removes a sub-tree from this tree.
        If the index is out-of-range, nothing will be changed.
        If the undoManager parameter is not nullptr, its UndoManager::perform() method will be used,
        so that this change can be undone. Be very careful not to mix undoable and non-undoable changes!
    */
    void removeChild (int childIndex, UndoManager* undoManager);

    /** Removes all child-trees.
        If the undoManager parameter is not nullptr, its UndoManager::perform() method will be used,
        so that this change can be undone. Be very careful not to mix undoable and non-undoable changes!
    */
    void removeAllChildren (UndoManager* undoManager);

    /** Moves one of the sub-trees to a different index.
        This will move the child to a specified index, shuffling along any intervening
        items as required. So for example, if you have a list of { 0, 1, 2, 3, 4, 5 }, then
        calling move (2, 4) would result in { 0, 1, 3, 4, 2, 5 }.

        @param currentIndex     the index of the item to be moved. If this isn't a
                                valid index, then nothing will be done
        @param newIndex         the index at which you'd like this item to end up. If this
                                is less than zero, the value will be moved to the end
                                of the list
        @param undoManager      the optional UndoManager to use to store this transaction
    */
    void moveChild (int currentIndex, int newIndex, UndoManager* undoManager);

    /** Returns true if this tree is a sub-tree (at any depth) of the given parent.
        This searches recursively, so returns true if it's a sub-tree at any level below the parent.
    */
    bool isAChildOf (const ValueTree& possibleParent) const noexcept;

    /** Returns the index of a child item in this parent.
        If the child isn't found, this returns -1.
    */
    int indexOf (const ValueTree& child) const noexcept;

    /** Returns the parent tree that contains this one.
        If the tree has no parent, this will return an invalid object. (You can use isValid() to
        check whether a tree is valid)
    */
    ValueTree getParent() const noexcept;

    /** Recursively finds the highest-level parent tree that contains this one.
        If the tree has no parent, this will return itself.
    */
    ValueTree getRoot() const noexcept;

    /** Returns one of this tree's siblings in its parent's child list.
        The delta specifies how far to move through the list, so a value of 1 would return the tree
        that follows this one, -1 would return the tree before it, 0 will return this one, etc.
        If the requested position is beyond the start or end of the child list, this will return an
        invalid object.
    */
    ValueTree getSibling (int delta) const noexcept;

    //==============================================================================
    /** Iterator for a ValueTree.
        You shouldn't ever need to use this class directly - it's used internally by ValueTree::begin()
        and ValueTree::end() to allow range-based-for loops on a ValueTree.
    */
    struct Iterator
    {
        Iterator (const ValueTree&, bool isEnd);
        Iterator& operator++();

        bool operator== (const Iterator&) const;
        bool operator!= (const Iterator&) const;
        ValueTree operator*() const;

        using difference_type    = std::ptrdiff_t;
        using value_type         = ValueTree;
        using reference          = ValueTree&;
        using pointer            = ValueTree*;
        using iterator_category  = std::forward_iterator_tag;

    private:
        SharedObject** internal = nullptr;
    };

    /** Returns a start iterator for the children in this tree. */
    Iterator begin() const noexcept;

    /** Returns an end iterator for the children in this tree. */
    Iterator end() const noexcept;

    //==============================================================================
    /** Creates an XmlElement that holds a complete image of this tree and all its children.
        If this tree is invalid, this may return nullptr. Otherwise, the XML that is produced can
        be used to recreate a similar tree by calling ValueTree::fromXml().
        @see fromXml, toXmlString
    */
    std::unique_ptr<XmlElement> createXml() const;

    /** Tries to recreate a tree from its XML representation.
        This isn't designed to cope with random XML data - it should only be fed XML that was created
        by the createXml() method.
    */
    static ValueTree fromXml (const XmlElement& xml);

    /** Tries to recreate a tree from its XML representation.
        This isn't designed to cope with random XML data - it should only be fed XML that was created
        by the createXml() method.
    */
    static ValueTree fromXml (const String& xmlText);

    /** This returns a string containing an XML representation of the tree.
        This is quite handy for debugging purposes, as it provides a quick way to view a tree.
        @see createXml()
    */
    String toXmlString (const XmlElement::TextFormat& format = {}) const;

    //==============================================================================
    /** Stores this tree (and all its children) in a binary format.

        Once written, the data can be read back with readFromStream().

        It's much faster to load/save your tree in binary form than as XML, but
        obviously isn't human-readable.
    */
    void writeToStream (OutputStream& output) const;

    /** Reloads a tree from a stream that was written with writeToStream().
        Trees nested more than 256 levels deep are rejected, and an invalid
        tree is returned.
    */
    static ValueTree readFromStream (InputStream& input);

    /** Reloads a tree from a data block that was written with writeToStream(). */
    static ValueTree readFromData (const void* data, size_t numBytes);

    /** Reloads a tree from a data block that was written with writeToStream() and
        then zipped using GZIPCompressorOutputStream.
    */
    static ValueTree readFromGZIPData (const void* data, size_t numBytes);

    //==============================================================================
    /** Listener class for events that happen to a ValueTree.

        To get events from a ValueTree, make your class implement this interface, and use
        ValueTree::addListener() and ValueTree::removeListener() to register it.
    */
    class JUCE_API  Listener
    {
    public:
        /** Destructor. */
        virtual ~Listener() = default;

        /** This method is called when a property of this tree (or of one of its sub-trees) is changed.
            Note that when you register a listener to a tree, it will receive this callback for
            property changes in that tree, and also for any of its children, (recursively, at any depth).
            If your tree has sub-trees but you only want to know about changes to the top level tree,
            simply check the tree parameter in this callback to make sure it's the tree you're interested in.
        */
        virtual void valueTreePropertyChanged (ValueTree& treeWhosePropertyHasChanged,
                                               const Identifier& property);

        /** This method is called when a child sub-tree is added.
            Note that when you register a listener to a tree, it will receive this callback for
            child changes in both that tree and any of its children, (recursively, at any depth).
            If your tree has sub-trees but you only want to know about changes to the top level tree,
            just check the parentTree parameter to make sure it's the one that you're interested in.
        */
        virtual void valueTreeChildAdded (ValueTree& parentTree,
                                          ValueTree& childWhichHasBeenAdded);

        /** This method is called when a child sub-tree is removed.

            Note that when you register a listener to a tree, it will receive this callback for
            child changes in both that tree and any of its children, (recursively, at any depth).
            If your tree has sub-trees but you only want to know about changes to the top level tree,
            just check the parentTree parameter to make sure it's the one that you're interested in.
        */
        virtual void valueTreeChildRemoved (ValueTree& parentTree,
                                            ValueTree& childWhichHasBeenRemoved,
                                            int indexFromWhichChildWasRemoved);

        /** This method is called when a tree's children have been re-shuffled.

            Note that when you register a listener to a tree, it will receive this callback for
            child changes in both that tree and any of its children, (recursively, at any depth).
            If your tree has sub-trees but you only want to know about changes to the top level tree,
            just check the parameter to make sure it's the tree that you're interested in.
        */
        virtual void valueTreeChildOrderChanged (ValueTree& parentTreeWhoseChildrenHaveMoved,
                                                 int oldIndex, int newIndex);

        /** This method is called when a tree has been added or removed from a parent.

            This callback happens when the tree to which the listener was registered is added or
            removed from a parent. Unlike the other callbacks, it applies only to the tree to which
            the listener is registered, and not to any of its children.
        */
        virtual void valueTreeParentChanged (ValueTree& treeWhoseParentHasChanged);

        /** This method is called when a tree is made to point to a different internal shared object.
            When operator= is used to make a ValueTree refer to a different object, this callback
            will be made.
        */
        virtual void valueTreeRedirected (ValueTree& treeWhichHasBeenChanged);
    };

    /** Adds a listener to receive callbacks when this tree is changed in some way.

        The listener is added to this specific ValueTree object, and not to the shared
        object that it refers to. When this object is deleted, all the listeners will
        be lost, even if other references to the same ValueTree still exist. And if you
        use the operator= to make this refer to a different ValueTree, any listeners will
        begin listening to changes to the new tree instead of the old one.

        When you're adding a listener, make sure that you add it to a ValueTree instance that
        will last for as long as you need the listener. In general, you'd never want to add a
        listener to a local stack-based ValueTree, and would usually add one to a member variable.

        @see removeListener
    */
    void addListener (Listener* listener);

    /** Removes a listener that was previously added with addListener(). */
    void removeListener (Listener* listener);

    /** Changes a named property of the tree, but will not notify a specified listener of the change.
        @see setProperty
    */
    ValueTree& setPropertyExcludingListener (Listener* listenerToExclude,
                                             const Identifier& name, const var& newValue,
                                             UndoManager* undoManager);

    /** Causes a property-change callback to be triggered for the specified property,
        calling any listeners that are registered.
    */
    void sendPropertyChangeMessage (const Identifier& property);

    //==============================================================================
    /** This method uses a comparator object to sort the tree's children into order.

        The object provided must have a method of the form:
        @code
        int compareElements (const ValueTree& first, const ValueTree& second);
        @endcode

        ...and this method must return:
          - a value of < 0 if the first comes before the second
          - a value of 0 if the two objects are equivalent
          - a value of > 0 if the second comes before the first

        To improve performance, the compareElements() method can be declared as static or const.

        @param comparator   the comparator to use for comparing elements.
        @param undoManager  optional UndoManager for storing the changes
        @param retainOrderOfEquivalentItems     if this is true, then items which the comparator says are
                            equivalent will be kept in the order in which they currently appear in the array.
                            This is slower to perform, but may be important in some cases. If it's false, a
                            faster algorithm is used, but equivalent elements may be rearranged.
    */
    template <typename ElementComparator>
    void sort (ElementComparator& comparator, UndoManager* undoManager, bool retainOrderOfEquivalentItems)
    {
        if (object != nullptr)
        {
            OwnedArray<ValueTree> sortedList;
            createListOfChildren (sortedList);
            ComparatorAdapter<ElementComparator> adapter (comparator);
            sortedList.sort (adapter, retainOrderOfEquivalentItems);
            reorderChildren (sortedList, undoManager);
        }
    }

    /** Returns the total number of references to the shared underlying data structure that this
        ValueTree is using.
    */
    int getReferenceCount() const noexcept;

   #if JUCE_ALLOW_STATIC_NULL_VARIABLES
    /** @cond */
    /* An invalid ValueTree that can be used if you need to return one as an error condition, etc. */
    [[deprecated ("If you need an empty ValueTree object, just use ValueTree() or {}.")]]
    static const ValueTree invalid;
    /** @endcond */
   #endif

private:
    //==============================================================================
    friend class SharedObject;

    ReferenceCountedObjectPtr<SharedObject> object;
    ListenerList<Listener> listeners;

    template <typename ElementComparator>
    struct ComparatorAdapter
    {
        ComparatorAdapter (ElementComparator& comp) noexcept : comparator (comp) {}

        int compareElements (const ValueTree* const first, const ValueTree* const second)
        {
            return comparator.compareElements (*first, *second);
        }

    private:
        ElementComparator& comparator;
        JUCE_DECLARE_NON_COPYABLE (ComparatorAdapter)
    };

    void createListOfChildren (OwnedArray<ValueTree>&) const;
    void reorderChildren (const OwnedArray<ValueTree>&, UndoManager*);

    explicit ValueTree (ReferenceCountedObjectPtr<SharedObject>) noexcept;
    explicit ValueTree (SharedObject&) noexcept;
};

} // namespace juce
