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

static size_t init_glyph_positions( Font font[1], TempChar chars[], uint32_t const text[], size_t text_len, int max_line_len )
{
	int32_t pos_x = 0;
	int32_t line = 0;
	size_t n = 0;
	int column = 0;
	size_t num_out = 0;
	
	for( n=0; n<text_len; n++ )
	{
		GlyphIndex glyph;
		uint32_t cha = text[n];
		int is_newline = 0;
		int is_visible = 1;
		
		glyph = get_cmap_entry( font, cha );
		chars[num_out].glyph = glyph;
		chars[num_out].pos_x = pos_x - font->hmetrics[ glyph ].lsb;
		chars[num_out].line_num = line;
		
		if ( cha == '\n' ) {
			is_newline = 1;
			is_visible = 0;
		} else if ( column == max_line_len ) {
			is_newline = 1;
		}
		
		if ( is_newline ) {
			column = 0;
			pos_x = 0;
			line += 1;
		} else if ( is_visible ) {
			pos_x += font->hmetrics[ glyph ].adv_width;
			column++;
		}
		
		num_out += is_visible;
	}
	
	return num_out;
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

/* fields of 'output':
"positions" MUST have been allocated to at least 2*text_len floats
"glyph_indices" will be allocated if it hasn't been already
"batch_len" will also be allocated if necessary, but "batch_len" must be NULL if "glyph_indices" is also NULL
"glyph_indices" and "batch_len" must be in the same contiguous block of memory one after another
*/
static int do_simple_layout_internal( struct Font *font, uint32_t const *text, size_t text_len, int max_line_len, float line_height_scale, GlyphBatch *output, TempChar *chars )
{
	double em_conv = 1.0 / font->units_per_em;
	double line_height = ( font->horz_ascender - font->horz_descender + font->horz_linegap ) * em_conv * line_height_scale;
	size_t n, num_batches, cur_batch, cur_batch_len;
	GlyphIndex prev_glyph;
	
	assert( text_len > 0 );
	assert( output );
	assert( output->positions );
	assert( chars );
	
	/* Map character codes to glyph indices. Then compute x and y coordinates for each glyph */
	text_len = init_glyph_positions( font, chars, text, text_len, max_line_len );
	
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
	
	if ( !output->glyph_indices )
	{
		output->glyph_indices = malloc( num_batches * ( sizeof( output->glyph_indices[0] ) + sizeof( size_t ) ) );
		output->batch_len = (size_t*)( output->glyph_indices + num_batches );
		
		if ( !output->glyph_indices )
			return 0;
	}
	
	assert( output->batch_len );
	output->batch_count = num_batches;
	
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
	
	return 1;
}

#define MAX_LIVE_LEN 320
void draw_text_live( struct Font *font, uint32_t const *text, size_t text_len, int max_line_len, float line_height_scale, float global_transform[16], int draw_flags )
{
	GlyphBatch batch;
	GlyphIndex glyph_indices[MAX_LIVE_LEN];
	size_t batch_len[MAX_LIVE_LEN];
	float positions[2*MAX_LIVE_LEN];
	TempChar chars[MAX_LIVE_LEN];
	
	if ( !text_len )
		return;
	
	if ( text_len > MAX_LIVE_LEN )
		text_len = MAX_LIVE_LEN;
	
	batch.positions = positions;
	batch.glyph_indices = glyph_indices;
	batch.batch_len = batch_len;
	batch.batch_count = 0;
	
	do_simple_layout_internal( font, text, text_len, max_line_len, line_height_scale, &batch, chars );
	draw_glyph_batches( font, &batch, global_transform, draw_flags );
}

static GlyphBatch THE_EMPTY_BATCH = {
	NULL, NULL, NULL, 0
};

GlyphBatch *do_simple_layout( struct Font *font, uint32_t const *text, size_t text_len, int max_line_len, float line_height_scale )
{
	GlyphBatch *b = NULL;
	TempChar *chars = NULL;
	float *positions = NULL;
	
	if ( !text_len )
		return &THE_EMPTY_BATCH;
	
	b = malloc( sizeof(*b) );
	chars = malloc( text_len * sizeof(*chars) );
	positions = malloc( text_len * sizeof(float) * 2 );
	
	if ( !chars || !b || !positions )
		goto error_handler;
	
	b->positions = positions;
	b->glyph_indices = NULL;
	b->batch_len = NULL;
	b->batch_count = 0;
	
	if ( !do_simple_layout_internal( font, text, text_len, max_line_len, line_height_scale, b, chars ) )
		goto error_handler;
	
	free( chars );
	return b;
	
error_handler:;
	if ( b ) free( b );
	if ( chars ) free( chars );
	if ( positions ) free( positions );
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
	if ( b != &THE_EMPTY_BATCH )
	{
		if ( b->positions ) free( b->positions );
		if ( b->glyph_indices ) free( b->glyph_indices );
		free( b );
	}
}
