#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <SDL.h>
#include "opengl.h"
#include "shaders.h"
#include "matrix.h"

#include "font_data.h"
#include "font_file.h"
#include "font_layout.h"
#include "font_shader.h"

/* todo:
Draw one huge glyph
Draw a block of text
Draw asian characters
Draw transformed/animated text
Move all font loading stuff into a self-contained library
*/

static const char cam_file[] = "camera.dat";
static int cull_mode = 0; /* 0=off, 1=back, 2=front */
static int wire_mode = 0; /* 0=solid, 1=wire */

/* the shader program */
static GLuint prog = 0;

/* grid floor VBO & IBO */
static size_t num_floor_verts = 0;
static GLuint floor_buffers[2] = {0,0}; /* vao, then vbo */

/* projection matrix */
static Mat4 projection = {0};
static Mat4 modelview = {0};
static Mat4 mvp;
static GLint mvp_loc;

static char *the_font_filename = "/usr/share/fonts/truetype/droid/DroidSans.ttf";
static Font the_font;

static const uint32 TEST_CHAR_CODE = L'春'; /* L'å' */

static void test_char( Font *font, uint32 ch )
{
	uint32 g;
	SimpleGlyph *gh;
	
	g = get_cmap_entry( font, ch );
	gh = font->glyphs[ g ];
	
	printf( "U+%04x glyph index %u\n", ch, (uint) g );
	printf( "- gh %s\n", gh ? "exists" : "is null" );
	
	if ( gh ) {
		if ( IS_SIMPLE_GLYPH( gh ) ) {
			printf( "- is a simple glyph\n- total points: %u\n", gh->tris.num_points_total );
		} else {
			printf( "- is a composite glyph\n- subglyphs: %u\n", gh->num_parts );
		}
	}
}

static int load_resources( void )
{
	/* read data files
	precompute things
	create vbos */
	
	int status;
	
	if ( !load_font_shaders() )
		return 0;
	
	printf( "Font: '%s'\n", the_font_filename );
	status = load_truetype( &the_font, the_font_filename );
	
	if ( status )
	{
		printf( "Failed to load the font (%d)\n", status );
		return 0;
	}
	
	printf( "Glyph indices for characters A, M, P, B, j, \xc3\xa5: %u %u %u %u %u %u\n",
		get_cmap_entry( &the_font, 'A' ),
		get_cmap_entry( &the_font, 'M' ),
		get_cmap_entry( &the_font, 'P' ),
		get_cmap_entry( &the_font, 'B' ),
		get_cmap_entry( &the_font, 'j' ),
		get_cmap_entry( &the_font, 0xe5 ) );
	
	test_char( &the_font, TEST_CHAR_CODE );
	#if 0
	printf( "Done\n" );
	exit( 0 );
	#endif
	
	prepare_font( &the_font );
	
	return 1;
}

static void free_resources( void )
{
	/* free memory, close files,
	release GPU buffers etc.. */
}

static void reset_simulation( void )
{
	/* set initial simulation state */
}

static void update_simulation( double dt )
{
	/* advance simulation */
	(void) dt;
}

static void update_vbos( void )
{
	/* update VBO's used to render the scene */
}

static size_t generate_grid_floor( GLuint vao, GLuint vbo, size_t horz_lines, size_t vert_lines, float x0, float y0, float x1, float y1 )
{
	GLfloat verts[1024][2];
	float dx = ( x1 - x0 ) / ( vert_lines - 1 );
	float dy = ( y1 - y0 ) / ( horz_lines - 1 );
	
	size_t n, num_verts = 2 * ( horz_lines + vert_lines );
	assert( num_verts <= sizeof( verts ) / sizeof( verts[0] ) );
	
	for( n=0; n<horz_lines; n++ )
	{
		float y = y0 + n * dy;
		verts[2*n][0] = x0;
		verts[2*n][1] = y;
		verts[2*n+1][0] = x1;
		verts[2*n+1][1] = y;
	}
	for( n=0; n<vert_lines; n++ )
	{
		float x = x0 + n * dx;
		verts[2*horz_lines+2*n][0] = x;
		verts[2*horz_lines+2*n][1] = y0;
		verts[2*horz_lines+2*n+1][0] = x;
		verts[2*horz_lines+2*n+1][1] = y1;
	}
	
	glBindVertexArray( vao );
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	
	glBufferData( GL_ARRAY_BUFFER, sizeof( verts[0] ) * num_verts, verts, GL_STATIC_DRAW );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*2, 0 );
	
	glBindVertexArray( 0 );
	
	return num_verts;
}

static void repaint( void )
{
	enum { GLYPH_ARRAY_S = 128 };
	static float w_pos[GLYPH_ARRAY_S][GLYPH_ARRAY_S][2];
	
	float glyph_pos[2*4] = {
		-2,.8,
		-1,.8,
		0,.8,
		1,.8
	};
	float text_matr[16] = {
		0.4, 0, 0, 0,
		0, 0.4 * ( 800 / 600.0 ), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	static uint32 cc = ~0;
	
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	
	glUseProgram( prog );
	glUniformMatrix4fv( mvp_loc, 1, GL_FALSE, mvp );
	
	/* Draw the floor */
	glBindVertexArray( floor_buffers[0] );
	glVertexAttrib3f( 1, 0, 0, 1 );
	glVertexAttrib3f( 2, 0.25, 0.25, 0.25 );
	glDrawArrays( GL_LINES, 0, num_floor_verts );
	glBindVertexArray( 0 );
	
	/* Draw other stuff */
	
	if ( !~cc )
	{
		int y, x;
		
		cc = get_cmap_entry( &the_font, TEST_CHAR_CODE );
		
		if ( cc == 0 )
		{
			cc = get_cmap_entry( &the_font, L'å' );
			if ( cc == 0 )
				cc = get_cmap_entry( &the_font, L'a' );
		}
		
		printf(
		"The character code = %u\n"
		"Number of instances (the grid) = %ux%u = %u\n"
		"Batches = %u\n",
		cc,
		GLYPH_ARRAY_S, GLYPH_ARRAY_S,
		GLYPH_ARRAY_S*GLYPH_ARRAY_S,
		(GLYPH_ARRAY_S*GLYPH_ARRAY_S+2023)/2024 );
		
		for( y=0; y<GLYPH_ARRAY_S; y++ )
		{
			for( x=0; x<GLYPH_ARRAY_S; x++ )
			{
				w_pos[y][x][0] = x - GLYPH_ARRAY_S / 2;
				w_pos[y][x][1] = y - GLYPH_ARRAY_S / 2;
			}
		}
	}
	
	glPolygonOffset( -1, 1 );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glEnable( GL_POLYGON_OFFSET_LINE );
	glEnable( GL_POLYGON_OFFSET_POINT );
	
	begin_text( &the_font );
	draw_glyphs( &the_font, text_matr, cc, 4, glyph_pos );
	draw_glyphs( &the_font, mvp, cc, GLYPH_ARRAY_S*GLYPH_ARRAY_S, &w_pos[0][0][0] );
	end_text();
	
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_POLYGON_OFFSET_LINE );
	glDisable( GL_POLYGON_OFFSET_POINT );
}

static void update_viewport( int w, int h )
{
	glViewport( 0, 0, w, h );
	mat4_persp( projection, RADIANS( 65.0f ), w / (float) h, 0.01f, 150.0f );
}

__attribute__((noreturn))
static void quit( void )
{
	printf( "** Program end sequence initiated\nReleasing graphics resources...\n" );
	
	glUseProgram( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	
	if ( floor_buffers[0] ) {
		glDeleteVertexArrays( 1, floor_buffers );
		glDeleteBuffers( 2, floor_buffers+1 );
	}
	
	if ( prog )
		glDeleteProgram( prog );
	
	free_resources();
	
	printf( "** Deinitializing SDL...\n" );
	SDL_Delay( 100 );
	SDL_Quit();
	
	printf( "** Calling exit\n" );
	exit(0);
}

int main( int argc, char *argv[] )
{	
	const Uint32 video_flags = SDL_OPENGL | SDL_RESIZABLE;
	
	float cam_x=0, cam_y=-2, cam_z=1.82;
	float cam_yaw=0, cam_pitch=2.2;
	
	FILE *cam_fp;
	
	int win_w = 800;
	int win_h = 600;
	
	Uint32 prev_ticks;
	int simulating = 0;
	int fast_forward = 0;
	
	int enable_aa = 0;
	int aa_samples = 8;
	
	if ( argc > 1 ) {
		the_font_filename = argv[1];
	}
	
	printf( "** Initializing SDL\n" );
	if ( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
		printf( "Failed to init SDL: %s\n", SDL_GetError() );
		return 1;
	}
	
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	/* SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 0 ); disables vsync */
	if ( enable_aa ) {
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, aa_samples );
	}
	
	if ( !SDL_SetVideoMode( win_w, win_h, 0, video_flags ) ) {
		printf( "Failed to set %dx%d video mode: %s\n", win_w, win_h, SDL_GetError() );
		SDL_Quit();
		return 1;
	}
	
	printf( "** Initializing GLEW\n" );
	glewExperimental = 1;
	glewInit();
	
	printf( "** Setting up OpenGL things\n" );
	
	if ( enable_aa )
		glEnable( GL_MULTISAMPLE );
	
	prog = load_shader_prog( "data/vert.glsl", "data/frag.glsl" );
	
	if ( !prog )
	{
		printf( "Failed to load shader program\n" );
		SDL_Quit();
		return 1;
	}
	
	mvp_loc = locate_uniform( prog, "mvp" );
	update_viewport( win_w, win_h );
	
	glGenVertexArrays( 1, floor_buffers );
	glGenBuffers( 1, floor_buffers+1 );
	num_floor_verts = generate_grid_floor( floor_buffers[0], floor_buffers[1], 81, 81, -40, -40, 40, 40 );
	
	glClearDepth( 1.0f );
	glDepthFunc( GL_LESS );
	glEnable( GL_DEPTH_TEST );
	
	printf( "Window size: %dx%d\nAntialias: %s\nFloor vertices: %u\n", win_w, win_h, enable_aa ? "on" : "off", (uint) num_floor_verts );
	
	printf( "** Reading camera position from %s\n", cam_file );
	if (( cam_fp = fopen( cam_file, "rb" ) ))
	{
		float data[5] = {0};
		size_t n_elem = sizeof( data ) / sizeof( data[0] );
		if ( fread( data, sizeof( data[0] ), n_elem, cam_fp ) == n_elem )
		{
			cam_x = data[0];
			cam_y = data[1];
			cam_z = data[2];
			cam_yaw = data[3];
			cam_pitch = data[4];
			printf( "^ Ok\n" );
		}
		fclose( cam_fp );
	}
	
	printf( "** Initializing scene\n" );
	if ( !load_resources() ) {
		SDL_Quit();
		return 1;
	}
	update_vbos();
	
	printf( "** Entering main loop\n" );
	prev_ticks = SDL_GetTicks();
	
	for( ;; )
	{
		SDL_Event e;
		Uint32 now_ticks;
		double timestep;
		
		now_ticks = SDL_GetTicks();
		timestep = ( now_ticks - prev_ticks ) / 1000.0;
		prev_ticks = now_ticks;
		
		while( SDL_PollEvent(&e) )
		{
			switch( e.type )
			{
				case SDL_KEYDOWN:
					switch( e.key.keysym.sym )
					{
						case SDLK_SPACE:
							printf( (( simulating = !simulating )) ? "** Unpaused\n" : "** Paused\n" );
							break;
						case SDLK_c:
							update_simulation( 1.0 / 50 );
							update_vbos();
							break;
						case SDLK_r:
							reset_simulation();
							update_vbos();
							break;
						case SDLK_j:
							printf( "%.4f %.4f %.4f | %.4f %.4f\n", cam_x, cam_y, cam_z, cam_yaw, cam_pitch );
							break;
						case SDLK_n:
							printf( "Cull mode: %s\n", ( cull_mode = ( cull_mode + 1 ) % 3 ) ? ( cull_mode == 1 ? "back" : "front" ) : "off" );
							break;
						case SDLK_m:
							wire_mode = !wire_mode;
							break;
						case SDLK_ESCAPE:
							quit();
						case SDLK_t:
							if (( cam_fp = fopen( cam_file, "wb" ) ))
							{
								float data[5];
								data[0] = cam_x;
								data[1] = cam_y;
								data[2] = cam_z;
								data[3] = cam_yaw;
								data[4] = cam_pitch;
								fwrite( data, sizeof( data[0] ), sizeof( data ) / sizeof( data[0] ), cam_fp );
								fclose( cam_fp );
								printf( "Camera position saved\n" );
							}
							break;
						case SDLK_v:
							fast_forward = !fast_forward;
							break;
						default:
							break;
					}
					break;
				
				case SDL_QUIT:
					quit();
				
				case SDL_VIDEORESIZE:
					win_w = e.resize.w;
					win_h = e.resize.h;
					SDL_SetVideoMode( win_w, win_h, 0, video_flags );
					update_viewport( win_w, win_h );
					break;
				
				default:
					break;
			}
		}
		
		if ( 1 )
		{
			float a[16], b[16], c[16], d[16];
			
			mat4_translation( a, cam_x, cam_y, cam_z );
			mat4_rotation_z( b, cam_yaw );
			mat4_rotation_x( c, cam_pitch );
			
			mat4_mult( d, b, a );
			mat4_mult( modelview, c, d );
			mat4_mult( mvp, projection, modelview );
		}
		
		if ( 1 )
		{
			Uint8 *keys = SDL_GetKeyState( NULL );
			float tx=0, ty=0, tz=0;
			float t = 3.0f * timestep;
			float r = 2.0f * timestep;
			float u[4], v[4];
			
			if ( SDL_GetModState() & KMOD_LSHIFT )
				t *= 10;
			
			if ( keys[SDLK_a] ) tx -= t;
			if ( keys[SDLK_d] ) tx += t;
			
			if ( keys[SDLK_q] ) ty += t;
			if ( keys[SDLK_e] ) ty -= t;
			
			if ( keys[SDLK_w] ) tz += t;
			if ( keys[SDLK_s] ) tz -= t;
			
			if ( keys[SDLK_LEFT] ) cam_yaw -= r;
			if ( keys[SDLK_RIGHT] ) cam_yaw += r;
			if ( keys[SDLK_UP] ) cam_pitch -= r;
			if ( keys[SDLK_DOWN] ) cam_pitch += r;
			
			u[0]=tx;
			u[1]=ty;
			u[2]=tz;
			u[3]=0;
			mat4_mult_vec( v, modelview, u );
			cam_x += v[0];
			cam_y += v[1];
			cam_z += v[2];
		}
		
		if ( simulating )
		{
			int n, loops = fast_forward ? 5 : 1;
			for( n=0; n<loops; n++ )
				update_simulation( timestep );
			update_vbos();
		}
		
		repaint();
		SDL_GL_SwapBuffers();
		
		if ( 0 )
			SDL_Delay( 20 );
	}
	
	quit();
	return 0;
}
