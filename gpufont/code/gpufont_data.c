#include <stdlib.h>
#include <string.h>
#include "gpufont_data.h"

void destroy_font( Font *font )
{
	if ( font->glyphs )
	{
		if ( font->all_glyphs )
		{
			/* merge_glyph_data has been called */
			free( font->all_glyphs );
			free( font->all_points );
			free( font->all_indices );
			free( font->all_flags );
		}
		else
		{
			/* must delete everything individually */
			size_t n;
			for( n=0; n<font->num_glyphs; n++ )
			{
				SimpleGlyph *g = font->glyphs[n];
				if ( g ) {
					if ( IS_SIMPLE_GLYPH( g ) ) {
						if ( g->tris.end_points ) free( g->tris.end_points );
						if ( g->tris.indices ) free( g->tris.indices );
						if ( g->tris.points ) free( g->tris.points );
						if ( g->tris.flags ) free( g->tris.flags );
					}
					free( g );
				}
			}
		}
		free( font->glyphs );
	}
	if ( font->hmetrics )
		free( font->hmetrics );
	memset( font, 0, sizeof(*font) );
}

/* Merges all vertex & index arrays together so that every glyph can be put into the same VBO. Returns 0 if failure, 1 if success */
int merge_glyph_data( Font *font )
{
	GlyphTriangles dummy;
	size_t const point_size = sizeof( dummy.points[0] ) * 2;
	size_t const index_size = sizeof( dummy.indices[0] );
	size_t const flag_size = sizeof( dummy.flags[0] );
	
	size_t total_points = 0;
	size_t total_indices = 0;
	size_t total_glyphs_mem = 0;
	char *all_glyphs=NULL, *glyph_p;
	PointCoord *all_points=NULL, *point_p;
	PointIndex *all_indices=NULL, *index_p;
	PointFlag *all_flags=NULL, *flag_p;
	size_t n;
	
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
	
	if ( total_points ) {
		all_points = malloc( point_size * total_points );
		all_flags = malloc( flag_size * total_points );
		if ( !all_points || !all_flags )
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
	flag_p = font->all_flags = all_flags;
	font->total_indices = total_indices;
	font->total_points = total_points;
	
	for( n=0; n<font->num_glyphs; n++ )
	{
		SimpleGlyph *g = font->glyphs[n];
		if ( g )
		{
			font->glyphs[n] = (SimpleGlyph*) glyph_p;
			
			if ( IS_SIMPLE_GLYPH( g ) )
			{
				size_t np = g->tris.num_points_total;
				size_t ni = g->tris.num_indices_total;
				
				if ( np )
				{
					memcpy( point_p, g->tris.points, np * point_size );
					memcpy( flag_p, g->tris.flags, np * flag_size );
					free( g->tris.points );
					free( g->tris.flags );
					g->tris.points = point_p;
					g->tris.flags = flag_p;
					point_p += np * 2;
					flag_p += np;
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
				glyph_p += s / sizeof( *glyph_p );
			}
		}
	}
	
	return 1;
	
out_of_mem:
	if ( all_points ) free( all_points );
	if ( all_flags ) free( all_flags );
	if ( all_indices ) free( all_indices );
	if ( all_glyphs ) free( all_glyphs );
	return 0;
}
