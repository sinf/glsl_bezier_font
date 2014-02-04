#include <stdio.h>
#include <assert.h>
#include "opengl.h"
#include "shaders.h"

#define MAX_BINDS 32
static GLchar bind_names[MAX_BINDS][64];
static GLuint bind_numbers[MAX_BINDS];
static size_t num_binds = 0;

/* Scans the shader code for the following pattern:
	//%bindattr NAME NUMBER
This is a workaround to shitty drivers that don't support the 'layout' qualifier
*/
static void scan_attr_bindings( const GLchar code[], size_t code_len )
{
	int size_check[ sizeof( char ) == sizeof( GLchar ) ];
	const GLchar key[] = "//%bindattr ";
	const size_t k_end = sizeof( key ) / sizeof( key[0] ) - 2;
	size_t n, k = 0;
	
	num_binds = 0;
	(void) size_check;
	
	for( n=0; n<code_len; n++ )
	{
		if ( code[n] == '\0' )
			break;
		else if ( code[n] == key[k] && ++k == k_end )
		{
			unsigned index = 0;
			if ( sscanf( code+n, " %64s %u", bind_names[num_binds], &index ) == 2 )
			{
				k = 0;
				bind_numbers[num_binds] = index;
				if ( ++num_binds == MAX_BINDS )
					break;
			}
		}
	}
}

static void bind_attributes( GLuint prog )
{
	size_t n;
	for( n=0; n<num_binds; n++ ) {
		printf( "Vertex attribute: %s -> %u\n", bind_names[n], bind_numbers[n] );
		glBindAttribLocation( prog, bind_numbers[n], bind_names[n] );
	}
}

static
GLuint compile_shader_code( const GLchar code[], size_t code_len, GLenum shader_type )
{
	GLuint s;
	GLint ok, slen=code_len;
	
	scan_attr_bindings( code, code_len );
	
	s = glCreateShader( shader_type );
	glShaderSource( s, 1, &code, &slen );
	glCompileShader( s );
	glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
	
	if ( ok == GL_FALSE )
	{
		GLchar buf[4096];
		GLsizei info_len = 0;
		glGetShaderInfoLog( s, sizeof( buf ) / sizeof( buf[0] ) - 1, &info_len, buf );
		glDeleteShader( s );
		buf[ info_len ] = s = 0;
		printf( "Failed to compile. Info log:\n%s\n", buf );
	}
	
	return s;
}

static
GLuint load_shader_file( const char *filename, GLenum shader_type )
{
	GLchar buf[1<<15]; /* 32 KiB */
	size_t len;
	FILE *fp;
	
	printf( "Loading shader '%s'\n", filename );
	
	fp = fopen( filename, "r" );
	if ( !fp ) {
		printf( "Failed to open file\n" );
		return 0;
	}
	
	len = fread( buf, sizeof( buf[0] ), sizeof( buf ) / sizeof( buf[0] ), fp );
	fclose( fp );
	
	return compile_shader_code( buf, len, shader_type );
}

GLuint load_shader_prog( const char *vs_filename, const char *fs_filename )
{
	GLuint p, vs, fs;
	
	p = glCreateProgram();
	
	vs = load_shader_file( vs_filename, GL_VERTEX_SHADER );
	bind_attributes( p );
	
	fs = load_shader_file( fs_filename, GL_FRAGMENT_SHADER );
	bind_attributes( p );
	
	if ( vs && fs )
	{
		GLint ok;
		
		glAttachShader( p, vs );
		glAttachShader( p, fs );
		glLinkProgram( p );
		
		ok = GL_FALSE;
		glGetProgramiv( p, GL_LINK_STATUS, &ok );
		
		if ( p == GL_FALSE ) {
			GLchar info[4096];
			GLsizei len = 0;
			glGetProgramInfoLog( p, sizeof( info ) / sizeof( info[0] ) - 1, &len, info );
			glDeleteProgram( p );
			info[len] = p = 0;
			printf( "Failed to link. Info log:\n%s\n", info );
		} else {
			glUseProgram( p );
		}
	}
	else
	{
		glDeleteProgram( p );
		p = 0;
	}
	
	/*
	If 'p' was created and linked succesfully:
		shaders will be deleted as soon as 'p' is deleted (i.e. at exit)
	otherwise:
		shaders get deleted immediately (or very quickly anyway)
	*/
	if ( vs ) glDeleteShader( vs );
	if ( fs ) glDeleteShader( fs );
	
	return p;
}

GLint locate_uniform( GLuint prog, const char *name )
{
	GLint u = glGetUniformLocation( prog, name );
	if ( u == -1 ) printf( "Warning: uniform '%s' not found\n", name );
	return u;
}
