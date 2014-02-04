#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "font_data.h"

int set_cmap_entry( Font *font, uint32 unicode, uint32 glyph_index )
{
	if ( unicode >= UNICODE_MAX )
		return 0;
	if ( glyph_index >= font->num_glyphs )
		return 0;
	if ( HAS_BIG_CMAP( font ) )
		font->cmap.big[ unicode ] = glyph_index;
	else
		font->cmap.small[ unicode ] = glyph_index;
	return 1;
}

uint32 get_cmap_entry( Font *font, uint32 unicode )
{
	if ( unicode >= UNICODE_MAX )
		return 0; /* bad unicode */
	if ( HAS_BIG_CMAP( font ) )
		return font->cmap.big[ unicode ];
	else
		return font->cmap.small[ unicode ];
}

void destroy_font( Font *font )
{
	if ( font->gl_buffers[0] || font->gl_buffers[1] || font->gl_buffers[2] ) {
		printf( "destroy_font: leaking graphics memory\n" );
	}
	if ( font->metrics )
		free( font->metrics );
	if ( font->glyphs )
	{
		if ( font->all_glyphs )
		{
			/* merge_glyph_data has been called */
			free( font->all_glyphs );
			free( font->all_points );
			free( font->all_indices );
		}
		else
		{
			/* must delete everything individually */
			uint32 n;
			for( n=0; n<font->num_glyphs; n++ )
			{
				SimpleGlyph *g = font->glyphs[n];
				if ( g ) {
					if ( IS_SIMPLE_GLYPH( g ) ) {
						if ( g->tris.end_points ) free( g->tris.end_points );
						if ( g->tris.indices ) free( g->tris.indices );
						if ( g->tris.points ) free( g->tris.points );
					}
					free( g );
				}
			}
		}
		free( font->glyphs );
	}
	if ( HAS_BIG_CMAP( font ) ) {
		if ( font->cmap.big )
			free( font->cmap.big );
	}
	else {
		if ( font->cmap.small )
			free( font->cmap.small );
	}
	memset( font, 0, sizeof(*font) );
}

/* Merges all vertex & index arrays together so that every glyph can be put into the same VBO. Returns 0 if failure, 1 if success */
int merge_glyph_data( Font *font )
{
	size_t const point_size = sizeof( float ) * 2;
	size_t const index_size = 2;
	uint32 total_points = 0;
	uint32 total_indices = 0;
	uint32 total_glyphs_mem = 0;
	float *all_points=NULL, *point_p;
	uint16 *all_indices=NULL, *index_p;
	uint8 *all_glyphs=NULL, *glyph_p;
	uint32 n;
	
	for( n=0; n<font->num_glyphs; n++ )
	{
		SimpleGlyph *g = font->glyphs[n];
		if ( g )
		{
			if ( IS_SIMPLE_GLYPH( g ) ) {
				total_points += g->tris.num_points_total;
				total_indices += g->tris.num_indices_total;
				total_glyphs_mem += sizeof( SimpleGlyph );
			} else {
				total_glyphs_mem += COMPOSITE_GLYPH_SIZE( g->num_parts );
			}
		}
	}
	
	printf( "Merging glyph data\n" );
	printf( "Total points: %u (%u KiB)\n", (uint) total_points, (uint)(total_points*point_size/1024) );
	printf( "Total indices: %u (%u KiB)\n", (uint) total_indices, (uint)(total_indices*index_size/1024) );
	
	if ( total_points ) {
		all_points = malloc( point_size * total_points );
		if ( !all_points )
			goto out_of_mem;
	}
	
	if ( total_indices ) {
		all_indices = malloc( index_size * total_indices );
		if ( !all_indices )
			goto out_of_mem;
	}
	
	if ( total_glyphs_mem ) {
		all_glyphs = malloc( total_glyphs_mem );
		if ( !all_glyphs )
			goto out_of_mem;
	}
	
	point_p = font->all_points = all_points;
	index_p = font->all_indices = all_indices;
	glyph_p = font->all_glyphs = all_glyphs;
	font->all_points_size = total_points * point_size;
	font->all_indices_size = total_indices * index_size;
	
	for( n=0; n<font->num_glyphs; n++ )
	{
		SimpleGlyph *g = font->glyphs[n];
		if ( g )
		{
			font->glyphs[n] = (SimpleGlyph*) glyph_p;
			
			if ( IS_SIMPLE_GLYPH( g ) )
			{
				uint32 np = g->tris.num_points_total;
				uint32 ni = g->tris.num_indices_total;
				
				if ( np )
				{
					memcpy( point_p, g->tris.points, np * point_size );
					free( g->tris.points );
					g->tris.points = point_p;
					point_p += np * 2;
				}
				
				if ( ni )
				{
					memcpy( index_p, g->tris.indices, ni * index_size );
					free( g->tris.indices );
					g->tris.indices = index_p;
					index_p += ni;
				}
				
				memcpy( glyph_p, g, sizeof( SimpleGlyph ) );
				free( g );
				glyph_p += sizeof( SimpleGlyph );
			}
			else
			{
				size_t s = COMPOSITE_GLYPH_SIZE( g->num_parts );
				memcpy( glyph_p, g, s );
				free( g );
				glyph_p += s;
			}
		}
	}
	
	return 1;
	
out_of_mem:
	if ( all_points ) free( all_points );
	if ( all_indices ) free( all_indices );
	if ( all_glyphs ) free( all_glyphs );
	return 0;
}
