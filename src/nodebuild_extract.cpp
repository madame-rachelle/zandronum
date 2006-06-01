/*
** nodebuild_extract.cpp
**
** Converts the nodes, segs, and subsectors from the node builder's
** internal format to the format used by the rest of the game.
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <string.h>
#include <float.h>

#include "nodebuild.h"
#include "templates.h"
#include "r_main.h"

void FNodeBuilder::Extract (node_t *&outNodes, int &nodeCount,
	seg_t *&outSegs, int &segCount,
	subsector_t *&outSubs, int &subCount,
	vertex_t *&outVerts, int &vertCount)
{
	int i;

	vertCount = Vertices.Size ();
	outVerts = new vertex_t[vertCount];

	for (i = 0; i < vertCount; ++i)
	{
		outVerts[i].x = Vertices[i].x;
		outVerts[i].y = Vertices[i].y;
	}

	subCount = Subsectors.Size();
	outSubs = new subsector_t[subCount];
	memset(outSubs,0,subCount * sizeof(subsector_t));

	nodeCount = Nodes.Size ();
	outNodes = new node_t[nodeCount];

	memcpy (outNodes, &Nodes[0], nodeCount*sizeof(node_t));
	for (i = 0; i < nodeCount; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			if (outNodes[i].intchildren[j] & 0x80000000)
			{
				outNodes[i].children[j] = (BYTE *)(outSubs + (outNodes[i].intchildren[j] & 0x7fffffff)) + 1;
			}
			else
			{
				outNodes[i].children[j] = outNodes + outNodes[i].intchildren[j];
			}
		}
	}

	if (GLNodes)
	{
		TArray<seg_t> segs (Segs.Size()*5/4);

		for (i = 0; i < subCount; ++i)
		{
			DWORD numsegs = CloseSubsector (segs, i, outVerts);
			outSubs[i].numlines = numsegs;
			outSubs[i].firstline = segs.Size() - numsegs;
			outSubs[i].poly = NULL;
		}

		segCount = segs.Size ();
		outSegs = new seg_t[segCount];
		memcpy (outSegs, &segs[0], segCount*sizeof(seg_t));

		for (i = 0; i < segCount; ++i)
		{
			if (outSegs[i].PartnerSeg != NULL)
			{
				outSegs[i].PartnerSeg = &outSegs[Segs[(unsigned int)(size_t)outSegs[i].PartnerSeg-1].storedseg];
			}
		}
	}
	else
	{
		memcpy (outSubs, &Subsectors[0], subCount*sizeof(subsector_t));
		segCount = Segs.Size ();
		outSegs = new seg_t[segCount];
		for (i = 0; i < segCount; ++i)
		{
			const FPrivSeg *org = &Segs[SegList[i].SegNum];
			seg_t *out = &outSegs[i];

			out->v1 = outVerts + org->v1;
			out->v2 = outVerts + org->v2;
			out->backsector = org->backsector;
			out->frontsector = org->frontsector;
			out->linedef = Level.Lines + org->linedef;
			out->sidedef = Level.Sides + org->sidedef;
			out->PartnerSeg = NULL;
			out->bPolySeg = false;
		}
	}

	//Printf ("%i segs, %i nodes, %i subsectors\n", segCount, nodeCount, subCount);

	for (i = 0; i < Level.NumLines; ++i)
	{
		Level.Lines[i].v1 = outVerts + (size_t)Level.Lines[i].v1;
		Level.Lines[i].v2 = outVerts + (size_t)Level.Lines[i].v2;
	}
}

int FNodeBuilder::CloseSubsector (TArray<seg_t> &segs, int subsector, vertex_t *outVerts)
{
	FPrivSeg *seg, *prev;
	angle_t prevAngle;
	double accumx, accumy;
	fixed_t midx, midy;
	int firstVert;
	DWORD first, max, count, i, j;
	bool diffplanes;
	int firstplane;

	first = Subsectors[subsector].firstline;
	max = first + Subsectors[subsector].numlines;
	count = 0;

	accumx = accumy = 0.0;
	diffplanes = false;
	firstplane = Segs[SegList[first].SegNum].planenum;

	// Calculate the midpoint of the subsector and also check for degenerate subsectors.
	// A subsector is degenerate if it exists in only one dimension, which can be
	// detected when all the segs lie in the same plane. This can happen if you have
	// outward-facing lines in the void that don't point toward any sector. (Some of the
	// polyobjects in Hexen are constructed like this.)
	for (i = first; i < max; ++i)
	{
		seg = &Segs[SegList[i].SegNum];
		accumx += double(Vertices[seg->v1].x) + double(Vertices[seg->v2].x);
		accumy += double(Vertices[seg->v1].y) + double(Vertices[seg->v2].y);
		if (firstplane != seg->planenum)
		{
			diffplanes = true;
		}
	}

	midx = fixed_t(accumx / (max - first) / 2);
	midy = fixed_t(accumy / (max - first) / 2);

	seg = &Segs[SegList[first].SegNum];
	prevAngle = PointToAngle (Vertices[seg->v1].x - midx, Vertices[seg->v1].y - midy);
	seg->storedseg = PushGLSeg (segs, seg, outVerts);
	count = 1;
	prev = seg;
	firstVert = seg->v1;

	if (diffplanes)
	{ // A well-behaved subsector. Output the segs sorted by the angle formed by connecting
	  // the subsector's center to their first vertex.

		for (i = first + 1; i < max; ++i)
		{
			angle_t bestdiff = ANGLE_MAX;
			FPrivSeg *bestseg = NULL;
			DWORD bestj = DWORD_MAX;
			j = first;
			do
			{
				seg = &Segs[SegList[j].SegNum];
				angle_t ang = PointToAngle (Vertices[seg->v1].x - midx, Vertices[seg->v1].y - midy);
				angle_t diff = prevAngle - ang;
				if (seg->v1 == prev->v2)
				{
					bestdiff = diff;
					bestseg = seg;
					bestj = j;
					break;
				}
				if (diff < bestdiff && diff > 0)
				{
					bestdiff = diff;
					bestseg = seg;
					bestj = j;
				}
			}
			while (++j < max);
			// Is a NULL bestseg actually okay?
			if (bestseg != NULL)
			{
				seg = bestseg;
			}
			if (prev->v2 != seg->v1)
			{
				// Add a new miniseg to connect the two segs
				PushConnectingGLSeg (subsector, segs, &outVerts[prev->v2], &outVerts[seg->v1]);
				count++;
			}

			prevAngle -= bestdiff;
			seg->storedseg = PushGLSeg (segs, seg, outVerts);
			count++;
			prev = seg;
			if (seg->v2 == firstVert)
			{
				prev = seg;
				break;
			}
		}
	}
	else
	{ // A degenerate subsector. These are handled in three stages:
	  // Stage 1. Proceed in the same direction as the start seg until we
	  //          hit the seg furthest from it.
	  // Stage 2. Reverse direction and proceed until we hit the seg
	  //          furthest from the start seg.
	  // Stage 3. Reverse direction again and insert segs until we get
	  //          to the start seg.
	  // A dot product serves to determine distance from the start seg.

		// Stage 1. Go forward.
		count += OutputDegenerateSubsector (segs, subsector, true, 0, prev, outVerts);

		// Stage 2. Go backward.
		count += OutputDegenerateSubsector (segs, subsector, false, DBL_MAX, prev, outVerts);

		// Stage 3. Go forward again.
		count += OutputDegenerateSubsector (segs, subsector, true, -DBL_MAX, prev, outVerts);
	}

	if (prev->v2 != firstVert)
	{
		PushConnectingGLSeg (subsector, segs, &outVerts[prev->v2], &outVerts[firstVert]);
		count++;
	}

	return count;
}

int FNodeBuilder::OutputDegenerateSubsector (TArray<seg_t> &segs, int subsector, bool bForward, double lastdot, FPrivSeg *&prev, vertex_t *outVerts)
{
	static const double bestinit[2] = { -DBL_MAX, DBL_MAX };
	FPrivSeg *seg;
	int i, j, first, max, count;
	double dot, x1, y1, dx, dy, dx2, dy2;
	bool wantside;

	first = Subsectors[subsector].firstline;
	max = first + Subsectors[subsector].numlines;
	count = 0;

	seg = &Segs[SegList[first].SegNum];
	x1 = Vertices[seg->v1].x;
	y1 = Vertices[seg->v1].y;
	dx = Vertices[seg->v2].x - x1;
	dy = Vertices[seg->v2].y - y1;
	wantside = seg->planefront ^ !bForward;

	for (i = first + 1; i < max; ++i)
	{
		double bestdot = bestinit[bForward];
		FPrivSeg *bestseg = NULL;
		for (j = first + 1; j < max; ++j)
		{
			seg = &Segs[SegList[j].SegNum];
			if (seg->planefront != wantside)
			{
				continue;
			}
			dx2 = Vertices[seg->v1].x - x1;
			dy2 = Vertices[seg->v1].y - y1;
			dot = dx*dx2 + dy*dy2;

			if (bForward)
			{
				if (dot < bestdot && dot > lastdot)
				{
					bestdot = dot;
					bestseg = seg;
				}
			}
			else
			{
				if (dot > bestdot && dot < lastdot)
				{
					bestdot = dot;
					bestseg = seg;
				}
			}
		}
		if (bestseg != NULL)
		{
			if (prev->v2 != bestseg->v1)
			{
				PushConnectingGLSeg (subsector, segs, &outVerts[prev->v2], &outVerts[bestseg->v1]);
				count++;
			}
			seg->storedseg = PushGLSeg (segs, bestseg, outVerts);
			count++;
			prev = bestseg;
			lastdot = bestdot;
		}
	}
	return count;
}

DWORD FNodeBuilder::PushGLSeg (TArray<seg_t> &segs, const FPrivSeg *seg, vertex_t *outVerts)
{
	seg_t newseg;

	newseg.v1 = outVerts + seg->v1;
	newseg.v2 = outVerts + seg->v2;
	newseg.backsector = seg->backsector;
	newseg.frontsector = seg->frontsector;
	if (seg->linedef != -1)
	{
		newseg.linedef = Level.Lines + seg->linedef;
		newseg.sidedef = Level.Sides + seg->sidedef;
	}
	else
	{
		newseg.linedef = NULL;
		newseg.sidedef = NULL;
	}
	newseg.PartnerSeg = (seg_t *)(seg->partner == DWORD_MAX ? 0 : (size_t)seg->partner + 1);
	newseg.bPolySeg = false;
	return (DWORD)segs.Push (newseg);
}

void FNodeBuilder::PushConnectingGLSeg (int subsector, TArray<seg_t> &segs, vertex_t *v1, vertex_t *v2)
{
	seg_t newseg;

	newseg.v1 = v1;
	newseg.v2 = v2;
	newseg.backsector = NULL;
	newseg.frontsector = NULL;
	newseg.linedef = NULL;
	newseg.sidedef = NULL;
	newseg.PartnerSeg = NULL;
	newseg.bPolySeg = false;
	segs.Push (newseg);
}
