/* 
 * tkTreeUtils.c --
 *
 *	This module implements misc routines for treectrl widgets.
 *
 * Copyright (c) 2002-2005 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeUtils.c,v 1.16 2005/05/19 20:28:13 treectrl Exp $
 */

#include "tkTreeCtrl.h"
#ifdef WIN32
#include "tkWinInt.h"
#endif

/* OffsetRgn() on Mac */
#ifdef MAC_OSX_TK
#include <Carbon/Carbon.h>
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

void wipefree(char *memPtr, int size)
{
	memset(memPtr, 0xAA, size);
	ckfree(memPtr);
}

void FormatResult(Tcl_Interp *interp, char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
}

int Ellipsis(Tk_Font tkfont, char *string, int numBytes, int *maxPixels, char *ellipsis, int force)
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

/* Draws a horizontal 1-pixel tall dotted line */
void HDotLine(TreeCtrl *tree, Drawable drawable, GC gc, int x1, int y1, int x2)
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

/* Draws a vertical 1-pixel wide dotted line */
void VDotLine(TreeCtrl *tree, Drawable drawable, GC gc, int x1, int y1, int y2)
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

/* Draws 0 or more sides of a rectangle, dot-on dot-off, XOR style */
void DrawActiveOutline(TreeCtrl *tree, Drawable drawable, int x, int y, int width, int height, int open)
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
#else
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

void DotRect(TreeCtrl *tree, Drawable drawable, int x, int y, int width, int height)
{
	DrawActiveOutline(tree, drawable, x, y, width, height, 0);
}

struct DotStatePriv
{
	TreeCtrl *tree;
	Drawable drawable;
#ifdef WIN32
	HDC dc;
	TkWinDCState dcState;
	HRGN rgn;
#else
	GC gc;
	TkRegion rgn;
#endif
};

void DotRect_Setup(TreeCtrl *tree, Drawable drawable, DotState *p)
{
	struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#ifdef WIN32
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
		tree->inset,
		tree->inset + Tree_HeaderHeight(tree),
		Tk_Width(tree->tkwin) - tree->inset,
		Tk_Height(tree->tkwin) - tree->inset);
	SelectClipRgn(dotState->dc, dotState->rgn);
#else
	gcValues.line_style = LineOnOffDash;
	gcValues.line_width = 1;
	gcValues.dash_offset = 0;
	gcValues.dashes = 1;
#if defined(MAC_TCL) || defined(MAC_OSX_TK)
	gcValues.function = GXxor;
#else
	gcValues.function = GXinvert;
#endif
	mask = GCLineWidth | GCLineStyle | GCDashList | GCDashOffset | GCFunction;
	dotState->gc = Tk_GetGC(tree->tkwin, mask, &gcValues);

	/* Keep drawing inside the contentbox */
	dotState->rgn = TkCreateRegion();
	xrect.x = tree->inset;
	xrect.y = tree->inset + Tree_HeaderHeight(tree);
	xrect.width = Tk_Width(tree->tkwin) - tree->inset - xrect.x;
	xrect.height = Tk_Height(tree->tkwin) - tree->inset - xrect.y;
	TkUnionRectWithRegion(&xrect, dotState->rgn, dotState->rgn);
	TkSetRegion(tree->display, dotState->gc, dotState->rgn);
#endif
}

void DotRect_Draw(DotState *p, int x, int y, int width, int height)
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
#else
	XDrawRectangle(dotState->tree->display, dotState->drawable, dotState->gc,
		x, y, width - 1, height - 1);
#endif
}

void DotRect_Restore(DotState *p)
{
	struct DotStatePriv *dotState = (struct DotStatePriv *) p;
#ifdef WIN32
	SelectClipRgn(dotState->dc, NULL);
	DeleteObject(dotState->rgn);
	TkWinReleaseDrawableDC(dotState->drawable, dotState->dc, &dotState->dcState);
#else
	XSetClipMask(dotState->tree->display, dotState->gc, None);
	Tk_FreeGC(dotState->tree->display, dotState->gc);
#endif
}

void Tk_FillRegion(Display *display, Drawable drawable, GC gc, TkRegion rgn)
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
#else
	XRectangle box;

	TkClipBox(rgn, &box);
	TkSetRegion(display, gc, rgn);
	XFillRectangle(display, drawable, gc, box.x, box.y, box.width, box.height);
	XSetClipMask(display, gc, None);
#endif
}

void Tk_OffsetRegion(TkRegion region, int xOffset, int yOffset)
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

#if defined(WIN32) || defined(MAC_TCL) || defined(MAC_OSX_TK)
void XImage2Photo(Tcl_Interp *interp, Tk_PhotoHandle photoH, XImage *ximage, int alpha)
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

    TK_PHOTOPUTBLOCK(tree->interp, photoH, &photoBlock, 0, 0, w, h,
	    TK_PHOTO_COMPOSITE_SET);

    Tcl_Free((char *) pixelPtr);
}
#else /* X11 */
void XImage2Photo(Tcl_Interp *interp, Tk_PhotoHandle photoH, XImage *ximage, int alpha)
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
	    pixelPtr[y * photoBlock.pitch + x * 4 + 3] = alpha;
	}
    }

    TK_PHOTOPUTBLOCK(interp, photoH, &photoBlock, 0, 0, w, h,
	    TK_PHOTO_COMPOSITE_SET);

    Tcl_Free((char *) pixelPtr);
    ckfree((char *) xcolors);
}
#endif

/*
 * Replacement for Tk_TextLayout stuff. Allows the caller to break lines
 * on character boundaries (as well as word boundaries). Allows the caller
 * to specify the maximum number of lines to display. Will add ellipsis "..."
 * to the end of text that is too long to fit (when max lines specified).
 */

typedef struct LayoutChunk
{
	CONST char *start;	/* Pointer to simple string to be displayed.
						 * * This is a pointer into the TkTextLayout's
						 * * string. */
	int numBytes;	/* The number of bytes in this chunk. */
	int numChars;	/* The number of characters in this chunk. */
	int numDisplayChars;	/* The number of characters to display when
							 * * this chunk is displayed.  Can be less than
							 * * numChars if extra space characters were
							 * * absorbed by the end of the chunk.  This
							 * * will be < 0 if this is a chunk that is
							 * * holding a tab or newline. */
	int x, y;	/* The origin of the first character in this
				 * * chunk with respect to the upper-left hand
				 * * corner of the TextLayout. */
	int totalWidth;	/* Width in pixels of this chunk.  Used
					 * * when hit testing the invisible spaces at
					 * * the end of a chunk. */
	int displayWidth;	/* Width in pixels of the displayable
						 * * characters in this chunk.  Can be less than
						 * * width if extra space characters were
						 * * absorbed by the end of the chunk. */
	int ellipsis; /* TRUE if adding "..." */
} LayoutChunk;

typedef struct LayoutInfo
{
	Tk_Font tkfont;	/* The font used when laying out the text. */
	CONST char *string;	/* The string that was layed out. */
	int numLines;	/* Number of lines */
	int width;	/* The maximum width of all lines in the
				 * * text layout. */
	int height;
	int numChunks;	/* Number of chunks actually used in
					 * * following array. */
	LayoutChunk chunks[1];	/* Array of chunks.  The actual size will
							 * * be maxChunks.  THIS FIELD MUST BE THE LAST
							 * * IN THE STRUCTURE. */
} LayoutInfo;

static LayoutChunk *NewChunk(LayoutInfo **layoutPtrPtr, int *maxPtr,
	CONST char *start, int numBytes, int curX, int newX, int y)
{
	LayoutInfo *layoutPtr;
	LayoutChunk *chunkPtr;
	int maxChunks, numChars;
	size_t s;

	layoutPtr = *layoutPtrPtr;
	maxChunks = *maxPtr;
	if (layoutPtr->numChunks == maxChunks)
	{
		maxChunks *= 2;
		s = sizeof(LayoutInfo) + ((maxChunks - 1) * sizeof(LayoutChunk));
		layoutPtr = (LayoutInfo *) ckrealloc((char *) layoutPtr, s);

		*layoutPtrPtr = layoutPtr;
		*maxPtr = maxChunks;
	}
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
	Tk_Font tkfont,	/* Font that will be used to display text. */
	CONST char *string,	/* String whose dimensions are to be
						 ** computed. */
	int numChars,	/* Number of characters to consider from
					 ** string, or < 0 for strlen(). */
	int wrapLength,	/* Longest permissible line length, in
					 ** pixels.  <= 0 means no automatic wrapping:
					 ** just let lines get as long as needed. */
	Tk_Justify justify,	/* How to justify lines. */
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

	maxChunks = 1;

	layoutPtr = (LayoutInfo *) ckalloc(sizeof(LayoutInfo) + (maxChunks -
			1) * sizeof(LayoutChunk));
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
				chunkPtr = NewChunk(&layoutPtr, &maxChunks, start,
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
				NewChunk(&layoutPtr, &maxChunks, start, 1, curX, newX,
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
				NewChunk(&layoutPtr, &maxChunks, start, 1, curX, curX,
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
				NewChunk(&layoutPtr, &maxChunks, start, 0, curX, curX,
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
			chunkPtr++;
		}
	}

	Tcl_DStringFree(&lineBuffer);

	/* We don't want single-line text layouts for text elements, but it happens for column titles */
/*	if (layoutPtr->numLines == 1)
		dbwin("WARNING: single-line TextLayout created\n"); */

	return (TextLayout) layoutPtr;
}

void TextLayout_Free(TextLayout textLayout)
{
	LayoutInfo *layoutPtr = (LayoutInfo *) textLayout;

	ckfree((char *) layoutPtr);
}

void TextLayout_Size(TextLayout textLayout, int *widthPtr, int *heightPtr)
{
	LayoutInfo *layoutPtr = (LayoutInfo *) textLayout;

	if (widthPtr != NULL)
		(*widthPtr) = layoutPtr->width;
	if (heightPtr != NULL)
		(*heightPtr) = layoutPtr->height;
}

void TextLayout_Draw(
	Display *display,	/* Display on which to draw. */
	Drawable drawable,	/* Window or pixmap in which to draw. */
	GC gc,	/* Graphics context to use for drawing text. */
	TextLayout layout,	/* Layout information, from a previous call
							 * * to Tk_ComputeTextLayout(). */
	int x, int y,	/* Upper-left hand corner of rectangle in
				 * * which to draw (pixels). */
	int firstChar,	/* The index of the first character to draw
					 * * from the given text item.  0 specfies the
					 * * beginning. */
	int lastChar	/* The index just after the last character
					 * * to draw from the given text item.  A number
					 * * < 0 means to draw all characters. */
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
		}
		firstChar -= chunkPtr->numChars;
		lastChar -= chunkPtr->numChars;
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
 *	Parse a pad amount configuration options.
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

/*****/

int ObjectIsEmpty(Tcl_Obj *obj)
{
    int length;

    if (obj == NULL)
	return 1;
    if (obj->bytes != NULL)
	return (obj->length == 0);
    Tcl_GetStringFromObj(obj, &length);
    return (length == 0);
}

void PerStateInfo_Free(
    TreeCtrl *tree,
    PerStateType *typePtr,
    PerStateInfo *pInfo)
{
    PerStateData *pData = pInfo->data;
    int i;

    if (pInfo->data == NULL)
	return;
#ifdef DEBUG_PSI
    if (pInfo->type != typePtr)
	panic("PerStateInfo_Free type mismatch: got %s expected %s",
		pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
#endif
    for (i = 0; i < pInfo->count; i++) {
	(*typePtr->freeProc)(tree, pData);
	pData = (PerStateData *) (((char *) pData) + typePtr->size);
    }
    wipefree((char *) pInfo->data, typePtr->size * pInfo->count);
    pInfo->data = NULL;
    pInfo->count = 0;
}

int PerStateInfo_FromObj(
    TreeCtrl *tree,
    StateFromObjProc proc,
    PerStateType *typePtr,
    PerStateInfo *pInfo)
{
    int i, j;
    int objc, objc2;
    Tcl_Obj **objv, **objv2;
    PerStateData *pData;

#ifdef DEBUG_PSI
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
	pData = (PerStateData *) ckalloc(typePtr->size);
	pData->stateOff = pData->stateOn = 0; /* all states */
	if ((*typePtr->fromObjProc)(tree, objv[0], pData) != TCL_OK) {
	    wipefree((char *) pData, typePtr->size);
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

    pData = (PerStateData *) ckalloc(typePtr->size * (objc / 2));
    pInfo->data = pData;
    for (i = 0; i < objc; i += 2) {
	if ((*typePtr->fromObjProc)(tree, objv[i], pData) != TCL_OK) {
	    PerStateInfo_Free(tree, typePtr, pInfo);
	    return TCL_ERROR;
	}
	pInfo->count++;
	if (Tcl_ListObjGetElements(tree->interp, objv[i + 1], &objc2, &objv2) != TCL_OK) {
	    PerStateInfo_Free(tree, typePtr, pInfo);
	    return TCL_ERROR;
	}
	pData->stateOff = pData->stateOn = 0; /* all states */
	for (j = 0; j < objc2; j++) {
	    if (proc(tree, objv2[j], &pData->stateOff, &pData->stateOn) != TCL_OK) {
		PerStateInfo_Free(tree, typePtr, pInfo);
		return TCL_ERROR;
	    }
	}
	pData = (PerStateData *) (((char *) pData) + typePtr->size);
    }
    return TCL_OK;
}

PerStateData *PerStateInfo_ForState(
    TreeCtrl *tree,
    PerStateType *typePtr,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateData *pData = pInfo->data;
    int stateOff = ~state, stateOn = state;
    int i;

#ifdef DEBUG_PSI
    if ((pInfo->data != NULL) && (pInfo->type != typePtr))
	panic("PerStateInfo_ForState type mismatch: got %s expected %s",
		pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
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

Tcl_Obj *PerStateInfo_ObjForState(
    TreeCtrl *tree,
    PerStateType *typePtr,
    PerStateInfo *pInfo,
    int state,
    int *match)
{
    PerStateData *pData;
    Tcl_Obj *obj;
    int i;

#ifdef DEBUG_PSI
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

void PerStateInfo_Undefine(
    TreeCtrl *tree,
    PerStateType *typePtr,
    PerStateInfo *pInfo,
    int state)
{
    PerStateData *pData = pInfo->data;
    int i, j, numStates, stateOff, stateOn;
    Tcl_Obj *configObj = pInfo->obj, *listObj, *stateObj;

#ifdef DEBUG_PSI
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
	     * the string rep of the sublist {state1 ...}, but not
	     * the parent list */
	    Tcl_InvalidateStringRep(configObj);
	}
	pData = (PerStateData *) (((char *) pData) + typePtr->size);
    }
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
#ifdef DEBUG_PSI
    "Bitmap",
#endif
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
#ifdef DEBUG_PSI
    "Border",
#endif
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
#ifdef DEBUG_PSI
    "Color",
#endif
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
#ifdef DEBUG_PSI
    "Font",
#endif
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
    /* don't free image */
}

PerStateType pstImage =
{
#ifdef DEBUG_PSI
    "Image",
#endif
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
#ifdef DEBUG_PSI
    "Relief",
#endif
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
#ifdef DEBUG_PSI
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
#define ALLOC_BLOCK_SIZE 128

typedef struct AllocElem AllocElem;
typedef struct AllocList AllocList;
typedef struct AllocData AllocData;

struct AllocElem
{
	AllocElem *next;
	int free;
	char d[1];
};

struct AllocList
{
	int size;
	AllocElem *head;
	AllocList *next;
	AllocElem **blocks;
	int blockCount;
};

struct AllocData
{
	AllocList *freeLists;
};

char *AllocHax_Alloc(ClientData data, int size)
{
	AllocList *freeLists = ((AllocData *) data)->freeLists;
	AllocList *freeList = freeLists;
	AllocElem *elem, *result;
	int i;

	while ((freeList != NULL) && (freeList->size != size))
		freeList = freeList->next;

	if (freeList == NULL) {
		freeList = (AllocList *) ckalloc(sizeof(AllocList));
		freeList->size = size;
		freeList->head = NULL;
		freeList->next = freeLists;
		freeList->blocks = NULL;
		freeList->blockCount = 0;
		freeLists = freeList;
		((AllocData *) data)->freeLists = freeLists;
	}

	if (freeList->head != NULL) {
		elem = freeList->head;
		freeList->head = elem->next;
		result = elem;
	} else {
		AllocElem *block;
		freeList->blockCount += 1;
		freeList->blocks = (AllocElem **) ckrealloc((char *) freeList->blocks, sizeof(AllocElem *) * freeList->blockCount);
		block = (AllocElem *) ckalloc((sizeof(AllocElem) - 1 + size) * ALLOC_BLOCK_SIZE);
		freeList->blocks[freeList->blockCount - 1] = block;
/* dbwin("AllocHax_Alloc alloc %d of size %d\n", ALLOC_BLOCK_SIZE, size); */
		freeList->head = block;
		elem = freeList->head;
		for (i = 1; i < ALLOC_BLOCK_SIZE - 1; i++) {
			elem->free = 1;
			elem->next = (AllocElem *) (((char *) freeList->head) + (sizeof(AllocElem) - 1 + size) * i);
			elem = elem->next;
		}
		elem->next = NULL;
		elem->free = 1;
		result = freeList->head;
		freeList->head = result->next;
	}

	if (!result->free)
		panic("AllocHax_Alloc: element not marked free");

	result->free = 0;
	return result->d;
}

void AllocHax_Free(ClientData data, char *ptr, int size)
{
	AllocList *freeLists = ((AllocData *) data)->freeLists;
	AllocList *freeList = freeLists;
	AllocElem *elem = (AllocElem *) (ptr - sizeof(AllocElem) + sizeof(int));

	if (elem->free)
		panic("AllocHax_Free: element already marked free");

	while (freeList != NULL && freeList->size != size)
		freeList = freeList->next;
memset(ptr, 0xAA, size);
	elem->next = freeList->head;
	elem->free = 1;
	freeList->head = elem;
}

char *AllocHax_CAlloc(ClientData data, int size, int count, int roundUp)
{
	int n = (count / roundUp) * roundUp + ((count % roundUp) ? roundUp : 0);
	return AllocHax_Alloc(data, size * n);
}

void AllocHax_CFree(ClientData data, char *ptr, int size, int count, int roundUp)
{
	int n = (count / roundUp) * roundUp + ((count % roundUp) ? roundUp : 0);
	AllocHax_Free(data, ptr, size * n);
}

ClientData AllocHax_Init(void)
{
	AllocData *data = (AllocData *) ckalloc(sizeof(AllocData));
	data->freeLists = NULL;
	return data;
}

void AllocHax_Finalize(ClientData data)
{
	AllocList *freeList = ((AllocData *) data)->freeLists;
	int i;

	while (freeList != NULL) {
		AllocList *nextList = freeList->next;
		for (i = 0; i < freeList->blockCount; i++) {
			AllocElem *block = freeList->blocks[i];
			ckfree((char *) block);
		}
		ckfree((char *) freeList->blocks);
		ckfree((char *) freeList);
		freeList = nextList;
	}

	ckfree((char *) data);
}

#endif /* ALLOC_HAX */
