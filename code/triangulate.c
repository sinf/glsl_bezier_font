#include <stdlib.h>
#include "types.h"
#include "font_data.h"
#include "opentype.h"
#include "linkedlist.h"

/* The contour triangulation process

Oletukset
- Mikään käyrä ei leikkaa toista käyrää

1. Joka ääriviivalle A
	Kerää pisteistä linkitetty lista
	Pätki A:n pisteet niin, ettei off-curve pisteitä ole kahta peräkkäin
	A.ulko = Etsi A:n pisteistä se ääriviiva B, joka sisältää kaikki A:n pisteet
	A.sisä = Etsi A:n pisteistä se ääriviiva C, jonka sisällä ei ole yhtään A:n pistettä
	for each off-curve point in B:
		kirjoita kupera kolmio
	for each off-curve point in C:
		kirjoita kovera kolmio
	Joka ääriviivalle D:
		jos D:n 1. piste on C:n sisäpuolella:
			A.lapset += [D]

2. Joka ääriviivalle A
	jos A.lapset on tyhjä:
		muodosta kolmioverkko (A.sisä)
	else:
		for B in A.lapset:
			jos leikkaavat( A.sisä, B.ulko )
				subdivide A ja B leikkauspisteen kohdalta
		// muodosta kolmioverkko (leikkaamattomien) ääriviivojen (A.sisä) ja (A.lapset[]) välille
*/

enum {
	/* memory limits */
	MAX_CONTOURS = 80, /* max contours per glyph */
	MAX_HOLES = 32, /* max holes per polygon */
	MAX_POINTS = 512 /* max points per contour */
};

typedef struct {
	LinkedList points;
	uint8 depth;
	uint8 num_holes;
	uint8 holes[MAX_HOLES]; /* indices of other contours */
} Contour;

/* Returns some point that is on the contour or ~0 on failure */
static uint16 get_on_curve_point( Contour c[1], uint8 flags[] )
{
	uint16 pt = 0xFFFF;
	if ( LL_HAS_NODES( c->points ) )
	{
		LLNodeID cur, start;
		cur = start = c->points.root_index;
		do {
			if ( flags[ cur ] & PT_ON_CURVE )
			{
				pt = cur;
				break;
			}
			cur = LL_NEXT( c->points, cur );
		} while( cur != start );
	}
	return pt;
}

static void split_consecutive_off_curve_points( Contour *c, uint8 point_flags[] )
{
	/* todo */
	(void) point_flags;
	(void) c;
}

static void find_inner_outer( Contour *c )
{
	/* todo */
	(void) c;
}

static int contour_contains_point( Contour *c, float p[2] )
{
	/* todo */
	(void) c;
	(void) p;
	return 0;
}

int triangulate_contours( GlyphTriangles *gt, uint8 point_flags[], float points[], uint16 end_points[], uint32 num_contours )
{
	LLNode con_node_pools[MAX_CONTOURS][MAX_POINTS];
	Contour con[MAX_CONTOURS];
	uint16 start=0, end, c, d, p;
	
	/* Construct a linked list for each contour */
	for( c=0; c<num_contours; c++ )
	{
		end = end_points[c];
		
		if ( end > MAX_POINTS )
			return 0;
		
		init_list( &con[c].points, con_node_pools[c], MAX_POINTS );
		con[c].depth = 0;
		con[c].num_holes = 0;
		
		for( p=start; p<=end; p++ )
			add_node_x( &con[c].points, p );
		
		start = end;
	}
	
	/* 1. Preprocess */
	for( c=0; c<num_contours; c++ )
	{
		Contour *c1 = con + c;
		
		/* Make sure that there are no 2 consecutive off-curve points anywhere */
		split_consecutive_off_curve_points( c1, point_flags );
		
		/* Find 2 polylines that can later be used to connect the contour to other contours */
		find_inner_outer( c1 );
		
		/* Determine which contour contains which contour */
		for( d=0; d<num_contours; d++ )
		{
			Contour *c2 = con + d;
			
			if ( c1== c2 )
				continue;
			
			/*
			If c1 contains at least 1 point of c2 then c1 must contain all the points of c2.
			This is because we assume that no contours intersect.
			*/
			
			p = get_on_curve_point( c2, point_flags );
			
			if ( p == 0xFFFF ) {
				/* c2 doesn't contain any on-curve points and is therefore invalid */
				return 0;
			}
			
			if ( contour_contains_point( c1, points + 2 * p ) )
			{
				if ( c1->num_holes == MAX_HOLES ) {
					/* too many holes */
					return 0;
				} else {
					c1->holes[ c1->num_holes++ ] = d;
					c2->depth++;
				}
			}
		}
	}
	
	/* 2. Triangulate */
	for( c=0; c<num_contours; c++ )
	{
		if ( ( con[c].depth & 1 ) == 0 )
		{
			/* The contour is an exterior (ulkoreuna)
			*/
			
			if ( con[c].num_holes )
			{
				/* A solid blob which contains holes
				todo:
					triangulate a (potentially concave) polygon with (potentially concave) holes in it
					(access holes via con[c].num_holes and con[c].holes)
				*/
			}
			else
			{
				/* A solid blob
				todo:
					triangulate a (potentially concave) polygon
				*/
			}
		}
		else
		{
			/* The contour is an interior (sisäreuna)
			This case is already handled above */
		}
	}
	
	gt->num_points_total = gt->num_points_orig = end_points[ num_contours - 1 ] + 1;
	gt->num_indices_total = 0;
	gt->num_indices_convex = 0;
	gt->num_indices_concave = 0;
	gt->num_indices_solid = 0;
	gt->end_points = end_points;
	gt->indices = NULL;
	gt->points = points;
	
	return 1;
}
