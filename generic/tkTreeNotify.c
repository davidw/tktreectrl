/* 
 * tkTreeNotify.c --
 *
 *	This module implements "qebind.c" events for treectrl widgets.
 *
 * Copyright (c) 2002-2005 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeNotify.c,v 1.14 2005/09/16 01:29:46 treectrl Exp $
 */

#include "tkTreeCtrl.h"

/* tkMacOSXPort.h should include this, I think, like the other platform
 * tk*Port.h files. Needed for TclFormatInt() */
#if defined(MAC_OSX_TK)
#include "tclInt.h"
#endif

static int EVENT_EXPAND,
    DETAIL_EXPAND_BEFORE,
    DETAIL_EXPAND_AFTER;
static int EVENT_COLLAPSE,
    DETAIL_COLLAPSE_BEFORE,
    DETAIL_COLLAPSE_AFTER;
static int EVENT_SELECTION;
static int EVENT_ACTIVEITEM;
static int EVENT_SCROLL,
    DETAIL_SCROLL_X,
    DETAIL_SCROLL_Y;
static int EVENT_ITEM_DELETE;
static int EVENT_ITEM_VISIBILITY;

/*
 *----------------------------------------------------------------------
 *
 * ExpandItem --
 *
 *	Append an item ID to a dynamic string.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
ExpandItem(
    TreeCtrl *tree,		/* Widget info. */
    int id,			/* Item ID. */
    Tcl_DString *result		/* Gets appended. Caller must initialize. */
    )
{
    if (tree->itemPrefixLen) {
	char buf[10 + TCL_INTEGER_SPACE];
	(void) sprintf(buf, "%s%d", tree->itemPrefix, id);
	Tcl_DStringAppend(result, buf, -1);
    } else
	QE_ExpandNumber(id, result);
}

/*
 *----------------------------------------------------------------------
 *
 * DumpPercents --
 *
 *	Appends a sublist to a dynamic string. The sublist contains
 *	%-char,value pairs. This is to handle the %? substitution.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
DumpPercents(
    QE_ExpandArgs *args,	/* %-substitution args. */
    QE_ExpandProc proc,		/* Function to return value for a given
				 * %-char. */
    CONST char *chars		/* NULL-terminated list of %-chars. */
    )
{
    char which = args->which;
    char buf[2];
    int i;

    buf[1] = '\0';

    Tcl_DStringStartSublist(args->result);
    for (i = 0; chars[i]; i++)
    {
	args->which = chars[i];
	buf[0] = chars[i];
	Tcl_DStringAppendElement(args->result, buf);
	Tcl_DStringAppend(args->result, " ", 1);
	(*proc)(args);
    }
    Tcl_DStringEndSublist(args->result);
    args->which = which;
}

/*
 *----------------------------------------------------------------------
 *
 * Percents_Any --
 *
 *	Append a value for a single %-char to a dynamic string. This
 *	function handles the default %-chars (d,e,P,W,T, and ?) and
 *	calls QE_ExpandUnknown() for any unrecognized %-char.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Percents_Any(
    QE_ExpandArgs *args,	/* %-substitution args. */
    QE_ExpandProc proc,		/* Function to return value for a given
				* %-char. */
    CONST char *chars		/* NULL-terminated list of %-chars. */
    )
{
    struct {
	TreeCtrl *tree;
    } *data = args->clientData;
    char chars2[64];

    switch (args->which)
    {
	case 'd': /* detail */
	    QE_ExpandDetail(args->bindingTable, args->event, args->detail,
		args->result);
	    break;

	case 'e': /* event */
	    QE_ExpandEvent(args->bindingTable, args->event, args->result);
	    break;

	case 'P': /* pattern */
	    QE_ExpandPattern(args->bindingTable, args->event, args->detail, args->result);
	    break;

	case 'W': /* object */
	    QE_ExpandString((char *) args->object, args->result);
	    break;

	case 'T': /* tree */
	    QE_ExpandString(Tk_PathName(data->tree->tkwin), args->result);
	    break;

	case '?':
	    strcpy(chars2, "TWPed");
	    strcat(chars2, chars);
	    DumpPercents(args, proc, chars2);
	    break;

	default:
	    QE_ExpandUnknown(args->which, args->result);
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Percents_Expand --
 *
 *	%-substitution callback for <Expand>.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Percents_Expand(
    QE_ExpandArgs *args		/* %-substitution args. */
    )
{
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	int id;
    } *data = args->clientData;

    switch (args->which)
    {
	case 'I':
	    ExpandItem(data->tree, data->id, args->result);
	    break;

	default:
	    Percents_Any(args, Percents_Expand, "I");
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Percents_ItemVisibility --
 *
 *	%-substitution callback for <ItemVisibility>.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Percents_ItemVisibility(
    QE_ExpandArgs *args		/* %-substitution args. */
    )
{
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	Tcl_HashTable *v;
	Tcl_HashTable *h;
    } *data = args->clientData;
    TreeCtrl *tree = data->tree;
    Tcl_HashTable *table;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;

    switch (args->which)
    {
	case 'v':
	case 'h':
	    table = (args->which == 'v') ? data->v : data->h;
	    Tcl_DStringStartSublist(args->result);
	    hPtr = Tcl_FirstHashEntry(table, &search);
	    while (hPtr != NULL)
	    {
		int id = (int) Tcl_GetHashKey(table, hPtr);
		if (tree->itemPrefixLen) {
		    char buf[10 + TCL_INTEGER_SPACE];
		    (void) sprintf(buf, "%s%d", tree->itemPrefix, id);
		    Tcl_DStringAppendElement(args->result, buf);
		} else {
		    char string[TCL_INTEGER_SPACE];
		    TclFormatInt(string, id);
		    Tcl_DStringAppendElement(args->result, string);
		}
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    Tcl_DStringEndSublist(args->result);
	    break;

	default:
	    Percents_Any(args, Percents_ItemVisibility, "vh");
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Percents_Selection --
 *
 *	%-substitution callback for <Selection>.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Percents_Selection(
    QE_ExpandArgs *args		/* %-substitution args. */
    )
{
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	int *select;
	int *deselect;
	int count;
    } *data = args->clientData;
    TreeCtrl *tree = data->tree;
    int *items = NULL;
    int i = 0;

    switch (args->which)
    {
	case 'c':
	    QE_ExpandNumber(data->count, args->result);
	    break;
	case 'D':
	case 'S':
	    items = (args->which == 'D') ? data->deselect : data->select;
	    if (items == NULL)
	    {
		Tcl_DStringAppend(args->result, "{}", 2);
		break;
	    }
	    Tcl_DStringStartSublist(args->result);
	    while (items[i] != -1)
	    {
		if (tree->itemPrefixLen) {
		    char buf[10 + TCL_INTEGER_SPACE];
		    (void) sprintf(buf, "%s%d", tree->itemPrefix, items[i]);
		    Tcl_DStringAppendElement(args->result, buf);
		} else {
		    char string[TCL_INTEGER_SPACE];
		    TclFormatInt(string, items[i]);
		    Tcl_DStringAppendElement(args->result, string);
		}
		i++;
	    }
	    Tcl_DStringEndSublist(args->result);
	    break;

	default:
	    Percents_Any(args, Percents_Selection, "cSD");
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Percents_ActiveItem --
 *
 *	%-substitution callback for <ActiveItem>.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Percents_ActiveItem(
    QE_ExpandArgs *args		/* %-substitution args. */
    )
{
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	int prev;
	int current;
    } *data = args->clientData;

    switch (args->which)
    {
	case 'c':
	    ExpandItem(data->tree, data->current, args->result);
	    break;

	case 'p':
	    ExpandItem(data->tree, data->prev, args->result);
	    break;

	default:
	    Percents_Any(args, Percents_ActiveItem, "cp");
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Percents_Scroll --
 *
 *	%-substitution callback for <Scroll>.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Percents_Scroll(
    QE_ExpandArgs *args		/* %-substitution args. */
    )
{
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	double lower;
	double upper;
    } *data = args->clientData;

    switch (args->which)
    {
	case 'l':
	    QE_ExpandDouble(data->lower, args->result);
	    break;

	case 'u':
	    QE_ExpandDouble(data->upper, args->result);
	    break;

	default:
	    Percents_Any(args, Percents_Scroll, "lu");
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNotifyCmd --
 *
 *	This procedure is invoked to process the [notify] widget
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
TreeNotifyCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandName[] = { "bind", "configure", "detailnames",
	"eventnames", "generate", "install", "linkage", "unbind", "uninstall",
	(char *) NULL };
    enum {
	COMMAND_BIND, COMMAND_CONFIGURE, COMMAND_DETAILNAMES,
	COMMAND_EVENTNAMES, COMMAND_GENERATE, COMMAND_INSTALL,
	COMMAND_LINKAGE, COMMAND_UNBIND, COMMAND_UNINSTALL
    };
    int index;

    if (objc < 3)
    {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandName, "command", 0,
	&index) != TCL_OK)
    {
	return TCL_ERROR;
    }

    switch (index)
    {
	case COMMAND_BIND:
	{
	    return QE_BindCmd(tree->bindingTable, 2, objc, objv);
	}

	case COMMAND_CONFIGURE:
	{
	    return QE_ConfigureCmd(tree->bindingTable, 2, objc, objv);
	}

	/* T notify detailnames $eventName */
	case COMMAND_DETAILNAMES:
	{
	    char *eventName;

	    if (objc != 4)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, "eventName");
		return TCL_ERROR;
	    }
	    eventName = Tcl_GetString(objv[3]);
	    return QE_GetDetailNames(tree->bindingTable, eventName);
	}

	/* T notify eventnames */
	case COMMAND_EVENTNAMES:
	{
	    if (objc != 3)
	    {
		Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    return QE_GetEventNames(tree->bindingTable);
	}

	case COMMAND_GENERATE:
	{
	    return QE_GenerateCmd(tree->bindingTable, 2, objc, objv);
	}

	case COMMAND_INSTALL:
	{
	    return QE_InstallCmd(tree->bindingTable, 2, objc, objv);
	}

	case COMMAND_LINKAGE:
	{
	    return QE_LinkageCmd(tree->bindingTable, 2, objc, objv);
	}

	case COMMAND_UNBIND:
	{
	    return QE_UnbindCmd(tree->bindingTable, 2, objc, objv);
	}

	case COMMAND_UNINSTALL:
	{
	    return QE_UninstallCmd(tree->bindingTable, 2, objc, objv);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNotify_OpenClose --
 *
 *	Generate an <Expand> or <Collapse> event.
 *
 * Results:
 *	Any scripts bound to the event are evaluated.
 *
 * Side effects:
 *	Whatever binding scripts do.
 *
 *----------------------------------------------------------------------
 */

void
TreeNotify_OpenClose(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem item,		/* Item token. */
    int state,			/* STATE_OPEN or 0 */
    int before			/* TRUE for <xxx-before> event, FALSE for
				 * <xxx-after> event. */
    )
{
    QE_Event event;
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	int id;
    } data;

    data.tree = tree;
    data.id = TreeItem_GetID(tree, item);

    if (state & STATE_OPEN)
    {
	event.type = EVENT_EXPAND;
	event.detail = before ? DETAIL_EXPAND_BEFORE : DETAIL_EXPAND_AFTER;
    }
    else
    {
	event.type = EVENT_COLLAPSE;
	event.detail = before ? DETAIL_COLLAPSE_BEFORE : DETAIL_COLLAPSE_AFTER;
    }
    event.clientData = (ClientData) &data;
    (void) QE_BindEvent(tree->bindingTable, &event);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNotify_Selection --
 *
 *	Generate a <Selection> event.
 *
 * Results:
 *	Any scripts bound to the event are evaluated.
 *
 * Side effects:
 *	Whatever binding scripts do.
 *
 *----------------------------------------------------------------------
 */

void
TreeNotify_Selection(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem select[],		/* NULL-terminated list of item tokens. */
    TreeItem deselect[]		/* NULL-terminated list of item tokens. */
    )
{
    QE_Event event;
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	int *select;
	int *deselect;
	int count;
    } data;
    int staticS[128], staticD[128];
    int i;

    data.tree = tree;

    if (select == NULL)
	data.select = NULL;
    else
    {
	for (i = 0; select[i] != NULL; i++)
	    /* nothing */;
	if (i < sizeof(staticS) / sizeof(staticS[0]))
	    data.select = staticS;
	else
	    data.select = (int *) ckalloc(sizeof(int) * (i + 1));
	for (i = 0; select[i] != NULL; i++)
	    data.select[i] = TreeItem_GetID(tree, select[i]);
	data.select[i] = -1;
    }

    if (deselect == NULL)
	data.deselect = NULL;
    else
    {
	for (i = 0; deselect[i] != NULL; i++)
	    /* nothing */;
	if (i < sizeof(staticD) / sizeof(staticD[0]))
	    data.deselect = staticD;
	else
	    data.deselect = (int *) ckalloc(sizeof(int) * (i + 1));
	for (i = 0; deselect[i] != NULL; i++)
	    data.deselect[i] = TreeItem_GetID(tree, deselect[i]);
	data.deselect[i] = -1;
    }

    data.count = tree->selectCount;

    event.type = EVENT_SELECTION;
    event.detail = 0;
    event.clientData = (ClientData) &data;

    (void) QE_BindEvent(tree->bindingTable, &event);

    if ((select != NULL) && (data.select != staticS))
	ckfree((char *) data.select);
    if ((deselect != NULL) && (data.deselect != staticD))
	ckfree((char *) data.deselect);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNotify_ActiveItem --
 *
 *	Generate an <ActiveItem> event.
 *
 * Results:
 *	Any scripts bound to the event are evaluated.
 *
 * Side effects:
 *	Whatever binding scripts do.
 *
 *----------------------------------------------------------------------
 */

void
TreeNotify_ActiveItem(
    TreeCtrl *tree,		/* Widget info. */
    TreeItem itemPrev,		/* Previous active item. */
    TreeItem itemCur		/* Current active item. */
    )
{
    QE_Event event;
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	int prev;
	int current;
    } data;

    data.tree = tree;
    data.prev = TreeItem_GetID(tree, itemPrev);
    data.current = TreeItem_GetID(tree, itemCur);

    event.type = EVENT_ACTIVEITEM;
    event.detail = 0;
    event.clientData = (ClientData) &data;

    (void) QE_BindEvent(tree->bindingTable, &event);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNotify_Scroll --
 *
 *	Generate a <Scroll> event.
 *
 * Results:
 *	Any scripts bound to the event are evaluated.
 *
 * Side effects:
 *	Whatever binding scripts do.
 *
 *----------------------------------------------------------------------
 */

void
TreeNotify_Scroll(
    TreeCtrl *tree,		/* Widget info. */
    double fractions[2],	/* Fractions suitable for a scrollbar's
				 * [set] command. */
    int vertical		/* TRUE for <Scroll-y>, FALSE for
				 * <Scroll-x>. */
    )
{
    QE_Event event;
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	double lower;
	double upper;
    } data;

    data.tree = tree;
    data.lower = fractions[0];
    data.upper = fractions[1];

    event.type = EVENT_SCROLL;
    event.detail = vertical ? DETAIL_SCROLL_Y : DETAIL_SCROLL_X;
    event.clientData = (ClientData) &data;

    (void) QE_BindEvent(tree->bindingTable, &event);
}

/*
 *----------------------------------------------------------------------
 *
 * Percents_ItemDelete --
 *
 *	%-substitution callback for <ItemDelete>.
 *
 * Results:
 *	DString gets longer.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

static void
Percents_ItemDelete(
    QE_ExpandArgs *args			/* %-substitution args. */
    )
{
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	int count;
	int *itemIds;
    } *data = args->clientData;
    TreeCtrl *tree = data->tree;
    int i;

    switch (args->which)
    {
	case 'i':
	    Tcl_DStringStartSublist(args->result);
	    for (i = 0; i < data->count; i++)
	    {
		if (tree->itemPrefixLen) {
		    char buf[10 + TCL_INTEGER_SPACE];
		    (void) sprintf(buf, "%s%d", tree->itemPrefix, data->itemIds[i]);
		    Tcl_DStringAppendElement(args->result, buf);
		} else {
		    char string[TCL_INTEGER_SPACE];
		    TclFormatInt(string, data->itemIds[i]);
		    Tcl_DStringAppendElement(args->result, string);
		}
	    }
	    Tcl_DStringEndSublist(args->result);
	    break;

	default:
	    Percents_Any(args, Percents_ItemDelete, "i");
	    break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNotify_ItemDeleted --
 *
 *	Generate an <ItemDelete> event.
 *
 * Results:
 *	Any scripts bound to the event are evaluated.
 *
 * Side effects:
 *	Whatever binding scripts do.
 *
 *----------------------------------------------------------------------
 */

void
TreeNotify_ItemDeleted(
    TreeCtrl *tree,		/* Widget info. */
    int itemIds[],		/* Array of item IDs. */
    int count			/* Number of item IDs. */
    )
{
    QE_Event event;
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	int count;
	int *itemIds;
    } data;

    data.tree = tree;
    data.count = count;
    data.itemIds = itemIds;

    event.type = EVENT_ITEM_DELETE;
    event.detail = 0;
    event.clientData = (ClientData) &data;

    (void) QE_BindEvent(tree->bindingTable, &event);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNotify_ItemVisibility --
 *
 *	Generate an <ItemVisibility> event.
 *
 * Results:
 *	Any scripts bound to the event are evaluated.
 *
 * Side effects:
 *	Whatever binding scripts do.
 *
 *----------------------------------------------------------------------
 */

void
TreeNotify_ItemVisibility(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_HashTable *v,		/* Each key is the ID of a newly-visible
				 * item. */
    Tcl_HashTable *h		/* Each key is the ID of a newly-hidden
				 * item. */
    )
{
    QE_Event event;
    struct {
	TreeCtrl *tree; /* Must be first. See Percents_Any(). */
	Tcl_HashTable *v;
	Tcl_HashTable *h;
    } data;

    data.tree = tree;
    data.v = v;
    data.h = h;

    event.type = EVENT_ITEM_VISIBILITY;
    event.detail = 0;
    event.clientData = (ClientData) &data;

    (void) QE_BindEvent(tree->bindingTable, &event);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeNotify_Init --
 *
 *	Perform event-related initialization when a new TreeCtrl is
 *	created.
 *
 * Results:
 *	Installs all the static events and details.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

int
TreeNotify_Init(
    TreeCtrl *tree		/* Widget info. */
    )
{
    tree->bindingTable = QE_CreateBindingTable(tree->interp);

    EVENT_EXPAND = QE_InstallEvent(tree->bindingTable, "Expand", Percents_Expand);
    DETAIL_EXPAND_BEFORE = QE_InstallDetail(tree->bindingTable, "before", EVENT_EXPAND, NULL);
    DETAIL_EXPAND_AFTER = QE_InstallDetail(tree->bindingTable, "after", EVENT_EXPAND, NULL);

    EVENT_COLLAPSE = QE_InstallEvent(tree->bindingTable, "Collapse", Percents_Expand);
    DETAIL_COLLAPSE_BEFORE = QE_InstallDetail(tree->bindingTable, "before", EVENT_COLLAPSE, NULL);
    DETAIL_COLLAPSE_AFTER = QE_InstallDetail(tree->bindingTable, "after", EVENT_COLLAPSE, NULL);

    EVENT_SELECTION = QE_InstallEvent(tree->bindingTable, "Selection", Percents_Selection);

    EVENT_ACTIVEITEM = QE_InstallEvent(tree->bindingTable, "ActiveItem", Percents_ActiveItem);

    EVENT_SCROLL = QE_InstallEvent(tree->bindingTable, "Scroll", Percents_Scroll);
    DETAIL_SCROLL_X = QE_InstallDetail(tree->bindingTable, "x", EVENT_SCROLL, NULL);
    DETAIL_SCROLL_Y = QE_InstallDetail(tree->bindingTable, "y", EVENT_SCROLL, NULL);

    EVENT_ITEM_DELETE = QE_InstallEvent(tree->bindingTable, "ItemDelete", Percents_ItemDelete);

    EVENT_ITEM_VISIBILITY = QE_InstallEvent(tree->bindingTable, "ItemVisibility", Percents_ItemVisibility);

    return TCL_OK;
}

