#ifndef _SHADERS_H
#define _SHADERS_H
#include "opengl.h"

#define have_opengl_ext glewIsSupported

/*
GLuint compile_shader_code( const GLchar code[], size_t code_len, GLenum shader_type );
GLuint load_shader_file( const char *filename, GLenum shader_type );
*/
GLuint load_shader_prog( const char *vs_filename, const char *fs_filename );
GLint locate_uniform( GLuint prog, const char *name );

#endif
