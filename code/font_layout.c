#include <stdlib.h>
#include "font_shader.h"
#include "font_layout.h"

#include <stdio.h>

typedef struct {
	uint32 character;
	uint16 column, line;
} TempChar;

static int sort_func( const void *x, const void *y )
{
	TempChar const *a=x, *b=y;
	if ( a->character > b->character )
		return -1;
	if ( a->character < b->character )
		return 1;
	return 0;
}

GlyphBatch *do_monospace_layout( Font *font, uint32 *text, size_t text_len, float adv_col, float adv_line )
{	
	GlyphBatch *batches=NULL, *cur_batch;
	TempChar *chars=NULL;
	uint16 col, line;
	size_t n, num_batches;
	uint32 prev_char;
	
	if ( !text_len )
		return NULL;
	
	chars = calloc( text_len, sizeof(*chars) );
	if ( !chars )
		return NULL;
	
	col = line = 0;
	for( n=0; n<text_len; n++ )
	{
		chars[n].character = text[n];
		chars[n].column = col;
		chars[n].line = line;
		
		if ( text[n] == '\n' ) {
			col = 0;
			line++;
		} else {
			col++;
		}
	}
	
	/* Sort text into batches of the same character */
	qsort( chars, text_len, sizeof(*chars), sort_func );
	
	prev_char = num_batches = 0;
	for( n=0; n<text_len; n++ )
	{
		if ( chars[n].character != prev_char ) {
			prev_char = chars[n].character;
			num_batches++;
		}
	}
	
	printf( "laid out %u batches\n", (uint) num_batches );
	
	batches = calloc( 1 + num_batches, sizeof(*batches) );
	if ( !batches )
		goto memory_error;
	
	prev_char = 0;
	cur_batch = batches - 1;
	for( n=0; n<text_len; n++ )
	{
		float *p;
		
		if ( chars[n].character != prev_char )
		{
			uint32 glyph;
			
			prev_char = chars[n].character;
			glyph = get_cmap_entry( font, prev_char );
			
			cur_batch++;
			cur_batch->glyph = glyph;
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
		draw_glyphs( font, global_transform, b->glyph, b->count, b->positions, draw_flags );
		b++;
	}
}
