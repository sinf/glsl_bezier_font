#include <stdlib.h>
#include "font_shader.h"
#include "font_layout.h"

#include <stdio.h>

typedef struct {
	uint32 glyph;
	float x, y;
} TempChar;

static int sort_func( const void *x, const void *y )
{
	TempChar const *a=x, *b=y;
	if ( a->glyph > b->glyph )
		return -1;
	if ( a->glyph < b->glyph )
		return 1;
	return 0;
}

GlyphBatch *do_simple_layout( Font *font, uint32 *text, size_t text_len, size_t max_line_len, float line_height_scale )
{
	GlyphBatch *batches=NULL, *cur_batch;
	TempChar *chars=NULL;
	size_t n, num_batches;
	uint32 prev_glyph;
	uint32 space_glyph;
	float pos_x, pos_y;
	float line_height;
	size_t column;
	
	if ( !text_len )
		return NULL;
	
	chars = calloc( text_len, sizeof(*chars) );
	if ( !chars )
		return NULL;
	
	line_height = ( font->metrics.ascent - font->metrics.descent + font->metrics.linegap ) * line_height_scale;
	space_glyph = get_cmap_entry( font, ' ' );
	pos_x = pos_y = 0;
	column = 0;
	for( n=0; n<text_len; n++ )
	{
		uint32 glyph;
		int newline = 0;
		float lsb, adv_x;
		
		glyph = get_cmap_entry( font, text[n] );
		lsb = font->metrics_lsb[ glyph ];
		adv_x = font->metrics_adv_x[ glyph ];
		
		chars[n].x = pos_x - lsb;
		chars[n].y = pos_y;
		
		if ( text[n] == '\n' ) {
			glyph = space_glyph;
			newline = 1;
		} else if ( column == max_line_len ) {
			newline = 1;
		}
		
		if ( newline ) {
			column = 0;
			pos_x = 0;
			pos_y += line_height;
		} else {
			pos_x += adv_x;
			column++;
		}
		
		chars[n].glyph = glyph;
	}
	
	/* Sort text into batches of the same character */
	qsort( chars, text_len, sizeof(*chars), sort_func );
	
	prev_glyph = num_batches = 0;
	for( n=0; n<text_len; n++ )
	{
		if ( chars[n].glyph != prev_glyph ) {
			prev_glyph = chars[n].glyph;
			num_batches++;
		}
	}
	
	/*
	When different character codes get mapped to the same glyph index (such as the "character not found" glyph) there will be many batches of the same glyph.
	This causes a big lag problem if the font is missing many glyphs
	TODO: merge batches with same same glyph index
	*/
	
	printf( "laid out %u batches\n", (uint) num_batches );
	
	batches = calloc( 1 + num_batches, sizeof(*batches) );
	if ( !batches )
		goto memory_error;
	
	prev_glyph = 0;
	cur_batch = batches - 1;
	for( n=0; n<text_len; n++ )
	{
		if ( chars[n].glyph != prev_glyph )
		{
			prev_glyph = chars[n].glyph;
			cur_batch++;
			cur_batch->glyph = prev_glyph;
			cur_batch->count = 0;
			cur_batch->positions = malloc( sizeof(float)*2*text_len );
			
			if ( !cur_batch->positions )
				goto memory_error;
		}
		
		if ( cur_batch->positions )
		{
			float *p = cur_batch->positions + 2 * cur_batch->count++;
			p[0] = chars[n].x;
			p[1] = chars[n].y;
		}
	}
	
	batches[num_batches].positions = NULL;
	batches[num_batches].glyph = 0;
	batches[num_batches].count = 0;
	
	free( chars );
	return batches;
	
memory_error:;
	if ( batches ) {
		for( n=0; n<num_batches; n++ )
			if ( batches[n].positions ) free( batches[n].positions );
		free( batches );
	}
	if ( chars )
		free( chars );
	return NULL;
}

void draw_glyph_batches( Font *font, GlyphBatch *b, float global_transform[16], int draw_flags )
{
	while( b->count ) {
		draw_glyphs( font, global_transform, b->glyph, b->count, b->positions, draw_flags );
		b++;
	}
}
