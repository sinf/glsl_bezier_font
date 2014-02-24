#ifndef _FONT_DATA_H
#define _FONT_DATA_H

#include <stddef.h>
#include <stdint.h>

#include "nibtree.h"
#define set_cmap_entry( font, code, glyph_index ) nibtree_set( &(font)->cmap, (code), (glyph_index) )
#define get_cmap_entry( font, code ) nibtree_get( &(font)->cmap, (code) )

/* Must be unsigned integer types: */
typedef uint16_t PointIndex;
typedef uint32_t GlyphIndex;
typedef uint32_t PointFlag;
/* Can be any types that can represent negative values: */
typedef float PointCoord; /* for contour point coordinates */
typedef float GlyphCoord; /* for glyph instance positions */

/* Glyph outline converted to triangles */
typedef struct GlyphTriangles {
	PointCoord *points; /* 2 floats per point */
	PointFlag *flags; /* on-curve flags */
	PointIndex *indices; /* 1. curve triangles 2. solid triangles */
	uint16_t *end_points; /* Straight from TTF. Free'd after triangulation by ttf_file.c */
	uint16_t num_points_total; /* total number of points, including generated points */
	uint16_t num_points_orig; /* number of the original points from TTF file */
	uint16_t num_indices_total;
	uint16_t num_indices_curve;
	uint16_t num_indices_solid;
	uint16_t num_contours; /* only used internally */
} GlyphTriangles;

typedef struct SimpleGlyph {
	size_t num_parts; /* if nonzero, then this struct is actually a CompositeGlyph and has no 'tris' field */
	GlyphTriangles tris;
} SimpleGlyph;

/* This is variable-sized and thus can't have an actual type defined.
struct CompositeGlyph {
	size_t num_parts;
	GlyphIndex subglyph_id [num_parts];
	float matrices_and_offsets [num_parts][6]; // first 2x2 matrix, then offset
}
*/

/* Use composite glyphs with these macros: */
#define GET_SUBGLYPH_COUNT(com) ((*(size_t*)(com)))
#define GET_SUBGLYPH_INDEX(com,n) (( * (GlyphIndex*) ( (size_t*)(com) + 1 ) + (n) ))
#define GET_SUBGLYPH_TRANSFORM(com,n) (( (float*)((GlyphIndex*)((size_t*)(com)+1) + GET_SUBGLYPH_COUNT(com)) + 6*(n) ))

/* Returns the size of a composite glyph (in bytes) */
#define COMPOSITE_GLYPH_SIZE(num_parts) (( sizeof(size_t) + (num_parts)*( sizeof(GlyphIndex) + sizeof(float)*6 ) ))

/* Composite glyph support can be globally disabled with this macro */
#define ENABLE_COMPOSITE_GLYPHS 0

/* Evaluates to true if given SimpleGlyph is really a simple glyph */
#define IS_SIMPLE_GLYPH(glyph) ((glyph)->num_parts == 0)

typedef struct {
	/* Directly from TTF file. They're given in EM units */
	uint16_t adv_width;
	int16_t lsb;
} LongHorzMetrics;

typedef struct Font {
	size_t num_glyphs; /* how many glyphs the font has */
	unsigned units_per_em; /* used to convert integer coordinates to floats */
	
	SimpleGlyph **glyphs; /* Array of pointers to glyphs. Each glyp can be either a SimpleGlyph or a composite glyph */
	void *all_glyphs; /* one huge array that contains all SimpleGlyphs and composite glyphs */
	PointCoord *all_points; /* a huge array that contains all the points of all simple glyphs */
	PointFlag *all_flags; /* all point flags of all simple glyphs */
	PointIndex *all_indices; /* all triangle indices of all simple glyphs */
	size_t total_points; /* length of all_points and all_flags */
	size_t total_indices; /* length of all_indices */
	
	/* Only used by gpufont_draw.c */
	uint32_t gl_buffers[4]; /* VAO, point coordinate VBO, IBO, point flag VBO */
	
	/* Maps character codes to glyph indices */
	NibTree cmap; /* Encoding could be anything. But its unicode for now */
	
	/* Horizontal metrics in EM units */
	LongHorzMetrics *hmetrics; /* has one entry for each glyph (unlike TTF file) */
	int horz_ascender;
	int horz_descender;
	int horz_linegap;
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
