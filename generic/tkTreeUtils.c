/* 
 * tkTreeUtils.c --
 *
 *	This module implements misc routines for treectrl widgets.
 *
 * Copyright (c) 2002-2006 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeUtils.c,v 1.48 2006/10/31 23:12:24 treectrl Exp $
 */

#include "tkTreeCtrl.h"
#ifdef WIN32
#include "tkWinInt.h"
#endif

/* OffsetRgn() on Mac */
#if defined(MAC_OSX_TK)
#include <Carbon/Carbon.h>
#include "tkMacOSXInt.h"
#include "tclInt.h"
static PixPatHandle gPenPat = NULL;
#endif

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	PadAmountOptionSet _ANSI_ARGS_((ClientData clientData,
		Tcl_Interp *interp, Tk_Window tkwin,
		Tcl_Obj **value, char *recordPtr, int internalOffset,
		char *saveInternalPtr, int flags));
static Tcl_Obj *PadAmountOptionGet _ANSI_ARGS_((ClientData clientData,
		Tk_Window tkwin, char *recordPtr, int internalOffset));
static void	PadAmountOptionRestore _ANSI_ARGS_((ClientData clientData,
		Tk_Window tkwin, char *internalPtr,
		char *saveInternalPtr));
static void	PadAmountOptionFree _ANSI_ARGS_((ClientData clientData,
		Tk_Window tkwin, char *internalPtr));

/*
 * The following Tk_ObjCustomOption structure can be used as clientData entry
 * of a Tk_OptionSpec record with a TK_OPTION_CUSTOM type in the form
 * "(ClientData) &PadAmountOption"; the option will then parse list with
 * one or two screen distances.
 */

Tk_ObjCustomOption PadAmountOption = {
    "pad amount",
    PadAmountOptionSet,
    PadAmountOptionGet,
    PadAmountOptionRestore,
    PadAmountOptionFree
};

/*
 *----------------------------------------------------------------------
 *
 * FormatResult --
 *
 *	Set the interpreter's result to a formatted string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Interpreter's result is modified.
 *
 *----------------------------------------------------------------------
 */

void
FormatResult(
    Tcl_Interp *interp,		/* Current interpreter. */
    char *fmt, ...		/* Format string and varargs. */
    )
{
    va_list ap;
    char buf[256];

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
}

/*
 *----------------------------------------------------------------------
 *
 * Ellipsis --
 *
 *	Determine the number of bytes from the string that will fit
 *	in the given horizontal span. If the entire string does not
 *	fit then determine the largest number of bytes of a substring
 *	with an ellipsis "..." appended that will fit.
 *
 * Results:
 *	When the return value is positive the caller should add the
 *	ellipsis to the string (unless the entire string fits).
 *	When the return value is negative the caller should not add
 *	the ellipsis because only a few characters fit.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Ellipsis(
    Tk_Font tkfont,		/* The font used to display the string. */
    char *string,		/* UTF-8 string, need not be NULL-terminated. */
    int numBytes,		/* Number of bytes to consider. */
    int *maxPixels,		/* In: maximum line length allowed.
				 * Out: length of string that fits (with
				 * ellipsis added if needed). */
    char *ellipsis,		/* NULL-terminated "..." */
    int force			/* TRUE if ellipsis should always be added
				 * even if the whole string fits in
				 * maxPixels. */
    )
{
    char staticStr[256], *tmpStr = staticStr;
    int pixels, pixelsTest, bytesThatFit, bytesTest;
    int ellipsisNumBytes = strlen(ellipsis);

    bytesThatFit = Tk_MeasureChars(tkfont, string, numBytes, *maxPixels, 0,
	&pixels);

    /* The whole string fits. No ellipsis needed (unless forced) */
    if ((bytesThatFit == numBytes) && !force)
    {
	(*maxPixels) = pixels;
	return numBytes;
    }

    if (bytesThatFit <= 1)
    {
	(*maxPixels) = pixels;
	return -bytesThatFit;
    }

    /* Strip off one character at a time, adding ellipsis, until it fits */
    if (force)
	bytesTest = bytesThatFit;
    else
	bytesTest = Tcl_UtfPrev(string + bytesThatFit, string) - string;
    if (bytesTest + ellipsisNumBytes > sizeof(staticStr))
	tmpStr = ckalloc(bytesTest + ellipsisNumBytes);
    memcpy(tmpStr, string, bytesTest);
    while (bytesTest > 0)
    {
	memcpy(tmpStr + bytesTest, ellipsis, ellipsisNumBytes);
	numBytes = Tk_MeasureChars(tkfont, tmpStr,
	    bytesTest + ellipsisNumBytes,
	    *maxPixels, 0, &pixelsTest);
	if (numBytes == bytesTest + ellipsisNumBytes)
	{
	    (*maxPixels) = pixelsTest;
	    if (tmpStr != staticStr)
		ckfree(tmpStr);
	    return bytesTest;
	}
	bytesTest = Tcl_UtfPrev(string + bytesTest, string) - string;
    }

    /* No single char + ellipsis fits. Return number of chars that fit */
    /* Negative tells caller to not add ellipsis */
    (*maxPixels) = pixels;
    if (tmpStr != staticStr)
	ckfree(tmpStr);
    return -bytesThatFit;
}

/*
 *----------------------------------------------------------------------
 *
 * HDotLine --
 *
 *	Draws a horizontal 1-pixel-tall dotted line.
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
HDotLine(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    GC gc,			/* Graphics context. */
    int x1, int y1, int x2	/* Left, top and right coordinates. */
    )
{
#ifdef WIN32
    TkWinDCState state;
    HDC dc;
    HPEN pen, oldPen;
    int nw;
    int wx = x1 + tree->drawableXOrigin;
    int wy = y1 + tree->drawableYOrigin;

    dc = TkWinGetDrawableDC(tree->display, drawable, &state);
    SetROP2(dc, R2_COPYPEN);

    pen = CreatePen(PS_SOLID, 1, gc->foreground);
    oldPen = SelectObject(dc, pen);

    nw = !(wx & 1) == !(wy & 1);
    for (x1 += !nw; x1 < x2; x1 += 2)
    {
	MoveToEx(dc, x1, y1, NULL);
	LineTo(dc, x1 + 1, y1);
    }

    SelectObject(dc, oldPen);
    DeleteObject(pen);

    TkWinReleaseDrawableDC(drawable, dc, &state);
#else
    int nw;
    int wx = x1 + tree->drawableXOrigin;
    int wy = y1 + tree->drawableYOrigin;

    nw = !(wx & 1) == !(wy & 1);
    for (x1 += !nw; x1 < x2; x1 += 2)
    {
	XDrawPoint(tree->display, drawable, gc, x1, y1);
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * VDotLine --
 *
 *	Draws a vertical 1-pixel-wide dotted line.
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
VDotLine(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    GC gc,			/* Graphics context. */
    int x1, int y1, int y2)	/* Left, top, and bottom coordinates. */
{
#ifdef WIN32
    TkWinDCState state;
    HDC dc;
    HPEN pen, oldPen;
    int nw;
    int wx = x1 + tree->drawableXOrigin;
    int wy = y1 + tree->drawableYOrigin;

    dc = TkWinGetDrawableDC(tree->display, drawable, &state);
    SetROP2(dc, R2_COPYPEN);

    pen = CreatePen(PS_SOLID, 1, gc->foreground);
    oldPen = SelectObject(dc, pen);

    nw = !(wx & 1) == !(wy & 1);
    for (y1 += !nw; y1 < y2; y1 += 2)
    {
	MoveToEx(dc, x1, y1, NULL);
	LineTo(dc, x1 + 1, y1);
    }

    SelectObject(dc, oldPen);
    DeleteObject(pen);

    TkWinReleaseDrawableDC(drawable, dc, &state);
#else
    int nw;
    int wx = x1 + tree->drawableXOrigin;
    int wy = y1 + tree->drawableYOrigin;

    nw = !(wx & 1) == !(wy & 1);
    for (y1 += !nw; y1 < y2; y1 += 2)
    {
	XDrawPoint(tree->display, drawable, gc, x1, y1);
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * DrawActiveOutline --
 *
 *	Draws 0 or more sides of a rectangle, dot-on dot-off, XOR style.
 *	This is used by rectangle Elements to indicate the "active"
 *	item.
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
DrawActiveOutline(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    int x, int y,		/* Left and top coordinates. */
    int width, int height,	/* Size of rectangle. */
    int open			/* Bitmask of edges not to draw:
				 * 0x01: left
				 * 0x02: top
				 * 0x04: right
				 * 0x08: bottom */
    )
{
#ifdef WIN32
    int wx = x + tree->drawableXOrigin;
    int wy = y + tree->drawableYOrigin;
    int w = !(open & 0x01);
    int n = !(open & 0x02);
    int e = !(open & 0x04);
    int s = !(open & 0x08);
    int nw, ne, sw, se;
    int i;
    TkWinDCState state;
    HDC dc;

    /* Dots on even pixels only */
    nw = !(wx & 1) == !(wy & 1);
    ne = !((wx + width - 1) & 1) == !(wy & 1);
    sw = !(wx & 1) == !((wy + height - 1) & 1);
    se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

    dc = TkWinGetDrawableDC(tree->display, drawable, &state);
    SetROP2(dc, R2_NOT);

    if (w) /* left */
    {
	for (i = !nw; i < height; i += 2)
	{
	    MoveToEx(dc, x, y + i, NULL);
	    LineTo(dc, x + 1, y + i);
	}
    }
    if (n) /* top */
    {
	for (i = nw ? w * 2 : 1; i < width; i += 2)
	{
	    MoveToEx(dc, x + i, y, NULL);
	    LineTo(dc, x + i + 1, y);
	}
    }
    if (e) /* right */
    {
	for (i = ne ? n * 2 : 1; i < height; i += 2)
	{
	    MoveToEx(dc, x + width - 1, y + i, NULL);
	    LineTo(dc, x + width, y + i);
	}
    }
    if (s) /* bottom */
    {
	for (i = sw ? w * 2 : 1; i < width - (se && e); i += 2)
	{
	    MoveToEx(dc, x + i, y + height - 1, NULL);
	    LineTo(dc, x + i + 1, y + height - 1);
	}
    }

    TkWinReleaseDrawableDC(drawable, dc, &state);
#else /* WIN32 */
    int wx = x + tree->drawableXOrigin;
    int wy = y + tree->drawableYOrigin;
    int w = !(open & 0x01);
    int n = !(open & 0x02);
    int e = !(open & 0x04);
    int s = !(open & 0x08);
    int nw, ne, sw, se;
    int i;
    XGCValues gcValues;
    unsigned long gcMask;
    GC gc;

    /* Dots on even pixels only */
    nw = !(wx & 1) == !(wy & 1);
    ne = !((wx + width - 1) & 1) == !(wy & 1);
    sw = !(wx & 1) == !((wy + height - 1) & 1);
    se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

#if defined(MAC_TCL) || defined(MAC_OSX_TK)
    gcValues.function = GXxor;
#else
    gcValues.function = GXinvert;
#endif
    gcMask = GCFunction;
    gc = Tk_GetGC(tree->tkwin, gcMask, &gcValues);

    if (w) /* left */
    {
	for (i = !nw; i < height; i += 2)
	{
	    XDrawPoint(tree->display, drawable, gc, x, y + i);
	}
    }
    if (n) /* top */
    {
	for (i = nw ? w * 2 : 1; i < width; i += 2)
	{
	    XDrawPoint(tree->display, drawable, gc, x + i, y);
	}
    }
    if (e) /* right */
    {
	for (i = ne ? n * 2 : 1; i < height; i += 2)
	{
	    XDrawPoint(tree->display, drawable, gc, x + width - 1, y + i);
	}
    }
    if (s) /* bottom */
    {
	for (i = sw ? w * 2 : 1; i < width - (se && e); i += 2)
	{
	    XDrawPoint(tree->display, drawable, gc, x + i, y + height - 1);
	}
    }

    Tk_FreeGC(tree->display, gc);
#endif
}

/*
 * The following structure is used when drawing a number of dotted XOR
 * rectangles.
 */
struct DotStatePriv
{
    TreeCtrl *tree;
    Drawable drawable;
#ifdef WIN32
    HDC dc;
    TkWinDCState dcState;
    HRGN rgn;
#elif defined(MAC_OSX_TK)
    CGrafPtr saveWorld;
    GDHandle saveDevice;
#else
    GC gc;
    TkRegion rgn;
#endif
};

/*
 *----------------------------------------------------------------------
 *
 * DotRect_Setup --
 *
 *	Prepare a drawable for drawing a series of dotted XOR rectangles.
 *
 * Results:
 *	State info is returned to be used by the other DotRect_xxx()
 *	procedures.
 *
 * Side effects:
 *	On Win32 and OSX the device context/graphics port is altered
 *	in preparation for drawing. On X11 a new graphics context is
 *	created.
 *
 *----------------------------------------------------------------------
 */

void
DotRect_Setup(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    DotState *p			/* Where to save state info. */
    )
{
    struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#ifdef WIN32
#elif defined(MAC_OSX_TK)
    GWorldPtr destPort;
#else
    XGCValues gcValues;
    unsigned long mask;
    XRectangle xrect;
#endif

    if (sizeof(*dotState) > sizeof(*p))
	panic("DotRect_Setup: DotState hack is too small");

    dotState->tree = tree;
    dotState->drawable = drawable;
#ifdef WIN32
    dotState->dc = TkWinGetDrawableDC(tree->display, drawable, &dotState->dcState);

    /* XOR drawing */
    SetROP2(dotState->dc, R2_NOT);

    /* Keep drawing inside the contentbox */
    dotState->rgn = CreateRectRgn(
	Tree_ContentLeft(tree),
	Tree_ContentTop(tree),
	Tree_ContentRight(tree),
	Tree_ContentBottom(tree));
    SelectClipRgn(dotState->dc, dotState->rgn);
#elif defined(MAC_OSX_TK)
    tree->display->request++;
    destPort = TkMacOSXGetDrawablePort(drawable);
    GetGWorld(&dotState->saveWorld, &dotState->saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(drawable);
    PenNormal();
    PenMode(patXor);
    ShowPen();
    /* FIXME: clipping region */
#else
    gcValues.line_style = LineOnOffDash;
    gcValues.line_width = 1;
    gcValues.dash_offset = 0;
    gcValues.dashes = 1;
#if defined(MAC_TCL)
    gcValues.function = GXxor;
#else
    gcValues.function = GXinvert;
#endif
    mask = GCLineWidth | GCLineStyle | GCDashList | GCDashOffset | GCFunction;
    dotState->gc = Tk_GetGC(tree->tkwin, mask, &gcValues);

    /* Keep drawing inside the contentbox */
    dotState->rgn = TkCreateRegion();
    xrect.x = Tree_ContentLeft(tree);
    xrect.y = Tree_ContentTop(tree);
    xrect.width = Tree_ContentRight(tree) - xrect.x;
    xrect.height = Tree_ContentBottom(tree) - xrect.y;
    TkUnionRectWithRegion(&xrect, dotState->rgn, dotState->rgn);
    TkSetRegion(tree->display, dotState->gc, dotState->rgn);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * DotRect_Draw --
 *
 *	Draw a dotted XOR rectangle.
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
DotRect_Draw(
    DotState *p,		/* Info returned by DotRect_Setup(). */
    int x, int y,		/* Left and top coordinates. */
    int width, int height	/* Size of rectangle. */
    )
{
    struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#ifdef WIN32
#if 1
    RECT rect;

    rect.left = x;
    rect.right = x + width;
    rect.top = y;
    rect.bottom = y + height;
    DrawFocusRect(dotState->dc, &rect);
#else
    HDC dc = dotState->dc; 
    int i;
    int wx = x + dotState->tree->drawableXOrigin;
    int wy = y + dotState->tree->drawableYOrigin;
    int nw, ne, sw, se;

    /* Dots on even pixels only */
    nw = !(wx & 1) == !(wy & 1);
    ne = !((wx + width - 1) & 1) == !(wy & 1);
    sw = !(wx & 1) == !((wy + height - 1) & 1);
    se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

    for (i = !nw; i < height; i += 2)
    {
	MoveToEx(dc, x, y + i, NULL);
	LineTo(dc, x + 1, y + i);
    }
    for (i = nw ? 2 : 1; i < width; i += 2)
    {
	MoveToEx(dc, x + i, y, NULL);
	LineTo(dc, x + i + 1, y);
    }
    for (i = ne ? 2 : 1; i < height; i += 2)
    {
	MoveToEx(dc, x + width - 1, y + i, NULL);
	LineTo(dc, x + width, y + i);
    }
    for (i = sw ? 2 : 1; i < width - se; i += 2)
    {
	MoveToEx(dc, x + i, y + height - 1, NULL);
	LineTo(dc, x + i + 1, y + height - 1);
    }
#endif
#elif defined(MAC_OSX_TK)
    MacDrawable *macWin = (MacDrawable *) dotState->drawable;
    int i;
    int wx = x + dotState->tree->drawableXOrigin;
    int wy = y + dotState->tree->drawableYOrigin;
    int nw, ne, sw, se;

    /* Dots on even pixels only */
    nw = !(wx & 1) == !(wy & 1);
    ne = !((wx + width - 1) & 1) == !(wy & 1);
    sw = !(wx & 1) == !((wy + height - 1) & 1);
    se = !((wx + width - 1) & 1) == !((wy + height - 1) & 1);

    x += macWin->xOff;
    y += macWin->yOff;

    for (i = !nw; i < height; i += 2)
    {
	MoveTo(x, y + i);
	LineTo(x, y + i);
    }
    for (i = nw ? 2 : 1; i < width; i += 2)
    {
	MoveTo(x + i, y);
	LineTo(x + i, y);
    }
    for (i = ne ? 2 : 1; i < height; i += 2)
    {
	MoveTo(x + width, y + i);
	LineTo(x + width, y + i);
    }
    for (i = sw ? 2 : 1; i < width - se; i += 2)
    {
	MoveTo(x + i, y + height - 1);
	LineTo(x + i, y + height - 1);
    }
#else /* MAC_OSX_TK */
    XDrawRectangle(dotState->tree->display, dotState->drawable, dotState->gc,
	x, y, width - 1, height - 1);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * DotRect_Restore --
 *
 *	Restore the drawing environment.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	On Win32 and OSX the device context/graphics port is restored.
 *	On X11 a new graphics context is freed.
 *
 *----------------------------------------------------------------------
 */

void
DotRect_Restore(
    DotState *p			/* Info returned by DotRect_Setup(). */
    )
{
    struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#ifdef WIN32
    SelectClipRgn(dotState->dc, NULL);
    DeleteObject(dotState->rgn);
    TkWinReleaseDrawableDC(dotState->drawable, dotState->dc, &dotState->dcState);
#elif defined(MAC_OSX_TK)
    HidePen();
    /* FIXME: clipping region */
    SetGWorld(dotState->saveWorld, dotState->saveDevice);
#else
    XSetClipMask(dotState->tree->display, dotState->gc, None);
    Tk_FreeGC(dotState->tree->display, dotState->gc);
#endif
}

#ifdef MAC_OSX_TK

/*
 *----------------------------------------------------------------------
 *
 * DrawXORLine --
 *
 *	Draw a 1-pixel thick XOR line (not dotted). This is used
 *	to draw the vertical column proxy when resizing columns.
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
DrawXORLine(
    Display *display,		/* Display. */
    Drawable drawable,		/* Where to draw. */
    int x1, int y1,		/* Left, top. */
    int x2, int y2		/* Bottom, right. */
    )
{
    MacDrawable *macWin = (MacDrawable *) drawable;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;

    destPort = TkMacOSXGetDrawablePort(drawable);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(drawable);
#if 1
    PenNormal();
#else
    TkMacOSXSetUpGraphicsPort(gc, destPort);
#endif
    PenMode(patXor);
    ShowPen();
    MoveTo(macWin->xOff + x1, macWin->yOff + y1);
    LineTo(macWin->xOff + x2, macWin->yOff + y2);
    HidePen();
    SetGWorld(saveWorld, saveDevice);
}

#endif /* MAC_OSX_TK */

/*
 *----------------------------------------------------------------------
 *
 * Tk_FillRegion --
 *
 *	Paint a region with the foreground color of a graphics context.
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
Tk_FillRegion(
    Display *display,		/* Display. */
    Drawable drawable,		/* Where to draw. */
    GC gc,			/* Foreground color. */
    TkRegion rgn		/* Region to paint. */
    )
{
#ifdef WIN32
    HDC dc;
    TkWinDCState dcState;
    HBRUSH brush;

    dc = TkWinGetDrawableDC(display, drawable, &dcState);
    SetROP2(dc, R2_COPYPEN);
    brush = CreateSolidBrush(gc->foreground);
    FillRgn(dc, (HRGN) rgn, brush);
    DeleteObject(brush);
    TkWinReleaseDrawableDC(drawable, dc, &dcState);
#elif defined(MAC_OSX_TK)
    MacDrawable *macWin = (MacDrawable *) drawable;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    RGBColor macColor;

    destPort = TkMacOSXGetDrawablePort(drawable);
    if (gPenPat == NULL)
	gPenPat = NewPixPat();
    if (TkSetMacColor(gc->foreground, &macColor) == true)
	MakeRGBPat(gPenPat, &macColor);
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    TkMacOSXSetUpClippingRgn(drawable);
    TkMacOSXSetUpGraphicsPort(gc, destPort);
    OffsetRgn((RgnHandle) rgn, macWin->xOff, macWin->yOff);
    ShowPen(); /* seemed to work without this */
    FillCRgn((RgnHandle) rgn, gPenPat);
    HidePen(); /* seemed to work without this */
    OffsetRgn((RgnHandle) rgn, -macWin->xOff, -macWin->yOff);
    SetGWorld(saveWorld, saveDevice);
#else
    XRectangle box;

    TkClipBox(rgn, &box);
    TkSetRegion(display, gc, rgn);
    XFillRectangle(display, drawable, gc, box.x, box.y, box.width, box.height);
    XSetClipMask(display, gc, None);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_OffsetRegion --
 *
 *	Offset a region.
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
Tk_OffsetRegion(
    TkRegion region,		/* Region to modify. */
    int xOffset, int yOffset	/* Horizontal and vertical offsets. */
    )
{
#ifdef WIN32
    OffsetRgn((HRGN) region, xOffset, yOffset);
#elif defined(MAC_TCL) || defined(MAC_OSX_TK)
    OffsetRgn((RgnHandle) region, (short) xOffset, (short) yOffset);
#else
    XOffsetRegion((Region) region, xOffset, yOffset);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_ScrollWindow --
 *
 *	Wrapper around TkScrollWindow() to fix an apparent bug with the
 *	Mac/OSX versions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stuff is scrolled in a drawable.
 *
 *----------------------------------------------------------------------
 */

int
Tree_ScrollWindow(
    TreeCtrl *tree,		/* Widget info. */
    GC gc,			/* Arg to TkScrollWindow(). */
    int x, int y,		/* Arg to TkScrollWindow(). */
    int width, int height,	/* Arg to TkScrollWindow(). */
    int dx, int dy,		/* Arg to TkScrollWindow(). */
    TkRegion damageRgn		/* Arg to TkScrollWindow(). */
    )
{
    int result = TkScrollWindow(tree->tkwin, gc, x, y, width, height, dx, dy,
	damageRgn);
#if defined(MAC_TCL) || defined(MAC_OSX_TK)
    {
        MacDrawable *macWin = (MacDrawable *) Tk_WindowId(tree->tkwin);
	/* BUG IN TK? */
	OffsetRgn((RgnHandle) damageRgn, -macWin->xOff, -macWin->yOff);
    }
#endif
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * UnsetClipMask --
 *
 *	Wrapper around XSetClipMask(). On Win32 Tk_DrawChars() does
 *	not clear the clipping region.
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
UnsetClipMask(
    TreeCtrl *tree,		/* Widget info. */
    Drawable drawable,		/* Where to draw. */
    GC gc			/* Graphics context to modify. */
    )
{
    XSetClipMask(tree->display, gc, None);
#ifdef WIN32
    /* Tk_DrawChars does not clear the clip region */
    if (drawable == Tk_WindowId(tree->tkwin)) {
	HDC dc;
	TkWinDCState dcState;

	dc = TkWinGetDrawableDC(tree->display, drawable, &dcState);
	SelectClipRgn(dc, NULL);
	TkWinReleaseDrawableDC(drawable, dc, &dcState);
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawBitmapWithGC --
 *
 *	Draw part of a bitmap.
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
Tree_DrawBitmapWithGC(
    TreeCtrl *tree,		/* Widget info. */
    Pixmap bitmap,		/* Bitmap to draw. */
    Drawable drawable,		/* Where to draw. */
    GC gc,			/* Graphics context. */
    int src_x, int src_y,	/* Left and top of part of bitmap to copy. */
    int width, int height,	/* Width and height of part of bitmap to
				 * copy. */
    int dest_x, int dest_y	/* Left and top coordinates to copy part of
				 * the bitmap to. */
    )
{
#ifdef WIN32
    TkpClipMask *clipPtr = (TkpClipMask *) gc->clip_mask;
#endif
    XSetClipOrigin(tree->display, gc, dest_x, dest_y);
#ifdef WIN32
    /*
     * It seems as though the device context is not set up properly
     * when drawing a transparent bitmap into a window. Normally Tk draws
     * into an offscreen pixmap which gets a temporary device context.
     * This fixes a bug with -doublebuffer none in the demo "Bitmaps".
     */
    if (drawable == Tk_WindowId(tree->tkwin)) {
	if ((clipPtr != NULL) &&
	    (clipPtr->type == TKP_CLIP_PIXMAP) &&
	    (clipPtr->value.pixmap == bitmap)) {
	    HDC dc;
	    TkWinDCState dcState;

	    dc = TkWinGetDrawableDC(tree->display, drawable, &dcState);
	    SetTextColor(dc, RGB(0,0,0));
	    SetBkColor(dc, RGB(255,255,255));
	    TkWinReleaseDrawableDC(drawable, dc, &dcState);
	}
    }
#endif
    XCopyPlane(tree->display, bitmap, drawable, gc,
	src_x, src_y, (unsigned int) width, (unsigned int) height,
	dest_x, dest_y, 1);
    XSetClipOrigin(tree->display, gc, 0, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * Tree_DrawBitmap --
 *
 *	Draw part of a bitmap.
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
Tree_DrawBitmap(
    TreeCtrl *tree,		/* Widget info. */
    Pixmap bitmap,		/* Bitmap to draw. */
    Drawable drawable,		/* Where to draw. */
    XColor *fg, XColor *bg,	/* Foreground and background colors.
				 * May be NULL. */
    int src_x, int src_y,	/* Left and top of part of bitmap to copy. */
    int width, int height,	/* Width and height of part of bitmap to
				 * copy. */
    int dest_x, int dest_y	/* Left and top coordinates to copy part of
				 * the bitmap to. */
    )
{
    XGCValues gcValues;
    GC gc;
    unsigned long mask = 0;

    if (fg != NULL) {
	gcValues.foreground = fg->pixel;
	mask |= GCForeground;
    }
    if (bg != NULL) {
	gcValues.background = bg->pixel;
	mask |= GCBackground;
    } else {
	gcValues.clip_mask = bitmap;
	mask |= GCClipMask;
    }
    gcValues.graphics_exposures = False;
    mask |= GCGraphicsExposures;
    gc = Tk_GetGC(tree->tkwin, mask, &gcValues);
    Tree_DrawBitmapWithGC(tree, bitmap, drawable, gc,
	src_x, src_y, width, height, dest_x, dest_y);
    Tk_FreeGC(tree->display, gc);
}

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

/*
 *----------------------------------------------------------------------
 *
 * XImage2Photo --
 *
 *	Copy pixels from an XImage to a Tk photo image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given photo image is blanked and all the pixels from the
 *	XImage are put into the photo image.
 *
 *----------------------------------------------------------------------
 */

#if defined(WIN32) || defined(MAC_TCL) || defined(MAC_OSX_TK)

void
XImage2Photo(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tk_PhotoHandle photoH,	/* Existing photo image. */
    XImage *ximage,		/* XImage to copy pixels from. */
    int alpha			/* Desired transparency of photo image.*/
    )
{
    Tk_PhotoImageBlock photoBlock;
    unsigned char *pixelPtr;
    int x, y, w = ximage->width, h = ximage->height;
#if defined(MAC_TCL) || defined(MAC_OSX_TK)
    unsigned long red_shift, green_shift, blue_shift;
#endif

    Tk_PhotoBlank(photoH);

    /* See TkPoscriptImage */

#if defined(MAC_TCL) || defined(MAC_OSX_TK)
    red_shift = green_shift = blue_shift = 0;
    while ((0x0001 & (ximage->red_mask >> red_shift)) == 0)
	red_shift++;
    while ((0x0001 & (ximage->green_mask >> green_shift)) == 0)
	green_shift++;
    while ((0x0001 & (ximage->blue_mask >> blue_shift)) == 0)
	blue_shift++;
#endif

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

	    /* FIXME: I think this blows up on classic Mac??? */
	    pixel = XGetPixel(ximage, x, y);
#ifdef WIN32
	    r = GetRValue(pixel);
	    g = GetGValue(pixel);
	    b = GetBValue(pixel);
#endif
#if defined(MAC_TCL) || defined(MAC_OSX_TK)
	    r = (pixel & ximage->red_mask) >> red_shift;
	    g = (pixel & ximage->green_mask) >> green_shift;
	    b = (pixel & ximage->blue_mask) >> blue_shift;
#endif
	    pixelPtr[y * photoBlock.pitch + x * 4 + 0] = r;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 1] = g;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 2] = b;
	    pixelPtr[y * photoBlock.pitch + x * 4 + 3] = alpha;
	}
    }

    TK_PHOTOPUTBLOCK(interp, photoH, &photoBlock, 0, 0, w, h,
	    TK_PHOTO_COMPOSITE_SET);

    Tcl_Free((char *) pixelPtr);
}

#else /* not X11 */

void
XImage2Photo(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tk_PhotoHandle photoH,	/* Existing photo image. */
    XImage *ximage,		/* XImage to copy pixels from. */
    int alpha			/* Desired transparency of photo image.*/
    )
{
    Tk_Window tkwin = Tk_MainWindow(interp);
    Display *display = Tk_Display(tkwin);
    Visual *visual = Tk_Visual(tkwin);
    Tk_PhotoImageBlock photoBlock;
    unsigned char *pixelPtr;
    int x, y, w = ximage->width, h = ximage->height;
    int i, ncolors;
    XColor *xcolors;
    unsigned long red_shift, green_shift, blue_shift;
    int separated = 0;

    Tk_PhotoBlank(photoH);

    /* See TkPoscriptImage */

    ncolors = visual->map_entries;
    xcolors = (XColor *) ckalloc(sizeof(XColor) * ncolors);

    if ((visual->class == DirectColor) || (visual->class == TrueColor)) {
	separated = 1;
	red_shift = green_shift = blue_shift = 0;
	/* ximage->red_mask etc are zero */
	while ((0x0001 & (visual->red_mask >> red_shift)) == 0)
	    red_shift++;
	while ((0x0001 & (visual->green_mask >> green_shift)) == 0)
	    green_shift++;
	while ((0x0001 & (visual->blue_mask >> blue_shift)) == 0)
	    blue_shift++;
	for (i = 0; i < ncolors; i++) {
	    xcolors[i].pixel =
		((i << red_shift) & visual->red_mask) |
		((i << green_shift) & visual->green_mask) |
		((i << blue_shift) & visual->blue_mask);
	}
    } else {
	red_shift = green_shift = blue_shift = 0;
	for (i = 0; i < ncolors; i++)
	    xcolors[i].pixel = i;
    }

    XQueryColors(display, Tk_Colormap(tkwin), xcolors, ncolors);

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
		r = (pixel & visual->red_mask) >> red_shift;
		g = (pixel & visual->green_mask) >> green_shift;
		b = (pixel & visual->blue_mask) >> blue_shift;
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
	    pixelPtr[y * photoBlock.pitch + x * 4 + 3] = alpha;
	}
    }

    TK_PHOTOPUTBLOCK(interp, photoH, &photoBlock, 0, 0, w, h,
	    TK_PHOTO_COMPOSITE_SET);

    Tcl_Free((char *) pixelPtr);
    ckfree((char *) xcolors);
}

#endif /* X11 */

/*
 * Replacement for Tk_TextLayout stuff. Allows the caller to break lines
 * on character boundaries (as well as word boundaries). Allows the caller
 * to specify the maximum number of lines to display. Will add ellipsis "..."
 * to the end of text that is too long to fit (when max lines specified).
 */

typedef struct LayoutChunk
{
    CONST char *start;		/* Pointer to simple string to be displayed.
				 * * This is a pointer into the TkTextLayout's
				 * * string. */
    int numBytes;		/* The number of bytes in this chunk. */
    int numChars;		/* The number of characters in this chunk. */
    int numDisplayChars;	/* The number of characters to display when
				 * * this chunk is displayed.  Can be less than
				 * * numChars if extra space characters were
				 * * absorbed by the end of the chunk.  This
				 * * will be < 0 if this is a chunk that is
				 * * holding a tab or newline. */
    int x, y;			/* The origin of the first character in this
				 * * chunk with respect to the upper-left hand
				 * * corner of the TextLayout. */
    int totalWidth;		/* Width in pixels of this chunk.  Used
				 * * when hit testing the invisible spaces at
				 * * the end of a chunk. */
    int displayWidth;		/* Width in pixels of the displayable
				 * * characters in this chunk.  Can be less than
				 * * width if extra space characters were
				 * * absorbed by the end of the chunk. */
    int ellipsis;		/* TRUE if adding "..." */
} LayoutChunk;

typedef struct LayoutInfo
{
    Tk_Font tkfont;		/* The font used when laying out the text. */
    CONST char *string;		/* The string that was layed out. */
    int numLines;		/* Number of lines */
    int width;			/* The maximum width of all lines in the
				 * * text layout. */
    int height;
    int numChunks;		/* Number of chunks actually used in
				 * * following array. */
    int totalWidth;
#define TEXTLAYOUT_ALLOCHAX
#ifdef TEXTLAYOUT_ALLOCHAX
    int maxChunks;
    struct LayoutInfo *nextFree;
#endif
    LayoutChunk chunks[1];	/* Array of chunks.  The actual size will
				 * * be maxChunks.  THIS FIELD MUST BE THE LAST
				 * * IN THE STRUCTURE. */
} LayoutInfo;

#ifdef TEXTLAYOUT_ALLOCHAX
TCL_DECLARE_MUTEX(textLayoutMutex)
static LayoutInfo *freeLayoutInfo = NULL;
#endif

#ifdef TEXTLAYOUT_ALLOCHAX
static LayoutChunk *NewChunk(LayoutInfo **layoutPtrPtr,
#else
static LayoutChunk *NewChunk(LayoutInfo **layoutPtrPtr, int *maxPtr,
#endif
    CONST char *start, int numBytes, int curX, int newX, int y)
{
    LayoutInfo *layoutPtr;
    LayoutChunk *chunkPtr;
#ifdef TEXTLAYOUT_ALLOCHAX
    int numChars;
#else
    int maxChunks, numChars;
#endif
    size_t s;

    layoutPtr = *layoutPtrPtr;
#ifdef TEXTLAYOUT_ALLOCHAX
    if (layoutPtr->numChunks == layoutPtr->maxChunks)
    {
	layoutPtr->maxChunks *= 2;
	s = sizeof(LayoutInfo) + ((layoutPtr->maxChunks - 1) * sizeof(LayoutChunk));
	layoutPtr = (LayoutInfo *) ckrealloc((char *) layoutPtr, s);

	*layoutPtrPtr = layoutPtr;
    }
#else
    maxChunks = *maxPtr;
    if (layoutPtr->numChunks == maxChunks)
    {
	maxChunks *= 2;
	s = sizeof(LayoutInfo) + ((maxChunks - 1) * sizeof(LayoutChunk));
	layoutPtr = (LayoutInfo *) ckrealloc((char *) layoutPtr, s);

	*layoutPtrPtr = layoutPtr;
	*maxPtr = maxChunks;
    }
#endif
    numChars = Tcl_NumUtfChars(start, numBytes);
    chunkPtr = &layoutPtr->chunks[layoutPtr->numChunks];
    chunkPtr->start = start;
    chunkPtr->numBytes = numBytes;
    chunkPtr->numChars = numChars;
    chunkPtr->numDisplayChars = numChars;
    chunkPtr->x = curX;
    chunkPtr->y = y;
    chunkPtr->totalWidth = newX - curX;
    chunkPtr->displayWidth = newX - curX;
    chunkPtr->ellipsis = FALSE;
    layoutPtr->numChunks++;

    return chunkPtr;
}

TextLayout TextLayout_Compute(
    Tk_Font tkfont,		/* Font that will be used to display text. */
    CONST char *string,		/* String whose dimensions are to be
				** computed. */
    int numChars,		/* Number of characters to consider from
				** string, or < 0 for strlen(). */
    int wrapLength,		/* Longest permissible line length, in
				** pixels.  <= 0 means no automatic wrapping:
				** just let lines get as long as needed. */
    Tk_Justify justify,		/* How to justify lines. */
    int maxLines,
    int flags	/* Flag bits OR-ed together.
		 ** TK_IGNORE_TABS means that tab characters
		 ** should not be expanded.  TK_IGNORE_NEWLINES
		 ** means that newline characters should not
		 ** cause a line break. */
    )
{
    CONST char *start, *end, *special;
    int n, y, bytesThisChunk, maxChunks;
    int baseline, height, curX, newX, maxWidth;
    LayoutInfo *layoutPtr;
    LayoutChunk *chunkPtr;
    Tk_FontMetrics fm;
    Tcl_DString lineBuffer;
    int *lineLengths;
    int curLine;
    int tabWidth = 20; /* FIXME */

    Tcl_DStringInit(&lineBuffer);

    Tk_GetFontMetrics(tkfont, &fm);
    height = fm.ascent + fm.descent;

    if (numChars < 0)
	numChars = Tcl_NumUtfChars(string, -1);
    if (wrapLength == 0)
	wrapLength = -1;

#ifdef TEXTLAYOUT_ALLOCHAX
    Tcl_MutexLock(&textLayoutMutex);
    if (freeLayoutInfo != NULL)
    {
	layoutPtr = freeLayoutInfo;
	freeLayoutInfo = layoutPtr->nextFree;
    }
    else
    {
	maxChunks = 1;
	layoutPtr = (LayoutInfo *) ckalloc(sizeof(LayoutInfo) +
	    (maxChunks - 1) * sizeof(LayoutChunk));
	layoutPtr->maxChunks = maxChunks;
    }
    Tcl_MutexUnlock(&textLayoutMutex);
#else
    maxChunks = 1;

    layoutPtr = (LayoutInfo *) ckalloc(sizeof(LayoutInfo) + (maxChunks -
	    1) * sizeof(LayoutChunk));
#endif
    layoutPtr->tkfont = tkfont;
    layoutPtr->string = string;
    layoutPtr->numChunks = 0;
    layoutPtr->numLines = 0;

    baseline = fm.ascent;
    maxWidth = 0;

    curX = 0;

    end = Tcl_UtfAtIndex(string, numChars);
    special = string;

    flags &= TK_WHOLE_WORDS | TK_IGNORE_TABS | TK_IGNORE_NEWLINES;
    flags |= TK_AT_LEAST_ONE;
    for (start = string; start < end;)
    {
	if (start >= special)
	{
	    for (special = start; special < end; special++)
	    {
		if (!(flags & TK_IGNORE_NEWLINES))
		{
		    if ((*special == '\n') || (*special == '\r'))
			break;
		}
		if (!(flags & TK_IGNORE_TABS))
		{
		    if (*special == '\t')
			break;
		}
	    }
	}

	chunkPtr = NULL;
	if (start < special)
	{
	    bytesThisChunk = Tk_MeasureChars(tkfont, start, special - start,
		wrapLength - curX, flags, &newX);
	    newX += curX;
	    flags &= ~TK_AT_LEAST_ONE;
	    if (bytesThisChunk > 0)
	    {
#ifdef TEXTLAYOUT_ALLOCHAX
		chunkPtr = NewChunk(&layoutPtr, start,
#else
		chunkPtr = NewChunk(&layoutPtr, &maxChunks, start,
#endif
		    bytesThisChunk, curX, newX, baseline);
		start += bytesThisChunk;
		curX = newX;
	    }
	}

	if ((start == special) && (special < end))
	{
	    chunkPtr = NULL;
	    if (*special == '\t')
	    {
		newX = curX + tabWidth;
		newX -= newX % tabWidth;
#ifdef TEXTLAYOUT_ALLOCHAX
		NewChunk(&layoutPtr, start, 1, curX, newX,
#else
		NewChunk(&layoutPtr, &maxChunks, start, 1, curX, newX,
#endif
		    baseline)->numDisplayChars = -1;
		start++;
		if ((start < end) && ((wrapLength <= 0) ||
		    (newX <= wrapLength)))
		{
		    curX = newX;
		    flags &= ~TK_AT_LEAST_ONE;
		    continue;
		}
	    }
	    else
	    {
#ifdef TEXTLAYOUT_ALLOCHAX
		NewChunk(&layoutPtr, start, 1, curX, curX,
#else
		NewChunk(&layoutPtr, &maxChunks, start, 1, curX, curX,
#endif
		    baseline)->numDisplayChars = -1;
		start++;
		goto wrapLine;
	    }
	}

	while ((start < end) && isspace(UCHAR(*start)))
	{
	    if (!(flags & TK_IGNORE_NEWLINES))
	    {
		if ((*start == '\n') || (*start == '\r'))
		    break;
	    }
	    if (!(flags & TK_IGNORE_TABS))
	    {
		if (*start == '\t')
		    break;
	    }
	    start++;
	}
	if (chunkPtr != NULL)
	{
	    CONST char *end;

	    end = chunkPtr->start + chunkPtr->numBytes;
	    bytesThisChunk = start - end;
	    if (bytesThisChunk > 0)
	    {
		bytesThisChunk =
		    Tk_MeasureChars(tkfont, end, bytesThisChunk, -1, 0,
		    &chunkPtr->totalWidth);
		chunkPtr->numBytes += bytesThisChunk;
		chunkPtr->numChars += Tcl_NumUtfChars(end, bytesThisChunk);
		chunkPtr->totalWidth += curX;
	    }
	}

wrapLine:
	flags |= TK_AT_LEAST_ONE;

	if (curX > maxWidth)
	    maxWidth = curX;

	Tcl_DStringAppend(&lineBuffer, (char *) &curX, sizeof(curX));

	curX = 0;
	baseline += height;
	layoutPtr->numLines++;

	if ((maxLines > 0) && (layoutPtr->numLines >= maxLines))
	    break;
    }

    if (start >= end)
    if ((layoutPtr->numChunks > 0) && !(flags & TK_IGNORE_NEWLINES))
    {
	if (layoutPtr->chunks[layoutPtr->numChunks - 1].start[0] == '\n')
	{
	    chunkPtr =
#ifdef TEXTLAYOUT_ALLOCHAX
		NewChunk(&layoutPtr, start, 0, curX, curX,
#else
		NewChunk(&layoutPtr, &maxChunks, start, 0, curX, curX,
#endif
		baseline);
	    chunkPtr->numDisplayChars = -1;
	    Tcl_DStringAppend(&lineBuffer, (char *) &curX, sizeof(curX));
	    baseline += height;
	}
    }

#if 1
    /* Fiddle with chunks on the last line to add ellipsis if there is some
     * text remaining */
    if ((start < end) && (layoutPtr->numChunks > 0))
    {
	char *ellipsis = "...";
	int ellipsisLen = strlen(ellipsis);
	char staticStr[256], *buf = staticStr;

	chunkPtr = &layoutPtr->chunks[layoutPtr->numChunks - 1];
	if (wrapLength > 0)
	{
	    y = chunkPtr->y;
	    for (n = layoutPtr->numChunks - 1; n >= 0; n--)
	    {
		chunkPtr = &layoutPtr->chunks[n];

		/* Only consider the last line */
		if (chunkPtr->y != y)
		    break;

		if (chunkPtr->start[0] == '\n')
		    continue;

		newX = chunkPtr->totalWidth - 1;
		if (chunkPtr->x + chunkPtr->totalWidth < wrapLength)
		    newX = wrapLength - chunkPtr->x;
		bytesThisChunk = Ellipsis(tkfont, (char *) chunkPtr->start,
		    chunkPtr->numBytes, &newX, ellipsis, TRUE);
		if (bytesThisChunk > 0)
		{
		    chunkPtr->numBytes = bytesThisChunk;
		    chunkPtr->numChars = Tcl_NumUtfChars(chunkPtr->start, bytesThisChunk);
		    chunkPtr->numDisplayChars = chunkPtr->numChars;
		    chunkPtr->ellipsis = TRUE;
		    chunkPtr->displayWidth = newX;
		    chunkPtr->totalWidth = newX;
		    lineLengths = (int *) Tcl_DStringValue(&lineBuffer);
		    lineLengths[layoutPtr->numLines - 1] = chunkPtr->x + newX;
		    if (chunkPtr->x + newX > maxWidth)
			maxWidth = chunkPtr->x + newX;
		    break;
		}
	    }
	}
	else
	{
	    if (chunkPtr->start[0] == '\n')
	    {
		if (layoutPtr->numChunks == 1)
		    goto finish;
		if (layoutPtr->chunks[layoutPtr->numChunks - 2].y != chunkPtr->y)
		    goto finish;
		chunkPtr = &layoutPtr->chunks[layoutPtr->numChunks - 2];
	    }

	    if (chunkPtr->numBytes + ellipsisLen > sizeof(staticStr))
		buf = ckalloc(chunkPtr->numBytes + ellipsisLen);
	    memcpy(buf, chunkPtr->start, chunkPtr->numBytes);
	    memcpy(buf + chunkPtr->numBytes, ellipsis, ellipsisLen);
	    Tk_MeasureChars(tkfont, buf,
		chunkPtr->numBytes + ellipsisLen, -1, 0,
		&chunkPtr->displayWidth);
	    chunkPtr->totalWidth = chunkPtr->displayWidth;
	    chunkPtr->ellipsis = TRUE;
	    lineLengths = (int *) Tcl_DStringValue(&lineBuffer);
	    lineLengths[layoutPtr->numLines - 1] = chunkPtr->x + chunkPtr->displayWidth;
	    if (chunkPtr->x + chunkPtr->displayWidth > maxWidth)
		maxWidth = chunkPtr->x + chunkPtr->displayWidth;
	    if (buf != staticStr)
		ckfree(buf);
	}
    }
finish:
#endif

    layoutPtr->width = maxWidth;
    layoutPtr->height = baseline - fm.ascent;
layoutPtr->totalWidth = 0;
    if (layoutPtr->numChunks == 0)
    {
	layoutPtr->height = height;

	layoutPtr->numChunks = 1;
	layoutPtr->chunks[0].start = string;
	layoutPtr->chunks[0].numBytes = 0;
	layoutPtr->chunks[0].numChars = 0;
	layoutPtr->chunks[0].numDisplayChars = -1;
	layoutPtr->chunks[0].x = 0;
	layoutPtr->chunks[0].y = fm.ascent;
	layoutPtr->chunks[0].totalWidth = 0;
	layoutPtr->chunks[0].displayWidth = 0;
    }
    else
    {
	curLine = 0;
	chunkPtr = layoutPtr->chunks;
	y = chunkPtr->y;
	lineLengths = (int *) Tcl_DStringValue(&lineBuffer);
	for (n = 0; n < layoutPtr->numChunks; n++)
	{
	    int extra;

	    if (chunkPtr->y != y)
	    {
		curLine++;
		y = chunkPtr->y;
	    }
	    extra = maxWidth - lineLengths[curLine];
	    if (justify == TK_JUSTIFY_CENTER)
	    {
		chunkPtr->x += extra / 2;
	    }
	    else if (justify == TK_JUSTIFY_RIGHT)
	    {
		chunkPtr->x += extra;
	    }
if (chunkPtr->x + chunkPtr->totalWidth > layoutPtr->totalWidth)
    layoutPtr->totalWidth = chunkPtr->x + chunkPtr->totalWidth;
	    chunkPtr++;
	}
/* dbwin("totalWidth %d displayWidth %d\n", layoutPtr->totalWidth, maxWidth); */
    Tcl_DStringFree(&lineBuffer);
    }

    /* We don't want single-line text layouts for text elements, but it happens for column titles */
/*	if (layoutPtr->numLines == 1)
	dbwin("WARNING: single-line TextLayout created\n"); */

    return (TextLayout) layoutPtr;
}

void TextLayout_Free(TextLayout textLayout)
{
    LayoutInfo *layoutPtr = (LayoutInfo *) textLayout;

#ifdef TEXTLAYOUT_ALLOCHAX
    Tcl_MutexLock(&textLayoutMutex);
    layoutPtr->nextFree = freeLayoutInfo;
    freeLayoutInfo = layoutPtr;
    Tcl_MutexUnlock(&textLayoutMutex);
#else
    ckfree((char *) layoutPtr);
#endif
}

void TextLayout_Size(TextLayout textLayout, int *widthPtr, int *heightPtr)
{
    LayoutInfo *layoutPtr = (LayoutInfo *) textLayout;

    if (widthPtr != NULL)
	(*widthPtr) = layoutPtr->width;
    if (heightPtr != NULL)
	(*heightPtr) = layoutPtr->height;
}

int TextLayout_TotalWidth(TextLayout textLayout)
{
    LayoutInfo *layoutPtr = (LayoutInfo *) textLayout;

    return layoutPtr->totalWidth;
}

void TextLayout_Draw(
    Display *display,		/* Display on which to draw. */
    Drawable drawable,		/* Window or pixmap in which to draw. */
    GC gc,			/* Graphics context to use for drawing text. */
    TextLayout layout,		/* Layout information, from a previous call
				 * * to Tk_ComputeTextLayout(). */
    int x, int y,		/* Upper-left hand corner of rectangle in
				 * * which to draw (pixels). */
    int firstChar,		/* The index of the first character to draw
				 * * from the given text item.  0 specfies the
				 * * beginning. */
    int lastChar,		/* The index just after the last character
				 * * to draw from the given text item.  A number
				 * * < 0 means to draw all characters. */
    int underline		/* Character index to underline, or < 0 for
				 * no underline. */
)
{
    LayoutInfo *layoutPtr = (LayoutInfo *) layout;
    int i, numDisplayChars, drawX;
    CONST char *firstByte;
    CONST char *lastByte;
    LayoutChunk *chunkPtr;

    if (lastChar < 0)
	lastChar = 100000000;
    chunkPtr = layoutPtr->chunks;
    for (i = 0; i < layoutPtr->numChunks; i++)
    {
	numDisplayChars = chunkPtr->numDisplayChars;
	if ((numDisplayChars > 0) && (firstChar < numDisplayChars))
	{
	    if (firstChar <= 0)
	    {
		drawX = 0;
		firstChar = 0;
		firstByte = chunkPtr->start;
	    }
	    else
	    {
		firstByte = Tcl_UtfAtIndex(chunkPtr->start, firstChar);
		Tk_MeasureChars(layoutPtr->tkfont, chunkPtr->start,
		    firstByte - chunkPtr->start, -1, 0, &drawX);
	    }
	    if (lastChar < numDisplayChars)
		numDisplayChars = lastChar;
	    lastByte = Tcl_UtfAtIndex(chunkPtr->start, numDisplayChars);
#if 1
	    if (chunkPtr->ellipsis)
	    {
		char staticStr[256], *buf = staticStr;
		char *ellipsis = "...";
		int ellipsisLen = strlen(ellipsis);

		if ((lastByte - firstByte) + ellipsisLen > sizeof(staticStr))
		    buf = ckalloc((lastByte - firstByte) + ellipsisLen);
		memcpy(buf, firstByte, (lastByte - firstByte));
		memcpy(buf + (lastByte - firstByte), ellipsis, ellipsisLen);
		Tk_DrawChars(display, drawable, gc, layoutPtr->tkfont,
		    buf, (lastByte - firstByte) + ellipsisLen,
		    x + chunkPtr->x + drawX, y + chunkPtr->y);
		if (buf != staticStr)
		    ckfree(buf);
	    }
	    else
#endif
	    Tk_DrawChars(display, drawable, gc, layoutPtr->tkfont,
		firstByte, lastByte - firstByte, x + chunkPtr->x + drawX,
		y + chunkPtr->y);
#if 1
	    if (underline >= firstChar && underline < numDisplayChars)
	    {
		CONST char *fstBytePtr = Tcl_UtfAtIndex(chunkPtr->start, underline);
		CONST char *sndBytePtr = Tcl_UtfNext(fstBytePtr);
		Tk_UnderlineChars(display, drawable, gc,
			layoutPtr->tkfont, firstByte,
			x + chunkPtr->x + drawX, y + chunkPtr->y, 
			fstBytePtr - chunkPtr->start, sndBytePtr - chunkPtr->start);
	    }
#endif
	}
	firstChar -= chunkPtr->numChars;
	lastChar -= chunkPtr->numChars;
#if 1
	underline -= chunkPtr->numChars;
#endif
	if (lastChar <= 0)
	    break;
	chunkPtr++;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreeCtrl_GetPadAmountFromObj --
 *
 *	Parse a pad amount configuration option.
 *	A pad amount (typically the value of an option -XXXpadx or
 *	-XXXpady, where XXX may be a possibly empty string) can
 *	be either a single pixel width, or a list of two pixel widths.
 *	If a single pixel width, the amount specified is used for 
 *	padding on both sides.  If two amounts are specified, then
 *	they specify the left/right or top/bottom padding.
 *
 * Results:
 *	Standard Tcl Result.
 *
 * Side effects:
 *	Sets internal representation of the object. In case of an error
 *	the result of the interpreter is modified.
 *
 *----------------------------------------------------------------------
 */

int
TreeCtrl_GetPadAmountFromObj(interp, tkwin, padObj, topLeftPtr, bottomRightPtr)
    Tcl_Interp *interp;		/* Interpreter for error reporting, or NULL,
				 * if no error message is wanted. */
    Tk_Window tkwin;		/* A window.  Needed by Tk_GetPixels() */
    Tcl_Obj *padObj;		/* Object containing a pad amount. */
    int *topLeftPtr;		/* Pointer to the location, where to store the
				   first component of the padding. */
    int *bottomRightPtr;	/* Pointer to the location, where to store the
				   second component of the padding. */
{
    int padc;			/* Number of element objects in padv. */
    Tcl_Obj **padv;		/* Pointer to the element objects of the
				 * parsed pad amount value. */
	int topLeft, bottomRight;

    if (Tcl_ListObjGetElements(interp, padObj, &padc, &padv) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * The value specifies a non empty string.
     * Check that this string is indeed a valid pad amount.
     */

    if (padc < 1 || padc > 2) {
	if (interp != NULL) {
	error:
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "bad pad amount \"",
		Tcl_GetString(padObj), "\": must be a list of ",
		"1 or 2 positive screen distances", (char *) NULL);
	}
	return TCL_ERROR;
    }
    if ((Tk_GetPixelsFromObj(interp, tkwin, padv[0], &topLeft)
	     != TCL_OK) || (topLeft < 0)) {
	goto error;
    }
    if (padc == 2) {
	if ((Tk_GetPixelsFromObj(interp, tkwin, padv[1], &bottomRight)
		!= TCL_OK) || (bottomRight < 0)) {
	    goto error;
	}
    } else {
	bottomRight = topLeft;
    }
    (*topLeftPtr) = topLeft;
    (*bottomRightPtr) = bottomRight;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TreeCtrl_NewPadAmountObj --
 *
 *	Create a Tcl object with an internal representation, that
 *	corresponds to a pad amount, i.e. an integer Tcl_Obj or a
 *	list Tcl_Obj with 2 elements.
 *
 * Results:
 *	The created object.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TreeCtrl_NewPadAmountObj(padAmounts)
    int *padAmounts;		/* Internal form of a pad amount. */
{
    Tcl_Obj *newObj;

    /*
     * If both values are the same, create a list with one value,
     * otherwise create a two element list with the top/left value
     * first followed by the bottom/right value.
     */

    if (padAmounts[PAD_TOP_LEFT] == padAmounts[PAD_BOTTOM_RIGHT]) {
	newObj = Tcl_NewIntObj(padAmounts[PAD_TOP_LEFT]);
    } else {
	newObj = Tcl_NewObj();
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, newObj,
	    Tcl_NewIntObj(padAmounts[PAD_TOP_LEFT]));
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, newObj,
	    Tcl_NewIntObj(padAmounts[PAD_BOTTOM_RIGHT]));
    }
    return newObj;
}

/*
 *----------------------------------------------------------------------
 *
 * PadAmountOptionSet --
 * PadAmountOptionGet --
 * PadAmountOptionRestore --
 * PadAmountOptionFree --
 *
 *	Handlers for object-based pad amount configuration options.
 *	A pad amount (typically the value of an option -XXXpadx or
 *	-XXXpady, where XXX may be a possibly empty string) can
 *	be either a single pixel width, or a list of two pixel widths.
 *	If a single pixel width, the amount specified is used for 
 *	padding on both sides.  If two amounts are specified, then
 *	they specify the left/right or top/bottom padding.
 *
 * Results:
 *	See user documentation for expected results from these functions.
 *		PadAmountOptionSet	Standard Tcl Result.
 *		PadAmountOptionGet	Tcl_Obj * containing a valid internal
 *					representation of the pad amount.
 *		PadAmountOptionRestore	None.
 *		PadAmountOptionFree	None.
 *
 * Side effects:
 *	Depends on the function.
 *		PadAmountOptionSet	Sets option value to new setting,
 *					allocating a new integer array.
 *		PadAmountOptionGet	Creates a new Tcl_Obj.
 *		PadAmountOptionRestore	Resets option value to original value.
 *		PadAmountOptionFree	Free storage for internal rep.
 *
 *----------------------------------------------------------------------
 */

static int
PadAmountOptionSet(clientData, interp, tkwin, valuePtr, recordPtr,
		   internalOffset, saveInternalPtr, flags)
    ClientData clientData;	/* unused. */
    Tcl_Interp *interp;		/* Interpreter for error reporting, or NULL,
				 * if no error message is wanted. */
    Tk_Window tkwin;		/* A window.  Needed by Tk_GetPixels() */
    Tcl_Obj **valuePtr;		/* The argument to "-padx", "-pady", "-ipadx",
				 * or "-ipady".  The thing to be parsed. */
    char *recordPtr;		/* Pointer to start of widget record. */
    int internalOffset;		/* Offset of internal representation or
				 * -1, if no internal repr is wanted. */
    char *saveInternalPtr;	/* Pointer to the place, where the saved
				 * internal form (of type "int *") resides. */
    int flags;			/* Flags as specified in Tk_OptionSpec. */
{
    int topLeft, bottomRight;	/* The two components of the padding. */
    int *new;			/* Pointer to the allocated array of integers
				 * containing the parsed pad amounts. */
    int **internalPtr;		/* Pointer to the place, where the internal
				 * form (of type "int *") resides. */

    /*
     * Check that the given object indeed specifies a valid pad amount.
     */

    if (TreeCtrl_GetPadAmountFromObj(interp, tkwin, *valuePtr,
	    &topLeft, &bottomRight) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Store a pointer to an allocated array of the two padding values
     * into the widget record at the specified offset.
     */

    if (internalOffset >= 0) {
	internalPtr = (int **) (recordPtr + internalOffset);
	*(int **) saveInternalPtr = *internalPtr;
	new = (int *) ckalloc(2 * sizeof(int));
	new[PAD_TOP_LEFT]     = topLeft;
	new[PAD_BOTTOM_RIGHT] = bottomRight;
	*internalPtr = new;
    }
    return TCL_OK;
}

static Tcl_Obj *
PadAmountOptionGet(clientData, tkwin, recordPtr, internalOffset)
    ClientData clientData;	/* unused. */
    Tk_Window tkwin;		/* A window; unused. */
    char *recordPtr;		/* Pointer to start of widget record. */
    int internalOffset;		/* Offset of internal representation. */
{
    int *padAmounts = *(int **)(recordPtr + internalOffset);

    return TreeCtrl_NewPadAmountObj(padAmounts);
}

static void
PadAmountOptionRestore(clientData, tkwin, internalPtr, saveInternalPtr)
    ClientData clientData;	/* unused. */
    Tk_Window tkwin;		/* A window; unused. */
    char *internalPtr;		/* Pointer to the place, where the internal
				 * form (of type "int *") resides. */
    char *saveInternalPtr;	/* Pointer to the place, where the saved
				 * internal form (of type "int *") resides. */
{
    *(int **) internalPtr = *(int **) saveInternalPtr;
}

static void
PadAmountOptionFree(clientData, tkwin, internalPtr)
    ClientData clientData;	/* unused. */
    Tk_Window tkwin;		/* A window; unused */
    char *internalPtr;		/* Pointer to the place, where the internal
				 * form (of type "int *") resides. */
{
    if (*(int **)internalPtr != NULL) {
	ckfree((char *) *(int **)internalPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ObjectIsEmpty --
 *
 *	This procedure tests whether the string value of an object is
 *	empty.
 *
 * Results:
 *	The return value is 1 if the string value of objPtr has length
 *	zero, and 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ObjectIsEmpty(
    Tcl_Obj *obj		/* Object to test.  May be NULL. */
    )
{
    int length;

    if (obj == NULL)
	return 1;
    if (obj->bytes != NULL)
	return (obj->length == 0);
    Tcl_GetStringFromObj(obj, &length);
    return (length == 0);
}

#define PERSTATE_ROUNDUP 5

/*
 *----------------------------------------------------------------------
 *
 * PerStateInfo_Free --
 *
 *	Frees memory and resources associated with a single per-state
 *	option. pInfo is set to an empty state ready to be used again.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated. Colors, etc are freed.
 *
 *----------------------------------------------------------------------
 */

void
PerStateInfo_Free(
    TreeCtrl *tree,		/* Widget info. */
    PerStateType *typePtr,	/* Type-specific functions and values. */
    PerStateInfo *pInfo		/* Per-state info to free. */
    )
{
    PerStateData *pData = pInfo->data;
    int i;

    if (pInfo->data == NULL)
	return;
#ifdef TREECTRL_DEBUG
    if (pInfo->type != typePtr)
	panic("PerStateInfo_Free type mismatch: got %s expected %s",
		pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
#endif
    for (i = 0; i < pInfo->count; i++) {
	(*typePtr->freeProc)(tree, pData);
	pData = (PerStateData *) (((char *) pData) + typePtr->size);
    }
#ifdef ALLOC_HAX
    AllocHax_CFree(tree->allocData, typePtr->name, (char *) pInfo->data, typePtr->size,
	pInfo->count, PERSTATE_ROUNDUP);
#else
    WIPEFREE(pInfo->data, typePtr->size * pInfo->count);
#endif
    pInfo->data = NULL;
    pInfo->count = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * PerStateInfo_FromObj --
 *
 *	Parse a Tcl_Obj to an array of PerStateData. The current data
 *	is freed (if any). If the Tcl_Obj is NULL then pInfo is left in
 *	an empty state ready to be used again.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Memory is allocated/deallocated.
 *
 *----------------------------------------------------------------------
 */

int
PerStateInfo_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    StateFromObjProc proc,	/* Procedure used to turn a Tcl_Obj into
				 * a state bit-flag. */
    PerStateType *typePtr,	/* Type-specific functions and values. */
    PerStateInfo *pInfo		/* Per-state info to return. pInfo->obj
				 * must be NULL or point to a valid Tcl_Obj. */
    )
{
    int i, j;
    int objc, objc2;
    Tcl_Obj **objv, **objv2;
    PerStateData *pData;

#ifdef TREECTRL_DEBUG
    pInfo->type = typePtr;
#endif

    PerStateInfo_Free(tree, typePtr, pInfo);

    if (pInfo->obj == NULL)
	return TCL_OK;

    if (Tcl_ListObjGetElements(tree->interp, pInfo->obj, &objc, &objv) != TCL_OK)
	return TCL_ERROR;

    if (objc == 0)
	return TCL_OK;

    if (objc == 1) {
#ifdef ALLOC_HAX
	pData = (PerStateData *) AllocHax_CAlloc(tree->allocData,
	    typePtr->name, typePtr->size, 1, PERSTATE_ROUNDUP);
#else
	pData = (PerStateData *) ckalloc(typePtr->size);
#endif
	pData->stateOff = pData->stateOn = 0; /* all states */
	if ((*typePtr->fromObjProc)(tree, objv[0], pData) != TCL_OK) {
#ifdef ALLOC_HAX
	    AllocHax_CFree(tree->allocData, typePtr->name, (char *) pData,
		typePtr->size, 1, PERSTATE_ROUNDUP);
#else
	    WIPEFREE(pData, typePtr->size);
#endif
	    return TCL_ERROR;
	}
	pInfo->data = pData;
	pInfo->count = 1;
	return TCL_OK;
    }

    if (objc & 1) {
	FormatResult(tree->interp, "list must have even number of elements");
	return TCL_ERROR;
    }

#ifdef ALLOC_HAX
    pData = (PerStateData *) AllocHax_CAlloc(tree->allocData,
	typePtr->name, typePtr->size, objc / 2, PERSTATE_ROUNDUP);
#else
    pData = (PerStateData *) ckalloc(typePtr->size * (objc / 2));
#endif
    pInfo->data = pData;
    for (i = 0; i < objc; i += 2) {
	if ((*typePtr->fromObjProc)(tree, objv[i], pData) != TCL_OK) {
	    goto freeIt;
	}
	pInfo->count++;
	if (Tcl_ListObjGetElements(tree->interp, objv[i + 1], &objc2, &objv2) != TCL_OK) {
	    goto freeIt;
	}
	pData->stateOff = pData->stateOn = 0; /* all states */
	for (j = 0; j < objc2; j++) {
	    if (proc(tree, objv2[j], &pData->stateOff, &pData->stateOn) != TCL_OK) {
		goto freeIt;
	    }
	}
	pData = (PerStateData *) (((char *) pData) + typePtr->size);
    }
    return TCL_OK;

freeIt:
    pData = pInfo->data;
    for (i = 0; i < pInfo->count; i++) {
	(*typePtr->freeProc)(tree, pData);
	pData = (PerStateData *) (((char *) pData) + typePtr->size);
    }
#ifdef ALLOC_HAX
    AllocHax_CFree(tree->allocData, typePtr->name, (char *) pInfo->data,
	typePtr->size, objc / 2, PERSTATE_ROUNDUP);
#else
    WIPEFREE(pInfo->data, typePtr->size * (objc / 2));
#endif
    pInfo->data = NULL;
    pInfo->count = 0;
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * PerStateInfo_ForState --
 *
 *	Return the best-matching PerStateData for a given state.
 *
 * Results:
 *	The return value is a pointer to the best-matching PerStateData.
 *	*match is set to a MATCH_xxx constant. NULL is returned if
 *	no appropriate PerStateData was found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

PerStateData *
PerStateInfo_ForState(
    TreeCtrl *tree,		/* Widget info. */
    PerStateType *typePtr,	/* Type-specific functions and values. */
    PerStateInfo *pInfo,	/* Per-state info to search. */
    int state,			/* State bit-flags to compare. */
    int *match			/* Returned MATCH_xxx constant. */
    )
{
    PerStateData *pData = pInfo->data;
    int stateOff = ~state, stateOn = state;
    int i;

#ifdef TREECTRL_DEBUG
    if ((pInfo->data != NULL) && (pInfo->type != typePtr)) {
	panic("PerStateInfo_ForState type mismatch: got %s expected %s",
		pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
    }
#endif

    for (i = 0; i < pInfo->count; i++) {
	/* Any state */
	if ((pData->stateOff == 0) &&
		(pData->stateOn == 0)) {
	    if (match) (*match) = MATCH_ANY;
	    return pData;
	}

	/* Exact match */
	if ((pData->stateOff == stateOff) &&
		(pData->stateOn == stateOn)) {
	    if (match) (*match) = MATCH_EXACT;
	    return pData;
	}

	/* Partial match */
	if (((pData->stateOff & stateOff) == pData->stateOff) &&
		((pData->stateOn & stateOn) == pData->stateOn)) {
	    if (match) (*match) = MATCH_PARTIAL;
	    return pData;
	}

	pData = (PerStateData *) (((char *) pData) + typePtr->size);
    }

    if (match) (*match) = MATCH_NONE;
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * PerStateInfo_ObjForState --
 *
 *	Return a Tcl_Obj from the list object that was parsed by
 *	PerStateInfo_FromObj(). pInfo is searched for the best-matching
 *	PerStateData for the given state and the object used to
 *	create that PerStateData is returned.
 *
 * Results:
 *	*match is set to a MATCH_xxx constant. NULL is returned if
 *	no appropriate PerStateData was found. The object should not
 *	be modified by the caller.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
PerStateInfo_ObjForState(
    TreeCtrl *tree,		/* Widget info. */
    PerStateType *typePtr,	/* Type-specific functions and values. */
    PerStateInfo *pInfo,	/* Per-state info to search. */
    int state,			/* State bit-flags to compare. */
    int *match			/* Returned MATCH_xxx constant. */
    )
{
    PerStateData *pData;
    Tcl_Obj *obj;
    int i;

#ifdef TREECTRL_DEBUG
    if ((pInfo->data != NULL) && (pInfo->type != typePtr))
	panic("PerStateInfo_ObjForState type mismatch: got %s expected %s",
		pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
#endif

    pData = PerStateInfo_ForState(tree, typePtr, pInfo, state, match);
    if (pData != NULL) {
	i = ((char *) pData - (char *) pInfo->data) / typePtr->size;
	Tcl_ListObjIndex(tree->interp, pInfo->obj, i * 2, &obj);
	return obj;
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * PerStateInfo_Undefine --
 *
 *	Called when a user-defined state flag is undefined. The state
 *	flag is cleared from every PerStateData using that flag. The
 *	list object that was parsed by PerStateInfo_FromObj() is modified
 *	by removing any reference to the undefined state.
 *
 * Results:
 *	The return value is a boolean indicating whether or not pInfo
 *	was modified.
 *
 * Side effects:
 *	The list object pointed to by pInfo->obj may be recreated.
 *
 *----------------------------------------------------------------------
 */

int PerStateInfo_Undefine(
    TreeCtrl *tree,		/* Widget info. */
    PerStateType *typePtr,	/* Type-specific functions and values. */
    PerStateInfo *pInfo,	/* Per-state info to modify. */
    int state			/* State bit-flag that was undefined. */
    )
{
    PerStateData *pData = pInfo->data;
    int i, j, numStates, stateOff, stateOn;
    Tcl_Obj *configObj = pInfo->obj, *listObj, *stateObj;
    int modified = 0;

#ifdef TREECTRL_DEBUG
    if ((pInfo->data != NULL) && (pInfo->type != typePtr))
	panic("PerStateInfo_Undefine type mismatch: got %s expected %s",
		pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
#endif

    for (i = 0; i < pInfo->count; i++) {
	if ((pData->stateOff | pData->stateOn) & state) {
	    pData->stateOff &= ~state;
	    pData->stateOn &= ~state;
	    if (Tcl_IsShared(configObj)) {
		configObj = Tcl_DuplicateObj(configObj);
		Tcl_DecrRefCount(pInfo->obj);
		Tcl_IncrRefCount(configObj);
		pInfo->obj = configObj;
	    }
	    Tcl_ListObjIndex(tree->interp, configObj, i * 2 + 1, &listObj);
	    if (Tcl_IsShared(listObj)) {
		listObj = Tcl_DuplicateObj(listObj);
		Tcl_ListObjReplace(tree->interp, configObj, i * 2 + 1, 1, 1, &listObj);
	    }
	    Tcl_ListObjLength(tree->interp, listObj, &numStates);
	    for (j = 0; j < numStates; ) {
		Tcl_ListObjIndex(tree->interp, listObj, j, &stateObj);
		stateOff = stateOn = 0;
		TreeStateFromObj(tree, stateObj, &stateOff, &stateOn);
		if ((stateOff | stateOn) & state) {
		    Tcl_ListObjReplace(tree->interp, listObj, j, 1, 0, NULL);
		    numStates--;
		} else
		    j++;
	    }
	    /* Given {bitmap {state1 state2 state3}}, we just invalidated
	     * the string rep of the sublist {state1 state2 state3}, but not
	     * the parent list. */
	    Tcl_InvalidateStringRep(configObj);
	    modified = 1;
	}
	pData = (PerStateData *) (((char *) pData) + typePtr->size);
    }
    return modified;
}

/*****/

void PerStateGC_Free(TreeCtrl *tree, struct PerStateGC **pGCPtr)
{
    struct PerStateGC *pGC = (*pGCPtr), *next;

    while (pGC != NULL) {
	next = pGC->next;
	Tk_FreeGC(tree->display, pGC->gc);
	WFREE(pGC, struct PerStateGC);
	pGC = next;
    }
    (*pGCPtr) = NULL;
}

GC PerStateGC_Get(TreeCtrl *tree, struct PerStateGC **pGCPtr, unsigned long mask, XGCValues *gcValues)
{
    struct PerStateGC *pGC;

    if ((mask | (GCFont | GCForeground | GCBackground | GCGraphicsExposures)) != 
	    (GCFont | GCForeground | GCBackground | GCGraphicsExposures))
	panic("PerStateGC_Get: unsupported mask");

    for (pGC = (*pGCPtr); pGC != NULL; pGC = pGC->next) {
	if (mask != pGC->mask)
	    continue;
	if ((mask & GCFont) &&
		(pGC->gcValues.font != gcValues->font))
	    continue;
	if ((mask & GCForeground) &&
		(pGC->gcValues.foreground != gcValues->foreground))
	    continue;
	if ((mask & GCBackground) &&
		(pGC->gcValues.background != gcValues->background))
	    continue;
	if ((mask & GCGraphicsExposures) &&
		(pGC->gcValues.graphics_exposures != gcValues->graphics_exposures))
	    continue;
	return pGC->gc;
    }

    pGC = (struct PerStateGC *) ckalloc(sizeof(*pGC));
    pGC->gcValues = (*gcValues);
    pGC->mask = mask;
    pGC->gc = Tk_GetGC(tree->tkwin, mask, gcValues);
    pGC->next = (*pGCPtr);
    (*pGCPtr) = pGC;

    return pGC->gc;
}

/*****/

typedef struct PerStateDataBitmap PerStateDataBitmap;
struct PerStateDataBitmap
{
    PerStateData header;
    Pixmap bitmap;
};

static int PSDBitmapFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataBitmap *pBitmap)
{
    if (ObjectIsEmpty(obj)) {
	/* Specify empty string to override masterX */
	pBitmap->bitmap = None;
    } else {
	pBitmap->bitmap = Tk_AllocBitmapFromObj(tree->interp, tree->tkwin, obj);
	if (pBitmap->bitmap == None)
	    return TCL_ERROR;
    }
    return TCL_OK;
}

static void PSDBitmapFree(TreeCtrl *tree, PerStateDataBitmap *pBitmap)
{
    if (pBitmap->bitmap != None)
	Tk_FreeBitmap(tree->display, pBitmap->bitmap);
}

PerStateType pstBitmap =
{
    "pstBitmap",
    sizeof(PerStateDataBitmap),
    (PerStateType_FromObjProc) PSDBitmapFromObj,
    (PerStateType_FreeProc) PSDBitmapFree
};

Pixmap PerStateBitmap_ForState(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateDataBitmap *pData;

    pData = (PerStateDataBitmap *) PerStateInfo_ForState(tree, &pstBitmap, pInfo, state, match);
    if (pData != NULL)
	return pData->bitmap;
    return None;
}

void PerStateBitmap_MaxSize(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int *widthPtr,
    int *heightPtr)
{
    PerStateDataBitmap *pData = (PerStateDataBitmap *) pInfo->data;
    int i, width, height, w, h;

    width = height = 0;

    for (i = 0; i < pInfo->count; i++, ++pData) {
	if (pData->bitmap == None)
	    continue;
	Tk_SizeOfBitmap(tree->display, pData->bitmap, &w, &h);
	width = MAX(width, w);
	height = MAX(height, h);
    }

    (*widthPtr) = width;
    (*heightPtr) = height;
}

/*****/

typedef struct PerStateDataBoolean PerStateDataBoolean;
struct PerStateDataBoolean
{
    PerStateData header;
    int value;
};

static int PSDBooleanFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataBoolean *pBoolean)
{
    if (ObjectIsEmpty(obj)) {
	pBoolean->value = -1;
    } else {
	if (Tcl_GetBooleanFromObj(tree->interp, obj, &pBoolean->value) != TCL_OK)
	    return TCL_ERROR;
    }
    return TCL_OK;
}

static void PSDBooleanFree(TreeCtrl *tree, PerStateDataBoolean *pBoolean)
{
}

PerStateType pstBoolean =
{
    "pstBoolean",
    sizeof(PerStateDataBoolean),
    (PerStateType_FromObjProc) PSDBooleanFromObj,
    (PerStateType_FreeProc) PSDBooleanFree
};

int PerStateBoolean_ForState(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateDataBoolean *pData;

    pData = (PerStateDataBoolean *) PerStateInfo_ForState(tree, &pstBoolean, pInfo, state, match);
    if (pData != NULL)
	return pData->value;
    return -1;
}

/*****/

typedef struct PerStateDataBorder PerStateDataBorder;
struct PerStateDataBorder
{
    PerStateData header;
    Tk_3DBorder border;
};

static int PSDBorderFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataBorder *pBorder)
{
    if (ObjectIsEmpty(obj)) {
	/* Specify empty string to override masterX */
	pBorder->border = NULL;
    } else {
	pBorder->border = Tk_Alloc3DBorderFromObj(tree->interp, tree->tkwin, obj);
	if (pBorder->border == NULL)
	    return TCL_ERROR;
    }
    return TCL_OK;
}

static void PSDBorderFree(TreeCtrl *tree, PerStateDataBorder *pBorder)
{
    if (pBorder->border != NULL)
	Tk_Free3DBorder(pBorder->border);
}

PerStateType pstBorder =
{
    "pstBorder",
    sizeof(PerStateDataBorder),
    (PerStateType_FromObjProc) PSDBorderFromObj,
    (PerStateType_FreeProc) PSDBorderFree
};

Tk_3DBorder PerStateBorder_ForState(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateDataBorder *pData;

    pData = (PerStateDataBorder *) PerStateInfo_ForState(tree, &pstBorder, pInfo, state, match);
    if (pData != NULL)
	return pData->border;
    return NULL;
}

/*****/

typedef struct PerStateDataColor PerStateDataColor;
struct PerStateDataColor
{
    PerStateData header;
    XColor *color;
};

static int PSDColorFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataColor *pColor)
{
    if (ObjectIsEmpty(obj)) {
	/* Specify empty string to override masterX */
	pColor->color = NULL;
    } else {
	pColor->color = Tk_AllocColorFromObj(tree->interp, tree->tkwin, obj);
	if (pColor->color == NULL)
	    return TCL_ERROR;
    }
    return TCL_OK;
}

static void PSDColorFree(TreeCtrl *tree, PerStateDataColor *pColor)
{
    if (pColor->color != NULL)
	Tk_FreeColor(pColor->color);
}

PerStateType pstColor =
{
    "pstColor",
    sizeof(PerStateDataColor),
    (PerStateType_FromObjProc) PSDColorFromObj,
    (PerStateType_FreeProc) PSDColorFree
};

XColor *PerStateColor_ForState(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateDataColor *pData;

    pData = (PerStateDataColor *) PerStateInfo_ForState(tree, &pstColor, pInfo, state, match);
    if (pData != NULL)
	return pData->color;
    return NULL;
}

/*****/

typedef struct PerStateDataFont PerStateDataFont;
struct PerStateDataFont
{
    PerStateData header;
    Tk_Font tkfont;
};

static int PSDFontFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataFont *pFont)
{
    if (ObjectIsEmpty(obj)) {
	/* Specify empty string to override masterX */
	pFont->tkfont = NULL;
    } else {
	pFont->tkfont = Tk_AllocFontFromObj(tree->interp, tree->tkwin, obj);
	if (pFont->tkfont == NULL)
	    return TCL_ERROR;
    }
    return TCL_OK;
}

static void PSDFontFree(TreeCtrl *tree, PerStateDataFont *pFont)
{
    if (pFont->tkfont != NULL)
	Tk_FreeFont(pFont->tkfont);
}

PerStateType pstFont =
{
    "pstFont",
    sizeof(PerStateDataFont),
    (PerStateType_FromObjProc) PSDFontFromObj,
    (PerStateType_FreeProc) PSDFontFree
};

Tk_Font PerStateFont_ForState(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateDataFont *pData;

    pData = (PerStateDataFont *) PerStateInfo_ForState(tree, &pstFont, pInfo, state, match);
    if (pData != NULL)
	return pData->tkfont;
    return NULL;
}

/*****/

typedef struct PerStateDataImage PerStateDataImage;
struct PerStateDataImage
{
    PerStateData header;
    Tk_Image image;
    char *string;
};

static int PSDImageFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataImage *pImage)
{
    int length;
    char *string;

    if (ObjectIsEmpty(obj)) {
	/* Specify empty string to override masterX */
	pImage->image = NULL;
	pImage->string = NULL;
    } else {
	string = Tcl_GetStringFromObj(obj, &length);
	pImage->image = Tree_GetImage(tree, string);
	if (pImage->image == NULL)
	    return TCL_ERROR;
	pImage->string = ckalloc(length + 1);
	strcpy(pImage->string, string);
    }
    return TCL_OK;
}

static void PSDImageFree(TreeCtrl *tree, PerStateDataImage *pImage)
{
    if (pImage->string != NULL)
	ckfree(pImage->string);
    if (pImage->image != NULL)
	Tree_FreeImage(tree, pImage->image);
}

PerStateType pstImage =
{
    "pstImage",
    sizeof(PerStateDataImage),
    (PerStateType_FromObjProc) PSDImageFromObj,
    (PerStateType_FreeProc) PSDImageFree
};

Tk_Image PerStateImage_ForState(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateDataImage *pData;

    pData = (PerStateDataImage *) PerStateInfo_ForState(tree, &pstImage, pInfo, state, match);
    if (pData != NULL)
	return pData->image;
    return NULL;
}

void PerStateImage_MaxSize(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int *widthPtr,
    int *heightPtr)
{
    PerStateDataImage *pData = (PerStateDataImage *) pInfo->data;
    int i, width, height, w, h;

    width = height = 0;

    for (i = 0; i < pInfo->count; i++, ++pData) {
	if (pData->image == None)
	    continue;
	Tk_SizeOfImage(pData->image, &w, &h);
	width = MAX(width, w);
	height = MAX(height, h);
    }

    (*widthPtr) = width;
    (*heightPtr) = height;
}

/*****/

typedef struct PerStateDataRelief PerStateDataRelief;
struct PerStateDataRelief
{
    PerStateData header;
    int relief;
};

static int PSDReliefFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataRelief *pRelief)
{
    if (ObjectIsEmpty(obj)) {
	/* Specify empty string to override masterX */
	pRelief->relief = TK_RELIEF_NULL;
    } else {
	if (Tk_GetReliefFromObj(tree->interp, obj, &pRelief->relief) != TCL_OK)
	    return TCL_ERROR;
    }
    return TCL_OK;
}

static void PSDReliefFree(TreeCtrl *tree, PerStateDataRelief *pRelief)
{
}

PerStateType pstRelief =
{
    "pstRelief",
    sizeof(PerStateDataRelief),
    (PerStateType_FromObjProc) PSDReliefFromObj,
    (PerStateType_FreeProc) PSDReliefFree
};

int PerStateRelief_ForState(
    TreeCtrl *tree,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateDataRelief *pData;

    pData = (PerStateDataRelief *) PerStateInfo_ForState(tree, &pstRelief, pInfo, state, match);
    if (pData != NULL)
	return pData->relief;
    return TK_RELIEF_NULL;
}

/*****/

void PSTSave(
    PerStateInfo *pInfo,
    PerStateInfo *pSave)
{
#ifdef TREECTRL_DEBUG
    pSave->type = pInfo->type; /* could be NULL */
#endif
    pSave->data = pInfo->data;
    pSave->count = pInfo->count;
    pInfo->data = NULL;
    pInfo->count = 0;
}

void PSTRestore(
    TreeCtrl *tree,
    PerStateType *typePtr,
    PerStateInfo *pInfo,
    PerStateInfo *pSave)
{
    PerStateInfo_Free(tree, typePtr, pInfo);
    pInfo->data = pSave->data;
    pInfo->count = pSave->count;
}

#ifdef ALLOC_HAX

/*
 * The following AllocHax_xxx calls implement a mini memory allocator that
 * allocates blocks of same-sized chunks, and holds on to those chunks when
 * they are freed so they can be reused quickly. If you don't want to use it
 * just comment out #define ALLOC_HAX in tkTreeCtrl.h.
 */

typedef struct AllocElem AllocElem;
typedef struct AllocBlock AllocBlock;
typedef struct AllocList AllocList;
typedef struct AllocData AllocData;

#ifdef TREECTRL_DEBUG
#define ALLOC_STATS
#endif

#ifdef ALLOC_STATS
typedef struct AllocStats AllocStats;
#endif

/*
 * One of the following structures exists for each client piece of memory.
 * These structures are allocated in arrays (blocks).
 */
struct AllocElem
{
    AllocElem *next;
#ifdef TREECTRL_DEBUG
    int free;
    int size;
#endif
    char body[1];	/* First byte of client's space.  Actual
			 * size of this field will be larger than
			 * one. */
};

struct AllocBlock
{
    int count;		/* Size of .elem[] */
    AllocBlock *next;	/* Next block with same-sized elems. */
    AllocElem elem[1];	/* Actual size will be larger than one. */
};

/*
 * One of the following structures maintains an array of blocks of AllocElems
 * of the same size.
 */
struct AllocList
{
    int size;		/* Size of every AllocElem.body[] */
    AllocElem *head;	/* Top of stack of unused pieces of memory. */
    AllocBlock *blocks;	/* Linked list of allocated blocks. The blocks
			 * may contain a different number of elements. */
    int blockSize;	/* The number of AllocElems per block to allocate.
			 * Starts at 16 and gets doubled up to 1024. */
    AllocList *next;	/* Points to an AllocList with a different .size */
};

/*
 * A pointer to one of the following structures is stored in each TreeCtrl.
 */
struct AllocData
{
    AllocList *freeLists;	/* Linked list. */
#ifdef ALLOC_STATS
    AllocStats *stats;		/* For memory-usage reporting. */
#endif
};

#ifdef ALLOC_STATS
struct AllocStats {
    Tk_Uid id;			/* Name for reporting results. */
    unsigned count;		/* Number of allocations. */
    unsigned size;		/* Total allocated bytes. */
    AllocStats *next;		/* Linked list. */
};
#endif

/*
 * The following macro computes the offset of the "body" field within
 * AllocElem.  It is used to get back to the header pointer from the
 * body pointer that's used by clients.
 */

#define BODY_OFFSET \
    ((unsigned long) (&((AllocElem *) 0)->body))

#ifdef ALLOC_STATS

static AllocStats *
AllocStats_Get(
    ClientData _data,
    Tk_Uid id
    )
{
    AllocData *data = (AllocData *) _data;
    AllocStats *stats = data->stats;

    while (stats != NULL) {
	if (stats->id == id)
	    break;
	stats = stats->next;
    }
    if (stats == NULL) {
	stats = (AllocStats *) ckalloc(sizeof(AllocStats));
	stats->id = id;
	stats->count = 0;
	stats->size = 0;
	stats->next = data->stats;
	data->stats = stats;
    }
    return stats;
}

void
AllocHax_Stats(
    Tcl_Interp *interp,
    ClientData _data
    )
{
    AllocData *data = (AllocData *) _data;
    AllocStats *stats = data->stats;
    char buf[128];
    Tcl_DString dString;

    Tcl_DStringInit(&dString);
    while (stats != NULL) {
	sprintf(buf, "%-20s: %8d : %8d B %5d KB\n",
		stats->id, stats->count,
		stats->size, (stats->size + 1023) / 1024);
	Tcl_DStringAppend(&dString, buf, -1);
	stats = stats->next;
    }
    Tcl_DStringResult(interp, &dString);
}

#endif /* ALLOC_STATS */

/*
 *----------------------------------------------------------------------
 *
 * AllocHax_Alloc --
 *
 *	Return storage for a piece of data of the given size.
 *
 * Results:
 *	The return value is a pointer to memory for the caller's
 *	use.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

char *
AllocHax_Alloc(
    ClientData _data,		/* Token returned by AllocHax_Init(). */
    Tk_Uid id,			/* ID for memory-usage reporting. */
    int size			/* Number of bytes needed. */
    )
{
    AllocData *data = (AllocData *) _data;
    AllocList *freeLists = data->freeLists;
    AllocList *freeList = freeLists;
    AllocBlock *block;
    AllocElem *elem, *result;
#ifdef ALLOC_STATS
    AllocStats *stats = AllocStats_Get(_data, id);
#endif
    int i;

#ifdef ALLOC_STATS
    stats->count++;
    stats->size += size;
#endif

    while ((freeList != NULL) && (freeList->size != size))
	freeList = freeList->next;

    if (freeList == NULL) {
	freeList = (AllocList *) ckalloc(sizeof(AllocList));
	freeList->size = size;
	freeList->head = NULL;
	freeList->next = freeLists;
	freeList->blocks = NULL;
	freeList->blockSize = 16;
	freeLists = freeList;
	((AllocData *) data)->freeLists = freeLists;
    }

    if (freeList->head == NULL) {
	unsigned elemSize = TCL_ALIGN(BODY_OFFSET + size);

	block = (AllocBlock *) ckalloc(Tk_Offset(AllocBlock, elem) +
		elemSize * freeList->blockSize);
	block->count = freeList->blockSize;
	block->next = freeList->blocks;

/* dbwin("AllocHax_Alloc alloc %d of size %d\n", freeList->blockSize, size); */
	freeList->blocks = block;
	if (freeList->blockSize < 1024)
	    freeList->blockSize *= 2;

	freeList->head = block->elem;
	elem = freeList->head;
	for (i = 1; i < block->count - 1; i++) {
#ifdef TREECTRL_DEBUG
	    elem->free = 1;
	    elem->size = size;
#endif
	    elem->next = (AllocElem *) (((char *) freeList->head) +
		elemSize * i);
	    elem = elem->next;
	}
	elem->next = NULL;
#ifdef TREECTRL_DEBUG
	elem->free = 1;
	elem->size = size;
#endif
    }

    result = freeList->head;
    freeList->head = result->next;
#ifdef TREECTRL_DEBUG
    if (!result->free)
	panic("AllocHax_Alloc: element not marked free");
    result->free = 0;
#endif
    return result->body;
}

/*
 *----------------------------------------------------------------------
 *
 * AllocHax_Realloc --
 *
 *	Realloc.
 *
 * Results:
 *	The return value is a pointer to memory for the caller's
 *	use.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

char *
AllocHax_Realloc(
    ClientData data,		/* Token returned by AllocHax_Init(). */
    Tk_Uid id,			/* ID for memory-usage reporting. */
    char *ptr,
    int size1,			/* Number of bytes in ptr. */
    int size2			/* Number of bytes needed. */
    )
{
    char *ptr2;

    ptr2 = AllocHax_Alloc(data, id, size2);
    memcpy(ptr2, ptr, MIN(size1, size2));
    AllocHax_Free(data, id, ptr, size1);
    return ptr2;
}

/*
 *----------------------------------------------------------------------
 *
 * AllocHax_Free --
 *
 *	Mark a piece of memory as free for reuse.
 *
 * Results:
 *	The piece of memory is added to a list of free pieces of the
 *	same size.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
AllocHax_Free(
    ClientData _data,		/* Token returned by AllocHax_Init(). */
    Tk_Uid id,			/* ID for memory-usage reporting. */
    char *ptr,			/* Memory to mark for reuse. Must have
				 * been allocated by AllocHax_Alloc(). */
    int size			/* Number of bytes. Must match the size
				 * passed to AllocHax_CAlloc(). */
    )
{
    AllocData *data = (AllocData *) _data;
    AllocList *freeLists = data->freeLists;
    AllocList *freeList = freeLists;
    AllocElem *elem;
#ifdef ALLOC_STATS
    AllocStats *stats = AllocStats_Get(_data, id);
#endif

#ifdef ALLOC_STATS
    stats->count--;
    stats->size -= size;
#endif

    /*
    * See comment from Tcl_DbCkfree before you change the following
    * line.
    */

    elem = (AllocElem *) (((unsigned long) ptr) - BODY_OFFSET);

#ifdef TREECTRL_DEBUG
    if (elem->free)
	panic("AllocHax_Free: element already marked free");
    if (elem->size != size)
	panic("AllocHax_Free: element size %d != size %d", elem->size, size);
#endif
    while (freeList != NULL && freeList->size != size)
	freeList = freeList->next;
    if (freeList == NULL)
	panic("AllocHax_Free: can't find free list for size %d", size);

    WIPE(elem->body, size);
    elem->next = freeList->head;
#ifdef TREECTRL_DEBUG
    elem->free = 1;
#endif
    freeList->head = elem;
}

/*
 *----------------------------------------------------------------------
 *
 * AllocHax_CAlloc --
 *
 *	Return storage for an array of pieces of memory.
 *
 * Results:
 *	Pointer to the available memory.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

char *
AllocHax_CAlloc(
    ClientData data,		/* Token returned by AllocHax_Init(). */
    Tk_Uid id,			/* ID for memory-usage reporting. */
    int size,			/* Number of bytes needed for each piece
				 * of memory. */
    int count,			/* Number of pieces of memory needed. */
    int roundUp			/* Positive number used to reduce the number
				 * of lists of memory pieces of different
				 * size. */
    )
{
    int n = (count / roundUp) * roundUp + ((count % roundUp) ? roundUp : 0);
#ifdef ALLOC_STATS
    AllocStats *stats = AllocStats_Get(data, id);
#endif

#ifdef ALLOC_STATS
    stats->count += count - 1;
#endif
    return AllocHax_Alloc(data, id, size * n);
}

/*
 *----------------------------------------------------------------------
 *
 * AllocHax_CFree --
 *
 *	Mark a piece of memory as free for reuse.
 *
 * Results:
 *	The piece of memory is added to a list of free pieces of the
 *	same size.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
AllocHax_CFree(
    ClientData data,		/* Token returned by AllocHax_Init(). */
    Tk_Uid id,			/* ID for memory-usage reporting. */
    char *ptr,			/* Memory to mark for reuse. Must have
				 * been allocated by AllocHax_CAlloc(). */
    int size,			/* Same arg to AllocHax_CAlloc(). */
    int count,			/* Same arg to AllocHax_CAlloc(). */
    int roundUp			/* Same arg to AllocHax_CAlloc(). */
    )
{
    int n = (count / roundUp) * roundUp + ((count % roundUp) ? roundUp : 0);
#ifdef ALLOC_STATS
    AllocStats *stats = AllocStats_Get(data, id);
#endif

    AllocHax_Free(data, id, ptr, size * n);
#ifdef ALLOC_STATS
    stats->count -= count - 1;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * AllocHax_Init --
 *
 *	Allocate and initialize a new memory-manager record.
 *
 * Results:
 *	Pointer to memory-manager record.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

ClientData
AllocHax_Init(void)
{
    AllocData *data = (AllocData *) ckalloc(sizeof(AllocData));
    data->freeLists = NULL;
#ifdef ALLOC_STATS
    data->stats = NULL;
#endif
    return data;
}

/*
 *----------------------------------------------------------------------
 *
 * AllocHax_Finalize --
 *
 *	Free all the memory associated with a memory-manager record.
 *
 * Results:
 *	Pointer to memory-manager record.
 *
 * Side effects:
 *	Memory is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
AllocHax_Finalize(
    ClientData _data		/* Pointer to AllocData created by
				 * AllocHax_Init(). */
    )
{
    AllocData *data = (AllocData *) _data;
    AllocList *freeList = data->freeLists;
#ifdef ALLOC_STATS
    AllocStats *stats = data->stats;
#endif

    while (freeList != NULL) {
	AllocList *nextList = freeList->next;
	AllocBlock *block = freeList->blocks;
	while (block != NULL) {
	    AllocBlock *nextBlock = block->next;
	    ckfree((char *) block);
	    block = nextBlock;
	}
	ckfree((char *) freeList);
	freeList = nextList;
    }

#ifdef ALLOC_STATS
    while (stats != NULL) {
	AllocStats *next = stats->next;
	ckfree((char *) stats);
	stats = next;
    }
#endif

    ckfree((char *) data);
}

#endif /* ALLOC_HAX */

/*
 *----------------------------------------------------------------------
 *
 * TreePtrList_Init --
 *
 *	Initializes an pointer list, discarding any previous contents
 *	of the pointer list (TreePtrList_Free should have been called already
 *	if the pointer list was previously in use).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The pointer list is initialized to be empty.
 *
 *----------------------------------------------------------------------
 */

void
TreePtrList_Init(
    TreeCtrl *tree,		/* Widget info. */
    TreePtrList *tplPtr,	/* Structure describing pointer list. */
    int count			/* Number of pointers the list should hold.
				 * 0 for default. */
    )
{
    tplPtr->tree = tree;
    tplPtr->pointers = tplPtr->pointerSpace;
    tplPtr->count = 0;
    tplPtr->space = TIL_STATIC_SPACE;

    if (count + 1 > TIL_STATIC_SPACE) {
	tplPtr->space = count + 1;
	tplPtr->pointers = (ClientData *) ckalloc(tplPtr->space * sizeof(ClientData));
    }

    tplPtr->pointers[0] = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TreePtrList_Grow --
 *
 *	Increase the available space in an pointer list.
 *
 * Results:
 *	The pointers[] array is resized if needed.
 *
 * Side effects:
 *	Memory gets reallocated if needed.
 *
 *----------------------------------------------------------------------
 */

void
TreePtrList_Grow(
    TreePtrList *tplPtr,	/* Structure describing pointer list. */
    int count			/* Number of pointers the list should hold. */
    )
{
    if (tplPtr->space >= count + 1)
	return;
    while (tplPtr->space < count + 1)
	tplPtr->space *= 2;
    if (tplPtr->pointers == tplPtr->pointerSpace) {
	ClientData *pointers;
	pointers = (ClientData *) ckalloc(tplPtr->space * sizeof(ClientData));
	memcpy(pointers, tplPtr->pointers, (tplPtr->count + 1) * sizeof(ClientData));
	tplPtr->pointers = pointers;
    } else {
	tplPtr->pointers = (ClientData *) ckrealloc((char *) tplPtr->pointers,
		tplPtr->space * sizeof(ClientData));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TreePtrList_Append --
 *
 *	Append an pointer to an pointer list.
 *
 * Results:
 *	The return value is a pointer to the list of pointers.
 *
 * Side effects:
 *	Memory gets reallocated if needed.
 *
 *----------------------------------------------------------------------
 */

ClientData *
TreePtrList_Append(
    TreePtrList *tplPtr,	/* Structure describing pointer list. */
    ClientData pointer		/* Item to append. */
    )
{
    TreePtrList_Grow(tplPtr, tplPtr->count + 1);
    tplPtr->pointers[tplPtr->count] = pointer;
    tplPtr->count++;
    tplPtr->pointers[tplPtr->count] = NULL;
    return tplPtr->pointers;
}

/*
 *----------------------------------------------------------------------
 *
 * TreePtrList_Concat --
 *
 *	Join two pointer lists.
 *
 * Results:
 *	The return value is a pointer to the list of pointers.
 *
 * Side effects:
 *	Memory gets reallocated if needed.
 *
 *----------------------------------------------------------------------
 */

ClientData *
TreePtrList_Concat(
    TreePtrList *tplPtr,	/* Structure describing pointer list. */
    TreePtrList *tpl2Ptr	/* Item list to append. */
    )
{
    TreePtrList_Grow(tplPtr, tplPtr->count + tpl2Ptr->count);
    memcpy(tplPtr->pointers + tplPtr->count, tpl2Ptr->pointers,
	tpl2Ptr->count * sizeof(ClientData));
    tplPtr->count += tpl2Ptr->count;
    tplPtr->pointers[tplPtr->count] = NULL;
    return tplPtr->pointers;
}

/*
 *----------------------------------------------------------------------
 *
 * TreePtrList_Free --
 *
 *	Frees up any memory allocated for the pointer list and
 *	reinitializes the pointer list to an empty state.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The previous contents of the pointer list are lost.
 *
 *---------------------------------------------------------------------- */

void
TreePtrList_Free(
    TreePtrList *tplPtr		/* Structure describing pointer list. */
    )
{
    if (tplPtr->pointers != tplPtr->pointerSpace) {
	ckfree((char *) tplPtr->pointers);
    }
    tplPtr->pointers = tplPtr->pointerSpace;
    tplPtr->count = 0;
    tplPtr->space = TIL_STATIC_SPACE;
    tplPtr->pointers[0] = NULL;
}

#define TAG_INFO_SIZE(tagSpace) \
    (Tk_Offset(TagInfo, tagPtr) + ((tagSpace) * sizeof(Tk_Uid)))

static CONST char *TagInfoUid = "TagInfo";

/*
 *----------------------------------------------------------------------
 *
 * TagInfo_Add --
 *
 *	Adds tags to a list of tags.
 *
 * Results:
 *	Non-duplicate tags are added.
 *
 * Side effects:
 *	Memory may be (re)allocated.
 *
 *----------------------------------------------------------------------
 */

TagInfo *
TagInfo_Add(
    TreeCtrl *tree,		/* Widget info. */
    TagInfo *tagInfo,		/* Tag list. May be NULL. */
    Tk_Uid tags[],
    int numTags
    )
{
    int i, j;

    if (tagInfo == NULL) {
	if (numTags <= TREE_TAG_SPACE) {
#ifdef ALLOC_HAX
	    tagInfo = (TagInfo *) AllocHax_Alloc(tree->allocData, TagInfoUid,
		    sizeof(TagInfo));
#else
	    tagInfo = (TagInfo *) ckalloc(sizeof(TagInfo));
#endif
	    tagInfo->tagSpace = TREE_TAG_SPACE;
	} else {
	    int tagSpace = (numTags / TREE_TAG_SPACE) * TREE_TAG_SPACE +
		((numTags % TREE_TAG_SPACE) ? TREE_TAG_SPACE : 0);
if (tagSpace % TREE_TAG_SPACE) panic("TagInfo_Add miscalc");
#ifdef ALLOC_HAX
	    tagInfo = (TagInfo *) AllocHax_Alloc(tree->allocData, TagInfoUid,
		TAG_INFO_SIZE(tagSpace));
#else
	    tagInfo = (TagInfo *) ckalloc(sizeof(TagInfo) +
		TAG_INFO_SIZE(tagSpace));
#endif
	    tagInfo->tagSpace = tagSpace;
	}
	tagInfo->numTags = 0;
    }
    for (i = 0; i < numTags; i++) {
	for (j = 0; j < tagInfo->numTags; j++) {
	    if (tagInfo->tagPtr[j] == tags[i])
		break;
	}
	if (j >= tagInfo->numTags) {
	    /* Resize existing storage if needed. */
	    if (tagInfo->tagSpace == tagInfo->numTags) {
		tagInfo->tagSpace += TREE_TAG_SPACE;
#ifdef ALLOC_HAX
		tagInfo = (TagInfo *) AllocHax_Realloc(tree->allocData,
		    TagInfoUid, (char *) tagInfo,
		    TAG_INFO_SIZE(tagInfo->tagSpace - TREE_TAG_SPACE),
		    TAG_INFO_SIZE(tagInfo->tagSpace));
#else
		tagInfo = (TagInfo *) ckrealloc((char *) tagInfo,
		    TAG_INFO_SIZE(tagSpace));
#endif
	    }
	    tagInfo->tagPtr[tagInfo->numTags++] = tags[i];
	}
    }
    return tagInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TagInfo_Remove --
 *
 *	Removes tags from a list of tags.
 *
 * Results:
 *	Existing tags are removed.
 *
 * Side effects:
 *	Memory may be reallocated.
 *
 *----------------------------------------------------------------------
 */

TagInfo *
TagInfo_Remove(
    TreeCtrl *tree,		/* Widget info. */
    TagInfo *tagInfo,		/* Tag list. May be NULL. */
    Tk_Uid tags[],
    int numTags
    )
{
    int i, j;

    if (tagInfo == NULL)
	return tagInfo;
    for (i = 0; i < numTags; i++) {
	for (j = 0; j < tagInfo->numTags; j++) {
	    if (tagInfo->tagPtr[j] == tags[i]) {
		tagInfo->tagPtr[j] =
		    tagInfo->tagPtr[tagInfo->numTags - 1];
		tagInfo->numTags--;
		break;
	    }
	}
    }
    if (tagInfo->numTags == 0) {
	TagInfo_Free(tree, tagInfo);
	tagInfo = NULL;
    }
    return tagInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * TagInfo_Names --
 *
 *	Build a list of unique tag names.
 *
 * Results:
 *	Unique tags are added to a dynamically-allocated list.
 *
 * Side effects:
 *	Memory may be (re)allocated.
 *
 *----------------------------------------------------------------------
 */

Tk_Uid *
TagInfo_Names(
    TreeCtrl *tree,		/* Widget info. */
    TagInfo *tagInfo,		/* Tag list. May be NULL. */
    Tk_Uid *tags,		/* Current list, may be NULL. */
    int *numTagsPtr,		/* Number of tags in tags[]. */
    int *tagSpacePtr		/* Size of tags[]. */
    )
{
    int numTags = *numTagsPtr, tagSpace = *tagSpacePtr;
    int i, j;

    if (tagInfo == NULL)
	return tags;
    for (i = 0; i < tagInfo->numTags; i++) {
	Tk_Uid tag = tagInfo->tagPtr[i];
	for (j = 0; j < numTags; j++) {
	    if (tag == tags[j])
		break;
	}
	if (j < numTags)
	    continue;
	if ((tags == NULL) || (numTags == tagSpace)) {
	    if (tags == NULL) {
		tagSpace = 32;
		tags = (Tk_Uid *) ckalloc(sizeof(Tk_Uid) * tagSpace);
	    }
	    else {
		tagSpace *= 2;
		tags = (Tk_Uid *) ckrealloc((char *) tags,
		    sizeof(Tk_Uid) * tagSpace);
	    }
	}
	tags[numTags++] = tag;
    }
    *numTagsPtr = numTags;
    *tagSpacePtr = tagSpace;
    return tags;
}

/*
 *----------------------------------------------------------------------
 *
 * TagInfo_Copy --
 *
 *	Copy a list of tags.
 *
 * Results:
 *	Allocates a new TagInfo if given one is not NULL.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

TagInfo *
TagInfo_Copy(
    TreeCtrl *tree,		/* Widget info. */
    TagInfo *tagInfo		/* Tag list. May be NULL. */
    )
{
    TagInfo *copy = NULL;

    if (tagInfo != NULL) {
	int tagSpace = tagInfo->tagSpace;
#ifdef ALLOC_HAX
	copy = (TagInfo *) AllocHax_Alloc(tree->allocData, TagInfoUid,
		TAG_INFO_SIZE(tagSpace));
#else
	copy = (TagInfo *) ckalloc(TAG_INFO_SIZE(tagSpace));
#endif
	memcpy(copy->tagPtr, tagInfo->tagPtr, tagInfo->numTags * sizeof(Tk_Uid));
	copy->numTags = tagInfo->numTags;
	copy->tagSpace = tagSpace;
    }
    return copy;
}

/*
 *----------------------------------------------------------------------
 *
 * TagInfo_Free --
 *
 *	Free a list of tags.
 *
 * Results:
 *	TagInfo struct is freed if non-NULL.
 *
 * Side effects:
 *	Memory may be freed.
 *
 *----------------------------------------------------------------------
 */

void
TagInfo_Free(
    TreeCtrl *tree,		/* Widget info. */
    TagInfo *tagInfo		/* Tag list. May be NULL. */
    )
{
    if (tagInfo != NULL)
#ifdef ALLOC_HAX
	AllocHax_Free(tree->allocData, TagInfoUid, (char *) tagInfo,
	    TAG_INFO_SIZE(tagInfo->tagSpace));
#else
	ckfree((char *) tagInfo);
#endif
}

int
TagInfo_FromObj(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *objPtr,
    TagInfo **tagInfoPtr
    )
{
    int i, numTags;
    Tcl_Obj **listObjv;
    TagInfo *tagInfo = NULL;

    if (Tcl_ListObjGetElements(tree->interp, objPtr, &numTags, &listObjv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (numTags == 0) {
	*tagInfoPtr = NULL;
	return TCL_OK;
    }
    for (i = 0; i < numTags; i++) {
	Tk_Uid tag = Tk_GetUid(Tcl_GetString(listObjv[i]));
	tagInfo = TagInfo_Add(tree, tagInfo, &tag, 1);
    }
    *tagInfoPtr = tagInfo;
    return TCL_OK;
}

Tcl_Obj *
TagInfo_ToObj(
    TreeCtrl *tree,		/* Widget info. */
    TagInfo *tagInfo
    )
{
    Tcl_Obj *listObj;
    int i;

    if (tagInfo == NULL)
	return NULL;

    listObj = Tcl_NewListObj(0, NULL);
    for (i = 0; i < tagInfo->numTags; i++) {
	Tcl_ListObjAppendElement(NULL, listObj,
		Tcl_NewStringObj((char *) tagInfo->tagPtr[i], -1));
    }
    return listObj;
}

/*
 *----------------------------------------------------------------------
 *
 * TagInfoCO_Set --
 * TagInfoCO_Get --
 * TagInfoCO_Restore --
 * TagInfoCO_Free --
 *
 *	These procedures implement a TK_OPTION_CUSTOM where the custom
 *	option is a TagInfo record.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TagInfoCO_Set(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags
    )
{
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    int objEmpty;
    TagInfo *new, **internalPtr;

    if (internalOffset >= 0)
	internalPtr = (TagInfo **) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	if (TagInfo_FromObj(tree, (*value), &new) != TCL_OK)
	    return TCL_ERROR;
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL)
	    new = NULL;
	*((TagInfo **) saveInternalPtr) = *internalPtr;
	*internalPtr = new;
    }

    return TCL_OK;
}

static Tcl_Obj *
TagInfoCO_Get(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset
    )
{
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    TagInfo *value = *(TagInfo **) (recordPtr + internalOffset);
    return TagInfo_ToObj(tree, value);
}

static void
TagInfoCO_Restore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr
    )
{
    *(TagInfo **) internalPtr = *(TagInfo **) saveInternalPtr;
}

static void
TagInfoCO_Free(
    ClientData clientData,	/* unused. */
    Tk_Window tkwin,		/* A window; unused */
    char *internalPtr		/* Pointer to the place, where the internal
				 * form resides. */
    )
{
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;

    TagInfo_Free(tree, *(TagInfo **)internalPtr);
}

Tk_ObjCustomOption TagInfoCO =
{
    "tag list",
    TagInfoCO_Set,
    TagInfoCO_Get,
    TagInfoCO_Restore,
    TagInfoCO_Free,
    (ClientData) NULL
};

/*
 *----------------------------------------------------------------------
 *
 * TagExpr_Init --
 *
 *	This procedure initializes a TagExpr struct by parsing a Tcl_Obj
 *	string representation of a tag expression.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

int
TagExpr_Init(
    TreeCtrl *tree,		/* Widget info. */
    Tcl_Obj *exprObj,		/* Tag expression string. */
    TagExpr *expr		/* Struct to initialize. */
    )
{
    int i;
    char *tag;

    expr->tree = tree;
    expr->index = 0;
    expr->length = 0;
    expr->uid = NULL;
    expr->allocated = sizeof(expr->staticUids) / sizeof(Tk_Uid);
    expr->uids = expr->staticUids;
    expr->simple = TRUE;
    expr->rewritebuffer = expr->staticRWB;

    tag = Tcl_GetStringFromObj(exprObj, &expr->stringLength);

    /* short circuit impossible searches for null tags */
    if (expr->stringLength == 0) {
	return TCL_OK;
    }

    /*
     * Pre-scan tag for at least one unquoted "&&" "||" "^" "!"
     *   if not found then use string as simple tag
     */
    for (i = 0; i < expr->stringLength ; i++) {
	if (tag[i] == '"') {
	    i++;
	    for ( ; i < expr->stringLength; i++) {
		if (tag[i] == '\\') {
		    i++;
		    continue;
		}
		if (tag[i] == '"') {
		    break;
		}
	    }
	} else {
	    if ((tag[i] == '&' && tag[i+1] == '&')
	     || (tag[i] == '|' && tag[i+1] == '|')
	     || (tag[i] == '^')
	     || (tag[i] == '!')) {
		expr->simple = FALSE;
		break;
	    }
	}
    }

    if (expr->simple) {
	expr->uid = Tk_GetUid(tag);
	return TCL_OK;
    }

    expr->string = tag;
    expr->stringIndex = 0;

    /* Allocate buffer for rewritten tags (after de-escaping) */
    if (expr->stringLength >= sizeof(expr->staticRWB))
	expr->rewritebuffer = ckalloc(expr->stringLength + 1);

    if (TagExpr_Scan(expr) != TCL_OK) {
	TagExpr_Free(expr);
	return TCL_ERROR;
    }
    expr->length = expr->index;
    return TCL_OK;
}

/*
 * Uids for operands in compiled tag expressions.
 * Initialization is done by GetStaticUids().
 */
typedef struct {
    Tk_Uid andUid;
    Tk_Uid orUid;
    Tk_Uid xorUid;
    Tk_Uid parenUid;
    Tk_Uid negparenUid;
    Tk_Uid endparenUid;
    Tk_Uid tagvalUid;
    Tk_Uid negtagvalUid;
} SearchUids;

static Tcl_ThreadDataKey dataKey;

/*
 *----------------------------------------------------------------------
 *
 * GetStaticUids --
 *
 *	This procedure is invoked to return a structure filled with
 *	the Uids used when doing tag searching. If it was never before
 *	called in the current thread, it initializes the structure for
 *	that thread (uids are only ever local to one thread [Bug
 *	1114977]).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static SearchUids *
GetStaticUids()
{
    SearchUids *searchUids = (SearchUids *)
	    Tcl_GetThreadData(&dataKey, sizeof(SearchUids));

    if (searchUids->andUid == NULL) {
	searchUids->andUid       = Tk_GetUid("&&");
	searchUids->orUid        = Tk_GetUid("||");
	searchUids->xorUid       = Tk_GetUid("^");
	searchUids->parenUid     = Tk_GetUid("(");
	searchUids->endparenUid  = Tk_GetUid(")");
	searchUids->negparenUid  = Tk_GetUid("!(");
	searchUids->tagvalUid    = Tk_GetUid("!!");
	searchUids->negtagvalUid = Tk_GetUid("!");
    }
    return searchUids;
}

/*
 *----------------------------------------------------------------------
 *
 * TagExpr_Scan --
 *
 *	This procedure recursively parses a string representation of a
 *	tag expression into an array of Tk_Uids.
 *
 * Results:
 *	The return value indicates if the tag expression
 *	was successfully scanned (syntax).
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

int
TagExpr_Scan(
    TagExpr *expr		/* Info about a tag expression. */
    )
{
    Tcl_Interp *interp = expr->tree->interp;
    int looking_for_tag;        /* When true, scanner expects
				 * next char(s) to be a tag,
				 * else operand expected */
    int found_tag;              /* One or more tags found */
    int found_endquote;         /* For quoted tag string parsing */
    int negate_result;          /* Pending negation of next tag value */
    char *tag;                  /* tag from tag expression string */
    SearchUids *searchUids;	/* Collection of uids for basic search
				 * expression terms. */
    char c;

    searchUids = GetStaticUids();
    negate_result = 0;
    found_tag = 0;
    looking_for_tag = 1;
    while (expr->stringIndex < expr->stringLength) {
	c = expr->string[expr->stringIndex++];

	if (expr->allocated == expr->index) {
	    expr->allocated += 15;
	    if (expr->uids != expr->staticUids) {
		expr->uids =
		    (Tk_Uid *) ckrealloc((char *)(expr->uids),
		    (expr->allocated)*sizeof(Tk_Uid));
	    } else {
		expr->uids =
		(Tk_Uid *) ckalloc((expr->allocated)*sizeof(Tk_Uid));
		memcpy(expr->uids, expr->staticUids, sizeof(expr->staticUids));
	    }
	}

	if (looking_for_tag) {

	    switch (c) {
		case ' '  :	/* ignore unquoted whitespace */
		case '\t' :
		case '\n' :
		case '\r' :
		    break;

		case '!'  :	/* negate next tag or subexpr */
		    if (looking_for_tag > 1) {
			Tcl_AppendResult(interp,
				"Too many '!' in tag search expression",
				(char *) NULL);
			return TCL_ERROR;
		    }
		    looking_for_tag++;
		    negate_result = 1;
		    break;

		case '('  :	/* scan (negated) subexpr recursively */
		    if (negate_result) {
			expr->uids[expr->index++] = searchUids->negparenUid;
			negate_result = 0;
		    } else {
			expr->uids[expr->index++] = searchUids->parenUid;
		    }
		    if (TagExpr_Scan(expr) != TCL_OK) {
			/* Result string should be already set
			 * by nested call to tag_expr_scan() */
			return TCL_ERROR;
		    }
		    looking_for_tag = 0;
		    found_tag = 1;
		    break;

		case '"'  :	/* quoted tag string */
		    if (negate_result) {
			expr->uids[expr->index++] = searchUids->negtagvalUid;
			negate_result = 0;
		    } else {
			expr->uids[expr->index++] = searchUids->tagvalUid;
		    }
		    tag = expr->rewritebuffer;
		    found_endquote = 0;
		    while (expr->stringIndex < expr->stringLength) {
			c = expr->string[expr->stringIndex++];
			if (c == '\\') {
			    c = expr->string[expr->stringIndex++];
			}
			if (c == '"') {
			    found_endquote = 1;
			    break;
			}
			*tag++ = c;
		    }
		    if (! found_endquote) {
			Tcl_AppendResult(interp,
				"Missing endquote in tag search expression",
				(char *) NULL);
			return TCL_ERROR;
		    }
		    if (! (tag - expr->rewritebuffer)) {
			Tcl_AppendResult(interp,
			    "Null quoted tag string in tag search expression",
			    (char *) NULL);
			return TCL_ERROR;
		    }
		    *tag++ = '\0';
		    expr->uids[expr->index++] =
			    Tk_GetUid(expr->rewritebuffer);
		    looking_for_tag = 0;
		    found_tag = 1;
		    break;

		case '&'  :	/* illegal chars when looking for tag */
		case '|'  :
		case '^'  :
		case ')'  :
		    Tcl_AppendResult(interp,
			    "Unexpected operator in tag search expression",
			    (char *) NULL);
		    return TCL_ERROR;

		default :	/* unquoted tag string */
		    if (negate_result) {
			expr->uids[expr->index++] = searchUids->negtagvalUid;
			negate_result = 0;
		    } else {
			expr->uids[expr->index++] = searchUids->tagvalUid;
		    }
		    tag = expr->rewritebuffer;
		    *tag++ = c;
		    /* copy rest of tag, including any embedded whitespace */
		    while (expr->stringIndex < expr->stringLength) {
			c = expr->string[expr->stringIndex];
			if (c == '!' || c == '&' || c == '|' || c == '^'
				|| c == '(' || c == ')' || c == '"') {
			    break;
			}
			*tag++ = c;
			expr->stringIndex++;
		    }
		    /* remove trailing whitespace */
		    while (1) {
			c = *--tag;
			/* there must have been one non-whitespace char,
			 *  so this will terminate */
			if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
			    break;
			}
		    }
		    *++tag = '\0';
		    expr->uids[expr->index++] =
			    Tk_GetUid(expr->rewritebuffer);
		    looking_for_tag = 0;
		    found_tag = 1;
	    }

	} else {    /* ! looking_for_tag */

	    switch (c) {
		case ' '  :	/* ignore whitespace */
		case '\t' :
		case '\n' :
		case '\r' :
		    break;

		case '&'  :	/* AND operator */
		    c = expr->string[expr->stringIndex++];
		    if (c != '&') {
			Tcl_AppendResult(interp,
				"Singleton '&' in tag search expression",
				(char *) NULL);
			return TCL_ERROR;
		    }
		    expr->uids[expr->index++] = searchUids->andUid;
		    looking_for_tag = 1;
		    break;

		case '|'  :	/* OR operator */
		    c = expr->string[expr->stringIndex++];
		    if (c != '|') {
			Tcl_AppendResult(interp,
				"Singleton '|' in tag search expression",
				(char *) NULL);
			return TCL_ERROR;
		    }
		    expr->uids[expr->index++] = searchUids->orUid;
		    looking_for_tag = 1;
		    break;

		case '^'  :	/* XOR operator */
		    expr->uids[expr->index++] = searchUids->xorUid;
		    looking_for_tag = 1;
		    break;

		case ')'  :	/* end subexpression */
		    expr->uids[expr->index++] = searchUids->endparenUid;
		    goto breakwhile;

		default   :	/* syntax error */
		    Tcl_AppendResult(interp,
			    "Invalid boolean operator in tag search expression",
			    (char *) NULL);
		    return TCL_ERROR;
	    }
	}
    }
breakwhile:
    if (found_tag && ! looking_for_tag) {
	return TCL_OK;
    }
    Tcl_AppendResult(interp, "Missing tag in tag search expression",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TagExpr_Eval --
 *
 *	This procedure recursively evaluates a compiled tag expression.
 *
 * Results:
 *	The return value indicates if the tag expression
 *	successfully matched the tags of the given item.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
_TagExpr_Eval(
    TagExpr *expr,		/* Info about a tag expression. */
    TagInfo *tagInfo		/* Tags to test. */
    )
{
    int looking_for_tag;        /* When true, scanner expects
				 * next char(s) to be a tag,
				 * else operand expected */
    int negate_result;          /* Pending negation of next tag value */
    Tk_Uid uid;
    Tk_Uid *tagPtr;
    int count;
    int result;                 /* Value of expr so far */
    int parendepth;
    SearchUids *searchUids;	/* Collection of uids for basic search
				 * expression terms. */
    TagInfo dummy;

    if (expr->stringLength == 0) /* empty expression (an error?) */
	return 0;

    /* No tags given. */
    if (tagInfo == NULL) {
	dummy.numTags = 0;
	tagInfo = &dummy;
    }

    /* A single tag. */
    if (expr->simple) {
	for (tagPtr = tagInfo->tagPtr, count = tagInfo->numTags;
	    count > 0; tagPtr++, count--) {
	    if (*tagPtr == expr->uid) {
		return 1;
	    }
	}
	return 0;
    }

    searchUids = GetStaticUids();
    result = 0;  /* just to keep the compiler quiet */

    negate_result = 0;
    looking_for_tag = 1;
    while (expr->index < expr->length) {
	uid = expr->uids[expr->index++];
	if (looking_for_tag) {
	    if (uid == searchUids->tagvalUid) {
/*
 *              assert(expr->index < expr->length);
 */
		uid = expr->uids[expr->index++];
		result = 0;
		/*
		 * set result 1 if tag is found in item's tags
		 */
		for (tagPtr = tagInfo->tagPtr, count = tagInfo->numTags;
		    count > 0; tagPtr++, count--) {
		    if (*tagPtr == uid) {
			result = 1;
			break;
		    }
		}

	    } else if (uid == searchUids->negtagvalUid) {
		negate_result = ! negate_result;
/*
 *              assert(expr->index < expr->length);
 */
		uid = expr->uids[expr->index++];
		result = 0;
		/*
		 * set result 1 if tag is found in item's tags
		 */
		for (tagPtr = tagInfo->tagPtr, count = tagInfo->numTags;
		    count > 0; tagPtr++, count--) {
		    if (*tagPtr == uid) {
			result = 1;
			break;
		    }
		}

	    } else if (uid == searchUids->parenUid) {
		/*
		 * evaluate subexpressions with recursion
		 */
		result = _TagExpr_Eval(expr, tagInfo);

	    } else if (uid == searchUids->negparenUid) {
		negate_result = ! negate_result;
		/*
		 * evaluate subexpressions with recursion
		 */
		result = _TagExpr_Eval(expr, tagInfo);
/*
 *          } else {
 *              assert(0);
 */
	    }
	    if (negate_result) {
		result = ! result;
		negate_result = 0;
	    }
	    looking_for_tag = 0;
	} else {    /* ! looking_for_tag */
	    if (((uid == searchUids->andUid) && (!result)) ||
		    ((uid == searchUids->orUid) && result)) {
		/*
		 * short circuit expression evaluation
		 *
		 * if result before && is 0, or result before || is 1,
		 *   then the expression is decided and no further
		 *   evaluation is needed.
		 */

		    parendepth = 0;
		while (expr->index < expr->length) {
		    uid = expr->uids[expr->index++];
		    if (uid == searchUids->tagvalUid ||
			    uid == searchUids->negtagvalUid) {
			expr->index++;
			continue;
		    }
		    if (uid == searchUids->parenUid ||
			    uid == searchUids->negparenUid) {
			parendepth++;
			continue;
		    } 
		    if (uid == searchUids->endparenUid) {
			parendepth--;
			if (parendepth < 0) {
			    break;
			}
		    }
		}
		return result;

	    } else if (uid == searchUids->xorUid) {
		/*
		 * if the previous result was 1
		 *   then negate the next result
		 */
		negate_result = result;

	    } else if (uid == searchUids->endparenUid) {
		return result;
/*
 *          } else {
 *               assert(0);
 */
	    }
	    looking_for_tag = 1;
	}
    }
/*
 *  assert(! looking_for_tag);
 */
    return result;
}

int
TagExpr_Eval(
    TagExpr *expr,		/* Info about a tag expression. */
    TagInfo *tagInfo		/* Tags to test. */
    )
{
    expr->index = 0;
    return _TagExpr_Eval(expr, tagInfo);
}

/*
 *----------------------------------------------------------------------
 *
 * TagExpr_Free --
 *
 *	This procedure frees the given struct.
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
TagExpr_Free(
    TagExpr *expr
    )
{
    if (expr->rewritebuffer != expr->staticRWB)
	ckfree(expr->rewritebuffer);
    if (expr->uids != expr->staticUids)
	ckfree((char *) expr->uids);
}

/*
 *----------------------------------------------------------------------
 *
 * OptionHax_Remember --
 * OptionHax_Forget --
 *
 *	These procedures are used to work around a limitation in
 *	the Tk_SavedOption structure: the internal form of a configuration
 *	option cannot be larger than a double.
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
OptionHax_Remember(
    TreeCtrl *tree,
    char *ptr
    )
{
#ifdef TREECTRL_DEBUG
    int i;
    for (i = 0; i < tree->optionHaxCnt; i++) {
	if (ptr == tree->optionHax[i]) {
	    panic("OptionHax_Remember: ptr is not new");
	}
    }
#endif
    tree->optionHax[tree->optionHaxCnt++] = ptr;
/*dbwin("OptionHax_Remember %p\n", ptr);*/
}

int
OptionHax_Forget(
    TreeCtrl *tree,
    char *ptr
    )
{
    int i;

    for (i = 0; i < tree->optionHaxCnt; i++) {
	if (ptr == tree->optionHax[i]) {
	    tree->optionHax[i] = tree->optionHax[--tree->optionHaxCnt];
/*dbwin("OptionHax_Forget %p\n", ptr);*/
	    return 1;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * PerStateCO_Set --
 * PerStateCO_Get --
 * PerStateCO_Restore --
 * PerStateCO_Free --
 *
 *	These procedures implement a TK_OPTION_CUSTOM where the custom
 *	option is a PerStateInfo record.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

typedef struct PerStateCOClientData
{
    PerStateType *typePtr;
    StateFromObjProc proc;
} PerStateCOClientData;

static int
PerStateCO_Set(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags
    )
{
    PerStateCOClientData *cd = (PerStateCOClientData *) clientData;
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    int objEmpty;
    PerStateInfo new, *internalPtr, *hax;

    if (internalOffset >= 0)
	internalPtr = (PerStateInfo *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*value));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*value) = NULL;
    else {
	new.obj = (*value);
	new.data = NULL;
	new.count = 0;
	Tcl_IncrRefCount((*value));
	if (PerStateInfo_FromObj(tree, cd->proc, cd->typePtr, &new) != TCL_OK) {
	    Tcl_DecrRefCount((*value));
	    return TCL_ERROR;
	}
    }
    if (internalPtr != NULL) {
	if ((*value) == NULL) {
	    new.obj = NULL;
	    new.data = NULL;
	    new.count = 0;
	}
	OptionHax_Remember(tree, saveInternalPtr);
	if (internalPtr->obj != NULL) {
	    hax = (PerStateInfo *) ckalloc(sizeof(PerStateInfo));
	    *hax = *internalPtr;
	    *((PerStateInfo **) saveInternalPtr) = hax;
	} else {
	    *((PerStateInfo **) saveInternalPtr) = NULL;
	}
	*internalPtr = new;
/*dbwin("PerStateCO_Set %p %s\n", internalPtr, cd->typePtr->name);*/
    }

    return TCL_OK;
}

static Tcl_Obj *
PerStateCO_Get(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset
    )
{
    PerStateInfo *value = (PerStateInfo *) (recordPtr + internalOffset);
    return value->obj; /* May be NULL. */
}

static void
PerStateCO_Restore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr
    )
{
    PerStateInfo *psi, *hax = *(PerStateInfo **) saveInternalPtr;
/*dbwin("PerStateCO_Restore\n");*/
    if (hax != NULL) {
	*(PerStateInfo *) internalPtr = *hax;
    } else {
	psi = (PerStateInfo *) internalPtr;
	psi->obj = NULL;
	psi->data = NULL;
	psi->count = 0;
    }
}

static void
PerStateCO_Free(
    ClientData clientData,	/* unused. */
    Tk_Window tkwin,		/* A window; unused */
    char *internalPtr		/* Pointer to the place, where the internal
				 * form resides. */
    )
{
    PerStateCOClientData *cd = (PerStateCOClientData *) clientData;
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    PerStateInfo *hax;
    Tcl_Obj *objPtr = NULL;

    if (OptionHax_Forget(tree, internalPtr)) {
	hax = *(PerStateInfo **) internalPtr;
	if (hax != NULL) {
	    objPtr = hax->obj;
	    PerStateInfo_Free(tree, cd->typePtr, hax);
	    ckfree((char *) hax);
	}
    } else {
/*dbwin("PerStateCO_Free %p %s\n", internalPtr, cd->typePtr->name);*/
	objPtr = ((PerStateInfo *) internalPtr)->obj;
	PerStateInfo_Free(tree, cd->typePtr, (PerStateInfo *) internalPtr);
    }
    if (objPtr != NULL)
	Tcl_DecrRefCount(objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PerStateCO_Alloc --
 *
 *	Allocates a Tk_ObjCustomOption record and clientData.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_ObjCustomOption *
PerStateCO_Alloc(
    CONST char *optionName,
    PerStateType *typePtr,
    StateFromObjProc proc
    )
{
    PerStateCOClientData *cd;
    Tk_ObjCustomOption *co;

    /* ClientData for the Tk custom option record */
    cd = (PerStateCOClientData *) ckalloc(sizeof(PerStateCOClientData));
    cd->typePtr = typePtr;
    cd->proc = proc;

    /* The Tk custom option record */
    co = (Tk_ObjCustomOption *) ckalloc(sizeof(Tk_ObjCustomOption));
    co->name = (char *) optionName + 1;
    co->setProc = PerStateCO_Set;
    co->getProc = PerStateCO_Get;
    co->restoreProc = PerStateCO_Restore;
    co->freeProc = PerStateCO_Free;
    co->clientData = (ClientData) cd;

    return co;
}

/*
 *----------------------------------------------------------------------
 *
 * PerStateCO_Init --
 *
 *	Initializes a Tk_OptionSpec.clientData for a custom option.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
PerStateCO_Init(
    Tk_OptionSpec *optionTable,
    CONST char *optionName,
    PerStateType *typePtr,
    StateFromObjProc proc
    )
{
    Tk_ObjCustomOption *co;
    int i;

    for (i = 0; optionTable[i].type != TK_OPTION_END; i++) {
	if (strcmp(optionTable[i].optionName, optionName))
	    continue;

	if (optionTable[i].clientData != NULL)
	    return TCL_OK;

	/* The Tk custom option record */
	co = PerStateCO_Alloc(optionName, typePtr, proc);
    
	/* Update the option table */
	optionTable[i].clientData = (ClientData) co;
	return TCL_OK;
    }
    return TCL_ERROR;
}

#define DEBUG_DYNAMICxxx

static CONST char *DynamicOptionUid = "DynamicOption";

/*
 *----------------------------------------------------------------------
 *
 * DynamicOption_Find --
 *
 *	Returns a pointer to a dynamic-option record or NULL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

DynamicOption *
DynamicOption_Find(
    DynamicOption *first,	/* Head of linked list. */
    int id			/* Unique id. */
    )
{
    DynamicOption *opt = first;

    while (opt != NULL) {
	if (opt->id == id)
	    return opt;
	opt = opt->next;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DynamicOption_FindData --
 *
 *	Returns a pointer to the option-specific data for a
 *	dynamic-option record or NULL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
DynamicOption_FindData(
    DynamicOption *first,	/* Head of linked list. */
    int id			/* Unique id. */
    )
{
    DynamicOption *opt = DynamicOption_Find(first, id);
    if (opt != NULL)
	return opt->data;
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DynamicOption_AllocIfNeeded --
 *
 *	Returns a pointer to a dynamic-option record.
 *
 * Results:
 *	If the dynamic-option record exists, it is returned. Otherwise
 *	a new one is allocated and initialized.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

DynamicOption *
DynamicOption_AllocIfNeeded(
    TreeCtrl *tree,
    DynamicOption **firstPtr,	/* Pointer to the head of linked list.
				 * Will be updated if a new record is
				 * created. */
    int id,			/* Unique id. */
    int size,			/* Size of option-specific data. */
    DynamicOptionInitProc *init	/* Proc to intialized the option-specific
				 * data. May be NULL. */
    )
{
    DynamicOption *opt = *firstPtr;

    while (opt != NULL) {
	if (opt->id == id)
	    return opt;
	opt = opt->next;
    }
#ifdef DEBUG_DYNAMIC
dbwin("DynamicOption_AllocIfNeeded allocated id=%d\n", id);
#endif
#ifdef ALLOC_HAX
    opt = (DynamicOption *) AllocHax_Alloc(tree->allocData, DynamicOptionUid,
	    Tk_Offset(DynamicOption, data) + size);
#else
    opt = (DynamicOption *) ckalloc(Tk_Offset(DynamicOption, data) + size);
#endif
    opt->id = id;
    memset(opt->data, '\0', size);
    if (init != NULL)
	(*init)(opt->data);
    opt->next = *firstPtr;
    *firstPtr = opt;
    return opt;
}

/*
 *----------------------------------------------------------------------
 *
 * DynamicCO_Set --
 * DynamicCO_Get --
 * DynamicCO_Restore --
 * DynamicCO_Free --
 *
 *	These procedures implement a TK_OPTION_CUSTOM where the custom
 *	option is a DynamicOption record.
 *
 *	A dynamic option is one for which storage is not allocated until
 *	the option is configured for the first time. Dynamic options are
 *	saved in a linked list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

/* This is the Tk_OptionSpec.clientData field for a dynamic option. */
typedef struct DynamicCOClientData
{
    int id;			/* Unique id. */
    int size;			/* Size of client data. */
    int objOffset;		/* Offset in the client data to store the
				 * object representation of the option.
				 * May be < 0. */
    int internalOffset;		/* Offset in the client data to store the
				 * internal representation of the option.
				 * May be < 0. */
    Tk_ObjCustomOption *custom;	/* Table of procedures and clientData for
				 * the actual option. */
    DynamicOptionInitProc *init;/* This gets called to initialize the client
				 * data when it is first allocated. May be
				 * NULL. */
} DynamicCOClientData;

/* This is used to save the current value of an option when a call to
 * Tk_SetOptions is in progress. */
typedef struct DynamicCOSave
{
    Tcl_Obj *objPtr;		/* The object representation of the option. */
    double internalForm;	/* The internal form of the option. */
} DynamicCOSave;

static int
DynamicCO_Set(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags
    )
{
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    DynamicCOClientData *cd = (DynamicCOClientData *) clientData;
    DynamicOption **firstPtr, *opt;
    DynamicCOSave *save;
    Tcl_Obj **objPtrPtr;

    /* Get pointer to the head of the list of dynamic options. */
    firstPtr = (DynamicOption **) (recordPtr + internalOffset);

    /* Get the dynamic option record. Create it if needed, and update the
     * linked list of dynamic options. */
    opt = DynamicOption_AllocIfNeeded(tree, firstPtr, cd->id, cd->size, cd->init);

    save = (DynamicCOSave *) ckalloc(sizeof(DynamicCOSave));
#ifdef DEBUG_DYNAMIC
dbwin("DynamicCO_Set id=%d saveInternalPtr=%p save=%p\n", cd->id, saveInternalPtr, save);
#endif
    if (cd->custom->setProc(cd->custom->clientData, interp, tkwin, value,
	    opt->data, cd->internalOffset, (char *) &save->internalForm,
	    flags) != TCL_OK) {
	ckfree((char *) save);
	return TCL_ERROR;
    }

    if (cd->objOffset >= 0) {
	objPtrPtr = (Tcl_Obj **) (opt->data + cd->objOffset);
	save->objPtr = *objPtrPtr;

#ifdef DEBUG_DYNAMIC
dbwin("saving object '%s'\n", *value ? Tcl_GetString(*value) : "NULL");
#endif
	*objPtrPtr = *value;
	if (*value != NULL)
	    Tcl_IncrRefCount(*value);
    }

    *(DynamicCOSave **) saveInternalPtr = save;
    OptionHax_Remember(tree, saveInternalPtr);

    return TCL_OK;
}

static Tcl_Obj *
DynamicCO_Get(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset
    )
{
    DynamicCOClientData *cd = (DynamicCOClientData *) clientData;
    DynamicOption *first = *(DynamicOption **) (recordPtr + internalOffset);
    DynamicOption *opt = DynamicOption_Find(first, cd->id);

#ifdef DEBUG_DYNAMIC
dbwin("DynamicCO_Get id=%d opt=%p objOffset=%d\n", cd->id, opt, cd->objOffset);
#endif
    if (opt == NULL)
	return NULL;

    if (cd->objOffset >= 0) {
	return *(Tcl_Obj **) (opt->data + cd->objOffset);
    }

    if (cd->custom->getProc != NULL)
	return cd->custom->getProc(cd->custom->clientData, tkwin, opt->data, cd->internalOffset);
    return NULL;
}

static void
DynamicCO_Restore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr
    )
{
    DynamicCOClientData *cd = (DynamicCOClientData *) clientData;
    DynamicOption *first = *(DynamicOption **) internalPtr;
    DynamicOption *opt = DynamicOption_Find(first, cd->id);
    DynamicCOSave *save;
    Tcl_Obj **objPtrPtr;

    if (opt == NULL)
	panic("DynamicCO_Restore: opt=NULL");
#ifdef DEBUG_DYNAMIC
dbwin("DynamicCO_Restore id=%d internalOffset=%d\n", cd->id, cd->internalOffset);
#endif
    if (cd->internalOffset >= 0 && cd->custom->restoreProc != NULL)
	cd->custom->restoreProc(cd->custom->clientData, tkwin,
	    opt->data + cd->internalOffset, saveInternalPtr);

    if (cd->objOffset >= 0) {
	objPtrPtr = (Tcl_Obj **) (opt->data + cd->objOffset);
	if (*objPtrPtr != NULL)
	    Tcl_DecrRefCount(*objPtrPtr);
	save = (DynamicCOSave *) saveInternalPtr;
	*objPtrPtr = save->objPtr;
    }
}

static void
DynamicCO_Free(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr
    )
{
    TreeCtrl *tree = (TreeCtrl *) ((TkWindow *) tkwin)->instanceData;
    DynamicCOClientData *cd = (DynamicCOClientData *) clientData;

#ifdef DEBUG_DYNAMIC
dbwin("DynamicCO_Free id=%d internalPtr=%p...\n", cd->id, internalPtr);
#endif
    if (OptionHax_Forget(tree, internalPtr)) {
	DynamicCOSave *save = *(DynamicCOSave **) internalPtr;
#ifdef DEBUG_DYNAMIC
dbwin("DynamicCO_Free id=%d internalPtr=%p save=%p\n", cd->id, internalPtr, save);
#endif
	if (cd->internalOffset >= 0 && cd->custom->freeProc != NULL)
	    cd->custom->freeProc(cd->custom->clientData, tkwin,
		    (char *) &save->internalForm);
	ckfree((char *) save);
    } else {
	DynamicOption *first = *(DynamicOption **) internalPtr;
	DynamicOption *opt = DynamicOption_Find(first, cd->id);
#ifdef DEBUG_DYNAMIC
dbwin("DynamicCO_Free id=%d internalPtr=%p save=NULL\n", cd->id, internalPtr);
#endif
	if (opt != NULL && cd->internalOffset >= 0 && cd->custom->freeProc != NULL)
	    cd->custom->freeProc(cd->custom->clientData, tkwin,
		opt->data + cd->internalOffset);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DynamicOption_Init --
 *
 *	Initialize a Tk_OptionSpec.clientData field before calling
 *	Tk_CreateOptionTable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *----------------------------------------------------------------------
 */

int
DynamicCO_Init(
    Tk_OptionSpec *optionTable,	/* Table to search. */
    CONST char *optionName,	/* Name of the option. */
    int id,			/* Unique id. */
    int size,			/* Size of client data. */
    int objOffset,		/* Offset in the client data to store the
				 * object representation of the option.
				 * May be < 0. */
    int internalOffset,		/* Offset in the client data to store the
				 * internal representation of the option.
				 * May be < 0. */
    Tk_ObjCustomOption *custom,	/* Table of procedures and clientData for
				 * the actual option. */
    DynamicOptionInitProc *init	/* This gets called to initialize the client
				 * data when it is first allocated. May be
				 * NULL. */
    )
{
    DynamicCOClientData *cd;
    Tk_ObjCustomOption *co;
    int i;

    if (size <= 0)
	panic("DynamicCO_Init: option %s size=%d", optionName, size);

    for (i = 0; optionTable[i].type != TK_OPTION_END; i++) {
	if (strcmp(optionTable[i].optionName, optionName))
	    continue;

	if (optionTable[i].clientData != NULL)
	    return TCL_OK;

	if (optionTable[i].type != TK_OPTION_CUSTOM)
	    panic("DynamicCO_Init: %s is not TK_OPTION_CUSTOM", optionName);

	/* ClientData for the Tk custom option record */
	cd = (DynamicCOClientData *) ckalloc(sizeof(DynamicCOClientData));
	cd->id = id;
	cd->size = size;
	cd->objOffset = objOffset;
	cd->internalOffset = internalOffset;
	cd->custom = custom;
	cd->init = init;

	/* The Tk custom option record */
	co = (Tk_ObjCustomOption *) ckalloc(sizeof(Tk_ObjCustomOption));
	co->name = (char *) optionName + 1;
	co->setProc = DynamicCO_Set;
	co->getProc = DynamicCO_Get;
	co->restoreProc = DynamicCO_Restore;
	co->freeProc = DynamicCO_Free;
	co->clientData = (ClientData) cd;

	/* Update the option table */
	optionTable[i].clientData = (ClientData) co;
#ifdef DEBUG_DYNAMIC
dbwin("DynamicCO_Init id=%d size=%d objOffset=%d internalOffset=%d custom->name=%s",
    id, size, objOffset, internalOffset, custom->name);
#endif
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DynamicOption_Free --
 *
 *	Free a linked list of dynamic-option records. This gets called
 *	after Tk_FreeConfigOptions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be freed.
 *
 *----------------------------------------------------------------------
 */

void
DynamicOption_Free(
    TreeCtrl *tree,
    DynamicOption *first,
    Tk_OptionSpec *optionTable
    )
{
    DynamicOption *opt = first;
    DynamicCOClientData *cd;
    Tk_ObjCustomOption *co;
    int i;

    while (opt != NULL) {
	DynamicOption *next = opt->next;

	for (i = 0; optionTable[i].type != TK_OPTION_END; i++) {

	    if (optionTable[i].type != TK_OPTION_CUSTOM)
		continue;

	    co = (Tk_ObjCustomOption *) optionTable[i].clientData;
	    if (co->setProc != DynamicCO_Set)
		continue;

	    cd = (DynamicCOClientData *) co->clientData;
	    if (cd->id != opt->id)
		continue;

#ifdef ALLOC_HAX
	    AllocHax_Free(tree->allocData, DynamicOptionUid, (char *) opt,
		    Tk_Offset(DynamicOption, data) + cd->size);
#else
	    ckfree((char *) opt);
#endif
	}

	opt = next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DynamicOption_Free1 --
 *
 *	Free a single dynamic-option record. This is a big hack so that
 *	dynamic-option records that aren't associated with a Tk_OptionSpec
 *	array can be used.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be freed.
 *
 *----------------------------------------------------------------------
 */

void
DynamicOption_Free1(
    TreeCtrl *tree,
    DynamicOption **firstPtr,
    int id,
    int size
    )
{
    DynamicOption *opt = *firstPtr, *prev = NULL;

    while (opt != NULL) {
	if (opt->id == id) {
	    if (prev == NULL)
		*firstPtr = opt->next;
	    else
		prev->next = opt->next;
#ifdef ALLOC_HAX
	    AllocHax_Free(tree->allocData, DynamicOptionUid, (char *) opt,
		    Tk_Offset(DynamicOption, data) + size);
#else
	    ckfree((char *) opt);
#endif
	    return;
	}
	prev = opt;
	opt = opt->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StringCO_Set --
 * StringCO_Get --
 * StringCO_Restore --
 * StringCO_Free --
 *
 *	These procedures implement a TK_OPTION_CUSTOM where the custom
 *	option is exactly the same as a TK_OPTION_STRING. This is used
 *	when storage for the option is dynamically allocated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
StringCO_Set(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **valuePtr,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags
    )
{
    int objEmpty;
    char *internalPtr, *new, *value;
    int length;

    if (internalOffset >= 0)
	internalPtr = (char *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*valuePtr));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty)
	(*valuePtr) = NULL;

    if (internalPtr != NULL) {
	if (*valuePtr != NULL) {
	    value = Tcl_GetStringFromObj(*valuePtr, &length);
	    new = ckalloc((unsigned) (length + 1));
	    strcpy(new, value);
	} else {
	    new = NULL;
	}
	*((char **) saveInternalPtr) = *((char **) internalPtr);
	*((char **) internalPtr) = new;
    }

    return TCL_OK;
}

static Tcl_Obj *
StringCO_Get(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset
    )
{
    char **internalPtr = (char **) (recordPtr + internalOffset);

    return Tcl_NewStringObj(*internalPtr, -1);
}

static void
StringCO_Restore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr
    )
{
    *(char **) internalPtr = *(char **) saveInternalPtr;
}

static void
StringCO_Free(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr
    )
{
    if (*((char **) internalPtr) != NULL) {
	ckfree(*((char **) internalPtr));
	*((char **) internalPtr) = NULL;
    }
}

Tk_ObjCustomOption stringCO =
{
    "string",
    StringCO_Set,
    StringCO_Get,
    StringCO_Restore,
    StringCO_Free,
    (ClientData) NULL
};

/*
 *----------------------------------------------------------------------
 *
 * PixelsCO_Set --
 * PixelsCO_Get --
 * PixelsCO_Restore --
 *
 *	These procedures implement a TK_OPTION_CUSTOM where the custom
 *	option is exactly the same as a TK_OPTION_PIXELS. This is used
 *	when storage for the option is dynamically allocated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PixelsCO_Set(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **valuePtr,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags
    )
{
    int objEmpty;
    int *internalPtr, new;

    if (internalOffset >= 0)
	internalPtr = (int *) (recordPtr + internalOffset);
    else
	internalPtr = NULL;

    objEmpty = ObjectIsEmpty((*valuePtr));

    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
	(*valuePtr) = NULL;
	new = 0;
    } else {
	if (Tk_GetPixelsFromObj(interp, tkwin, *valuePtr, &new) != TCL_OK)
	    return TCL_ERROR;
    }

    if (internalPtr != NULL) {
	*((int *) saveInternalPtr) = *((int *) internalPtr);
	*((int *) internalPtr) = new;
    }

    return TCL_OK;
}

static Tcl_Obj *
PixelsCO_Get(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset
    )
{
    int *internalPtr = (int *) (recordPtr + internalOffset);

    return Tcl_NewIntObj(*internalPtr);
}

static void
PixelsCO_Restore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr
    )
{
    *(int *) internalPtr = *(int *) saveInternalPtr;
}

Tk_ObjCustomOption pixelsCO =
{
    "string",
    PixelsCO_Set,
    PixelsCO_Get,
    PixelsCO_Restore,
    NULL,
    (ClientData) NULL
};

