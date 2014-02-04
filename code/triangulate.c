#include <stdlib.h>
#include "types.h"
#include "font_data.h"
#include "opentype.h"

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

struct Point;
typedef struct Point Point;
struct Point {
	Point *prev, *next;
	uint32 index;
};

/* Cyclic linked list */
typedef struct {
	Point *head, *tail;
	uint32 num_points;
} PointList;

static const PointList EMPTY_POINT_LIST = {NULL,NULL,0};

enum {
	/* memory limits */
	MAX_CONTOURS = 128,
	MAX_CONTOUR_HOLES = 127
};

typedef struct {
	PointList all, outer, inner; /* inner and outer are polylines that can be used to connect this contour to some other contour with triangles */
	uint32 inc_depth; /* how deep in the hierarchy this contour is (hole inside hole inside hole....) */
	uint8 num_inc; /* number of valid items in inc */
	uint8 inc[MAX_CONTOUR_HOLES]; /* indices of contours that are inside this contour */
} Contour;

static Point *add_point( PointList *list, uint32 index )
{
	Point *pt = calloc( sizeof(Point), 1 );
	
	if ( !pt )
		return NULL;
	
	pt->index = index;
	
	if ( list->head )
	{
		pt->next = list->head;
		list->head->prev = pt;
	}
	else
	{
		list->head = pt;
		pt->next = pt;
	}
	
	if ( list->tail )
	{
		pt->prev = list_tail;
		list->tail->next = pt;
	}
	else
		pt->prev = pt;
	
	list->tail = pt;
	list->num_points++;
	
	return pt;
}

static void remove_point( PointList *list, Point *p )
{
	Point *prev=p->prev, *next=p->next;
	if ( next == prev ) {
		list->head = list->tail = NULL;
	} else {
		if ( prev ) prev->next = next;
		if ( next ) next->prev = prev;
		if ( p == list->head ) list->head = next;
		if ( p == list->tail ) list->tail = prev;
	}
	list->num_points--;
	free( p );
}

static void delete_list( PointList *list )
{
	while( list->head )
		remove_point( list, list->head );
}

static void split_consecutive_off_curve_points( Contour *c, uint8 point_flags[] )
{
	Point *p, *start;
	uint32 num_off = 0;
	
	start = p = c->all.head;
	
	do {
		uint8 prev_on = point_flags[ p->prev->index ] & PT_ON_CURVE;
		uint8 next_on = point_flags[ p->next->index ] & PT_ON_CURVE;
		uint8 this_on = point_flags[ p->index ] & PT_ON_CURVE;
		
		
		if ( !( point_flags[ p->index ] & PT_ON_CURVE ) )
			num_off++;
		if ( num_off == 2 )
		{
			/* split between p->prev and p */
		}
		p = p->next;
	} while( p != start );
	
	/* todo */
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

void triangulate_contours( GlyphTriangles *gt, uint8 point_flags[], float points[], uint16 end_points[], uint32 num_contours )
{
	Contour con[MAX_CONTOURS];
	uint16 on_curve_point[MAX_CONTOURS]; /* 1 on-curve point per contour */
	uint16 start=0, end, c, d, p;
	
	/* Construct linked lists for convenience */
	for( c=0; c<num_contours; c++ )
	{
		end = end_points[c];
		con[c].all = con[c].outer = con[c].inner = EMPTY_POINT_LIST;
		con[c].inc_depth = con[c].num_inc = 0;
		/* Put all contour points into a linked list */
		for( p=start; p<=end; p++ )
			add_point( &con[c].all, p );
		/* Find the first on-curve point (default to the first point if there are no on-curve points) */
		on_curve_point[c] = start;
		for( p=start; p<=end; p++ ) {
			if ( point_flags[p] & PT_ON_CURVE ) {
				on_curve_point[c] = p;
				break;
			}
		}
		start = end;
	}
	
	/* 1. Preprocess */
	for( c=0; c<num_contours; c++ )
	{
		Contour *C = con+c;
		
		/* Make sure that there are no 2 consecutive off-curve points anywhere */
		split_consecutive_off_curve_points( C );
		
		/* Find 2 polylines that can later be used to connect the contour to other contours */
		find_inner_outer( C );
		
		/* Determine which contour contains which contour */
		for( d=0; d<num_contours; d++ )
		{
			Contour *D = con+d;
			
			if ( c == d )
				continue;
			
			/*
			If contour C contains at least 1 point of D,
			and no contours intersect, then C must contain all points in D
			*/
			
			if ( contour_contains_point( C, points + 2 * on_curve_point[d] ) )
			{
				if ( C->num_inc == MAX_CONTOUR_HOLES ) {
					/* error */
				} else {
					C->inc[ C->num_inc++ ] = d;
					D->inc_depth++;
				}
			}
		}
	}
	
	/* 2. Triangulate */
	for( c=0; c<num_contours; c++ )
	{
		if ( con[c].num_inc )
		{
			if ( ( con[c].inc_depth & 1 ) == 0 )
			{
				/* Polygon has holes in it
				Connect
					con[c].inner
				and
					con[ con[c].inc[n] ].outer ( where n goes from 0 to con[c].num_inc )
				Using triangles.
				This is tricky
				*/
			}
			else
			{
				/* Hole within a hole */
			}
		}
		else
		{
			if ( con[c].inc_depth == 0 )
			{
				/* Contour is not included by any other contour. Therefore it represents a solid blob.
				=> Triangulate a (potentially) concave polygon
				*/
			}
			else
			{
				/* Contour is included by some other contour, which means this contour represents a hole
				This case is already handled above
				*/
			}
		}
	}
	
	/* 3. Clean up the mess */
	for( c=0; c<num_contours; c++ )
	{
		delete_list( &con[c].all );
		delete_list( &con[c].outer );
		delete_list( &con[c].inner );
	}
	
	gt->num_points_total = gt->num_points_orig = end_points[ num_contours - 1 ] + 1;
	gt->num_indices_total = 0;
	gt->num_indices_convex = 0;
	gt->num_indices_concave = 0;
	gt->num_indices_solid = 0;
	gt->end_points = end_points;
	gt->indices = NULL;
	gt->points = points;
}
