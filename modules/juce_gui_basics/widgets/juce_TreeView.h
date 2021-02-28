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

class TreeView;


//==============================================================================
/**
    An item in a treeview.

    A TreeViewItem can either be a leaf-node in the tree, or it can contain its
    own sub-items.

    To implement an item that contains sub-items, override the itemOpennessChanged()
    method so that when it is opened, it adds the new sub-items to itself using the
    addSubItem method. Depending on the nature of the item it might choose to only
    do this the first time it's opened, or it might want to refresh itself each time.
    It also has the option of deleting its sub-items when it is closed, or leaving them
    in place.

    @tags{GUI}
*/
class JUCE_API  TreeViewItem
{
public:
    //==============================================================================
    /** Constructor. */
    TreeViewItem();

    /** Destructor. */
    virtual ~TreeViewItem();

    //==============================================================================
    /** Returns the number of sub-items that have been added to this item.
        Note that this doesn't mean much if the node isn't open.
        @see getSubItem, mightContainSubItems, addSubItem
    */
    int getNumSubItems() const noexcept;

    /** Returns one of the item's sub-items.
        Remember that the object returned might get deleted at any time when its parent
        item is closed or refreshed, depending on the nature of the items you're using.

        @see getNumSubItems
    */
    TreeViewItem* getSubItem (int index) const noexcept;

    /** Removes any sub-items. */
    void clearSubItems();

    /** Adds a sub-item.

        @param newItem  the object to add to the item's sub-item list. Once added, these can be
                        found using getSubItem(). When the items are later removed with
                        removeSubItem() (or when this item is deleted), they will be deleted.
        @param insertPosition   the index which the new item should have when it's added. If this
                                value is less than 0, the item will be added to the end of the list.
    */
    void addSubItem (TreeViewItem* newItem, int insertPosition = -1);

    /** Adds a sub-item with a sort-comparator, assuming that the existing items are already sorted.

        @param comparator  the comparator object for sorting - see sortSubItems() for details about
                        the methods this class must provide.
        @param newItem  the object to add to the item's sub-item list. Once added, these can be
                        found using getSubItem(). When the items are later removed with
                        removeSubItem() (or when this item is deleted), they will be deleted.
    */
    template <class ElementComparator>
    void addSubItemSorted (ElementComparator& comparator, TreeViewItem* newItem)
    {
        addSubItem (newItem, findInsertIndexInSortedArray (comparator, subItems.begin(), newItem, 0, subItems.size()));
    }

    /** Removes one of the sub-items.

        @param index        the item to remove
        @param deleteItem   if true, the item that is removed will also be deleted.
    */
    void removeSubItem (int index, bool deleteItem = true);

    /** Sorts the list of sub-items using a standard array comparator.

        This will use a comparator object to sort the elements into order. The comparator
        object must have a method of the form:
        @code
        int compareElements (TreeViewItem* first, TreeViewItem* second);
        @endcode

        ..and this method must return:
          - a value of < 0 if the first comes before the second
          - a value of 0 if the two objects are equivalent
          - a value of > 0 if the second comes before the first

        To improve performance, the compareElements() method can be declared as static or const.
    */
    template <class ElementComparator>
    void sortSubItems (ElementComparator& comparator)
    {
        subItems.sort (comparator);
    }

    //==============================================================================
    /** Returns the TreeView to which this item belongs. */
    TreeView* getOwnerView() const noexcept             { return ownerView; }

    /** Returns the item within which this item is contained. */
    TreeViewItem* getParentItem() const noexcept        { return parentItem; }

    //==============================================================================
    /** True if this item is currently open in the treeview.
        @see getOpenness
    */
    bool isOpen() const noexcept;

    /** Opens or closes the item.

        When opened or closed, the item's itemOpennessChanged() method will be called,
        and a subclass should use this callback to create and add any sub-items that
        it needs to.

        Note that if this is called when the item is in its default openness state, and
        this call would not change whether it's open or closed, then no change will be
        stored. If you want to explicitly set the openness state to be non-default then
        you should use setOpenness instead.

        @see setOpenness, itemOpennessChanged, mightContainSubItems
    */
    void setOpen (bool shouldBeOpen);

    /** An enum of states to describe the explicit or implicit openness of an item. */
    enum Openness
    {
        opennessDefault = 0,
        opennessClosed = 1,
        opennessOpen = 2
    };

    /** Returns the openness state of this item.
        @see isOpen
    */
    Openness getOpenness() const noexcept;

    /** Opens or closes the item.

        If this causes the value of isOpen() to change, then the item's itemOpennessChanged()
        method will be called, and a subclass should use this callback to create and add any
        sub-items that it needs to.

        @see setOpen
    */
    void setOpenness (Openness newOpenness);

    /** True if this item is currently selected.
        Use this when painting the node, to decide whether to draw it as selected or not.
    */
    bool isSelected() const noexcept;

    /** Selects or deselects the item.
        If shouldNotify == sendNotification, then a callback will be made
        to itemSelectionChanged() if the item's selection has changed.
    */
    void setSelected (bool shouldBeSelected,
                      bool deselectOtherItemsFirst,
                      NotificationType shouldNotify = sendNotification);

    /** Returns the rectangle that this item occupies.

        If relativeToTreeViewTopLeft is true, the coordinates are relative to the
        top-left of the TreeView comp, so this will depend on the scroll-position of
        the tree. If false, it is relative to the top-left of the topmost item in the
        tree (so this would be unaffected by scrolling the view).
    */
    Rectangle<int> getItemPosition (bool relativeToTreeViewTopLeft) const noexcept;

    /** Sends a signal to the treeview to make it refresh itself.
        Call this if your items have changed and you want the tree to update to reflect this.
    */
    void treeHasChanged() const noexcept;

    /** Sends a repaint message to redraw just this item.

        Note that you should only call this if you want to repaint a superficial change. If
        you're altering the tree's nodes, you should instead call treeHasChanged().
    */
    void repaintItem() const;

    /** Returns the row number of this item in the tree.
        The row number of an item will change according to which items are open.
        @see TreeView::getNumRowsInTree(), TreeView::getItemOnRow()
    */
    int getRowNumberInTree() const noexcept;

    /** Returns true if all the item's parent nodes are open.
        This is useful to check whether the item might actually be visible or not.
    */
    bool areAllParentsOpen() const noexcept;

    /** Changes whether lines are drawn to connect any sub-items to this item.
        By default, line-drawing is turned on according to LookAndFeel::areLinesDrawnForTreeView().
    */
    void setLinesDrawnForSubItems (bool shouldDrawLines) noexcept;

    //==============================================================================
    /** Tells the tree whether this item can potentially be opened.

        If your item could contain sub-items, this should return true; if it returns
        false then the tree will not try to open the item. This determines whether or
        not the item will be drawn with a 'plus' button next to it.
    */
    virtual bool mightContainSubItems() = 0;

    /** Returns a string to uniquely identify this item.

        If you're planning on using the TreeView::getOpennessState() or
        TreeView::getItemIdentifierString() methods, then these strings will be used
        to identify which nodes are open. The string should be unique amongst the item's
        sibling items, but it's ok for there to be duplicates at other levels of the tree.

        If you're not going to store the openness state nor search for TreeViewItems with
        the TreeView::findItemFromIdentifierString() method, then it's ok not to bother
        implementing this method.
    */
    virtual String getUniqueName() const;

    /** Called when an item is opened or closed.

        When setOpen() is called and the item has specified that it might
        have sub-items with the mightContainSubItems() method, this method
        is called to let the item create or manage its sub-items.

        So when this is called with isNowOpen set to true (i.e. when the item is being
        opened), a subclass might choose to use clearSubItems() and addSubItem() to
        refresh its sub-item list.

        When this is called with isNowOpen set to false, the subclass might want
        to use clearSubItems() to save on space, or it might choose to leave them,
        depending on the nature of the tree.

        You could also use this callback as a trigger to start a background process
        which asynchronously creates sub-items and adds them, if that's more
        appropriate for the task in hand.

        @see mightContainSubItems
    */
    virtual void itemOpennessChanged (bool isNowOpen);

    /** Must return the width required by this item.

        If your item needs to have a particular width in pixels, return that value; if
        you'd rather have it just fill whatever space is available in the treeview,
        return -1.

        If all your items return -1, no horizontal scrollbar will be shown, but if any
        items have fixed widths and extend beyond the width of the treeview, a
        scrollbar will appear.

        Each item can be a different width, but if they change width, you should call
        treeHasChanged() to update the tree.
    */
    virtual int getItemWidth() const                                { return -1; }

    /** Must return the height required by this item.

        This is the height in pixels that the item will take up. Items in the tree
        can be different heights, but if they change height, you should call
        treeHasChanged() to update the tree.
    */
    virtual int getItemHeight() const                               { return 20; }

    /** You can override this method to return false if you don't want to allow the
        user to select this item.
    */
    virtual bool canBeSelected() const                              { return true; }

    /** Creates a component that will be used to represent this item.

        You don't have to implement this method - if it returns nullptr then no component
        will be used for the item, and you can just draw it using the paintItem()
        callback. But if you do return a component, it will be positioned in the
        treeview so that it can be used to represent this item.

        The component returned will be managed by the treeview, so always return
        a new component, and don't keep a reference to it, as the treeview will
        delete it later when it goes off the screen or is no longer needed. Also
        bear in mind that if the component keeps a reference to the item that
        created it, that item could be deleted before the component. Its position
        and size will be completely managed by the tree, so don't attempt to move it
        around.

        Something you may want to do with your component is to give it a pointer to
        the TreeView that created it. This is perfectly safe, and there's no danger
        of it becoming a dangling pointer because the TreeView will always delete
        the component before it is itself deleted.

        As long as you stick to these rules you can return whatever kind of
        component you like. It's most useful if you're doing things like drag-and-drop
        of items, or want to use a Label component to edit item names, etc.
    */
    virtual Component* createItemComponent()                        { return nullptr; }

    //==============================================================================
    /** Draws the item's contents.

        You can choose to either implement this method and draw each item, or you
        can use createItemComponent() to create a component that will represent the
        item.

        If all you need in your tree is to be able to draw the items and detect when
        the user selects or double-clicks one of them, it's probably enough to
        use paintItem(), itemClicked() and itemDoubleClicked(). If you need more
        complicated interactions, you may need to use createItemComponent() instead.

        @param g        the graphics context to draw into
        @param width    the width of the area available for drawing
        @param height   the height of the area available for drawing
    */
    virtual void paintItem (Graphics& g, int width, int height);

    /** Draws the item's open/close button.

        If you don't implement this method, the default behaviour is to call
        LookAndFeel::drawTreeviewPlusMinusBox(), but you can override it for custom
        effects. You may want to override it and call the base-class implementation
        with a different backgroundColour parameter, if your implementation has a
        background colour other than the default (white).
    */
    virtual void paintOpenCloseButton (Graphics&, const Rectangle<float>& area,
                                       Colour backgroundColour, bool isMouseOver);

    /** Draws the line that connects this item to the vertical line extending below its parent. */
    virtual void paintHorizontalConnectingLine (Graphics&, const Line<float>& line);

    /** Draws the line that extends vertically up towards one of its parents, or down to one of its children. */
    virtual void paintVerticalConnectingLine (Graphics&, const Line<float>& line);

    /** Called when the user clicks on this item.

        If you're using createItemComponent() to create a custom component for the
        item, the mouse-clicks might not make it through to the treeview, but this
        is how you find out about clicks when just drawing each item individually.

        The associated mouse-event details are passed in, so you can find out about
        which button, where it was, etc.

        @see itemDoubleClicked
    */
    virtual void itemClicked (const MouseEvent&);

    /** Called when the user double-clicks on this item.

        If you're using createItemComponent() to create a custom component for the
        item, the mouse-clicks might not make it through to the treeview, but this
        is how you find out about clicks when just drawing each item individually.

        The associated mouse-event details are passed in, so you can find out about
        which button, where it was, etc.

        If not overridden, the base class method here will open or close the item as
        if the 'plus' button had been clicked.

        @see itemClicked
    */
    virtual void itemDoubleClicked (const MouseEvent&);

    /** Called when the item is selected or deselected.

        Use this if you want to do something special when the item's selectedness
        changes. By default it'll get repainted when this happens.
    */
    virtual void itemSelectionChanged (bool isNowSelected);

    /** Called when the owner view changes */
    virtual void ownerViewChanged (TreeView* newOwner);

    /** The item can return a tool tip string here if it wants to.
        @see TooltipClient
    */
    virtual String getTooltip();

    //==============================================================================
    /** To allow items from your treeview to be dragged-and-dropped, implement this method.

        If this returns a non-null variant then when the user drags an item, the treeview will
        try to find a DragAndDropContainer in its parent hierarchy, and will use it to trigger
        a drag-and-drop operation, using this string as the source description, with the treeview
        itself as the source component.

        If you need more complex drag-and-drop behaviour, you can use custom components for
        the items, and use those to trigger the drag.

        To accept drag-and-drop in your tree, see isInterestedInDragSource(),
        isInterestedInFileDrag(), etc.

        @see DragAndDropContainer::startDragging
    */
    virtual var getDragSourceDescription();

    /** If you want your item to be able to have files drag-and-dropped onto it, implement this
        method and return true.

        If you return true and allow some files to be dropped, you'll also need to implement the
        filesDropped() method to do something with them.

        Note that this will be called often, so make your implementation very quick! There's
        certainly no time to try opening the files and having a think about what's inside them!

        For responding to internal drag-and-drop of other types of object, see isInterestedInDragSource().
        @see FileDragAndDropTarget::isInterestedInFileDrag, isInterestedInDragSource
    */
    virtual bool isInterestedInFileDrag (const StringArray& files);

    /** When files are dropped into this item, this callback is invoked.

        For this to work, you'll need to have also implemented isInterestedInFileDrag().
        The insertIndex value indicates where in the list of sub-items the files were dropped.
        If files are dropped onto an area of the tree where there are no visible items, this
        method is called on the root item of the tree, with an insert index of 0.
        @see FileDragAndDropTarget::filesDropped, isInterestedInFileDrag
    */
    virtual void filesDropped (const StringArray& files, int insertIndex);

    /** If you want your item to act as a DragAndDropTarget, implement this method and return true.

        If you implement this method, you'll also need to implement itemDropped() in order to handle
        the items when they are dropped.
        To respond to drag-and-drop of files from external applications, see isInterestedInFileDrag().
        @see DragAndDropTarget::isInterestedInDragSource, itemDropped
    */
    virtual bool isInterestedInDragSource (const DragAndDropTarget::SourceDetails& dragSourceDetails);

    /** When a things are dropped into this item, this callback is invoked.

        For this to work, you need to have also implemented isInterestedInDragSource().
        The insertIndex value indicates where in the list of sub-items the new items should be placed.
        If files are dropped onto an area of the tree where there are no visible items, this
        method is called on the root item of the tree, with an insert index of 0.
        @see isInterestedInDragSource, DragAndDropTarget::itemDropped
    */
    virtual void itemDropped (const DragAndDropTarget::SourceDetails& dragSourceDetails, int insertIndex);

    //==============================================================================
    /** Sets a flag to indicate that the item wants to be allowed
        to draw all the way across to the left edge of the treeview.

        By default this is false, which means that when the paintItem()
        method is called, its graphics context is clipped to only allow
        drawing within the item's rectangle. If this flag is set to true,
        then the graphics context isn't clipped on its left side, so it
        can draw all the way across to the left margin. Note that the
        context will still have its origin in the same place though, so
        the coordinates of anything to its left will be negative. It's
        mostly useful if you want to draw a wider bar behind the
        highlighted item.
    */
    void setDrawsInLeftMargin (bool canDrawInLeftMargin) noexcept;

    /** Sets a flag to indicate that the item wants to be allowed
        to draw all the way across to the right edge of the treeview.

        Similar to setDrawsInLeftMargin: when this flag is set to true,
        then the graphics context isn't clipped on the right side. Unlike
        setDrawsInLeftMargin, you will very rarely need to use this function,
        as this method won't clip the right margin unless your TreeViewItem
        overrides getItemWidth to return a positive value.

        @see setDrawsInLeftMargin, getItemWidth
     */
    void setDrawsInRightMargin (bool canDrawInRightMargin) noexcept;

    //==============================================================================
    /** Saves the current state of open/closed nodes so it can be restored later.

        This takes a snapshot of which sub-nodes have been explicitly opened or closed,
        and records it as XML. To identify node objects it uses the
        TreeViewItem::getUniqueName() method to create named paths. This
        means that the same state of open/closed nodes can be restored to a
        completely different instance of the tree, as long as it contains nodes
        whose unique names are the same.

        You'd normally want to use TreeView::getOpennessState() rather than call it
        for a specific item, but this can be handy if you need to briefly save the state
        for a section of the tree.

        Note that if all nodes of the tree are in their default state, then this may
        return a nullptr.

        @see TreeView::getOpennessState, restoreOpennessState
    */
    std::unique_ptr<XmlElement> getOpennessState() const;

    /** Restores the openness of this item and all its sub-items from a saved state.

        See TreeView::restoreOpennessState for more details.

        You'd normally want to use TreeView::restoreOpennessState() rather than call it
        for a specific item, but this can be handy if you need to briefly save the state
        for a section of the tree.

        @see TreeView::restoreOpennessState, getOpennessState
    */
    void restoreOpennessState (const XmlElement& xml);

    //==============================================================================
    /** Returns the index of this item in its parent's sub-items. */
    int getIndexInParent() const noexcept;

    /** Returns true if this item is the last of its parent's sub-items. */
    bool isLastOfSiblings() const noexcept;

    /** Creates a string that can be used to uniquely retrieve this item in the tree.

        The string that is returned can be passed to TreeView::findItemFromIdentifierString().
        The string takes the form of a path, constructed from the getUniqueName() of this
        item and all its parents, so these must all be correctly implemented for it to work.
        @see TreeView::findItemFromIdentifierString, getUniqueName
    */
    String getItemIdentifierString() const;

    //==============================================================================
    /**
        This handy class takes a copy of a TreeViewItem's openness when you create it,
        and restores that openness state when its destructor is called.

        This can very handy when you're refreshing sub-items - e.g.
        @code
        void MyTreeViewItem::updateChildItems()
        {
            OpennessRestorer openness (*this);  //  saves the openness state here..

            clearSubItems();

            // add a bunch of sub-items here which may or may not be the same as the ones that
            // were previously there
            addSubItem (...

            // ..and at this point, the old openness is restored, so any items that haven't
            // changed will have their old openness retained.
        }
        @endcode
    */
    class OpennessRestorer
    {
    public:
        OpennessRestorer (TreeViewItem&);
        ~OpennessRestorer();

    private:
        TreeViewItem& treeViewItem;
        std::unique_ptr<XmlElement> oldOpenness;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpennessRestorer)
    };

private:
    //==============================================================================
    TreeView* ownerView = nullptr;
    TreeViewItem* parentItem = nullptr;
    OwnedArray<TreeViewItem> subItems;
    int y = 0, itemHeight = 0, totalHeight = 0, itemWidth = 0, totalWidth = 0;
    int uid = 0;
    bool selected           : 1;
    bool redrawNeeded       : 1;
    bool drawLinesInside    : 1;
    bool drawLinesSet       : 1;
    bool drawsInLeftMargin  : 1;
    bool drawsInRightMargin : 1;
    unsigned int openness   : 2;

    friend class TreeView;

    void updatePositions (int newY);
    int getIndentX() const noexcept;
    void setOwnerView (TreeView*) noexcept;
    void paintRecursively (Graphics&, int width);
    TreeViewItem* getTopLevelItem() noexcept;
    TreeViewItem* findItemRecursively (int y) noexcept;
    TreeViewItem* getDeepestOpenParentItem() noexcept;
    int getNumRows() const noexcept;
    TreeViewItem* getItemOnRow (int index) noexcept;
    void deselectAllRecursively (TreeViewItem* itemToIgnore);
    int countSelectedItemsRecursively (int depth) const noexcept;
    TreeViewItem* getSelectedItemWithIndex (int index) noexcept;
    TreeViewItem* getNextVisibleItem (bool recurse) const noexcept;
    TreeViewItem* findItemFromIdentifierString (const String&);
    void restoreToDefaultOpenness();
    bool isFullyOpen() const noexcept;
    XmlElement* getOpennessState (bool canReturnNull) const;
    bool removeSubItemFromList (int index, bool deleteItem);
    void removeAllSubItemsFromList();
    bool areLinesDrawn() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TreeViewItem)
};


//==============================================================================
/**
    A tree-view component.

    Use one of these to hold and display a structure of TreeViewItem objects.


    @tags{GUI}
*/
class JUCE_API  TreeView  : public Component,
                            public SettableTooltipClient,
                            public FileDragAndDropTarget,
                            public DragAndDropTarget
{
public:
    //==============================================================================
    /** Creates an empty treeview.

        Once you've got a treeview component, you'll need to give it something to
        display, using the setRootItem() method.
    */
    TreeView (const String& componentName = String());

    /** Destructor. */
    ~TreeView() override;

    //==============================================================================
    /** Sets the item that is displayed in the treeview.

        A tree has a single root item which contains as many sub-items as it needs. If
        you want the tree to contain a number of root items, you should still use a single
        root item above these, but hide it using setRootItemVisible().

        You can pass nullptr to this method to clear the tree and remove its current root item.

        The object passed in will not be deleted by the treeview, it's up to the caller
        to delete it when no longer needed. BUT make absolutely sure that you don't delete
        this item until you've removed it from the tree, either by calling setRootItem (nullptr),
        or by deleting the tree first. You can also use deleteRootItem() as a quick way
        to delete it.
    */
    void setRootItem (TreeViewItem* newRootItem);

    /** Returns the tree's root item.

        This will be the last object passed to setRootItem(), or nullptr if none has been set.
    */
    TreeViewItem* getRootItem() const noexcept                      { return rootItem; }

    /** This will remove and delete the current root item.
        It's a convenient way of deleting the item and calling setRootItem (nullptr).
    */
    void deleteRootItem();

    /** Changes whether the tree's root item is shown or not.

        If the root item is hidden, only its sub-items will be shown in the treeview - this
        lets you make the tree look as if it's got many root items. If it's hidden, this call
        will also make sure the root item is open (otherwise the treeview would look empty).
    */
    void setRootItemVisible (bool shouldBeVisible);

    /** Returns true if the root item is visible.

        @see setRootItemVisible
    */
    bool isRootItemVisible() const noexcept                         { return rootItemVisible; }

    /** Sets whether items are open or closed by default.

        Normally, items are closed until the user opens them, but you can use this
        to make them default to being open until explicitly closed.

        @see areItemsOpenByDefault
    */
    void setDefaultOpenness (bool isOpenByDefault);

    /** Returns true if the tree's items default to being open.

        @see setDefaultOpenness
    */
    bool areItemsOpenByDefault() const noexcept                     { return defaultOpenness; }

    /** This sets a flag to indicate that the tree can be used for multi-selection.

        You can always select multiple items internally by calling the
        TreeViewItem::setSelected() method, but this flag indicates whether the user
        is allowed to multi-select by clicking on the tree.

        By default it is disabled.

        @see isMultiSelectEnabled
    */
    void setMultiSelectEnabled (bool canMultiSelect);

    /** Returns whether multi-select has been enabled for the tree.

        @see setMultiSelectEnabled
    */
    bool isMultiSelectEnabled() const noexcept                      { return multiSelectEnabled; }

    /** Sets a flag to indicate whether to hide the open/close buttons.

        @see areOpenCloseButtonsVisible
    */
    void setOpenCloseButtonsVisible (bool shouldBeVisible);

    /** Returns whether open/close buttons are shown.

        @see setOpenCloseButtonsVisible
    */
    bool areOpenCloseButtonsVisible() const noexcept                { return openCloseButtonsVisible; }

    //==============================================================================
    /** Deselects any items that are currently selected. */
    void clearSelectedItems();

    /** Returns the number of items that are currently selected.
        If maximumDepthToSearchTo is >= 0, it lets you specify a maximum depth to which the
        tree will be recursed.
        @see getSelectedItem, clearSelectedItems
    */
    int getNumSelectedItems (int maximumDepthToSearchTo = -1) const noexcept;

    /** Returns one of the selected items in the tree.
        @param index    the index, 0 to (getNumSelectedItems() - 1)
    */
    TreeViewItem* getSelectedItem (int index) const noexcept;

    /** Moves the selected row up or down by the specified number of rows. */
    void moveSelectedRow (int deltaRows);

    //==============================================================================
    /** Returns the number of rows the tree is using.
        This will depend on which items are open.
        @see TreeViewItem::getRowNumberInTree()
    */
    int getNumRowsInTree() const;

    /** Returns the item on a particular row of the tree.
        If the index is out of range, this will return nullptr.
        @see getNumRowsInTree, TreeViewItem::getRowNumberInTree()
    */
    TreeViewItem* getItemOnRow (int index) const;

    /** Returns the item that contains a given y position.
        The y is relative to the top of the TreeView component.
    */
    TreeViewItem* getItemAt (int yPosition) const noexcept;

    /** Tries to scroll the tree so that this item is on-screen somewhere. */
    void scrollToKeepItemVisible (TreeViewItem* item);

    /** Returns the treeview's Viewport object. */
    Viewport* getViewport() const noexcept;

    /** Returns the number of pixels by which each nested level of the tree is indented.
        @see setIndentSize
    */
    int getIndentSize() noexcept;

    /** Changes the distance by which each nested level of the tree is indented.
        @see getIndentSize
    */
    void setIndentSize (int newIndentSize);

    /** Searches the tree for an item with the specified identifier.
        The identifier string must have been created by calling TreeViewItem::getItemIdentifierString().
        If no such item exists, this will return false. If the item is found, all of its items
        will be automatically opened.
    */
    TreeViewItem* findItemFromIdentifierString (const String& identifierString) const;

    //==============================================================================
    /** Saves the current state of open/closed nodes so it can be restored later.

        This takes a snapshot of which nodes have been explicitly opened or closed,
        and records it as XML. To identify node objects it uses the
        TreeViewItem::getUniqueName() method to create named paths. This
        means that the same state of open/closed nodes can be restored to a
        completely different instance of the tree, as long as it contains nodes
        whose unique names are the same.

        @param alsoIncludeScrollPosition    if this is true, the state will also
                                            include information about where the
                                            tree has been scrolled to vertically,
                                            so this can also be restored
        @see restoreOpennessState
    */
    std::unique_ptr<XmlElement> getOpennessState (bool alsoIncludeScrollPosition) const;

    /** Restores a previously saved arrangement of open/closed nodes.

        This will try to restore a snapshot of the tree's state that was created by
        the getOpennessState() method. If any of the nodes named in the original
        XML aren't present in this tree, they will be ignored.

        If restoreStoredSelection is true, it will also try to re-select any items that
        were selected in the stored state.

        @see getOpennessState
    */
    void restoreOpennessState (const XmlElement& newState,
                               bool restoreStoredSelection);

    //==============================================================================
    /** A set of colour IDs to use to change the colour of various aspects of the treeview.

        These constants can be used either via the Component::setColour(), or LookAndFeel::setColour()
        methods.

        @see Component::setColour, Component::findColour, LookAndFeel::setColour, LookAndFeel::findColour
    */
    enum ColourIds
    {
        backgroundColourId             = 0x1000500, /**< A background colour to fill the component with. */
        linesColourId                  = 0x1000501, /**< The colour to draw the lines with.*/
        dragAndDropIndicatorColourId   = 0x1000502, /**< The colour to use for the drag-and-drop target position indicator. */
        selectedItemBackgroundColourId = 0x1000503, /**< The colour to use to fill the background of any selected items. */
        oddItemsColourId               = 0x1000504, /**< The colour to use to fill the background of the odd numbered items. */
        evenItemsColourId              = 0x1000505  /**< The colour to use to fill the background of the even numbered items. */
    };

    //==============================================================================
    /** This abstract base class is implemented by LookAndFeel classes to provide
        treeview drawing functionality.
    */
    struct JUCE_API  LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;

        virtual void drawTreeviewPlusMinusBox (Graphics&, const Rectangle<float>& area,
                                               Colour backgroundColour, bool isItemOpen, bool isMouseOver) = 0;

        virtual bool areLinesDrawnForTreeView (TreeView&) = 0;
        virtual int getTreeViewIndentSize (TreeView&) = 0;
    };

    //==============================================================================
    /** @internal */
    void paint (Graphics&) override;
    /** @internal */
    void resized() override;
    /** @internal */
    bool keyPressed (const KeyPress&) override;
    /** @internal */
    void colourChanged() override;
    /** @internal */
    void enablementChanged() override;
    /** @internal */
    bool isInterestedInFileDrag (const StringArray& files) override;
    /** @internal */
    void fileDragEnter (const StringArray& files, int x, int y) override;
    /** @internal */
    void fileDragMove (const StringArray& files, int x, int y) override;
    /** @internal */
    void fileDragExit (const StringArray& files) override;
    /** @internal */
    void filesDropped (const StringArray& files, int x, int y) override;
    /** @internal */
    bool isInterestedInDragSource (const SourceDetails&) override;
    /** @internal */
    void itemDragEnter (const SourceDetails&) override;
    /** @internal */
    void itemDragMove (const SourceDetails&) override;
    /** @internal */
    void itemDragExit (const SourceDetails&) override;
    /** @internal */
    void itemDropped (const SourceDetails&) override;

private:
    friend class TreeViewItem;

    class ContentComponent;
    class TreeViewport;
    class InsertPointHighlight;
    class TargetGroupHighlight;

    std::unique_ptr<TreeViewport> viewport;
    CriticalSection nodeAlterationLock;
    TreeViewItem* rootItem = nullptr;
    std::unique_ptr<InsertPointHighlight> dragInsertPointHighlight;
    std::unique_ptr<TargetGroupHighlight> dragTargetGroupHighlight;
    int indentSize = -1;
    bool defaultOpenness = false, needsRecalculating = true, rootItemVisible = true;
    bool multiSelectEnabled = false, openCloseButtonsVisible = true;

    void itemsChanged() noexcept;
    void recalculateIfNeeded();
    void updateButtonUnderMouse (const MouseEvent&);
    struct InsertPoint;
    void showDragHighlight (const InsertPoint&) noexcept;
    void hideDragHighlight() noexcept;
    void handleDrag (const StringArray&, const SourceDetails&);
    void handleDrop (const StringArray&, const SourceDetails&);
    bool toggleOpenSelectedItem();
    void moveOutOfSelectedItem();
    void moveIntoSelectedItem();
    void moveByPages (int numPages);

   #if JUCE_CATCH_DEPRECATED_CODE_MISUSE
    // this method has been deprecated - see the new version..
    virtual int paintOpenCloseButton (Graphics&, int, int, bool) { return 0; }
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TreeView)
};

} // namespace juce
