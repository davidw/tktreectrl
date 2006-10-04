/* 
 * tkTreeRow.c --
 *
 *	This module implements a treectrl's row labels.
 *
 * Copyright (c) 2002-2006 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeRowLabel.c,v 1.1 2006/10/04 03:52:23 treectrl Exp $
 */

#include "tkTreeCtrl.h"

typedef struct RowLabel RowLabel;

/*
 * The following structure holds information about a single
 * row label in a TreeCtrl.
 */
struct RowLabel
{
    int height;			/* -height */
    Tcl_Obj *heightObj;		/* -height */
    int minHeight;		/* -minheight */
    Tcl_Obj *minHeightObj;	/* -minheight */
    int maxHeight;		/* -maxheight */
    Tcl_Obj *maxHeightObj;	/* -maxheight */
    int visible;		/* -visible */
    int resize;			/* -resize */
    Tcl_Obj *styleObj;		/* -style */
    int state;			/* state flags. FIXME: item states have no meaning here */

    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int id;			/* unique identifier */
    int index;			/* order in list of rows */
    TreeStyle style;		/* style */
    int onScreen;		/* TRUE if onscreen. */
    TagInfo *tagInfo;		/* Tags. May be NULL. */
    RowLabel *prev;
    RowLabel *next;
};

#define ROW_CONF_WIDTH		0x0002
#define ROW_CONF_HEIGHT		0x0004
#define ROW_CONF_DISPLAY	0x0040
#define ROW_CONF_STYLE		0x0100
#define ROW_CONF_TAGS		0x0200
#define ROW_CONF_RANGES		0x0800

static Tk_OptionSpec rowSpecs[] = {
    {TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(RowLabel, heightObj), Tk_Offset(RowLabel, height),
     TK_OPTION_NULL_OK, (ClientData) NULL, ROW_CONF_HEIGHT},
    {TK_OPTION_PIXELS, "-maxheight", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(RowLabel, maxHeightObj),
     Tk_Offset(RowLabel, maxHeight),
     TK_OPTION_NULL_OK, (ClientData) NULL, ROW_CONF_HEIGHT},
    {TK_OPTION_PIXELS, "-minheight", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(RowLabel, minHeightObj),
     Tk_Offset(RowLabel, minHeight),
     TK_OPTION_NULL_OK, (ClientData) NULL, ROW_CONF_HEIGHT},
    {TK_OPTION_BOOLEAN, "-resize", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(RowLabel, resize), 0, (ClientData) NULL, 0},
    {TK_OPTION_STRING, "-style", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(RowLabel, styleObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, ROW_CONF_STYLE},
    {TK_OPTION_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(RowLabel, tagInfo),
     TK_OPTION_NULL_OK, (ClientData) &TagInfoCO, ROW_CONF_TAGS},
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(RowLabel, visible),
     0, (ClientData) NULL, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/*
 *----------------------------------------------------------------------
 *
 * RowOptionSet --
 *
 *	Tk_ObjCustomOption.setProc(). Converts a Tcl_Obj holding a
 *	row description into a pointer to a Row.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May store a TreeRowLabel pointer into the internal representation
 *	pointer.  May change the pointer to the Tcl_Obj to NULL to indicate
 *	that the specified string was empty and that is acceptable.
 *
 *----------------------------------------------------------------------
 */

static int
RowOptionSet(
    ClientData clientData,	/* RFO_xxx flags to control the conversion. */
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
    int rfoFlags = (int) clientData;
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    int objEmpty;
    TreeRowLabel new, *internalPtr;

    if (internalOffset >= 0)
	internalPtr = (TreeRowLabel *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	if (TreeRowLabel_FromObj(tree, (*value), &new, rfoFlags) != TCL_OK)
	    return TCL_ERROR;
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL)
	    new = NULL;
	*((TreeRowLabel *) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RowOptionGet --
 *
 *	Tk_ObjCustomOption.getProc(). Converts a TreeRowLabel into a
 *	Tcl_Obj string representation.
 *
 * Results:
 *	Tcl_Obj containing the string representation of the row.
 *	Returns NULL if the TreeRowLabel is NULL.
 *
 * Side effects:
 *	May create a new Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
RowOptionGet(
    ClientData clientData,	/* Not used. */
    Tk_Window tkwin,		/* Window for which option is being set. */
    char *recordPtr,		/* Pointer to widget record. */
    int internalOffset		/* Offset within *recordPtr containing the
				 * sticky value. */
    )
{
    TreeRowLabel value = *(TreeRowLabel *) (recordPtr + internalOffset);
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    if (value == NULL)
	return NULL;
    if (value == ROW_ALL)
	return Tcl_NewStringObj("all", -1);
    return TreeRowLabel_ToObj(tree, value);
}

/*
 *----------------------------------------------------------------------
 *
 * RowOptionRestore --
 *
 *	Tk_ObjCustomOption.restoreProc(). Restores a TreeRowLabel value
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
RowOptionRestore(
    ClientData clientData,	/* Not used. */
    Tk_Window tkwin,		/* Not used. */
    char *internalPtr,		/* Where to store old value. */
    char *saveInternalPtr)	/* Pointer to old value. */
{
    *(TreeRowLabel *) internalPtr = *(TreeRowLabel *) saveInternalPtr;
}

/*
 * The following structure contains pointers to functions used for processing
 * a custom config option that handles Tcl_Obj<->TreeRowLabel conversion.
 * A row description must refer to a valid row. "all" is not allowed.
 */
Tk_ObjCustomOption rowCustomOption =
{
    "row",
    RowOptionSet,
    RowOptionGet,
    RowOptionRestore,
    NULL,
    (ClientData) (RFO_NOT_MANY | RFO_NOT_NULL)
};

static Tk_OptionSpec dragSpecs[] = {
    {TK_OPTION_BOOLEAN, "-enable", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(TreeCtrl, rowDrag.enable),
     0, (ClientData) NULL, 0},
    {TK_OPTION_INT, "-imagealpha", (char *) NULL, (char *) NULL,
     "128", -1, Tk_Offset(TreeCtrl, rowDrag.alpha),
     0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-imagecolor", (char *) NULL, (char *) NULL,
     "gray75", -1, Tk_Offset(TreeCtrl, rowDrag.color),
     0, (ClientData) NULL, 0},
    {TK_OPTION_CUSTOM, "-imagerow", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, rowDrag.row),
     TK_OPTION_NULL_OK, (ClientData) &rowCustomOption, 0},
    {TK_OPTION_PIXELS, "-imageoffset", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(TreeCtrl, rowDrag.offsetObj),
     Tk_Offset(TreeCtrl, rowDrag.offset), 0, (ClientData) NULL, 0},
    {TK_OPTION_COLOR, "-indicatorcolor", (char *) NULL, (char *) NULL,
     "Black", -1, Tk_Offset(TreeCtrl, rowDrag.indColor),
     0, (ClientData) NULL, 0},
    {TK_OPTION_CUSTOM, "-indicatorrow", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(TreeCtrl, rowDrag.indRow),
     TK_OPTION_NULL_OK, (ClientData) &rowCustomOption, 0},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

typedef struct Qualifiers {
    TreeCtrl *tree;
    int visible;		/* 1 for -visible TRUE,
				   0 for -visible FALSE,
				   -1 for unspecified. */
    int states[3];		/* States that must be on or off. */
    TagExpr expr;		/* Tag expression. */
    int exprOK;			/* TRUE if expr is valid. */
} Qualifiers;

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Init --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Qualifiers_Init(
    TreeCtrl *tree,		/* Widget info. */
    Qualifiers *q		/* Out: Initialized qualifiers. */
    )
{
    q->tree = tree;
    q->visible = -1;
    q->states[0] = q->states[1] = q->states[2] = 0;
    q->exprOK = FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Scan --
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
Qualifiers_Scan(
    Qualifiers *q,		/* Must call Qualifiers_Init first,
				 * and Qualifiers_Free if result is TCL_OK. */
    int objc,			/* Number of arguments. */
    Tcl_Obj **objv,		/* Argument values. */
    int startIndex,		/* First objv[] index to look at. */
    int *argsUsed		/* Out: number of objv[] used. */
    )
{
    TreeCtrl *tree = q->tree;
    Tcl_Interp *interp = tree->interp;
    int qual, j = startIndex;

    static CONST char *qualifiers[] = {
	"state", "tag", "visible", "!visible", NULL
    };
    enum qualEnum {
	QUAL_STATE, QUAL_TAG, QUAL_VISIBLE, QUAL_NOT_VISIBLE
    };
    /* Number of arguments used by qualifiers[]. */
    static int qualArgs[] = {
	2, 2, 1, 1
    };

    *argsUsed = 0;

    for (; j < objc; ) {
	if (Tcl_GetIndexFromObj(NULL, objv[j], qualifiers, NULL, 0,
		&qual) != TCL_OK)
	    break;
	if (objc - j < qualArgs[qual]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(objv[j]), "\" qualifier", NULL);
	    goto errorExit;
	}
	switch ((enum qualEnum) qual) {
	    case QUAL_STATE:
	    {
		if (Tree_StateFromListObj(tree, objv[j + 1], q->states,
			SFO_NOT_TOGGLE) != TCL_OK)
		    goto errorExit;
		break;
	    }
	    case QUAL_TAG:
	    {
		if (q->exprOK)
		    TagExpr_Free(&q->expr);
		if (TagExpr_Init(tree, objv[j + 1], &q->expr) != TCL_OK)
		    return TCL_ERROR;
		q->exprOK = TRUE;
		break;
	    }
	    case QUAL_VISIBLE:
	    {
		q->visible = 1;
		break;
	    }
	    case QUAL_NOT_VISIBLE:
	    {
		q->visible = 0;
		break;
	    }
	}
	*argsUsed += qualArgs[qual];
	j += qualArgs[qual];
    }
    return TCL_OK;
errorExit:
    if (q->exprOK)
	TagExpr_Free(&q->expr);
    return TCL_ERROR;
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
    Qualifiers *q,		/* Qualifiers to check. */
    RowLabel *row			/* The row to test. May be NULL. */
    )
{
    /* Note: if the row is NULL it is a "match" because we have run
     * out of row to check. */
    if (row == NULL)
	return 1;
    if ((q->visible == 1) && !row->visible)
	return 0;
    else if ((q->visible == 0) && row->visible)
	return 0;
    if (q->states[STATE_OP_OFF] & row->state)
	return 0;
    if ((q->states[STATE_OP_ON] & row->state) != q->states[STATE_OP_ON])
	return 0;
    if (q->exprOK && !TagExpr_Eval(&q->expr, row->tagInfo))
	return 0;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Qualifiers_Free --
 *
 *	Helper routine for TreeItem_FromObj.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Qualifiers_Free(
    Qualifiers *q		/* Out: Initialized qualifiers. */
    )
{
    if (q->exprOK)
	TagExpr_Free(&q->expr);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabelList_FromObj --
 *
 *	Parse a Tcl_Obj rowlabel description to get a list of rowlabels.
 *
 * ID MODIFIERS
 * TAG QUALIFIERS
 * all QUALIFIERS
 * first QUALIFIERS MODIFIERS
 * end|last QUALIFIERS MODIFIERS
 * list listOfDescs
 * order N QUALIFIERS MODIFIERS
 * range first last QUALIFIERS
 * tag tagExpr QUALFIERS
 *
 * MODIFIERS:
 * next QUALIFIERS
 * prev QUALIFIERS
 *
 * QUALIFIERS:
 * state stateList
 * tag tagExpr
 * visible
 * !visible
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
TreeRowLabelList_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* RowLabel description. */
    TreeRowLabelList *rows,	/* Uninitialized list. Caller must free
				 * it with TreeRowLabelList_Free unless the
				 * result of this function is TCL_ERROR. */
    int flags			/* RFO_xxx flags. */
    )
{
    Tcl_Interp *interp = tree->interp;
    int objc;
    int index, listIndex;
    Tcl_Obj **objv, *elemPtr;
    RowLabel *row = NULL;
    Qualifiers q;
    Tcl_HashEntry *hPtr;

    static CONST char *indexName[] = {
	"all", "end", "first", "last", "list", "order", "range", "tag",
	(char *) NULL
    };
    enum indexEnum {
	INDEX_ALL, INDEX_END, INDEX_FIRST, INDEX_LAST, INDEX_LIST, INDEX_ORDER,
	INDEX_RANGE, INDEX_TAG
    } ;
    /* Number of arguments used by indexName[]. */
    static int indexArgs[] = {
	1, 1, 1, 1, 2, 2, 3, 2
    };
    /* Boolean: can indexName[] be followed by 1 or more qualifiers. */
    static int indexQual[] = {
	1, 1, 1, 1, 1, 1, 1, 1
    };

    static CONST char *modifiers[] = {
	"next", "prev", (char *) NULL
    };
    enum modEnum {
	TMOD_NEXT, TMOD_PREV
    };
    /* Number of arguments used by modifiers[]. */
    static int modArgs[] = {
	1, 1
    };
    /* Boolean: can modifiers[] be followed by 1 or more qualifiers. */
    static int modQual[] = {
	1, 1
    };

    TreeRowLabelList_Init(tree, rows, 0);
    Qualifiers_Init(tree, &q);

    if (Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK)
	goto badDesc;
    if (objc == 0)
	goto badDesc;

    listIndex = 0;
    elemPtr = objv[listIndex];
    if (Tcl_GetIndexFromObj(NULL, elemPtr, indexName, NULL, 0, &index)
	    == TCL_OK) {
	int qualArgsTotal = 0;

	if (objc - listIndex < indexArgs[index]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(elemPtr), "\" keyword", NULL);
	    goto errorExit;
	}
	if (indexQual[index]) {
	    if (Qualifiers_Scan(&q, objc, objv, listIndex + indexArgs[index],
		    &qualArgsTotal) != TCL_OK) {
		goto errorExit;
	    }
	}

	switch ((enum indexEnum) index) {
	    case INDEX_ALL:
	    {
		if (qualArgsTotal) {
		    row = (RowLabel *) tree->rows;
		    while (row != NULL) {
			if (Qualifies(&q, row)) {
			    TreeRowLabelList_Append(rows, (TreeRowLabel) row);
			}
			row = row->next;
		    }
		    row = NULL;
		} else {
		    row = (RowLabel *) ROW_ALL;
		}
		break;
	    }
	    case INDEX_FIRST:
	    {
		row = (RowLabel *) tree->rows;
		while (!Qualifies(&q, row))
		    row = row->next;
		break;
	    }
	    case INDEX_END:
	    case INDEX_LAST:
	    {
		row = (RowLabel *) tree->rowLabelLast;
		while (!Qualifies(&q, row))
		    row = row->prev;
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
		    TreeRowLabelList row2s;
		    if (TreeRowLabelList_FromObj(tree, listObjv[i], &row2s, flags)
			    != TCL_OK)
			goto errorExit;
		    TreeRowLabelList_Concat(rows, &row2s);
		    TreeRowLabelList_Free(&row2s);
		}
		/* If any of the rowlabel descriptions in the list is "all", then
		 * clear the list of rowlabels and use "all". */
		count = TreeRowLabelList_Count(rows);
		for (i = 0; i < count; i++) {
		    TreeRowLabel row = TreeRowLabelList_Nth(rows, i);
		    if (row == ROW_ALL)
			break;
		}
		if (i < count) {
		    TreeRowLabelList_Free(rows);
		    row = (RowLabel *) ROW_ALL;
		} else
		    row = NULL;
		break;
	    }
	    case INDEX_ORDER:
	    {
		int order;

		if (Tcl_GetIntFromObj(NULL, objv[listIndex + 1], &order) != TCL_OK)
		    goto errorExit;
		row = (RowLabel *) tree->rows;
		while (row != NULL) {
		    if (Qualifies(&q, row))
			if (order-- <= 0)
			    break;
		    row = row->next;
		}
		break;
	    }
	    case INDEX_RANGE:
	    {
		TreeRowLabel _first, _last;
		RowLabel *first, *last;

		if (TreeRowLabel_FromObj(tree, objv[listIndex + 1],
			&_first, RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		    goto errorExit;
		first = (RowLabel *) _first;
		if (TreeRowLabel_FromObj(tree, objv[listIndex + 2],
			&_last, RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		    goto errorExit;
		last = (RowLabel *) _last;
		if (first->index > last->index) {
		    row = first;
		    first = last;
		    last = row;
		}
		row = first;
		while (1) {
		    if (Qualifies(&q, row)) {
			TreeRowLabelList_Append(rows, row);
		    }
		    if (row == last)
			break;
		    row = row->next;
		}
		row = NULL;
		break;
	    }
	    case INDEX_TAG:
	    {
		TagExpr expr;

		if (TagExpr_Init(tree, objv[listIndex + 1], &expr) != TCL_OK)
		    goto errorExit;
		row = (RowLabel *) tree->rows;
		while (row != NULL) {
		    if (TagExpr_Eval(&expr, row->tagInfo) &&
			    Qualifies(&q, row)) {
			TreeRowLabelList_Append(rows, (TreeRowLabel) row);
		    }
		    row = row->next;
		}
		TagExpr_Free(&expr);
		row = NULL;
		break;
	    }
	}
	/* If 1 rowlabel, use it and clear the list. */
	if (TreeRowLabelList_Count(rows) == 1) {
	    row = (RowLabel *) TreeRowLabelList_Nth(rows, 0);
	    rows->count = 0;

	}

	/* If > 1 rowlabel, no modifiers may follow. */
	if ((TreeRowLabelList_Count(rows) > 1) || (row == (RowLabel *) ROW_ALL)) {
	    if (listIndex + indexArgs[index] + qualArgsTotal < objc) {
		FormatResult(interp,
		    "unexpected arguments after \"%s\" keyword",
		    indexName[index]);
		goto errorExit;
	    }
	}
	listIndex += indexArgs[index] + qualArgsTotal;
    } else {
	int gotId = FALSE, id = -1;

	if (tree->rowPrefixLen) {
	    char *end, *t = Tcl_GetString(elemPtr);
	    if (strncmp(t, tree->rowPrefix, tree->rowPrefixLen) == 0)
	    {
		t += tree->rowPrefixLen;
		id = strtoul(t, &end, 10);
		if ((end != t) && (*end == '\0'))
		    gotId = TRUE;
	    }

	} else if (Tcl_GetIntFromObj(NULL, elemPtr, &id) == TCL_OK) {
	    gotId = TRUE;
	}
	if (gotId) {
	    hPtr = Tcl_FindHashEntry(&tree->rowIDHash, (char *) id);
	    if (hPtr != NULL) {
		row = (RowLabel *) Tcl_GetHashValue(hPtr);
	    }
	    listIndex++;
	} else {
	    TagExpr expr;
	    int qualArgsTotal = 0;

	    if (objc > 1) {
		if (Qualifiers_Scan(&q, objc, objv, listIndex + 1,
			&qualArgsTotal) != TCL_OK) {
		    goto errorExit;
		}
	    }
	    if (TagExpr_Init(tree, elemPtr, &expr) != TCL_OK)
		goto errorExit;
	    row = (RowLabel *) tree->rows;
	    while (row != NULL) {
		if (TagExpr_Eval(&expr, row->tagInfo) && Qualifies(&q, row)) {
		    TreeRowLabelList_Append(rows, (TreeRowLabel) row);
		}
		row = row->next;
	    }
	    TagExpr_Free(&expr);
	    row = NULL;

	    /* If 1 rowlabel, use it and clear the list. */
	    if (TreeRowLabelList_Count(rows) == 1) {
		row = (RowLabel *) TreeRowLabelList_Nth(rows, 0);
		rows->count = 0;

	    }

	    /* If > 1 rowlabel, no modifiers may follow. */
	    if (TreeRowLabelList_Count(rows) > 1) {
		if (listIndex + 1 + qualArgsTotal < objc) {
		    FormatResult(interp,
			"unexpected arguments after \"%s\"",
			Tcl_GetString(elemPtr));
		    goto errorExit;
		}
	    }

	    listIndex += 1 + qualArgsTotal;
	}
    }

    /* This means a valid specification was given, but there is no such row */
    if ((TreeRowLabelList_Count(rows) == 0) && (row == NULL)) {
	if (flags & RFO_NOT_NULL)
	    goto notNull;
	/* Empty list returned */
	goto goodExit;
    }

    for (; listIndex < objc; /* nothing */) {
	int qualArgsTotal = 0;

	elemPtr = objv[listIndex];
	if (Tcl_GetIndexFromObj(interp, elemPtr, modifiers, "modifier", 0,
		    &index) != TCL_OK)
	    return TCL_ERROR;
	if (objc - listIndex < modArgs[index]) {
	    Tcl_AppendResult(interp, "missing arguments to \"",
		    Tcl_GetString(elemPtr), "\" modifier", NULL);
	    goto errorExit;
	}
	if (modQual[index]) {
	    Qualifiers_Free(&q);
	    Qualifiers_Init(tree, &q);
	    if (Qualifiers_Scan(&q, objc, objv, listIndex + modArgs[index],
		    &qualArgsTotal) != TCL_OK) {
		goto errorExit;
	    }
	}
	switch ((enum modEnum) index) {
	    case TMOD_NEXT:
	    {
		row = row->next;
		while (!Qualifies(&q, row)) {
		    row = row->next;
		}
		break;
	    }
	    case TMOD_PREV:
	    {
		row = row->prev;
		while (!Qualifies(&q, row))
		    row = row->prev;
		break;
	    }
	}
	if ((TreeRowLabelList_Count(rows) == 0) && (row == NULL)) {
	    if (flags & RFO_NOT_NULL)
		goto notNull;
	    /* Empty list returned. */
	    goto goodExit;
	}
	listIndex += modArgs[index] + qualArgsTotal;
    }
    if ((flags & RFO_NOT_MANY) && ((row == (RowLabel *) ROW_ALL) ||
	    (TreeRowLabelList_Count(rows) > 1))) {
	FormatResult(interp, "can't specify > 1 rowlabel for this command");
	goto errorExit;
    }
    if ((flags & RFO_NOT_NULL) && (TreeRowLabelList_Count(rows) == 0) &&
	    (row == NULL)) {
notNull:
	FormatResult(interp, "rowlabel \"%s\" doesn't exist", Tcl_GetString(objPtr));
	goto errorExit;
    }
    if (TreeRowLabelList_Count(rows)) {
    } else if (row == (RowLabel *) ROW_ALL) {
	if ((flags & RFO_NOT_NULL) && (tree->rowCount == 0))
	    goto notNull;
	TreeRowLabelList_Append(rows, ROW_ALL);
    } else {
	TreeRowLabelList_Append(rows, (TreeRowLabel) row);
    }
goodExit:
    Qualifiers_Free(&q);
    return TCL_OK;

badDesc:
    FormatResult(interp, "bad rowlabel description \"%s\"", Tcl_GetString(objPtr));
    goto errorExit;

errorExit:
    Qualifiers_Free(&q);
    TreeRowLabelList_Free(rows);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_FromObj --
 *
 *	Parse a Tcl_Obj column description to get a single column.
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
TreeRowLabel_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,		/* Object to parse to an item. */
    TreeRowLabel *rowPtr,		/* Returned item. */
    int flags			/* RFO_xxx flags */
    )
{
    TreeRowLabelList rows;

    if (TreeRowLabelList_FromObj(tree, objPtr, &rows, flags) != TCL_OK)
	return TCL_ERROR;
    /* May be NULL. */
    (*rowPtr) = TreeRowLabelList_Nth(&rows, 0);
    TreeRowLabelList_Free(&rows);
    return TCL_OK;
}

typedef struct RowForEach RowForEach;
struct RowForEach {
    TreeCtrl *tree;
    int error;
    int all;
    TreeRowLabel current;
    TreeRowLabelList *list;
    int index;
};

#define ROW_FOR_EACH(row, rows, iter) \
    for (row = RowForEach_Start(rows, iter); \
	 row != NULL; \
	 row = RowForEach_Next(iter))

/*
 *----------------------------------------------------------------------
 *
 * RowForEach_Start --
 *
 *	Begin iterating over rowlabels.
 *
 * Results:
 *	Returns the first item to iterate over. If an error occurs
 *	then RowForEach.error is set to 1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeRowLabel
RowForEach_Start(
    TreeRowLabelList *rows,	/* List of rowlabels. */
    RowForEach *iter		/* Returned info, pass to
				   RowForEach_Next. */
    )
{
    TreeCtrl *tree = rows->tree;
    TreeRowLabel row;

    row = TreeRowLabelList_Nth(rows, 0);

    iter->tree = tree;
    iter->all = FALSE;
    iter->error = 0;
    iter->list = NULL;

    if (row == ROW_ALL) {
	iter->all = TRUE;
	return iter->current = tree->rows;
    }

    iter->list = rows;
    iter->index = 0;
    return iter->current = row;
}

/*
 *----------------------------------------------------------------------
 *
 * RowForEach_Next --
 *
 *	Returns the next column to iterate over. Keep calling this until
 *	the result is NULL.
 *
 * Results:
 *	Returns the next column to iterate over or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeRowLabel
RowForEach_Next(
    RowForEach *iter		/* Initialized by RowForEach_Start. */
    )
{
    if (iter->all) {
	return iter->current = TreeRowLabel_Next(iter->current);
    }

    if (iter->index >= TreeRowLabelList_Count(iter->list))
	return iter->current = NULL;
    return iter->current = TreeRowLabelList_Nth(iter->list, ++iter->index);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_ToObj --
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
TreeRowLabel_ToObj(
    TreeCtrl *tree,		/* Widget info. */
    TreeRowLabel row_		/* RowLabel token to get Tcl_Obj for. */
    )
{
    RowLabel *row = (RowLabel *) row_;

    if (tree->rowPrefixLen) {
	char buf[100 + TCL_INTEGER_SPACE];
	(void) sprintf(buf, "%s%d", tree->rowPrefix, row->id);
	return Tcl_NewStringObj(buf, -1);
    }
    return Tcl_NewIntObj(row->id);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FindRow --
 *
 *	Get the N'th row in a TreeCtrl.
 *
 * Results:
 *	Token for the N'th row.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeRowLabel
Tree_FindRow(
    TreeCtrl *tree,		/* Widget info. */
    int rowIndex		/* 0-based index of the row to return. */
    )
{
    RowLabel *row = (RowLabel *) tree->rows;

    while (row != NULL) {
	if (row->index == rowIndex)
	    break;
	row = row->next;
    }
    return (TreeRowLabel) row;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_Next --
 *
 *	Return the row to the right of the given one.
 *
 * Results:
 *	Token for the next row.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeRowLabel
TreeRowLabel_Next(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    return (TreeRowLabel) ((RowLabel *) row_)->next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_OnScreen --
 *
 *	Called by display code.
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
TreeRowLabel_OnScreen(
    TreeRowLabel row_,		/* RowLabel token. */
    int onScreen
    )
{
    RowLabel *row = (RowLabel *) row_;
    TreeCtrl *tree = row->tree;

    if (onScreen == row->onScreen)
	return;
    row->onScreen = onScreen;
    if (row->style != NULL)
	TreeStyle_OnScreen(tree, row->style, onScreen);
}

/*
 *----------------------------------------------------------------------
 *
 * Row_Config --
 *
 *	This procedure is called to process an objc/objv list to set
 *	configuration options for a Row.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then an error message is left in interp's result.
 *
 * Side effects:
 *	Configuration information, such as height, span, etc. get set
 *	for row; old resources get freed, if there were any. Display
 *	changes may occur.
 *
 *----------------------------------------------------------------------
 */

static int
Row_Config(
    RowLabel *row,		/* RowLabel record. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[],	/* Argument values. */
    int createFlag		/* TRUE if the RowLabel is being created. */
    )
{
    TreeCtrl *tree = row->tree;
    Tk_SavedOptions savedOptions;
    RowLabel saved;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;
    TreeStyle style = NULL; /* master style */

    saved.visible = row->visible;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) row,
			row->optionTable, objc, objv, tree->tkwin,
			&savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    /* Wouldn't have to do this if Tk_InitOptions() would return
	     * a mask of configured options like Tk_SetOptions() does. */
	    if (createFlag) {
		if (row->styleObj != NULL)
		    mask |= ROW_CONF_STYLE;
	    }

	    /*
	     * Step 1: Save old values
	     */

	    /*
	     * Step 2: Process new values
	     */

	    if (mask & ROW_CONF_STYLE) {
		if (row->styleObj != NULL) {
		    if (TreeStyle_FromObj(tree, row->styleObj, &style) != TCL_OK)
			continue;
		}
	    }

	    /*
	     * Step 3: Free saved values
	     */

	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /* *** */

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    /* Wouldn't have to do this if Tk_InitOptions() would return
    * a mask of configured options like Tk_SetOptions() does. */
    if (createFlag) {
    }

    if (mask & ROW_CONF_STYLE) {
	if (style == NULL) {
	    if (row->style != NULL) {
		TreeStyle_FreeResources(tree, row->style);
		row->style = NULL;
		mask |= ROW_CONF_WIDTH | ROW_CONF_HEIGHT;
	    }
	} else {
	    if ((row->style == NULL) || (TreeStyle_GetMaster(tree, row->style) != style)) {
		if (row->style != NULL) {
		    TreeStyle_FreeResources(tree, row->style);
		}
		row->style = TreeStyle_NewInstance(tree, style);
		mask |= ROW_CONF_WIDTH | ROW_CONF_HEIGHT;
	    }
	}
    }

    if (mask & ROW_CONF_HEIGHT) {
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
    }
    if (mask & ROW_CONF_WIDTH) {
	tree->neededWidthOfRows = -1; /* requested width of row labels */
    }
    if (mask & (ROW_CONF_WIDTH | ROW_CONF_HEIGHT)) {
	Tree_DInfoChanged(tree, DINFO_DRAW_ROWLABELS);
    }
    if (saved.visible != row->visible)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Row_Alloc --
 *
 *	Allocate and initialize a new RowLabel record.
 *
 * Results:
 *	Pointer to the new RowLabel, or NULL if errors occurred.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static RowLabel *
Row_Alloc(
    TreeCtrl *tree		/* Widget info. */
    )
{
    RowLabel *row;
    Tcl_HashEntry *hPtr;
    int isNew;

    row = (RowLabel *) ckalloc(sizeof(RowLabel));
    memset(row, '\0', sizeof(RowLabel));
    row->tree = tree;
    row->optionTable = Tk_CreateOptionTable(tree->interp, rowSpecs);
    if (Tk_InitOptions(tree->interp, (char *) row, row->optionTable,
		tree->tkwin) != TCL_OK) {
	WFREE(row, RowLabel);
	return NULL;
    }
    row->id = tree->nextRowId++;
    tree->rowCount++;
    hPtr = Tcl_CreateHashEntry(&tree->rowIDHash, (char *) row->id, &isNew);
    Tcl_SetHashValue(hPtr, (char *) row);
    
    return row;
}

/*
 *----------------------------------------------------------------------
 *
 * Row_Free --
 *
 *	Free a Row.
 *
 * Results:
 *	Pointer to the next row.
 *
 * Side effects:
 *	Memory is deallocated. If this is the last row being
 *	deleted, the TreeCtrl.nextRowId field is reset to zero.
 *
 *----------------------------------------------------------------------
 */

static RowLabel *
Row_Free(
    RowLabel *row		/* RowLabel record. */
    )
{
    TreeCtrl *tree = row->tree;
    RowLabel *next = row->next;
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&tree->rowIDHash, (char *) row->id);
    Tcl_DeleteHashEntry(hPtr);
    if (row->style != NULL)
	TreeStyle_FreeResources(tree, row->style);
    Tk_FreeConfigOptions((char *) row, row->optionTable, tree->tkwin);
    WFREE(row, RowLabel);
    tree->rowCount--;
    if (tree->rowCount == 0)
	tree->nextRowId = 0;
    return next;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_FixedHeight --
 *
 *	Return the value of the -height option.
 *
 * Results:
 *	The pixel height or -1 if the -height option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeRowLabel_FixedHeight(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    RowLabel *row = (RowLabel *) row_;
    return row->heightObj ? row->height : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_MinHeight --
 *
 *	Return the value of the -minheight option.
 *
 * Results:
 *	The pixel height or -1 if the -minheight option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeRowLabel_MinHeight(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    RowLabel *row = (RowLabel *) row_;
    return row->minHeightObj ? row->minHeight : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_MaxHeight --
 *
 *	Return the value of the -maxheight option.
 *
 * Results:
 *	The pixel height or -1 if the -maxheight option is unspecified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeRowLabel_MaxHeight(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    RowLabel *row = (RowLabel *) row_;
    return row->maxHeightObj ? row->maxHeight : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_NeededWidth --
 *
 *	Returns the requested width of a Row.
 *
 * Results:
 *	If the RowLabel has a style, the requested width of the style
 *	is returned (a positive pixel value). Otherwise 0 is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeRowLabel_NeededWidth(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    RowLabel *row = (RowLabel *) row_;
    TreeCtrl *tree = row->tree;

    if (row->style != NULL)
	return TreeStyle_NeededWidth(tree, row->style, row->state);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_NeededHeight --
 *
 *	Returns the requested height of a Row.
 *
 * Results:
 *	If the RowLabel has a style, the requested height of the style
 *	is returned (a positive pixel value). Otherwise 0 is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeRowLabel_NeededHeight(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    RowLabel *row = (RowLabel *) row_;
    TreeCtrl *tree = row->tree;

    if (row->style != NULL)
	return TreeStyle_NeededHeight(tree, row->style, row->state);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_Visible --
 *
 *	Return the value of the -visible config option for a row.
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
TreeRowLabel_Visible(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    return ((RowLabel *) row_)->visible;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_GetID --
 *
 *	Return the unique identifier for a row.
 *
 * Results:
 *	Unique integer id.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int TreeRowLabel_GetID(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    return ((RowLabel *) row_)->id;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_GetStyle --
 *
 *	Returns the style assigned to a Row.
 *
 * Results:
 *	Returns the style, or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TreeStyle TreeRowLabel_GetStyle(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    return ((RowLabel *) row_)->style;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_ForgetStyle --
 *
 *	Free the style assigned to a Row.
 *
 * Results:
 *	RowLabel has no style assigned anymore.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TreeRowLabel_ForgetStyle(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    RowLabel *row = (RowLabel *) row_;
    TreeCtrl *tree = row->tree;

    if (row->style != NULL) {
	TreeStyle_FreeResources(tree, row->style);
	Tcl_DecrRefCount(row->styleObj);
	row->styleObj = NULL;
	row->style = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_Index --
 *
 *	Return the 0-based index for a row.
 *
 * Results:
 *	Position of the row in the list of rows.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TreeRowLabel_Index(
    TreeRowLabel row_		/* RowLabel token. */
    )
{
    return ((RowLabel *) row_)->index;
}

/*
 *----------------------------------------------------------------------
 *
 * NoStyleMsg --
 *
 *	Utility to set the interpreter result with a message indicating
 *	a RowLabel has no assigned style.
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
    RowLabel *row			/* RowLabel record. */
    )
{
    TreeCtrl *tree = row->tree;

    FormatResult(tree->interp, "rowlabel %s%d has no style",
	    tree->rowPrefix, row->id);
}

/*
 *----------------------------------------------------------------------
 *
 * RowElementCmd --
 *
 *	This procedure is invoked to process the [rowlabel element] widget
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
RowElementCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = { "cget", "configure", "perstate",
	(char *) NULL };
    enum { COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_PERSTATE };
    int index;
    TreeRowLabelList rows;
    TreeRowLabel row_;
    RowLabel *row;
    RowForEach iter;

    if (objc < 6) {
	Tcl_WrongNumArgs(interp, 3, objv, "command row element ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
	/* T rowlabel element perstate R E option ?stateList? */
	case COMMAND_PERSTATE:
	{
	    int state;

	    if (objc < 7 || objc > 8) {
		Tcl_WrongNumArgs(tree->interp, 4, objv,
			"row element option ?stateList?");
		return TCL_ERROR;
	    }
	    if (TreeRowLabel_FromObj(tree, objv[4], &row_,
		    RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    row = (RowLabel *) row_;
	    if (row->style == NULL) {
		NoStyleMsg(row);
		return TCL_ERROR;
	    }
	    state = row->state;
	    if (objc == 8) {
		int states[3];

		if (Tree_StateFromListObj(tree, objv[7], states,
			    SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK) {
		    return TCL_ERROR;
		}
		state = states[STATE_OP_ON];
	    }
	    return TreeStyle_ElementActual(tree, row->style,
		    state, objv[6], objv[7]);
	}

	/* T rowlabel element cget R E option */
	case COMMAND_CGET:
	{
	    if (objc != 7) {
		Tcl_WrongNumArgs(tree->interp, 4, objv,
			"row element option");
		return TCL_ERROR;
	    }
	    if (TreeRowLabel_FromObj(tree, objv[4], &row_,
		    RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    row = (RowLabel *) row_;
	    if (row->style == NULL) {
		NoStyleMsg(row);
		return TCL_ERROR;
	    }
	    return TreeStyle_ElementCget(tree, (TreeItem) NULL,
		    (TreeItemColumn) NULL, row_, row->style, objv[5], objv[6]);
	}

	/* T rowlabel element configure R E ... */
	case COMMAND_CONFIGURE:
	{
	    int result = TCL_OK;

	    if (TreeRowLabelList_FromObj(tree, objv[4], &rows,
		    RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;

	    ROW_FOR_EACH(row_, &rows, &iter) {
		row = (RowLabel *) row_;

		/* T row element configure R E option value \
		*     + E option value + ... */
		int eMask, rMask = 0;
		int indexElem = 5;

		while (1) {
		    int numArgs = 0;
		    char breakChar = '\0';

		    /* Look for a + */
		    for (index = indexElem + 1; index < objc; index++) {
			if (numArgs % 2 == 0) {
			    int length;
			    char *s = Tcl_GetStringFromObj(objv[index], &length);

			    if ((length == 1) && (s[0] == '+')) {
				breakChar = s[0];
				break;
			    }
			}
			numArgs++;
		    }

		    /* Require at least one option-value pair if more than one
		     * element is specified. */
		    if ((breakChar || indexElem != 5) && (numArgs < 2)) {
			FormatResult(interp,
			    "missing option-value pair after element \"%s\"",
			    Tcl_GetString(objv[indexElem]));
			result = TCL_ERROR;
			break;
		    }

		    result = TreeStyle_ElementConfigure(tree, (TreeItem) NULL,
			    (TreeItemColumn) NULL, row_, row->style, objv[indexElem],
			    numArgs, (Tcl_Obj **) objv + indexElem + 1, &eMask);
		    if (result != TCL_OK)
			break;

		    rMask |= eMask;

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

		    } else if (index == objc)
			break;
		}
		if (rMask & CS_LAYOUT) {
		    tree->neededWidthOfRows = -1;
		}
		if (rMask & CS_DISPLAY)
		    Tree_DInfoChanged(tree, DINFO_DRAW_ROWLABELS);
		if (result != TCL_OK)
		    break;
	    }
	    TreeRowLabelList_Free(&rows);
	    return result;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Row_ChangeState --
 *
 *	Toggles zero or more STATE_xxx flags for a Row.
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
Row_ChangeState(
    RowLabel *row,		/* RowLabel info. */
    int stateOff,		/* STATE_xxx flags to turn off. */
    int stateOn			/* STATE_xxx flags to turn on. */
    )
{
    TreeCtrl *tree = row->tree;
    int state;
    int sMask = 0;

    state = row->state;
    state &= ~stateOff;
    state |= stateOn;

    if (state == row->state)
	return 0;

    if (row->style != NULL) {
	sMask = TreeStyle_ChangeState(tree, row->style, row->state, state);
	if (sMask & CS_LAYOUT) {
	    tree->neededWidthOfRows = -1;
	}
	if (sMask & CS_DISPLAY)
	    Tree_DInfoChanged(tree, DINFO_DRAW_ROWLABELS);
    }

    row->state = state;
    return sMask;
}

/*
 *----------------------------------------------------------------------
 *
 * RowStateCmd --
 *
 *	This procedure is invoked to process the [row state] widget
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
RowStateCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = {
	"get", "set", (char *) NULL
    };
    enum {
	COMMAND_GET, COMMAND_SET
    };
    int index;
    TreeRowLabelList rows;
    TreeRowLabel row_;
    RowLabel *row;
    RowForEach iter;

    if (objc < 5) {
	Tcl_WrongNumArgs(interp, 3, objv, "command rowlabel ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {

	/* T row state get R ?state? */
	case COMMAND_GET:
	{
	    Tcl_Obj *listObj;
	    int i, states[3];

	    if (objc > 6) {
		Tcl_WrongNumArgs(interp, 5, objv, "?state?");
		return TCL_ERROR;
	    }
	    if (TreeRowLabel_FromObj(tree, objv[4], &row_,
		    RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    row = (RowLabel *) row_;
	    if (objc == 6) {
		states[STATE_OP_ON] = 0;
		if (Tree_StateFromObj(tree, objv[5], states, NULL,
			    SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp,
			Tcl_NewBooleanObj((row->state & states[STATE_OP_ON]) != 0));
		break;
	    }
	    listObj = Tcl_NewListObj(0, NULL);
	    for (i = 0; i < 32; i++) {
		if (tree->stateNames[i] == NULL)
		    continue;
		if (row->state & (1L << i)) {
		    Tcl_ListObjAppendElement(interp, listObj,
			    Tcl_NewStringObj(tree->stateNames[i], -1));
		}
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T row state set R {state ...} */
	case COMMAND_SET:
	{
	    int states[3], stateOn, stateOff;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 5, objv, "stateList");
		return TCL_ERROR;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[4], &rows,
		    RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    if (Tree_StateFromListObj(tree, objv[5], states,
			SFO_NOT_STATIC) != TCL_OK) {
		TreeRowLabelList_Free(&rows);
		return  TCL_ERROR;
	    }
	    if (states[0] || states[1] || states[2]) {
		ROW_FOR_EACH(row_, &rows, &iter) {
		    row = (RowLabel *) row_;
		    stateOn = states[STATE_OP_ON];
		    stateOff = states[STATE_OP_OFF];
		    stateOn |= ~row->state & states[STATE_OP_TOGGLE];
		    stateOff |= row->state & states[STATE_OP_TOGGLE];
		    Row_ChangeState(row, stateOff, stateOn);
		}
	    }
	    TreeRowLabelList_Free(&rows);
	    return TCL_OK;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RowTagCmd --
 *
 *	This procedure is invoked to process the [rowlabel tag] widget
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
RowTagCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = {
	"add", "expr", "names", "remove", (char *) NULL
    };
    enum {
	COMMAND_ADD, COMMAND_EXPR, COMMAND_NAMES, COMMAND_REMOVE
    };
    int index;
    RowForEach iter;
    TreeRowLabelList rows;
    TreeRowLabel _row;
    RowLabel *row;
    int result = TCL_OK;

    if (objc < 4)
    {
	Tcl_WrongNumArgs(interp, 3, objv, "command ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
	&index) != TCL_OK)
    {
	return TCL_ERROR;
    }

    switch (index)
    {
	/* T rowlabel tag add R tagList */
	case COMMAND_ADD:
	{
	    int i, numTags;
	    Tcl_Obj **listObjv;
	    Tk_Uid staticTags[STATIC_SIZE], *tags = staticTags;

	    if (objc != 6)
	    {
		Tcl_WrongNumArgs(interp, 4, objv, "rowlabel tagList");
		return TCL_ERROR;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[4], &rows, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (Tcl_ListObjGetElements(interp, objv[5], &numTags, &listObjv) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    STATIC_ALLOC(tags, Tk_Uid, numTags);
	    for (i = 0; i < numTags; i++) {
		tags[i] = Tk_GetUid(Tcl_GetString(listObjv[i]));
	    }
	    ROW_FOR_EACH(_row, &rows, &iter) {
		row = (RowLabel *) _row;
		row->tagInfo = TagInfo_Add(row->tagInfo, tags, numTags);
	    }
	    STATIC_FREE(tags, Tk_Uid, numTags);
	    break;
	}

	/* T rowlabel tag expr R tagExpr */
	case COMMAND_EXPR:
	{
	    TagExpr expr;
	    int ok = TRUE;

	    if (objc != 6)
	    {
		Tcl_WrongNumArgs(interp, 4, objv, "rowlabel tagExpr");
		return TCL_ERROR;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[4], &rows, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (TagExpr_Init(tree, objv[5], &expr) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    ROW_FOR_EACH(_row, &rows, &iter) {
		row = (RowLabel *) _row;
		if (!TagExpr_Eval(&expr, row->tagInfo)) {
		    ok = FALSE;
		    break;
		}
	    }
	    TagExpr_Free(&expr);
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(ok));
	    break;
	}

	/* T rowlabel tag names R */
	case COMMAND_NAMES:
	{
	    Tcl_Obj *listObj;
	    Tk_Uid *tags = NULL;
	    int i, tagSpace, numTags = 0;

	    if (objc != 5)
	    {
		Tcl_WrongNumArgs(interp, 4, objv, "rowlabel");
		return TCL_ERROR;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[4], &rows, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ROW_FOR_EACH(_row, &rows, &iter) {
		row = (RowLabel *) _row;
		tags = TagInfo_Names(row->tagInfo, tags, &numTags, &tagSpace);
	    }
	    if (numTags) {
		listObj = Tcl_NewListObj(0, NULL);
		for (i = 0; i < numTags; i++) {
		    Tcl_ListObjAppendElement(NULL, listObj,
			    Tcl_NewStringObj((char *) tags[i], -1));
		}
		Tcl_SetObjResult(interp, listObj);
		ckfree((char *) tags);
	    }
	    break;
	}

	/* T rowlabel tag remove R tagList */
	case COMMAND_REMOVE:
	{
	    int i, numTags;
	    Tcl_Obj **listObjv;
	    Tk_Uid staticTags[STATIC_SIZE], *tags = staticTags;

	    if (objc != 6)
	    {
		Tcl_WrongNumArgs(interp, 4, objv, "rowlabel tagList");
		return TCL_ERROR;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[4], &rows, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (Tcl_ListObjGetElements(interp, objv[5], &numTags, &listObjv) != TCL_OK) {
		result = TCL_ERROR;
		break;
	    }
	    STATIC_ALLOC(tags, Tk_Uid, numTags);
	    for (i = 0; i < numTags; i++) {
		tags[i] = Tk_GetUid(Tcl_GetString(listObjv[i]));
	    }
	    ROW_FOR_EACH(_row, &rows, &iter) {
		row = (RowLabel *) _row;
		row->tagInfo = TagInfo_Remove(row->tagInfo, tags, numTags);
	    }
	    STATIC_FREE(tags, Tk_Uid, numTags);
	    break;
	}
    }

    TreeRowLabelList_Free(&rows);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowCmd --
 *
 *	This procedure is invoked to process the [row] widget
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
TreeRowLabelCmd(
    ClientData clientData,	/* Widget info. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[]	/* Argument values. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = {
	"bbox", "cget", "compare", "configure", "count", "create", "delete",
	"dragcget", "dragconfigure", "element", "id", "image", "list", "move",
	"neededheight", "order", "state", "tag", "text", "width", (char *) NULL
    };
    enum {
	COMMAND_BBOX, COMMAND_CGET, COMMAND_COMPARE, COMMAND_CONFIGURE,
	COMMAND_COUNT, COMMAND_CREATE, COMMAND_DELETE, COMMAND_DRAGCGET,
	COMMAND_DRAGCONF, COMMAND_ELEMENT, COMMAND_ID, COMMAND_IMAGE,
	COMMAND_LIST, COMMAND_MOVE, COMMAND_NHEIGHT, COMMAND_ORDER,
	COMMAND_STATE, COMMAND_TAG, COMMAND_TEXT, COMMAND_WIDTH
    };
    int index;
    TreeRowLabelList rows;
    TreeRowLabel _row;
    RowLabel *row;
    RowForEach iter;

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
	    int x, y, w, h;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel");
		return TCL_ERROR;
	    }
	    if (TreeRowLabel_FromObj(tree, objv[3], &_row,
			RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    if (Tree_RowLabelBbox(tree, _row, &x, &y, &w, &h) < 0)
		break;
	    x -= tree->xOrigin;
	    y -= tree->yOrigin;
	    FormatResult(interp, "%d %d %d %d", x, y, x + w, y + h);
	    break;
	}

	case COMMAND_CGET:
	{
	    TreeRowLabel row;
	    Tcl_Obj *resultObjPtr;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel option");
		return TCL_ERROR;
	    }
	    if (TreeRowLabel_FromObj(tree, objv[3], &row,
			RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) row,
		    ((RowLabel *) row)->optionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T row compare C op C */
	case COMMAND_COMPARE:
	{
	    TreeRowLabel row1, row2;
	    static CONST char *opName[] = { "<", "<=", "==", ">=", ">", "!=", NULL };
	    int op, compare = 0, index1, index2;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel1 op rowlabel2");
		return TCL_ERROR;
	    }
	    if (TreeRowLabel_FromObj(tree, objv[3], &row1,
		    RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    if (Tcl_GetIndexFromObj(interp, objv[4], opName,
		    "comparison operator", 0, &op) != TCL_OK)
		return TCL_ERROR;
	    if (TreeRowLabel_FromObj(tree, objv[5], &row2,
		    RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    index1 = TreeRowLabel_Index(row1);
	    index2 = TreeRowLabel_Index(row2);
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

	/* T rowlabel configure R ?option? ?value? ?option value ...? */
	case COMMAND_CONFIGURE:
	{
	    int result = TCL_OK;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel ?option? ?value? ?option value ...?");
		return TCL_ERROR;
	    }
	    if (objc <= 5) {
		Tcl_Obj *resultObjPtr;
		if (TreeRowLabel_FromObj(tree, objv[3], &_row,
			RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		    return TCL_ERROR;
		row = (RowLabel *) _row;
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) row,
			row->optionTable,
			(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[3], &rows,
		    RFO_NOT_NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	    ROW_FOR_EACH(_row, &rows, &iter) {
		if (Row_Config((RowLabel *) _row, objc - 4, objv + 4, FALSE)
			!= TCL_OK) {
		    result = TCL_ERROR;
		    break;
		};
	    }
	    TreeRowLabelList_Free(&rows);
	    return result;
	}

	case COMMAND_CREATE:
	{
	    RowLabel *row, *last = (RowLabel *) tree->rowLabelLast;

	    /* FIXME: -count N -tags $tags */
	    row = Row_Alloc(tree);
	    if (Row_Config(row, objc - 3, objv + 3, TRUE) != TCL_OK)
	    {
		Row_Free(row);
		return TCL_ERROR;
	    }

	    if (tree->rows == NULL) {
		row->index = 0;
		tree->rows = (TreeRowLabel) row;
	    } else {
		last->next = row;
		row->prev = last;
		row->index = last->index + 1;
	    }
	    tree->rowLabelLast = (TreeRowLabel) row;

	    tree->neededWidthOfRows = -1;
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    Tcl_SetObjResult(interp, TreeRowLabel_ToObj(tree, (TreeRowLabel) row));
	    break;
	}

	case COMMAND_DELETE:
	{
	    RowLabel *prev, *next;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel");
		return TCL_ERROR;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[3], &rows, 0) != TCL_OK)
		return TCL_ERROR;
	    ROW_FOR_EACH(_row, &rows, &iter) {
		if (iter.all) {
		    row = (RowLabel *) tree->rows;
		    while (row != NULL) {
			row = Row_Free(row);
		    }
		    tree->rows = NULL;
		    tree->rowLabelLast = NULL;
		    tree->rowDrag.row = tree->rowDrag.indRow = NULL;
		    tree->neededWidthOfRows = -1;
		    Tree_DInfoChanged(tree, DINFO_DRAW_ROWLABELS);
		    break;
		}
		row = (RowLabel *) _row;

		if (row == (RowLabel *) tree->rowDrag.row)
		    tree->rowDrag.row = NULL;
		if (row == (RowLabel *) tree->rowDrag.indRow)
		    tree->rowDrag.indRow = NULL;

		prev = row->prev;
		next = row->next;
		if (prev != NULL)
		    prev->next = next;
		if (next != NULL)
		    next->prev = prev;
		if (prev == NULL)
		    tree->rows = (TreeRowLabel) next;
		if (next == NULL)
		    tree->rowLabelLast = (TreeRowLabel) prev;
		(void) Row_Free(row);
	    }
	    TreeRowLabelList_Free(&rows);

	    /* Renumber rows */
	    index = 0;
	    row = (RowLabel *) tree->rows;
	    while (row != NULL) {
		row->index = index++;
		row = row->next;
	    }

	    tree->neededWidthOfRows = -1;
	    Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    break;
	}

	/* T rowlabel dragcget option */
	case COMMAND_DRAGCGET:
	{
	    Tcl_Obj *resultObjPtr;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "option");
		return TCL_ERROR;
	    }
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) tree,
		    tree->rowDrag.optionTable, objv[3], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	/* T rowlabel dragconfigure ?option? ?value? ?option value ...? */
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
			tree->rowDrag.optionTable,
			(objc == 3) ? (Tcl_Obj *) NULL : objv[3],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    result = Tk_SetOptions(interp, (char *) tree,
		    tree->rowDrag.optionTable, objc - 3, objv + 3, tree->tkwin,
		    &savedOptions, &mask);
	    if (result != TCL_OK) {
		Tk_RestoreSavedOptions(&savedOptions);
		return TCL_ERROR;
	    }
	    Tk_FreeSavedOptions(&savedOptions);

	    if (tree->rowDrag.alpha < 0)
		tree->rowDrag.alpha = 0;
	    if (tree->rowDrag.alpha > 255)
		tree->rowDrag.alpha = 255;

	    Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
	    break;
	}

	case COMMAND_COUNT:
	{
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->rowCount));
	    break;
	}

	case COMMAND_ELEMENT:
	{
	    return RowElementCmd(clientData, interp, objc, objv);
	}

	case COMMAND_ID:
	{
	    Tcl_Obj *listObj;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel");
		return TCL_ERROR;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[3], &rows, 0) != TCL_OK)
		return TCL_ERROR;
	    listObj = Tcl_NewListObj(0, NULL);
	    ROW_FOR_EACH(_row, &rows, &iter) {
		Tcl_ListObjAppendElement(interp, listObj,
			TreeRowLabel_ToObj(tree, _row));
	    }
	    TreeRowLabelList_Free(&rows);
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T rowlabel list ?-visible? */
	case COMMAND_LIST:
	{
	    RowLabel *row = (RowLabel *) tree->rows;
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
	    while (row != NULL) {
		if (!visible || row->visible)
		    Tcl_ListObjAppendElement(interp, listObj,
				TreeRowLabel_ToObj(tree, (TreeRowLabel) row));
		row = row->next;
	    }
	    Tcl_SetObjResult(interp, listObj);
	    break;
	}

	/* T rowlabel move R before */
	case COMMAND_MOVE:
	{
	    TreeRowLabel _move, _before;
	    RowLabel *move, *before, *prev, *next, *last, *rowAfterLast = NULL;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel before");
		return TCL_ERROR;
	    }
	    if (TreeRowLabel_FromObj(tree, objv[3], &_move,
		    RFO_NOT_MANY | RFO_NOT_NULL ) != TCL_OK)
		return TCL_ERROR;
	    move = (RowLabel *) _move;
	    if (TreeRowLabel_FromObj(tree, objv[4], &_before,
		    RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    before = (RowLabel *) _before;
	    if (move == before)
		break;
	    if (move->index == before->index - 1)
		break;

	    /* Unlink. */
	    prev = move->prev;
	    next = move->next;
	    if (prev == NULL)
		tree->rows = (TreeRowLabel) next;
	    else
		prev->next = next;
	    if (next == NULL)
		tree->rowLabelLast = (TreeRowLabel) prev;
	    else
		next->prev = prev;

	    /* Link. */
	    if (before == rowAfterLast) { /* FIXME */
		last = (RowLabel *) tree->rowLabelLast;
		last->next = move;
		move->prev = last;
		move->next = NULL;
		tree->rowLabelLast = (TreeRowLabel) move;
	    } else {
		prev = before->prev;
		if (prev == NULL)
		    tree->rows = (TreeRowLabel) move;
		else
		    prev->next = move;
		before->prev = move;
		move->prev = prev;
		move->next = before;
	    }

	    /* Renumber rows */
	    index = 0;
	    row = (RowLabel *) tree->rows;
	    while (row != NULL) {
		row->index = index++;
		row = row->next;
	    }

	    if (move->visible) {
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	    }
	    break;
	}

	case COMMAND_NHEIGHT:
	{
	    TreeRowLabel _row;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel");
		return TCL_ERROR;
	    }
	    if (TreeRowLabel_FromObj(tree, objv[3], &_row,
		    RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;

	    Tcl_SetObjResult(interp, Tcl_NewIntObj(TreeRowLabel_NeededHeight(_row)));
	    break;
	}

	/* T rowlabel order R ?-visible? */
	case COMMAND_ORDER:
	{
	    int visible = FALSE;
	    int index = 0;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "rowlabel ?-visible?");
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
	    if (TreeRowLabel_FromObj(tree, objv[3], &_row,
		    RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    row = (RowLabel *) _row;
	    if (visible) {
		RowLabel *walk = (RowLabel *) tree->rows;
		while (walk != NULL) {
		    if (walk == row)
			break;
		    if (walk->visible)
			index++;
		    walk = walk->next;
		}
		if (!row->visible)
		    index = -1;
	    } else {
		index = row->index;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(index));
	    break;
	}

	case COMMAND_STATE:
	{
	    return RowStateCmd(clientData, interp, objc, objv);
	}

	case COMMAND_TAG:
	{
	    return RowTagCmd(clientData, interp, objc, objv);
	}

	/* T row image R ?image? */
	case COMMAND_IMAGE:
	/* T row text R ?text? */
	case COMMAND_TEXT:
	{
	    Tcl_Obj *objPtr;
	    int isImage = (index == COMMAND_IMAGE);
	    int result = TCL_OK;

	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv,
			isImage ? "rowlabel ?image?" : "rowlabel ?text?");
		return TCL_ERROR;
	    }
	    if (objc == 4) {
		if (TreeRowLabel_FromObj(tree, objv[3], &_row,
			RFO_NOT_MANY | RFO_NOT_NULL) != TCL_OK)
		    return TCL_ERROR;
		row = (RowLabel *) _row;
		if (row->style == NULL) {
		    NoStyleMsg(row);
		    return TCL_ERROR;
		}
		objPtr = isImage ?
		    TreeStyle_GetImage(tree, row->style) :
		    TreeStyle_GetText(tree, row->style);
		if (objPtr != NULL)
		    Tcl_SetObjResult(interp, objPtr);
		break;
	    }
	    if (TreeRowLabelList_FromObj(tree, objv[3], &rows,
		    RFO_NOT_NULL) != TCL_OK)
		return TCL_ERROR;
	    ROW_FOR_EACH(_row, &rows, &iter) {
		row = (RowLabel *) _row;
		result = isImage ?
		    TreeStyle_SetImage(tree, (TreeItem) NULL,
			(TreeItemColumn) NULL, _row, row->style, objv[4]) :
		    TreeStyle_SetText(tree, (TreeItem) NULL,
			(TreeItemColumn) NULL, _row, row->style, objv[4]);
		if (result != TCL_OK)
		    break;
	    }
	    TreeRowLabelList_Free(&rows);
	    tree->neededWidthOfRows = -1;
	    Tree_DInfoChanged(tree, DINFO_DRAW_ROWLABELS);
	    return result;
	}

	case COMMAND_WIDTH:
	{
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, NULL);
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(Tree_WidthOfRowLabels(tree)));
	    break;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_Draw --
 *
 *	Draw a row label.
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
TreeRowLabel_Draw(
    TreeRowLabel row_,		/* RowLabel record. */
    int x, int y,		/* Drawable coordinates of the row. */
    int width, int height,	/* Total size of the row. */
    Drawable drawable		/* Where to draw. */
    )
{
    RowLabel *row = (RowLabel *) row_;
    TreeCtrl *tree = row->tree;
    StyleDrawArgs drawArgs;

    if (row->style == NULL)
	return;

    drawArgs.tree = tree;
    drawArgs.drawable = drawable;
    drawArgs.state = row->state;
    drawArgs.style = row->style;
    drawArgs.indent = 0;
    drawArgs.x = x;
    drawArgs.y = y;
    drawArgs.width = width;
    drawArgs.height = height;
    drawArgs.justify = TK_JUSTIFY_LEFT;
    TreeStyle_Draw(&drawArgs);
}

/*
 *----------------------------------------------------------------------
 *
 * TreeRowLabel_Identify --
 *
 *	Determine which element the given point is in.
 *	This is used by the [identify] widget command.
 *
 * Results:
 *	If the RowLabel is not visible or no Rows are visible
 *	then buf[] is untouched. Otherwise the given string may be
 *	appended with "elem E".
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TreeRowLabel_Identify(
    TreeRowLabel row_,		/* RowLabel token. */
    int x, int y,		/* RowLabel coords to hit-test with. */
    char *buf			/* NULL-terminated string which may be
				 * appended. */
    )
{
    RowLabel *row = (RowLabel *) row_;
    TreeCtrl *tree = row->tree;
    int left, top, width, height;
    StyleDrawArgs drawArgs;
    char *elem;

    if (row->style == NULL)
	return;

    if (Tree_RowLabelBbox(tree, row_, &left, &top, &width, &height) < 0)
	return;
    drawArgs.tree = tree;
    drawArgs.drawable = None;
    drawArgs.state = row->state;
    drawArgs.style = row->style;
    drawArgs.indent = 0;
    drawArgs.x = 0;
    drawArgs.y = 0;
    drawArgs.width = width;
    drawArgs.height = height;
    drawArgs.justify = TK_JUSTIFY_LEFT;
    elem = TreeStyle_Identify(&drawArgs, x, y);
    if (elem != NULL)
	sprintf(buf + strlen(buf), " elem %s", elem);
}

#if 0
/*
 *----------------------------------------------------------------------
 *
 * SetImageForRow --
 *
 *	Set a photo image containing a simplified picture of a row label.
 *	This image is used when dragging and dropping a row label.
 *
 * Results:
 *	Token for a photo image, or NULL if the image could not be
 *	created.
 *
 * Side effects:
 *	A photo image called "TreeCtrlRowImage" will be created if
 *	it doesn't exist. The image is set to contain a picture of the
 *	row label.
 *
 *----------------------------------------------------------------------
 */

static Tk_Image
SetImageForRow(
    TreeCtrl *tree,		/* Widget info. */
    RowLabel *row			/* Row record. */
    )
{
    Tk_PhotoHandle photoH;
    Pixmap pixmap;
    int width = Tree_WidthOfRowLabels(tree);
    int height = tree->headerHeight;
    XImage *ximage;

    photoH = Tk_FindPhoto(tree->interp, "TreeCtrlRowImage");
    if (photoH == NULL) {
	Tcl_GlobalEval(tree->interp, "image create photo TreeCtrlRowImage");
	photoH = Tk_FindPhoto(tree->interp, "TreeCtrlRowImage");
	if (photoH == NULL)
	    return NULL;
    }

    pixmap = Tk_GetPixmap(tree->display, Tk_WindowId(tree->tkwin),
	    width, height, Tk_Depth(tree->tkwin));

    Row_Draw(row, pixmap, 0, 0, TRUE);

    /* Pixmap -> XImage */
    ximage = XGetImage(tree->display, pixmap, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);
    if (ximage == NULL)
	panic("ximage is NULL");

    /* XImage -> Tk_Image */
    XImage2Photo(tree->interp, photoH, ximage, tree->rowDrag.alpha);

    XDestroyImage(ximage);
    Tk_FreePixmap(tree->display, pixmap);

    return Tk_GetImage(tree->interp, tree->tkwin, "TreeCtrlRowImage",
	NULL, (ClientData) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawHeader --
 *
 *	Draw the header of every row.
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
    RowLabel *column = (RowLabel *) tree->columns;
    Tk_Window tkwin = tree->tkwin;
    int minX, maxX, width, height;
    Drawable pixmap;
    int x2 = x;

    /* Update layout if needed */
    (void) Tree_HeaderHeight(tree);
    (void) Tree_WidthOfRows(tree);

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
		Row_Draw(column, pixmap, x, y, FALSE);
	    x += column->useWidth;
	}
	column = column->next;
    }

    /* Draw "tail" column */
    if (x < maxX) {
	column = (RowLabel *) tree->columnTail;
	width = maxX - x + column->borderWidth;
	height = tree->headerHeight;
	if (tree->useTheme &&
	    (TreeTheme_DrawHeaderItem(tree, pixmap, 0, 0, x, y, width, height) == TCL_OK)) {
	} else {
	    Tk_3DBorder border;
	    border = PerStateBorder_ForState(tree, &column->border,
		Row_MakeState(column), NULL);
	    if (border == NULL)
		border = tree->border;
	    Tk_Fill3DRectangle(tkwin, pixmap, border,
		    x, y, width, height, column->borderWidth, TK_RELIEF_RAISED);
	}
    }

    {
	Tk_Image image = NULL;
	int imageX = 0, imageW = 0, indDraw = FALSE, indX = 0;

	column = (RowLabel *) tree->columns;
	while (column != NULL) {
	    if (column->visible) {
		if (column == (RowLabel *) tree->columnDrag.column) {
		    image = SetImageForRow(tree, column);
		    imageX = x2;
		    imageW = column->useWidth;
		}
		if (column == (RowLabel *) tree->columnDrag.indRow) {
		    indX = x2 - 1;
		    indDraw = TRUE;
		}
		x2 += column->useWidth;
	    }
	    if (tree->columnDrag.indRow == tree->columnTail) {
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
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tree_WidthOfRowLabels --
 *
 *	Returns the display width of the row labels.
 *
 * Results:
 *	If the -rowlabelwidth option is set, that is the result. Otherwise
 *	the result is the maximum requested width of every visible row
 *	label, clipped to -minrowlabelwidth and -maxrowlabelwidth.
 *
 * Side effects:
 *	The size of elements and styles may be updated if they are
 *	marked out-of-date.
 *
 *----------------------------------------------------------------------
 */

int
Tree_WidthOfRowLabels(
    TreeCtrl *tree		/* Widget info. */
    )
{
    RowLabel *row;
    int width;

    if (!tree->showRowLabels)
	return 0;

    /* The treectrl option -rowlabelwidth specifies a fixed width for
     * the row labels. */
    if (tree->rowLabelWidthObj != NULL && tree->rowLabelWidth >= 0)
	return tree->rowLabelWidth;

    /* Recalculate the maximum requested width of every visible row label. */
    if (tree->neededWidthOfRows < 0) {
	width = 0;
	row = (RowLabel *) tree->rows;
	while (row != NULL) {
	    if (row->visible) {
		width = MAX(TreeRowLabel_NeededWidth((TreeRowLabel) row), width);
	    }
	    row = row->next;
	}
	tree->neededWidthOfRows = width;
    }

    width = tree->neededWidthOfRows;
    if ((tree->minRowLabelWidthObj != NULL) && (tree->minRowLabelWidth >= 0) &&
	    (width < tree->minRowLabelWidth))
	width = tree->minRowLabelWidth;
    if ((tree->maxRowLabelWidthObj != NULL) && (tree->maxRowLabelWidth >= 0) &&
	    (width > tree->maxRowLabelWidth))
	width = tree->maxRowLabelWidth;
    return width;
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_InitRowLabels --
 *
 *	Perform row-related initialization when a new TreeCtrl is
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
Tree_InitRowLabels(
    TreeCtrl *tree		/* Widget info. */
    )
{
    tree->rows = NULL;
    tree->rowLabelLast = NULL;
    tree->nextRowId = 0;
    tree->rowCount = 0;
    tree->neededWidthOfRows = -1;

    Tcl_InitHashTable(&tree->rowIDHash, TCL_ONE_WORD_KEYS);

    tree->rowDrag.optionTable = Tk_CreateOptionTable(tree->interp, dragSpecs);
    (void) Tk_InitOptions(tree->interp, (char *) tree,
	    tree->rowDrag.optionTable, tree->tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_FreeRowLabels --
 *
 *	Free row-related resources for a deleted TreeCtrl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

void Tree_FreeRowLabels(
    TreeCtrl *tree		/* Widget info. */
    )
{
    RowLabel *row = (RowLabel *) tree->rows;

    while (row != NULL) {
	row = Row_Free(row);
    }
    Tcl_DeleteHashTable(&tree->rowIDHash);
}

