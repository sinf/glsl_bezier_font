#ifndef _FONT_LAYOUT_H
#define _FONT_LAYOUT_H
#include <stddef.h>
#include "font_data.h"

/*
Inputs:
	A block of ASCII/unicode/wchar_t/HTML text
	Font metrics
Output:
	Array of glyph index/position tuples
*/

typedef struct {
	float *positions; /* glyph position array */
	size_t *glyph_indices; /* one glyph index per batch */
	size_t *batch_len; /* length of each batch */
	size_t batch_count; /* how many batches */
} GlyphBatch;

GlyphBatch *do_simple_layout( Font *font, uint32 *text, size_t text_len, size_t max_line_len, float line_height_scale );
void draw_glyph_batches( Font *font, GlyphBatch *b, float global_transform[16], int draw_flags );
void delete_layout( GlyphBatch *b );

#endif
