#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "opengl.h"
#include "gpufont_data.h"
#include "gpufont_draw.h"

#define GET_UINT_TYPE( t ) ( sizeof( t ) == 1 ? GL_UNSIGNED_BYTE : ( sizeof( t ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT ) )
#define GET_INT_TYPE( t ) ( sizeof( t ) == 1 ? GL_BYTE : ( sizeof( t ) == 2 ? GL_SHORT : GL_INT ) )
#define GET_FLOAT_TYPE( t ) ( sizeof( t ) == 4 ? GL_FLOAT : GL_DOUBLE )
#define GET_INTFLOAT_TYPE( t ) (( ((t)0.1) ? GET_FLOAT_TYPE(t) : GET_INT_TYPE(t) ))

#define GLYPH_INDEX_GL_TYPE GET_UINT_TYPE( GlyphIndex )
#define POINT_INDEX_GL_TYPE GET_UINT_TYPE( PointIndex )
#define POINT_FLAG_GL_TYPE GET_UINT_TYPE( PointFlag )
#define POINT_COORD_GL_TYPE GET_INTFLOAT_TYPE( PointCoord )
#define GLYPH_COORD_GL_TYPE GET_INTFLOAT_TYPE( GlyphCoord )

/* Uniform locations */
static struct {
	GLint the_matrix;
	GLint the_color;
	GLint fill_mode;
	GLint coord_scale;
} uniforms = {0};

typedef enum {
	FILL_CURVE=0,
	FILL_SOLID=2,
	SHOW_FLAGS=3
} FillMode;

/* Vertex shader attribute numbers */
enum {
	ATTRIB_POS=0,
	ATTRIB_FLAG=1,
	ATTRIB_GLYPH_POS=2
};

/* The shader program used to draw text */
static GLuint the_prog = 0;

int init_font_shader( GLuint_ linked_compiled_prog )
{
	int size_check[ sizeof( GLuint_ ) >= sizeof( GLuint ) ];
	(void) size_check;
	the_prog = linked_compiled_prog;
	glUseProgram( the_prog );
	uniforms.the_matrix = glGetUniformLocation( the_prog, "the_matrix" );
	uniforms.the_color = glGetUniformLocation( the_prog, "the_color" );
	uniforms.fill_mode = glGetUniformLocation( the_prog, "fill_mode" );
	uniforms.coord_scale = glGetUniformLocation( the_prog, "coordinate_scale" );
	return 1;
}

void deinit_font_shader( void ) {
	glDeleteProgram( the_prog );
}

static void add_glyph_stats( Font *font, SimpleGlyph *glyph, size_t counts[3], unsigned limits[3] )
{
	if ( !glyph )
		return;
	if ( IS_SIMPLE_GLYPH( glyph ) ) {
		counts[0] += glyph->tris.num_points_total;
		counts[1] += glyph->tris.num_indices_curve;
		counts[2] += glyph->tris.num_indices_solid;
		limits[0] = ( glyph->tris.num_points_total > limits[0] ) ? glyph->tris.num_points_total : limits[0];
		limits[1] = ( glyph->tris.num_indices_curve > limits[1] ) ? glyph->tris.num_indices_curve : limits[1];
		limits[2] = ( glyph->tris.num_indices_solid > limits[2] ) ? glyph->tris.num_indices_solid : limits[2];
	} else {
		size_t k;
		for( k=0; k < ( glyph->num_parts ); k++ )
		{
			GlyphIndex subglyph_id = GET_SUBGLYPH_INDEX( glyph, k );
			add_glyph_stats( font, font->glyphs[subglyph_id], counts, limits );
		}
	}
}

static void get_average_glyph_stats( Font *font, unsigned avg[3], unsigned max[3] )
{
	size_t n, total[3] = {0,0,0};
	max[0] = max[1] = max[2] = 0;
	for( n=0; n<( font->num_glyphs ); n++ )
		add_glyph_stats( font, font->glyphs[n], total, max );
	for( n=0; n<3; n++ )
		avg[n] = total[n] / font->num_glyphs;
}

void prepare_font( Font *font )
{
	int size_check[ sizeof( font->gl_buffers[0] ) >= sizeof( GLuint ) ];
	GLuint *buf = font->gl_buffers;
	unsigned stats[3], limits[3];
	
	(void) size_check;
	
	get_average_glyph_stats( font, stats, limits );
	
	printf(
	"Uploading font to GL\n"
	"Average glyph:\n"
	"	Points: %u\n"
	"	Indices: %u curved, %u solid\n"
	"Maximum counts:\n"
	"    Points: %u\n"
	"    Indices: %u curved, %u solid\n"
	,
	stats[0], stats[1], stats[2],
	limits[0], limits[1], limits[2] );
	
	glUseProgram( the_prog );
	
	glGenVertexArrays( 1, buf );
	glGenBuffers( 3, buf+1 );
	
	glBindVertexArray( buf[0] );
	glEnableVertexAttribArray( ATTRIB_POS );
	glEnableVertexAttribArray( ATTRIB_FLAG );
	glEnableVertexAttribArray( ATTRIB_GLYPH_POS ); /* don't have a VBO for this attribute yet */
	glVertexAttribDivisor( ATTRIB_GLYPH_POS, 1 );
	
	/* point coords & indices */
	glBindBuffer( GL_ARRAY_BUFFER, buf[1] );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buf[2] );
	glBufferData( GL_ARRAY_BUFFER, font->total_points * sizeof( PointCoord ) * 2, font->all_points, GL_STATIC_DRAW );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, font->total_indices * sizeof( PointIndex ), font->all_indices, GL_STATIC_DRAW );
	glVertexAttribPointer( ATTRIB_POS, 2, POINT_COORD_GL_TYPE, GL_FALSE, 0, 0 );
	
	/* point flags */
	glBindBuffer( GL_ARRAY_BUFFER, buf[3] );
	glBufferData( GL_ARRAY_BUFFER, font->total_points * sizeof( PointFlag ), font->all_flags, GL_STATIC_DRAW );
	glVertexAttribIPointer( ATTRIB_FLAG, 1, POINT_FLAG_GL_TYPE, 0, 0 );
	
	glBindVertexArray( 0 );
	
	/* todo:
	catch out of memory error (although a >32MiB font probably doesn't even exist)
	*/
}

void release_font( Font *font )
{
	printf( "Releasing font GL buffers\n" );
	glDeleteVertexArrays( 1, font->gl_buffers );
	glDeleteBuffers( 3, font->gl_buffers+1 );
}

void set_text_color( float c[4] ) {
	glUniform4fv( uniforms.the_color, 1, c );
}

static void debug_color( int c )
{
	float colors[][4] = {
		{1,1,1,1},
		{1,0,0,1},
		{0,0,1,1},
		{0,1,0,1},
		{.6,.6,.6,1}
	};
	c %= sizeof( colors ) / sizeof( colors[0] );
	glUniform4fv( uniforms.the_color, 1, colors[c] );
}

void begin_text( Font *font )
{
	glUseProgram( the_prog );
	glUniform1f( uniforms.coord_scale, 1.0f / font->units_per_em );
	debug_color( 0 );
	glBindVertexArray( font->gl_buffers[0] );
}

void end_text( void )
{
	glBindVertexArray( 0 );
}

static void send_matrix( float matrix[16] )
{
	/* Every instance of the glyph is transformed by the same matrix */
	glUniformMatrix4fv( uniforms.the_matrix, 1, GL_FALSE, matrix );
}

static void set_fill_mode( FillMode mode )
{
	static FillMode cur_mode = FILL_SOLID; /* should be set to the same initial value as in the fragment shader code */
	if ( mode == cur_mode ) return;
	cur_mode = mode;
	glUniform1i( uniforms.fill_mode, mode );
}

/* Draws only simple glyphs !! */
static void draw_instances( Font *font, size_t num_instances, size_t glyph_index, int flags )
{
	SimpleGlyph *glyph = font->glyphs[ glyph_index ];
	GLint first_vertex;
	FillMode fill_curve=FILL_CURVE, show_flags=SHOW_FLAGS;
	
	if ( glyph->tris.num_points_total == 0 )
		return;
	
	if ( flags & F_ALL_SOLID )
		fill_curve = show_flags = FILL_SOLID;
	
	/* divided by 2 because each point has both X and Y coordinate */
	first_vertex = ( glyph->tris.points - font->all_points ) / 2;
	
	if ( flags & F_DRAW_TRIS )
	{
		size_t offset = ( sizeof( PointIndex ) * ( glyph->tris.indices - font->all_indices ) );
		unsigned n_curve = glyph->tris.num_indices_curve;
		unsigned n_solid = glyph->tris.num_indices_solid;
		
		if ( ( flags & F_DRAW_CURVE ) && n_curve ) {
			if ( flags & F_DEBUG_COLORS )
				debug_color( 1 );
			set_fill_mode( fill_curve );
			glDrawElementsInstancedBaseVertex( GL_TRIANGLES, n_curve, POINT_INDEX_GL_TYPE, (void*) offset, num_instances, first_vertex );
		}
		if ( ( flags & F_DRAW_SOLID ) && n_solid ) {
			if ( flags & F_DEBUG_COLORS )
				debug_color( 4 );
			set_fill_mode( FILL_SOLID );
			glDrawElementsInstancedBaseVertex( GL_TRIANGLES, n_solid, POINT_INDEX_GL_TYPE, (void*)( offset + sizeof(PointIndex) * n_curve ), num_instances, first_vertex );
		}
	}
	
	if ( flags & F_DRAW_POINTS )
	{
		if ( flags & F_DEBUG_COLORS ) {
			set_fill_mode( show_flags );
			/* this fill mode causes colors to be generated out of nowhere */
		} else {
			set_fill_mode( FILL_SOLID );
		}
		glDrawArraysInstancedARB( GL_POINTS, first_vertex, glyph->tris.num_points_total, num_instances );
	}
}

#if ENABLE_COMPOSITE_GLYPHS
static void pad_2x2_to_4x4( float out[16], float const in[4] )
{
	memset( out+2, 0, sizeof(float)*14 );
	out[0] = in[0];
	out[1] = in[1];
	out[4] = in[2];
	out[5] = in[3];
	out[15] = 1;
}

static void compute_subglyph_matrix( float out[16], float sg_mat[4], float sg_offset[2], float transform[16] )
{
	float a[16];
	float b[16];
	float c[16];
	
	/*
	memcpy( out, transform, sizeof(float)*16 );
	return;
	*/
	
	pad_2x2_to_4x4( a, sg_mat );
	mat4_translation( b, -sg_offset[0], -sg_offset[1], 0 );
	mat4_mult( c, a, b );
	mat4_mult( out, transform, c );
	
	/*
	memcpy( out, transform, sizeof(float)*16 );
	(void) sg_mat;
	(void) sg_offset;
	*/
	/* todo */
}

static void draw_composite_glyph( Font *font, void *glyph, size_t num_instances, float global_transform[16], int flags )
{
	size_t num_parts = GET_SUBGLYPH_COUNT( glyph );
	size_t p;
	
	for( p=0; p < num_parts; p++ )
	{
		GlyphIndex subglyph_index = GET_SUBGLYPH_INDEX( glyph, p );
		SimpleGlyph *subglyph = font->glyphs[ subglyph_index ];
		float *matrix = GET_SUBGLYPH_TRANSFORM( glyph, p );
		float *offset = matrix + 4;
		float m[16];
		
		if ( !subglyph )
			continue;
		
		compute_subglyph_matrix( m, matrix, offset, global_transform );
		
		if ( subglyph->num_parts == 0 ) {
			send_matrix( m );
			draw_instances( font, num_instances, subglyph_index, flags );
		} else {
			/* composite glyph contains other composite glyphs. At least FreeSans.ttf has these */
			/*
			printf( "A rare case of a recursive composite glyph has been discovered!\n" );
			exit(0);
			*/
			draw_composite_glyph( font, subglyph, num_instances, m, flags );
		}
	}
}
#endif

void bind_glyph_positions( GLuint_ vbo, size_t first )
{
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glVertexAttribPointer( ATTRIB_GLYPH_POS, 2, GLYPH_COORD_GL_TYPE, GL_FALSE, 0, (void*)( first * 2 * sizeof( GlyphCoord ) ) );
}

void draw_glyphs( struct Font *font, float global_transform[16], size_t glyph_index, size_t num_instances, int flags )
{
	SimpleGlyph *glyph;
	
	if ( !num_instances )
		return;
	
	glyph = font->glyphs[ glyph_index ];
	
	if ( !glyph || !glyph->tris.num_points_total ) {
		/* glyph has no outline or doesn't even exist */
		return;
	}
	
	if ( IS_SIMPLE_GLYPH( glyph ) ) {
		send_matrix( global_transform );
		draw_instances( font, num_instances, glyph_index, flags );
	} else {
		#if ENABLE_COMPOSITE_GLYPHS
		draw_composite_glyph( font, glyph, num_instances, global_transform, flags );
		#endif
	}
}
