/* 
 * tkTreeStyle.c --
 *
 *	This module implements styles for treectrl widgets.
 *
 * Copyright (c) 2002-2005 Tim Baker
 *
 * RCS: @(#) $Id: tkTreeStyle.c,v 1.37 2005/06/20 23:31:07 treectrl Exp $
 */

#include "tkTreeCtrl.h"
#include "tkTreeElem.h"

typedef struct Style Style;
typedef struct ElementLink ElementLink;

#define NEEDEDHAX

struct Style
{
	Tk_OptionTable optionTable;
	Tk_Uid name;
	int numElements;
	ElementLink *elements;
	int neededWidth;
	int neededHeight;
#ifdef NEEDEDHAX /* needed for debugging only */
	int neededState;
#endif
	int minWidth;
	int minHeight;
	int layoutWidth;
	int layoutHeight;
	Style *master;
	int vertical;
};

#define ELF_eEXPAND_W 0x0001 /* expand Layout.ePadX[0] */
#define ELF_eEXPAND_N 0x0002
#define ELF_eEXPAND_E 0x0004
#define ELF_eEXPAND_S 0x0008
#define ELF_iEXPAND_W 0x0010 /* expand Layout.iPadX[0] */
#define ELF_iEXPAND_N 0x0020
#define ELF_iEXPAND_E 0x0040
#define ELF_iEXPAND_S 0x0080
#define ELF_SQUEEZE_X 0x0100 /* shrink Layout.useWidth if needed */
#define ELF_SQUEEZE_Y 0x0200
#define ELF_DETACH 0x0400
#define ELF_INDENT 0x0800 /* don't layout under button&line area */
#define ELF_STICKY_W 0x1000
#define ELF_STICKY_N 0x2000
#define ELF_STICKY_E 0x4000
#define ELF_STICKY_S 0x8000
#define ELF_iEXPAND_X 0x00010000 /* expand Layout.useWidth */
#define ELF_iEXPAND_Y 0x00020000

#define ELF_eEXPAND_WE (ELF_eEXPAND_W | ELF_eEXPAND_E)
#define ELF_eEXPAND_NS (ELF_eEXPAND_N | ELF_eEXPAND_S)
#define ELF_eEXPAND (ELF_eEXPAND_WE | ELF_eEXPAND_NS)
#define ELF_iEXPAND_WE (ELF_iEXPAND_W | ELF_iEXPAND_E)
#define ELF_iEXPAND_NS (ELF_iEXPAND_N | ELF_iEXPAND_S)
#define ELF_iEXPAND (ELF_iEXPAND_WE | ELF_iEXPAND_NS)
#define ELF_EXPAND_WE (ELF_eEXPAND_WE | ELF_iEXPAND_WE)
#define ELF_EXPAND_NS (ELF_eEXPAND_NS | ELF_iEXPAND_NS)
#define ELF_EXPAND_W (ELF_eEXPAND_W | ELF_iEXPAND_W)
#define ELF_EXPAND_N (ELF_eEXPAND_N | ELF_iEXPAND_N)
#define ELF_EXPAND_E (ELF_eEXPAND_E | ELF_iEXPAND_E)
#define ELF_EXPAND_S (ELF_eEXPAND_S | ELF_iEXPAND_S)
#define ELF_STICKY (ELF_STICKY_W | ELF_STICKY_N | ELF_STICKY_E | ELF_STICKY_S)

struct ElementLink
{
	Element *elem;
	int neededWidth;
	int neededHeight;
	int layoutWidth;
	int layoutHeight;
	int ePadX[2]; /* external horizontal padding */
	int ePadY[2]; /* external vertical padding */
	int iPadX[2]; /* internal horizontal padding */
	int iPadY[2]; /* internal vertical padding */
	int flags; /* ELF_xxx */
	int *onion, onionCount; /* -union option info */
	int minWidth, fixedWidth, maxWidth;
	int minHeight, fixedHeight, maxHeight;
};

static char *orientStringTable[] = { "horizontal", "vertical", (char *) NULL };

static Tk_OptionSpec styleOptionSpecs[] = {
	{TK_OPTION_STRING_TABLE, "-orient", (char *) NULL, (char *) NULL,
		"horizontal", -1, Tk_Offset(Style, vertical),
		0, (ClientData) orientStringTable, 0},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

struct Layout
{
	ElementLink *eLink;
	ElementLink *master;
	int useWidth;
	int useHeight;
	int x; /* left of ePad */
	int y; /* above ePad */
	int eWidth; /* ePad + iPad + useWidth + iPad + ePad */
	int eHeight; /* ePad + iPad + useHeight + iPad + ePad */
	int iWidth; /* iPad + useWidth + iPad */
	int iHeight; /* iPad + useHeight + iPad */
	int ePadX[2]; /* external horizontal padding */
	int ePadY[2]; /* external vertical padding */
	int iPadX[2]; /* internal horizontal padding */
	int iPadY[2]; /* internal vertical padding */
	int uPadX[2]; /* padding due to -union */
	int uPadY[2]; /* padding due to -union */
	int temp;
};

static int Style_DoExpandH(struct Layout *layout, int right)
{
	ElementLink *eLink1 = layout->master;
	int flags = eLink1->flags;
	int numExpand = 0, spaceRemaining, spaceUsed = 0;
	int *ePadX, *iPadX, *uPadX;

	if (!(flags & (ELF_EXPAND_WE | ELF_iEXPAND_X)))
		return 0;

	ePadX = layout->ePadX;
	iPadX = layout->iPadX;
	uPadX = layout->uPadX;

	spaceRemaining = right - (layout->x + ePadX[PAD_TOP_LEFT] +
		layout->iWidth + MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]));
	if (spaceRemaining <= 0)
		return 0;

	if (layout->temp)
		numExpand = layout->temp;
	/* For -detach or vertical layout, just set layout->temp to zero */
	else
	{
		if (flags & ELF_eEXPAND_W) numExpand++;
		if (flags & ELF_iEXPAND_W) numExpand++;
		if (flags & ELF_iEXPAND_X)
		{
			if ((eLink1->maxWidth < 0) ||
				(eLink1->maxWidth > layout->useWidth))
				numExpand++;
		}
		if (flags & ELF_iEXPAND_E) numExpand++;
		if (flags & ELF_eEXPAND_E) numExpand++;
	}

	while ((spaceRemaining > 0) && (numExpand > 0))
	{
		int each = (spaceRemaining >= numExpand) ? (spaceRemaining / numExpand) : 1;

		numExpand = 0;

		/* Allocate extra space to the *right* padding first so that any
		 * extra single pixel is given to the right. */

		if (flags & ELF_eEXPAND_E)
		{
			int add = each;
			ePadX[PAD_BOTTOM_RIGHT] += add;
			layout->eWidth += add;
			spaceRemaining -= add;
			spaceUsed += add;
			if (!spaceRemaining)
				break;
			numExpand++;
		}

		if (flags & ELF_iEXPAND_E)
		{
			int add = each;
			iPadX[PAD_BOTTOM_RIGHT] += add;
			layout->iWidth += add;
			layout->eWidth += add;
			spaceRemaining -= add;
			spaceUsed += add;
			if (!spaceRemaining)
				break;
			numExpand++;
		}

		if (flags & ELF_iEXPAND_X)
		{
			int max = eLink1->maxWidth;
			if ((max < 0) || (layout->useWidth < max))
			{
				int add = (max < 0) ? each : MIN(each, max - layout->useWidth);
				layout->useWidth += add;
				layout->iWidth += add;
				layout->eWidth += add;
				spaceRemaining -= add;
				spaceUsed += add;
				if ((max >= 0) && (max == layout->useWidth))
					layout->temp--;
				if (!spaceRemaining)
					break;
				if ((max < 0) || (max > layout->useWidth))
					numExpand++;
			}
		}

		if (flags & ELF_iEXPAND_W)
		{
			int add = each;
			iPadX[PAD_TOP_LEFT] += add;
			layout->iWidth += add;
			layout->eWidth += add;
			spaceRemaining -= add;
			spaceUsed += add;
			if (!spaceRemaining)
				break;
			numExpand++;
		}

		if (flags & ELF_eEXPAND_W)
		{
			int add = each;
			ePadX[PAD_TOP_LEFT] += add;
			layout->eWidth += add;
			spaceRemaining -= add;
			spaceUsed += add;
			if (!spaceRemaining)
				break;
			numExpand++;
		}
	}

	return spaceUsed;
}

static int Style_DoExpandV(struct Layout *layout, int bottom)
{
	ElementLink *eLink1 = layout->master;
	int flags = eLink1->flags;
	int numExpand = 0, spaceRemaining, spaceUsed = 0;
	int *ePadY, *iPadY, *uPadY;

	if (!(flags & (ELF_EXPAND_NS | ELF_iEXPAND_Y)))
		return 0;

	ePadY = layout->ePadY;
	iPadY = layout->iPadY;
	uPadY = layout->uPadY;

	spaceRemaining = bottom - (layout->y + ePadY[PAD_TOP_LEFT] +
		layout->iHeight + MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]));
	if (spaceRemaining <= 0)
		return 0;

	if (layout->temp)
		numExpand = layout->temp;
	/* For -detach or vertical layout, just set layout->temp to zero */
	else
	{
		if (flags & ELF_eEXPAND_N) numExpand++;
		if (flags & ELF_iEXPAND_N) numExpand++;
		if (flags & ELF_iEXPAND_Y)
		{
			if ((eLink1->maxHeight < 0) ||
				(eLink1->maxHeight > layout->useHeight))
				numExpand++;
		}
		if (flags & ELF_iEXPAND_S) numExpand++;
		if (flags & ELF_eEXPAND_S) numExpand++;
	}

	while ((spaceRemaining > 0) && (numExpand > 0))
	{
		int each = (spaceRemaining >= numExpand) ? (spaceRemaining / numExpand) : 1;

		numExpand = 0;

		/* Allocate extra space to the *bottom* padding first so that any
		 * extra single pixel is given to the bottom. */

		if (flags & ELF_eEXPAND_S)
		{
			int add = each;
			ePadY[PAD_BOTTOM_RIGHT] += add;
			layout->eHeight += add;
			spaceRemaining -= add;
			spaceUsed += add;
			if (!spaceRemaining)
				break;
			numExpand++;
		}

		if (flags & ELF_iEXPAND_S)
		{
			int add = each;
			iPadY[PAD_BOTTOM_RIGHT] += add;
			layout->iHeight += add;
			layout->eHeight += add;
			spaceRemaining -= add;
			spaceUsed += add;
			if (!spaceRemaining)
				break;
			numExpand++;
		}

		if (flags & ELF_iEXPAND_Y)
		{
			int max = eLink1->maxHeight;
			if ((max < 0) || (layout->useHeight < max))
			{
				int add = (max < 0) ? each : MIN(each, max - layout->useHeight);
				layout->useHeight += add;
				layout->iHeight += add;
				layout->eHeight += add;
				spaceRemaining -= add;
				spaceUsed += add;
				if ((max >= 0) && (max == layout->useHeight))
					layout->temp--;
				if (!spaceRemaining)
					break;
				if ((max < 0) || (max > layout->useHeight))
					numExpand++;
			}
		}

		if (flags & ELF_iEXPAND_N)
		{
			int add = each;
			iPadY[PAD_TOP_LEFT] += add;
			layout->iHeight += add;
			layout->eHeight += add;
			spaceRemaining -= add;
			spaceUsed += add;
			if (!spaceRemaining)
				break;
			numExpand++;
		}

		if (flags & ELF_eEXPAND_N)
		{
			int add = each;
			ePadY[PAD_TOP_LEFT] += add;
			layout->eHeight += add;
			spaceRemaining -= add;
			spaceUsed += add;
			if (!spaceRemaining)
				break;
			numExpand++;
		}
	}

	return spaceUsed;
}

static int Style_DoLayoutH(StyleDrawArgs *drawArgs, struct Layout layouts[])
{
	Style *style = (Style *) drawArgs->style;
	Style *masterStyle = style->master;
	ElementLink *eLinks1, *eLinks2, *eLink1, *eLink2;
	int x = drawArgs->indent;
	int w, e;
	int *ePadX, *iPadX, *uPadX, *ePadY, *iPadY, *uPadY;
	int numExpandWE = 0;
	int numSqueezeX = 0;
	int i, j, eLinkCount = 0;
	int rightEdge = 0;

	eLinks1 = masterStyle->elements;
	eLinks2 = style->elements;
	eLinkCount = style->numElements;

	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		layout->eLink = eLink2;
		layout->master = eLink1;

		/* Width before squeezing/expanding */
		if (eLink1->onion != NULL)
			layout->useWidth = 0;
		else
			layout->useWidth = eLink2->neededWidth;

		for (j = 0; j < 2; j++)
		{
			/* Pad values before expansion */
			layout->ePadX[j] = eLink1->ePadX[j];
			layout->ePadY[j] = eLink1->ePadY[j];
			layout->iPadX[j] = eLink1->iPadX[j];
			layout->iPadY[j] = eLink1->iPadY[j];

			/* No -union padding yet */
			layout->uPadX[j] = 0;
			layout->uPadY[j] = 0;
		}

		/* Count all non-union, non-detach squeezeable elements */
		if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;
		if (eLink1->flags & ELF_SQUEEZE_X)
			numSqueezeX++;
	}

	/* Calculate the padding around elements surrounded by -union elements */
	for (i = 0; i < eLinkCount; i++)
	{
		eLink1 = &eLinks1[i];

		if (eLink1->onion == NULL)
			continue;

		ePadX = eLink1->ePadX;
		ePadY = eLink1->ePadY;
		iPadX = eLink1->iPadX;
		iPadY = eLink1->iPadY;

		for (j = 0; j < eLink1->onionCount; j++)
		{
			struct Layout *layout = &layouts[eLink1->onion[j]];

			uPadX = layout->uPadX;
			uPadY = layout->uPadY;

			if (masterStyle->vertical)
			{
				uPadX[PAD_TOP_LEFT] = MAX(uPadX[PAD_TOP_LEFT], iPadX[PAD_TOP_LEFT] + ePadX[PAD_TOP_LEFT]);
				uPadX[PAD_BOTTOM_RIGHT] = MAX(uPadX[PAD_BOTTOM_RIGHT], iPadX[PAD_BOTTOM_RIGHT] + ePadX[PAD_BOTTOM_RIGHT]);
				if (j == 0) /* topmost */
					uPadY[PAD_TOP_LEFT] = MAX(uPadY[PAD_TOP_LEFT], iPadY[PAD_TOP_LEFT] + ePadY[PAD_TOP_LEFT]);
				if (j == eLink1->onionCount - 1) /* bottommost */
					uPadY[PAD_BOTTOM_RIGHT] = MAX(uPadY[PAD_BOTTOM_RIGHT], iPadY[PAD_BOTTOM_RIGHT] + ePadY[PAD_BOTTOM_RIGHT]);
			}
			else
			{
				if (j == 0) /* leftmost */
					uPadX[PAD_TOP_LEFT] = MAX(uPadX[PAD_TOP_LEFT], iPadX[PAD_TOP_LEFT] + ePadX[PAD_TOP_LEFT]);
				if (j == eLink1->onionCount - 1) /* rightmost */
					uPadX[PAD_BOTTOM_RIGHT] = MAX(uPadX[PAD_BOTTOM_RIGHT], iPadX[PAD_BOTTOM_RIGHT] + ePadX[PAD_BOTTOM_RIGHT]);
				uPadY[PAD_TOP_LEFT] = MAX(uPadY[PAD_TOP_LEFT], iPadY[PAD_TOP_LEFT] + ePadY[PAD_TOP_LEFT]);
				uPadY[PAD_BOTTOM_RIGHT] = MAX(uPadY[PAD_BOTTOM_RIGHT], iPadY[PAD_BOTTOM_RIGHT] + ePadY[PAD_BOTTOM_RIGHT]);
			}
		}
	}

	/* Left-to-right layout. Make the width of some elements less than they
	 * need */
	if (!masterStyle->vertical &&
		(drawArgs->width < style->neededWidth + drawArgs->indent) &&
		(numSqueezeX > 0))
	{
		int numSqueeze = numSqueezeX;
		int spaceRemaining  = (style->neededWidth + drawArgs->indent) - drawArgs->width;

		while ((spaceRemaining > 0) && (numSqueeze > 0))
		{
			int each = (spaceRemaining >= numSqueeze) ? (spaceRemaining / numSqueeze) : 1;

			numSqueeze = 0;
			for (i = 0; i < eLinkCount; i++)
			{
				struct Layout *layout = &layouts[i];
				int min = 0;

				eLink1 = &eLinks1[i];

				if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
					continue;

				if (!(eLink1->flags & ELF_SQUEEZE_X))
					continue;

				if (eLink1->minWidth >= 0)
					min = eLink1->minWidth;
				if (layout->useWidth > min)
				{
					int sub = MIN(each, layout->useWidth - min);
					layout->useWidth -= sub;
					spaceRemaining -= sub;
					if (!spaceRemaining) break;
					if (layout->useWidth > min)
						numSqueeze++;
				}
			}
		}
	}

	/* Reduce the width of all non-union elements, except for the
	 * cases handled above. */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];
		int width, subtract;

		eLink1 = &eLinks1[i];

		if (eLink1->onion != NULL)
			continue;

		if (!(eLink1->flags & ELF_SQUEEZE_X))
			continue;

		if (!(eLink1->flags & ELF_DETACH) && !masterStyle->vertical)
			continue;

		ePadX = eLink1->ePadX;
		iPadX = eLink1->iPadX;
		uPadX = layout->uPadX;

		width =
			MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]) +
			iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT] +
			MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]);
		subtract = width - drawArgs->width;

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->flags & ELF_INDENT))
			subtract += drawArgs->indent;

		if (subtract > 0)
		{
			if ((eLink1->minWidth >= 0) &&
				(eLink1->minWidth <= layout->useWidth) &&
				(layout->useWidth - subtract < eLink1->minWidth))
				layout->useWidth = eLink1->minWidth;
			else
				layout->useWidth -= subtract;
		}
	}

	/* Layout elements left-to-right */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];
		int right;

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		ePadX = eLink1->ePadX;
		iPadX = eLink1->iPadX;
		uPadX = layout->uPadX;

		if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		layout->x = x + abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];

		right = layout->x + layout->ePadX[PAD_TOP_LEFT] +
			layout->iWidth +
			MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]);

		if (masterStyle->vertical)
			rightEdge = MAX(rightEdge, right);
		else
		{
			rightEdge = right;
			x = layout->x + layout->eWidth;
		}

		/* Count number that want to expand */
		if (eLink1->flags & (ELF_EXPAND_WE | ELF_iEXPAND_X))
			numExpandWE++;
	}

	/* Left-to-right layout. Expand some elements horizontally if we have
	 * more space available horizontally than is needed by the Style. */
	if (!masterStyle->vertical &&
		(drawArgs->width > rightEdge) &&
		(numExpandWE > 0))
	{
		int numExpand = 0;
		int spaceRemaining = drawArgs->width - rightEdge;

		/* Each element has 5 areas that can optionally expand. */
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			layout->temp = 0;

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			if (eLink1->flags & ELF_eEXPAND_W) layout->temp++;
			if (eLink1->flags & ELF_iEXPAND_W) layout->temp++;
			if (eLink1->flags & ELF_iEXPAND_X)
			{
				if ((eLink1->maxWidth < 0) ||
					(eLink1->maxWidth > layout->useWidth))
					layout->temp++;
			}
			if (eLink1->flags & ELF_iEXPAND_E) layout->temp++;
			if (eLink1->flags & ELF_eEXPAND_E) layout->temp++;

			numExpand += layout->temp;
		}

		while ((spaceRemaining > 0) && (numExpand > 0))
		{
			int each = (spaceRemaining >= numExpand) ? spaceRemaining / numExpand : 1;

			numExpand = 0;
			for (i = 0; i < eLinkCount; i++)
			{
				struct Layout *layout = &layouts[i];
				int spaceUsed;

				if (!layout->temp)
					continue;

				eLink1 = &eLinks1[i];

				spaceUsed = Style_DoExpandH(layout,
					layout->x + layout->ePadX[PAD_TOP_LEFT] + layout->iWidth +
					MAX(layout->ePadX[PAD_BOTTOM_RIGHT], layout->uPadX[PAD_BOTTOM_RIGHT]) +
					MIN(each * layout->temp, spaceRemaining));

				if (spaceUsed)
				{
					/* Shift following elements to the right */
					for (j = i + 1; j < eLinkCount; j++)
						if (!(eLinks1[j].flags & ELF_DETACH) &&
							(eLinks1[j].onion == NULL))
							layouts[j].x += spaceUsed;

					rightEdge += spaceUsed;

					spaceRemaining -= spaceUsed;
					if (!spaceRemaining)
						break;

					numExpand += layout->temp;
				}
				else
					layout->temp = 0;
			}
		}
	}

	/* Top-to-bottom layout. Expand some elements horizontally */
	if (masterStyle->vertical && (numExpandWE > 0))
	{
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];
			int right;

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			layout->temp = 0;
			Style_DoExpandH(layout, drawArgs->width);

			right = layout->x + layout->ePadX[PAD_TOP_LEFT] +
				layout->iWidth +
				MAX(layout->ePadX[PAD_BOTTOM_RIGHT], layout->uPadX[PAD_BOTTOM_RIGHT]);
			rightEdge = MAX(rightEdge, right);
		}
	}

	/* Now handle column justification */
	/* All the non-union, non-detach elements are moved as a group */
	if (drawArgs->width > rightEdge)
	{
		int dx = drawArgs->width - rightEdge;

		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			switch (drawArgs->justify)
			{
				case TK_JUSTIFY_LEFT:
					break;
				case TK_JUSTIFY_RIGHT:
					layout->x += dx;
					break;
				case TK_JUSTIFY_CENTER:
					layout->x += dx / 2;
					break;
			}
		}
	}

	/* Position and expand -detach elements */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		ePadX = eLink1->ePadX;
		iPadX = eLink1->iPadX;
		uPadX = layout->uPadX;

		layout->x = abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
		if (eLink1->flags & ELF_INDENT)
			layout->x += drawArgs->indent;
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];

		layout->temp = 0;
		Style_DoExpandH(layout, drawArgs->width);
	}

	/* Now calculate layout of -union elements. */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (eLink1->onion == NULL)
			continue;

		ePadX = eLink1->ePadX;
		iPadX = eLink1->iPadX;

		w = 1000000, e = -1000000;

		for (j = 0; j < eLink1->onionCount; j++)
		{
			struct Layout *layout2 = &layouts[eLink1->onion[j]];

			w = MIN(w, layout2->x + layout2->ePadX[PAD_TOP_LEFT]);
			e = MAX(e, layout2->x + layout2->ePadX[PAD_TOP_LEFT] + layout2->iWidth);
		}

		layout->x = w - iPadX[PAD_TOP_LEFT] - ePadX[PAD_TOP_LEFT];
		layout->useWidth = (e - w);
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];
	}

	/* Expand -union elements if needed: horizontal */
	/* Expansion of "-union" elements is different than non-"-union" elements */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];
		int extraWidth;

		eLink1 = &eLinks1[i];

		if ((eLink1->onion == NULL) || !(eLink1->flags & ELF_EXPAND_WE))
			continue;

		if (drawArgs->width - (layout->eWidth + drawArgs->indent) <= 0)
			continue;

		/* External and internal expansion: W */
		extraWidth = layout->x - drawArgs->indent;
		if ((extraWidth > 0) && (eLink1->flags & ELF_EXPAND_W))
		{
			if ((eLink1->flags & ELF_EXPAND_W) == ELF_EXPAND_W)
			{
				int eExtra = extraWidth / 2;
				int iExtra = extraWidth - extraWidth / 2;

				layout->x = drawArgs->indent;

				/* External expansion */
				layout->ePadX[PAD_TOP_LEFT] += eExtra;
				layout->eWidth += extraWidth;

				/* Internal expansion */
				layout->iPadX[PAD_TOP_LEFT] += iExtra;
				layout->iWidth += iExtra;
			}

			/* External expansion only: W */
			else if (eLink1->flags & ELF_eEXPAND_W)
			{
				layout->ePadX[PAD_TOP_LEFT] += extraWidth;
				layout->x = drawArgs->indent;
				layout->eWidth += extraWidth;
			}

			/* Internal expansion only: W */
			else
			{
				layout->iPadX[PAD_TOP_LEFT] += extraWidth;
				layout->x = drawArgs->indent;
				layout->iWidth += extraWidth;
				layout->eWidth += extraWidth;
			}
		}

		/* External and internal expansion: E */
		extraWidth = drawArgs->width - (layout->x + layout->eWidth);
		if ((extraWidth > 0) && (eLink1->flags & ELF_EXPAND_E))
		{
			if ((eLink1->flags & ELF_EXPAND_E) == ELF_EXPAND_E)
			{
				int eExtra = extraWidth / 2;
				int iExtra = extraWidth - extraWidth / 2;

				/* External expansion */
				layout->ePadX[PAD_BOTTOM_RIGHT] += eExtra;
				layout->eWidth += extraWidth; /* all the space */

				/* Internal expansion */
				layout->iPadX[PAD_BOTTOM_RIGHT] += iExtra;
				layout->iWidth += iExtra;
			}

			/* External expansion only: E */
			else if (eLink1->flags & ELF_eEXPAND_E)
			{
				layout->ePadX[PAD_BOTTOM_RIGHT] += extraWidth;
				layout->eWidth += extraWidth;
			}

			/* Internal expansion only: E */
			else
			{
				layout->iPadX[PAD_BOTTOM_RIGHT] += extraWidth;
				layout->iWidth += extraWidth;
				layout->eWidth += extraWidth;
			}
		}
	}

	/* Add internal padding to display area for -union elements */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];

		if (eLink1->onion == NULL)
			continue;

		iPadX = layout->iPadX;

		layout->useWidth += iPadX[PAD_TOP_LEFT] + iPadX[PAD_BOTTOM_RIGHT];
		iPadX[PAD_TOP_LEFT] = iPadX[PAD_BOTTOM_RIGHT] = 0;
	}

	return eLinkCount;
}

static int Style_DoLayoutV(StyleDrawArgs *drawArgs, struct Layout layouts[])
{
	Style *style = (Style *) drawArgs->style;
	Style *masterStyle = style->master;
	ElementLink *eLinks1, *eLinks2, *eLink1, *eLink2;
	int y = 0;
	int n, s;
	int *ePadY, *iPadY, *uPadY;
	int numExpandNS = 0;
	int numSqueezeY = 0;
	int i, j, eLinkCount = 0;
	int bottomEdge = 0;

	eLinks1 = masterStyle->elements;
	eLinks2 = style->elements;
	eLinkCount = style->numElements;

	for (i = 0; i < eLinkCount; i++)
	{
		eLink1 = &eLinks1[i];

		/* Count all non-union, non-detach squeezeable elements */
		if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;
		if (eLink1->flags & ELF_SQUEEZE_Y)
			numSqueezeY++;
	}

	/* Top-top-bottom layout. Make the height of some elements less than they
	 * need */
	if (masterStyle->vertical &&
		(drawArgs->height < style->neededHeight) &&
		(numSqueezeY > 0))
	{
		int numSqueeze = numSqueezeY;
		int spaceRemaining  = style->neededHeight - drawArgs->height;

		while ((spaceRemaining > 0) && (numSqueeze > 0))
		{
			int each = (spaceRemaining >= numSqueeze) ? (spaceRemaining / numSqueeze) : 1;

			numSqueeze = 0;
			for (i = 0; i < eLinkCount; i++)
			{
				struct Layout *layout = &layouts[i];
				int min = 0;

				eLink1 = &eLinks1[i];

				if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
					continue;

				if (!(eLink1->flags & ELF_SQUEEZE_Y))
					continue;

				if (eLink1->minHeight >= 0)
					min = eLink1->minHeight;
				if (layout->useHeight > min)
				{
					int sub = MIN(each, layout->useHeight - min);
					layout->useHeight -= sub;
					spaceRemaining -= sub;
					if (!spaceRemaining) break;
					if (layout->useHeight > min)
						numSqueeze++;
				}
			}
		}
	}

	/* Reduce the height of all non-union elements, except for the
	 * cases handled above. */
	if (drawArgs->height < style->neededHeight)
	{
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];
			int height, subtract;

			eLink1 = &eLinks1[i];

			if (eLink1->onion != NULL)
				continue;

			if (!(eLink1->flags & ELF_SQUEEZE_Y))
				continue;

			if (!(eLink1->flags & ELF_DETACH) && masterStyle->vertical)
				continue;

			ePadY = eLink1->ePadY;
			iPadY = eLink1->iPadY;
			uPadY = layout->uPadY;

			height =
				MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]) +
				iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT] +
				MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]);
			subtract = height - drawArgs->height;

			if (subtract > 0)
			{
				if ((eLink1->minHeight >= 0) &&
					(eLink1->minHeight <= layout->useHeight) &&
					(layout->useHeight - subtract < eLink1->minHeight))
					layout->useHeight = eLink1->minHeight;
				else
					layout->useHeight -= subtract;
			}
		}
	}

	/* Layout elements top-to-bottom */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;
		uPadY = layout->uPadY;

		if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		layout->y = y + abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		if (masterStyle->vertical)
			y = layout->y + layout->eHeight;

		if (masterStyle->vertical)
		{
			bottomEdge = layout->y + layout->ePadY[PAD_TOP_LEFT] +
				layout->iHeight +
				MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]);
		}

		/* Count number that want to expand */
		if (eLink1->flags & (ELF_EXPAND_NS | ELF_iEXPAND_Y))
			numExpandNS++;
	}

	/* Top-to-bottom layout. Expand some elements vertically if we have
	 * more space available vertically than is needed by the Style. */
	if (masterStyle->vertical &&
		(drawArgs->height > bottomEdge) &&
		(numExpandNS > 0))
	{
		int numExpand = 0;
		int spaceRemaining = drawArgs->height - bottomEdge;

		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			layout->temp = 0;

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			if (eLink1->flags & ELF_eEXPAND_N) layout->temp++;
			if (eLink1->flags & ELF_iEXPAND_N) layout->temp++;
			if (eLink1->flags & ELF_iEXPAND_Y)
			{
				if ((eLink1->maxHeight < 0) ||
					(eLink1->maxHeight > layout->useHeight))
					layout->temp++;
			}
			if (eLink1->flags & ELF_iEXPAND_S) layout->temp++;
			if (eLink1->flags & ELF_eEXPAND_S) layout->temp++;

			numExpand += layout->temp;
		}

		while ((spaceRemaining > 0) && (numExpand > 0))
		{
			int each = (spaceRemaining >= numExpand) ? spaceRemaining / numExpand : 1;

			numExpand = 0;
			for (i = 0; i < eLinkCount; i++)
			{
				struct Layout *layout = &layouts[i];
				int spaceUsed;

				if (!layout->temp)
					continue;

				eLink1 = &eLinks1[i];

				spaceUsed = Style_DoExpandV(layout,
					layout->y + layout->ePadY[PAD_TOP_LEFT] + layout->iHeight +
					MAX(layout->ePadY[PAD_BOTTOM_RIGHT], layout->uPadY[PAD_BOTTOM_RIGHT]) +
					MIN(each * layout->temp, spaceRemaining));

				if (spaceUsed)
				{
					/* Shift following elements down */
					for (j = i + 1; j < eLinkCount; j++)
						if (!(eLinks1[j].flags & ELF_DETACH) &&
							(eLinks1[j].onion == NULL))
							layouts[j].y += spaceUsed;

					spaceRemaining -= spaceUsed;
					if (!spaceRemaining)
						break;

					numExpand += layout->temp;
				}
				else
					layout->temp = 0;
			}
		}
	}

	/* Left-to-right layout. Expand some elements vertically */
	if (!masterStyle->vertical && (numExpandNS > 0))
	{
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			layout->temp = 0;
			Style_DoExpandV(layout, drawArgs->height);
		}
	}

	/* Position and expand -detach elements */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;
		uPadY = layout->uPadY;

		layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		layout->temp = 0;
		Style_DoExpandV(layout, drawArgs->height);
	}

	/* Now calculate layout of -union elements. */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (eLink1->onion == NULL)
			continue;

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;

		n = 1000000, s = -1000000;

		for (j = 0; j < eLink1->onionCount; j++)
		{
			struct Layout *layout2 = &layouts[eLink1->onion[j]];

			n = MIN(n, layout2->y + layout2->ePadY[PAD_TOP_LEFT]);
			s = MAX(s, layout2->y + layout2->ePadY[PAD_TOP_LEFT] + layout2->iHeight);
		}

		layout->y = n - iPadY[PAD_TOP_LEFT] - ePadY[PAD_TOP_LEFT];
		layout->useHeight = (s - n);
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];
	}

	/* Expand -union elements if needed: vertical */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];
		int extraHeight;

		eLink1 = &eLinks1[i];

		if ((eLink1->onion == NULL) || !(eLink1->flags & ELF_EXPAND_NS))
			continue;

		if (drawArgs->height - layout->eHeight <= 0)
			continue;

		/* External and internal expansion: N */
		extraHeight = layout->y;
		if ((extraHeight > 0) && (eLink1->flags & ELF_EXPAND_N))
		{
			if ((eLink1->flags & ELF_EXPAND_N) == ELF_EXPAND_N)
			{
				int eExtra = extraHeight / 2;
				int iExtra = extraHeight - extraHeight / 2;

				/* External expansion */
				layout->ePadY[PAD_TOP_LEFT] += eExtra;
				layout->y = 0;
				layout->eHeight += extraHeight;

				/* Internal expansion */
				layout->iPadY[PAD_TOP_LEFT] += iExtra;
				layout->iHeight += iExtra;
			}

			/* External expansion only: N */
			else if (eLink1->flags & ELF_eEXPAND_N)
			{
				layout->ePadY[PAD_TOP_LEFT] += extraHeight;
				layout->y = 0;
				layout->eHeight += extraHeight;
			}

			/* Internal expansion only: N */
			else
			{
				layout->iPadY[PAD_TOP_LEFT] += extraHeight;
				layout->y = 0;
				layout->iHeight += extraHeight;
				layout->eHeight += extraHeight;
			}
		}

		/* External and internal expansion: S */
		extraHeight = drawArgs->height - (layout->y + layout->eHeight);
		if ((extraHeight > 0) && (eLink1->flags & ELF_EXPAND_S))
		{
			if ((eLink1->flags & ELF_EXPAND_S) == ELF_EXPAND_S)
			{
				int eExtra = extraHeight / 2;
				int iExtra = extraHeight - extraHeight / 2;

				/* External expansion */
				layout->ePadY[PAD_BOTTOM_RIGHT] += eExtra;
				layout->eHeight += extraHeight; /* all the space */

				/* Internal expansion */
				layout->iPadY[PAD_BOTTOM_RIGHT] += iExtra;
				layout->iHeight += iExtra;
			}

			/* External expansion only: S */
			else if (eLink1->flags & ELF_eEXPAND_S)
			{
				layout->ePadY[PAD_BOTTOM_RIGHT] += extraHeight;
				layout->eHeight += extraHeight;
			}

			/* Internal expansion only */
			else
			{
				layout->iPadY[PAD_BOTTOM_RIGHT] += extraHeight;
				layout->iHeight += extraHeight;
				layout->eHeight += extraHeight;
			}
		}
	}

	/* Add internal padding to display area for -union elements */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];

		if (eLink1->onion == NULL)
			continue;

		iPadY = layout->iPadY;

		layout->useHeight += iPadY[PAD_TOP_LEFT] + iPadY[PAD_BOTTOM_RIGHT];
		iPadY[PAD_TOP_LEFT] = iPadY[PAD_BOTTOM_RIGHT] = 0;
	}

	return eLinkCount;
}

/* Calculate the height and width of all the Elements */
static void Layout_Size(int vertical, int numLayouts, struct Layout layouts[], int *widthPtr, int *heightPtr)
{
	int i, W, N, E, S;
	int width = 0, height = 0;

	W = 1000000, N = 1000000, E = -1000000, S = -1000000;

	for (i = 0; i < numLayouts; i++)
	{
		struct Layout *layout = &layouts[i];
		int w, n, e, s;
		int *ePadX, *iPadX, *uPadX, *ePadY, *iPadY, *uPadY;

		ePadX = layout->ePadX, iPadX = layout->iPadX, uPadX = layout->uPadX;
		ePadY = layout->ePadY, iPadY = layout->iPadY, uPadY = layout->uPadY;

		w = layout->x + ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]);
		n = layout->y + ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]);
		e = layout->x + layout->eWidth - ePadX[PAD_BOTTOM_RIGHT] + MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]);
		s = layout->y + layout->eHeight - ePadY[PAD_BOTTOM_RIGHT] + MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]);

		if (vertical)
		{
			N = MIN(N, n);
			S = MAX(S, s);
			width = MAX(width, e - w);
		}
		else
		{
			W = MIN(W, w);
			E = MAX(E, e);
			height = MAX(height, s - n);
		}
	}

	if (vertical)
		height = MAX(height, S - N);
	else
		width = MAX(width, E - W);

	(*widthPtr) = width;
	(*heightPtr) = height;
}

/* */
void Style_DoLayoutNeededV(StyleDrawArgs *drawArgs, struct Layout layouts[])
{
	Style *style = (Style *) drawArgs->style;
	Style *masterStyle = style->master;
	ElementLink *eLinks1, *eLinks2, *eLink1, *eLink2;
	int *ePadY, *iPadY, *uPadY;
	int i;
	int y = 0;

	eLinks1 = masterStyle->elements;
	eLinks2 = style->elements;

	/* Layout elements left-to-right, or top-to-bottom */
	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		/* The size of a -union element is determined by the elements
		 * it surrounds */
		if (eLink1->onion != NULL)
		{
			/* I don't need good values because I'm only calculating the
			 * needed height */
			layout->y = layout->iHeight = layout->eHeight = 0;
			continue;
		}

		/* -detach elements are positioned by themselves */
		if (eLink1->flags & ELF_DETACH)
			continue;

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;
		uPadY = layout->uPadY;

		layout->y = y + abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		if (masterStyle->vertical)
			y = layout->y + layout->eHeight;
	}

	/* -detach elements */
	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;
		uPadY = layout->uPadY;

		layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];
	}
}

/* Arrange all the Elements considering drawArgs.width and maybe drawArgs.height */
static void Style_DoLayout(StyleDrawArgs *drawArgs, struct Layout layouts[],
	int neededV, char *file, int line)
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	int i;

	if (style->neededWidth == -1)
		panic("Style_DoLayout(file %s line %d): style.neededWidth == -1",
			file, line);
	if (style->minWidth + drawArgs->indent > drawArgs->width)
		panic("Style_DoLayout(file %s line %d): style.minWidth + drawArgs->indent %d > drawArgs.width %d",
			file, line, style->minWidth + drawArgs->indent, drawArgs->width);

	Style_DoLayoutH(drawArgs, layouts);

	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];
		ElementLink *eLink1 = layout->master;
		ElementLink *eLink2 = layout->eLink;
		ElementArgs args;

		/* The size of a -union element is determined by the elements
		 * it surrounds */
		if (eLink1->onion != NULL)
		{
			layout->useHeight = 0;
			continue;
		}

		layout->useHeight = eLink2->neededHeight;

		/* If a Text Element is given less width than it needs (due to
		 * -squeeze x layout), then it may wrap lines. This means
		 * the height can vary depending on the width. */
		if (eLink2->elem->typePtr->heightProc == NULL)
			continue;

		if (eLink1->fixedHeight >= 0)
			continue;

		/* Not squeezed */
		if (layout->useWidth >= eLink2->neededWidth)
			continue;

		/* Already calculated the height at this width */
		if (layout->useWidth == eLink2->layoutWidth)
		{
			layout->useHeight = eLink2->layoutHeight;
			continue;
		}
#if 0
		/* */
		if ((eLink2->layoutWidth == -1) &&
			(layout->useWidth >= eLink2->neededWidth))
			continue;
#endif
		args.tree = tree;
		args.state = state;
		args.elem = eLink2->elem;
		args.height.fixedWidth = layout->useWidth;
		(*args.elem->typePtr->heightProc)(&args);

		if (eLink1->fixedHeight >= 0)
			layout->useHeight = eLink1->fixedHeight;
		else if ((eLink1->minHeight >= 0) &&
			(args.height.height <  eLink1->minHeight))
			layout->useHeight = eLink1->minHeight;
		else if ((eLink1->maxHeight >= 0) &&
			(args.height.height >  eLink1->maxHeight))
			layout->useHeight = eLink1->maxHeight;
		else
			layout->useHeight = args.height.height;

		eLink2->layoutWidth = layout->useWidth;
		eLink2->layoutHeight = layout->useHeight;
	}

	neededV ?
		Style_DoLayoutNeededV(drawArgs, layouts) :
		Style_DoLayoutV(drawArgs, layouts);
}

/* Arrange Elements to determine the needed height and width of the Style */
static int Style_NeededSize(TreeCtrl *tree, Style *style, int state,
	int *widthPtr, int *heightPtr, int squeeze)
{
	Style *masterStyle = style->master;
	ElementLink *eLinks1, *eLinks2, *eLink1, *eLink2;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
	int *ePadX, *iPadX, *uPadX, *ePadY, *iPadY, *uPadY;
	int i, j;
	int x = 0, y = 0;
	int hasSqueeze = 0;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	if (style->master != NULL)
	{
		eLinks1 = masterStyle->elements;
		eLinks2 = style->elements;
	}
	else
	{
		eLinks1 = masterStyle->elements;
		eLinks2 = masterStyle->elements;
	}

	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		/* No -union padding yet */
		layout->uPadX[PAD_TOP_LEFT]     = 0;
		layout->uPadX[PAD_BOTTOM_RIGHT] = 0;
		layout->uPadY[PAD_TOP_LEFT]     = 0;
		layout->uPadY[PAD_BOTTOM_RIGHT] = 0;

		eLink1 = &eLinks1[i];
		if ((eLink1->onion == NULL) &&
			(eLink1->flags & (ELF_SQUEEZE_X | ELF_SQUEEZE_Y)))
			hasSqueeze = 1;

	}

	/* Figure out the padding around elements surrounded by -union elements */
	for (i = 0; i < style->numElements; i++)
	{
		eLink1 = &eLinks1[i];

		if (eLink1->onion == NULL)
			continue;

		ePadX = eLink1->ePadX;
		ePadY = eLink1->ePadY;
		iPadX = eLink1->iPadX;
		iPadY = eLink1->iPadY;

		for (j = 0; j < eLink1->onionCount; j++)
		{
			struct Layout *layout = &layouts[eLink1->onion[j]];

			uPadX = layout->uPadX;
			uPadY = layout->uPadY;

			if (masterStyle->vertical)
			{
				uPadX[PAD_TOP_LEFT] = MAX(uPadX[PAD_TOP_LEFT], iPadX[PAD_TOP_LEFT] + ePadX[PAD_TOP_LEFT]);
				uPadX[PAD_BOTTOM_RIGHT] = MAX(uPadX[PAD_BOTTOM_RIGHT], iPadX[PAD_BOTTOM_RIGHT] + ePadX[PAD_BOTTOM_RIGHT]);
				if (j == 0) /* topmost */
					uPadY[PAD_TOP_LEFT] = MAX(uPadY[PAD_TOP_LEFT], iPadY[PAD_TOP_LEFT] + ePadY[PAD_TOP_LEFT]);
				if (j == eLink1->onionCount - 1) /* bottommost */
					uPadY[PAD_BOTTOM_RIGHT] = MAX(uPadY[PAD_BOTTOM_RIGHT], iPadY[PAD_BOTTOM_RIGHT] + ePadY[PAD_BOTTOM_RIGHT]);
			}
			else
			{
				if (j == 0) /* leftmost */
					uPadX[PAD_TOP_LEFT] = MAX(uPadX[PAD_TOP_LEFT], iPadX[PAD_TOP_LEFT] + ePadX[PAD_TOP_LEFT]);
				if (j == eLink1->onionCount - 1) /* rightmost */
					uPadX[PAD_BOTTOM_RIGHT] = MAX(uPadX[PAD_BOTTOM_RIGHT], iPadX[PAD_BOTTOM_RIGHT] + ePadX[PAD_BOTTOM_RIGHT]);
				uPadY[PAD_TOP_LEFT] = MAX(uPadY[PAD_TOP_LEFT], iPadY[PAD_TOP_LEFT] + ePadY[PAD_TOP_LEFT]);
				uPadY[PAD_BOTTOM_RIGHT] = MAX(uPadY[PAD_BOTTOM_RIGHT], iPadY[PAD_BOTTOM_RIGHT] + ePadY[PAD_BOTTOM_RIGHT]);
			}
		}
	}

	/* Layout elements left-to-right, or top-to-bottom */
	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		ePadX = eLink1->ePadX;
		ePadY = eLink1->ePadY;
		iPadX = eLink1->iPadX;
		iPadY = eLink1->iPadY;
		uPadX = layout->uPadX;
		uPadY = layout->uPadY;

		/* The size of a -union element is determined by the elements
		 * it surrounds */
		if (eLink1->onion != NULL)
		{
			layout->x = layout->y = layout->eWidth = layout->eHeight = 0;
			layout->ePadX[PAD_TOP_LEFT]     = 0;
			layout->ePadX[PAD_BOTTOM_RIGHT] = 0;
			layout->ePadY[PAD_TOP_LEFT]     = 0;
			layout->ePadY[PAD_BOTTOM_RIGHT] = 0;
			layout->iPadX[PAD_TOP_LEFT]     = 0;
			layout->iPadX[PAD_BOTTOM_RIGHT] = 0;
			layout->iPadY[PAD_TOP_LEFT]     = 0;
			layout->iPadY[PAD_BOTTOM_RIGHT] = 0;
			continue;
		}

		if ((eLink2->neededWidth == -1) || (eLink2->neededHeight == -1))
		{
			if ((eLink1->fixedWidth >= 0) && (eLink1->fixedHeight >= 0))
			{
				eLink2->neededWidth = eLink1->fixedWidth;
				eLink2->neededHeight = eLink1->fixedHeight;
			}
			else
			{
				ElementArgs args;
				int width, height;

				args.tree = tree;
				args.state = state;
				args.elem = eLink2->elem;
				args.needed.fixedWidth = eLink1->fixedWidth;
				args.needed.fixedHeight = eLink1->fixedHeight;
				if (eLink1->maxWidth > eLink1->minWidth)
					args.needed.maxWidth = eLink1->maxWidth;
				else
					args.needed.maxWidth = -1;
				if (eLink1->maxHeight > eLink1->minHeight)
					args.needed.maxHeight = eLink1->maxHeight;
				else
					args.needed.maxHeight = -1;
				(*args.elem->typePtr->neededProc)(&args);
				width = args.needed.width;
				height = args.needed.height;

				if (eLink1->fixedWidth >= 0)
					width = eLink1->fixedWidth;
				else if ((eLink1->minWidth >= 0) &&
					(width < eLink1->minWidth))
					width = eLink1->minWidth;
				else if ((eLink1->maxWidth >= 0) &&
					(width > eLink1->maxWidth))
					width = eLink1->maxWidth;

				if (eLink1->fixedHeight >= 0)
					height = eLink1->fixedHeight;
				else if ((eLink1->minHeight >= 0) &&
					(height < eLink1->minHeight))
					height = eLink1->minHeight;
				else if ((eLink1->maxHeight >= 0) &&
					(height > eLink1->maxHeight))
					height = eLink1->maxHeight;

				eLink2->neededWidth = width;
				eLink2->neededHeight = height;

				eLink2->layoutWidth = -1;
			}
		}

		layout->useWidth = eLink2->neededWidth;
		layout->useHeight = eLink2->neededHeight;
		if (squeeze)
		{
			if (eLink1->flags & ELF_SQUEEZE_X)
			{
				if ((eLink1->minWidth >= 0) &&
					(eLink1->minWidth <= layout->useWidth))
					layout->useWidth = eLink1->minWidth;
				else
					layout->useWidth = 0;
			}
			if (eLink1->flags & ELF_SQUEEZE_Y)
			{
				if ((eLink1->minHeight >= 0) &&
					(eLink1->minHeight <= layout->useHeight))
					layout->useHeight = eLink1->minHeight;
				else
					layout->useHeight = 0;
			}
		}

		/* -detach elements are positioned by themselves */
		if (eLink1->flags & ELF_DETACH)
			continue;

		layout->eLink = eLink2;
		layout->x = x + abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
		layout->y = y + abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		for (j = 0; j < 2; j++)
		{
			layout->ePadX[j] = eLink1->ePadX[j];
			layout->ePadY[j] = eLink1->ePadY[j];
			layout->iPadX[j] = eLink1->iPadX[j];
			layout->iPadY[j] = eLink1->iPadY[j];
		}

		if (masterStyle->vertical)
			y = layout->y + layout->eHeight;
		else
			x = layout->x + layout->eWidth;
	}

	/* -detach elements */
	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		ePadX = eLink1->ePadX;
		ePadY = eLink1->ePadY;
		iPadX = eLink1->iPadX;
		iPadY = eLink1->iPadY;
		uPadX = layout->uPadX;
		uPadY = layout->uPadY;

		layout->eLink = eLink2;
		layout->master = eLink1;
		layout->x = abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
		layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		for (j = 0; j < 2; j++)
		{
			layout->ePadX[j] = eLink1->ePadX[j];
			layout->ePadY[j] = eLink1->ePadY[j];
			layout->iPadX[j] = eLink1->iPadX[j];
			layout->iPadY[j] = eLink1->iPadY[j];
		}
	}

	Layout_Size(masterStyle->vertical, style->numElements, layouts,
		widthPtr, heightPtr);

	STATIC_FREE(layouts, struct Layout, style->numElements);

	return hasSqueeze;
}

static void Style_CheckNeededSize(TreeCtrl *tree, Style *style, int state)
{
	if (style->neededWidth == -1)
	{
		int hasSqueeze = Style_NeededSize(tree, style, state,
			&style->neededWidth, &style->neededHeight, FALSE);
		if (hasSqueeze)
		{
			Style_NeededSize(tree, style, state, &style->minWidth,
				&style->minHeight, TRUE);
		}
		else
		{
			style->minWidth = style->neededWidth;
			style->minHeight = style->neededHeight;
		}
		style->layoutWidth = -1;
#ifdef NEEDEDHAX
		style->neededState = state;
#endif
	}
#ifdef NEEDEDHAX
	if (style->neededState != state)
		panic("Style_CheckNeededSize: neededState %d != state %d\n",
			style->neededState, state);
#endif
}

int TreeStyle_NeededWidth(TreeCtrl *tree, TreeStyle style_, int state)
{
	Style *style = (Style *) style_;

	Style_CheckNeededSize(tree, style, state);
	return style->neededWidth;
}

int TreeStyle_NeededHeight(TreeCtrl *tree, TreeStyle style_, int state)
{
	Style *style = (Style *) style_;

	Style_CheckNeededSize(tree, style, state);
	return style->neededHeight;
}

/* Calculate height of Style considering drawArgs.width */
int TreeStyle_UseHeight(StyleDrawArgs *drawArgs)
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
	int width, height;

	Style_CheckNeededSize(tree, style, state);

	/*
	 * If we have:
	 * a) infinite space available, or
	 * b) more width than the style needs, or
	 * c) less width than the style needs, but it has no -squeeze x elements
	 * then return the needed height of the style. This is safe since no
	 * text elements will be growing vertically when lines wrap.
	 */
	if ((drawArgs->width == -1) ||
		(drawArgs->width >= style->neededWidth + drawArgs->indent) ||
		(style->neededWidth == style->minWidth))
	{
		return style->neededHeight;
	}

	/* We never lay out the style at less than the minimum width */
	if (drawArgs->width < style->minWidth + drawArgs->indent)
		drawArgs->width = style->minWidth + drawArgs->indent;

	/* We have less space than the style needs, and have already calculated
	 * the height of the style at this width. (The height may change because
	 * of text elements wrapping lines). */
	if (drawArgs->width == style->layoutWidth)
		return style->layoutHeight;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, TRUE, __FILE__, __LINE__);

	Layout_Size(style->master->vertical, style->numElements, layouts,
		&width, &height);

	STATIC_FREE(layouts, struct Layout, style->numElements);

	style->layoutWidth = drawArgs->width;
	style->layoutHeight = height;

	return height;
}

void TreeStyle_Draw(StyleDrawArgs *drawArgs)
{
	Style *style = (Style *) drawArgs->style;
	TreeCtrl *tree = drawArgs->tree;
	ElementArgs args;
	int i;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
	int debugDraw = FALSE;

	if (drawArgs->width < style->minWidth + drawArgs->indent)
		drawArgs->width = style->minWidth + drawArgs->indent;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

	args.tree = tree;
	args.state = drawArgs->state;
	args.display.drawable = drawArgs->drawable;

	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		if (debugDraw && layout->master->onion != NULL)
			continue;

		if ((layout->useWidth > 0) && (layout->useHeight > 0))
		{
			args.elem = layout->eLink->elem;
			args.display.x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
			args.display.y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
			args.display.x += layout->iPadX[PAD_TOP_LEFT];
			args.display.y += layout->iPadY[PAD_TOP_LEFT];
			args.display.width = layout->useWidth;
			args.display.height = layout->useHeight;
			args.display.sticky = layout->master->flags & ELF_STICKY;
			if (debugDraw)
			{
				XColor *color[3];
				GC gc[3];

				if (layout->master->onion != NULL)
				{
					color[0] = Tk_GetColor(tree->interp, tree->tkwin, "blue2");
					gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
					color[1] = Tk_GetColor(tree->interp, tree->tkwin, "blue3");
					gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));
				}
				else
				{
					color[0] = Tk_GetColor(tree->interp, tree->tkwin, "gray50");
					gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
					color[1] = Tk_GetColor(tree->interp, tree->tkwin, "gray60");
					gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));
					color[2] = Tk_GetColor(tree->interp, tree->tkwin, "gray70");
					gc[2] = Tk_GCForColor(color[2], Tk_WindowId(args.tree->tkwin));
				}

				/* external */
				XFillRectangle(tree->display, args.display.drawable,
					gc[2],
					args.display.x - layout->ePadX[PAD_TOP_LEFT],
					args.display.y - layout->ePadY[PAD_TOP_LEFT],
					layout->eWidth, layout->eHeight);
				/* internal */
				XFillRectangle(tree->display, args.display.drawable,
					gc[1],
					args.display.x, args.display.y,
					args.display.width, args.display.height);
				/* needed */
				if (!layout->master->onion && !(layout->master->flags & ELF_DETACH))
				XFillRectangle(tree->display, args.display.drawable,
					gc[0],
					args.display.x + layout->iPadX[PAD_TOP_LEFT],
					args.display.y + layout->iPadY[PAD_TOP_LEFT],
					layout->eLink->neededWidth, layout->eLink->neededHeight);
			}
			else
				(*args.elem->typePtr->displayProc)(&args);
		}
	}

	if (debugDraw)
		for (i = 0; i < style->numElements; i++)
		{
			struct Layout *layout = &layouts[i];

			if (layout->master->onion == NULL)
				continue;
			if (layout->useWidth > 0 && layout->useHeight > 0)
			{
				args.elem = layout->eLink->elem;
				args.display.x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
				args.display.y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
				args.display.width = layout->iWidth;
				args.display.height = layout->iHeight;
				{
					XColor *color[3];
					GC gc[3];

					color[0] = Tk_GetColor(tree->interp, tree->tkwin, "blue2");
					gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
					color[1] = Tk_GetColor(tree->interp, tree->tkwin, "blue3");
					gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));

					/* external */
					XDrawRectangle(tree->display, args.display.drawable,
						gc[0],
						args.display.x - layout->ePadX[PAD_TOP_LEFT],
						args.display.y - layout->ePadY[PAD_TOP_LEFT],
						layout->eWidth - 1, layout->eHeight - 1);
					/* internal */
					XDrawRectangle(tree->display, args.display.drawable,
						gc[1],
						args.display.x, args.display.y,
						args.display.width - 1, args.display.height - 1);
				}
			}
		}

	STATIC_FREE(layouts, struct Layout, style->numElements);
}

void TreeStyle_UpdateWindowPositions(StyleDrawArgs *drawArgs)
{
	Style *style = (Style *) drawArgs->style;
	TreeCtrl *tree = drawArgs->tree;
	ElementArgs args;
	int i;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;

	if (drawArgs->width < style->minWidth + drawArgs->indent)
		drawArgs->width = style->minWidth + drawArgs->indent;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

	args.tree = tree;
	args.state = drawArgs->state;
	args.display.drawable = drawArgs->drawable;

	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		if (!ELEMENT_TYPE_MATCHES(layout->eLink->elem->typePtr, &elemTypeWindow))
			continue;

		if ((layout->useWidth > 0) && (layout->useHeight > 0))
		{
			args.elem = layout->eLink->elem;
			args.display.x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
			args.display.y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
			args.display.x += layout->iPadX[PAD_TOP_LEFT];
			args.display.y += layout->iPadY[PAD_TOP_LEFT];
			args.display.width = layout->useWidth;
			args.display.height = layout->useHeight;
			args.display.sticky = layout->master->flags & ELF_STICKY;
			(*args.elem->typePtr->displayProc)(&args);
		}
	}

	STATIC_FREE(layouts, struct Layout, style->numElements);
}

void TreeStyle_HideWindows(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	ElementArgs args;
	int i;

	args.tree = tree;

	for (i = 0; i < style->numElements; i++)
	{
		ElementLink *eLink = &style->elements[i];

		if (!ELEMENT_TYPE_MATCHES(eLink->elem->typePtr, &elemTypeWindow))
			continue;

		args.elem = eLink->elem;
		args.display.width = -1;
		args.display.height = -1;
		(*args.elem->typePtr->displayProc)(&args);
	}
}

static void Element_FreeResources(TreeCtrl *tree, Element *elem)
{
	ElementArgs args;
	Tcl_HashEntry *hPtr;

	if (elem->master == NULL)
	{
		hPtr = Tcl_FindHashEntry(&tree->elementHash, elem->name);
		Tcl_DeleteHashEntry(hPtr);
	}
	args.tree = tree;
	args.elem = elem;
	(*elem->typePtr->deleteProc)(&args);
	Tk_FreeConfigOptions((char *) elem,
		elem->typePtr->optionTable,
		tree->tkwin);
#ifdef ALLOC_HAX
	AllocHax_Free(tree->allocData, (char *) elem, elem->typePtr->size);
#else
	WFREE(elem, Element);
#endif
}

static ElementLink *ElementLink_Init(ElementLink *eLink, Element *elem)
{
	memset(eLink, '\0', sizeof(ElementLink));
	eLink->elem = elem;
	eLink->flags |= ELF_INDENT;
	eLink->minWidth = eLink->fixedWidth = eLink->maxWidth = -1;
	eLink->minHeight = eLink->fixedHeight = eLink->maxHeight = -1;
	eLink->flags |= ELF_STICKY;
	return eLink;
}

static void ElementLink_FreeResources(TreeCtrl *tree, ElementLink *eLink)
{
	if (eLink->elem->master != NULL)
		Element_FreeResources(tree, eLink->elem);
	if (eLink->onion != NULL)
		wipefree((char *) eLink->onion, sizeof(int) * eLink->onionCount);
}

void TreeStyle_FreeResources(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	int i;
	Tcl_HashEntry *hPtr;

	if (style->master == NULL)
	{
		hPtr = Tcl_FindHashEntry(&tree->styleHash, style->name);
		Tcl_DeleteHashEntry(hPtr);
	}
	if (style->numElements > 0)
	{
		for (i = 0; i < style->numElements; i++)
			ElementLink_FreeResources(tree, &style->elements[i]);
#ifdef ALLOC_HAX
		AllocHax_CFree(tree->allocData, (char *) style->elements, sizeof(ElementLink), style->numElements, 5);
#else
		wipefree((char *) style->elements, sizeof(ElementLink) *
			style->numElements);
#endif
	}
#ifdef ALLOC_HAX
	AllocHax_Free(tree->allocData, (char *) style, sizeof(Style));
#else
	WFREE(style, Style);
#endif
}

static ElementLink *Style_FindElem(TreeCtrl *tree, Style *style, Element *master, int *index)
{
	int i;

	for (i = 0; i < style->numElements; i++)
	{
		if (style->elements[i].elem->name == master->name)
		{
			if (index != NULL) (*index) = i;
			return &style->elements[i];
		}
	}

	return NULL;
}

int TreeStyle_FindElement(TreeCtrl *tree, TreeStyle style_, TreeElement elem_, int *index)
{
	if (Style_FindElem(tree, (Style *) style_, (Element *) elem_, index) == NULL)
	{
		FormatResult(tree->interp, "style %s does not use element %s",
			((Style *) style_)->name, ((Element *) elem_)->name);
		return TCL_ERROR;
	}
	return TCL_OK;
}

static Element *Element_CreateAndConfig(TreeCtrl *tree, TreeItem item,
	TreeItemColumn column, Element *masterElem, ElementType *type,
	CONST char *name, int objc, Tcl_Obj *CONST objv[])
{
	Element *elem;
	ElementArgs args;

	if (masterElem != NULL)
	{
		type = masterElem->typePtr;
		name = masterElem->name;
	}

#ifdef ALLOC_HAX
	elem = (Element *) AllocHax_Alloc(tree->allocData, type->size);
#else
	elem = (Element *) ckalloc(type->size);
#endif
	memset(elem, '\0', type->size);
	elem->name = Tk_GetUid(name);
	elem->typePtr = type;
	elem->master = masterElem;

	args.tree = tree;
	args.elem = elem;
	args.create.item = item;
	args.create.column = column;
	if ((*type->createProc)(&args) != TCL_OK)
	{
#ifdef ALLOC_HAX
		AllocHax_Free(tree->allocData, (char *) elem, type->size);
#else
		WFREE(elem, Element);
#endif
		return NULL;
	}

	if (Tk_InitOptions(tree->interp, (char *) elem,
		type->optionTable, tree->tkwin) != TCL_OK)
	{
#ifdef ALLOC_HAX
		AllocHax_Free(tree->allocData, (char *) elem, type->size);
#else
		WFREE(elem, Element);
#endif
		return NULL;
	}
	args.config.objc = objc;
	args.config.objv = objv;
	args.config.flagSelf = 0;
	if ((*type->configProc)(&args) != TCL_OK)
	{
		(*type->deleteProc)(&args);
		Tk_FreeConfigOptions((char *) elem,
			elem->typePtr->optionTable,
			tree->tkwin);
#ifdef ALLOC_HAX
		AllocHax_Free(tree->allocData, (char *) elem, type->size);
#else
		WFREE(elem, Element);
#endif
		return NULL;
	}

	args.change.flagSelf = args.config.flagSelf;
	args.change.flagTree = 0;
	args.change.flagMaster = 0;
	(*type->changeProc)(&args);

	return elem;
}

/* Create an instance Element if it doesn't exist in this Style */
static ElementLink *Style_CreateElem(TreeCtrl *tree, TreeItem item,
	TreeItemColumn column, Style *style, Element *masterElem, int *isNew)
{
	ElementLink *eLink = NULL;
	Element *elem;
	int i;

	if (masterElem->master != NULL)
		panic("Style_CreateElem called with instance Element");

	if (isNew != NULL) (*isNew) = FALSE;

	for (i = 0; i < style->numElements; i++)
	{
		eLink = &style->elements[i];

		if (eLink->elem == masterElem)
		{
			/* Master Styles use master Elements only */
			if (style->master == NULL)
				return eLink;

			/* Allocate instance Element here */
			break;
		}

		/* Instance Style already has instance Element */
		if (eLink->elem->name == masterElem->name)
			return eLink;
	}

	/* Error: Element isn't in the master Style */
	if (i == style->numElements)
		return NULL;

	elem = Element_CreateAndConfig(tree, item, column, masterElem, NULL, NULL, 0, NULL);
	if (elem == NULL)
		return NULL;

	eLink->elem = elem;
	if (isNew != NULL) (*isNew) = TRUE;
	return eLink;
}

TreeStyle TreeStyle_NewInstance(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	Style *copy;
	ElementLink *eLink;
	int i;

#ifdef ALLOC_HAX
	copy = (Style *) AllocHax_Alloc(tree->allocData, sizeof(Style));
#else
	copy = (Style *) ckalloc(sizeof(Style));
#endif
	memset(copy, '\0', sizeof(Style));
	copy->name = style->name;
	copy->neededWidth = -1;
	copy->neededHeight = -1;
	copy->master = style;
	copy->numElements = style->numElements;
	if (style->numElements > 0)
	{
#ifdef ALLOC_HAX
		copy->elements = (ElementLink *) AllocHax_CAlloc(tree->allocData, sizeof(ElementLink), style->numElements, 5);
#else
		copy->elements = (ElementLink *) ckalloc(sizeof(ElementLink) * style->numElements);
#endif
		memset(copy->elements, '\0', sizeof(ElementLink) * style->numElements);
		for (i = 0; i < style->numElements; i++)
		{
			eLink = &copy->elements[i];

			/* The only fields needed by an instance */
			eLink->elem = style->elements[i].elem;
			eLink->neededWidth = -1;
			eLink->neededHeight = -1;
		}
	}

	return (TreeStyle) copy;
}

static int Element_FromObj(TreeCtrl *tree, Tcl_Obj *obj, Element **elemPtr)
{
	char *name;
	Tcl_HashEntry *hPtr;

	name = Tcl_GetString(obj);
	hPtr = Tcl_FindHashEntry(&tree->elementHash, name);
	if (hPtr == NULL)
	{
		Tcl_AppendResult(tree->interp, "element \"", name, "\" doesn't exist",
			NULL);
		return TCL_ERROR;
	}
	(*elemPtr) = (Element *) Tcl_GetHashValue(hPtr);
	return TCL_OK;
}

int TreeElement_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeElement *elemPtr)
{
	return Element_FromObj(tree, obj, (Element **) elemPtr);
}

int TreeElement_IsType(TreeCtrl *tree, TreeElement elem_, CONST char *type)
{
	return strcmp(((Element *) elem_)->typePtr->name, type) == 0;
}

int TreeStyle_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeStyle *stylePtr)
{
	char *name;
	Tcl_HashEntry *hPtr;

	name = Tcl_GetString(obj);
	hPtr = Tcl_FindHashEntry(&tree->styleHash, name);
	if (hPtr == NULL)
	{
		Tcl_AppendResult(tree->interp, "style \"", name, "\" doesn't exist",
			NULL);
		return TCL_ERROR;
	}
	(*stylePtr) = (TreeStyle) Tcl_GetHashValue(hPtr);
	return TCL_OK;
}

static Tcl_Obj *Element_ToObj(Element *elem)
{
	return Tcl_NewStringObj(elem->name, -1);
}

Tcl_Obj *TreeStyle_ToObj(TreeStyle style_)
{
	Style *style = (Style *) style_;

	return Tcl_NewStringObj(style->name, -1);
}

static void Style_Changed(TreeCtrl *tree, Style *masterStyle)
{
	TreeItem item;
	TreeItemColumn column;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	int columnIndex, layout;
	int updateDInfo = FALSE;

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		layout = FALSE;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if ((style != NULL) && (style->master == masterStyle))
			{
				int i;
				for (i = 0; i < style->numElements; i++)
				{
					ElementLink *eLink = &style->elements[i];
					/* This is needed if the -width/-height layout options change */
					eLink->neededWidth = eLink->neededHeight = -1;
				}
				style->neededWidth = style->neededHeight = -1;
				Tree_InvalidateColumnWidth(tree, columnIndex);
				TreeItemColumn_InvalidateSize(tree, column);
				layout = TRUE;
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		if (layout)
		{
			TreeItem_InvalidateHeight(tree, item);
			Tree_FreeItemDInfo(tree, item, NULL);
			updateDInfo = TRUE;
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
	if (updateDInfo)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

static void Style_ChangeElementsAux(TreeCtrl *tree, Style *style, int count, Element **elemList, int *map)
{
	ElementLink *eLink, *eLinks = NULL;
	int i, staticKeep[STATIC_SIZE], *keep = staticKeep;

	STATIC_ALLOC(keep, int, style->numElements);

	if (count > 0)
#ifdef ALLOC_HAX
		eLinks = (ElementLink *) AllocHax_CAlloc(tree->allocData, sizeof(ElementLink), count, 5);
#else
		eLinks = (ElementLink *) ckalloc(sizeof(ElementLink) * count);
#endif

	/* Assume we are discarding all the old ElementLinks */
	for (i = 0; i < style->numElements; i++)
		keep[i] = 0;

	for (i = 0; i < count; i++)
	{
		if (map[i] != -1)
		{
			eLinks[i] = style->elements[map[i]];
			keep[map[i]] = 1;
		}
		else
		{
			eLink = ElementLink_Init(&eLinks[i], elemList[i]);
			eLink->neededWidth = eLink->neededHeight = -1;
		}
	}

	if (style->numElements > 0)
	{
		/* Free unused ElementLinks */
		for (i = 0; i < style->numElements; i++)
		{
			if (!keep[i])
				ElementLink_FreeResources(tree, &style->elements[i]);
		}
#ifdef ALLOC_HAX
		AllocHax_CFree(tree->allocData, (char *) style->elements, sizeof(ElementLink), style->numElements, 5);
#else
		wipefree((char *) style->elements, sizeof(ElementLink) *
			style->numElements);
#endif
	}

	STATIC_FREE(keep, int, style->numElements);

	style->elements = eLinks;
	style->numElements = count;
}

static void Style_ChangeElements(TreeCtrl *tree, Style *masterStyle, int count, Element **elemList, int *map)
{
	TreeItem item;
	TreeItemColumn column;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	int columnIndex, layout;
	int updateDInfo = FALSE;
	int i, j, k;

	/* Update -union lists */
	for (i = 0; i < masterStyle->numElements; i++)
	{
		ElementLink *eLink = &masterStyle->elements[i];
		int staticKeep[STATIC_SIZE], *keep = staticKeep;
		int onionCnt = 0, *onion = NULL;

		if (eLink->onion == NULL)
			continue;

		STATIC_ALLOC(keep, int, eLink->onionCount);

		/* Check every Element in this -union */
		for (j = 0; j < eLink->onionCount; j++)
		{
			ElementLink *eLink2 = &masterStyle->elements[eLink->onion[j]];

			/* Check the new list of Elements */
			keep[j] = -1;
			for (k = 0; k < count; k++)
			{
				/* This new Element is in the -union */
				if (elemList[k] == eLink2->elem)
				{
					keep[j] = k;
					onionCnt++;
					break;
				}
			}
		}

		if (onionCnt > 0)
		{
			if (onionCnt != eLink->onionCount)
				onion = (int *) ckalloc(sizeof(int) * onionCnt);
			else
				onion = eLink->onion;
			k = 0;
			for (j = 0; j < eLink->onionCount; j++)
			{
				if (keep[j] != -1)
					onion[k++] = keep[j];
			}
		}

		STATIC_FREE(keep, int, eLink->onionCount);

		if (onionCnt != eLink->onionCount)
		{
			wipefree((char *) eLink->onion, sizeof(int) * eLink->onionCount);
			eLink->onion = onion;
			eLink->onionCount = onionCnt;
		}
	}

	Style_ChangeElementsAux(tree, masterStyle, count, elemList, map);

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		layout = FALSE;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if ((style != NULL) && (style->master == masterStyle))
			{
				Style_ChangeElementsAux(tree, style, count, elemList, map);
				style->neededWidth = style->neededHeight = -1;
				Tree_InvalidateColumnWidth(tree, columnIndex);
				TreeItemColumn_InvalidateSize(tree, column);
				layout = TRUE;
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		if (layout)
		{
			TreeItem_InvalidateHeight(tree, item);
			Tree_FreeItemDInfo(tree, item, NULL);
			updateDInfo = TRUE;
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
	if (updateDInfo)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

static void Style_ElemChanged(TreeCtrl *tree, Style *masterStyle,
	Element *masterElem, int flagM, int flagT, int csM)
{
	TreeItem item;
	TreeItemColumn column;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	ElementLink *eLink;
	int i, columnIndex;
	ElementArgs args;
	int eMask, iMask;
	int updateDInfo = FALSE;

	args.tree = tree;
	args.change.flagTree = flagT;
	args.change.flagMaster = flagM;
	args.change.flagSelf = 0;

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		iMask = 0;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if ((style != NULL) && (style->master == masterStyle))
			{
				for (i = 0; i < style->numElements; i++)
				{
					eLink = &style->elements[i];
					if (eLink->elem == masterElem)
					{
						if (csM & CS_LAYOUT)
							eLink->neededWidth = eLink->neededHeight = -1;
						break;
					}
					/* Instance element */
					if (eLink->elem->master == masterElem)
					{
						args.elem = eLink->elem;
						eMask = (*masterElem->typePtr->changeProc)(&args);
						if (eMask & CS_LAYOUT)
							eLink->neededWidth = eLink->neededHeight = -1;
						iMask |= eMask;
						break;
					}
				}
				iMask |= csM;
				if (iMask & CS_LAYOUT)
				{
					style->neededWidth = style->neededHeight = -1;
					Tree_InvalidateColumnWidth(tree, columnIndex);
					TreeItemColumn_InvalidateSize(tree, column);
				}
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		if (iMask & CS_LAYOUT)
		{
			TreeItem_InvalidateHeight(tree, item);
			Tree_FreeItemDInfo(tree, item, NULL);
			updateDInfo = TRUE;
		}
		if (iMask & CS_DISPLAY)
			Tree_InvalidateItemDInfo(tree, item, NULL);
		hPtr = Tcl_NextHashEntry(&search);
	}
	if (updateDInfo)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

TreeStyle TreeStyle_GetMaster(TreeCtrl *tree, TreeStyle style_)
{
	return (TreeStyle) ((Style *) style_)->master;
}

static Tcl_Obj *confTextObj = NULL;

Tcl_Obj *TreeStyle_GetText(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	ElementLink *eLink;
	int i;

	if (confTextObj == NULL)
	{
		confTextObj = Tcl_NewStringObj("-text", -1);
		Tcl_IncrRefCount(confTextObj);
	}

	for (i = 0; i < style->numElements; i++)
	{
		eLink = &style->elements[i];
		if (ELEMENT_TYPE_MATCHES(eLink->elem->typePtr, &elemTypeText))
		{
			Tcl_Obj *resultObjPtr;
			resultObjPtr = Tk_GetOptionValue(tree->interp,
				(char *) eLink->elem, eLink->elem->typePtr->optionTable,
				confTextObj, tree->tkwin);
			return resultObjPtr;
		}
	}

	return NULL;
}

void TreeStyle_SetText(TreeCtrl *tree, TreeItem item, TreeItemColumn column,
	TreeStyle style_, Tcl_Obj *textObj)
{
	Style *style = (Style *) style_;
	Style *masterStyle = style->master;
	ElementLink *eLink;
	int i;

	if (masterStyle == NULL)
		panic("TreeStyle_SetText called with master Style");

	if (confTextObj == NULL)
	{
		confTextObj = Tcl_NewStringObj("-text", -1);
		Tcl_IncrRefCount(confTextObj);
	}

	for (i = 0; i < style->numElements; i++)
	{
		eLink = &masterStyle->elements[i];
		if (ELEMENT_TYPE_MATCHES(eLink->elem->typePtr, &elemTypeText))
		{
			Tcl_Obj *objv[2];
			ElementArgs args;

			eLink = Style_CreateElem(tree, item, column, style, eLink->elem, NULL);
			objv[0] = confTextObj;
			objv[1] = textObj;
			args.tree = tree;
			args.elem = eLink->elem;
			args.config.objc = 2;
			args.config.objv = objv;
			args.config.flagSelf = 0;
			(void) (*eLink->elem->typePtr->configProc)(&args);

			args.change.flagSelf = args.config.flagSelf;
			args.change.flagTree = 0;
			args.change.flagMaster = 0;
			(*eLink->elem->typePtr->changeProc)(&args);

			eLink->neededWidth = eLink->neededHeight = -1;
			style->neededWidth = style->neededHeight = -1;
			return;
		}
	}
}

static void Style_Deleted(TreeCtrl *tree, Style *masterStyle)
{
	TreeItem item;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	TreeItemColumn column;
	int columnIndex;

	if (masterStyle->master != NULL)
		panic("Style_Deleted called with instance Style");

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if ((style != NULL) && (style->master == masterStyle))
			{
				Tree_InvalidateColumnWidth(tree, columnIndex);
				TreeItemColumn_ForgetStyle(tree, column);
				TreeItem_InvalidateHeight(tree, item);
				Tree_FreeItemDInfo(tree, item, NULL);
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		hPtr = Tcl_NextHashEntry(&search);
	}

	/* Update -defaultstyle option */
	if (tree->defaultStyle.stylesObj != NULL)
	{
		Tcl_Obj *stylesObj = tree->defaultStyle.stylesObj;
		if (Tcl_IsShared(stylesObj))
		{
			stylesObj = Tcl_DuplicateObj(stylesObj);
			Tcl_DecrRefCount(tree->defaultStyle.stylesObj);
			Tcl_IncrRefCount(stylesObj);
			tree->defaultStyle.stylesObj = stylesObj;
		}
		for (columnIndex = 0; columnIndex < tree->defaultStyle.numStyles; columnIndex++)
		{
			Tcl_Obj *emptyObj;
			if (tree->defaultStyle.styles[columnIndex] != (TreeStyle) masterStyle)
				continue;
			tree->defaultStyle.styles[columnIndex] = NULL;
			emptyObj = Tcl_NewObj();
			Tcl_ListObjReplace(tree->interp, stylesObj, columnIndex, 1, 1, &emptyObj);
		}
	}
}

static void Element_Changed(TreeCtrl *tree, Element *masterElem, int flagM, int flagT, int csM)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Style *masterStyle;
	ElementLink *eLink;
	int i;

	hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
	while (hPtr != NULL)
	{
		masterStyle = (Style *) Tcl_GetHashValue(hPtr);
		for (i = 0; i < masterStyle->numElements; i++)
		{
			eLink = &masterStyle->elements[i];
			if (eLink->elem == masterElem)
			{
				Style_ElemChanged(tree, masterStyle, masterElem, flagM, flagT, csM);
				break;
			}
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
}

static void Element_Deleted(TreeCtrl *tree, Element *masterElem)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Style *masterStyle;
	ElementLink *eLink;
	int i, j;

	hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
	while (hPtr != NULL)
	{
		masterStyle = (Style *) Tcl_GetHashValue(hPtr);
		for (i = 0; i < masterStyle->numElements; i++)
		{
			eLink = &masterStyle->elements[i];
			if (eLink->elem == masterElem)
			{
				Element *staticElemList[STATIC_SIZE], **elemList = staticElemList;
				int staticElemMap[STATIC_SIZE], *elemMap = staticElemMap;

				STATIC_ALLOC(elemList, Element *, masterStyle->numElements);
				STATIC_ALLOC(elemMap, int, masterStyle->numElements);

				for (j = 0; j < masterStyle->numElements; j++)
				{
					if (j == i)
						continue;
					elemList[(j < i) ? j : (j - 1)] =
						masterStyle->elements[j].elem;
					elemMap[(j < i) ? j : (j - 1)] = j;
				}
				Style_ChangeElements(tree, masterStyle,
					masterStyle->numElements - 1, elemList, elemMap);
				STATIC_FREE(elemList, Element *, masterStyle->numElements + 1);
				STATIC_FREE(elemMap, int, masterStyle->numElements + 1);
				break;
			}
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
}

void Tree_RedrawElement(TreeCtrl *tree, TreeItem item, Element *elem)
{
	/* Master element */
	if (elem->master == NULL)
	{
	}

	/* Instance element */
	else
	{
		Tree_InvalidateItemDInfo(tree, item, NULL);
	}
}

typedef struct Iterate
{
	TreeCtrl *tree;
	TreeItem item;
	TreeItemColumn column;
	int columnIndex;
	Style *style;
	ElementType *elemTypePtr;
	ElementLink *eLink;
	Tcl_HashSearch search;
	Tcl_HashEntry *hPtr;
} Iterate;

static int IterateItem(Iterate *iter)
{
	int i;

	while (iter->column != NULL)
	{
		iter->style = (Style *) TreeItemColumn_GetStyle(iter->tree, iter->column);
		if (iter->style != NULL)
		{
			for (i = 0; i < iter->style->numElements; i++)
			{
				iter->eLink = &iter->style->elements[i];
				if (ELEMENT_TYPE_MATCHES(iter->eLink->elem->typePtr, iter->elemTypePtr))
					return 1;
			}
		}
		iter->column = TreeItemColumn_GetNext(iter->tree, iter->column);
		iter->columnIndex++;
	}
	return 0;
}

TreeIterate Tree_ElementIterateBegin(TreeCtrl *tree, ElementType *elemTypePtr)
{
	Iterate *iter;

	iter = (Iterate *) ckalloc(sizeof(Iterate));
	iter->tree = tree;
	iter->elemTypePtr = elemTypePtr;
	iter->hPtr = Tcl_FirstHashEntry(&tree->itemHash, &iter->search);
	while (iter->hPtr != NULL)
	{
		iter->item = (TreeItem) Tcl_GetHashValue(iter->hPtr);
		iter->column = TreeItem_GetFirstColumn(tree, iter->item);
		iter->columnIndex = 0;
		if (IterateItem(iter))
			return (TreeIterate) iter;
		iter->hPtr = Tcl_NextHashEntry(&iter->search);
	}
	ckfree((char *) iter);
	return NULL;
}

TreeIterate Tree_ElementIterateNext(TreeIterate iter_)
{
	Iterate *iter = (Iterate *) iter_;

	iter->column = TreeItemColumn_GetNext(iter->tree, iter->column);
	iter->columnIndex++;
	if (IterateItem(iter))
		return iter_;
	iter->hPtr = Tcl_NextHashEntry(&iter->search);
	while (iter->hPtr != NULL)
	{
		iter->item = (TreeItem) Tcl_GetHashValue(iter->hPtr);
		iter->column = TreeItem_GetFirstColumn(iter->tree, iter->item);
		iter->columnIndex = 0;
		if (IterateItem(iter))
			return iter_;
		iter->hPtr = Tcl_NextHashEntry(&iter->search);
	}
	ckfree((char *) iter);
	return NULL;
}

void Tree_ElementChangedItself(TreeCtrl *tree, TreeItem item,
	TreeItemColumn column, Element *elem, int mask)
{
	if (mask & CS_LAYOUT)
	{
		Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
		int i;
		ElementLink *eLink = NULL;
		TreeItemColumn column2;
		int columnIndex = 0;

		if (style == NULL)
			panic("Tree_ElementChangedItself but style is NULL\n");

		for (i = 0; i < style->numElements; i++)
		{
			eLink = &style->elements[i];
			if (eLink->elem == elem)
				break;
		}

		if (eLink == NULL)
			panic("Tree_ElementChangedItself but eLink is NULL\n");

		column2 = TreeItem_GetFirstColumn(tree, item);
		while (column2 != NULL)
		{
			if (column2 == column)
				break;
			columnIndex++;
			column2 = TreeItemColumn_GetNext(tree, column2);
		}
		if (column2 == NULL)
			panic("Tree_ElementChangedItself but column2 is NULL\n");

		eLink->neededWidth = eLink->neededHeight = -1;
		style->neededWidth = style->neededHeight = -1;

		Tree_InvalidateColumnWidth(tree, columnIndex);
		TreeItemColumn_InvalidateSize(tree, column);
		TreeItem_InvalidateHeight(tree, item);
		Tree_FreeItemDInfo(tree, item, NULL);
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	}
	if (mask & CS_DISPLAY)
		Tree_InvalidateItemDInfo(tree, item, NULL);
}

void Tree_ElementIterateChanged(TreeIterate iter_, int mask)
{
	Iterate *iter = (Iterate *) iter_;

	if (mask & CS_LAYOUT)
	{
		iter->eLink->neededWidth = iter->eLink->neededHeight = -1;
		iter->style->neededWidth = iter->style->neededHeight = -1;
		Tree_InvalidateColumnWidth(iter->tree, iter->columnIndex);
		TreeItemColumn_InvalidateSize(iter->tree, iter->column);
		TreeItem_InvalidateHeight(iter->tree, iter->item);
		Tree_FreeItemDInfo(iter->tree, iter->item, NULL);
		Tree_DInfoChanged(iter->tree, DINFO_REDO_RANGES);
	}
	if (mask & CS_DISPLAY)
		Tree_InvalidateItemDInfo(iter->tree, iter->item, NULL);
}

Element *Tree_ElementIterateGet(TreeIterate iter_)
{
	Iterate *iter = (Iterate *) iter_;

	return iter->eLink->elem;
}

void TreeStyle_TreeChanged(TreeCtrl *tree, int flagT)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Element *masterElem;
	ElementArgs args;
	int eMask;

	if (flagT == 0)
		return;

	args.tree = tree;
	args.change.flagTree = flagT;
	args.change.flagMaster = 0;
	args.change.flagSelf = 0;

	hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
	while (hPtr != NULL)
	{
		masterElem = (Element *) Tcl_GetHashValue(hPtr);
		args.elem = masterElem;
		eMask = (*masterElem->typePtr->changeProc)(&args);
		Element_Changed(tree, masterElem, 0, flagT, eMask);
		hPtr = Tcl_NextHashEntry(&search);
	}
}

int TreeStyle_ElementCget(TreeCtrl *tree, TreeItem item,
	TreeItemColumn column, TreeStyle style_, Tcl_Obj *elemObj, Tcl_Obj *obj)
{
	Style *style = (Style *) style_;
	Tcl_Obj *resultObjPtr = NULL;
	Element *elem;
	ElementLink *eLink;

	if (Element_FromObj(tree, elemObj, &elem) != TCL_OK)
		return TCL_ERROR;

	eLink = Style_FindElem(tree, style, elem, NULL);
	if ((eLink != NULL) && (eLink->elem == elem) && (style->master != NULL))
	{
		int index = TreeItemColumn_Index(tree, item, column);
		TreeColumn treeColumn = Tree_FindColumn(tree, index);

		FormatResult(tree->interp,
			"element %s is not configured in item %s%d column %s%d",
			elem->name, tree->itemPrefix, TreeItem_GetID(tree, item),
			tree->columnPrefix, TreeColumn_GetID(treeColumn));
		return TCL_ERROR;
	}
	if (eLink == NULL)
	{
		FormatResult(tree->interp, "style %s does not use element %s",
			style->name, elem->name);
		return TCL_ERROR;
	}

	resultObjPtr = Tk_GetOptionValue(tree->interp, (char *) eLink->elem,
		eLink->elem->typePtr->optionTable, obj, tree->tkwin);
	if (resultObjPtr == NULL)
		return TCL_ERROR;
	Tcl_SetObjResult(tree->interp, resultObjPtr);
	return TCL_OK;
}

int TreeStyle_ElementConfigure(TreeCtrl *tree, TreeItem item,
	TreeItemColumn column, TreeStyle style_, Tcl_Obj *elemObj,
	int objc, Tcl_Obj **objv, int *eMask)
{
	Style *style = (Style *) style_;
	Element *elem;
	ElementLink *eLink;
	ElementArgs args;

	(*eMask) = 0;

	if (Element_FromObj(tree, elemObj, &elem) != TCL_OK)
		return TCL_ERROR;

	if (objc <= 1)
	{
		Tcl_Obj *resultObjPtr;

		eLink = Style_FindElem(tree, style, elem, NULL);
		if ((eLink != NULL) && (eLink->elem == elem) && (style->master != NULL))
		{
			int index = TreeItemColumn_Index(tree, item, column);
			TreeColumn treeColumn = Tree_FindColumn(tree, index);

			FormatResult(tree->interp,
				"element %s is not configured in item %s%d column %s%d",
				elem->name, tree->itemPrefix, TreeItem_GetID(tree, item),
				tree->columnPrefix, TreeColumn_GetID(treeColumn));
			return TCL_ERROR;
		}
		if (eLink == NULL)
		{
			FormatResult(tree->interp, "style %s does not use element %s",
				style->name, elem->name);
			return TCL_ERROR;
		}

		resultObjPtr = Tk_GetOptionInfo(tree->interp, (char *) eLink->elem,
			eLink->elem->typePtr->optionTable,
			(objc == 0) ? (Tcl_Obj *) NULL : objv[0],
			tree->tkwin);
		if (resultObjPtr == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(tree->interp, resultObjPtr);
	}
	else
	{
		int isNew;

		eLink = Style_CreateElem(tree, item, column, style, elem, &isNew);
		if (eLink == NULL)
		{
			FormatResult(tree->interp, "style %s does not use element %s",
				style->name, elem->name);
			return TCL_ERROR;
		}

		/* Do this before configProc(). If eLink was just allocated and an
		 * error occurs in configProc() it won't be done */
		(*eMask) = 0;
		if (isNew)
		{
			eLink->neededWidth = eLink->neededHeight = -1;
			style->neededWidth = style->neededHeight = -1;
			(*eMask) = CS_DISPLAY | CS_LAYOUT;
		}

		args.tree = tree;
		args.elem = eLink->elem;
		args.config.objc = objc;
		args.config.objv = objv;
		args.config.flagSelf = 0;
		if ((*args.elem->typePtr->configProc)(&args) != TCL_OK)
			return TCL_ERROR;

		args.change.flagSelf = args.config.flagSelf;
		args.change.flagTree = 0;
		args.change.flagMaster = 0;
		(*eMask) |= (*elem->typePtr->changeProc)(&args);

		if (!isNew && ((*eMask) & CS_LAYOUT))
		{
			eLink->neededWidth = eLink->neededHeight = -1;
			style->neededWidth = style->neededHeight = -1;
		}
	}
	return TCL_OK;
}

int TreeStyle_ElementActual(TreeCtrl *tree, TreeStyle style_, int state, Tcl_Obj *elemObj, Tcl_Obj *obj)
{
	Style *style = (Style *) style_;
	Element *masterElem;
	ElementLink *eLink;
	ElementArgs args;

	if (Element_FromObj(tree, elemObj, &masterElem) != TCL_OK)
		return TCL_ERROR;

	eLink = Style_FindElem(tree, style, masterElem, NULL);
	if (eLink == NULL)
	{
		FormatResult(tree->interp, "style %s does not use element %s",
			style->name, masterElem->name);
		return TCL_ERROR;
	}

	args.tree = tree;
	args.elem = eLink->elem;
	args.state = state;
	args.actual.obj = obj;
	return (*masterElem->typePtr->actualProc)(&args);
}

int TreeElementCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandNames[] = {
		"cget", "configure", "create", "delete", "names", "type",
		(char *) NULL
	};
	enum {
		COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_CREATE, COMMAND_DELETE,
		COMMAND_NAMES, COMMAND_TYPE
	};
	int index;

	if (objc < 3)
	{
		Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg...?");
		return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
		&index) != TCL_OK)
	{
		return TCL_ERROR;
	}

	switch (index)
	{
		case COMMAND_CGET:
		{
			Tcl_Obj *resultObjPtr = NULL;
			Element *elem;

			if (objc != 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name option");
				return TCL_ERROR;
			}
			if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
				return TCL_ERROR;
			resultObjPtr = Tk_GetOptionValue(interp, (char *) elem,
				elem->typePtr->optionTable, objv[4], tree->tkwin);
			if (resultObjPtr == NULL)
				return TCL_ERROR;
			Tcl_SetObjResult(interp, resultObjPtr);
			break;
		}

		case COMMAND_CONFIGURE:
		{
			Tcl_Obj *resultObjPtr = NULL;
			Element *elem;
			int eMask;

			if (objc < 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name ?option? ?value option value...?");
				return TCL_ERROR;
			}
			if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
				return TCL_ERROR;
			if (objc <= 5)
			{
				resultObjPtr = Tk_GetOptionInfo(interp, (char *) elem,
					elem->typePtr->optionTable,
					(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
					tree->tkwin);
				if (resultObjPtr == NULL)
					return TCL_ERROR;
				Tcl_SetObjResult(interp, resultObjPtr);
			}
			else
			{
				ElementArgs args;

				args.tree = tree;
				args.elem = elem;
				args.config.objc = objc - 4;
				args.config.objv = objv + 4;
				args.config.flagSelf = 0;
				if ((*elem->typePtr->configProc)(&args) != TCL_OK)
					return TCL_ERROR;

				args.change.flagSelf = args.config.flagSelf;
				args.change.flagTree = 0;
				args.change.flagMaster = 0;
				eMask = (*elem->typePtr->changeProc)(&args);

				Element_Changed(tree, elem, args.change.flagSelf, 0, eMask);
			}
			break;
		}

		case COMMAND_CREATE:
		{
			char *name;
			int length;
			int isNew;
			Element *elem;
			ElementType *typePtr;
			Tcl_HashEntry *hPtr;

			if (objc < 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name type ?option value...?");
				return TCL_ERROR;
			}
			name = Tcl_GetStringFromObj(objv[3], &length);
			if (!length)
				return TCL_ERROR;
			hPtr = Tcl_FindHashEntry(&tree->elementHash, name);
			if (hPtr != NULL)
			{
				FormatResult(interp, "element \"%s\" already exists", name);
				return TCL_ERROR;
			}
			if (TreeElement_TypeFromObj(tree, objv[4], &typePtr) != TCL_OK)
				return TCL_ERROR;
			elem = Element_CreateAndConfig(tree, NULL, NULL, NULL, typePtr, name, objc - 5, objv + 5);
			if (elem == NULL)
				return TCL_ERROR;
			hPtr = Tcl_CreateHashEntry(&tree->elementHash, name, &isNew);
			Tcl_SetHashValue(hPtr, elem);
			Tcl_SetObjResult(interp, Element_ToObj(elem));
			break;
		}

		case COMMAND_DELETE:
		{
			Element *elem;
			int i;

			if (objc < 3)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "?name ...?");
				return TCL_ERROR;
			}
			for (i = 3; i < objc; i++)
			{
				if (Element_FromObj(tree, objv[i], &elem) != TCL_OK)
					return TCL_ERROR;
				Element_Deleted(tree, elem);
				Element_FreeResources(tree, elem);
			}
			break;
		}

		case COMMAND_NAMES:
		{
			Tcl_Obj *listObj;
			Tcl_HashSearch search;
			Tcl_HashEntry *hPtr;
			Element *elem;

			listObj = Tcl_NewListObj(0, NULL);
			hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
			while (hPtr != NULL)
			{
				elem = (Element *) Tcl_GetHashValue(hPtr);
				Tcl_ListObjAppendElement(interp, listObj, Element_ToObj(elem));
				hPtr = Tcl_NextHashEntry(&search);
			}
			Tcl_SetObjResult(interp, listObj);
			break;
		}

		case COMMAND_TYPE:
		{
			Element *elem;

			if (objc != 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name");
				return TCL_ERROR;
			}
			if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
				return TCL_ERROR;
			Tcl_SetResult(interp, elem->typePtr->name, TCL_STATIC); /* Tk_Uid */
			break;
		}
	}
	return TCL_OK;
}

static Style *Style_CreateAndConfig(TreeCtrl *tree, char *name, int objc, Tcl_Obj *CONST objv[])
{
	Style *style;

#ifdef ALLOC_HAX
	style = (Style *) AllocHax_Alloc(tree->allocData, sizeof(Style));
#else
	style = (Style *) ckalloc(sizeof(Style));
#endif
	memset(style, '\0', sizeof(Style));
	style->optionTable = Tk_CreateOptionTable(tree->interp, styleOptionSpecs); 
	style->name = Tk_GetUid(name);

	if (Tk_InitOptions(tree->interp, (char *) style,
		style->optionTable, tree->tkwin) != TCL_OK)
	{
#ifdef ALLOC_HAX
		AllocHax_Free(tree->allocData, (char *) style, sizeof(Style));
#else
		WFREE(style, Style);
#endif
		return NULL;
	}

	if (Tk_SetOptions(tree->interp, (char *) style,
		style->optionTable, objc, objv, tree->tkwin,
		NULL, NULL) != TCL_OK)
	{
		Tk_FreeConfigOptions((char *) style, style->optionTable, tree->tkwin);
#ifdef ALLOC_HAX
		AllocHax_Free(tree->allocData, (char *) style, sizeof(Style));
#else
		WFREE(style, Style);
#endif
		return NULL;
	}

	return style;
}

void TreeStyle_ListElements(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	Tcl_Obj *listObj;
	ElementLink *eLink;
	int i;

	if (style->numElements <= 0)
		return;

	listObj = Tcl_NewListObj(0, NULL);
	for (i = 0; i < style->numElements; i++)
	{
		eLink = &style->elements[i];
		if ((style->master != NULL) && (eLink->elem->master == NULL))
			continue;
		Tcl_ListObjAppendElement(tree->interp, listObj,
			Element_ToObj(eLink->elem));
	}
	Tcl_SetObjResult(tree->interp, listObj);
}

enum {
	OPTION_DETACH, OPTION_EXPAND, OPTION_HEIGHT, OPTION_iEXPAND,
	OPTION_INDENT, OPTION_iPADX, OPTION_iPADY, OPTION_MAXHEIGHT,
	OPTION_MAXWIDTH, OPTION_MINHEIGHT, OPTION_MINWIDTH, OPTION_PADX,
	OPTION_PADY, OPTION_SQUEEZE, OPTION_STICKY, OPTION_UNION,
	OPTION_WIDTH
};

static Tcl_Obj *LayoutOptionToObj(TreeCtrl *tree, Style *style, ElementLink *eLink, int option)
{
	Tcl_Interp *interp = tree->interp;

	switch (option)
	{
		case OPTION_PADX:
			return TreeCtrl_NewPadAmountObj(eLink->ePadX);
		case OPTION_PADY:
			return TreeCtrl_NewPadAmountObj(eLink->ePadY);
		case OPTION_iPADX:
			return TreeCtrl_NewPadAmountObj(eLink->iPadX);
		case OPTION_iPADY:
			return TreeCtrl_NewPadAmountObj(eLink->iPadY);
		case OPTION_DETACH:
			return Tcl_NewStringObj((eLink->flags & ELF_DETACH) ? "yes" : "no", -1);
		case OPTION_EXPAND:
		{
			char flags[4];
			int n = 0;

			if (eLink->flags & ELF_eEXPAND_W) flags[n++] = 'w';
			if (eLink->flags & ELF_eEXPAND_N) flags[n++] = 'n';
			if (eLink->flags & ELF_eEXPAND_E) flags[n++] = 'e';
			if (eLink->flags & ELF_eEXPAND_S) flags[n++] = 's';
			if (n)
				return Tcl_NewStringObj(flags, n);
			break;
		}
		case OPTION_iEXPAND:
		{
			char flags[6];
			int n = 0;

			if (eLink->flags & ELF_iEXPAND_X) flags[n++] = 'x';
			if (eLink->flags & ELF_iEXPAND_Y) flags[n++] = 'y';
			if (eLink->flags & ELF_iEXPAND_W) flags[n++] = 'w';
			if (eLink->flags & ELF_iEXPAND_N) flags[n++] = 'n';
			if (eLink->flags & ELF_iEXPAND_E) flags[n++] = 'e';
			if (eLink->flags & ELF_iEXPAND_S) flags[n++] = 's';
			if (n)
				return Tcl_NewStringObj(flags, n);
			break;
		}
		case OPTION_INDENT:
			return Tcl_NewStringObj((eLink->flags & ELF_INDENT) ? "yes" : "no", -1);
		case OPTION_SQUEEZE:
		{
			char flags[2];
			int n = 0;

			if (eLink->flags & ELF_SQUEEZE_X) flags[n++] = 'x';
			if (eLink->flags & ELF_SQUEEZE_Y) flags[n++] = 'y';
			if (n)
				return Tcl_NewStringObj(flags, n);
			break;
		}
		case OPTION_UNION:
		{
			int i;
			Tcl_Obj *objPtr;

			if (eLink->onionCount == 0)
				break;
			objPtr = Tcl_NewListObj(0, NULL);
			for (i = 0; i < eLink->onionCount; i++)
				Tcl_ListObjAppendElement(interp, objPtr,
					Element_ToObj(style->elements[eLink->onion[i]].elem));
			return objPtr;
		}
		case OPTION_MAXHEIGHT:
		{
			if (eLink->maxHeight >= 0)
				return Tcl_NewIntObj(eLink->maxHeight);
			break;
		}
		case OPTION_MINHEIGHT:
		{
			if (eLink->minHeight >= 0)
				return Tcl_NewIntObj(eLink->minHeight);
			break;
		}
		case OPTION_HEIGHT:
		{
			if (eLink->fixedHeight >= 0)
				return Tcl_NewIntObj(eLink->fixedHeight);
			break;
		}
		case OPTION_MAXWIDTH:
		{
			if (eLink->maxWidth >= 0)
				return Tcl_NewIntObj(eLink->maxWidth);
			break;
		}
		case OPTION_MINWIDTH:
		{
			if (eLink->minWidth >= 0)
				return Tcl_NewIntObj(eLink->minWidth);
			break;
		}
		case OPTION_WIDTH:
		{
			if (eLink->fixedWidth >= 0)
				return Tcl_NewIntObj(eLink->fixedWidth);
			break;
		}
		case OPTION_STICKY:
		{
			char flags[4];
			int n = 0;

			if (eLink->flags & ELF_STICKY_W) flags[n++] = 'w';
			if (eLink->flags & ELF_STICKY_N) flags[n++] = 'n';
			if (eLink->flags & ELF_STICKY_E) flags[n++] = 'e';
			if (eLink->flags & ELF_STICKY_S) flags[n++] = 's';
			if (n)
				return Tcl_NewStringObj(flags, n);
			break;
		}
	}
	return NULL;
}

static int StyleLayoutCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	Style *style;
	Element *elem;
	ElementLink saved, *eLink;
	int i, index;
	static CONST char *optionNames[] = {
		"-detach", "-expand", "-height", "-iexpand",
		"-indent", "-ipadx", "-ipady", "-maxheight", "-maxwidth", "-minheight",
		"-minwidth", "-padx", "-pady", "-squeeze", "-sticky", "-union",
		"-width",
		(char *) NULL
	};
	if (objc < 5)
	{
		Tcl_WrongNumArgs(interp, 3, objv, "name element ?option? ?value? ?option value ...?");
		return TCL_ERROR;
	}

	if (TreeStyle_FromObj(tree, objv[3], (TreeStyle *) &style) != TCL_OK)
		return TCL_ERROR;

	if (Element_FromObj(tree, objv[4], &elem) != TCL_OK)
		return TCL_ERROR;

	eLink = Style_FindElem(tree, style, elem, NULL);
	if (eLink == NULL)
	{
		FormatResult(interp, "style %s does not use element %s",
			style->name, elem->name);
		return TCL_ERROR;
	}

	/* T style layout S E */
	if (objc == 5)
	{
		Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
		Tcl_Obj *objPtr;

		for (i = 0; optionNames[i] != NULL; i++)
		{
			Tcl_ListObjAppendElement(interp, listObj,
				Tcl_NewStringObj(optionNames[i], -1));
			objPtr = LayoutOptionToObj(tree, style, eLink, i);
			Tcl_ListObjAppendElement(interp, listObj,
				objPtr ? objPtr : Tcl_NewObj());
		}
		Tcl_SetObjResult(interp, listObj);
		return TCL_OK;
	}

	/* T style layout S E option */
	if (objc == 6)
	{
		Tcl_Obj *objPtr;

		if (Tcl_GetIndexFromObj(interp, objv[5], optionNames, "option",
			0, &index) != TCL_OK)
			return TCL_ERROR;
		objPtr = LayoutOptionToObj(tree, style, eLink, index);
		if (objPtr != NULL)
			Tcl_SetObjResult(interp, objPtr);
		return TCL_OK;
	}

	saved = *eLink;

	for (i = 5; i < objc; i += 2)
	{
		if (i + 2 > objc)
		{
			FormatResult(interp, "value for \"%s\" missing",
				Tcl_GetString(objv[i]));
			goto badConfig;
		}
		if (Tcl_GetIndexFromObj(interp, objv[i], optionNames, "option",
			0, &index) != TCL_OK)
		{
			goto badConfig;
		}
		switch (index)
		{
			case OPTION_PADX:
			{
				if (TreeCtrl_GetPadAmountFromObj(interp,
					tree->tkwin, objv[i + 1],
					&eLink->ePadX[PAD_TOP_LEFT],
					&eLink->ePadX[PAD_BOTTOM_RIGHT]) != TCL_OK)
					goto badConfig;
				break;
			}
			case OPTION_PADY:
			{
				if (TreeCtrl_GetPadAmountFromObj(interp,
					tree->tkwin, objv[i + 1],
					&eLink->ePadY[PAD_TOP_LEFT],
					&eLink->ePadY[PAD_BOTTOM_RIGHT]) != TCL_OK)
					goto badConfig;
				break;
			}
			case OPTION_iPADX:
			{
				if (TreeCtrl_GetPadAmountFromObj(interp,
					tree->tkwin, objv[i + 1],
					&eLink->iPadX[PAD_TOP_LEFT],
					&eLink->iPadX[PAD_BOTTOM_RIGHT]) != TCL_OK)
					goto badConfig;
				break;
			}
			case OPTION_iPADY:
			{
				if (TreeCtrl_GetPadAmountFromObj(interp,
					tree->tkwin, objv[i + 1],
					&eLink->iPadY[PAD_TOP_LEFT],
					&eLink->iPadY[PAD_BOTTOM_RIGHT]) != TCL_OK)
					goto badConfig;
				break;
			}
			case OPTION_DETACH:
			{
				int detach;
				if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &detach) != TCL_OK)
					goto badConfig;
				if (detach)
					eLink->flags |= ELF_DETACH;
				else
					eLink->flags &= ~ELF_DETACH;
				break;
			}
			case OPTION_EXPAND:
			{
				char *expand;
				int len, k;
				expand = Tcl_GetStringFromObj(objv[i + 1], &len);
				eLink->flags &= ~ELF_eEXPAND;
				for (k = 0; k < len; k++)
				{
					switch (expand[k])
					{
						case 'w': case 'W': eLink->flags |= ELF_eEXPAND_W; break;
						case 'n': case 'N': eLink->flags |= ELF_eEXPAND_N; break;
						case 'e': case 'E': eLink->flags |= ELF_eEXPAND_E; break;
						case 's': case 'S': eLink->flags |= ELF_eEXPAND_S; break;
						default:
						{
							Tcl_ResetResult(tree->interp);
							Tcl_AppendResult(tree->interp, "bad expand value \"",
								expand, "\": must be a string ",
								"containing zero or more of n, e, s, and w",
								(char *) NULL);
							goto badConfig;
						}
					}
				}
				break;
			}
			case OPTION_iEXPAND:
			{
				char *expand;
				int len, k;
				expand = Tcl_GetStringFromObj(objv[i + 1], &len);
				eLink->flags &= ~(ELF_iEXPAND | ELF_iEXPAND_X | ELF_iEXPAND_Y);
				for (k = 0; k < len; k++)
				{
					switch (expand[k])
					{
						case 'x': case 'X': eLink->flags |= ELF_iEXPAND_X; break;
						case 'y': case 'Y': eLink->flags |= ELF_iEXPAND_Y; break;
						case 'w': case 'W': eLink->flags |= ELF_iEXPAND_W; break;
						case 'n': case 'N': eLink->flags |= ELF_iEXPAND_N; break;
						case 'e': case 'E': eLink->flags |= ELF_iEXPAND_E; break;
						case 's': case 'S': eLink->flags |= ELF_iEXPAND_S; break;
						default:
						{
							Tcl_ResetResult(tree->interp);
							Tcl_AppendResult(tree->interp, "bad iexpand value \"",
								expand, "\": must be a string ",
								"containing zero or more of x, y, n, e, s, and w",
								(char *) NULL);
							goto badConfig;
						}
					}
				}
				break;
			}
			case OPTION_INDENT:
			{
				int indent;
				if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &indent) != TCL_OK)
					goto badConfig;
				if (indent)
					eLink->flags |= ELF_INDENT;
				else
					eLink->flags &= ~ELF_INDENT;
				break;
			}
			case OPTION_SQUEEZE:
			{
				char *string;
				int len, k;
				string = Tcl_GetStringFromObj(objv[i + 1], &len);
				eLink->flags &= ~(ELF_SQUEEZE_X | ELF_SQUEEZE_Y);
				for (k = 0; k < len; k++)
				{
					switch (string[k])
					{
						case 'x': case 'X': eLink->flags |= ELF_SQUEEZE_X; break;
						case 'y': case 'Y': eLink->flags |= ELF_SQUEEZE_Y; break;
						default:
						{
							Tcl_ResetResult(tree->interp);
							Tcl_AppendResult(tree->interp, "bad squeeze value \"",
								string, "\": must be a string ",
								"containing zero or more of x and y",
								(char *) NULL);
							goto badConfig;
						}
					}
				}
				break;
			}
			case OPTION_UNION:
			{
				int objc1;
				Tcl_Obj **objv1;
				int j, k, n, *onion, count = 0;

				if (Tcl_ListObjGetElements(interp, objv[i + 1],
					&objc1, &objv1) != TCL_OK)
					goto badConfig;
				if (objc1 == 0)
				{
					if (eLink->onion != NULL)
					{
						if (eLink->onion != saved.onion)
							wipefree((char *) eLink->onion,
								sizeof(int) * eLink->onionCount);
						eLink->onionCount = 0;
						eLink->onion = NULL;
					}
					break;
				}
				onion = (int *) ckalloc(sizeof(int) * objc1);
				for (j = 0; j < objc1; j++)
				{
					Element *elem2;
					ElementLink *eLink2;

					if (Element_FromObj(tree, objv1[j], &elem2) != TCL_OK)
					{
						ckfree((char *) onion);
						goto badConfig;
					}

					eLink2 = Style_FindElem(tree, style, elem2, &n);
					if (eLink2 == NULL)
					{
						ckfree((char *) onion);
						FormatResult(interp,
							"style %s does not use element %s",
							style->name, elem2->name);
						goto badConfig;
					}
					if (eLink == eLink2)
					{
						ckfree((char *) onion);
						FormatResult(interp,
							"element %s can't form union with itself",
							elem2->name);
						goto badConfig;
					}
					/* Silently ignore duplicates */
					for (k = 0; k < count; k++)
					{
						if (onion[k] == n)
							break;
					}
					if (k < count)
						continue;
					onion[count++] = n;
				}
				if ((eLink->onion != NULL) && (eLink->onion != saved.onion))
					wipefree((char *) eLink->onion,
						sizeof(int) * eLink->onionCount);
				if (count == objc1)
					eLink->onion = onion;
				else
				{
					eLink->onion = (int *) ckalloc(sizeof(int) * count);
					for (k = 0; k < count; k++)
						eLink->onion[k] = onion[k];
					ckfree((char *) onion);
				}
				eLink->onionCount = count;
				break;
			}
			case OPTION_MAXHEIGHT:
			{
				int height;
				if (ObjectIsEmpty(objv[i + 1]))
				{
					eLink->maxHeight = -1;
					break;
				}
				if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
					&height) != TCL_OK) || (height < 0))
				{
					FormatResult(interp, "bad screen distance \"%s\"",
						Tcl_GetString(objv[i + 1]));
					goto badConfig;
				}
				eLink->maxHeight = height;
				break;
			}
			case OPTION_MINHEIGHT:
			{
				int height;
				if (ObjectIsEmpty(objv[i + 1]))
				{
					eLink->minHeight = -1;
					break;
				}
				if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
					&height) != TCL_OK) || (height < 0))
				{
					FormatResult(interp, "bad screen distance \"%s\"",
						Tcl_GetString(objv[i + 1]));
					goto badConfig;
				}
				eLink->minHeight = height;
				break;
			}
			case OPTION_HEIGHT:
			{
				int height;
				if (ObjectIsEmpty(objv[i + 1]))
				{
					eLink->fixedHeight = -1;
					break;
				}
				if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
					&height) != TCL_OK) || (height < 0))
				{
					FormatResult(interp, "bad screen distance \"%s\"",
						Tcl_GetString(objv[i + 1]));
					goto badConfig;
				}
				eLink->fixedHeight = height;
				break;
			}
			case OPTION_MAXWIDTH:
			{
				int width;
				if (ObjectIsEmpty(objv[i + 1]))
				{
					eLink->maxWidth = -1;
					break;
				}
				if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
					&width) != TCL_OK) || (width < 0))
				{
					FormatResult(interp, "bad screen distance \"%s\"",
						Tcl_GetString(objv[i + 1]));
					goto badConfig;
				}
				eLink->maxWidth = width;
				break;
			}
			case OPTION_MINWIDTH:
			{
				int width;
				if (ObjectIsEmpty(objv[i + 1]))
				{
					eLink->minWidth = -1;
					break;
				}
				if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
					&width) != TCL_OK) || (width < 0))
				{
					FormatResult(interp, "bad screen distance \"%s\"",
						Tcl_GetString(objv[i + 1]));
					goto badConfig;
				}
				eLink->minWidth = width;
				break;
			}
			case OPTION_WIDTH:
			{
				int width;
				if (ObjectIsEmpty(objv[i + 1]))
				{
					eLink->fixedWidth = -1;
					break;
				}
				if ((Tk_GetPixelsFromObj(interp, tree->tkwin, objv[i + 1],
					&width) != TCL_OK) || (width < 0))
				{
					FormatResult(interp, "bad screen distance \"%s\"",
						Tcl_GetString(objv[i + 1]));
					goto badConfig;
				}
				eLink->fixedWidth = width;
				break;
			}
			case OPTION_STICKY:
			{
				char *sticky;
				int len, k;
				sticky = Tcl_GetStringFromObj(objv[i + 1], &len);
				eLink->flags &= ~ELF_STICKY;
				for (k = 0; k < len; k++)
				{
					switch (sticky[k])
					{
						case 'w': case 'W': eLink->flags |= ELF_STICKY_W; break;
						case 'n': case 'N': eLink->flags |= ELF_STICKY_N; break;
						case 'e': case 'E': eLink->flags |= ELF_STICKY_E; break;
						case 's': case 'S': eLink->flags |= ELF_STICKY_S; break;
						default:
						{
							Tcl_ResetResult(tree->interp);
							Tcl_AppendResult(tree->interp, "bad sticky value \"",
								sticky, "\": must be a string ",
								"containing zero or more of n, e, s, and w",
								(char *) NULL);
							goto badConfig;
						}
					}
				}
				break;
			}
		}
	}
	if (saved.onion && (eLink->onion != saved.onion))
		wipefree((char *) saved.onion, sizeof(int) * saved.onionCount);
	Style_Changed(tree, style);
	return TCL_OK;

badConfig:
	if (eLink->onion && (eLink->onion != saved.onion))
		wipefree((char *) eLink->onion, sizeof(int) * eLink->onionCount);
	*eLink = saved;
	return TCL_ERROR;
}

int TreeStyleCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandNames[] = {
		"cget", "configure", "create", "delete", "elements", "layout",
		"names", (char *) NULL };
	enum {
		COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_CREATE, COMMAND_DELETE,
		COMMAND_ELEMENTS, COMMAND_LAYOUT, COMMAND_NAMES };
	int index;

	if (objc < 3)
	{
		Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg...?");
		return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
		&index) != TCL_OK)
	{
		return TCL_ERROR;
	}

	switch (index)
	{
		case COMMAND_CGET:
		{
			Tcl_Obj *resultObjPtr;
			Style *style;

			if (objc != 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name option");
				return TCL_ERROR;
			}
			if (TreeStyle_FromObj(tree, objv[3], (TreeStyle *) &style) != TCL_OK)
				return TCL_ERROR;
			resultObjPtr = Tk_GetOptionValue(interp, (char *) style,
				style->optionTable, objv[4], tree->tkwin);
			if (resultObjPtr == NULL)
				return TCL_ERROR;
			Tcl_SetObjResult(interp, resultObjPtr);
			break;
		}

		case COMMAND_CONFIGURE:
		{
			Tcl_Obj *resultObjPtr = NULL;
			Style *style;

			if (objc < 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name ?option? ?value option value...?");
				return TCL_ERROR;
			}
			if (TreeStyle_FromObj(tree, objv[3], (TreeStyle *) &style) != TCL_OK)
				return TCL_ERROR;
			if (objc <= 5)
			{
				resultObjPtr = Tk_GetOptionInfo(interp, (char *) style,
					style->optionTable,
					(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
					tree->tkwin);
				if (resultObjPtr == NULL)
					return TCL_ERROR;
				Tcl_SetObjResult(interp, resultObjPtr);
			}
			else
			{
				if (Tk_SetOptions(tree->interp, (char *) style,
					style->optionTable, objc - 4, objv + 4, tree->tkwin,
					NULL, NULL) != TCL_OK)
					return TCL_ERROR;
				Style_Changed(tree, style);
			}
			break;
		}

		case COMMAND_CREATE:
		{
			char *name;
			int len;
			Tcl_HashEntry *hPtr;
			int isNew;
			Style *style;

			if (objc < 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name ?option value...?");
				return TCL_ERROR;
			}
			name = Tcl_GetStringFromObj(objv[3], &len);
			if (!len)
			{
				FormatResult(interp, "invalid style name \"\"");
				return TCL_ERROR;
			}
			hPtr = Tcl_FindHashEntry(&tree->styleHash, name);
			if (hPtr != NULL)
			{
				FormatResult(interp, "style \"%s\" already exists", name);
				return TCL_ERROR;
			}
			style = Style_CreateAndConfig(tree, name, objc - 4, objv + 4);
			if (style == NULL)
				return TCL_ERROR;
			hPtr = Tcl_CreateHashEntry(&tree->styleHash, name, &isNew);
			Tcl_SetHashValue(hPtr, style);
			Tcl_SetObjResult(interp, TreeStyle_ToObj((TreeStyle) style));
			break;
		}

		case COMMAND_DELETE:
		{
			Style *style;
			int i;

			if (objc < 3)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "?name ...?");
				return TCL_ERROR;
			}
			for (i = 3; i < objc; i++)
			{
				if (TreeStyle_FromObj(tree, objv[i], (TreeStyle *) &style) != TCL_OK)
					return TCL_ERROR;
				Style_Deleted(tree, style);
				TreeStyle_FreeResources(tree, (TreeStyle) style);
			}
			break;
		}

		/* T style elements S ?{E ...}? */
		case COMMAND_ELEMENTS:
		{
			Style *style;
			Element *elem, **elemList = NULL;
			int i, j, count = 0;
			int staticMap[STATIC_SIZE], *map = staticMap;
			int listObjc;
			Tcl_Obj **listObjv;

			if (objc < 4 || objc > 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name ?elementList?");
				return TCL_ERROR;
			}
			if (TreeStyle_FromObj(tree, objv[3], (TreeStyle *) &style) != TCL_OK)
				return TCL_ERROR;
			if (objc == 5)
			{
				if (Tcl_ListObjGetElements(interp, objv[4], &listObjc, &listObjv) != TCL_OK)
					return TCL_ERROR;
				if (listObjc > 0)
					elemList = (Element **) ckalloc(sizeof(Element *) * listObjc);
				for (i = 0; i < listObjc; i++)
				{
					if (Element_FromObj(tree, listObjv[i], &elem) != TCL_OK)
					{
						ckfree((char *) elemList);
						return TCL_ERROR;
					}

					/* Ignore duplicate elements */
					for (j = 0; j < count; j++)
					{
						if (elemList[j] == elem)
							break;
					}
					if (j < count)
						continue;

					elemList[count++] = elem;
				}

				STATIC_ALLOC(map, int, count);

				for (i = 0; i < count; i++)
					map[i] = -1;

				/* Reassigning Elements to a Style */
				if (style->numElements > 0)
				{
					/* Check each Element */
					for (i = 0; i < count; i++)
					{
						/* See if this Element is already used by the Style */
						for (j = 0; j < style->numElements; j++)
						{
							if (elemList[i] == style->elements[j].elem)
							{
								/* Preserve it */
								map[i] = j;
								break;
							}
						}
					}
				}
				Style_ChangeElements(tree, style, count, elemList, map);
				if (elemList != NULL)
					ckfree((char *) elemList);
				STATIC_FREE(map, int, count);
				break;
			}
			TreeStyle_ListElements(tree, (TreeStyle) style);
			break;
		}

		/* T style layout S E ?option? ?value? ?option value ...? */
		case COMMAND_LAYOUT:
		{
			return StyleLayoutCmd(clientData, interp, objc, objv);
		}

		case COMMAND_NAMES:
		{
			Tcl_Obj *listObj;
			Tcl_HashSearch search;
			Tcl_HashEntry *hPtr;
			TreeStyle style;

			listObj = Tcl_NewListObj(0, NULL);
			hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
			while (hPtr != NULL)
			{
				style = (TreeStyle) Tcl_GetHashValue(hPtr);
				Tcl_ListObjAppendElement(interp, listObj,
					TreeStyle_ToObj(style));
				hPtr = Tcl_NextHashEntry(&search);
			}
			Tcl_SetObjResult(interp, listObj);
			break;
		}
	}
	return TCL_OK;
}

int ButtonMaxWidth(TreeCtrl *tree)
{
	int w, h, width = 0;

	PerStateImage_MaxSize(tree, &tree->buttonImage, &w, &h);
	width = MAX(width, w);

	PerStateBitmap_MaxSize(tree, &tree->buttonBitmap, &w, &h);
	width = MAX(width, w);

	if (tree->useTheme)
	{
		if (TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
			TRUE, &w, &h) == TCL_OK)
			width = MAX(width, w);
		if (TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
			FALSE, &w, &h) == TCL_OK)
			width = MAX(width, w);
	}

	return MAX(width, tree->buttonSize);
}

int ButtonHeight(TreeCtrl *tree, int state)
{
	Tk_Image image;
	Pixmap bitmap;
	int w, h;

	image = PerStateImage_ForState(tree, &tree->buttonImage, state, NULL);
	if (image != NULL) {
	    Tk_SizeOfImage(image, &w, &h);
	    return h;
	}

	bitmap = PerStateBitmap_ForState(tree, &tree->buttonBitmap, state, NULL);
	if (bitmap != None) {
		Tk_SizeOfBitmap(tree->display, bitmap, &w, &h);
		return h;
	}

	if (tree->useTheme &&
		TreeTheme_GetButtonSize(tree, Tk_WindowId(tree->tkwin),
			(state & STATE_OPEN) != 0, &w, &h) == TCL_OK)
		return h;

	return tree->buttonSize;
}

char *TreeStyle_Identify(StyleDrawArgs *drawArgs, int x, int y)
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	ElementLink *eLink = NULL;
	int i;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;

	Style_CheckNeededSize(tree, style, state);

	if (drawArgs->width < style->minWidth + drawArgs->indent)
		drawArgs->width = style->minWidth + drawArgs->indent;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	x -= drawArgs->x;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

	for (i = style->numElements - 1; i >= 0; i--)
	{
		struct Layout *layout = &layouts[i];
		eLink = layout->eLink;
		if ((x >= layout->x + layout->ePadX[PAD_TOP_LEFT]) && (x < layout->x + layout->ePadX[PAD_TOP_LEFT] + layout->iWidth) &&
			(y >= layout->y + layout->ePadY[PAD_TOP_LEFT]) && (y < layout->y + layout->ePadY[PAD_TOP_LEFT] + layout->iHeight))
		{
			goto done;
		}
	}
	eLink = NULL;
done:
	STATIC_FREE(layouts, struct Layout, style->numElements);
	if (eLink != NULL)
		return (char *) eLink->elem->name;
	return NULL;
}

void TreeStyle_Identify2(StyleDrawArgs *drawArgs,
	int x1, int y1, int x2, int y2, Tcl_Obj *listObj)
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	ElementLink *eLink;
	int i;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;

	Style_CheckNeededSize(tree, style, state);

	if (drawArgs->width < style->minWidth + drawArgs->indent)
		drawArgs->width = style->minWidth + drawArgs->indent;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

	for (i = style->numElements - 1; i >= 0; i--)
	{
		struct Layout *layout = &layouts[i];
		eLink = layout->eLink;
		if ((drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT] < x2) &&
			(drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT] + layout->iWidth > x1) &&
			(drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT] < y2) &&
			(drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT] + layout->iHeight > y1))
		{
			Tcl_ListObjAppendElement(drawArgs->tree->interp, listObj,
				Tcl_NewStringObj(eLink->elem->name, -1));
		}
	}

	STATIC_FREE(layouts, struct Layout, style->numElements);
}

int TreeStyle_Remap(TreeCtrl *tree, TreeStyle styleFrom_, TreeStyle styleTo_, int objc, Tcl_Obj *CONST objv[])
{
	Style *styleFrom = (Style *) styleFrom_;
	Style *styleTo = (Style *) styleTo_;
	int i, indexFrom, indexTo;
	int staticMap[STATIC_SIZE], *map = staticMap;
	ElementLink *eLink;
	Element *elemFrom, *elemTo;
	Element *staticElemMap[STATIC_SIZE], **elemMap = staticElemMap;
	int styleFromNumElements = styleFrom->numElements;
	int result = TCL_OK;

	/* Must be instance */
	if ((styleFrom == NULL) || (styleFrom->master == NULL))
		return TCL_ERROR;

	/* Must be master */
	if ((styleTo == NULL) || (styleTo->master != NULL))
		return TCL_ERROR;

	/* Nothing to do */
	if (styleFrom->master == styleTo)
		return TCL_OK;

	if (objc & 1)
		return TCL_ERROR;

	STATIC_ALLOC(map, int, styleFrom->numElements);
	STATIC_ALLOC(elemMap, Element *, styleFrom->numElements);

	for (i = 0; i < styleFrom->numElements; i++)
		map[i] = -1;

	for (i = 0; i < objc; i += 2)
	{
		/* Get the old-style element */
		if (Element_FromObj(tree, objv[i], &elemFrom) != TCL_OK)
		{
			result = TCL_ERROR;
			goto done;
		}

		/* Verify the old style uses the element */
		eLink = Style_FindElem(tree, styleFrom->master, elemFrom, &indexFrom);
		if (eLink == NULL)
		{
			FormatResult(tree->interp, "style %s does not use element %s",
				styleFrom->name, elemFrom->name);
			result = TCL_ERROR;
			goto done;
		}

		/* Get the new-style element */
		if (Element_FromObj(tree, objv[i + 1], &elemTo) != TCL_OK)
		{
			result = TCL_ERROR;
			goto done;
		}

		/* Verify the new style uses the element */
		eLink = Style_FindElem(tree, styleTo, elemTo, &indexTo);
		if (eLink == NULL)
		{
			FormatResult(tree->interp, "style %s does not use element %s",
				styleTo->name, elemTo->name);
			result = TCL_ERROR;
			goto done;
		}

		/* Must be the same type */
		if (elemFrom->typePtr != elemTo->typePtr)
		{
			FormatResult(tree->interp, "can't map element type %s to %s",
				elemFrom->typePtr->name, elemTo->typePtr->name);
			result = TCL_ERROR;
			goto done;
		}

		/* See if the instance style has any info for this element */
		eLink = &styleFrom->elements[indexFrom];
		if (eLink->elem->master != NULL)
		{
			map[indexFrom] = indexTo;
			elemMap[indexFrom] = eLink->elem;
		}
	}

	for (i = 0; i < styleFrom->numElements; i++)
	{
		eLink = &styleFrom->elements[i];
		indexTo = map[i];

		/* Free info for any Elements not being remapped */
		if ((indexTo == -1) && (eLink->elem->master != NULL))
		{
			elemFrom = eLink->elem->master;
			Element_FreeResources(tree, eLink->elem);
			eLink->elem = elemFrom;
		}

		/* Remap this Element */
		if (indexTo != -1)
		{
			elemMap[i]->master = styleTo->elements[indexTo].elem;
			elemMap[i]->name = styleTo->elements[indexTo].elem->name;
		}
	}

	if (styleFrom->numElements != styleTo->numElements)
	{
#ifdef ALLOC_HAX
		if (styleFrom->numElements > 0)
			AllocHax_CFree(tree->allocData, (char *) styleFrom->elements,
				sizeof(ElementLink), styleFrom->numElements, 5);
		styleFrom->elements = (ElementLink *) AllocHax_CAlloc(tree->allocData,
			sizeof(ElementLink), styleTo->numElements, 5);
#else
		if (styleFrom->numElements > 0)
			wipefree((char *) styleFrom->elements, sizeof(ElementLink) *
			styleFrom->numElements);
		styleFrom->elements = (ElementLink *) ckalloc(sizeof(ElementLink) *
			styleTo->numElements);
#endif
		memset(styleFrom->elements, '\0', sizeof(ElementLink) * styleTo->numElements);
	}
	for (i = 0; i < styleTo->numElements; i++)
	{
		styleFrom->elements[i].elem = styleTo->elements[i].elem;
		styleFrom->elements[i].neededWidth = -1;
		styleFrom->elements[i].neededHeight = -1;
	}
	for (i = 0; i < styleFrom->numElements; i++)
	{
		indexTo = map[i];
		if (indexTo != -1)
			styleFrom->elements[indexTo].elem = elemMap[i];
	}
	styleFrom->name = styleTo->name;
	styleFrom->master = styleTo;
	styleFrom->neededWidth = styleFrom->neededHeight = -1;
	styleFrom->numElements = styleTo->numElements;

done:
	STATIC_FREE(map, int, styleFromNumElements);
	STATIC_FREE(elemMap, Element *, styleFromNumElements);
	return result;
}

int TreeStyle_GetSortData(TreeCtrl *tree, TreeStyle style_, int elemIndex, int type, long *lv, double *dv, char **sv)
{
	Style *style = (Style *) style_;
	ElementLink *eLink = style->elements;
	int i;

	if (elemIndex == -1)
	{
		for (i = 0; i < style->numElements; i++)
		{
			if (ELEMENT_TYPE_MATCHES(eLink->elem->typePtr, &elemTypeText))
				return Element_GetSortData(tree, eLink->elem, type, lv, dv, sv);
			eLink++;
		}
	}
	else
	{
		if ((elemIndex < 0) || (elemIndex >= style->numElements))
			panic("bad elemIndex %d to TreeStyle_GetSortData", elemIndex);
		eLink = &style->elements[elemIndex];
		if (ELEMENT_TYPE_MATCHES(eLink->elem->typePtr, &elemTypeText))
			return Element_GetSortData(tree, eLink->elem, type, lv, dv, sv);
	}

	FormatResult(tree->interp, "can't find text element in style %s",
		style->name);
	return TCL_ERROR;
}

int TreeStyle_ValidateElements(StyleDrawArgs *drawArgs, int objc, Tcl_Obj *CONST objv[])
{
	Style *style = (Style *) drawArgs->style;
	Style *master = style->master ? style->master : style;
	Element *elem;
	ElementLink *eLink;
	int i;

	for (i = 0; i < objc; i++)
	{
		if (Element_FromObj(drawArgs->tree, objv[i], &elem) != TCL_OK)
			return TCL_ERROR;

		eLink = Style_FindElem(drawArgs->tree, master, elem, NULL);
		if (eLink == NULL)
		{
			FormatResult(drawArgs->tree->interp,
				"style %s does not use element %s",
				style->name, elem->name);
			return TCL_ERROR;
		}
	}
	return TCL_OK;
}

int TreeStyle_GetElemRects(StyleDrawArgs *drawArgs, int objc,
	Tcl_Obj *CONST objv[], XRectangle rects[])
{
	Style *style = (Style *) drawArgs->style;
	Style *master = style->master ? style->master : style;
	int i, j, count = 0;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
	Element *staticElems[STATIC_SIZE], **elems = staticElems;
	ElementLink *eLink;

	STATIC_ALLOC(elems, Element *, objc);

	for (j = 0; j < objc; j++)
	{
		if (Element_FromObj(drawArgs->tree, objv[j], &elems[j]) != TCL_OK)
		{
			count = -1;
			goto done;
		}

		eLink = Style_FindElem(drawArgs->tree, master, elems[j], NULL);
		if (eLink == NULL)
		{
			FormatResult(drawArgs->tree->interp,
				"style %s does not use element %s",
				style->name, elems[j]->name);
			count = -1;
			goto done;
		}
	}

	if (drawArgs->width < style->minWidth + drawArgs->indent)
		drawArgs->width = style->minWidth + drawArgs->indent;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, FALSE, __FILE__, __LINE__);

	for (i = style->numElements - 1; i >= 0; i--)
	{
		struct Layout *layout = &layouts[i];
		if (objc > 0)
		{
			for (j = 0; j < objc; j++)
				if (elems[j] == layout->eLink->elem ||
					elems[j] == layout->master->elem)
					break;
			if (j == objc)
				continue;
		}
		rects[count].x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
		rects[count].y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
		if (layout->master->onion == NULL)
		{
			rects[count].x += layout->iPadX[PAD_TOP_LEFT];
			rects[count].y += layout->iPadY[PAD_TOP_LEFT];
			rects[count].width = layout->useWidth;
			rects[count].height = layout->useHeight;
		}
		else
		{
			rects[count].width = layout->iWidth;
			rects[count].height = layout->iHeight;
		}
		count++;
	}

	STATIC_FREE(layouts, struct Layout, style->numElements);

done:
	STATIC_FREE(elems, Element *, objc);
	return count;
}

int TreeStyle_ChangeState(TreeCtrl *tree, TreeStyle style_, int state1, int state2)
{
	Style *style = (Style *) style_;
	ElementLink *eLink;
	ElementArgs args;
	int i, eMask, mask = 0;

	if (state1 == state2)
		return 0;

	args.tree = tree;
	args.states.state1 = state1;
	args.states.state2 = state2;

	for (i = 0; i < style->numElements; i++)
	{
		eLink = &style->elements[i];
		args.elem = eLink->elem;
		eMask = (*eLink->elem->typePtr->stateProc)(&args);
		if (eMask)
		{
			if (eMask & CS_LAYOUT)
				eLink->neededWidth = eLink->neededHeight = -1;
			mask |= eMask;
		}
	}

	if (mask & CS_LAYOUT)
		style->neededWidth = style->neededHeight = -1;

#ifdef NEEDEDHAX
	if (style->neededWidth != -1)
		style->neededState = state2;
#endif

	return mask;
}

void Tree_UndefineState(TreeCtrl *tree, int state)
{
	TreeItem item;
	TreeItemColumn column;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	ElementLink *eLink;
	int i, columnIndex;
	ElementArgs args;

	args.tree = tree;
	args.state = state;

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if (style != NULL)
			{
				for (i = 0; i < style->numElements; i++)
				{
					eLink = &style->elements[i];
					/* Instance element */
					if (eLink->elem->master != NULL) {
						args.elem = eLink->elem;
						(*args.elem->typePtr->undefProc)(&args);
					}
					eLink->neededWidth = eLink->neededHeight = -1;
				}
				style->neededWidth = style->neededHeight = -1;
				TreeItemColumn_InvalidateSize(tree, column);
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		TreeItem_InvalidateHeight(tree, item);
		Tree_FreeItemDInfo(tree, item, NULL);
		TreeItem_UndefineState(tree, item, state);
		hPtr = Tcl_NextHashEntry(&search);
	}
	Tree_InvalidateColumnWidth(tree, -1);
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

	hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
	while (hPtr != NULL)
	{
		args.elem = (Element *) Tcl_GetHashValue(hPtr);
		(*args.elem->typePtr->undefProc)(&args);
		hPtr = Tcl_NextHashEntry(&search);
	}
}

int TreeStyle_NumElements(TreeCtrl *tree, TreeStyle style_)
{
	return ((Style *) style_)->numElements;
}

int TreeStyle_Init(Tcl_Interp *interp)
{
	return TCL_OK;
}

void TreeStyle_Free(TreeCtrl *tree)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Element *elem;
	Style *style;

	while (1)
	{
		hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
		if (hPtr == NULL)
			break;
		style = (Style *) Tcl_GetHashValue(hPtr);
		TreeStyle_FreeResources(tree, (TreeStyle) style);
	}

	while (1)
	{
		hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
		if (hPtr == NULL)
			break;
		elem = (Element *) Tcl_GetHashValue(hPtr);
		Element_FreeResources(tree, elem);
	}

	Tcl_DeleteHashTable(&tree->elementHash);
	Tcl_DeleteHashTable(&tree->styleHash);
}

