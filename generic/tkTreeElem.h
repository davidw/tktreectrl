/* 
 * tkTreeElem.h --
 *
 *	This module is the header for elements in treectrl widgets.
 *
 * Copyright (c) 2002-2004 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeElem.h,v 1.7 2004/10/12 03:53:06 treectrl Exp $
 */

typedef struct ElementType ElementType;
typedef struct Element Element;
typedef struct ElementArgs ElementArgs;

struct ElementArgs
{
	TreeCtrl *tree;
	TreeItem item;
	Element *elem;
	int state;
	struct {
		int noop;
	} create;
	struct {
		int noop;
	} delete;
	struct {
		int objc;
		Tcl_Obj *CONST *objv;
		int flagSelf;
	} config;
	struct {
		int x;
		int y;
		int width;
		int height;
		int pad[4];
		Drawable drawable;
	} display;
	struct {
		int squeeze;
		int width;
		int height;
	} layout;
	struct {
		int flagTree;
		int flagMaster;
		int flagSelf;
	} change;
	struct {
		int state1;
		int state2;
	} states;
	struct {
		Tcl_Obj *obj;
	} actual;
};

struct ElementType
{
	char *name; /* "image", "text" */
	int size; /* size of an Element */
	Tk_OptionSpec *optionSpecs;
	Tk_OptionTable optionTable;
	int (*createProc)(ElementArgs *args);
	void (*deleteProc)(ElementArgs *args);
	int (*configProc)(ElementArgs *args);
	void (*displayProc)(ElementArgs *args);
	void (*layoutProc)(ElementArgs *args);
	int (*changeProc)(ElementArgs *args);
	int (*stateProc)(ElementArgs *args);
	void (*undefProc)(ElementArgs *args);
	int (*actualProc)(ElementArgs *args);
	ElementType *next;
};

/* list of these for each style */
struct Element
{
	Tk_Uid name; /* "image", "text" etc */
	ElementType *typePtr;
	Element *master; /* NULL if this is master */
	/* type-specific data here */
};

extern ElementType elemTypeBitmap;
extern ElementType elemTypeBorder;
extern ElementType elemTypeImage;
extern ElementType elemTypeRect;
extern ElementType elemTypeText;

/***** ***** *****/

extern ElementType *elementTypeList;

extern int Element_GetSortData(TreeCtrl *tree, Element *elem, int type, long *lv, double *dv, char **sv);

typedef struct PerStateData PerStateData;
typedef struct PerStateInfo PerStateInfo;
typedef struct PerStateType PerStateType;

/* There is one of these for each XColor, Tk_Font, Tk_Image etc */
struct PerStateData
{
	int stateOff;
	int stateOn;
	/* Type-specific fields go here */
};

#define DEBUG_PSI

struct PerStateInfo
{
#ifdef DEBUG_PSI
	PerStateType *type;
#endif
	Tcl_Obj *obj;
	int count;
	PerStateData *data;
};

typedef int (*PerStateType_FromObjProc)(TreeCtrl *, Tcl_Obj *, PerStateData *);
typedef void (*PerStateType_FreeProc)(TreeCtrl *, PerStateData *);

struct PerStateType
{
#ifdef DEBUG_PSI
	char *name;
#endif
	int size;
	PerStateType_FromObjProc fromObjProc;
	PerStateType_FreeProc freeProc;
};

#define MATCH_NONE		0
#define MATCH_ANY		1
#define MATCH_PARTIAL	2
#define MATCH_EXACT		3

typedef struct TreeIterate_ *TreeIterate;

extern void Tree_RedrawElement(TreeCtrl *tree, TreeItem item, Element *elem);
extern TreeIterate Tree_ElementIterateBegin(TreeCtrl *tree, ElementType *elemTypePtr);
extern TreeIterate Tree_ElementIterateNext(TreeIterate iter_);
extern Element *Tree_ElementIterateGet(TreeIterate iter_);
extern void Tree_ElementIterateChanged(TreeIterate iter_, int mask);

typedef struct TreeCtrlStubs TreeCtrlStubs;
struct TreeCtrlStubs
{
	int (*TreeCtrl_RegisterElementType)(Tcl_Interp *interp, ElementType *typePtr);
	void (*Tree_RedrawElement)(TreeCtrl *tree, TreeItem item, Element *elem);
	TreeIterate (*Tree_ElementIterateBegin)(TreeCtrl *tree, ElementType *elemTypePtr);
	TreeIterate (*Tree_ElementIterateNext)(TreeIterate iter_);
	Element *(*Tree_ElementIterateGet)(TreeIterate iter_);
	void (*Tree_ElementIterateChanged)(TreeIterate iter_, int mask);
	void (*PerStateInfo_Free)(TreeCtrl *tree, PerStateType *typePtr, PerStateInfo *pInfo);
	int (*PerStateInfo_FromObj)(TreeCtrl *tree, PerStateType *typePtr, PerStateInfo *pInfo);
	PerStateData *(*PerStateInfo_ForState)(TreeCtrl *tree, PerStateType *typePtr, PerStateInfo *pInfo, int state, int *match);
	Tcl_Obj *(*PerStateInfo_ObjForState)(TreeCtrl *tree, PerStateType *typePtr, PerStateInfo *pInfo, int state, int *match);
	void (*PerStateInfo_Undefine)(TreeCtrl *tree, PerStateType *typePtr, PerStateInfo *pInfo, int state);
};

