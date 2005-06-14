/* 
 * tkTreeElem.h --
 *
 *	This module is the header for elements in treectrl widgets.
 *
 * Copyright (c) 2002-2005 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeElem.h,v 1.13 2005/06/14 00:21:45 treectrl Exp $
 */

typedef struct ElementType ElementType;
typedef struct Element Element;
typedef struct ElementArgs ElementArgs;

struct ElementArgs
{
	TreeCtrl *tree;
	Element *elem;
	int state;
	struct {
		TreeItem item;
		TreeItemColumn column;
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
		int squeeze;
		int pad[4];
#define STICKY_W 0x1000 /* These values must match ELF_STICKY_xxx */
#define STICKY_N 0x2000
#define STICKY_E 0x4000
#define STICKY_S 0x8000
		int sticky;
		Drawable drawable;
	} display;
	struct {
		int fixedWidth;
		int fixedHeight;
		int maxWidth;
		int maxHeight;
		int width;
		int height;
	} needed;
	struct {
		int fixedWidth;
		int height;
	} height;
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
	void (*neededProc)(ElementArgs *args);
	void (*heightProc)(ElementArgs *args);
	int (*changeProc)(ElementArgs *args);
	int (*stateProc)(ElementArgs *args);
	int (*undefProc)(ElementArgs *args);
	int (*actualProc)(ElementArgs *args);
	ElementType *next;
};

/* list of these for each style */
struct Element
{
	Tk_Uid name; /* "elem2", "eText" etc */
	ElementType *typePtr;
	Element *master; /* NULL if this is master */
	/* type-specific data here */
};

extern ElementType elemTypeBitmap;
extern ElementType elemTypeBorder;
extern ElementType elemTypeCheckButton;
extern ElementType elemTypeImage;
extern ElementType elemTypeRect;
extern ElementType elemTypeText;
extern ElementType elemTypeWindow;

#define ELEMENT_TYPE_MATCHES(t1,t2) ((t1)->name == (t2)->name)

/***** ***** *****/

extern int Element_GetSortData(TreeCtrl *tree, Element *elem, int type, long *lv, double *dv, char **sv);

typedef struct TreeIterate_ *TreeIterate;

extern int TreeElement_TypeFromObj(TreeCtrl *tree, Tcl_Obj *objPtr, ElementType **typePtrPtr);
extern void Tree_RedrawElement(TreeCtrl *tree, TreeItem item, Element *elem);
extern TreeIterate Tree_ElementIterateBegin(TreeCtrl *tree, ElementType *elemTypePtr);
extern TreeIterate Tree_ElementIterateNext(TreeIterate iter_);
extern Element *Tree_ElementIterateGet(TreeIterate iter_);
extern void Tree_ElementIterateChanged(TreeIterate iter_, int mask);
extern void Tree_ElementChangedItself(TreeCtrl *tree, TreeItem item,
	TreeItemColumn column, Element *elem, int mask);
	
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
	int (*PerStateInfo_FromObj)(TreeCtrl *tree, StateFromObjProc proc, PerStateType *typePtr, PerStateInfo *pInfo);
	PerStateData *(*PerStateInfo_ForState)(TreeCtrl *tree, PerStateType *typePtr, PerStateInfo *pInfo, int state, int *match);
	Tcl_Obj *(*PerStateInfo_ObjForState)(TreeCtrl *tree, PerStateType *typePtr, PerStateInfo *pInfo, int state, int *match);
	int (*PerStateInfo_Undefine)(TreeCtrl *tree, PerStateType *typePtr, PerStateInfo *pInfo, int state);
};

