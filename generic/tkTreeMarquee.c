/* 
 * tkTreeMarquee.c --
 *
 *	This module implements the selection rectangle for treectrl widgets.
 *
 * Copyright (c) 2002-2005 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeMarquee.c,v 1.7 2005/09/17 00:05:37 treectrl Exp $
 */

#include "tkTreeCtrl.h"

typedef struct Marquee Marquee;

/*
 * The following structure holds info about the selection rectangle.
 * There is one of these per TreeCtrl.
 */
struct Marquee
{
    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int visible;			/* -visible option. */
    int x1, y1, x2, y2;			/* Opposing corners. */
    int onScreen;			/* TRUE if it was drawn. */
    int sx, sy;				/* Offset of canvas from top-left
					 * corner of the window when we
					 * were drawn. */
};

#define MARQ_CONF_VISIBLE		0x0001

static Tk_OptionSpec optionSpecs[] = {
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
	"0", -1, Tk_Offset(Marquee, visible),
	0, (ClientData) NULL, MARQ_CONF_VISIBLE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Init --
 *
 *	Perform marquee-related initialization when a new TreeCtrl is
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
TreeMarquee_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Marquee *marquee;

    marquee = (Marquee *) ckalloc(sizeof(Marquee));
    memset(marquee, '\0', sizeof(Marquee));
    marquee->tree = tree;
    marquee->optionTable = Tk_CreateOptionTable(tree->interp, optionSpecs);
    if (Tk_InitOptions(tree->interp, (char *) marquee, marquee->optionTable,
	tree->tkwin) != TCL_OK)
    {
	WFREE(marquee, Marquee);
	return TCL_ERROR;
    }
    tree->marquee = (TreeMarquee) marquee;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Free --
 *
 *	Free marquee-related resources when a TreeCtrl is deleted.
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
TreeMarquee_Free(
    TreeMarquee marquee_	/* Marquee token. */
    )
{
    Marquee *marquee = (Marquee *) marquee_;

    Tk_FreeConfigOptions((char *) marquee, marquee->optionTable,
	marquee->tree->tkwin);
    WFREE(marquee, Marquee);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Display --
 *
 *	Draw the selection rectangle if it is not already displayed and if
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
TreeMarquee_Display(
    TreeMarquee marquee_	/* Marquee token. */
    )
{
    Marquee *marquee = (Marquee *) marquee_;
    TreeCtrl *tree = marquee->tree;

    if (!marquee->onScreen && marquee->visible)
    {
	marquee->sx = 0 - tree->xOrigin;
	marquee->sy = 0 - tree->yOrigin;
	TreeMarquee_Draw(marquee_, Tk_WindowId(tree->tkwin), marquee->sx, marquee->sy);
	marquee->onScreen = TRUE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Undisplay --
 *
 *	Erase the selection rectangle if it is displayed.
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
TreeMarquee_Undisplay(
    TreeMarquee marquee_	/* Marquee token. */
    )
{
    Marquee *marquee = (Marquee *) marquee_;
    TreeCtrl *tree = marquee->tree;

    if (marquee->onScreen)
    {
	TreeMarquee_Draw(marquee_, Tk_WindowId(tree->tkwin), marquee->sx, marquee->sy);
	marquee->onScreen = FALSE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarquee_Draw --
 *
 *	Draw (or erase) the selection rectangle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is drawn (or erased, since this is XOR drawing).
 *
 *----------------------------------------------------------------------
 */

void
TreeMarquee_Draw(
    TreeMarquee marquee_,	/* Marquee token. */
    Drawable drawable,		/* Where to draw. */
    int x1, int y1		/* Offset of canvas from top-left corner
				 * of the window. */
    )
{
    Marquee *marquee = (Marquee *) marquee_;
    TreeCtrl *tree = marquee->tree;
    int x, y, w, h;
    DotState dotState;

    x = MIN(marquee->x1, marquee->x2);
    w = abs(marquee->x1 - marquee->x2) + 1;
    y = MIN(marquee->y1, marquee->y2);
    h = abs(marquee->y1 - marquee->y2) + 1;

    DotRect_Setup(tree, drawable, &dotState);
    DotRect_Draw(&dotState, x1 + x, y1 + y, w, h);
    DotRect_Restore(&dotState);
}

/*
 *----------------------------------------------------------------------
 *
 * Marquee_Config --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a Marquee.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for marquee;  old resources get freed, if there
 *	were any.  Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
Marquee_Config(
    Marquee *marquee,		/* Marquee record. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = marquee->tree;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;

    for (error = 0; error <= 1; error++)
    {
	if (error == 0)
	{
	    if (Tk_SetOptions(tree->interp, (char *) marquee, marquee->optionTable,
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

    if (mask & MARQ_CONF_VISIBLE)
    {
	TreeMarquee_Undisplay((TreeMarquee) marquee);
	TreeMarquee_Display((TreeMarquee) marquee);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeMarqueeCmd --
 *
 *	This procedure is invoked to process the [marquee] widget
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
TreeMarqueeCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    Marquee *marquee = (Marquee *) tree->marquee;
    static CONST char *commandNames[] = { "anchor", "cget", "configure",
	"coords", "corner", "identify", (char *) NULL };
    enum { COMMAND_ANCHOR, COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_COORDS,
	COMMAND_CORNER, COMMAND_IDENTIFY };
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
	/* T marquee anchor ?x y?*/
	case COMMAND_ANCHOR:
	{
	    int x, y;

	    if (objc != 3 && objc != 5)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3)
	    {
		FormatResult(interp, "%d %d", marquee->x1, marquee->y1);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
		return TCL_ERROR;
	    if ((x == marquee->x1) && (y == marquee->y1))
		break;
	    TreeMarquee_Undisplay(tree->marquee);
	    marquee->x1 = x;
	    marquee->y1 = y;
	    TreeMarquee_Display(tree->marquee);
	    break;
	}

	/* T marquee cget option */
	case COMMAND_CGET:
	{
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) marquee,
		marquee->optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T marquee configure ?option? ?value? ?option value ...? */
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
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) marquee,
		    marquee->optionTable,
		    (objc == 3) ? (Tcl_Obj *) NULL : objv[3],
		    tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    return Marquee_Config(marquee, objc - 3, objv + 3);
	}

	/* T marquee coords ?x y x y? */
	case COMMAND_COORDS:
	{
	    int x1, y1, x2, y2;

	    if (objc != 3 && objc != 7)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3)
	    {
		FormatResult(interp, "%d %d %d %d", marquee->x1, marquee->y1,
		    marquee->x2, marquee->y2);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x1) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y1) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[5], &x2) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[6], &y2) != TCL_OK)
		return TCL_ERROR;
	    if (x1 == marquee->x1 && y1 == marquee->y1 &&
		x2 == marquee->x2 && y2 == marquee->y2)
		break;
	    TreeMarquee_Undisplay(tree->marquee);
	    marquee->x1 = x1;
	    marquee->y1 = y1;
	    marquee->x2 = x2;
	    marquee->y2 = y2;
	    TreeMarquee_Display(tree->marquee);
	    break;
	}

	/* T marquee corner ?x y?*/
	case COMMAND_CORNER:
	{
	    int x, y;

	    if (objc != 3 && objc != 5)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "?x y?");
		return TCL_ERROR;
	    }
	    if (objc == 3)
	    {
		FormatResult(interp, "%d %d", marquee->x2, marquee->y2);
		break;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
		return TCL_ERROR;
	    if (x == marquee->x2 && y == marquee->y2)
		break;
	    TreeMarquee_Undisplay(tree->marquee);
	    marquee->x2 = x;
	    marquee->y2 = y;
	    TreeMarquee_Display(tree->marquee);
	    break;
	}

	/* T marquee identify */
	case COMMAND_IDENTIFY:
	{
	    int x1, y1, x2, y2, n = 0;
	    int totalWidth = Tree_TotalWidth(tree);
	    int totalHeight = Tree_TotalHeight(tree);
	    TreeItem *items;
	    Tcl_Obj *listObj;

	    if (objc != 3)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }

	    x1 = MIN(marquee->x1, marquee->x2);
	    x2 = MAX(marquee->x1, marquee->x2);
	    y1 = MIN(marquee->y1, marquee->y2);
	    y2 = MAX(marquee->y1, marquee->y2);

	    if (x2 <= 0)
		break;
	    if (x1 >= totalWidth)
		break;

	    if (y2 <= 0)
		break;
	    if (y1 >= totalHeight)
		break;

	    if (x1 < 0)
		x1 = 0;
	    if (x2 > totalWidth)
		x2 = totalWidth;

	    if (y1 < 0)
		y1 = 0;
	    if (y2 > totalHeight)
		y2 = totalHeight;

	    items = Tree_ItemsInArea(tree, x1, y1, x2, y2);
	    if (items == NULL)
		break;

	    listObj = Tcl_NewListObj(0, NULL);
	    while (items[n] != NULL)
	    {
		Tcl_Obj *subListObj = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(interp, subListObj,
		    TreeItem_ToObj(tree, items[n]));
		TreeItem_Identify2(tree, items[n], x1, y1, x2, y2, subListObj);
		Tcl_ListObjAppendElement(interp, listObj, subListObj);
		n++;
	    }
	    ckfree((char *) items);
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}
    }

    return TCL_OK;
}
