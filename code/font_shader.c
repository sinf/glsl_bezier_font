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

static struct {
	GLint glyph_positions;
	GLint the_matrix;
	GLint the_color;
	GLint fill_mode;
} uniforms = {0};

typedef enum {
	FILL_CONVEX=0,
	FILL_CONCAVE=1,
	FILL_SOLID=2
} FillMode;

static GLuint the_prog = 0;
/* static int use_instancing = 1; */

enum {
	/* Debug graphics switches. Enable/Disable */
	DRAW_EM_SQUARE = 0,
	DRAW_OUTLINE = 1
};
static GLuint em_sq_vao = 0, em_sq_vbo = 0;
static float const em_sq_points[4*2] = {0,0,1,0,1,1,0,1};

int load_font_shaders( void )
{
	/*
	if ( !have_opengl_ext( "GL_ARB_draw_instanced" ) )
	{
		printf( "Required extension 'GL_ARB_draw_instanced' is not supported\n" );
		return 0;
	}
	*/
	
	the_prog = load_shader_prog( "data/bezv.glsl", "data/bezf.glsl" );
	if ( !the_prog )
		return 0;
	
	glUseProgram( the_prog );
	uniforms.the_matrix = glGetUniformLocation( the_prog, "the_matrix" );
	uniforms.the_color = glGetUniformLocation( the_prog, "the_color" );
	uniforms.glyph_positions = glGetUniformLocation( the_prog, "glyph_positions" );
	uniforms.fill_mode = glGetUniformLocation( the_prog, "fill_mode" );
	
	if ( DRAW_EM_SQUARE )
	{
		glGenVertexArrays( 1, &em_sq_vao );
		glGenBuffers( 1, &em_sq_vbo );
		
		glBindVertexArray( em_sq_vao );
		
		glBindBuffer( GL_ARRAY_BUFFER, em_sq_vbo );
		glBufferData( GL_ARRAY_BUFFER, sizeof( em_sq_points ), em_sq_points, GL_STATIC_DRAW );
		
		glEnableVertexAttribArray( 0 );
		glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
		
		glBindVertexArray( 0 );
	}
	
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
	glGenBuffers( 2, buf+1 );
	
	glBindVertexArray( buf[0] );
	
	glBindBuffer( GL_ARRAY_BUFFER, buf[1] );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buf[2] );
	glBufferData( GL_ARRAY_BUFFER, font->all_points_size, font->all_points, GL_STATIC_DRAW );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, font->all_indices_size, font->all_indices, GL_STATIC_DRAW );
	
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
	
	glBindVertexArray( 0 );
	
	/* todo:
	catch out of memory error (although a >32MiB font probably doesn't even exist)
	*/
}

void release_font( Font *font )
{
	printf( "Releasing font GL buffers\n" );
	glDeleteVertexArrays( 1, font->gl_buffers );
	glDeleteBuffers( 2, font->gl_buffers+1 );
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
		{0,1,0,1}
	};
	c %= sizeof( colors ) / sizeof( colors[0] );
	glUniform4fv( uniforms.the_color, 1, colors[c] );
}

void begin_text( Font *font )
{
	glUseProgram( the_prog );
	glBindVertexArray( font->gl_buffers[0] );
	set_color( 0 );
}

void end_text( void )
{
	glBindVertexArray( 0 );
}

static void send_positions( uint32 count, float positions[] )
{
	/* One XY position per glyph instance */
	glUniform2fv( uniforms.glyph_positions, count, positions );
}

static void send_matrix( float matrix[16] )
{
	/* Every instance of the glyph is transformed by the same matrix */
	glUniformMatrix4fv( uniforms.the_matrix, 1, GL_FALSE, matrix );
}

/* Draws only simple glyphs !! */
static void draw_instances( Font *font, uint32 num_instances, uint32 glyph_index )
{
	SimpleGlyph *glyph = font->glyphs[ glyph_index ];
	GLint first_vertex;
	uint32 n=0;
	uint32 start=0, end, count;
	
	if ( glyph->tris.num_points_total == 0 )
		return;
	
	first_vertex = ( glyph->tris.points - font->all_points ) / 2;
	
	if ( DRAW_OUTLINE )
	{
		do {
			end = glyph->tris.end_points[n++];
			count = end - start + 1;
			glDrawArraysInstancedARB( GL_LINE_LOOP, first_vertex+start, count, num_instances );
			start = end + 1;
		} while( start < glyph->tris.num_points_orig );
	}
	
	/* todo */
	
	if ( DRAW_EM_SQUARE )
	{
		glBindVertexArray( em_sq_vao );
		set_color( 2 );
		
		glDrawArraysInstancedARB( GL_LINE_LOOP, 0, 4, num_instances );
		
		glBindVertexArray( font->gl_buffers[0] );
		set_color( 0 );
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

static void draw_composite_glyph( Font *font, void *glyph, uint32 num_instances, float global_transform[16] )
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
			draw_instances( font, num_instances, subglyph_index );
		} else {
			/* composite glyph contains other composite glyphs */
			printf( "A rare case of a recursive composite glyph has been discovered!\n" );
			exit(0);
			draw_composite_glyph( font, subglyph, num_instances, m );
		}
	}
}

void draw_glyphs( Font *font, float global_transform[16], uint32 glyph_index, uint32 num_instances, float positions[] )
{
	enum { BATCH_SIZE = 2024 };
	SimpleGlyph *glyph;
	uint32 start, end, count;
	
	glyph = font->glyphs[ glyph_index ];
	
	if ( !glyph ) {
		/* glyph has no outline or doesn't even exist */
		return;
	}
	
	start = 0;
	end = num_instances % BATCH_SIZE;
	if ( !end )
		end = end < BATCH_SIZE ? num_instances : BATCH_SIZE;
	
	if ( IS_SIMPLE_GLYPH( glyph ) ) {
		/* The glyph consists of only one part
		and every batch draws many instances of that same part.
		Therefore every batch shares the same matrix */
		
		send_matrix( global_transform );
		
		do {
			count = end - start;
			
			send_positions( count, positions + 2 * start );
			draw_instances( font, count, glyph_index );
			
			start = end;
			end += BATCH_SIZE;
			
		} while( end < num_instances );
	}
	else
	{
		/* Composite glyph. */
		
		do {
			count = end - start;
			
			send_positions( count, positions + 2 * start );
			draw_composite_glyph( font, glyph, count, global_transform );
			
			start = end;
			end += BATCH_SIZE;
			
		} while( end < num_instances );
	}
}
