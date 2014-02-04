#ifndef _FONT_SHADER_H
#define _FONT_SHADER_H
#include "font_data.h"

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

int load_font_shaders( void );
void unload_font_shaders( void );
void prepare_font( Font * );
void release_font( Font * );

void begin_text( Font * );
void end_text( void );

/* None of the arguments must be NULL
(even though this function passes NULL to itself) */
void draw_glyphs( Font *font, float global_transform[16], uint32 glyph_index, uint32 num_instances, float positions[] );

#endif
