/* 
 * tkTreeElem.c --
 *
 *	This module implements elements for treectrl widgets.
 *
 * Copyright (c) 2002-2006 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeElem.c,v 1.39 2006/09/27 01:46:13 treectrl Exp $
 */

#include "tkTreeCtrl.h"
#include "tkTreeElem.h"

/* BEGIN custom "boolean" option */

/* Just like TK_OPTION_BOOLEAN but supports TK_OPTION_NULL_OK */
/* Internal value is -1 for no-such-value */

static int BooleanSet(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags)
{
    int objEmpty;
    int new, *internalPtr;

    if (internalOffset >= 0)
	internalPtr = (int *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	if (Tcl_GetBooleanFromObj(interp, (*value), &new) != TCL_OK)
	    return TCL_ERROR;
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL)
	    new = -1;
	*((int *) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

static Tcl_Obj *BooleanGet(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset)
{
    int value = *(int *) (recordPtr + internalOffset);
    if (value == -1)
	return NULL;
    return Tcl_NewBooleanObj(value);
}

static void BooleanRestore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr)
{
    *(int *) internalPtr = *(int *) saveInternalPtr;
}

static Tk_ObjCustomOption booleanCO =
{
    "boolean",
    BooleanSet,
    BooleanGet,
    BooleanRestore,
    NULL,
    (ClientData) NULL
};

/* END custom "boolean" option */

/* BEGIN custom "integer" option */

/* Just like TK_OPTION_INT but supports TK_OPTION_NULL_OK and bounds checking */

typedef struct IntegerClientData
{
    int min;
    int max;
    int empty; /* internal form if empty */
    int flags; /* 0x01 - use min, 0x02 - use max */
} IntegerClientData;

static int IntegerSet(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags)
{
    IntegerClientData *info = (IntegerClientData *) clientData;
    int objEmpty;
    int new, *internalPtr;

    if (internalOffset >= 0)
	internalPtr = (int *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	if (Tcl_GetIntFromObj(interp, (*value), &new) != TCL_OK)
	    return TCL_ERROR;
	if ((info->flags & 0x01) && (new < info->min)) {
	    FormatResult(interp,
		    "bad integer value \"%d\": must be >= %d",
		    new, info->min);
	    return TCL_ERROR;
	}
	if ((info->flags & 0x02) && (new > info->max)) {
	    FormatResult(interp,
		    "bad integer value \"%d\": must be <= %d",
		    new, info->max);
	    return TCL_ERROR;
	}
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL)
	    new = info->empty;
	*((int *) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

static Tcl_Obj *IntegerGet(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset)
{
    IntegerClientData *info = (IntegerClientData *) clientData;
    int value = *(int *) (recordPtr + internalOffset);
    if (value == info->empty)
	return NULL;
    return Tcl_NewIntObj(value);
}

static void IntegerRestore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr)
{
    *(int *) internalPtr = *(int *) saveInternalPtr;
}

/* END custom "integer" option */

/*****/

/* BEGIN custom "stringtable" option */

/* Just like TK_OPTION_STRING_TABLE but supports TK_OPTION_NULL_OK */
/* The integer rep is -1 if empty string specified */

typedef struct StringTableClientData
{
    CONST char **tablePtr; /* NULL-termintated list of strings */
    CONST char *msg; /* Tcl_GetIndexFromObj() message */
} StringTableClientData;

static int StringTableSet(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags)
{
    StringTableClientData *info = (StringTableClientData *) clientData;
    int objEmpty;
    int new, *internalPtr;

    if (internalOffset >= 0)
	internalPtr = (int *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	if (Tcl_GetIndexFromObj(interp, (*value), info->tablePtr,
		    info->msg, 0, &new) != TCL_OK)
	    return TCL_ERROR;
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL)
	    new = -1;
	*((int *) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

static Tcl_Obj *StringTableGet(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset)
{
    StringTableClientData *info = (StringTableClientData *) clientData;
    int index = *(int *) (recordPtr + internalOffset);

    if (index == -1)
	return NULL;
    return Tcl_NewStringObj(info->tablePtr[index], -1);
}

static void StringTableRestore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr)
{
    *(int *) internalPtr = *(int *) saveInternalPtr;
}

/* END custom "stringtable" option */

int BooleanCO_Init(Tk_OptionSpec *optionTable, CONST char *optionName)
{
    int i;

    for (i = 0; optionTable[i].type != TK_OPTION_END; i++) {
	if (!strcmp(optionTable[i].optionName, optionName)) {

	    /* Update the option table */
	    optionTable[i].clientData = (ClientData) &booleanCO;
	    return TCL_OK;
	}
    }
    return TCL_ERROR;
}

int IntegerCO_Init(Tk_OptionSpec *optionTable, CONST char *optionName,
	int min, int max, int empty, int flags)
{
    IntegerClientData *cd;
    Tk_ObjCustomOption *co;
    int i;

    for (i = 0; optionTable[i].type != TK_OPTION_END; i++) {
	if (!strcmp(optionTable[i].optionName, optionName)) {

	    /* ClientData for the Tk custom option record */
	    cd = (IntegerClientData *) ckalloc(sizeof(IntegerClientData));
	    cd->min = min;
	    cd->max = max;
	    cd->empty = empty;
	    cd->flags = flags;

	    /* The Tk custom option record */
	    co = (Tk_ObjCustomOption *) ckalloc(sizeof(Tk_ObjCustomOption));
	    co->name = (char *) optionName + 1;
	    co->setProc = IntegerSet;
	    co->getProc = IntegerGet;
	    co->restoreProc = IntegerRestore;
	    co->freeProc = NULL;
	    co->clientData = (ClientData) cd;

	    /* Update the option table */
	    optionTable[i].clientData = (ClientData) co;
	    return TCL_OK;
	}
    }
    return TCL_ERROR;
}

int StringTableCO_Init(Tk_OptionSpec *optionTable, CONST char *optionName, CONST char **tablePtr)
{
    StringTableClientData *cd;
    Tk_ObjCustomOption *co;
    int i;

    for (i = 0; optionTable[i].type != TK_OPTION_END; i++) {
	if (!strcmp(optionTable[i].optionName, optionName)) {

	    /* ClientData for the Tk custom option record */
	    cd = (StringTableClientData *) ckalloc(sizeof(StringTableClientData));
	    cd->tablePtr = tablePtr;
	    cd->msg = optionName + 1;

	    /* The Tk custom option record */
	    co = (Tk_ObjCustomOption *) ckalloc(sizeof(Tk_ObjCustomOption));
	    co->name = (char *) optionName + 1;
	    co->setProc = StringTableSet;
	    co->getProc = StringTableGet;
	    co->restoreProc = StringTableRestore;
	    co->freeProc = NULL;
	    co->clientData = (ClientData) cd;

	    /* Update the option table */
	    optionTable[i].clientData = (ClientData) co;
	    return TCL_OK;
	}
    }
    return TCL_ERROR;
}

/*****/

int TreeStateFromObj(TreeCtrl *tree, Tcl_Obj *obj, int *stateOff, int *stateOn)
{
    int states[3];

    states[STATE_OP_ON] = states[STATE_OP_OFF] = states[STATE_OP_TOGGLE] = 0;
    if (Tree_StateFromObj(tree, obj, states, NULL, SFO_NOT_TOGGLE) != TCL_OK)
	return TCL_ERROR;

    (*stateOn) |= states[STATE_OP_ON];
    (*stateOff) |= states[STATE_OP_OFF];
    return TCL_OK;
}

static void AdjustForSticky(int sticky, int cavityWidth, int cavityHeight,
    int expandX, int expandY,
    int *xPtr, int *yPtr, int *widthPtr, int *heightPtr)
{
    int dx = 0;
    int dy = 0;

    if (cavityWidth > *widthPtr) {
	dx = cavityWidth - *widthPtr;
    }

    if (cavityHeight > *heightPtr) {
	dy = cavityHeight - *heightPtr;
    }

    if ((sticky & STICKY_W) && (sticky & STICKY_E)) {
	if (expandX)
	    *widthPtr += dx;
	else
	    sticky &= ~(STICKY_W | STICKY_E);
    }
    if ((sticky & STICKY_N) && (sticky & STICKY_S)) {
	if (expandY)
	    *heightPtr += dy;
	else
	    sticky &= ~(STICKY_N | STICKY_S);
    }
    if (!(sticky & STICKY_W)) {
	*xPtr += (sticky & STICKY_E) ? dx : dx / 2;
    }
    if (!(sticky & STICKY_N)) {
	*yPtr += (sticky & STICKY_S) ? dy : dy / 2;
    }
}

/* This macro gets the value of a per-state option for an element, then
 * looks for a better match from the master element if it exists */
#define OPTION_FOR_STATE(xFUNC,xTYPE,xVAR,xFIELD,xSTATE) \
    xVAR = xFUNC(tree, &elemX->xFIELD, xSTATE, &match); \
    if ((match != MATCH_EXACT) && (masterX != NULL)) { \
	xTYPE varM = xFUNC(tree, &masterX->xFIELD, xSTATE, &match2); \
	if (match2 > match) \
	    xVAR = varM; \
    }
#define BITMAP_FOR_STATE(xVAR,xFIELD,xSTATE) \
    OPTION_FOR_STATE(PerStateBitmap_ForState,Pixmap,xVAR,xFIELD,xSTATE)
#define BOOLEAN_FOR_STATE(xVAR,xFIELD,xSTATE) \
    OPTION_FOR_STATE(PerStateBoolean_ForState,int,xVAR,xFIELD,xSTATE)
#define BORDER_FOR_STATE(xVAR,xFIELD,xSTATE) \
    OPTION_FOR_STATE(PerStateBorder_ForState,Tk_3DBorder,xVAR,xFIELD,xSTATE)
#define COLOR_FOR_STATE(xVAR,xFIELD,xSTATE) \
    OPTION_FOR_STATE(PerStateColor_ForState,XColor*,xVAR,xFIELD,xSTATE)
#define FONT_FOR_STATE(xVAR,xFIELD,xSTATE) \
    OPTION_FOR_STATE(PerStateFont_ForState,Tk_Font,xVAR,xFIELD,xSTATE)
#define IMAGE_FOR_STATE(xVAR,xFIELD,xSTATE) \
    OPTION_FOR_STATE(PerStateImage_ForState,Tk_Image,xVAR,xFIELD,xSTATE)
#define RELIEF_FOR_STATE(xVAR,xFIELD,xSTATE) \
    OPTION_FOR_STATE(PerStateRelief_ForState,int,xVAR,xFIELD,xSTATE)

/* This macro gets the object for a per-state option for an element, then
 * looks for a better match from the master element if it exists */
#define OBJECT_FOR_STATE(xVAR,xTYPE,xFIELD,xSTATE) \
    xVAR = PerStateInfo_ObjForState(tree, &xTYPE, &elemX->xFIELD, xSTATE, &match); \
    if ((match != MATCH_EXACT) && (masterX != NULL)) { \
	Tcl_Obj *objM = PerStateInfo_ObjForState(tree, &xTYPE, &masterX->xFIELD, xSTATE, &matchM); \
	if (matchM > match) \
	    xVAR = objM; \
    }

/*****/

typedef struct ElementBitmap ElementBitmap;

struct ElementBitmap
{
    Element header;
    PerStateInfo draw;
    PerStateInfo bitmap;
    PerStateInfo fg;
    PerStateInfo bg;
};

#define BITMAP_CONF_BITMAP 0x0001
#define BITMAP_CONF_FG 0x0002
#define BITMAP_CONF_BG 0x0004
#define BITMAP_CONF_DRAW 0x0008

static Tk_OptionSpec bitmapOptionSpecs[] = {
    {TK_OPTION_STRING, "-background", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBitmap, bg.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, BITMAP_CONF_BG},
    {TK_OPTION_STRING, "-bitmap", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBitmap, bitmap.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, BITMAP_CONF_BITMAP},
    {TK_OPTION_STRING, "-draw", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBitmap, draw.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, BITMAP_CONF_DRAW},
    {TK_OPTION_STRING, "-foreground", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBitmap, fg.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, BITMAP_CONF_FG},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteProcBitmap(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBitmap *elemX = (ElementBitmap *) elem;

    PerStateInfo_Free(tree, &pstBoolean, &elemX->draw);
    PerStateInfo_Free(tree, &pstBitmap, &elemX->bitmap);
    PerStateInfo_Free(tree, &pstColor, &elemX->fg);
    PerStateInfo_Free(tree, &pstColor, &elemX->bg);
}

static int WorldChangedProcBitmap(ElementArgs *args)
{
    int flagM = args->change.flagMaster;
    int flagS = args->change.flagSelf;
    int mask = 0;

    if ((flagS | flagM) & BITMAP_CONF_BITMAP)
	mask |= CS_DISPLAY | CS_LAYOUT;

    if ((flagS | flagM) & (BITMAP_CONF_DRAW | BITMAP_CONF_FG | BITMAP_CONF_BG))
	mask |= CS_DISPLAY;

    return mask;
}

static int ConfigProcBitmap(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBitmap *elemX = (ElementBitmap *) elem;
    ElementBitmap savedX;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) elemX,
			elem->typePtr->optionTable,
			args->config.objc, args->config.objv, tree->tkwin,
			&savedOptions, &args->config.flagSelf) != TCL_OK) {
		args->config.flagSelf = 0;
		continue;
	    }

	    if (args->config.flagSelf & BITMAP_CONF_DRAW)
		PSTSave(&elemX->draw, &savedX.draw);
	    if (args->config.flagSelf & BITMAP_CONF_BITMAP)
		PSTSave(&elemX->bitmap, &savedX.bitmap);
	    if (args->config.flagSelf & BITMAP_CONF_FG)
		PSTSave(&elemX->fg, &savedX.fg);
	    if (args->config.flagSelf & BITMAP_CONF_BG)
		PSTSave(&elemX->bg, &savedX.bg);

	    if (args->config.flagSelf & BITMAP_CONF_DRAW) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBoolean, &elemX->draw) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & BITMAP_CONF_BITMAP) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBitmap, &elemX->bitmap) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & BITMAP_CONF_FG) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstColor, &elemX->fg) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & BITMAP_CONF_BG) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstColor, &elemX->bg) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & BITMAP_CONF_DRAW)
		PerStateInfo_Free(tree, &pstBoolean, &savedX.draw);
	    if (args->config.flagSelf & BITMAP_CONF_BITMAP)
		PerStateInfo_Free(tree, &pstBitmap, &savedX.bitmap);
	    if (args->config.flagSelf & BITMAP_CONF_FG)
		PerStateInfo_Free(tree, &pstColor, &savedX.fg);
	    if (args->config.flagSelf & BITMAP_CONF_BG)
		PerStateInfo_Free(tree, &pstColor, &savedX.bg);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (args->config.flagSelf & BITMAP_CONF_DRAW)
		PSTRestore(tree, &pstBoolean, &elemX->draw, &savedX.draw);

	    if (args->config.flagSelf & BITMAP_CONF_BITMAP)
		PSTRestore(tree, &pstBitmap, &elemX->bitmap, &savedX.bitmap);

	    if (args->config.flagSelf & BITMAP_CONF_FG)
		PSTRestore(tree, &pstColor, &elemX->fg, &savedX.fg);

	    if (args->config.flagSelf & BITMAP_CONF_BG)
		PSTRestore(tree, &pstColor, &elemX->bg, &savedX.bg);

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

static int CreateProcBitmap(ElementArgs *args)
{
    return TCL_OK;
}

static void DisplayProcBitmap(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBitmap *elemX = (ElementBitmap *) elem;
    ElementBitmap *masterX = (ElementBitmap *) elem->master;
    int state = args->state;
    int x = args->display.x, y = args->display.y;
    int width, height;
    int match, match2;
    int draw;
    Pixmap bitmap;
    XColor *fg, *bg;
    int imgW, imgH;

    BOOLEAN_FOR_STATE(draw, draw, state)
    if (!draw)
	return;

    BITMAP_FOR_STATE(bitmap, bitmap, state)
    if (bitmap == None)
	return;

    COLOR_FOR_STATE(fg, fg, state)
    COLOR_FOR_STATE(bg, bg, state)

    Tk_SizeOfBitmap(tree->display, bitmap, &imgW, &imgH);
    width = imgW, height = imgH;
    AdjustForSticky(args->display.sticky,
	args->display.width, args->display.height,
	FALSE, FALSE,
	&x, &y, &width, &height);
    if (imgW > args->display.width)
	imgW = args->display.width;
    if (imgH > args->display.height)
	imgH = args->display.height;
    Tree_DrawBitmap(tree, bitmap, args->display.drawable, fg, bg,
	0, 0, (unsigned int) imgW, (unsigned int) imgH,
	x, y);
}

static void NeededProcBitmap(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBitmap *elemX = (ElementBitmap *) elem;
    ElementBitmap *masterX = (ElementBitmap *) elem->master;
    int state = args->state;
    int width = 0, height = 0;
    int match, match2;
    Pixmap bitmap;

    BITMAP_FOR_STATE(bitmap, bitmap, state)

    if (bitmap != None)
	Tk_SizeOfBitmap(tree->display, bitmap, &width, &height);

    args->needed.width = width;
    args->needed.height = height;
}

static int StateProcBitmap(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBitmap *elemX = (ElementBitmap *) elem;
    ElementBitmap *masterX = (ElementBitmap *) elem->master;
    int match, match2;
    int draw1, draw2;
    Pixmap bitmap1, bitmap2;
    XColor *fg1, *fg2;
    XColor *bg1, *bg2;
    int mask = 0;

    BOOLEAN_FOR_STATE(draw1, draw, args->states.state1)
    if (draw1 == -1)
	draw1 = 1;
    BITMAP_FOR_STATE(bitmap1, bitmap, args->states.state1)
    COLOR_FOR_STATE(fg1, fg, args->states.state1)
    COLOR_FOR_STATE(bg1, bg, args->states.state1)

    BOOLEAN_FOR_STATE(draw2, draw, args->states.state2)
    if (draw2 == -1)
	draw2 = 1;
    BITMAP_FOR_STATE(bitmap2, bitmap, args->states.state2)
    COLOR_FOR_STATE(fg2, fg, args->states.state2)
    COLOR_FOR_STATE(bg2, bg, args->states.state2)

    if ((draw1 != draw2) || (fg1 != fg2) || (bg1 != bg2) || (bitmap1 != bitmap2))
	mask |= CS_DISPLAY;

    if (bitmap1 != bitmap2) {
	if ((bitmap1 != None) && (bitmap2 != None)) {
	    int w1, h1, w2, h2;
	    Tk_SizeOfBitmap(tree->display, bitmap1, &w1, &h1);
	    Tk_SizeOfBitmap(tree->display, bitmap2, &w2, &h2);
	    if ((w1 != w2) || (h1 != h2))
		mask |= CS_LAYOUT;
	} else
	    mask |= CS_LAYOUT;
    }

    return mask;
}

static int UndefProcBitmap(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementBitmap *elemX = (ElementBitmap *) args->elem;
    int modified = 0;

    modified |= PerStateInfo_Undefine(tree, &pstBoolean, &elemX->draw, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstColor, &elemX->fg, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstColor, &elemX->bg, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstBitmap, &elemX->bitmap, args->state);
    return modified;
}

static int ActualProcBitmap(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementBitmap *elemX = (ElementBitmap *) args->elem;
    ElementBitmap *masterX = (ElementBitmap *) args->elem->master;
    static CONST char *optionName[] = {
	"-background", "-bitmap", "-draw", "-foreground",
	(char *) NULL };
    int index, match, matchM;
    Tcl_Obj *obj = NULL;

    if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	case 0:
	{
	    OBJECT_FOR_STATE(obj, pstColor, bg, args->state)
	    break;
	}
	case 1:
	{
	    OBJECT_FOR_STATE(obj, pstBitmap, bitmap, args->state)
	    break;
	}
	case 2:
	{
	    OBJECT_FOR_STATE(obj, pstBoolean, draw, args->state)
	    break;
	}
	case 3:
	{
	    OBJECT_FOR_STATE(obj, pstColor, fg, args->state)
	    break;
	}
    }
    if (obj != NULL)
	Tcl_SetObjResult(tree->interp, obj);
    return TCL_OK;
}

ElementType elemTypeBitmap = {
    "bitmap",
    sizeof(ElementBitmap),
    bitmapOptionSpecs,
    NULL,
    CreateProcBitmap,
    DeleteProcBitmap,
    ConfigProcBitmap,
    DisplayProcBitmap,
    NeededProcBitmap,
    NULL, /* heightProc */
    WorldChangedProcBitmap,
    StateProcBitmap,
    UndefProcBitmap,
    ActualProcBitmap,
    NULL /* onScreenProc */
};

/*****/

typedef struct ElementBorder ElementBorder;

struct ElementBorder
{
    Element header; /* Must be first */
    PerStateInfo draw;
    PerStateInfo border;
    PerStateInfo relief;
    int thickness;
    Tcl_Obj *thicknessObj;
    int width;
    Tcl_Obj *widthObj;
    int height;
    Tcl_Obj *heightObj;
    int filled;
};

#define BORDER_CONF_BG 0x0001
#define BORDER_CONF_RELIEF 0x0002
#define BORDER_CONF_SIZE 0x0004
#define BORDER_CONF_THICKNESS 0x0008
#define BORDER_CONF_FILLED 0x0010
#define BORDER_CONF_DRAW 0x0020

static Tk_OptionSpec borderOptionSpecs[] = {
    {TK_OPTION_STRING, "-background", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBorder, border.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_BG},
    {TK_OPTION_STRING, "-draw", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBorder, draw.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_DRAW},
    {TK_OPTION_CUSTOM, "-filled", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementBorder, filled),
     TK_OPTION_NULL_OK, (ClientData) &booleanCO, BORDER_CONF_FILLED},
    {TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBorder, heightObj),
     Tk_Offset(ElementBorder, height),
     TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_SIZE},
    {TK_OPTION_STRING, "-relief", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBorder, relief.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_RELIEF},
    {TK_OPTION_PIXELS, "-thickness", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBorder, thicknessObj),
     Tk_Offset(ElementBorder, thickness),
     TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_THICKNESS},
    {TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementBorder, widthObj),
     Tk_Offset(ElementBorder, width),
     TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_SIZE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteProcBorder(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBorder *elemX = (ElementBorder *) elem;

    PerStateInfo_Free(tree, &pstBoolean, &elemX->draw);
    PerStateInfo_Free(tree, &pstBorder, &elemX->border);
    PerStateInfo_Free(tree, &pstRelief, &elemX->relief);
}

static int WorldChangedProcBorder(ElementArgs *args)
{
    int flagM = args->change.flagMaster;
    int flagS = args->change.flagSelf;
    int mask = 0;

    if ((flagS | flagM) & BORDER_CONF_SIZE)
	mask |= CS_DISPLAY | CS_LAYOUT;

    if ((flagS | flagM) & (BORDER_CONF_DRAW | BORDER_CONF_BG |
		BORDER_CONF_RELIEF | BORDER_CONF_THICKNESS |
		BORDER_CONF_FILLED))
	mask |= CS_DISPLAY;

    return mask;
}

static int ConfigProcBorder(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBorder *elemX = (ElementBorder *) elem;
    ElementBorder savedX;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) elemX,
			elem->typePtr->optionTable,
			args->config.objc, args->config.objv, tree->tkwin,
			&savedOptions, &args->config.flagSelf) != TCL_OK) {
		args->config.flagSelf = 0;
		continue;
	    }

	    if (args->config.flagSelf & BORDER_CONF_DRAW)
		PSTSave(&elemX->draw, &savedX.draw);
	    if (args->config.flagSelf & BORDER_CONF_BG)
		PSTSave(&elemX->border, &savedX.border);
	    if (args->config.flagSelf & BORDER_CONF_RELIEF)
		PSTSave(&elemX->relief, &savedX.relief);

	    if (args->config.flagSelf & BORDER_CONF_DRAW) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBoolean, &elemX->draw) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & BORDER_CONF_BG) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBorder, &elemX->border) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & BORDER_CONF_RELIEF) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstRelief, &elemX->relief) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & BORDER_CONF_DRAW)
		PerStateInfo_Free(tree, &pstBoolean, &savedX.draw);
	    if (args->config.flagSelf & BORDER_CONF_BG)
		PerStateInfo_Free(tree, &pstBorder, &savedX.border);
	    if (args->config.flagSelf & BORDER_CONF_RELIEF)
		PerStateInfo_Free(tree, &pstRelief, &savedX.relief);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (args->config.flagSelf & BORDER_CONF_DRAW)
		PSTRestore(tree, &pstBoolean, &elemX->draw, &savedX.draw);

	    if (args->config.flagSelf & BORDER_CONF_BG)
		PSTRestore(tree, &pstBorder, &elemX->border, &savedX.border);

	    if (args->config.flagSelf & BORDER_CONF_RELIEF)
		PSTRestore(tree, &pstRelief, &elemX->relief, &savedX.relief);

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

static int CreateProcBorder(ElementArgs *args)
{
    Element *elem = args->elem;
    ElementBorder *elemX = (ElementBorder *) elem;

    elemX->filled = -1;
    return TCL_OK;
}

static void DisplayProcBorder(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBorder *elemX = (ElementBorder *) elem;
    ElementBorder *masterX = (ElementBorder *) elem->master;
    int state = args->state;
    int x = args->display.x, y = args->display.y;
    int width = args->display.width, height = args->display.height;
    int match, match2;
    int draw;
    Tk_3DBorder border;
    int relief, filled = FALSE;
    int thickness = 0;

    BOOLEAN_FOR_STATE(draw, draw, state)
    if (!draw)
	return;

    BORDER_FOR_STATE(border, border, state)
    if (border == NULL)
	return;

    RELIEF_FOR_STATE(relief, relief, state)
    if (relief == TK_RELIEF_NULL)
	relief = TK_RELIEF_FLAT;

    if (elemX->thicknessObj)
	thickness = elemX->thickness;
    else if ((masterX != NULL) && (masterX->thicknessObj != NULL))
	thickness = masterX->thickness;

    if (elemX->filled != -1)
	filled = elemX->filled;
    else if ((masterX != NULL) && (masterX->filled != -1))
	filled = masterX->filled;

    if (elemX->widthObj != NULL)
	width = elemX->width;
    else if ((masterX != NULL) && (masterX->widthObj != NULL))
	width = masterX->width;

    if (elemX->heightObj != NULL)
	height = elemX->height;
    else if ((masterX != NULL) && (masterX->heightObj != NULL))
	height = masterX->height;

    AdjustForSticky(args->display.sticky,
	args->display.width, args->display.height,
	TRUE, TRUE,
	&x, &y, &width, &height);

    if (filled) {
	Tk_Fill3DRectangle(tree->tkwin, args->display.drawable, border,
		x, y, width, height, thickness, relief);
    } else if (thickness > 0) {
	Tk_Draw3DRectangle(tree->tkwin, args->display.drawable, border,
		x, y, width, height, thickness, relief);
    }
}

static void NeededProcBorder(ElementArgs *args)
{
    Element *elem = args->elem;
    ElementBorder *elemX = (ElementBorder *) elem;
    ElementBorder *masterX = (ElementBorder *) elem->master;
    int width = 0, height = 0;

    if (elemX->widthObj != NULL)
	width = elemX->width;
    else if ((masterX != NULL) && (masterX->widthObj != NULL))
	width = masterX->width;

    if (elemX->heightObj != NULL)
	height = elemX->height;
    else if ((masterX != NULL) && (masterX->heightObj != NULL))
	height = masterX->height;

    args->needed.width = width;
    args->needed.height = height;
}

static int StateProcBorder(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBorder *elemX = (ElementBorder *) elem;
    ElementBorder *masterX = (ElementBorder *) elem->master;
    int match, match2;
    int draw1, draw2;
    Tk_3DBorder border1, border2;
    int relief1, relief2;
    int mask = 0;

    BOOLEAN_FOR_STATE(draw1, draw, args->states.state1)
    if (draw1 == -1)
	draw1 = 1;
    BORDER_FOR_STATE(border1, border, args->states.state1)
    RELIEF_FOR_STATE(relief1, relief, args->states.state1)

    BOOLEAN_FOR_STATE(draw2, draw, args->states.state2)
    if (draw2 == -1)
	draw2 = 1;
    BORDER_FOR_STATE(border2, border, args->states.state2)
    RELIEF_FOR_STATE(relief2, relief, args->states.state2)

    if ((draw1 != draw2) || (border1 != border2) || (relief1 != relief2))
	mask |= CS_DISPLAY;

    return mask;
}

static int UndefProcBorder(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementBorder *elemX = (ElementBorder *) args->elem;
    int modified = 0;

    modified |= PerStateInfo_Undefine(tree, &pstBoolean, &elemX->draw, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstBorder, &elemX->border, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstRelief, &elemX->relief, args->state);
    return modified;
}

static int ActualProcBorder(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementBorder *elemX = (ElementBorder *) args->elem;
    ElementBorder *masterX = (ElementBorder *) args->elem->master;
    static CONST char *optionName[] = {
	"-background", "-draw", "-relief",
	(char *) NULL };
    int index, match, matchM;
    Tcl_Obj *obj = NULL;

    if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	case 0:
	{
	    OBJECT_FOR_STATE(obj, pstBorder, border, args->state)
	    break;
	}
	case 1:
	{
	    OBJECT_FOR_STATE(obj, pstBoolean, draw, args->state)
	    break;
	}
	case 2:
	{
	    OBJECT_FOR_STATE(obj, pstRelief, relief, args->state)
	    break;
	}
    }
    if (obj != NULL)
	Tcl_SetObjResult(tree->interp, obj);
    return TCL_OK;
}

ElementType elemTypeBorder = {
    "border",
    sizeof(ElementBorder),
    borderOptionSpecs,
    NULL,
    CreateProcBorder,
    DeleteProcBorder,
    ConfigProcBorder,
    DisplayProcBorder,
    NeededProcBorder,
    NULL, /* heightProc */
    WorldChangedProcBorder,
    StateProcBorder,
    UndefProcBorder,
    ActualProcBorder,
    NULL /* onScreenProc */
};

/*****/
#if 0

static CONST char *chkbutStateST[] = {
    "checked", "mixed", "normal", "active", "pressed", "disabled", (char *) NULL
};

typedef struct ElementCheckButton ElementCheckButton;

struct ElementCheckButton
{
    Element header;
    PerStateInfo image;
    int state;
};

#define CHKBUT_CONF_IMAGE 0x0001
#define CHKBUT_CONF_STATE 0x0002

static Tk_OptionSpec chkbutOptionSpecs[] = {
    {TK_OPTION_STRING, "-image", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementCheckButton, image.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, CHKBUT_CONF_IMAGE},
    {TK_OPTION_STRING_TABLE, "-state", (char *) NULL, (char *) NULL,
     "normal", -1, Tk_Offset(ElementCheckButton, state),
     0, (ClientData) chkbutStateST, CHKBUT_CONF_STATE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteProcCheckButton(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementCheckButton *elemX = (ElementCheckButton *) elem;

    PerStateInfo_Free(tree, &pstImage, &elemX->image);
}

static int WorldChangedProcCheckButton(ElementArgs *args)
{
    int flagM = args->change.flagMaster;
    int flagS = args->change.flagSelf;
    int mask = 0;

    if ((flagS | flagM) & (CHKBUT_CONF_IMAGE | CHKBUT_CONF_STATE))
	mask |= CS_DISPLAY | CS_LAYOUT;

    return mask;
}

static int ChkButStateFromObj(TreeCtrl *tree, Tcl_Obj *obj, int *stateOff, int *stateOn)
{
    Tcl_Interp *interp = tree->interp;
    int i, op = STATE_OP_ON, op2, op3, length, state = 0;
    char ch0, *string;
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
    for (i = 0; chkbutStateST[i] != NULL; i++) {
	if ((ch0 == chkbutStateST[i][0]) && !strcmp(string, chkbutStateST[i])) {
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

static int ConfigProcCheckButton(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementCheckButton *elemX = (ElementCheckButton *) elem;
    ElementCheckButton savedX;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) elemX,
			elem->typePtr->optionTable,
			args->config.objc, args->config.objv, tree->tkwin,
			&savedOptions, &args->config.flagSelf) != TCL_OK) {
		args->config.flagSelf = 0;
		continue;
	    }

	    if (args->config.flagSelf & CHKBUT_CONF_IMAGE)
		PSTSave(&elemX->image, &savedX.image);

	    if (args->config.flagSelf & CHKBUT_CONF_IMAGE) {
		if (PerStateInfo_FromObj(tree, ChkButStateFromObj, &pstImage, &elemX->image) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & CHKBUT_CONF_IMAGE)
		PerStateInfo_Free(tree, &pstImage, &savedX.image);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (args->config.flagSelf & CHKBUT_CONF_IMAGE)
		PSTRestore(tree, &pstImage, &elemX->image, &savedX.image);

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

static int CreateProcCheckButton(ElementArgs *args)
{
    return TCL_OK;
}

static void DisplayProcCheckButton(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementCheckButton *elemX = (ElementCheckButton *) elem;
    ElementCheckButton *masterX = (ElementCheckButton *) elem->master;
    int state = args->state;
    int match, matchM;
    Tk_Image image;
    int imgW, imgH;
    int dx = 0, dy = 0;

    image = PerStateImage_ForState(tree, &elemX->image, state, &match);
    if ((match != MATCH_EXACT) && (masterX != NULL)) {
	Tk_Image imageM = PerStateImage_ForState(tree, &masterX->image,
		state, &matchM);
	if (matchM > match)
	    image = imageM;
    }

    if (image != NULL) {
	Tk_SizeOfImage(image, &imgW, &imgH);
	if (imgW < args->display.width)
	    dx = (args->display.width - imgW) / 2;
	else if (imgW > args->display.width)
	    imgW = args->display.width;
	if (imgH < args->display.height)
	    dy = (args->display.height - imgH) / 2;
	else if (imgH > args->display.height)
	    imgH = args->display.height;
	Tk_RedrawImage(image, 0, 0, imgW, imgH, args->display.drawable,
		args->display.x + dx,
		args->display.y + dy);
    }
}

static void NeededProcCheckButton(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementCheckButton *elemX = (ElementCheckButton *) elem;
    ElementCheckButton *masterX = (ElementCheckButton *) elem->master;
    int state = args->state;
    int match, match2;
    Tk_Image image;
    int width = 0, height = 0;

    image = PerStateImage_ForState(tree, &elemX->image, state, &match);
    if ((match != MATCH_EXACT) && (masterX != NULL)) {
	Tk_Image image2 = PerStateImage_ForState(tree, &masterX->image,
		state, &match2);
	if (match2 > match)
	    image = image2;
    }

    if (image != NULL)
	Tk_SizeOfImage(image, &width, &height);

    args->layout.width = width;
    args->layout.height = height;
}

static int StateProcCheckButton(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementCheckButton *elemX = (ElementCheckButton *) elem;
    ElementCheckButton *masterX = (ElementCheckButton *) elem->master;
    int match, match2;
    Tk_Image image1, image2;
    int mask = 0;

    image1 = PerStateImage_ForState(tree, &elemX->image,
	    args->states.state1, &match);
    if ((match != MATCH_EXACT) && (masterX != NULL)) {
	Tk_Image image = PerStateImage_ForState(tree, &masterX->image, args->states.state1, &match2);
	if (match2 > match)
	    image1 = image;
    }

    image2 = PerStateImage_ForState(tree, &elemX->image,
	    args->states.state2, &match);
    if ((match != MATCH_EXACT) && (masterX != NULL)) {
	Tk_Image image = PerStateImage_ForState(tree, &masterX->image,
		args->states.state2, &match2);
	if (match2 > match)
	    image2 = image;
    }

    if (image1 != image2) {
	mask |= CS_DISPLAY;
	if ((image1 != NULL) && (image2 != NULL)) {
	    int w1, h1, w2, h2;
	    Tk_SizeOfImage(image1, &w1, &h1);
	    Tk_SizeOfImage(image2, &w2, &h2);
	    if ((w1 != w2) || (h1 != h2))
		mask |= CS_LAYOUT;
	} else
	    mask |= CS_LAYOUT;
    }

    return mask;
}

static int UndefProcCheckButton(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementCheckButton *elemX = (ElementCheckButton *) args->elem;

    return PerStateInfo_Undefine(tree, &pstImage, &elemX->image, args->state);
}

static int ActualProcCheckButton(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementCheckButton *elemX = (ElementCheckButton *) args->elem;
    ElementCheckButton *masterX = (ElementCheckButton *) args->elem->master;
    static CONST char *optionName[] = {
	"-image",
	(char *) NULL };
    int index, match, matchM;
    Tcl_Obj *obj = NULL;

    if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	case 0:
	{
	    obj = PerStateInfo_ObjForState(tree, &pstImage,
		    &elemX->image, args->state, &match);
	    if ((match != MATCH_EXACT) && (masterX != NULL)) {
		objM = PerStateInfo_ObjForState(tree, &pstImage,
			&masterX->image, args->state, &matchM);
		if (matchM > match)
		    obj = objM;
	    }
	    break;
	}
    }
    if (obj != NULL)
	Tcl_SetObjResult(tree->interp, obj);
    return TCL_OK;
}

ElementType elemTypeCheckButton = {
    "checkbutton",
    sizeof(ElementCheckButton),
    chkbutOptionSpecs,
    NULL,
    CreateProcCheckButton,
    DeleteProcCheckButton,
    ConfigProcCheckButton,
    DisplayProcCheckButton,
    NeededProcCheckButton,
    NULL, /* heightProc */
    WorldChangedProcCheckButton,
    StateProcCheckButton,
    UndefProcCheckButton,
    ActualProcCheckButton,
    NULL /* onScreenProc */
};

#endif

/*****/

typedef struct ElementImage ElementImage;

struct ElementImage
{
    Element header;
    PerStateInfo draw;
    PerStateInfo image;
    int width;
    Tcl_Obj *widthObj;
    int height;
    Tcl_Obj *heightObj;
    int tiled;
};

#define IMAGE_CONF_IMAGE 0x0001
#define IMAGE_CONF_SIZE 0x0002
#define IMAGE_CONF_DRAW 0x0004
#define IMAGE_CONF_DISPLAY 0x0008

static Tk_OptionSpec imageOptionSpecs[] = {
    {TK_OPTION_STRING, "-draw", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementImage, draw.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, IMAGE_CONF_DRAW},
    {TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementImage, heightObj),
     Tk_Offset(ElementImage, height),
     TK_OPTION_NULL_OK, (ClientData) NULL, IMAGE_CONF_SIZE},
    {TK_OPTION_STRING, "-image", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementImage, image.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, IMAGE_CONF_IMAGE},
    {TK_OPTION_CUSTOM, "-tiled", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementImage, tiled),
     TK_OPTION_NULL_OK, (ClientData) &booleanCO, IMAGE_CONF_DISPLAY},
    {TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementImage, widthObj),
     Tk_Offset(ElementImage, width),
     TK_OPTION_NULL_OK, (ClientData) NULL, IMAGE_CONF_SIZE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteProcImage(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementImage *elemX = (ElementImage *) elem;

    PerStateInfo_Free(tree, &pstBoolean, &elemX->draw);
    PerStateInfo_Free(tree, &pstImage, &elemX->image);
}

static int WorldChangedProcImage(ElementArgs *args)
{
    int flagM = args->change.flagMaster;
    int flagS = args->change.flagSelf;
    int mask = 0;

    if ((flagS | flagM) & (IMAGE_CONF_DRAW | IMAGE_CONF_IMAGE | IMAGE_CONF_SIZE))
	mask |= CS_DISPLAY | CS_LAYOUT;
    if ((flagS | flagM) & IMAGE_CONF_DISPLAY)
	mask |= CS_DISPLAY;

    return mask;
}

static int ConfigProcImage(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementImage *elemX = (ElementImage *) elem;
    ElementImage savedX;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) elemX,
			elem->typePtr->optionTable,
			args->config.objc, args->config.objv, tree->tkwin,
			&savedOptions, &args->config.flagSelf) != TCL_OK) {
		args->config.flagSelf = 0;
		continue;
	    }

	    if (args->config.flagSelf & IMAGE_CONF_DRAW)
		PSTSave(&elemX->draw, &savedX.draw);
	    if (args->config.flagSelf & IMAGE_CONF_IMAGE)
		PSTSave(&elemX->image, &savedX.image);

	    if (args->config.flagSelf & IMAGE_CONF_DRAW) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBoolean, &elemX->draw) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & IMAGE_CONF_IMAGE) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstImage, &elemX->image) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & IMAGE_CONF_DRAW)
		PerStateInfo_Free(tree, &pstBoolean, &savedX.draw);
	    if (args->config.flagSelf & IMAGE_CONF_IMAGE)
		PerStateInfo_Free(tree, &pstImage, &savedX.image);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (args->config.flagSelf & IMAGE_CONF_DRAW)
		PSTRestore(tree, &pstBoolean, &elemX->draw, &savedX.draw);

	    if (args->config.flagSelf & IMAGE_CONF_IMAGE)
		PSTRestore(tree, &pstImage, &elemX->image, &savedX.image);

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

static int CreateProcImage(ElementArgs *args)
{
    ElementImage *elemX = (ElementImage *) args->elem;

    elemX->tiled = -1;
    return TCL_OK;
}

static void DisplayProcImage(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementImage *elemX = (ElementImage *) elem;
    ElementImage *masterX = (ElementImage *) elem->master;
    int state = args->state;
    int x = args->display.x, y = args->display.y;
    int width, height;
    int match, match2;
    int draw;
    Tk_Image image;
    int imgW, imgH;
    int tiled = 0;

    BOOLEAN_FOR_STATE(draw, draw, state)
    if (!draw)
	return;

    IMAGE_FOR_STATE(image, image, state)
    if (image == NULL)
	return;

    if (elemX->tiled != -1)
	tiled = elemX->tiled;
    else if ((masterX != NULL) && (masterX->tiled != -1))
	tiled = masterX->tiled;
    if (tiled) {
	Tree_DrawTiledImage(tree, args->display.drawable, image, x, y,
	    x + args->display.width, y + args->display.height, -x, -y);
	return;
    }

    Tk_SizeOfImage(image, &imgW, &imgH);
    width = imgW, height = imgH;
    AdjustForSticky(args->display.sticky,
	args->display.width, args->display.height,
	FALSE, FALSE,
	&x, &y, &width, &height);
    if (imgW > args->display.width)
	imgW = args->display.width;
    if (imgH > args->display.height)
	imgH = args->display.height;
    Tk_RedrawImage(image, 0, 0, imgW, imgH, args->display.drawable, x, y);
}

static void NeededProcImage(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementImage *elemX = (ElementImage *) elem;
    ElementImage *masterX = (ElementImage *) elem->master;
    int state = args->state;
    int width = 0, height = 0;
    int match, match2;
    Tk_Image image;

    IMAGE_FOR_STATE(image, image, state)

    if (image != NULL)
	Tk_SizeOfImage(image, &width, &height);

    if (elemX->widthObj != NULL)
	width = elemX->width;
    else if ((masterX != NULL) && (masterX->widthObj != NULL))
	width = masterX->width;

    if (elemX->heightObj != NULL)
	height = elemX->height;
    else if ((masterX != NULL) && (masterX->heightObj != NULL))
	height = masterX->height;

    args->needed.width = width;
    args->needed.height = height;
}

static int StateProcImage(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementImage *elemX = (ElementImage *) elem;
    ElementImage *masterX = (ElementImage *) elem->master;
    int match, match2;
    int draw1, draw2;
    Tk_Image image1, image2;
    int mask = 0;

    BOOLEAN_FOR_STATE(draw1, draw, args->states.state1)
    if (draw1 == -1)
	draw1 = 1;
    IMAGE_FOR_STATE(image1, image, args->states.state1)

    BOOLEAN_FOR_STATE(draw2, draw, args->states.state2)
    if (draw2 == -1)
	draw2 = 1;
    IMAGE_FOR_STATE(image2, image, args->states.state2)

    if (image1 != image2) {
	mask |= CS_DISPLAY;
	if ((image1 != NULL) && (image2 != NULL)) {
	    int w1, h1, w2, h2;
	    Tk_SizeOfImage(image1, &w1, &h1);
	    Tk_SizeOfImage(image2, &w2, &h2);
	    if ((w1 != w2) || (h1 != h2))
		mask |= CS_LAYOUT;
	} else
	    mask |= CS_LAYOUT;
    } else if (draw1 != draw2)
	mask |= CS_DISPLAY;

    return mask;
}

static int UndefProcImage(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementImage *elemX = (ElementImage *) args->elem;
    int modified = 0;

    modified |= PerStateInfo_Undefine(tree, &pstBoolean, &elemX->draw, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstImage, &elemX->image, args->state);
    return modified;
}

static int ActualProcImage(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementImage *elemX = (ElementImage *) args->elem;
    ElementImage *masterX = (ElementImage *) args->elem->master;
    static CONST char *optionName[] = {
	"-draw", "-image",
	(char *) NULL };
    int index, match, matchM;
    Tcl_Obj *obj = NULL;

    if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	case 0:
	{
	    OBJECT_FOR_STATE(obj, pstBoolean, draw, args->state)
	    break;
	}
	case 1:
	{
	    OBJECT_FOR_STATE(obj, pstImage, image, args->state)
	    break;
	}
    }
    if (obj != NULL)
	Tcl_SetObjResult(tree->interp, obj);
    return TCL_OK;
}

ElementType elemTypeImage = {
    "image",
    sizeof(ElementImage),
    imageOptionSpecs,
    NULL,
    CreateProcImage,
    DeleteProcImage,
    ConfigProcImage,
    DisplayProcImage,
    NeededProcImage,
    NULL, /* heightProc */
    WorldChangedProcImage,
    StateProcImage,
    UndefProcImage,
    ActualProcImage,
    NULL /* onScreenProc */
};

/*****/

typedef struct ElementRect ElementRect;

struct ElementRect
{
    Element header;
    PerStateInfo draw;
    int width;
    Tcl_Obj *widthObj;
    int height;
    Tcl_Obj *heightObj;
    PerStateInfo fill;
    PerStateInfo outline;
    int outlineWidth;
    Tcl_Obj *outlineWidthObj;
    int open;
    char *openString;
    int showFocus;
};

#define RECT_CONF_FILL 0x0001
#define RECT_CONF_OUTLINE 0x0002
#define RECT_CONF_OUTWIDTH 0x0004
#define RECT_CONF_OPEN 0x0008
#define RECT_CONF_SIZE 0x0010
#define RECT_CONF_FOCUS 0x0020
#define RECT_CONF_DRAW 0x0040

static Tk_OptionSpec rectOptionSpecs[] = {
    {TK_OPTION_STRING, "-draw", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementRect, draw.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_DRAW},
    {TK_OPTION_STRING, "-fill", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementRect, fill.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_FILL},
    {TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementRect, heightObj),
     Tk_Offset(ElementRect, height),
     TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_SIZE},
    {TK_OPTION_STRING, "-open", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementRect, openString),
     TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_OPEN},
    {TK_OPTION_STRING, "-outline", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementRect, outline.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_OUTLINE},
    {TK_OPTION_PIXELS, "-outlinewidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementRect, outlineWidthObj),
     Tk_Offset(ElementRect, outlineWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_OUTWIDTH},
    {TK_OPTION_CUSTOM, "-showfocus", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementRect, showFocus),
     TK_OPTION_NULL_OK, (ClientData) &booleanCO, RECT_CONF_FOCUS},
    {TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementRect, widthObj),
     Tk_Offset(ElementRect, width),
     TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_SIZE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteProcRect(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementRect *elemX = (ElementRect *) args->elem;

    PerStateInfo_Free(tree, &pstBoolean, &elemX->draw);
    PerStateInfo_Free(tree, &pstColor, &elemX->fill);
    PerStateInfo_Free(tree, &pstColor, &elemX->outline);
}

static int WorldChangedProcRect(ElementArgs *args)
{
    int flagM = args->change.flagMaster;
    int flagS = args->change.flagSelf;
    int mask = 0;

    if ((flagS | flagM) & (RECT_CONF_SIZE | RECT_CONF_OUTWIDTH))
	mask |= CS_DISPLAY | CS_LAYOUT;

    if ((flagS | flagM) & (RECT_CONF_DRAW | RECT_CONF_FILL | RECT_CONF_OUTLINE |
		RECT_CONF_OPEN | RECT_CONF_FOCUS))
	mask |= CS_DISPLAY;

    return mask;
}

static int ConfigProcRect(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementRect *elemX = (ElementRect *) elem;
    ElementRect savedX;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int i;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) elemX,
			elem->typePtr->optionTable,
			args->config.objc, args->config.objv, tree->tkwin,
			&savedOptions, &args->config.flagSelf) != TCL_OK) {
		args->config.flagSelf = 0;
		continue;
	    }

	    if (args->config.flagSelf & RECT_CONF_DRAW)
		PSTSave(&elemX->draw, &savedX.draw);
	    if (args->config.flagSelf & RECT_CONF_FILL)
		PSTSave(&elemX->fill, &savedX.fill);
	    if (args->config.flagSelf & RECT_CONF_OUTLINE)
		PSTSave(&elemX->outline, &savedX.outline);
	    if (args->config.flagSelf & RECT_CONF_OPEN)
		savedX.open = elemX->open;

	    if (args->config.flagSelf & RECT_CONF_DRAW) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBoolean, &elemX->draw) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & RECT_CONF_FILL) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstColor, &elemX->fill) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & RECT_CONF_OUTLINE) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstColor, &elemX->outline) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & RECT_CONF_OPEN) {
		elemX->open = 0;
		if (elemX->openString != NULL) {
		    int badChar = 0;

		    for (i = 0; elemX->openString[i]; i++) {
			switch (elemX->openString[i]) {
			    case 'w': case 'W': elemX->open |= 0x01; break;
			    case 'n': case 'N': elemX->open |= 0x02; break;
			    case 'e': case 'E': elemX->open |= 0x04; break;
			    case 's': case 'S': elemX->open |= 0x08; break;
			    default:
			    {
				Tcl_ResetResult(tree->interp);
				Tcl_AppendResult(tree->interp, "bad open value \"",
					elemX->openString, "\": must be a string ",
					"containing zero or more of n, e, s, and w",
					(char *) NULL);
				badChar = 1;
				break;
			    }
			}
			if (badChar)
			    break;
		    }
		    if (badChar)
			continue;
		}
	    }

	    if (args->config.flagSelf & RECT_CONF_DRAW)
		PerStateInfo_Free(tree, &pstBoolean, &savedX.draw);
	    if (args->config.flagSelf & RECT_CONF_FILL)
		PerStateInfo_Free(tree, &pstColor, &savedX.fill);
	    if (args->config.flagSelf & RECT_CONF_OUTLINE)
		PerStateInfo_Free(tree, &pstColor, &savedX.outline);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (args->config.flagSelf & RECT_CONF_DRAW)
		PSTRestore(tree, &pstBoolean, &elemX->draw, &savedX.draw);

	    if (args->config.flagSelf & RECT_CONF_FILL)
		PSTRestore(tree, &pstColor, &elemX->fill, &savedX.fill);

	    if (args->config.flagSelf & RECT_CONF_OUTLINE)
		PSTRestore(tree, &pstColor, &elemX->outline, &savedX.outline);

	    if (args->config.flagSelf & RECT_CONF_OPEN)
		elemX->open = savedX.open;

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

static int CreateProcRect(ElementArgs *args)
{
    ElementRect *elemX = (ElementRect *) args->elem;

    elemX->showFocus = -1;
    return TCL_OK;
}

static void DisplayProcRect(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementRect *elemX = (ElementRect *) elem;
    ElementRect *masterX = (ElementRect *) elem->master;
    int state = args->state;
    int x = args->display.x, y = args->display.y;
    int width = args->display.width, height = args->display.height;
    int match, match2;
    int draw;
    XColor *color;
    int open = 0;
    int outlineWidth = 0;
    int showFocus = 0;

    BOOLEAN_FOR_STATE(draw, draw, state)
    if (!draw)
	return;

    if (elemX->outlineWidthObj != NULL)
	outlineWidth = elemX->outlineWidth;
    else if ((masterX != NULL) && (masterX->outlineWidthObj != NULL))
	outlineWidth = masterX->outlineWidth;

    if (elemX->openString != NULL)
	open = elemX->open;
    else if ((masterX != NULL) && (masterX->openString != NULL))
	open = masterX->open;

    if (elemX->showFocus != -1)
	showFocus = elemX->showFocus;
    else if ((masterX != NULL) && (masterX->showFocus != -1))
	showFocus = masterX->showFocus;

    if (elemX->widthObj != NULL)
	width = elemX->width;
    else if ((masterX != NULL) && (masterX->widthObj != NULL))
	width = masterX->width;

    if (elemX->heightObj != NULL)
	height = elemX->height;
    else if ((masterX != NULL) && (masterX->heightObj != NULL))
	height = masterX->height;

    AdjustForSticky(args->display.sticky,
	args->display.width, args->display.height,
	TRUE, TRUE,
	&x, &y, &width, &height);

    COLOR_FOR_STATE(color, fill, state)
    if (color != NULL) {
	GC gc = Tk_GCForColor(color, Tk_WindowId(tree->tkwin));
	XFillRectangle(tree->display, args->display.drawable, gc,
		x, y, width, height);
    }

    COLOR_FOR_STATE(color, outline, state)
    if ((color != NULL) && (outlineWidth > 0)) {
	GC gc = Tk_GCForColor(color, Tk_WindowId(tree->tkwin));
	if (!(open & 0x01))
	    XFillRectangle(tree->display, args->display.drawable, gc,
		    x, y, outlineWidth, height);
	if (!(open & 0x02))
	    XFillRectangle(tree->display, args->display.drawable, gc,
		    x, y, width, outlineWidth);
	if (!(open & 0x04))
	    XFillRectangle(tree->display, args->display.drawable, gc,
		    x + width - outlineWidth, y, outlineWidth, height);
	if (!(open & 0x08))
	    XFillRectangle(tree->display, args->display.drawable, gc,
		    x, y + height - outlineWidth, width, outlineWidth);
    }

    if (showFocus && (state & STATE_FOCUS) && (state & STATE_ACTIVE)) {
	DrawActiveOutline(tree, args->display.drawable,
		args->display.x, args->display.y,
		args->display.width, args->display.height,
		open);
    }
}

static void NeededProcRect(ElementArgs *args)
{
    Element *elem = args->elem;
    ElementRect *elemX = (ElementRect *) elem;
    ElementRect *masterX = (ElementRect *) elem->master;
    int width = 0, height = 0;
    int outlineWidth = 0;

    if (elemX->outlineWidthObj != NULL)
	outlineWidth = elemX->outlineWidth;
    else if ((masterX != NULL) && (masterX->outlineWidthObj != NULL))
	outlineWidth = masterX->outlineWidth;

    if (elemX->widthObj != NULL)
	width = elemX->width;
    else if ((masterX != NULL) && (masterX->widthObj != NULL))
	width = masterX->width;

    if (elemX->heightObj != NULL)
	height = elemX->height;
    else if ((masterX != NULL) && (masterX->heightObj != NULL))
	height = masterX->height;

    args->needed.width = MAX(width, outlineWidth * 2);
    args->needed.height = MAX(height, outlineWidth * 2);
}

static int StateProcRect(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementRect *elemX = (ElementRect *) elem;
    ElementRect *masterX = (ElementRect *) elem->master;
    int match, match2;
    int draw1, draw2;
    XColor *f1, *f2;
    XColor *o1, *o2;
    int s1, s2;
    int showFocus = 0;
    int mask = 0;

    if (elemX->showFocus != -1)
	showFocus = elemX->showFocus;
    else if ((masterX != NULL) && (masterX->showFocus != -1))
	showFocus = masterX->showFocus;

    BOOLEAN_FOR_STATE(draw1, draw, args->states.state1)
    if (draw1 == -1)
	draw1 = 1;
    COLOR_FOR_STATE(f1, fill, args->states.state1)
    COLOR_FOR_STATE(o1, outline, args->states.state1)

    s1 = showFocus &&
	(args->states.state1 & STATE_FOCUS) &&
	(args->states.state1 & STATE_ACTIVE);

    BOOLEAN_FOR_STATE(draw2, draw, args->states.state2)
    if (draw2 == -1)
	draw2 = 1;
    COLOR_FOR_STATE(f2, fill, args->states.state2)
    COLOR_FOR_STATE(o2, outline, args->states.state2)

    s2 = showFocus &&
	(args->states.state2 & STATE_FOCUS) &&
	(args->states.state2 & STATE_ACTIVE);

    if ((draw1 != draw2) || (f1 != f2) || (o1 != o2) || (s1 != s2))
	mask |= CS_DISPLAY;

    return mask;
}

static int UndefProcRect(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementRect *elemX = (ElementRect *) args->elem;
    int modified = 0;

    modified |= PerStateInfo_Undefine(tree, &pstBoolean, &elemX->draw, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstColor, &elemX->fill, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstColor, &elemX->outline, args->state);
    return modified;
}

static int ActualProcRect(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementRect *elemX = (ElementRect *) args->elem;
    ElementRect *masterX = (ElementRect *) args->elem->master;
    static CONST char *optionName[] = {
	"-draw", "-fill", "-outline",
	(char *) NULL };
    int index, match, matchM;
    Tcl_Obj *obj = NULL;

    if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	case 0:
	{
	    OBJECT_FOR_STATE(obj, pstBoolean, draw, args->state)
	    break;
	}
	case 1:
	{
	    OBJECT_FOR_STATE(obj, pstColor, fill, args->state)
	    break;
	}
	case 2:
	{
	    OBJECT_FOR_STATE(obj, pstColor, outline, args->state)
	    break;
	}
    }
    if (obj != NULL)
	Tcl_SetObjResult(tree->interp, obj);
    return TCL_OK;
}

ElementType elemTypeRect = {
    "rect",
    sizeof(ElementRect),
    rectOptionSpecs,
    NULL,
    CreateProcRect,
    DeleteProcRect,
    ConfigProcRect,
    DisplayProcRect,
    NeededProcRect,
    NULL, /* heightProc */
    WorldChangedProcRect,
    StateProcRect,
    UndefProcRect,
    ActualProcRect,
    NULL /* onScreenProc */
};

/*****/

typedef struct ElementText ElementText;

struct ElementText
{
    Element header;
    PerStateInfo draw;			/* -draw */
    Tcl_Obj *textObj;			/* -text */
    char *text;
    int textLen;
    Tcl_Obj *dataObj;
#define TDT_NULL -1
#define TDT_DOUBLE 0
#define TDT_INTEGER 1
#define TDT_LONG 2
#define TDT_STRING 3
#define TDT_TIME 4
#define TEXT_CONF_DATA 0x0001000	/* for Tk_SetOptions() */
    int dataType;			/* -datatype */
    Tcl_Obj *formatObj;			/* -format */
    int stringRepInvalid;
    PerStateInfo font;			/* -font */
    PerStateInfo fill;			/* -fill */
    struct PerStateGC *gc;
#define TK_JUSTIFY_NULL -1
    int justify;			/* -justify */
    int lines;				/* -lines */
    Tcl_Obj *widthObj;			/* -width */
    int width;				/* -width */
#define TEXT_WRAP_NULL -1
#define TEXT_WRAP_CHAR 0
#define TEXT_WRAP_NONE 1
#define TEXT_WRAP_WORD 2
    int wrap;				/* -wrap */
    TextLayout layout;
    int layoutWidth;
    int neededWidth;
    int totalWidth;
#define TEXTVAR
#ifdef TEXTVAR
    Tcl_Obj *varNameObj;		/* -textvariable */
    TreeCtrl *tree;			/* needed to redisplay */
    TreeItem item;			/* needed to redisplay */
    TreeItemColumn column;		/* needed to redisplay */
#endif
};

/* for Tk_SetOptions() */
#define TEXT_CONF_FONT 0x0001
#define TEXT_CONF_FILL 0x0002
#define TEXT_CONF_TEXTOBJ 0x0004
#define TEXT_CONF_LAYOUT 0x0008
#ifdef TEXTVAR
#define TEXT_CONF_TEXTVAR 0x0010
#endif
#define TEXT_CONF_DRAW 0x0020

static CONST char *textDataTypeST[] = { "double", "integer", "long", "string",
					"time", (char *) NULL };
static CONST char *textJustifyST[] = { "left", "right", "center", (char *) NULL };
static CONST char *textWrapST[] = { "char", "none", "word", (char *) NULL };

static Tk_OptionSpec textOptionSpecs[] = {
    {TK_OPTION_STRING, "-data", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementText, dataObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_DATA},
    {TK_OPTION_CUSTOM, "-datatype", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementText, dataType),
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_DATA},
    {TK_OPTION_STRING, "-draw", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementText, draw.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_DRAW},
    {TK_OPTION_STRING, "-format", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementText, formatObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_DATA},
    {TK_OPTION_STRING, "-fill", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementText, fill.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL,  TEXT_CONF_FILL},
    {TK_OPTION_STRING, "-font", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementText, font.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_FONT},
    {TK_OPTION_CUSTOM, "-justify", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementText, justify),
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_LAYOUT},
    {TK_OPTION_CUSTOM, "-lines", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementText, lines),
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_LAYOUT},
    {TK_OPTION_STRING, "-text", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementText, textObj),
     Tk_Offset(ElementText, text),
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_TEXTOBJ},
#ifdef TEXTVAR
    {TK_OPTION_STRING, "-textvariable", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementText, varNameObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_TEXTVAR},
#endif
    {TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementText, widthObj),
     Tk_Offset(ElementText, width),
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_LAYOUT},
    {TK_OPTION_CUSTOM, "-wrap", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementText, wrap),
     TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_LAYOUT},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

static int WorldChangedProcText(ElementArgs *args)
{
/*	TreeCtrl *tree = args->tree;*/
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;
/*	ElementText *masterX = (ElementText *) elem->master;*/
    int flagT = args->change.flagTree;
    int flagM = args->change.flagMaster;
    int flagS = args->change.flagSelf;
    int mask = 0;

#ifdef TEXTVAR
    if ((flagS | flagM) & (TEXT_CONF_DATA | TEXT_CONF_TEXTOBJ | TEXT_CONF_TEXTVAR)) {
#else
    if ((flagS | flagM) & (TEXT_CONF_DATA | TEXT_CONF_TEXTOBJ)) {
#endif
	elemX->stringRepInvalid = TRUE;
	mask |= CS_DISPLAY | CS_LAYOUT;
    }

    if (elemX->stringRepInvalid ||
	    ((flagS | flagM) & (TEXT_CONF_FONT | TEXT_CONF_LAYOUT)) ||
	    /* Not always needed */
	    (flagT & TREE_CONF_FONT)) {
	mask |= CS_DISPLAY | CS_LAYOUT;
    }

    if ((flagS | flagM) & (TEXT_CONF_DRAW | TEXT_CONF_FILL))
	mask |= CS_DISPLAY;

    return mask;
}

static void TextUpdateStringRep(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;
    ElementText *masterX = (ElementText *) elem->master;
    Tcl_Obj *dataObj, *formatObj, *textObj;
#ifdef TEXTVAR
    Tcl_Obj *varNameObj = elemX->varNameObj;
#endif
    int dataType;

    dataObj = elemX->dataObj;
    if ((dataObj == NULL) && (masterX != NULL))
	dataObj = masterX->dataObj; 

    dataType = elemX->dataType;
    if ((dataType == TDT_NULL) && (masterX != NULL))
	dataType = masterX->dataType; 

    formatObj = elemX->formatObj;
    if ((formatObj == NULL) && (masterX != NULL))
	formatObj = masterX->formatObj; 

    textObj = elemX->textObj;
    if ((textObj == NULL) && (masterX != NULL))
	textObj = masterX->textObj; 

    if ((elemX->textObj == NULL) && (elemX->text != NULL)) {
	ckfree(elemX->text);
	elemX->text = NULL;
    }

    /* If -text is specified, -data, -datatype and -format are ignored */
    if (textObj != NULL) {
	if (elemX->textObj != NULL)
	    (void) Tcl_GetStringFromObj(elemX->textObj, &elemX->textLen);
    }

#ifdef TEXTVAR
    else if (varNameObj != NULL) {
	Tcl_Obj *valueObj = Tcl_ObjGetVar2(tree->interp, varNameObj, NULL, TCL_GLOBAL_ONLY);
	
	if (valueObj == NULL) {
	    /* not possible I think */
	} else {
	    char *string = Tcl_GetStringFromObj(valueObj, &elemX->textLen);
	    elemX->text = ckalloc(elemX->textLen);
	    memcpy(elemX->text, string, elemX->textLen);
	}
    }
#endif
    /* Only create a string rep if elemX (not masterX) has dataObj,
       dataType or formatObj. */
    else if ((dataObj != NULL) && (dataType != TDT_NULL) &&
	    ((elemX->dataObj != NULL) ||
		    (elemX->dataType != TDT_NULL) ||
		    (elemX->formatObj != NULL))) {
	int i, objc = 0;
	Tcl_Obj *objv[5], *resultObj = NULL;
	static Tcl_Obj *staticObj[3] = { NULL };
	static Tcl_Obj *staticFormat[4] = { NULL };
	Tcl_ObjCmdProc *clockObjCmd = NULL, *formatObjCmd = NULL;
	ClientData clockClientData = NULL, formatClientData = NULL;
	Tcl_CmdInfo cmdInfo;

	if (staticFormat[0] == NULL) {
	    staticFormat[0] = Tcl_NewStringObj("%g", -1);
	    staticFormat[1] = Tcl_NewStringObj("%d", -1);
	    staticFormat[2] = Tcl_NewStringObj("%ld", -1);
	    staticFormat[3] = Tcl_NewStringObj("%s", -1);
	    for (i = 0; i < 4; i++)
		Tcl_IncrRefCount(staticFormat[i]);
	}
	if (staticObj[0] == NULL) {
	    staticObj[0] = Tcl_NewStringObj("clock", -1);
	    staticObj[1] = Tcl_NewStringObj("format", -1);
	    staticObj[2] = Tcl_NewStringObj("-format", -1);
	    for (i = 0; i < 3; i++)
		Tcl_IncrRefCount(staticObj[i]);
	}
	if (Tcl_GetCommandInfo(tree->interp, "::clock", &cmdInfo) == 1) {
	    clockObjCmd = cmdInfo.objProc;
	    clockClientData = cmdInfo.objClientData;
	}
	if (Tcl_GetCommandInfo(tree->interp, "::format", &cmdInfo) == 1) {
	    formatObjCmd = cmdInfo.objProc;
	    formatClientData = cmdInfo.objClientData;
	}

	/* Important to remove any shared result object, otherwise
	 * calls like Tcl_SetStringObj(Tcl_GetObjResult()) fail. */
	Tcl_ResetResult(tree->interp);

	switch (dataType) {
	    case TDT_DOUBLE:
		if (formatObjCmd == NULL)
		    break;
		if (formatObj == NULL) formatObj = staticFormat[0];
		objv[objc++] = staticObj[1]; /* format */
		objv[objc++] = formatObj;
		objv[objc++] = dataObj;
		if (formatObjCmd(formatClientData, tree->interp, objc, objv)
			    == TCL_OK)
		    resultObj = Tcl_GetObjResult(tree->interp);
		break;
	    case TDT_INTEGER:
		if (formatObjCmd == NULL)
		    break;
		if (formatObj == NULL) formatObj = staticFormat[1];
		objv[objc++] = staticObj[1]; /* format */
		objv[objc++] = formatObj;
		objv[objc++] = dataObj;
		if (formatObjCmd(formatClientData, tree->interp, objc, objv)
			    == TCL_OK)
		    resultObj = Tcl_GetObjResult(tree->interp);
		break;
	    case TDT_LONG:
		if (formatObjCmd == NULL)
		    break;
		if (formatObj == NULL) formatObj = staticFormat[2];
		objv[objc++] = staticObj[1]; /* format */
		objv[objc++] = formatObj;
		objv[objc++] = dataObj;
		if (formatObjCmd(formatClientData, tree->interp, objc, objv)
			    == TCL_OK)
		    resultObj = Tcl_GetObjResult(tree->interp);
		break;
	    case TDT_STRING:
		if (formatObjCmd == NULL)
		    break;
		if (formatObj == NULL) formatObj = staticFormat[3];
		objv[objc++] = staticObj[1]; /* format */
		objv[objc++] = formatObj;
		objv[objc++] = dataObj;
		if (formatObjCmd(formatClientData, tree->interp, objc, objv)
			    == TCL_OK)
		    resultObj = Tcl_GetObjResult(tree->interp);
		break;
	    case TDT_TIME:
		if (clockObjCmd == NULL)
		    break;
		objv[objc++] = staticObj[0];
		objv[objc++] = staticObj[1];
		objv[objc++] = dataObj;
		if (formatObj != NULL) {
		    objv[objc++] = staticObj[2];
		    objv[objc++] = formatObj;
		}
		if (clockObjCmd(clockClientData, tree->interp, objc, objv)
			    == TCL_OK)
		    resultObj = Tcl_GetObjResult(tree->interp);
		break;
	    default:
		panic("unknown ElementText dataType");
		break;
	}

	if (resultObj != NULL) {
	    char *string = Tcl_GetStringFromObj(resultObj, &elemX->textLen);
	    elemX->text = ckalloc(elemX->textLen);
	    memcpy(elemX->text, string, elemX->textLen);
	}
    }
}

static void TextUpdateLayout(char *func, ElementArgs *args, int fixedWidth, int maxWidth)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;
    ElementText *masterX = (ElementText *) elem->master;
    int state = args->state;
    int match, match2;
    Tk_Font tkfont;
    char *text = NULL;
    int textLen = 0;
    int justify = TK_JUSTIFY_LEFT;
    int lines = 0;
    int wrap = TEXT_WRAP_WORD;
    int width = 0;
    int flags = 0;
    int i, multiLine = FALSE;
    int textWidth;

    if (tree->debug.enable && tree->debug.textLayout)
	dbwin("TextUpdateLayout: %s %p (%s) %s\n",
	    Tk_PathName(tree->tkwin), elemX, masterX ? "instance" : "master", func);

    if (elemX->layout != NULL) {
	if (tree->debug.enable && tree->debug.textLayout)
	    dbwin("    FREE\n");
	TextLayout_Free(elemX->layout);
	elemX->layout = NULL;
    }

    if (elemX->text != NULL) {
	text = elemX->text;
	textLen = elemX->textLen;
    } else if ((masterX != NULL) && (masterX->text != NULL)) {
	text = masterX->text;
	textLen = masterX->textLen;
    }
    if ((text == NULL) || (textLen == 0))
	return;

    if (elemX->lines != -1)
	lines = elemX->lines;
    else if ((masterX != NULL) && (masterX->lines != -1))
	lines = masterX->lines;
    if (lines == 1)
	return;

    FONT_FOR_STATE(tkfont, font, state)
    if (tkfont == NULL)
	tkfont = tree->tkfont;

    if (elemX->wrap != TEXT_WRAP_NULL)
	wrap = elemX->wrap;
    else if ((masterX != NULL) && (masterX->wrap != TEXT_WRAP_NULL))
	wrap = masterX->wrap;

    if (wrap != TEXT_WRAP_NONE) {
	if (fixedWidth >= 0)
	    width = fixedWidth;
	else if (maxWidth >= 0)
	    width = maxWidth;
	if (elemX->widthObj != NULL) {
	    if (!width || (elemX->width < width))
		width = elemX->width;
	} else if ((masterX != NULL) && (masterX->widthObj != NULL)) {
	    if (!width || (masterX->width < width))
		width = masterX->width;
	}
    }

    for (i = 0; i < textLen; i++) {
	if ((text[i] == '\n') || (text[i] == '\r')) {
	    multiLine = TRUE;
	    break;
	}
    }
    if (tree->debug.enable && tree->debug.textLayout)
	dbwin("    lines %d multiLine %d width %d wrap %s\n",
		lines, multiLine, width, textWrapST[wrap]);
    if (!multiLine) {
	if (width == 0)
	    return;
	textWidth = Tk_TextWidth(tkfont, text, textLen);
if (tree->debug.enable && tree->debug.textLayout) dbwin("    textWidth %d\n", textWidth);
	if (width >= textWidth)
	    return;
    }

    if (elemX->justify != TK_JUSTIFY_NULL)
	justify = elemX->justify;
    else if ((masterX != NULL) && (masterX->justify != TK_JUSTIFY_NULL))
	justify = masterX->justify;

    if (wrap == TEXT_WRAP_WORD)
	flags |= TK_WHOLE_WORDS;

    elemX->layout = TextLayout_Compute(tkfont, text,
	    Tcl_NumUtfChars(text, textLen), width, justify, lines, flags);

    if (tree->debug.enable && tree->debug.textLayout)
	dbwin("    ALLOC\n");
}

#ifdef TEXTVAR
static Tcl_VarTraceProc VarTraceProc_Text;

static void TextTraceSet(Tcl_Interp *interp, ElementText *elemX)
{
    if (elemX->varNameObj != NULL) {
	Tcl_TraceVar2(interp, Tcl_GetString(elemX->varNameObj),
	    NULL,
	    TCL_GLOBAL_ONLY | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	    VarTraceProc_Text, (ClientData) elemX);
    }
}

static void TextTraceUnset(Tcl_Interp *interp, ElementText *elemX)
{
    if (elemX->varNameObj != NULL) {
	Tcl_UntraceVar2(interp, Tcl_GetString(elemX->varNameObj),
	    NULL,
	    TCL_GLOBAL_ONLY | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	    VarTraceProc_Text, (ClientData) elemX);
    }
}

static char *VarTraceProc_Text(ClientData clientData, Tcl_Interp *interp,
    CONST char *name1, CONST char *name2, int flags)
{
    ElementText *elemX = (ElementText *) clientData;
    ElementText *masterX = (ElementText *) elemX->header.master;
    Tcl_Obj *varNameObj = elemX->varNameObj;
    Tcl_Obj *valueObj;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    char *text = elemX->text;
	    int textLen = elemX->textLen;
	    if ((text == NULL) && (masterX != NULL)) {
		text = masterX->text;
		textLen = masterX->textLen;
	    }
	    if (text != NULL) {
		valueObj = Tcl_NewStringObj(text, textLen);
	    } else {
		valueObj = Tcl_NewStringObj("", 0);
	    }
	    Tcl_IncrRefCount(valueObj);
	    Tcl_ObjSetVar2(interp, varNameObj, NULL, valueObj,
		TCL_GLOBAL_ONLY);
	    Tcl_DecrRefCount(valueObj);
	    TextTraceSet(interp, elemX);
	}
	return (char *) NULL;
    }

    elemX->stringRepInvalid = TRUE;
    Tree_ElementChangedItself(elemX->tree, elemX->item, elemX->column,
	(Element *) elemX, CS_LAYOUT | CS_DISPLAY);

    return (char *) NULL;
}
#endif /* TEXTVAR */

static void DeleteProcText(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;

    PerStateInfo_Free(tree, &pstBoolean, &elemX->draw);
    if (elemX->gc != NULL)
	PerStateGC_Free(tree, &elemX->gc);
    PerStateInfo_Free(tree, &pstColor, &elemX->fill);
    PerStateInfo_Free(tree, &pstFont, &elemX->font);
    if ((elemX->textObj == NULL) && (elemX->text != NULL)) {
	ckfree(elemX->text);
	elemX->text = NULL;
    }
    if (elemX->layout != NULL)
	TextLayout_Free(elemX->layout);
#ifdef TEXTVAR
    TextTraceUnset(tree->interp, elemX);
#endif
}

static int ConfigProcText(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Tcl_Interp *interp = tree->interp;
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;
    ElementText savedX;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;

#ifdef TEXTVAR
    TextTraceUnset(interp, elemX);
#endif

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(interp, (char *) elemX,
			elem->typePtr->optionTable,
			args->config.objc, args->config.objv, tree->tkwin,
			&savedOptions, &args->config.flagSelf) != TCL_OK) {
		args->config.flagSelf = 0;
		continue;
	    }

	    if (args->config.flagSelf & TEXT_CONF_DRAW)
		PSTSave(&elemX->draw, &savedX.draw);
	    if (args->config.flagSelf & TEXT_CONF_FILL)
		PSTSave(&elemX->fill, &savedX.fill);
	    if (args->config.flagSelf & TEXT_CONF_FONT)
		PSTSave(&elemX->font, &savedX.font);

	    if (args->config.flagSelf & TEXT_CONF_DRAW) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBoolean, &elemX->draw) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & TEXT_CONF_FILL) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstColor, &elemX->fill) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & TEXT_CONF_FONT) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstFont, &elemX->font) != TCL_OK)
		    continue;
	    }

#ifdef TEXTVAR
	    if (elemX->varNameObj != NULL) {
		Tcl_Obj *valueObj;
		valueObj = Tcl_ObjGetVar2(interp, elemX->varNameObj,
			NULL, TCL_GLOBAL_ONLY);
		if (valueObj == NULL) {
		    ElementText *masterX = (ElementText *) elem->master;
		    char *text = elemX->text;
		    int textLen = elemX->textLen;
		    if ((text == NULL) && (masterX != NULL)) {
			text = masterX->text;
			textLen = masterX->textLen;
		    }
		    /* This may not be the current string rep */
		    if (text != NULL) {
			valueObj = Tcl_NewStringObj(text, textLen);
		    } else {
			valueObj = Tcl_NewStringObj("", 0);
		    }
		    Tcl_IncrRefCount(valueObj);
		    /* This validates the variable name. We get an error
		     * if it is the name of an array */
		    if (Tcl_ObjSetVar2(interp, elemX->varNameObj, NULL,
			    valueObj, TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG)
			    == NULL) {
			Tcl_DecrRefCount(valueObj);
			continue;
		    }
		    Tcl_DecrRefCount(valueObj);
		}
	    }
#endif

	    if (args->config.flagSelf & TEXT_CONF_DRAW)
		PerStateInfo_Free(tree, &pstBoolean, &savedX.draw);
	    if (args->config.flagSelf & TEXT_CONF_FILL)
		PerStateInfo_Free(tree, &pstColor, &savedX.fill);
	    if (args->config.flagSelf & TEXT_CONF_FONT)
		PerStateInfo_Free(tree, &pstFont, &savedX.font);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (args->config.flagSelf & TEXT_CONF_DRAW)
		PSTRestore(tree, &pstBoolean, &elemX->draw, &savedX.draw);

	    if (args->config.flagSelf & TEXT_CONF_FILL)
		PSTRestore(tree, &pstColor, &elemX->fill, &savedX.fill);

	    if (args->config.flagSelf & TEXT_CONF_FONT)
		PSTRestore(tree, &pstFont, &elemX->font, &savedX.font);
	}
    }

#ifdef TEXTVAR
    TextTraceSet(interp, elemX);
#endif

    if (error) {
	Tcl_SetObjResult(interp, errorResult);
	Tcl_DecrRefCount(errorResult);
	return TCL_ERROR;
    }

    return TCL_OK;
}

static int CreateProcText(ElementArgs *args)
{
    ElementText *elemX = (ElementText *) args->elem;

    elemX->dataType = TDT_NULL;
    elemX->justify = TK_JUSTIFY_NULL;
    elemX->lines = -1;
    elemX->wrap = TEXT_WRAP_NULL;
#ifdef TEXTVAR
    elemX->tree = args->tree;
    elemX->item = args->create.item;
    elemX->column = args->create.column;
#endif
    return TCL_OK;
}

static void TextRedoLayoutIfNeeded(char *func, ElementArgs *args, int fixedWidth)
{
    ElementText *elemX = (ElementText *) args->elem;
    ElementText *masterX = (ElementText *) args->elem->master;
    int doLayout = 0;
    int wrap = TEXT_WRAP_WORD;

    /* If text wrapping is disabled, the layout doesn't change */
    if (elemX->wrap != TEXT_WRAP_NULL)
	wrap = elemX->wrap;
    else if ((masterX != NULL) && (masterX->wrap != TEXT_WRAP_NULL))
	wrap = masterX->wrap;
    if (wrap == TEXT_WRAP_NONE)
	return;

    if (elemX->layout != NULL) {
	/* See comment in NeededProc about totalWidth */
	if ((elemX->neededWidth != -1) && (fixedWidth >= elemX->neededWidth))
	    fixedWidth = elemX->totalWidth;

	/* Already layed out at this width */
	if (fixedWidth == elemX->layoutWidth)
	    return;
    }

    /* May switch from layout -> no layout or vice versa */
    if (elemX->layout == NULL)
	doLayout = 1;

    /* Width was constrained and we have more space now */
    else if ((elemX->layoutWidth != -1) && (fixedWidth > elemX->layoutWidth))
	doLayout = 1;

    /* Width was unconstrained or we have less space now */
    else {
	int width;
	TextLayout_Size(elemX->layout, &width, NULL);
	/* Redo if we are narrower than the layout */
	if (fixedWidth < width)
	    doLayout = 1;
    }
    if (doLayout)
	TextUpdateLayout(func, args, fixedWidth, -1);
    if (elemX->layout != NULL)
	elemX->layoutWidth = fixedWidth;
    else
	elemX->layoutWidth = -1;
}

static void DisplayProcText(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;
    ElementText *masterX = (ElementText *) elem->master;
    int state = args->state;
    int x = args->display.x, y = args->display.y;
    int width, height;
    int match, match2;
    int draw;
    XColor *color;
    char *text = elemX->text;
    int textLen = elemX->textLen;
    Tk_Font tkfont;
    TextLayout layout = NULL;
    Tk_FontMetrics fm;
    GC gc;
    int bytesThatFit, pixelsForText;
    char *ellipsis = "...";
    TkRegion clipRgn = NULL;

    BOOLEAN_FOR_STATE(draw, draw, state)
    if (!draw)
	return;

    if ((text == NULL) && (masterX != NULL)) {
	text = masterX->text;
	textLen = masterX->textLen;
    }

    if (text == NULL) /* always false (or layout sets height/width to zero) */
	return;

    COLOR_FOR_STATE(color, fill, state)

    FONT_FOR_STATE(tkfont, font, state)

    /* FIXME: -font {"" {state...}}*/
    if ((color != NULL) || (tkfont != NULL)) {
	XGCValues gcValues;
	unsigned long gcMask = 0;
	if (color == NULL)
	    color = tree->fgColorPtr;
	gcValues.foreground = color->pixel;
	gcMask |= GCForeground;
	if (tkfont == NULL)
	    tkfont = tree->tkfont;
	gcValues.font = Tk_FontId(tkfont);
	gcMask |= GCFont;
	gcValues.graphics_exposures = False;
	gcMask |= GCGraphicsExposures;
	gc = PerStateGC_Get(tree, (masterX != NULL) ? &masterX->gc :
		&elemX->gc, gcMask, &gcValues);
    } else {
	tkfont = tree->tkfont;
	gc = tree->textGC;
    }

    TextRedoLayoutIfNeeded("DisplayProcText", args, args->display.width);

    if (elemX->layout != NULL)
	layout = elemX->layout;

    if (layout != NULL) {
	TextLayout_Size(layout, &width, &height);
	/* Hack -- The actual size of the text may be slightly smaller than
	 * the available space when squeezed. If so we don't want to center
	 * the text horizontally */
	if ((elemX->neededWidth == -1) || (elemX->neededWidth > width))
	    width = args->display.width;
	AdjustForSticky(args->display.sticky,
	    args->display.width, args->display.height,
	    FALSE, FALSE,
	    &x, &y, &width, &height);
	/* Use clipping if text is taller than display height */
	if (height > args->display.height) {
	    XRectangle rect;
	    clipRgn = TkCreateRegion();
	    rect.x = x;
	    rect.y = y;
	    rect.width = args->display.width;
	    rect.height = args->display.height;
	    TkUnionRectWithRegion(&rect, clipRgn, clipRgn);
	    TkSetRegion(tree->display, gc, clipRgn);
	}
	TextLayout_Draw(tree->display, args->display.drawable, gc,
		layout, x, y, 0, -1);
	if (clipRgn != NULL) {
	    UnsetClipMask(tree, args->display.drawable, gc);
	    TkDestroyRegion(clipRgn);
	}
	return;
    }

    Tk_GetFontMetrics(tkfont, &fm);

    pixelsForText = args->display.width;
    bytesThatFit = Ellipsis(tkfont, text, textLen, &pixelsForText, ellipsis, FALSE);
    width = pixelsForText, height = fm.linespace;
    /* Hack -- The actual size of the text may be slightly smaller than
    * the available space when squeezed. If so we don't want to center
    * the text horizontally */
    if (bytesThatFit != textLen)
	width = args->display.width;
    AdjustForSticky(args->display.sticky,
	args->display.width, args->display.height,
	FALSE, FALSE,
	&x, &y, &width, &height);
    /* Use clipping if text is taller than display height */
    if (height > args->display.height) {
	XRectangle rect;
	clipRgn = TkCreateRegion();
	rect.x = x;
	rect.y = y;
	rect.width = args->display.width;
	rect.height = args->display.height;
	TkUnionRectWithRegion(&rect, clipRgn, clipRgn);
	TkSetRegion(tree->display, gc, clipRgn);
    }
    if (bytesThatFit != textLen) {
	char staticStr[256], *buf = staticStr;
	int bufLen = abs(bytesThatFit);
	int ellipsisLen = strlen(ellipsis);

	if (bufLen + ellipsisLen > sizeof(staticStr))
	    buf = ckalloc(bufLen + ellipsisLen);
	memcpy(buf, text, bufLen);
	if (bytesThatFit > 0) {
	    memcpy(buf + bufLen, ellipsis, ellipsisLen);
	    bufLen += ellipsisLen;
	}
	Tk_DrawChars(tree->display, args->display.drawable, gc,
		tkfont, buf, bufLen, x, y + fm.ascent);
	if (buf != staticStr)
	    ckfree(buf);
    } else {
	Tk_DrawChars(tree->display, args->display.drawable, gc,
		tkfont, text, textLen, x, y + fm.ascent);
    }
    if (clipRgn != NULL) {
	UnsetClipMask(tree, args->display.drawable, gc);
	TkDestroyRegion(clipRgn);
    }
}

static void NeededProcText(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;
    ElementText *masterX = (ElementText *) elem->master;
    int state = args->state;
    int match, match2;
    char *text = NULL;
    int textLen = 0;
    Tk_Font tkfont;
    Tk_FontMetrics fm;
    int width = 0, height = 0;

    if ((masterX != NULL) && masterX->stringRepInvalid) {
	args->elem = (Element *) masterX;
	TextUpdateStringRep(args);
	args->elem = elem;
	masterX->stringRepInvalid = FALSE;
    }
    if (elemX->stringRepInvalid) {
	TextUpdateStringRep(args);
	elemX->stringRepInvalid = FALSE;
    }

    TextUpdateLayout("NeededProcText", args, args->needed.fixedWidth, args->needed.maxWidth);
    elemX->layoutWidth = -1;
    elemX->neededWidth = -1;

    if (elemX->layout != NULL) {
	TextLayout_Size(elemX->layout, &width, &height);
	if (args->needed.fixedWidth >= 0)
	    elemX->layoutWidth = args->needed.fixedWidth;
	else if (args->needed.maxWidth >= 0)
	    elemX->layoutWidth = args->needed.maxWidth;
	elemX->neededWidth = width;

	/*
	 * Hack -- If we call TextLayout_Compute() with the same width
	 * returned by TextLayout_Size(), we may get a different layout.
	 * I think this has to do with whitespace at the end of lines.
	 * So if HeightProc or DisplayProc is given neededWidth, I do the
	 * layout at totalWidth, not neededWidth.
	 */
	elemX->totalWidth = TextLayout_TotalWidth(elemX->layout);
    } else {
	if (elemX->text != NULL) {
	    text = elemX->text;
	    textLen = elemX->textLen;
	} else if ((masterX != NULL) && (masterX->text != NULL)) {
	    text = masterX->text;
	    textLen = masterX->textLen;
	}
	if (text != NULL) {
	    int maxWidth = -1;

	    FONT_FOR_STATE(tkfont, font, state)
	    if (tkfont == NULL)
		tkfont = tree->tkfont;

	    width = Tk_TextWidth(tkfont, text, textLen);
	    if (elemX->widthObj != NULL)
		maxWidth = elemX->width;
	    else if ((masterX != NULL) && (masterX->widthObj != NULL))
		maxWidth = masterX->width;
	    if ((maxWidth >= 0) && (maxWidth < width))
		width = maxWidth;

	    Tk_GetFontMetrics(tkfont, &fm);
	    height = fm.linespace;
	}
    }

    args->needed.width = width;
    args->needed.height = height;
}

static void HeightProcText(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;
    ElementText *masterX = (ElementText *) elem->master;
    int state = args->state;
    int match, match2;
    int height = 0;
    char *text = NULL;
    Tk_Font tkfont;
    Tk_FontMetrics fm;

    TextRedoLayoutIfNeeded("HeightProcText", args, args->height.fixedWidth);
   
    if (elemX->layout != NULL) {
	TextLayout_Size(elemX->layout, NULL, &height);
    } else {
	if (elemX->text != NULL) {
	    text = elemX->text;
	} else if ((masterX != NULL) && (masterX->text != NULL)) {
	    text = masterX->text;
	}
	if (text != NULL) {
	    FONT_FOR_STATE(tkfont, font, state)
	    if (tkfont == NULL)
		tkfont = tree->tkfont;
	    Tk_GetFontMetrics(tkfont, &fm);
	    height = fm.linespace;
	}
    }

    args->height.height = height;
}

int Element_GetSortData(TreeCtrl *tree, Element *elem, int type, long *lv, double *dv, char **sv)
{
    ElementText *elemX = (ElementText *) elem;
    ElementText *masterX = (ElementText *) elem->master;
    Tcl_Obj *dataObj = elemX->dataObj;
    int dataType = elemX->dataType;
    Tcl_Obj *obj;

    if (dataType == TDT_NULL && masterX != NULL)
	dataType = masterX->dataType;

    switch (type) {
	case SORT_ASCII:
	case SORT_DICT:
	    if (dataObj != NULL && dataType != TDT_NULL)
		(*sv) = Tcl_GetString(dataObj);
	    else
		(*sv) = elemX->text;
	    break;
	case SORT_DOUBLE:
	    if (dataObj != NULL && dataType == TDT_DOUBLE)
		obj = dataObj;
	    else
		obj = elemX->textObj;
	    if (obj == NULL)
		return TCL_ERROR;
	    if (Tcl_GetDoubleFromObj(tree->interp, obj, dv) != TCL_OK)
		return TCL_ERROR;
	    break;
	case SORT_LONG:
	    if (dataObj != NULL && dataType != TDT_NULL) {
		if (dataType == TDT_LONG || dataType == TDT_TIME) {
		    if (Tcl_GetLongFromObj(tree->interp, dataObj, lv) != TCL_OK)
			return TCL_ERROR;
		    break;
		}
		if (dataType == TDT_INTEGER) {
		    int iv;
		    if (Tcl_GetIntFromObj(tree->interp, dataObj, &iv) != TCL_OK)
			return TCL_ERROR;
		    (*lv) = iv;
		    break;
		}
	    }
	    if (elemX->textObj != NULL)
		if (Tcl_GetLongFromObj(tree->interp, elemX->textObj, lv) != TCL_OK)
		    return TCL_ERROR;
	    break;
    }
    return TCL_OK;
}

static int StateProcText(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementText *elemX = (ElementText *) elem;
    ElementText *masterX = (ElementText *) elem->master;
    int match, match2;
    int draw1, draw2;
    XColor *f1, *f2;
    Tk_Font tkfont1, tkfont2;
    int mask = 0;

    BOOLEAN_FOR_STATE(draw1, draw, args->states.state1)
    if (draw1 == -1)
	draw1 = 1;
    COLOR_FOR_STATE(f1, fill, args->states.state1)
    FONT_FOR_STATE(tkfont1, font, args->states.state1)

    BOOLEAN_FOR_STATE(draw2, draw, args->states.state2)
    if (draw2 == -1)
	draw2 = 1;
    COLOR_FOR_STATE(f2, fill, args->states.state2)
    FONT_FOR_STATE(tkfont2, font, args->states.state2)

    if (tkfont1 != tkfont2)
	mask |= CS_DISPLAY | CS_LAYOUT;

    if ((draw1 != draw2) || (f1 != f2))
	mask |= CS_DISPLAY;

    return mask;
}

static int UndefProcText(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementText *elemX = (ElementText *) args->elem;
    int modified = 0;

    modified |= PerStateInfo_Undefine(tree, &pstBoolean, &elemX->draw, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstColor, &elemX->fill, args->state);
    modified |= PerStateInfo_Undefine(tree, &pstFont, &elemX->font, args->state);
    return modified;
}

static int ActualProcText(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementText *elemX = (ElementText *) args->elem;
    ElementText *masterX = (ElementText *) args->elem->master;
    static CONST char *optionName[] = {
	"-draw", "-fill", "-font",
	(char *) NULL };
    int index, match, matchM;
    Tcl_Obj *obj = NULL;

    if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	case 0:
	{
	    OBJECT_FOR_STATE(obj, pstBoolean, draw, args->state)
	    break;
	}
	case 1:
	{
	    OBJECT_FOR_STATE(obj, pstColor, fill, args->state)
	    break;
	}
	case 2:
	{
	    OBJECT_FOR_STATE(obj, pstFont, font, args->state)
	    break;
	}
    }
    if (obj != NULL)
	Tcl_SetObjResult(tree->interp, obj);
    return TCL_OK;
}

ElementType elemTypeText = {
    "text",
    sizeof(ElementText),
    textOptionSpecs,
    NULL,
    CreateProcText,
    DeleteProcText,
    ConfigProcText,
    DisplayProcText,
    NeededProcText,
    HeightProcText,
    WorldChangedProcText,
    StateProcText,
    UndefProcText,
    ActualProcText,
    NULL /* onScreenProc */
};

/*****/

typedef struct ElementWindow ElementWindow;

struct ElementWindow
{
    Element header;
    PerStateInfo draw;		/* -draw */
    TreeCtrl *tree;
    TreeItem item; 		/* Needed if window changes size */
    TreeItemColumn column; 	/* Needed if window changes size */
    Tk_Window tkwin;		/* Window associated with item.  NULL means
				 * window has been destroyed. */
    int destroy;		/* Destroy window when element
				 * is deleted */
#define CLIP_WINDOW 1
#ifdef CLIP_WINDOW
    int clip;			/* TRUE if tkwin is a borderless frame
				 * widget whose first child is the actual
				 * window we want displayed. */
    Tk_Window child;		/* The first child of tkwin. tkwin is resized
				 * so that it is never out-of-bounds, and
				 * the child is positioned within tkwin to
				 * provide clipping of the child. */
#endif
};

#define EWIN_CONF_WINDOW 0x0001
#define EWIN_CONF_DRAW 0x0002

static Tk_OptionSpec windowOptionSpecs[] = {
#ifdef CLIP_WINDOW
    {TK_OPTION_CUSTOM, "-clip", (char *) NULL, (char *) NULL,
     (char) NULL, -1, Tk_Offset(ElementWindow, clip),
     TK_OPTION_NULL_OK, (ClientData) &booleanCO, 0},
#endif
    {TK_OPTION_CUSTOM, "-destroy", (char *) NULL, (char *) NULL,
     (char) NULL, -1, Tk_Offset(ElementWindow, destroy),
     TK_OPTION_NULL_OK, (ClientData) &booleanCO, 0},
    {TK_OPTION_STRING, "-draw", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementWindow, draw.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, EWIN_CONF_DRAW},
    {TK_OPTION_WINDOW, "-window", (char *) NULL, (char *) NULL,
     (char) NULL, -1, Tk_Offset(ElementWindow, tkwin),
     TK_OPTION_NULL_OK, (ClientData) NULL, EWIN_CONF_WINDOW},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void
WinItemStructureProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to record describing window elem. */
    XEvent *eventPtr;		/* Describes what just happened. */
{
    ElementWindow *elemX = (ElementWindow *) clientData;

    if (eventPtr->type == DestroyNotify) {
	elemX->tkwin = elemX->child = NULL;
	Tree_ElementChangedItself(elemX->tree, elemX->item, elemX->column,
	    (Element *) elemX, CS_LAYOUT | CS_DISPLAY);
    }
}

static void
WinItemRequestProc(clientData, tkwin)
    ClientData clientData;		/* Pointer to record for window item. */
    Tk_Window tkwin;			/* Window that changed its desired
					 * size. */
{
    ElementWindow *elemX = (ElementWindow *) clientData;

#ifdef CLIP_WINDOW
    /* We don't care about size changes for the clip window. */
    if (elemX->child != NULL && tkwin != elemX->child)
	return;
#endif
    Tree_ElementChangedItself(elemX->tree, elemX->item, elemX->column,
	(Element *) elemX, CS_LAYOUT | CS_DISPLAY);
}

static void
WinItemLostSlaveProc(clientData, tkwin)
    ClientData clientData;	/* WindowItem structure for slave window that
				 * was stolen away. */
    Tk_Window tkwin;		/* Tk's handle for the slave window. */
{
    ElementWindow *elemX = (ElementWindow *) clientData;
    TreeCtrl *tree = elemX->tree;

#ifdef CLIP_WINDOW
    if (tkwin == elemX->child) {
	Tk_DeleteEventHandler(elemX->child, StructureNotifyMask,
		WinItemStructureProc, (ClientData) elemX);
	Tk_UnmapWindow(elemX->child);
	elemX->child = NULL;
    } else {
	Tk_DeleteEventHandler(elemX->tkwin, StructureNotifyMask,
		WinItemStructureProc, (ClientData) elemX);
	if (tree->tkwin != Tk_Parent(elemX->tkwin)) {
	    Tk_UnmaintainGeometry(elemX->tkwin, tree->tkwin);
	}
	Tk_UnmapWindow(elemX->tkwin);
	elemX->tkwin = NULL;
    }
#else
    Tk_DeleteEventHandler(elemX->tkwin, StructureNotifyMask,
	    WinItemStructureProc, (ClientData) elemX);
    if (tree->tkwin != Tk_Parent(elemX->tkwin)) {
	Tk_UnmaintainGeometry(elemX->tkwin, tree->tkwin);
    }
    Tk_UnmapWindow(elemX->tkwin);
    elemX->tkwin = NULL;
#endif
    Tree_ElementChangedItself(elemX->tree, elemX->item, elemX->column,
	(Element *) elemX, CS_LAYOUT | CS_DISPLAY);
}

static Tk_GeomMgr winElemGeomType = {
    "treectrl",				/* name */
    WinItemRequestProc,			/* requestProc */
    WinItemLostSlaveProc,		/* lostSlaveProc */
};

static void DeleteProcWindow(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementWindow *elemX = (ElementWindow *) elem;
    ElementWindow *masterX = (ElementWindow *) elem->master;

    if (elemX->tkwin != NULL) {
#ifdef CLIP_WINDOW
	if (elemX->child != NULL) {
	    Tk_DeleteEventHandler(elemX->child, StructureNotifyMask,
		    WinItemStructureProc, (ClientData) elemX);
	    Tk_ManageGeometry(elemX->child, (Tk_GeomMgr *) NULL,
		    (ClientData) NULL);
	    Tk_UnmapWindow(elemX->child);
	    elemX->child = NULL;
	}
#endif
	Tk_DeleteEventHandler(elemX->tkwin, StructureNotifyMask,
		WinItemStructureProc, (ClientData) elemX);
	Tk_ManageGeometry(elemX->tkwin, (Tk_GeomMgr *) NULL,
		(ClientData) NULL);
	if (tree->tkwin != Tk_Parent(elemX->tkwin)) {
	    Tk_UnmaintainGeometry(elemX->tkwin, tree->tkwin);
	}
	Tk_UnmapWindow(elemX->tkwin);

	if ((elemX->destroy == 1) ||
		((masterX != NULL) && (masterX->destroy == 1))) {
	    Tk_DestroyWindow(elemX->tkwin);
	}

	elemX->tkwin = NULL;
    }

    PerStateInfo_Free(tree, &pstBoolean, &elemX->draw);
}

static int WorldChangedProcWindow(ElementArgs *args)
{
    int flagM = args->change.flagMaster;
    int flagS = args->change.flagSelf;
    int mask = 0;

    if ((flagS | flagM) & (EWIN_CONF_DRAW))
	mask |= CS_DISPLAY;

    if ((flagS | flagM) & (EWIN_CONF_WINDOW))
	mask |= CS_DISPLAY | CS_LAYOUT;

    return mask;
}

static int ConfigProcWindow(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementWindow *elemX = (ElementWindow *) elem;
    ElementWindow *masterX = (ElementWindow *) elem->master;
    ElementWindow savedX;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;

    savedX.tkwin = elemX->tkwin;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) elemX,
			elem->typePtr->optionTable,
			args->config.objc, args->config.objv, tree->tkwin,
			&savedOptions, &args->config.flagSelf) != TCL_OK) {
		args->config.flagSelf = 0;
		continue;
	    }

	    if (args->config.flagSelf & EWIN_CONF_DRAW)
		PSTSave(&elemX->draw, &savedX.draw);

	    if (args->config.flagSelf & EWIN_CONF_DRAW) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBoolean, &elemX->draw) != TCL_OK)
		    continue;
	    }

	    if (args->config.flagSelf & EWIN_CONF_WINDOW) {
		if ((elem->master == NULL) && (elemX->tkwin != NULL)){
		    FormatResult(tree->interp, "can't specify -window for a master element");
		    continue;
		}
	    }

	    if (args->config.flagSelf & BITMAP_CONF_DRAW)
		PerStateInfo_Free(tree, &pstBoolean, &savedX.draw);
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (args->config.flagSelf & EWIN_CONF_DRAW)
		PSTRestore(tree, &pstBoolean, &elemX->draw, &savedX.draw);

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    if (savedX.tkwin != elemX->tkwin) {
	if (savedX.tkwin != NULL) {
#ifdef CLIP_WINDOW
	    if (elemX->child != NULL) {
		Tk_DeleteEventHandler(elemX->child, StructureNotifyMask,
			WinItemStructureProc, (ClientData) elemX);
		Tk_ManageGeometry(elemX->child, (Tk_GeomMgr *) NULL,
			(ClientData) NULL);
		Tk_UnmapWindow(elemX->child);
		elemX->child = NULL;
	    }
#endif
	    Tk_DeleteEventHandler(savedX.tkwin, StructureNotifyMask,
		    WinItemStructureProc, (ClientData) elemX);
	    Tk_ManageGeometry(savedX.tkwin, (Tk_GeomMgr *) NULL,
		    (ClientData) NULL);
	    Tk_UnmaintainGeometry(savedX.tkwin, tree->tkwin);
	    Tk_UnmapWindow(savedX.tkwin);
	}
	if (elemX->tkwin != NULL) {
	    Tk_Window ancestor, parent;

	    /*
	     * Make sure that the treectrl is either the parent of the
	     * window associated with the element or a descendant of that
	     * parent.  Also, don't allow a top-of-hierarchy window to be
	     * managed inside a treectrl.
	     */

	    parent = Tk_Parent(elemX->tkwin);
	    for (ancestor = tree->tkwin; ;
		    ancestor = Tk_Parent(ancestor)) {
		if (ancestor == parent) {
		    break;
		}
		if (((Tk_FakeWin *) (ancestor))->flags & TK_TOP_HIERARCHY) {
		    badWindow:
		    FormatResult(tree->interp,
			    "can't use %s in a window element of %s",
			    Tk_PathName(elemX->tkwin),
			    Tk_PathName(tree->tkwin));
		    elemX->tkwin = NULL;
		    return TCL_ERROR;
		}
	    }
	    if (((Tk_FakeWin *) (elemX->tkwin))->flags & TK_TOP_HIERARCHY) {
		goto badWindow;
	    }
	    if (elemX->tkwin == tree->tkwin) {
		goto badWindow;
	    }
#ifdef CLIP_WINDOW
	    if ((elemX->clip == 1) || ((masterX != NULL) && (masterX->clip == 1))) {
		elemX->child = (Tk_Window) ((TkWindow *) elemX->tkwin)->childList;
		if (elemX->child != NULL) {
		    Tk_CreateEventHandler(elemX->child, StructureNotifyMask,
			    WinItemStructureProc, (ClientData) elemX);
		    Tk_ManageGeometry(elemX->child, &winElemGeomType,
			    (ClientData) elemX);
		}
	    }
#endif
	    Tk_CreateEventHandler(elemX->tkwin, StructureNotifyMask,
		    WinItemStructureProc, (ClientData) elemX);
	    Tk_ManageGeometry(elemX->tkwin, &winElemGeomType,
		    (ClientData) elemX);
	}
    }
#if 0
    if ((elemX->tkwin != NULL)
	    && (itemPtr->state == TK_STATE_HIDDEN)) {
	if (tree->tkwin == Tk_Parent(elemX->tkwin)) {
	    Tk_UnmapWindow(elemX->tkwin);
	} else {
	    Tk_UnmaintainGeometry(elemX->tkwin, tree->tkwin);
	}
    }
#endif
    return TCL_OK;
}

static int CreateProcWindow(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementWindow *elemX = (ElementWindow *) elem;

    elemX->tree = tree;
    elemX->item = args->create.item;
    elemX->column = args->create.column;
    elemX->destroy = -1;
#ifdef CLIP_WINDOW
    elemX->clip = -1;
#endif
    return TCL_OK;
}

static void DisplayProcWindow(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementWindow *elemX = (ElementWindow *) elem;
    ElementWindow *masterX = (ElementWindow *) elem->master;
    int state = args->state;
    int x = args->display.x, y = args->display.y;
    int minX, maxX, minY, maxY;
    int width, height;
    int match, match2;
    int draw;

    BOOLEAN_FOR_STATE(draw, draw, state);
    if (!draw)
	goto hideIt;

    if (elemX->tkwin == NULL)
	return;

#ifdef CLIP_WINDOW
    if (elemX->child != NULL) {
	width = Tk_ReqWidth(elemX->child);
	height = Tk_ReqHeight(elemX->child);
    } else {
	width = Tk_ReqWidth(elemX->tkwin);
	height = Tk_ReqHeight(elemX->tkwin);
    }
    if (width < 1 || height < 1)
	goto hideIt;
#else
    width = Tk_ReqWidth(elemX->tkwin);
    height = Tk_ReqHeight(elemX->tkwin);
#endif
    AdjustForSticky(args->display.sticky,
	args->display.width, args->display.height,
	TRUE, TRUE,
	&x, &y, &width, &height);
    x += tree->drawableXOrigin - tree->xOrigin;
    y += tree->drawableYOrigin - tree->yOrigin;

    /*
     * If the window is completely out of the visible area of the treectrl
     * then unmap it.  The window could suddenly reappear if the treectrl
     * window gets resized.
     */

    minX = args->display.bounds[0];
    minY = args->display.bounds[1];
    maxX = args->display.bounds[2];
    maxY = args->display.bounds[3];

    if (((x + width) <= minX) || ((y + height) <= minY)
	    || (x >= maxX) || (y >= maxY)) {
hideIt:
	if (tree->tkwin == Tk_Parent(elemX->tkwin)) {
	    Tk_UnmapWindow(elemX->tkwin); 
	} else {
	    Tk_UnmaintainGeometry(elemX->tkwin, tree->tkwin);
	}
	return;
    }

#ifdef CLIP_WINDOW
    if (elemX->child != NULL) {
	int cx = x, cy = y, cw = width, ch = height; /* clip win coords */

	if (cx < minX) {
	    cw -= minX - cx;
	    cx = minX;
	}
	if (cy < minY) {
	    ch -= minY - cy;
	    cy = minY;
	}
	if (cx + cw > maxX)
	    cw = maxX - cx;
	if (cy + ch > maxY)
	    ch = maxY - cy;

	/*
	* Reposition and map the window (but in different ways depending
	* on whether the treectrl is the window's parent).
	*/

	if (tree->tkwin == Tk_Parent(elemX->tkwin)) {
	    if ((cx != Tk_X(elemX->tkwin)) || (cy != Tk_Y(elemX->tkwin))
		    || (cw != Tk_Width(elemX->tkwin))
		    || (ch != Tk_Height(elemX->tkwin))) {
		Tk_MoveResizeWindow(elemX->tkwin, cx, cy, cw, ch);
	    }
	    Tk_MapWindow(elemX->tkwin);
	} else {
	    Tk_MaintainGeometry(elemX->tkwin, tree->tkwin, cx, cy, cw, ch);
	}

	/*
	 * Position the child window within the clip window.
	 */
	x -= cx;
	y -= cy;
	if ((x != Tk_X(elemX->child)) || (y != Tk_Y(elemX->child))
		|| (width != Tk_Width(elemX->child))
		|| (height != Tk_Height(elemX->child))) {
	    Tk_MoveResizeWindow(elemX->child, x, y, width, height);
	}
	Tk_MapWindow(elemX->child);
	return;
    }
#endif /* CLIP_WINDOW */

    /*
     * Reposition and map the window (but in different ways depending
     * on whether the treectrl is the window's parent).
     */

    if (tree->tkwin == Tk_Parent(elemX->tkwin)) {
	if ((x != Tk_X(elemX->tkwin)) || (y != Tk_Y(elemX->tkwin))
		|| (width != Tk_Width(elemX->tkwin))
		|| (height != Tk_Height(elemX->tkwin))) {
	    Tk_MoveResizeWindow(elemX->tkwin, x, y, width, height);
	}
	Tk_MapWindow(elemX->tkwin);
    } else {
	Tk_MaintainGeometry(elemX->tkwin, tree->tkwin, x, y,
		width, height);
    }
}

static void NeededProcWindow(ElementArgs *args)
{
/*    TreeCtrl *tree = args->tree;*/
    Element *elem = args->elem;
    ElementWindow *elemX = (ElementWindow *) elem;
/*    ElementWindow *masterX = (ElementWindow *) elem->master;
    int state = args->state;*/
    int width = 0, height = 0;

#ifdef CLIP_WINDOW
    if (elemX->child != NULL) {
	width = Tk_ReqWidth(elemX->child);
	if (width <= 0) {
	    width = 1;
	}
	height = Tk_ReqHeight(elemX->child);
	if (height <= 0) {
	    height = 1;
	}
    } else
#endif
    if (elemX->tkwin) {
	width = Tk_ReqWidth(elemX->tkwin);
	if (width <= 0) {
	    width = 1;
	}
	height = Tk_ReqHeight(elemX->tkwin);
	if (height <= 0) {
	    height = 1;
	}
    }
    args->needed.width = width;
    args->needed.height = height;
}

static int StateProcWindow(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementBitmap *elemX = (ElementBitmap *) elem;
    ElementBitmap *masterX = (ElementBitmap *) elem->master;
    int match, match2;
    int draw1, draw2;
    int mask = 0;

    BOOLEAN_FOR_STATE(draw1, draw, args->states.state1)
    if (draw1 == -1)
	draw1 = 1;

    BOOLEAN_FOR_STATE(draw2, draw, args->states.state2)
    if (draw2 == -1)
	draw2 = 1;

    if (draw1 != draw2)
	mask |= CS_DISPLAY;

    return mask;
}

static int UndefProcWindow(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementWindow *elemX = (ElementWindow *) elem;

    return PerStateInfo_Undefine(tree, &pstBoolean, &elemX->draw, args->state);
}

static int ActualProcWindow(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementText *elemX = (ElementText *) args->elem;
    ElementText *masterX = (ElementText *) args->elem->master;
    static CONST char *optionName[] = {
	"-draw",
	(char *) NULL };
    int index, match, matchM;
    Tcl_Obj *obj = NULL;

    if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	case 0:
	{
	    OBJECT_FOR_STATE(obj, pstBoolean, draw, args->state)
	    break;
	}
    }
    if (obj != NULL)
	Tcl_SetObjResult(tree->interp, obj);
    return TCL_OK;
}

static void OnScreenProcWindow(ElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    Element *elem = args->elem;
    ElementWindow *elemX = (ElementWindow *) elem;

    if (!args->screen.visible && (elemX->tkwin != NULL)) {
	if (tree->tkwin == Tk_Parent(elemX->tkwin)) {
	    Tk_UnmapWindow(elemX->tkwin); 
	} else {
	    Tk_UnmaintainGeometry(elemX->tkwin, tree->tkwin);
	}
    }
}

ElementType elemTypeWindow = {
    "window",
    sizeof(ElementWindow),
    windowOptionSpecs,
    NULL,
    CreateProcWindow,
    DeleteProcWindow,
    ConfigProcWindow,
    DisplayProcWindow,
    NeededProcWindow,
    NULL, /* heightProc */
    WorldChangedProcWindow,
    StateProcWindow,
    UndefProcWindow,
    ActualProcWindow,
    OnScreenProcWindow
};

/*****/

typedef struct ElementAssocData ElementAssocData;

struct ElementAssocData
{
    ElementType *typeList;
};

int TreeElement_TypeFromObj(TreeCtrl *tree, Tcl_Obj *objPtr, ElementType **typePtrPtr)
{
    Tcl_Interp *interp = tree->interp;
    ElementAssocData *assocData;
    char *typeStr;
    int length;
    ElementType *typeList;
    ElementType *typePtr, *matchPtr = NULL;

    assocData = Tcl_GetAssocData(interp, "TreeCtrlElementTypes", NULL);
    typeList = assocData->typeList;

    typeStr = Tcl_GetStringFromObj(objPtr, &length);
    if (!length)
    {
	FormatResult(interp, "invalid element type \"\"");
	return TCL_ERROR;
    }
    for (typePtr = typeList;
	typePtr != NULL;
	typePtr = typePtr->next)
    {
	if ((typeStr[0] == typePtr->name[0]) &&
		!strncmp(typeStr, typePtr->name, length))
	{
	    if (matchPtr != NULL)
	    {
		FormatResult(interp,
			"ambiguous element type \"%s\"",
			typeStr);
		return TCL_ERROR;
	    }
	    matchPtr = typePtr;
	}
    }
    if (matchPtr == NULL)
    {
	FormatResult(interp, "unknown element type \"%s\"", typeStr);
	return TCL_ERROR;
    }
    *typePtrPtr = matchPtr;

    return TCL_OK;
}

int TreeCtrl_RegisterElementType(Tcl_Interp *interp, ElementType *newTypePtr)
{
    ElementAssocData *assocData;
    ElementType *typeList;
    ElementType *prevPtr, *typePtr, *nextPtr;

    assocData = Tcl_GetAssocData(interp, "TreeCtrlElementTypes", NULL);
    typeList = assocData->typeList;

    for (typePtr = typeList, prevPtr = NULL;
	 typePtr != NULL;
	 prevPtr = typePtr, typePtr = nextPtr) {
	nextPtr = typePtr->next;
	/* Remove duplicate type */
	if (!strcmp(typePtr->name, newTypePtr->name)) {
	    if (prevPtr == NULL)
		typeList = typePtr->next;
	    else
		prevPtr->next = typePtr->next;
	    ckfree((char *) typePtr);
	}
    }
    typePtr = (ElementType *) ckalloc(sizeof(ElementType));
    memcpy(typePtr, newTypePtr, sizeof(ElementType));

    typePtr->next = typeList;
    typeList = typePtr;

    typePtr->optionTable = Tk_CreateOptionTable(interp,
	    newTypePtr->optionSpecs);

    assocData->typeList = typeList;

    return TCL_OK;
}

TreeCtrlStubs stubs = {
    TreeCtrl_RegisterElementType,
    Tree_RedrawElement,
    Tree_ElementIterateBegin,
    Tree_ElementIterateNext,
    Tree_ElementIterateGet,
    Tree_ElementIterateChanged,
    PerStateInfo_Free,
    PerStateInfo_FromObj,
    PerStateInfo_ForState,
    PerStateInfo_ObjForState,
    PerStateInfo_Undefine,
    &pstBoolean,
    PerStateBoolean_ForState,
    PSTSave,
    PSTRestore,
    TreeStateFromObj,
    BooleanCO_Init,
    StringTableCO_Init
};

static void FreeAssocData(ClientData clientData, Tcl_Interp *interp)
{
    ElementAssocData *assocData = (ElementAssocData *) clientData;
    ElementType *typeList = assocData->typeList;
    ElementType *next;

    while (typeList != NULL)
    {
	next = typeList->next;
	/* The ElementType.optionTables are freed when the interp is deleted */
	ckfree((char *) typeList);
	typeList = next;
    }
    ckfree((char *) assocData);
}

int TreeElement_Init(Tcl_Interp *interp)
{
    ElementAssocData *assocData;

    IntegerCO_Init(elemTypeText.optionSpecs, "-lines",
	0, 	/* min */
	0, 	/* max (ignored) */
	-1, 	/* empty */
	0x01); 	/* flags: min */
    StringTableCO_Init(elemTypeText.optionSpecs, "-datatype", textDataTypeST);
    StringTableCO_Init(elemTypeText.optionSpecs, "-justify", textJustifyST);
    StringTableCO_Init(elemTypeText.optionSpecs, "-wrap", textWrapST);

    assocData = (ElementAssocData *) ckalloc(sizeof(ElementAssocData));
    assocData->typeList = NULL;
    Tcl_SetAssocData(interp, "TreeCtrlElementTypes", FreeAssocData, assocData);

    TreeCtrl_RegisterElementType(interp, &elemTypeBitmap);
    TreeCtrl_RegisterElementType(interp, &elemTypeBorder);
/*    TreeCtrl_RegisterElementType(interp, &elemTypeCheckButton);*/
    TreeCtrl_RegisterElementType(interp, &elemTypeImage);
    TreeCtrl_RegisterElementType(interp, &elemTypeRect);
    TreeCtrl_RegisterElementType(interp, &elemTypeText);
    TreeCtrl_RegisterElementType(interp, &elemTypeWindow);

    Tcl_SetAssocData(interp, "TreeCtrlStubs", NULL, &stubs);

    return TCL_OK;
}
