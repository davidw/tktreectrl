/* 
 * tkTreeCtrl.c --
 *
 *	This module implements treectrl widgets for the Tk toolkit.
 *
 * Copyright (c) 2002-2005 Tim Baker
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003-2004 ActiveState, a division of Sophos
 *
 * RCS: @(#) $Id: tkTreeCtrl.c,v 1.49 2005/07/12 03:37:13 treectrl Exp $
 */

#include "tkTreeCtrl.h"

/*
 * TIP #116 altered Tk_PhotoPutBlock API to add interp arg.
 * We need to remove that for compiling with 8.4.
 */
#if (TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 5)
#define TK_PHOTOPUTBLOCK(interp, hdl, blk, x, y, w, h, cr) \
		Tk_PhotoPutBlock(hdl, blk, x, y, w, h, cr)
#define TK_PHOTOPUTZOOMEDBLOCK(interp, hdl, blk, x, y, w, h, \
				zx, zy, sx, sy, cr) \
		Tk_PhotoPutZoomedBlock(hdl, blk, x, y, w, h, \
				zx, zy, sx, sy, cr)
#else
#define TK_PHOTOPUTBLOCK	Tk_PhotoPutBlock
#define TK_PHOTOPUTZOOMEDBLOCK	Tk_PhotoPutZoomedBlock
#endif

static CONST char *bgModeST[] = {
    "column", "order", "ordervisible", "row", "index", "visindex", (char *) NULL
};
static CONST char *columnResizeModeST[] = {
    "proxy", "realtime", (char *) NULL
};
static CONST char *doubleBufferST[] = {
    "none", "item", "window", (char *) NULL
};
static CONST char *lineStyleST[] = {
    "dot", "solid", (char *) NULL
};
static CONST char *orientStringTable[] = {
    "horizontal", "vertical", (char *) NULL
};

extern Tk_ObjCustomOption columnCustomOption_NOT_TAIL;

static Tk_OptionSpec optionSpecs[] = {
    {TK_OPTION_BORDER, "-background", "background", "Background",
     "white", -1, Tk_Offset(TreeCtrl, border), 0, 
     (ClientData) "white", TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-backgroundimage", "backgroundImage", "BackgroundImage",
      (char *) NULL, -1, Tk_Offset(TreeCtrl, backgroundImageString),
      TK_OPTION_NULL_OK, (ClientData) NULL,
      TREE_CONF_BG_IMAGE | TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING_TABLE, "-backgroundmode",
     "backgroundMode", "BackgroundMode",
     "row", -1, Tk_Offset(TreeCtrl, backgroundMode),
     0, (ClientData) bgModeST, TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING_TABLE, "-columnresizemode",
     "columnResizeMode", "ColumnResizeMode",
     "proxy", -1, Tk_Offset(TreeCtrl, columnResizeMode),
     0, (ClientData) columnResizeModeST, 0},
    {TK_OPTION_SYNONYM, "-bd", (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) "-borderwidth"},
    {TK_OPTION_SYNONYM, "-bg", (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) "-background"},
    {TK_OPTION_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
     DEF_LISTBOX_BORDER_WIDTH, Tk_Offset(TreeCtrl, borderWidthObj),
     Tk_Offset(TreeCtrl, borderWidth),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_COLOR, "-buttoncolor", "buttonColor", "ButtonColor",
     "#808080", -1, Tk_Offset(TreeCtrl, buttonColor),
     0, (ClientData) NULL, TREE_CONF_BUTTON | TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-buttonbitmap", "buttonBitmap", "ButtonBitmap",
     (char *) NULL, Tk_Offset(TreeCtrl, buttonBitmap.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL,
     TREE_CONF_BUTTON | TREE_CONF_BUTBMP | TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-buttonimage", "buttonImage", "ButtonImage",
     (char *) NULL, Tk_Offset(TreeCtrl, buttonImage.obj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL,
     TREE_CONF_BUTTON | TREE_CONF_BUTIMG | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-buttonsize", "buttonSize", "ButtonSize",
     "9", Tk_Offset(TreeCtrl, buttonSizeObj),
     Tk_Offset(TreeCtrl, buttonSize),
     0, (ClientData) NULL, TREE_CONF_BUTTON | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-buttonthickness",
     "buttonThickness", "ButtonThickness",
     "1", Tk_Offset(TreeCtrl, buttonThicknessObj),
     Tk_Offset(TreeCtrl, buttonThickness),
     0, (ClientData) NULL, TREE_CONF_BUTTON | TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-columnprefix", "columnPrefix", "ColumnPrefix",
     "", -1, Tk_Offset(TreeCtrl, columnPrefix), 0, (ClientData) NULL, 0},
    {TK_OPTION_PIXELS, "-columnproxy", "columnProxy", "ColumnProxy",
     (char *) NULL, Tk_Offset(TreeCtrl, columnProxy.xObj),
     Tk_Offset(TreeCtrl, columnProxy.x),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_PROXY},
    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor",
     (char *) NULL, -1, Tk_Offset(TreeCtrl, cursor),
     TK_OPTION_NULL_OK, (ClientData) NULL, 0},
    {TK_OPTION_STRING, "-defaultstyle", "defaultStyle", "DefaultStyle",
     (char *) NULL, Tk_Offset(TreeCtrl, defaultStyle.stylesObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_DEFSTYLE},
    {TK_OPTION_STRING_TABLE, "-doublebuffer",
     "doubleBuffer", "DoubleBuffer",
     "item", -1, Tk_Offset(TreeCtrl, doubleBuffer),
     0, (ClientData) doubleBufferST, TREE_CONF_REDISPLAY},
    {TK_OPTION_SYNONYM, "-fg", (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) "-foreground"},
    {TK_OPTION_FONT, "-font", "font", "Font",
     DEF_LISTBOX_FONT, Tk_Offset(TreeCtrl, fontObj),
     Tk_Offset(TreeCtrl, tkfont),
     0, (ClientData) NULL, TREE_CONF_FONT | TREE_CONF_RELAYOUT},
    {TK_OPTION_COLOR, "-foreground", "foreground", "Foreground",
     DEF_LISTBOX_FG, Tk_Offset(TreeCtrl, fgObj), Tk_Offset(TreeCtrl, fgColorPtr),
     0, (ClientData) NULL, TREE_CONF_FG | TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-height", "height", "Height",
     "200", Tk_Offset(TreeCtrl, heightObj), Tk_Offset(TreeCtrl, height),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_COLOR, "-highlightbackground", "highlightBackground",
     "HighlightBackground", DEF_LISTBOX_HIGHLIGHT_BG, -1, 
     Tk_Offset(TreeCtrl, highlightBgColorPtr),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
     DEF_LISTBOX_HIGHLIGHT, -1, Tk_Offset(TreeCtrl, highlightColorPtr),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-highlightthickness", "highlightThickness",
     "HighlightThickness", DEF_LISTBOX_HIGHLIGHT_WIDTH,
     Tk_Offset(TreeCtrl, highlightWidthObj),
     Tk_Offset(TreeCtrl, highlightWidth),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-indent", "indent", "Indent",
     "19", Tk_Offset(TreeCtrl, indentObj),
     Tk_Offset(TreeCtrl, indent),
     0, (ClientData) NULL, TREE_CONF_INDENT | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-itemheight", "itemHeight", "ItemHeight",
     "0", Tk_Offset(TreeCtrl, itemHeightObj),
     Tk_Offset(TreeCtrl, itemHeight),
     0, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
#if 0
    {TK_OPTION_CUSTOM, "-itempadx", (char *) NULL, (char *) NULL,
     "0",
     Tk_Offset(TreeCtrl, itemPadXObj),
     Tk_Offset(TreeCtrl, itemPadX),
     0, (ClientData) &PadAmountOption, 0},
    {TK_OPTION_CUSTOM, "-itempady", (char *) NULL, (char *) NULL,
     "0",
     Tk_Offset(TreeCtrl, itemPadYObj),
     Tk_Offset(TreeCtrl, itemPadY),
     0, (ClientData) &PadAmountOption, 0},
#endif
    {TK_OPTION_STRING, "-itemprefix", "itemPrefix", "ItemPrefix",
     "", -1, Tk_Offset(TreeCtrl, itemPrefix), 0, (ClientData) NULL, 0},
    {TK_OPTION_PIXELS, "-itemwidth", "itemWidth", "ItemWidth",
     "", Tk_Offset(TreeCtrl, itemWidthObj), Tk_Offset(TreeCtrl, itemWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-itemwidthequal", "itemWidthEqual", "ItemWidthEqual",
     "0", -1, Tk_Offset(TreeCtrl, itemWidthEqual),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-itemwidthmultiple", "itemWidthMultiple", "ItemWidthMultiple",
     "", Tk_Offset(TreeCtrl, itemWidMultObj), Tk_Offset(TreeCtrl, itemWidMult),
     TK_OPTION_NULL_OK, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_COLOR, "-linecolor", "lineColor", "LineColor",
     "#808080", -1, Tk_Offset(TreeCtrl, lineColor),
     0, (ClientData) NULL, TREE_CONF_LINE | TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING_TABLE, "-linestyle", "lineStyle", "LineStyle",
     "dot", -1, Tk_Offset(TreeCtrl, lineStyle),
     0, (ClientData) lineStyleST, TREE_CONF_LINE | TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-linethickness", "lineThickness", "LineThickness",
     "1", Tk_Offset(TreeCtrl, lineThicknessObj),
     Tk_Offset(TreeCtrl, lineThickness),
     0, (ClientData) NULL, TREE_CONF_LINE | TREE_CONF_REDISPLAY},
    {TK_OPTION_PIXELS, "-minitemheight", "minItemHeight", "MinItemHeight",
     "0", Tk_Offset(TreeCtrl, minItemHeightObj),
     Tk_Offset(TreeCtrl, minItemHeight),
     0, (ClientData) NULL, TREE_CONF_ITEMSIZE | TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING_TABLE, "-orient", "orient", "Orient",
     "vertical", -1, Tk_Offset(TreeCtrl, vertical),
     0, (ClientData) orientStringTable, TREE_CONF_RELAYOUT},
    {TK_OPTION_RELIEF, "-relief", "relief", "Relief",
     "sunken", -1, Tk_Offset(TreeCtrl, relief),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-scrollmargin", "scrollMargin", "ScrollMargin",
     "0", Tk_Offset(TreeCtrl, scrollMargin), -1,
     0, (ClientData) NULL, 0},
    {TK_OPTION_STRING, "-selectmode", "selectMode", "SelectMode",
     DEF_LISTBOX_SELECT_MODE, -1, Tk_Offset(TreeCtrl, selectMode),
     TK_OPTION_NULL_OK, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-showbuttons", "showButtons",
     "ShowButtons", "1", -1, Tk_Offset(TreeCtrl, showButtons),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showheader", "showHeader", "ShowHeader",
     "1", -1, Tk_Offset(TreeCtrl, showHeader),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showlines", "showLines",
     "ShowLines", "1", -1, Tk_Offset(TreeCtrl, showLines),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_BOOLEAN, "-showrootlines", "showRootLines",
     "ShowRootLines", "1", -1, Tk_Offset(TreeCtrl, showRootLines),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_BOOLEAN, "-showroot", "showRoot",
     "ShowRoot", "1", -1, Tk_Offset(TreeCtrl, showRoot),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-showrootbutton", "showRootButton",
     "ShowRootButton", "0", -1, Tk_Offset(TreeCtrl, showRootButton),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-takefocus", "takeFocus", "TakeFocus",
     DEF_LISTBOX_TAKE_FOCUS, -1, Tk_Offset(TreeCtrl, takeFocus),
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_CUSTOM, "-treecolumn", "treeColumn", "TreeColumn",
     (char *) NULL, -1, Tk_Offset(TreeCtrl, columnTree),
     TK_OPTION_NULL_OK, (ClientData) &columnCustomOption_NOT_TAIL,
     TREE_CONF_RELAYOUT},
    {TK_OPTION_BOOLEAN, "-usetheme", "useTheme",
     "UseTheme", "0", -1, Tk_Offset(TreeCtrl, useTheme),
     0, (ClientData) NULL, TREE_CONF_THEME | TREE_CONF_RELAYOUT},
    {TK_OPTION_PIXELS, "-width", "width", "Width",
     "200", Tk_Offset(TreeCtrl, widthObj), Tk_Offset(TreeCtrl, width),
     0, (ClientData) NULL, TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-wrap", "wrap", "Wrap",
     (char *) NULL, Tk_Offset(TreeCtrl, wrapObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL,
     TREE_CONF_WRAP | TREE_CONF_RELAYOUT},
    {TK_OPTION_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
     (char *) NULL, -1, Tk_Offset(TreeCtrl, xScrollCmd),
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_STRING, "-xscrolldelay", "xScrollDelay", "ScrollDelay",
     "50", Tk_Offset(TreeCtrl, xScrollDelay), -1,
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_PIXELS, "-xscrollincrement", "xScrollIncrement", "ScrollIncrement",
     "0", -1, Tk_Offset(TreeCtrl, xScrollIncrement),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
     (char *) NULL, -1, Tk_Offset(TreeCtrl, yScrollCmd),
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_STRING, "-yscrolldelay", "yScrollDelay", "ScrollDelay",
     "50", Tk_Offset(TreeCtrl, yScrollDelay), -1,
     TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_PIXELS, "-yscrollincrement", "yScrollIncrement", "ScrollIncrement",
     "0", -1, Tk_Offset(TreeCtrl, yScrollIncrement),
     0, (ClientData) NULL, TREE_CONF_REDISPLAY},
    {TK_OPTION_END}
};

static Tk_OptionSpec debugSpecs[] = {
    {TK_OPTION_INT, "-displaydelay", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeCtrl, debug.displayDelay),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-data", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, debug.data),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-display", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, debug.display),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-enable", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeCtrl, debug.enable),
     0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-erasecolor", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, debug.eraseColor),
     TK_OPTION_NULL_OK, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-textlayout", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(TreeCtrl, debug.textLayout),
     0, (ClientData) NULL, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

static int TreeWidgetCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
static int TreeConfigure(Tcl_Interp *interp, TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[], int createFlag);
static void TreeEventProc(ClientData clientData, XEvent * eventPtr);
static void TreeDestroy(char *memPtr);
static void TreeCmdDeletedProc(ClientData clientData);
static void TreeWorldChanged(ClientData instanceData);
static void TreeComputeGeometry(TreeCtrl *tree);
static int TreeStateCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
static int TreeSelectionCmd(Tcl_Interp *interp, TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
static int TreeXviewCmd(Tcl_Interp *interp, TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
static int TreeYviewCmd(Tcl_Interp *interp, TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
static int TreeDebugCmd(ClientData clientData, Tcl_Interp *interp, int objc,Tcl_Obj *CONST objv[]);

static Tk_ClassProcs treectrlClass = {
    sizeof(Tk_ClassProcs),	/* size */
    TreeWorldChanged,		/* worldChangedProc. */
    NULL,			/* createProc. */
    NULL			/* modalProc. */
};

static int TreeObjCmd(ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[])
{
    TreeCtrl *tree;
    Tk_Window tkwin;
    Tk_OptionTable optionTable;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName ?options?");
	return TCL_ERROR;
    }

    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), 
	    Tcl_GetStringFromObj(objv[1], NULL), (char *) NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }

    optionTable = Tk_CreateOptionTable(interp, optionSpecs);

    tree = (TreeCtrl *) ckalloc(sizeof(TreeCtrl));
    memset(tree, '\0', sizeof(TreeCtrl));
    tree->tkwin		= tkwin;
    tree->display	= Tk_Display(tkwin);
    tree->interp	= interp;
    tree->widgetCmd	= Tcl_CreateObjCommand(interp,
				Tk_PathName(tree->tkwin), TreeWidgetCmd,
				(ClientData) tree, TreeCmdDeletedProc);
    tree->optionTable	= optionTable;
    tree->relief	= TK_RELIEF_SUNKEN;
    tree->prevWidth	= Tk_Width(tkwin);
    tree->prevHeight	= Tk_Height(tkwin);
    tree->updateIndex	= 1;

    tree->stateNames[0]	= "open";
    tree->stateNames[1]	= "selected";
    tree->stateNames[2]	= "enabled";
    tree->stateNames[3]	= "active";
    tree->stateNames[4]	= "focus";

    Tcl_InitHashTable(&tree->selection, TCL_ONE_WORD_KEYS);

    /* Do this before Tree_InitColumns() which does Tk_InitOptions(), which
     * calls Tk_GetOption() which relies on the window class */
    Tk_SetClass(tkwin, "TreeCtrl");
    Tk_SetClassProcs(tkwin, &treectrlClass, (ClientData) tree);

    tree->debug.optionTable = Tk_CreateOptionTable(interp, debugSpecs);
    (void) Tk_InitOptions(interp, (char *) tree, tree->debug.optionTable,
	    tkwin);

    Tcl_InitHashTable(&tree->itemHash, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&tree->elementHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&tree->styleHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&tree->imageHash, TCL_STRING_KEYS);

#ifdef ALLOC_HAX
    tree->allocData = AllocHax_Init();
#endif

    Tree_InitColumns(tree);
    TreeItem_Init(tree);
    TreeNotify_Init(tree);
    TreeMarquee_Init(tree);
    TreeDragImage_Init(tree);
    TreeDInfo_Init(tree);

    Tk_CreateEventHandler(tree->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask|ActivateMask,
	    TreeEventProc, (ClientData) tree);

    /* Must do this on Unix because Tk_GCForColor() uses
     * Tk_WindowId(tree->tkwin) */
    Tk_MakeWindowExist(tree->tkwin);

    if (Tk_InitOptions(interp, (char *) tree, optionTable, tkwin) != TCL_OK) {
	Tk_DestroyWindow(tree->tkwin);
	WFREE(tree, TreeCtrl);
	return TCL_ERROR;
    }

    if (TreeConfigure(interp, tree, objc - 2, objv + 2, TRUE) != TCL_OK) {
	Tk_DestroyWindow(tree->tkwin);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(Tk_PathName(tree->tkwin), -1));
    return TCL_OK;
}

#define W2Cx(x) ((x) + tree->xOrigin)
#define C2Wx(x) ((x) - tree->xOrigin)
#define C2Ox(x) ((x) - tree->inset)

#define W2Cy(y) ((y) + tree->yOrigin)
#define C2Wy(y) ((y) - tree->yOrigin)
#define C2Oy(y) ((y) - tree->inset - Tree_HeaderHeight(tree))

static int TreeWidgetCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    int result = TCL_OK;
    static CONST char *commandName[] = {
	"activate", "canvasx", "canvasy", "cget", "collapse",
	"column", "compare", "configure", "contentbox",
	"debug", "depth", "dragimage",
	"element", "expand", "identify", "index", "item",
	"marquee", "notify", "numcolumns", "numitems", "orphans",
	"range", "scan", "see", "selection", "state", "style",
	"toggle", "xview", "yview", (char *) NULL
    };
    enum {
	COMMAND_ACTIVATE, COMMAND_CANVASX, COMMAND_CANVASY, COMMAND_CGET,
	COMMAND_COLLAPSE, COMMAND_COLUMN, COMMAND_COMPARE, COMMAND_CONFIGURE,
	COMMAND_CONTENTBOX, COMMAND_DEBUG, COMMAND_DEPTH,
	COMMAND_DRAGIMAGE, COMMAND_ELEMENT, COMMAND_EXPAND,COMMAND_IDENTIFY,
	COMMAND_INDEX, COMMAND_ITEM, COMMAND_MARQUEE, COMMAND_NOTIFY,
	COMMAND_NUMCOLUMNS, COMMAND_NUMITEMS, COMMAND_ORPHANS, COMMAND_RANGE,
	COMMAND_SCAN, COMMAND_SEE, COMMAND_SELECTION, COMMAND_STATE,
	COMMAND_STYLE, COMMAND_TOGGLE, COMMAND_XVIEW, COMMAND_YVIEW
    };
    Tcl_Obj *resultObjPtr;
    int index;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], commandName, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_Preserve((ClientData) tree);

    switch (index) {
	case COMMAND_ACTIVATE:
	{
	    TreeItem item;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "item");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &item, 0) != TCL_OK) {
		goto error;
	    }
	    if (item != tree->activeItem) {
		TreeNotify_ActiveItem(tree, tree->activeItem, item);
		TreeItem_ChangeState(tree, tree->activeItem, STATE_ACTIVE, 0);
		tree->activeItem = item;
		TreeItem_ChangeState(tree, tree->activeItem, 0, STATE_ACTIVE);
	    }
	    break;
	}

	case COMMAND_CANVASX:
	{
	    int x;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "x");
		goto error;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(x + tree->xOrigin));
	    break;
	}

	case COMMAND_CANVASY:
	{
	    int y;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "y");
		goto error;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[2], &y) != TCL_OK)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(y + tree->yOrigin));
	    break;
	}

	case COMMAND_CGET:
	{
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "option");
		goto error;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) tree,
		    tree->optionTable, objv[2], tree->tkwin);
	    if (resultObjPtr == NULL) {
		result = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, resultObjPtr);
	    }
	    break;
	}

	case COMMAND_CONFIGURE:
	{
	    resultObjPtr = NULL;
	    if (objc <= 3) {
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) tree,
			tree->optionTable,
			(objc == 2) ? (Tcl_Obj *) NULL : objv[2],
			tree->tkwin);
		if (resultObjPtr == NULL) {
		    result = TCL_ERROR;
		} else {
		    Tcl_SetObjResult(interp, resultObjPtr);
		}
	    } else {
		result = TreeConfigure(interp, tree, objc - 2, objv + 2, FALSE);
	    }
	    break;
	}

	case COMMAND_CONTENTBOX:
	{
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		goto error;
	    }
	    FormatResult(interp, "%d %d %d %d",
		    tree->inset, tree->inset + Tree_HeaderHeight(tree),
		    Tk_Width(tree->tkwin) - tree->inset,
		    Tk_Height(tree->tkwin) - tree->inset);
	    break;
	}

	case COMMAND_COLLAPSE:
	case COMMAND_EXPAND:
	case COMMAND_TOGGLE:
	{
	    char *s;
	    int i;
	    int recurse = 0;
	    TreeItem item;

	    if (objc == 2)
		break;
	    s = Tcl_GetString(objv[2]);
	    if (!strcmp(s, "-recurse")) {
		recurse = 1;
		if (objc == 3)
		    break;
	    }
	    for (i = 2 + recurse; i < objc; i++) {
		int oldRecurse = recurse;
		if (TreeItem_FromObj(tree, objv[i], &item, IFO_ALLOK) != TCL_OK)
		    goto error;
		if (item == ITEM_ALL) {
		    item = tree->root;
		    recurse = 1;
		}
		switch (index) {
		    case COMMAND_COLLAPSE:
			TreeItem_OpenClose(tree, item, 0, recurse);
			break;
		    case COMMAND_EXPAND:
			TreeItem_OpenClose(tree, item, 1, recurse);
			break;
		    case COMMAND_TOGGLE:
			TreeItem_OpenClose(tree, item, -1, recurse);
			break;
		}
		recurse = oldRecurse;
	    }
#ifdef SELECTION_VISIBLE
	    Tree_DeselectHidden(tree);
#endif
#if 0
	    if (tree->debug.enable)
		Tree_Debug(tree);
#endif
	    break;
	}

	case COMMAND_COLUMN:
	{
	    result = TreeColumnCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_COMPARE:
	{
	    TreeItem item1, item2;
	    static CONST char *opName[] = { "<", "<=", "==", ">=", ">", "!=", NULL };
	    int op, compare = 0, index1, index2;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 2, objv, "item1 op item2");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &item1, 0) != TCL_OK)
		goto error;
	    if (Tcl_GetIndexFromObj(interp, objv[3], opName, "comparison operator", 0,
		    &op) != TCL_OK)
		goto error;
	    if (TreeItem_FromObj(tree, objv[4], &item2, 0) != TCL_OK)
		goto error;
	    if (TreeItem_RootAncestor(tree, item1) !=
		    TreeItem_RootAncestor(tree, item2)) {
		FormatResult(interp,
			"item %s%d and item %s%d don't share a common ancestor",
			tree->itemPrefix, TreeItem_GetID(tree, item1),
			tree->itemPrefix, TreeItem_GetID(tree, item2));
		goto error;
	    }
	    TreeItem_ToIndex(tree, item1, &index1, NULL);
	    TreeItem_ToIndex(tree, item2, &index2, NULL);
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

	case COMMAND_DEBUG:
	{
	    result = TreeDebugCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_DEPTH:
	{
	    TreeItem item;
	    int depth;

	    if (objc < 2 || objc > 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?item?");
		goto error;
	    }
	    if (objc == 3) {
		if (TreeItem_FromObj(tree, objv[2], &item, 0) != TCL_OK)
		    goto error;
		depth = TreeItem_GetDepth(tree, item);
		if (TreeItem_RootAncestor(tree, item) == tree->root)
		    depth++;
		Tcl_SetObjResult(interp, Tcl_NewIntObj(depth));
		break;
	    }
	    if (tree->updateIndex)
		Tree_UpdateItemIndex(tree);
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->depth + 1));
	    break;
	}

	case COMMAND_DRAGIMAGE:
	{
	    result = DragImageCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_ELEMENT:
	{
	    result = TreeElementCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_IDENTIFY:
	{
	    int x, y, depth;
	    TreeItem item;
	    char buf[64];
/*
  set id [$tree identify $x $y]
  "item I column C" : mouse is in column C of item I
  "item I column C elem E" : mouse is in element E in column C of item I
  "item I button" : mouse is in button-area of item I
  "item I line J" : mouse is near line coming from item J
  "header C ?left|right?" : mouse is in header column C
  "" : mouse is not in any item
*/
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "x y");
		goto error;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK)
		goto error;
	    if (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK)
		goto error;

	    /* Require point inside borders */
	    if ((x < tree->inset) || (x >= Tk_Width(tree->tkwin) - tree->inset))
		break;
	    if ((y < tree->inset) || (y >= Tk_Height(tree->tkwin) - tree->inset))
		break;

	    /* Point in header */
	    if (y < tree->inset + Tree_HeaderHeight(tree)) {
		int left = 0 - tree->xOrigin;
		TreeColumn column = tree->columns;

		while (column != NULL) {
		    int width = TreeColumn_UseWidth(column);
		    if ((x >= left) && (x < left + width)) {
			sprintf(buf, "header %s%d", tree->columnPrefix,
			    TreeColumn_GetID(column));
			if (x < left + 4)
			    sprintf(buf + strlen(buf), " left");
			else if (x >= left + width - 4)
			    sprintf(buf + strlen(buf), " right");
			Tcl_SetResult(interp, buf, TCL_VOLATILE);
			goto done;
		    }
		    left += width;
		    column = TreeColumn_Next(column);
		}
		strcpy(buf, "header tail");
		if (x < left + 4)
		    sprintf(buf + strlen(buf), " left");
		Tcl_SetResult(interp, buf, TCL_VOLATILE);
		break;
	    }

	    item = Tree_ItemUnderPoint(tree, &x, &y, FALSE);
	    if (item == NULL)
		break;

	    sprintf(buf, "item %s%d", tree->itemPrefix, TreeItem_GetID(tree, item)); /* TreeItem_ToObj() */
	    depth = TreeItem_GetDepth(tree, item);
	    if (tree->showRoot || tree->showButtons || tree->showLines)
		depth++;
	    if (tree->showRoot && tree->showButtons && tree->showRootButton)
		depth++;
	    if (item == tree->root)
		depth = (tree->showButtons && tree->showRootButton) ? 1 : 0;

	    /* Point is in a line or button */
	    if (tree->columnTreeVis && (x >= tree->columnTreeLeft) &&
		    (x < tree->columnTreeLeft + TreeColumn_UseWidth(tree->columnTree)) &&
		    (x < tree->columnTreeLeft + depth * tree->useIndent)) {
		int column = (x - tree->columnTreeLeft) / tree->useIndent + 1;
		if (column == depth) {
		    if (tree->showButtons && TreeItem_GetButton(tree, item))
			sprintf(buf + strlen(buf), " button");
		}
		else if (tree->showLines) {
		    TreeItem sibling;
		    do {
			item = TreeItem_GetParent(tree, item);
		    } while (++column < depth);
		    sibling = TreeItem_NextSiblingVisible(tree, item);
		    if ((sibling != NULL) &&
			((TreeItem_GetParent(tree, sibling) != tree->root) ||
			tree->showRootLines))
			sprintf(buf + strlen(buf), " line %s%d", tree->itemPrefix,
				TreeItem_GetID(tree, item)); /* TreeItem_ToObj() */
		}
#if 1
	    } else {
		TreeItem_Identify(tree, item, x, y, buf);
	    }
#else
	    } else if (tree->columnCountVis == 1) {
		char *elem;

		sprintf(buf + strlen(buf), " column %s%d",
			tree->columnPrefix, TreeColumn_GetID(tree->columnVis));
		elem = TreeItem_Identify(tree, item, x, y);
		if (elem != NULL)
		    sprintf(buf + strlen(buf), " elem %s", elem);
	    } else {
		/* Point is in a TreeItemColumn */
		int left = 0;
		TreeColumn treeColumn;
		TreeItemColumn itemColumn;

		treeColumn = tree->columns;
		itemColumn = TreeItem_GetFirstColumn(tree, item);
		while (itemColumn != NULL) {
		    int width = TreeColumn_UseWidth(treeColumn);
		    if ((x >= left) && (x < left + width)) {
			char *elem;
			sprintf(buf + strlen(buf), " column %s%d",
				tree->columnPrefix, TreeColumn_GetID(treeColumn));
			elem = TreeItem_Identify(tree, item, x, y);
			if (elem != NULL)
			    sprintf(buf + strlen(buf), " elem %s", elem);
			break;
		    }
		    left += width;
		    treeColumn = TreeColumn_Next(treeColumn);
		    itemColumn = TreeItemColumn_GetNext(tree, itemColumn);
		}
	    }
#endif
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	    break;
	}

	case COMMAND_INDEX:
	{
	    TreeItem item;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "item");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &item, IFO_NULLOK) != TCL_OK) {
		goto error;
	    }
	    if (item != NULL)
		Tcl_SetObjResult(interp, TreeItem_ToObj(tree, item));
	    break;
	}

	case COMMAND_ITEM:
	{
	    result = TreeItemCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_MARQUEE:
	{
	    result = TreeMarqueeCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_NOTIFY:
	{
	    result = TreeNotifyCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_NUMCOLUMNS:
	{
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		goto error;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->columnCount));
	    break;
	}

	case COMMAND_NUMITEMS:
	{
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		goto error;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->itemCount));
	    break;
	}

	case COMMAND_ORPHANS:
	{
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;
	    Tcl_Obj *listObj;
	    TreeItem item;

	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		goto error;
	    }

	    /* Pretty slow. Could keep a hash table of orphans */
	    listObj = Tcl_NewListObj(0, NULL);
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		if ((item != tree->root) &&
			(TreeItem_GetParent(tree, item) == NULL)) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    TreeItem_ToObj(tree, item));
		}
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	case COMMAND_RANGE:
	{
	    TreeItem item, itemFirst, itemLast;
	    int indexFirst, indexLast;
	    Tcl_Obj *listObj;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "first last");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &itemFirst, 0) != TCL_OK)
		goto error;
	    if (TreeItem_FromObj(tree, objv[3], &itemLast, 0) != TCL_OK)
		goto error;
	    if (TreeItem_RootAncestor(tree, itemFirst) !=
		    TreeItem_RootAncestor(tree, itemLast)) {
		FormatResult(interp,
			"item %s%d and item %s%d don't share a common ancestor",
			tree->itemPrefix, TreeItem_GetID(tree, itemFirst),
			tree->itemPrefix, TreeItem_GetID(tree, itemLast));
		goto error;
	    }
	    TreeItem_ToIndex(tree, itemFirst, &indexFirst, NULL);
	    TreeItem_ToIndex(tree, itemLast, &indexLast, NULL);
	    if (indexFirst > indexLast) {
		item = itemFirst;
		itemFirst = itemLast;
		itemLast = item;
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    item = itemFirst;
	    while (item != NULL) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, item));
		if (item == itemLast)
		    break;
		item = TreeItem_Next(tree, item);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	case COMMAND_SCAN:
	{
	    static CONST char *optionName[] = { "dragto", "mark",
						(char *) NULL };
	    int x, y, gain = 10, xOrigin, yOrigin;

	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "option ?arg arg ...?");
		goto error;
	    }
	    if (Tcl_GetIndexFromObj(interp, objv[2], optionName, "option",
		    2, &index) != TCL_OK)
		goto error;
	    switch (index) {
		/* T scan dragto x y ?gain? */
		case 0:
		    if ((objc < 5) || (objc > 6)) {
			Tcl_WrongNumArgs(interp, 3, objv, "x y ?gain?");
			goto error;
		    }
		    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
			    (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK))
			goto error;
		    if (objc == 6) {
			if (Tcl_GetIntFromObj(interp, objv[5], &gain) != TCL_OK)
			    goto error;
		    }
		    xOrigin = tree->scanXOrigin - gain * (x - tree->scanX);
		    yOrigin = tree->scanYOrigin - gain * (y - tree->scanY);
		    Tree_SetOriginX(tree, xOrigin);
		    Tree_SetOriginY(tree, yOrigin);
		    break;

		/* T scan mark x y ?gain? */
		case 1:
		    if (objc != 5) {
			Tcl_WrongNumArgs(interp, 3, objv, "x y");
			goto error;
		    }
		    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK) ||
			    (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK))
			goto error;
		    tree->scanX = x;
		    tree->scanY = y;
		    tree->scanXOrigin = tree->xOrigin;
		    tree->scanYOrigin = tree->yOrigin;
		    break;
	    }
	    break;
	}

	case COMMAND_SEE:
	{
	    TreeItem item;
	    int x, y, w, h;
	    int topInset = tree->inset + Tree_HeaderHeight(tree);
	    int visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
	    int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;
	    int xOrigin = tree->xOrigin;
	    int yOrigin = tree->yOrigin;
	    int minX = tree->inset;
	    int minY = topInset;
	    int maxX = minX + visWidth;
	    int maxY = minY + visHeight;
	    int index, offset;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "item");
		goto error;
	    }
	    if (TreeItem_FromObj(tree, objv[2], &item, 0) != TCL_OK)
		goto error;

	    /* Canvas coords */
	    if (Tree_ItemBbox(tree, item, &x, &y, &w, &h) < 0)
		break;

	    if ((C2Wx(x) > maxX) || (C2Wx(x + w) <= minX) || (w <= visWidth)) {
		if ((C2Wx(x) < minX) || (w > visWidth)) {
		    index = Increment_FindX(tree, x);
		    offset = Increment_ToOffsetX(tree, index);
		    xOrigin = C2Ox(offset);
		}
		else if (C2Wx(x + w) > maxX) {
		    index = Increment_FindX(tree, x + w - visWidth);
		    offset = Increment_ToOffsetX(tree, index);
		    if (offset < x + w - visWidth) {
			index++;
			offset = Increment_ToOffsetX(tree, index);
		    }
		    xOrigin = C2Ox(offset);
		}
	    }

	    if ((C2Wy(y) > maxY) || (C2Wy(y + h) <= minY) || (h <= visHeight)) {
		if ((C2Wy(y) < minY) || (h > visHeight)) {
		    index = Increment_FindY(tree, y);
		    offset = Increment_ToOffsetY(tree, index);
		    yOrigin = C2Oy(offset);
		}
		else if (C2Wy(y + h) > maxY) {
		    index = Increment_FindY(tree, y + h - visHeight);
		    offset = Increment_ToOffsetY(tree, index);
		    if (offset < y + h - visHeight) {
			index++;
			offset = Increment_ToOffsetY(tree, index);
		    }
		    yOrigin = C2Oy(offset);
		}
	    }

	    Tree_SetOriginX(tree, xOrigin);
	    Tree_SetOriginY(tree, yOrigin);
	    break;
	}

	case COMMAND_SELECTION:
	{
	    result = TreeSelectionCmd(interp, tree, objc, objv);
	    break;
	}

	case COMMAND_STATE:
	{
	    result = TreeStateCmd(tree, objc, objv);
	    break;
	}

	case COMMAND_STYLE:
	{
	    result = TreeStyleCmd(clientData, interp, objc, objv);
	    break;
	}

	case COMMAND_XVIEW:
	{
	    result = TreeXviewCmd(interp, tree, objc, objv);
	    break;
	}

	case COMMAND_YVIEW:
	{
	    result = TreeYviewCmd(interp, tree, objc, objv);
	    break;
	}
    }
done:
    Tcl_Release((ClientData) tree);
    return result;

error:
    Tcl_Release((ClientData) tree);
    return TCL_ERROR;
}

static int TreeConfigure(Tcl_Interp *interp, TreeCtrl *tree, int objc,
	Tcl_Obj *CONST objv[], int createFlag)
{
    int error;
    Tcl_Obj *errorResult = NULL;
    TreeCtrl saved;
    Tk_SavedOptions savedOptions;
    int oldShowRoot = tree->showRoot;
    int mask;
    XGCValues gcValues;
    unsigned long gcMask;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(interp, (char *) tree, tree->optionTable, objc,
		    objv, tree->tkwin, &savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* Wouldn't have to do this if Tk_InitOptions() would return
	    * a mask of configured options like Tk_SetOptions() does. */
	    if (createFlag) {
		if (tree->backgroundImageString != NULL)
		    mask |= TREE_CONF_BG_IMAGE;
		if (tree->buttonBitmap.obj != NULL)
		    mask |= TREE_CONF_BUTBMP;
		if (tree->buttonImage.obj != NULL)
		    mask |= TREE_CONF_BUTIMG;
		if (tree->defaultStyle.stylesObj != NULL)
		    mask |= TREE_CONF_DEFSTYLE;
		if (tree->wrapObj != NULL)
		    mask |= TREE_CONF_WRAP;
		if (!ObjectIsEmpty(tree->itemWidthObj))
		    mask |= TREE_CONF_ITEMSIZE;
		if (!ObjectIsEmpty(tree->itemWidMultObj))
		    mask |= TREE_CONF_ITEMSIZE;
	    }

	    /*
	     * Step 1: Save old values
	     */

	    if (mask & TREE_CONF_BG_IMAGE)
		saved.backgroundImage = tree->backgroundImage;
	    if (mask & TREE_CONF_BUTBMP)
		PSTSave(&tree->buttonBitmap, &saved.buttonBitmap);
	    if (mask & TREE_CONF_BUTIMG)
		PSTSave(&tree->buttonImage, &saved.buttonImage);
	    if (mask & TREE_CONF_DEFSTYLE) {
		saved.defaultStyle.styles = tree->defaultStyle.styles;
		saved.defaultStyle.numStyles = tree->defaultStyle.numStyles;
	    }
	    if (mask & TREE_CONF_WRAP) {
		saved.wrapMode = tree->wrapMode;
		saved.wrapArg = tree->wrapArg;
	    }

	    /*
	     * Step 2: Process new values
	     */

	    if (mask & TREE_CONF_BG_IMAGE) {
		tree->backgroundImage = NULL;
		if (tree->backgroundImageString != NULL) {
		    Tk_Image image = Tree_GetImage(tree, tree->backgroundImageString);
		    if (image == NULL)
			continue;
		    tree->backgroundImage = image;
		}
	    }

	    if (mask & TREE_CONF_BUTBMP) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstBitmap,
		    &tree->buttonBitmap) != TCL_OK)
		    continue;
	    }

	    if (mask & TREE_CONF_BUTIMG) {
		if (PerStateInfo_FromObj(tree, TreeStateFromObj, &pstImage,
		    &tree->buttonImage) != TCL_OK)
		    continue;
	    }

	    if (mask & TREE_CONF_DEFSTYLE) {
		if (tree->defaultStyle.stylesObj == NULL) {
		    tree->defaultStyle.styles = NULL;
		    tree->defaultStyle.numStyles = 0;
		} else {
		    int i, listObjc;
		    Tcl_Obj **listObjv;
		    TreeStyle style;

		    if ((Tcl_ListObjGetElements(interp,
			tree->defaultStyle.stylesObj, &listObjc, &listObjv)
			!= TCL_OK)) continue;
		    tree->defaultStyle.styles =
			(TreeStyle *) ckalloc(sizeof(TreeStyle) * listObjc);
		    tree->defaultStyle.numStyles = listObjc;
		    for (i = 0; i < listObjc; i++) {
			if (ObjectIsEmpty(listObjv[i])) {
			    style = NULL;
			} else {
			    if (TreeStyle_FromObj(tree, listObjv[i], &style) != TCL_OK) {
				ckfree((char *) tree->defaultStyle.styles);
				break;
			    }
			}
			tree->defaultStyle.styles[i] = style;
		    }
		    if (i < listObjc)
			continue;
		}
	    }

	    /* Parse -wrap string into wrapMode and wrapArg */
	    if (mask & TREE_CONF_WRAP) {
		int listObjc;
		Tcl_Obj **listObjv;

		if (tree->wrapObj == NULL) {
		    tree->wrapMode = TREE_WRAP_NONE;
		    tree->wrapArg = 0;
		} else {
		    int len0, len1;
		    char *s0, *s1, ch0, ch1;

		    if ((Tcl_ListObjGetElements(interp, tree->wrapObj, &listObjc,
			    &listObjv) != TCL_OK) || (listObjc > 2)) {
badWrap:
			FormatResult(interp, "bad wrap \"%s\"",
				Tcl_GetString(tree->wrapObj));
			continue;
		    }
		    if (listObjc == 1) {
			s0 = Tcl_GetStringFromObj(listObjv[0], &len0);
			ch0 = s0[0];
			if ((ch0 == 'w') && !strncmp(s0, "window", len0)) {
			    tree->wrapMode = TREE_WRAP_WINDOW;
			    tree->wrapArg = 0;
			}
			else
			    goto badWrap;
		    } else {
			s1 = Tcl_GetStringFromObj(listObjv[1], &len1);
			ch1 = s1[0];
			if ((ch1 == 'i') && !strncmp(s1, "items", len1)) {
			    int n;
			    if ((Tcl_GetIntFromObj(interp, listObjv[0], &n) != TCL_OK) ||
				    (n < 0)) {
				goto badWrap;
			    }
			    tree->wrapMode = TREE_WRAP_ITEMS;
			    tree->wrapArg = n;
			}
			else if ((ch1 == 'p') && !strncmp(s1, "pixels", len1)) {
			    int n;
			    if (Tk_GetPixelsFromObj(interp, tree->tkwin, listObjv[0], &n)
				    != TCL_OK) {
				goto badWrap;
			    }
			    tree->wrapMode = TREE_WRAP_PIXELS;
			    tree->wrapArg = n;
			}
			else
			    goto badWrap;
		    }
		}
	    }

	    /*
	     * Step 3: Free saved values
	     */

	    if (mask & TREE_CONF_BG_IMAGE)
		; /* nothing to free */
	    if (mask & TREE_CONF_DEFSTYLE) {
		if (saved.defaultStyle.styles != NULL)
		    ckfree((char *) saved.defaultStyle.styles);
	    }
	    if (mask & TREE_CONF_BUTBMP)
		PerStateInfo_Free(tree, &pstBitmap, &saved.buttonBitmap);
	    if (mask & TREE_CONF_BUTIMG)
		PerStateInfo_Free(tree, &pstImage, &saved.buttonImage);

	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (mask & TREE_CONF_BG_IMAGE) {
		tree->backgroundImage = saved.backgroundImage;
	    }
	    if (mask & TREE_CONF_BUTBMP) {
		PSTRestore(tree, &pstBitmap, &tree->buttonBitmap,
		    &saved.buttonBitmap);
	    }
	    if (mask & TREE_CONF_BUTIMG) {
		PSTRestore(tree, &pstImage, &tree->buttonImage,
		    &saved.buttonImage);
	    }
	    if (mask & TREE_CONF_DEFSTYLE) {
		tree->defaultStyle.styles = saved.defaultStyle.styles;
		tree->defaultStyle.numStyles = saved.defaultStyle.numStyles;
	    }
	    if (mask & TREE_CONF_WRAP) {
		tree->wrapMode = saved.wrapMode;
		tree->wrapArg = saved.wrapArg;
	    }

	    Tcl_SetObjResult(interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    tree->itemPrefixLen = strlen(tree->itemPrefix);
    tree->columnPrefixLen = strlen(tree->columnPrefix);

    Tk_SetWindowBackground(tree->tkwin,
	    Tk_3DBorderColor(tree->border)->pixel);

    if (createFlag)
	mask |= TREE_CONF_FONT | TREE_CONF_RELAYOUT;

    if (mask & (TREE_CONF_FONT | TREE_CONF_FG)) {
	/*
	 * Should be blended into TreeWorldChanged.
	 */
	gcValues.font = Tk_FontId(tree->tkfont);
	gcValues.foreground = tree->fgColorPtr->pixel;
	gcValues.graphics_exposures = False;
	gcMask = GCForeground | GCFont | GCGraphicsExposures;
	if (tree->textGC != None)
	    Tk_FreeGC(tree->display, tree->textGC);
	tree->textGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
    }

    if (tree->copyGC == None) {
	gcValues.function = GXcopy;
	gcValues.graphics_exposures = False;
	gcMask = GCFunction | GCGraphicsExposures;
	tree->copyGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
    }

    if (createFlag)
	mask |= TREE_CONF_BUTTON;

    if (mask & TREE_CONF_BUTTON) {
	if (tree->buttonGC != None)
	    Tk_FreeGC(tree->display, tree->buttonGC);
	gcValues.foreground = tree->buttonColor->pixel;
	gcValues.line_width = tree->buttonThickness;
	gcMask = GCForeground | GCLineWidth;
	tree->buttonGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
    }

    if (createFlag)
	mask |= TREE_CONF_LINE;

    if (mask & TREE_CONF_LINE) {
	if (tree->lineGC != None)
	    Tk_FreeGC(tree->display, tree->lineGC);
	gcValues.foreground = tree->lineColor->pixel;
	gcValues.line_width = tree->lineThickness;
	gcMask = GCForeground | GCLineWidth;
	tree->lineGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
    }

    if (mask & TREE_CONF_PROXY) {
	TreeColumnProxy_Undisplay(tree);
	TreeColumnProxy_Display(tree);
    }

    tree->useIndent = MAX(tree->indent, ButtonMaxWidth(tree));

    if (tree->highlightWidth < 0)
	tree->highlightWidth = 0;
    tree->inset = tree->highlightWidth + tree->borderWidth;

    if (oldShowRoot != tree->showRoot) {
	TreeItem_InvalidateHeight(tree, tree->root);
	tree->updateIndex = 1;
    }

    TreeStyle_TreeChanged(tree, mask);
    TreeColumn_TreeChanged(tree, mask);

    if (mask & TREE_CONF_RELAYOUT) {
	TreeComputeGeometry(tree);
	Tree_InvalidateColumnWidth(tree, -1);
	Tree_InvalidateColumnHeight(tree, -1); /* In case -usetheme changes */
	Tree_RelayoutWindow(tree);
    } else if (mask & TREE_CONF_REDISPLAY) {
	Tree_RelayoutWindow(tree);
    }

    return TCL_OK;
}

static void TreeWorldChanged(ClientData instanceData)
{
    TreeCtrl *tree = (TreeCtrl *) instanceData;
    XGCValues gcValues;
    unsigned long gcMask;

    gcValues.font = Tk_FontId(tree->tkfont);
    gcValues.foreground = tree->fgColorPtr->pixel;
    gcValues.graphics_exposures = False;
    gcMask = GCForeground | GCFont | GCGraphicsExposures;
    if (tree->textGC != None)
	Tk_FreeGC(tree->display, tree->textGC);
    tree->textGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);

    TreeStyle_TreeChanged(tree, TREE_CONF_FONT | TREE_CONF_RELAYOUT);
    TreeColumn_TreeChanged(tree, TREE_CONF_FONT | TREE_CONF_RELAYOUT);

    TreeComputeGeometry(tree);
    Tree_InvalidateColumnWidth(tree, -1);
    Tree_RelayoutWindow(tree);
}

static void TreeEventProc(ClientData clientData, XEvent *eventPtr)
{
    TreeCtrl *tree = (TreeCtrl *) clientData;

    switch (eventPtr->type) {
	case Expose:
	{
	    int x = eventPtr->xexpose.x;
	    int y = eventPtr->xexpose.y;
	    Tree_RedrawArea(tree, x, y,
		    x + eventPtr->xexpose.width,
		    y + eventPtr->xexpose.height);
	    break;
	}
	case ConfigureNotify:
	{
	    if ((tree->prevWidth != Tk_Width(tree->tkwin)) ||
		    (tree->prevHeight != Tk_Height(tree->tkwin))) {
		tree->widthOfColumns = -1;
		Tree_RelayoutWindow(tree);
		tree->prevWidth = Tk_Width(tree->tkwin);
		tree->prevHeight = Tk_Height(tree->tkwin);
	    }
	    break;
	}
	case FocusIn:
	    /* Handle focus as Tile does */
	    if (   eventPtr->xfocus.detail == NotifyInferior
		|| eventPtr->xfocus.detail == NotifyAncestor
		|| eventPtr->xfocus.detail == NotifyNonlinear)
	    {
		Tree_FocusChanged(tree, 1);
	    }
	    break;
	case FocusOut:
	    /* Handle focus as Tile does */
	    if (   eventPtr->xfocus.detail == NotifyInferior
		|| eventPtr->xfocus.detail == NotifyAncestor
		|| eventPtr->xfocus.detail == NotifyNonlinear)
	    {
		Tree_FocusChanged(tree, 0);
	    }
	    break;
	case ActivateNotify:
	    Tree_Activate(tree, 1);
	    break;
	case DeactivateNotify:
	    Tree_Activate(tree, 0);
	    break;
	case DestroyNotify:
	    if (!tree->deleted) {
		tree->deleted = 1;
		Tcl_DeleteCommandFromToken(tree->interp, tree->widgetCmd);
		Tcl_EventuallyFree((ClientData) tree, TreeDestroy);
	    }
	    break;
    }
}

static void TreeCmdDeletedProc(ClientData clientData)
{
    TreeCtrl *tree = (TreeCtrl *) clientData;

    if (!tree->deleted) {
	Tk_DestroyWindow(tree->tkwin);
    }
}

static void TreeDestroy(char *memPtr)
{
    TreeCtrl *tree = (TreeCtrl *) memPtr;
    TreeItem item;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tk_Image image;
    int i;

    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
    while (hPtr != NULL) {
	item = (TreeItem) Tcl_GetHashValue(hPtr);
	TreeItem_FreeResources(tree, item);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&tree->itemHash);

    hPtr = Tcl_FirstHashEntry(&tree->imageHash, &search);
    while (hPtr != NULL) {
	image = (Tk_Image) Tcl_GetHashValue(hPtr);
	Tk_FreeImage(image);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&tree->imageHash);

    TreeStyle_Free(tree);

    TreeDragImage_Free(tree->dragImage);
    TreeMarquee_Free(tree->marquee);
    TreeDInfo_Free(tree);

    if (tree->copyGC != None)
	Tk_FreeGC(tree->display, tree->copyGC);
    if (tree->textGC != None)
	Tk_FreeGC(tree->display, tree->textGC);
    if (tree->buttonGC != None)
	Tk_FreeGC(tree->display, tree->buttonGC);
    if (tree->lineGC != None)
	Tk_FreeGC(tree->display, tree->lineGC);

    Tree_FreeColumns(tree);

    QE_DeleteBindingTable(tree->bindingTable);

    for (i = STATE_USER - 1; i < 32; i++)
	if (tree->stateNames[i] != NULL)
	    ckfree(tree->stateNames[i]);

    Tk_FreeConfigOptions((char *) tree, tree->debug.optionTable,
	    tree->tkwin);

    PerStateInfo_Free(tree, &pstBitmap, &tree->buttonBitmap);
    PerStateInfo_Free(tree, &pstImage, &tree->buttonImage);

    Tk_FreeConfigOptions((char *) tree, tree->optionTable, tree->tkwin);

    Tcl_DeleteHashTable(&tree->selection);

#ifdef ALLOC_HAX
    AllocHax_Finalize(tree->allocData);
#endif

    WFREE(tree, TreeCtrl);
}

void Tree_UpdateScrollbarX(TreeCtrl *tree)
{
    Tcl_Interp *interp = tree->interp;
    int result;
    double fractions[2];
    char buffer[TCL_DOUBLE_SPACE * 2];
    char *xScrollCmd;

    Tree_GetScrollFractionsX(tree, fractions);
    TreeNotify_Scroll(tree, fractions, FALSE);

    if (tree->xScrollCmd == NULL)
	return;

    Tcl_Preserve((ClientData) interp);
    Tcl_Preserve((ClientData) tree);

    xScrollCmd = tree->xScrollCmd;
    Tcl_Preserve((ClientData) xScrollCmd);
    sprintf(buffer, "%g %g", fractions[0], fractions[1]);
    result = Tcl_VarEval(interp, xScrollCmd, " ", buffer, (char *) NULL);
    if (result != TCL_OK)
	Tcl_BackgroundError(interp);
    Tcl_ResetResult(interp);
    Tcl_Release((ClientData) xScrollCmd);

    Tcl_Release((ClientData) tree);
    Tcl_Release((ClientData) interp);
}

void Tree_UpdateScrollbarY(TreeCtrl *tree)
{
    Tcl_Interp *interp = tree->interp;
    int result;
    double fractions[2];
    char buffer[TCL_DOUBLE_SPACE * 2];
    char *yScrollCmd;

    Tree_GetScrollFractionsY(tree, fractions);
    TreeNotify_Scroll(tree, fractions, TRUE);

    if (tree->yScrollCmd == NULL)
	return;

    Tcl_Preserve((ClientData) interp);
    Tcl_Preserve((ClientData) tree);

    yScrollCmd = tree->yScrollCmd;
    Tcl_Preserve((ClientData) yScrollCmd);
    sprintf(buffer, "%g %g", fractions[0], fractions[1]);
    result = Tcl_VarEval(interp, yScrollCmd, " ", buffer, (char *) NULL);
    if (result != TCL_OK)
	Tcl_BackgroundError(interp);
    Tcl_ResetResult(interp);
    Tcl_Release((ClientData) yScrollCmd);

    Tcl_Release((ClientData) tree);
    Tcl_Release((ClientData) interp);
}

static void TreeComputeGeometry(TreeCtrl *tree)
{
    Tk_SetInternalBorder(tree->tkwin, tree->inset);
    Tk_GeometryRequest(tree->tkwin, tree->width + tree->inset * 2,
	    tree->height + tree->inset * 2);
}

void Tree_AddItem(TreeCtrl *tree, TreeItem item)
{
    Tcl_HashEntry *hPtr;
    int id, isNew;

    id = TreeItem_SetID(tree, item, tree->nextItemId++);
    hPtr = Tcl_CreateHashEntry(&tree->itemHash, (char *) id, &isNew);
    Tcl_SetHashValue(hPtr, item);
    tree->itemCount++;
}

void Tree_RemoveItem(TreeCtrl *tree, TreeItem item)
{
    Tcl_HashEntry *hPtr;

    if (TreeItem_GetSelected(tree, item))
	Tree_RemoveFromSelection(tree, item);

    hPtr = Tcl_FindHashEntry(&tree->itemHash,
	    (char *) TreeItem_GetID(tree, item));
    Tcl_DeleteHashEntry(hPtr);
    tree->itemCount--;
    if (tree->itemCount == 1)
	tree->nextItemId = TreeItem_GetID(tree, tree->root) + 1;
}

/* Called when Tk_Image is deleted or modified */
static void ImageChangedProc(
    ClientData clientData,
    int x, int y,
    int width, int height,
    int imageWidth, int imageHeight)
{
    /* I would like to know the image was deleted... */
    TreeCtrl *tree = (TreeCtrl *) clientData;

    Tree_DInfoChanged(tree, DINFO_INVALIDATE | DINFO_OUT_OF_DATE);
}

Tk_Image Tree_GetImage(TreeCtrl *tree, char *imageName)
{
    Tcl_HashEntry *hPtr;
    Tk_Image image;
    int isNew;

    hPtr = Tcl_CreateHashEntry(&tree->imageHash, imageName, &isNew);
    if (isNew) {
	image = Tk_GetImage(tree->interp, tree->tkwin, imageName,
		ImageChangedProc, (ClientData) tree);
	if (image == NULL) {
	    Tcl_DeleteHashEntry(hPtr);
	    return NULL;
	}
	Tcl_SetHashValue(hPtr, image);
    }
    return (Tk_Image) Tcl_GetHashValue(hPtr);
}

int Tree_StateFromObj(TreeCtrl *tree, Tcl_Obj *obj, int states[3], int *indexPtr, int flags)
{
    Tcl_Interp *interp = tree->interp;
    int i, op = STATE_OP_ON, op2, op3, length, state = 0;
    char ch0, *string;

    string = Tcl_GetStringFromObj(obj, &length);
    if (length == 0)
	goto unknown;
    ch0 = string[0];
    if (ch0 == '!') {
	if (flags & SFO_NOT_OFF) {
	    FormatResult(interp, "can't specify '!' for this command");
	    return TCL_ERROR;
	}
	op = STATE_OP_OFF;
	++string;
	ch0 = string[0];
    } else if (ch0 == '~') {
	if (flags & SFO_NOT_TOGGLE) {
	    FormatResult(interp, "can't specify '~' for this command");
	    return TCL_ERROR;
	}
	op = STATE_OP_TOGGLE;
	++string;
	ch0 = string[0];
    }
    for (i = 0; i < 32; i++) {
	if (tree->stateNames[i] == NULL)
	    continue;
	if ((ch0 == tree->stateNames[i][0]) &&
		(strcmp(string, tree->stateNames[i]) == 0)) {
	    if ((i < STATE_USER - 1) && (flags & SFO_NOT_STATIC)) {
		FormatResult(interp,
			"can't specify state \"%s\" for this command",
			tree->stateNames[i]);
		return TCL_ERROR;
	    }
	    state = 1L << i;
	    break;
	}
    }
    if (state == 0)
	goto unknown;

    if (states != NULL) {
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
    }
    if (indexPtr != NULL) (*indexPtr) = i;
    return TCL_OK;

unknown:
    FormatResult(interp, "unknown state \"%s\"", string);
    return TCL_ERROR;
}

static int TreeStateCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[])
{
    Tcl_Interp *interp = tree->interp;
    static CONST char *commandName[] = {
	"define", "linkage", "names",  "undefine", (char *) NULL
    };
    enum {
	COMMAND_DEFINE, COMMAND_LINKAGE, COMMAND_NAMES, COMMAND_UNDEFINE
    };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandName, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case COMMAND_DEFINE:
	{
	    char *string;
	    int i, length, slot = -1;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "stateName");
		return TCL_ERROR;
	    }
	    string = Tcl_GetStringFromObj(objv[3], &length);
	    if (!length || (*string == '~') || (*string == '!')) {
		FormatResult(interp, "invalid state name \"%s\"", string);
		return TCL_ERROR;
	    }
	    for (i = 0; i < 32; i++) {
		if (tree->stateNames[i] == NULL) {
		    if (slot == -1)
			slot = i;
		    continue;
		}
		if (strcmp(tree->stateNames[i], string) == 0) {
		    FormatResult(interp, "state \"%s\" already defined", string);
		    return TCL_ERROR;
		}
	    }
	    if (slot == -1) {
		FormatResult(interp, "cannot define any more states");
		return TCL_ERROR;
	    }
	    tree->stateNames[slot] = ckalloc(length + 1);
	    strcpy(tree->stateNames[slot], string);
	    break;
	}

	case COMMAND_LINKAGE:
	{
	    int index;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "state");
		return TCL_ERROR;
	    }
	    if (Tree_StateFromObj(tree, objv[3], NULL, &index,
		    SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		(index < STATE_USER - 1) ? "static" : "dynamic", -1));
	    break;
	}

	case COMMAND_NAMES:
	{
	    Tcl_Obj *listObj;
	    int i;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    for (i = STATE_USER - 1; i < 32; i++) {
		if (tree->stateNames[i] != NULL)
		    Tcl_ListObjAppendElement(interp, listObj,
			    Tcl_NewStringObj(tree->stateNames[i], -1));
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	case COMMAND_UNDEFINE:
	{
	    int i, index;

	    for (i = 3; i < objc; i++) {
		if (Tree_StateFromObj(tree, objv[i], NULL, &index,
			SFO_NOT_STATIC | SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
		    return TCL_ERROR;
		Tree_UndefineState(tree, 1L << index);
		PerStateInfo_Undefine(tree, &pstBitmap, &tree->buttonBitmap,
			1L << index);
		PerStateInfo_Undefine(tree, &pstImage, &tree->buttonImage,
			1L << index);
		ckfree(tree->stateNames[index]);
		tree->stateNames[index] = NULL;
	    }
	    break;
	}
    }

    return TCL_OK;
}

void Tree_AddToSelection(TreeCtrl *tree, TreeItem item)
{
    Tcl_HashEntry *hPtr;
    int isNew;

#ifdef SELECTION_VISIBLE
    if (!TreeItem_ReallyVisible(tree, item))
	panic("Tree_AddToSelection: item %d not ReallyVisible",
		TreeItem_GetID(tree, item));
#endif
    if (TreeItem_GetSelected(tree, item))
	panic("Tree_AddToSelection: item %d already selected",
		TreeItem_GetID(tree, item));
    TreeItem_ChangeState(tree, item, 0, STATE_SELECTED);
    hPtr = Tcl_CreateHashEntry(&tree->selection, (char *) item, &isNew);
    if (!isNew)
	panic("Tree_AddToSelection: item %d already in selection hash table",
		TreeItem_GetID(tree, item));
    tree->selectCount++;
}

void Tree_RemoveFromSelection(TreeCtrl *tree, TreeItem item)
{
    Tcl_HashEntry *hPtr;

    if (!TreeItem_GetSelected(tree, item))
	panic("Tree_RemoveFromSelection: item %d isn't selected",
		TreeItem_GetID(tree, item));
    TreeItem_ChangeState(tree, item, STATE_SELECTED, 0);
    hPtr = Tcl_FindHashEntry(&tree->selection, (char *) item);
    if (hPtr == NULL)
	panic("Tree_RemoveFromSelection: item %d not found in selection hash table",
		TreeItem_GetID(tree, item));
    Tcl_DeleteHashEntry(hPtr);
    tree->selectCount--;
}

static int TreeSelectionCmd(Tcl_Interp *interp,
	TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[])
{
    static CONST char *commandName[] = {
	"add", "anchor", "clear", "count", "get", "includes", "modify", NULL
    };
    enum {
	COMMAND_ADD, COMMAND_ANCHOR, COMMAND_CLEAR, COMMAND_COUNT,
	COMMAND_GET, COMMAND_INCLUDES, COMMAND_MODIFY
    };
    int index;
    TreeItem item, itemFirst, itemLast;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandName, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case COMMAND_ADD:
	{
	    int indexFirst, indexLast, count;
	    TreeItem staticItems[STATIC_SIZE], *items = staticItems;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "first ?last?");
		return TCL_ERROR;
	    }
	    if (TreeItem_FromObj(tree, objv[3], &itemFirst, IFO_ALLOK) != TCL_OK)
		return TCL_ERROR;
	    itemLast = NULL;
	    if (objc == 5) {
		if (TreeItem_FromObj(tree, objv[4], &itemLast, IFO_ALLOK) != TCL_OK)
		    return TCL_ERROR;
	    }
	    if ((itemFirst == ITEM_ALL) || (itemLast == ITEM_ALL)) {
		count = tree->itemCount - tree->selectCount;
		STATIC_ALLOC(items, TreeItem, count + 1);
		count = 0;

		/* Include detached items */
		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashValue(hPtr);
#ifdef SELECTION_VISIBLE
		    if (!TreeItem_GetSelected(tree, item) &&
			    TreeItem_ReallyVisible(tree, item)) {
#else
		    if (!TreeItem_GetSelected(tree, item)) {
#endif
			Tree_AddToSelection(tree, item);
			items[count++] = item;
		    }
		    hPtr = Tcl_NextHashEntry(&search);
		}
		goto doneADD;
	    }
	    if (objc == 4) {
		itemLast = itemFirst;
		count = 1;
	    } else {
		if (TreeItem_RootAncestor(tree, itemFirst) !=
			TreeItem_RootAncestor(tree, itemLast)) {
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

		    index = indexFirst;
		    indexFirst = indexLast;
		    indexLast = index;
		}
		count = indexLast - indexFirst + 1;
	    }
	    STATIC_ALLOC(items, TreeItem, count + 1);
	    count = 0;
	    item = itemFirst;
	    while (item != NULL) {
#ifdef SELECTION_VISIBLE
		if (!TreeItem_GetSelected(tree, item) &&
			TreeItem_ReallyVisible(tree, item)) {
#else
		if (!TreeItem_GetSelected(tree, item)) {
#endif
		    Tree_AddToSelection(tree, item);
		    items[count++] = item;
		}
		if (item == itemLast)
		    break;
		item = TreeItem_Next(tree, item);
	    }
doneADD:
	    if (count) {
		items[count] = NULL;
		TreeNotify_Selection(tree, items, NULL);
	    }
	    STATIC_FREE2(items, staticItems);
	    break;
	}

	case COMMAND_ANCHOR:
	{
	    if (objc != 3 && objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "?index?");
		return TCL_ERROR;
	    }
	    if (objc == 4) {
		if (TreeItem_FromObj(tree, objv[3], &item, 0) != TCL_OK) {
		    return TCL_ERROR;
		}
		tree->anchorItem = item;
	    }
	    Tcl_SetObjResult(interp, TreeItem_ToObj(tree, tree->anchorItem));
	    break;
	}

	case COMMAND_CLEAR:
	{
	    int indexFirst, indexLast, count;
	    TreeItem staticItems[STATIC_SIZE], *items = staticItems;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

	    if (objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "?first? ?last?");
		return TCL_ERROR;
	    }
	    itemFirst = itemLast = NULL;
	    if (objc >= 4) {
		if (TreeItem_FromObj(tree, objv[3], &itemFirst, IFO_ALLOK) != TCL_OK)
		    return TCL_ERROR;
	    }
	    if (objc == 5) {
		if (TreeItem_FromObj(tree, objv[4], &itemLast, IFO_ALLOK) != TCL_OK)
		    return TCL_ERROR;
	    }
	    if (tree->selectCount < 1)
		break;
	    if ((objc == 3) || (itemFirst == ITEM_ALL) || (itemLast == ITEM_ALL)) {
		count = tree->selectCount;
		STATIC_ALLOC(items, TreeItem, count + 1);
		count = 0;
#if 1
		hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		    items[count++] = item;
		    hPtr = Tcl_NextHashEntry(&search);
		}
		for (index = 0; index < count; index++)
		    Tree_RemoveFromSelection(tree, items[index]);
#else
		while (tree->selectCount) {
		    hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
		    item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		    Tree_RemoveFromSelection(tree, item);
		    items[count++] = item;
		}
#endif
		goto doneCLEAR;
	    }
	    if (objc == 4) {
		itemLast = itemFirst;
		count = 1;
	    } else {
		if (TreeItem_RootAncestor(tree, itemFirst) !=
			TreeItem_RootAncestor(tree, itemLast)) {
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

		    index = indexFirst;
		    indexFirst = indexLast;
		    indexLast = index;
		}
		count = indexLast - indexFirst + 1;
	    }
	    STATIC_ALLOC(items, TreeItem, count + 1);
	    count = 0;
	    item = itemFirst;
	    while (item != NULL) {
		if (TreeItem_GetSelected(tree, item)) {
		    Tree_RemoveFromSelection(tree, item);
		    items[count++] = item;
		}
		if (item == itemLast)
		    break;
		item = TreeItem_Next(tree, item);
	    }
doneCLEAR:
	    if (count) {
		items[count] = NULL;
		TreeNotify_Selection(tree, NULL, items);
	    }
	    STATIC_FREE2(items, staticItems);
	    break;
	}

	case COMMAND_COUNT:
	{
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->selectCount));
	    break;
	}

	case COMMAND_GET:
	{
	    TreeItem item;
	    Tcl_Obj *listObj;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    if (tree->selectCount < 1)
		break;
	    listObj = Tcl_NewListObj(0, NULL);
	    hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		Tcl_ListObjAppendElement(interp, listObj,
			TreeItem_ToObj(tree, item));
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	case COMMAND_INCLUDES:
	{
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "index");
		return TCL_ERROR;
	    }
	    if (TreeItem_FromObj(tree, objv[3], &item, 0) != TCL_OK)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
		TreeItem_GetSelected(tree, item)));
	    break;
	}

	case COMMAND_MODIFY:
	{
	    int i, j, count, objcS, objcD;
	    Tcl_Obj **objvS, **objvD;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;
	    TreeItem item;
	    TreeItem staticItemS[STATIC_SIZE], *itemS = staticItemS;
	    TreeItem staticItemD[STATIC_SIZE], *itemD = staticItemD;
	    TreeItem staticNewS[STATIC_SIZE], *newS = staticNewS;
	    TreeItem staticNewD[STATIC_SIZE], *newD = staticNewD;
	    int allS = FALSE, allD = FALSE;
	    int countS, countD;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "select deselect");
		return TCL_ERROR;
	    }
	    if (Tcl_ListObjGetElements(interp, objv[3], &objcS, &objvS) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_ListObjGetElements(interp, objv[4], &objcD, &objvD) != TCL_OK)
		return TCL_ERROR;

	    /* No change */
	    if (!objcS && !objcD)
		break;

	    /* List of items to select */
	    if (objcS > 0) {
		STATIC_ALLOC(itemS, TreeItem, objcS);
		for (i = 0; i < objcS; i++) {
		    if (TreeItem_FromObj(tree, objvS[i], &itemS[i], IFO_ALLOK) != TCL_OK) {
			STATIC_FREE2(itemS, staticItemS);
			return TCL_ERROR;
		    }
		    if (itemS[i] == ITEM_ALL) {
			STATIC_FREE2(itemS, staticItemS);
			allS = TRUE;
			break;
		    }
		}
	    }

	    /* Select all */
	    if (allS) {
		count = tree->itemCount - tree->selectCount;
		STATIC_ALLOC(newS, TreeItem, count + 1);
		count = 0;
#ifdef SELECTION_VISIBLE
		item = tree->root;
		if (!TreeItem_ReallyVisible(tree, item))
		    item = TreeItem_NextVisible(tree, item);
		while (item != NULL) {
		    if (!TreeItem_GetSelected(tree, item)) {
			Tree_AddToSelection(tree, item);
			newS[count++] = item;
		    }
		    item = TreeItem_NextVisible(tree, item);
		}
#else
		/* Include detached items */
		hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashValue(hPtr);
		    if (!TreeItem_GetSelected(tree, item)) {
			Tree_AddToSelection(tree, item);
			newS[count++] = item;
		    }
		    hPtr = Tcl_NextHashEntry(&search);
		}
#endif
		if (count) {
		    newS[count] = NULL;
		    TreeNotify_Selection(tree, newS, NULL);
		}
		STATIC_FREE2(newS, staticNewS);
		break;
	    }

	    /* Select some, deselect none */
	    if ((objcS > 0) && (objcD == 0)) {
		STATIC_ALLOC(newS, TreeItem, objcS + 1);
		count = 0;
		for (i = 0; i < objcS; i++) {
		    item = itemS[i];
		    if (TreeItem_GetSelected(tree, item))
			continue;
#ifdef SELECTION_VISIBLE
		    if (!TreeItem_ReallyVisible(tree, item))
			continue;
#endif
		    /* Add unique item to newly-selected list */
		    for (j = 0; j < count; j++)
			if (newS[j] == item)
			    break;
		    if (j == count) {
			Tree_AddToSelection(tree, item);
			newS[count++] = item;
		    }
		}
		if (count) {
		    newS[count] = NULL;
		    TreeNotify_Selection(tree, newS, NULL);
		}
		STATIC_FREE2(newS, staticNewS);
		STATIC_FREE2(itemS, staticItemS);
		break;
	    }

	    /* Nothing to deselect */
	    if (objcD == 0)
		break;

	    /* List of items to deselect */
	    STATIC_ALLOC(itemD, TreeItem, objcD);
	    for (i = 0; i < objcD; i++) {
		if (TreeItem_FromObj(tree, objvD[i], &itemD[i], IFO_ALLOK) != TCL_OK) {
		    STATIC_FREE2(itemS, staticItemS);
		    STATIC_FREE2(itemD, staticItemD);
		    return TCL_ERROR;
		}
		if (itemD[i] == ITEM_ALL) {
		    allD = TRUE;
		    break;
		}
	    }

	    /* Select none, Deselect all */
	    if ((objcS == 0) && allD) {
		count = tree->selectCount;
		STATIC_ALLOC(newD, TreeItem, count + 1);
		count = 0;
		hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
		while (hPtr != NULL) {
		    item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		    newD[count++] = item;
		    hPtr = Tcl_NextHashEntry(&search);
		}
		for (i = 0; i < count; i++)
		    Tree_RemoveFromSelection(tree, newD[i]);
		if (count) {
		    newD[count] = NULL;
		    TreeNotify_Selection(tree, NULL, newD);
		}
		STATIC_FREE2(newD, staticNewD);
		break;
	    }

	    /* Select none, deselect some */
	    if ((objcS == 0) && !allD) {
		STATIC_ALLOC(newD, TreeItem, objcD + 1);
		count = 0;
		for (i = 0; i < objcD; i++) {
		    item = itemD[i];
		    if (!TreeItem_GetSelected(tree, item))
			continue;
		    /* Add unique item to newly-deselected list */
		    for (j = 0; j < count; j++)
			if (newD[j] == item)
			    break;
		    if (j == count) {
			Tree_RemoveFromSelection(tree, item);
			newD[count++] = item;
		    }
		}
		if (count) {
		    newD[count] = NULL;
		    TreeNotify_Selection(tree, NULL, newD);
		}
		STATIC_FREE2(newD, staticNewD);
		STATIC_FREE2(itemD, staticItemD);
		break;
	    }

	    /* Select some, deselect some */
	    if ((objcS > 0) && !allD) {
		STATIC_ALLOC(newS, TreeItem, objcS + 1);
		STATIC_ALLOC(newD, TreeItem, objcD + 1);
		countS = 0;
		for (i = 0; i < objcS; i++) {
		    item = itemS[i];
		    if (TreeItem_GetSelected(tree, item))
			continue;
#ifdef SELECTION_VISIBLE
		    if (!TreeItem_ReallyVisible(tree, item))
			continue;
#endif
		    /* Add unique item to newly-selected list */
		    for (j = 0; j < countS; j++)
			if (newS[j] == item)
			    break;
		    if (j == countS) {
			Tree_AddToSelection(tree, item);
			newS[countS++] = item;
		    }
		}
		countD = 0;
		for (i = 0; i < objcD; i++) {
		    item = itemD[i];
		    if (!TreeItem_GetSelected(tree, item))
			continue;
		    /* Don't deselect an item in the selected list */
		    for (j = 0; j < objcS; j++)
			if (item == itemS[j])
			    break;
		    if (j != objcS)
			continue;
		    /* Add unique item to newly-deselected list */
		    for (j = 0; j < countD; j++)
			if (newD[j] == item)
			    break;
		    if (j == countD) {
			Tree_RemoveFromSelection(tree, item);
			newD[countD++] = item;
		    }
		}
		if (countS || countD) {
		    newS[countS] = NULL;
		    newD[countD] = NULL;
		    TreeNotify_Selection(tree, newS, newD);
		}
		STATIC_FREE2(newS, staticNewS);
		STATIC_FREE2(itemS, staticItemS);
		STATIC_FREE2(newD, staticNewD);
		STATIC_FREE2(itemD, staticItemD);
		break;
	    }

	    /* Select some, deselect all */
	    countD = tree->selectCount;
	    STATIC_ALLOC(newD, TreeItem, countD + 1);
	    countD = 0;
	    hPtr = Tcl_FirstHashEntry(&tree->selection, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashKey(&tree->selection, hPtr);
		/* Don't deselect an item in the selected list */
		for (j = 0; j < objcS; j++)
		    if (item == itemS[j])
			break;
		if (j == objcS) {
		    newD[countD++] = item;
		}
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    for (i = 0; i < countD; i++)
		Tree_RemoveFromSelection(tree, newD[i]);
	    STATIC_ALLOC(newS, TreeItem, objcS + 1);
	    countS = 0;
	    for (i = 0; i < objcS; i++) {
		item = itemS[i];
		if (TreeItem_GetSelected(tree, item))
		    continue;
#ifdef SELECTION_VISIBLE
		if (!TreeItem_ReallyVisible(tree, item))
		    continue;
#endif
		/* Add unique item to newly-selected list */
		for (j = 0; j < countS; j++)
		    if (newS[j] == item)
			break;
		if (j == countS) {
		    Tree_AddToSelection(tree, item);
		    newS[countS++] = item;
		}
	    }
	    if (countS || countD) {
		newS[countS] = NULL;
		newD[countD] = NULL;
		TreeNotify_Selection(tree, newS, newD);
	    }
	    STATIC_FREE2(newS, staticNewS);
	    STATIC_FREE2(itemS, staticItemS);
	    STATIC_FREE2(newD, staticNewD);
	    STATIC_FREE2(itemD, staticItemD);
	    break;
	}
    }

    return TCL_OK;
}

static int A_XviewCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[])
{
    Tcl_Interp *interp = tree->interp;

    if (objc == 2) {
	double fractions[2];
	char buf[TCL_DOUBLE_SPACE * 2];

	Tree_GetScrollFractionsX(tree, fractions);
	sprintf(buf, "%g %g", fractions[0], fractions[1]);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
    } else {
	int count, index = 0, indexMax, offset, type;
	double fraction;
	int visWidth = Tk_Width(tree->tkwin) - 2 * tree->inset;
	int totWidth = Tree_TotalWidth(tree);
	int xIncr = tree->xScrollIncrement;

	if (visWidth < 0)
	    visWidth = 0;
	if (totWidth <= visWidth)
	    return TCL_OK;

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
		offset = tree->inset + tree->xOrigin;
		index = offset / xIncr;
		index += count;
		break;
	}

	/* Don't scroll too far left */
	if (index < 0)
	    index = 0;

	/* Don't scroll too far right */
	if (index > indexMax)
	    index = indexMax;

	offset = Increment_ToOffsetX(tree, index);
	if (offset - tree->inset != tree->xOrigin) {
	    tree->xOrigin = offset - tree->inset;
	    Tree_EventuallyRedraw(tree);
	}
    }
    return TCL_OK;
}

static int A_YviewCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[])
{
    Tcl_Interp *interp = tree->interp;

    if (objc == 2) {
	double fractions[2];
	char buf[TCL_DOUBLE_SPACE * 2];

	Tree_GetScrollFractionsY(tree, fractions);
	sprintf(buf, "%g %g", fractions[0], fractions[1]);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
    } else {
	int count, index = 0, indexMax, offset, type;
	double fraction;
	int topInset = tree->inset + Tree_HeaderHeight(tree);
	int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;
	int totHeight = Tree_TotalHeight(tree);
	int yIncr = tree->yScrollIncrement;

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
	} else {
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
		offset = topInset + tree->yOrigin;
		index = offset / yIncr;
		index += count;
		break;
	}

	/* Don't scroll too far left */
	if (index < 0)
	    index = 0;

	/* Don't scroll too far right */
	if (index > indexMax)
	    index = indexMax;

	offset = Increment_ToOffsetY(tree, index);
	if (offset - topInset != tree->yOrigin) {
	    tree->yOrigin = offset - topInset;
	    Tree_EventuallyRedraw(tree);
	}
    }
    return TCL_OK;
}

static int TreeXviewCmd(Tcl_Interp *interp, TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[])
{
    if (tree->xScrollIncrement <= 0)
	return B_XviewCmd(tree, objc, objv);
    return A_XviewCmd(tree, objc, objv);
}

static int TreeYviewCmd(Tcl_Interp *interp, TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[])
{
    if (tree->yScrollIncrement <= 0)
	return B_YviewCmd(tree, objc, objv);
    return A_YviewCmd(tree, objc, objv);
}

void Tree_Debug(TreeCtrl *tree)
{
    if (TreeItem_Debug(tree, tree->root) != TCL_OK) {
	dbwin("Tree_Debug: %s\n", Tcl_GetStringResult(tree->interp));
	Tcl_BackgroundError(tree->interp);
    }
}

static int TreeDebugCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = { "cget", "configure", "dinfo",
					  "scroll", (char *) NULL };
    enum { COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_DINFO, COMMAND_SCROLL };
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
	/* T debug cget option */
	case COMMAND_CGET:
	{
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) tree,
		    tree->debug.optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T debug configure ?option? ?value? ?option value ...? */
	case COMMAND_CONFIGURE:
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
			tree->debug.optionTable,
			(objc == 3) ? (Tcl_Obj *) NULL : objv[3],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    result = Tk_SetOptions(interp, (char *) tree,
		    tree->debug.optionTable, objc - 3, objv + 3, tree->tkwin,
		    &savedOptions, &mask);
	    if (result != TCL_OK) {
		Tk_RestoreSavedOptions(&savedOptions);
		return TCL_ERROR;
	    }
	    Tk_FreeSavedOptions(&savedOptions);
	    if (tree->debug.eraseColor != NULL) {
		tree->debug.gcErase = Tk_GCForColor(tree->debug.eraseColor,
			Tk_WindowId(tree->tkwin));
	    }
	    break;
	}

	case COMMAND_DINFO:
	{
	    extern void DumpDInfo(TreeCtrl *tree);
	    DumpDInfo(tree);
	    break;
	}

	case COMMAND_SCROLL:
	{
	    int topInset = Tree_HeaderHeight(tree) + tree->inset;
	    int visHeight = Tk_Height(tree->tkwin) - topInset - tree->inset;
	    int totHeight = Tree_TotalHeight(tree);
	    int yIncr = tree->yScrollIncrement;
	    if (yIncr <= 0)
		yIncr = tree->itemHeight;
	    if (yIncr <= 0)
		yIncr = 1;
	    FormatResult(interp, "visHeight %d totHeight %d visHeight %% yIncr %d totHeight %% yIncr %d",
		    visHeight,
		    totHeight,
		    visHeight % yIncr,
		    totHeight % yIncr
		);
	    break;
	}
    }

    return TCL_OK;
}

/*
textlayout $font $text
	-width pixels
	-wrap word|char
	-justify left|center|right
	-ignoretabs boolean
	-ignorenewlines boolean
*/
int TextLayoutCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    Tk_Font tkfont;
    Tk_Window tkwin = Tk_MainWindow(interp);
    char *text;
    int flags = 0;
    Tk_Justify justify = TK_JUSTIFY_LEFT;
    Tk_TextLayout layout;
    int width = 0, height;
    int result = TCL_OK;
    int i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "font text ?options ...?");
	return TCL_ERROR;
    }

    tkfont = Tk_AllocFontFromObj(interp, tkwin, objv[1]);
    if (tkfont == NULL)
	return TCL_ERROR;
    text = Tcl_GetString(objv[2]);

    for (i = 3; i < objc; i += 2) {
	static CONST char *optionNames[] = {
	    "-ignoretabs", "-ignorenewlines",
	    "-justify", "-width", (char *) NULL
	};
	enum { OPT_IGNORETABS, OPT_IGNORENEWLINES, OPT_JUSTIFY, OPT_WIDTH };
	int index;

	if (Tcl_GetIndexFromObj(interp, objv[i], optionNames, "option", 0,
		&index) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}

	if (i + 1 == objc) {
	    FormatResult(interp, "missing value for \"%s\" option",
		    optionNames[index]);
	    goto done;
	}

	switch (index) {
	    case OPT_IGNORENEWLINES:
	    {
		int v;
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &v) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		if (v)
		    flags |= TK_IGNORE_NEWLINES;
		else
		    flags &= ~TK_IGNORE_NEWLINES;
		break;
	    }
	    case OPT_IGNORETABS:
	    {
		int v;
		if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &v) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		if (v)
		    flags |= TK_IGNORE_TABS;
		else
		    flags &= ~TK_IGNORE_TABS;
		break;
	    }
	    case OPT_JUSTIFY:
	    {
		if (Tk_GetJustifyFromObj(interp, objv[i + 1], &justify) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		break;
	    }
	    case OPT_WIDTH:
	    {
		if (Tk_GetPixelsFromObj(interp, tkwin, objv[i + 1], &width) != TCL_OK) {
		    result = TCL_ERROR;
		    goto done;
		}
		break;
	    }
	}
    }

    layout = Tk_ComputeTextLayout(tkfont, text, -1, width, justify, flags,
	    &width, &height);
    FormatResult(interp, "%d %d", width, height);
    Tk_FreeTextLayout(layout);

done:
    Tk_FreeFont(tkfont);
    return result;
}

int ImageTintCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    char *imageName;
    Tk_PhotoHandle photoH;
    Tk_PhotoImageBlock photoBlock;
    XColor *xColor;
    unsigned char *pixelPtr, *photoPix;
    int x, y, alpha, imgW, imgH, pitch;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "imageName color alpha");
	return TCL_ERROR;
    }

    imageName = Tcl_GetStringFromObj(objv[1], NULL);
    photoH = Tk_FindPhoto(interp, imageName);
    if (photoH == NULL) {
	Tcl_AppendResult(interp, "image \"", imageName,
		"\" doesn't exist or is not a photo image",
		(char *) NULL);
	return TCL_ERROR;
    }

    xColor = Tk_AllocColorFromObj(interp, Tk_MainWindow(interp), objv[2]);
    if (xColor == NULL)
	return TCL_ERROR;

    if (Tcl_GetIntFromObj(interp, objv[3], &alpha) != TCL_OK)
	return TCL_ERROR;
    if (alpha < 0)
	alpha = 0;
    if (alpha > 255)
	alpha = 255;

    Tk_PhotoGetImage(photoH, &photoBlock);
    photoPix = photoBlock.pixelPtr;
    imgW = photoBlock.width;
    imgH = photoBlock.height;
    pitch = photoBlock.pitch;

    pixelPtr = (unsigned char *) Tcl_Alloc(imgW * 4);
    photoBlock.pixelPtr = pixelPtr;
    photoBlock.width = imgW;
    photoBlock.height = 1;
    photoBlock.pitch = imgW * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;

    for (x = 0; x < imgW; x++) {
	pixelPtr[x*4 + 0] = UCHAR(((double) xColor->red / USHRT_MAX) * 255);
	pixelPtr[x*4 + 1] = UCHAR(((double) xColor->green / USHRT_MAX) * 255);
	pixelPtr[x*4 + 2] = UCHAR(((double) xColor->blue / USHRT_MAX) * 255);
    }
    for (y = 0; y < imgH; y++) {
	for (x = 0; x < imgW; x++) {
	    if (photoPix[x * 4 + 3]) {
		pixelPtr[x * 4 + 3] = alpha;
	    } else {
		pixelPtr[x * 4 + 3] = 0;
	    }
	}
	TK_PHOTOPUTBLOCK(interp, photoH, &photoBlock, 0, y,
		imgW, 1, TK_PHOTO_COMPOSITE_OVERLAY);
#if 1
	photoPix += pitch;
#else
	{
	    Tk_PhotoImageBlock photoBlock;
	    Tk_PhotoGetImage(photoH, &photoBlock);
	    photoPix = photoBlock.pixelPtr;
	    photoPix += photoBlock.pitch * (y + 1);
	}
#endif
    }
    Tcl_Free((char *) photoBlock.pixelPtr);

    return TCL_OK;
}

#if !defined(WIN32) && !defined(MAC_TCL) && !defined(MAC_OSX_TK)

int LoupeCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    Tk_Window tkwin = Tk_MainWindow(interp);
    Display *display = Tk_Display(tkwin);
    int screenNum = Tk_ScreenNumber(tkwin);
    Visual *visual = Tk_Visual(tkwin);
    Window rootWindow = RootWindow(display, screenNum);
    int displayW = DisplayWidth(display, screenNum);
    int displayH = DisplayHeight(display, screenNum);
    char *imageName;
    Tk_PhotoHandle photoH;
    Tk_PhotoImageBlock photoBlock;
    unsigned char *pixelPtr;
    int x, y, w, h, zoom;
    int grabX, grabY, grabW, grabH;
    XImage *ximage;
    int i, ncolors;
    XColor *xcolors;
    unsigned long red_shift, green_shift, blue_shift;
    int separated = 0;

    if (objc != 7) {
	Tcl_WrongNumArgs(interp, 1, objv, "imageName x y w h zoom");
	return TCL_ERROR;
    }

    imageName = Tcl_GetStringFromObj(objv[1], NULL);
    photoH = Tk_FindPhoto(interp, imageName);
    if (photoH == NULL) {
	Tcl_AppendResult(interp, "image \"", imageName,
		"\" doesn't exist or is not a photo image",
		(char *) NULL);
	return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK)
	return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK)
	return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[4], &w) != TCL_OK)
	return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[5], &h) != TCL_OK)
	return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[6], &zoom) != TCL_OK)
	return TCL_ERROR;

    grabX = x - w / zoom / 2;
    grabY = y - h / zoom / 2;
    grabW = w / zoom;
    grabH = h / zoom;
    if (grabW > displayW)
	grabW = displayW;
    if (grabH > displayH)
	grabH = displayH;
    if (grabX < 0)
	grabX = 0;
    if (grabY < 0)
	grabY = 0;
    if (grabX + grabW > displayW)
	grabX = displayW - grabW;
    if (grabY + grabH > displayH)
	grabY = displayH - grabH;
    ximage = XGetImage(display, rootWindow,
	    grabX, grabY, grabW, grabH, AllPlanes, ZPixmap);
    if (ximage == NULL) {
	FormatResult(interp, "XGetImage() failed");
	return TCL_ERROR;
    }

    /* See TkPoscriptImage */

    ncolors = visual->map_entries;
    xcolors = (XColor *) ckalloc(sizeof(XColor) * ncolors);

    if ((visual->class == DirectColor) || (visual->class == TrueColor)) {
	separated = 1;
	red_shift = green_shift = blue_shift = 0;
	while ((0x0001 & (ximage->red_mask >> red_shift)) == 0)
	    red_shift++;
	while ((0x0001 & (ximage->green_mask >> green_shift)) == 0)
	    green_shift++;
	while ((0x0001 & (ximage->blue_mask >> blue_shift)) == 0)
	    blue_shift++;
	for (i = 0; i < ncolors; i++) {
	    xcolors[i].pixel =
		((i << red_shift) & ximage->red_mask) |
		((i << green_shift) & ximage->green_mask) |
		((i << blue_shift) & ximage->blue_mask);
	}
    } else {
	for (i = 0; i < ncolors; i++)
	    xcolors[i].pixel = i;
    }

    XQueryColors(display, Tk_Colormap(tkwin), xcolors, ncolors);

    /* XImage -> Tk_Image */
    pixelPtr = (unsigned char *) Tcl_Alloc(ximage->width * ximage->height * 4);
    photoBlock.pixelPtr  = pixelPtr;
    photoBlock.width     = ximage->width;
    photoBlock.height    = ximage->height;
    photoBlock.pitch     = ximage->width * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;

    for (y = 0; y < ximage->height; y++) {
	for (x = 0; x < ximage->width; x++) {
	    int r, g, b;
	    unsigned long pixel;

	    pixel = XGetPixel(ximage, x, y);
	    if (separated) {
		r = (pixel & ximage->red_mask) >> red_shift;
		g = (pixel & ximage->green_mask) >> green_shift;
		b = (pixel & ximage->blue_mask) >> blue_shift;
		r = ((double) xcolors[r].red / USHRT_MAX) * 255;
		g = ((double) xcolors[g].green / USHRT_MAX) * 255;
		b = ((double) xcolors[b].blue / USHRT_MAX) * 255;
	    } else {
		r = ((double) xcolors[pixel].red / USHRT_MAX) * 255;
		g = ((double) xcolors[pixel].green / USHRT_MAX) * 255;
		b = ((double) xcolors[pixel].blue / USHRT_MAX) * 255;
	    }
	    pixelPtr[y * photoBlock.pitch + x * 4 + 0] = r;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 1] = g;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 2] = b;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 3] = 255;
	}
    }

    TK_PHOTOPUTZOOMEDBLOCK(interp, photoH, &photoBlock, 0, 0, w, h,
	    zoom, zoom, 1, 1, TK_PHOTO_COMPOSITE_SET);

    Tcl_Free((char *) pixelPtr);
    ckfree((char *) xcolors);
    XDestroyImage(ximage);

    return TCL_OK;
}

#endif /* not WIN32 && not MAC_TCL && not MAC_OSX_TK */

/* Taken from tkFont.c */
static void RecomputeWidgets(TkWindow *winPtr)
{
    Tk_ClassWorldChangedProc *proc;

    /* Clomp! Stomp! All over the internals */
    proc = Tk_GetClassProc(winPtr->classProcsPtr, worldChangedProc);
    if (proc == TreeWorldChanged) {
	TreeWorldChanged(winPtr->instanceData);
    }

    for (winPtr = winPtr->childList; winPtr != NULL; winPtr = winPtr->nextPtr) {
	RecomputeWidgets(winPtr);
    }
}

/*
 * Called when the system theme changes.
 */
void Tree_TheWorldHasChanged(Tcl_Interp *interp)
{
    /* Could send a <<ThemeChanged>> event to every window like Tile does. */
    /* Could keep a list of treectrl widgets */
    TkWindow *winPtr = (TkWindow *) Tk_MainWindow(interp);
    RecomputeWidgets(winPtr);
}

static char initScript[] = "if {![llength [info proc ::TreeCtrl::Init]]} {\n\
  namespace eval ::TreeCtrl {}\n\
  proc ::TreeCtrl::Init {} {\n\
    global treectrl_library\n\
    tcl_findLibrary treectrl " PACKAGE_PATCHLEVEL " " PACKAGE_PATCHLEVEL " treectrl.tcl TREECTRL_LIBRARY treectrl_library\n\
  }\n\
}\n\
::TreeCtrl::Init";


DLLEXPORT int Treectrl_Init(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.4", 0) == NULL) {
	return TCL_ERROR;
    }
#endif
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, "8.4", 0) == NULL) {
	return TCL_ERROR;
    }
#endif
    if (TreeElement_Init(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    if (TreeStyle_Init(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    /* We don't care if this fails */
    (void) TreeTheme_Init(interp);

    /* Hack for editing a text Element */
    Tcl_CreateObjCommand(interp, "textlayout", TextLayoutCmd, NULL, NULL);

    /* Hack for colorizing a image (like Win98 explorer) */
    Tcl_CreateObjCommand(interp, "imagetint", ImageTintCmd, NULL, NULL);
#if !defined(WIN32) && !defined(MAC_TCL) && !defined(MAC_OSX_TK)
    /* Screen magnifier to check those dotted lines */
    Tcl_CreateObjCommand(interp, "loupe", LoupeCmd, NULL, NULL);
#endif
    Tcl_CreateObjCommand(interp, "treectrl", TreeObjCmd, NULL, NULL);
    if (Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_PATCHLEVEL) != TCL_OK) {
	return TCL_ERROR;
    }
    return Tcl_EvalEx(interp, initScript, -1, TCL_EVAL_GLOBAL);
}

DLLEXPORT int Treectrl_SafeInit(Tcl_Interp *interp)
{
    return Treectrl_Init(interp);
}

