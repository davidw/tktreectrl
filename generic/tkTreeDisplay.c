/* 
 * tkTreeDisplay.c --
 *
 *	This module implements treectrl widget's main display code.
 *
 * Copyright (c) 2002-2006 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeDisplay.c,v 1.39 2006/09/27 01:43:42 treectrl Exp $
 */

#include "tkTreeCtrl.h"

typedef struct RItem RItem;
typedef struct Range Range;
typedef struct DItem DItem;
typedef struct DInfo DInfo;

static void Range_RedoIfNeeded(TreeCtrl *tree);
static int Range_TotalWidth(TreeCtrl *tree, Range *range_);
static int Range_TotalHeight(TreeCtrl *tree, Range *range_);
static void Range_Redo(TreeCtrl *tree);
static Range *Range_UnderPoint(TreeCtrl *tree, int *x_, int *y_, int nearest);
static RItem *Range_ItemUnderPoint(TreeCtrl *tree, Range *range, int *x_, int *y_);

/* One of these per TreeItem. */
struct RItem
{
    TreeItem item;		/* The item. */
    Range *range;		/* Range the item is in. */
    int size;			/* Height or width consumed in Range. */
    int offset;			/* Vertical or horizontal offset in Range. */
    int index;			/* 0-based index in Range. */
};

/* A collection of visible TreeItems. */
struct Range
{
    RItem *first;
    RItem *last;
    int totalWidth;
    int totalHeight;
    int index;			/* 0-based index in list of Ranges. */
    int offset;			/* vertical/horizontal offset from canvas
				 * top/left. */
    Range *prev;
    Range *next;
};

/* Display information for a TreeItem. */
struct DItem
{
    char magic[4];	/* debug */
    TreeItem item;
    int x, y;			/* Where it should be drawn, window coords. */
    int oldX, oldY;		/* Where it was last drawn, window coords. */
    int width, height;		/* Current width and height. */
    int dirty[4];		/* Dirty area in item coords. */
    Range *range;		/* Range the TreeItem is in. */
    int index;			/* Used for alternating background colors. */
#define DITEM_DIRTY 0x0001
#define DITEM_ALL_DIRTY 0x0002
    int flags;
    DItem *next;
};

/* Display information for a TreeCtrl. */
struct DInfo
{
    GC scrollGC;
    int xOrigin;		/* Last seen TreeCtrl.xOrigin */
    int yOrigin;		/* Last seen TreeCtrl.yOrigin */
    int totalWidth;		/* Last seen Tree_TotalWidth() */
    int totalHeight;		/* Last seen Tree_TotalHeight() */
    int *columnWidth;		/* Last seen column widths */
    int columnWidthSize;	/* Num elements in columnWidth */
    int headerHeight;		/* Last seen TreeCtrl.headerHeight */
    DItem *dItem;		/* Head of list for each displayed item */
    DItem *dItemLast;		/* Temp for UpdateDInfo() */
    Range *rangeFirst;		/* Head of Ranges */
    Range *rangeLast;		/* Tail of Ranges */
    RItem *rItem;		/* Block of RItems for all Ranges */
    int rItemMax;		/* size of rItem[] */
    Range *rangeFirstD;		/* First range with valid display info */
    Range *rangeLastD; 		/* Last range with valid display info */
    int itemHeight;		/* Observed max TreeItem height */
    int itemWidth;		/* Observed max TreeItem width */
    Pixmap pixmap;		/* DOUBLEBUFFER_WINDOW */
    int dirty[4];		/* DOUBLEBUFFER_WINDOW */
    int flags;			/* DINFO_XXX */
    int xScrollIncrement;	/* Last seen TreeCtr.xScrollIncrement */
    int yScrollIncrement;	/* Last seen TreeCtr.yScrollIncrement */
    int *xScrollIncrements;	/* When tree->xScrollIncrement is zero */
    int *yScrollIncrements;	/* When tree->yScrollIncrement is zero */
    int xScrollIncrementCount;	/* Size of xScrollIncrements. */
    int yScrollIncrementCount;	/* Size of yScrollIncrements. */
    int incrementTop;		/* yScrollIncrement[] index of item at top */
    int incrementLeft;		/* xScrollIncrement[] index of item at left */
    TkRegion wsRgn;		/* Region containing whitespace */
    Tcl_HashTable itemVisHash;	/* Table of visible items */
    DItem *dItemFree;		/* List of unused DItems */
    int requests;		/* Incremented for every call to
				   Tree_EventuallyRedraw */
};

/*========*/

void
Tree_FreeItemRInfo(TreeCtrl *tree, TreeItem item)
{
    TreeItem_SetRInfo(tree, item, NULL);
}

static Range *
Range_Free(TreeCtrl *tree, Range *range)
{
    Range *next = range->next;
    WFREE(range, Range);
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * Range_Redo --
 *
 *	This procedure puts all ReallyVisible() TreeItems into a list of
 *	Ranges. If tree->wrapMode is TREE_WRAP_NONE there will only be a
 *	single Range.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Range_Redo(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *rangeList = dInfo->rangeFirst;
    Range *range;
    RItem *rItem;
    TreeItem item = tree->root;
    int fixedWidth = -1, stepWidth = -1;
    int wrapCount = 0, wrapPixels = 0;
    int count, pixels, rItemCount = 0;
    int rangeIndex = 0, itemIndex;

    if (tree->debug.enable && tree->debug.display)
	dbwin("Range_Redo %s\n", Tk_PathName(tree->tkwin));

    /* Update column variables */
    (void) Tree_WidthOfColumns(tree);

    dInfo->rangeFirst = NULL;
    dInfo->rangeLast = NULL;
    if (tree->columnCountVis < 1)
	goto freeRanges;

    switch (tree->wrapMode) {
	case TREE_WRAP_NONE:
	    break;
	case TREE_WRAP_ITEMS:
	    wrapCount = tree->wrapArg;
	    break;
	case TREE_WRAP_PIXELS:
	    wrapPixels = tree->wrapArg;
	    break;
	case TREE_WRAP_WINDOW:
	    wrapPixels = tree->vertical ?
		Tk_Height(tree->tkwin) - tree->inset * 2 - Tree_HeaderHeight(tree) :
		Tk_Width(tree->tkwin) - tree->inset * 2;
	    if (wrapPixels < 0)
		wrapPixels = 0;
	    break;
    }

    /* For horizontal layout with wrapping I need to know how wide each item
     * is. This is the same block of code as in Range_TotalWidth */
    if ((wrapPixels > 0) && !tree->vertical) {

	/* More than one item column, so all items have the same width */
	if (tree->columnCountVis > 1)
	    fixedWidth = Tree_WidthOfColumns(tree);

	/* Single item column, fixed width for all items */
	else if (tree->itemWidth > 0)
	    fixedWidth = tree->itemWidth;

	/* Single item column, fixed width for all items */
	/* THIS IS FOR COMPATIBILITY ONLY */
	else if (TreeColumn_FixedWidth(tree->columnVis) != -1)
	    fixedWidth = TreeColumn_FixedWidth(tree->columns);

	/* Single item column, want all items same width */
	else if (tree->itemWidthEqual ||
		/* This option is deprecated */
		TreeColumn_WidthHack(tree->columnVis)) {

	    fixedWidth = TreeColumn_WidthOfItems(tree->columnVis);

	    /* Each item is a multiple of this width */
	    if (tree->itemWidMult > 0)
		stepWidth = tree->itemWidMult;
	    else
		/* This option is deprecated */
		stepWidth = TreeColumn_StepWidth(tree->columnVis);

	    if ((stepWidth != -1) && (fixedWidth % stepWidth))
		fixedWidth += stepWidth - fixedWidth % stepWidth;

	/* Single item column, variable item width */
	} else {

	    /* Each item is a multiple of this width */
	    if (tree->itemWidMult > 0)
		stepWidth = tree->itemWidMult;
	    else
		/* This option is deprecated */
		stepWidth = TreeColumn_StepWidth(tree->columnVis);
	}
    }

    /* Speed up ReallyVisible() and get itemVisCount */
    if (tree->updateIndex)
	Tree_UpdateItemIndex(tree);

    if (dInfo->rItemMax < tree->itemVisCount) {
	dInfo->rItem = (RItem *) ckrealloc((char *) dInfo->rItem,
		tree->itemVisCount * sizeof(RItem));
	dInfo->rItemMax = tree->itemVisCount;
    }

    if (!TreeItem_ReallyVisible(tree, item))
	item = TreeItem_NextVisible(tree, item);
    while (item != NULL) {
	if (rangeList == NULL)
	    range = (Range *) ckalloc(sizeof(Range));
	else {
	    range = rangeList;
	    rangeList = rangeList->next;
	}
	memset(range, '\0', sizeof(Range));
	range->totalWidth = -1;
	range->totalHeight = -1;
	range->index = rangeIndex++;
	count = 0;
	pixels = 0;
	itemIndex = 0;
	while (1) {
	    rItem = dInfo->rItem + rItemCount;
	    if (rItemCount >= dInfo->rItemMax)
		panic("rItemCount > dInfo->rItemMax");
	    if (range->first == NULL)
		range->first = range->last = rItem;
	    TreeItem_SetRInfo(tree, item, (TreeItemRInfo) rItem);
	    rItem->item = item;
	    rItem->range = range;
	    rItem->index = itemIndex;

	    /* Range must be <= this number of pixels */
	    if (wrapPixels > 0) {
		rItem->offset = pixels;
		if (tree->vertical) {
		    rItem->size = TreeItem_Height(tree, item);
		} else {
		    if (fixedWidth != -1) {
			rItem->size = fixedWidth;
		    } else {
			TreeItemColumn itemColumn =
			    TreeItem_FindColumn(tree, item,
				    TreeColumn_Index(tree->columnVis));
			if (itemColumn != NULL) {
			    int columnWidth =
				TreeItemColumn_NeededWidth(tree, item,
					itemColumn);
			    if (tree->columnTreeVis)
				columnWidth += TreeItem_Indent(tree, item);
			    rItem->size = columnWidth;
			}
			else
			    rItem->size = 0;
			if ((stepWidth != -1) && (rItem->size % stepWidth))
			    rItem->size += stepWidth - rItem->size % stepWidth;
		    }
		}
		/* Too big */
		if (pixels + rItem->size > wrapPixels) {
		    /* Ensure at least one item is in this Range */
		    if (itemIndex == 0) {
			pixels += rItem->size;
			rItemCount++;
		    }
		    break;
		}
		pixels += rItem->size;
	    }
	    range->last = rItem;
	    itemIndex++;
	    rItemCount++;
	    if (++count == wrapCount)
		break;
	    item = TreeItem_NextVisible(tree, item);
	    if (item == NULL)
		break;
	}
	/* Since we needed to calculate the height or width of this range,
	 * we don't need to do it later in Range_TotalWidth/Height() */
	if (wrapPixels > 0) {
	    if (tree->vertical)
		range->totalHeight = pixels;
	    else
		range->totalWidth = pixels;
	}

	if (dInfo->rangeFirst == NULL)
	    dInfo->rangeFirst = range;
	else {
	    range->prev = dInfo->rangeLast;
	    dInfo->rangeLast->next = range;
	}
	dInfo->rangeLast = range;
	item = TreeItem_NextVisible(tree, range->last->item);
    }

    freeRanges:
    while (rangeList != NULL)
	rangeList = Range_Free(tree, rangeList);
}

/*
 *----------------------------------------------------------------------
 *
 * Range_TotalWidth --
 *
 *	Return the width of a Range. The width is only calculated if
 *	it hasn't been done yet by Range_Redo().
 *
 * Results:
 *	Pixel width of the Range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Range_TotalWidth(
    TreeCtrl *tree,		/* Widget info. */
    Range *range		/* Range to return the width of. */
    )
{
    TreeItem item;
    TreeItemColumn itemColumn;
    RItem *rItem;
    int fixedWidth = -1, stepWidth = -1;
    int itemWidth;

    if (range->totalWidth >= 0)
	return range->totalWidth;

    if (tree->vertical) {

	/* More than one item column, so all ranges have the same width */
	if (tree->columnCountVis > 1)
	    return range->totalWidth = Tree_WidthOfColumns(tree);

	/* If wrapping is disabled, then use the column width,
	 * since it may expand to fill the window */
	if (tree->wrapMode == TREE_WRAP_NONE)
	    return range->totalWidth = TreeColumn_UseWidth(tree->columnVis);

	/* Single item column, fixed width for all ranges */
	if (tree->itemWidth > 0)
	    return range->totalWidth = tree->itemWidth;

	/* Single item column, fixed width for all ranges */
	/* THIS IS FOR COMPATIBILITY ONLY */
	if (TreeColumn_FixedWidth(tree->columnVis) != -1)
	    return range->totalWidth = TreeColumn_FixedWidth(tree->columnVis);

	/* Single item column, each item is a multiple of this width */
	if (tree->itemWidMult > 0)
	    stepWidth = tree->itemWidMult;
	else
	    /* This option is deprecated */
	    stepWidth = TreeColumn_StepWidth(tree->columnVis);

	/* Single item column, want all items same width */
	if (tree->itemWidthEqual ||
		/* This option is deprecated */
		TreeColumn_WidthHack(tree->columnVis)) {
	    range->totalWidth = TreeColumn_WidthOfItems(tree->columnVis);
	    if ((stepWidth != -1) && (range->totalWidth % stepWidth))
		range->totalWidth += stepWidth - range->totalWidth % stepWidth;
	    return range->totalWidth;
	}

	/* Max needed width of items in this range */
	range->totalWidth = 0;
	rItem = range->first;
	while (1) {
	    item = rItem->item;
	    itemColumn = TreeItem_FindColumn(tree, item,
		    TreeColumn_Index(tree->columnVis));
	    if (itemColumn != NULL)
		itemWidth = TreeItemColumn_NeededWidth(tree, item,
			itemColumn);
	    else
		itemWidth = 0;
	    if (tree->columnTreeVis)
		itemWidth += TreeItem_Indent(tree, item);
	    if (itemWidth > range->totalWidth)
		range->totalWidth = itemWidth;
	    if (rItem == range->last)
		break;
	    rItem++;
	}
	if ((stepWidth != -1) && (range->totalWidth % stepWidth))
	    range->totalWidth += stepWidth - range->totalWidth % stepWidth;
	return range->totalWidth;
    }
    else {
	/* More than one item column, so all items have the same width */
	if (tree->columnCountVis > 1)
	    fixedWidth = Tree_WidthOfColumns(tree);

	/* Single item column, fixed width for all items */
	else if (tree->itemWidth > 0)
	    fixedWidth = tree->itemWidth;

	/* Single item column, fixed width for all items */
	/* THIS IS FOR COMPATIBILITY ONLY */
	else if (TreeColumn_FixedWidth(tree->columnVis) != -1)
	    fixedWidth = TreeColumn_FixedWidth(tree->columnVis);

	/* Single item column, want all items same width */
	else if (tree->itemWidthEqual ||
		/* This option is deprecated */
		TreeColumn_WidthHack(tree->columnVis)) {

	    fixedWidth = TreeColumn_WidthOfItems(tree->columnVis);

	    /* Each item is a multiple of this width */
	    if (tree->itemWidMult > 0)
		stepWidth = tree->itemWidMult;
	    else
		/* This option is deprecated */
		stepWidth = TreeColumn_StepWidth(tree->columnVis);

	    if ((stepWidth != -1) && (fixedWidth % stepWidth))
		fixedWidth += stepWidth - fixedWidth % stepWidth;
	}

	/* Single item column, variable item width */
	else {
	    /* Each item is a multiple of this width */
	    if (tree->itemWidMult > 0)
		stepWidth = tree->itemWidMult;
	    else
		/* This option is deprecated */
		stepWidth = TreeColumn_StepWidth(tree->columnVis);
	}

	/* Sum of widths of items in this range */
	range->totalWidth = 0;
	rItem = range->first;
	while (1) {
	    item = rItem->item;
	    if (fixedWidth != -1)
		itemWidth = fixedWidth;
	    else {
		itemColumn = TreeItem_FindColumn(tree, item,
			TreeColumn_Index(tree->columnVis));
		if (itemColumn != NULL)
		    itemWidth = TreeItemColumn_NeededWidth(tree, item,
			    itemColumn);
		else
		    itemWidth = 0;

		if (tree->columnTreeVis)
		    itemWidth += TreeItem_Indent(tree, item);

		if ((stepWidth != -1) && (itemWidth % stepWidth))
		    itemWidth += stepWidth - itemWidth % stepWidth;
	    }

	    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
	    rItem->offset = range->totalWidth;
	    rItem->size = itemWidth;

	    range->totalWidth += itemWidth;
	    if (rItem == range->last)
		break;
	    rItem++;
	}
	return range->totalWidth;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Range_TotalHeight --
 *
 *	Return the height of a Range. The height is only calculated if
 *	it hasn't been done yet by Range_Redo().
 *
 * Results:
 *	Pixel height of the Range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Range_TotalHeight(
    TreeCtrl *tree,		/* Widget info. */
    Range *range		/* Range to return the height of. */
    )
{
    TreeItem item;
    RItem *rItem;
    int itemHeight;

    if (range->totalHeight >= 0)
	return range->totalHeight;

    range->totalHeight = 0;
    rItem = range->first;
    while (1) {
	item = rItem->item;
	itemHeight = TreeItem_Height(tree, item);
	if (tree->vertical) {
	    rItem->offset = range->totalHeight;
	    rItem->size = itemHeight;
	    range->totalHeight += itemHeight;
	}
	else {
	    if (itemHeight > range->totalHeight)
		range->totalHeight = itemHeight;
	}
	if (rItem == range->last)
	    break;
	rItem++;
    }
    return range->totalHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_TotalWidth --
 *
 *	Return the width needed by all the Ranges. The width is only
 *	recalculated if it was marked out-of-date.
 *
 * Results:
 *	Pixel width of the "canvas".
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *----------------------------------------------------------------------
 */

int
Tree_TotalWidth(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range;
    int rangeWidth;

    Range_RedoIfNeeded(tree);

    if (tree->totalWidth >= 0)
	return tree->totalWidth;

    tree->totalWidth = 0;
    range = dInfo->rangeFirst;
    while (range != NULL) {
	rangeWidth = Range_TotalWidth(tree, range);
	if (tree->vertical) {
	    range->offset = tree->totalWidth;
	    tree->totalWidth += rangeWidth;
	}
	else {
	    if (rangeWidth > tree->totalWidth)
		tree->totalWidth = rangeWidth;
	}
	range = range->next;
    }
    return tree->totalWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_TotalHeight --
 *
 *	Return the height needed by all the Ranges. The height is only
 *	recalculated if it was marked out-of-date.
 *
 * Results:
 *	Pixel height of the "canvas".
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *----------------------------------------------------------------------
 */

int
Tree_TotalHeight(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range;
    int rangeHeight;

    Range_RedoIfNeeded(tree);

    if (tree->totalHeight >= 0)
	return tree->totalHeight;

    tree->totalHeight = 0;
    range = dInfo->rangeFirst;
    while (range != NULL) {
	rangeHeight = Range_TotalHeight(tree, range);
	if (tree->vertical) {
	    if (rangeHeight > tree->totalHeight)
		tree->totalHeight = rangeHeight;
	}
	else {
	    range->offset = tree->totalHeight;
	    tree->totalHeight += rangeHeight;
	}
	range = range->next;
    }
    return tree->totalHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * Range_UnderPoint --
 *
 *	Return the Range containing the given coordinates.
 *
 * Results:
 *	Range containing the coordinates or NULL if the point is
 *	outside any Range.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *----------------------------------------------------------------------
 */

static Range *
Range_UnderPoint(
    TreeCtrl *tree,		/* Widget info. */
    int *x_,			/* In: window x coordinate.
				 * Out: x coordinate relative to the top-left
				 * of the Range. */
    int *y_,			/* In: window y coordinate.
				 * Out: y coordinate relative to the top-left
				 * of the Range. */
    int nearest			/* TRUE if the Range nearest the coordinates
				 * should be returned. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range;
    int x = *x_, y = *y_;

    Range_RedoIfNeeded(tree);

    if ((Tree_TotalWidth(tree) <= 0) || (Tree_TotalHeight(tree) <= 0))
	return NULL;

    range = dInfo->rangeFirst;

    if (nearest) {
	int visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
	int topInset = tree->inset + Tree_HeaderHeight(tree);
	int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;

	if ((visWidth <= 0) || (visHeight <= 0))
	    return NULL;

	/* Keep inside borders and header. Perhaps another flag needed. */
	if (x < tree->inset)
	    x = tree->inset;
	if (x >= Tk_Width(tree->tkwin) - tree->inset)
	    x = Tk_Width(tree->tkwin) - tree->inset - 1;
	if (y < topInset)
	    y = topInset;
	if (y >= Tk_Height(tree->tkwin) - tree->inset)
	    y = Tk_Height(tree->tkwin) - tree->inset - 1;
    }

    /* Window -> canvas */
    x += tree->xOrigin;
    y += tree->yOrigin;

    if (nearest) {
	if (x < 0)
	    x = 0;
	if (x >= Tree_TotalWidth(tree))
	    x = Tree_TotalWidth(tree) - 1;
	if (y < 0)
	    y = 0;
	if (y >= Tree_TotalHeight(tree))
	    y = Tree_TotalHeight(tree) - 1;
    }
    else {
	if (x < 0)
	    return NULL;
	if (x >= Tree_TotalWidth(tree))
	    return NULL;
	if (y < 0)
	    return NULL;
	if (y >= Tree_TotalHeight(tree))
	    return NULL;
    }

    if (tree->vertical) {
	while (range != NULL) {
	    if ((x >= range->offset) && (x < range->offset + range->totalWidth)) {
		if (nearest || (y < range->totalHeight)) {
		    (*x_) = x - range->offset;
		    (*y_) = MIN(y, range->totalHeight - 1);
		    return range;
		}
		return NULL;
	    }
	    range = range->next;
	}
	return NULL;
    }
    else {
	while (range != NULL) {
	    if ((y >= range->offset) && (y < range->offset + range->totalHeight)) {
		if (nearest || (x < range->totalWidth)) {
		    (*x_) = MIN(x, range->totalWidth - 1);
		    (*y_) = y - range->offset;
		    return range;
		}
		return NULL;
	    }
	    range = range->next;
	}
	return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Range_ItemUnderPoint --
 *
 *	Return the RItem containing the given x and/or y coordinates.
 *
 * Results:
 *	RItem containing the coordinates. Panics() if no RItem is found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static RItem *
Range_ItemUnderPoint(
    TreeCtrl *tree,		/* Widget info. */
    Range *range,		/* Range to search. */
    int *x_,			/* In: x coordinate relative to top-left of
				 * the Range.
				 * Out: x coordinate relative to top-left of
				 * the returned RItem.
				 * May be NULL if y_ is not NULL. */
    int *y_			/* In: y coordinate relative to top-left of
				 * the Range.
				 * Out: y coordinate relative to top-left of
				 * the returned RItem.
				 * May be NULL if x_ is not NULL. */
    )
{
    RItem *rItem;
    int x = -666, y = -666;
    int i, l, u;

    if (x_ != NULL) {
	x = (*x_);
	if (x < 0 || x >= range->totalWidth)
	    goto panicNow;
    }
    if (y_ != NULL) {
	y = (*y_);
	if (y < 0 || y >= range->totalHeight)
	    goto panicNow;
    }

    if (tree->vertical) {
	/* Binary search */
	l = 0;
	u = range->last->index;
	while (l <= u) {
	    i = (l + u) / 2;
	    rItem = range->first + i;
	    if ((y >= rItem->offset) && (y < rItem->offset + rItem->size)) {
		/* Range -> item coords */
		if (x_ != NULL) (*x_) = x;
		if (y_ != NULL) (*y_) = y - rItem->offset;
		return rItem;
	    }
	    if (y < rItem->offset)
		u = i - 1;
	    else
		l = i + 1;
	}
    }
    else {
	/* Binary search */
	l = 0;
	u = range->last->index;
	while (l <= u) {
	    i = (l + u) / 2;
	    rItem = range->first + i;
	    if ((x >= rItem->offset) && (x < rItem->offset + rItem->size)) {
		/* Range -> item coords */
		if (x_ != NULL) (*x_) = x - rItem->offset;
		if (y_ != NULL) (*y_) = y;
		return rItem;
	    }
	    if (x < rItem->offset)
		u = i - 1;
	    else
		l = i + 1;
	}
    }

    panicNow:
    panic("Range_ItemUnderPoint: can't find TreeItem in Range: x %d y %d W %d H %d",
	    x, y, range->totalWidth, range->totalHeight);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Increment_AddX --
 *
 *	Appends one or more values to the list of horizontal scroll
 *	increments.
 *
 * Results:
 *	New size of DInfo.xScrollIncrements.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static int
Increment_AddX(
    TreeCtrl *tree,		/* Widget info. */
    int offset,			/* Offset to add. */
    int size			/* Current size of DInfo.xScrollIncrements. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    int visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;

    while ((visWidth > 1) &&
	    (offset - dInfo->xScrollIncrements[dInfo->xScrollIncrementCount - 1]
		    > visWidth)) {
	size = Increment_AddX(tree,
		dInfo->xScrollIncrements[dInfo->xScrollIncrementCount - 1] + visWidth,
		size);
    }
    if (dInfo->xScrollIncrementCount + 1 > size) {
	size *= 2;
	dInfo->xScrollIncrements = (int *) ckrealloc(
	    (char *) dInfo->xScrollIncrements, size * sizeof(int));
    }
    dInfo->xScrollIncrements[dInfo->xScrollIncrementCount++] = offset;
    return size;
}

/*
 *----------------------------------------------------------------------
 *
 * Increment_AddY --
 *
 *	Appends one or more values to the list of vertical scroll
 *	increments.
 *
 * Results:
 *	New size of DInfo.yScrollIncrements.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static int
Increment_AddY(
    TreeCtrl *tree,		/* Widget info. */
    int offset,			/* Offset to add. */
    int size			/* Current size of DInfo.yScrollIncrements. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    int topInset = tree->inset + Tree_HeaderHeight(tree);
    int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;

    while ((visHeight > 1) &&
	    (offset - dInfo->yScrollIncrements[dInfo->yScrollIncrementCount - 1]
		    > visHeight)) {
	size = Increment_AddY(tree,
		dInfo->yScrollIncrements[dInfo->yScrollIncrementCount - 1] + visHeight,
		size);
    }
    if (dInfo->yScrollIncrementCount + 1 > size) {
	size *= 2;
	dInfo->yScrollIncrements = (int *) ckrealloc(
	    (char *) dInfo->yScrollIncrements, size * sizeof(int));
    }
    dInfo->yScrollIncrements[dInfo->yScrollIncrementCount++] = offset;
    return size;
}

/*
 *----------------------------------------------------------------------
 *
 * RItemsToIncrementsX --
 *
 *	Recalculate the list of horizontal scroll increments. This gets
 *	called when the TreeCtrl -orient option is "horizontal" and
 *	-xscrollincrement option is "".
 *
 * Results:
 *	DInfo.xScrollIncrements is updated if the canvas width is > 0.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
RItemsToIncrementsX(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range;
    RItem *rItem;
    int visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
    int totalWidth = Tree_TotalWidth(tree);
    int x1, x2, x;
    int size;

    if (totalWidth <= 0 /* dInfo->rangeFirst == NULL */)
	return;

    /* First increment is zero */
    size = 10;
    dInfo->xScrollIncrements = (int *) ckalloc(size * sizeof(int));
    dInfo->xScrollIncrements[dInfo->xScrollIncrementCount++] = 0;

    x1 = 0;
    while (1) {
	x2 = totalWidth;
	for (range = dInfo->rangeFirst;
	     range != NULL;
	     range = range->next) {
	    if (x1 >= range->totalWidth)
		continue;

	    /* Find RItem whose right side is >= x1 by smallest amount */
	    x = x1;
	    rItem = Range_ItemUnderPoint(tree, range, &x, NULL);
	    if (rItem->offset + rItem->size < x2)
		x2 = rItem->offset + rItem->size;
	}
	if (x2 == totalWidth)
	    break;
	size = Increment_AddX(tree, x2, size);
	x1 = x2;
    }
    if ((visWidth > 1) && (totalWidth -
		dInfo->xScrollIncrements[dInfo->xScrollIncrementCount - 1] > visWidth)) {
	Increment_AddX(tree, totalWidth, size);
	dInfo->xScrollIncrementCount--;
	dInfo->xScrollIncrements[dInfo->xScrollIncrementCount - 1] = totalWidth - visWidth;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RItemsToIncrementsY --
 *
 *	Recalculate the list of vertical scroll increments. This gets
 *	called when the TreeCtrl -orient option is "vertical" and
 *	-yscrollincrement option is "".
 *
 * Results:
 *	DInfo.yScrollIncrements is updated if the canvas height is > 0.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
RItemsToIncrementsY(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range;
    RItem *rItem;
    int topInset = tree->inset + Tree_HeaderHeight(tree);
    int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;
    int totalHeight = Tree_TotalHeight(tree);
    int y1, y2, y;
    int size;

    if (totalHeight <= 0 /* dInfo->rangeFirst == NULL */)
	return;

    /* First increment is zero */
    size = 10;
    dInfo->yScrollIncrements = (int *) ckalloc(size * sizeof(int));
    dInfo->yScrollIncrements[dInfo->yScrollIncrementCount++] = 0;

    y1 = 0;
    while (1) {
	y2 = totalHeight;
	for (range = dInfo->rangeFirst;
	     range != NULL;
	     range = range->next) {
	    if (y1 >= range->totalHeight)
		continue;

	    /* Find RItem whose bottom edge is >= y1 by smallest amount */
	    y = y1;
	    rItem = Range_ItemUnderPoint(tree, range, NULL, &y);
	    if (rItem->offset + rItem->size < y2)
		y2 = rItem->offset + rItem->size;
	}
	if (y2 == totalHeight)
	    break;
	size = Increment_AddY(tree, y2, size);
	y1 = y2;
    }
    if ((visHeight > 1) && (totalHeight -
		dInfo->yScrollIncrements[dInfo->yScrollIncrementCount - 1] > visHeight)) {
	size = Increment_AddY(tree, totalHeight, size);
	dInfo->yScrollIncrementCount--;
	dInfo->yScrollIncrements[dInfo->yScrollIncrementCount - 1] = totalHeight - visHeight;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RangesToIncrementsX --
 *
 *	Recalculate the list of horizontal scroll increments. This gets
 *	called when the TreeCtrl -orient option is "vertical" and
 *	-xscrollincrement option is "".
 *
 * Results:
 *	DInfo.xScrollIncrements is updated if there are any Ranges.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
RangesToIncrementsX(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range = dInfo->rangeFirst;
    int visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
    int totalWidth = Tree_TotalWidth(tree);
    int size;

    if (dInfo->rangeFirst == NULL)
	return;

    /* First increment is zero */
    size = 10;
    dInfo->xScrollIncrements = (int *) ckalloc(size * sizeof(int));
    dInfo->xScrollIncrements[dInfo->xScrollIncrementCount++] = 0;

    range = dInfo->rangeFirst->next;
    while (range != NULL) {
	size = Increment_AddX(tree, range->offset, size);
	range = range->next;
    }
    if ((visWidth > 1) && (totalWidth -
		dInfo->xScrollIncrements[dInfo->xScrollIncrementCount - 1] > visWidth)) {
	size = Increment_AddX(tree, totalWidth, size);
	dInfo->xScrollIncrementCount--;
	dInfo->xScrollIncrements[dInfo->xScrollIncrementCount - 1] = totalWidth - visWidth;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RangesToIncrementsY --
 *
 *	Recalculate the list of vertical scroll increments. This gets
 *	called when the TreeCtrl -orient option is "horizontal" and
 *	-yscrollincrement option is "".
 *
 * Results:
 *	DInfo.yScrollIncrements is updated if there are any Ranges.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
RangesToIncrementsY(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range = dInfo->rangeFirst;
    int topInset = tree->inset + Tree_HeaderHeight(tree);
    int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;
    int totalHeight = Tree_TotalHeight(tree);
    int size;

    if (dInfo->rangeFirst == NULL)
	return;

    /* First increment is zero */
    size = 10;
    dInfo->yScrollIncrements = (int *) ckalloc(size * sizeof(int));
    dInfo->yScrollIncrements[dInfo->yScrollIncrementCount++] = 0;

    range = dInfo->rangeFirst->next;
    while (range != NULL) {
	size = Increment_AddY(tree, range->offset, size);
	range = range->next;
    }
    if ((visHeight > 1) && (totalHeight -
		dInfo->yScrollIncrements[dInfo->yScrollIncrementCount - 1] > visHeight)) {
	size = Increment_AddY(tree, totalHeight, size);
	dInfo->yScrollIncrementCount--;
	dInfo->yScrollIncrements[dInfo->yScrollIncrementCount - 1] = totalHeight - visHeight;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Increment_Redo --
 *
 *	Recalculate the lists of scroll increments.
 *
 * Results:
 *	DInfo.xScrollIncrements and DInfo.xScrollIncrements are updated.
 *	Either may be set to NULL. The old values are freed, if any.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Increment_Redo(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    /* Free x */
    if (dInfo->xScrollIncrements != NULL)
	ckfree((char *) dInfo->xScrollIncrements);
    dInfo->xScrollIncrements = NULL;
    dInfo->xScrollIncrementCount = 0;

    /* Free y */
    if (dInfo->yScrollIncrements != NULL)
	ckfree((char *) dInfo->yScrollIncrements);
    dInfo->yScrollIncrements = NULL;
    dInfo->yScrollIncrementCount = 0;

    if (tree->vertical) {
	/* No xScrollIncrement is given. Snap to left edge of a Range */
	if (tree->xScrollIncrement <= 0)
	    RangesToIncrementsX(tree);

	/* No yScrollIncrement is given. Snap to top edge of a TreeItem */
	if (tree->yScrollIncrement <= 0)
	    RItemsToIncrementsY(tree);
    }
    else {
	/* No xScrollIncrement is given. Snap to left edge of a TreeItem */
	if (tree->xScrollIncrement <= 0)
	    RItemsToIncrementsX(tree);

	/* No yScrollIncrement is given. Snap to top edge of a Range */
	if (tree->yScrollIncrement <= 0)
	    RangesToIncrementsY(tree);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Increment_RedoIfNeeded --
 *
 *	Recalculate the lists of scroll increments if needed.
 *
 * Results:
 *	DInfo.xScrollIncrements and DInfo.xScrollIncrements may be
 *	updated.
 *
 * Side effects:
 *	Memory may be allocated. The list of Ranges will be recalculated
 *	if needed.
 *
 *----------------------------------------------------------------------
 */

static void
Increment_RedoIfNeeded(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    Range_RedoIfNeeded(tree);

    /* Check for x|yScrollIncrement >0 changing to <=0 */
    if (((dInfo->yScrollIncrement > 0) != (tree->yScrollIncrement > 0)) ||
	    ((dInfo->xScrollIncrement > 0) != (tree->xScrollIncrement > 0))) {
	dInfo->yScrollIncrement = tree->yScrollIncrement;
	dInfo->xScrollIncrement = tree->xScrollIncrement;
	dInfo->flags |= DINFO_REDO_INCREMENTS;
    }
    if (dInfo->flags & DINFO_REDO_INCREMENTS) {
	Increment_Redo(tree);
	dInfo->flags &= ~DINFO_REDO_INCREMENTS;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * B_IncrementFind --
 *
 *	Search a list of increments and return one nearest to the
 *	given offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *	Panic() if no appropriate offset if found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
B_IncrementFind(
    int *increments,		/* DInfo.x|yScrollIncrements. */
    int count,			/* Length of increments[]. */
    int offset			/* Offset to search with. */
    )
{
    int i, l, u, v;

    if (offset < 0)
	offset = 0;

    /* Binary search */
    l = 0;
    u = count - 1;
    while (l <= u) {
	i = (l + u) / 2;
	v = increments[i];
	if ((offset >= v) && ((i == count - 1) || (offset < increments[i + 1])))
	    return i;
	if (offset < v)
	    u = i - 1;
	else
	    l = i + 1;
    }
    panic("B_IncrementFind failed (count %d offset %d)", count, offset);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * B_IncrementFindX --
 *
 *	Search DInfo.xScrollIncrements and return one nearest to the
 *	given offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *	Panic() if no appropriate offset if found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
B_IncrementFindX(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Offset to search with. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    return B_IncrementFind(
	dInfo->xScrollIncrements,
	dInfo->xScrollIncrementCount,
	offset);
}

/*
 *----------------------------------------------------------------------
 *
 * B_IncrementFindY --
 *
 *	Search DInfo.yScrollIncrements and return one nearest to the
 *	given offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *	Panic() if no appropriate offset if found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
B_IncrementFindY(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Offset to search with. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    return B_IncrementFind(
	dInfo->yScrollIncrements,
	dInfo->yScrollIncrementCount,
	offset);
}

/*
 *--------------------------------------------------------------
 *
 * B_XviewCmd --
 *
 *	This procedure is invoked to process the "xview" option for
 *	the widget command for a TreeCtrl. See the user documentation
 *	for details on what it does.
 *
 *	NOTE: This procedure is called when the -xscrollincrement option
 *	is unspecified.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
B_XviewCmd(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tcl_Interp *interp = tree->interp;
    DInfo *dInfo = (DInfo *) tree->dInfo;

    if (objc == 2) {
	double fractions[2];
	char buf[TCL_DOUBLE_SPACE * 2];

	Tree_GetScrollFractionsX(tree, fractions);
	sprintf(buf, "%g %g", fractions[0], fractions[1]);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
    } else {
	int count, index = 0, indexMax, offset, type;
	double fraction;
	int visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
	int totWidth = Tree_TotalWidth(tree);

	if (visWidth < 0)
	    visWidth = 0;
	if (totWidth <= visWidth)
	    return TCL_OK;

	if (visWidth > 1) {
	    /* Find incrementLeft when scrolled to right */
	    indexMax = Increment_FindX(tree, totWidth - visWidth);
	    offset = Increment_ToOffsetX(tree, indexMax);
	    if (offset < totWidth - visWidth) {
		indexMax++;
		offset = Increment_ToOffsetX(tree, indexMax);
	    }

	    /* Add some fake content to right */
	    if (offset + visWidth > totWidth)
		totWidth = offset + visWidth;
	} else {
	    indexMax = Increment_FindX(tree, totWidth);
	    visWidth = 1;
	}

	type = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
	switch (type) {
	    case TK_SCROLL_ERROR:
		return TCL_ERROR;
	    case TK_SCROLL_MOVETO:
		offset = (int) (fraction * totWidth + 0.5);
		index = Increment_FindX(tree, offset);
		break;
	    case TK_SCROLL_PAGES:
		offset = tree->inset + tree->xOrigin;
		offset += (int) (count * visWidth * 0.9);
		index = Increment_FindX(tree, offset);
		if ((count > 0) && (index ==
			    Increment_FindX(tree, tree->inset + tree->xOrigin)))
		    index++;
		break;
	    case TK_SCROLL_UNITS:
		index = dInfo->incrementLeft + count;
		break;
	}

	/* Don't scroll too far left */
	if (index < 0)
	    index = 0;

	/* Don't scroll too far right */
	if (index > indexMax)
	    index = indexMax;

	offset = Increment_ToOffsetX(tree, index);
	if ((index != dInfo->incrementLeft) || (tree->xOrigin != offset - tree->inset)) {
	    dInfo->incrementLeft = index;
	    tree->xOrigin = offset - tree->inset;
	    Tree_EventuallyRedraw(tree);
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * B_YviewCmd --
 *
 *	This procedure is invoked to process the "yview" option for
 *	the widget command for a TreeCtrl. See the user documentation
 *	for details on what it does.
 *
 *	NOTE: This procedure is called when the -yscrollincrement option
 *	is unspecified.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
B_YviewCmd(
    TreeCtrl *tree,		/* Widget info. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    Tcl_Interp *interp = tree->interp;
    DInfo *dInfo = (DInfo *) tree->dInfo;

    if (objc == 2) {
	double fractions[2];
	char buf[TCL_DOUBLE_SPACE * 2];

	Tree_GetScrollFractionsY(tree, fractions);
	sprintf(buf, "%g %g", fractions[0], fractions[1]);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
    }
    else {
	int count, index = 0, indexMax, offset, type;
	double fraction;
	int topInset = tree->inset + Tree_HeaderHeight(tree);
	int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;
	int totHeight = Tree_TotalHeight(tree);

	if (visHeight < 0)
	    visHeight = 0;
	if (totHeight <= visHeight)
	    return TCL_OK;

	if (visHeight > 1) {
	    /* Find incrementTop when scrolled to bottom */
	    indexMax = Increment_FindY(tree, totHeight - visHeight);
	    offset = Increment_ToOffsetY(tree, indexMax);
	    if (offset < totHeight - visHeight) {
		indexMax++;
		offset = Increment_ToOffsetY(tree, indexMax);
	    }

	    /* Add some fake content to bottom */
	    if (offset + visHeight > totHeight)
		totHeight = offset + visHeight;
	}
	else {
	    indexMax = Increment_FindY(tree, totHeight);
	    visHeight = 1;
	}

	type = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
	switch (type) {
	    case TK_SCROLL_ERROR:
		return TCL_ERROR;
	    case TK_SCROLL_MOVETO:
		offset = (int) (fraction * totHeight + 0.5);
		index = Increment_FindY(tree, offset);
		break;
	    case TK_SCROLL_PAGES:
		offset = topInset + tree->yOrigin;
		offset += (int) (count * visHeight * 0.9);
		index = Increment_FindY(tree, offset);
		if ((count > 0) && (index ==
			    Increment_FindY(tree, topInset + tree->yOrigin)))
		    index++;
		break;
	    case TK_SCROLL_UNITS:
		index = dInfo->incrementTop + count;
		break;
	}

	/* Don't scroll too far up */
	if (index < 0)
	    index = 0;

	/* Don't scroll too far down */
	if (index > indexMax)
	    index = indexMax;

	offset = Increment_ToOffsetY(tree, index);
	if ((index != dInfo->incrementTop) || (tree->yOrigin != offset - topInset)) {
	    dInfo->incrementTop = index;
	    tree->yOrigin = offset - topInset;
	    Tree_EventuallyRedraw(tree);
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemUnderPoint --
 *
 *	Return a TreeItem containing the given coordinates.
 *
 * Results:
 *	TreeItem token or NULL if no item contains the point.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

TreeItem
Tree_ItemUnderPoint(
    TreeCtrl *tree,		/* Widget info. */
    int *x_, int *y_,		/* In: window coordinates.
				 * Out: coordinates relative to top-left
				 * corner of the returned item. */
    int nearest			/* TRUE if the item nearest the coordinates
				 * should be returned. */
    )
{
    Range *range;
    RItem *rItem;

    range = Range_UnderPoint(tree, x_, y_, nearest);
    if (range == NULL)
	return NULL;
    rItem = Range_ItemUnderPoint(tree, range, x_, y_);
    if (rItem != NULL)
	return rItem->item;
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemBbox --
 *
 *	Return the bounding box for an item.
 *
 * Results:
 *	Return value is -1 if the item is not ReallyVisible()
 *	or if there are no visible columns. The coordinates
 *	are relative to the top-left corner of the canvas.
 *
 * Side effects:
 *	Column layout will be updated if needed.
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

int
Tree_ItemBbox(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item whose bbox is needed. */
    int *x, int *y,		/* Returned left and top. */
    int *w, int *h		/* Returned width and height. */
    )
{
    Range *range;
    RItem *rItem;

    /* Update columnCountVis if needed */
    (void) Tree_WidthOfColumns(tree);

    if (!TreeItem_ReallyVisible(tree, item) || (tree->columnCountVis < 1))
	return -1;
    Range_RedoIfNeeded(tree);
    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
    range = rItem->range;
    if (tree->vertical) {
	(*x) = range->offset;
	(*w) = range->totalWidth;
	(*y) = rItem->offset;
	(*h) = rItem->size;
    }
    else {
	(*x) = rItem->offset;
	(*w) = rItem->size;
	(*y) = range->offset;
	(*h) = range->totalHeight;
    }
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemLARB --
 *
 *	Return an adjacent item above, below, to the left or to the
 *	right of the given item.
 *
 * Results:
 *	An adjacent item or NULL if there is no such item.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

TreeItem
Tree_ItemLARB(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item to use as a reference. */
    int vertical,		/* TRUE if items are arranged
				 * from top-to-bottom in each Range. */
    int prev			/* TRUE for above/left,
				 * FALSE for below/right. */
    )
{
    RItem *rItem, *rItem2;
    Range *range;
    int i, l, u;

    if (!TreeItem_ReallyVisible(tree, item) || (tree->columnCountVis < 1))
	return NULL;
    Range_RedoIfNeeded(tree);
    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
    if (vertical) {
	if (prev) {
	    if (rItem == rItem->range->first)
		return NULL;
	    rItem--;
	}
	else {
	    if (rItem == rItem->range->last)
		return NULL;
	    rItem++;
	}
	return rItem->item;
    }
    else {
	/* Find the previous range */
	range = prev ? rItem->range->prev : rItem->range->next;
	if (range == NULL)
	    return NULL;

	/* Find item with same index */
	/* Binary search */
	l = 0;
	u = range->last->index;
	while (l <= u) {
	    i = (l + u) / 2;
	    rItem2 = range->first + i;
	    if (rItem2->index == rItem->index)
		return rItem2->item;
	    if (rItem->index < rItem2->index)
		u = i - 1;
	    else
		l = i + 1;
	}
    }
    return NULL;
}

TreeItem
Tree_ItemLeft(TreeCtrl *tree, TreeItem item)
{
    return Tree_ItemLARB(tree, item, !tree->vertical, TRUE);
}

TreeItem
Tree_ItemAbove(TreeCtrl *tree, TreeItem item)
{
    return Tree_ItemLARB(tree, item, tree->vertical, TRUE);
}

TreeItem
Tree_ItemRight(TreeCtrl *tree, TreeItem item)
{
    return Tree_ItemLARB(tree, item, !tree->vertical, FALSE);
}

TreeItem
Tree_ItemBelow(TreeCtrl *tree, TreeItem item)
{
    return Tree_ItemLARB(tree, item, tree->vertical, FALSE);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemFL --
 *
 *	Return the first or last item in the same row or column
 *	as the given item.
 *
 * Results:
 *	First/last item or NULL if there is no such item.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

TreeItem
Tree_ItemFL(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item to use as a reference. */
    int vertical,		/* TRUE if items are arranged
				 * from top-to-bottom in each Range. */
    int first			/* TRUE for top/left,
				 * FALSE for bottom/right. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    RItem *rItem, *rItem2;
    Range *range;
    int i, l, u;

    if (!TreeItem_ReallyVisible(tree, item) || (tree->columnCountVis < 1)) {
	return NULL;
    }
    Range_RedoIfNeeded(tree);
    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
    if (vertical) {
	return (first ? rItem->range->first->item : rItem->range->last->item);
    } else {
	/* Find the first/last range */
	range = first ? dInfo->rangeFirst : dInfo->rangeLast;

	/* Check next/prev range until happy */
	while (1) {
	    if (range == rItem->range)
		return item;

	    /* Find item with same index */
	    /* Binary search */
	    l = 0;
	    u = range->last->index;
	    while (l <= u) {
		i = (l + u) / 2;
		rItem2 = range->first + i;
		if (rItem2->index == rItem->index)
		    return rItem2->item;
		if (rItem->index < rItem2->index)
		    u = i - 1;
		else
		    l = i + 1;
	    }

	    range = first ? range->next : range->prev;
	}
    }
    return NULL;
}

TreeItem
Tree_ItemTop(TreeCtrl *tree, TreeItem item)
{
    return Tree_ItemFL(tree, item, tree->vertical, TRUE);
}

TreeItem
Tree_ItemBottom(TreeCtrl *tree, TreeItem item)
{
    return Tree_ItemFL(tree, item, tree->vertical, FALSE);
}

TreeItem
Tree_ItemLeftMost(TreeCtrl *tree, TreeItem item)
{
    return Tree_ItemFL(tree, item, !tree->vertical, TRUE);
}

TreeItem
Tree_ItemRightMost(TreeCtrl *tree, TreeItem item)
{
    return Tree_ItemFL(tree, item, !tree->vertical, FALSE);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemToRNC --
 *
 *	Return the row and column for the given item.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

int
Tree_ItemToRNC(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item to get row n' column of. */
    int *row, int *col		/* Returned row and column. */
    )
{
    RItem *rItem;

    if (!TreeItem_ReallyVisible(tree, item) || (tree->columnCountVis < 1))
	return TCL_ERROR;
    Range_RedoIfNeeded(tree);
    rItem = (RItem *) TreeItem_GetRInfo(tree, item);
    if (tree->vertical) {
	(*row) = rItem->index;
	(*col) = rItem->range->index;
    }
    else {
	(*row) = rItem->range->index;
	(*col) = rItem->index;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_RNCToItem --
 *
 *	Return the item at a given row and column.
 *
 * Results:
 *	Token for the item. Never returns NULL unless there are no
 *	Ranges.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed.
 *
 *--------------------------------------------------------------
 */

TreeItem
Tree_RNCToItem(
    TreeCtrl *tree,		/* Widget info. */
    int row, int col		/* Row and column. These values are
				 * clipped to valid values. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range;
    RItem *rItem;
    int i, l, u;

    Range_RedoIfNeeded(tree);
    range = dInfo->rangeFirst;
    if (range == NULL)
	return NULL;
    if (row < 0)
	row = 0;
    if (col < 0)
	col = 0;
    if (tree->vertical) {
	if (col > dInfo->rangeLast->index)
	    col = dInfo->rangeLast->index;
	while (range->index != col)
	    range = range->next;
	rItem = range->last;
	if (row > rItem->index)
	    row = rItem->index;
	/* Binary search */
	l = 0;
	u = range->last->index;
	while (l <= u) {
	    i = (l + u) / 2;
	    rItem = range->first + i;
	    if (rItem->index == row)
		break;
	    if (row < rItem->index)
		u = i - 1;
	    else
		l = i + 1;
	}
    }
    else {
	if (row > dInfo->rangeLast->index)
	    row = dInfo->rangeLast->index;
	while (range->index != row)
	    range = range->next;
	rItem = range->last;
	if (col > rItem->index)
	    col = rItem->index;
	/* Binary search */
	l = 0;
	u = range->last->index;
	while (l <= u) {
	    i = (l + u) / 2;
	    rItem = range->first + i;
	    if (rItem->index == col)
		break;
	    if (col < rItem->index)
		u = i - 1;
	    else
		l = i + 1;
	}
    }
    return rItem->item;
}

/*=============*/

static void
DisplayDelay(TreeCtrl *tree)
{
    if (tree->debug.enable &&
	    tree->debug.display &&
	    tree->debug.displayDelay > 0) {
#if !defined(WIN32) && !defined(MAC_TCL) && !defined(MAC_OSX_TK)
	XSync(tree->display, False);
#endif
	Tcl_Sleep(tree->debug.displayDelay);
    }
}

/*
 *--------------------------------------------------------------
 *
 * DItem_Alloc --
 *
 *	Allocate and initialize a new DItem, and store a pointer to it
 *	in the given item.
 *
 * Results:
 *	Pointer to the DItem which may come from an existing pool of
 *	unused DItems.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static DItem *
DItem_Alloc(
    TreeCtrl *tree,		/* Widget info. */
    RItem *rItem		/* Range info for the item. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;

    dItem = (DItem *) TreeItem_GetDInfo(tree, rItem->item);
    if (dItem != NULL)
	panic("tried to allocate duplicate DItem");

    /* Pop unused DItem from stack */
    if (dInfo->dItemFree != NULL) {
	dItem = dInfo->dItemFree;
	dInfo->dItemFree = dItem->next;

    /* No free DItems, alloc a new one */
    } else {
	dItem = (DItem *) ckalloc(sizeof(DItem));
    }
    memset(dItem, '\0', sizeof(DItem));
    strncpy(dItem->magic, "MAGC", 4);
    dItem->item = rItem->item;
    dItem->flags = DITEM_DIRTY | DITEM_ALL_DIRTY;
    TreeItem_SetDInfo(tree, rItem->item, (TreeItemDInfo) dItem);
    return dItem;
}

/*
 *--------------------------------------------------------------
 *
 * DItem_Unlink --
 *
 *	Remove a DItem from a linked list of DItems.
 *
 * Results:
 *	Pointer to the given list of DItems.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static DItem *
DItem_Unlink(
    DItem *head,		/* First in linked list. */
    DItem *dItem		/* DItem to remove from list. */
    )
{
    DItem *prev;

    if (head == dItem)
	head = dItem->next;
    else {
	for (prev = head;
	     prev->next != dItem;
	     prev = prev->next) {
	    /* nothing */
	}
	prev->next = dItem->next;
    }
    dItem->next = NULL;
    return head;
}

/*
 *--------------------------------------------------------------
 *
 * DItem_Free --
 *
 *	Add a DItem to the pool of unused DItems. If the DItem belongs
 *	to a TreeItem the pointer to the DItem is set to NULL in that
 *	TreeItem.
 *
 * Results:
 *	Pointer to the next DItem.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static DItem *
DItem_Free(
    TreeCtrl *tree,		/* Widget info. */
    DItem *dItem		/* DItem to free. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *next = dItem->next;
    if (strncmp(dItem->magic, "MAGC", 4) != 0)
	panic("DItem_Free: dItem.magic != MAGC");
    if (dItem->item != NULL)
	TreeItem_SetDInfo(tree, dItem->item, (TreeItemDInfo) NULL);
    /* Push unused DItem on the stack */
    dItem->next = dInfo->dItemFree;
    dInfo->dItemFree = dItem;
    return next;
}

/*
 *--------------------------------------------------------------
 *
 * FreeDItems --
 *
 *	Add a list of DItems to the pool of unused DItems,
 *	optionally removing the DItems from the DInfo.dItem list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
FreeDItems(
    TreeCtrl *tree,		/* Widget info. */
    DItem *first,		/* First DItem to free. */
    DItem *last,		/* DItem after the last one to free. */
    int unlink			/* TRUE if the DItems should be removed
				 * from the DInfo.dItem list. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *prev;

    if (unlink) {
	if (dInfo->dItem == first)
	    dInfo->dItem = last;
	else {
	    for (prev = dInfo->dItem;
		 prev->next != first;
		 prev = prev->next) {
		/* nothing */
	    }
	    prev->next = last;
	}
    }
    while (first != last)
	first = DItem_Free(tree, first);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_ItemsInArea --
 *
 *	Return a list of items overlapping the given area.
 *
 * Results:
 *	Pointer to an allocated array of TreeItems which the caller
 *	must free. NULL is returned if no items overlap.
 *
 * Side effects:
 *	The list of Ranges will be recalculated if needed. Memory may
 *	be allocated.
 *
 *--------------------------------------------------------------
 */

TreeItem *
Tree_ItemsInArea(
    TreeCtrl *tree,		/* Widget info. */
    int minX, int minY,		/* Left, top in canvas coordinates. */
    int maxX, int maxY		/* Right, bottom in canvas coordinates.
				 * Points on the right/bottom edge are not
				 * included in the area. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    int x, y, rx = 0, ry = 0, ix, iy, dx, dy;
    Range *range;
    RItem *rItem;
    TreeItem *items;
    int count = 0, size = 25;

    Range_RedoIfNeeded(tree);
    range = dInfo->rangeFirst;

    if (tree->vertical) {
	/* Find the first range which could be in the area horizontally */
	while (range != NULL) {
	    if ((range->offset < maxX) &&
		    (range->offset + range->totalWidth >= minX)) {
		rx = range->offset;
		ry = 0;
		break;
	    }
	    range = range->next;
	}
    }
    else {
	/* Find the first range which could be in the area vertically */
	while (range != NULL) {
	    if ((range->offset < maxY) &&
		    (range->offset + range->totalHeight >= minY)) {
		rx = 0;
		ry = range->offset;
		break;
	    }
	    range = range->next;
	}
    }

    if (range == NULL)
	return NULL;

    items = (TreeItem *) ckalloc(sizeof(TreeItem) * size);

    while (range != NULL) {
	if ((rx + range->totalWidth > minX) &&
		(ry + range->totalHeight > minY)) {
	    if (tree->vertical) {
		/* Range coords */
		dx = MAX(minX - rx, 0);
		dy = minY;
	    }
	    else {
		dx = minX;
		dy = MAX(minY - ry, 0);
	    }
	    ix = dx;
	    iy = dy;
	    rItem = Range_ItemUnderPoint(tree, range, &ix, &iy);

	    /* Canvas coords of top-left of item */
	    x = rx + dx - ix;
	    y = ry + dy - iy;

	    while (1) {
		if (count + 1 > size) {
		    size *= 2;
		    items = (TreeItem *) ckrealloc((char *) items, sizeof(TreeItem) * size);
		}
		items[count++] = rItem->item;
		if (tree->vertical) {
		    y += rItem->size;
		    if (y >= maxY)
			break;
		}
		else {
		    x += rItem->size;
		    if (x >= maxX)
			break;
		}
		if (rItem == range->last)
		    break;
		rItem++;
	    }
	}
	if (tree->vertical) {
	    if (rx + range->totalWidth >= maxX)
		break;
	    rx += range->totalWidth;
	}
	else {
	    if (ry + range->totalHeight >= maxY)
		break;
	    ry += range->totalHeight;
	}
	range = range->next;
    }

    /* NULL terminator */
    if (count + 1 > size) {
	size += 1;
	items = (TreeItem *) ckrealloc((char *) items, sizeof(TreeItem) * size);
    }
    items[count++] = NULL;

    return items;
}

/*
 *--------------------------------------------------------------
 *
 * UpdateDInfoForRange --
 *
 *	Allocates or updates a DItem for every on-screen item in a Range.
 *	If an item already has a DItem (because the item was previously
 *	displayed), then the DItem may be marked dirty if there were
 *	changes to the item's on-screen size or position.
 *
 * Results:
 *	The return value is the possibly-updated dItemHead.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static DItem *
UpdateDInfoForRange(
    TreeCtrl *tree,		/* Widget info. */
    DItem *dItemHead,		/* Linked list of used DItems. */
    Range *range,		/* Range to update DItems for. */
    RItem *rItem,		/* First item in the Range we care about. */
    int x, int y		/* Left & top window coordinates of rItem. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;
    TreeItem item;
    int maxX, maxY;
    int index, indexVis;
    int bgImgWidth, bgImgHeight;

    if (tree->backgroundImage != NULL)
	Tk_SizeOfImage(tree->backgroundImage, &bgImgWidth, &bgImgHeight);

    maxX = Tk_Width(tree->tkwin) - tree->inset;
    maxY = Tk_Height(tree->tkwin) - tree->inset;

    /* Top-to-bottom */
    if (tree->vertical) {
	while (1) {
	    item = rItem->item;

	    /* Update item/style layout. This can be needed when using fixed
	     * column widths. */
	    (void) TreeItem_Height(tree, item);

	    TreeItem_ToIndex(tree, item, &index, &indexVis);
	    switch (tree->backgroundMode) {
		case BG_MODE_INDEX:
		case BG_MODE_ORDER: break;
		case BG_MODE_ORDERVIS:
		case BG_MODE_VISINDEX: index = indexVis; break;
		case BG_MODE_COLUMN: index = range->index; break;
		case BG_MODE_ROW: index = rItem->index; break;
	    }

	    dItem = (DItem *) TreeItem_GetDInfo(tree, item);

	    /* Re-use a previously allocated DItem */
	    if (dItem != NULL) {
		dItemHead = DItem_Unlink(dItemHead, dItem);

		/* This item is already marked for total redraw */
		if (dItem->flags & DITEM_ALL_DIRTY)
		    ; /* nothing */

		/* All display info is marked as invalid */
		else if (dInfo->flags & DINFO_INVALIDATE)
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* The range may have changed size */
		else if ((dItem->width != range->totalWidth) ||
			(dItem->height != rItem->size))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* Items may have alternating background colors. */
		else if ((tree->columnBgCnt > 1) &&
			((index % tree->columnBgCnt) !=
				(dItem->index % tree->columnBgCnt)))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We don't copy items horizontally to their new position,
		 * except for horizontal scrolling which moves the whole
		 * range */
		else if (x != dItem->oldX + (dInfo->xOrigin - tree->xOrigin))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* If we are displaying dotted lines and the item has moved
		 * from odd-top to non-odd-top or vice versa, must redraw
		 * the lines for this item. */
		else if (tree->showLines &&
			(tree->lineStyle == LINE_STYLE_DOT) &&
			tree->columnTreeVis &&
			((dItem->oldY & 1) != (y & 1)))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We can't copy the item to its new position unless it
		 * has the same part of the background image behind it */
		else if ((tree->backgroundImage != NULL) &&
			(((dItem->oldY + dInfo->yOrigin) % bgImgHeight) !=
				((y + tree->yOrigin) % bgImgHeight)))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
	    }

	    /* Make a new DItem */
	    else {
		dItem = DItem_Alloc(tree, rItem);
	    }

	    dItem->x = x;
	    dItem->y = y;
	    dItem->width = Range_TotalWidth(tree, range);
	    dItem->height = rItem->size;
	    dItem->range = range;
	    dItem->index = index;

	    /* Keep track of the maximum item size */
	    if (dItem->width > dInfo->itemWidth)
		dInfo->itemWidth = dItem->width;
	    if (dItem->height > dInfo->itemHeight)
		dInfo->itemHeight = dItem->height;

	    /* Linked list of DItems */
	    if (dInfo->dItem == NULL)
		dInfo->dItem = dItem;
	    else
		dInfo->dItemLast->next = dItem;
	    dInfo->dItemLast = dItem;

	    if (rItem == range->last)
		break;

	    /* Advance to next TreeItem */
	    rItem++;

	    /* Stop when out of bounds */
	    y += dItem->height;
	    if (y >= maxY)
		break;
	}
    }

    /* Left-to-right */
    else {
	while (1) {
	    item = rItem->item;

	    /* Update item/style layout. This can be needed when using fixed
	     * column widths. */
	    (void) TreeItem_Height(tree, item);

	    TreeItem_ToIndex(tree, item, &index, &indexVis);
	    switch (tree->backgroundMode) {
		case BG_MODE_INDEX:
		case BG_MODE_ORDER: break;
		case BG_MODE_ORDERVIS:
		case BG_MODE_VISINDEX: index = indexVis; break;
		case BG_MODE_COLUMN: index = rItem->index; break;
		case BG_MODE_ROW: index = range->index; break;
	    }

	    dItem = (DItem *) TreeItem_GetDInfo(tree, item);

	    /* Re-use a previously allocated DItem */
	    if (dItem != NULL) {
		dItemHead = DItem_Unlink(dItemHead, dItem);

		/* This item is already marked for total redraw */
		if (dItem->flags & DITEM_ALL_DIRTY)
		    ; /* nothing */

		/* All display info is marked as invalid */
		else if (dInfo->flags & DINFO_INVALIDATE)
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* The range may have changed size */
		else if ((dItem->width != rItem->size) ||
			(dItem->height != range->totalHeight))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* Items may have alternating background colors. */
		else if ((tree->columnBgCnt > 1) && ((index % tree->columnBgCnt) !=
				 (dItem->index % tree->columnBgCnt)))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We don't copy items vertically to their new position,
		 * except for vertical scrolling which moves the whole range */
		else if (y != dItem->oldY + (dInfo->yOrigin - tree->yOrigin))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* If we are displaying dotted lines and the item has moved
		 * from odd-top to non-odd-top or vice versa, must redraw
		 * the lines for this item. */
		else if (tree->showLines &&
			(tree->lineStyle == LINE_STYLE_DOT) &&
			tree->columnTreeVis &&
			((dItem->oldY & 1) != (y & 1)))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;

		/* We can't copy the item to its new position unless it
		 * has the same part of the background image behind it */
		else if ((tree->backgroundImage != NULL) &&
			(((dItem->oldX + dInfo->xOrigin) % bgImgWidth) !=
				((x + tree->xOrigin) % bgImgWidth)))
		    dItem->flags |= DITEM_DIRTY | DITEM_ALL_DIRTY;
	    }

	    /* Make a new DItem */
	    else {
		dItem = DItem_Alloc(tree, rItem);
	    }

	    dItem->x = x;
	    dItem->y = y;
	    dItem->width = rItem->size;
	    dItem->height = Range_TotalHeight(tree, range);
	    dItem->range = range;
	    dItem->index = index;

	    /* Keep track of the maximum item size */
	    if (dItem->width > dInfo->itemWidth)
		dInfo->itemWidth = dItem->width;
	    if (dItem->height > dInfo->itemHeight)
		dInfo->itemHeight = dItem->height;

	    /* Linked list of DItems */
	    if (dInfo->dItem == NULL)
		dInfo->dItem = dItem;
	    else
		dInfo->dItemLast->next = dItem;
	    dInfo->dItemLast = dItem;

	    if (rItem == range->last)
		break;

	    /* Advance to next TreeItem */
	    rItem++;

	    /* Stop when out of bounds */
	    x += dItem->width;
	    if (x >= maxX)
		break;
	}
    }

    return dItemHead;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_UpdateDInfo --
 *
 *	At the finish of this procedure every on-screen item will have
 *	a DItem associated with it and no off-screen item will have
 *	a DItem.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

void
Tree_UpdateDInfo(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItemHead = dInfo->dItem;
    int x, y, rx = 0, ry = 0, ix, iy, dx, dy;
    int minX, minY, maxX, maxY;
    Range *range;
    RItem *rItem;

    if (tree->debug.enable && tree->debug.display)
	dbwin("Tree_UpdateDInfo %s\n", Tk_PathName(tree->tkwin));

    minX = tree->inset;
    minY = tree->inset + Tree_HeaderHeight(tree);
    maxX = Tk_Width(tree->tkwin) - tree->inset;
    maxY = Tk_Height(tree->tkwin) - tree->inset;

    if ((maxX - minX < 1) || (maxY - minY < 1))
	goto done;

    range = dInfo->rangeFirst;
    if (tree->vertical) {
	/* Find the first range which could be onscreen horizontally.
	 * It may not be onscreen if it has less height than other ranges. */
	while (range != NULL) {
	    if ((range->offset < maxX + tree->xOrigin) &&
		    (range->offset + range->totalWidth >= minX + tree->xOrigin)) {
		rx = range->offset;
		ry = 0;
		break;
	    }
	    range = range->next;
	}
    }
    else {
	/* Find the first range which could be onscreen vertically.
	 * It may not be onscreen if it has less width than other ranges. */
	while (range != NULL) {
	    if ((range->offset < maxY + tree->yOrigin) &&
		    (range->offset + range->totalHeight >= minY + tree->yOrigin)) {
		rx = 0;
		ry = range->offset;
		break;
	    }
	    range = range->next;
	}
    }

    dInfo->dItem = dInfo->dItemLast = NULL;
    dInfo->rangeFirstD = dInfo->rangeLastD = NULL;
    dInfo->itemWidth = dInfo->itemHeight = 0;

    while (range != NULL) {
	if ((rx + range->totalWidth > minX + tree->xOrigin) &&
		(ry + range->totalHeight > minY + tree->yOrigin)) {
	    if (tree->vertical) {
		/* Range coords */
		dx = MAX(minX + tree->xOrigin - rx, 0);
		dy = minY + tree->yOrigin;
	    }
	    else {
		dx = minX + tree->xOrigin;
		dy = MAX(minY + tree->yOrigin - ry, 0);
	    }
	    ix = dx;
	    iy = dy;
	    rItem = Range_ItemUnderPoint(tree, range, &ix, &iy);

	    /* Window coords of top-left of item */
	    x = (rx - tree->xOrigin) + dx - ix;
	    y = (ry - tree->yOrigin) + dy - iy;
	    dItemHead = UpdateDInfoForRange(tree, dItemHead, range, rItem, x, y);
	}

	/* Track this range even if it has no DItems, so whitespace is
	 * erased */
	if (dInfo->rangeFirstD == NULL)
	    dInfo->rangeFirstD = range;
	dInfo->rangeLastD = range;

	if (tree->vertical) {
	    rx += range->totalWidth;
	    if (rx >= maxX + tree->xOrigin)
		break;
	}
	else {
	    ry += range->totalHeight;
	    if (ry >= maxY + tree->yOrigin)
		break;
	}
	range = range->next;
    }

    if (dInfo->dItemLast != NULL)
	dInfo->dItemLast->next = NULL;
    done:
    while (dItemHead != NULL)
	dItemHead = DItem_Free(tree, dItemHead);

    dInfo->flags &= ~DINFO_INVALIDATE;
}

/*
 *--------------------------------------------------------------
 *
 * InvalidateDItemX --
 *
 *	Mark a horizontal span of a DItem as dirty (needing to be
 *	redrawn). The caller must set the DITEM_DIRTY flag afterwards.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
InvalidateDItemX(
    DItem *dItem,		/* Item to mark dirty. */
    int itemX,			/* x-coordinate of item. */
    int dirtyX,			/* Left edge of area to mark as dirty. */
    int dirtyWidth		/* Width of area to mark as dirty. */
    )
{
    int x1, x2;

    if (dirtyX <= itemX)
	dItem->dirty[LEFT] = 0;
    else {
	x1 = dirtyX - itemX;
	if (!(dItem->flags & DITEM_DIRTY) || (x1 < dItem->dirty[LEFT]))
	    dItem->dirty[LEFT] = x1;
    }

    if (dirtyX + dirtyWidth >= itemX + dItem->width)
	dItem->dirty[RIGHT] = dItem->width;
    else {
	x2 = dirtyX + dirtyWidth - itemX;
	if (!(dItem->flags & DITEM_DIRTY) || (x2 > dItem->dirty[RIGHT]))
	    dItem->dirty[RIGHT] = x2;
    }
}

/*
 *--------------------------------------------------------------
 *
 * InvalidateDItemY --
 *
 *	Mark a vertical span of a DItem as dirty (needing to be
 *	redrawn). The caller must set the DITEM_DIRTY flag afterwards.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
InvalidateDItemY(
    DItem *dItem,		/* Item to mark dirty. */
    int itemY,			/* y-coordinate of item. */
    int dirtyY,			/* Top edge of area to mark as dirty. */
    int dirtyHeight		/* Height of area to mark as dirty. */
    )
{
    int y1, y2;

    if (dirtyY <= itemY)
	dItem->dirty[TOP] = 0;
    else {
	y1 = dirtyY - itemY;
	if (!(dItem->flags & DITEM_DIRTY) || (y1 < dItem->dirty[TOP]))
	    dItem->dirty[TOP] = y1;
    }

    if (dirtyY + dirtyHeight >= itemY + dItem->height)
	dItem->dirty[BOTTOM] = dItem->height;
    else {
	y2 = dirtyY + dirtyHeight - itemY;
	if (!(dItem->flags & DITEM_DIRTY) || (y2 > dItem->dirty[BOTTOM]))
	    dItem->dirty[BOTTOM] = y2;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Range_RedoIfNeeded --
 *
 *	Recalculate the list of Ranges if they are marked out-of-date.
 *	Also calculate the height and width of the canvas based on the
 *	list of Ranges.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *--------------------------------------------------------------
 */

static void
Range_RedoIfNeeded(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    if (dInfo->flags & DINFO_REDO_RANGES) {
	dInfo->rangeFirstD = dInfo->rangeLastD = NULL;
	dInfo->flags |= DINFO_OUT_OF_DATE;
	Range_Redo(tree);
	dInfo->flags &= ~DINFO_REDO_RANGES;

	/* Do this after clearing REDO_RANGES to prevent infinite loop */
	tree->totalWidth = tree->totalHeight = -1;
	(void) Tree_TotalWidth(tree);
	(void) Tree_TotalHeight(tree);
	dInfo->flags |= DINFO_REDO_INCREMENTS;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ScrollVerticalComplex --
 *
 *	Perform scrolling by copying the pixels of items from the
 *	previous display position to the current position. Any areas
 *	of items copied over by the moved items are marked dirty.
 *
 * Results:
 *	The number of items whose pixels were copied.
 *
 * Side effects:
 *	Pixels are copied in the TreeCtrl window or in the
 *	offscreen pixmap (if double-buffering is used).
 *
 *--------------------------------------------------------------
 */

static int
ScrollVerticalComplex(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem, *dItem2;
    Range *range;
    TkRegion damageRgn;
    int minX, minY, maxX, maxY;
    int oldX, oldY, width, height, offset;
    int y;
    int numCopy = 0;

    minX = tree->inset;
    maxX = Tk_Width(tree->tkwin) - tree->inset;
    minY = tree->inset + Tree_HeaderHeight(tree);
    maxY = Tk_Height(tree->tkwin) - tree->inset;

    /* Try updating the display by copying items on the screen to their
     * new location */
    for (dItem = dInfo->dItem;
	 dItem != NULL;
	 dItem = dItem->next) {
	/* Copy an item to its new location unless:
	 * (a) item display info is invalid
	 * (b) item is in same location as last draw */
	if ((dItem->flags & DITEM_ALL_DIRTY) ||
		(dItem->oldY == dItem->y))
	    continue;

	numCopy++;

	range = dItem->range;

	/* This item was previously displayed so it only needs to be
	 * copied to the new location. Copy all such items as one */
	offset = dItem->y - dItem->oldY;
	height = dItem->height;
	for (dItem2 = dItem->next;
	     dItem2 != NULL;
	     dItem2 = dItem2->next) {
	    if ((dItem2->range != range) ||
		    (dItem2->flags & DITEM_ALL_DIRTY) ||
		    (dItem2->oldY + offset != dItem2->y))
		break;
	    numCopy++;
	    height += dItem2->height;
	}

	y = dItem->y;
	oldY = dItem->oldY;

	/* Don't copy part of the window border */
	if (oldY < minY) {
	    height -= minY - oldY;
	    oldY = minY;
	}
	if (oldY + height > maxY)
	    height = maxY - oldY;

	/* Don't copy over the window border */
	if (oldY + offset < minY) {
	    height -= minY - (oldY + offset);
	    oldY += minY - (oldY + offset);
	}
	if (oldY + offset + height > maxY)
	    height = maxY - (oldY + offset);

	oldX = dItem->oldX;
	width = dItem->width;
	if (oldX < minX) {
	    width -= minX - oldX;
	    oldX = minX;
	}
	if (oldX + width > maxX)
	    width = maxX - oldX;

	/* Update oldY of copied items */
	while (1) {
	    /* If an item was partially visible, invalidate the exposed area */
	    if ((dItem->oldY < minY) && (offset > 0)) {
		InvalidateDItemX(dItem, oldX, oldX, width);
		InvalidateDItemY(dItem, dItem->oldY, dItem->oldY, minY - dItem->oldY);
		dItem->flags |= DITEM_DIRTY;
	    }
	    if ((dItem->oldY + dItem->height > maxY) && (offset < 0)) {
		InvalidateDItemX(dItem, oldX, oldX, width);
		InvalidateDItemY(dItem, dItem->oldY, maxY, maxY - dItem->oldY + dItem->height);
		dItem->flags |= DITEM_DIRTY;
	    }

	    dItem->oldY = dItem->y;
	    if (dItem->next == dItem2)
		break;
	    dItem = dItem->next;
	}

	/* Invalidate parts of items being copied over */
	for ( ; dItem2 != NULL; dItem2 = dItem2->next) {
	    if (dItem2->range != range)
		break;
	    if (!(dItem2->flags & DITEM_ALL_DIRTY) &&
		    (dItem2->oldY + dItem2->height > y) &&
		    (dItem2->oldY < y + height)) {
		InvalidateDItemX(dItem2, oldX, oldX, width);
		InvalidateDItemY(dItem2, dItem2->oldY, y, height);
		dItem2->flags |= DITEM_DIRTY;
	    }
	}

	if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	    int dirtyMin, dirtyMax;
	    XCopyArea(tree->display, dInfo->pixmap, dInfo->pixmap,
		    tree->copyGC,
		    oldX, oldY, width, height,
		    oldX, oldY + offset);
	    if (offset < 0) {
		dirtyMin = oldY + offset + height;
		dirtyMax = oldY + height;
	    } else {
		dirtyMin = oldY;
		dirtyMax = oldY + offset;
	    }
	    Tree_InvalidateArea(tree, oldX, dirtyMin, oldX + width, dirtyMax);
	    dInfo->dirty[LEFT] = MIN(dInfo->dirty[LEFT], oldX);
	    dInfo->dirty[TOP] = MIN(dInfo->dirty[TOP], oldY + offset);
	    dInfo->dirty[RIGHT] = MAX(dInfo->dirty[RIGHT], oldX + width);
	    dInfo->dirty[BOTTOM] = MAX(dInfo->dirty[BOTTOM], oldY + offset + height);
	    continue;
	}

	/* Copy */
	damageRgn = TkCreateRegion();
	if (Tree_ScrollWindow(tree, dInfo->scrollGC,
		    oldX, oldY, width, height, 0, offset, damageRgn)) {
	    DisplayDelay(tree);
	    Tree_InvalidateRegion(tree, damageRgn);
	}
	TkDestroyRegion(damageRgn);
    }
    return numCopy;
}

/*
 *--------------------------------------------------------------
 *
 * ScrollHorizontalSimple --
 *
 *	Perform scrolling by shifting the pixels in the content area of
 *	the list to the left or right.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is copied/scrolled in the TreeCtrl window or in the
 *	offscreen pixmap (if double-buffering is used).
 *
 *--------------------------------------------------------------
 */

static void
ScrollHorizontalSimple(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;
    TkRegion rgn, damageRgn;
    XRectangle rect;
    int minX, minY, maxX, maxY;
    int width, offset;
    int x, y;

    minX = tree->inset;
    maxX = Tk_Width(tree->tkwin) - tree->inset;
    minY = tree->inset + Tree_HeaderHeight(tree);
    maxY = Tk_Height(tree->tkwin) - tree->inset;

    /* Horizontal scrolling */
    if (dInfo->xOrigin != tree->xOrigin) {
	int dirtyMin, dirtyMax;

	offset = dInfo->xOrigin - tree->xOrigin;
	width = maxX - minX - abs(offset);
	if (width < 0)
	    width = maxX - minX;
	/* Move pixels right */
	if (offset > 0) {
	    x = minX;
	}
	/* Move pixels left */
	else {
	    x = maxX - width;
	}

	dirtyMin = minX + width;
	dirtyMax = maxX - width;

	/* Update oldX */
	for (dItem = dInfo->dItem;
	     dItem != NULL;
	     dItem = dItem->next) {
	    dItem->oldX = dItem->x;
	}

	damageRgn = TkCreateRegion();

	/* Offset and clip the whitespace region */
	Tk_OffsetRegion(dInfo->wsRgn, offset, 0);
	rgn = TkCreateRegion();
	rect.x = minX;
	rect.y = minY;
	rect.width = maxX - minX;
	rect.height = maxY - minY;
	TkUnionRectWithRegion(&rect, rgn, damageRgn);
	TkIntersectRegion(dInfo->wsRgn, damageRgn, dInfo->wsRgn);
	TkDestroyRegion(rgn);

	/* We only scroll the content, not the whitespace */
	y = 0 - tree->yOrigin + Tree_TotalHeight(tree);
	if (y < maxY)
	    maxY = y;

	if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	    XCopyArea(tree->display, dInfo->pixmap, dInfo->pixmap,
		    tree->copyGC,
		    x, minY, width, maxY - minY,
		    x + offset, minY);
	    if (offset < 0)
		dirtyMax = maxX;
	    else
		dirtyMin = minX;
	    Tree_InvalidateArea(tree, dirtyMin, minY, dirtyMax, maxY);
	    TkDestroyRegion(damageRgn);
	    return;
	}

	if (Tree_ScrollWindow(tree, dInfo->scrollGC,
		    x, minY, width, maxY - minY, offset, 0, damageRgn)) {
	    DisplayDelay(tree);
	    Tree_InvalidateRegion(tree, damageRgn);
	}
	TkDestroyRegion(damageRgn);
#ifndef WIN32
	if (offset < 0)
	    dirtyMax = maxX;
	else
	    dirtyMin = minX;
#endif
	if (dirtyMin < dirtyMax)
	    Tree_InvalidateArea(tree, dirtyMin, minY, dirtyMax, maxY);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ScrollVerticalSimple --
 *
 *	Perform scrolling by shifting the pixels in the content area of
 *	the list up or down.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is copied/scrolled in the TreeCtrl window or in the
 *	offscreen pixmap (if double-buffering is used).
 *
 *--------------------------------------------------------------
 */

static void
ScrollVerticalSimple(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;
    TkRegion rgn, damageRgn;
    XRectangle rect;
    int minX, minY, maxX, maxY;
    int height, offset;
    int x, y;

    minX = tree->inset;
    maxX = Tk_Width(tree->tkwin) - tree->inset;
    minY = tree->inset + Tree_HeaderHeight(tree);
    maxY = Tk_Height(tree->tkwin) - tree->inset;

    /* Vertical scrolling */
    if (dInfo->yOrigin != tree->yOrigin) {
	int dirtyMin, dirtyMax;

	offset = dInfo->yOrigin - tree->yOrigin;
	height = maxY - minY - abs(offset);
	if (height < 0)
	    height = maxY - minY;
	/* Move pixels down */
	if (offset > 0) {
	    y = minY;
	}
	/* Move pixels up */
	else {
	    y = maxY - height;
	}

	dirtyMin = minY + height;
	dirtyMax = maxY - height;

	/* Update oldY */
	for (dItem = dInfo->dItem;
	     dItem != NULL;
	     dItem = dItem->next) {
	    dItem->oldY = dItem->y;
	}

	damageRgn = TkCreateRegion();

	/* Offset and clip the whitespace region */
	Tk_OffsetRegion(dInfo->wsRgn, 0, offset);
	rgn = TkCreateRegion();
	rect.x = minX;
	rect.y = minY;
	rect.width = maxX - minX;
	rect.height = maxY - minY;
	TkUnionRectWithRegion(&rect, rgn, damageRgn);
	TkIntersectRegion(dInfo->wsRgn, damageRgn, dInfo->wsRgn);
	TkDestroyRegion(rgn);

	/* We only scroll the content, not the whitespace */
	x = 0 - tree->xOrigin + Tree_TotalWidth(tree);
	if (x < maxX)
	    maxX = x;

	if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	    XCopyArea(tree->display, dInfo->pixmap, dInfo->pixmap,
		    tree->copyGC,
		    minX, y, maxX - minX, height,
		    minX, y + offset);
	    if (offset < 0)
		dirtyMax = maxY;
	    else
		dirtyMin = minY;
	    Tree_InvalidateArea(tree, minX, dirtyMin, maxX, dirtyMax);
	    TkDestroyRegion(damageRgn);
	    return;
	}

	if (Tree_ScrollWindow(tree, dInfo->scrollGC,
		    minX, y, maxX - minX, height, 0, offset, damageRgn)) {
	    DisplayDelay(tree);
	    Tree_InvalidateRegion(tree, damageRgn);
	}
	TkDestroyRegion(damageRgn);
#ifndef WIN32
	if (offset < 0)
	    dirtyMax = maxY;
	else
	    dirtyMin = minY;
#endif
	if (dirtyMin < dirtyMax)
	    Tree_InvalidateArea(tree, minX, dirtyMin, maxX, dirtyMax);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ScrollHorizontalComplex --
 *
 *	Perform scrolling by copying the pixels of items from the
 *	previous display position to the current position. Any areas
 *	of items copied over by the moved items are marked dirty.
 *
 * Results:
 *	The number of items whose pixels were copied.
 *
 * Side effects:
 *	Pixels are copied in the TreeCtrl window or in the
 *	offscreen pixmap (if double-buffering is used).
 *
 *--------------------------------------------------------------
 */

static int
ScrollHorizontalComplex(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem, *dItem2;
    Range *range;
    TkRegion damageRgn;
    int minX, minY, maxX, maxY;
    int oldX, oldY, width, height, offset;
    int x;
    int numCopy = 0;

    minX = tree->inset;
    maxX = Tk_Width(tree->tkwin) - tree->inset;
    minY = tree->inset + Tree_HeaderHeight(tree);
    maxY = Tk_Height(tree->tkwin) - tree->inset;

    /* Try updating the display by copying items on the screen to their
     * new location */
    for (dItem = dInfo->dItem;
	 dItem != NULL;
	 dItem = dItem->next) {
	/* Copy an item to its new location unless:
	 * (a) item display info is invalid
	 * (b) item is in same location as last draw */
	if ((dItem->flags & DITEM_ALL_DIRTY) ||
		(dItem->oldX == dItem->x))
	    continue;

	numCopy++;

	range = dItem->range;

	/* This item was previously displayed so it only needs to be
	 * copied to the new location. Copy all such items as one */
	offset = dItem->x - dItem->oldX;
	width = dItem->width;
	for (dItem2 = dItem->next;
	     dItem2 != NULL;
	     dItem2 = dItem2->next) {
	    if ((dItem2->range != range) ||
		    (dItem2->flags & DITEM_ALL_DIRTY) ||
		    (dItem2->oldX + offset != dItem2->x))
		break;
	    numCopy++;
	    width += dItem2->width;
	}

	x = dItem->x;
	oldX = dItem->oldX;

	/* Don't copy part of the window border */
	if (oldX < minX) {
	    width -= minX - oldX;
	    oldX = minX;
	}
	if (oldX + width > maxX)
	    width = maxX - oldX;

	/* Don't copy over the window border */
	if (oldX + offset < minX) {
	    width -= minX - (oldX + offset);
	    oldX += minX - (oldX + offset);
	}
	if (oldX + offset + width > maxX)
	    width = maxX - (oldX + offset);

	oldY = dItem->oldY;
	height = dItem->height; /* range->totalHeight */
	if (oldY < minY) {
	    height -= minY - oldY;
	    oldY = minY;
	}
	if (oldY + height > maxY)
	    height = maxY - oldY;

	/* Update oldX of copied items */
	while (1) {
	    /* If an item was partially visible, invalidate the exposed area */
	    if ((dItem->oldX < minX) && (offset > 0)) {
		InvalidateDItemX(dItem, dItem->oldX, dItem->oldX, minX - dItem->oldX);
		InvalidateDItemY(dItem, oldY, oldY, height);
		dItem->flags |= DITEM_DIRTY;
	    }
	    if ((dItem->oldX + dItem->width > maxX) && (offset < 0)) {
		InvalidateDItemX(dItem, dItem->oldX, maxX, maxX - dItem->oldX + dItem->width);
		InvalidateDItemY(dItem, oldY, oldY, height);
		dItem->flags |= DITEM_DIRTY;
	    }

	    dItem->oldX = dItem->x;
	    if (dItem->next == dItem2)
		break;
	    dItem = dItem->next;
	}

	/* Invalidate parts of items being copied over */
	for ( ; dItem2 != NULL; dItem2 = dItem2->next) {
	    if (dItem2->range != range)
		break;
	    if (!(dItem2->flags & DITEM_ALL_DIRTY) &&
		    (dItem2->oldX + dItem2->width > x) &&
		    (dItem2->oldX < x + width)) {
		InvalidateDItemX(dItem2, dItem2->oldX, x, width);
		InvalidateDItemY(dItem2, oldY, oldY, height);
		dItem2->flags |= DITEM_DIRTY;
	    }
	}

	if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	    int dirtyMin, dirtyMax;
	    XCopyArea(tree->display, dInfo->pixmap, dInfo->pixmap,
		    tree->copyGC,
		    oldX, oldY, width, height,
		    oldX + offset, oldY);
	    if (offset < 0) {
		dirtyMin = oldX + offset + width;
		dirtyMax = oldX + width;
	    } else {
		dirtyMin = oldX;
		dirtyMax = oldX + offset;
	    }
	    Tree_InvalidateArea(tree, dirtyMin, oldY, dirtyMax, oldY + height);
	    dInfo->dirty[LEFT] = MIN(dInfo->dirty[LEFT], oldX + offset);
	    dInfo->dirty[TOP] = MIN(dInfo->dirty[TOP], oldY);
	    dInfo->dirty[RIGHT] = MAX(dInfo->dirty[RIGHT], oldX + offset + width);
	    dInfo->dirty[BOTTOM] = MAX(dInfo->dirty[BOTTOM], oldY + height);
	    continue;
	}

	/* Copy */
	damageRgn = TkCreateRegion();
	if (Tree_ScrollWindow(tree, dInfo->scrollGC,
		    oldX, oldY, width, height, offset, 0, damageRgn)) {
	    DisplayDelay(tree);
	    Tree_InvalidateRegion(tree, damageRgn);
	}
	TkDestroyRegion(damageRgn);
    }
    return numCopy;
}

/*
 *--------------------------------------------------------------
 *
 * TreeColumnProxy_Draw --
 *
 *	Draw (or erase) the visual indicator used when the user is
 *	resizing a column (and -columnresizemode is "proxy").
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in the TreeCtrl window (or erased, since this
 *	is XOR drawing).
 *
 *--------------------------------------------------------------
 */

void
TreeColumnProxy_Draw(
    TreeCtrl *tree		/* Widget info. */
    )
{
#if defined(MAC_OSX_TK)
    DrawXORLine(tree->display, Tk_WindowId(tree->tkwin),
	    tree->columnProxy.sx,
	    tree->inset,
	    tree->columnProxy.sx,
	    Tk_Height(tree->tkwin) - tree->inset);
#else
    XGCValues gcValues;
    unsigned long gcMask;
    GC gc;

#if defined(MAC_TCL)
    gcValues.function = GXxor;
#else
    gcValues.function = GXinvert;
#endif
    gcValues.graphics_exposures = False;
    gcMask = GCFunction | GCGraphicsExposures;
    gc = Tk_GetGC(tree->tkwin, gcMask, &gcValues);

    /* GXinvert doesn't work with XFillRectangle() on Win32 or Mac */
#if defined(WIN32) || defined(MAC_TCL)
    XDrawLine(tree->display, Tk_WindowId(tree->tkwin), gc,
	    tree->columnProxy.sx,
	    tree->inset,
	    tree->columnProxy.sx,
	    Tk_Height(tree->tkwin) - tree->inset);
#else
    XFillRectangle(tree->display, Tk_WindowId(tree->tkwin), gc,
	    tree->columnProxy.sx,
	    tree->inset,
	    1,
	    Tk_Height(tree->tkwin) - tree->inset * 2);
#endif

    Tk_FreeGC(tree->display, gc);
#endif
}

/*
 *--------------------------------------------------------------
 *
 * TreeColumnProxy_Undisplay --
 *
 *	Hide the visual indicator used when the user is
 *	resizing a column (if it is displayed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is erased in the TreeCtrl window.
 *
 *--------------------------------------------------------------
 */

void
TreeColumnProxy_Undisplay(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (tree->columnProxy.onScreen) {
	TreeColumnProxy_Draw(tree);
	tree->columnProxy.onScreen = FALSE;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeColumnProxy_Display --
 *
 *	Display the visual indicator used when the user is
 *	resizing a column (if it isn't displayed and should be
 *	displayed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in the TreeCtrl window.
 *
 *--------------------------------------------------------------
 */

void
TreeColumnProxy_Display(
    TreeCtrl *tree		/* Widget info. */
    )
{
    if (!tree->columnProxy.onScreen && (tree->columnProxy.xObj != NULL)) {
	tree->columnProxy.sx = tree->columnProxy.x;
	TreeColumnProxy_Draw(tree);
	tree->columnProxy.onScreen = TRUE;
    }
}

/*
 *--------------------------------------------------------------
 *
 * CalcWhiteSpaceRegion --
 *
 *	Create a new region containing all the whitespace of the list
 *	The whitespace is the area inside the borders/header where items
 *	are not displayed.
 *
 * Results:
 *	The new whitespace region, which may be empty.
 *
 * Side effects:
 *	A new region is allocated.
 *
 *--------------------------------------------------------------
 */

static TkRegion
CalcWhiteSpaceRegion(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    int x, y, minX, minY, maxX, maxY;
    int left, right, top, bottom;
    TkRegion wsRgn;
    XRectangle rect;
    Range *range;

    x = 0 - tree->xOrigin;
    y = 0 - tree->yOrigin;

    minX = tree->inset;
    maxX = Tk_Width(tree->tkwin) - tree->inset;
    minY = tree->inset + Tree_HeaderHeight(tree);
    maxY = Tk_Height(tree->tkwin) - tree->inset;

    wsRgn = TkCreateRegion();

    if (tree->vertical) {
	/* Erase area to right of last Range */
	if (x + Tree_TotalWidth(tree) < maxX) {
	    rect.x = x + Tree_TotalWidth(tree);
	    rect.y = minY;
	    rect.width = maxX - (x + Tree_TotalWidth(tree));
	    rect.height = maxY - minY;
	    TkUnionRectWithRegion(&rect, wsRgn, wsRgn);
	}
    }
    else {
	/* Erase area below last Range */
	if (y + Tree_TotalHeight(tree) < maxY) {
	    rect.x = tree->inset;
	    rect.y = y + Tree_TotalHeight(tree);
	    rect.width = maxX - minX;
	    rect.height = maxY - (y + Tree_TotalHeight(tree));
	    TkUnionRectWithRegion(&rect, wsRgn, wsRgn);
	}
    }

    for (range = dInfo->rangeFirstD;
	 range != NULL;
	 range = range->next) {
	if (tree->vertical) {
	    left = MAX(x + range->offset, minX);
	    right = MIN(x + range->offset + range->totalWidth, maxX);
	    top = MAX(y + range->totalHeight, minY);
	    bottom = maxY;

	    /* Erase area below Range */
	    if (top < bottom) {
		rect.x = left;
		rect.y = top;
		rect.width = right - left;
		rect.height = bottom - top;
		TkUnionRectWithRegion(&rect, wsRgn, wsRgn);
	    }
	}
	else {
	    left = MAX(x + range->totalWidth, minX);
	    right = maxX;
	    top = MAX(y + range->offset, minY);
	    bottom = MIN(y + range->offset + range->totalHeight, maxY);

	    /* Erase area to right of Range */
	    if (left < right) {
		rect.x = left;
		rect.y = top;
		rect.width = right - left;
		rect.height = bottom - top;
		TkUnionRectWithRegion(&rect, wsRgn, wsRgn);
	    }
	}
	if (range == dInfo->rangeLastD)
	    break;
    }
    return wsRgn;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawTiledImage --
 *
 *	This procedure draws a tiled image in the indicated box.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn.
 *
 *----------------------------------------------------------------------
 */

void
Tree_DrawTiledImage(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    Tk_Image image,		/* The image to draw. */
    int x1, int y1,		/* Left & top of area to fill with the image. */
    int x2, int y2,		/* Right & bottom, of area to fill with the
				 * image. */
    int xOffset, int yOffset	/* Used to keep the image aligned with an
				 * origin. */
    )
{
    int imgWidth, imgHeight;
    int srcX, srcY;
    int srcW, srcH;
    int dstX, dstY;

    Tk_SizeOfImage(image, &imgWidth, &imgHeight);

    srcX = (x1 + xOffset) % imgWidth;
    dstX = x1;
    while (dstX < x2) {
	srcW = imgWidth - srcX;
	if (dstX + srcW > x2) {
	    srcW = x2 - dstX;
	}

	srcY = (y1 + yOffset) % imgHeight;
	dstY = y1;
	while (dstY < y2) {
	    srcH = imgHeight - srcY;
	    if (dstY + srcH > y2) {
		srcH = y2 - dstY;
	    }
	    Tk_RedrawImage(image, srcX, srcY, srcW, srcH, drawable, dstX, dstY);
	    srcY = 0;
	    dstY += srcH;
	}
	srcX = 0;

	/* the last tile gives dstX == x2 which ends the while loop; same
	* for dstY above */
	dstX += srcW;
    };
}

/*
 *--------------------------------------------------------------
 *
 * Tree_Display --
 *
 *	This procedure is called at idle time when something has happened
 *	that might require the list to be redisplayed. An effort is made
 *	to only redraw what is needed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn in the TreeCtrl window.
 *
 *--------------------------------------------------------------
 */

void
Tree_Display(
    ClientData clientData	/* Widget info. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;
    Tk_Window tkwin = tree->tkwin;
    Drawable drawable = Tk_WindowId(tkwin);
    int minX, minY, maxX, maxY, height, width;
    int left, right, top, bottom;
    int count;
    int numCopy = 0, numDraw = 0;
    TkRegion wsRgnNew, wsRgnDif;
    XRectangle wsBox;
    int requests;

    if (tree->debug.enable && tree->debug.display && 0)
	dbwin("Tree_Display %s\n", Tk_PathName(tkwin));

    if (tree->deleted) {
	dInfo->flags &= ~(DINFO_REDRAW_PENDING);
	return;
    }

    /* */
    Tcl_Preserve((ClientData) tree);
    Tree_PreserveItems(tree);

displayRetry:

    /* Some change requires selection changes */
    if (dInfo->flags & DINFO_REDO_SELECTION) {
#ifdef SELECTION_VISIBLE
	/* Possible <Selection> event. */
	Tree_DeselectHidden(tree);
	if (tree->deleted)
	    goto displayExit;
#endif
	dInfo->flags &= ~(DINFO_REDO_SELECTION);
    }

    /* A column was deleted */
    if (dInfo->flags & DINFO_REDO_COLUMN_WIDTH) {
	TreeColumn treeColumn = tree->columns;
	int columnIndex = 0;

	if (dInfo->columnWidthSize < tree->columnCount) {
	    dInfo->columnWidthSize = tree->columnCount + 10;
	    dInfo->columnWidth = (int *) ckrealloc((char *) dInfo->columnWidth,
		    sizeof(int) * dInfo->columnWidthSize);
	}
	while (treeColumn != NULL) {
	    dInfo->columnWidth[columnIndex++] = TreeColumn_UseWidth(treeColumn);
	    treeColumn = TreeColumn_Next(treeColumn);
	}
	dInfo->flags &= ~(DINFO_REDO_COLUMN_WIDTH | DINFO_CHECK_COLUMN_WIDTH);
	dInfo->flags |=
	    DINFO_INVALIDATE |
	    DINFO_OUT_OF_DATE |
	    DINFO_REDO_RANGES |
	    DINFO_DRAW_HEADER;
    }
    /* The width of one or more columns *might* have changed */
    if (dInfo->flags & DINFO_CHECK_COLUMN_WIDTH) {
	TreeColumn treeColumn = tree->columns;
	int columnIndex = 0;

	if (dInfo->columnWidthSize < tree->columnCount) {
	    dInfo->columnWidthSize = tree->columnCount + 10;
	    dInfo->columnWidth = (int *) ckrealloc((char *) dInfo->columnWidth,
		    sizeof(int) * dInfo->columnWidthSize);
	}
	while (treeColumn != NULL) {
	    if (dInfo->columnWidth[columnIndex] != TreeColumn_UseWidth(treeColumn)) {
		dInfo->columnWidth[columnIndex] = TreeColumn_UseWidth(treeColumn);
		dInfo->flags |=
		    DINFO_INVALIDATE |
		    DINFO_OUT_OF_DATE |
		    DINFO_REDO_RANGES |
		    DINFO_DRAW_HEADER;
	    }
	    columnIndex++;
	    treeColumn = TreeColumn_Next(treeColumn);
	}
	dInfo->flags &= ~DINFO_CHECK_COLUMN_WIDTH;
    }
    if (dInfo->headerHeight != Tree_HeaderHeight(tree)) {
	dInfo->headerHeight = Tree_HeaderHeight(tree);
	dInfo->flags |=
	    DINFO_OUT_OF_DATE |
	    DINFO_SET_ORIGIN_Y |
	    DINFO_UPDATE_SCROLLBAR_Y |
	    DINFO_DRAW_HEADER;
	if (tree->vertical && (tree->wrapMode == TREE_WRAP_WINDOW))
	    dInfo->flags |= DINFO_REDO_RANGES;
    }
    Range_RedoIfNeeded(tree);
    Increment_RedoIfNeeded(tree);
    if (dInfo->xOrigin != tree->xOrigin) {
	dInfo->flags |=
	    DINFO_UPDATE_SCROLLBAR_X |
	    DINFO_OUT_OF_DATE |
	    DINFO_DRAW_HEADER;
    }
    if (dInfo->yOrigin != tree->yOrigin) {
	dInfo->flags |=
	    DINFO_UPDATE_SCROLLBAR_Y |
	    DINFO_OUT_OF_DATE;
    }
    if (dInfo->totalWidth != Tree_TotalWidth(tree)) {
	dInfo->totalWidth = Tree_TotalWidth(tree);
	dInfo->flags |=
	    DINFO_SET_ORIGIN_X |
	    DINFO_UPDATE_SCROLLBAR_X |
	    DINFO_OUT_OF_DATE;
    }
    if (dInfo->totalHeight != Tree_TotalHeight(tree)) {
	dInfo->totalHeight = Tree_TotalHeight(tree);
	dInfo->flags |=
	    DINFO_SET_ORIGIN_Y |
	    DINFO_UPDATE_SCROLLBAR_Y |
	    DINFO_OUT_OF_DATE;
    }
    if (dInfo->flags & DINFO_SET_ORIGIN_X) {
	Tree_SetOriginX(tree, tree->xOrigin);
	dInfo->flags &= ~DINFO_SET_ORIGIN_X;
    }
    if (dInfo->flags & DINFO_SET_ORIGIN_Y) {
	Tree_SetOriginY(tree, tree->yOrigin);
	dInfo->flags &= ~DINFO_SET_ORIGIN_Y;
    }
    /*
     * dInfo->requests counts the number of calls to Tree_EventuallyRedraw().
     * If binding scripts do something that causes a redraw to be requested,
     * then we abort the current draw and start again.
     */
    requests = dInfo->requests;
    if (dInfo->flags & DINFO_UPDATE_SCROLLBAR_X) {
	/* Possible <Scroll-x> event. */
	Tree_UpdateScrollbarX(tree);
	dInfo->flags &= ~DINFO_UPDATE_SCROLLBAR_X;
    }
    if (dInfo->flags & DINFO_UPDATE_SCROLLBAR_Y) {
	/* Possible <Scroll-y> event. */
	Tree_UpdateScrollbarY(tree);
	dInfo->flags &= ~DINFO_UPDATE_SCROLLBAR_Y;
    }
    if (tree->deleted || !Tk_IsMapped(tkwin))
	goto displayExit;
    if (requests != dInfo->requests) {
	goto displayRetry;
    }
    if (dInfo->flags & DINFO_OUT_OF_DATE) {
	Tree_UpdateDInfo(tree);
	dInfo->flags &= ~DINFO_OUT_OF_DATE;
    }

    /*
     * When an item goes from visible to hidden, "window" elements in the
     * item must be hidden. An item may become hidden because of scrolling,
     * or because an ancestor was collapsed, or because the -visible option
     * of the item changed.
     */
    {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	TreeItemList newV, newH;
	TreeItem item;
	int isNew, i, count;

	TreeItemList_Init(tree, &newV, 0);
	TreeItemList_Init(tree, &newH, 0);

	for (dItem = dInfo->dItem;
	    dItem != NULL;
	    dItem = dItem->next) {

	    hPtr = Tcl_FindHashEntry(&dInfo->itemVisHash, (char *) dItem->item);
	    if (hPtr == NULL) {
		/* This item is now visible, wasn't before */
		TreeItemList_Append(&newV, dItem->item);
		TreeItem_OnScreen(tree, dItem->item, TRUE);
	    }
	}

	hPtr = Tcl_FirstHashEntry(&dInfo->itemVisHash, &search);
	while (hPtr != NULL) {
	    item = (TreeItem) Tcl_GetHashKey(&dInfo->itemVisHash, hPtr);
	    if (TreeItem_GetDInfo(tree, item) == NULL) {
		/* This item was visible but isn't now */
		TreeItemList_Append(&newH, item);
		TreeItem_OnScreen(tree, item, FALSE);
	    }
	    hPtr = Tcl_NextHashEntry(&search);
	}

	/* Remove newly-hidden items from itemVisHash */
	count = TreeItemList_Count(&newH);
	for (i = 0; i < count; i++) {
	    item = TreeItemList_ItemN(&newH, i);
	    hPtr = Tcl_FindHashEntry(&dInfo->itemVisHash, (char *) item);
	    Tcl_DeleteHashEntry(hPtr);
	}

	/* Add newly-visible items to itemVisHash */
	count = TreeItemList_Count(&newV);
	for (i = 0; i < count; i++) {
	    item = TreeItemList_ItemN(&newV, i);
	    hPtr = Tcl_CreateHashEntry(&dInfo->itemVisHash, (char *) item, &isNew);
	}

	requests = dInfo->requests;

	/*
	 * Generate an <ItemVisibility> event here. This can be used to set
	 * an item's styles when the item is about to be displayed, and to
	 * clear an item's styles when the item is no longer displayed.
	 */
	if (TreeItemList_Count(&newV) || TreeItemList_Count(&newH)) {
	    /* Possible <ItemVisibility> event. */
	    TreeNotify_ItemVisibility(tree, &newV, &newH);
	}

	TreeItemList_Free(&newV);
	TreeItemList_Free(&newH);

	if (tree->deleted || !Tk_IsMapped(tkwin))
	    goto displayExit;

	if (requests != dInfo->requests) {
	    goto displayRetry;
	}
    }

    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	dInfo->dirty[LEFT] = dInfo->dirty[TOP] = 100000;
	dInfo->dirty[RIGHT] = dInfo->dirty[BOTTOM] = -100000;
	drawable = dInfo->pixmap;
    }

    minX = tree->inset;
    maxX = Tk_Width(tkwin) - tree->inset;
    minY = tree->inset + Tree_HeaderHeight(tree);
    maxY = Tk_Height(tkwin) - tree->inset;

    /* XOR off */
    TreeColumnProxy_Undisplay(tree);
    TreeDragImage_Undisplay(tree->dragImage);
    TreeMarquee_Undisplay(tree->marquee);

    if (dInfo->flags & DINFO_DRAW_HEADER) {
	if (tree->showHeader) {
	    Tree_DrawHeader(tree, drawable, 0 - tree->xOrigin, tree->inset);
	    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
		dInfo->dirty[LEFT] = minX;
		dInfo->dirty[TOP] = tree->inset;
		dInfo->dirty[RIGHT] = maxX;
		dInfo->dirty[BOTTOM] = minY;
	    }
	}
	dInfo->flags &= ~DINFO_DRAW_HEADER;
    }

    if (tree->vertical) {
	numCopy = ScrollVerticalComplex(tree);
	ScrollHorizontalSimple(tree);
    }
    else {
	ScrollVerticalSimple(tree);
	numCopy = ScrollHorizontalComplex(tree);
    }

    /* If we scrolled, then copy the entire pixmap, plus the header
     * if needed. */
    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	if ((dInfo->xOrigin != tree->xOrigin) ||
		(dInfo->yOrigin != tree->yOrigin)) {
	    dInfo->dirty[LEFT] = minX;
	    /* might include header */
	    dInfo->dirty[TOP] = MIN(dInfo->dirty[TOP], minY);
	    dInfo->dirty[RIGHT] = maxX;
	    dInfo->dirty[BOTTOM] = maxY;
	}
    }

    if (tree->backgroundImage != NULL) {
	wsRgnNew = CalcWhiteSpaceRegion(tree);

	/* If we scrolled, redraw entire whitespace area */
	if (dInfo->xOrigin != tree->xOrigin ||
		dInfo->yOrigin != tree->yOrigin) {
	    wsRgnDif = wsRgnNew;
	} else {
	    wsRgnDif = TkCreateRegion();
	    TkSubtractRegion(wsRgnNew, dInfo->wsRgn, wsRgnDif);
	}
	TkClipBox(wsRgnDif, &wsBox);
	if ((wsBox.width > 0) && (wsBox.height > 0)) {
	    Drawable pixmap = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
		    wsBox.width, wsBox.height, Tk_Depth(tkwin));
	    GC gc = Tk_3DBorderGC(tkwin, tree->border, TK_3D_FLAT_GC);

	    if (tree->debug.enable && tree->debug.display && tree->debug.eraseColor) {
		Tk_FillRegion(tree->display, Tk_WindowId(tkwin),
			tree->debug.gcErase, wsRgnDif);
		DisplayDelay(tree);
	    }

	    /* FIXME: only if backgroundImage is transparent */
	    Tk_OffsetRegion(wsRgnDif, -wsBox.x, -wsBox.y);
	    Tk_FillRegion(tree->display, pixmap, gc, wsRgnDif);
	    Tk_OffsetRegion(wsRgnDif, wsBox.x, wsBox.y);

/*	    tree->drawableXOrigin = tree->xOrigin + wsBox.x;
	    tree->drawableYOrigin = tree->yOrigin + wsBox.y;*/

	    Tree_DrawTiledImage(tree, pixmap, tree->backgroundImage,
		    0, 0, wsBox.width, wsBox.height,
		    tree->xOrigin + wsBox.x, tree->yOrigin + wsBox.y);

	    TkSetRegion(tree->display, tree->copyGC, wsRgnNew);
/*			XSetClipOrigin(tree->display, tree->copyGC, 0,
			0);*/
	    XCopyArea(tree->display, pixmap, drawable, tree->copyGC,
		    0, 0, wsBox.width, wsBox.height,
		    wsBox.x, wsBox.y);
	    XSetClipMask(tree->display, tree->copyGC, None);
/*			XSetClipOrigin(tree->display, tree->copyGC, 0, 0);*/

	    Tk_FreePixmap(tree->display, pixmap);

	    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
		dInfo->dirty[LEFT] = MIN(dInfo->dirty[LEFT], wsBox.x);
		dInfo->dirty[TOP] = MIN(dInfo->dirty[TOP], wsBox.y);
		dInfo->dirty[RIGHT] = MAX(dInfo->dirty[RIGHT], wsBox.x + wsBox.width);
		dInfo->dirty[BOTTOM] = MAX(dInfo->dirty[BOTTOM], wsBox.y + wsBox.height);
	    }
	}
	if (wsRgnDif != wsRgnNew)
	    TkDestroyRegion(wsRgnDif);
	TkDestroyRegion(dInfo->wsRgn);
	dInfo->wsRgn = wsRgnNew;
    }

    dInfo->xOrigin = tree->xOrigin;
    dInfo->yOrigin = tree->yOrigin;

    /* Does this need to be here? */
    dInfo->flags &= ~(DINFO_REDRAW_PENDING);

    if (tree->backgroundImage == NULL) {
	/* Calculate the current whitespace region, subtract the old whitespace
	 * region, and fill the difference with the background color. */
	wsRgnNew = CalcWhiteSpaceRegion(tree);
	wsRgnDif = TkCreateRegion();
	TkSubtractRegion(wsRgnNew, dInfo->wsRgn, wsRgnDif);
	TkClipBox(wsRgnDif, &wsBox);
	if ((wsBox.width > 0) && (wsBox.height > 0)) {
	    GC gc = Tk_3DBorderGC(tkwin, tree->border, TK_3D_FLAT_GC);
	    if (tree->debug.enable && tree->debug.display && tree->debug.eraseColor) {
		Tk_FillRegion(tree->display, Tk_WindowId(tkwin),
			tree->debug.gcErase, wsRgnDif);
		DisplayDelay(tree);
	    }
	    Tk_FillRegion(tree->display, drawable, gc, wsRgnDif);
	    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
		dInfo->dirty[LEFT] = MIN(dInfo->dirty[LEFT], wsBox.x);
		dInfo->dirty[TOP] = MIN(dInfo->dirty[TOP], wsBox.y);
		dInfo->dirty[RIGHT] = MAX(dInfo->dirty[RIGHT], wsBox.x + wsBox.width);
		dInfo->dirty[BOTTOM] = MAX(dInfo->dirty[BOTTOM], wsBox.y + wsBox.height);
	    }
	}
	TkDestroyRegion(wsRgnDif);
	TkDestroyRegion(dInfo->wsRgn);
	dInfo->wsRgn = wsRgnNew;
    }

    /* See if there are any dirty items */
    count = 0;
    for (dItem = dInfo->dItem;
	 dItem != NULL;
	 dItem = dItem->next) {
	if (dItem->flags & DITEM_DIRTY) {
	    count++;
	    break;
	}
    }

    /* Display dirty items */
    if (count > 0) {
	Drawable pixmap = drawable;
	if (tree->doubleBuffer != DOUBLEBUFFER_NONE) {
	    /* Allocate pixmap for largest item */
	    width = MIN(maxX - minX, dInfo->itemWidth);
	    height = MIN(maxY - minY, dInfo->itemHeight);
	    pixmap = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
		    width, height, Tk_Depth(tkwin));
	}
	for (dItem = dInfo->dItem;
	     dItem != NULL;
	     dItem = dItem->next) {

	    if (!(dItem->flags & DITEM_DIRTY)) {

		/* This is needed for updating window positions */
		tree->drawableXOrigin = tree->xOrigin;
		tree->drawableYOrigin = tree->yOrigin;

		/*
		 * This item was visible and still is. Handle scrolling.
		 */
		TreeItem_UpdateWindowPositions(tree, dItem->item,
		    dItem->x, dItem->y, dItem->width, dItem->height);
		continue;
	    }

	    if (dItem->flags & DITEM_ALL_DIRTY) {
		left = dItem->x;
		right = dItem->x + dItem->width;
		top = dItem->y;
		bottom = dItem->y + dItem->height;
	    }
	    else {
		left = dItem->x + dItem->dirty[LEFT];
		right = dItem->x + dItem->dirty[RIGHT];
		top = dItem->y + dItem->dirty[TOP];
		bottom = dItem->y + dItem->dirty[BOTTOM];
	    }

	    if (left < minX)
		left = minX;
	    if (right > maxX)
		right = maxX;
	    if (top < minY)
		top = minY;
	    if (bottom > maxY)
		bottom = maxY;

	    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
		dInfo->dirty[LEFT] = MIN(dInfo->dirty[LEFT], left);
		dInfo->dirty[TOP] = MIN(dInfo->dirty[TOP], top);
		dInfo->dirty[RIGHT] = MAX(dInfo->dirty[RIGHT], right);
		dInfo->dirty[BOTTOM] = MAX(dInfo->dirty[BOTTOM], bottom);
	    }

	    if (tree->doubleBuffer != DOUBLEBUFFER_NONE) {
		/* The top-left corner of the drawable is at this
		 * point in the canvas */
		tree->drawableXOrigin = left + tree->xOrigin;
		tree->drawableYOrigin = top + tree->yOrigin;

		TreeItem_Draw(tree, dItem->item,
			dItem->x - left,
			dItem->y - top,
			dItem->width, dItem->height,
			pixmap,
			0, right - left,
			dItem->index);
		XCopyArea(tree->display, pixmap, drawable,
			tree->copyGC,
			0, 0,
			right - left, bottom - top,
			left, top);
	    }
	    /* DOUBLEBUFFER_NONE */
	    else {
		/* The top-left corner of the drawable is at this
		 * point in the canvas */
		tree->drawableXOrigin = tree->xOrigin;
		tree->drawableYOrigin = tree->yOrigin;

		TreeItem_Draw(tree, dItem->item,
			dItem->x,
			dItem->y,
			dItem->width, dItem->height,
			pixmap,
			minX, maxX,
			dItem->index);
	    }
	    DisplayDelay(tree);
	    numDraw++;

	    dItem->oldX = dItem->x;
	    dItem->oldY = dItem->y;
	    dItem->flags &= ~(DITEM_DIRTY | DITEM_ALL_DIRTY);
	}
	if (tree->doubleBuffer != DOUBLEBUFFER_NONE)
	    Tk_FreePixmap(tree->display, pixmap);
    }

    if (tree->debug.enable && tree->debug.display)
	dbwin("copy %d draw %d %s\n", numCopy, numDraw, Tk_PathName(tkwin));

    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	drawable = Tk_WindowId(tkwin);

	if (dInfo->dirty[LEFT] < dInfo->dirty[RIGHT]) {
	    XCopyArea(tree->display, dInfo->pixmap, drawable,
		    tree->copyGC,
		    dInfo->dirty[LEFT], dInfo->dirty[TOP],
		    dInfo->dirty[RIGHT] - dInfo->dirty[LEFT],
		    dInfo->dirty[BOTTOM] - dInfo->dirty[TOP],
		    dInfo->dirty[LEFT], dInfo-> dirty[TOP]);
	}
    }

    /* XOR on */
    TreeMarquee_Display(tree->marquee);
    TreeDragImage_Display(tree->dragImage);
    TreeColumnProxy_Display(tree);

    if (tree->doubleBuffer == DOUBLEBUFFER_NONE)
	dInfo->flags |= DINFO_DRAW_HIGHLIGHT | DINFO_DRAW_BORDER;

    /* Draw focus rectangle (outside of 3D-border) */
    if ((dInfo->flags & DINFO_DRAW_HIGHLIGHT) && (tree->highlightWidth > 0)) {
	GC fgGC, bgGC;

	bgGC = Tk_GCForColor(tree->highlightBgColorPtr, drawable);
	if (tree->gotFocus)
	    fgGC = Tk_GCForColor(tree->highlightColorPtr, drawable);
	else
	    fgGC = bgGC;
	TkpDrawHighlightBorder(tkwin, fgGC, bgGC, tree->highlightWidth, drawable);
	dInfo->flags &= ~DINFO_DRAW_HIGHLIGHT;
    }

    /* Draw 3D-border (inside of focus rectangle) */
    if ((dInfo->flags & DINFO_DRAW_BORDER) && (tree->borderWidth > 0)) {
	Tk_Draw3DRectangle(tkwin, drawable, tree->border, tree->highlightWidth,
		tree->highlightWidth, Tk_Width(tkwin) - tree->highlightWidth * 2,
		Tk_Height(tkwin) - tree->highlightWidth * 2, tree->borderWidth,
		tree->relief);
	dInfo->flags &= ~DINFO_DRAW_BORDER;
    }

displayExit:
    dInfo->flags &= ~(DINFO_REDRAW_PENDING);
    Tree_ReleaseItems(tree);
    Tcl_Release((ClientData) tree);
}

/*
 *--------------------------------------------------------------
 *
 * A_IncrementFindX --
 *
 *	Return a horizontal scroll position nearest to the given
 *	offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
A_IncrementFindX(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas x-coordinate. */
    )
{
    int totWidth = Tree_TotalWidth(tree);
    int xIncr = tree->xScrollIncrement;
    int index, indexMax;

    indexMax = totWidth / xIncr;
    if (totWidth % xIncr == 0)
	indexMax--;
    if (offset < 0)
	offset = 0;
    index = offset / xIncr;
    if (index > indexMax)
	index = indexMax;
    return index;
}

/*
 *--------------------------------------------------------------
 *
 * A_IncrementFindY --
 *
 *	Return a vertical scroll position nearest to the given
 *	offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
A_IncrementFindY(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas y-coordinate. */
    )
{
    int totHeight = Tree_TotalHeight(tree);
    int yIncr = tree->yScrollIncrement;
    int index, indexMax;

    indexMax = totHeight / yIncr;
    if (totHeight % yIncr == 0)
	indexMax--;
    if (offset < 0)
	offset = 0;
    index = offset / yIncr;
    if (index > indexMax)
	index = indexMax;
    return index;
}

/*
 *--------------------------------------------------------------
 *
 * Increment_FindX --
 *
 *	Return a horizontal scroll position nearest to the given
 *	offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Increment_FindX(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas x-coordinate. */
    )
{
    if (tree->xScrollIncrement <= 0) {
	Increment_RedoIfNeeded(tree);
	return B_IncrementFindX(tree, offset);
    }
    return A_IncrementFindX(tree, offset);
}

/*
 *--------------------------------------------------------------
 *
 * Increment_FindY --
 *
 *	Return a vertical scroll position nearest to the given
 *	offset.
 *
 * Results:
 *	Index of the nearest increment <= the given offset.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Increment_FindY(
    TreeCtrl *tree,		/* Widget info. */
    int offset			/* Canvas y-coordinate. */
    )
{
    if (tree->yScrollIncrement <= 0) {
	Increment_RedoIfNeeded(tree);
	return B_IncrementFindY(tree, offset);
    }
    return A_IncrementFindY(tree, offset);
}

/*
 *--------------------------------------------------------------
 *
 * Increment_ToOffsetX --
 *
 *	Return the canvas coordinate for a scroll position.
 *
 * Results:
 *	Pixel distance.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Increment_ToOffsetX(
    TreeCtrl *tree,		/* Widget info. */
    int index			/* Index of the increment. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    if (tree->xScrollIncrement <= 0) {
	if (index < 0 || index >= dInfo->xScrollIncrementCount)
	    panic("Increment_ToOffsetX: bad index %d (must be 0-%d)",
		    index, dInfo->xScrollIncrementCount-1);
	return dInfo->xScrollIncrements[index];
    }
    return index * tree->xScrollIncrement;
}

/*
 *--------------------------------------------------------------
 *
 * Increment_ToOffsetY --
 *
 *	Return the canvas coordinate for a scroll position.
 *
 * Results:
 *	Pixel distance.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Increment_ToOffsetY(
    TreeCtrl *tree,		/* Widget info. */
    int index			/* Index of the increment. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    if (tree->yScrollIncrement <= 0) {
	if (index < 0 || index >= dInfo->yScrollIncrementCount) {
	    panic("Increment_ToOffsetY: bad index %d (must be 0-%d)\ntotHeight %d visHeight %d",
		    index, dInfo->yScrollIncrementCount - 1,
		    Tree_TotalHeight(tree), Tk_Height(tree->tkwin) - Tree_HeaderHeight(tree) - tree->inset);
	}
	return dInfo->yScrollIncrements[index];
    }
    return index * tree->yScrollIncrement;
}

/*
 *--------------------------------------------------------------
 *
 * GetScrollFractions --
 *
 *	Return the fractions that may be passed to a scrollbar "set"
 *	command.
 *
 * Results:
 *	Two fractions from 0.0 to 1.0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
GetScrollFractions(
    int screen1, int screen2,	/* Min/max coordinates that are visible in
				 * the window. */
    int object1, int object2,	/* Min/max coordinates of the scrollable
				 * content (usually 0 to N where N is the
				 * total width or height of the canvas). */
    double fractions[2]		/* Returned values. */
    )
{
    double range, f1, f2;

    range = object2 - object1;
    if (range <= 0) {
	f1 = 0;
	f2 = 1.0;
    }
    else {
	f1 = (screen1 - object1) / range;
	if (f1 < 0)
	    f1 = 0.0;
	f2 = (screen2 - object1) / range;
	if (f2 > 1.0)
	    f2 = 1.0;
	if (f2 < f1)
	    f2 = f1;
    }

    fractions[0] = f1;
    fractions[1] = f2;
}

/*
 *--------------------------------------------------------------
 *
 * Tree_GetScrollFractionsX --
 *
 *	Return the fractions that may be passed to a scrollbar "set"
 *	command for a horizontal scrollbar.
 *
 * Results:
 *	Two fractions from 0 to 1.0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_GetScrollFractionsX(
    TreeCtrl *tree,		/* Widget info. */
    double fractions[2]		/* Returned values. */
    )
{
    int left = tree->xOrigin + tree->inset;
    int visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
    int totWidth = Tree_TotalWidth(tree);
    int index, offset;

    /* The tree is empty, or everything fits in the window */
    if (visWidth < 0)
	visWidth = 0;
    if (totWidth <= visWidth) {
	fractions[0] = 0.0;
	fractions[1] = 1.0;
	return;
    }

    if (visWidth <= 1) {
	GetScrollFractions(left, left + 1, 0, totWidth, fractions);
	return;
    }

    /* Find incrementLeft when scrolled to extreme right */
    index = Increment_FindX(tree, totWidth - visWidth);
    offset = Increment_ToOffsetX(tree, index);
    if (offset < totWidth - visWidth) {
	index++;
	offset = Increment_ToOffsetX(tree, index);
    }

    /* Add some fake content to right */
    if (offset + visWidth > totWidth)
	totWidth = offset + visWidth;

    GetScrollFractions(left, left + visWidth, 0, totWidth, fractions);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_GetScrollFractionsY --
 *
 *	Return the fractions that may be passed to a scrollbar "set"
 *	command for a vertical scrollbar.
 *
 * Results:
 *	Two fractions from 0 to 1.0.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_GetScrollFractionsY(
    TreeCtrl *tree,		/* Widget info. */
    double fractions[2]		/* Returned values. */
    )
{
    int topInset = tree->inset + Tree_HeaderHeight(tree);
    int top = topInset + tree->yOrigin; /* canvas coords */
    int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;
    int totHeight = Tree_TotalHeight(tree);
    int index, offset;

    /* The tree is empty, or everything fits in the window */
    if (visHeight < 0)
	visHeight = 0;
    if (totHeight <= visHeight) {
	fractions[0] = 0.0;
	fractions[1] = 1.0;
	return;
    }

    if (visHeight <= 1) {
	GetScrollFractions(top, top + 1, 0, totHeight, fractions);
	return;
    }

    /* Find incrementTop when scrolled to bottom */
    index = Increment_FindY(tree, totHeight - visHeight);
    offset = Increment_ToOffsetY(tree, index);
    if (offset < totHeight - visHeight) {
	index++;
	offset = Increment_ToOffsetY(tree, index);
    }

    /* Add some fake content to bottom */
    if (offset + visHeight > totHeight)
	totHeight = offset + visHeight;

    GetScrollFractions(top, top + visHeight, 0, totHeight, fractions);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_SetOriginX --
 *
 *	Change the horizontal scroll position.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the horizontal scroll position changes, then the widget is
 *	redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_SetOriginX(
    TreeCtrl *tree,		/* Widget info. */
    int xOrigin			/* The desired offset from the left edge
				 * of the window to the left edge of the
				 * canvas. The actual value will be clipped
				 * to the nearest scroll increment. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    int totWidth = Tree_TotalWidth(tree);
    int visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
    int index, indexMax, offset;

    /* The tree is empty, or everything fits in the window */
    if (visWidth < 0)
	visWidth = 0;
    if (totWidth <= visWidth) {
	xOrigin = 0 - tree->inset;
	if (xOrigin != tree->xOrigin) {
	    tree->xOrigin = xOrigin;
	    dInfo->incrementLeft = 0;
	    Tree_EventuallyRedraw(tree);
	}
	return;
    }

    if (visWidth > 1) {
	/* Find incrementLeft when scrolled to extreme right */
	indexMax = Increment_FindX(tree, totWidth - visWidth);
	offset = Increment_ToOffsetX(tree, indexMax);
	if (offset < totWidth - visWidth) {
	    indexMax++;
	    offset = Increment_ToOffsetX(tree, indexMax);
	}

	/* Add some fake content to right */
	if (offset + visWidth > totWidth)
	    totWidth = offset + visWidth;
    }
    else
	indexMax = Increment_FindX(tree, totWidth);

    xOrigin += tree->inset; /* origin -> canvas */
    index = Increment_FindX(tree, xOrigin);

    /* Don't scroll too far left */
    if (index < 0)
	index = 0;

    /* Don't scroll too far right */
    if (index > indexMax)
	index = indexMax;

    offset = Increment_ToOffsetX(tree, index);
    xOrigin = offset - tree->inset;

    if (xOrigin == tree->xOrigin)
	return;

    tree->xOrigin = xOrigin;
    dInfo->incrementLeft = index;

    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_SetOriginY --
 *
 *	Change the vertical scroll position.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the vertical scroll position changes, then the widget is
 *	redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_SetOriginY(
    TreeCtrl *tree,		/* Widget info. */
    int yOrigin			/* The desired offset from the top edge
				 * of the window to the top edge of the
				 * canvas. The actual value will be clipped
				 * to the nearest scroll increment. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    int topInset = tree->inset + Tree_HeaderHeight(tree);
    int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;
    int totHeight = Tree_TotalHeight(tree);
    int index, indexMax, offset;

    /* The tree is empty, or everything fits in the window */
    if (visHeight < 0)
	visHeight = 0;
    if (totHeight <= visHeight) {
	yOrigin = 0 - topInset;
	if (yOrigin != tree->yOrigin) {
	    tree->yOrigin = yOrigin;
	    dInfo->incrementTop = 0;
	    Tree_EventuallyRedraw(tree);
	}
	return;
    }

    if (visHeight > 1) {
	/* Find incrementTop when scrolled to bottom */
	indexMax = Increment_FindY(tree, totHeight - visHeight);
	offset = Increment_ToOffsetY(tree, indexMax);
	if (offset < totHeight - visHeight) {
	    indexMax++;
	    offset = Increment_ToOffsetY(tree, indexMax);
	}

	/* Add some fake content to bottom */
	if (offset + visHeight > totHeight)
	    totHeight = offset + visHeight;
    }
    else
	indexMax = Increment_FindY(tree, totHeight);

    yOrigin += topInset; /* origin -> canvas */
    index = Increment_FindY(tree, yOrigin);

    /* Don't scroll too far left */
    if (index < 0)
	index = 0;

    /* Don't scroll too far right */
    if (index > indexMax)
	index = indexMax;

    offset = Increment_ToOffsetY(tree, index);
    yOrigin = offset - topInset;
    if (yOrigin == tree->yOrigin)
	return;

    tree->yOrigin = yOrigin;
    dInfo->incrementTop = index;

    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_EventuallyRedraw --
 *
 *	Schedule an idle task to redisplay the widget, if one is not
 *	already scheduled and the widget is mapped and the widget
 *	hasn't been deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget may be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_EventuallyRedraw(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    dInfo->requests++;
    if ((dInfo->flags & DINFO_REDRAW_PENDING) ||
	    tree->deleted ||
	    !Tk_IsMapped(tree->tkwin)) {
	return;
    }
    dInfo->flags |= DINFO_REDRAW_PENDING;
    Tcl_DoWhenIdle(Tree_Display, (ClientData) tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_RelayoutWindow --
 *
 *	Invalidate all the layout info for the widget and schedule a
 *	redisplay at idle time. This gets called when certain config
 *	options change and when the size of the widget changes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_RelayoutWindow(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    FreeDItems(tree, dInfo->dItem, NULL, 0);
    dInfo->dItem = NULL;
    dInfo->flags |=
	DINFO_REDO_RANGES |
	DINFO_OUT_OF_DATE |
	DINFO_CHECK_COLUMN_WIDTH |
	DINFO_DRAW_HEADER |
	DINFO_SET_ORIGIN_X |
	DINFO_SET_ORIGIN_Y |
	DINFO_UPDATE_SCROLLBAR_X |
	DINFO_UPDATE_SCROLLBAR_Y;
    if (tree->highlightWidth > 0)
	dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
    if (tree->borderWidth > 0)
	dInfo->flags |= DINFO_DRAW_BORDER;
    dInfo->xOrigin = tree->xOrigin;
    dInfo->yOrigin = tree->yOrigin;

    /* Needed if background color changes */
    TkSubtractRegion(dInfo->wsRgn, dInfo->wsRgn, dInfo->wsRgn);

    if (tree->doubleBuffer == DOUBLEBUFFER_WINDOW) {
	if (dInfo->pixmap == None) {
	    dInfo->pixmap = Tk_GetPixmap(tree->display,
		    Tk_WindowId(tree->tkwin),
		    Tk_Width(tree->tkwin),
		    Tk_Height(tree->tkwin),
		    Tk_Depth(tree->tkwin));
	}
	else if ((tree->prevWidth != Tk_Width(tree->tkwin)) ||
		(tree->prevHeight != Tk_Height(tree->tkwin))) {
	    Tk_FreePixmap(tree->display, dInfo->pixmap);
	    dInfo->pixmap = Tk_GetPixmap(tree->display,
		    Tk_WindowId(tree->tkwin),
		    Tk_Width(tree->tkwin),
		    Tk_Height(tree->tkwin),
		    Tk_Depth(tree->tkwin));
	}
    }
    else if (dInfo->pixmap != None) {
	Tk_FreePixmap(tree->display, dInfo->pixmap);
	dInfo->pixmap = None;
    }

    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_FocusChanged --
 *
 *	This procedure handles the widget gaining or losing the input
 *	focus. The state of every item has STATE_FOCUS toggled on or
 *	off.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget may be redisplayed at idle time if -highlightthickness
 *	is > 0, or if any Elements change appearance because of the
 *	state change.
 *
 *--------------------------------------------------------------
 */

void
Tree_FocusChanged(
    TreeCtrl *tree,		/* Widget info. */
    int gotFocus		/* TRUE if the widget has the focus,
				 * otherwise FALSE. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    int redraw = FALSE;
    TreeItem item;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int stateOn, stateOff;

    tree->gotFocus = gotFocus;

    if (gotFocus)
	stateOff = 0, stateOn = STATE_FOCUS;
    else
	stateOff = STATE_FOCUS, stateOn = 0;

    /* Slow. Change state of every item */
    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	TreeItem_ChangeState(tree, item, stateOff, stateOn);
	hPtr = Tcl_NextHashEntry(&search);
    }

    if (tree->highlightWidth > 0) {
	dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
	redraw = TRUE;
    }
    if (redraw)
	Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_Activate --
 *
 *	This procedure handles the widget's toplevel being the "active"
 *	foreground window (on Macintosh and Windows). Currently it just
 *	redraws the header if -usetheme is TRUE and the header is
 *	visible.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget may be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_Activate(
    TreeCtrl *tree,		/* Widget info. */
    int isActive		/* TRUE if the widget's toplevel is the
				 * active window, otherwise FALSE. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    tree->isActive = isActive;

    /* TODO: Like Tree_FocusChanged, change state of every item. */
    /* Would need a new item state STATE_ACTIVEWINDOW or something. */
    /* Would want to merge this with Tree_FocusChanged code to avoid
     * double-iteration of items. */

    /* Aqua column header looks different when window is not active */
    if (tree->useTheme && tree->showHeader) {
	dInfo->flags |= DINFO_DRAW_HEADER;
	Tree_EventuallyRedraw(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_FreeItemDInfo --
 *
 *	Free any DItem associated with each item in a range of items.
 *	This is called when the size of an item changed or an item is
 *	deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_FreeItemDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item1,		/* First item in the range. */
    TreeItem item2		/* Last item in the range, or NULL. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;
    TreeItem item = item1;
    int changed = 0;

    while (item != NULL) {
	dItem = (DItem *) TreeItem_GetDInfo(tree, item);
	if (dItem != NULL) {
	    FreeDItems(tree, dItem, dItem->next, 1);
	    changed = 1;
	}
	if (item == item2 || item2 == NULL)
	    break;
	item = TreeItem_Next(tree, item);
    }
    changed = 1;
    if (changed) {
	dInfo->flags |= DINFO_OUT_OF_DATE;
	Tree_EventuallyRedraw(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateItemDInfo --
 *
 *	Mark as dirty any DItem associated with each item in a range
 *	of items. This is called when the appearance of an item changed
 *	(but not its size).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time if any of the items
 *	had a DItem.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateItemDInfo(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item1,		/* First item in the range. */
    TreeItem item2		/* Last item in the range, or NULL. */
    )
{
    /* DInfo *dInfo = (DInfo *) tree->dInfo; */
    DItem *dItem;
    TreeItem item = item1;
    int changed = 0;

    while (item != NULL) {
	dItem = (DItem *) TreeItem_GetDInfo(tree, item);
	if (dItem != NULL) {
	    dItem->flags |= (DITEM_DIRTY | DITEM_ALL_DIRTY);
	    changed = 1;
	}
	if (item == item2 || item2 == NULL)
	    break;
	item = TreeItem_Next(tree, item);
    }
    if (changed) {
	/*dInfo->flags |= DINFO_OUT_OF_DATE; */
	Tree_EventuallyRedraw(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TreeDisplay_ItemDeleted --
 *
 *	Removes an item from the hash table of on-screen items.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TreeDisplay_ItemDeleted(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item		/* Item to remove. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&dInfo->itemVisHash, (char *) item);
    if (hPtr != NULL)
	Tcl_DeleteHashEntry(hPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_DInfoChanged --
 *
 *	Set some DINFO_xxx flags and schedule a redisplay.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_DInfoChanged(
    TreeCtrl *tree,		/* Widget info. */
    int flags			/* DINFO_xxx flags. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    dInfo->flags |= flags;
    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateArea --
 *
 *	Mark as dirty parts of any DItems in the given area. If the given
 *	area overlaps the borders they are marked as needing to be
 *	redrawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateArea(
    TreeCtrl *tree,		/* Widget info. */
    int x1, int y1,		/* Left & top of dirty area in window
				 * coordinates. */
    int x2, int y2		/* Right & bottom of dirty area in window
				 * coordinates. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;

    if (x1 >= x2 || y1 >= y2)
	return;

    if ((y2 > tree->inset) && (y1 < tree->inset + Tree_HeaderHeight(tree)))
	dInfo->flags |= DINFO_DRAW_HEADER;

    dItem = dInfo->dItem;
    while (dItem != NULL) {
	if (!(dItem->flags & DITEM_ALL_DIRTY) &&
		(x2 > dItem->x) && (x1 < dItem->x + dItem->width) &&
		(y2 > dItem->y) && (y1 < dItem->y + dItem->height)) {
	    InvalidateDItemX(dItem, dItem->x, x1, x2 - x1);
	    InvalidateDItemY(dItem, dItem->y, y1, y2 - y1);
	    dItem->flags |= DITEM_DIRTY;
	}
	dItem = dItem->next;
    }

    /* Could check border and highlight separately */
    if (tree->inset > 0) {
	if ((x1 < tree->inset) ||
		(y1 < tree->inset) ||
		(x2 > Tk_Width(tree->tkwin) - tree->inset) ||
		(y2 > Tk_Height(tree->tkwin) - tree->inset)) {
	    if (tree->highlightWidth > 0)
		dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
	    if (tree->borderWidth > 0)
		dInfo->flags |= DINFO_DRAW_BORDER;
	}
    }

    if (tree->debug.enable && tree->debug.display && tree->debug.eraseColor) {
	XFillRectangle(tree->display, Tk_WindowId(tree->tkwin),
		tree->debug.gcErase, x1, y1, x2 - x1, y2 - y1);
	DisplayDelay(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateRegion --
 *
 *	Mark as dirty parts of any DItems in the given area. If the given
 *	area overlaps the borders they are marked as needing to be
 *	redrawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateRegion(
    TreeCtrl *tree,		/* Widget info. */
    TkRegion region		/* Region to mark as dirty, in window
				 * coordinates. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;
    int minX = tree->inset, maxX = Tk_Width(tree->tkwin) - tree->inset;
    int ymin = tree->inset, maxY = Tk_Height(tree->tkwin) - tree->inset;
    XRectangle rect;
    int x1, x2, y1, y2;

    TkClipBox(region, &rect);
    if (!rect.width || !rect.height)
	return;
    x1 = rect.x, x2 = rect.x + rect.width;
    y1 = rect.y, y2 = rect.y + rect.height;

    if (TkRectInRegion(region, minX, ymin, maxX - minX, Tree_HeaderHeight(tree)) != RectangleOut)
	dInfo->flags |= DINFO_DRAW_HEADER;

    dItem = dInfo->dItem;
    while (dItem != NULL) {
	if (!(dItem->flags & DITEM_ALL_DIRTY) &&
		(TkRectInRegion(region, dItem->x, dItem->y,
		dItem->width, dItem->height) != RectangleOut)) {
	    InvalidateDItemX(dItem, dItem->x, x1, x2 - x1);
	    InvalidateDItemY(dItem, dItem->y, y1, y2 - y1);
	    dItem->flags |= DITEM_DIRTY;
	}
	dItem = dItem->next;
    }

    /* Could check border and highlight separately */
    if (tree->inset > 0) {
	if ((x1 < minX) || (y1 < ymin) || (x2 > maxX) || (y2 > maxY)) {
	    if (tree->highlightWidth > 0)
		dInfo->flags |= DINFO_DRAW_HIGHLIGHT;
	    if (tree->borderWidth > 0)
		dInfo->flags |= DINFO_DRAW_BORDER;
	}
    }

    if (tree->debug.enable && tree->debug.display && tree->debug.eraseColor) {
	Tk_FillRegion(tree->display, Tk_WindowId(tree->tkwin),
		tree->debug.gcErase, region);
	DisplayDelay(tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tree_InvalidateItemArea --
 *
 *	Mark as dirty parts of any DItems in the given area. This is
 *	like Tree_InvalidateArea() but the given area is clipped inside
 *	the borders/header.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
Tree_InvalidateItemArea(
    TreeCtrl *tree,		/* Widget info. */
    int x1, int y1,		/* Left & top of dirty area in window
				 * coordinates. */
    int x2, int y2		/* Right & bottom of dirty area in window
				 * coordinates. */
    )
{
    if (x1 < tree->inset)
	x1 = tree->inset;
    if (y1 < tree->inset + Tree_HeaderHeight(tree))
	y1 = tree->inset + Tree_HeaderHeight(tree);
    if (x2 > Tk_Width(tree->tkwin) - tree->inset)
	x2 = Tk_Width(tree->tkwin) - tree->inset;
    if (y2 > Tk_Height(tree->tkwin) - tree->inset)
	y2 = Tk_Height(tree->tkwin) - tree->inset;
    Tree_InvalidateArea(tree, x1, y1, x2, y2);
}

/*
 *--------------------------------------------------------------
 *
 * Tree_RedrawArea --
 *
 *	Mark as dirty parts of any DItems in the given area. If the given
 *	area overlaps the borders they are marked as needing to be
 *	redrawn. The given area is subtracted from the whitespace region
 *	so that that part of the whitespace region will be redrawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget will be redisplayed at idle time.
 *
 *--------------------------------------------------------------
 */

void
Tree_RedrawArea(
    TreeCtrl *tree,		/* Widget info. */
    int x1, int y1,		/* Left & top of dirty area in window
				 * coordinates. */
    int x2, int y2		/* Right & bottom of dirty area in window
				 * coordinates. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;

    if (x1 < x2 && y1 < y2) {
	XRectangle rect;
	TkRegion rgn = TkCreateRegion();

	rect.x = x1;
	rect.y = y1;
	rect.width = x2 - x1;
	rect.height = y2 - y1;
	TkUnionRectWithRegion(&rect, rgn, rgn);
	TkSubtractRegion(dInfo->wsRgn, rgn, dInfo->wsRgn);
	TkDestroyRegion(rgn);
    }

    Tree_InvalidateArea(tree, x1, y1, x2, y2);
    Tree_EventuallyRedraw(tree);
}

/*
 *--------------------------------------------------------------
 *
 * TreeDInfo_Init --
 *
 *	Perform display-related initialization when a new TreeCtrl is
 *	created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *--------------------------------------------------------------
 */

void
TreeDInfo_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo;
    XGCValues gcValues;

    dInfo = (DInfo *) ckalloc(sizeof(DInfo));
    memset(dInfo, '\0', sizeof(DInfo));
    gcValues.graphics_exposures = True;
    dInfo->scrollGC = Tk_GetGC(tree->tkwin, GCGraphicsExposures, &gcValues);
    dInfo->flags = DINFO_OUT_OF_DATE;
    dInfo->columnWidthSize = 10;
    dInfo->columnWidth = (int *) ckalloc(sizeof(int) * dInfo->columnWidthSize);
    dInfo->wsRgn = TkCreateRegion();
    Tcl_InitHashTable(&dInfo->itemVisHash, TCL_ONE_WORD_KEYS);
    tree->dInfo = (TreeDInfo) dInfo;
}

/*
 *--------------------------------------------------------------
 *
 * TreeDInfo_Free --
 *
 *	Free display-related resources for a deleted TreeCtrl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *--------------------------------------------------------------
 */

void
TreeDInfo_Free(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    Range *range = dInfo->rangeFirst;

    if (dInfo->rItem != NULL)
	ckfree((char *) dInfo->rItem);
    while (dInfo->dItem != NULL) {
	DItem *next = dInfo->dItem->next;
	WFREE(dInfo->dItem, DItem);
	dInfo->dItem = next;
    }
    while (dInfo->dItemFree != NULL) {
	DItem *next = dInfo->dItemFree->next;
	WFREE(dInfo->dItemFree, DItem);
	dInfo->dItemFree = next;
    }
    while (range != NULL)
	range = Range_Free(tree, range);
    Tk_FreeGC(tree->display, dInfo->scrollGC);
    if (dInfo->flags & DINFO_REDRAW_PENDING)
	Tcl_CancelIdleCall(Tree_Display, (ClientData) tree);
    if (dInfo->pixmap != None)
	Tk_FreePixmap(tree->display, dInfo->pixmap);
    if (dInfo->xScrollIncrements != NULL)
	ckfree((char *) dInfo->xScrollIncrements);
    if (dInfo->yScrollIncrements != NULL)
	ckfree((char *) dInfo->yScrollIncrements);
    TkDestroyRegion(dInfo->wsRgn);
    Tcl_DeleteHashTable(&dInfo->itemVisHash);
    WFREE(dInfo, DInfo);
}

void
DumpDInfo(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DInfo *dInfo = (DInfo *) tree->dInfo;
    DItem *dItem;
    Range *range;
    RItem *rItem;

    dbwin("DumpDInfo: itemW,H %d,%d totalW,H %d,%d flags 0x%0x vertical %d itemVisCount %d\n",
	    dInfo->itemWidth, dInfo->itemHeight,
	    dInfo->totalWidth, dInfo->totalHeight,
	    dInfo->flags, tree->vertical, tree->itemVisCount);
    dItem = dInfo->dItem;
    while (dItem != NULL) {
	if (dItem->item == NULL) {
	    dbwin("    item NULL\n");
	} else {
	    dbwin("    item %d x,y,w,h %d,%d,%d,%d dirty %d,%d,%d,%d flags %0X\n",
		    TreeItem_GetID(tree, dItem->item),
		    dItem->x, dItem->y, dItem->width, dItem->height,
		    dItem->dirty[LEFT], dItem->dirty[TOP],
		    dItem->dirty[RIGHT], dItem->dirty[BOTTOM],
		    dItem->flags);
	}
	dItem = dItem->next;
    }

    dbwin("  dInfo.rangeFirstD %p dInfo.rangeLastD %p\n",
	    dInfo->rangeFirstD, dInfo->rangeLastD);
    for (range = dInfo->rangeFirstD;
	 range != NULL;
	 range = range->next) {
	dbwin("  Range: totalW,H %d,%d offset %d\n", range->totalWidth,
		range->totalHeight, range->offset);
	if (range == dInfo->rangeLastD)
	    break;
    }

    dbwin("  dInfo.rangeFirst %p dInfo.rangeLast %p\n",
	    dInfo->rangeFirst, dInfo->rangeLast);
    for (range = dInfo->rangeFirst;
	 range != NULL;
	 range = range->next) {
	dbwin("   Range: first %p last %p totalW,H %d,%d offset %d\n",
		range->first, range->last,
		range->totalWidth, range->totalHeight, range->offset);
	rItem = range->first;
	while (1) {
	    dbwin("    RItem: item %d index %d offset %d size %d\n",
		    TreeItem_GetID(tree, rItem->item), rItem->index, rItem->offset, rItem->size);
	    if (rItem == range->last)
		break;
	    rItem++;
	}
    }
}

