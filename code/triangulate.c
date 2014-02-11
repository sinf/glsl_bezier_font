#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <GL/glu.h>

#include "types.h"
#include "font_data.h"
#include "opentype.h"
#include "linkedlist.h"
#include "triangulate.h"

typedef struct {
	LinkedList points;
	int clockwise; /* 1 if clockwise, 0 if counter-clockwise */
	int convex; /* 1 if convex, 0 if concave */
	int is_hole;
} Contour;

static void subs_vec2( PointCoord c[2], PointCoord const a[2], PointCoord const b[2] )
{
	c[0] = a[0] - b[0];
	c[1] = a[1] - b[1];
}

static void interpolate( PointCoord c[2], PointCoord const a[2], PointCoord const b[2], PointCoord t )
{
	float q = 1 - t;
	c[0] = q * a[0] + t * b[0];
	c[1] = q * a[1] + t * b[1];
}

/* Returns Z-component of the resulting vector (because X and Y would be zero) */
static PointCoord cross2( PointCoord const a[2], PointCoord const b[2] ) {
	return a[0] * b[1] - a[1] * b[0];
}

static PointCoord ac_cross_ab( PointCoord const a[2], PointCoord const b[2], PointCoord const c[2] )
{
	PointCoord ab[2], ac[2];
	subs_vec2( ab, b, a );
	subs_vec2( ac, c, a );
	return cross2( ac, ab );
}

static TrError split_consecutive_off_curve_points( Contour *co, PointCoord coords[2*MAX_GLYPH_POINTS], PointFlag flags[MAX_GLYPH_POINTS] )
{
	LLNodeID a, start;
	a = start = co->points.root;
	
	if ( start == LL_BAD_INDEX ) {
		/* contour has no points */
		return TR_EMPTY_CONTOUR;
	}
	
	if ( co->points.length < 3 ) {
		return TR_SUCCESS;
	}
	
	do {
		LLNodeID b = LL_NEXT( co->points, a );
		
		if ( !( flags[a] & PT_ON_CURVE ) && !( flags[b] & PT_ON_CURVE ) )
		{
			PointCoord const *coord_a = coords + 2*a;
			PointCoord const *coord_b = coords + 2*b;
			LLNodeID c = add_node( &co->points, b ); /* add a node between a & b */
			
			if ( c == LL_BAD_INDEX ) {
				/* can't add more points */
				return TR_POINTS_LIMIT;
			}
			
			assert( c < MAX_GLYPH_POINTS );
			assert( LL_NEXT( co->points, a ) == c );
			assert( LL_PREV( co->points, b ) == c );
			
			interpolate( coords + 2*c, coord_a, coord_b, 0.5 );
			flags[c] = PT_ON_CURVE;
		}
		
		a = b;
	} while( a != start );
	
	return TR_SUCCESS;
}

static void detect_winding_and_if_convex( Contour con[1], PointCoord const coords[] )
{
	LLNodeID a;
	float sum = 0;
	float prev_area = 0;
	int is_convex = 1;
	
	con->clockwise = 0;
	con->convex = 1;
	
	if ( con->points.length < 3 )
		return;
	
	a = con->points.root;
	do {
		LLNodeID b, c;
		PointCoord area;
		
		b = LL_NEXT( con->points, a );
		c = LL_NEXT( con->points, b );
		area = ac_cross_ab( coords+2*a, coords+2*b, coords+2*c );
		
		if ( area * prev_area < 0 ) {
			/* if the sign of that cross product changes even once then the contour is not convex */
			is_convex = 0;
		}
		
		sum += area;
		prev_area = area;
		a = b;
	} while( a != con->points.root );
	
	con->convex = is_convex;
	con->clockwise = ( sum > 0 );
}

/* This function treats the contour as a polygon (which it is NOT), so it might fail sometimes */
static int point_in_contour( Contour con[1], PointCoord const p[2], PointCoord const coords[] )
{
	LLNodeID node;
	int inside = 0;
	
	if ( con->points.length < 3 )
		return 0;
	
	node = con->points.root;
	
	do {
		LLNodeID next;
		PointCoord const *a, *b;
		
		next = LL_NEXT( con->points, node );
		a = coords + 2 * node;
		b = coords + 2 * next;
		
		/* There is an intersection if points a and b lie on different sides of the horizontal line y=p[1] */
		if (( a[1] <= p[1] && b[1] > p[1] ) || ( a[1] > p[1] && b[1] <= p[1] ))
		{
			/* division by zero can happen if ( b[1] == a[1] ), but that is handled by the ifs above */
			inside ^= ( p[1] - a[1] ) * ( b[0] - a[0] ) / ( b[1] - a[1] ) + ( a[0] ) < p[0];
		}
		
		node = next;
	} while( node != con->points.root );
	
	return inside;
}

static int contour_in_contour( Contour in[1], Contour out[2], PointCoord const coords[] )
{
	if ( in->points.length > 3 )
	{
	#if 1
		/* The simple, quick solution: test only 1 point */
		PointCoord const *p0 = coords + 2 * in->points.root;
		return point_in_contour( out, p0, coords );
	#else
		/* Slower but fixes problems with some fonts */
		LLNodeID node = in->points.root;
		do {
			if ( !point_in_contour( out, coords + 2 * node, coords ) )
				return 0;
			node = LL_NEXT( in->points, node );
		} while( node != in->points.root );
		return 1;
	#endif
	}
	return 0;
}

typedef struct {
	PointIndex *ptr; /* index output array */
	size_t num; /* number of indices */
} MyGLUCallbackArg;

static void glu_vertex_callback( void *index, MyGLUCallbackArg *p ) {
	if ( p->num < MAX_GLYPH_TRI_INDICES )
		p->ptr[ p->num++ ] = (size_t) index;
}

TrError triangulate_contours(
	GlyphTriangles gt[1],
	PointFlag point_flags[MAX_GLYPH_POINTS],
	PointCoord point_coords[2*MAX_GLYPH_POINTS],
	uint16 end_points[],
	uint32 num_contours )
{
	size_t num_tris[3] = {0,0,0};
	PointIndex triangles[3][MAX_GLYPH_TRI_INDICES]; /* triangle indices: 1. convex 2. concave 3. solid */
	LLNode node_pool[MAX_GLYPH_POINTS];
	Contour con[MAX_GLYPH_CONTOURS];
	uint16 start=0, end, c;
	uint16 num_orig_points;
	LinkedList dummy;
	
	num_orig_points = end_points[ num_contours - 1 ] + 1;
	
	gt->num_points_orig = num_orig_points;
	gt->end_points = end_points;
	gt->points = point_coords;
	gt->flags = point_flags;
	
	/* all contours share the same "empty" list, which begins after the last original point */
	init_list( &dummy, node_pool, num_orig_points, MAX_GLYPH_POINTS - 1 );
	
	/* Construct a linked list for each contour */
	for( c=0; c<num_contours; c++ )
	{
		TrError err;
		Contour *c1 = con + c;
		
		end = end_points[c];
		
		if ( end >= MAX_GLYPH_POINTS )
			return TR_POINTS_LIMIT;
		
		init_list( &c1->points, node_pool, start, end );
		c1->points.root = start;
		c1->points.free_root_p = &dummy.free_root;
		c1->points.length = end - start + 1;
		c1->is_hole = 0;
		
		detect_winding_and_if_convex( c1, point_coords );
		
		/* Make sure that there are no multiple consecutive off-curve points anywhere */
		err = split_consecutive_off_curve_points( c1, point_coords, point_flags );
		if ( err != TR_SUCCESS )
			return err;
		
		start = end + 1;
	}
	
	gt->num_points_total = 0;
	for( c=0; c<num_contours; c++ )
	{
		uint16 d;
		
		/* Compute total number of points */
		gt->num_points_total += con[c].points.length;
		
		/* Determine, which polygons represent holes */
		for( d=0; d<num_contours; d++ )
		{
			if ( d != c )
				con[c].is_hole ^= contour_in_contour( con+c, con+d, point_coords );
		}
	}
	
	/* Triangulate */
	if ( 1 )
	{
		GLdouble glu_coords[MAX_GLYPH_TRI_INDICES][3];
		GLUtesselator *handle;
		MyGLUCallbackArg arg;
		
		arg.num = num_tris[2] * 3;
		arg.ptr = triangles[2];
		
		handle = gluNewTess();
		if ( !handle )
			return TR_ALLOC_FAIL;
		
		/* Registering an edge flag callback prevents GLU from outputting triangle fans and strips
		(even if all the callback does is to compute the absolute value of it's argument) */
		
		gluTessCallback( handle, GLU_TESS_VERTEX_DATA, (_GLUfuncptr) glu_vertex_callback );
		gluTessCallback( handle, GLU_TESS_EDGE_FLAG, (_GLUfuncptr) abs );
		gluTessProperty( handle, GLU_TESS_BOUNDARY_ONLY, GL_FALSE );
		gluTessProperty( handle, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO );
		gluTessNormal( handle, 0, 0, 1 );
		gluTessBeginPolygon( handle, &arg );
		
		/*
		ODD or NONZERO ?
		GLU_TESS_WINDING_ODD would make self-intersecting polygons behave as expected but breaks some fonts where contours partially overlap.
		see http://www.glprogramming.com/red/chapter11.html for winding rules and other GLU stuff
		*/
		
		for( c=0; c<num_contours; c++ )
		{
			if ( con[c].points.length >= 3 )
			{
				LLNodeID node, next;
				PointFlag vertex_id_bit = 2;
				
				gluTessBeginContour( handle );
				
				/*
				todo:
				fix this: sometimes start and end points end up with the same vertex ID bit, causing a nearby curve to be rendered incorrectly
				*/
				
				node = con[c].points.root;
				do {
					int must_add = 1;
					
					next = LL_NEXT( con[c].points, node );
					
					if ( !( point_flags[ node ] & PT_ON_CURVE ) )
					{
						/* Off-curve point */
						LLNodeID prev;
						int is_clockwise;
						int is_concave;
						size_t t;
						
						prev = LL_PREV( con[c].points, node );
						is_clockwise = ac_cross_ab( point_coords + 2 * prev, point_coords + 2 * node, point_coords + 2 * next ) > 0;
						is_concave = ( is_clockwise != con[c].clockwise ) ^ con[c].is_hole;
						must_add &= is_concave;
						t = num_tris[ is_concave ] * 3;
						
						if ( t < MAX_GLYPH_TRI_INDICES - 3 )
						{
							/* add a triangle */
							
							num_tris[ is_concave ] += 1;
							triangles[ is_concave ][ t ] = node;
							triangles[ is_concave ][ t + 1 + !is_concave ] = next;
							triangles[ is_concave ][ t + 1 + is_concave ] = prev;
							
							point_flags[ prev ] = vertex_id_bit | PT_ON_CURVE;
							point_flags[ next ] = ( vertex_id_bit = vertex_id_bit ^ 2 ) | PT_ON_CURVE;
							/* point_flags[ node ] = 0; should already be zero */
						}
					}
					
					if ( must_add )
					{
						glu_coords[node][0] = point_coords[ 2*node ];
						glu_coords[node][1] = point_coords[ 2*node+1 ];
						glu_coords[node][2] = 0;
						gluTessVertex( handle, glu_coords[node], (void*)(size_t) node );
					}
					
					node = next;
				} while( node != con[c].points.root );
				
				gluTessEndContour( handle );
			}
		}
		
		gluTessEndPolygon( handle );
		gluDeleteTess( handle );
		num_tris[2] = arg.num / 3;
	}
	
	gt->num_indices_total = 3 * ( num_tris[0] + num_tris[1] + num_tris[2] );
	gt->num_indices_convex = 3 * num_tris[0];
	gt->num_indices_concave = 3 * num_tris[1];
	gt->num_indices_solid = 3 * num_tris[2];
	gt->indices = NULL;
	
	if ( gt->num_indices_total > 0 )
	{
		gt->indices = malloc( sizeof( PointIndex ) * gt->num_indices_total );
		
		if ( !gt->indices )
			return TR_ALLOC_FAIL;
		
		memcpy(
			gt->indices,
			triangles[0],
			3 * num_tris[0] * sizeof( PointIndex ) );
		memcpy(
			gt->indices + 3 * num_tris[0],
			triangles[1],
			3 * num_tris[1] * sizeof( PointIndex ) );
		memcpy(
			gt->indices + 3 * ( num_tris[0] + num_tris[1] ),
			triangles[2],
			3 * num_tris[2] * sizeof( PointIndex ) );
	}
	
	return TR_SUCCESS;
}
