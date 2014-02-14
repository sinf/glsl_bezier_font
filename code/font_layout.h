#ifndef _FONT_LAYOUT_H
#define _FONT_LAYOUT_H
#include "types.h"

/*
Inputs:
	A block of ASCII/unicode/wchar_t/HTML text
	Font metrics
Output:
	Array of glyph index/position tuples
*/

typedef struct {
	float *positions;
	uint32 glyph; /* glyph index */
	uint32 count; /* how many instances */
} GlyphBatch;

GlyphBatch *do_simple_layout( Font *font, uint32 *text, size_t text_len, size_t max_line_len, float line_height_multiplier );
void draw_glyph_batches( Font *font, GlyphBatch *b, float global_transform[16], int draw_flags );

#endif
