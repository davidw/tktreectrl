/* 
 * tkTreeItem.c --
 *
 *	This module implements items for treectrl widgets.
 *
 * Copyright (c) 2002-2006 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeItem.c,v 1.61 2006/09/21 06:35:11 treectrl Exp $
 */

#include "tkTreeCtrl.h"

typedef struct Column Column;
typedef struct Item Item;

/*
 * A data structure of the following type is kept for a single column in a
 * single item.
 */
struct Column {
    int cstate;		/* STATE_xxx flags manipulated with the
			 * [item state forcolumn] command */
#ifdef COLUMN_SPAN
    int span;		/* Number of columns this column's style covers */
#endif
    int neededWidth;	/* unused */
    int neededHeight;	/* unused */
    TreeStyle style;
    Column *next;	/* Column to the right of this one */
};

/*
 * A data structure of the following type is kept for each item.
 */
struct Item {
    int id;		/* unique id */
    int depth;		/* tree depth (-1 for the unique root item) */
    int neededHeight;	/* unused */
    int fixedHeight;	/* -height: desired height of this item (0 for
			 * no-such-value) */
    int numChildren;
    int index;		/* "row" in flattened tree */
    int indexVis;	/* visible "row" in flattened tree, -1 if hidden */
    int state;		/* STATE_xxx flags */
    int isVisible;	/* -visible option */
    int hasButton;	/* -button option */
    Item *parent;
    Item *firstChild;
    Item *lastChild;
    Item *prevSibling;
    Item *nextSibling;
    TreeItemDInfo dInfo; /* display info, or NULL */
    TreeItemRInfo rInfo; /* range info, or NULL */
    Column *columns;
#define ITEM_FLAG_DELETED	0x0001 /* Item is being deleted */
    int flags;
};

/*
 * Macro to test whether an item is the unique root item
 */
#define ISROOT(i) ((i)->depth == -1)

#define IS_DELETED(i) (((i)->flags & ITEM_FLAG_DELETED) != 0)

/*
 * Flags returned by Tk_SetOptions() (see itemOptionSpecs below).
 */
#define ITEM_CONF_BUTTON		0x0001
#define ITEM_CONF_SIZE			0x0002
#define ITEM_CONF_VISIBLE		0x0004

/*
 * Information used for Item objv parsing.
 */
static Tk_OptionSpec itemOptionSpecs[] = {
    {TK_OPTION_BOOLEAN, "-button", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(Item, hasButton),
     0, (ClientData) NULL, ITEM_CONF_BUTTON},
    {TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Item, fixedHeight),
     TK_OPTION_NULL_OK, (ClientData) NULL, ITEM_CONF_SIZE},
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(Item, isVisible),
     0, (ClientData) NULL, ITEM_CONF_VISIBLE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * Column_Alloc --
 *
 *	Allocate and initialize a new Column record.
 *
 * Results:
 *	Pointer to allocated Column.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static Column *
Column_Alloc(
    TreeCtrl *tree		/* Widget info. */
    )
{
#ifdef ALLOC_HAX
    Column *column = (Column *) AllocHax_Alloc(tree->allocData, sizeof(Column));
#else
    Column *column = (Column *) ckalloc(sizeof(Column));
#endif
    memset(column, '\0', sizeof(Column));
#ifdef COLUMN_SPAN
    column->span = 1;
#endif
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_InvalidateSize --
 *
 *	Marks the needed height and width of the column as out-of-date.
 *	NOTE: Column.neededWidth and Column.neededHeight are currently
 *	unused.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItemColumn_InvalidateSize(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemColumn column_	/* Column token. */
    )
{
    Column *column = (Column *) column_;

    column->neededWidth = column->neededHeight = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_NeededWidth --
 *
 *	Returns the requested width of a Column.
 *
 * Results:
 *	If the Column has a style, the requested width of the style
 *	is returned (a positive pixel value). Otherwise 0 is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItemColumn_NeededWidth(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    TreeItemColumn column_	/* Column token. */
    )
{
    Item *item = (Item *) item_;
    Column *self = (Column *) column_;

    if (self->style != NULL)
	return TreeStyle_NeededWidth(tree, self->style,
		item->state | self->cstate);
    return 0;
}

#ifdef EXPENSIVE_SPAN_WIDTH /* NOT USED */

/*
 * When a style spans 2 or more columns, all of the requested width goes
 * to the first column in the span. Ideally the width needed by the style
 * would be distributed over each column in the span. To be done properly,
 * the visibility and fixed-width of each column needs to be considered.
 *
 * This routine does an okay job of calculating the requested width of a
 * column in an item when spans are involved; however there is a lot of
 * processing involved, and this routine is called for each column in
 * a span separately (it would be better to calculate the width of all
 * the columns in a span at the same time).
 */

int
TreeItem_NeededWidthOfColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int _columnIndex		/* Column index. */
    )
{
    Item *item = (Item *) item_;
    TreeColumn treeColumn = tree->columns;
    Column *column = item->columns;
    int columnIndex = 0, itemColumnIndex = 0, span = 1;
    int selfIndex = _columnIndex, fixedWidth = 0, width = 0;
    int spanStart, spanEnd;
    struct span {
	Column *itemColumn;
	int itemColumnIndex;
	int fixedWidth;
	int spanWidth;
	int visible;
    } staticSpans[STATIC_SIZE], *spans = staticSpans;

    STATIC_ALLOC(spans, struct span, tree->columnCount);

    while (treeColumn != NULL) {
	spans[columnIndex].visible = TreeColumn_Visible(treeColumn);
	if (--span == 0) {
	    if (spans[columnIndex].visible)
		span = column ? column->span : 1;
	    else
		span = 1;
	    itemColumnIndex = columnIndex;
	}
	spans[columnIndex].itemColumn = column;
	spans[columnIndex].itemColumnIndex = itemColumnIndex;
	spans[columnIndex].spanWidth = 0;
	spans[columnIndex].fixedWidth = TreeColumn_FixedWidth(treeColumn);
	++columnIndex;
	treeColumn = TreeColumn_Next(treeColumn);
	if (column != NULL)
	    column = column->next;
    }

    span = 0;
    columnIndex = spanStart = spanEnd = spans[selfIndex].itemColumnIndex;
    while (spans[columnIndex].itemColumnIndex == spanStart) {
	if (spans[columnIndex].visible) {
	    if (spans[columnIndex].fixedWidth >= 0) {
		fixedWidth += spans[columnIndex].fixedWidth;
		spans[columnIndex].spanWidth = spans[columnIndex].fixedWidth;
	    } else
		++span; /* number of visible columns in the span with no
			 * fixed width */
	}
	spanEnd = columnIndex;
	++columnIndex;
	if (columnIndex == tree->columnCount)
	    break;
    }

    column = spans[spanStart].itemColumn;
    if (column != NULL && column->style != NULL) {
	width = TreeStyle_NeededWidth(tree, column->style,
		item->state | column->cstate);
	if (width > fixedWidth && span > 0) {
	    width -= fixedWidth;
	    while (width > 0) {
		int each = (width > span) ? width / span : 1;
		columnIndex = spanStart;
		while (columnIndex <= spanEnd) {
		    if (spans[columnIndex].visible) {
			if (spans[columnIndex].fixedWidth < 0) {
			    spans[columnIndex].spanWidth += each;
			    width -= each;
			    if (width <= 0)
				break;
			}
		    }
		    ++columnIndex;
		}
	    }
	};
    }

if (0)
{
    int i;
    for (i = 0; i < tree->columnCount; i++) {
	dbwin("%d,%d: itemColumn %p itemColumnIndex %d fixedWidth %d spanWidth %d\n",
	    item->id, i, spans[i].itemColumn, spans[i].itemColumnIndex, spans[i].fixedWidth,
	    spans[i].spanWidth);
    }
}
    width = spans[selfIndex].spanWidth;
    STATIC_FREE(spans, struct span, tree->columnCount);

    return width;
}

#endif /* COLUMN_SPAN */

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_GetStyle --
 *
 *	Returns the style assigned to a Column.
 *
 * Results:
 *	Returns the style, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeStyle
TreeItemColumn_GetStyle(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemColumn column	/* Column token. */
    )
{
    return ((Column *) column)->style;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_Index --
 *
 *	Return the 0-based index of a Column in an Item's linked list of
 *	Columns.
 *
 * Results:
 *	Integer index of the Column.
 *
 * Side effects:
 *	Tcl_Panic() if the Column isn't found.
 *
 *----------------------------------------------------------------------
 */

int
TreeItemColumn_Index(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    TreeItemColumn column_	/* Column token. */
    )
{
    Item *item = (Item *) item_;
    Column *column;
    int i = 0;

    column = item->columns;
    while ((column != NULL) && (column != (Column *) column_)) {
	i++;
	column = column->next;
    }
    if (column == NULL)
	panic("TreeItemColumn_Index: couldn't find the column\n");
    return i;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_ForgetStyle --
 *
 *	Free the style assigned to a Column.
 *
 * Results:
 *	Column has no style assigned anymore.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TreeItemColumn_ForgetStyle(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemColumn column_	/* Column token. */
    )
{
    Column *self = (Column *) column_;

    if (self->style != NULL) {
	TreeStyle_FreeResources(tree, self->style);
	self->style = NULL;
	self->neededWidth = self->neededHeight = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemColumn_GetNext --
 *
 *	Return the Column to the right of this one.
 *
 * Results:
 *	The next Column in the linked list, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemColumn
TreeItemColumn_GetNext(
    TreeCtrl *tree,		/* Widget info. */
    TreeItemColumn column	/* Column token. */
    )
{
    return (TreeItemColumn) ((Column *) column)->next;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_FreeResources --
 *
 *	Free the style and memory associated with the given Column.
 *
 * Results:
 *	The next Column in the linked list, or NULL.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

Column *
Column_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    Column *self		/* Column to free. */
    )
{
    Column *next = self->next;

    if (self->style != NULL)
	TreeStyle_FreeResources(tree, self->style);
#ifdef ALLOC_HAX
    AllocHax_Free(tree->allocData, (char *) self, sizeof(Column));
#else
    WFREE(self, Column);
#endif
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_UpdateIndex --
 *
 *	Set the Item.depth, Item.index and Item.indexVis fields of the
 *	given Item and all its descendants.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TreeCtrl.depth field may be updated to track the maximum
 *	depth of all items.
 *
 *----------------------------------------------------------------------
 */

static void
Item_UpdateIndex(TreeCtrl *tree,
    Item *item,			/* Item to update. */
    int *index,			/* New Item.index value for the item.
				 * Value is incremented. */
    int *indexVis		/* New Item.indexVis value for the item if
				 * the item is ReallyVisible().
				 * Value is incremented if the item is
				 * ReallyVisible(). */
    )
{
    Item *child;
    int parentVis, parentOpen;

    /* Also track max depth */
    if (item->parent != NULL)
	item->depth = item->parent->depth + 1;
    else
	item->depth = 0;
    if (item->depth > tree->depth)
	tree->depth = item->depth;

    item->index = (*index)++;
    item->indexVis = -1;
    if (item->parent != NULL) {
	parentOpen = (item->parent->state & STATE_OPEN) != 0;
	parentVis = item->parent->indexVis != -1;
	if (ISROOT(item->parent) && !tree->showRoot) {
	    parentOpen = TRUE;
	    parentVis = item->parent->isVisible;
	}
	if (parentVis && parentOpen && item->isVisible)
	    item->indexVis = (*indexVis)++;
    }
    child = item->firstChild;
    while (child != NULL) {
	Item_UpdateIndex(tree, child, index, indexVis);
	child = child->nextSibling;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_UpdateItemIndex --
 *
 *	Set the Item.depth, Item.index and Item.indexVis fields of the
 *	every Item. Set TreeCtrl.depth to the maximum depth of all
 *	Items. Set TreeCtrl.itemVisCount to the count of all visible
 *	items.
 *
 *	Because this is slow we try not to do it until necessary.
 *	The tree->updateIndex flags indicates when this is needed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tree_UpdateItemIndex(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Item *item = (Item *) tree->root;
    int index = 1, indexVis = 0;

    if (tree->debug.enable && tree->debug.data)
	dbwin("Tree_UpdateItemIndex %s\n", Tk_PathName(tree->tkwin));

    /* Also track max depth */
    tree->depth = -1;

    item->index = 0;
    item->indexVis = -1;
    if (tree->showRoot && item->isVisible)
	item->indexVis = indexVis++;
    item = item->firstChild;
    while (item != NULL) {
	Item_UpdateIndex(tree, item, &index, &indexVis);
	item = item->nextSibling;
    }
    tree->itemVisCount = indexVis;
    tree->updateIndex = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_Alloc --
 *
 *	Allocate an initialize a new Item record.
 *
 * Results:
 *	Pointer to the allocated Item record.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static Item *
Item_Alloc(
    TreeCtrl *tree		/* Widget info. */
    )
{
#ifdef ALLOC_HAX
    Item *item = (Item *) AllocHax_Alloc(tree->allocData, sizeof(Item));
#else
    Item *item = (Item *) ckalloc(sizeof(Item));
#endif
    memset(item, '\0', sizeof(Item));
    if (Tk_InitOptions(tree->interp, (char *) item,
		tree->itemOptionTable, tree->tkwin) != TCL_OK)
	panic("Tk_InitOptions() failed in Item_Alloc()");
    item->state =
	STATE_OPEN |
	STATE_ENABLED;
    if (tree->gotFocus)
	item->state |= STATE_FOCUS;
    item->indexVis = -1;
    Tree_AddItem(tree, (TreeItem) item);
    return item;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_AllocRoot --
 *
 *	Allocate an initialize a new Item record for the root item.
 *
 * Results:
 *	Pointer to the allocated Item record.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static Item *
Item_AllocRoot(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Item *item;

    item = Item_Alloc(tree);
    item->depth = -1;
    item->state |= STATE_ACTIVE;
    return item;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetFirstColumn --
 *
 *	Return the first Column record for an Item.
 *
 * Results:
 *	Token for the column, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemColumn
TreeItem_GetFirstColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    return (TreeItemColumn) ((Item *) item)->columns;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetState --
 *
 *	Return the state flags for an Item.
 *
 * Results:
 *	Bit mask of STATE_xxx flags.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetState(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    return ((Item *) item_)->state;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_ChangeState --
 *
 *	Toggles zero or more STATE_xxx flags for a Column. If the
 *	Column has a style assigned, its state may be changed.
 *
 * Results:
 *	Bit mask of CS_LAYOUT and CS_DISPLAY flags, or zero if no
 *	changes occurred.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static int
Column_ChangeState(
    TreeCtrl *tree,		/* Widget info. */
    Item *item,			/* Item containing the column. */
    Column *column,		/* Column to modify the state of. */
    int columnIndex,		/* 0-based column index. */
    int stateOff,		/* STATE_xxx flags to turn off. */
    int stateOn			/* STATE_xxx flags to turn on. */
    )
{
    int cstate, state;
    int sMask, iMask = 0;

    cstate = column->cstate;
    cstate &= ~stateOff;
    cstate |= stateOn;

    if (cstate == column->cstate)
	return 0;

    state = item->state | column->cstate;
    state &= ~stateOff;
    state |= stateOn;

    if (column->style != NULL) {
	sMask = TreeStyle_ChangeState(tree, column->style,
		item->state | column->cstate, state);
	if (sMask) {
	    if (sMask & CS_LAYOUT)
		Tree_InvalidateColumnWidth(tree, columnIndex);
	    iMask |= sMask;
	}

	if (iMask & CS_LAYOUT) {
	    TreeItem_InvalidateHeight(tree, (TreeItem) item);
#if 1
	    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
	    Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
#endif
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	}
	if (iMask & CS_DISPLAY)
	    Tree_InvalidateItemDInfo(tree, (TreeItem) item, NULL);
    }

    column->cstate = cstate;

    return iMask;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ChangeState --
 *
 *	Toggles zero or more STATE_xxx flags for an Item. If the
 *	Column has a style assigned, its state may be changed.
 *
 * Results:
 *	Bit mask of CS_LAYOUT and CS_DISPLAY flags, or zero if no
 *	changes occurred.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_ChangeState(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int stateOff,		/* STATE_xxx flags to turn off. */
    int stateOn			/* STATE_xxx flags to turn on. */
    )
{
    Item *item = (Item *) item_;
    Column *column;
    int columnIndex = 0, state, cstate;
    int sMask, iMask = 0;

    state = item->state;
    state &= ~stateOff;
    state |= stateOn;

    if (state == item->state)
	return 0;

    column = item->columns;
    while (column != NULL) {
	if (column->style != NULL) {
	    cstate = item->state | column->cstate;
	    cstate &= ~stateOff;
	    cstate |= stateOn;
	    sMask = TreeStyle_ChangeState(tree, column->style,
		    item->state | column->cstate, cstate);
	    if (sMask) {
		if (sMask & CS_LAYOUT) {
		    Tree_InvalidateColumnWidth(tree, columnIndex);
#if 1
		    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
#endif
		}
		iMask |= sMask;
	    }
	}
	columnIndex++;
	column = column->next;
    }

    /* This item has a button */
    if (item->hasButton && tree->showButtons
	    && (!ISROOT(item) || tree->showRootButton)) {

	Tk_Image image1, image2;
	Pixmap bitmap1, bitmap2;
	int butOpen, butClosed;
	int themeOpen, themeClosed;
	int w1, h1, w2, h2;
	void *ptr1 = NULL, *ptr2 = NULL;

	/*
	 * Compare the image/bitmap/theme/xlib button for the old state
	 * to the image/bitmap/theme/xlib button for the new state. Figure
	 * out if the size or appearance has changed.
	 */

	/* image > bitmap > theme > draw */
	image1 = PerStateImage_ForState(tree, &tree->buttonImage, item->state, NULL);
	if (image1 != NULL) {
	    Tk_SizeOfImage(image1, &w1, &h1);
	    ptr1 = image1;
	}
	if (ptr1 == NULL) {
	    bitmap1 = PerStateBitmap_ForState(tree, &tree->buttonBitmap, item->state, NULL);
	    if (bitmap1 != None) {
		Tk_SizeOfBitmap(tree->display, bitmap1, &w1, &h1);
		ptr1 = (void *) bitmap1;
	    }
	}
	if (ptr1 == NULL) {
	    if (tree->useTheme &&
		TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
		(item->state & STATE_OPEN) != 0, &w1, &h1) == TCL_OK) {
		ptr1 = (item->state & STATE_OPEN) ? &themeOpen : &themeClosed;
	    }
	}
	if (ptr1 == NULL) {
	    w1 = h1 = tree->buttonSize;
	    ptr1 = (item->state & STATE_OPEN) ? &butOpen : &butClosed;
	}

	/* image > bitmap > theme > draw */
	image2 = PerStateImage_ForState(tree, &tree->buttonImage, state, NULL);
	if (image2 != NULL) {
	    Tk_SizeOfImage(image2, &w2, &h2);
	    ptr2 = image2;
	}
	if (ptr2 == NULL) {
	    bitmap2 = PerStateBitmap_ForState(tree, &tree->buttonBitmap, state, NULL);
	    if (bitmap2 != None) {
		Tk_SizeOfBitmap(tree->display, bitmap2, &w2, &h2);
		ptr2 = (void *) bitmap2;
	    }
	}
	if (ptr2 == NULL) {
	    if (tree->useTheme &&
		TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
		(state & STATE_OPEN) != 0, &w2, &h2) == TCL_OK) {
		ptr2 = (state & STATE_OPEN) ? &themeOpen : &themeClosed;
	    }
	}
	if (ptr2 == NULL) {
	    w2 = h2 = tree->buttonSize;
	    ptr2 = (state & STATE_OPEN) ? &butOpen : &butClosed;
	}

	if ((w1 != w2) || (h1 != h2)) {
	    iMask |= CS_LAYOUT | CS_DISPLAY;
	} else if (ptr1 != ptr2) {
	    iMask |= CS_DISPLAY;
	}
    }

    if (iMask & CS_LAYOUT) {
	TreeItem_InvalidateHeight(tree, item_);
#if 1
	Tree_FreeItemDInfo(tree, item_, NULL);
#endif
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
    }
    if (iMask & CS_DISPLAY)
	Tree_InvalidateItemDInfo(tree, item_, NULL);

    item->state = state;

    return iMask;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_UndefineState --
 *
 *	Clear a STATE_xxx flag in an Item and its Columns. This is
 *	called when a user-defined state is undefined via the
 *	[state undefine] widget command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_UndefineState(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int state			/* STATE_xxx flag that is undefined. */
    )
{
    Item *item = (Item *) item_;
    Column *column = item->columns;

    while (column != NULL) {
	column->cstate &= ~state;
	column = column->next;
    }

    item->state &= ~state;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetButton --
 *
 *	Return the value of the -button configuration option for an
 *	Item.
 *
 * Results:
 *	Boolean indicating whether the Item could have a button
 *	displayed next to it (-showbuttons must also be set).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetButton(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    return item->hasButton;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetDepth --
 *
 *	Return the depth of an Item.
 *
 * Results:
 *	Integer >= -1. (-1 for the root)
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetDepth(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
#if 0
    if (tree->updateIndex)
	Tree_UpdateItemIndex(tree);
#endif
    return item->depth;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetID --
 *
 *	Return the unique ID of an Item.
 *
 * Results:
 *	Integer >= 0. (0 for the root)
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetID(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    return item->id;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SetID --
 *
 *	Set the unique ID of an Item. This is called when the item
 *	is created.
 *
 * Results:
 *	The given ID.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_SetID(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int id			/* Unique ID for the item. */
    )
{
    Item *item = (Item *) item_;
    return item->id = id;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetEnabled --
 *
 *	Return whether an Item is enabled or not.
 *
 * Results:
 *	TRUE if the item is enabled, FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetEnabled(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    return (item->state & STATE_ENABLED) != 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetSelected --
 *
 *	Return whether an Item is selected or not.
 *
 * Results:
 *	TRUE if the item is part of the selection, FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_GetSelected(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    return (item->state & STATE_SELECTED) != 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetParent --
 *
 *	Return the parent of an Item.
 *
 * Results:
 *	Token for parent Item, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_GetParent(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    return (TreeItem) item->parent;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetNextSibling --
 *
 *	Return the next sibling of an Item.
 *
 * Results:
 *	Token for next sibling Item, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_GetNextSibling(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    return (TreeItem) item->nextSibling;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_NextSiblingVisible --
 *
 *	Find a following sibling that is ReallyVisible().
 *
 * Results:
 *	Token for a sibling Item, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_NextSiblingVisible(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    item = TreeItem_GetNextSibling(tree, item);
    while (item != NULL) {
	if (TreeItem_ReallyVisible(tree, item))
	    return item;
	item = TreeItem_GetNextSibling(tree, item);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SetDInfo --
 *
 *	Store a display-info token in an Item. Called by the display
 *	code.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_SetDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    TreeItemDInfo dInfo		/* Display-info token. */
    )
{
    Item *item = (Item *) item_;
    item->dInfo = dInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetDInfo --
 *
 *	Return the display-info token of an Item. Called by the display
 *	code.
 *
 * Results:
 *	The display-info token or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemDInfo
TreeItem_GetDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    return item->dInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_SetRInfo --
 *
 *	Store a range-info token in an Item. Called by the display
 *	code.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_SetRInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    TreeItemRInfo rInfo		/* Range-info token */
    )
{
    Item *item = (Item *) item_;
    item->rInfo = rInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_GetRInfo --
 *
 *	Return the range-info token of an Item. Called by the display
 *	code.
 *
 * Results:
 *	The range-info token or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemRInfo
TreeItem_GetRInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    return item->rInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Next --
 *
 *	Return the Item after the given one.
 *
 * Results:
 *	Result will be:
 *	  a) the first child, or
 *	  b) the next sibling, or
 *	  c) the next sibling of an ancestor, or
 *	  d) NULL if no item follows this one
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_Next(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;

    if (item->firstChild != NULL)
	return (TreeItem) item->firstChild;
    if (item->nextSibling != NULL)
	return (TreeItem) item->nextSibling;
    while (1) {
	item = item->parent;
	if (item == NULL)
	    break;
	if (item->nextSibling != NULL)
	    return (TreeItem) item->nextSibling;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_NextVisible --
 *
 *	Return the ReallyVisible() Item after the given one.
 *
 * Results:
 *	Result will be:
 *	  a) the first ReallyVisible() child, or
 *	  b) the next ReallyVisible() sibling, or
 *	  c) the next ReallyVisible() sibling of an ancestor, or
 *	  d) NULL if no ReallyVisible() item follows this one
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_NextVisible(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    item = TreeItem_Next(tree, item);
    while (item != NULL) {
	if (TreeItem_ReallyVisible(tree, item))
	    return item;
	item = TreeItem_Next(tree, item);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Prev --
 *
 *	Return the Item before the given one.
 *
 * Results:
 *	Result will be:
 *	  a) the last descendant of the previous sibling, or
 *	  b) the parent, or
 *	  c) NULL if the given Item is the root
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_Prev(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    Item *walk;

    if (item->parent == NULL) /* root */
	return NULL;
    walk = item->parent;
    if (item->prevSibling) {
	walk = item->prevSibling;
	while (walk->lastChild != NULL)
	    walk = walk->lastChild;
    }
    return (TreeItem) walk;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_PrevVisible --
 *
 *	Return the ReallyVisible() Item before the given one.
 *
 * Results:
 *	Result will be:
 *	  a) the last descendant of the previous sibling, or
 *	  b) the parent, or
 *	  c) NULL if the given Item is the root
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem
TreeItem_PrevVisible(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item token. */
    )
{
    item = TreeItem_Prev(tree, item);
    while (item != NULL) {
	if (TreeItem_ReallyVisible(tree, item))
	    return item;
	item = TreeItem_Prev(tree, item);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ToIndex --
 *
 *	Return Item.index and Item.indexVis.
 *
 * Results:
 *	The zero-based indexes of the Item.
 *
 * Side effects:
 *	Will recalculate the indexes of every item if needed.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_ToIndex(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int *index,			/* Returned Item.index, may be NULL */
    int *indexVis		/* Returned Item.indexVis, may be NULL */
    )
{
    Item *item = (Item *) item_;

    if (tree->updateIndex)
	Tree_UpdateItemIndex(tree);
    if (index != NULL) (*index) = item->index;
    if (indexVis != NULL) (*indexVis) = item->indexVis;
}

/*
 *----------------------------------------------------------------------
 *
 * GatherQualifiers --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GatherQualifiers(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj **objv,		/* Argument values. */
    int startIndex,		/* First objv[] index to look at. */
    int *argsUsed,		/* Out: number of objv[] used. */
    int *visible,		/* Out: TRUE if the item must be ReallyVisible(). */
    int states[3]		/* Out: Item states that must be on or off. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int qual, j = startIndex;

    static CONST char *qualifiers[] = {
	"state", "visible", NULL
    };
    enum qualEnum {
	QUAL_STATE, QUAL_VISIBLE
    };
    /* Number of arguments used by qualifiers[]. */
    static int qualArgs[] = {
	2, 1
    };

    *argsUsed = 0;
    *visible = FALSE;
    states[0] = states[1] = states[2] = 0;

    for (; j < objc; ) {
	if (Tcl_GetIndexFromObj(NULL, objv[j], qualifiers, NULL, 0,
		&qual) != TCL_OK)
	    break;
	if (objc - j < qualArgs[qual]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(objv[j]), "\" qualifier", NULL);
	    return TCL_ERROR;
	}
	switch ((enum qualEnum) qual) {
	    case QUAL_STATE:
	    {
		int i, listObjc;
		Tcl_Obj **listObjv;
		if (Tcl_ListObjGetElements(interp, objv[j + 1],
			&listObjc, &listObjv) != TCL_OK)
		    return TCL_ERROR;
		for (i = 0; i < listObjc; i++) {
		    if (Tree_StateFromObj(tree, listObjv[i], states,
			    NULL, SFO_NOT_TOGGLE) != TCL_OK)
			return TCL_ERROR;
		}
		break;
	    }
	    case QUAL_VISIBLE:
	    {
		*visible = TRUE;
		break;
	    }
	}
	*argsUsed += qualArgs[qual];
	j += qualArgs[qual];
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifies --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	Returns TRUE if the item meets the given criteria.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Qualifies(
    TreeCtrl *tree,		/* Widget info. */
    Item *item,			/* The item to test. May be NULL. */
    int visible,		/* TRUE if the item must be ReallyVisible() */
    int states[3]		/* Item states that must be on or off
				 * for the item. */
    )
{
    /* Note: if the item is NULL it is a "match" because we have run
     * out of items to check. */
    if (item == NULL)
	return 1;
    if (visible && !TreeItem_ReallyVisible(tree, (TreeItem) item))
	return 0;
    if (states[STATE_OP_OFF] & item->state)
	return 0;
    if ((states[STATE_OP_ON] & item->state) != states[STATE_OP_ON])
	return 0;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItemList_FromObj --
 *
 *	Parse a Tcl_Obj item description to get a list of items.
 *
 *   all
 *   -- returning a single item --
 *   "active MODIFIERS"
 *   "anchor MODIFIERS"
 *   "nearest x y MODIFIERS"
 *   "root MODIFIERS"
 *   "first QUALIFIERS MODIFIERS"
 *   "last QUALIFIERS MODIFIERS"
 *   "end QUALIFIERS MODIFIERS"
 *   "rnc row col MODIFIERS"
 *   "ID MODIFIERS"
 *   -- returning multiple items --
 *   list listOfItemDescs
 *   range QUALIFIERS
 *
 *   MODIFIERS:
 *   -- returning a single item --
 *   above
 *   below
 *   left
 *   right
 *   top
 *   bottom
 *   leftmost
 *   rightmost
 *   next QUALIFIERS
 *   prev QUALIFIERS
 *   parent
 *   firstchild QUALIFIERS
 *   lastchild QUALIFIERS
 *   child N QUALIFIERS
 *   nextsibling QUALIFIERS
 *   prevsibling QUALIFIERS
 *   sibling N QUALIFIERS
 *   -- returning multiple items --
 *   ancestors QUALIFIERS
 *   children QUALIFIERS
 *
 *   QUALIFIERS:
 *   state stateList
 *   visible
 *
 *   Examples:
 *   $T item id "first visible firstchild"
 *   $T item id "first visible firstchild visible"
 *   $T item id "nearest x y nextsibling visible"
 *   $T item id "last visible state enabled" 
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItemList_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object to parse to a list of items. */
    TreeItemList *items,	/* Uninitialized item list. Caller must free
				 * it with TreeItemList_Free unless the
				 * result of this function is TCL_ERROR. */
    int flags			/* IFO_xxx flags */
    )
{
    Tcl_Interp *interp = tree->interp;
    int objc;
    int index, listIndex, id;
    Tcl_HashEntry *hPtr;
    Tcl_Obj **objv, *elemPtr;
    Item *item = NULL;

    static CONST char *indexName[] = {
	"active", "all", "anchor", "end", "first", "last", "list",
	"nearest", "range", "rnc", "root", (char *) NULL
    };
    enum indexEnum {
	INDEX_ACTIVE, INDEX_ALL, INDEX_ANCHOR, INDEX_END, INDEX_FIRST,
	INDEX_LAST, INDEX_LIST, INDEX_NEAREST, INDEX_RANGE, INDEX_RNC,
	INDEX_ROOT
    };
    /* Number of arguments used by indexName[]. */
    static int indexArgs[] = {
	1, 1, 1, 1, 1, 1, 2, 3, 3, 3, 1
    };
    /* Boolean: can indexName[] be followed by 1 or more qualifiers. */
    static int indexQual[] = {
	0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0
    };

    static CONST char *modifiers[] = {
	"above", "ancestors", "below", "bottom", "child", "children",
	"firstchild", "lastchild", "left", "leftmost", "next", "nextsibling",
	"parent", "prev", "prevsibling", "right", "rightmost", "sibling",
	"top", (char *) NULL
    };
    enum modEnum {
	TMOD_ABOVE, TMOD_ANCESTORS, TMOD_BELOW, TMOD_BOTTOM, TMOD_CHILD,
	TMOD_CHILDREN, TMOD_FIRSTCHILD,
	TMOD_LASTCHILD, TMOD_LEFT, TMOD_LEFTMOST, TMOD_NEXT, TMOD_NEXTSIBLING,
	TMOD_PARENT, TMOD_PREV, TMOD_PREVSIBLING, TMOD_RIGHT, TMOD_RIGHTMOST,
	TMOD_SIBLING, TMOD_TOP
    };
    /* Number of arguments used by modifiers[]. */
    static int modArgs[] = {
	1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1
    };
    /* Boolean: can modifiers[] be followed by 1 or more qualifiers. */
    static int modQual[] = {
	0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0
    };

    TreeItemList_Init(tree, items, 0);

    if (Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK)
	goto baditem;
    if (objc == 0)
	goto baditem;

    listIndex = 0;
    elemPtr = objv[listIndex];
    if (Tcl_GetIndexFromObj(NULL, elemPtr, indexName, NULL, 0, &index)
	    == TCL_OK) {
	int visible = FALSE;
	int states[3] = {0, 0, 0};
	int qualArgsTotal = 0;

	if (objc - listIndex < indexArgs[index]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(elemPtr), "\" keyword", NULL);
	    goto errorExit;
	}

	/* Gather up any qualifiers that follow this index. */
	if (indexQual[index]) {
	    if (GatherQualifiers(tree, objc, objv, listIndex + indexArgs[index],
		    &qualArgsTotal, &visible, states) != TCL_OK) {
		goto errorExit;
	    }
	}

	switch ((enum indexEnum) index) {
	    case INDEX_ACTIVE:
	    {
		item = (Item *) tree->activeItem;
		break;
	    }
	    case INDEX_ALL:
	    {
		if (!(flags & IFO_ALLOK)) {
		    Tcl_AppendResult(interp,
			    "can't specify \"all\" for this command", NULL);
		    goto errorExit;
		}
		if (objc > 1)
		    goto baditem;
		TreeItemList_Append(items, ITEM_ALL);
		return TCL_OK;
	    }
	    case INDEX_ANCHOR:
	    {
		item = (Item *) tree->anchorItem;
		break;
	    }
	    case INDEX_FIRST:
	    {
		item = (Item *) tree->root;
		while (!Qualifies(tree, item, visible, states))
		    item = (Item *) TreeItem_Next(tree, (TreeItem) item);
		break;
	    }
	    case INDEX_END:
	    case INDEX_LAST:
	    {
		item = (Item *) tree->root;
		while (item->lastChild) {
		    item = item->lastChild;
		}
		while (!Qualifies(tree, item, visible, states))
		    item = (Item *) TreeItem_Prev(tree, (TreeItem) item);
		break;
	    }
	    case INDEX_LIST:
	    {
		int listObjc;
		Tcl_Obj **listObjv;
		int i, count;

		if (Tcl_ListObjGetElements(interp, objv[listIndex + 1],
			&listObjc, &listObjv) != TCL_OK)
		    goto errorExit;
		for (i = 0; i < listObjc; i++) {
		    TreeItemList item2s;
		    if (TreeItemList_FromObj(tree, listObjv[i], &item2s, flags)
			    != TCL_OK)
			goto errorExit;
		    TreeItemList_Concat(items, &item2s);
		    TreeItemList_Free(&item2s);
		}
		/* If any of the item descriptions in the list is "all", then
		 * clear the list of items and replace it with "all". */
		count = TreeItemList_Count(items);
		for (i = 0; i < count; i++) {
		    TreeItem item = TreeItemList_ItemN(items, i);
		    if (item == ITEM_ALL)
			break;
		}
		if (i < count) {
		    TreeItemList_Free(items);
		    TreeItemList_Init(tree, items, 0);
		    TreeItemList_Append(items, ITEM_ALL);
		}
		goto endOfIndexArgs;
	    }
	    case INDEX_NEAREST:
	    {
		int x, y;

		if (Tcl_GetIntFromObj(NULL, objv[listIndex + 1], &x) != TCL_OK)
		    goto baditem;
		if (Tcl_GetIntFromObj(NULL, objv[listIndex + 2], &y) != TCL_OK)
		    goto baditem;
		item = (Item *) Tree_ItemUnderPoint(tree, &x, &y, TRUE);
		break;
	    }
	    case INDEX_RANGE:
	    {
		TreeItem itemFirst, itemLast;

		if (TreeItem_FromObj(tree, objv[listIndex + 1], &itemFirst,
			0) != TCL_OK)
		    goto errorExit;
		if (TreeItem_FromObj(tree, objv[listIndex + 2], &itemLast,
			0) != TCL_OK)
		    goto errorExit;
		if (TreeItem_FirstAndLast(tree, &itemFirst, &itemLast) == 0)
		    goto errorExit;
		while (1) {
		    if (Qualifies(tree, (Item *) itemFirst, visible, states)) {
			if (ISROOT((Item *) itemFirst) && (flags & IFO_NOTROOT))
			    goto notRoot;
			if (!((Item *) itemFirst)->parent && (flags & IFO_NOTORPHAN))
			    goto notOrphan;
			TreeItemList_Append(items, itemFirst);
		    }
		    if (itemFirst == itemLast)
			break;
		    itemFirst = TreeItem_Next(tree, itemFirst);
		}
endOfIndexArgs:
		if (listIndex + indexArgs[index] + qualArgsTotal < objc) {
		    Tcl_AppendResult(interp, "unexpected arguments after \"",
			    indexName[index], "\" index", NULL);
		    goto errorExit;
		}
		if (!TreeItemList_Count(items) && !(flags & IFO_NULLOK))
		    goto noitem;
		return TCL_OK;
	    }
	    case INDEX_RNC:
	    {
		int row, col;

		if (Tcl_GetIntFromObj(NULL, objv[listIndex + 1], &row) != TCL_OK)
		    goto baditem;
		if (Tcl_GetIntFromObj(NULL, objv[listIndex + 2], &col) != TCL_OK)
		    goto baditem;
		item = (Item *) Tree_RNCToItem(tree, row, col);
		break;
	    }
	    case INDEX_ROOT:
	    {
		item = (Item *) tree->root;
		break;
	    }
	}
	listIndex += indexArgs[index] + qualArgsTotal;

    /* No indexName[] was found. Try an itemPrefix + item ID. */
    } else if (tree->itemPrefixLen) {
	char *end, *t = Tcl_GetString(elemPtr);
	if (strncmp(t, tree->itemPrefix, tree->itemPrefixLen) != 0)
	    goto baditem;
	t += tree->itemPrefixLen;
	id = strtoul(t, &end, 10);
	if ((end == t) || (*end != '\0'))
	    goto baditem;

	hPtr = Tcl_FindHashEntry(&tree->itemHash, (char *) id);
	if (!hPtr) {
	    if (!(flags & IFO_NULLOK))
		goto noitem;
	    /* Empty list returned */
	    return TCL_OK;
	}
	item = (Item *) Tcl_GetHashValue(hPtr);
	listIndex++;

    /* Try an item ID. */
    } else if (Tcl_GetIntFromObj(NULL, elemPtr, &id) == TCL_OK) {
	hPtr = Tcl_FindHashEntry(&tree->itemHash, (char *) id);
	if (!hPtr) {
	    if (!(flags & IFO_NULLOK))
		goto noitem;
	    /* Empty list returned */
	    return TCL_OK;
	}
	item = (Item *) Tcl_GetHashValue(hPtr);
	listIndex++;
    } else {
	goto baditem;
    }

    /* This means a valid specification was given, but there is no such item */
    if (item == NULL) {
	if (!(flags & IFO_NULLOK))
	    goto noitem;
	/* Empty list returned */
	return TCL_OK;
    }

    /* Process any modifiers following the item we matched above. */
    for (; listIndex < objc; /* nothing */) {
	int visible = FALSE;
	int states[3] = {0, 0, 0};
	int qualArgsTotal = 0;

	elemPtr = objv[listIndex];
	if (Tcl_GetIndexFromObj(interp, elemPtr, modifiers, "modifier", 0,
		    &index) != TCL_OK) {
	    goto errorExit;
	}
	if (objc - listIndex < modArgs[index]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(elemPtr), "\" modifier", NULL);
	    goto errorExit;
	}

	/* Gather up any qualifiers that follow this modifier. */
	if (modQual[index]) {
	    if (GatherQualifiers(tree, objc, objv, listIndex + modArgs[index],
		    &qualArgsTotal, &visible, states) != TCL_OK) {
		goto errorExit;
	    }
	}

	switch ((enum modEnum) index) {
	    case TMOD_ABOVE:
	    {
		item = (Item *) Tree_ItemAbove(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_ANCESTORS:
	    {
		item = item->parent;
		while (item != NULL) {
		    if (Qualifies(tree, item, visible, states)) {
			if (ISROOT(item) && (flags & IFO_NOTROOT))
			    goto notRoot;
			if (!item->parent && (flags & IFO_NOTORPHAN))
			    goto notOrphan;
			TreeItemList_Append(items, (TreeItem) item);
		    }
		    item = item->parent;
		}
endOfModArgs:
		if (listIndex + modArgs[index] + qualArgsTotal < objc) {
		    Tcl_AppendResult(interp, "unexpected arguments after \"",
			    modifiers[index], "\" modifier", NULL);
		    goto errorExit;
		}
		if (!TreeItemList_Count(items) && !(flags & IFO_NULLOK))
		    goto noitem;
		return TCL_OK;
	    }
	    case TMOD_BELOW:
	    {
		item = (Item *) Tree_ItemBelow(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_BOTTOM:
	    {
		item = (Item *) Tree_ItemBottom(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_CHILD:
	    {
		int n;

		if (Tcl_GetIntFromObj(interp, objv[listIndex + 1],
			    &n) != TCL_OK) {
		    goto errorExit;
		}
		item = item->firstChild;
		while (item != NULL) {
		    if (Qualifies(tree, item, visible, states))
			if (n-- <= 0)
			    break;
		    item = item->nextSibling;
		}
		break;
	    }
	    case TMOD_CHILDREN:
	    {
		item = item->firstChild;
		while (item != NULL) {
		    if (Qualifies(tree, item, visible, states)) {
			if (ISROOT(item) && (flags & IFO_NOTROOT))
			    goto notRoot;
			if (!item->parent && (flags & IFO_NOTORPHAN))
			    goto notOrphan;
			TreeItemList_Append(items, (TreeItem) item);
		    }
		    item = item->nextSibling;
		}
		goto endOfModArgs;
	    }
	    case TMOD_FIRSTCHILD:
	    {
		item = item->firstChild;
		while (!Qualifies(tree, item, visible, states))
		    item = item->nextSibling;
		break;
	    }
	    case TMOD_LASTCHILD:
	    {
		item = item->lastChild;
		while (!Qualifies(tree, item, visible, states))
		    item = item->prevSibling;
		break;
	    }
	    case TMOD_LEFT:
	    {
		item = (Item *) Tree_ItemLeft(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_LEFTMOST:
	    {
		item = (Item *) Tree_ItemLeftMost(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_NEXT:
	    {
		item = (Item *) TreeItem_Next(tree, (TreeItem) item);
		while (!Qualifies(tree, item, visible, states))
		    item = (Item *) TreeItem_Next(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_NEXTSIBLING:
	    {
		item = item->nextSibling;
		while (!Qualifies(tree, item, visible, states))
		    item = item->nextSibling;
		break;
	    }
	    case TMOD_PARENT:
	    {
		item = item->parent;
		break;
	    }
	    case TMOD_PREV:
	    {
		item = (Item *) TreeItem_Prev(tree, (TreeItem) item);
		while (!Qualifies(tree, item, visible, states))
		    item = (Item *) TreeItem_Prev(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_PREVSIBLING:
	    {
		item = item->prevSibling;
		while (!Qualifies(tree, item, visible, states))
		    item = item->prevSibling;
		break;
	    }
	    case TMOD_RIGHT:
	    {
		item = (Item *) Tree_ItemRight(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_RIGHTMOST:
	    {
		item = (Item *) Tree_ItemRightMost(tree, (TreeItem) item);
		break;
	    }
	    case TMOD_SIBLING:
	    {
		int n;

		if (Tcl_GetIntFromObj(interp, objv[listIndex + 1], &n) != TCL_OK) {
		    goto errorExit;
		}
		item = item->parent;
		if (item == NULL)
		    break;
		item = item->firstChild;
		while (item != NULL) {
		    if (Qualifies(tree, item, visible, states))
			if (n-- <= 0)
			    break;
		    item = item->nextSibling;
		}
		break;
	    }
	    case TMOD_TOP:
	    {
		item = (Item *) Tree_ItemTop(tree, (TreeItem) item);
		break;
	    }
	}
	if (item == NULL) {
	    if (!(flags & IFO_NULLOK))
		goto noitem;
	    /* Empty list returned. */
	    return TCL_OK;
	}
	listIndex += modArgs[index] + qualArgsTotal;
    }
    if (ISROOT(item) && (flags & IFO_NOTROOT)) {
notRoot:
	Tcl_AppendResult(interp,
		"can't specify \"root\" for this command", NULL);
	goto errorExit;
    }
    if ((item->parent == NULL) && (flags & IFO_NOTORPHAN)) {
notOrphan:
	Tcl_AppendResult(interp,
		"item \"", Tcl_GetString(objPtr), "\" has no parent", NULL);
	goto errorExit;
    }
    TreeItemList_Append(items, (TreeItem) item);
    return TCL_OK;

baditem:
    Tcl_AppendResult(interp, "bad item description \"", Tcl_GetString(objPtr),
	    "\"", NULL);
    goto errorExit;

noitem:
    Tcl_AppendResult(interp, "item \"", Tcl_GetString(objPtr),
	    "\" doesn't exist", NULL);

errorExit:
    TreeItemList_Free(items);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_FromObj --
 *
 *	Parse a Tcl_Obj item description to get a single item.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object to parse to an item. */
    TreeItem *itemPtr,		/* Returned item. */
    int flags			/* IFO_xxx flags */
    )
{
    TreeItemList items;

    if (TreeItemList_FromObj(tree, objPtr, &items, flags) != TCL_OK)
	return TCL_ERROR;
    /* May be NULL. */
    (*itemPtr) = TreeItemList_ItemN(&items, 0);
    TreeItemList_Free(&items);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_ToggleOpen --
 *
 *	Inverts the STATE_OPEN flag of an Item.
 *
 * Results:
 *	Items may be displayed/undisplayed.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
Item_ToggleOpen(
    TreeCtrl *tree,		/* Widget info. */
    Item *item,			/* Item record. */
    int stateOff,		/* STATE_OPEN or 0 */
    int stateOn			/* STATE_OPEN or 0 */
    )
{
    int mask;

    mask = TreeItem_ChangeState(tree, (TreeItem) item, stateOff, stateOn);

    if (ISROOT(item) && !tree->showRoot)
	return;

#if 0
    /* Don't affect display if we weren't visible */
    if (!TreeItem_ReallyVisible(tree, (TreeItem) item))
	return;

    /* Invalidate display info for this item, so it is redrawn later. */
    Tree_InvalidateItemDInfo(tree, (TreeItem) item, NULL);
#endif

    if (item->numChildren > 0) {
	/* indexVis needs updating for all items after this one, if we
	 * have any visible children */
	tree->updateIndex = 1;
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

	/* Hiding/showing children may change the width of any column */
	Tree_InvalidateColumnWidth(tree, -1);
    }

    /* If this item was previously onscreen, this call is repetitive. */
    Tree_EventuallyRedraw(tree);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_OpenClose --
 *
 *	Inverts the STATE_OPEN flag of an Item, possibly recursively.
 *
 * Results:
 *	Items may be displayed/undisplayed.
 *
 * Side effects:
 *	Display changes. <Expand> and <Collapse> events may be
 *	generated.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_OpenClose(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int mode,			/* -1: toggle
				 * 0: close
				 * 1: open */
    int recurse			/* TRUE to perform same op on descendants,
				 * FALSE to only affect given item. */
    )
{
    Item *item = (Item *) item_;
    Item *child;
    int stateOff = 0, stateOn = 0;

    if (mode == -1) {
	if (item->state & STATE_OPEN)
	    stateOff = STATE_OPEN;
	else
	    stateOn = STATE_OPEN;
    } else if (!mode && (item->state & STATE_OPEN))
	stateOff = STATE_OPEN;
    else if (mode && !(item->state & STATE_OPEN))
	stateOn = STATE_OPEN;

    if (stateOff != stateOn) {
	TreeNotify_OpenClose(tree, item_, stateOn, TRUE);
	Item_ToggleOpen(tree, item, stateOff, stateOn);
	TreeNotify_OpenClose(tree, item_, stateOn, FALSE);
    }
    if (recurse) {
	for (child = item->firstChild; child != NULL; child = child->nextSibling)
	    TreeItem_OpenClose(tree, (TreeItem) child, mode, recurse);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Delete --
 *
 *	Recursively frees resources associated with an Item and its
 *	descendants.
 *
 * Results:
 *	Items are removed from their parent and freed.
 *
 * Side effects:
 *	Memory is freed. If the active item or selection-anchor item
 *	is deleted, the root becomes the active/anchor item.
 *	Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_Delete(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;

    if (TreeItem_ReallyVisible(tree, item_))
	Tree_InvalidateColumnWidth(tree, -1);

    while (self->numChildren > 0)
	TreeItem_Delete(tree, (TreeItem) self->firstChild);

    TreeItem_RemoveFromParent(tree, item_);
    TreeDisplay_ItemDeleted(tree, item_);
    Tree_RemoveItem(tree, item_);
    TreeItem_FreeResources(tree, item_);
    if (tree->activeItem == item_) {
	tree->activeItem = tree->root;
	TreeItem_ChangeState(tree, tree->activeItem, 0, STATE_ACTIVE);
    }
    if (tree->anchorItem == item_)
	tree->anchorItem = tree->root;
    if (tree->debug.enable && tree->debug.data)
	Tree_Debug(tree);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_FirstAndLast --
 *
 *	Determine the order of two items and swap them if needed.
 *
 * Results:
 *	If the items do not share a common ancestor, 0 is returned and
 *	an error message is left in the interpreter result.
 *	Otherwise the return value is the number of items in the
 *	range between first and last.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_FirstAndLast(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem *first,		/* Item token. */
    TreeItem *last		/* Item token. */
    )
{
    int indexFirst, indexLast, index;

    if (TreeItem_RootAncestor(tree, *first) !=
	    TreeItem_RootAncestor(tree, *last)) {
	FormatResult(tree->interp,
		"item %s%d and item %s%d don't share a common ancestor",
		tree->itemPrefix, TreeItem_GetID(tree, *first),
		tree->itemPrefix, TreeItem_GetID(tree, *last));
	return 0;
    }
    TreeItem_ToIndex(tree, *first, &indexFirst, NULL);
    TreeItem_ToIndex(tree, *last, &indexLast, NULL);
    if (indexFirst > indexLast) {
	TreeItem item = *first;
	*first = *last;
	*last = item;

	index = indexFirst;
	indexFirst = indexLast;
	indexLast = index;
    }
    return indexLast - indexFirst + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ListDescendants --
 *
 *	Appends descendants of an item to a list of items.
 *
 * Results:
 *	List of items may grow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_ListDescendants(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    TreeItemList *items		/* List of items to append descendants to. */
    )
{
    Item *item = (Item *) item_;
    Item *last;

    last = item;
    while (last->lastChild != NULL)
	last = last->lastChild;
    while (item_ != (TreeItem) last) {
	TreeItemList_Append(items, item_);
	item_ = TreeItem_Next(tree, item_);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * TreeItem_UpdateDepth --
 *
 *	Recursively updates Item.depth of an Item and its
 *	descendants.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_UpdateDepth(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;
    Item *child;

    if (ISROOT(self))
	return;
    if (self->parent != NULL)
	self->depth = self->parent->depth + 1;
    else
	self->depth = 0;
    child = self->firstChild;
    while (child != NULL) {
	TreeItem_UpdateDepth(tree, (TreeItem) child);
	child = child->nextSibling;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_AddToParent --
 *
 *	Called *after* an Item is added as a child to another Item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_AddToParent(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;
    Item *last;

    /* If this is the new last child, redraw the lines of the previous
     * sibling and all of its descendants so the line from the previous
     * sibling reaches this item */
    if ((self->prevSibling != NULL) &&
	    (self->nextSibling == NULL) &&
	    tree->showLines) {
	last = self->prevSibling;
	while (last->lastChild != NULL)
	    last = last->lastChild;
	Tree_InvalidateItemDInfo(tree, (TreeItem) self->prevSibling,
		(TreeItem) last);
    }

    tree->updateIndex = 1;
    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    /* Tree_UpdateItemIndex() also recalcs depth, but in one of my demos
     * I retrieve item depth during list creation. Since Tree_UpdateItemIndex()
     * is slow I will keep depth up-to-date here. */
    TreeItem_UpdateDepth(tree, item_);

    Tree_InvalidateColumnWidth(tree, -1);

    if (tree->debug.enable && tree->debug.data)
	Tree_Debug(tree);
}

/*
 *----------------------------------------------------------------------
 *
 * RemoveFromParentAux --
 *
 *	Recursively update Item.depth, Item.index and Item.indexVis.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Display changes.
 *
 *----------------------------------------------------------------------
 */

static void
RemoveFromParentAux(
    TreeCtrl *tree,		/* Widget info. */
    Item *self,			/* Item being removed. */
    int *index			/* New value of Item.index. Is incremented. */
    )
{
    Item *child;

    /* Invalidate display info. Don't free it because we may just be
     * moving the item to a new parent. FIXME: if it is being moved,
     * it might not actually need to be redrawn (just copied) */
    if (self->dInfo != NULL)
	Tree_InvalidateItemDInfo(tree, (TreeItem) self, NULL);

    if (self->parent != NULL)
	self->depth = self->parent->depth + 1;
    else
	self->depth = 0;
    self->index = (*index)++;
    self->indexVis = -1;
    child = self->firstChild;
    while (child != NULL) {
	RemoveFromParentAux(tree, child, index);
	child = child->nextSibling;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RemoveFromParent --
 *
 *	Remove an Item from its parent (if any).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Display changes. 
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_RemoveFromParent(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;
    Item *parent = self->parent;
    Item *last;
    int index = 0;

    if (parent == NULL)
	return;

    /* If this is the last child, redraw the lines of the previous
     * sibling and all of its descendants because the line from
     * the previous sibling to us is now gone */
    if ((self->prevSibling != NULL) &&
	    (self->nextSibling == NULL) &&
	    tree->showLines) {
	last = self->prevSibling;
	while (last->lastChild != NULL)
	    last = last->lastChild;
	Tree_InvalidateItemDInfo(tree, (TreeItem) self->prevSibling,
		(TreeItem) last);
    }

    /*
     * Set a flag indicating that item indexes are out-of-date. This doesn't
     * cover the current item being removed.
     */
    tree->updateIndex = 1;
    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    if (self->prevSibling)
	self->prevSibling->nextSibling = self->nextSibling;
    if (self->nextSibling)
	self->nextSibling->prevSibling = self->prevSibling;
    if (parent->firstChild == self) {
	parent->firstChild = self->nextSibling;
	if (!parent->firstChild)
	    parent->lastChild = NULL;
    }
    if (parent->lastChild == self)
	parent->lastChild = self->prevSibling;
    self->prevSibling = self->nextSibling = NULL;
    self->parent = NULL;
    parent->numChildren--;

    /*
     * Update Item.depth, Item.index and Item.indexVis for the item and its
     * descendants. An up-to-date Item.index is needed for some operations that
     * use a range of items, such as [item delete].
     */
    RemoveFromParentAux(tree, self, &index);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RemoveColumns --
 *
 *	Free a range of Columns in an Item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated. 
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_RemoveColumns(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int first,			/* 0-based column index at start of
				 * the range. Must be <= last */
    int last			/* 0-based column index at end of
				 * the range. Must be >= first */
    )
{
    Item *self = (Item *) item_;
    Column *column = self->columns;
    Column *prev = NULL, *next = NULL;
    int i = 0;

    while (column != NULL) {
	next = column->next;
	if (i == first - 1)
	    prev = column;
	else if (i >= first)
	    Column_FreeResources(tree, column);
	if (i == last)
	    break;
	++i;
	column = next;
    }
    if (prev != NULL)
	prev->next = next;
    else
	self->columns = next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RemoveAllColumns --
 *
 *	Free all the Columns in an Item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated. 
 *
 *----------------------------------------------------------------------
 */

void TreeItem_RemoveAllColumns(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;
    Column *column = self->columns;

    while (column != NULL) {
	Column *next = column->next;
	Column_FreeResources(tree, column);
	column = next;
    }
    self->columns = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_CreateColumn --
 *
 *	Allocate a Column record for an Item if it doesn't already
 *	exist.
 *
 * Results:
 *	Pointer to new or existing Column record.
 *
 * Side effects:
 *	Any column records preceding the desired one are allocated
 *	if they weren't already. Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static Column *
Item_CreateColumn(
    TreeCtrl *tree,		/* Widget info. */
    Item *self,			/* Item to contain the column. */
    int columnIndex,		/* 0-based index of new column. */
    int *isNew			/* May be NULL. Set to TRUE if the
				 * column record was created. */
    )
{
    Column *column;
    int i;

    if (isNew != NULL) (*isNew) = FALSE;
    column = self->columns;
    if (column == NULL) {
	column = Column_Alloc(tree);
	column->neededWidth = column->neededHeight = -1;
	self->columns = column;
	if (isNew != NULL) (*isNew) = TRUE;
    }
    for (i = 0; i < columnIndex; i++) {
	if (column->next == NULL) {
	    column->next = Column_Alloc(tree);
	    column->next->neededWidth = column->next->neededHeight = -1;
	    if (isNew != NULL) (*isNew) = TRUE;
	}
	column = column->next;
    }

    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_MoveColumn --
 *
 *	Rearranges an Item''s list of Column records by moving one
 *	in front of another.
 *
 * Results:
 *	If the Column to be moved does not exist and the Column to place it
 *	in front of does not exist, then nothing happens. If the Column is
 *	to be moved past all currently allocated Columns, then new
 *	Column records are allocated.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_MoveColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int columnIndex,		/* 0-based index of the column to move. */
    int beforeIndex		/* 0-based index of the second column to move
				 * the first column to the left of. */
    )
{
    Item *self = (Item *) item;
    Column *before = NULL, *move = NULL;
    Column *prevM = NULL, *prevB = NULL;
    Column *last = NULL, *prev, *walk;
    int index = 0;

    prev = NULL;
    walk = self->columns;
    while (walk != NULL) {
	if (index == columnIndex) {
	    prevM = prev;
	    move = walk;
	}
	if (index == beforeIndex) {
	    prevB = prev;
	    before = walk;
	}
	prev = walk;
	if (walk->next == NULL)
	    last = walk;
	index++;
	walk = walk->next;
    }

    if (move == NULL && before == NULL)
	return;
    if (move == NULL)
	move = Column_Alloc(tree);
    else {
	if (before == NULL) {
	    prevB = Item_CreateColumn(tree, self, beforeIndex - 1, NULL);
	    last = prevB;
	}
	if (prevM == NULL)
	    self->columns = move->next;
	else
	    prevM->next = move->next;
    }
    if (before == NULL) {
	last->next = move;
	move->next = NULL;
    } else {
	if (prevB == NULL)
	    self->columns = move;
	else
	    prevB->next = move;
	move->next = before;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_FreeResources --
 *
 *	Free memory etc assocated with an Item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated. Display changes.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_FreeResources(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;
    Column *column;

    column = self->columns;
    while (column != NULL)
	column = Column_FreeResources(tree, column);
    if (self->dInfo != NULL)
	Tree_FreeItemDInfo(tree, item_, NULL);
    if (self->rInfo != NULL)
	Tree_FreeItemRInfo(tree, item_);
#ifdef ALLOC_HAX
    AllocHax_Free(tree->allocData, (char *) self, sizeof(Item));
#else
    WFREE(self, Item);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Item_UseHeight --
 *
 *	Return the height used by styles in an Item.
 *
 * Results:
 *	Maximum height in pixels of the style in each visible Column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Item_HeightOfStyles(
    TreeCtrl *tree,		/* Widget info. */
    Item *item			/* Item record. */
    )
{
    Column *column = item->columns;
    TreeColumn treeColumn = tree->columns;
    StyleDrawArgs drawArgs;
    int height = 0;

    drawArgs.tree = tree;

    while (column != NULL) {
	if (TreeColumn_Visible(treeColumn) && (column->style != NULL)) {
	    drawArgs.state = item->state | column->cstate;
	    drawArgs.style = column->style;
	    drawArgs.indent = (treeColumn == tree->columnTree) ?
		TreeItem_Indent(tree, (TreeItem) item) : 0;
	    if ((TreeColumn_FixedWidth(treeColumn) != -1) ||
		    TreeColumn_Squeeze(treeColumn)) {
		drawArgs.width = TreeColumn_UseWidth(treeColumn);
	    } else
		drawArgs.width = -1;
	    height = MAX(height, TreeStyle_UseHeight(&drawArgs));
	}
	treeColumn = TreeColumn_Next(treeColumn);
	column = column->next;
    }

    return height;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Height --
 *
 *	Return the height of an Item.
 *
 * Results:
 *	If the Item -height option is > 0, the result is the maximum
 *	of the button height (if a button is displayed) and the -height
 *	option.
 *	If the TreeCtrl -itemheight option is > 0, the result is the maximum
 *	of the button height (if a button is displayed) and the -itemheight
 *	option.
 *	Otherwise the result is the maximum of the button height (if a button
 *	is displayed) AND the TreeCtrl -minitemheight AND the height of
 *	the style in each visible Column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeItem_Height(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;
    int buttonHeight = 0;
    int useHeight;

    if (!self->isVisible || (ISROOT(self) && !tree->showRoot))
	return 0;

    /* Get requested height of the style in each column */
    useHeight = Item_HeightOfStyles(tree, self);

    /* Can't have less height than our button */
    if (tree->showButtons && self->hasButton && (!ISROOT(self) || tree->showRootButton)) {
	buttonHeight = ButtonHeight(tree, self->state);
    }

    /* User specified a fixed height for this item */
    if (self->fixedHeight > 0)
	return MAX(self->fixedHeight, buttonHeight);

    /* Fixed height of all items */
    if (tree->itemHeight > 0)
	return MAX(tree->itemHeight, buttonHeight);

    /* Minimum height of all items */
    if (tree->minItemHeight > 0)
	useHeight = MAX(useHeight, tree->minItemHeight);

    /* No fixed height specified */
    return MAX(useHeight, buttonHeight);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_InvalidateHeight --
 *
 *	Marks Item.neededHeight out-of-date.
 *	NOTE: Item.neededHeight is unused.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_InvalidateHeight(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;

    if (self->neededHeight < 0)
	return;
    self->neededHeight = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_FindColumn --
 *
 *	Return a Column record given a zero-based index.
 *
 * Results:
 *	The Column record or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Column *
Item_FindColumn(
    TreeCtrl *tree,		/* Widget info. */
    Item *self,			/* Item record. */
    int columnIndex		/* 0-based index of column to find. */
    )
{
    Column *column;
    int i = 0;

    column = self->columns;
    if (!column)
	return NULL;
    while (column != NULL && i < columnIndex) {
	column = column->next;
	i++;
    }
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * Item_FindColumnFromObj --
 *
 *	Return a Column record given a Tcl_Obj column description.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Item_FindColumnFromObj(
    TreeCtrl *tree,		/* Widget info. */
    Item *item,			/* Item record. */
    Tcl_Obj *obj,		/* Column description. */
    Column **columnPtr,		/* Returned column, or NULL. */
    int *indexPtr		/* May be NULL. Returned 0-based index of
				 * the column. */
    )
{
    TreeColumn treeColumn;
    int columnIndex;

    if (TreeColumn_FromObj(tree, obj, &treeColumn, CFO_NOT_ALL | CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
	return TCL_ERROR;
    columnIndex = TreeColumn_Index(treeColumn);
    (*columnPtr) = Item_FindColumn(tree, item, columnIndex);
    if (indexPtr != NULL)
	(*indexPtr) = columnIndex;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_FindColumn --
 *
 *	Return an item-column token given a zero-based index.
 *
 * Results:
 *	The item-column token or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItemColumn
TreeItem_FindColumn(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int columnIndex		/* 0-based index of column to find. */
    )
{
    return (TreeItemColumn) Item_FindColumn(tree, (Item *) item, columnIndex);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ColumnFromObj --
 *
 *	Return an item-column token given a Tcl_Obj column description.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_ColumnFromObj(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    Tcl_Obj *obj,		/* Column description. */
    TreeItemColumn *columnPtr,	/* Returned column, or NULL. */
    int *indexPtr		/* May be NULL. Returned 0-based index of
				 * the column. */
    )
{
    return Item_FindColumnFromObj(tree, (Item *) item, obj, (Column **) columnPtr, indexPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Item_CreateColumnFromObj --
 *
 *	Return a Column record given a Tcl_Obj column description.
 *	The Column record is allocated if it doesn't exist.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static int
Item_CreateColumnFromObj(
    TreeCtrl *tree,		/* Widget info. */
    Item *item,			/* Item record. */
    Tcl_Obj *obj,		/* Column description. */
    Column **column,		/* Returned column. */
    int *indexPtr,		/* May be NULL. Returned 0-based index of
				 * the column. */
    int *isNew			/* May be NULL. Set to TRUE if the
				 * column record was created. */
    )
{
    TreeColumn treeColumn;
    int columnIndex;

    if (TreeColumn_FromObj(tree, obj, &treeColumn, CFO_NOT_ALL | CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
	return TCL_ERROR;
    columnIndex = TreeColumn_Index(treeColumn);
    (*column) = Item_CreateColumn(tree, item, columnIndex, isNew);
    if (indexPtr != NULL)
	(*indexPtr) = columnIndex;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Indent --
 *
 *	Return the amount of indentation for the given item. This is
 *	the width of the buttons/lines. 
 *
 * Results:
 *	Pixel value >= 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeItem_Indent(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;
    int indent;

    if (ISROOT(self))
	return (tree->showRoot && tree->showButtons && tree->showRootButton) ? tree->useIndent : 0;

    if (tree->updateIndex)
	Tree_UpdateItemIndex(tree);

    indent = tree->useIndent * self->depth;
    if (tree->showRoot || tree->showButtons || tree->showLines)
	indent += tree->useIndent;
    if (tree->showRoot && tree->showButtons && tree->showRootButton)
	indent += tree->useIndent;
    return indent;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemDrawBackground --
 *
 *	Draws part of the background area of an Item. The area is
 *	erased to the -itembackground color of the tree column or the
 *	TreeCtrl -background color. If the TreeCtrl -backgroundimage
 *	option is specified then that image is tiled over the given area.
 *
 *	NOTE: The given area may span multiple columns due to column-
 *	spanning. In this case the -itembackground color of the first column
 *	is used (if any).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

static void
ItemDrawBackground(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn treeColumn,	/* Tree-column token. */
    Item *item,			/* Item record. */
    Column *column,		/* First column. */
    Drawable drawable,		/* Where to draw. */
    int x, int y,		/* Area of the item to draw. */
    int width, int height,	/* ^ */
    int index			/* Used to select a color from the
				 * tree-column's -itembackground option. */
    )
{
    GC gc = None;

    gc = TreeColumn_BackgroundGC(treeColumn, index);
    if (gc == None)
	gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
    /*
     * FIXME: If the background image is non-transparent, there is no
     * need to erase first.
     */
    XFillRectangle(tree->display, drawable, gc, x, y, width, height);
    if (tree->backgroundImage != NULL) {
	Tree_DrawTiledImage(tree, drawable, tree->backgroundImage, x, y, 
		x + width, y + height,
		tree->drawableXOrigin, tree->drawableYOrigin);
    }
}

#ifdef COLUMN_SPAN

/*
 * The following structure holds information about which item column
 * is displayed at a given tree column.
 */
typedef struct SpanInfo {
    TreeColumn treeColumn;	/* Always non-null. */
    TreeItemColumn itemColumn;	/* May be null. */
    int itemColumnIndex;	/* Actual Column displayed here. */
    int width;			/* Width of the tree column. */
} SpanInfo;

/*
 *----------------------------------------------------------------------
 *
 * Item_GetSpans --
 *
 *	Fills an array of SpanInfo records, one per tree column, with
 *	information about the Column displayed in each tree column.
 *
 * Results:
 *	spans[] is filled in.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Item_GetSpans(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    SpanInfo spans[]		/* Returned span records. */
    )
{
    Item *self = (Item *) item_;
    TreeColumn treeColumn = tree->columns;
    Column *column = self->columns;
    int columnIndex = 0, itemColumnIndex = 0, span = 1;

    while (treeColumn != NULL) {
	if (--span == 0) {
	    if (TreeColumn_Visible(treeColumn))
		span = column ? column->span : 1;
	    else
		span = 1;
	    itemColumnIndex = columnIndex;
	}
	spans[columnIndex].treeColumn = treeColumn;
	spans[columnIndex].itemColumn = (TreeItemColumn) column;
	spans[columnIndex].itemColumnIndex = itemColumnIndex;
	spans[columnIndex].width = TreeColumn_UseWidth(treeColumn);
	++columnIndex;
	treeColumn = TreeColumn_Next(treeColumn);
	if (column != NULL)
	    column = column->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Draw --
 *
 *	Draws part of an Item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_Draw(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int x, int y,		/* Drawable coordinates of the item. */
    int width, int height,	/* Total size of the item. */
    Drawable drawable,		/* Where to draw. */
    int minX, int maxX,		/* Left/right edge that needs to be drawn. */
    int index			/* Used to select a color from a
				 * tree-column's -itembackground option. */
    )
{
    Item *self = (Item *) item_;
    int indent, columnWidth, totalWidth;
    Column *column;
    StyleDrawArgs drawArgs;
    TreeColumn treeColumn;
    int i, columnIndex;
    SpanInfo staticSpans[STATIC_SIZE], *spans = staticSpans;

    STATIC_ALLOC(spans, SpanInfo, tree->columnCount);
    Item_GetSpans(tree, item_, spans);

    drawArgs.tree = tree;
    drawArgs.drawable = drawable;

    totalWidth = 0;
    for (columnIndex = 0; columnIndex < tree->columnCount; columnIndex++) {
	treeColumn = spans[columnIndex].treeColumn;

	/* A preceding item column is displayed here */
	if (spans[columnIndex].itemColumnIndex != columnIndex)
	    continue;

	/* If this is the single visible column, use the provided width which
	 * may be different than the column's width */
	if ((tree->columnCountVis == 1) && (treeColumn == tree->columnVis)) {
	    columnWidth = width;
	    if ((columnWidth >= 0) &&
		    (x + totalWidth < maxX) &&
		    (x + totalWidth + columnWidth > minX)) {
		column = (Column *) spans[columnIndex].itemColumn;
		ItemDrawBackground(tree, treeColumn, self, column, drawable,
			x + totalWidth, y, columnWidth, height, index);
	    }

	/* More than one column is visible, or this is not the visible
	 * column. Add up the widths of all columns this column spans */
	} else {
	    columnWidth = 0;
	    for (i = columnIndex; i < tree->columnCount; i++) {
		int x2 = x + totalWidth + columnWidth;
		if (spans[i].itemColumnIndex != columnIndex)
		    break;
		if ((spans[i].width >= 0) && (x2 < maxX) &&
			(x2 + spans[i].width > minX)) {
		    column = (Column *) spans[columnIndex].itemColumn;
		    ItemDrawBackground(tree, spans[i].treeColumn, self, column,
			    drawable, x2, y, spans[i].width, height, index);
		}
		columnWidth += spans[i].width;
	    }
	}
	if (columnWidth <= 0)
	    continue;
	if ((x + totalWidth < maxX) && (x + totalWidth + columnWidth > minX)) {
	    column = (Column *) spans[columnIndex].itemColumn;
	    if ((column != NULL) && (column->style != NULL)) {
		if (treeColumn == tree->columnTree)
		    indent = TreeItem_Indent(tree, item_);
		else
		    indent = 0;
		drawArgs.state = self->state | column->cstate;
		drawArgs.style = column->style;
		drawArgs.indent = indent;
		drawArgs.x = x + totalWidth;
		drawArgs.y = y;
		drawArgs.width = columnWidth;
		drawArgs.height = height;
		drawArgs.justify = TreeColumn_Justify(treeColumn);
		TreeStyle_Draw(&drawArgs);
	    }
	    if (treeColumn == tree->columnTree) {
		if (tree->showLines)
		    TreeItem_DrawLines(tree, item_, x, y, width, height,
			    drawable);
		if (tree->showButtons)
		    TreeItem_DrawButton(tree, item_, x, y, width, height,
			    drawable);
	    }
	}
	totalWidth += columnWidth;
    }

    STATIC_FREE(spans, SpanInfo, tree->columnCount);
}

#else /* COLUMN_SPAN */

void TreeItem_Draw(TreeCtrl *tree, TreeItem item_, int x, int y,
	int width, int height, Drawable drawable, int minX, int maxX, int index)
{
    Item *self = (Item *) item_;
    int indent, columnWidth, totalWidth;
    Column *column;
    StyleDrawArgs drawArgs;
    TreeColumn treeColumn;

    drawArgs.tree = tree;
    drawArgs.drawable = drawable;

    totalWidth = 0;
    treeColumn = tree->columns;
    column = self->columns;
    while (treeColumn != NULL) {
	if (!TreeColumn_Visible(treeColumn))
	    columnWidth = 0;
	else if (tree->columnCountVis == 1)
	    columnWidth = width;
	else
	    columnWidth = TreeColumn_UseWidth(treeColumn);
	if (columnWidth > 0) {
	    if (treeColumn == tree->columnTree) {
		indent = TreeItem_Indent(tree, item_);
	    } else
		indent = 0;
	    if ((x /* + indent */ + totalWidth < maxX) &&
		    (x + totalWidth + columnWidth > minX)) {
		ItemDrawBackground(tree, treeColumn, self, column, drawable,
			x + totalWidth /* + indent*/ , y,
			columnWidth /* - indent */, height,
			index);
		if ((column != NULL) && (column->style != NULL)) {
		    drawArgs.state = self->state | column->cstate;
		    drawArgs.style = column->style;
		    drawArgs.indent = indent;
		    drawArgs.x = x + totalWidth;
		    drawArgs.y = y;
		    drawArgs.width = columnWidth;
		    drawArgs.height = height;
		    drawArgs.justify = TreeColumn_Justify(treeColumn);
		    TreeStyle_Draw(&drawArgs);
		}
		if (treeColumn == tree->columnTree) {
		    if (tree->showLines)
			TreeItem_DrawLines(tree, item_, x, y, width, height,
				drawable);
		    if (tree->showButtons)
			TreeItem_DrawButton(tree, item_, x, y, width, height,
				drawable);
		}
	    }
	    totalWidth += columnWidth;
	}
	treeColumn = TreeColumn_Next(treeColumn);
	if (column != NULL)
	    column = column->next;
    }
}

#endif /* not COLUMN_SPAN */

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_DrawLines --
 *
 *	Draws horizontal and vertical lines indicating parent-child
 *	relationship in an item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_DrawLines(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int x, int y,		/* Drawable coordinates of the item. */
    int width, int height,	/* Total size of the item. */
    Drawable drawable		/* Where to draw. */
    )
{
    Item *self = (Item *) item_;
    Item *item, *parent;
    int indent, left, lineLeft, lineTop;
    int hasPrev, hasNext;
    int i, vert = 0;

    indent = TreeItem_Indent(tree, item_);

    /* Left edge of button/line area */
    left = x + tree->columnTreeLeft + indent - tree->useIndent;

    /* Left edge of vertical line */
    lineLeft = left + (tree->useIndent - tree->lineThickness) / 2;

    /* Top edge of horizontal line */
    lineTop = y + (height - tree->lineThickness) / 2;

    /* NOTE: The next three checks do not call TreeItem_ReallyVisible()
     * since 'self' is ReallyVisible */

    /* Check for ReallyVisible previous sibling */
    item = self->prevSibling;
    while ((item != NULL) && !item->isVisible)
	item = item->prevSibling;
    hasPrev = (item != NULL);

    /* Check for ReallyVisible parent */
    if ((self->parent != NULL) && (!ISROOT(self->parent) || tree->showRoot))
	hasPrev = TRUE;

    /* Check for ReallyVisible next sibling */
    item = self->nextSibling;
    while ((item != NULL) && !item->isVisible)
	item = item->nextSibling;
    hasNext = (item != NULL);

    /* Option: Don't connect children of root item */
    if ((self->parent != NULL) && ISROOT(self->parent) && !tree->showRootLines)
	hasPrev = hasNext = FALSE;

    /* Vertical line to parent and/or previous/next sibling */
    if (hasPrev || hasNext) {
	int top = y, bottom = y + height;

	if (!hasPrev)
	    top = lineTop;
	if (!hasNext)
	    bottom = lineTop + tree->lineThickness;

	if (tree->lineStyle == LINE_STYLE_DOT) {
	    for (i = 0; i < tree->lineThickness; i++)
		VDotLine(tree, drawable, tree->lineGC,
			lineLeft + i,
			top,
			bottom);
	} else
	    XFillRectangle(tree->display, drawable, tree->lineGC,
		    lineLeft,
		    top,
		    tree->lineThickness,
		    bottom - top);

	/* Don't overlap horizontal line */
	vert = tree->lineThickness;
    }

    /* Horizontal line to self */
    if (hasPrev || hasNext) {
	if (tree->lineStyle == LINE_STYLE_DOT) {
	    for (i = 0; i < tree->lineThickness; i++)
		HDotLine(tree, drawable, tree->lineGC,
			lineLeft + vert,
			lineTop + i,
			x + tree->columnTreeLeft + indent);
	} else
	    XFillRectangle(tree->display, drawable, tree->lineGC,
		    lineLeft + vert,
		    lineTop,
		    left + tree->useIndent - (lineLeft + vert),
		    tree->lineThickness);
    }

    /* Vertical lines from ancestors to their next siblings */
    for (parent = self->parent;
	 parent != NULL;
	 parent = parent->parent) {
	lineLeft -= tree->useIndent;

	/* Option: Don't connect children of root item */
	if ((parent->parent != NULL) && ISROOT(parent->parent) && !tree->showRootLines)
	    continue;

	/* Check for ReallyVisible next sibling */
	item = parent->nextSibling;
	while ((item != NULL) && !item->isVisible)
	    item = item->nextSibling;

	if (item != NULL) {
	    if (tree->lineStyle == LINE_STYLE_DOT) {
		for (i = 0; i < tree->lineThickness; i++)
		    VDotLine(tree, drawable, tree->lineGC,
			    lineLeft + i,
			    y,
			    y + height);
	    } else
		XFillRectangle(tree->display, drawable, tree->lineGC,
			lineLeft,
			y,
			tree->lineThickness,
			height);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_DrawButton --
 *
 *	Draws the button (if any) in an item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in a drawable.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_DrawButton(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int x, int y,		/* Drawable coordinates of the item. */
    int width, int height,	/* Total size of the item. */
    Drawable drawable		/* Where to draw. */
    )
{
    Item *self = (Item *) item_;
    int indent, left, lineLeft, lineTop;
    int buttonLeft, buttonTop, w1;
    int macoffset = 0;
    Tk_Image image;
    Pixmap bitmap;

    if (!self->hasButton)
	return;
    if (ISROOT(self) && !tree->showRootButton)
	return;

#if defined(MAC_TCL) || defined(MAC_OSX_TK)
	/* QuickDraw on Mac is offset by one pixel in both x and y. */
	macoffset = 1;
#endif

    indent = TreeItem_Indent(tree, item_);

    /* Left edge of button/line area */
    left = x + tree->columnTreeLeft + indent - tree->useIndent;

    image = PerStateImage_ForState(tree, &tree->buttonImage, self->state, NULL);
    if (image != NULL) {
	int imgW, imgH;
	Tk_SizeOfImage(image, &imgW, &imgH);
	Tk_RedrawImage(image, 0, 0, imgW, imgH, drawable,
	    left + (tree->useIndent - imgW) / 2,
	    y + (height - imgH) / 2);
	return;
    }

    bitmap = PerStateBitmap_ForState(tree, &tree->buttonBitmap, self->state, NULL);
    if (bitmap != None) {
	int bmpW, bmpH;
	int bx, by;
	Tk_SizeOfBitmap(tree->display, bitmap, &bmpW, &bmpH);
	bx = left + (tree->useIndent - bmpW) / 2;
	by = y + (height - bmpH) / 2;
	Tree_DrawBitmap(tree, bitmap, drawable, NULL, NULL,
		0, 0, (unsigned int) bmpW, (unsigned int) bmpH,
		bx, by);
	return;
    }

    if (tree->useTheme) {
	int bw, bh;
	if (TreeTheme_GetButtonSize(tree, drawable, self->state & STATE_OPEN,
	    &bw, &bh) == TCL_OK) {
	    if (TreeTheme_DrawButton(tree, drawable, self->state & STATE_OPEN,
		left + (tree->useIndent - bw) / 2, y + (height - bh) / 2,
		bw, bh) == TCL_OK) {
		return;
	    }
	}
    }

    w1 = tree->buttonThickness / 2;

    /* Left edge of vertical line */
    /* Make sure this matches TreeItem_DrawLines() */
    lineLeft = left + (tree->useIndent - tree->buttonThickness) / 2;

    /* Top edge of horizontal line */
    /* Make sure this matches TreeItem_DrawLines() */
    lineTop = y + (height - tree->buttonThickness) / 2;

    buttonLeft = left + (tree->useIndent - tree->buttonSize) / 2;
    buttonTop = y + (height - tree->buttonSize) / 2;

    /* Erase button background */
    XFillRectangle(tree->display, drawable,
	    Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC),
	    buttonLeft + tree->buttonThickness,
	    buttonTop + tree->buttonThickness,
	    tree->buttonSize - tree->buttonThickness,
	    tree->buttonSize - tree->buttonThickness);

    /* Draw button outline */
    XDrawRectangle(tree->display, drawable, tree->buttonGC,
	    buttonLeft + w1,
	    buttonTop + w1,
	    tree->buttonSize - tree->buttonThickness + macoffset,
	    tree->buttonSize - tree->buttonThickness + macoffset);

    /* Horizontal '-' */
    XFillRectangle(tree->display, drawable, tree->buttonGC,
	    buttonLeft + tree->buttonThickness * 2,
	    lineTop,
	    tree->buttonSize - tree->buttonThickness * 4,
	    tree->buttonThickness);

    if (!(self->state & STATE_OPEN)) {
	/* Finish '+' */
	XFillRectangle(tree->display, drawable, tree->buttonGC,
		lineLeft,
		buttonTop + tree->buttonThickness * 2,
		tree->buttonThickness,
		tree->buttonSize - tree->buttonThickness * 4);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_UpdateWindowPositions --
 *
 *	Updates the geometry of any window elements. Called by the
 *	display code when an item was possibly scrolled.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Windows in window elements may be resized/repositioned.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_UpdateWindowPositions(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int x, int y,		/* Window coordinates of the item. */
    int width, int height	/* Total size of the item. */
    )
{
    Item *self = (Item *) item_;
    int indent, columnWidth, totalWidth;
    Column *column;
    StyleDrawArgs drawArgs;
    TreeColumn treeColumn;

    drawArgs.tree = tree;
    drawArgs.drawable = None;

    totalWidth = 0;
    treeColumn = tree->columns;
    column = self->columns;
    while ((column != NULL) && (treeColumn != NULL)) {
	if (!TreeColumn_Visible(treeColumn))
	    columnWidth = 0;
	else if (tree->columnCountVis == 1)
	    columnWidth = width;
	else
	    columnWidth = TreeColumn_UseWidth(treeColumn);
	if (columnWidth > 0) {
	    if (treeColumn == tree->columnTree)
		indent = TreeItem_Indent(tree, item_);
	    else
		indent = 0;
	    if ((column != NULL) && (column->style != NULL)) {
		drawArgs.state = self->state | column->cstate;
		drawArgs.style = column->style;
		drawArgs.indent = indent;
		drawArgs.x = x + totalWidth;
		drawArgs.y = y;
		drawArgs.width = columnWidth;
		drawArgs.height = height;
		drawArgs.justify = TreeColumn_Justify(treeColumn);
		TreeStyle_UpdateWindowPositions(&drawArgs);
	    }
	    totalWidth += columnWidth;
	}
	treeColumn = TreeColumn_Next(treeColumn);
	column = column->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_OnScreen --
 *
 *	Called by the display code when the item becomes visible
 *	(i.e., actually displayed) or hidden.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Windows in window elements may be mapped/unmapped.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_OnScreen(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int onScreen		/* TRUE if item is displayed. */
    )
{
    Item *self = (Item *) item_;
    Column *column = self->columns;

    while (column != NULL) {
	if (column->style != NULL) {
	    TreeStyle_OnScreen(tree, column->style, onScreen);
	}
	column = column->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ReallyVisible --
 *
 *	Return whether the given Item could be displayed.
 *
 * Results:
 *	TRUE if the item's -visible is TRUE, all of its ancestors'
 *	-visible options are TRUE, and all of its ancestors are open.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeItem_ReallyVisible(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *self = (Item *) item_;
#if 0
    if (tree->updateIndex)
	Tree_UpdateItemIndex(tree);
    return self->indexVis != -1;
#else
    if (!tree->updateIndex)
	return self->indexVis != -1;

    if (!self->isVisible)
	return 0;
    if (self->parent == NULL)
	return ISROOT(self) ? tree->showRoot : 0;
    if (ISROOT(self->parent)) {
	if (!self->parent->isVisible)
	    return 0;
	if (!tree->showRoot)
	    return 1;
	if (!(self->parent->state & STATE_OPEN))
	    return 0;
    }
    if (!self->parent->isVisible || !(self->parent->state & STATE_OPEN))
	return 0;
    return TreeItem_ReallyVisible(tree, (TreeItem) self->parent);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_RootAncestor --
 *
 *	Return the toplevel ancestor of an Item.
 *
 * Results:
 *	Returns the root, or an orphan ancestor, or the given Item.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeItem TreeItem_RootAncestor(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;

    while (item->parent != NULL)
	item = item->parent;
    return (TreeItem) item;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_IsAncestor --
 *
 *	Determine if one Item is the ancestor of another.
 *
 * Results:
 *	TRUE if item1 is an ancestor of item2.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_IsAncestor(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item1,		/* Possible ancestor. */
    TreeItem item2		/* Item to check ancestry of. */
    )
{
    if (item1 == item2)
	return 0;
    while (item2 && item2 != item1)
	item2 = (TreeItem) ((Item *) item2)->parent;
    return item2 != NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_ToObj --
 *
 *	Convert an Item to a Tcl_Obj.
 *
 * Results:
 *	A new Tcl_Obj representing the Item.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *TreeItem_ToObj(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    if (tree->itemPrefixLen) {
	char buf[100 + TCL_INTEGER_SPACE];
	(void) sprintf(buf, "%s%d", tree->itemPrefix, ((Item *) item_)->id);
	return Tcl_NewStringObj(buf, -1);
    }
    return Tcl_NewIntObj(((Item *) item_)->id);
}

/*
 *----------------------------------------------------------------------
 *
 * Item_Configure --
 *
 *	This procedure is called to process an objc/objv list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	an Item.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	Configuration information gets set for item;  old resources get
 *	freed, if there were any.
 *
 *----------------------------------------------------------------------
 */

static int Item_Configure(
    TreeCtrl *tree,		/* Widget info. */
    Item *item,			/* Item to configure. */
    int objc,			/* Number of arguments */
    Tcl_Obj *CONST objv[]	/* Array of arguments */
    )
{
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;
    int lastVisible = item->isVisible;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) item, tree->itemOptionTable,
			objc, objv, tree->tkwin, &savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* xxx */

	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /* xxx */

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    if (mask & ITEM_CONF_SIZE) {
	Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
    }

    if (mask & ITEM_CONF_BUTTON)
	Tree_InvalidateItemDInfo(tree, (TreeItem) item, NULL);

    if ((mask & ITEM_CONF_VISIBLE) && (item->isVisible != lastVisible)) {
	/* May change the width of any column */
	Tree_InvalidateColumnWidth(tree, -1);

	/* If this is the last child, redraw the lines of the previous
	 * sibling and all of its descendants because the line from
	 * the previous sibling to us is appearing/disappearing */
	if ((item->prevSibling != NULL) &&
		(item->nextSibling == NULL) &&
		tree->showLines) {
	    Item *last = item->prevSibling;
	    while (last->lastChild != NULL)
		last = last->lastChild;
	    Tree_InvalidateItemDInfo(tree,
		    (TreeItem) item->prevSibling,
		    (TreeItem) last);
	}

	tree->updateIndex = 1;
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES | DINFO_REDO_SELECTION);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemCreateCmd --
 *
 *	This procedure is invoked to process the [item create] widget
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ItemCreateCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *optionNames[] = { "-button", "-count", "-height",
	"-nextsibling", "-open", "-parent", "-prevsibling", "-returnid",
	"-visible",
	(char *) NULL };
    enum { OPT_BUTTON, OPT_COUNT, OPT_HEIGHT, OPT_NEXTSIBLING,
	OPT_OPEN, OPT_PARENT, OPT_PREVSIBLING, OPT_RETURNID, OPT_VISIBLE };
    int index, i, count = 1, button = 0, returnId = 1, open = 1, visible = 1;
    int height = 0;
    TreeItem _item;
    Item *item, *parent = NULL, *prevSibling = NULL, *nextSibling = NULL;
    Item *head = NULL, *tail = NULL;
    Tcl_Obj *listObj = NULL;

    for (i = 3; i < objc; i += 2) {
	if (Tcl_GetIndexFromObj(interp, objv[i], optionNames, "option", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (i + 1 == objc) {
	    FormatResult(interp, "missing value for \"%s\" option",
		    optionNames[index]);
	    return TCL_ERROR;
	}
	switch (index) {
	    case OPT_BUTTON:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &button)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case OPT_COUNT:
		if (Tcl_GetIntFromObj(interp, objv[i + 1], &count) != TCL_OK)
		    return TCL_ERROR;
		if (count < 0) {
		    FormatResult(interp, "bad count \"%d\": must be > 0",
			    count);
		}
		break;
	    case OPT_HEIGHT:
		if (Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
			&height) != TCL_OK)
		    return TCL_ERROR;
		if (height < 0) {
		    FormatResult(interp, "bad screen distance \"%s\": must be > 0",
			    Tcl_GetString(objv[i + 1]));
		}
		break;
	    case OPT_NEXTSIBLING:
		if (TreeItem_FromObj(tree, objv[i + 1], &_item,
			IFO_NOTROOT | IFO_NOTORPHAN) != TCL_OK) {
		    return TCL_ERROR;
		}
		nextSibling = (Item *) _item;
		parent = prevSibling = NULL;
		break;
	    case OPT_OPEN:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &open)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case OPT_PARENT:
		if (TreeItem_FromObj(tree, objv[i + 1], &_item, 0) != TCL_OK) {
		    return TCL_ERROR;
		}
		parent = (Item *) _item;
		prevSibling = nextSibling = NULL;
		break;
	    case OPT_PREVSIBLING:
		if (TreeItem_FromObj(tree, objv[i + 1], &_item,
			IFO_NOTROOT | IFO_NOTORPHAN) != TCL_OK) {
		    return TCL_ERROR;
		}
		prevSibling = (Item *) _item;
		parent = nextSibling = NULL;
		break;
	    case OPT_RETURNID:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &returnId)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	    case OPT_VISIBLE:
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &visible)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		break;
	}
    }

    if (!count)
	return TCL_OK;

    if (returnId)
	listObj = Tcl_NewListObj(0, NULL);

    /* Don't allow non-deleted items to become children of a
    * deleted item. */
    if ((parent && IS_DELETED(parent)) ||
	(prevSibling && IS_DELETED(prevSibling->parent)) ||
	(nextSibling && IS_DELETED(nextSibling->parent)))
	parent = prevSibling = nextSibling = NULL;

    for (i = 0; i < count; i++) {
	item = Item_Alloc(tree);
	item->hasButton = button;
	item->isVisible = visible;
	if (!open)
	    item->state &= ~STATE_OPEN;
	item->fixedHeight = height;

	/* Apply default styles */
	if (tree->defaultStyle.numStyles) {
	    int i, n = MIN(tree->columnCount, tree->defaultStyle.numStyles);

	    for (i = 0; i < n; i++) {
		Column *column = Item_CreateColumn(tree, item, i, NULL);
		if (tree->defaultStyle.styles[i] != NULL) {
		    column->style = TreeStyle_NewInstance(tree,
			    tree->defaultStyle.styles[i]);
		}
	    }
	}

	/* Link the new items together as siblings */
	if (parent || prevSibling || nextSibling) {
	    if (head == NULL)
		head = item;
	    if (tail != NULL) {
		tail->nextSibling = item;
		item->prevSibling = tail;
	    }
	    tail = item;
	}

	if (returnId)
	    Tcl_ListObjAppendElement(interp, listObj, TreeItem_ToObj(tree,
		    (TreeItem) item));
    }

    if (parent != NULL) {
	head->prevSibling = parent->lastChild;
	if (parent->lastChild != NULL)
	    parent->lastChild->nextSibling = head;
	else
	    parent->firstChild = head;
	parent->lastChild = tail;
    } else if (prevSibling != NULL) {
	parent = prevSibling->parent;
	if (prevSibling->nextSibling != NULL)
	    prevSibling->nextSibling->prevSibling = tail;
	else
	    parent->lastChild = tail;
	head->prevSibling = prevSibling;
	tail->nextSibling = prevSibling->nextSibling;
	prevSibling->nextSibling = head;
    } else if (nextSibling != NULL) {
	parent = nextSibling->parent;
	if (nextSibling->prevSibling != NULL)
	    nextSibling->prevSibling->nextSibling = head;
	else
	    parent->firstChild = head;
	head->prevSibling = nextSibling->prevSibling;
	tail->nextSibling = nextSibling;
	nextSibling->prevSibling = tail;
    }

    if (parent != NULL) {
	for (item = head; item != NULL; item = item->nextSibling) {
	    item->parent = parent;
	    item->depth = parent->depth + 1;
	}
	parent->numChildren += count;
	TreeItem_AddToParent(tree, (TreeItem) head);
    }

    if (returnId)
	Tcl_SetObjResult(interp, listObj);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NoStyleMsg --
 *
 *	Utility to set the interpreter result with a message indicating
 *	a Column has no assigned style.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Interpreter result is changed.
 *
 *----------------------------------------------------------------------
 */

static void
NoStyleMsg(
    TreeCtrl *tree,		/* Widget info. */
    Item *item,			/* Item record. */
    int columnIndex		/* 0-based index of the column that
				 * has no style. */
    )
{
    FormatResult(tree->interp,
	    "item %s%d column %s%d has no style",
	    tree->itemPrefix, item->id,
	    tree->columnPrefix,
	    TreeColumn_GetID(Tree_FindColumn(tree, columnIndex)));
}

/*
 *----------------------------------------------------------------------
 *
 * ItemElementCmd --
 *
 *	This procedure is invoked to process the [item element] widget
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ItemElementCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = { "actual", "cget", "configure",
	"perstate", (char *) NULL };
    enum { COMMAND_ACTUAL, COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_PERSTATE };
    int index;
    int columnIndex;
    Column *column;
    TreeItem _item;
    Item *item;

    if (objc < 7) {
	Tcl_WrongNumArgs(interp, 3, objv, "command item column element ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
	return TCL_ERROR;

    if (TreeItem_FromObj(tree, objv[4], &_item, 0) != TCL_OK)
	return TCL_ERROR;
    item = (Item *) _item;

    if (Item_FindColumnFromObj(tree, item, objv[5], &column, &columnIndex) != TCL_OK)
	return TCL_ERROR;

    if ((column == NULL) || (column->style == NULL)) {
	NoStyleMsg(tree, item, columnIndex);
	return TCL_ERROR;
    }

    switch (index) {
	/* T item element perstate I C E option ?stateList? */
	case COMMAND_ACTUAL:
	case COMMAND_PERSTATE:
	{
	    int state = item->state | column->cstate;

	    if (objc < 8 || objc > 9) {
		Tcl_WrongNumArgs(tree->interp, 4, objv,
			"item column element option ?stateList?");
		return TCL_ERROR;
	    }
	    if (objc == 9) {
		int i, listObjc;
		Tcl_Obj **listObjv;
		int states[3];

		if (Tcl_ListObjGetElements(interp, objv[8], &listObjc,
			&listObjv) != TCL_OK)
		    return TCL_ERROR;

		states[0] = states[1] = states[2] = 0;
		for (i = 0; i < listObjc; i++) {
		    if (Tree_StateFromObj(tree, listObjv[i], states, NULL,
				SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
			return TCL_ERROR;
		}
		state = states[STATE_OP_ON];
	    }
	    return TreeStyle_ElementActual(tree, column->style,
		    state, objv[6], objv[7]);
	}

	/* T item element cget I C E option */
	case COMMAND_CGET:
	{
	    if (objc != 8) {
		Tcl_WrongNumArgs(tree->interp, 4, objv,
			"item column element option");
		return TCL_ERROR;
	    }
	    return TreeStyle_ElementCget(tree, (TreeItem) item,
		    (TreeItemColumn) column, column->style, objv[6], objv[7]);
	}

	/* T item element configure I C E ... */
	case COMMAND_CONFIGURE:
	{
	    /* T item element configure I C E option value \
	     *     + E option value , C E option value */
	    int eMask, cMask = 0, iMask = 0;
	    int indexElem = 6;
	    int result;

	    while (1) {
		int numArgs = 0;
		char breakChar = '\0';

		/* Look for a + or , */
		for (index = indexElem + 1; index < objc; index++) {
		    if (numArgs % 2 == 0) {
			int length;
			char *s = Tcl_GetStringFromObj(objv[index], &length);

			if ((length == 1) && ((s[0] == '+') || (s[0] == ','))) {
			    breakChar = s[0];
			    break;
			}
		    }
		    numArgs++;
		}

		/* Require at least one option-value pair if more than one
		 * element is specified */
		if (breakChar && numArgs < 2) {
		    FormatResult(interp,
			"missing option-value pair after element \"%s\"",
			Tcl_GetString(objv[indexElem]));
		    result = TCL_ERROR;
		    break;
		}

		result = TreeStyle_ElementConfigure(tree, (TreeItem) item,
			(TreeItemColumn) column, column->style, objv[indexElem],
			numArgs, (Tcl_Obj **) objv + indexElem + 1, &eMask);
		if (result != TCL_OK)
		    break;

		cMask |= eMask;
		if (cMask & CS_LAYOUT) {
		    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
		    Tree_InvalidateColumnWidth(tree, columnIndex);
		}
		iMask |= cMask;

		if (breakChar) {

		    if (index == objc - 1) {
			FormatResult(interp, "missing %s after \"%c\"",
			    (breakChar == '+') ? "element name" : "column",
			    breakChar);
			result = TCL_ERROR;
			break;
		    }

		    /* + indicates start of another element */
		    if (breakChar == '+') {
			indexElem = index + 1;
		    }

		    /* , indicates start of another column */
		    else if (breakChar == ',') {
			if (Item_FindColumnFromObj(tree, item, objv[index + 1],
				&column, &columnIndex) != TCL_OK) {
			    result = TCL_ERROR;
			    break;
			}
			if ((column == NULL) || (column->style == NULL)) {
			    NoStyleMsg(tree, item, columnIndex);
			    result = TCL_ERROR;
			    break;
			}
			indexElem = index + 2;

			if (indexElem == objc) {
			    FormatResult(interp,
				"missing element name after column \"%s\"",
				Tcl_GetString(objv[index + 1]));
			    result = TCL_ERROR;
			    break;
			}
		    }
		} else if (index == objc)
		    break;
	    }
	    if (iMask & CS_LAYOUT) {
		TreeItem_InvalidateHeight(tree, (TreeItem) item);
		Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    }
	    else if (iMask & CS_DISPLAY)
		Tree_InvalidateItemDInfo(tree, (TreeItem) item, NULL);
	    return result;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemStyleCmd --
 *
 *	This procedure is invoked to process the [item style] widget
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ItemStyleCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = { "elements", "map", "set", (char *) NULL };
    enum { COMMAND_ELEMENTS, COMMAND_MAP, COMMAND_SET };
    int index;
    TreeItem _item;
    Item *item;

    if (objc < 5) {
	Tcl_WrongNumArgs(interp, 3, objv, "command item ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK) {
	return TCL_ERROR;
    }

    if (TreeItem_FromObj(tree, objv[4], &_item, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    item = (Item *) _item;

    switch (index) {
	/* T item style elements I C */
	case COMMAND_ELEMENTS:
	{
	    Column *column;
	    int columnIndex;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 4, objv, "item column");
		return TCL_ERROR;
	    }
	    if (Item_FindColumnFromObj(tree, item, objv[5], &column, &columnIndex) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if ((column == NULL) || (column->style == NULL)) {
		NoStyleMsg(tree, item, columnIndex);
		return TCL_ERROR;
	    }
	    TreeStyle_ListElements(tree, column->style);
	    break;
	}

	/* T item style map I C S map */
	case COMMAND_MAP:
	{
	    TreeStyle style;
	    Column *column;
	    int columnIndex;
	    int objcM;
	    Tcl_Obj **objvM;

	    if (objc != 8) {
		Tcl_WrongNumArgs(interp, 4, objv, "item column style map");
		return TCL_ERROR;
	    }
	    if (Item_CreateColumnFromObj(tree, item, objv[5], &column,
		    &columnIndex, NULL) != TCL_OK)
		return TCL_ERROR;
	    if (TreeStyle_FromObj(tree, objv[6], &style) != TCL_OK)
		return TCL_ERROR;
	    if (column->style != NULL) {
		if (Tcl_ListObjGetElements(interp, objv[7], &objcM, &objvM) != TCL_OK)
		    return TCL_ERROR;
		if (objcM & 1) {
		    FormatResult(interp, "list must contain even number of elements");
		    return TCL_ERROR;
		}
		if (TreeStyle_Remap(tree, column->style, style, objcM, objvM) != TCL_OK)
		    return TCL_ERROR;
	    } else
		column->style = TreeStyle_NewInstance(tree, style);
	    Tree_InvalidateColumnWidth(tree, columnIndex);
	    TreeItem_InvalidateHeight(tree, (TreeItem) item);
	    Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
	    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    break;
	}

	/* T item style set I ?C? ?S? ?C S ...?*/
	case COMMAND_SET:
	{
	    TreeStyle style;
	    Column *column;
	    int i, columnIndex, length;

	    if (objc < 5) {
		Tcl_WrongNumArgs(interp, 4, objv, "item ?column? ?style? ?column style ...?");
		return TCL_ERROR;
	    }
	    if (objc == 5) {
		TreeColumn treeColumn = tree->columns;
		Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
		column = item->columns;
		while (treeColumn != NULL) {
		    if ((column != NULL) && (column->style != NULL))
			Tcl_ListObjAppendElement(interp, listObj,
				TreeStyle_ToObj(column->style));
		    else
			Tcl_ListObjAppendElement(interp, listObj,
				Tcl_NewObj());
		    treeColumn = TreeColumn_Next(treeColumn);
		    if (column != NULL)
			column = column->next;
		}
		Tcl_SetObjResult(interp, listObj);
		break;
	    }
	    if (objc == 6) {
		if (Item_FindColumnFromObj(tree, item, objv[5], &column, NULL) != TCL_OK)
		    return TCL_ERROR;
		if ((column != NULL) && (column->style != NULL))
		    Tcl_SetObjResult(interp, TreeStyle_ToObj(column->style));
		break;
	    }
	    for (i = 5; i < objc; i += 2) {
		if (Item_CreateColumnFromObj(tree, item, objv[i], &column,
			&columnIndex, NULL) != TCL_OK)
		    return TCL_ERROR;
		if (i + 1 == objc) {
		    FormatResult(interp, "missing style for column \"%s\"",
			Tcl_GetString(objv[i]));
		    return TCL_ERROR;
		}
		(void) Tcl_GetStringFromObj(objv[i + 1], &length);
		if (length == 0) {
		    if (column->style == NULL)
			continue;
		    TreeItemColumn_ForgetStyle(tree, (TreeItemColumn) column);
		} else {
		    if (TreeStyle_FromObj(tree, objv[i + 1], &style) != TCL_OK)
			return TCL_ERROR;
		    if (column->style != NULL) {
			if (TreeStyle_GetMaster(tree, column->style) == style)
			    continue;
			TreeItemColumn_ForgetStyle(tree, (TreeItemColumn) column);
		    }
		    column->style = TreeStyle_NewInstance(tree, style);
		}
		Tree_InvalidateColumnWidth(tree, columnIndex);
		TreeItem_InvalidateHeight(tree, (TreeItem) item);
		Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
		TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    }
	    break;
	}
    }

    return TCL_OK;
}

/* one per column per SortItem */
struct SortItem1
{
    long longValue;
    double doubleValue;
    char *string;
};

/* one per Item */
struct SortItem
{
    Item *item;
    struct SortItem1 *item1;
    Tcl_Obj *obj; /* TreeItem_ToObj() */
};

typedef struct SortData SortData;

/* Used to process -element option */
struct SortElement
{
    TreeStyle style;
    TreeElement elem;
    int elemIndex;
};

/* One per TreeColumn */
struct SortColumn
{
    int (*proc)(SortData *, struct SortItem *, struct SortItem *, int); 
    int sortBy;
    int column;
    int order;
    Tcl_Obj *command;
    struct SortElement elems[20];
    int elemCount;
};

/* Data for sort as a whole */
struct SortData
{
    TreeCtrl *tree;
    struct SortItem *items;
    struct SortItem1 *item1s; /* SortItem.item1 points in here */
#define MAX_SORT_COLUMNS 40
    struct SortColumn columns[MAX_SORT_COLUMNS];
    int columnCount; /* max number of columns to compare */
    int result;
};

/* from Tcl 8.4.0 */
static int
DictionaryCompare(
    char *left,
    char *right
    )
{
    Tcl_UniChar uniLeft, uniRight, uniLeftLower, uniRightLower;
    int diff, zeros;
    int secondaryDiff = 0;

    while (1) {
	if (isdigit(UCHAR(*right)) && isdigit(UCHAR(*left))) { /* INTL: digit */
	    /*
	     * There are decimal numbers embedded in the two
	     * strings.  Compare them as numbers, rather than
	     * strings.  If one number has more leading zeros than
	     * the other, the number with more leading zeros sorts
	     * later, but only as a secondary choice.
	     */

	    zeros = 0;
	    while ((*right == '0') && (isdigit(UCHAR(right[1])))) {
		right++;
		zeros--;
	    }
	    while ((*left == '0') && (isdigit(UCHAR(left[1])))) {
		left++;
		zeros++;
	    }
	    if (secondaryDiff == 0) {
		secondaryDiff = zeros;
	    }

	    /*
	     * The code below compares the numbers in the two
	     * strings without ever converting them to integers.  It
	     * does this by first comparing the lengths of the
	     * numbers and then comparing the digit values.
	     */

	    diff = 0;
	    while (1) {
		if (diff == 0) {
		    diff = UCHAR(*left) - UCHAR(*right);
		}
		right++;
		left++;
		if (!isdigit(UCHAR(*right))) {	/* INTL: digit */
		    if (isdigit(UCHAR(*left))) {	/* INTL: digit */
			return 1;
		    } else {
			/*
			 * The two numbers have the same length. See
			 * if their values are different.
			 */

			if (diff != 0) {
			    return diff;
			}
			break;
		    }
		} else if (!isdigit(UCHAR(*left))) {	/* INTL: digit */
		    return -1;
		}
	    }
	    continue;
	}

	/*
	 * Convert character to Unicode for comparison purposes.  If either
	 * string is at the terminating null, do a byte-wise comparison and
	 * bail out immediately.
	 */

	if ((*left != '\0') && (*right != '\0')) {
	    left += Tcl_UtfToUniChar(left, &uniLeft);
	    right += Tcl_UtfToUniChar(right, &uniRight);
	    /*
	     * Convert both chars to lower for the comparison, because
	     * dictionary sorts are case insensitve.  Covert to lower, not
	     * upper, so chars between Z and a will sort before A (where most
	     * other interesting punctuations occur)
	     */
	    uniLeftLower = Tcl_UniCharToLower(uniLeft);
	    uniRightLower = Tcl_UniCharToLower(uniRight);
	} else {
	    diff = UCHAR(*left) - UCHAR(*right);
	    break;
	}

	diff = uniLeftLower - uniRightLower;
	if (diff) {
	    return diff;
	} else if (secondaryDiff == 0) {
	    if (Tcl_UniCharIsUpper(uniLeft) &&
		    Tcl_UniCharIsLower(uniRight)) {
		secondaryDiff = -1;
	    } else if (Tcl_UniCharIsUpper(uniRight) &&
		    Tcl_UniCharIsLower(uniLeft)) {
		secondaryDiff = 1;
	    }
	}
    }
    if (diff == 0) {
	diff = secondaryDiff;
    }
    return diff;
}

static int
CompareAscii(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    char *left  = a->item1[n].string;
    char *right = b->item1[n].string;

    /* make sure to handle case where no string value has been set */
    if (left == NULL) {
	return ((right == NULL) ? 0 : (0 - UCHAR(*right)));
    } else if (right == NULL) {
	return UCHAR(*left);
    } else {
	return strcmp(left, right);
    }
}

static int
CompareDict(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    char *left  = a->item1[n].string;
    char *right = b->item1[n].string;

    /* make sure to handle case where no string value has been set */
    if (left == NULL) {
	return ((right == NULL) ? 0 : (0 - UCHAR(*right)));
    } else if (right == NULL) {
	return UCHAR(*left);
    } else {
	return DictionaryCompare(left, right);
    }
}

static int
CompareDouble(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    return (a->item1[n].doubleValue < b->item1[n].doubleValue) ? -1 :
	((a->item1[n].doubleValue == b->item1[n].doubleValue) ? 0 : 1);
}

static int
CompareLong(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    return (a->item1[n].longValue < b->item1[n].longValue) ? -1 :
	((a->item1[n].longValue == b->item1[n].longValue) ? 0 : 1);
}

static int
CompareCmd(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b,
    int n			/* Column index. */
    )
{
    Tcl_Interp *interp = sortData->tree->interp;
    Tcl_Obj **objv, *paramObjv[2];
    int objc, v;

    paramObjv[0] = a->obj;
    paramObjv[1] = b->obj;

    Tcl_ListObjLength(interp, sortData->columns[n].command, &objc);
    Tcl_ListObjReplace(interp, sortData->columns[n].command, objc - 2,
	    2, 2, paramObjv);
    Tcl_ListObjGetElements(interp, sortData->columns[n].command,
	    &objc, &objv);

    sortData->result = Tcl_EvalObjv(interp, objc, objv, 0);

    if (sortData->result != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (evaluating item sort -command)");
	return 0;
    }

    sortData->result = Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &v);
    if (sortData->result != TCL_OK) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
		"-command returned non-numeric result", -1);
	return 0;
    }

    return v;
}

static int
CompareProc(
    SortData *sortData,
    struct SortItem *a,
    struct SortItem *b
    )
{
    int i, v;

    if (a->item == b->item)
	return 0;

    for (i = 0; i < sortData->columnCount; i++) {
	v = (*sortData->columns[i].proc)(sortData, a, b, i);

	/* -command returned error */
	if (sortData->result != TCL_OK)
	    return 0;

	if (v != 0) {
	    if (i && (sortData->columns[i].order != sortData->columns[0].order))
		v *= -1;
	    return v;
	}
    }
    return 0;
}

/* BEGIN custom quicksort() */

static int
find_pivot(
    SortData *sortData,
    struct SortItem *left,
    struct SortItem *right,
    struct SortItem *pivot
    )
{
    struct SortItem *a, *b, *c, *p, *tmp;
    int v;

    a = left;
    b = (left + (right - left) / 2);
    c = right;

    /* Arrange a <= b <= c. */
    v = CompareProc(sortData, a, b);
    if (sortData->result != TCL_OK)
	return 0;
    if (v > 0) { tmp = a; a = b; b = tmp; }

    v = CompareProc(sortData, a, c);
    if (sortData->result != TCL_OK)
	return 0;
    if (v > 0) { tmp = a; a = c; c = tmp; }

    v = CompareProc(sortData, b, c);
    if (sortData->result != TCL_OK)
	return 0;
    if (v > 0) { tmp = b; b = c; c = tmp; }

    /* if (a < b) pivot = b */
    v = CompareProc(sortData, a, b);
    if (sortData->result != TCL_OK)
	return 0;
    if (v < 0) {
	(*pivot) = *b;
	return 1;
    }

    /* if (b < c) pivot = c */
    v = CompareProc(sortData, b, c);
    if (sortData->result != TCL_OK)
	return 0;
    if (v < 0) {
	(*pivot) = *c;
	return 1;
    }

    for (p = left + 1; p <= right; p++) {
	int v = CompareProc(sortData, p, left);
	if (sortData->result != TCL_OK)
	    return 0;
	if (v != 0) {
	    (*pivot) = (v < 0) ? *left : *p;
	    return 1;
	}
    }
    return 0;
}

/* If the user provides a -command which does not properly compare two
 * elements, quicksort may go into an infinite loop or access illegal memory.
 * This #define indicates parts of the code which are not part of a normal
 * quicksort, but are present to detect the aforementioned bugs. */
#define BUGGY_COMMAND

static struct SortItem *
partition(
    SortData *sortData,
    struct SortItem *left,
    struct SortItem *right,
    struct SortItem *pivot
    )
{
    int v;
#ifdef BUGGY_COMMAND
    struct SortItem *min = left, *max = right;
#endif

    while (left <= right) {
	/*
	  while (*left < *pivot)
	    ++left;
	*/
	while (1) {
	    v = CompareProc(sortData, left, pivot);
	    if (sortData->result != TCL_OK)
		return NULL;
	    if (v >= 0)
		break;
#ifdef BUGGY_COMMAND
	    /* If -command always returns < 0, 'left' becomes invalid */
	    if (left == max)
		goto buggy;
#endif
	    left++;
	}
	/*
	  while (*right >= *pivot)
	    --right;
	*/
	while (1) {
	    v = CompareProc(sortData, right, pivot);
	    if (sortData->result != TCL_OK)
		return NULL;
	    if (v < 0)
		break;
#ifdef BUGGY_COMMAND
	    /* If -command always returns >= 0, 'right' becomes invalid */
	    if (right == min)
		goto buggy;
#endif
	    right--;
	}
	if (left < right) {
	    struct SortItem tmp = *left;
	    *left = *right;
	    *right = tmp;
	    left++;
	    right--;
	}
    }
    return left;
#ifdef BUGGY_COMMAND
    buggy:
    FormatResult(sortData->tree->interp, "buggy item sort -command detected");
    sortData->result = TCL_ERROR;
    return NULL;
#endif
}

static void
quicksort(
    SortData *sortData,
    struct SortItem *left,
    struct SortItem *right
    )
{
    struct SortItem *p, pivot;

    if (sortData->result != TCL_OK)
	return;

    if (left == right)
	return;

    /* FIXME: switch to insertion sort or similar when the number of
     * elements is small. */

    if (find_pivot(sortData, left, right, &pivot) == 1) {
	p = partition(sortData, left, right, &pivot);
	quicksort(sortData, left, p - 1);
	quicksort(sortData, p, right);
    }
}

/* END custom quicksort() */

/*
 *----------------------------------------------------------------------
 *
 * ItemSortCmd --
 *
 *	This procedure is invoked to process the [item sort] widget
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ItemSortCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    TreeItem _item;
    Item *item, *first, *last, *walk, *lastChild;
    Column *column;
    int i, j, count, elemIndex, index, indexF = 0, indexL = 0;
    int sawColumn = FALSE, sawCmd = FALSE;
    static int (*sortProc[5])(SortData *, struct SortItem *, struct SortItem *, int) =
	{ CompareAscii, CompareDict, CompareDouble, CompareLong, CompareCmd };
    SortData sortData;
    TreeColumn treeColumn;
    struct SortElement *elemPtr;
    int notReally = FALSE;
    int result = TCL_OK;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 3, objv, "item ?option ...?");
	return TCL_ERROR;
    }

    if (TreeItem_FromObj(tree, objv[3], &_item, 0) != TCL_OK)
	return TCL_ERROR;
    item = (Item *) _item;

    /* If the item has no children, then nothing is done and no error
     * is generated. */
    if (item->numChildren < 1)
	return TCL_OK;

    /* Defaults: sort ascii strings in column 0 only */
    sortData.tree = tree;
    sortData.columnCount = 1;
    sortData.columns[0].column = 0;
    sortData.columns[0].sortBy = SORT_ASCII;
    sortData.columns[0].order = 1;
    sortData.columns[0].elemCount = 0;
    sortData.result = TCL_OK;

    first = item->firstChild;
    last = item->lastChild;

    for (i = 4; i < objc; ) {
	static CONST char *optionName[] = { "-ascii", "-column", "-command",
					    "-decreasing", "-dictionary", "-element", "-first", "-increasing",
					    "-integer", "-last", "-notreally", "-real", NULL };
	int numArgs[] = { 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 1, 1 };
	enum { OPT_ASCII, OPT_COLUMN, OPT_COMMAND, OPT_DECREASING, OPT_DICT,
	       OPT_ELEMENT, OPT_FIRST, OPT_INCREASING, OPT_INTEGER, OPT_LAST,
	       OPT_NOT_REALLY, OPT_REAL };

	if (Tcl_GetIndexFromObj(interp, objv[i], optionName, "option", 0,
		    &index) != TCL_OK)
	    return TCL_ERROR;
	if (objc - i < numArgs[index]) {
	    FormatResult(interp, "missing value for \"%s\" option",
		    optionName[index]);
	    return TCL_ERROR;
	}
	switch (index) {
	    case OPT_ASCII:
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_ASCII;
		break;
	    case OPT_COLUMN:
		if (TreeColumn_FromObj(tree, objv[i + 1], &treeColumn,
			    CFO_NOT_ALL | CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		    return TCL_ERROR;
		/* The first -column we see is the first column we compare */
		if (sawColumn) {
		    if (sortData.columnCount + 1 > MAX_SORT_COLUMNS) {
			FormatResult(interp,
				"can't compare more than %d columns",
				MAX_SORT_COLUMNS);
			return TCL_ERROR;
		    }
		    sortData.columnCount++;
		    /* Defaults for this column */
		    sortData.columns[sortData.columnCount - 1].sortBy = SORT_ASCII;
		    sortData.columns[sortData.columnCount - 1].order = 1;
		    sortData.columns[sortData.columnCount - 1].elemCount = 0;
		}
		sortData.columns[sortData.columnCount - 1].column = TreeColumn_Index(treeColumn);
		sawColumn = TRUE;
		break;
	    case OPT_COMMAND:
		sortData.columns[sortData.columnCount - 1].command = objv[i + 1];
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_COMMAND;
		sawCmd = TRUE;
		break;
	    case OPT_DECREASING:
		sortData.columns[sortData.columnCount - 1].order = 0;
		break;
	    case OPT_DICT:
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_DICT;
		break;
	    case OPT_ELEMENT:
	    {
		int listObjc;
		Tcl_Obj **listObjv;

		if (Tcl_ListObjGetElements(interp, objv[i + 1], &listObjc,
			    &listObjv) != TCL_OK)
		    return TCL_ERROR;
		elemPtr = sortData.columns[sortData.columnCount - 1].elems;
		sortData.columns[sortData.columnCount - 1].elemCount = 0;
		if (listObjc == 0) {
		} else if (listObjc == 1) {
		    if (TreeElement_FromObj(tree, listObjv[0], &elemPtr->elem)
			    != TCL_OK) {
			Tcl_AddErrorInfo(interp,
				"\n    (processing -element option)");
			return TCL_ERROR;
		    }
		    if (!TreeElement_IsType(tree, elemPtr->elem, "text")) {
			FormatResult(interp,
				"element %s is not of type \"text\"",
				Tcl_GetString(listObjv[0]));
			Tcl_AddErrorInfo(interp,
				"\n    (processing -element option)");
			return TCL_ERROR;
		    }
		    elemPtr->style = NULL;
		    elemPtr->elemIndex = -1;
		    sortData.columns[sortData.columnCount - 1].elemCount++;
		} else {
		    if (listObjc & 1) {
			FormatResult(interp,
				"list must have even number of elements");
			Tcl_AddErrorInfo(interp,
				"\n    (processing -element option)");
			return TCL_ERROR;
		    }
		    for (j = 0; j < listObjc; j += 2) {
			if ((TreeStyle_FromObj(tree, listObjv[j],
				     &elemPtr->style) != TCL_OK) ||
				(TreeElement_FromObj(tree, listObjv[j + 1],
					&elemPtr->elem) != TCL_OK) ||
				(TreeStyle_FindElement(tree, elemPtr->style,
					elemPtr->elem, &elemPtr->elemIndex) != TCL_OK)) {
			    Tcl_AddErrorInfo(interp,
				    "\n    (processing -element option)");
			    return TCL_ERROR;
			}
			if (!TreeElement_IsType(tree, elemPtr->elem, "text")) {
			    FormatResult(interp,
				    "element %s is not of type \"text\"",
				    Tcl_GetString(listObjv[j + 1]));
			    Tcl_AddErrorInfo(interp,
				    "\n    (processing -element option)");
			    return TCL_ERROR;
			}
			sortData.columns[sortData.columnCount - 1].elemCount++;
			elemPtr++;
		    }
		}
		break;
	    }
	    case OPT_FIRST:
		if (TreeItem_FromObj(tree, objv[i + 1], &_item, 0) != TCL_OK)
		    return TCL_ERROR;
		first = (Item *) _item;
		if (first->parent != item) {
		    FormatResult(interp,
			    "item %s%d is not a child of item %s%d",
			    tree->itemPrefix, first->id, tree->itemPrefix, item->id);
		    return TCL_ERROR;
		}
		break;
	    case OPT_INCREASING:
		sortData.columns[sortData.columnCount - 1].order = 1;
		break;
	    case OPT_INTEGER:
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_LONG;
		break;
	    case OPT_LAST:
		if (TreeItem_FromObj(tree, objv[i + 1], &_item, 0) != TCL_OK)
		    return TCL_ERROR;
		last = (Item *) _item;
		if (last->parent != item) {
		    FormatResult(interp,
			    "item %s%d is not a child of item %s%d",
			    tree->itemPrefix, last->id, tree->itemPrefix, item->id);
		    return TCL_ERROR;
		}
		break;
	    case OPT_NOT_REALLY:
		notReally = TRUE;
		break;
	    case OPT_REAL:
		sortData.columns[sortData.columnCount - 1].sortBy = SORT_DOUBLE;
		break;
	}
	i += numArgs[index];
    }

    /* If there are no columns, we cannot perform a sort unless -command
     * is specified. */
    if ((tree->columnCount < 1) && (sortData.columns[0].sortBy != SORT_COMMAND)) {
	FormatResult(interp, "there are no columns");
	return TCL_ERROR;
    }

    /* If there is only one item to sort, then return early. */
    if (first == last) {
	if (notReally)
	    Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) first));
	return TCL_OK;
    }

    for (i = 0; i < sortData.columnCount; i++) {

	/* Initialize the sort procedure for this column. */
	sortData.columns[i].proc = sortProc[sortData.columns[i].sortBy];

	/* Append two dummy args to the -command argument. These two dummy
	 * args are replaced by the 2 item ids being compared. See
	 * CompareCmd(). */
	if (sortData.columns[i].sortBy == SORT_COMMAND) {
	    Tcl_Obj *obj = Tcl_DuplicateObj(sortData.columns[i].command);
	    Tcl_Obj *obj2 = Tcl_NewObj();
	    Tcl_IncrRefCount(obj);
	    if (Tcl_ListObjAppendElement(interp, obj, obj2) != TCL_OK) {
		Tcl_DecrRefCount(obj);
		Tcl_IncrRefCount(obj2);
		Tcl_DecrRefCount(obj2);

		for (j = 0; j < i; j++) {
		    if (sortData.columns[j].sortBy == SORT_COMMAND) {
			Tcl_DecrRefCount(sortData.columns[j].command);
		    }
		}

		return TCL_ERROR;
	    }
	    (void) Tcl_ListObjAppendElement(interp, obj, obj2);
	    sortData.columns[i].command = obj;
	}
    }

    index = 0;
    walk = item->firstChild;
    while (walk != NULL) {
	if (walk == first)
	    indexF = index;
	if (walk == last)
	    indexL = index;
	index++;
	walk = walk->nextSibling;
    }
    if (indexF > indexL) {
	walk = last;
	last = first;
	first = walk;

	index = indexL;
	indexL = indexF;
	indexF = index;
    }
    count = indexL - indexF + 1;

    sortData.item1s = (struct SortItem1 *) ckalloc(sizeof(struct SortItem1) * count * sortData.columnCount);
    sortData.items = (struct SortItem *) ckalloc(sizeof(struct SortItem) * count);
    for (i = 0; i < count; i++) {
	sortData.items[i].item1 = sortData.item1s + i * sortData.columnCount;
	sortData.items[i].obj = NULL;
    }

    index = 0;
    walk = first;
    while (walk != last->nextSibling) {
	struct SortItem *sortItem = &sortData.items[index];

	sortItem->item = walk;
	if (sawCmd) {
	    Tcl_Obj *obj = TreeItem_ToObj(tree, (TreeItem) walk);
	    Tcl_IncrRefCount(obj);
	    sortData.items[index].obj = obj;
	}
	for (i = 0; i < sortData.columnCount; i++) {
	    struct SortItem1 *sortItem1 = sortItem->item1 + i;

	    if (sortData.columns[i].sortBy == SORT_COMMAND)
		continue;

	    column = Item_FindColumn(tree, walk, sortData.columns[i].column);
	    if ((column == NULL) || (column->style == NULL)) {
		NoStyleMsg(tree, walk, sortData.columns[i].column);
		result = TCL_ERROR;
		goto done;
	    }

	    /* -element was empty. Find the first text element in the style */
	    if (sortData.columns[i].elemCount == 0)
		elemIndex = -1;

	    /* -element was element name. Find the element in the style */
	    else if ((sortData.columns[i].elemCount == 1) &&
		    (sortData.columns[i].elems[0].style == NULL)) {
		if (TreeStyle_FindElement(tree, column->style,
			    sortData.columns[i].elems[0].elem, &elemIndex) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
	    }

	    /* -element was style/element pair list */
	    else {
		TreeStyle masterStyle = TreeStyle_GetMaster(tree, column->style);

		/* If the item style does not match any in the -element list,
		 * we will use the first text element in the item style. */
		elemIndex = -1;

		/* Match a style from the -element list. Look in reverse order
		 * to handle duplicates. */
		for (j = sortData.columns[i].elemCount - 1; j >= 0; j--) {
		    if (sortData.columns[i].elems[j].style == masterStyle) {
			elemIndex = sortData.columns[i].elems[j].elemIndex;
			break;
		    }
		}
	    }
	    if (TreeStyle_GetSortData(tree, column->style, elemIndex,
			sortData.columns[i].sortBy,
			&sortItem1->longValue,
			&sortItem1->doubleValue,
			&sortItem1->string) != TCL_OK) {
		result = TCL_ERROR;
		goto done;
	    }
	}
	index++;
	walk = walk->nextSibling;
    }

    quicksort(&sortData, sortData.items, sortData.items + count - 1);

    if (sortData.result != TCL_OK) {
	result = sortData.result;
	goto done;
    }

    if (sawCmd)
	Tcl_ResetResult(interp);

    if (notReally) {
	Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
	Tcl_Obj *itemObj;

	/* Smallest to largest */
	if (sortData.columns[0].order == 1) {
	    for (i = 0; i < count; i++) {
		itemObj = sortData.items[i].obj;
		if (itemObj == NULL)
		    itemObj = TreeItem_ToObj(tree,
			    (TreeItem) sortData.items[i].item);
		Tcl_ListObjAppendElement(interp, listObj, itemObj);
	    }
	}

	/* Largest to smallest */
	else {
	    for (i = count - 1; i >= 0; i--) {
		itemObj = sortData.items[i].obj;
		if (itemObj == NULL)
		    itemObj = TreeItem_ToObj(tree,
			    (TreeItem) sortData.items[i].item);
		Tcl_ListObjAppendElement(interp, listObj, itemObj);
	    }
	}

	Tcl_SetObjResult(interp, listObj);
	goto done;
    }

    first = first->prevSibling;
    last = last->nextSibling;

    /* Smallest to largest */
    if (sortData.columns[0].order == 1) {
	for (i = 0; i < count - 1; i++) {
	    sortData.items[i].item->nextSibling = sortData.items[i + 1].item;
	    sortData.items[i + 1].item->prevSibling = sortData.items[i].item;
	}
	indexF = 0;
	indexL = count - 1;
    }

    /* Largest to smallest */
    else {
	for (i = count - 1; i > 0; i--) {
	    sortData.items[i].item->nextSibling = sortData.items[i - 1].item;
	    sortData.items[i - 1].item->prevSibling = sortData.items[i].item;
	}
	indexF = count - 1;
	indexL = 0;
    }

    lastChild = item->lastChild;

    sortData.items[indexF].item->prevSibling = first;
    if (first)
	first->nextSibling = sortData.items[indexF].item;
    else
	item->firstChild = sortData.items[indexF].item;

    sortData.items[indexL].item->nextSibling = last;
    if (last)
	last->prevSibling = sortData.items[indexL].item;
    else
	item->lastChild = sortData.items[indexL].item;

    /* Redraw the lines of the old/new lastchild */
    if ((item->lastChild != lastChild) && tree->showLines) {
	if (lastChild->dInfo != NULL)
	    Tree_InvalidateItemDInfo(tree, (TreeItem) lastChild,
		    (TreeItem) NULL);
	if (item->lastChild->dInfo != NULL)
	    Tree_InvalidateItemDInfo(tree, (TreeItem) item->lastChild,
		    (TreeItem) NULL);
    }

    tree->updateIndex = 1;
    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    done:
    for (i = 0; i < count; i++) {
	if (sortData.items[i].obj != NULL) {
	    Tcl_DecrRefCount(sortData.items[i].obj);
	}
    }
    for (i = 0; i < sortData.columnCount; i++) {
	if (sortData.columns[i].sortBy == SORT_COMMAND) {
	    Tcl_DecrRefCount(sortData.columns[i].command);
	}
    }
    ckfree((char *) sortData.item1s);
    ckfree((char *) sortData.items);

    if (tree->debug.enable && tree->debug.data) {
	Tree_Debug(tree);
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemStateCmd --
 *
 *	This procedure is invoked to process the [item state] widget
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ItemStateCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = {
	"forcolumn", "get", "set", (char *) NULL
    };
    enum {
	COMMAND_FORCOLUMN, COMMAND_GET, COMMAND_SET
    };
    int index;
    TreeItem _item;
    Item *item;

    if (objc < 5) {
	Tcl_WrongNumArgs(interp, 3, objv, "command item ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	/* T item state forcolumn I C ?stateList? */
	case COMMAND_FORCOLUMN:
	{
	    Tcl_Obj *listObj;
	    Column *column;
	    int columnIndex;
	    int i, states[3], stateOn, stateOff;
	    int listObjc;
	    Tcl_Obj **listObjv;

	    if (objc < 6 || objc > 7) {
		Tcl_WrongNumArgs(interp, 5, objv, "column ?stateList?");
		return TCL_ERROR;
	    }
	    if (TreeItem_FromObj(tree, objv[4], &_item, 0)
		    != TCL_OK)
		return TCL_ERROR;
	    item = (Item *) _item;
	    if (Item_FindColumnFromObj(tree, item, objv[5], &column,
			&columnIndex) != TCL_OK)
		return TCL_ERROR;
	    if (objc == 6) {
		if ((column == NULL) || !column->cstate)
		    break;
		listObj = Tcl_NewListObj(0, NULL);
		for (i = 0; i < 32; i++) {
		    if (tree->stateNames[i] == NULL)
			continue;
		    if (column->cstate & (1L << i)) {
			Tcl_ListObjAppendElement(interp, listObj,
				Tcl_NewStringObj(tree->stateNames[i], -1));
		    }
		}
		Tcl_SetObjResult(interp, listObj);
		break;
	    }
	    states[0] = states[1] = states[2] = 0;
	    if (Tcl_ListObjGetElements(interp, objv[6], &listObjc, &listObjv)
		    != TCL_OK)
		return TCL_ERROR;
	    if (listObjc == 0)
		break;
	    for (i = 0; i < listObjc; i++) {
		if (Tree_StateFromObj(tree, listObjv[i], states, NULL,
			    SFO_NOT_STATIC) != TCL_OK)
		    return TCL_ERROR;
	    }
	    if (column == NULL) {
		column = Item_CreateColumn(tree, item, columnIndex, NULL);
	    }
	    stateOn = states[STATE_OP_ON];
	    stateOff = states[STATE_OP_OFF];
	    stateOn |= ~column->cstate & states[STATE_OP_TOGGLE];
	    stateOff |= column->cstate & states[STATE_OP_TOGGLE];
	    Column_ChangeState(tree, item, column, columnIndex,
		    stateOff, stateOn);
	    break;
	}

	/* T item state get I ?state? */
	case COMMAND_GET:
	{
	    Tcl_Obj *listObj;
	    int i, states[3];

	    if (objc > 6) {
		Tcl_WrongNumArgs(interp, 5, objv, "?state?");
		return TCL_ERROR;
	    }
	    if (TreeItem_FromObj(tree, objv[4], &_item, 0) != TCL_OK)
		return TCL_ERROR;
	    item = (Item *) _item;
	    if (objc == 6) {
		states[STATE_OP_ON] = 0;
		if (Tree_StateFromObj(tree, objv[5], states, NULL,
			    SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp,
			Tcl_NewBooleanObj((item->state & states[STATE_OP_ON]) != 0));
		break;
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    for (i = 0; i < 32; i++) {
		if (tree->stateNames[i] == NULL)
		    continue;
		if (item->state & (1L << i)) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    Tcl_NewStringObj(tree->stateNames[i], -1));
		}
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T item state set I ?I? {state ...} */
	case COMMAND_SET:
	{
	    TreeItem item, itemFirst, itemLast;
	    int i, states[3], stateOn, stateOff;
	    int listObjc;
	    Tcl_Obj **listObjv;

	    if (objc < 6 || objc > 7) {
		Tcl_WrongNumArgs(interp, 5, objv, "?last? stateList");
		return TCL_ERROR;
	    }
	    if (TreeItem_FromObj(tree, objv[4], &itemFirst, IFO_ALLOK) != TCL_OK)
		return TCL_ERROR;
	    if (objc == 6) {
		itemLast = itemFirst;
	    }
	    if (objc == 7) {
		if (TreeItem_FromObj(tree, objv[5], &itemLast, IFO_ALLOK) != TCL_OK)
		    return TCL_ERROR;
	    }
	    states[0] = states[1] = states[2] = 0;
	    if (Tcl_ListObjGetElements(interp, objv[objc - 1],
			&listObjc, &listObjv) != TCL_OK)
		return TCL_ERROR;
	    if (listObjc == 0)
		break;
	    for (i = 0; i < listObjc; i++) {
		if (Tree_StateFromObj(tree, listObjv[i], states, NULL,
			    SFO_NOT_STATIC) != TCL_OK)
		    return TCL_ERROR;
	    }
	    if ((itemFirst == ITEM_ALL) || (itemLast == ITEM_ALL)) {
		Tcl_HashEntry *hPtr;
		Tcl_HashSearch search;

		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashValue(hPtr);
		    stateOn = states[STATE_OP_ON];
		    stateOff = states[STATE_OP_OFF];
		    stateOn |= ~((Item *) item)->state & states[STATE_OP_TOGGLE];
		    stateOff |= ((Item *) item)->state & states[STATE_OP_TOGGLE];
		    TreeItem_ChangeState(tree, item, stateOff, stateOn);
		    hPtr = Tcl_NextHashEntry(&search);
		}
		break;
	    }
	    if (objc == 7) {
		int indexFirst, indexLast;

		if (TreeItem_RootAncestor(tree, itemFirst) !=
			TreeItem_RootAncestor(tree,itemLast)) {
		    FormatResult(interp,
			    "item %s%d and item %s%d don't share a common ancestor",
			    tree->itemPrefix, TreeItem_GetID(tree, itemFirst),
			    tree->itemPrefix, TreeItem_GetID(tree, itemLast));
		    return TCL_ERROR;
		}
		TreeItem_ToIndex(tree, itemFirst, &indexFirst, NULL);
		TreeItem_ToIndex(tree, itemLast, &indexLast, NULL);
		if (indexFirst > indexLast) {
		    item = itemFirst;
		    itemFirst = itemLast;
		    itemLast = item;
		}
	    }
	    item = itemFirst;
	    while (item != NULL) {
		stateOn = states[STATE_OP_ON];
		stateOff = states[STATE_OP_OFF];
		stateOn |= ~((Item *) item)->state & states[STATE_OP_TOGGLE];
		stateOff |= ((Item *) item)->state & states[STATE_OP_TOGGLE];
		TreeItem_ChangeState(tree, item, stateOff, stateOn);
		if (item == itemLast)
		    break;
		item = TreeItem_Next(tree, item);
	    }
	    break;
	}
    }

    return TCL_OK;
}

#ifdef SELECTION_VISIBLE

/*
 *----------------------------------------------------------------------
 *
 * Tree_DeselectHidden --
 *
 *	Removes any selected items which are no longer ReallyVisible()
 *	from the selection.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	<Selection> event may be generated.
 *
 *----------------------------------------------------------------------
 */

/*
 * FIXME: optimize all calls to this routine.
 * Optionally call Tree_DInfoChanged(tree, DINFO_REDO_SELECTION) instead.
 */
void
Tree_DeselectHidden(
    TreeCtrl *tree		/* Widget info. */
    )
{
    TreeItemList items;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    TreeItem item;
    int i;

    if (tree->selectCount < 1)
	return;

    if (tree->updateIndex)
	Tree_UpdateItemIndex(tree);

    TreeItemList_Init(tree, &items, tree->selectCount);

    hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
	if (!TreeItem_ReallyVisible(tree, item))
	    TreeItemList_Append(&items, item);
	hPtr = Tcl_NextHashEntry(&search);
    }
    for (i = 0; i < TreeItemList_Count(&items); i++)
	Tree_RemoveFromSelection(tree, TreeItemList_ItemN(&items, i));
    if (TreeItemList_Count(&items)) {
	TreeNotify_Selection(tree, NULL, &items);
    }
    TreeItemList_Free(&items);
}

#endif /* SELECTION_VISIBLE */

/*
 *----------------------------------------------------------------------
 *
 * TreeItemCmd --
 *
 *	This procedure is invoked to process the [item] widget
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
TreeItemCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = {
	"ancestors",
	"children",
	"create",
	"delete",
	"firstchild",
	"lastchild",
	"nextsibling",
	"numchildren",
	"parent",
	"prevsibling",
	"remove",

	"bbox",
	"cget",
	"collapse",
	"compare",
	"complex",
	"configure",
	"count",
	"dump",
	"element",
	"enabled",
	"expand",
	"id",
	"image",
	"isancestor",
	"isopen",
	"order",
	"range",
	"rnc",
	"sort",
#ifdef COLUMN_SPAN
	"span",
#endif
	"state",
	"style",
	"text",
	"toggle",
	(char *) NULL
    };
    enum {
	COMMAND_ANCESTORS,
	COMMAND_CHILDREN,
	COMMAND_CREATE,
	COMMAND_DELETE,
	COMMAND_FIRSTCHILD,
	COMMAND_LASTCHILD,
	COMMAND_NEXTSIBLING,
	COMMAND_NUMCHILDREN,
	COMMAND_PARENT,
	COMMAND_PREVSIBLING,
	COMMAND_REMOVE,

	COMMAND_BBOX,
	COMMAND_CGET,
	COMMAND_COLLAPSE,
	COMMAND_COMPARE,
	COMMAND_COMPLEX,
	COMMAND_CONFIGURE,
	COMMAND_COUNT,
	COMMAND_DUMP,
	COMMAND_ELEMENT,
	COMMAND_ENABLED,
	COMMAND_EXPAND,
	COMMAND_ID,
	COMMAND_IMAGE,
	COMMAND_ISANCESTOR,
	COMMAND_ISOPEN,
	COMMAND_ORDER,
	COMMAND_RANGE,
	COMMAND_RNC,
	COMMAND_SORT,
#ifdef COLUMN_SPAN
	COMMAND_SPAN,
#endif
	COMMAND_STATE,
	COMMAND_STYLE,
	COMMAND_TEXT,
	COMMAND_TOGGLE
    };
#define AF_NOTANCESTOR	0x00010000 /* item can't be ancestor of other item */
#define AF_NOT_EQUAL	0x00020000 /* second item can't be same as first */
#define AF_SAMEROOT	0x00040000 /* both items must be descendants of a common ancestor */
#define AF_NOT_ITEM	0x00080000 /* arg is not an Item */
    struct {
	int minArgs;
	int maxArgs;
	int flags;	/* AF_xxx | IFO_xxx for 1st arg. */
	int flags2;	/* AF_xxx | IFO_xxx for 2nd arg. */
	int flags3;	/* AF_xxx | IFO_xxx for 3rd arg. */
	char *argString;
	Tcl_ObjCmdProc *proc;
    } argInfo[] = {
	{ 1, 1, 0, 0, 0, "item", NULL }, /* ancestors */
	{ 1, 1, 0, 0, 0, "item", NULL }, /* children */
	{ 0, 0, 0, 0, 0, NULL, ItemCreateCmd }, /* create */
	{ 1, 2, IFO_ALLOK, IFO_ALLOK | AF_SAMEROOT, 0, "first ?last?", NULL }, /* delete */
	{ 1, 2, 0, IFO_NOTROOT | AF_NOTANCESTOR | AF_NOT_EQUAL, 0, "item ?newFirstChild?", NULL }, /* firstchild */
	{ 1, 2, 0, IFO_NOTROOT | AF_NOTANCESTOR | AF_NOT_EQUAL, 0, "item ?newLastChild?", NULL }, /* lastchild */
	{ 1, 2, IFO_NOTROOT | IFO_NOTORPHAN, IFO_NOTROOT | AF_NOTANCESTOR | AF_NOT_EQUAL, 0, "item ?newNextSibling?", NULL }, /* nextsibling */
	{ 1, 1, 0, 0, 0, "item", NULL }, /* numchildren */
	{ 1, 1, 0, 0, 0, "item", NULL }, /* parent */
	{ 1, 2, IFO_NOTROOT | IFO_NOTORPHAN, IFO_NOTROOT | AF_NOTANCESTOR | AF_NOT_EQUAL, 0, "item ?newPrevSibling?", NULL }, /* prevsibling */
	{ 1, 1, IFO_NOTROOT, 0, 0, "item", NULL }, /* remove */
    
	{ 1, 3, 0, AF_NOT_ITEM, AF_NOT_ITEM, "item ?column? ?element?", NULL }, /* bbox */
	{ 2, 2, 0, AF_NOT_ITEM, 0, "item option", NULL }, /* cget */
	{ 1, 2, IFO_ALLOK, AF_NOT_ITEM, 0, "item ?-recurse?", NULL}, /* collapse */
	{ 3, 3, 0, AF_NOT_ITEM, AF_SAMEROOT, "item1 op item2", NULL }, /* compare */
	{ 2, 100000, 0, AF_NOT_ITEM, AF_NOT_ITEM, "item list ...", NULL }, /* complex */
	{ 1, 100000, 0, AF_NOT_ITEM, AF_NOT_ITEM, "item ?option? ?value? ?option value ...?", NULL }, /* configure */
	{ 0, 1, AF_NOT_ITEM, 0, 0, "?-visible?" , NULL}, /* count */
	{ 1, 1, 0, 0, 0, "item", NULL }, /* dump */
	{ 0, 0, 0, 0, 0, NULL, ItemElementCmd }, /* element */
	{ 1, 2, 0, AF_NOT_ITEM, 0, "item ?boolean?", NULL }, /* enabled */
	{ 1, 2, IFO_ALLOK, AF_NOT_ITEM, 0, "item ?-recurse?", NULL}, /* expand */
	{ 1, 1, IFO_NULLOK, 0, 0, "item", NULL }, /* id */
	{ 1, 100000, 0, AF_NOT_ITEM, AF_NOT_ITEM, "item ?column? ?image? ?column image ...?", NULL }, /* image */
	{ 2, 2, 0, 0, 0, "item item2", NULL }, /* isancestor */
	{ 1, 1, 0, 0, 0, "item", NULL }, /* isopen */
	{ 1, 2, 0, AF_NOT_ITEM, 0, "item ?-visible?", NULL }, /* order */
	{ 2, 2, 0, AF_SAMEROOT, 0, "first last", NULL }, /* range */
	{ 1, 1, 0, 0, 0, "item", NULL }, /* rnc */
	{ 0, 0, 0, 0, 0, NULL, ItemSortCmd }, /* sort */
#ifdef COLUMN_SPAN
	{ 1, 100000, 0, AF_NOT_ITEM, AF_NOT_ITEM, "item ?column? ?span? ?column span ...?", NULL }, /* span */
#endif
	{ 0, 0, 0, 0, 0, NULL, ItemStateCmd }, /* state */
	{ 0, 0, 0, 0, 0, NULL, ItemStyleCmd }, /* style */
	{ 1, 100000, 0, AF_NOT_ITEM, AF_NOT_ITEM, "item ?column? ?text? ?column text ...?", NULL }, /* text */
	{ 1, 2, IFO_ALLOK, AF_NOT_ITEM, 0, "item ?-recurse?", NULL}, /* toggle */
    };
    int index;
    int numArgs = objc - 3;
    TreeItemList itemList, item2List;
    TreeItem _item = NULL, _item2 = NULL;
    Item *item = NULL, *item2 = NULL, *child;
    int result = TCL_OK;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
		&index) != TCL_OK) {
	return TCL_ERROR;
    }

    if (argInfo[index].proc != NULL)
	return argInfo[index].proc(clientData, interp, objc, objv);

    if ((numArgs < argInfo[index].minArgs) ||
	    (numArgs > argInfo[index].maxArgs)) {
	Tcl_WrongNumArgs(interp, 3, objv, argInfo[index].argString);
	return TCL_ERROR;
    }

    TreeItemList_Init(tree, &itemList, 0);
    TreeItemList_Init(tree, &item2List, 0);

    if ((numArgs >= 1) && !(argInfo[index].flags & AF_NOT_ITEM)) {
	if (TreeItemList_FromObj(tree, objv[3], &itemList,
		    argInfo[index].flags & 0xFFFF) != TCL_OK) {
	    goto errorExit;
	}
	_item = TreeItemList_ItemN(&itemList, 0); /* May be NULL. */
	item = (Item *) _item;
    }
    if (((numArgs >= 2) && !(argInfo[index].flags2 & AF_NOT_ITEM)) ||
	    ((numArgs >= 3) && !(argInfo[index].flags3 & AF_NOT_ITEM))) {
	int flags, obji;
	if (argInfo[index].flags2 & AF_NOT_ITEM) {
	    obji = 5;
	    flags = argInfo[index].flags3;
	} else {
	    obji = 4;
	    flags = argInfo[index].flags2;
	}
	if (TreeItemList_FromObj(tree, objv[obji], &item2List,
		flags & 0xFFFF) != TCL_OK) {
	    goto errorExit;
	}
	_item2 = TreeItemList_ItemN(&item2List, 0); /* May be NULL. */
	item2 = (Item *) _item2;
	if ((flags & AF_NOT_EQUAL) && (item == item2)) {
	    FormatResult(interp, "item %s%d same as second item", tree->itemPrefix,
		    item->id);
	    goto errorExit;
	}
	if ((argInfo[index].flags & AF_NOTANCESTOR) &&
		TreeItem_IsAncestor(tree, (TreeItem) item, (TreeItem) item2)) {
	    FormatResult(interp, "item %s%d is ancestor of item %s%d",
		    tree->itemPrefix, item->id, tree->itemPrefix, item2->id);
	    goto errorExit;
	}
	if ((flags & AF_NOTANCESTOR) &&
		TreeItem_IsAncestor(tree, (TreeItem) item2, (TreeItem) item)) {
	    FormatResult(interp, "item %s%d is ancestor of item %s%d",
		    tree->itemPrefix, item2->id, tree->itemPrefix, item->id);
	    goto errorExit;
	}
	if ((flags & AF_SAMEROOT) &&
		TreeItem_RootAncestor(tree, (TreeItem) item) !=
		TreeItem_RootAncestor(tree, (TreeItem) item2)) {
	    FormatResult(interp,
		    "item %s%d and item %s%d don't share a common ancestor",
		    tree->itemPrefix, item->id, tree->itemPrefix, item2->id);
	    goto errorExit;
	}
    }

    switch (index) {
	case COMMAND_ANCESTORS:
	{
	    Tcl_Obj *listObj;
	    Item *parent = item->parent;

	    listObj = Tcl_NewListObj(0, NULL);
	    while (parent != NULL) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, (TreeItem) parent));
		parent = parent->parent;
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
	/* T item bbox I ?C? ?E? */
	case COMMAND_BBOX:
	{
	    int x, y, w, h;
	    int i, columnIndex, indent, totalWidth;
	    TreeColumn treeColumn;
	    Column *itemColumn;
	    StyleDrawArgs drawArgs;
	    TreeItem item_ = (TreeItem) item;
	    XRectangle rect;

	    if (Tree_ItemBbox(tree, item_, &x, &y, &w, &h) < 0)
		break;
	    if (objc > 4) {
		if (Item_FindColumnFromObj(tree, item, objv[4],
			    &itemColumn, &columnIndex) != TCL_OK)
		    goto errorExit;
		totalWidth = 0;
		treeColumn = tree->columns;
		for (i = 0; i < columnIndex; i++) {
		    totalWidth += TreeColumn_UseWidth(treeColumn);
		    treeColumn = TreeColumn_Next(treeColumn);
		}
		if (treeColumn == tree->columnTree)
		    indent = TreeItem_Indent(tree, item_);
		else
		    indent = 0;
		if (objc == 5) {
		    FormatResult(interp, "%d %d %d %d",
			    x + totalWidth + indent - tree->xOrigin,
			    y - tree->yOrigin,
			    x + totalWidth + TreeColumn_UseWidth(treeColumn) - tree->xOrigin,
			    y + h - tree->yOrigin);
		    break;
		}
		if ((itemColumn == NULL) || (itemColumn->style == NULL)) {
		    NoStyleMsg(tree, item, columnIndex);
		    goto errorExit;
		}
		drawArgs.style = itemColumn->style;
		drawArgs.tree = tree;
		drawArgs.drawable = None;
		drawArgs.state = TreeItem_GetState(tree, item_);
		drawArgs.indent = indent;
		drawArgs.x = x + totalWidth;
		drawArgs.y = y;
		drawArgs.width = TreeColumn_UseWidth(treeColumn);
		drawArgs.height = h;
		drawArgs.justify = TreeColumn_Justify(treeColumn);
		if (TreeStyle_GetElemRects(&drawArgs, objc - 5, objv + 5,
			    &rect) == -1)
		    goto errorExit;
		x = rect.x;
		y = rect.y;
		w = rect.width;
		h = rect.height;
	    }
	    FormatResult(interp, "%d %d %d %d",
		    x - tree->xOrigin,
		    y - tree->yOrigin,
		    x - tree->xOrigin + w,
		    y - tree->yOrigin + h);
	    break;
	}
	case COMMAND_CGET:
	{
	    Tcl_Obj *resultObjPtr;

	    resultObjPtr = Tk_GetOptionValue(interp, (char *) item,
		    tree->itemOptionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		goto errorExit;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}
	case COMMAND_CHILDREN:
	{
	    if (item->numChildren != 0) {
		Tcl_Obj *listObj;

		listObj = Tcl_NewListObj(0, NULL);
		child = item->firstChild;
		while (child != NULL) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    TreeItem_ToObj(tree, (TreeItem) child));
		    child = child->nextSibling;
		}
		Tcl_SetObjResult(interp, listObj);
	    }
	    break;
	}
	case COMMAND_COLLAPSE:
	case COMMAND_EXPAND:
	case COMMAND_TOGGLE:
	{
	    int recurse = 0;
	    int mode = 0; /* lint */
	    int i, count;

	    if (numArgs == 2) {
		char *s = Tcl_GetString(objv[4]);
		if (strcmp(s, "-recurse")) {
		    FormatResult(interp, "bad option \"%s\": must be -recurse",
			    s);
		    goto errorExit;
		}
		recurse = 1;
	    }
	    switch (index) {
		case COMMAND_COLLAPSE:
		    mode = 0;
		    break;
		case COMMAND_EXPAND:
		    mode = 1;
		    break;
		case COMMAND_TOGGLE:
		    mode = -1;
		    break;
	    }
	    if (item == (Item *) ITEM_ALL) {
		Tcl_HashEntry *hPtr;
		Tcl_HashSearch search;
		TreeItemList items;

		TreeItemList_Init(tree, &items, tree->itemCount);
		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    _item = (TreeItem) Tcl_GetHashValue(hPtr);
		    TreeItemList_Append(&items, _item);
		    hPtr = Tcl_NextHashEntry(&search);
		}
		count = TreeItemList_Count(&items);
		for (i = 0; i < count; i++) {
		    _item = TreeItemList_ItemN(&items, i);
		    TreeItem_OpenClose(tree, _item, mode, FALSE);
		}
		TreeItemList_Free(&items);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
		break;
	    }
	    if (recurse) {
		count = TreeItemList_Count(&itemList);
		for (i = 0; i < count; i++) {
		    _item = TreeItemList_ItemN(&itemList, i);
		    TreeItem_ListDescendants(tree, _item, &itemList);
		}
	    }
	    count = TreeItemList_Count(&itemList);
	    for (i = 0; i < count; i++) {
		_item = TreeItemList_ItemN(&itemList, i);
		TreeItem_OpenClose(tree, _item, mode, FALSE);
	    }
#ifdef SELECTION_VISIBLE
	    Tree_DeselectHidden(tree);
#endif
	    break;
	}
	/* T item compare I op I */
	case COMMAND_COMPARE:
	{
	    static CONST char *opName[] = { "<", "<=", "==", ">=", ">", "!=", NULL };
	    int op, compare = 0, index1, index2;

	    if (Tcl_GetIndexFromObj(interp, objv[4], opName, "comparison operator", 0,
		    &op) != TCL_OK)
		goto errorExit;
	    TreeItem_ToIndex(tree, _item, &index1, NULL);
	    TreeItem_ToIndex(tree, _item2, &index2, NULL);
	    switch (op) {
		case 0: compare = index1 < index2; break;
		case 1: compare = index1 <= index2; break;
		case 2: compare = index1 == index2; break;
		case 3: compare = index1 >= index2; break;
		case 4: compare = index1 > index2; break;
		case 5: compare = index1 != index2; break;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(compare));
	    break;
	}
	case COMMAND_COMPLEX:
	{
	    int i, j, columnIndex;
	    int objc1, objc2;
	    Tcl_Obj **objv1, **objv2;
	    TreeColumn treeColumn = tree->columns;
	    Column *column;
	    int eMask, cMask, iMask = 0;

	    if (objc <= 4)
		break;
	    columnIndex = 0;
	    for (i = 4; i < objc; i++, columnIndex++,
		    treeColumn = TreeColumn_Next(treeColumn)) {
		if (treeColumn == NULL) {
		    FormatResult(interp, "column #%d doesn't exist",
			    columnIndex);
		    result = TCL_ERROR;
		    goto doneComplex;
		}
		column = Item_FindColumn(tree, item, columnIndex);
		if (column == NULL) {
		    FormatResult(interp, "item %s%d doesn't have column %s%d",
			    tree->itemPrefix, item->id,
			    tree->columnPrefix, TreeColumn_GetID(treeColumn));
		    result = TCL_ERROR;
		    goto doneComplex;
		}
		/* List of element-configs per column */
		if (Tcl_ListObjGetElements(interp, objv[i],
			    &objc1, &objv1) != TCL_OK) {
		    result = TCL_ERROR;
		    goto doneComplex;
		}
		if (objc1 == 0)
		    continue;
		if (column->style == NULL) {
		    FormatResult(interp, "item %s%d column %s%d has no style",
			    tree->itemPrefix, item->id,
			    tree->columnPrefix, TreeColumn_GetID(treeColumn));
		    result = TCL_ERROR;
		    goto doneComplex;
		}
		cMask = 0;
		for (j = 0; j < objc1; j++) {
		    /* elem option value... */
		    if (Tcl_ListObjGetElements(interp, objv1[j],
				&objc2, &objv2) != TCL_OK) {
			result = TCL_ERROR;
			goto doneComplex;
		    }
		    if (objc2 < 3) {
			FormatResult(interp,
				"wrong # args: should be \"element option value ...\"");
			result = TCL_ERROR;
			goto doneComplex;
		    }
		    if (TreeStyle_ElementConfigure(tree, (TreeItem) item,
				(TreeItemColumn) column, column->style,
				objv2[0], objc2 - 1, objv2 + 1, &eMask) != TCL_OK) {
			result = TCL_ERROR;
			goto doneComplex;
		    }
		    cMask |= eMask;
		    iMask |= eMask;
		}
		if (cMask & CS_LAYOUT)
		    TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
	    }
	    doneComplex:
	    if (iMask & CS_DISPLAY)
		Tree_InvalidateItemDInfo(tree, (TreeItem) item, NULL);
	    if (iMask & CS_LAYOUT) {
		Tree_InvalidateColumnWidth(tree, -1);
		TreeItem_InvalidateHeight(tree, (TreeItem) item);
		Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    }
	    break;
	}
	case COMMAND_CONFIGURE:
	{
	    int i, count;
	    if (objc <= 5) {
		Tcl_Obj *resultObjPtr;

		resultObjPtr = Tk_GetOptionInfo(interp, (char *) item,
			tree->itemOptionTable,
			(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    goto errorExit;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    /* FIXME: ITEM_ALL */
	    count = TreeItemList_Count(&itemList);
	    for (i = 0; i < count; i++) {
		item = (Item *) TreeItemList_ItemN(&itemList, i);
		result = Item_Configure(tree, item, objc - 4, objv + 4);
		if (result != TCL_OK)
		    break;
	    }
	    break;
	}
	case COMMAND_COUNT:
	{
	    int visible = FALSE;
	    if (objc == 4) {
		int len;
		char *s = Tcl_GetStringFromObj(objv[3], &len);
		if ((s[0] == '-') && (strncmp(s, "-visible", len) == 0))
		    visible = TRUE;
		else {
		    FormatResult(interp, "bad switch \"%s\": must be -visible",
			s);
		    goto errorExit;
		}
	    }
	    if (visible) {
		if (tree->updateIndex)
		    Tree_UpdateItemIndex(tree);
		Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->itemVisCount));
	    } else {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->itemCount));
	    }
	    break;
	}
	case COMMAND_DELETE:
	{
	    TreeItemList deleted, selected;
	    int i, count;

	    /* The root is never deleted */
	    if (tree->itemCount == 1)
		break;

	    if (objc == 5) {
		if (item == NULL || item2 == NULL) {
		    FormatResult(interp, "invalid range \"%s\" to \"%s\"",
			    Tcl_GetString(objv[3]), Tcl_GetString(objv[4]));
		    goto errorExit;
		}
	    }

	    /*
	     * ITEM_FLAG_DELETED prevents us from adding the same item
	     * twice to the 'deleted' list. It also prevents recursive
	     * calls to this command (through binding scripts) deleting
	     * the same item twice.
	     */

	    if ((_item == ITEM_ALL) || (_item2 == ITEM_ALL)) {
		Tcl_HashEntry *hPtr;
		Tcl_HashSearch search;

		TreeItemList_Init(tree, &deleted, tree->itemCount - 1);
		TreeItemList_Init(tree, &selected, tree->selectCount);

		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    item = (Item *) Tcl_GetHashValue(hPtr);
		    if (!ISROOT(item) && !IS_DELETED(item)) {
			item->flags |= ITEM_FLAG_DELETED;
			TreeItemList_Append(&deleted, (TreeItem) item);
			if (TreeItem_GetSelected(tree, (TreeItem) item))
			    TreeItemList_Append(&selected, (TreeItem) item);
		    }
		    hPtr = Tcl_NextHashEntry(&search);
		}
	    } else {
		/* A range of items was specified. */
		if (objc == 5) {
		    count = TreeItem_FirstAndLast(tree, &_item, &_item2);
		    TreeItemList_Free(&itemList);
		    TreeItemList_Init(tree, &itemList, count);
		    while (1) {
			TreeItemList_Append(&itemList, _item);
			if (_item == _item2)
			    break;
			_item = TreeItem_Next(tree, _item);
		    }
		}

		TreeItemList_Init(tree, &deleted, 0);
		TreeItemList_Init(tree, &selected, tree->selectCount);

		count = TreeItemList_Count(&itemList);
		for (i = 0; i < count; i++) {
		    item = (Item *) TreeItemList_ItemN(&itemList, i);
		    if (ISROOT(item))
			continue;
		    if (IS_DELETED(item))
			continue;

		    /* Check every descendant. */
		    item2 = item;
		    while (item2->lastChild != NULL)
			item2 = item2->lastChild;
		    while (1) {
			if (IS_DELETED(item)) {
			    /* Skip all descendants (they are already flagged). */
			    while (item->lastChild != NULL)
				item = item->lastChild;
			} else {
			    item->flags |= ITEM_FLAG_DELETED;
			    TreeItemList_Append(&deleted, (TreeItem) item);
			    if (TreeItem_GetSelected(tree, (TreeItem) item))
				TreeItemList_Append(&selected, (TreeItem) item);
			}
			if (item == item2)
			    break;
			item = (Item *) TreeItem_Next(tree, (TreeItem) item);
		    }
		}
	    }

	    count = TreeItemList_Count(&selected);
	    if (count) {
		for (i = 0; i < count; i++) {
		    _item = TreeItemList_ItemN(&selected, i);
		    Tree_RemoveFromSelection(tree, _item);
		}
		/* Generate <Selection> event for selected items being deleted. */
		TreeNotify_Selection(tree, NULL, &selected);
	    }

	    count = TreeItemList_Count(&deleted);
	    if (count) {
		/* Generate <ItemDelete> event for items being deleted. */
		TreeNotify_ItemDeleted(tree, &deleted);

		/* Add deleted items to the preserved list so they can be
		 * freed when no longer in use. */
		TreeItemList_Concat(&tree->preserveItemList, &deleted);
	    }

	    TreeItemList_Free(&selected);
	    TreeItemList_Free(&deleted);
	    break;
	}
	case COMMAND_DUMP:
	{
	    if (tree->updateIndex)
		Tree_UpdateItemIndex(tree);
	    FormatResult(interp, "index %d indexVis %d neededHeight %d",
		    item->index, item->indexVis, item->neededHeight);
	    break;
	}
	case COMMAND_ENABLED:
	{
	    if (objc == 5) {
		int i, count, enabled;
		TreeItemList newD;
		if (Tcl_GetBooleanFromObj(interp, objv[4], &enabled) != TCL_OK)
		    goto errorExit;
		TreeItemList_Init(tree, &newD, tree->selectCount);
		/* FIXME: ITEM_ALL */
		count = TreeItemList_Count(&itemList);
		for (i = 0; i < count; i++) {
		    _item = TreeItemList_ItemN(&itemList, i);
		    if (enabled != TreeItem_GetEnabled(tree, _item)) {
			int stateOff = enabled ? 0 : STATE_ENABLED;
			int stateOn = enabled ? STATE_ENABLED : 0;
			TreeItem_ChangeState(tree, _item, stateOff, stateOn);
			/* Disabled items cannot be selected. */
			if (!enabled && TreeItem_GetSelected(tree, _item)) {
			    Tree_RemoveFromSelection(tree, _item);
			    TreeItemList_Append(&newD, _item);
			}
		    }
		}
		if (TreeItemList_Count(&newD))
		    TreeNotify_Selection(tree, NULL, &newD);
		TreeItemList_Free(&newD);
	    }
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(item->state & STATE_ENABLED));
	    break;
	}
	case COMMAND_FIRSTCHILD:
	{
	    /* Don't allow non-deleted items to become children of a
	     * deleted item. */
	    if (item2 != NULL && item2 != item->firstChild &&
		    (!IS_DELETED(item) || IS_DELETED(item2))) {
		TreeItem_RemoveFromParent(tree, (TreeItem) item2);
		item2->nextSibling = item->firstChild;
		if (item->firstChild != NULL)
		    item->firstChild->prevSibling = item2;
		else
		    item->lastChild = item2;
		item->firstChild = item2;
		item2->parent = item;
		item->numChildren++;
		TreeItem_AddToParent(tree, (TreeItem) item2);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
	    }
	    if (item->firstChild != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->firstChild));
	    break;
	}
	case COMMAND_ID:
	{
	    Tcl_Obj *listObj;
	    int i, count;

	    /* FIXME: ITEM_ALL */
	    count = TreeItemList_Count(&itemList);
	    if (count) {
		listObj = Tcl_NewListObj(0, NULL);
		for (i = 0; i < count; i++) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    TreeItem_ToObj(tree, TreeItemList_ItemN(&itemList, i)));
		}
		Tcl_SetObjResult(interp, listObj);
	    }
	    break;
	}
	case COMMAND_ISANCESTOR:
	{
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
				 TreeItem_IsAncestor(tree, (TreeItem) item, (TreeItem) item2)));
	    break;
	}
	case COMMAND_ISOPEN:
	{
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(item->state & STATE_OPEN));
	    break;
	}
	case COMMAND_LASTCHILD:
	{
	    /* Don't allow non-deleted items to become children of a
	     * deleted item. */
	    if (item2 != NULL && item2 != item->lastChild &&
		    (!IS_DELETED(item) || IS_DELETED(item2))) {
		TreeItem_RemoveFromParent(tree, (TreeItem) item2);
		item2->prevSibling = item->lastChild;
		if (item->lastChild != NULL)
		    item->lastChild->nextSibling = item2;
		else
		    item->firstChild = item2;
		item->lastChild = item2;
		item2->parent = item;
		item->numChildren++;
		TreeItem_AddToParent(tree, (TreeItem) item2);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
	    }
	    if (item->lastChild != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->lastChild));
	    break;
	}
	case COMMAND_NEXTSIBLING:
	{
	    if (item2 != NULL && item2 != item->nextSibling) {
		TreeItem_RemoveFromParent(tree, (TreeItem) item2);
		item2->prevSibling = item;
		if (item->nextSibling != NULL) {
		    item->nextSibling->prevSibling = item2;
		    item2->nextSibling = item->nextSibling;
		} else
		    item->parent->lastChild = item2;
		item->nextSibling = item2;
		item2->parent = item->parent;
		item->parent->numChildren++;
		TreeItem_AddToParent(tree, (TreeItem) item2);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
	    }
	    if (item->nextSibling != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->nextSibling));
	    break;
	}
	case COMMAND_NUMCHILDREN:
	{
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(item->numChildren));
	    break;
	}
	/* T item order I ?-visible? */
	case COMMAND_ORDER:
	{
	    int visible = FALSE;
	    if (objc == 5) {
		int len;
		char *s = Tcl_GetStringFromObj(objv[4], &len);
		if ((s[0] == '-') && (strncmp(s, "-visible", len) == 0))
		    visible = TRUE;
		else {
		    FormatResult(interp, "bad switch \"%s\": must be -visible",
			s);
		    goto errorExit;
		}
	    }
	    if (tree->updateIndex)
		Tree_UpdateItemIndex(tree);
	    Tcl_SetObjResult(interp,
		    Tcl_NewIntObj(visible ? item->indexVis : item->index));
	    break;
	}
	/* T item range I I */
	case COMMAND_RANGE:
	{
	    TreeItem itemFirst = (TreeItem) item;
	    TreeItem itemLast = (TreeItem) item2;
	    Tcl_Obj *listObj;

	    if (itemFirst == itemLast) {
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, itemFirst));
		break;
	    }
	    (void) TreeItem_FirstAndLast(tree, &itemFirst, &itemLast);
	    listObj = Tcl_NewListObj(0, NULL);
	    while (itemFirst != NULL) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, itemFirst));
		if (itemFirst == itemLast)
		    break;
		itemFirst = TreeItem_Next(tree, itemFirst);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
	case COMMAND_PARENT:
	{
	    if (item->parent != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->parent));
	    break;
	}
	case COMMAND_PREVSIBLING:
	{
	    if (item2 != NULL && item2 != item->prevSibling) {
		TreeItem_RemoveFromParent(tree, (TreeItem) item2);
		item2->nextSibling = item;
		if (item->prevSibling != NULL) {
		    item->prevSibling->nextSibling = item2;
		    item2->prevSibling = item->prevSibling;
		} else
		    item->parent->firstChild = item2;
		item->prevSibling = item2;
		item2->parent = item->parent;
		item->parent->numChildren++;
		TreeItem_AddToParent(tree, (TreeItem) item2);
#ifdef SELECTION_VISIBLE
		Tree_DeselectHidden(tree);
#endif
	    }
	    if (item->prevSibling != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->prevSibling));
	    break;
	}
	case COMMAND_REMOVE:
	{
	    if (item->parent == NULL)
		break;
	    TreeItem_RemoveFromParent(tree, (TreeItem) item);
	    if (tree->debug.enable && tree->debug.data)
		Tree_Debug(tree);
	    Tree_InvalidateColumnWidth(tree, -1);
	    Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
#ifdef SELECTION_VISIBLE
	    Tree_DeselectHidden(tree);
#endif
	    break;
	}
	case COMMAND_RNC:
	{
	    int row,col;

	    if (Tree_ItemToRNC(tree, (TreeItem) item, &row, &col) == TCL_OK)
		FormatResult(interp, "%d %d", row, col);
	    break;
	}
#ifdef COLUMN_SPAN
	/* T item span I ?C? ?span? ?C span ...? */
	case COMMAND_SPAN:
	{
	    TreeColumn treeColumn = tree->columns;
	    Column *column = item->columns;
	    Tcl_Obj *listObj;
	    int i, columnIndex, span;

	    if (objc == 4) {
		listObj = Tcl_NewListObj(0, NULL);
		while (treeColumn != NULL) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    Tcl_NewIntObj(column ? column->span : 1));
		    treeColumn = TreeColumn_Next(treeColumn);
		    if (column != NULL)
			column = column->next;
		}
		Tcl_SetObjResult(interp, listObj);
		break;
	    }
	    if (objc == 5) {
		if (Item_FindColumnFromObj(tree, item, objv[4], &column, NULL) != TCL_OK) {
		    goto errorExit;
		}
		Tcl_SetObjResult(interp, Tcl_NewIntObj(column ? column->span : 1));
		break;
	    }
	    if ((objc - 4) & 1) {
		FormatResult(interp, "wrong # args: should be \"column span column span ...\"");
		goto errorExit;
	    }
	    TreeItem_InvalidateHeight(tree, (TreeItem) item);
	    Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    for (i = 4; i < objc; i += 2) {
		if (Item_CreateColumnFromObj(tree, item, objv[i], &column,
			&columnIndex, NULL) != TCL_OK)
		    goto errorExit;
		if (Tcl_GetIntFromObj(interp, objv[i + 1], &span) != TCL_OK)
		    goto errorExit;
		if (span <= 0) {
		    FormatResult(interp, "bad span \"%d\": must be > 0", span);
		    goto errorExit;
		}
		column->span = span;
		TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
		Tree_InvalidateColumnWidth(tree, columnIndex);
	    }
	    break;
	}
#endif
	/* T item image I ?C? ?image? ?C image ...? */
	case COMMAND_IMAGE:
	/* T item text I ?C? ?text? ?C text ...? */
	case COMMAND_TEXT:
	{
	    TreeColumn treeColumn = tree->columns;
	    Column *column = item->columns;
	    Tcl_Obj *objPtr;
	    int i, columnIndex;
	    int isImage = (index == COMMAND_IMAGE);
	    int result;

	    if (objc == 4) {
		Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
		while (treeColumn != NULL) {
		    if ((column != NULL) && (column->style != NULL))
			objPtr = isImage ?
			    TreeStyle_GetImage(tree, column->style) :
			    TreeStyle_GetText(tree, column->style);
		    else
			objPtr = NULL;
		    if (objPtr == NULL)
			objPtr = Tcl_NewObj();
		    Tcl_ListObjAppendElement(interp, listObj, objPtr);
		    treeColumn = TreeColumn_Next(treeColumn);
		    if (column != NULL)
			column = column->next;
		}
		Tcl_SetObjResult(interp, listObj);
		break;
	    }
	    if (objc == 5) {
		if (Item_FindColumnFromObj(tree, item, objv[4], &column, NULL) != TCL_OK) {
		    goto errorExit;
		}
		if ((column != NULL) && (column->style != NULL)) {
		    objPtr = isImage ?
			TreeStyle_GetImage(tree, column->style) :
			TreeStyle_GetText(tree, column->style);
		    if (objPtr != NULL)
			Tcl_SetObjResult(interp, objPtr);
		}
		break;
	    }
	    if ((objc - 4) & 1) {
		FormatResult(interp, "missing argument after column \"%s\"",
			Tcl_GetString(objv[objc - 1]));
		goto errorExit;
	    }
	    TreeItem_InvalidateHeight(tree, (TreeItem) item);
	    Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    for (i = 4; i < objc; i += 2) {
		if (Item_FindColumnFromObj(tree, item, objv[i], &column,
			&columnIndex) != TCL_OK)
		    goto errorExit;
		if ((column == NULL) || (column->style == NULL)) {
		    NoStyleMsg(tree, item, columnIndex);
		    goto errorExit;
		}
		result = isImage ?
		    TreeStyle_SetImage(tree, (TreeItem) item,
			(TreeItemColumn) column, column->style, objv[i + 1]) :
		    TreeStyle_SetText(tree, (TreeItem) item,
			(TreeItemColumn) column, column->style, objv[i + 1]);
		if (result != TCL_OK)
		    goto errorExit;
		TreeItemColumn_InvalidateSize(tree, (TreeItemColumn) column);
		Tree_InvalidateColumnWidth(tree, columnIndex);
	    }
	    break;
	}
    }

    TreeItemList_Free(&itemList);
    TreeItemList_Free(&item2List);
    return result;

errorExit:
    TreeItemList_Free(&itemList);
    TreeItemList_Free(&item2List);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Debug --
 *
 *	Perform some sanity checks on an Item and its descendants.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeItem_Debug(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_		/* Item token. */
    )
{
    Item *item = (Item *) item_;
    Item *child;
    Tcl_Interp *interp = tree->interp;
    int count;

    if (item->parent == item) {
	FormatResult(interp,
		"parent of %d is itself", item->id);
	return TCL_ERROR;
    }

    if (item->parent == NULL) {
	if (item->prevSibling != NULL) {
	    FormatResult(interp,
		    "parent of %d is nil, prevSibling is not nil",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->nextSibling != NULL) {
	    FormatResult(interp,
		    "parent of %d is nil, nextSibling is not nil",
		    item->id);
	    return TCL_ERROR;
	}
    }

    if (item->prevSibling != NULL) {
	if (item->prevSibling == item) {
	    FormatResult(interp,
		    "prevSibling of %d is itself",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->prevSibling->nextSibling != item) {
	    FormatResult(interp,
		    "item%d.prevSibling.nextSibling is not it",
		    item->id);
	    return TCL_ERROR;
	}
    }

    if (item->nextSibling != NULL) {
	if (item->nextSibling == item) {
	    FormatResult(interp,
		    "nextSibling of %d is itself",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->nextSibling->prevSibling != item) {
	    FormatResult(interp,
		    "item%d.nextSibling->prevSibling is not it",
		    item->id);
	    return TCL_ERROR;
	}
    }

    if (item->numChildren < 0) {
	FormatResult(interp,
		"numChildren of %d is %d",
		item->id, item->numChildren);
	return TCL_ERROR;
    }

    if (item->numChildren == 0) {
	if (item->firstChild != NULL) {
	    FormatResult(interp,
		    "item%d.numChildren is zero, firstChild is not nil",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->lastChild != NULL) {
	    FormatResult(interp,
		    "item%d.numChildren is zero, lastChild is not nil",
		    item->id);
	    return TCL_ERROR;
	}
    }

    if (item->numChildren > 0) {
	if (item->firstChild == NULL) {
	    FormatResult(interp,
		    "item%d.firstChild is nil",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->firstChild == item) {
	    FormatResult(interp,
		    "item%d.firstChild is itself",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->firstChild->parent != item) {
	    FormatResult(interp,
		    "item%d.firstChild.parent is not it",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->firstChild->prevSibling != NULL) {
	    FormatResult(interp,
		    "item%d.firstChild.prevSibling is not nil",
		    item->id);
	    return TCL_ERROR;
	}

	if (item->lastChild == NULL) {
	    FormatResult(interp,
		    "item%d.lastChild is nil",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->lastChild == item) {
	    FormatResult(interp,
		    "item%d.lastChild is itself",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->lastChild->parent != item) {
	    FormatResult(interp,
		    "item%d.lastChild.parent is not it",
		    item->id);
	    return TCL_ERROR;
	}
	if (item->lastChild->nextSibling != NULL) {
	    FormatResult(interp,
		    "item%d.lastChild.nextSibling is not nil",
		    item->id);
	    return TCL_ERROR;
	}

	/* Count number of children */
	count = 0;
	child = item->firstChild;
	while (child != NULL) {
	    count++;
	    child = child->nextSibling;
	}
	if (count != item->numChildren) {
	    FormatResult(interp,
		    "item%d.numChildren is %d, but counted %d",
		    item->id, item->numChildren, count);
	    return TCL_ERROR;
	}

	/* Debug each child recursively */
	child = item->firstChild;
	while (child != NULL) {
	    if (child->parent != item) {
		FormatResult(interp,
			"child->parent of %d is not it",
			item->id);
		return TCL_ERROR;
	    }
	    if (TreeItem_Debug(tree, (TreeItem) child) != TCL_OK)
		return TCL_ERROR;
	    child = child->nextSibling;
	}
    }
    return TCL_OK;
}

#ifdef COLUMN_SPAN

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Identify --
 *
 *	Determine which column and element the given point is in.
 *	This is used by the [identify] widget command.
 *
 * Results:
 *	If the Item is not ReallyVisible() or no columns are visible
 *	then buf[] is untouched. Otherwise the given string may be
 *	appended with "column C" followed by "elem E" although both
 *	are optional.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_Identify(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int x, int y,		/* Window coords to hit-test with. */
    char *buf			/* NULL-terminated string which may be
				 * appended. */
    )
{
    Item *self = (Item *) item_;
    int left, top, width, height;
    int indent, columnWidth, totalWidth;
    Column *column;
    StyleDrawArgs drawArgs;
    TreeColumn treeColumn;
    int i, columnIndex;
    SpanInfo staticSpans[STATIC_SIZE], *spans = staticSpans;
    char *elem;

    if (Tree_ItemBbox(tree, item_, &left, &top, &width, &height) < 0)
	return;

    STATIC_ALLOC(spans, SpanInfo, tree->columnCount);
    Item_GetSpans(tree, item_, spans);

    drawArgs.tree = tree;
    drawArgs.drawable = None;

    totalWidth = 0;
    for (columnIndex = 0; columnIndex < tree->columnCount; columnIndex++) {
	treeColumn = spans[columnIndex].treeColumn;
	if (spans[columnIndex].itemColumnIndex != columnIndex)
	    continue;
	if ((tree->columnCountVis == 1) && (treeColumn == tree->columnVis))
	    columnWidth = width;
	else {
	    columnWidth = 0;
	    for (i = columnIndex; i < tree->columnCount; i++) {
		if (spans[i].itemColumnIndex != columnIndex)
		    break;
		columnWidth += spans[i].width;
	    }
	}
	if (columnWidth <= 0)
	    continue;
	if (treeColumn == tree->columnTree)
	    indent = TreeItem_Indent(tree, item_);
	else
	    indent = 0;
	if ((x >= totalWidth + indent) && (x < totalWidth + columnWidth)) {
	    sprintf(buf + strlen(buf), " column %s%d",
		    tree->columnPrefix, TreeColumn_GetID(treeColumn));
	    column = (Column *) spans[columnIndex].itemColumn;
	    if ((column != NULL) && (column->style != NULL)) {
		drawArgs.state = self->state | column->cstate;
		drawArgs.style = column->style;
		drawArgs.indent = indent;
		drawArgs.x = totalWidth;
		drawArgs.y = 0;
		drawArgs.width = columnWidth;
		drawArgs.height = height;
		drawArgs.justify = TreeColumn_Justify(treeColumn);
		elem = TreeStyle_Identify(&drawArgs, x, y);
		if (elem != NULL)
		    sprintf(buf + strlen(buf), " elem %s", elem);
		break;
	    }
	    break;
	}
	totalWidth += columnWidth;
    }

    STATIC_FREE(spans, SpanInfo, tree->columnCount);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Identify2 --
 *
 *	Determine which columns and elements intersect the given
 *	area. This is used by the [marquee identify] widget command.
 *
 * Results:
 *	If the Item is not ReallyVisible() or no columns are visible
 *	then listObj is untouched. Otherwise the list is appended
 *	with C {E ...} C {E...}.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeItem_Identify2(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item_,		/* Item token. */
    int x1, int y1,		/* Top-left of area to hit-test. */
    int x2, int y2,		/* Bottom-right of area to hit-test. */
    Tcl_Obj *listObj		/* Initialized list object. */
    )
{
    Item *self = (Item *) item_;
    int indent, columnWidth, totalWidth;
    int x, y, w, h;
    Column *column;
    StyleDrawArgs drawArgs;
    TreeColumn treeColumn;
    int i, columnIndex;
    SpanInfo staticSpans[STATIC_SIZE], *spans = staticSpans;

    if (Tree_ItemBbox(tree, item_, &x, &y, &w, &h) < 0)
	return;

    STATIC_ALLOC(spans, SpanInfo, tree->columnCount);
    Item_GetSpans(tree, item_, spans);

    drawArgs.tree = tree;
    drawArgs.drawable = None;

    totalWidth = 0;
    for (columnIndex = 0; columnIndex < tree->columnCount; columnIndex++) {
	treeColumn = spans[columnIndex].treeColumn;
	if (spans[columnIndex].itemColumnIndex != columnIndex)
	    continue;
	if ((tree->columnCountVis == 1) && (treeColumn == tree->columnVis))
	    columnWidth = w;
	else {
	    columnWidth = 0;
	    for (i = columnIndex; i < tree->columnCount; i++) {
		if (spans[i].itemColumnIndex != columnIndex)
		    break;
		columnWidth += spans[i].width;
	    }
	}
	if (columnWidth <= 0)
	    continue;
	if (treeColumn == tree->columnTree)
	    indent = TreeItem_Indent(tree, item_);
	else
	    indent = 0;
	if ((x2 >= x + totalWidth + indent) && (x1 < x + totalWidth + columnWidth)) {
	    Tcl_Obj *subListObj = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(tree->interp, subListObj,
		    TreeColumn_ToObj(tree, treeColumn));
	    column = (Column *) spans[columnIndex].itemColumn;
	    if ((column != NULL) && (column->style != NULL)) {
		drawArgs.state = self->state | column->cstate;
		drawArgs.style = column->style;
		drawArgs.indent = indent;
		drawArgs.x = x + totalWidth;
		drawArgs.y = y;
		drawArgs.width = columnWidth;
		drawArgs.height = h;
		drawArgs.justify = TreeColumn_Justify(treeColumn);
		TreeStyle_Identify2(&drawArgs, x1, y1, x2, y2, subListObj);
	    }
	    Tcl_ListObjAppendElement(tree->interp, listObj, subListObj);
	}
	totalWidth += columnWidth;
    }

    STATIC_FREE(spans, SpanInfo, tree->columnCount);
}

#else /* COLUMN_SPAN */

char *TreeItem_Identify(TreeCtrl *tree, TreeItem item_, int x, int y)
{
    Item *self = (Item *) item_;
    int left, top, width, height;
    int indent, columnWidth, totalWidth;
    Column *column;
    StyleDrawArgs drawArgs;
    TreeColumn treeColumn;

    if (Tree_ItemBbox(tree, item_, &left, &top, &width, &height) < 0)
	return NULL;
#if 0
    if (y >= Tk_Height(tree->tkwin) || y + height <= 0)
	return NULL;
#endif
    drawArgs.tree = tree;
    drawArgs.drawable = None;

    totalWidth = 0;
    treeColumn = tree->columns;
    column = self->columns;
    while (column != NULL) {
	if (!TreeColumn_Visible(treeColumn))
	    columnWidth = 0;
	else if (tree->columnCountVis == 1)
	    columnWidth = width;
	else
	    columnWidth = TreeColumn_UseWidth(treeColumn);
	if (columnWidth > 0) {
	    if (treeColumn == tree->columnTree)
		indent = TreeItem_Indent(tree, item_);
	    else
		indent = 0;
	    if ((x >= totalWidth + indent) && (x < totalWidth + columnWidth)) {
		if (column->style != NULL) {
		    drawArgs.state = self->state | column->cstate;
		    drawArgs.style = column->style;
		    drawArgs.indent = indent;
		    drawArgs.x = totalWidth;
		    drawArgs.y = 0;
		    drawArgs.width = columnWidth;
		    drawArgs.height = height;
		    drawArgs.justify = TreeColumn_Justify(treeColumn);
		    return TreeStyle_Identify(&drawArgs, x, y);
		}
		return NULL;
	    }
	    totalWidth += columnWidth;
	}
	treeColumn = TreeColumn_Next(treeColumn);
	column = column->next;
    }
    return NULL;
}

void TreeItem_Identify2(TreeCtrl *tree, TreeItem item_,
	int x1, int y1, int x2, int y2, Tcl_Obj *listObj)
{
    Item *self = (Item *) item_;
    int indent, columnWidth, totalWidth;
    int x, y, w, h;
    Column *column;
    StyleDrawArgs drawArgs;
    TreeColumn treeColumn;

    if (Tree_ItemBbox(tree, item_, &x, &y, &w, &h) < 0)
	return;

    drawArgs.tree = tree;
    drawArgs.drawable = None;

    totalWidth = 0;
    treeColumn = tree->columns;
    column = self->columns;
    while (treeColumn != NULL) {
	if (!TreeColumn_Visible(treeColumn))
	    columnWidth = 0;
	else if (tree->columnCountVis == 1)
	    columnWidth = w;
	else
	    columnWidth = TreeColumn_UseWidth(treeColumn);
	if (columnWidth > 0) {
	    if (treeColumn == tree->columnTree)
		indent = TreeItem_Indent(tree, item_);
	    else
		indent = 0;
	    if ((x2 >= x + totalWidth + indent) && (x1 < x + totalWidth + columnWidth)) {
		Tcl_Obj *subListObj = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(tree->interp, subListObj,
			TreeColumn_ToObj(tree, treeColumn));
		if ((column != NULL) && (column->style != NULL)) {
		    drawArgs.state = self->state | column->cstate;
		    drawArgs.style = column->style;
		    drawArgs.indent = indent;
		    drawArgs.x = x + totalWidth;
		    drawArgs.y = y;
		    drawArgs.width = columnWidth;
		    drawArgs.height = h;
		    drawArgs.justify = TreeColumn_Justify(treeColumn);
		    TreeStyle_Identify2(&drawArgs, x1, y1, x2, y2, subListObj);
		}
		Tcl_ListObjAppendElement(tree->interp, listObj, subListObj);
	    }
	    totalWidth += columnWidth;
	}
	treeColumn = TreeColumn_Next(treeColumn);
	if (column != NULL)
	    column = column->next;
    }
}

#endif /* not COLUMN_SPAN */

/*
 *----------------------------------------------------------------------
 *
 * TreeItem_Init --
 *
 *	Perform item-related initialization when a new TreeCtrl is
 *	created.
 *
 * Results:
 *	TCL_OK.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

int
TreeItem_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    tree->itemOptionTable = Tk_CreateOptionTable(tree->interp, itemOptionSpecs);

    tree->root = (TreeItem) Item_AllocRoot(tree);
    tree->activeItem = tree->root; /* always non-null */
    tree->anchorItem = tree->root; /* always non-null */

    return TCL_OK;
}
