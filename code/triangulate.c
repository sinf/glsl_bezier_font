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

#if 1
#define subs_vec2(c,a,b) { (c)[0]=(a)[0]-(b)[0]; (c)[1]=(a)[1]-(b)[1]; }
#define interpolate(c,a,b,t) { (c)[0]=(a)[0] * (1-t) + (b)[0] * t; (c)[1]=(a)[1] * (1-t) + (b)[1] * t; }
#define cross2(a,b) ((a)[0]*(b)[1]-(a)[1]*(b)[0])
#else
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
#endif

static PointCoord ac_cross_ab( PointCoord const a[2], PointCoord const b[2], PointCoord const c[2] )
{
	PointCoord ab[2], ac[2];
	subs_vec2( ab, b, a );
	subs_vec2( ac, c, a );
	return cross2( ac, ab );
}

static int any_point_in_triangle( PointCoord const coords[], size_t num_points, PointCoord const a[2], PointCoord const b[2], PointCoord const c[2] )
{
	PointCoord ab[2], bc[2], ca[2];
	size_t n;
	
	subs_vec2( ab, b, a );
	subs_vec2( bc, c, b );
	subs_vec2( ca, a, c );
	
	for( n=0; n<num_points; n++ )
	{
		PointCoord ap[2], bp[2], cp[2];
		PointCoord const *p = coords + 2*n;
		int s, t, u;
		
		if ( p == a || p == b || p == c )
			continue;
		
		subs_vec2( ap, p, a );
		subs_vec2( bp, p, b );
		subs_vec2( cp, p, c );
		
		s = cross2( ab, ap ) > 0;
		t = cross2( bc, bp ) > 0;
		u = cross2( ca, cp ) > 0;
		
		if ( s == t && s == u )
			return 1;
	}
	
	return 0;
}

static void merge_extra_verts( Contour *co, PointCoord coords[], PointFlag flags[], size_t num_orig_points )
{
	struct {
		LLNodeID a, b, c, d, e;
		LLNodeID f, g;
	} nodes;
	
	if ( co->points.length < 5 )
		return;
	
	nodes.c = co->points.root;
	do {
		nodes.b = LL_PREV( co->points, nodes.c );
		nodes.d = LL_NEXT( co->points, nodes.c );
		nodes.a = LL_PREV( co->points, nodes.b );
		nodes.e = LL_NEXT( co->points, nodes.d );
		
		if (
		( flags[nodes.a] & PT_ON_CURVE )
		&& !( flags[nodes.b] & PT_ON_CURVE )
		&& ( flags[nodes.c] & PT_ON_CURVE ) )
		{
			PointCoord *a, *b, *c, ab[2]; /* , bc[2]; */
			
			a = coords + 2 * nodes.a;
			b = coords + 2 * nodes.b;
			c = coords + 2 * nodes.c;
			
			subs_vec2( ab, b, a );
			/* subs_vec2( bc, c, b ); */
			
			/* Subdivide overlapping triangles */
			if ( any_point_in_triangle( coords, num_orig_points, a, b, c ) )
			{
				PointCoord *f, *g;
				
				nodes.f = add_node( &co->points, nodes.b );
				if ( nodes.f == LL_BAD_INDEX )
					return;
				
				nodes.g = add_node( &co->points, nodes.c );
				if ( nodes.g == LL_BAD_INDEX ) {
					pop_node( &co->points, nodes.f );
					return;
				}
				
				f = coords + 2 * nodes.f;
				g = coords + 2 * nodes.g;
				
				interpolate( f, a, b, 0.5 );
				interpolate( g, c, b, 0.5 );
				interpolate( b, f, g, 0.5 );
				
				flags[ nodes.f ] = 0;
				flags[ nodes.g ] = 0;
				flags[ nodes.b ] = PT_ON_CURVE;
			}
			else
			/* Delete redudant points */
			if ( !( flags[nodes.d] & PT_ON_CURVE ) && ( flags[nodes.e] & PT_ON_CURVE ) && 0 )
			{
				/* We have found a on-OFF-on-OFF-on sequence
				If B,C,D are on the same side of line AE, then it is possible to remove B and D without changing the geometry
				Also, the resulting triangle ACE must not overlap with other geometry (or many glitches happens)
				*/
				
				PointCoord ae[2], ad[2], /* bd[2], */ ed[2], *d, *e;
				int sign1, sign2;
				
				d = coords + 2 * nodes.d;
				e = coords + 2 * nodes.e;
				
				subs_vec2( ae, e, a );
				subs_vec2( ad, d, a );
				/* subs_vec2( bd, d, b ); */
				subs_vec2( ed, d, e );
				
				sign1 = cross2( ae, ab ) <= 0;
				sign2 = cross2( ae, ad ) <= 0;
				/* sign3 = cross2( bd, bc ) <= 0; */
				
				/* ( b and d lie on the same side of line ae ) && ( c lies on that very same side of line bd ) */
				if ( sign1 == sign2 )
				{
					const PointCoord epsilon = 0.0001;
					PointCoord p[2];
					
					interpolate( p, b, d, 0.5 );
					subs_vec2( p, p, c );
					
					if ( p[0]*p[0] + p[1]*p[1] < epsilon*epsilon )
					{
						/* c lies approximately halfway trough between b and d */
						
						PointCoord w;
						w = e[0] + ed[0] * a[1] - ed[0] * e[1] - ed[1] * a[0];
						w /= ed[1] * ab[0] - ed[0] * ab[1];
						p[0] = a[0] + w * ab[0];
						p[1] = a[1] + w * ab[1];
						
						if ( !any_point_in_triangle( coords, num_orig_points, a, p, e ) )
						{
							c[0] = p[0];
							c[1] = p[1];
							flags[ nodes.c ] = 0;
							pop_node( &co->points, nodes.b );
							pop_node( &co->points, nodes.d );
						}
					}
					
					/**
					P = The position where B,C,D could be merged
				1)	P = A + w * AB
				2)	P = E + g * ED
					
					A + w*AB = E + g*ED
					
					Components:
				3)	a[0] + w * ab[0] = e[0] + g * ed[0]
				4)	a[1] + w * ab[1] = e[1] + g * ed[1]
					
					Solve g from equation 4):
					a[1] + w * ab[1] = e[1] + g * ed[1]
					a[1] + w * ab[1] - e[1] = g * ed[1]
					( a[1] + w * ab[1] - e[1] ) / ed[1] = g
					
					Plug that g to equation 3):
					a[0] + w * ab[0] = e[0] + { ( a[1] + w * ab[1] - e[1] ) / ed[1] } * ed[0]
					a[0] + w * ab[0] = e[0] + { ed[0] * ( a[1] + w * ab[1] - e[1] ) } / ed[1]
					a[0] + w * ab[0] = e[0] + { ed[0] * a[1] + ed[0] * w * ab[1] - ed[0] * e[1] } / ed[1]
					ed[1] * a[0] + ed[1] * w * ab[0] = e[0] + { ed[0] * a[1] + ed[0] * w * ab[1] - ed[0] * e[1] }
					ed[1] * a[0] + ed[1] * w * ab[0] = e[0] + ed[0] * a[1] + ed[0] * w * ab[1] - ed[0] * e[1]
					ed[1] * w * ab[0] - ed[0] * w * ab[1] = e[0] + ed[0] * a[1] - ed[0] * e[1] - ed[1] * a[0]
					w * ( ed[1] * ab[0] - ed[0] * ab[1] ) = e[0] + ed[0] * a[1] - ed[0] * e[1] - ed[1] * a[0]
					w = { e[0] + ed[0] * a[1] - ed[0] * e[1] - ed[1] * a[0] } / { ed[1] * ab[0] - ed[0] * ab[1] }
					
					Now compute w and then obtain P from equation 1)
					**/
				}
			}
		}
		
		nodes.c = LL_NEXT( co->points, nodes.c );
	} while( nodes.c != co->points.root );
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
			PointCoord *coord_a = coords + 2*a;
			PointCoord *coord_b = coords + 2*b;
			
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

static double get_signed_polygon_area( PointCoord const coords[], size_t num_points )
{
	size_t a=0, b=1;
	double area;
	
	if ( num_points < 3 )
		return 0;
	
	do {
		PointCoord const
			*p0 = coords + 2*a,
			*p1 = coords + 2 *b;
		
		area += p0[0] * (double) p1[1] - p1[0] * (double) p0[1];
		
		a = b;
		b = ( b + 1 ) % num_points;
	} while( a );
	
	return area;
}

static int point_in_polygon( PointCoord const coords[], size_t num_points, PointCoord const p[2] )
{
	size_t p0=0, p1=1;
	int inside = 0;
	
	if ( num_points < 3 )
		return 0;
	
	do {
		PointCoord const *a, *b;
		a = coords + 2 * p0;
		b = coords + 2 * p1;
		
		/* There is an intersection if points a and b lie on different sides of the horizontal line y=p[1] */
		if (( a[1] <= p[1] && b[1] > p[1] ) || ( a[1] > p[1] && b[1] <= p[1] ))
		{
			/* the condition above avoids division by zero when b[1]==a[1] */
			inside ^= ( p[1] - a[1] ) * ( b[0] - a[0] ) / ( b[1] - a[1] ) + ( a[0] ) < p[0];
		}
		
		p0 = p1;
		p1 = ( p1 + 1 ) % num_points;
	} while( p0 );
	
	return inside;
}

typedef struct {
	PointCoord *coords;
	PointFlag *flags;
	LinkedList *newpts;
	PointIndex *ptr; /* index output array */
	size_t num; /* number of indices */
	uint16 *num_points_total;
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
	
	if ( node+1 > p->num_points_total[0] )
		p->num_points_total[0] = node+1;
	
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
		uint16 d;
		uint16 d_start, d_end;
		
		end = end_points[c];
		if ( end >= MAX_GLYPH_POINTS )
			return TR_POINTS_LIMIT;
		
		init_list( &c1->points, node_pool, start, end );
		c1->points.root = start;
		c1->points.free_root_p = &new_points_list.free_root;
		c1->points.length = end - start + 1;
		c1->is_hole = 0;
		
		/* Detect vertex winding */
		c1->clockwise = get_signed_polygon_area( point_coords + 2 * start, end - start + 1 ) < 0;
		
		/* Determine, whether c1 is an exterior outline or an interior one */
		d_start = 0;
		for( d=0; d<num_contours; d++ )
		{
			d_end = end_points[d];
			if ( d != c && d_end < MAX_GLYPH_POINTS )
			{
				size_t d_length = d_end - d_start + 1;
				size_t p = start;
				int c_inside_d = 1;
				while( p <= end )
				{
					if ( !point_in_polygon( point_coords + 2*d_start, d_length, point_coords + 2*p ) ) {
						c_inside_d = 0;
						break;
					}
					p++;
				}
				c1->is_hole ^= c_inside_d;
			}
			d_start = d_end + 1;
		}
		
		start = end + 1;
	}
	
	/*
	From now on points may be added, removed or modified.
	Functions such as point_in_polygon and get_signed_polygon_area shouldn't be used anymore
	because contours' point lists are no longer guaranteed to be laid out continuously in memory
	*/
	
	for( c=0; c<num_contours; c++ )
	{
		TrError err;
		
		if ( con[c].points.length < 3 ) {
			/* invalid contour */
			continue;
		}
		
		/* Make sure that there are no multiple consecutive off-curve points anywhere */
		err = split_consecutive_off_curve_points( con+c, point_coords, point_flags );
		if ( err != TR_SUCCESS )
			return err;
	}
	
	for( c=0; c<num_contours; c++ ) {
		merge_extra_verts( con+c, point_coords, point_flags, gt->num_points_orig );
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
		GLdouble glu_coords[MAX_GLYPH_TRI_INDICES+1][2];
		MyGLUCallbackArg arg;
		GLUtesselator *handle = glu_tess_handle;
		
		arg.coords = point_coords;
		arg.flags = point_flags;
		arg.newpts = &new_points_list;
		arg.num = num_tris[2] * 3;
		arg.ptr = triangles[2];
		arg.num_points_total = &gt->num_points_total;
		
		gluTessBeginPolygon( handle, &arg );
		
		for( c=0; c<num_contours; c++ )
		{
			LLNodeID node, next, prev;
			PointFlag vertex_id_bit = 2;
			
			if ( con[c].points.length < 3 )
				continue;
			
			/*
			todo:
			fix this: sometimes start and end points end up with the same vertex ID bit, causing a nearby curve to be rendered incorrectly
			*/
			
			if ( 1 )
			{
				/* 2 on-curve points on both sides of an off-curve point might sometimes get the same ID bit.
				Then the on-curve points would have the same texture coordinates and the curve would render incorrectly
				This can be solved by duplicating a point */
				
				LLNodeID dummy, root;
				
				root = con[c].points.root;
				dummy = add_node( &con[c].points, root );
				
				if ( dummy != LL_BAD_INDEX ) {
					point_flags[dummy] = point_flags[root];
					point_coords[ 2 * dummy ] = point_coords[ 2 * root ];
					point_coords[ 2 * dummy + 1 ] = point_coords[ 2 * root + 1 ];
				}
			}
			
			gluTessBeginContour( handle );
			node = con[c].points.root;
			
			do {
				int must_add = 1;
				
				next = LL_NEXT( con[c].points, node );
				
				/* Get the highest node index in use */
				if ( node+1 > gt->num_points_total )
					gt->num_points_total = node+1;
				
				if ( !( point_flags[ node ] & PT_ON_CURVE ) )
				{
					/* Off-curve point */
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
					/* glu_coords[node][2] = 0; Using the X coordinate of the next vertex as a Z coordinate seems to work just fine */
					
					gluTessVertex( handle, glu_coords[node], (void*)(size_t) node );
				}
				
				node = next;
			} while( node != con[c].points.root );
			
			gluTessEndContour( handle );
		}
		
		gluTessEndPolygon( handle );
		
		num_tris[2] = arg.num / 3;
		/** gt->num_points_total += new_points_list.length; **/
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
