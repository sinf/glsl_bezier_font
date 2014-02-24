#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <SDL.h>
#include <wchar.h>
#include "opengl.h"
#include "shaders.h"
#include "matrix.h"
#include "types.h"
#include "microsec.h"

#include "gpufont_data.h"
#include "gpufont_ttf_file.h"
#include "gpufont_layout.h"
#include "gpufont_draw.h"

static enum {
	CULL_OFF=0,
	CULL_CW=1,
	CULL_CCW=2
} cull_mode = CULL_OFF;

/* 0=solid, 1=wire, 2=even more wire */
static int wire_mode = 0;

static const char cam_file[] = "camera.dat";

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
static char *the_novel_filename = "data/artofwar.txt";
static Font the_font;

static int32 the_char_code = 0;
static uint32 the_glyph_index = 0;

enum { GLYPH_ARRAY_S = 128 };
static GlyphCoord the_glyph_coords[GLYPH_ARRAY_S][GLYPH_ARRAY_S][2];
static GLuint the_glyph_coords_vbo = 0;
static GlyphBuffer *my_layout = NULL;

static int lookup_test_char( Font *font, int32 cc )
{
	SimpleGlyph *gh;
	
	the_char_code = cc;
	the_glyph_index = get_cmap_entry( font, cc );
	
	if ( !the_glyph_index )
		return 0;
	
	gh = font->glyphs[ the_glyph_index ];
	
	printf( "U+%04x glyph index %u\n", (uint) cc, (uint) the_glyph_index );
	
	if ( gh ) {
		if ( IS_SIMPLE_GLYPH( gh ) ) {
			printf(
			"- is a simple glyph\n"
			"- total points: %hu\n"
			"- indices/curve: %hu\n"
			"- indices/solid: %hu\n"
			"- indices/total: %hu\n"
			,
			gh->tris.num_points_total,
			gh->tris.num_indices_curve,
			gh->tris.num_indices_solid,
			gh->tris.num_indices_total );
		} else {
			printf( "- is a composite glyph\n- subglyphs: %u\n", (uint) gh->num_parts );
		}
		return 1;
	} else {
		printf( "- no glyph found\n" );
		return 0;
	}
}

static GlyphBuffer *load_text_file( Font *font, const char *filename )
{
	GlyphBuffer *layout = NULL;
	FILE *fp;
	uint32 *text;
	long len;
	
	printf( "Loading text file %s\n", filename );
	fp = fopen( filename, "rb" );
	
	if ( !fp ) {
		printf( "Failed to open file\n" );
		return NULL;
	}
	
	if ( !fseek( fp, 0, SEEK_END )
	&& ( len = ftell( fp ) ) >= 4
	&& !fseek( fp, 0, SEEK_SET ) ) {
		/* Length of the file has been succesfully determined and the file is not empty. */
		text = malloc( len & ~3 );
		len >>= 2;
		if ( text ) {
			/* There was enough system memory available to hold the contents of the file. Yippee! */
			if ( fread( text, 4, len, fp ) == (size_t) len )
			{
				layout = do_simple_layout( font, text, len, 80, -1 );
				if ( !layout )
				{
					printf( "Failed to lay out text\n" );
				}
				else
				{
					printf(
					"Success!\n"
					"Characters in file: %ld\n"
					"do_simple_layout gave us %u batches\n", len, (uint)*(size_t*)layout );
				}
			}
			free( text );
		} else
			printf( "Failed to allocate memory\n" );
	}
	
	fclose( fp );
	return layout;
}

static int load_resources( void )
{
	/* read data files
	precompute things
	create vbos */
	
	int status;
	Uint32 millis;
	GLuint font_prog;
	
	font_prog = load_shader_prog( "data/bezv.glsl", "data/bezf.glsl" );
	if ( !font_prog )
		return 0;
	
	if ( !init_font_shader( font_prog ) )
		return 0;
	
	printf( "Font: '%s'\n", the_font_filename );
	millis = SDL_GetTicks();
	status = load_ttf_file( &the_font, the_font_filename );
	
	if ( status )
	{
		printf( "Failed to load the font (%d)\n", status );
		return 0;
	}
	
	millis = SDL_GetTicks() - millis;
	printf( "Loading the font file took %u milliseconds\n", (uint) millis );
	
	printf( "Total points: %u\n", (unsigned) the_font.total_points );
	printf( "Total indices: %u\n", (unsigned) the_font.total_indices );
	
	printf(
	"Data types\n"
	"sizeof(short) %d\n"
	"sizeof(int) %d\n"
	"sizeof(long) %d\n"
	"sizeof(size_t) %d\n"
	"sizeof(char) %d\n"
	"sizeof(wchar_t) %d\n"
	"sizeof(PointIndex) %d\n"
	"sizeof(PointCoord) %d\n"
	"sizeof(PointFlag) %d\n"
	"sizeof(GlyphIndex) %d\n"
	"sizeof(GlyphCoord) %d\n"
	,
	(int) sizeof(short),
	(int) sizeof(int),
	(int) sizeof(long),
	(int) sizeof(size_t),
	(int) sizeof(char),
	(int) sizeof(wchar_t),
	(int) sizeof(PointIndex),
	(int) sizeof(PointCoord),
	(int) sizeof(PointFlag),
	(int) sizeof(GlyphIndex),
	(int) sizeof(GlyphCoord) );
	
	if ( the_char_code )
	{
		if ( !lookup_test_char( &the_font, the_char_code ) )
		{
			printf( "Glyph for character %u not found\n", (uint) the_char_code );
			return 0;
		}
		else
		{
			long x, y;
			GLuint vbo;
			
			printf(
				"Number of glyph instances (the grid) = %ux%u = %u\n"
				"Batches = %u\n",
				GLYPH_ARRAY_S, GLYPH_ARRAY_S,
				GLYPH_ARRAY_S*GLYPH_ARRAY_S,
				(GLYPH_ARRAY_S*GLYPH_ARRAY_S+2023)/2024 );
			
			for( y=0; y<GLYPH_ARRAY_S; y++ )
			{
				for( x=0; x<GLYPH_ARRAY_S; x++ )
				{
					the_glyph_coords[y][x][0] = ( x - GLYPH_ARRAY_S / 2 ) * the_font.units_per_em;
					the_glyph_coords[y][x][1] = ( y - GLYPH_ARRAY_S / 2 ) * the_font.units_per_em;
				}
			}
			
			glGenBuffers( 1, &vbo );
			glBindBuffer( GL_ARRAY_BUFFER, vbo );
			the_glyph_coords_vbo = vbo;
			glBufferData( GL_ARRAY_BUFFER, sizeof( the_glyph_coords ), the_glyph_coords, GL_STATIC_DRAW );
		}
	}
	else
	{
		my_layout = load_text_file( &the_font, the_novel_filename );
		if ( !my_layout )
			return 0;
	}
	
	prepare_font( &the_font );
	return 1;
}

static void free_resources( void )
{
	/* todo:
	free memory, close files,
	release GPU buffers etc.. */
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

/* supposed to be in main() or anywhere else but here */
static float cam_x=0, cam_y=-2, cam_z=1.82;
static float cam_yaw=PI, cam_pitch=PI;
static int win_w = 800;
static int win_h = 600;

static void get_micros_per_frame( unsigned long avg_time[1], unsigned long delta_time )
{
	/* how many frames to average */
	enum { SAMPLES = 20 };
	static unsigned long accum_frames=0, accum_time=0;
	
	accum_time += delta_time;
	accum_frames++;
	
	if ( accum_frames == SAMPLES ) {
		avg_time[0] = accum_time / accum_frames;
		accum_frames = 0;
		accum_time = 0;
	}
}

static void repaint( void )
{
	static float c_white[4] = {1,1,1,1};
	static float c_green[4] = {0,1,0,1};
	int glyph_draw_flags = 0;
	
	static uint64_t avg_frame_micros = 0;
	unsigned long frame_start_time = get_microsec();
	
	switch( wire_mode )
	{
		case 0:
			glyph_draw_flags = F_DRAW_TRIS;
			break;
		case 1:
			glyph_draw_flags = F_DRAW_TRIS | F_DEBUG_COLORS | F_DRAW_POINTS;
			break;
		case 2:
			glyph_draw_flags = F_DRAW_POINTS | F_DEBUG_COLORS;
			break;
		default:
			assert(0);
	}
	
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	
	glUseProgram( prog );
	glUniformMatrix4fv( mvp_loc, 1, GL_FALSE, mvp );
	
	if ( 0 )
	{
		/* Draw the floor */
		glBindVertexArray( floor_buffers[0] );
		glVertexAttrib3f( 1, 0, 0, 1 );
		glVertexAttrib3f( 2, 0.25, 0.25, 0.25 );
		glDrawArrays( GL_LINES, 0, num_floor_verts );
		glBindVertexArray( 0 );
	}
	
	/* Draw other stuff */
	
	if ( cull_mode != CULL_OFF ) {
		glCullFace( GL_FRONT );
		glFrontFace( cull_mode == CULL_CW ? GL_CW : GL_CCW );
		glEnable( GL_CULL_FACE );
	}
	
	glPolygonOffset( -1, 1 );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glEnable( GL_POLYGON_OFFSET_LINE );
	glEnable( GL_POLYGON_OFFSET_POINT );
	glEnable( GL_POLYGON_SMOOTH );
	glEnable( GL_LINE_SMOOTH );
	glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );
	glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
	glPointSize( 4 ); /* obsolete but works anyway */
	
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_ALPHA_TEST );
	glAlphaFunc( GL_GREATER, 0.00001 );
	
	begin_text( &the_font );
	
	if ( the_char_code )
	{
		uint32 g = the_glyph_index;
		
		/* draw a huge array of the same glyph */
		set_text_color( c_white );
		bind_glyph_positions( the_glyph_coords_vbo, 0 );
		
		draw_glyphs( &the_font, mvp, g,
			GLYPH_ARRAY_S*GLYPH_ARRAY_S,
			wire_mode ? glyph_draw_flags : F_DRAW_TRIS );
		
		if ( wire_mode == 1 )
		{
			glDisable( GL_DEPTH_TEST );
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			
			set_text_color( c_white );
			draw_glyphs( &the_font, mvp, g,
				GLYPH_ARRAY_S*GLYPH_ARRAY_S,
				F_DRAW_TRIS | F_ALL_SOLID );
			
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glEnable( GL_DEPTH_TEST );
		}
	}
	else if ( my_layout )
	{
		/* Show a text file */
		
		set_text_color( c_white );
		draw_glyph_buffer( &the_font, my_layout, mvp, glyph_draw_flags );
		
		if ( wire_mode == 1 )
		{
			glDisable( GL_DEPTH_TEST );
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			set_text_color( c_white );
			draw_glyph_buffer( &the_font, my_layout, mvp, F_DRAW_TRIS | F_ALL_SOLID );
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glEnable( GL_DEPTH_TEST );
		}
	}
	
	/* Draw a line of text that changes in real time */
	if ( sizeof( wchar_t ) == 4 )
	{
		wchar_t s[300];
		int len;
		
		len = swprintf( s, sizeof( s ) / sizeof( s[0] ),
			L"  Pos (%.3f, %.3f, %.3f) Yaw %.3f Pitch %.3f\nAverage frame time: %lu µs (%.1lf fps)",
			cam_x, cam_y, cam_z, cam_yaw, cam_pitch, avg_frame_micros, 1000000.0 / avg_frame_micros );
		
		if ( len > 0 )
		{
			float scale = 50;
			float matrix[16];
			
			memcpy( matrix, &MAT4_IDENTITY, sizeof(matrix) );
			
			matrix[0] = scale / win_w;
			if ( 1 ) matrix[4] = matrix[0] * 0.5; /* makes the line slightly skewed */
			matrix[5] = scale / win_h;
			matrix[12] = -0.99;
			matrix[13] = -0.95;
			
			set_text_color( c_green );
			draw_text_live( &the_font, (uint32_t*) s, len, -1, 1, matrix, F_DRAW_TRIS );
		}
	}
	
	end_text();
	
	glPointSize( 1 );
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_POLYGON_OFFSET_LINE );
	glDisable( GL_POLYGON_OFFSET_POINT );
	glDisable( GL_BLEND );
	glDisable( GL_ALPHA_TEST );
	glDisable( GL_POLYGON_SMOOTH );
	glDisable( GL_LINE_SMOOTH );
	
	if ( cull_mode != CULL_OFF )
		glDisable( GL_CULL_FACE );
	
	glFinish();
	get_micros_per_frame( &avg_frame_micros, get_microsec() - frame_start_time );
}

static void update_viewport( int w, int h )
{
	float near = 0.005;
	float far = 1000.0;
	
	glViewport( 0, 0, w, h );
	mat4_persp( projection, RADIANS( 65.0f ), w / (float) h, near, far );
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

static void help_screen_exit( void )
{
	printf(
	"---- Valid command line arguments ----\n"
	"-f FILENAME    Load font from given TTF file\n"
	"-t FILENAME    Load text from given file. The file must be in UTF-32 encoding\n"
	"-c NUMBER      This character code will be displayed in a grid pattern\n"
	"-h             Print this information and exit\n"
	);
	printf(
	"\n---- Camera controls ----\n"
	"Arrow keys rotate the camera\n"
	"A,W,S,D move the camera\n"
	"Mouse wheel moves the camera along Y axis\n"
	"Holding LCTRL, SHIFT, or ALT multiply speed by factors 0.05, 10 and 100\n"
	"T: Save camera position to camera.dat\n"
	"M: Toggle wireframe modes (off,lines,points)\n"
	"N: Toggle back face fulling (off,cw,ccw)\n"
	"\n"
	);
	exit( 0 );
}

static void parse_args( int argc, char **argv )
{
	int end = argc - 1;
	int n;
	
	if ( argc == 2 )
		help_screen_exit();
	
	for( n=1; n<end; n++ )
	{
		printf( "%s\n", argv[n] );
		
		if ( !strcmp( argv[n], "-f" ) )
			the_font_filename = argv[++n];
		else if ( !strcmp( argv[n], "-t" ) )
			the_novel_filename = argv[++n];
		else if ( !strcmp( argv[n], "-c" ) )
			the_char_code = atoi( argv[++n] );
		else
			help_screen_exit();
	}
}

int main( int argc, char *argv[] )
{
	const Uint32 video_flags = SDL_OPENGL | SDL_RESIZABLE | SDL_NOFRAME;
	FILE *cam_fp;
	Uint32 prev_ticks;
	int msaa_enabled = 1;
	int msaa_samples = 8;
	
	parse_args( argc, argv );
	
	printf( "** Initializing SDL\n" );
	if ( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
		printf( "Failed to init SDL: %s\n", SDL_GetError() );
		return 1;
	}
	
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	/* SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 0 ); disables vsync */
	if ( msaa_enabled ) {
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, msaa_samples );
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
	
	if ( msaa_enabled )
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
	glDepthFunc( GL_LEQUAL );
	glEnable( GL_DEPTH_TEST );
	glClearColor( 0, 0, 0, 0 );
	
	printf( "Window size: %dx%d\nAntialias: %s\nFloor vertices: %u\n", win_w, win_h, msaa_enabled ? "on" : "off", (uint) num_floor_verts );
	
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
	
	printf( "** Entering main loop\n" );
	prev_ticks = SDL_GetTicks();
	
	for( ;; )
	{
		SDL_Event e;
		Uint32 now_ticks, ticks_passed;
		double timestep;
		
		now_ticks = SDL_GetTicks();
		ticks_passed = now_ticks - prev_ticks;
		timestep = ticks_passed / 1000.0;
		prev_ticks = now_ticks;
		
		while( SDL_PollEvent(&e) )
		{
			switch( e.type )
			{
				case SDL_KEYDOWN:
					switch( e.key.keysym.sym )
					{
						case SDLK_n:
							printf( "Cull mode: %s\n",
								( cull_mode = ( cull_mode + 1 ) % 3 )
								? ( cull_mode == CULL_CW ? "clockwise" : "counterclockwise" )
								: "disabled" );
							break;
						case SDLK_m:
							wire_mode = ( wire_mode + 1 ) % 3;
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
						default:
							break;
					}
					break;
				
				case SDL_MOUSEBUTTONDOWN:
					{
						float t = 0;
						
						if ( e.button.button == SDL_BUTTON_WHEELUP )
							t = -1;
						else if ( e.button.button == SDL_BUTTON_WHEELDOWN )
							t = 1;
						
						if ( SDL_GetModState() & KMOD_CTRL )
							t *= 0.1;
						else
							t *= 5;
						
						cam_y += t;
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
			else if ( SDL_GetModState() & KMOD_LCTRL ) {
				r *= 0.05;
				t *= 0.05;
			} else if ( SDL_GetModState() & KMOD_ALT )
				t *= 100;
			
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
			
			cam_yaw = fmod( 2 * PI + cam_yaw, 2 * PI );
			cam_pitch = fmod( 2 * PI + cam_pitch, 2 * PI );
			
			u[0]=tx;
			u[1]=ty;
			u[2]=tz;
			u[3]=0;
			mat4_mult_vec( v, modelview, u );
			cam_x += v[0];
			cam_y += v[1];
			cam_z += v[2];
		}
		
		repaint();
		SDL_GL_SwapBuffers();
	}
	
	quit();
	return 0;
}
