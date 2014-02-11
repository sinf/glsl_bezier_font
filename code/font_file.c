#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h> /* ntohl */
#include <string.h> /* memset */
#include <stdio.h>
#include "font_file.h"
#include "opentype.h"
#include "triangulate.h"

#pragma pack(1)

enum {
	DEBUG_DUMP = 1, /* enable/disable level 1 debug messages */
	DEBUG_DUMP2 = 0 /* enable/disable level 2 debug messages */
};

/* todo:
- fix DroidSerif-Regular.ttf failure
- triangulate_contours()
- metrics
- support more cmap formats
- proper TTC support
*/

static int read_shorts( FILE *fp, uint16 x[], uint32 count )
{
	uint32 n;
	if ( fread( x, 2, count, fp ) != count )
		return 1;
	for( n=0; n<count; n++ )
		x[n] = ntohs( x[n] );
	return 0;
}

/* Used by read_glyph */
static void *read_composite_glyph( FILE *fp, float units_per_em, Font font[1], FontStatus status[1] )
{
	/* SubGlyphHeader */
	struct {
		uint16 flags, glyph_index;
	} sgh;
	
	uint16 num = 0; /* temporary counter used to indicate the index of current subglyph in the CompositeGlyph */
	void *glyph_data = NULL; /* points to a CompositeGlyph, whose size is not yet known */
	
	/* num_subglyphs begins as zero because of calloc */
	glyph_data = calloc( 1, 4 );
	
	if ( !glyph_data ) {
		*status = F_FAIL_ALLOC;
		return NULL;
	}
	
	do {
		int16 args[2];
		/* These pointers alias glyph_data */
		uint32 *num_subglyphs;
		uint32 *sg_indices;
		float *sg_matrix;
		float *sg_offset;
		int16 fixed_matrix[4];
		int e;
		
		num_subglyphs = glyph_data;
		*num_subglyphs += 1;
		glyph_data = realloc( glyph_data, COMPOSITE_GLYPH_SIZE( *num_subglyphs ) );
		
		if ( !glyph_data ) {
			*status = F_FAIL_ALLOC;
			return NULL;
		}
		
		num_subglyphs = glyph_data;
		sg_indices = (uint32*) glyph_data + 1;
		sg_matrix = (float*) glyph_data + 1 + *num_subglyphs + num * 6;
		sg_offset = sg_matrix + 4;
		
		if ( read_shorts( fp, &sgh.flags, 2 ) ) {
			*status = F_FAIL_EOF;
			goto error_handler;
		}
		
		if ( sgh.glyph_index >= font->num_glyphs )
			sgh.glyph_index = 0;
		
		sg_indices[ num ] = sgh.glyph_index;
		
		if ( sgh.flags & COM_ARGS_ARE_WORDS ) {
			/* 16-bit args */
			if ( read_shorts( fp, (uint16*) args, 2 ) ) {
				*status = F_FAIL_EOF;
				goto error_handler;
			}
		} else {
			/* 8-bit args */
			int8 temp[2];
			if ( fread( temp, 1, 2, fp ) != 2 ) {
				*status = F_FAIL_EOF;
				goto error_handler;
			}
			args[0] = temp[0];
			args[1] = temp[1];
		}
		
		sg_offset[0] = 0;
		sg_offset[1] = 0;
		
		if ( sgh.flags & COM_ARGS_ARE_XY_VALUES ) {
			/* args are an offset vector */
			sg_offset[0] = args[0] / units_per_em;
			sg_offset[1] = args[1] / units_per_em;
		} else {
			/* args are point indices
			todo:
			1. find the GlyphTriangles that corresponds to glyph_index
			2. get coordinates of the relevant points
			
			Either arg1 or arg2 is presumably a point index of the subglyph.
			But the composite glyph has no points of it's own, so what does the other argument refer to???
			*/
			(void) font;
			printf( "todo: match points\n" );
		}
		
		fixed_matrix[0] = 1;
		fixed_matrix[1] = 0;
		fixed_matrix[2] = 0;
		fixed_matrix[3] = 1;
		
		/* FreeType has "else" between these 3 ifs
		But what if all 3 bits are set? */
		if ( sgh.flags & COM_HAVE_A_SCALE )
		{
			if ( read_shorts( fp, (uint16*) fixed_matrix, 1 ) ) {
				*status = F_FAIL_EOF;
				goto error_handler;
			}
			fixed_matrix[3] = fixed_matrix[0];
		}
		else if ( sgh.flags & COM_HAVE_X_AND_Y_SCALE )
		{
			int16 temp[2];
			if ( read_shorts( fp, (uint16*) temp, 2 ) ) {
				*status = F_FAIL_EOF;
				goto error_handler;
			}
			fixed_matrix[0] = temp[0];
			fixed_matrix[3] = temp[1];
		}
		else if ( sgh.flags & COM_HAVE_MATRIX )
		{
			if ( read_shorts( fp, (uint16*) fixed_matrix, 4 ) ) {
				*status = F_FAIL_EOF;
				goto error_handler;
			}
		}
		
		for( e=0; e<4; e++ )
			sg_matrix[e] = fixed_matrix[e] / (float) ( 1 << 14 );
		
		num++;
	} while( sgh.flags & 0x20 );
	
	*status = F_SUCCESS;
	return glyph_data;
	
error_handler:;
	if ( glyph_data )
		free( glyph_data );
	return NULL;
}

/* Used by read_simple_glyph() to read point X and Y coordinates */
static FontStatus read_contour_coord( FILE *fp, long file_pos[1], uint8 flags, int32 prev_co[1], int32 new_co[1] )
{
	uint8 short_bit, same_bit;
	
	if ( fseek( fp, *file_pos, SEEK_SET ) < 0 )
		return F_FAIL_CORRUPT;
	
	short_bit = flags & PT_SHORT_X;
	same_bit = flags & PT_SAME_X;
	*new_co = *prev_co;
	
	if ( short_bit ) {
		/* same_bit is now the sign bit; 0x10=positive, 0x00=negative */
		uint8 delta;
		if ( fread( &delta, 1, 1, fp ) != 1 )
			return F_FAIL_EOF;
		if ( same_bit )
			*new_co += delta;
		else
			*new_co -= delta;
		*file_pos += 1;
	} else {
		/* Use the previous coordinate if same_bit is set
		Otherwise, read a 16-bit delta value */
		if ( !same_bit ) {
			int16 delta;
			if ( read_shorts( fp, (uint16*) &delta, 1 ) )
				return F_FAIL_EOF;
			*new_co += delta;
			*file_pos += 2;
		}
	}
	
	*prev_co = *new_co;
	return F_SUCCESS;
}

/* Used by read_glyph */
static SimpleGlyph *read_simple_glyph( FILE *fp, float units_per_em, uint16 num_contours, FontStatus status[1] )
{
	SimpleGlyph *glyph = NULL;
	uint16 *end_points = NULL;
	float *final_points = NULL;
	uint32 num_points;
	uint32 n;
	long x_data_pos, y_data_pos;
	uint32 x_coords_size;
	int32 prev_x, prev_y;
	uint8 *point_flags = NULL;
	PointFlag *final_flags = NULL;
	uint16 num_instr;
	
	end_points = calloc( num_contours, 2 );
	if ( !end_points ) {
		*status = F_FAIL_ALLOC;
		return NULL;
	}
	if ( read_shorts( fp, end_points, num_contours ) ) {
		*status = F_FAIL_EOF;
		goto error_handler;
	}
	
	num_points = end_points[ num_contours - 1 ] + 1;
	if ( num_points > MAX_GLYPH_POINTS ) {
		*status = F_FAIL_BUFFER_LIMIT;
		goto error_handler;
	}
	
	point_flags = calloc( MAX_GLYPH_POINTS, 1 );
	final_flags = calloc( MAX_GLYPH_POINTS, sizeof( PointFlag ) );
	final_points = calloc( MAX_GLYPH_POINTS, sizeof( PointCoord ) * 2 );
	
	if ( !point_flags || !final_points || !final_flags ) {
		*status = F_FAIL_ALLOC;
		goto error_handler;
	}
	
	/* Skip the hinting instruction */
	if ( read_shorts( fp, &num_instr, 1 ) ) {
		*status = F_FAIL_EOF;
		goto error_handler;
	}
	if ( fseek( fp, num_instr, SEEK_CUR ) < 0 ) {
		*status = F_FAIL_CORRUPT;
		goto error_handler;
	}
	
	/* Determine the size of X coordinate array by scanning the flags */
	x_coords_size = n = 0;
	while( n < num_points ) 
	{
		uint32 x_incr, end, count=1;
		uint8 flags;
		
		if ( fread( &flags, 1, 1, fp ) != 1 ) {
			*status = F_FAIL_EOF;
			goto error_handler;
		}
		
		if ( flags & PT_SHORT_X )
			x_incr = 1;
		else
			x_incr = ( flags & PT_SAME_X ) ? 0 : 2;
		
		if ( flags & PT_SAME_FLAGS )
		{
			uint8 repeat;
			if ( fread( &repeat, 1, 1, fp ) != 1 ) {
				*status = F_FAIL_EOF;
				goto error_handler;
			}
			count += repeat;
		}
		
		x_coords_size += x_incr * count;
		end = n + count;
		
		if ( end > num_points ) {
			/* more flags than points */
			*status = F_FAIL_CORRUPT;
			goto error_handler;
		}
		
		while( n < end )
			point_flags[n++] = flags;
	}
	
	if ( n != num_points ) {
		/* less flags than points */
		*status = F_FAIL_CORRUPT;
		goto error_handler;
	}
	
	x_data_pos = ftell( fp );
	y_data_pos = x_data_pos + x_coords_size;
	prev_x = prev_y = 0;
	
	if ( x_data_pos < 0 ) {
		/* ftell failed */
		*status = F_FAIL_IMPOSSIBLE;
		goto error_handler;
	}
	
	/* Read the points' coordinates */
	for( n=0; n<num_points; n++ )
	{
		uint8 flags = point_flags[n];
		int32 new_x, new_y;
		if ( (*status = read_contour_coord( fp, &x_data_pos, flags, &prev_x, &new_x )) != F_SUCCESS ) goto error_handler;
		if ( (*status = read_contour_coord( fp, &y_data_pos, flags>>1, &prev_y, &new_y )) != F_SUCCESS ) goto error_handler;
		final_points[2*n+0] = new_x / units_per_em;
		final_points[2*n+1] = new_y / units_per_em;
		final_flags[n] = point_flags[n] & PT_ON_CURVE; /* discard all flags except the one that matters */
	}
	
	glyph = calloc( 1, sizeof( SimpleGlyph ) );
	if ( !glyph ) {
		*status = F_FAIL_ALLOC;
	} else {
		TrError e = triangulate_contours( &glyph->tris, final_flags, final_points, end_points, num_contours );
		if ( e != TR_SUCCESS )
		{
			if ( DEBUG_DUMP )
				printf( "Failed to triangulate contours (%u)\n", e );
			
			*status = F_FAIL_TRIANGULATE;
		}
		else
		{
			/* these must not be free'd since they are in use: */
			end_points = NULL;
			final_points = NULL;
			final_flags = NULL;
		}
	}
	
error_handler:;
	if ( final_points ) free( final_points );
	if ( final_flags ) free( final_flags );
	if ( end_points ) free( end_points );
	if ( point_flags ) free( point_flags );
	
	return glyph;
}

static FontStatus read_glyph( FILE *fp, Font font[1], uint32 glyph_index, float units_per_em, uint32 glyph_file_pos, uint16 max_contours, unsigned glyph_counts[2] )
{
	/* GlyphHeader */
	struct {
		uint16 num_contours;
		int16 xmin, ymin, xmax, ymax;
	} header;
	FontStatus status = F_FAIL_IMPOSSIBLE;
	
	if ( fseek( fp, glyph_file_pos, SEEK_SET ) < 0 )
		return F_FAIL_CORRUPT;
	
	if ( read_shorts( fp, &header.num_contours, 5 ) )
		return F_FAIL_EOF;
	
	if ( header.num_contours >= 0x1000 )
	{
		font->glyphs[ glyph_index ] = read_composite_glyph( fp, units_per_em, font, &status );
		glyph_counts[1] += ( status == F_SUCCESS );
	}
	else
	{
		if ( header.num_contours > max_contours ) {
			return F_FAIL_CORRUPT;
		}
		font->glyphs[ glyph_index ] = read_simple_glyph( fp, units_per_em, header.num_contours, &status );
		glyph_counts[0] += ( status == F_SUCCESS );
	}
	
	return status;
}

/* Reads both 'loca' and 'glyf' tables */
static FontStatus read_all_glyphs( FILE *fp, Font font[1], int16 format, float units_per_em, uint32 glyph_base_offset, uint16 max_contours )
{
	void *loca_p;
	uint32 n = 0;
	FontStatus status;
	unsigned glyph_counts[2] = {0,0};
	
	if ( DEBUG_DUMP )
		printf( "loca format %u (%s)\n", format, format ? "32-bit" : "16-bit" );
	
	if ( format == 0 )
	{
		/* 16-bit glyph location table */
		
		uint16 *loca, prev_loc;
		loca = loca_p = calloc( font->num_glyphs, 2 );
		
		if ( !loca )
			return F_FAIL_ALLOC;
		
		if ( read_shorts( fp, loca, font->num_glyphs ) )
			status = F_FAIL_EOF;
		else
		{
			prev_loc = loca[1];
			status = F_FAIL_INCOMPLETE;
			
			for( n=0; n<font->num_glyphs; n++ )
			{
				uint32 loc = loca[n];
				
				if ( loc == prev_loc ) {
					/* This glyph has no outline and can be left as NULL */
					continue;
				}
				
				if ( DEBUG_DUMP2 )
					printf( "Reading glyph %u out of %u\n", (uint) n, (uint) font->num_glyphs );
				
				prev_loc = loc;
				status = read_glyph( fp, font, n, units_per_em, (uint32) loc * 2 + glyph_base_offset, max_contours, glyph_counts );
				
				if ( status != F_SUCCESS )
					break;
			}
		}
	}
	else
	{
		/* 32-bit glyph location table */
		
		uint32 *loca, prev_loc;
		loca = loca_p = calloc( font->num_glyphs, 4 );
		
		if ( !loca )
			return F_FAIL_ALLOC;
		
		if ( fread( loca, 4, font->num_glyphs, fp ) != font->num_glyphs ) {
			status = F_FAIL_EOF;
		} else {
			prev_loc = loca[1];
			status = F_FAIL_INCOMPLETE;
			
			for( n=0; n<font->num_glyphs; n++ )
			{
				if ( loca[n] == prev_loc )
					continue;
				
				if ( DEBUG_DUMP2 )
					printf( "Reading glyph %u\n", (uint) n );
				
				prev_loc = loca[n];
				status = read_glyph( fp, font, n, units_per_em, ntohl( loca[n] ) + glyph_base_offset, max_contours, glyph_counts );
				
				if ( status != F_SUCCESS )
					break;
			}
		}
	}
	
	if ( DEBUG_DUMP )
	{
		printf( "Read %u out of %u glyphs\n"
			"Simple glyphs: %u\n"
			"Composite glyphs: %u\n",
			(uint) n,
			(uint) font->num_glyphs,
			glyph_counts[0], glyph_counts[1] );
	}
	
	free( loca_p );
	return status;
}

static FontStatus read_cmap_format4( FILE *fp, Font font[1], uint32 total_length )
{
	uint16 *whole_table;
	uint16 *end_codes, *start_codes, *id_range_offset;
	int16 *id_delta;
	uint16 seg_count, s;
	uint32 max_k;
	unsigned total_indices = 0;
	unsigned n_valid = 0;
	
	/* because format and length have been already read */
	total_length -= 2*2;
	
	if ( ( whole_table = malloc( total_length ) ) == NULL )
		return F_FAIL_ALLOC;
	
	if ( read_shorts( fp, whole_table, total_length >> 1 ) )
		return F_FAIL_EOF;
	
	seg_count = whole_table[1] >> 1;
	end_codes = whole_table + 5;
	start_codes = end_codes + seg_count + 1;
	id_delta = (int16*) start_codes + seg_count;
	id_range_offset = start_codes + 2 * seg_count;
	max_k = total_length / 2 - ( id_range_offset - whole_table );
	
	if ( DEBUG_DUMP )
		printf( "Segments: %u\nmax_k=%u\n", (uint) seg_count, (uint) max_k );
	
	for( s=0; s<seg_count; s++ )
	{
		uint16 c, start, end, stop;
		uint16 idro;
		int16 idde;
		
		end = end_codes[s];
		start = start_codes[s];
		idro = id_range_offset[s];
		idde = id_delta[s];
		stop = end + 1;
		total_indices += end - start + 1;
		
		/*
		printf( "start %u end %u idro %u idde %u\n", start, end, idro, idde );
		*/
		
		if ( start > end ) {
			free( whole_table );
			return F_FAIL_CORRUPT;
		}
		
		if ( idro != 0 )
		{
			/* glyphIndex = *( &idRangeOffset[i] + idRangeOffset[i] / 2 + (c - startCode[i]) )
			a <= c <= b
			
			glyphIndex = *( &idRangeOffset[i] + idRangeOffset[i] / 2 + (c - startCode[i]) )
			glyphIndex = *( &idRangeOffset[i] + idRangeOffset[i] / 2 + c - startCode[i] )
			glyphIndex = *( idRangeOffset + i + idRangeOffset[i] / 2 + c - startCode[i] )
			glyphIndex = idRangeOffset[ i + idRangeOffset[i] / 2 + c - startCode[i] ]
			*/
			
			for( c=start; c != stop; c++ ) {
				uint16 k = idro / 2 + c - start; /* + s ??? */
				if ( k < max_k )
				{
					k = id_range_offset[k];
					if ( k != 0 )
						n_valid += set_cmap_entry( font, c, ( idde + k ) & 0xFFFF );
				}
			}
		}
		else
		{
			/* glyphIndex = idDelta[i] + c,
			a <= c <= b
			*/
			for( c=start; c != stop; c++ )
				n_valid += set_cmap_entry( font, c, ( idde + c ) & 0xFFFF );
		}
	}
	
	if ( DEBUG_DUMP )
	{
		printf( "Success (%u/%u indices set, %u/%u segs)\n", n_valid, total_indices, (uint) s, (uint) seg_count );
		
		#if USE_BINTREE_CMAP
		printf( "Binary tree allocated length: %u\n", font->cmap.data_len );
		#endif
	}
	
	free( whole_table );
	return F_SUCCESS;
}

FontStatus read_cmap( FILE *fp, Font *font )
{
	struct { uint16 version, num_tables; } h;
	long cmap_header_start = ftell( fp );
	int has_read_cmap = 0;
	FontStatus status = F_FAIL_INCOMPLETE;
	
	if ( read_shorts( fp, &h.version, 2 ) )
		return F_FAIL_EOF;
	
	if ( h.version != 0 )
		return F_FAIL_UNSUP_VER;
	
	(void) font->cmap.data_len; /* just to make sure font->cmap is still a NibTree */
	memset( &font->cmap, 0, sizeof( font->cmap ) );
	
	while( h.num_tables-- )
	{	
		uint32 temp[2];
		uint32 subtable_offset;
		uint32 plat_enc; /* combined platform and specific encoding */
		long next_tabh_pos;
		
		if ( fread( temp, 4, 2, fp ) != 2 )
			return F_FAIL_EOF;
		
		plat_enc = ntohl( temp[0] );
		subtable_offset = cmap_header_start + ntohl( temp[1] );
		next_tabh_pos = ftell( fp );
		
		if ( fseek( fp, subtable_offset, SEEK_SET ) < 0 )
			return F_FAIL_CORRUPT;
		else
		{
			struct { uint16 format, length; } q;
			
			if ( read_shorts( fp, &q.format, 2 ) )
				return F_FAIL_EOF;
			
			if ( DEBUG_DUMP ) {
				printf( "platform = %u | encoding = %u | offset=%08x | format=%d | length=%d\n",
					plat_enc >> 16, plat_enc & 0xFFFF, subtable_offset, q.format, q.length );
			}
			
			/* Most common cmap formats seem to be 4 (the most common of all), 6 and 12
				So it seems reasonable to support just format 4 and nothing else */
			
			if ( !has_read_cmap && q.format == 4 )
			{
				status = read_cmap_format4( fp, font, q.length );
				has_read_cmap = 1;
			}
		}
		
		if ( fseek( fp, next_tabh_pos, SEEK_SET ) < 0 )
			return F_FAIL_IMPOSSIBLE;
		
		/*
		Platform/Encoding IDs:
			http://www.microsoft.com/typography/otspec/name.htm
		cmap formats:
			https://developer.apple.com/fonts/ttrefman/rm06/Chap6cmap.html
		*/
	}
	
	return status;
}

/* Assumes that the file is positioned after the very first field of Offset Table (sfnt version) */
static FontStatus read_offset_table( FILE *fp, Font font[1] )
{
	/* Indices of the tables we are interested in.
	table_pos and table_len are accessed with these  */
	enum {
		TAB_HEAD=0,
		TAB_MAXP,
		TAB_LOCA,
		TAB_GLYF,
		TAB_CMAP,
	/*
		TAB_HHEA,
		TAB_HMTX,
		TAB_VHEA,
		TAB_VMTX,
	*/
		NUM_USED_TABLES
	};
	
	uint32 table_pos[NUM_USED_TABLES] = {0};
	uint16 n, num_tables, num_glyphs;
	HeadTable head = {0};
	MaxProTableOne maxp = {0};
	int status;
	
	if ( read_shorts( fp, &num_tables, 1 ) )
		return F_FAIL_EOF;
	
	/* Skip rest of the offset table header */
	if ( fseek( fp, 3*2, SEEK_CUR ) < 0 )
		return F_FAIL_EOF;
	
	for( n=0; n<num_tables; n++ )
	{
		/* TableRecord */
		struct {
			uint32 tag;
			uint32 checksum;
			uint32 file_offset;
			uint32 length;
		} rec;
		int tab_num;
		
		if ( fread( &rec, 4, 4, fp ) != 4 )
			return F_FAIL_EOF;
		
		/* todo: remove this ntohl and convert the constants instead */
		switch( ntohl( rec.tag ) ) {
			case 0x68656164: tab_num = TAB_HEAD; break;
			case 0x6d617870: tab_num = TAB_MAXP; break;
			case 0x6c6f6361: tab_num = TAB_LOCA; break;
			case 0x676c7966: tab_num = TAB_GLYF; break;
			case 0x636d6170: tab_num = TAB_CMAP; break;
			default:
				if ( DEBUG_DUMP )
				{
					/* todo */
					printf( "unsupported table: %.4s\n", (char*) &rec.tag );
				}
				continue;
		}
		
		table_pos[ tab_num ] = ntohl( rec.file_offset );
		/* table_len[ tab_num ] = ntohl( rec.length ); */
		/* todo: verify checksum */
	}
	
	for( n=0; n<NUM_USED_TABLES; n++ ) {
		if ( !table_pos[n] ) {
			/* Missing important tables */
			return F_FAIL_INCOMPLETE;
		}
	}
	
	/* Read table: "head" */
	if ( fseek( fp, table_pos[TAB_HEAD], SEEK_SET ) < 0 )
		return F_FAIL_CORRUPT;
	if ( fread( &head, 54, 1, fp ) != 1 )
		return F_FAIL_EOF;
	if ( head.magic != htonl( 0x5F0F3CF5 ) )
		return F_FAIL_CORRUPT;
	
	/* Read table: "maxp" */
	if ( fseek( fp, table_pos[TAB_MAXP], SEEK_SET ) < 0 )
		return F_FAIL_CORRUPT;
	if ( fread( &maxp, 6, 1, fp ) != 1 )
		return F_FAIL_EOF;
	if ( maxp.version == htonl( 0x5000 ) ) {
		/* maxp version 0.5 */
	} else if ( maxp.version == htonl( 0x10000 ) ) {
		/* maxp version 1.0 */
		if ( fread( &maxp.max_points, 26, 1, fp ) != 1 )
			return F_FAIL_EOF;
	} else {
		/* unsupported maxp version */
		return F_FAIL_UNSUP_VER;
	}
	
	num_glyphs = ntohs( maxp.num_glyphs );
	
	if ( DEBUG_DUMP )
	{
		printf(
			"Font statistics:\n"
			"Version: %08x\n"
			"Revision: %08x\n"
			"Tables: %hu\n"
			"head / Flags: %08hx\n"
			"head / Units per EM: %hu\n"
			"maxp 0.5 / Glyphs: %hu\n",
			(uint) ntohl( head.version ),
			(uint) ntohl( head.font_rev ),
			(ushort) num_tables,
			(ushort) ntohs( head.flags ),
			(ushort) ntohs( head.units_per_em ),
			(ushort) num_glyphs );
		if ( maxp.version == htonl( 0x10000 ) )
		{
			printf(
			"maxp 1.0 / Max contours %hu\n"
			"maxp 1.0 / Max points (simple glyph) %hu\n"
			"maxp 1.0 / Max contours (simple glyph) %hu\n"
			"maxp 1.0 / Max composite recursion %hu\n",
			(ushort) ntohs( maxp.max_contours ),
			(ushort) ntohs( maxp.max_points ),
			(ushort) ntohs( maxp.max_contours ),
			(ushort) ntohs( maxp.max_com_recursion ) );
		}
	}
	
	font->num_glyphs = num_glyphs;
	if (( ( font->metrics = calloc( num_glyphs, sizeof( font->metrics[0] ) ) ) == NULL )) return F_FAIL_ALLOC;
	if (( ( font->glyphs = calloc( num_glyphs, sizeof( font->glyphs[0] ) ) ) == NULL )) return F_FAIL_ALLOC;
	
	if ( fseek( fp, table_pos[TAB_LOCA], SEEK_SET ) < 0 )
		return F_FAIL_CORRUPT;
	
	/* Read glyph contours using tables "loca" and "glyf" */
	status = read_all_glyphs( fp, font, head.index_to_loc_format, ntohs( head.units_per_em ), table_pos[TAB_GLYF], ntohs( maxp.max_contours ) );
	if ( status != F_SUCCESS )
		return status;
	
	/* Merges contour points and indices into a contiguous block of memory */
	if ( !merge_glyph_data( font ) )
		return F_FAIL_ALLOC;
	
	/* Read table "cmap" */
	if ( fseek( fp, table_pos[TAB_CMAP], SEEK_SET ) < 0 )
		return F_FAIL_CORRUPT;
	status = read_cmap( fp, font );
	if ( status != F_SUCCESS )
		return status;
	
	/* todo:
	
	handle errors properly
	
	for each glyph:
		read location from the 'loca' table
		read glyph data
		group curves into triangles
		subdivide overlapping triangles
		generate solid triangles to fill the glyph interior
	create VBOs
	for each glyph:
		upload VBO data
	read cmap
	read hhead, hmtx (horizontal metrics)
	read vhea, vmtx (vertical metrics)
	
	Other useful tables:
	BASE - baseline data. Needed to mix glyphs from different scripts (e.g. some math symbols and CJK)
	GDEF, GPOS - used to change position of glyphs based on context
	GSUB - used to replace glyphs based on context
	JSTF - additional positioning crap
	post - has some interesting fields: italicAngle, underlinePosition, underlineThickness, isFixedPitch
	kern - glyph positioning. same as GPOS but less useful?
	name - font name & family name
	*/
	
	return F_SUCCESS;
}

static FontStatus read_ttc( FILE *fp, Font font[1] )
{
	/* the tag "ttcf" has been already consumed */
	uint32 h[3];
	
	if ( fread( h, 4, 3, fp ) != 3 )
		return F_FAIL_EOF;
	
	if ( h[0] != htonl( 0x10000 ) && h[0] != htonl( 0x20000 ) ) {
		/* unsupported TTC version */
		return F_FAIL_UNSUP_VER;
	}
	
	if ( h[1] == 0 ) {
		/* TTC doesn't contain any fonts. Still a valid TTC though? */
		return F_FAIL_INCOMPLETE;
	}
	
	/*
	The font has at least 1 font
	- todo: read more than 1 font
	*/
	if ( fseek( fp, ntohl( h[2] )+4, SEEK_SET ) < 0 ) {
		return F_FAIL_CORRUPT;
	}
	
	return read_offset_table( fp, font );
}

FontStatus load_truetype( Font font[1], const char filename[] )
{
	FILE *fp = NULL;
	uint32 file_ident;
	FontStatus status;
	
	memset( font, 0, sizeof(*font) );
	fp = fopen( filename, "rb" );
	
	if ( !fp )
		return F_FAIL_OPEN;
	
	if ( fread( &file_ident, 4, 1, fp ) != 1 ) {
		status = F_FAIL_EOF;
	} else {
		if ( file_ident == htonl( 0x10000 ) ) {
			/* This is a TrueType font file (sfnt version 1.0)
			todo: handle other identifiers ("true", "typ1", "OTTO") */
			status = read_offset_table( fp, font );
		} else if ( file_ident == *(uint32*)"ttcf" ) {
			/* Is a TrueType Collection */
			status = read_ttc( fp, font );
		} else {
			/* Unsupported file format */
			status = F_FAIL_UNK_FILEF;
		}
	}
	
	fclose( fp );
	return status;
}
