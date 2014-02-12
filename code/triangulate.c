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

#define ENABLE_SUBDIV 0
#define DEBUG_DUMP 1

#if DEBUG_DUMP
#include <stdio.h>
#endif

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

/* The contour must have at least 1 point */
static TrError split_consecutive_off_curve_points( Contour *co, PointCoord coords[2*MAX_GLYPH_POINTS], PointFlag flags[MAX_GLYPH_POINTS] )
{
	LLNodeID a, start;
	a = start = co->points.root;
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

/* The contour must have at least 1 point  */
static int contour_is_clockwise( Contour con[1], PointCoord const coords[] )
{
	float area = 0;
	LLNodeID a = con->points.root;
	do {
		LLNodeID b = LL_NEXT( con->points, a );
		PointCoord const
			*p0 = coords + 2 * a,
			*p1 = coords + 2 * b;
		area += p0[0] * p1[1] - p1[0] * p0[1];
		a = b;
	} while( a != con->points.root );
	return ( area < 0 );
}

/* This function treats the contour as a polygon (which it is NOT), so it might fail sometimes
The contour must have at least 1 point */
static int point_in_contour( Contour con[1], PointCoord const p[2], PointCoord const coords[] )
{
	LLNodeID node;
	int inside = 0;
	
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

/* The contour must have at least 1 point  */
static int contour_in_contour( Contour in[1], Contour out[2], PointCoord const coords[] )
{
#if 0
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

#if ENABLE_SUBDIV
static int point_in_triangle( PointCoord const a[2], PointCoord const b[2], PointCoord const c[2], PointCoord const p[2] )
{
	PointCoord ab[2], bc[2], ca[2];
	PointCoord x, t;
	int num = 0;
	
	subs_vec2( ab, b, a );
	subs_vec2( bc, c, b );
	subs_vec2( ca, a, c );
	
	t = ( p[1] - a[1] ) / ab[1];
	x = a[0] + t * ab[0];
	num += ( t >= 0 && t < 1 && x > p[0] );
	
	t = ( p[1] - b[1] ) / bc[1];
	x = b[0] + t * bc[0];
	num += ( t >= 0 && t < 1 && x > p[0] );
	
	t = ( p[1] - c[1] ) / ca[1];
	x = c[0] + t * ca[0];
	num += ( t >= 0 && t < 1 && x > p[0] );
	
	return ( num == 1 );
}
static TrError subdivide_overlapping_triangles( Contour *con1, Contour *con2, PointCoord coords[2*MAX_GLYPH_POINTS], PointFlag flags[MAX_GLYPH_POINTS] )
{
	LLNodeID prev, cur, next;
	
	if ( con1->points.length < 3 || con2->points.length < 3 )
		return TR_SUCCESS;
	
	cur = con1->points.root;
	do {
		prev = LL_PREV( con1->points, cur );
		next = LL_NEXT( con1->points, cur );
		
		if (( flags[prev] & PT_ON_CURVE )
		&& !( flags[cur] & PT_ON_CURVE )
		&& ( flags[next] & PT_ON_CURVE ))
		{
			PointCoord const *a,  *c;
			PointCoord *b;
			LLNodeID other;
			
			a = coords + 2 * prev;
			b = coords + 2 * cur;
			c = coords + 2 * next;
			
			other = con2->points.root;
			do {
				if ( other != prev && other != cur && other != next
				&& point_in_triangle( a, b, c, coords + 2 * other ) )
				{
					/* subdivide ABC */
					
					LLNodeID b1, c1;
					
					b1 = add_node( &con1->points, cur );
					if ( b1 == LL_BAD_INDEX )
						return TR_POINTS_LIMIT;
					
					c1 = add_node( &con1->points, next );
					if ( c1 == LL_BAD_INDEX )
						return TR_POINTS_LIMIT;
					
					interpolate( coords+2*b1, a, b, 0.5 );
					interpolate( coords+2*c1, b, c, 0.5 );
					interpolate( b, coords+2*b1, coords+2*c1, 0.5 );
					
					flags[ b1 ] = flags[ c1 ] = 0;
					flags[ cur ] = PT_ON_CURVE;
					break;
				}
				other = LL_NEXT( con2->points, other );
			} while( other != con2->points.root );
		}
		
		cur = next;
	} while( cur != con1->points.root );
	
	return TR_SUCCESS;
}
#endif

typedef struct {
	PointCoord *coords;
	PointFlag *flags;
	LinkedList *newpts;
	PointIndex *ptr; /* index output array */
	size_t num; /* number of indices */
} MyGLUCallbackArg;

static void glu_combine_callback( GLdouble co[3], size_t input[4], GLfloat weight[4], size_t output[1], MyGLUCallbackArg p[1] )
{
	LLNodeID node = add_node( p->newpts, LL_BAD_INDEX );
	if ( node == LL_BAD_INDEX ) {
		#if DEBUG_DUMP
		printf( "GLU combine callback: not enough memory\n" );
		#endif
		exit( 79 );
	}
	p->coords[ 2 * node ] = co[0];
	p->coords[ 2 * node + 1 ] = co[1];
	p->flags[ node ] = PT_ON_CURVE;
	output[0] = node;
	
	(void) weight;
	(void) input;
}

static void glu_vertex_callback( size_t index, MyGLUCallbackArg *p ) {
	if ( p->num < MAX_GLYPH_TRI_INDICES )
		p->ptr[ p->num++ ] = index;
}

#if DEBUG_DUMP
static void glu_error_handler( GLenum code )
{
	static int rep = 0;
	if ( rep < 15 ) {
		rep++;
		printf( "GLU error callback: %d\n", code );
	}
}
#endif

void triangulator_end( void *handle ) { gluDeleteTess( handle ); }
void *triangulator_begin( void )
{
	GLUtesselator *handle;
	
	handle = gluNewTess();
	if ( !handle )
		return NULL;
	
	/* Registering an edge flag callback prevents GLU from outputting triangle fans and strips
	(even if all the callback does is to compute the absolute value of it's argument)
	Also,
	see http://www.glprogramming.com/red/chapter11.html for winding rules and other GLU stuff
	*/
	
	#if DEBUG_DUMP
	gluTessCallback( handle, GLU_TESS_ERROR, (_GLUfuncptr) glu_error_handler );
	#endif
	
	gluTessCallback( handle, GLU_TESS_COMBINE_DATA, (_GLUfuncptr) glu_combine_callback );
	gluTessCallback( handle, GLU_TESS_VERTEX_DATA, (_GLUfuncptr) glu_vertex_callback );
	gluTessCallback( handle, GLU_TESS_EDGE_FLAG, (_GLUfuncptr) abs );
	gluTessProperty( handle, GLU_TESS_BOUNDARY_ONLY, GL_FALSE );
	gluTessProperty( handle, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO );
	gluTessNormal( handle, 0, 0, 1 );
	return handle;
}

TrError triangulate_contours( void *glu_tess_handle, GlyphTriangles gt[1] )
{
	uint16 num_contours = gt->num_contours;
	PointFlag *point_flags = gt->flags;
	PointCoord *point_coords = gt->points;
	uint16 *end_points = gt->end_points;
	
	size_t num_tris[3] = {0,0,0};
	PointIndex *convex_tris;
	PointIndex concave_tris[MAX_GLYPH_TRI_INDICES];
	PointIndex solid_tris[MAX_GLYPH_TRI_INDICES];
	PointIndex *triangles[3]; /* triangle indices: 1. convex 2. concave 3. solid */
	LLNode node_pool[MAX_GLYPH_POINTS];
	Contour con[MAX_GLYPH_CONTOURS];
	uint16 start=0, end, c;
	LinkedList new_points_list;
	
	gt->num_points_total = 0;
	gt->end_points = end_points;
	gt->points = point_coords;
	gt->flags = point_flags;
	
	/* all contours share the same "empty" list, which begins after the last original point */
	init_list( &new_points_list, node_pool, gt->num_points_orig, MAX_GLYPH_POINTS - 1 );
	
	/* Construct a linked list for each contour */
	for( c=0; c<num_contours; c++ )
	{
		Contour *c1 = con + c;
		
		end = end_points[c];
		if ( end >= MAX_GLYPH_POINTS )
			return TR_POINTS_LIMIT;
		
		init_list( &c1->points, node_pool, start, end );
		c1->points.root = start;
		c1->points.free_root_p = &new_points_list.free_root;
		c1->points.length = end - start + 1;
		c1->is_hole = 0;
		
		start = end + 1;
	}
	
	for( c=0; c<num_contours; c++ )
	{
		uint16 d;
		TrError err;
		
		if ( con[c].points.length < 3 ) {
			/* invalid contour */
			continue;
		}
		
		/* clockwise or counterclockwise */
		con[c].clockwise = contour_is_clockwise( con+c, point_coords );
		
		/* Make sure that there are no multiple consecutive off-curve points anywhere */
		err = split_consecutive_off_curve_points( con+c, point_coords, point_flags );
		if ( err != TR_SUCCESS )
			return err;
		
		for( d=0; d<num_contours; d++ )
		{
			/*
			Overlapping curves don't render properly (due to Z-buffer)
			and they greatly confuse the GLU tesselator
			*/
			
			#if ENABLE_SUBDIV
			size_t loops=0, old_len;
			TrError err;
			do {
				/* Keep subdividing until nothing overlaps. */
				old_len = con[c].points.length;
				err = subdivide_overlapping_triangles( con+c, con+d, point_coords, point_flags );
				if ( err != TR_SUCCESS )
					return err;
			} while( ++loops < 3 && old_len != con[c].points.length );
			#endif
			
			/* Determine, which polygons represent holes */
			if ( d != c )
				con[c].is_hole ^= contour_in_contour( con+c, con+d, point_coords );
		}
		
		/* Compute total number of points */
		gt->num_points_total += con[c].points.length;
	}
	
	convex_tris = malloc( MAX_GLYPH_TRI_INDICES * sizeof( convex_tris[0] ) );
	if ( !convex_tris )
		return TR_ALLOC_FAIL;
	
	triangles[0] = convex_tris;
	triangles[1] = concave_tris;
	triangles[2] = solid_tris;
	
	/* Triangulate */
	if ( 1 )
	{
		GLdouble glu_coords[MAX_GLYPH_TRI_INDICES][3];
		MyGLUCallbackArg arg;
		GLUtesselator *handle = glu_tess_handle;
		
		arg.coords = point_coords;
		arg.flags = point_flags;
		arg.newpts = &new_points_list;
		arg.num = num_tris[2] * 3;
		arg.ptr = triangles[2];
		
		gluTessBeginPolygon( handle, &arg );
		
		for( c=0; c<num_contours; c++ )
		{
			LLNodeID node, next;
			PointFlag vertex_id_bit = 2;
			
			if ( con[c].points.length < 3 )
				continue;
			
			/*
			todo:
			fix this: sometimes start and end points end up with the same vertex ID bit, causing a nearby curve to be rendered incorrectly
			*/
			
			gluTessBeginContour( handle );
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
					int test[ sizeof(void*) == sizeof(size_t) ];
					(void) test;
					
					glu_coords[node][0] = point_coords[ 2*node ];
					glu_coords[node][1] = point_coords[ 2*node+1 ];
					glu_coords[node][2] = 0;
					gluTessVertex( handle, glu_coords[node], (void*)(size_t) node );
				}
				
				node = next;
			} while( node != con[c].points.root );
			
			gluTessEndContour( handle );
		}
		
		gluTessEndPolygon( handle );
		
		num_tris[2] = arg.num / 3;
		gt->num_points_total += new_points_list.length;
	}
	
	gt->num_indices_total = 3 * ( num_tris[0] + num_tris[1] + num_tris[2] );
	gt->num_indices_convex = 3 * num_tris[0];
	gt->num_indices_concave = 3 * num_tris[1];
	gt->num_indices_solid = 3 * num_tris[2];
	gt->indices = NULL;
	
	if ( gt->num_indices_total > 0 )
	{
		gt->indices = realloc( triangles[0], sizeof( PointIndex ) * gt->num_indices_total );
		
		if ( !gt->indices )
		{
			free( triangles[0] );
			return TR_ALLOC_FAIL;
		}
		
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
