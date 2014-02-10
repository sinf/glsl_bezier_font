#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "opengl.h"
#include "shaders.h"
#include "font_shader.h"
#include "matrix.h"

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
	FILL_CONVEX=0,
	FILL_CONCAVE=1,
	FILL_SOLID=2,
	SHOW_FLAGS=3
} FillMode;

enum {
	BATCH_SIZE = 2024,
	UBLOCK_BINDING = 1,
	USE_UBO = 0
};

static GLuint the_prog = 0;
static GLuint em_sq_vao = 0, em_sq_vbo = 0;
static float const em_sq_points[4*2] = {0,0,1,0,1,1,0,1};

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

int load_font_shaders( void )
{
	/*
	if ( !have_opengl_ext( "GL_ARB_draw_instanced" ) )
	{
		printf( "Required extension 'GL_ARB_draw_instanced' is not supported\n" );
		return 0;
	}
	*/
	
	GLint max_ubo_size;
	glGetIntegerv( GL_MAX_UNIFORM_BLOCK_SIZE, &max_ubo_size );
	printf( "Max uniform block size: %d bytes (%d vec2's)\n", (int) max_ubo_size, (int)(max_ubo_size/sizeof(float)/2) );
	
	the_prog = load_shader_prog( "data/bezv.glsl", "data/bezf.glsl" );
	if ( !the_prog )
		return 0;
	
	glUseProgram( the_prog );
	uniforms.the_matrix = glGetUniformLocation( the_prog, "the_matrix" );
	uniforms.the_color = glGetUniformLocation( the_prog, "the_color" );
	uniforms.fill_mode = glGetUniformLocation( the_prog, "fill_mode" );
	
	if ( USE_UBO ) {
		uniforms.glyph_positions_ub = glGetUniformBlockIndex( the_prog, "GlyphPosition" );
		glUniformBlockBinding( the_prog, uniforms.glyph_positions_ub, UBLOCK_BINDING );
	} else {
		uniforms.glyph_positions = glGetUniformLocation( the_prog, "glyph_positions" );
	}
	
	prepare_em_sq();
	
	return 1;
}

void unload_font_shaders( void )
{
	glDeleteVertexArrays( 1, &em_sq_vao );
	glDeleteBuffers( 1, &em_sq_vbo );
	glDeleteProgram( the_prog );
}

void prepare_font( Font *font )
{
	int size_check[ sizeof( GLuint ) == sizeof( font->gl_buffers[0] ) ];
	GLuint *buf = font->gl_buffers;
	
	printf( "Uploading font to GL\n" );
	(void) size_check;
	
	glGenVertexArrays( 1, buf );
	glGenBuffers( 3, buf+1 );
	
	glBindVertexArray( buf[0] );
	
	/* point coords & indices */
	glBindBuffer( GL_ARRAY_BUFFER, buf[1] );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buf[2] );
	glBufferData( GL_ARRAY_BUFFER, font->total_points * sizeof( PointCoord ) * 2, font->all_points, GL_STATIC_DRAW );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, font->total_indices * sizeof( PointIndex ), font->all_indices, GL_STATIC_DRAW );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT_TYPE( PointCoord ), GL_FALSE, 0, 0 );
	
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

/*
void draw_text( ..... )
{
	1. upload global uniforms: color, transformation matrix
	2. break glyphs into components
	3. sort glyph components by index
	4. for each unique component type:
		bind relevant data
		upload array of component positions
		set uniform fill_mode = FILL_CONVEX
		draw convex curves (glDrawArraysInstancedARB)
		set uniform fill_mode = FILL_CONCAVE
		draw concave curves
		set uniform fill mode = FILL_SOLID
		draw solid triangles
}
*/

static void set_color( int c )
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

static GLuint cur_vao = 0;
static void bind_vertex_array( GLuint vao )
{
	if ( cur_vao == vao ) return;
	glBindVertexArray( vao );
	cur_vao = vao;
}

void begin_text( Font *font )
{
	glUseProgram( the_prog );
	set_color( 0 );
	bind_vertex_array( font->gl_buffers[0] );
}

void end_text( void )
{
	bind_vertex_array( 0 );
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
static void draw_instances( Font *font, uint32 num_instances, uint32 glyph_index, int flags )
{
	SimpleGlyph *glyph = font->glyphs[ glyph_index ];
	GLint first_vertex;
	uint32 n=0;
	uint32 start=0, end, count;
	FillMode solid=FILL_SOLID, convex=FILL_CONVEX, concave=FILL_CONCAVE, show_flags=SHOW_FLAGS;
	
	if ( glyph->tris.num_points_total == 0 )
		return;
	
	if ( flags & F_ALL_SOLID )
		convex = concave = show_flags = FILL_SOLID;
	
	/* divided by 2 because each point has both X and Y coordinate */
	first_vertex = ( glyph->tris.points - font->all_points ) / 2;
	
	if ( flags & F_DRAW_SQUARE )
	{
		if ( flags & F_DEBUG_COLORS )
			set_color( 2 );
		set_fill_mode( solid );
		bind_vertex_array( em_sq_vao );
		glDrawArraysInstancedARB( GL_LINE_LOOP, 0, 4, num_instances );
		bind_vertex_array( font->gl_buffers[0] );
	}
	
	if ( flags & F_DRAW_TRIS )
	{
		uint16 n_convex, n_concave, n_solid;
		uint16 *offset;
		GLenum index_type = GL_UINT_TYPE( PointIndex );
		
		offset = (uint16*) ( sizeof( font->all_indices[0] ) * ( glyph->tris.indices - font->all_indices ) );
		
		n_convex = glyph->tris.num_indices_convex;
		n_concave = glyph->tris.num_indices_concave;
		n_solid = glyph->tris.num_indices_solid;
		
		if ( ( flags & F_DEBUG_COLORS ) == 0 )
			set_color( 0 );
		
		if ( ( flags & F_DRAW_CONVEX ) && n_convex ) {
			if ( flags & F_DEBUG_COLORS )
				set_color( 1 );
			set_fill_mode( convex );
			glDrawElementsInstancedBaseVertex( GL_TRIANGLES, n_convex, index_type, offset, num_instances, first_vertex );
		}
		if ( ( flags & F_DRAW_CONCAVE ) && n_concave ) {
			if ( flags & F_DEBUG_COLORS )
				set_color( 2 );
			set_fill_mode( concave );
			glDrawElementsInstancedBaseVertex( GL_TRIANGLES, n_concave, index_type, offset+n_convex, num_instances, first_vertex );
		}
		if ( ( flags & F_DRAW_SOLID ) && n_solid ) {
			if ( flags & F_DEBUG_COLORS )
				set_color( 4 );
			set_fill_mode( solid );
			glDrawElementsInstancedBaseVertex( GL_TRIANGLES, n_solid, index_type, offset+n_convex+n_concave, num_instances, first_vertex );
		}
	}
	
	if ( flags & F_DRAW_OUTLINE )
	{
		/* For each contour... */
		set_fill_mode( solid );
		set_color( 0 );
		do {
			end = glyph->tris.end_points[n++];
			count = end - start + 1;
			glDrawArraysInstancedARB( GL_LINE_LOOP, first_vertex+start, count, num_instances );
			start = end + 1;
		} while( start < glyph->tris.num_points_orig );
	}
	
	if ( flags & F_DRAW_POINTS )
	{
		if ( flags & F_DEBUG_COLORS ) {
			set_fill_mode( show_flags );
		} else {
			set_fill_mode( solid );
			set_color( 0 );
		}
		glDrawArraysInstancedARB( GL_POINTS, first_vertex, glyph->tris.num_points_total, num_instances );
	}
}

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
	
	memcpy( out, transform, sizeof(float)*16 );
	return;
	
	pad_2x2_to_4x4( a, sg_mat );
	mat4_translation( b, -sg_offset[0], -sg_offset[1], 0 );
	mat4_mult( c, b, a );
	mat4_mult( out, c, transform );
	
	/*
	memcpy( out, transform, sizeof(float)*16 );
	(void) sg_mat;
	(void) sg_offset;
	*/
	/* todo */
}

static void draw_composite_glyph( Font *font, void *glyph, uint32 num_instances, float global_transform[16], int flags )
{
	uint32 p, num_parts = *(uint32*) glyph;
	
	for( p=0; p < num_parts; p++ )
	{
		uint32 subglyph_index = *( (uint32*) glyph + 1 + p );
		SimpleGlyph *subglyph = font->glyphs[ subglyph_index ];
		
		float *matrix = (float*) glyph + 1 + num_parts + num_parts * 6;
		float *offset = matrix + 4;
		float m[16];
		
		compute_subglyph_matrix( m, matrix, offset, global_transform );
		
		if ( subglyph->num_parts == 0 ) {
			send_matrix( m );
			draw_instances( font, num_instances, subglyph_index, flags );
		} else {
			/* composite glyph contains other composite glyphs */
			printf( "A rare case of a recursive composite glyph has been discovered!\n" );
			exit(0);
			draw_composite_glyph( font, subglyph, num_instances, m, flags );
		}
	}
}

void draw_glyphs( Font *font, float global_transform[16], uint32 glyph_index, uint32 num_instances, float positions[], int flags )
{
	SimpleGlyph *glyph;
	uint32 start, end, count;
	GLuint ubo;
	
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
		glGenBuffers( 1, &ubo );
		glBindBuffer( GL_UNIFORM_BUFFER, ubo );
		glBufferData( GL_UNIFORM_BUFFER, sizeof(float)*2*num_instances, positions, GL_STATIC_DRAW );
	}
	
	if ( IS_SIMPLE_GLYPH( glyph ) ) {
		/* The glyph consists of only one part
		and every batch draws many instances of that same part.
		Therefore every batch shares the same matrix */
		
		send_matrix( global_transform );
		
		do {
			count = end - start;
			
			if ( USE_UBO ) {
				size_t k = 2 * sizeof( float );
				glBindBufferRange( GL_UNIFORM_BUFFER, uniforms.glyph_positions_ub, ubo, start*k, count*k );
			}
			else
				glUniform2fv( uniforms.glyph_positions, count, positions+2*start );
			
			draw_instances( font, count, glyph_index, flags );
			
			start = end;
			end += BATCH_SIZE;
			
		} while( end <= num_instances );
	}
	else
	{
		/* Composite glyph. */
		
		do {
			count = end - start;
			
			if ( USE_UBO ) {
				size_t k = 2 * sizeof( float );
				glBindBufferRange( GL_UNIFORM_BUFFER, uniforms.glyph_positions_ub, ubo, start*k, count*k );
			}
			else
				glUniform2fv( uniforms.glyph_positions, count, positions+2*start );
			
			draw_composite_glyph( font, glyph, count, global_transform, flags );
			
			start = end;
			end += BATCH_SIZE;
			
		} while( end <= num_instances );
	}
	
	if ( USE_UBO ) {
		glBindBuffer( GL_UNIFORM_BUFFER, 0 );
		glDeleteBuffers( 1, &ubo );
	}
}
