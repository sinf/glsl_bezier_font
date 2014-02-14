#ifndef _FONT_DATA_H
#define _FONT_DATA_H
#include <stddef.h>
#include "types.h"
#include "nibtree.h"

#define set_cmap_entry( font, code, glyph_index ) nibtree_set( &(font)->cmap, (code), (glyph_index) )
#define get_cmap_entry( font, code ) nibtree_get( &(font)->cmap, (code) )

/* todo: use these everywhere */
typedef uint16 PointIndex;
typedef uint32 PointFlag; /* todo: figure out why uint8 and uint16 don't work */
typedef float PointCoord;

/* Glyph outline converted to triangles */
typedef struct {
	PointIndex *indices; /* 1. convex curves 2. concave curves 3. solid triangles */
	PointCoord *points; /* First come the points from TTF file in the original order, then additional generated points (2 floats per point) */
	PointFlag *flags; /* on-curve flags */
	uint16 *end_points; /* straight from TTF */
	uint16 num_points_total; /* total number of points, including generated points */
	uint16 num_points_orig; /* number of the original points from TTF file */
	uint16 num_indices_total;
	uint16 num_indices_convex;
	uint16 num_indices_concave;
	uint16 num_indices_solid;
	uint16 num_contours;
} GlyphTriangles;

typedef struct {
	uint32 num_parts; /* if nonzero, then this struct is actually a CompositeGlyph and has no 'tris' field */
	GlyphTriangles tris;
} SimpleGlyph;

/* This is variable-sized and thus can't have an actual type defined.
struct CompositeGlyph {
	uint32 num_parts;
	uint32 subglyph_id [num_parts];
	float matrices_and_offsets [num_parts][6]; // first 2x2 matrix, then offset
}
*/

typedef struct {
	float ascent;
	float descent;
	float linegap;
} GlobalMetrics;

#define IS_SIMPLE_GLYPH(glyph) ((glyph)->num_parts == 0)
#define COMPOSITE_GLYPH_SIZE(num_parts) (( 4+(num_parts)*(4+6*sizeof(float)) ))

typedef struct {
	SimpleGlyph **glyphs; /* Array of pointers to CompositeGlyph and SimpleGlyph */
	void *all_glyphs; /* SimpleGlyphs and composite glyphs */
	PointCoord *all_points;
	PointIndex *all_indices;
	PointFlag *all_flags;
	size_t total_points;
	size_t total_indices;
	uint32 gl_buffers[4]; /* vao, vbo, ibo, another vbo */
	uint32 num_glyphs; /* sizeof of glyphs array */
	NibTree cmap;
	GlobalMetrics metrics;
	float *metrics_adv_x; /* one per glyph */
	float *metrics_lsb; /* one per glyph */
} Font;

void destroy_font( Font *font );

/* Merges all vertex & index arrays together so that every glyph can be put into the same VBO
Returns 0 if failure, 1 if success */
int merge_glyph_data( Font *font );

/* combined platform and platform specific encoding fields */
enum {
	/* Platform 0: Unicode Transformation Format */
	ENC_UTF_DEFAULT = 0,
	ENC_UTF_11 = 1,
	ENC_UTF_ISO10646 = 2,
	ENC_UTF_20 = 3,
	
	/* Platform 1: Macintosh
	???
	*/
	
	/* Platform 3: Microsoft */
	ENC_MS_SYMBOL = ( 3 << 16 ),
	ENC_MS_UCS2 = ( 3 << 16 ) | 1, /* Unicode BMP (UCS-2) */
	ENC_MS_SHIFTJIS = ( 3 << 16 ) | 2,
	ENC_MS_PRC = ( 3 << 16 ) | 3,
	ENC_MS_BIG5 = ( 3 << 16 ) | 4,
	ENC_MS_WANSUNG = ( 3 << 16 ) | 5,
	ENC_MS_JOHAB = ( 3 << 16 ) | 6,
	ENC_MS_UCS4 = ( 3 << 16 ) | 10
};

#endif
