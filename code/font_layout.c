#include <stdlib.h>
#include "font_shader.h"
#include "font_layout.h"

#include <stdio.h>

typedef struct {
	uint32 glyph;
	uint16 column, line;
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

GlyphBatch *do_monospace_layout( Font *font, uint32 *text, size_t text_len, float adv_col, float adv_line, uint max_line_len )
{
	GlyphBatch *batches=NULL, *cur_batch;
	TempChar *chars=NULL;
	uint col, line;
	size_t n, num_batches;
	uint32 prev_glyph;
	
	if ( !text_len )
		return NULL;
	
	chars = calloc( text_len, sizeof(*chars) );
	if ( !chars )
		return NULL;
	
	col = line = 0;
	for( n=0; n<text_len; n++ )
	{
		chars[n].glyph = get_cmap_entry( font, text[n] );
		chars[n].column = col;
		chars[n].line = line;
		
		if ( text[n] == '\n' || col == max_line_len ) {
			col = 0;
			line++;
		} else {
			col++;
		}
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
		float *p;
		
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
		
		p = cur_batch->positions + 2 * cur_batch->count++;
		p[0] = chars[n].column * adv_col;
		p[1] = chars[n].line * adv_line;
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
		if ( b->glyph != 0 )
			draw_glyphs( font, global_transform, b->glyph, b->count, b->positions, draw_flags );
		b++;
	}
}
