#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>
#include "gpufont_data.h"
#include "gpufont_draw.h"
#include "gpufont_layout.h"

typedef struct {
	GlyphIndex glyph;
	int32_t pos_x;
	int32_t line_num;
} TempChar;

static void init_glyph_positions( Font font[1], TempChar chars[], uint32_t const text[], size_t text_len, size_t max_line_len )
{
	int32_t pos_x = 0;
	int32_t line = 0;
	size_t n, column = 0;
	
	for( n=0; n<text_len; n++ )
	{
		GlyphIndex glyph;
		uint32_t cha = text[n];
		
		chars[n].glyph = glyph = get_cmap_entry( font, cha );
		chars[n].pos_x = pos_x - font->hmetrics[ glyph ].lsb;
		chars[n].line_num = line;
		
		if ( cha == '\n' || column == max_line_len ) {
			column = 0;
			pos_x = 0;
			line += 1;
		} else {
			pos_x += font->hmetrics[ glyph ].adv_width;
			column++;
		}
	}
}

static int sort_func( const void *x, const void *y )
{
	TempChar const *a=x, *b=y;
	if ( a->glyph > b->glyph )
		return -1;
	if ( a->glyph < b->glyph )
		return 1;
	return 0;
}

GlyphBatch *do_simple_layout( struct Font *font, uint32_t const *text, size_t text_len, size_t max_line_len, float line_height_scale )
{
	double em_conv = 1.0 / font->units_per_em;
	double line_height = ( font->horz_ascender - font->horz_descender + font->horz_linegap ) * em_conv * line_height_scale;
	TempChar *chars = NULL;
	GlyphBatch *output = NULL;
	size_t n, num_batches, cur_batch, cur_batch_len;
	GlyphIndex prev_glyph;
	
	output = malloc( text_len * sizeof(*output) );
	if ( !output )
		return NULL;
	
	if ( !text_len ) {
		output->positions = NULL;
		output->glyph_indices = NULL;
		output->batch_len = NULL;
		output->batch_count = 0;
		return output;
	}
	
	chars = malloc( text_len * sizeof(*chars) );
	if ( !chars )
		goto error_handler;
	
	output->positions = malloc( text_len * sizeof(float) * 2 );
	if ( !output->positions )
		goto error_handler;
	
	/* Map character codes to glyph indices. Then compute x and y coordinates for each glyph */
	init_glyph_positions( font, chars, text, text_len, max_line_len );
	
	/* Put same glyphs into the same batches */
	qsort( chars, text_len, sizeof(*chars), sort_func );
	
	/* See out how many batches there are */
	prev_glyph = chars[0].glyph;
	num_batches = 1;
	for( n=1; n<text_len; n++ )
	{
		if ( chars[n].glyph != prev_glyph ) {
			prev_glyph = chars[n].glyph;
			num_batches++;
		}
	}
	
	output->glyph_indices = malloc( num_batches * sizeof( size_t ) * 2 );
	output->batch_len = output->glyph_indices + num_batches;
	output->batch_count = num_batches;
	
	if ( !output->glyph_indices ) {
		free( output->positions );
		goto error_handler;
	}
	
	/* Write batch information */
	prev_glyph = chars[0].glyph;
	cur_batch = cur_batch_len = 0;
	for( n=0; n<text_len; n++ )
	{
		float *p = output->positions + 2 * n;
		p[0] = chars[n].pos_x * em_conv;
		p[1] = chars[n].line_num * line_height;
		
		if ( chars[n].glyph != prev_glyph )
		{
			output->batch_len[ cur_batch ] = cur_batch_len;
			output->glyph_indices[ cur_batch ] = prev_glyph;
			cur_batch++;
			
			cur_batch_len = 0;
			prev_glyph = chars[n].glyph;
		}
		
		cur_batch_len++;
	}
	
	/* Initialize the final batch */
	output->batch_len[ cur_batch ] = cur_batch_len;
	output->glyph_indices[ cur_batch ] = prev_glyph;
	
	free( chars );
	return output;
	
error_handler:;
	if ( chars ) free( chars );
	if ( output ) free( output );
	return NULL;
}

void draw_glyph_batches( struct Font *font, GlyphBatch *layout, float global_transform[16], int draw_flags )
{
	float *pos = layout->positions;
	size_t b, num_batches = layout->batch_count;
	for( b=0; b<num_batches; b++ )
	{
		GlyphIndex index = layout->glyph_indices[ b ];
		size_t count = layout->batch_len[ b ];
		draw_glyphs( font, global_transform, index, count, pos, draw_flags );
		pos += 2 * count;
	}
}

void delete_layout( GlyphBatch *b )
{
	if ( b->positions ) free( b->positions );
	if ( b->glyph_indices ) free( b->glyph_indices );
	free( b );
}
