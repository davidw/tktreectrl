/* 
 * tkTreeColumn.c --
 *
 *	This module implements treectrl widget's columns.
 *
 * Copyright (c) 2002-2005 Tim Baker
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003 ActiveState Corporation
 *
 * RCS: @(#) $Id: tkTreeColumn.c,v 1.20 2005/05/10 21:58:43 treectrl Exp $
 */

#include "tkTreeCtrl.h"

typedef struct Column Column;

struct Column
{
    Tcl_Obj *textObj;		/* -text */
    char *text;			/* -text */
    int width;			/* -width */
    Tcl_Obj *widthObj;		/* -width */
    int minWidth;		/* -minwidth */
    Tcl_Obj *minWidthObj;	/* -minwidth */
    int stepWidth;		/* -stepwidth */
    Tcl_Obj *stepWidthObj;	/* -stepwidth */
    int widthHack;		/* -widthhack */
    Tk_Font tkfont;		/* -font */
    Tk_Justify justify;		/* -justify */
    PerStateInfo border;	/* -background */
    Tcl_Obj *borderWidthObj;	/* -borderwidth */
    int borderWidth;		/* -borderwidth */
    XColor *textColor;		/* -textcolor */
    int expand;			/* -expand */
    int squeeze;		/* -squeeze */
    int visible;		/* -visible */
    char *tag;			/* -tag */
    char *imageString;		/* -image */
    PerStateInfo arrowBitmap;	/* -arrowbitmap */
    PerStateInfo arrowImage;	/* -arrowimage */
    Pixmap bitmap;		/* -bitmap */
    Tcl_Obj *itemBgObj;		/* -itembackground */
    int button;			/* -button */
    Tcl_Obj *textPadXObj;	/* -textpadx */
    int *textPadX;		/* -textpadx */
    Tcl_Obj *textPadYObj;	/* -textpady */
    int *textPadY;		/* -textpady */
    Tcl_Obj *imagePadXObj;	/* -imagepadx */
    int *imagePadX;		/* -imagepadx */
    Tcl_Obj *imagePadYObj;	/* -imagepady */
    int *imagePadY;		/* -imagepady */
    Tcl_Obj *arrowPadXObj;	/* -arrowpadx */
    int *arrowPadX;		/* -arrowpadx */
    Tcl_Obj *arrowPadYObj;	/* -arrowpady */
    int *arrowPadY;		/* -arrowpady */

#define ARROW_NONE 0
#define ARROW_UP 1
#define ARROW_DOWN 2
    int arrow;			/* -arrow */

#define SIDE_LEFT 0
#define SIDE_RIGHT 1
    int arrowSide;		/* -arrowside */
    int arrowGravity;		/* -arrowgravity */

#define COLUMN_STATE_NORMAL 0
#define COLUMN_STATE_ACTIVE 1
#define COLUMN_STATE_PRESSED 2
    int state;			/* -state */

    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int index;			/* column number */
    int textLen;
    int textWidth;
    Tk_Image image;
    int neededWidth;		/* calculated from borders + image/bitmap +
				 * text + arrow */
    int neededHeight;		/* calculated from borders + image/bitmap +
				 * text */
    int useWidth;		/* -width, -minwidth, or required+expansion */
    int widthOfItems;		/* width of all TreeItemColumns */
    int itemBgCount;
    XColor **itemBgColor;
    GC bitmapGC;
    Column *next;
#define COL_LAYOUT
#ifdef COL_LAYOUT
    TextLayout textLayout;	/* multi-line titles */
    int textLayoutWidth;	/* width passed to TextLayout_Compute */
    int textLayoutInvalid;
#define TEXT_WRAP_NULL -1
#define TEXT_WRAP_CHAR 0
#define TEXT_WRAP_WORD 1
    int textWrap;		/* -textwrap */
    int textLines;		/* -textlines */
#endif
};

static char *arrowST[] = { "none", "up", "down", (char *) NULL };
static char *arrowSideST[] = { "left", "right", (char *) NULL };
static char *stateST[] = { "normal", "active", "pressed", (char *) NULL };

#define COLU_CONF_IMAGE		0x0001
#define COLU_CONF_NWIDTH	0x0002	/* neededWidth */
#define COLU_CONF_NHEIGHT	0x0004	/* neededHeight */
#define COLU_CONF_TWIDTH	0x0008	/* totalWidth */
#define COLU_CONF_ITEMBG	0x0010
#define COLU_CONF_DISPLAY	0x0040
#define COLU_CONF_JUSTIFY	0x0080
#define COLU_CONF_TAG		0x0100
#define COLU_CONF_TEXT		0x0200
#define COLU_CONF_BITMAP	0x0400
#define COLU_CONF_RANGES	0x0800
#define COLU_CONF_ARROWBMP	0x1000
#define COLU_CONF_ARROWIMG	0x2000
#define COLU_CONF_BG		0x4000

static Tk_OptionSpec columnSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-arrow", (char *) NULL, (char *) NULL,
     "none", -1, Tk_Offset(Column, arrow),
     0, (ClientData) arrowST, COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING, "-arrowbitmap", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, arrowBitmap.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_ARROWBMP |
     COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING_TABLE, "-arrowgravity", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(Column, arrowGravity),
     0, (ClientData) arrowSideST, COLU_CONF_DISPLAY},
    {TK_OPTION_STRING, "-arrowimage", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, arrowImage.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_ARROWIMG |
     COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-arrowpadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(Column, arrowPadXObj), Tk_Offset(Column, arrowPadX),
     0, (ClientData) &PadAmountOption, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-arrowpady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(Column, arrowPadYObj), Tk_Offset(Column, arrowPadY),
     0, (ClientData) &PadAmountOption, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING_TABLE, "-arrowside", (char *) NULL, (char *) NULL,
     "right", -1, Tk_Offset(Column, arrowSide),
     0, (ClientData) arrowSideST, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
     /* NOTE: -background is a per-state option, so DEF_BUTTON_BG_COLOR
      * must be a list of one element */
    {TK_OPTION_STRING, "-background", (char *) NULL, (char *) NULL,
     DEF_BUTTON_BG_COLOR, Tk_Offset(Column, border.obj), -1,
     0, (ClientData) DEF_BUTTON_BG_MONO, COLU_CONF_BG | COLU_CONF_DISPLAY},
    {TK_OPTION_BITMAP, "-bitmap", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Column, bitmap),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_BITMAP | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_PIXELS, "-borderwidth", (char *) NULL, (char *) NULL,
     "2", Tk_Offset(Column, borderWidthObj), Tk_Offset(Column, borderWidth),
     0, (ClientData) NULL, COLU_CONF_TWIDTH | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_BOOLEAN, "-button", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(Column, button),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-expand", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(Column, expand),
     0, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_FONT, "-font", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Column, tkfont),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_NWIDTH |
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY | COLU_CONF_TEXT},
    {TK_OPTION_STRING, "-image", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Column, imageString),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_IMAGE | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-imagepadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(Column, imagePadXObj),
     Tk_Offset(Column, imagePadX), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-imagepady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(Column, imagePadYObj),
     Tk_Offset(Column, imagePadY), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING, "-itembackground", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, itemBgObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_ITEMBG},
    {TK_OPTION_JUSTIFY, "-justify", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(Column, justify),
     0, (ClientData) NULL, COLU_CONF_DISPLAY | COLU_CONF_JUSTIFY},
    {TK_OPTION_PIXELS, "-minwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, minWidthObj),
     Tk_Offset(Column, minWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_BOOLEAN, "-squeeze", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(Column, squeeze),
     0, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_STRING_TABLE, "-state", (char *) NULL, (char *) NULL,
     "normal", -1, Tk_Offset(Column, state), 0, (ClientData) stateST,
     COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_PIXELS, "-stepwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, stepWidthObj),
     Tk_Offset(Column, stepWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_RANGES},
    {TK_OPTION_STRING, "-tag", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Column, tag),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TAG},
    {TK_OPTION_STRING, "-text", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, textObj), Tk_Offset(Column, text),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_TEXT | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_COLOR, "-textcolor", (char *) NULL, (char *) NULL,
     DEF_BUTTON_FG, -1, Tk_Offset(Column, textColor),
     0, (ClientData) NULL, COLU_CONF_DISPLAY},
#ifdef COL_LAYOUT
    {TK_OPTION_INT, "-textlines", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(Column, textLines),
     0, (ClientData) NULL, COLU_CONF_TEXT | COLU_CONF_NWIDTH |
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
#endif
    {TK_OPTION_CUSTOM, "-textpadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(Column, textPadXObj),
     Tk_Offset(Column, textPadX), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-textpady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(Column, textPadYObj),
     Tk_Offset(Column, textPadY), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, widthObj), Tk_Offset(Column, width),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(Column, visible),
     0, (ClientData) NULL, COLU_CONF_TWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_BOOLEAN, "-widthhack", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(Column, widthHack),
     0, (ClientData) NULL, COLU_CONF_RANGES},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/* Called when Tk_Image is deleted or modified */
static void ImageChangedProc(
    ClientData clientData,
    int x, int y,
    int width, int height,
    int imageWidth, int imageHeight)
{
    /* I would like to know the image was deleted... */
    Column *column = (Column *) clientData;

    Tree_DInfoChanged(column->tree, DINFO_INVALIDATE | DINFO_OUT_OF_DATE);
}

int Tree_FindColumnByTag(TreeCtrl *tree, Tcl_Obj *obj, TreeColumn *columnPtr, int flags)
{
    Column *walk = (Column *) tree->columns;
    char *string = Tcl_GetString(obj);

    if (!strcmp(string, "tail")) {
	if (!(flags & CFO_NOT_TAIL)) {
	    (*columnPtr) = tree->columnTail;
	    return TCL_OK;
	}
	FormatResult(tree->interp, "can't specify \"tail\" for this command");
	return TCL_ERROR;
    }

    while (walk != NULL) {
	if ((walk->tag != NULL) && !strcmp(walk->tag, string)) {
	    (*columnPtr) = (TreeColumn) walk;
	    return TCL_OK;
	}
	walk = walk->next;
    }
    FormatResult(tree->interp, "column with tag \"%s\" doesn't exist",
	    string);
    return TCL_ERROR;
}

int TreeColumn_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeColumn *columnPtr, int flags)
{
    int columnIndex;

    if (Tcl_GetIntFromObj(NULL, obj, &columnIndex) == TCL_OK) {
	if (columnIndex < 0 || columnIndex >= tree->columnCount) {
	    if (tree->columnCount > 0)
		FormatResult(tree->interp,
			"bad column index \"%d\": must be from 0 to %d",
			columnIndex, tree->columnCount - 1);
	    else
		FormatResult(tree->interp,
			"bad column index \"%d\": there are no columns",
			columnIndex);
	    return TCL_ERROR;
	}
	(*columnPtr) = Tree_FindColumn(tree, columnIndex);
	return TCL_OK;
    }

    return Tree_FindColumnByTag(tree, obj, columnPtr, flags);
}

TreeColumn Tree_FindColumn(TreeCtrl *tree, int columnIndex)
{
    Column *column = (Column *) tree->columns;

    while (column != NULL) {
	if (column->index == columnIndex)
	    break;
	column = column->next;
    }
    return (TreeColumn) column;
}

TreeColumn TreeColumn_Next(TreeColumn column_)
{
    return (TreeColumn) ((Column *) column_)->next;
}

static int Column_MakeState(Column *column)
{
    int state = 0;
    if (column->state == COLUMN_STATE_NORMAL)
	state |= 1L << 0;
    else if (column->state == COLUMN_STATE_ACTIVE)
	state |= 1L << 1;
    else if (column->state == COLUMN_STATE_PRESSED)
	state |= 1L << 2;
    if (column->arrow == ARROW_UP)
	state |= 1L << 3;
    return state;
}

static void Column_FreeColors(XColor **colors, int count)
{
    int i;

    if (colors == NULL) {
	return;
    }
    for (i = 0; i < count; i++) {
	if (colors[i] != NULL) {
	    Tk_FreeColor(colors[i]);
	}
    }
    wipefree((char *) colors, sizeof(XColor *) * count);
}

static int ColumnStateFromObj(TreeCtrl *tree, Tcl_Obj *obj, int *stateOff, int *stateOn)
{
    Tcl_Interp *interp = tree->interp;
    int i, op = STATE_OP_ON, op2, op3, length, state = 0;
    char ch0, *string;
    CONST char *stateNames[4] = { "normal", "active", "pressed", "up" };
    int states[3];

    states[STATE_OP_ON] = 0;
    states[STATE_OP_OFF] = 0;
    states[STATE_OP_TOGGLE] = 0;

    string = Tcl_GetStringFromObj(obj, &length);
    if (length == 0)
	goto unknown;
    ch0 = string[0];
    if (ch0 == '!') {
	op = STATE_OP_OFF;
	++string;
	ch0 = string[0];
    } else if (ch0 == '~') {
	if (1) {
	    FormatResult(interp, "can't specify '~' for this command");
	    return TCL_ERROR;
	}
	op = STATE_OP_TOGGLE;
	++string;
	ch0 = string[0];
    }
    for (i = 0; i < 4; i++) {
	if ((ch0 == stateNames[i][0]) && !strcmp(string, stateNames[i])) {
	    state = 1L << i;
	    break;
	}
    }
    if (state == 0)
	goto unknown;

    if (op == STATE_OP_ON) {
	op2 = STATE_OP_OFF;
	op3 = STATE_OP_TOGGLE;
    }
    else if (op == STATE_OP_OFF) {
	op2 = STATE_OP_ON;
	op3 = STATE_OP_TOGGLE;
    } else {
	op2 = STATE_OP_ON;
	op3 = STATE_OP_OFF;
    }
    states[op2] &= ~state;
    states[op3] &= ~state;
    states[op] |= state;

    *stateOn |= states[STATE_OP_ON];
    *stateOff |= states[STATE_OP_OFF];

    return TCL_OK;

unknown:
    FormatResult(interp, "unknown state \"%s\"", string);
    return TCL_ERROR;
}

static int Column_Config(Column *column, int objc, Tcl_Obj *CONST objv[], int createFlag)
{
    TreeCtrl *tree = column->tree;
    Column saved, *walk;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;
    XGCValues gcValues;
    unsigned long gcMask;
/*    int stateOld = Column_MakeState(column), stateNew;*/

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) column,
			column->optionTable, objc, objv, tree->tkwin,
			&savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* Wouldn't have to do this if Tk_InitOptions() would return
	     * a mask of configured options like Tk_SetOptions() does. */
	    if (createFlag) {
		if (column->arrowBitmap.obj != NULL)
		    mask |= COLU_CONF_ARROWBMP;
		if (column->arrowImage.obj != NULL)
		    mask |= COLU_CONF_ARROWIMG;
		if (column->border.obj != NULL)
		    mask |= COLU_CONF_BG;
		if ((column != (Column *) tree->columnTail) && (column->tag != NULL))
		    mask |= COLU_CONF_TAG;
		if (column->imageString != NULL)
		    mask |= COLU_CONF_IMAGE;
		if (column->itemBgObj != NULL)
		    mask |= COLU_CONF_ITEMBG;
	    }

	    /*
	     * Step 1: Save old values
	     */

	    if (mask & COLU_CONF_ARROWBMP)
		PSTSave(&column->arrowBitmap, &saved.arrowBitmap);
	    if (mask & COLU_CONF_ARROWIMG)
		PSTSave(&column->arrowImage, &saved.arrowImage);
	    if (mask & COLU_CONF_BG)
		PSTSave(&column->border, &saved.border);
	    if (mask & COLU_CONF_IMAGE)
		saved.image = column->image;
	    if (mask & COLU_CONF_ITEMBG) {
		saved.itemBgColor = column->itemBgColor;
		saved.itemBgCount = column->itemBgCount;
	    }

	    /*
	     * Step 2: Process new values
	     */

	    if (mask & COLU_CONF_ARROWBMP) {
		if (PerStateInfo_FromObj(tree, ColumnStateFromObj, &pstBitmap,
		    &column->arrowBitmap) != TCL_OK)
		    continue;
	    }

	    if (mask & COLU_CONF_ARROWIMG) {
		if (PerStateInfo_FromObj(tree, ColumnStateFromObj, &pstImage,
		    &column->arrowImage) != TCL_OK)
		    continue;
	    }

	    if (mask & COLU_CONF_BG) {
		if (PerStateInfo_FromObj(tree, ColumnStateFromObj, &pstBorder,
		    &column->border) != TCL_OK)
		    continue;
	    }

	    if (mask & COLU_CONF_IMAGE) {
		column->image = NULL;
		if (column->imageString != NULL) {
		    column->image = Tk_GetImage(tree->interp, tree->tkwin,
			    column->imageString, ImageChangedProc,
			    (ClientData) column);
		    if (column->image == NULL)
			continue;
		}
	    }

	    if (mask & COLU_CONF_ITEMBG) {
		column->itemBgColor = NULL;
		column->itemBgCount = 0;
		if (column->itemBgObj != NULL) {
		    int i, length, listObjc;
		    Tcl_Obj **listObjv;
		    XColor **colors;

		    if (Tcl_ListObjGetElements(tree->interp, column->itemBgObj,
				&listObjc, &listObjv) != TCL_OK)
			continue;
		    colors = (XColor **) ckalloc(sizeof(XColor *) * listObjc);
		    for (i = 0; i < listObjc; i++)
			colors[i] = NULL;
		    for (i = 0; i < listObjc; i++) {
			/* Can specify "" for tree background */
			(void) Tcl_GetStringFromObj(listObjv[i], &length);
			if (length != 0) {
			    colors[i] = Tk_AllocColorFromObj(tree->interp,
				    tree->tkwin, listObjv[i]);
			    if (colors[i] == NULL)
				break;
			}
		    }
		    if (i < listObjc) {
			Column_FreeColors(colors, listObjc);
			continue;
		    }
		    column->itemBgColor = colors;
		    column->itemBgCount = listObjc;
		}
	    }

	    if ((mask & COLU_CONF_TAG) && (column->tag != NULL)) {
		if (column == (Column *) tree->columnTail) {
		    FormatResult(tree->interp,
			    "can't change tag of tail column");
		    continue;
		}
		if (!strcmp(column->tag, "tail")) {
		    FormatResult(tree->interp, "column tag \"%s\" is not unique",
			    column->tag);
		    continue;
		}
		/* Verify -tag is unique */
		walk = (Column *) tree->columns;
		while (walk != NULL) {
		    if ((walk != column) && (walk->tag != NULL) && !strcmp(walk->tag, column->tag)) {
			FormatResult(tree->interp, "column tag \"%s\" is not unique",
				column->tag);
			break;
		    }
		    walk = walk->next;
		}
		if (walk != NULL)
		    continue;
	    }

	    /*
	     * Step 3: Free saved values
	     */

	    if (mask & COLU_CONF_ARROWBMP)
		PerStateInfo_Free(tree, &pstBitmap, &saved.arrowBitmap);
	    if (mask & COLU_CONF_ARROWIMG)
		PerStateInfo_Free(tree, &pstImage, &saved.arrowImage);
	    if (mask & COLU_CONF_BG)
		PerStateInfo_Free(tree, &pstBorder, &saved.border);
	    if (mask & COLU_CONF_IMAGE) {
		if (saved.image != NULL)
		    Tk_FreeImage(saved.image);
	    }
	    if (mask & COLU_CONF_ITEMBG)
		Column_FreeColors(saved.itemBgColor, saved.itemBgCount);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (mask & COLU_CONF_ARROWBMP)
		PSTRestore(tree, &pstBitmap, &column->arrowBitmap,
		    &saved.arrowBitmap);
	    if (mask & COLU_CONF_ARROWIMG)
		PSTRestore(tree, &pstImage, &column->arrowImage,
		    &saved.arrowImage);
	    if (mask & COLU_CONF_BG)
		PSTRestore(tree, &pstBorder, &column->border,
		    &saved.border);
	    if (mask & COLU_CONF_IMAGE) {
		if (column->image != NULL)
		    Tk_FreeImage(column->image);
		column->image = saved.image;
	    }
	    if (mask & COLU_CONF_ITEMBG) {
		Column_FreeColors(column->itemBgColor, column->itemBgCount);
		column->itemBgColor = saved.itemBgColor;
		column->itemBgCount = saved.itemBgCount;
	    }

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    /* Wouldn't have to do this if Tk_InitOptions() would return
    * a mask of configured options like Tk_SetOptions() does. */
    if (createFlag) {
	if (column->textObj != NULL)
	    mask |= COLU_CONF_TEXT;
	if (column->bitmap != None)
	    mask |= COLU_CONF_BITMAP;
    }

    if (mask & COLU_CONF_TEXT) {
	if (column->textObj != NULL)
	    (void) Tcl_GetStringFromObj(column->textObj, &column->textLen);
	else
	    column->textLen = 0;
	if (column->textLen) {
	    Tk_Font tkfont = column->tkfont ? column->tkfont : tree->tkfont;
	    column->textWidth = Tk_TextWidth(tkfont, column->text, column->textLen);
	} else
	    column->textWidth = 0;
    }

    if (mask & COLU_CONF_BITMAP) {
	if (column->bitmapGC != None) {
	    Tk_FreeGC(tree->display, column->bitmapGC);
	    column->bitmapGC = None;
	}
	if (column->bitmap != None) {
	    gcValues.clip_mask = column->bitmap;
	    gcValues.graphics_exposures = False;
	    gcMask = GCClipMask | GCGraphicsExposures;
	    column->bitmapGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
	}
    }

    if (mask & COLU_CONF_ITEMBG) {
	/* Set max -itembackground */
	tree->columnBgCnt = 0;
	walk = (Column *) tree->columns;
	while (walk != NULL) {
	    if (walk->itemBgCount > tree->columnBgCnt)
		tree->columnBgCnt = walk->itemBgCount;
	    walk = walk->next;
	}
	if (createFlag)
	    if (column->itemBgCount > tree->columnBgCnt)
		tree->columnBgCnt = column->itemBgCount;
	Tree_DInfoChanged(tree, DINFO_INVALIDATE | DINFO_OUT_OF_DATE);
    }

#if 0
    stateNew = Column_MakeState(column);
    if (stateOld != stateNew) {
	int csMask = 0;
	if (column->arrowImage.obj != NULL) {
	    Tk_Image image1, image2;
	    Pixmap bitmap1, bitmap2;
	    int arrowUp, arrowDown;
	    int w1, h1, w2, h2;
	    void *ptr1 = NULL, *ptr2 = NULL;

	    /* image > bitmap > theme > draw */
	    image1 = PerStateImage_ForState(tree, &column->arrowImage, stateOld, NULL);
	    if (image1 != NULL) {
		Tk_SizeOfImage(image1, &w1, &h1);
		ptr1 = image1;
	    }
	    if (ptr1 == NULL) {
		bitmap1 = PerStateBitmap_ForState(tree, &column->arrowBitmap, stateOld, NULL);
		if (bitmap1 != None) {
		    Tk_SizeOfBitmap(tree->display, bitmap1, &w1, &h1);
		    ptr1 = (void *) bitmap1;
		}
	    }
	    if (ptr1 == NULL) {
		w1 = h1 = 1; /* any value will do */
		ptr1 = (stateOld & (1L << 3)) ? &arrowUp : &arrowDown;
	    }

	    /* image > bitmap > theme > draw */
	    image2 = PerStateImage_ForState(tree, &column->arrowImage, stateNew, NULL);
	    if (image2 != NULL) {
		Tk_SizeOfImage(image2, &w2, &h2);
		ptr2 = image2;
	    }
	    if (ptr2 == NULL) {
		bitmap2 = PerStateBitmap_ForState(tree, &column->arrowBitmap, stateNew, NULL);
		if (bitmap2 != None) {
		    Tk_SizeOfBitmap(tree->display, bitmap2, &w2, &h2);
		    ptr2 = (void *) bitmap2;
		}
	    }
	    if (ptr2 == NULL) {
		w2 = h2 = 1; /* any value will do */
		ptr2 = (stateNew & (1L << 3)) ? &arrowUp : &arrowDown;
	    }

	    if ((w1 != w2) || (h1 != h2)) {
		csMask |= CS_LAYOUT | CS_DISPLAY;
	    } else if (ptr1 != ptr2) {
		csMask |= CS_DISPLAY;
	    }
	}
	if (1 /* column->border.obj != NULL */ /* NEVER NULL */) {
	    Tk_3DBorder border1, border2;
	    border1 = PerStateBorder_ForState(tree, &column->border,
		stateOld, NULL);
	    border2 = PerStateBorder_ForState(tree, &column->border,
		stateNew, NULL);
	    if (border1 != border2) {
		csMask |= CS_DISPLAY;
	    }
	}
	if (csMask & CS_LAYOUT)
	    mask |= COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT;
	if (csMask & CS_DISPLAY)
	    mask |= COLU_CONF_DISPLAY;
#ifdef THEME
	if (tree->useTheme)
	    mask |= COLU_CONF_DISPLAY; /* assume no size changes */
#endif
    }
#endif

#ifdef COL_LAYOUT
    if (mask & (COLU_CONF_NWIDTH | COLU_CONF_TWIDTH))
	mask |= COLU_CONF_NHEIGHT;
    if (mask & (COLU_CONF_JUSTIFY | COLU_CONF_TEXT))
	column->textLayoutInvalid = TRUE;
#endif

    if (mask & COLU_CONF_NWIDTH)
	column->neededWidth = -1;
    if (mask & COLU_CONF_NHEIGHT) {
	column->neededHeight = -1;
	tree->headerHeight = -1;
    }

    if (mask & COLU_CONF_JUSTIFY)
	Tree_DInfoChanged(tree, DINFO_INVALIDATE | DINFO_OUT_OF_DATE);

    /* -stepwidth and  -widthHack */
    if (mask & COLU_CONF_RANGES)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    /* Redraw everything */
    if (mask & (COLU_CONF_TWIDTH | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT)) {
	tree->widthOfColumns = -1;
	Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH | DINFO_DRAW_HEADER);
    }

    /* Redraw header only */
    else if (mask & COLU_CONF_DISPLAY) {
	Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
    }

    return TCL_OK;
}

static Column *Column_Alloc(TreeCtrl *tree)
{
    Column *column;

    column = (Column *) ckalloc(sizeof(Column));
    memset(column, '\0', sizeof(Column));
    column->tree = tree;
    column->optionTable = Tk_CreateOptionTable(tree->interp, columnSpecs);
    if (Tk_InitOptions(tree->interp, (char *) column, column->optionTable,
		tree->tkwin) != TCL_OK) {
	WFREE(column, Column);
	return NULL;
    }
#if 0
    if (Tk_SetOptions(header->tree->interp, (char *) column,
		column->optionTable, 0,
		NULL, header->tree->tkwin, &savedOptions,
		(int *) NULL) != TCL_OK) {
	WFREE(column, Column);
	return NULL;
    }
#endif
    column->neededWidth = column->neededHeight = -1;
    tree->headerHeight = tree->widthOfColumns = -1;
    tree->columnCount++;
    return column;
}

static Column *Column_Free(Column *column)
{
    TreeCtrl *tree = column->tree;
    Column *next = column->next;

    Column_FreeColors(column->itemBgColor, column->itemBgCount);
    if (column->bitmapGC != None)
	Tk_FreeGC(tree->display, column->bitmapGC);
    if (column->image != NULL)
	Tk_FreeImage(column->image);
#ifdef COL_LAYOUT
    if (column->textLayout != NULL)
	TextLayout_Free(column->textLayout);
#endif
    Tk_FreeConfigOptions((char *) column, column->optionTable, tree->tkwin);
    WFREE(column, Column);
    tree->columnCount--;
    return next;
}

int TreeColumn_FixedWidth(TreeColumn column_)
{
    return ((Column *) column_)->widthObj ? ((Column *) column_)->width : -1;
}

int TreeColumn_MinWidth(TreeColumn column_)
{
    return ((Column *) column_)->minWidthObj ? ((Column *) column_)->minWidth : -1;
}

int TreeColumn_StepWidth(TreeColumn column_)
{
    return ((Column *) column_)->stepWidthObj ? ((Column *) column_)->stepWidth : -1;
}

#ifdef COL_LAYOUT
static void Column_UpdateTextLayout(Column *column, int width)
{
    Tk_Font tkfont;
    char *text = column->text;
    int textLen = column->textLen;
    int justify = column->justify;
    int maxLines = MAX(column->textLines, 0); /* -textlines */
    int wrap = TEXT_WRAP_WORD; /* -textwrap */
    int flags = 0;
    int i, multiLine = FALSE;

    if (column->textLayout != NULL) {
	TextLayout_Free(column->textLayout);
	column->textLayout = NULL;
    }

    if ((text == NULL) || (textLen == 0))
	return;

    for (i = 0; i < textLen; i++) {
	if ((text[i] == '\n') || (text[i] == '\r')) {
	    multiLine = TRUE;
	    break;
	}
    }

    if (!multiLine && ((maxLines == 1) || (!width || (width >= column->textWidth))))
	return;

    tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;

    if (wrap == TEXT_WRAP_WORD)
	flags |= TK_WHOLE_WORDS;

    column->textLayout = TextLayout_Compute(tkfont, text,
	    Tcl_NumUtfChars(text, textLen), width, justify, maxLines, flags);
}
#endif

static void Column_GetArrowSize(Column *column, int *widthPtr, int *heightPtr)
{
    TreeCtrl *tree = column->tree;
    int state = Column_MakeState(column);
    int arrowWidth = -1, arrowHeight;
    Tk_Image image;
    Pixmap bitmap;

    /* image > bitmap > theme > draw */
    image = PerStateImage_ForState(tree, &column->arrowImage,
	state, NULL);
    if (image != NULL) {
	Tk_SizeOfImage(image, &arrowWidth, &arrowHeight);
    }
    if (arrowWidth == -1) {
	bitmap = PerStateBitmap_ForState(tree, &column->arrowBitmap,
	    state, NULL);
	if (bitmap != None) {
	    Tk_SizeOfBitmap(tree->display, bitmap, &arrowWidth, &arrowHeight);
	}
    }
#ifdef THEME
    /* FIXME: theme size */
#endif
    if (arrowWidth == -1) {
	Tk_Font tkfont = column->tkfont ? column->tkfont : tree->tkfont;
	Tk_FontMetrics fm;
	Tk_GetFontMetrics(tkfont, &fm);
	arrowWidth = (fm.linespace + column->textPadY[PAD_TOP_LEFT] +
	    column->textPadY[PAD_BOTTOM_RIGHT] + column->borderWidth * 2) / 2;
	if (!(arrowWidth & 1))
	    arrowWidth--;
	arrowHeight = arrowWidth;
    }

    (*widthPtr) = arrowWidth;
    (*heightPtr) = arrowHeight;
}

struct Layout
{
    Tk_Font tkfont;
    Tk_FontMetrics fm;
    int width; /* Provided by caller */
    int height; /* Provided by caller */
    int textLeft;
    int textWidth;
    int bytesThatFit;
    int imageLeft;
    int imageWidth;
    int arrowLeft;
    int arrowWidth;
    int arrowHeight;
};

struct LayoutPart
{
    int padX[2];
    int padY[2];
    int width;
    int height;
    int left;
    int top;
};

static void Column_DoLayoutH(Column *column, struct Layout *layout)
{
    struct LayoutPart *parts[3];
    struct LayoutPart partArrow, partImage, partText;
    int i, padList[4], widthList[3], n = 0;
    int iArrow = -1, iImage = -1, iText = -1;
    int left, right;
    int widthForText = 0;

    if (column->arrow != ARROW_NONE) {
	Column_GetArrowSize(column, &partArrow.width, &partArrow.height);
	partArrow.padX[PAD_TOP_LEFT] = column->arrowPadX[PAD_TOP_LEFT];
	partArrow.padX[PAD_BOTTOM_RIGHT] = column->arrowPadX[PAD_BOTTOM_RIGHT];
	partArrow.padY[PAD_TOP_LEFT] = column->arrowPadY[PAD_TOP_LEFT];
	partArrow.padY[PAD_BOTTOM_RIGHT] = column->arrowPadY[PAD_BOTTOM_RIGHT];
    }
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_LEFT)) {
	parts[n] = &partArrow;
	padList[n] = partArrow.padX[PAD_TOP_LEFT];
	padList[n + 1] = partArrow.padX[PAD_BOTTOM_RIGHT];
	iArrow = n++;
    }
    if ((column->image != NULL) || (column->bitmap != None)) {
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &partImage.width, &partImage.height);
	else
	    Tk_SizeOfBitmap(column->tree->display, column->bitmap, &partImage.width, &partImage.height);
	partImage.padX[PAD_TOP_LEFT] = column->imagePadX[PAD_TOP_LEFT];
	partImage.padX[PAD_BOTTOM_RIGHT] = column->imagePadX[PAD_BOTTOM_RIGHT];
	partImage.padY[PAD_TOP_LEFT] = column->imagePadY[PAD_TOP_LEFT];
	partImage.padY[PAD_BOTTOM_RIGHT] = column->imagePadY[PAD_BOTTOM_RIGHT];
	parts[n] = &partImage;
	padList[n] = MAX(partImage.padX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = partImage.padX[PAD_BOTTOM_RIGHT];
	iImage = n++;
    }
    if (column->textLen > 0) {
	struct LayoutPart *parts2[3];
	int n2 = 0;

	partText.padX[PAD_TOP_LEFT] = column->textPadX[PAD_TOP_LEFT];
	partText.padX[PAD_BOTTOM_RIGHT] = column->textPadX[PAD_BOTTOM_RIGHT];
	partText.padY[PAD_TOP_LEFT] = column->textPadY[PAD_TOP_LEFT];
	partText.padY[PAD_BOTTOM_RIGHT] = column->textPadY[PAD_BOTTOM_RIGHT];

	/* Calculate space for the text */
	if (iArrow != -1)
	    parts2[n2++] = &partArrow;
	if (iImage != -1)
	    parts2[n2++] = &partImage;
	parts2[n2++] = &partText;
	if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_RIGHT))
	    parts2[n2++] = &partArrow;
	widthForText = layout->width;
	for (i = 0; i < n2; i++) {
	    if (i)
		widthForText -= MAX(parts2[i]->padX[0], parts2[i-1]->padX[1]);
	    else
		widthForText -= parts2[i]->padX[0];
	    if (parts2[i] != &partText)
		widthForText -= parts2[i]->width;
	}
	widthForText -= parts2[n2-1]->padX[1];
    }
    layout->bytesThatFit = 0;
    if (widthForText > 0) {
#ifdef COL_LAYOUT
	if (column->textLayoutInvalid || (column->textLayoutWidth != widthForText)) {
	    Column_UpdateTextLayout(column, widthForText);
	    column->textLayoutInvalid = FALSE;
	    column->textLayoutWidth = widthForText;
	}
	if (column->textLayout != NULL) {
	    TextLayout_Size(column->textLayout, &partText.width, &partText.height);
	    parts[n] = &partText;
	    padList[n] = MAX(partText.padX[PAD_TOP_LEFT], padList[n]);
	    padList[n + 1] = partText.padX[PAD_BOTTOM_RIGHT];
	    iText = n++;
	} else {
#endif
	layout->tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;
	Tk_GetFontMetrics(layout->tkfont, &layout->fm);
	if (widthForText >= column->textWidth) {
	    partText.width = column->textWidth;
	    partText.height = layout->fm.linespace;
	    layout->bytesThatFit = column->textLen;
	} else {
	    partText.width = widthForText;
	    partText.height = layout->fm.linespace;
	    layout->bytesThatFit = Ellipsis(layout->tkfont, column->text,
		    column->textLen, &partText.width, "...", FALSE);
	}
	parts[n] = &partText;
	padList[n] = MAX(partText.padX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = partText.padX[PAD_BOTTOM_RIGHT];
	iText = n++;
#ifdef COL_LAYOUT
    }
#endif
    }
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_RIGHT)) {
	parts[n] = &partArrow;
	padList[n] = MAX(partArrow.padX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = partArrow.padX[PAD_BOTTOM_RIGHT];
	iArrow = n++;
    }

    if (n == 0)
	return;

    for (i = 0; i < n; i++) {
	padList[i] = parts[i]->padX[0];
	if (i)
	    padList[i] = MAX(padList[i], parts[i-1]->padX[1]);
	padList[i + 1] = parts[i]->padX[1];
	widthList[i] = parts[i]->width;
    }
    if (iText != -1) {
	switch (column->justify) {
	    case TK_JUSTIFY_LEFT:
		partText.left = 0;
		break;
	    case TK_JUSTIFY_RIGHT:
		partText.left = layout->width;
		break;
	    case TK_JUSTIFY_CENTER:
		if (iImage == -1)
		    partText.left = (layout->width - partText.width) / 2;
		else
		    partText.left = (layout->width - partImage.width -
			    padList[iText] - partText.width) / 2 + partImage.width +
			padList[iText];
		break;
	}
    }

    if (iImage != -1) {
	switch (column->justify) {
	    case TK_JUSTIFY_LEFT:
		partImage.left = 0;
		break;
	    case TK_JUSTIFY_RIGHT:
		partImage.left = layout->width;
		break;
	    case TK_JUSTIFY_CENTER:
		if (iText == -1)
		    partImage.left = (layout->width - partImage.width) / 2;
		else
		    partImage.left = (layout->width - partImage.width -
			    padList[iText] - partText.width) / 2;
		break;
	}
    }

    if (iArrow == -1)
	goto finish;

    switch (column->justify) {
	case TK_JUSTIFY_LEFT:
	    switch (column->arrowSide) {
		case SIDE_LEFT:
		    partArrow.left = 0;
		    break;
		case SIDE_RIGHT:
		    switch (column->arrowGravity) {
			case SIDE_LEFT:
			    partArrow.left = 0;
			    break;
			case SIDE_RIGHT:
			    partArrow.left = layout->width;
			    break;
		    }
		    break;
	    }
	    break;
	case TK_JUSTIFY_RIGHT:
	    switch (column->arrowSide) {
		case SIDE_LEFT:
		    switch (column->arrowGravity) {
			case SIDE_LEFT:
			    partArrow.left = 0;
			    break;
			case SIDE_RIGHT:
			    partArrow.left = layout->width;
			    break;
		    }
		    break;
		case SIDE_RIGHT:
		    partArrow.left = layout->width;
		    break;
	    }
	    break;
	case TK_JUSTIFY_CENTER:
	    switch (column->arrowSide) {
		case SIDE_LEFT:
		    switch (column->arrowGravity) {
			case SIDE_LEFT:
			    partArrow.left = 0;
			    break;
			case SIDE_RIGHT:
			    if (n == 3)
				partArrow.left =
				    (layout->width - widthList[1] - padList[2] -
					    widthList[2]) / 2 - padList[1] - widthList[0];
			    else if (n == 2)
				partArrow.left =
				    (layout->width - widthList[1]) / 2 -
				    padList[1] - widthList[0];
			    else
				partArrow.left = layout->width;
			    break;
		    }
		    break;
		case SIDE_RIGHT:
		    switch (column->arrowGravity) {
			case SIDE_LEFT:
			    if (n == 3)
				partArrow.left =
				    (layout->width - widthList[0] - padList[1] -
					    widthList[1]) / 2 + widthList[0] + padList[1] +
				    widthList[1] + padList[2];
			    else if (n == 2)
				partArrow.left =
				    (layout->width - widthList[0]) / 2 +
				    widthList[0] + padList[1];
			    else
				partArrow.left = 0;
			    break;
			case SIDE_RIGHT:
			    partArrow.left = layout->width;
			    break;
		    }
		    break;
	    }
	    break;
    }

finish:
    right = layout->width - padList[n];
    for (i = n - 1; i >= 0; i--) {
	if (parts[i]->left + parts[i]->width > right)
	    parts[i]->left = right - parts[i]->width;
	right -= parts[i]->width + padList[i];
    }
    left = padList[0];
    for (i = 0; i < n; i++) {
	if (parts[i]->left < left)
	    parts[i]->left = left;
	left += parts[i]->width + padList[i + 1];
    }

    if (iArrow != -1) {
	layout->arrowLeft = partArrow.left;
	layout->arrowWidth = partArrow.width;
	layout->arrowHeight = partArrow.height;
    }
    if (iImage != -1) {
	layout->imageLeft = partImage.left;
	layout->imageWidth = partImage.width;
    }
    if (iText != -1) {
	layout->textLeft = partText.left;
	layout->textWidth = partText.width;
    }
}

int TreeColumn_NeededWidth(TreeColumn column_)
{
    Column *column = (Column *) column_;
    TreeCtrl *tree = column->tree;
    int i, widthList[3], padList[4], n = 0;
    int arrowWidth, arrowHeight;

    if (column->neededWidth >= 0)
	return column->neededWidth;

    for (i = 0; i < 3; i++) widthList[i] = 0;
    for (i = 0; i < 4; i++) padList[i] = 0;

    if (column->arrow != ARROW_NONE)
	Column_GetArrowSize(column, &arrowWidth, &arrowHeight);
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_LEFT)) {
	widthList[n] = arrowWidth;
	padList[n] = column->arrowPadX[PAD_TOP_LEFT];
	padList[n + 1] = column->arrowPadX[PAD_BOTTOM_RIGHT];
	n++;
    }
    if ((column->image != NULL) || (column->bitmap != None)) {
	int imgWidth, imgHeight;
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &imgWidth, &imgHeight);
	else
	    Tk_SizeOfBitmap(tree->display, column->bitmap, &imgWidth, &imgHeight);
	padList[n] = MAX(column->imagePadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = column->imagePadX[PAD_BOTTOM_RIGHT];
	widthList[n] = imgWidth;
	n++;
    }
    if (column->textLen > 0) {
	padList[n] = MAX(column->textPadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = column->textPadX[PAD_BOTTOM_RIGHT];
#ifdef COL_LAYOUT
	if (column->textLayoutInvalid || (column->textLayoutWidth != 0)) {
	    Column_UpdateTextLayout(column, 0);
	    column->textLayoutInvalid = FALSE;
	    column->textLayoutWidth = 0;
	}
	if (column->textLayout != NULL)
	    TextLayout_Size(column->textLayout, &widthList[n], NULL);
	else
#endif
	widthList[n] = column->textWidth;
	n++;
    }
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_RIGHT)) {
	widthList[n] = arrowWidth;
	padList[n] = MAX(column->arrowPadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = column->arrowPadX[PAD_BOTTOM_RIGHT];
	n++;
    }

    column->neededWidth = 0;
    for (i = 0; i < n; i++)
	column->neededWidth += widthList[i] + padList[i];
    column->neededWidth += padList[n];

    /* Notice I'm not considering column->borderWidth. */

    return column->neededWidth;
}

int TreeColumn_NeededHeight(TreeColumn column_)
{
    Column *column = (Column *) column_;
    TreeCtrl *tree = column->tree;
#ifdef THEME
    int margins[4];
#endif

    if (column->neededHeight >= 0)
	return column->neededHeight;

    column->neededHeight = 0;
    if (column->arrow != ARROW_NONE) {
	int arrowWidth, arrowHeight;
	Column_GetArrowSize(column, &arrowWidth, &arrowHeight);
	arrowHeight += column->arrowPadY[PAD_TOP_LEFT]
	    + column->arrowPadY[PAD_BOTTOM_RIGHT];
	column->neededHeight = MAX(column->neededHeight, arrowHeight);
    }
    if ((column->image != NULL) || (column->bitmap != None)) {
	int imgWidth, imgHeight;
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &imgWidth, &imgHeight);
	else
	    Tk_SizeOfBitmap(tree->display, column->bitmap, &imgWidth, &imgHeight);
	imgHeight += column->imagePadY[PAD_TOP_LEFT]
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	column->neededHeight = MAX(column->neededHeight, imgHeight);
    }
    if (column->text != NULL) {
#ifdef COL_LAYOUT
	struct Layout layout;
	layout.width = TreeColumn_UseWidth(column_);
	layout.height = -1;
	Column_DoLayoutH(column, &layout);
	if (column->textLayout != NULL) {
	    int height;
	    TextLayout_Size(column->textLayout, NULL, &height);
	    height += column->textPadY[PAD_TOP_LEFT]
		+ column->textPadY[PAD_BOTTOM_RIGHT];
	    column->neededHeight = MAX(column->neededHeight, height);
	} else {
#endif
	Tk_Font tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;
	Tk_FontMetrics fm;
	Tk_GetFontMetrics(tkfont, &fm);
	fm.linespace += column->textPadY[PAD_TOP_LEFT]
	    + column->textPadY[PAD_BOTTOM_RIGHT];
	column->neededHeight = MAX(column->neededHeight, fm.linespace);
#ifdef COL_LAYOUT
	}
#endif
    }
#ifdef THEME
    if (column->tree->useTheme &&
	(TreeTheme_GetHeaderContentMargins(tree, column->state, margins) == TCL_OK)) {
#ifdef WIN32
	/* I'm hacking these margins since the default XP theme does not give
	 * reasonable ContentMargins for HP_HEADERITEM */
	int bw = MAX(column->borderWidth, 3);
	margins[1] = MAX(margins[1], bw);
	margins[3] = MAX(margins[3], bw);
#endif /* WIN32 */
	column->neededHeight += margins[1] + margins[3];
    } else
#endif /* THEME */
    column->neededHeight += column->borderWidth * 2;

    return column->neededHeight;
}

int TreeColumn_UseWidth(TreeColumn column_)
{
    /* Update layout if needed */
    (void) Tree_WidthOfColumns(((Column *) column_)->tree);

    return ((Column *) column_)->useWidth;
}

void TreeColumn_SetUseWidth(TreeColumn column_, int width)
{
    ((Column *) column_)->useWidth = width;
}

Tk_Justify TreeColumn_Justify(TreeColumn column_)
{
    return ((Column *) column_)->justify;
}

int TreeColumn_WidthHack(TreeColumn column_)
{
    return ((Column *) column_)->widthHack;
}

int TreeColumn_Squeeze(TreeColumn column_)
{
    return ((Column *) column_)->squeeze;
}

GC TreeColumn_BackgroundGC(TreeColumn column_, int index)
{
    Column *column = (Column *) column_;
    XColor *color;

    if ((index < 0) || (column->itemBgCount == 0))
	return None;
    color = column->itemBgColor[index % column->itemBgCount];
    if (color == NULL)
	return None;
    return Tk_GCForColor(color, Tk_WindowId(column->tree->tkwin));
}

int TreeColumn_Visible(TreeColumn column_)
{
    return ((Column *) column_)->visible;
}

int TreeColumn_Index(TreeColumn column_)
{
    return ((Column *) column_)->index;
}

int TreeColumnCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = {
	"bbox", "cget", "configure", "create", "delete", "index", "move",
	"neededwidth", "width", (char *) NULL
    };
    enum {
	COMMAND_BBOX, COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_CREATE,
	COMMAND_DELETE,	COMMAND_INDEX, COMMAND_MOVE, COMMAND_NEEDEDWIDTH,
	COMMAND_WIDTH
    };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
		&index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case COMMAND_BBOX:
	{
	    int x = 0;
	    Column *column, *walk;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column, CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    if (!tree->showHeader || !column->visible)
		break;
	    /* Update layout */
	    Tree_WidthOfColumns(tree);
	    walk = (Column *) tree->columns;
	    while (walk != column) {
		if (column->visible)
		    x += walk->useWidth;
		walk = walk->next;
	    }
	    FormatResult(interp, "%d %d %d %d",
		    x - tree->xOrigin,
		    tree->inset,
		    x - tree->xOrigin + column->useWidth,
		    tree->inset + Tree_HeaderHeight(tree));
	    break;
	}

	case COMMAND_CGET:
	{
	    TreeColumn column;
	    Tcl_Obj *resultObjPtr;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column option");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column, 0) != TCL_OK)
		return TCL_ERROR;
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) column,
		    ((Column *) column)->optionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	case COMMAND_CONFIGURE:
	{
	    Column *column;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column ?option? ?value?");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column,
		    0) != TCL_OK)
		return TCL_ERROR;
	    if (objc <= 5) {
		Tcl_Obj *resultObjPtr;
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) column,
			column->optionTable,
			(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    return Column_Config(column, objc - 4, objv + 4, FALSE);
	}

	case COMMAND_CREATE:
	{
	    Column *walk = (Column *) tree->columns;
	    Column *column;

	    column = Column_Alloc(tree);
	    if (Column_Config(column, objc - 3, objv + 3, TRUE) != TCL_OK)
	    {
		Column_Free(column);
		return TCL_ERROR;
	    }

	    if (walk == NULL) {
		column->index = 0;
		tree->columns = (TreeColumn) column;
	    } else {
		while (walk->next != NULL) {
		    walk = walk->next;
		}
		walk->next = column;
		column->index = walk->index + 1;
	    }
	    ((Column *) tree->columnTail)->index++;
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(column->index));
	    break;
	}

	case COMMAND_DELETE:
	{
	    int columnIndex;
	    Column *column, *prev;
	    TreeItem item;
	    TreeItemColumn itemColumn;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column,
			CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    columnIndex = column->index;
	    if (columnIndex > 0) {
		prev = (Column *) Tree_FindColumn(tree, columnIndex - 1);
		prev->next = column->next;
	    } else {
		tree->columns = (TreeColumn) column->next;
	    }
	    Column_Free(column);

	    if (columnIndex == tree->columnTree)
		tree->columnTree = -1;

	    /* Delete all TreeItemColumns */
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		itemColumn = TreeItem_FindColumn(tree, item, columnIndex);
		if (itemColumn != NULL)
		    TreeItem_RemoveColumn(tree, item, itemColumn);
		hPtr = Tcl_NextHashEntry(&search);
	    }

	    /* Renumber columns */
	    column = (Column *) tree->columns;
	    columnIndex = 0;
	    while (column != NULL) {
		column->index = columnIndex++;
		column = column->next;
	    }
	    ((Column *) tree->columnTail)->index--;

	    tree->widthOfColumns = tree->headerHeight = -1;
	    Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH);
	    break;
	}

	case COMMAND_WIDTH:
	{
	    Column *column;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column, 0) != TCL_OK)
		return TCL_ERROR;
	    /* Update layout if needed */
	    (void) Tree_WidthOfColumns(tree);
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(column->useWidth));
	    break;
	}

	case COMMAND_INDEX:
	{
	    Column *column;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column, 0) != TCL_OK)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(column->index));
	    break;
	}

	/* T column move C before */
	case COMMAND_MOVE:
	{
	    Column *move, *before;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;
	    TreeItem item;
	    int numStyles;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column before");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &move, CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    if (TreeColumn_FromObj(tree, objv[4], (TreeColumn *) &before, 0) != TCL_OK)
		return TCL_ERROR;
	    if (move == before)
		break;
	    if (move->index == before->index - 1)
		break;

	    /* Move the column in every item */
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		TreeItem_MoveColumn(tree, item, move->index, before->index);
		hPtr = Tcl_NextHashEntry(&search);
	    }

	    /* Re-order -defaultstyle */
	    numStyles = tree->defaultStyle.numStyles;
	    if ((numStyles > 0) && ((before->index <numStyles) ||
		    (move->index < numStyles))) {
		TreeStyle style, *styles;
		int i, j;
		Tcl_Obj *staticObjv[STATIC_SIZE], **objv = staticObjv;

		/* Case 1: move existing */
		if ((before->index <= numStyles) && (move->index < numStyles)) {
		    styles = tree->defaultStyle.styles;
		    style = styles[move->index];
		    for (i = move->index; i < numStyles - 1; i++)
			styles[i] = styles[i + 1];
		    j = before->index;
		    if (move->index < before->index)
			j--;
		    for (i = numStyles - 1; i > j; i--)
		        styles[i] = styles[i - 1];
		    styles[j] = style;

		/* Case 2: insert empty between existing */
		} else if (before->index < numStyles) {
		    numStyles++;
		    styles = (TreeStyle *) ckalloc(numStyles * sizeof(TreeStyle));
		    for (i = 0; i < before->index; i++)
			styles[i] = tree->defaultStyle.styles[i];
		    styles[i++] = NULL;
		    for (; i < numStyles; i++)
		        styles[i] = tree->defaultStyle.styles[i - 1];

		/* Case 3: move existing past end */
		} else {
		    numStyles += before->index - numStyles;
		    styles = (TreeStyle *) ckalloc(numStyles * sizeof(TreeStyle));
		    style = tree->defaultStyle.styles[move->index];
		    for (i = 0; i < move->index; i++)
			styles[i] = tree->defaultStyle.styles[i];
		    for (; i < tree->defaultStyle.numStyles - 1; i++)
			styles[i] = tree->defaultStyle.styles[i + 1];
		    for (; i < numStyles - 1; i++)
			styles[i] = NULL;
		    styles[i] = style;
		}
		Tcl_DecrRefCount(tree->defaultStyle.stylesObj);
		STATIC_ALLOC(objv, Tcl_Obj *, numStyles);
		for (i = 0; i < numStyles; i++) {
		    if (styles[i] != NULL)
			objv[i] = TreeStyle_ToObj(styles[i]);
		    else
			objv[i] = Tcl_NewObj();
		}
		tree->defaultStyle.stylesObj = Tcl_NewListObj(numStyles, objv);
		Tcl_IncrRefCount(tree->defaultStyle.stylesObj);
		STATIC_FREE(objv, Tcl_Obj *, numStyles);
		if (styles != tree->defaultStyle.styles) {
		    ckfree((char *) tree->defaultStyle.styles);
		    tree->defaultStyle.styles = styles;
		    tree->defaultStyle.numStyles = numStyles;
		}
	    }

	    {
		Column *prevM = NULL, *prevB = NULL;
		Column *last = NULL, *prev, *walk;
		Column *columnTree = NULL;
		int index;

		prev = NULL;
		walk = (Column *) tree->columns;
		while (walk != NULL) {
		    if (walk == move)
			prevM = prev;
		    if (walk == before)
			prevB = prev;
		    if (walk->index == tree->columnTree)
			columnTree = walk;
		    prev = walk;
		    if (walk->next == NULL)
			last = walk;
		    walk = walk->next;
		}
		if (prevM == NULL)
		    tree->columns = (TreeColumn) move->next;
		else
		    prevM->next = move->next;
		if (before == (Column *) tree->columnTail) {
		    last->next = move;
		    move->next = NULL;
		} else {
		    if (prevB == NULL)
			tree->columns = (TreeColumn) move;
		    else
			prevB->next = move;
		    move->next = before;
		}

		/* Renumber columns */
		walk = (Column *) tree->columns;
		index = 0;
		while (walk != NULL) {
		    walk->index = index++;
		    walk = walk->next;
		}

		if (columnTree != NULL)
		    tree->columnTree = columnTree->index;
	    }
	    if (move->visible) {
		/* Must update column widths because of expansion. */
		/* Also update columnTreeLeft. */
		tree->widthOfColumns = -1;
		Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH |
			DINFO_INVALIDATE | DINFO_OUT_OF_DATE);
		/* BUG 784245 */
		Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
	    }
	    break;
	}

	case COMMAND_NEEDEDWIDTH:
	{
	    Column *column;
	    int width;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column, 0) != TCL_OK)
		return TCL_ERROR;
	    /* Update layout if needed */
	    (void) Tree_TotalWidth(tree);
	    width = TreeColumn_WidthOfItems((TreeColumn) column);
	    width = MAX(width, TreeColumn_NeededWidth((TreeColumn) column));
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(width));
	    break;
	}
    }

    return TCL_OK;
}

static void Column_DrawArrow(Column *column, Drawable drawable, int x, int y,
    struct Layout layout)
{
    TreeCtrl *tree = column->tree;
    int height = tree->headerHeight;
    int sunken = column->state == COLUMN_STATE_PRESSED;
    Tk_Image image = NULL;
    Pixmap bitmap;
    Tk_3DBorder border;
    int state = Column_MakeState(column);
    int arrowPadY = column->arrowPadY[PAD_TOP_LEFT] +
	column->arrowPadY[PAD_BOTTOM_RIGHT];

    if (column->arrow == ARROW_NONE)
	return;

    image = PerStateImage_ForState(tree, &column->arrowImage, state, NULL);
    if (image != NULL) {
	Tk_RedrawImage(image, 0, 0, layout.arrowWidth, layout.arrowHeight,
	    drawable,
	    x + layout.arrowLeft + sunken,
	    y + (height - (layout.arrowHeight + arrowPadY)) / 2 + sunken);
	return;
    }

    bitmap = PerStateBitmap_ForState(tree, &column->arrowBitmap, state, NULL);
    if (bitmap != None) {
	int bx, by;
	XGCValues gcValues;
	GC gc;
	gcValues.clip_mask = bitmap;
	gcValues.graphics_exposures = False;
	gc = Tk_GetGC(tree->tkwin, GCClipMask | GCGraphicsExposures, &gcValues);
	bx = x + layout.arrowLeft + sunken;
	by = y + (height - (layout.arrowHeight + arrowPadY)) / 2 + sunken;
	XSetClipOrigin(tree->display, gc, bx, by);
	XCopyPlane(tree->display, bitmap, drawable, gc,
		0, 0,
		(unsigned int) layout.arrowWidth, (unsigned int) layout.arrowHeight,
		bx, by, 1);
	Tk_FreeGC(tree->display, gc);
	return;
    }

#ifdef THEME
    if (tree->useTheme) {
	if (TreeTheme_DrawHeaderArrow(tree, drawable,
	    column->arrow == ARROW_UP, x + layout.arrowLeft,
	    y + (height - (layout.arrowHeight + arrowPadY)) / 2, layout.arrowWidth,
	    layout.arrowHeight) == TCL_OK)
	    return;
    }
#endif

    if (1) {
	int arrowWidth = layout.arrowWidth;
	int arrowHeight = layout.arrowHeight;
	int arrowTop = y + (height - (layout.arrowHeight + arrowPadY)) / 2 + column->arrowPadY[PAD_TOP_LEFT];
	int arrowBottom = arrowTop + arrowHeight;
	XPoint points[5];
	int color1 = 0, color2 = 0;
	int i;

	switch (column->arrow) {
	    case ARROW_UP:
		points[0].x = x + layout.arrowLeft;
		points[0].y = arrowBottom - 1;
		points[1].x = x + layout.arrowLeft + arrowWidth / 2;
		points[1].y = arrowTop - 1;
		color1 = TK_3D_DARK_GC;
		points[4].x = x + layout.arrowLeft + arrowWidth / 2;
		points[4].y = arrowTop - 1;
		points[3].x = x + layout.arrowLeft + arrowWidth - 1;
		points[3].y = arrowBottom - 1;
		points[2].x = x + layout.arrowLeft;
		points[2].y = arrowBottom - 1;
		color2 = TK_3D_LIGHT_GC;
		break;
	    case ARROW_DOWN:
		points[0].x = x + layout.arrowLeft + arrowWidth - 1;
		points[0].y = arrowTop;
		points[1].x = x + layout.arrowLeft + arrowWidth / 2;
		points[1].y = arrowBottom;
		color1 = TK_3D_LIGHT_GC;
		points[2].x = x + layout.arrowLeft + arrowWidth - 1;
		points[2].y = arrowTop;
		points[3].x = x + layout.arrowLeft;
		points[3].y = arrowTop;
		points[4].x = x + layout.arrowLeft + arrowWidth / 2;
		points[4].y = arrowBottom;
		color2 = TK_3D_DARK_GC;
		break;
	}
	for (i = 0; i < 5; i++) {
	    points[i].x += sunken;
	    points[i].y += sunken;
	}

	border = PerStateBorder_ForState(tree, &column->border,
	    Column_MakeState(column), NULL);
	if (border == NULL)
	    border = tree->border;
	XDrawLines(tree->display, drawable,
		Tk_3DBorderGC(tree->tkwin, border, color2),
		points + 2, 3, CoordModeOrigin);
	XDrawLines(tree->display, drawable,
		Tk_3DBorderGC(tree->tkwin, border, color1),
		points, 2, CoordModeOrigin);
    }
}

#ifdef COLUMN_DRAG_IMAGE
static void Column_Draw(Column *column, Drawable drawable, int x, int y, int dragImage)
#else
static void Column_Draw(Column *column, Drawable drawable, int x, int y)
#endif
{
    TreeCtrl *tree = column->tree;
    int height = tree->headerHeight;
    struct Layout layout;
    int width = column->useWidth;
    int sunken = column->state == COLUMN_STATE_PRESSED;
    int relief = sunken ? TK_RELIEF_SUNKEN : TK_RELIEF_RAISED;
    Tk_3DBorder border;
#ifdef THEME
    int theme = TCL_ERROR;
#endif

    layout.width = width;
    layout.height = height;
    Column_DoLayoutH(column, &layout);

    border = PerStateBorder_ForState(tree, &column->border,
	Column_MakeState(column), NULL);
    if (border == NULL)
	border = tree->border;

#ifdef COLUMN_DRAG_IMAGE
    if (dragImage) {
	GC gc = Tk_GCForColor(tree->columnDrag.color, Tk_WindowId(tree->tkwin));
	XFillRectangle(tree->display, drawable, gc, x, y, width, height);
    } else {
#endif
#ifdef THEME
    if (tree->useTheme) {
	theme = TreeTheme_DrawHeaderItem(tree, drawable, column->state, x, y,
		width, height);
    }
    if (theme != TCL_OK)
#endif
    Tk_Fill3DRectangle(tree->tkwin, drawable, border,
	    x, y, width, height, 0, TK_RELIEF_FLAT);
#ifdef COLUMN_DRAG_IMAGE
    }
#endif

    if (column->image != NULL) {
	int imgW, imgH, ix, iy, h;
	Tk_SizeOfImage(column->image, &imgW, &imgH);
	ix = x + layout.imageLeft + sunken;
	h = column->imagePadY[PAD_TOP_LEFT] + imgH
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	iy = y + (height - h) / 2 + sunken;
	iy += column->imagePadY[PAD_TOP_LEFT];
	Tk_RedrawImage(column->image, 0, 0, imgW, imgH, drawable, ix, iy);
    } else if (column->bitmap != None) {
	int imgW, imgH, bx, by, h;

	Tk_SizeOfBitmap(tree->display, column->bitmap, &imgW, &imgH);
	bx = x + layout.imageLeft + sunken;
	h = column->imagePadY[PAD_TOP_LEFT] + imgH
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	by = y + (height - h) / 2 + sunken;
	by += column->imagePadY[PAD_TOP_LEFT];
	XSetClipOrigin(tree->display, column->bitmapGC, bx, by);
	XCopyPlane(tree->display, column->bitmap, drawable, column->bitmapGC,
		0, 0, (unsigned int) imgW, (unsigned int) imgH,
		bx, by, 1);
	XSetClipOrigin(tree->display, column->bitmapGC, 0, 0);
    }

#ifdef COL_LAYOUT
    if ((column->text != NULL) && (column->textLayout != NULL)) {
	int h;
	XGCValues gcValues;
	GC gc;
	unsigned long mask;
	TextLayout_Size(column->textLayout, NULL, &h);
	h += column->textPadY[PAD_TOP_LEFT] + column->textPadY[PAD_BOTTOM_RIGHT];
	gcValues.font = Tk_FontId(column->tkfont ? column->tkfont : tree->tkfont);
	gcValues.foreground = column->textColor->pixel;
	gcValues.graphics_exposures = False;
	mask = GCFont | GCForeground | GCGraphicsExposures;
	gc = Tk_GetGC(tree->tkwin, mask, &gcValues);
	TextLayout_Draw(tree->display, drawable, gc,
		column->textLayout,
		x + layout.textLeft + sunken,
		y + (height - h) / 2 + column->textPadY[PAD_TOP_LEFT] + sunken,
		0, -1);
	Tk_FreeGC(tree->display, gc);
    } else
#endif
    if ((column->text != NULL) && (layout.bytesThatFit != 0)) {
	XGCValues gcValues;
	GC gc;
	unsigned long mask;
	char staticStr[256], *text = staticStr;
	int textLen = column->textLen;
	char *ellipsis = "...";
	int ellipsisLen = strlen(ellipsis);
	int tx, ty, h;

	if (textLen + ellipsisLen > sizeof(staticStr))
	    text = ckalloc(textLen + ellipsisLen);
	memcpy(text, column->text, textLen);
	if (layout.bytesThatFit != textLen) {
	    textLen = abs(layout.bytesThatFit);
	    if (layout.bytesThatFit > 0) {
		memcpy(text + layout.bytesThatFit, ellipsis, ellipsisLen);
		textLen += ellipsisLen;
	    }
	}

	gcValues.font = Tk_FontId(layout.tkfont);
	gcValues.foreground = column->textColor->pixel;
	gcValues.graphics_exposures = False;
	mask = GCFont | GCForeground | GCGraphicsExposures;
	gc = Tk_GetGC(tree->tkwin, mask, &gcValues);
	tx = x + layout.textLeft + sunken;
	h = column->textPadY[PAD_TOP_LEFT] + layout.fm.linespace
	    + column->textPadY[PAD_BOTTOM_RIGHT];
	ty = y + (height - h) / 2 + layout.fm.ascent + sunken;
	ty += column->textPadY[PAD_TOP_LEFT];
	Tk_DrawChars(tree->display, drawable, gc,
		layout.tkfont, text, textLen, tx, ty);
	Tk_FreeGC(tree->display, gc);
	if (text != staticStr)
	    ckfree(text);
    }

#ifdef COLUMN_DRAG_IMAGE
    if (dragImage)
	return;
#endif

    Column_DrawArrow(column, drawable, x, y, layout);

#ifdef THEME
    if (theme != TCL_OK)
#endif
    Tk_Draw3DRectangle(tree->tkwin, drawable, border,
	    x, y, width, height, column->borderWidth, relief);
}

#ifdef COLUMN_DRAG_IMAGE

Tk_Image SetImageForColumn(TreeCtrl *tree, Column *column)
{
    Tk_PhotoHandle photoH;
    Pixmap pixmap;
    int width = column->useWidth; /* the entire column, not just what is visible */
    int height = tree->headerHeight;
    XImage *ximage;

    photoH = Tk_FindPhoto(tree->interp, "TreeCtrlColumnImage");
    if (photoH == NULL) {
	Tcl_GlobalEval(tree->interp, "image create photo TreeCtrlColumnImage");
	photoH = Tk_FindPhoto(tree->interp, "TreeCtrlColumnImage");
	if (photoH == NULL)
	    return NULL;
    }

    pixmap = Tk_GetPixmap(tree->display, Tk_WindowId(tree->tkwin),
	    width, height, Tk_Depth(tree->tkwin));

    Column_Draw(column, pixmap, 0, 0, TRUE);

    /* Pixmap -> XImage */
    ximage = XGetImage(tree->display, pixmap, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
    if (ximage == NULL)
	panic("ximage is NULL");

    /* XImage -> Tk_Image */
    XImage2Photo(tree->interp, photoH, ximage, tree->columnDrag.alpha);

    XDestroyImage(ximage);
    Tk_FreePixmap(tree->display, pixmap);

    return Tk_GetImage(tree->interp, tree->tkwin, "TreeCtrlColumnImage",
	NULL, (ClientData) NULL);
}

#if 0
Tk_PhotoHandle SetImageForHeader(TreeCtrl *tree)
{
    Tk_PhotoHandle photoH;
    Pixmap pixmap;
    int width = Tk_Width(tree->tkwin) - tree->inset * 2;
    int height = tree->headerHeight;
    int doubleBuffer = tree->doubleBuffer;
    XImage *ximage;

    photoH = Tk_FindPhoto(tree->interp, "TreeCtrlHeaderImage");
    if (photoH == NULL)
	return NULL;

    pixmap = Tk_GetPixmap(tree->display, Tk_WindowId(tree->tkwin),
	    width, height, Tk_Depth(tree->tkwin));

    tree->doubleBuffer = DOUBLEBUFFER_NONE;
    Tree_DrawHeader(tree, pixmap, 0 - tree->xOrigin - tree->inset, 0);
    tree->doubleBuffer = doubleBuffer;

    ximage = XGetImage(tree->display, pixmap, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
    if (ximage == NULL)
	panic("ximage is NULL");

    SetTkImageFromXImage(tree, photoH, ximage, 255);

    XDestroyImage(ximage);
    Tk_FreePixmap(tree->display, pixmap);

    return photoH;
}

void CompositeColumnImageWithHeaderImage(TreeCtrl *tree, Column *column)
{
    Tk_PhotoImageBlock photoBlock;
    Tk_PhotoHandle photoH1, photoH2;
    int x, y;

    photoH1 = SetImageForHeader(tree);
    photoH2 = SetImageForColumn(tree);

    if (!photoH1 || !photoH2)
	return;

    Tk_PhotoGetImage(photoH2, &photoBlock);

    TK_PHOTOPUTBLOCK(interp, photoH1, &photoBlock, column->dragOffset, 0,
	    photoBlock.width, photoBlock.height, TK_PHOTO_COMPOSITE_OVERLAY);
}
#endif
#endif

void Tree_DrawHeader(TreeCtrl *tree, Drawable drawable, int x, int y)
{
    Column *column = (Column *) tree->columns;
    Tk_Window tkwin = tree->tkwin;
    int minX, maxX, width, height;
    Drawable pixmap;
#ifdef COLUMN_DRAG_IMAGE
    int x2 = x;
#endif

    /* Update layout if needed */
    (void) Tree_HeaderHeight(tree);
    (void) Tree_WidthOfColumns(tree);

    minX = tree->inset;
    maxX = Tk_Width(tkwin) - tree->inset;

    if (tree->doubleBuffer == DOUBLEBUFFER_ITEM)
	pixmap = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
		Tk_Width(tkwin), tree->inset + tree->headerHeight, Tk_Depth(tkwin));
    else
	pixmap = drawable;

    while (column != NULL) {
	if (column->visible) {
	    if ((x < maxX) && (x + column->useWidth > minX))
#ifdef COLUMN_DRAG_IMAGE
		Column_Draw(column, pixmap, x, y, FALSE);
#else
		Column_Draw(column, pixmap, x, y);
#endif
	    x += column->useWidth;
	}
	column = column->next;
    }

    /* Draw "tail" column */
    if (x < maxX) {
	column = (Column *) tree->columnTail;
	width = maxX - x + column->borderWidth;
	height = tree->headerHeight;
#ifdef THEME
	if (tree->useTheme &&
	    (TreeTheme_DrawHeaderItem(tree, pixmap, 0, x, y, width, height) == TCL_OK)) {
	} else
#endif
	{
	    Tk_3DBorder border;
	    border = PerStateBorder_ForState(tree, &column->border,
		Column_MakeState(column), NULL);
	    if (border == NULL)
		border = tree->border;
	    Tk_Fill3DRectangle(tkwin, pixmap, border,
		    x, y, width, height, column->borderWidth, TK_RELIEF_RAISED);
	}
    }

#ifdef COLUMN_DRAG_IMAGE
    {
	Tk_Image image = NULL;
	int imageX = 0, imageW = 0, indDraw = FALSE, indX = 0;

	column = (Column *) tree->columns;
	while (column != NULL) {
	    if (column->visible) {
		if (tree->columnDrag.column == column->index) {
		    image = SetImageForColumn(tree, column);
		    imageX = x2;
		    imageW = column->useWidth;
		}
		if (tree->columnDrag.indColumn == column->index) {
		    indX = x2 - 1;
		    indDraw = TRUE;
		}
		x2 += column->useWidth;
	    }
	    if ((column->next == NULL) && (tree->columnDrag.indColumn == tree->columnCount)) {
		indX = x2 - 1;
		indDraw = TRUE;
	    }
	    column = column->next;
	}
	if (indDraw) {
	    GC gc = Tk_GCForColor(tree->columnDrag.indColor, Tk_WindowId(tree->tkwin));
	    XFillRectangle(tree->display, pixmap, gc,
		indX, y, 2, tree->headerHeight);
	}
	if (image != NULL) {
	    Tk_RedrawImage(image, 0, 0, imageW,
		tree->headerHeight, pixmap,
		imageX + tree->columnDrag.offset, y);
	    Tk_FreeImage(image);
	}
    }
#endif

    if (tree->doubleBuffer == DOUBLEBUFFER_ITEM) {
	XCopyArea(tree->display, pixmap, drawable,
		tree->copyGC, minX, y,
		maxX - minX, tree->headerHeight,
		tree->inset, y);

	Tk_FreePixmap(tree->display, pixmap);
    }
}

/* Calculate the maximum needed width of all ReallyVisible TreeItemColumns */
int TreeColumn_WidthOfItems(TreeColumn column_)
{
    Column *column = (Column *) column_;
    TreeCtrl *tree = column->tree;
    TreeItem item;
    TreeItemColumn itemColumn;
    int width;

    if (column->widthOfItems >= 0)
	return column->widthOfItems;

    column->widthOfItems = 0;
    item = tree->root;
    if (!TreeItem_ReallyVisible(tree, item))
	item = TreeItem_NextVisible(tree, item);
    while (item != NULL) {
	itemColumn = TreeItem_FindColumn(tree, item, column->index);
	if (itemColumn != NULL) {
	    width = TreeItemColumn_NeededWidth(tree, item, itemColumn);
	    if (column->index == tree->columnTree)
		width += TreeItem_Indent(tree, item);
	    column->widthOfItems = MAX(column->widthOfItems, width);
	}
	item = TreeItem_NextVisible(tree, item);
    }

    return column->widthOfItems;
}

/* Set useWidth for all columns */
void Tree_LayoutColumns(TreeCtrl *tree)
{
    Column *column = (Column *) tree->columns;
    int width, visWidth, totalWidth = 0;
    int numExpand = 0;
    int numSqueeze = 0, squeezeWidth = 0;

    while (column != NULL) {
	if (column->visible) {
	    if (column->widthObj != NULL)
		width = column->width;
	    else {
		width = TreeColumn_WidthOfItems((TreeColumn) column);
		width = MAX(width, TreeColumn_NeededWidth((TreeColumn) column));
		width = MAX(width, TreeColumn_MinWidth((TreeColumn) column));
		if (column->expand)
		    numExpand++;
		if (column->squeeze) {
		    numSqueeze++;
		    squeezeWidth += width - MAX(0, TreeColumn_MinWidth((TreeColumn) column));
		}
	    }
	    column->useWidth = width;
	    totalWidth += width;
	} else
	    column->useWidth = 0;
	column = column->next;
    }

    visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
    if (visWidth <= 0) return;

    /* Squeeze columns */
    if ((visWidth < totalWidth) && (numSqueeze > 0)) {
	int need, allow, each;
	need = totalWidth - visWidth;
	allow = MIN(squeezeWidth, need);
	while (allow > 0) {
	    if (allow >= numSqueeze)
		each = allow / numSqueeze;
	    else
		each = 1;
	    numSqueeze = 0;
	    column = (Column *) tree->columns;
	    while (column != NULL) {
		if (column->visible && column->squeeze && (column->widthObj == NULL)) {
		    int min = MAX(0, TreeColumn_MinWidth((TreeColumn) column));
		    if (column->useWidth > min) {
			int sub = MIN(each, column->useWidth - min);
			column->useWidth -= sub;
			allow -= sub;
			if (!allow) break;
			if (column->useWidth > min)
			    numSqueeze++;
		    }
		}
		column = column->next;
	    }
	}
    }

    /* Expand columns */
    if ((visWidth > totalWidth) && (numExpand > 0)) {
	int extraWidth = (visWidth - totalWidth) / numExpand;
	int fudge = (visWidth - totalWidth) - extraWidth * numExpand;
	int seen = 0;

	column = (Column *) tree->columns;
	while (column != NULL) {
	    if (column->visible && column->expand && (column->widthObj == NULL)) {
		column->useWidth += extraWidth;
		if (++seen == numExpand) {
		    column->useWidth += fudge;
		    break;
		}
	    }
	    column = column->next;
	}
    }
}

void Tree_InvalidateColumnWidth(TreeCtrl *tree, int columnIndex)
{
    Column *column;

    if (columnIndex == -1) {
	column = (Column *) tree->columns;
	while (column != NULL) {
	    column->widthOfItems = -1;
	    column = column->next;
	}
    } else {
	column = (Column *) Tree_FindColumn(tree, columnIndex);
	if (column != NULL)
	    column->widthOfItems = -1;
    }
    tree->widthOfColumns = -1;
    Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH);
}

void Tree_InvalidateColumnHeight(TreeCtrl *tree, int columnIndex)
{
    Column *column;

    if (columnIndex == -1) {
	column = (Column *) tree->columns;
	while (column != NULL) {
	    column->neededHeight = -1;
	    column = column->next;
	}
    } else {
	column = (Column *) Tree_FindColumn(tree, columnIndex);
	if (column != NULL)
	    column->neededHeight = -1;
    }
    tree->headerHeight = -1;
}

void TreeColumn_TreeChanged(TreeCtrl *tree, int flagT)
{
    Column *column;

    /* Column widths are invalidated elsewhere */
    if (flagT & TREE_CONF_FONT) {
	column = (Column *) tree->columns;
	while (column != NULL) {
	    if ((column->tkfont == NULL) && (column->textLen > 0)) {
		column->textWidth = Tk_TextWidth(tree->tkfont, column->text, column->textLen);
		column->neededHeight = -1;
#ifdef COL_LAYOUT
		column->textLayoutInvalid = TRUE;
		column->neededWidth = -1;
#endif
	    }
	    column = column->next;
	}
	tree->headerHeight = -1;
    }
}

int Tree_HeaderHeight(TreeCtrl *tree)
{
    Column *column;
    int height;

    if (!tree->showHeader)
	return 0;

    if (tree->headerHeight >= 0)
	return tree->headerHeight;

    height = 0;
    column = (Column *) tree->columns;
    while (column != NULL) {
	if (column->visible)
	    height = MAX(height, TreeColumn_NeededHeight((TreeColumn) column));
	column = column->next;
    }
    return tree->headerHeight = height;
}

int Tree_WidthOfColumns(TreeCtrl *tree)
{
    Column *column;
    int width;

    if (tree->widthOfColumns >= 0)
	return tree->widthOfColumns;

    Tree_LayoutColumns(tree);

    tree->columnTreeLeft = 0;
    tree->columnTreeVis = FALSE;
    tree->columnVis = NULL;
    tree->columnCountVis = 0;
    width = 0;
    column = (Column *) tree->columns;
    while (column != NULL) {
	if (column->visible) {
	    if (tree->columnVis == NULL)
		tree->columnVis = (TreeColumn) column;
	    tree->columnCountVis++;
	    if (column->index == tree->columnTree) {
		tree->columnTreeLeft = width;
		tree->columnTreeVis = TRUE;
	    }
	    width += column->useWidth;
	}
	column = column->next;
    }

    return tree->widthOfColumns = width;
}

void Tree_InitColumns(TreeCtrl *tree)
{
    Column *column;

    column = Column_Alloc(tree);
    column->tag = ckalloc(5);
    strcpy(column->tag, "tail");
    tree->columnTail = (TreeColumn) column;
    tree->columnCount = 0;
    Column_Config(column, 0, NULL, TRUE);
}

void Tree_FreeColumns(TreeCtrl *tree)
{
    Column *column = (Column *) tree->columns;

    while (column != NULL) {
	column = Column_Free(column);
    }

    Column_Free((Column *) tree->columnTail);
    tree->columnCount = 0;
}

