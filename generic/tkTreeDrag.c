/* 
 * tkTreeDrag.c --
 *
 *	This module implements outline dragging for treectrl widgets.
 *
 * Copyright (c) 2002-2006 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeDrag.c,v 1.16 2006/09/05 21:56:15 treectrl Exp $
 */

#include "tkTreeCtrl.h"

typedef struct DragElem DragElem;
typedef struct DragImage DragImage;

/*
 * The following structure holds info about a single element of the drag
 * image.
 */
struct DragElem
{
    int x, y, width, height;
    DragElem *next;
};

/*
 * The following structure holds info about the drag image. There is one of
 * these per TreeCtrl.
 */
struct DragImage
{
    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int visible;
    int x, y; /* offset to draw at in canvas coords */
    int bounds[4]; /* bounds of all DragElems */
    DragElem *elem;
    int onScreen; /* TRUE if is displayed */
    int sx, sy; /* Window coords where displayed */
};

#define DRAG_CONF_VISIBLE		0x0001

static Tk_OptionSpec optionSpecs[] = {
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
	"0", -1, Tk_Offset(DragImage, visible),
	0, (ClientData) NULL, DRAG_CONF_VISIBLE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * DragElem_Alloc --
 *
 *	Allocate and initialize a new DragElem record. Add the record
 *	to the list of records for the drag image.
 *
 * Results:
 *	Pointer to allocated DragElem.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static DragElem *
DragElem_Alloc(
    DragImage *dragImage	/* Drag image record. */
    )
{
    DragElem *elem = (DragElem *) ckalloc(sizeof(DragElem));
    DragElem *walk = dragImage->elem;
    memset(elem, '\0', sizeof(DragElem));
    if (dragImage->elem == NULL)
	dragImage->elem = elem;
    else
    {
	while (walk->next != NULL)
	    walk = walk->next;
	walk->next = elem;
    }
    return elem;
}

/*
 *----------------------------------------------------------------------
 *
 * DragElem_Free --
 *
 *	Free a DragElem.
 *
 * Results:
 *	Pointer to the next DragElem.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

static DragElem *
DragElem_Free(
    DragImage *dragImage,	/* Drag image record. */
    DragElem *elem		/* Drag element to free. */
    )
{
    DragElem *next = elem->next;
    WFREE(elem, DragElem);
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Init --
 *
 *	Perform drag-image-related initialization when a new TreeCtrl is
 *	created.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

int
TreeDragImage_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    DragImage *dragImage;

    dragImage = (DragImage *) ckalloc(sizeof(DragImage));
    memset(dragImage, '\0', sizeof(DragImage));
    dragImage->tree = tree;
    dragImage->optionTable = Tk_CreateOptionTable(tree->interp, optionSpecs);
    if (Tk_InitOptions(tree->interp, (char *) dragImage, dragImage->optionTable,
	tree->tkwin) != TCL_OK)
    {
	WFREE(dragImage, DragImage);
	return TCL_ERROR;
    }
    tree->dragImage = (TreeDragImage) dragImage;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Free --
 *
 *	Free drag-image-related resources when a TreeCtrl is deleted.
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
TreeDragImage_Free(
    TreeDragImage dragImage_	/* Drag image token. */
    )
{
    DragImage *dragImage = (DragImage *) dragImage_;
    DragElem *elem = dragImage->elem;

    while (elem != NULL)
	elem = DragElem_Free(dragImage, elem);
    Tk_FreeConfigOptions((char *) dragImage, dragImage->optionTable,
	dragImage->tree->tkwin);
    WFREE(dragImage, DragImage);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Display --
 *
 *	Draw the drag image if it is not already displayed and if
 *	it's -visible option is TRUE.
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
TreeDragImage_Display(
    TreeDragImage dragImage_	/* Drag image token. */
    )
{
    DragImage *dragImage = (DragImage *) dragImage_;
    TreeCtrl *tree = dragImage->tree;

    if (!dragImage->onScreen && dragImage->visible)
    {
	dragImage->sx = 0 - tree->xOrigin;
	dragImage->sy = 0 - tree->yOrigin;
	TreeDragImage_Draw(dragImage_, Tk_WindowId(tree->tkwin), dragImage->sx, dragImage->sy);
	dragImage->onScreen = TRUE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Undisplay --
 *
 *	Erase the drag image if it is displayed.
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
TreeDragImage_Undisplay(
    TreeDragImage dragImage_	/* Drag image token. */
    )
{
    DragImage *dragImage = (DragImage *) dragImage_;
    TreeCtrl *tree = dragImage->tree;

    if (dragImage->onScreen)
    {
	TreeDragImage_Draw(dragImage_, Tk_WindowId(tree->tkwin), dragImage->sx, dragImage->sy);
	dragImage->onScreen = FALSE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DragImage_Config --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a DragImage.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for dragImage;  old resources get freed, if there
 *	were any.  Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
DragImage_Config(
    DragImage *dragImage,	/* Drag image record. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = dragImage->tree;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;

    for (error = 0; error <= 1; error++)
    {
	if (error == 0)
	{
	    if (Tk_SetOptions(tree->interp, (char *) dragImage, dragImage->optionTable,
		objc, objv, tree->tkwin, &savedOptions, &mask) != TCL_OK)
	    {
		mask = 0;
		continue;
	    }

	    /* xxx */

	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	}
	else
	{
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

		/* xxx */

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    if (mask & DRAG_CONF_VISIBLE)
    {
	TreeDragImage_Undisplay((TreeDragImage) dragImage);
	TreeDragImage_Display((TreeDragImage) dragImage);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeDragImage_Draw --
 *
 *	Draw (or erase) the elements that make up the drag image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn (or erased, since this is XOR drawing).
 *
 *----------------------------------------------------------------------
 */

void TreeDragImage_Draw(TreeDragImage dragImage_, Drawable drawable, int x, int y)
{
    DragImage *dragImage = (DragImage *) dragImage_;
    TreeCtrl *tree = dragImage->tree;
    DragElem *elem = dragImage->elem;
    DotState dotState;

/*	if (!dragImage->visible)
	return; */
    if (elem == NULL)
	return;

    DotRect_Setup(tree, drawable, &dotState);

    while (elem != NULL)
    {
	DotRect_Draw(&dotState,
	    x + dragImage->x + elem->x,
	    y + dragImage->y + elem->y,
	    elem->width, elem->height);
	elem = elem->next;
    }

    DotRect_Restore(&dotState);
}

/*
 *----------------------------------------------------------------------
 *
 * DragImageCmd --
 *
 *	This procedure is invoked to process the [dragimage] widget
 *	command.  See the user documentation for details on what it
 *	does.
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
DragImageCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    DragImage *dragImage = (DragImage *) tree->dragImage;
    static CONST char *commandNames[] = { "add", "cget", "clear", "configure",
	"offset", (char *) NULL };
    enum { COMMAND_ADD, COMMAND_CGET, COMMAND_CLEAR, COMMAND_CONFIGURE,
	COMMAND_OFFSET };
    int index;

    if (objc < 3)
    {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
	&index) != TCL_OK)
    {
	return TCL_ERROR;
    }

    switch (index)
    {
	/* T dragimage add I ?C? ?E ...? */
	case COMMAND_ADD:
	{
	    XRectangle staticRects[STATIC_SIZE], *rects = staticRects;
	    TreeItem item;
	    TreeItemColumn itemColumn = NULL;
	    TreeColumn treeColumn;
	    int i, count, columnIndex = -1;
	    int indent, width, totalWidth;
	    int x, y, w, h;
	    DragElem *elem;
	    StyleDrawArgs drawArgs;
	    int result = TCL_OK;

	    if (objc < 4)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "item ?column? ?element ...?");
		return TCL_ERROR;
	    }

	    if (TreeItem_FromObj(tree, objv[3], &item, 0) != TCL_OK)
		return TCL_ERROR;

	    /* Validate all of the arguments, even if the command would exit
	     * early without needing to check those arguments. */
	    if (objc > 4)
	    {
		if (TreeItem_ColumnFromObj(tree, item, objv[4], &itemColumn, &columnIndex) != TCL_OK)
		    return TCL_ERROR;
		if (objc > 5)
		{
		    if ((itemColumn == NULL) ||
			(TreeItemColumn_GetStyle(tree, itemColumn) == NULL)) {
			FormatResult(interp,
			    "item %s%d column %s%d has no style",
			    tree->itemPrefix, TreeItem_GetID(tree, item),
			    tree->columnPrefix,
			    TreeColumn_GetID(Tree_FindColumn(tree, columnIndex)));
			return TCL_ERROR;
		    }
		    drawArgs.tree = tree;
		    drawArgs.style = TreeItemColumn_GetStyle(tree, itemColumn);
		    if (TreeStyle_ValidateElements(&drawArgs,
			objc - 5, objv + 5) != TCL_OK)
			return TCL_ERROR;
		}
	    }

	    if (Tree_ItemBbox(tree, item, &x, &y, &w, &h) < 0)
		return TCL_OK;
	    if (w < 1 || h < 1)
		return TCL_OK;

	    drawArgs.tree = tree;
	    drawArgs.drawable = None;
	    drawArgs.state = TreeItem_GetState(tree, item);
	    drawArgs.y = y;
	    drawArgs.height = h;

	    TreeDragImage_Undisplay(tree->dragImage);

	    if (objc > 4)
	    {
		treeColumn = Tree_FindColumn(tree, columnIndex);
		if (!TreeColumn_Visible(treeColumn))
		    goto doneAdd;
		drawArgs.style = TreeItemColumn_GetStyle(tree, itemColumn);
		totalWidth = 0;
		treeColumn = tree->columns;
		for (i = 0; i < columnIndex; i++)
		{
		    totalWidth += TreeColumn_UseWidth(treeColumn);
		    treeColumn = TreeColumn_Next(treeColumn);
		}
		if (treeColumn == tree->columnTree)
		    indent = TreeItem_Indent(tree, item);
		else
		    indent = 0;
		drawArgs.indent = indent;
		drawArgs.x = x + totalWidth;
		drawArgs.width = TreeColumn_UseWidth(treeColumn);
		drawArgs.justify = TreeColumn_Justify(treeColumn);
		if (objc - 5 > STATIC_SIZE)
		STATIC_ALLOC(rects, XRectangle, objc - 5);
		count = TreeStyle_GetElemRects(&drawArgs, objc - 5, objv + 5, rects);
		if (count == -1)
		{
		    result = TCL_ERROR;
		    goto doneAdd;
		}
		for (i = 0; i < count; i++)
		{
		    elem = DragElem_Alloc(dragImage);
		    elem->x = rects[i].x;
		    elem->y = rects[i].y;
		    elem->width = rects[i].width;
		    elem->height = rects[i].height;
		}
	    }
	    else
	    {
		totalWidth = 0;
		treeColumn = tree->columns;
		itemColumn = TreeItem_GetFirstColumn(tree, item);
		while (itemColumn != NULL)
		{
		    if (!TreeColumn_Visible(treeColumn))
			goto nextColumn;
		    width = TreeColumn_UseWidth(treeColumn);
		    if (treeColumn == tree->columnTree)
			indent = TreeItem_Indent(tree, item);
		    else
			indent = 0;
		    drawArgs.style = TreeItemColumn_GetStyle(tree, itemColumn);
		    if (drawArgs.style != NULL)
		    {
			drawArgs.indent = indent;
			drawArgs.x = x + totalWidth;
			drawArgs.width = width;
			drawArgs.justify = TreeColumn_Justify(treeColumn);
			count = TreeStyle_NumElements(tree, drawArgs.style);
			STATIC_ALLOC(rects, XRectangle, count);
			count = TreeStyle_GetElemRects(&drawArgs, 0, NULL, rects);
			if (count == -1)
			{
			    result = TCL_ERROR;
			    goto doneAdd;
			}
			for (i = 0; i < count; i++)
			{
			    elem = DragElem_Alloc(dragImage);
			    elem->x = rects[i].x;
			    elem->y = rects[i].y;
			    elem->width = rects[i].width;
			    elem->height = rects[i].height;
			}
			if (rects != staticRects)
			{
			    ckfree((char *) rects);
			    rects = staticRects;
			}
		    }
		    totalWidth += width;
nextColumn:
		    treeColumn = TreeColumn_Next(treeColumn);
		    itemColumn = TreeItemColumn_GetNext(tree, itemColumn);
		}
	    }
	    dragImage->bounds[0] = 100000;
	    dragImage->bounds[1] = 100000;
	    dragImage->bounds[2] = -100000;
	    dragImage->bounds[3] = -100000;
	    for (elem = dragImage->elem;
		elem != NULL;
		elem = elem->next)
	    {
		if (elem->x < dragImage->bounds[0])
		    dragImage->bounds[0] = elem->x;
		if (elem->y < dragImage->bounds[1])
		    dragImage->bounds[1] = elem->y;
		if (elem->x + elem->width > dragImage->bounds[2])
		    dragImage->bounds[2] = elem->x + elem->width;
		if (elem->y + elem->height > dragImage->bounds[3])
		    dragImage->bounds[3] = elem->y + elem->height;
	    }
doneAdd:
	    if (rects != staticRects)
		ckfree((char *) rects);
	    TreeDragImage_Display(tree->dragImage);
	    return result;
	}

	/* T dragimage cget option */
	case COMMAND_CGET:
	{
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) dragImage,
		dragImage->optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T dragimage clear */
	case COMMAND_CLEAR:
	{
	    if (objc != 3)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    if (dragImage->elem != NULL)
	    {
		DragElem *elem = dragImage->elem;
		TreeDragImage_Undisplay(tree->dragImage);
/*				if (dragImage->visible)
		    DragImage_Redraw(dragImage); */
		while (elem != NULL)
		    elem = DragElem_Free(dragImage, elem);
		dragImage->elem = NULL;
	    }
	    break;
	}

	/* T dragimage configure ?option? ?value? ?option value ...? */
	case COMMAND_CONFIGURE:
	{
	    Tcl_Obj *resultObjPtr;

	    if (objc < 3)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "?option? ?value?");
		return TCL_ERROR;
	    }
	    if (objc <= 4)
	    {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) dragImage,
		    dragImage->optionTable,
		    (objc == 3) ? (Tcl_Obj *) NULL : objv[3],
		    tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    return DragImage_Config(dragImage, objc - 3, objv + 3);
	}

	/* T dragimage offset ?x y? */
	case COMMAND_OFFSET:
	{
	    int x, y;

	    if (objc != 3 && objc != 5)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3)
	    {
		FormatResult(interp, "%d %d", dragImage->x, dragImage->y);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
		return TCL_ERROR;
	    TreeDragImage_Undisplay(tree->dragImage);
/*			if (dragImage->visible)
		DragImage_Redraw(dragImage); */
	    dragImage->x = x;
	    dragImage->y = y;
	    TreeDragImage_Display(tree->dragImage);
	    break;
	}
    }

    return TCL_OK;
}
