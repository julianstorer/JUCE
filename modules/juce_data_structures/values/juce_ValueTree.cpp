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

class ValueTree::SharedObject  : public ReferenceCountedObject
{
public:
    using Ptr = ReferenceCountedObjectPtr<SharedObject>;

    explicit SharedObject (const Identifier& t) noexcept  : type (t) {}

    SharedObject (const SharedObject& other)
        : ReferenceCountedObject(), type (other.type), properties (other.properties)
    {
        for (auto* c : other.children)
        {
            auto* child = new SharedObject (*c);
            child->parent = this;
            children.add (child);
        }
    }

    SharedObject& operator= (const SharedObject&) = delete;

    ~SharedObject()
    {
        jassert (parent == nullptr); // this should never happen unless something isn't obeying the ref-counting!

        for (auto i = children.size(); --i >= 0;)
        {
            const Ptr c (children.getObjectPointerUnchecked (i));
            c->parent = nullptr;
            children.remove (i);
            c->sendParentChangeMessage();
        }
    }

    SharedObject& getRoot() noexcept
    {
        return parent == nullptr ? *this : parent->getRoot();
    }

    template <typename Function>
    void callListeners (ValueTree::Listener* listenerToExclude, Function fn) const
    {
        auto numListeners = valueTreesWithListeners.size();

        if (numListeners == 1)
        {
            valueTreesWithListeners.getUnchecked (0)->listeners.callExcluding (listenerToExclude, fn);
        }
        else if (numListeners > 0)
        {
            auto listenersCopy = valueTreesWithListeners;

            for (int i = 0; i < numListeners; ++i)
            {
                auto* v = listenersCopy.getUnchecked (i);

                if (i == 0 || valueTreesWithListeners.contains (v))
                    v->listeners.callExcluding (listenerToExclude, fn);
            }
        }
    }

    template <typename Function>
    void callListenersForAllParents (ValueTree::Listener* listenerToExclude, Function fn) const
    {
        for (auto* t = this; t != nullptr; t = t->parent)
            t->callListeners (listenerToExclude, fn);
    }

    void sendPropertyChangeMessage (const Identifier& property, ValueTree::Listener* listenerToExclude = nullptr)
    {
        ValueTree tree (*this);
        callListenersForAllParents (listenerToExclude, [&] (Listener& l) { l.valueTreePropertyChanged (tree, property); });
    }

    void sendChildAddedMessage (ValueTree child)
    {
        ValueTree tree (*this);
        callListenersForAllParents (nullptr, [&] (Listener& l) { l.valueTreeChildAdded (tree, child); });
    }

    void sendChildRemovedMessage (ValueTree child, int index)
    {
        ValueTree tree (*this);
        callListenersForAllParents (nullptr, [=, &tree, &child] (Listener& l) { l.valueTreeChildRemoved (tree, child, index); });
    }

    void sendChildOrderChangedMessage (int oldIndex, int newIndex)
    {
        ValueTree tree (*this);
        callListenersForAllParents (nullptr, [=, &tree] (Listener& l) { l.valueTreeChildOrderChanged (tree, oldIndex, newIndex); });
    }

    void sendParentChangeMessage()
    {
        ValueTree tree (*this);

        for (auto j = children.size(); --j >= 0;)
            if (auto* child = children.getObjectPointer (j))
                child->sendParentChangeMessage();

        callListeners (nullptr, [&] (Listener& l) { l.valueTreeParentChanged (tree); });
    }

    void setProperty (const Identifier& name, const var& newValue, UndoManager* undoManager,
                      ValueTree::Listener* listenerToExclude = nullptr)
    {
        if (undoManager == nullptr)
        {
            if (properties.set (name, newValue))
                sendPropertyChangeMessage (name, listenerToExclude);
        }
        else
        {
            if (auto* existingValue = properties.getVarPointer (name))
            {
                if (*existingValue != newValue)
                    undoManager->perform (new SetPropertyAction (*this, name, newValue, *existingValue,
                                                                 false, false, listenerToExclude));
            }
            else
            {
                undoManager->perform (new SetPropertyAction (*this, name, newValue, {},
                                                             true, false, listenerToExclude));
            }
        }
    }

    bool hasProperty (const Identifier& name) const noexcept
    {
        return properties.contains (name);
    }

    void removeProperty (const Identifier& name, UndoManager* undoManager)
    {
        if (undoManager == nullptr)
        {
            if (properties.remove (name))
                sendPropertyChangeMessage (name);
        }
        else
        {
            if (properties.contains (name))
                undoManager->perform (new SetPropertyAction (*this, name, {}, properties[name], false, true));
        }
    }

    void removeAllProperties (UndoManager* undoManager)
    {
        if (undoManager == nullptr)
        {
            while (properties.size() > 0)
            {
                auto name = properties.getName (properties.size() - 1);
                properties.remove (name);
                sendPropertyChangeMessage (name);
            }
        }
        else
        {
            for (auto i = properties.size(); --i >= 0;)
                undoManager->perform (new SetPropertyAction (*this, properties.getName (i), {},
                                                             properties.getValueAt (i), false, true));
        }
    }

    void copyPropertiesFrom (const SharedObject& source, UndoManager* undoManager)
    {
        for (auto i = properties.size(); --i >= 0;)
            if (! source.properties.contains (properties.getName (i)))
                removeProperty (properties.getName (i), undoManager);

        for (int i = 0; i < source.properties.size(); ++i)
            setProperty (source.properties.getName (i), source.properties.getValueAt (i), undoManager);
    }

    ValueTree getChildWithName (const Identifier& typeToMatch) const
    {
        for (auto* s : children)
            if (s->type == typeToMatch)
                return ValueTree (*s);

        return {};
    }

    ValueTree getOrCreateChildWithName (const Identifier& typeToMatch, UndoManager* undoManager)
    {
        for (auto* s : children)
            if (s->type == typeToMatch)
                return ValueTree (*s);

        auto newObject = new SharedObject (typeToMatch);
        addChild (newObject, -1, undoManager);
        return ValueTree (*newObject);
    }

    ValueTree getChildWithProperty (const Identifier& propertyName, const var& propertyValue) const
    {
        for (auto* s : children)
            if (s->properties[propertyName] == propertyValue)
                return ValueTree (*s);

        return {};
    }

    bool isAChildOf (const SharedObject* possibleParent) const noexcept
    {
        for (auto* p = parent; p != nullptr; p = p->parent)
            if (p == possibleParent)
                return true;

        return false;
    }

    int indexOf (const ValueTree& child) const noexcept
    {
        return children.indexOf (child.object);
    }

    void addChild (SharedObject* child, int index, UndoManager* undoManager)
    {
        if (child != nullptr && child->parent != this)
        {
            if (child != this && ! isAChildOf (child))
            {
                // You should always make sure that a child is removed from its previous parent before
                // adding it somewhere else - otherwise, it's ambiguous as to whether a different
                // undomanager should be used when removing it from its current parent..
                jassert (child->parent == nullptr);

                if (child->parent != nullptr)
                {
                    jassert (child->parent->children.indexOf (child) >= 0);
                    child->parent->removeChild (child->parent->children.indexOf (child), undoManager);
                }

                if (undoManager == nullptr)
                {
                    children.insert (index, child);
                    child->parent = this;
                    sendChildAddedMessage (ValueTree (*child));
                    child->sendParentChangeMessage();
                }
                else
                {
                    if (! isPositiveAndBelow (index, children.size()))
                        index = children.size();

                    undoManager->perform (new AddOrRemoveChildAction (*this, index, child));
                }
            }
            else
            {
                // You're attempting to create a recursive loop! A node
                // can't be a child of one of its own children!
                jassertfalse;
            }
        }
    }

    void removeChild (int childIndex, UndoManager* undoManager)
    {
        if (auto child = Ptr (children.getObjectPointer (childIndex)))
        {
            if (undoManager == nullptr)
            {
                children.remove (childIndex);
                child->parent = nullptr;
                sendChildRemovedMessage (ValueTree (child), childIndex);
                child->sendParentChangeMessage();
            }
            else
            {
                undoManager->perform (new AddOrRemoveChildAction (*this, childIndex, {}));
            }
        }
    }

    void removeAllChildren (UndoManager* undoManager)
    {
        while (children.size() > 0)
            removeChild (children.size() - 1, undoManager);
    }

    void moveChild (int currentIndex, int newIndex, UndoManager* undoManager)
    {
        // The source index must be a valid index!
        jassert (isPositiveAndBelow (currentIndex, children.size()));

        if (currentIndex != newIndex
             && isPositiveAndBelow (currentIndex, children.size()))
        {
            if (undoManager == nullptr)
            {
                children.move (currentIndex, newIndex);
                sendChildOrderChangedMessage (currentIndex, newIndex);
            }
            else
            {
                if (! isPositiveAndBelow (newIndex, children.size()))
                    newIndex = children.size() - 1;

                undoManager->perform (new MoveChildAction (*this, currentIndex, newIndex));
            }
        }
    }

    void reorderChildren (const OwnedArray<ValueTree>& newOrder, UndoManager* undoManager)
    {
        jassert (newOrder.size() == children.size());

        for (int i = 0; i < children.size(); ++i)
        {
            auto* child = newOrder.getUnchecked (i)->object.get();

            if (children.getObjectPointerUnchecked (i) != child)
            {
                auto oldIndex = children.indexOf (child);
                jassert (oldIndex >= 0);
                moveChild (oldIndex, i, undoManager);
            }
        }
    }

    bool isEquivalentTo (const SharedObject& other) const noexcept
    {
        if (type != other.type
             || properties.size() != other.properties.size()
             || children.size() != other.children.size()
             || properties != other.properties)
            return false;

        for (int i = 0; i < children.size(); ++i)
            if (! children.getObjectPointerUnchecked (i)->isEquivalentTo (*other.children.getObjectPointerUnchecked (i)))
                return false;

        return true;
    }

    XmlElement* createXml() const
    {
        auto* xml = new XmlElement (type);
        properties.copyToXmlAttributes (*xml);

        // (NB: it's faster to add nodes to XML elements in reverse order)
        for (auto i = children.size(); --i >= 0;)
            xml->prependChildElement (children.getObjectPointerUnchecked (i)->createXml());

        return xml;
    }

    void writeToStream (OutputStream& output) const
    {
        output.writeString (type.toString());
        output.writeCompressedInt (properties.size());

        for (int j = 0; j < properties.size(); ++j)
        {
            output.writeString (properties.getName (j).toString());
            properties.getValueAt (j).writeToStream (output);
        }

        output.writeCompressedInt (children.size());

        for (auto* c : children)
            writeObjectToStream (output, c);
    }

    static void writeObjectToStream (OutputStream& output, const SharedObject* object)
    {
        if (object != nullptr)
        {
            object->writeToStream (output);
        }
        else
        {
            output.writeString ({});
            output.writeCompressedInt (0);
            output.writeCompressedInt (0);
        }
    }

    //==============================================================================
    struct SetPropertyAction  : public UndoableAction
    {
        SetPropertyAction (Ptr targetObject, const Identifier& propertyName,
                           const var& newVal, const var& oldVal, bool isAdding, bool isDeleting,
                           ValueTree::Listener* listenerToExclude = nullptr)
            : target (std::move (targetObject)),
              name (propertyName), newValue (newVal), oldValue (oldVal),
              isAddingNewProperty (isAdding), isDeletingProperty (isDeleting),
              excludeListener (listenerToExclude)
        {
        }

        bool perform() override
        {
            jassert (! (isAddingNewProperty && target->hasProperty (name)));

            if (isDeletingProperty)
                target->removeProperty (name, nullptr);
            else
                target->setProperty (name, newValue, nullptr, excludeListener);
            // only exclude on first invocation
            excludeListener = nullptr;
            return true;
        }

        bool undo() override
        {
            if (isAddingNewProperty)
                target->removeProperty (name, nullptr);
            else
                target->setProperty (name, oldValue, nullptr);

            return true;
        }

        int getSizeInUnits() override
        {
            return (int) sizeof (*this); //xxx should be more accurate
        }

        UndoableAction* createCoalescedAction (UndoableAction* nextAction) override
        {
            if (! (isAddingNewProperty || isDeletingProperty))
            {
                if (auto* next = dynamic_cast<SetPropertyAction*> (nextAction))
                    if (next->target == target && next->name == name
                          && ! (next->isAddingNewProperty || next->isDeletingProperty))
                        return new SetPropertyAction (*target, name, next->newValue, oldValue, false, false);
            }

            return nullptr;
        }

    private:
        const Ptr target;
        const Identifier name;
        const var newValue;
        var oldValue;
        const bool isAddingNewProperty : 1, isDeletingProperty : 1;
        ValueTree::Listener* excludeListener;

        JUCE_DECLARE_NON_COPYABLE (SetPropertyAction)
    };

    //==============================================================================
    struct AddOrRemoveChildAction  : public UndoableAction
    {
        AddOrRemoveChildAction (Ptr parentObject, int index, SharedObject* newChild)
            : target (std::move (parentObject)),
              child (newChild != nullptr ? newChild : target->children.getObjectPointer (index)),
              childIndex (index),
              isDeleting (newChild == nullptr)
        {
            jassert (child != nullptr);
        }

        bool perform() override
        {
            if (isDeleting)
                target->removeChild (childIndex, nullptr);
            else
                target->addChild (child.get(), childIndex, nullptr);

            return true;
        }

        bool undo() override
        {
            if (isDeleting)
            {
                target->addChild (child.get(), childIndex, nullptr);
            }
            else
            {
                // If you hit this, it seems that your object's state is getting confused - probably
                // because you've interleaved some undoable and non-undoable operations?
                jassert (childIndex < target->children.size());
                target->removeChild (childIndex, nullptr);
            }

            return true;
        }

        int getSizeInUnits() override
        {
            return (int) sizeof (*this); //xxx should be more accurate
        }

    private:
        const Ptr target, child;
        const int childIndex;
        const bool isDeleting;

        JUCE_DECLARE_NON_COPYABLE (AddOrRemoveChildAction)
    };

    //==============================================================================
    struct MoveChildAction  : public UndoableAction
    {
        MoveChildAction (Ptr parentObject, int fromIndex, int toIndex) noexcept
            : parent (std::move (parentObject)), startIndex (fromIndex), endIndex (toIndex)
        {
        }

        bool perform() override
        {
            parent->moveChild (startIndex, endIndex, nullptr);
            return true;
        }

        bool undo() override
        {
            parent->moveChild (endIndex, startIndex, nullptr);
            return true;
        }

        int getSizeInUnits() override
        {
            return (int) sizeof (*this); //xxx should be more accurate
        }

        UndoableAction* createCoalescedAction (UndoableAction* nextAction) override
        {
            if (auto* next = dynamic_cast<MoveChildAction*> (nextAction))
                if (next->parent == parent && next->startIndex == endIndex)
                    return new MoveChildAction (parent, startIndex, next->endIndex);

            return nullptr;
        }

    private:
        const Ptr parent;
        const int startIndex, endIndex;

        JUCE_DECLARE_NON_COPYABLE (MoveChildAction)
    };

    //==============================================================================
    const Identifier type;
    NamedValueSet properties;
    ReferenceCountedArray<SharedObject> children;
    SortedSet<ValueTree*> valueTreesWithListeners;
    SharedObject* parent = nullptr;

    JUCE_LEAK_DETECTOR (SharedObject)
};

//==============================================================================
ValueTree::ValueTree() noexcept
{
}

JUCE_DECLARE_DEPRECATED_STATIC (const ValueTree ValueTree::invalid;)

ValueTree::ValueTree (const Identifier& type)  : object (new ValueTree::SharedObject (type))
{
    jassert (type.toString().isNotEmpty()); // All objects must be given a sensible type name!
}

ValueTree::ValueTree (const Identifier& type,
                      std::initializer_list<NamedValueSet::NamedValue> properties,
                      std::initializer_list<ValueTree> subTrees)
    : ValueTree (type)
{
    object->properties = NamedValueSet (std::move (properties));

    for (auto& tree : subTrees)
        addChild (tree, -1, nullptr);
}

ValueTree::ValueTree (SharedObject::Ptr so) noexcept  : object (std::move (so)) {}
ValueTree::ValueTree (SharedObject& so) noexcept      : object (so) {}

ValueTree::ValueTree (const ValueTree& other) noexcept  : object (other.object)
{
}

ValueTree& ValueTree::operator= (const ValueTree& other)
{
    if (object != other.object)
    {
        if (listeners.isEmpty())
        {
            object = other.object;
        }
        else
        {
            if (object != nullptr)
                object->valueTreesWithListeners.removeValue (this);

            if (other.object != nullptr)
                other.object->valueTreesWithListeners.add (this);

            object = other.object;

            listeners.call ([this] (Listener& l) { l.valueTreeRedirected (*this); });
        }
    }

    return *this;
}

ValueTree::ValueTree (ValueTree&& other) noexcept
    : object (std::move (other.object))
{
    if (object != nullptr)
        object->valueTreesWithListeners.removeValue (&other);
}

ValueTree::~ValueTree()
{
    if (! listeners.isEmpty() && object != nullptr)
        object->valueTreesWithListeners.removeValue (this);
}

bool ValueTree::operator== (const ValueTree& other) const noexcept
{
    return object == other.object;
}

bool ValueTree::operator!= (const ValueTree& other) const noexcept
{
    return object != other.object;
}

bool ValueTree::isEquivalentTo (const ValueTree& other) const
{
    return object == other.object
            || (object != nullptr && other.object != nullptr
                 && object->isEquivalentTo (*other.object));
}

ValueTree ValueTree::createCopy() const
{
    if (object != nullptr)
        return ValueTree (*new SharedObject (*object));

    return {};
}

void ValueTree::copyPropertiesFrom (const ValueTree& source, UndoManager* undoManager)
{
    jassert (object != nullptr || source.object == nullptr); // Trying to add properties to a null ValueTree will fail!

    if (source.object == nullptr)
        removeAllProperties (undoManager);
    else if (object != nullptr)
        object->copyPropertiesFrom (*(source.object), undoManager);
}

void ValueTree::copyPropertiesAndChildrenFrom (const ValueTree& source, UndoManager* undoManager)
{
    jassert (object != nullptr || source.object == nullptr); // Trying to copy to a null ValueTree will fail!

    copyPropertiesFrom (source, undoManager);
    removeAllChildren (undoManager);

    if (object != nullptr && source.object != nullptr)
        for (auto& child : source.object->children)
            object->addChild (createCopyIfNotNull (child), -1, undoManager);
}

bool ValueTree::hasType (const Identifier& typeName) const noexcept
{
    return object != nullptr && object->type == typeName;
}

Identifier ValueTree::getType() const noexcept
{
    return object != nullptr ? object->type : Identifier();
}

ValueTree ValueTree::getParent() const noexcept
{
    if (object != nullptr)
        if (auto p = object->parent)
            return ValueTree (*p);

    return {};
}

ValueTree ValueTree::getRoot() const noexcept
{
    if (object != nullptr)
        return ValueTree (object->getRoot());

    return {};
}

ValueTree ValueTree::getSibling (int delta) const noexcept
{
    if (object != nullptr)
        if (auto* p = object->parent)
            if (auto* c = p->children.getObjectPointer (p->indexOf (*this) + delta))
                return ValueTree (*c);

    return {};
}

static const var& getNullVarRef() noexcept
{
    static var nullVar;
    return nullVar;
}

const var& ValueTree::operator[] (const Identifier& name) const noexcept
{
    return object == nullptr ? getNullVarRef() : object->properties[name];
}

const var& ValueTree::getProperty (const Identifier& name) const noexcept
{
    return object == nullptr ? getNullVarRef() : object->properties[name];
}

var ValueTree::getProperty (const Identifier& name, const var& defaultReturnValue) const
{
    return object == nullptr ? defaultReturnValue
                             : object->properties.getWithDefault (name, defaultReturnValue);
}

const var* ValueTree::getPropertyPointer (const Identifier& name) const noexcept
{
    return object == nullptr ? nullptr
                             : object->properties.getVarPointer (name);
}

ValueTree& ValueTree::setProperty (const Identifier& name, const var& newValue, UndoManager* undoManager)
{
    return setPropertyExcludingListener (nullptr, name, newValue, undoManager);
}

ValueTree& ValueTree::setPropertyExcludingListener (Listener* listenerToExclude, const Identifier& name,
                                                    const var& newValue, UndoManager* undoManager)
{
    jassert (name.toString().isNotEmpty()); // Must have a valid property name!
    jassert (object != nullptr); // Trying to add a property to a null ValueTree will fail!

    if (object != nullptr)
        object->setProperty (name, newValue, undoManager, listenerToExclude);

    return *this;
}

bool ValueTree::hasProperty (const Identifier& name) const noexcept
{
    return object != nullptr && object->hasProperty (name);
}

void ValueTree::removeProperty (const Identifier& name, UndoManager* undoManager)
{
    if (object != nullptr)
        object->removeProperty (name, undoManager);
}

void ValueTree::removeAllProperties (UndoManager* undoManager)
{
    if (object != nullptr)
        object->removeAllProperties (undoManager);
}

int ValueTree::getNumProperties() const noexcept
{
    return object == nullptr ? 0 : object->properties.size();
}

Identifier ValueTree::getPropertyName (int index) const noexcept
{
    return object == nullptr ? Identifier()
                             : object->properties.getName (index);
}

int ValueTree::getReferenceCount() const noexcept
{
    return object != nullptr ? object->getReferenceCount() : 0;
}

//==============================================================================
struct ValueTreePropertyValueSource  : public Value::ValueSource,
                                       private ValueTree::Listener
{
    ValueTreePropertyValueSource (const ValueTree& vt, const Identifier& prop, UndoManager* um, bool sync)
        : tree (vt), property (prop), undoManager (um), updateSynchronously (sync)
    {
        tree.addListener (this);
    }

    ~ValueTreePropertyValueSource() override
    {
        tree.removeListener (this);
    }

    var getValue() const override                 { return tree[property]; }
    void setValue (const var& newValue) override  { tree.setProperty (property, newValue, undoManager); }

private:
    ValueTree tree;
    const Identifier property;
    UndoManager* const undoManager;
    const bool updateSynchronously;

    void valueTreePropertyChanged (ValueTree& changedTree, const Identifier& changedProperty) override
    {
        if (tree == changedTree && property == changedProperty)
            sendChangeMessage (updateSynchronously);
    }

    void valueTreeChildAdded (ValueTree&, ValueTree&) override {}
    void valueTreeChildRemoved (ValueTree&, ValueTree&, int) override {}
    void valueTreeChildOrderChanged (ValueTree&, int, int) override {}
    void valueTreeParentChanged (ValueTree&) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreePropertyValueSource)
};

Value ValueTree::getPropertyAsValue (const Identifier& name, UndoManager* undoManager, bool updateSynchronously)
{
    return Value (new ValueTreePropertyValueSource (*this, name, undoManager, updateSynchronously));
}

//==============================================================================
int ValueTree::getNumChildren() const noexcept
{
    return object == nullptr ? 0 : object->children.size();
}

ValueTree ValueTree::getChild (int index) const
{
    if (object != nullptr)
        if (auto* c = object->children.getObjectPointer (index))
            return ValueTree (*c);

    return {};
}

ValueTree::Iterator::Iterator (const ValueTree& v, bool isEnd)
   : internal (v.object != nullptr ? (isEnd ? v.object->children.end() : v.object->children.begin()) : nullptr)
{
}

ValueTree::Iterator& ValueTree::Iterator::operator++()
{
    internal = static_cast<SharedObject**> (internal) + 1;
    return *this;
}

bool ValueTree::Iterator::operator== (const Iterator& other) const  { return internal == other.internal; }
bool ValueTree::Iterator::operator!= (const Iterator& other) const  { return internal != other.internal; }

ValueTree ValueTree::Iterator::operator*() const
{
    return ValueTree (SharedObject::Ptr (*static_cast<SharedObject**> (internal)));
}

ValueTree::Iterator ValueTree::begin() const noexcept   { return Iterator (*this, false); }
ValueTree::Iterator ValueTree::end() const noexcept     { return Iterator (*this, true); }

ValueTree ValueTree::getChildWithName (const Identifier& type) const
{
    return object != nullptr ? object->getChildWithName (type) : ValueTree();
}

ValueTree ValueTree::getOrCreateChildWithName (const Identifier& type, UndoManager* undoManager)
{
    return object != nullptr ? object->getOrCreateChildWithName (type, undoManager) : ValueTree();
}

ValueTree ValueTree::getChildWithProperty (const Identifier& propertyName, const var& propertyValue) const
{
    return object != nullptr ? object->getChildWithProperty (propertyName, propertyValue) : ValueTree();
}

bool ValueTree::isAChildOf (const ValueTree& possibleParent) const noexcept
{
    return object != nullptr && object->isAChildOf (possibleParent.object.get());
}

int ValueTree::indexOf (const ValueTree& child) const noexcept
{
    return object != nullptr ? object->indexOf (child) : -1;
}

void ValueTree::addChild (const ValueTree& child, int index, UndoManager* undoManager)
{
    jassert (object != nullptr); // Trying to add a child to a null ValueTree!

    if (object != nullptr)
        object->addChild (child.object.get(), index, undoManager);
}

void ValueTree::appendChild (const ValueTree& child, UndoManager* undoManager)
{
    addChild (child, -1, undoManager);
}

void ValueTree::removeChild (int childIndex, UndoManager* undoManager)
{
    if (object != nullptr)
        object->removeChild (childIndex, undoManager);
}

void ValueTree::removeChild (const ValueTree& child, UndoManager* undoManager)
{
    if (object != nullptr)
        object->removeChild (object->children.indexOf (child.object), undoManager);
}

void ValueTree::removeAllChildren (UndoManager* undoManager)
{
    if (object != nullptr)
        object->removeAllChildren (undoManager);
}

void ValueTree::moveChild (int currentIndex, int newIndex, UndoManager* undoManager)
{
    if (object != nullptr)
        object->moveChild (currentIndex, newIndex, undoManager);
}

//==============================================================================
void ValueTree::createListOfChildren (OwnedArray<ValueTree>& list) const
{
    if (object != nullptr)
        for (auto* o : object->children)
            if (o != nullptr)
                list.add (new ValueTree (*o));
}

void ValueTree::reorderChildren (const OwnedArray<ValueTree>& newOrder, UndoManager* undoManager)
{
    if (object != nullptr)
        object->reorderChildren (newOrder, undoManager);
}

//==============================================================================
void ValueTree::addListener (Listener* listener)
{
    if (listener != nullptr)
    {
        if (listeners.isEmpty() && object != nullptr)
            object->valueTreesWithListeners.add (this);

        listeners.add (listener);
    }
}

void ValueTree::removeListener (Listener* listener)
{
    listeners.remove (listener);

    if (listeners.isEmpty() && object != nullptr)
        object->valueTreesWithListeners.removeValue (this);
}

void ValueTree::sendPropertyChangeMessage (const Identifier& property)
{
    if (object != nullptr)
        object->sendPropertyChangeMessage (property);
}

//==============================================================================
std::unique_ptr<XmlElement> ValueTree::createXml() const
{
    return std::unique_ptr<XmlElement> (object != nullptr ? object->createXml() : nullptr);
}

ValueTree ValueTree::fromXml (const XmlElement& xml)
{
    if (! xml.isTextElement())
    {
        ValueTree v (xml.getTagName());
        v.object->properties.setFromXmlAttributes (xml);

        for (auto* e : xml.getChildIterator())
            v.appendChild (fromXml (*e), nullptr);

        return v;
    }

    // ValueTrees don't have any equivalent to XML text elements!
    jassertfalse;
    return {};
}

ValueTree ValueTree::fromXml (const String& xmlText)
{
    if (auto xml = parseXML (xmlText))
        return fromXml (*xml);

    return {};
}

String ValueTree::toXmlString (const XmlElement::TextFormat& format) const
{
    if (auto xml = createXml())
        return xml->toString (format);

    return {};
}

//==============================================================================
void ValueTree::writeToStream (OutputStream& output) const
{
    SharedObject::writeObjectToStream (output, object.get());
}

ValueTree ValueTree::readFromStream (InputStream& input)
{
    auto type = input.readString();

    if (type.isEmpty())
        return {};

    ValueTree v (type);

    auto numProps = input.readCompressedInt();

    if (numProps < 0)
    {
        jassertfalse;  // trying to read corrupted data!
        return v;
    }

    for (int i = 0; i < numProps; ++i)
    {
        auto name = input.readString();

        if (name.isNotEmpty())
            v.object->properties.set (name, var::readFromStream (input));
        else
            jassertfalse;  // trying to read corrupted data!
    }

    auto numChildren = input.readCompressedInt();
    v.object->children.ensureStorageAllocated (numChildren);

    for (int i = 0; i < numChildren; ++i)
    {
        auto child = readFromStream (input);

        if (! child.isValid())
            return v;

        v.object->children.add (child.object);
        child.object->parent = v.object.get();
    }

    return v;
}

ValueTree ValueTree::readFromData (const void* data, size_t numBytes)
{
    MemoryInputStream in (data, numBytes, false);
    return readFromStream (in);
}

ValueTree ValueTree::readFromGZIPData (const void* data, size_t numBytes)
{
    MemoryInputStream in (data, numBytes, false);
    GZIPDecompressorInputStream gzipStream (in);
    return readFromStream (gzipStream);
}

void ValueTree::Listener::valueTreePropertyChanged   (ValueTree&, const Identifier&) {}
void ValueTree::Listener::valueTreeChildAdded        (ValueTree&, ValueTree&)        {}
void ValueTree::Listener::valueTreeChildRemoved      (ValueTree&, ValueTree&, int)   {}
void ValueTree::Listener::valueTreeChildOrderChanged (ValueTree&, int, int)          {}
void ValueTree::Listener::valueTreeParentChanged     (ValueTree&)                    {}
void ValueTree::Listener::valueTreeRedirected        (ValueTree&)                    {}


//==============================================================================
//==============================================================================
#if JUCE_UNIT_TESTS

class ValueTreeTests  : public UnitTest
{
public:
    ValueTreeTests()
        : UnitTest ("ValueTrees", UnitTestCategories::values)
    {}

    static String createRandomIdentifier (Random& r)
    {
        char buffer[50] = { 0 };
        const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-:";

        for (int i = 1 + r.nextInt (numElementsInArray (buffer) - 2); --i >= 0;)
            buffer[i] = chars[r.nextInt (sizeof (chars) - 1)];

        String result (buffer);

        if (! XmlElement::isValidXmlName (result))
            result = createRandomIdentifier (r);

        return result;
    }

    static String createRandomWideCharString (Random& r)
    {
        juce_wchar buffer[50] = { 0 };

        for (int i = r.nextInt (numElementsInArray (buffer) - 1); --i >= 0;)
        {
            if (r.nextBool())
            {
                do
                {
                    buffer[i] = (juce_wchar) (1 + r.nextInt (0x10ffff - 1));
                }
                while (! CharPointer_UTF16::canRepresent (buffer[i]));
            }
            else
                buffer[i] = (juce_wchar) (1 + r.nextInt (0x7e));
        }

        return CharPointer_UTF32 (buffer);
    }

    static ValueTree createRandomTree (UndoManager* undoManager, int depth, Random& r)
    {
        ValueTree v (createRandomIdentifier (r));

        for (int i = r.nextInt (10); --i >= 0;)
        {
            switch (r.nextInt (5))
            {
                case 0: v.setProperty (createRandomIdentifier (r), createRandomWideCharString (r), undoManager); break;
                case 1: v.setProperty (createRandomIdentifier (r), r.nextInt(), undoManager); break;
                case 2: if (depth < 5) v.addChild (createRandomTree (undoManager, depth + 1, r), r.nextInt (v.getNumChildren() + 1), undoManager); break;
                case 3: v.setProperty (createRandomIdentifier (r), r.nextBool(), undoManager); break;
                case 4: v.setProperty (createRandomIdentifier (r), r.nextDouble(), undoManager); break;
                default: break;
            }
        }

        return v;
    }

    void runTest() override
    {
        {
            beginTest ("ValueTree");

            auto r = getRandom();

            for (int i = 10; --i >= 0;)
            {
                MemoryOutputStream mo;
                auto v1 = createRandomTree (nullptr, 0, r);
                v1.writeToStream (mo);

                MemoryInputStream mi (mo.getData(), mo.getDataSize(), false);
                auto v2 = ValueTree::readFromStream (mi);
                expect (v1.isEquivalentTo (v2));

                MemoryOutputStream zipped;
                {
                    GZIPCompressorOutputStream zippedOut (zipped);
                    v1.writeToStream (zippedOut);
                }
                expect (v1.isEquivalentTo (ValueTree::readFromGZIPData (zipped.getData(), zipped.getDataSize())));

                auto xml1 = v1.createXml();
                auto xml2 = v2.createCopy().createXml();
                expect (xml1->isEquivalentTo (xml2.get(), false));

                auto v4 = v2.createCopy();
                expect (v1.isEquivalentTo (v4));
            }
        }

        {
            beginTest ("Float formatting");

            ValueTree testVT ("Test");
            Identifier number ("number");

            std::map<double, String> tests;
            tests[1] = "1.0";
            tests[1.1] = "1.1";
            tests[1.01] = "1.01";
            tests[0.76378] = "0.76378";
            tests[-10] = "-10.0";
            tests[10.01] = "10.01";
            tests[0.0123] = "0.0123";
            tests[-3.7e-27] = "-3.7e-27";
            tests[1e+40] = "1.0e40";
            tests[-12345678901234567.0] = "-1.234567890123457e16";
            tests[192000] = "192000.0";
            tests[1234567] = "1.234567e6";
            tests[0.00006] = "0.00006";
            tests[0.000006] = "6.0e-6";

            for (auto& test : tests)
            {
                testVT.setProperty (number, test.first, nullptr);
                auto lines = StringArray::fromLines (testVT.toXmlString());
                lines.removeEmptyStrings();
                auto numLines = lines.size();
                expect (numLines > 1);
                expectEquals (lines[numLines - 1], "<Test number=\"" + test.second + "\"/>");
            }
        }
    }
};

static ValueTreeTests valueTreeTests;

#endif

} // namespace juce
