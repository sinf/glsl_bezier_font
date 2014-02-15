#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "opengl.h"
#include "gpufont_draw.h"

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
	BATCH_SIZE = 2024, /* 1024 for chinese, 2024 for western??? */
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

int init_font_shader( uint32 linked_compiled_prog )
{
	int size_check[ sizeof( linked_compiled_prog ) == sizeof( GLuint ) ];
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

static void add_glyph_stats( Font *font, SimpleGlyph *glyph, size_t counts[4], size_t limits[4] )
{
	if ( !glyph )
		return;
	if ( IS_SIMPLE_GLYPH( glyph ) ) {
		counts[0] += glyph->tris.num_points_total;
		counts[1] += glyph->tris.num_indices_convex;
		counts[2] += glyph->tris.num_indices_concave;
		counts[3] += glyph->tris.num_indices_solid;
		limits[0] = ( glyph->tris.num_points_total > limits[0] ) ? glyph->tris.num_points_total : limits[0];
		limits[1] = ( glyph->tris.num_indices_convex > limits[1] ) ? glyph->tris.num_indices_convex : limits[1];
		limits[2] = ( glyph->tris.num_indices_concave > limits[2] ) ? glyph->tris.num_indices_concave : limits[2];
		limits[3] = ( glyph->tris.num_indices_solid > limits[3] ) ? glyph->tris.num_indices_solid : limits[3];
	} else {
		size_t k;
		for( k=0; k < ( glyph->num_parts ); k++ )
		{
			uint32 subglyph_id = *( (uint32*) glyph + 1 + k );
			add_glyph_stats( font, font->glyphs[subglyph_id], counts, limits );
		}
	}
}

static void get_average_glyph_stats( Font *font, unsigned avg[4], size_t max[4] )
{
	size_t n, total[4] = {0,0,0,0};
	max[0] = max[1] = max[2] = max[3] = 0;
	for( n=0; n<( font->num_glyphs ); n++ )
		add_glyph_stats( font, font->glyphs[n], total, max );
	for( n=0; n<4; n++ )
		avg[n] = total[n] / font->num_glyphs;
}

void prepare_font( Font *font )
{
	int size_check[ sizeof( GLuint ) == sizeof( font->gl_buffers[0] ) ];
	GLuint *buf = font->gl_buffers;
	unsigned stats[4];
	size_t limits[4];
	
	(void) size_check;
	
	get_average_glyph_stats( font, stats, limits );
	
	printf(
	"Uploading font to GL\n"
	"Average glyph:\n"
	"	Points: %u\n"
	"	Indices: %u/%u/%u, total %u\n"
	"Maximum counts:\n"
	"    Points: %u\n"
	"    Indices: %u/%u/%u\n"
	,
	stats[0], stats[1], stats[2], stats[3],
	stats[1]+stats[2]+stats[3],
	(uint) limits[0], (uint) limits[1], (uint) limits[2], (uint) limits[3]
	);
	
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

void begin_text( Font *font )
{
	glUseProgram( the_prog );
	set_color( 0 );
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
static void draw_instances( Font *font, uint32 num_instances, uint32 glyph_index, int flags )
{
	SimpleGlyph *glyph = font->glyphs[ glyph_index ];
	GLint first_vertex;
	FillMode solid=FILL_SOLID, convex=FILL_CONVEX, concave=FILL_CONCAVE, show_flags=SHOW_FLAGS;
	
	if ( glyph->tris.num_points_total == 0 )
		return;
	
	if ( flags & F_ALL_SOLID )
		convex = concave = show_flags = FILL_SOLID;
	
	/* divided by 2 because each point has both X and Y coordinate */
	first_vertex = ( glyph->tris.points - font->all_points ) / 2;
	
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

static void draw_squares( Font *font, uint32 num_instances, int flags )
{
	if ( flags & F_DEBUG_COLORS )
		set_color( 2 );
	set_fill_mode( FILL_SOLID );
	glBindVertexArray( em_sq_vao );
	glDrawArraysInstancedARB( GL_LINE_LOOP, 0, 4, num_instances );
	glBindVertexArray( font->gl_buffers[0] );
}

void draw_glyphs( Font *font, float global_transform[16], uint32 glyph_index, uint32 num_instances, float positions[], int flags )
{
	SimpleGlyph *glyph;
	uint32 start, end, count;
	GLuint ubo;
	GLuint ub_index;
	
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
	
	if ( IS_SIMPLE_GLYPH( glyph ) ) {
		/* The glyph consists of only one part
		and every batch draws many instances of that same part.
		Therefore every batch shares the same matrix */
		
		send_matrix( global_transform );
		
		do {
			count = end - start;
			
			if ( USE_UBO ) {
				glBindBufferRange( GL_UNIFORM_BUFFER, ub_index, ubo, 0, sizeof( float ) * 2 * count );
				glBufferSubData( GL_UNIFORM_BUFFER, 0, sizeof( float ) * 2 * count, positions + 2*start );
			}
			else
				glUniform2fv( uniforms.glyph_positions, count, positions+2*start );
			
			draw_instances( font, count, glyph_index, flags );
			
			if ( flags & F_DRAW_SQUARE )
				draw_squares( font, count, flags );
			
			start = end;
			end += BATCH_SIZE;
			
		} while( end <= num_instances );
	}
	else
	{
		/* Composite glyph. */
		#if ENABLE_COMPOSITE_GLYPHS
		do {
			count = end - start;
			
			if ( USE_UBO ) {
				glBindBufferRange( GL_UNIFORM_BUFFER, ub_index, ubo, 0, sizeof( float ) * 2 * count );
				glBufferSubData( GL_UNIFORM_BUFFER, 0, sizeof( float ) * 2 * count, positions + 2*start );
			}
			else
				glUniform2fv( uniforms.glyph_positions, count, positions+2*start );
			
			draw_composite_glyph( font, glyph, count, global_transform, flags );
			
			if ( flags & F_DRAW_SQUARE )
				draw_squares( font, count, flags );
			
			start = end;
			end += BATCH_SIZE;
			
		} while( end <= num_instances );
		#endif
	}
	
	if ( USE_UBO ) {
		glBindBuffer( GL_UNIFORM_BUFFER, 0 );
		glDeleteBuffers( 1, &ubo );
	}
}
