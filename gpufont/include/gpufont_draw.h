#ifndef _FONT_SHADER_H
#define _FONT_SHADER_H
#include <stddef.h>

/*

Inputs:
	glyph contours
Outputs:
	preprocessed glyph data

1. Triangulate + subdivide glyph countours
2. Generate UV coordinates
3. Create VBOs

Inputs:
	array of glyph index/position tuples
Outputs:
	none (glyphs get rasterized by OpenGL)

4. Sort glyphs by indices
5. Upload uniforms
6. Draw

*/

/* draw flags */
enum {
	F_DRAW_SQUARE=1, /* draw unit square (also known as EM square) */
	F_DRAW_POINTS=4, /* draw points */
	F_DRAW_CONVEX=8, /* draw convex curves (triangles) */
	F_DRAW_CONCAVE=16, /* draw concave curves (triangles) */
	F_DRAW_SOLID=32, /* draw solid triangles */
	F_DEBUG_COLORS=64,
	F_ALL_SOLID=128, /* draw everything using FILL_SOLID */
	F_DRAW_TRIS=( F_DRAW_CONVEX | F_DRAW_CONCAVE | F_DRAW_SOLID )
};

typedef unsigned GLuint_; /* don't need bloated opengl header for just this one type */

int init_font_shader( GLuint_ linked_compiled_font_program );
void deinit_font_shader( void ); /* deletes the program passed to init_font_shader */

void prepare_font( struct Font * );
void release_font( struct Font * );

void begin_text( struct Font * );
void end_text( void );

/* None of the arguments must be NULL
(even though this function passes NULL to itself) */
void draw_glyphs( struct Font *font, float global_transform[16], size_t glyph_index, size_t num_instances, float positions[], int flags );

#endif
