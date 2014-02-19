#ifndef _FONT_SHADER_H
#define _FONT_SHADER_H
#include <stddef.h>

/* Draw flags. Nothing will be drawn if flags are zero */
enum {
	F_DRAW_SQUARE=1, /* Draw unit square (also known as EM square). Causes a performance hit due to additional state changes */
	F_DRAW_POINTS=4, /* Draw points */
	F_DRAW_CURVE=8, /* Draw curves */
	F_DRAW_SOLID=32, /* Draw solid triangles */
	F_DEBUG_COLORS=64, /* Draw different parts of the glyph using certain colors. This flag will overwrite whatever color was specified with set_text_color */
	F_ALL_SOLID=128, /* Draw everything using FILL_SOLID (curves appear as solid triangles) */
	F_DRAW_TRIS=( F_DRAW_CURVE | F_DRAW_SOLID ) /* The "normal" mode */
};

/* don't need bloated opengl header for just this one type */
typedef unsigned GLuint_;

/* Bind/locate shader uniforms and stuff */
int init_font_shader( GLuint_ linked_compiled_font_program );
void deinit_font_shader( void ); /* deletes the program passed to init_font_shader */

struct Font;

/* Create/destroy the VBOs associated with a font */
void prepare_font( struct Font * );
void release_font( struct Font * );

/* Bind/unbind relevant VBOs and VAOs */
void begin_text( struct Font * );
void end_text( void );

/* Actual draw calls. positions_vbo should be a VBO that has data like this: vec2 positions[num_instances] */
void set_text_color( float color[4] );
void bind_glyph_positions( GLuint_ vbo, size_t first ); /* The vbo should have as many vec2's as the num_instances passed to draw_glyphs. 'first' is interpreted as an index to the first vec2 */
void draw_glyphs( struct Font *font, float global_transform[16], size_t glyph_index, size_t num_instances, int flags );

#endif
