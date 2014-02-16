#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "opengl.h"
#include "gpufont_data.h"
#include "gpufont_draw.h"

typedef uint32_t uint32;
typedef uint16_t uint16;

/* todo:
- Draw the glyphs properly instead of points/lines
- Use Uniform Buffer Objects to store glyph positions
- Alternative rendering code for GPUs that lack the instancing extension
- Use glBindAttribLocation instead of the GLSL layout qualifier
*/

#define GL_UINT_TYPE( t ) ( sizeof( t ) == 1 ? GL_UNSIGNED_BYTE : ( sizeof( t ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT ) )
#define GL_INT_TYPE( t ) ( sizeof( t ) == 1 ? GL_BYTE : ( sizeof( t ) == 2 ? GL_SHORT : GL_INT ) )
#define GL_FLOAT_TYPE( t ) ( sizeof( t ) == 4 ? GL_FLOAT : GL_DOUBLE )

static struct {
	GLuint glyph_positions_ub;
	GLint glyph_positions;
	GLint the_matrix;
	GLint the_color;
	GLint fill_mode;
} uniforms = {0};

typedef enum {
	FILL_CURVE=0,
	FILL_SOLID=2,
	SHOW_FLAGS=3
} FillMode;

enum {
	BATCH_SIZE = 2024, /* used to be 2024 */
	UBLOCK_BINDING = 0,
	USE_UBO = 0
};

static GLuint the_prog = 0;
static GLuint em_sq_vao = 0, em_sq_vbo = 0;
static float const em_sq_points[4*2] = {0,0,1,0,1,1,0,1};
static GLint max_ubo_size = 0;

static void prepare_em_sq( void )
{
	/* prepare EM square VBO and VAO */
	
	glGenVertexArrays( 1, &em_sq_vao );
	glGenBuffers( 1, &em_sq_vbo );
	
	glBindVertexArray( em_sq_vao );
	
	glBindBuffer( GL_ARRAY_BUFFER, em_sq_vbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( em_sq_points ), em_sq_points, GL_STATIC_DRAW );
	
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
	
	glBindVertexArray( 0 );
}

int init_font_shader( GLuint_ linked_compiled_prog )
{
	int size_check[ sizeof( GLuint_ ) >= sizeof( GLuint ) ];
	(void) size_check;
	
	/*
	if ( !have_opengl_ext( "GL_ARB_draw_instanced" ) )
	{
		printf( "Required extension 'GL_ARB_draw_instanced' is not supported\n" );
		return 0;
	}
	*/
	glGetIntegerv( GL_MAX_UNIFORM_BLOCK_SIZE, &max_ubo_size );
	printf( "Max uniform block size: %d bytes (%d vec2's)\n", (int) max_ubo_size, (int)(max_ubo_size/sizeof(float)/2) );
	
	the_prog = linked_compiled_prog;
	
	glUseProgram( the_prog );
	uniforms.the_matrix = glGetUniformLocation( the_prog, "the_matrix" );
	uniforms.the_color = glGetUniformLocation( the_prog, "the_color" );
	uniforms.fill_mode = glGetUniformLocation( the_prog, "fill_mode" );
	
	if ( USE_UBO ) {
		/* temporarily moved to draw_glyphs */
		/*
		const GLchar *block_name = "GlyphPositions";
		GLuint block_index;
		
		printf( "Setting up uniform block\n" );
		block_index = glGetUniformBlockIndex( the_prog, block_name );
		
		if ( block_index == GL_INVALID_INDEX ) {
			printf( "Could not find uniform block %s\n", block_name );
			return 0;
		} else {
			printf( "Uniform block index: %u\n", block_index );
		}
		
		uniforms.glyph_positions_ub = block_index;
		glUniformBlockBinding( the_prog, block_index, UBLOCK_BINDING );
		*/
	} else {
		printf( "Not using UBO\n" );
		uniforms.glyph_positions = glGetUniformLocation( the_prog, "glyph_positions" );
	}
	
	prepare_em_sq();
	
	return 1;
}

void deinit_font_shader( void )
{
	glDeleteVertexArrays( 1, &em_sq_vao );
	glDeleteBuffers( 1, &em_sq_vbo );
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
			uint32 subglyph_id = *( (uint32*) glyph + 1 + k );
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
	for( n=0; n<4; n++ )
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
	
	glGenVertexArrays( 1, buf );
	glGenBuffers( 3, buf+1 );
	
	glBindVertexArray( buf[0] );
	
	/* point coords & indices */
	glBindBuffer( GL_ARRAY_BUFFER, buf[1] );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buf[2] );
	glBufferData( GL_ARRAY_BUFFER, font->total_points * sizeof( float ) * 2, font->all_points, GL_STATIC_DRAW );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, font->total_indices * sizeof( PointIndex ), font->all_indices, GL_STATIC_DRAW );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
	
	/* point flags */
	glBindBuffer( GL_ARRAY_BUFFER, buf[3] );
	glBufferData( GL_ARRAY_BUFFER, font->total_points * sizeof( PointFlag ), font->all_flags, GL_STATIC_DRAW );
	glEnableVertexAttribArray( 1 );
	glVertexAttribIPointer( 1, 1, GL_UINT_TYPE( PointFlag ), 0, 0 );
	
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
		uint16 n_curve, n_solid;
		uint16 *offset;
		GLenum index_type = GL_UINT_TYPE( PointIndex );
		
		offset = (uint16*) ( sizeof( font->all_indices[0] ) * ( glyph->tris.indices - font->all_indices ) );
		
		n_curve = glyph->tris.num_indices_curve;
		n_solid = glyph->tris.num_indices_solid;
		
		if ( ( flags & F_DRAW_CURVE ) && n_curve ) {
			if ( flags & F_DEBUG_COLORS )
				debug_color( 1 );
			set_fill_mode( fill_curve );
			glDrawElementsInstancedBaseVertex( GL_TRIANGLES, n_curve, index_type, offset, num_instances, first_vertex );
		}
		if ( ( flags & F_DRAW_SOLID ) && n_solid ) {
			if ( flags & F_DEBUG_COLORS )
				debug_color( 4 );
			set_fill_mode( FILL_SOLID );
			glDrawElementsInstancedBaseVertex( GL_TRIANGLES, n_solid, index_type, offset+n_curve, num_instances, first_vertex );
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

void draw_glyphs( struct Font *font, float global_transform[16], size_t glyph_index, size_t num_instances, float positions[], int flags )
{
	SimpleGlyph *glyph;
	size_t start, end, count;
	GLuint ubo;
	GLuint ub_index;
	int is_simple;
	
	if ( !num_instances )
		return;
	
	glyph = font->glyphs[ glyph_index ];
	
	if ( !glyph || !glyph->tris.num_points_total ) {
		/* glyph has no outline or doesn't even exist */
		return;
	}
	
	start = 0;
	end = num_instances % BATCH_SIZE;
	if ( !end )
		end = end < BATCH_SIZE ? num_instances : BATCH_SIZE;
	
	if ( USE_UBO ) {
		const GLchar *block_name = "GlyphPositions";
		GLint ubo_size;
		
		ub_index = glGetUniformBlockIndex( the_prog, block_name );
		if ( ub_index == GL_INVALID_INDEX ) {
			printf( "Could not find uniform block %s\n", block_name );
			exit( 1 );
		}
		
		glUniformBlockBinding( the_prog, ub_index, UBLOCK_BINDING );
		ubo_size = sizeof( float ) * 2 * BATCH_SIZE;
		
		if ( ubo_size > max_ubo_size ) {
			printf( "Too big UBO; need %u but can have at most %u bytes\n", ubo_size, max_ubo_size );
			return;
		}
		
		glGenBuffers( 1, &ubo );
		glBindBuffer( GL_UNIFORM_BUFFER, ubo );
		glBufferData( GL_UNIFORM_BUFFER, ubo_size, NULL, GL_STREAM_DRAW );
	}
	
	is_simple = IS_SIMPLE_GLYPH( glyph );
	if ( is_simple ) {
		/* The glyph consists of only one part
		and every batch draws many instances of that same part.
		Therefore every batch shares the same matrix */
		send_matrix( global_transform );
	}
	
	do {
		count = end - start;
		
		if ( USE_UBO ) {
			glBindBufferRange( GL_UNIFORM_BUFFER, ub_index, ubo, 0, sizeof( float ) * 2 * count );
			glBufferSubData( GL_UNIFORM_BUFFER, 0, sizeof( float ) * 2 * count, positions + 2*start );
		} else {
			glUniform2fv( uniforms.glyph_positions, count, positions+2*start );
		}
		
		if ( is_simple )
			draw_instances( font, count, glyph_index, flags );
		#if ENABLE_COMPOSITE_GLYPHS
		else
			draw_composite_glyph( font, glyph, count, global_transform, flags );
		#endif
		
		if ( flags & F_DRAW_SQUARE ) {
			if ( flags & F_DEBUG_COLORS ) debug_color( 2 );
			set_fill_mode( FILL_SOLID );
			glBindVertexArray( em_sq_vao );
			glDrawArraysInstancedARB( ( flags & F_ALL_SOLID ) ? GL_TRIANGLE_STRIP : GL_LINE_LOOP, 0, 4, num_instances );
			glBindVertexArray( font->gl_buffers[0] );
		}
		
		start = end;
		end += BATCH_SIZE;
		
	} while( end <= num_instances );
	
	if ( USE_UBO ) {
		glBindBuffer( GL_UNIFORM_BUFFER, 0 );
		glDeleteBuffers( 1, &ubo );
	}
}
