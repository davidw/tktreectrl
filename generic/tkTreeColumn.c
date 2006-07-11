/* 
 * tkTreeColumn.c --
 *
 *	This module implements treectrl widget's columns.
 *
 * Copyright (c) 2002-2005 Tim Baker
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003 ActiveState Corporation
 *
 * RCS: @(#) $Id: tkTreeColumn.c,v 1.39 2006/07/11 00:09:52 treectrl Exp $
 */

#include "tkTreeCtrl.h"

typedef struct Column Column;

/*
 * The following structure holds information about a single
 * column in a TreeCtrl.
 */
struct Column
{
    Tcl_Obj *textObj;		/* -text */
    char *text;			/* -text */
    int width;			/* -width */
    Tcl_Obj *widthObj;		/* -width */
    int minWidth;		/* -minwidth */
    Tcl_Obj *minWidthObj;	/* -minwidth */
    int maxWidth;		/* -maxwidth */
    Tcl_Obj *maxWidthObj;	/* -maxwidth */
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
    int resize;			/* -resize */
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
    int id;			/* unique column identifier */
    int index;			/* order in list of columns */
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
    TextLayout textLayout;	/* multi-line titles */
    int textLayoutWidth;	/* width passed to TextLayout_Compute */
    int textLayoutInvalid;
#define TEXT_WRAP_NULL -1
#define TEXT_WRAP_CHAR 0
#define TEXT_WRAP_WORD 1
    int textWrap;		/* -textwrap */
    int textLines;		/* -textlines */
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
    {TK_OPTION_PIXELS, "-maxwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, maxWidthObj),
     Tk_Offset(Column, maxWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_PIXELS, "-minwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, minWidthObj),
     Tk_Offset(Column, minWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_BOOLEAN, "-resize", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(Column, resize), 0, (ClientData) NULL, 0},
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
    {TK_OPTION_INT, "-textlines", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(Column, textLines),
     0, (ClientData) NULL, COLU_CONF_TEXT | COLU_CONF_NWIDTH |
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
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

/*
 *----------------------------------------------------------------------
 *
 * ColumnOptionSet --
 *
 *	Tk_ObjCustomOption.setProc(). Converts a Tcl_Obj holding a
 *	column description into a pointer to a Column.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May store a TreeColumn pointer into the internal representation
 *	pointer.  May change the pointer to the Tcl_Obj to NULL to indicate
 *	that the specified string was empty and that is acceptable.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnOptionSet(
    ClientData clientData,	/* CFO_xxx flags to control the conversion. */
    Tcl_Interp *interp,		/* Current interpreter. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    Tcl_Obj **value,		/* Pointer to the pointer to the value object.
				 * We use a pointer to the pointer because
				 * we may need to return a value (NULL). */
    char *recordPtr,		/* Pointer to storage for the widget record. */
    int internalOffset,		/* Offset within *recordPtr at which the
				 * internal value is to be stored. */
    char *saveInternalPtr,	/* Pointer to storage for the old value. */
    int flags			/* Flags for the option, set Tk_SetOptions. */
    )
{
    int cfoFlags = (int) clientData;
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    int objEmpty;
    TreeColumn new, *internalPtr;

    if (internalOffset >= 0)
	internalPtr = (TreeColumn *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	if (TreeColumn_FromObj(tree, (*value), &new, cfoFlags) != TCL_OK)
	    return TCL_ERROR;
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL)
	    new = NULL;
	*((TreeColumn *) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnOptionGet --
 *
 *	Tk_ObjCustomOption.getProc(). Converts a TreeColumn into a
 *	Tcl_Obj string representation.
 *
 * Results:
 *	Tcl_Obj containing the string representation of the column.
 *	Returns NULL if the TreeColumn is NULL.
 *
 * Side effects:
 *	May create a new Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ColumnOptionGet(
    ClientData clientData,	/* Not used. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    char *recordPtr,		/* Pointer to widget record. */
    int internalOffset		/* Offset within *recordPtr containing the
				 * sticky value. */
    )
{
    TreeColumn value = *(TreeColumn *) (recordPtr + internalOffset);
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    if (value == NULL)
	return NULL;
    if (value == COLUMN_ALL)
	return Tcl_NewStringObj("all", -1);
    return TreeColumn_ToObj(tree, value);
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnOptionRestore --
 *
 *	Tk_ObjCustomOption.restoreProc(). Restores a TreeColumn value
 *	from a saved value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the old value.
 *
 *----------------------------------------------------------------------
 */

static void
ColumnOptionRestore(
    ClientData clientData,	/* Not used. */
    Tk_Window tkwin,		/* Not used. */
    char *internalPtr,		/* Where to store old value. */
    char *saveInternalPtr)	/* Pointer to old value. */
{
    *(TreeColumn *) internalPtr = *(TreeColumn *) saveInternalPtr;
}

/*
 * The following structure contains pointers to functions used for processing
 * a custom config option that handles Tcl_Obj<->TreeColumn conversion.
 * A column description must refer to a valid column. "all" is not allowed.
 */
Tk_ObjCustomOption columnCustomOption =
{
    "column",
    ColumnOptionSet,
    ColumnOptionGet,
    ColumnOptionRestore,
    NULL,
    (ClientData) (CFO_NOT_ALL | CFO_NOT_NULL)
};

/*
 * The following structure contains pointers to functions used for processing
 * a custom config option that handles Tcl_Obj<->TreeColumn conversion.
 * A column description must refer to a valid column. "all" is not allowed.
 * "tail" is not allowed.
 */
Tk_ObjCustomOption columnCustomOption_NOT_TAIL =
{
    "column",
    ColumnOptionSet,
    ColumnOptionGet,
    ColumnOptionRestore,
    NULL,
    (ClientData) (CFO_NOT_ALL | CFO_NOT_NULL | CFO_NOT_TAIL)
};

static Tk_OptionSpec dragSpecs[] = {
    {TK_OPTION_BOOLEAN, "-enable", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeCtrl, columnDrag.enable),
     0, (ClientData) NULL, 0},
    {TK_OPTION_INT, "-imagealpha", (char *) NULL, (char *) NULL,
     "128", -1, Tk_Offset(TreeCtrl, columnDrag.alpha),
     0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-imagecolor", (char *) NULL, (char *) NULL,
     "gray75", -1, Tk_Offset(TreeCtrl, columnDrag.color),
     0, (ClientData) NULL, 0},
    {TK_OPTION_CUSTOM, "-imagecolumn", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, columnDrag.column),
     TK_OPTION_NULL_OK, (ClientData) &columnCustomOption_NOT_TAIL, 0},
    {TK_OPTION_PIXELS, "-imageoffset", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeCtrl, columnDrag.offsetObj),
     Tk_Offset(TreeCtrl, columnDrag.offset), 0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-indicatorcolor", (char *) NULL, (char *) NULL,
     "Black", -1, Tk_Offset(TreeCtrl, columnDrag.indColor),
     0, (ClientData) NULL, 0},
    {TK_OPTION_CUSTOM, "-indicatorcolumn", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, columnDrag.indColumn),
     TK_OPTION_NULL_OK, (ClientData) &columnCustomOption, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * ImageChangedProc --
 *
 *	This procedure is invoked by the image code whenever the manager
 *	for an image does something that affects the size or contents
 *	of an image displayed in a column header.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Invalidates the size of the column and schedules a redisplay.
 *
 *----------------------------------------------------------------------
 */

static void
ImageChangedProc(
    ClientData clientData,		/* Pointer to Column record. */
    int x, int y,			/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, int height,		/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imageWidth, int imageHeight	/* New dimensions of image. */
    )
{
    /* I would like to know the image was deleted... */
    Column *column = (Column *) clientData;
    TreeCtrl *tree = column->tree;

    /* Duplicate the effects of configuring the -image option. */
    column->neededWidth = -1;
    column->neededHeight = -1;
    tree->headerHeight = -1;
    tree->widthOfColumns = -1;
    Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH | DINFO_DRAW_HEADER);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FindColumnByTag --
 *
 *	Find a column with a matching -tag option.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_FindColumnByTag(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *obj,		/* Tag name. */
    TreeColumn *columnPtr,	/* Returned column. */
    int flags			/* CFO_NOT_TAIL or 0. */
    )
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

/*
 *----------------------------------------------------------------------
 *
 * IndexFromList --
 *
 *	Utility wrapper around Tcl_GetIndexFromObj().
 *
 * Results:
 *	The result of Tcl_GetIndexFromObj() or -1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
IndexFromList(
    int listIndex,		/* Index of objv[] to pass to
				 * Tcl_GetIndexFromObj */
    int objc,			/* Number of arguments. */
    Tcl_Obj **objv,		/* Arguments. */
    CONST char **indexNames	/* NULL-terminated list of names. */
    )
{
    Tcl_Obj *elemPtr;
    int index;

    if (listIndex >= objc)
	return -1;
    elemPtr = objv[listIndex];
    if (Tcl_GetIndexFromObj(NULL, elemPtr, indexNames, NULL, 0, &index)
	    != TCL_OK)
	return -1;
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_FromObj --
 *
 *	Convert a Tcl_Obj column description to a column.
 *
 * ID MODIFIERS
 * TAG MODIFIERS
 * all
 * first ?visible? MODIFIERS
 * end|last ?visible? MODIFIERS
 * order N ?visible? MODIFIERS
 * MODIFIERS:
 * next ?visible?
 * prev ?visible?
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Column description. */
    TreeColumn *columnPtr,	/* Returned column. */
    int flags			/* CFO_xxx flags. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int objc;
    int index, listIndex;
    Tcl_Obj **objv, *elemPtr;
    Column *column = NULL;
    static CONST char *indexName[] = {
	"all", "end", "first", "last", "order", "tail", "tree", (char *) NULL
    };
    enum indexEnum {
	INDEX_ALL, INDEX_END, INDEX_FIRST, INDEX_LAST, INDEX_ORDER, INDEX_TAIL,
	INDEX_TREE
    } ;
    static CONST char *modifiers[] = {
	"next", "prev", "visible", (char *) NULL
    };
    enum modEnum {
	TMOD_NEXT, TMOD_PREV, TMOD_VISIBLE
    };
    static int modArgs[] = {
	1, 1, 1
    };

    if (Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK)
	goto badDesc;
    if (objc == 0)
	goto badDesc;

    listIndex = 0;
    elemPtr = objv[listIndex++];
    if (Tcl_GetIndexFromObj(NULL, elemPtr, indexName, NULL, 0, &index)
	    == TCL_OK) {
	switch ((enum indexEnum) index) {
	    case INDEX_ALL:
	    {
		if (flags & CFO_NOT_ALL) {
		    FormatResult(interp,
			    "can't specify \"all\" for this command");
		    return TCL_ERROR;
		}
		if (objc > 1)
		    goto badDesc;
		(*columnPtr) = COLUMN_ALL;
		return TCL_OK;
	    }
	    case INDEX_FIRST:
	    {
		column = (Column *) tree->columns;
		if (IndexFromList(listIndex, objc, objv,
			    modifiers) == TMOD_VISIBLE) {
		    while (column && !column->visible)
			column = column->next;
		    listIndex++;
		}
		break;
	    }
	    case INDEX_END:
	    case INDEX_LAST:
	    {
		column = (Column *) tree->columns;
		if (IndexFromList(listIndex, objc, objv,
			    modifiers) == TMOD_VISIBLE) {
		    Column *visible = NULL;
		    while (column) {
			if (column->visible)
			    visible = column;
			column = column->next;
		    }
		    column = visible;
		    listIndex++;
		} else if (column) {
		    while (column->next)
			column = column->next;
		}
		break;
	    }
	    case INDEX_ORDER:
	    {
		int index = 0, order, visible = FALSE;

		if (objc < 2)
		    goto badDesc;
		if (Tcl_GetIntFromObj(NULL, objv[listIndex++], &order) != TCL_OK)
		    goto badDesc;
		if (IndexFromList(listIndex, objc, objv, modifiers)
			    == TMOD_VISIBLE) {
		    visible = TRUE;
		    listIndex++;
		}
		column = (Column *) tree->columns;
		while (column) {
		    if (!visible && (column->index == order))
			break;
		    if (visible && column->visible && (index == order))
			break;
		    if (!visible || column->visible)
			index++;
		    column = column->next;
		}
		break;
	    }
	    case INDEX_TAIL:
	    {
		if (flags & CFO_NOT_TAIL) {
		    FormatResult(tree->interp,
			    "can't specify \"tail\" for this command");
		    return TCL_ERROR;
		}
		column = (Column *) tree->columnTail;
		break;
	    }
	    case INDEX_TREE:
	    {
		column = (Column *) tree->columnTree;
		break;
	    }
	}
    } else {
	int gotId = FALSE, id = -1;

	if (tree->columnPrefixLen) {
	    char *end, *t = Tcl_GetString(elemPtr);
	    if (strncmp(t, tree->columnPrefix, tree->columnPrefixLen) == 0)
	    {
		t += tree->columnPrefixLen;
		id = strtoul(t, &end, 10);
		if ((end != t) && (*end == '\0'))
		    gotId = TRUE;
	    }

	} else if (Tcl_GetIntFromObj(NULL, elemPtr, &id) == TCL_OK) {
	    gotId = TRUE;
	}
	if (gotId) {
	    column = (Column *) tree->columns;
	    while (column) {
		if (column->id == id)
		    break;
		column = column->next;
	    }
	} else {
	    char *string = Tcl_GetString(elemPtr);
	    column = (Column *) tree->columns;
	    while (column) {
		if ((column->tag != NULL) && !strcmp(column->tag, string)) {
		    break;
		}
		column = column->next;
	    }
	}
    }

    /* This means a valid specification was given, but there is no such column */
    if (column == NULL) {
	if (flags & CFO_NOT_NULL)
	    goto noColumn;
	(*columnPtr) = NULL;
	return TCL_OK;
    }

    for (; listIndex < objc; /* nothing */) {
	int nextIsVisible = FALSE;

	elemPtr = objv[listIndex];
	if (Tcl_GetIndexFromObj(interp, elemPtr, modifiers, "modifier", 0,
		    &index) != TCL_OK)
	    return TCL_ERROR;
	if (objc - listIndex < modArgs[index])
	    goto badDesc;
	if (IndexFromList(listIndex + modArgs[index], objc, objv,
		    modifiers) == TMOD_VISIBLE)
	    nextIsVisible = TRUE;
	switch ((enum modEnum) index) {
	    case TMOD_NEXT:
	    {
		int isTail = column == (Column *) tree->columnTail;
		if (isTail) {
		    column = NULL;
		    break;
		}
		column = column->next;
		if (nextIsVisible) {
		    while (column && !column->visible)
			column = column->next;
		}
		if (column == NULL)
		    column = (Column *) tree->columnTail;
		break;
	    }
	    case TMOD_PREV:
	    {
		Column *walk = (Column *) tree->columns;
		int isTail = column == (Column *) tree->columnTail;
		if (column == (Column *) tree->columns) {
		    column = NULL;
		    break;
		}
		if (isTail && !tree->columnCount) {
		    column = NULL;
		    break;
		}
		if (nextIsVisible) {
		    Column *visible = NULL;
		    while (1) {
			if (walk->visible)
			    visible = walk;
			walk = walk->next;
			if (walk == (isTail ? NULL : column))
			    break;
		    }
		    column = visible;
		} else {
		    while (walk->next != (isTail ? NULL : column))
			walk = walk->next;
		    column = walk;
		}
		break;
	    }
	    case TMOD_VISIBLE:
	    {
		goto badDesc;
	    }
	}
	if (column == NULL) {
	    if (flags & CFO_NOT_NULL)
		goto noColumn;
	    (*columnPtr) = NULL;
	    return TCL_OK;
	}
	listIndex += modArgs[index];
	if (nextIsVisible)
	    listIndex++;
    }
    if (column == (Column *) tree->columnTail) {
	if (flags & CFO_NOT_TAIL) {
	    FormatResult(tree->interp,
		    "can't specify \"tail\" for this command");
	    return TCL_ERROR;
	}
    }
    (*columnPtr) = (TreeColumn) column;
    return TCL_OK;

    badDesc:
    Tcl_AppendResult(interp, "bad column description \"", Tcl_GetString(objPtr),
	    "\"", NULL);
    return TCL_ERROR;

    noColumn:
    Tcl_AppendResult(interp, "column \"", Tcl_GetString(objPtr),
	    "\" doesn't exist", NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_ToObj --
 *
 *	Return a Tcl_Obj representing a column.
 *
 * Results:
 *	A Tcl_Obj.
 *
 * Side effects:
 *	Allocates a Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeColumn_ToObj(
    TreeCtrl *tree,		/* Widget info. */
    TreeColumn column_		/* Column token to get Tcl_Obj for. */
    )
{
    if (column_ == tree->columnTail)
	return Tcl_NewStringObj("tail", -1);
    if (tree->columnPrefixLen) {
	char buf[100 + TCL_INTEGER_SPACE];
	(void) sprintf(buf, "%s%d", tree->columnPrefix, ((Column *) column_)->id);
	return Tcl_NewStringObj(buf, -1);
    }
    return Tcl_NewIntObj(((Column *) column_)->id);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FindColumn --
 *
 *	Get the N'th column in a TreeCtrl.
 *
 * Results:
 *	Token for the N'th column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColumn
Tree_FindColumn(
    TreeCtrl *tree,		/* Widget info. */
    int columnIndex		/* 0-based index of the column to return. */
    )
{
    Column *column = (Column *) tree->columns;

    while (column != NULL) {
	if (column->index == columnIndex)
	    break;
	column = column->next;
    }
    return (TreeColumn) column;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Next --
 *
 *	Return the column to the right of the given one.
 *
 * Results:
 *	Token for the next column.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeColumn
TreeColumn_Next(
    TreeColumn column_		/* Column token. */
    )
{
    return (TreeColumn) ((Column *) column_)->next;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_MakeState --
 *
 *	Return a bit mask suitable for passing to the PerState_xxx
 *	functions.
 *
 * Results:
 *	State flags for the column's current state.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Column_MakeState(
    Column *column		/* Column record. */
    )
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

/*
 *----------------------------------------------------------------------
 *
 * Column_FreeColors --
 *
 *	Frees an array of XColors. This is used to free the -itembackground
 *	array of colors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated, colors are freed.
 *
 *----------------------------------------------------------------------
 */

static void
Column_FreeColors(
    XColor **colors,		/* Array of colors. May be NULL. */
    int count			/* Number of colors. */
    )
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
    WCFREE(colors, XColor *, count);
}

/*
 *----------------------------------------------------------------------
 *
 * ColumnStateFromObj --
 *
 *	Parses a string object containing "state" or "!state" to a
 *	state bit flag.
 *	This function is passed to PerStateInfo_FromObj().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnStateFromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *obj,		/* String object to parse. */
    int *stateOff,		/* OR'd with state bit if "!state" is
				 * specified. Caller must initialize. */
    int *stateOn		/* OR'd with state bit if "state" is
				 * specified. Caller must initialize. */
    )
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

/*
 *----------------------------------------------------------------------
 *
 * Column_Config --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a Column.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for column;  old resources get freed, if there
 *	were any.  Display changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
Column_Config(
    Column *column,		/* Column record. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument values. */
    int createFlag		/* TRUE if the Column is being created. */
    )
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

    if (mask & (COLU_CONF_NWIDTH | COLU_CONF_TWIDTH))
	mask |= COLU_CONF_NHEIGHT;
    if (mask & (COLU_CONF_JUSTIFY | COLU_CONF_TEXT))
	column->textLayoutInvalid = TRUE;

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

/*
 *----------------------------------------------------------------------
 *
 * Column_Alloc --
 *
 *	Allocate and initialize a new Column record.
 *
 * Results:
 *	Pointer to the new Column, or NULL if errors occurred.
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
    column->id = tree->nextColumnId++;
    tree->columnCount++;
    return column;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_Free --
 *
 *	Free a Column.
 *
 * Results:
 *	Pointer to the next column.
 *
 * Side effects:
 *	Memory is deallocated. If this is the last column being
 *	deleted, the TreeCtrl.nextColumnId field is reset to zero.
 *
 *----------------------------------------------------------------------
 */

static Column *
Column_Free(
    Column *column		/* Column record. */
    )
{
    TreeCtrl *tree = column->tree;
    Column *next = column->next;

    Column_FreeColors(column->itemBgColor, column->itemBgCount);
    if (column->bitmapGC != None)
	Tk_FreeGC(tree->display, column->bitmapGC);
    if (column->image != NULL)
	Tk_FreeImage(column->image);
    if (column->textLayout != NULL)
	TextLayout_Free(column->textLayout);
    PerStateInfo_Free(tree, &pstBitmap, &column->arrowBitmap);
    PerStateInfo_Free(tree, &pstImage, &column->arrowImage);
    PerStateInfo_Free(tree, &pstBorder, &column->border);
    Tk_FreeConfigOptions((char *) column, column->optionTable, tree->tkwin);
    WFREE(column, Column);
    tree->columnCount--;
    if (tree->columnCount == 0)
	tree->nextColumnId = 0;
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_FixedWidth --
 *
 *	Return the value of the -width option.
 *
 * Results:
 *	The pixel width or -1 if the -width option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_FixedWidth(
    TreeColumn column_		/* Column token. */
    )
{
    Column *column = (Column *) column_;
    return column->widthObj ? column->width : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_MinWidth --
 *
 *	Return the value of the -minwidth option.
 *
 * Results:
 *	The pixel width or -1 if the -minwidth option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_MinWidth(
    TreeColumn column_		/* Column token. */
    )
{
    Column *column = (Column *) column_;
    return column->minWidthObj ? column->minWidth : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_MaxWidth --
 *
 *	Return the value of the -maxwidth option.
 *
 * Results:
 *	The pixel width or -1 if the -maxwidth option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_MaxWidth(
    TreeColumn column_		/* Column token. */
    )
{
    Column *column = (Column *) column_;
    return column->maxWidthObj ? column->maxWidth : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_StepWidth --
 *
 *	Return the value of the -stepwidth option.
 *	NOTE: -stepwidth is deprecated.
 *
 * Results:
 *	The pixel width or -1 if the -stepwidth option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_StepWidth(
    TreeColumn column_		/* Column token. */
    )
{
    Column *column = (Column *) column_;
    return column->stepWidthObj ? column->stepWidth : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_UpdateTextLayout --
 *
 *	Recalculate the TextLayout for the text displayed in the
 *	column header. The old TextLayout (if any) is freed. If
 *	there is no text or if it is only one line then no TextLayout
 *	is created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated/deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
Column_UpdateTextLayout(
    Column *column,		/* Column record. */
    int width			/* Maximum line length. Zero means there
				 * is no limit. */
    )
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

#ifdef MAC_OSX_TK
    /* The height of the header is fixed on Aqua. There is only room for
     * a single line of text. */
    if (column->tree->useTheme)
	maxLines = 1;
#endif

    if (!multiLine && ((maxLines == 1) || (!width || (width >= column->textWidth))))
	return;

    tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;

    if (wrap == TEXT_WRAP_WORD)
	flags |= TK_WHOLE_WORDS;

    column->textLayout = TextLayout_Compute(tkfont, text,
	    Tcl_NumUtfChars(text, textLen), width, justify, maxLines, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Column_GetArrowSize --
 *
 *	Return the size of the sort arrow displayed in the column header
 *	for the column's current state.
 *
 * Results:
 *	Height and width of the arrow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Column_GetArrowSize(
    Column *column,		/* Column record. */
    int *widthPtr,		/* Returned width. */
    int *heightPtr		/* Returned height. */
    )
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
    if ((arrowWidth == -1) && tree->useTheme &&
	TreeTheme_GetArrowSize(tree, Tk_WindowId(tree->tkwin),
	column->arrow == ARROW_UP, &arrowWidth, &arrowHeight) == TCL_OK) {
    }
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

/*
 * The following structure holds size/position info for all the graphical
 * elements of a column header.
 */
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

/*
 * The following structure is used by the Column_DoLayout() procedure to
 * hold size/position info for each graphical element displayed in the
 * header.
 */
struct LayoutPart
{
    int padX[2];
    int padY[2];
    int width;
    int height;
    int left;
    int top;
};

/*
 *----------------------------------------------------------------------
 *
 * Column_DoLayout --
 *
 *	Arrange all the graphical elements making up a column header.
 *
 * Results:
 *	Layout info is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Column_DoLayout(
    Column *column,		/* Column record. */
    struct Layout *layout	/* Returned layout info. The width and
				 * height fields must be initialized. */
    )
{
#if defined(MAC_OSX_TK)
    TreeCtrl *tree = column->tree;
#endif
    struct LayoutPart *parts[3];
    struct LayoutPart partArrow, partImage, partText;
    int i, padList[4], widthList[3], n = 0;
    int iArrow = -1, iImage = -1, iText = -1;
    int left, right;
    int widthForText = 0;
#if defined(MAC_OSX_TK)
    int margins[4];
    int arrow = column->arrow;
    int arrowSide = column->arrowSide;
    int arrowGravity = column->arrowGravity;
#endif

#if defined(MAC_OSX_TK)
    /* Under Aqua, we let the Appearance Manager draw the sort arrow */
    if (tree->useTheme) {
	column->arrow = ARROW_NONE;
	column->arrowSide = SIDE_RIGHT;
	column->arrowGravity = SIDE_RIGHT;
    }
#endif

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
	}
    }
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_RIGHT)) {
	parts[n] = &partArrow;
	padList[n] = MAX(partArrow.padX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = partArrow.padX[PAD_BOTTOM_RIGHT];
	iArrow = n++;
    }

#if defined(MAC_OSX_TK)
    /* Under Aqua, we let the Appearance Manager draw the sort arrow */
    /* This code assumes the arrow is on the right */
    if (tree->useTheme && (arrow != ARROW_NONE)) {
	if (TreeTheme_GetHeaderContentMargins(tree, column->state,
		arrow, margins) == TCL_OK) {
	    parts[n] = &partArrow;
	    partArrow.width = margins[2];
	    padList[n] = MAX(0, padList[n]); /* ignore -arrowpadx */
	    padList[n + 1] = 0;
	    iArrow = n++;
	}
    }
    if (n == 0) {
	column->arrow = arrow;
	column->arrowSide = arrowSide;
	column->arrowGravity = arrowGravity;
    }
#endif

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

#if defined(MAC_OSX_TK)
    /* Under Aqua, we let the Appearance Manager draw the sort arrow */
    column->arrow = arrow;
    column->arrowSide = arrowSide;
    column->arrowGravity = arrowGravity;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_NeededWidth --
 *
 *	Return the total width requested by all the graphical elements
 *	that make up a column header.  The width is recalculated if it
 *	is marked out-of-date.
 *
 * Results:
 *	The width needed by the current arrangement of the
 *	bitmap/image/text/arrow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_NeededWidth(
    TreeColumn column_		/* Column token. */
    )
{
    Column *column = (Column *) column_;
    TreeCtrl *tree = column->tree;
    int i, widthList[3], padList[4], n = 0;
    int arrowWidth, arrowHeight;
#if defined(MAC_OSX_TK)
    int margins[4];
    int arrow = column->arrow;
#endif

    if (column->neededWidth >= 0)
	return column->neededWidth;

    for (i = 0; i < 3; i++) widthList[i] = 0;
    for (i = 0; i < 4; i++) padList[i] = 0;

#if defined(MAC_OSX_TK)
    /* Under OSX we let the Appearance Manager draw the sort arrow. */
    if (tree->useTheme)
	column->arrow = ARROW_NONE;
#endif

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
	if (column->textLayoutInvalid || (column->textLayoutWidth != 0)) {
	    Column_UpdateTextLayout(column, 0);
	    column->textLayoutInvalid = FALSE;
	    column->textLayoutWidth = 0;
	}
	if (column->textLayout != NULL)
	    TextLayout_Size(column->textLayout, &widthList[n], NULL);
	else
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

#if defined(MAC_OSX_TK)
    if (tree->useTheme)
	column->arrow = arrow;

    /* Under OSX we let the Appearance Manager draw the sort arrow. This code
     * assumes the arrow is on the right. */
    if (tree->useTheme &&
	(TreeTheme_GetHeaderContentMargins(tree, column->state, column->arrow,
		margins) == TCL_OK)) {
	column->neededWidth += margins[2];
    }
#endif

    /* Notice I'm not considering column->borderWidth. */

    return column->neededWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_NeededHeight --
 *
 *	Return the total height requested by all the graphical elements
 *	that make up a column header.  The height is recalculated if it
 *	is marked out-of-date.
 *
 * Results:
 *	The height needed by the current arrangement of the
 *	bitmap/image/text/arrow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_NeededHeight(
    TreeColumn column_		/* Column token. */
    )
{
    Column *column = (Column *) column_;
    TreeCtrl *tree = column->tree;
    int margins[4];

    if (column->neededHeight >= 0)
	return column->neededHeight;

#if defined(MAC_OSX_TK)
    /* List headers are a fixed height on Aqua */
    if (tree->useTheme) {
	(void) TreeTheme_GetHeaderFixedHeight(tree, &column->neededHeight);
	return column->neededHeight;
    }
#endif

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
	struct Layout layout;
	layout.width = TreeColumn_UseWidth(column_);
	layout.height = -1;
	Column_DoLayout(column, &layout);
	if (column->textLayout != NULL) {
	    int height;
	    TextLayout_Size(column->textLayout, NULL, &height);
	    height += column->textPadY[PAD_TOP_LEFT]
		+ column->textPadY[PAD_BOTTOM_RIGHT];
	    column->neededHeight = MAX(column->neededHeight, height);
	} else {
	    Tk_Font tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;
	    Tk_FontMetrics fm;
	    Tk_GetFontMetrics(tkfont, &fm);
	    fm.linespace += column->textPadY[PAD_TOP_LEFT]
		+ column->textPadY[PAD_BOTTOM_RIGHT];
	    column->neededHeight = MAX(column->neededHeight, fm.linespace);
	}
    }
    if (column->tree->useTheme &&
	(TreeTheme_GetHeaderContentMargins(tree, column->state,
		column->arrow, margins) == TCL_OK)) {
#ifdef WIN32
	/* I'm hacking these margins since the default XP theme does not give
	 * reasonable ContentMargins for HP_HEADERITEM */
	int bw = MAX(column->borderWidth, 3);
	margins[1] = MAX(margins[1], bw);
	margins[3] = MAX(margins[3], bw);
#endif /* WIN32 */
	column->neededHeight += margins[1] + margins[3];
    } else {
	column->neededHeight += column->borderWidth * 2;
    }

    return column->neededHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_UseWidth --
 *
 *	Return the actual display width of a column.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	The size of any column that is marked out-of-date is
 *	recalculated. This could involve recalculating the size of
 *	every element and style in the column in all items.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_UseWidth(
    TreeColumn column_		/* Column token. */
    )
{
    /* Update layout if needed */
    (void) Tree_WidthOfColumns(((Column *) column_)->tree);

    return ((Column *) column_)->useWidth;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Justify --
 *
 *	Return the value of the -justify config option for a column.
 *
 * Results:
 *	TK_JUSTIFY_xxx constant.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Justify
TreeColumn_Justify(
    TreeColumn column_		/* Column token. */
    )
{
    return ((Column *) column_)->justify;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_WidthHack --
 *
 *	Return the value of the -widthhack config option for a column.
 *	NOTE: -widthhack is deprecated.
 *
 * Results:
 *	Boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_WidthHack(
    TreeColumn column_		/* Column token. */
    )
{
    return ((Column *) column_)->widthHack;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Squeeze --
 *
 *	Return the value of the -squeeze config option for a column.
 *
 * Results:
 *	Boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_Squeeze(
    TreeColumn column_		/* Column token. */
    )
{
    return ((Column *) column_)->squeeze;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_BackgroundGC --
 *
 *	Return a graphics context for one color of the -itembackground
 *	config option for a column.
 *
 * Results:
 *	A graphics context, or None.
 *
 * Side effects:
 *	Might allocate a new graphics context? But the GC is freed
 *	when the last reference to the color is lost, so the caller
 *	need not worry about it.
 *
 *----------------------------------------------------------------------
 */

GC
TreeColumn_BackgroundGC(
    TreeColumn column_,		/* Column token. */
    int index			/* This number is determined by the display
				 * code. */
    )
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

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Visible --
 *
 *	Return the value of the -visible config option for a column.
 *
 * Results:
 *	Boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_Visible(
    TreeColumn column_		/* Column token. */
    )
{
    return ((Column *) column_)->visible;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_GetID --
 *
 *	Return the unique identifier for a column.
 *
 * Results:
 *	Unique integer id.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeColumn_GetID(
    TreeColumn column_		/* Column token. */
    )
{
    return ((Column *) column_)->id;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_Index --
 *
 *	Return the 0-based index for a column.
 *
 * Results:
 *	Position of the column in the list of columns.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_Index(
    TreeColumn column_		/* Column token. */
    )
{
    return ((Column *) column_)->index;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumnCmd --
 *
 *	This procedure is invoked to process the [column] widget
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
TreeColumnCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = {
	"bbox", "cget", "compare", "configure", "count", "create", "delete",
	"dragcget", "dragconfigure", "id", "index", "list", "move",
	"neededwidth", "order", "width", (char *) NULL
    };
    enum {
	COMMAND_BBOX, COMMAND_CGET, COMMAND_COMPARE, COMMAND_CONFIGURE,
	COMMAND_COUNT, COMMAND_CREATE, COMMAND_DELETE, COMMAND_DRAGCGET,
	COMMAND_DRAGCONF, COMMAND_ID, COMMAND_INDEX, COMMAND_LIST,
	COMMAND_MOVE, COMMAND_NEEDEDWIDTH, COMMAND_ORDER, COMMAND_WIDTH
    };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
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
	    TreeColumn _column;
	    Column *column, *walk;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &_column,
			CFO_NOT_ALL | CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    column = (Column *) _column;
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
	    if (TreeColumn_FromObj(tree, objv[3], &column,
			CFO_NOT_ALL | CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) column,
		    ((Column *) column)->optionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T column compare C op C */
	case COMMAND_COMPARE:
	{
	    TreeColumn column1, column2;
	    static CONST char *opName[] = { "<", "<=", "==", ">=", ">", "!=", NULL };
	    int op, compare = 0, index1, index2;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 3, objv, "column1 op column2");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column1,
		    CFO_NOT_ALL | CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIndexFromObj(interp, objv[4], opName,
		    "comparison operator", 0, &op) != TCL_OK)
		return TCL_ERROR;
	    if (TreeColumn_FromObj(tree, objv[5], &column2,
		    CFO_NOT_ALL | CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    index1 = TreeColumn_Index(column1);
	    index2 = TreeColumn_Index(column2);
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

	case COMMAND_CONFIGURE:
	{
	    TreeColumn _column;
	    Column *column;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column ?option? ?value?");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &_column,
		    CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    column = (Column *) _column;
	    if (objc <= 5 && (column == (Column *) COLUMN_ALL)) {
		FormatResult(interp, "can't specify \"all\" without an option-value pair");
		return TCL_ERROR;
	    }
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
	    if (column == (Column *) COLUMN_ALL) {
		column = (Column *) tree->columns;
		while (column != NULL) {
		    if (Column_Config(column, objc - 4, objv + 4, FALSE) != TCL_OK)
			return TCL_ERROR;
		    column = column->next;
		}
		if (Column_Config((Column *) tree->columnTail, objc - 4, objv + 4, FALSE) != TCL_OK)
		    return TCL_ERROR;
	    } else
		return Column_Config(column, objc - 4, objv + 4, FALSE);
	    break;
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
	    Tcl_SetObjResult(interp, TreeColumn_ToObj(tree, (TreeColumn) column));
	    break;
	}

	case COMMAND_DELETE:
	{
	    int firstIndex, lastIndex;
	    TreeColumn _column;
	    Column *column, *first, *last = NULL, *prev = NULL, *next = NULL;
	    TreeItem item;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "first ?last?");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &_column,
			CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    first = last = (Column *) _column;
	    if (objc == 5) {
		if (TreeColumn_FromObj(tree, objv[4], &_column,
			    CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		    return TCL_ERROR;
		last = (Column *) _column;
	    }

	    /* T column delete "all" */
	    if (first == (Column *) COLUMN_ALL ||
		    last == (Column *) COLUMN_ALL) {
		column = (Column *) tree->columns;
		while (column != NULL) {
		    Column *next = column->next;
		    Column_Free(column);
		    column = next;
		}
		((Column *) tree->columnTail)->index = 0;
		tree->columns = NULL;

		/* Delete all TreeItemColumns */
		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashValue(hPtr);
		    TreeItem_RemoveAllColumns(tree, item);
		    hPtr = Tcl_NextHashEntry(&search);
		}

		tree->columnTree = NULL;
		tree->columnDrag.column = tree->columnDrag.indColumn = NULL;
		tree->widthOfColumns = tree->headerHeight = -1;
		Tree_DInfoChanged(tree, DINFO_REDO_COLUMN_WIDTH);
		break;
	    }

	    /* Swap first and last if needed */
	    if (last->index < first->index) {
		column = first;
		first = last;
		last = column;
	    }

	    firstIndex = first->index;
	    lastIndex = last->index;

	    column = (Column *) tree->columns;
	    while (column != NULL) {
		next = column->next;
		if (next == first)
		    prev = column;
		else if (column->index >= firstIndex) {
		    if (column == (Column *) tree->columnTree)
			tree->columnTree = NULL;
		    if (column == (Column *) tree->columnDrag.column)
			tree->columnDrag.column = NULL;
		    if (column == (Column *) tree->columnDrag.indColumn)
			tree->columnDrag.indColumn = NULL;
		    Column_Free(column);
		}
		if (column == last)
		    break;
		column = next;
	    }

	    if (prev != NULL)
		prev->next = next;
	    else
		tree->columns = (TreeColumn) next;

	    /* Delete all TreeItemColumns in the range */
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		TreeItem_RemoveColumns(tree, item, firstIndex, lastIndex);
		hPtr = Tcl_NextHashEntry(&search);
	    }

	    /* Renumber trailing columns */
	    column = next;
	    while (column != NULL) {
		column->index = firstIndex++;
		column = column->next;
	    }
	    ((Column *) tree->columnTail)->index = firstIndex;

	    tree->widthOfColumns = tree->headerHeight = -1;
	    Tree_DInfoChanged(tree, DINFO_REDO_COLUMN_WIDTH);
	    break;
	}

	/* T column dragcget option */
	case COMMAND_DRAGCGET:
	{
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) tree,
		    tree->columnDrag.optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T column dragconfigure ?option? ?value? ?option value ...? */
	case COMMAND_DRAGCONF:
	{
	    Tcl_Obj *resultObjPtr;
	    Tk_SavedOptions savedOptions;
	    int mask, result;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 3, objv, "?option? ?value?");
		return TCL_ERROR;
	    }
	    if (objc <= 4) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) tree,
			tree->columnDrag.optionTable,
			(objc == 3) ? (Tcl_Obj *) NULL : objv[3],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    result = Tk_SetOptions(interp, (char *) tree,
		    tree->columnDrag.optionTable, objc - 3, objv + 3, tree->tkwin,
		    &savedOptions, &mask);
	    if (result != TCL_OK) {
		Tk_RestoreSavedOptions(&savedOptions);
		return TCL_ERROR;
	    }
	    Tk_FreeSavedOptions(&savedOptions);

	    if (tree->columnDrag.alpha < 0)
		tree->columnDrag.alpha = 0;
	    if (tree->columnDrag.alpha > 255)
		tree->columnDrag.alpha = 255;

	    Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
	    break;
	}

	case COMMAND_COUNT:
	{
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->columnCount));
	    break;
	}

	case COMMAND_WIDTH:
	{
	    TreeColumn _column;
	    Column *column;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &_column,
			CFO_NOT_ALL | CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    column = (Column *) _column;
	    /* Update layout if needed */
	    (void) Tree_WidthOfColumns(tree);
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(column->useWidth));
	    break;
	}

	case COMMAND_ID:
	case COMMAND_INDEX:
	{
	    TreeColumn column;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column,
		CFO_NOT_ALL) != TCL_OK)
		return TCL_ERROR;
	    if (column != NULL)
		Tcl_SetObjResult(interp, TreeColumn_ToObj(tree, column));
	    break;
	}

	/* T column list ?-visible? */
	case COMMAND_LIST:
	{
	    Column *column = (Column *) tree->columns;
	    Tcl_Obj *listObj;
	    int visible = FALSE;

	    if (objc > 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "?-visible?");
		return TCL_ERROR;
	    }
	    if (objc == 4) {
		int len;
		char *s = Tcl_GetStringFromObj(objv[3], &len);
		if ((s[0] == '-') && (strncmp(s, "-visible", len) == 0))
		    visible = TRUE;
		else {
		    FormatResult(interp, "bad switch \"%s\": must be -visible",
			s);
		    return TCL_ERROR;
		}
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    while (column != NULL) {
		if (!visible || column->visible)
		    Tcl_ListObjAppendElement(interp, listObj,
				TreeColumn_ToObj(tree, (TreeColumn) column));
		column = column->next;
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T column move C before */
	case COMMAND_MOVE:
	{
	    TreeColumn _move, _before;
	    Column *move, *before;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;
	    TreeItem item;
	    int numStyles;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column before");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &_move,
		    CFO_NOT_ALL | CFO_NOT_NULL | CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    move = (Column *) _move;
	    if (TreeColumn_FromObj(tree, objv[4], &_before,
		    CFO_NOT_ALL | CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    before = (Column *) _before;
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
	    if ((numStyles > 0) && ((before->index < numStyles) ||
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
		int index;

		prev = NULL;
		walk = (Column *) tree->columns;
		while (walk != NULL) {
		    if (walk == move)
			prevM = prev;
		    if (walk == before)
			prevB = prev;
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
	    TreeColumn _column;
	    Column *column;
	    int width;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &_column,
			CFO_NOT_ALL | CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    column = (Column *) _column;
	    /* Update layout if needed */
	    (void) Tree_TotalWidth(tree);
	    width = TreeColumn_WidthOfItems((TreeColumn) column);
	    width = MAX(width, TreeColumn_NeededWidth((TreeColumn) column));
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(width));
	    break;
	}

	/* T column order C ?-visible? */
	case COMMAND_ORDER:
	{
	    TreeColumn _column;
	    Column *column;
	    int visible = FALSE;
	    int index = 0;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column ?-visible?");
		return TCL_ERROR;
	    }
	    if (objc == 5) {
		int len;
		char *s = Tcl_GetStringFromObj(objv[4], &len);
		if ((s[0] == '-') && (strncmp(s, "-visible", len) == 0))
		    visible = TRUE;
		else {
		    FormatResult(interp, "bad switch \"%s\": must be -visible",
			s);
		    return TCL_ERROR;
		}
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &_column,
		CFO_NOT_ALL | CFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    column = (Column *) _column;
	    if (visible) {
		Column *walk = (Column *) tree->columns;
		while (walk != NULL) {
		    if (walk == column)
			break;
		    if (walk->visible)
			index++;
		    walk = walk->next;
		}
		if (!column->visible)
		    index = -1;
	    } else {
		index = column->index;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(index));
	    break;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Column_DrawArrow --
 *
 *	Draw the sort arrow for a column.
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
Column_DrawArrow(
    Column *column,		/* Column record. */
    Drawable drawable,		/* Where to draw. */
    int x, int y,		/* Top-left corner of the column's header. */
    struct Layout layout	/* Size/position info. */
    )
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
	bx = x + layout.arrowLeft + sunken;
	by = y + (height - (layout.arrowHeight + arrowPadY)) / 2 + sunken;
	Tree_DrawBitmap(tree, bitmap, drawable, NULL, NULL,
		0, 0,
		(unsigned int) layout.arrowWidth, (unsigned int) layout.arrowHeight,
		bx, by);
	return;
    }

    if (tree->useTheme) {
	if (TreeTheme_DrawHeaderArrow(tree, drawable,
	    column->arrow == ARROW_UP, x + layout.arrowLeft + sunken,
	    y + (height - (layout.arrowHeight + arrowPadY)) / 2 + sunken,
	    layout.arrowWidth, layout.arrowHeight) == TCL_OK)
	    return;
    }

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

/*
 *----------------------------------------------------------------------
 *
 * Column_Draw --
 *
 *	Draw the header for a column.
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
Column_Draw(
    Column *column,		/* Column record. */
    Drawable drawable,		/* Where to draw. */
    int x, int y,		/* Top-left corner of the column's header. */
    int dragImage		/* TRUE if we are creating a transparent
				 * drag image for this header. */
    )
{
    TreeCtrl *tree = column->tree;
    int height = tree->headerHeight;
    struct Layout layout;
    int width = column->useWidth;
    int sunken = column->state == COLUMN_STATE_PRESSED;
    int relief = sunken ? TK_RELIEF_SUNKEN : TK_RELIEF_RAISED;
    Tk_3DBorder border;
    int theme = TCL_ERROR;

    layout.width = width;
    layout.height = height;
    Column_DoLayout(column, &layout);

    border = PerStateBorder_ForState(tree, &column->border,
	Column_MakeState(column), NULL);
    if (border == NULL)
	border = tree->border;

    if (dragImage) {
	GC gc = Tk_GCForColor(tree->columnDrag.color, Tk_WindowId(tree->tkwin));
	XFillRectangle(tree->display, drawable, gc, x, y, width, height);
    } else {
	if (tree->useTheme) {
	    theme = TreeTheme_DrawHeaderItem(tree, drawable, column->state,
		    column->arrow, x, y, width, height);
	}
	if (theme != TCL_OK)
	    Tk_Fill3DRectangle(tree->tkwin, drawable, border,
		    x, y, width, height, 0, TK_RELIEF_FLAT);
    }

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
	Tree_DrawBitmapWithGC(tree, column->bitmap, drawable, column->bitmapGC,
		0, 0, (unsigned int) imgW, (unsigned int) imgH,
		bx, by);
    }

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
    } else if ((column->text != NULL) && (layout.bytesThatFit != 0)) {
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

    if (dragImage)
	return;

#if defined(MAC_OSX_TK)
    /* Under Aqua, we let the Appearance Manager draw the sort arrow */
    if (theme != TCL_OK)
#endif
    Column_DrawArrow(column, drawable, x, y, layout);

    if (theme != TCL_OK)
	Tk_Draw3DRectangle(tree->tkwin, drawable, border,
		x, y, width, height, column->borderWidth, relief);
}

/*
 *----------------------------------------------------------------------
 *
 * SetImageForColumn --
 *
 *	Set a photo image containing a simplified picture of the header
 *	of a column. This image is used when dragging and dropping a column
 *	header.
 *
 * Results:
 *	Token for a photo image, or NULL if the image could not be
 *	created.
 *
 * Side effects:
 *	A photo image called "TreeCtrlColumnImage" will be created if
 *	it doesn't exist. The image is set to contain a picture of the
 *	column header.
 *
 *----------------------------------------------------------------------
 */

static Tk_Image
SetImageForColumn(
    TreeCtrl *tree,		/* Widget info. */
    Column *column		/* Column record. */
    )
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

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawHeader --
 *
 *	Draw the header of every column.
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
Tree_DrawHeader(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    int x, int y		/* Top-left corner of the header. */
    )
{
    Column *column = (Column *) tree->columns;
    Tk_Window tkwin = tree->tkwin;
    int minX, maxX, width, height;
    Drawable pixmap;
    int x2 = x;

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
		Column_Draw(column, pixmap, x, y, FALSE);
	    x += column->useWidth;
	}
	column = column->next;
    }

    /* Draw "tail" column */
    if (x < maxX) {
	column = (Column *) tree->columnTail;
	width = maxX - x + column->borderWidth;
	height = tree->headerHeight;
	if (tree->useTheme &&
	    (TreeTheme_DrawHeaderItem(tree, pixmap, 0, 0, x, y, width, height) == TCL_OK)) {
	} else {
	    Tk_3DBorder border;
	    border = PerStateBorder_ForState(tree, &column->border,
		Column_MakeState(column), NULL);
	    if (border == NULL)
		border = tree->border;
	    Tk_Fill3DRectangle(tkwin, pixmap, border,
		    x, y, width, height, column->borderWidth, TK_RELIEF_RAISED);
	}
    }

    {
	Tk_Image image = NULL;
	int imageX = 0, imageW = 0, indDraw = FALSE, indX = 0;

	column = (Column *) tree->columns;
	while (column != NULL) {
	    if (column->visible) {
		if (column == (Column *) tree->columnDrag.column) {
		    image = SetImageForColumn(tree, column);
		    imageX = x2;
		    imageW = column->useWidth;
		}
		if (column == (Column *) tree->columnDrag.indColumn) {
		    indX = x2 - 1;
		    indDraw = TRUE;
		}
		x2 += column->useWidth;
	    }
	    if (tree->columnDrag.indColumn == tree->columnTail) {
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
#if !defined(WIN32) && !defined(MAC_TCL) && !defined(MAC_OSX_TK)
	    int ix = 0, iy = 0, iw = imageW, ih = tree->headerHeight;
	    /*
	     * Do boundary clipping, so that Tk_RedrawImage is passed
	     * valid coordinates. [Tk Bug 979239]
	     */
	    imageX += tree->columnDrag.offset;
	    if (imageX < minX) {
		ix = minX - imageX;
		iw -= ix;
		imageX = minX;
	    } else if (imageX + imageW >= maxX) {
		iw -= (imageX + imageW) - maxX;
	    }
	    Tk_RedrawImage(image, ix, iy, iw, ih, pixmap,
		imageX, y);
#else
	    Tk_RedrawImage(image, 0, 0, imageW,
		tree->headerHeight, pixmap,
		imageX + tree->columnDrag.offset, y);
#endif
	    Tk_FreeImage(image);
	}
    }

    if (tree->doubleBuffer == DOUBLEBUFFER_ITEM) {
	XCopyArea(tree->display, pixmap, drawable,
		tree->copyGC, minX, y,
		maxX - minX, tree->headerHeight,
		tree->inset, y);

	Tk_FreePixmap(tree->display, pixmap);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_WidthOfItems --
 *
 *	Calculate the maximum needed width of the styles in every
 *	ReallyVisible() item for a particular column. The width will
 *	only be recalculated if it is marked out-of-date.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	The size of elements and styles will be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

int
TreeColumn_WidthOfItems(
    TreeColumn column_		/* Column token. */
    )
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
#ifdef EXPENSIVE_SPAN_WIDTH /* NOT USED */
	width = TreeItem_NeededWidthOfColumn(tree, item, column->index);
	if (column == (Column *) tree->columnTree)
	    width += TreeItem_Indent(tree, item);
	column->widthOfItems = MAX(column->widthOfItems, width);
#else
	itemColumn = TreeItem_FindColumn(tree, item, column->index);
	if (itemColumn != NULL) {
	    width = TreeItemColumn_NeededWidth(tree, item, itemColumn);
	    if (column == (Column *) tree->columnTree)
		width += TreeItem_Indent(tree, item);
	    column->widthOfItems = MAX(column->widthOfItems, width);
	}
#endif
	item = TreeItem_NextVisible(tree, item);
    }

    return column->widthOfItems;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_LayoutColumns --
 *
 *	Calculates the display width of every column.
 *
 * Results:
 *	The useWidth field of every column is updated.
 *
 * Side effects:
 *	The size of elements and styles will be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

void
Tree_LayoutColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Column *column = (Column *) tree->columns;
    int width, visWidth, totalWidth = 0;
    int numExpand = 0, numSqueeze = 0;

    while (column != NULL) {
	if (column->visible) {
	    if (column->widthObj != NULL)
		width = column->width;
	    else {
		width = TreeColumn_WidthOfItems((TreeColumn) column);
		width = MAX(width, TreeColumn_NeededWidth((TreeColumn) column));
		width = MAX(width, TreeColumn_MinWidth((TreeColumn) column));
		if (TreeColumn_MaxWidth((TreeColumn) column) != -1)
		    width = MIN(width, TreeColumn_MaxWidth((TreeColumn) column));
		if (column->expand)
		    numExpand++;
		if (column->squeeze)
		    numSqueeze++;
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
	int spaceRemaining = totalWidth - visWidth;
	while ((spaceRemaining > 0) && (numSqueeze > 0)) {
	    int each = (spaceRemaining >= numSqueeze) ?
		spaceRemaining / numSqueeze : 1;
	    numSqueeze = 0;
	    column = (Column *) tree->columns;
	    while (column != NULL) {
		if (column->visible && column->squeeze && (column->widthObj == NULL)) {
		    int min = MAX(0, TreeColumn_MinWidth((TreeColumn) column));
		    if (column->useWidth > min) {
			int sub = MIN(each, column->useWidth - min);
			column->useWidth -= sub;
			spaceRemaining -= sub;
			if (!spaceRemaining) break;
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
	int spaceRemaining = visWidth - totalWidth;
	while ((spaceRemaining > 0) && (numExpand > 0)) {
	    int each = (spaceRemaining >= numExpand) ?
		spaceRemaining / numExpand : 1;
	    numExpand = 0;
	    column = (Column *) tree->columns;
	    while (column != NULL) {
		if (column->visible && column->expand && (column->widthObj == NULL)) {
		    int max = TreeColumn_MaxWidth((TreeColumn) column);
		    if ((max == -1) || (column->useWidth < max)) {
			int add = (max == -1) ? each : MIN(each, max - column->useWidth);
			column->useWidth += add;
			spaceRemaining -= add;
			if (!spaceRemaining) break;
			if ((max == -1) || (column->useWidth < max))
			    numExpand++;
		    }
		}
		column = column->next;
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_InvalidateColumnWidth --
 *
 *	Marks the width of zero or more columns as out-of-date.
 *	Schedules a redisplay to check the widths of columns which
 *	will perform any relayout necessary.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Idle task may be scheduled.
 *
 *----------------------------------------------------------------------
 */

void
Tree_InvalidateColumnWidth(
    TreeCtrl *tree,		/* Widget info. */
    int columnIndex		/* Index of the column to modify. -1 means
				 * modify every column. */
    )
{
    Column *column;

#ifdef COLUMN_SPANxxx
    /* It may be necessary to recalculate the width of other columns as
     * well when column-spanning is in effect. */
    columnIndex = -1;
#endif

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

/*
 *----------------------------------------------------------------------
 *
 * Tree_InvalidateColumnHeight --
 *
 *	Marks the height of zero or more column headers as out-of-date.
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
Tree_InvalidateColumnHeight(
    TreeCtrl *tree,		/* Widget info. */
    int columnIndex		/* Index of the column to modify. -1 means
				 * modify every column. */
    )
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

/*
 *----------------------------------------------------------------------
 *
 * TreeColumn_TreeChanged --
 *
 *	Called when a TreeCtrl is configured. Performs any relayout
 *	necessary on column headers.
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
TreeColumn_TreeChanged(
    TreeCtrl *tree,		/* Widget info. */
    int flagT			/* TREE_CONF_xxx flags. */
    )
{
    Column *column;

    /* Column widths are invalidated elsewhere */
    if (flagT & TREE_CONF_FONT) {
	column = (Column *) tree->columns;
	while (column != NULL) {
	    if ((column->tkfont == NULL) && (column->textLen > 0)) {
		column->textWidth = Tk_TextWidth(tree->tkfont, column->text,
		    column->textLen);
		column->neededWidth = column->neededHeight = -1;
		column->textLayoutInvalid = TRUE;
	    }
	    column = column->next;
	}
	tree->headerHeight = -1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_HeaderHeight --
 *
 *	Return the total height of the column header area. The height
 *	is only recalculated if it is marked out-of-date.
 *
 * Results:
 *	Pixel height. Will be zero if the -showheader option is FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tree_HeaderHeight(
    TreeCtrl *tree		/* Widget info. */
    )
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

/*
 *----------------------------------------------------------------------
 *
 * Tree_WidthOfColumns --
 *
 *	Return the total display width of all columns (except the tail).
 *	The width is only recalculated if it is marked out-of-date.
 *	Other fields of the TreeCtrl are updated to reflect the current
 *	arrangement of columns.
 *
 * Results:
 *	Pixel width.
 *
 * Side effects:
 *	The size of elements and styles may be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

int
Tree_WidthOfColumns(
    TreeCtrl *tree		/* Widget info. */
    )
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
	    if (column == (Column *) tree->columnTree) {
		tree->columnTreeLeft = width;
		tree->columnTreeVis = TRUE;
	    }
	    width += column->useWidth;
	}
	column = column->next;
    }

    return tree->widthOfColumns = width;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_InitColumns --
 *
 *	Perform column-related initialization when a new TreeCtrl is
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

void
Tree_InitColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Column *column;

    column = Column_Alloc(tree);
    column->tag = ckalloc(5);
    strcpy(column->tag, "tail");
    column->id = -1;
    tree->columnTail = (TreeColumn) column;
    tree->nextColumnId = 0;
    tree->columnCount = 0;
    Column_Config(column, 0, NULL, TRUE);

    tree->columnDrag.optionTable = Tk_CreateOptionTable(tree->interp, dragSpecs);
    (void) Tk_InitOptions(tree->interp, (char *) tree,
	    tree->columnDrag.optionTable, tree->tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FreeColumns --
 *
 *	Free column-related resources for a deleted TreeCtrl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

void Tree_FreeColumns(
    TreeCtrl *tree		/* Widget info. */
    )
{
    Column *column = (Column *) tree->columns;

    while (column != NULL) {
	column = Column_Free(column);
    }

    Column_Free((Column *) tree->columnTail);
    tree->columnCount = 0;
}

