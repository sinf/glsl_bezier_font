#ifndef _FONT_LAYOUT_H
#define _FONT_LAYOUT_H
#include <stddef.h>
#include <stdint.h>
#include "gpufont_data.h"

/*
Inputs:
	A block of ASCII/unicode/wchar_t/HTML text
	Font metrics
Output:
	Array of glyph index/position tuples
*/

typedef struct {
	float *positions; /* glyph position array */
	GlyphIndex *glyph_indices; /* one glyph index per batch */
	size_t *batch_len; /* length of each batch */
	size_t batch_count; /* how many batches */
} GlyphBatch;

struct Font;

/* if max_line_len < 0 then text is not wrapped at all */
GlyphBatch *do_simple_layout( struct Font *font, uint32_t const *text, size_t text_len, int max_line_len, float line_height_scale );
void draw_glyph_batches( struct Font *font, GlyphBatch *b, float global_transform[16], int draw_flags );
void delete_layout( GlyphBatch *b );

/* Suitable for drawing short strings that are updated in real time. Will truncate string if its too long to fit in stack-allocated buffers */
void draw_text_live( struct Font *font, uint32_t const *text, size_t text_len, int max_line_len, float line_height_scale, float global_transform[16], int draw_flags );

#endif
