#ifndef _FONT_DATA_H
#define _FONT_DATA_H
#include <stddef.h>
#include "types.h"

/* Font data structure
function abstraction macros
get/set functions/macros
function pointers
constants
misc crap
*/

#define UNICODE_MAX 0x10FFFF

/* todo: use these everywhere */
typedef uint16 PointIndex;
typedef uint32 PointFlag; /* todo: figure out why uint8 and uint16 don't work */
typedef float PointCoord;

/* Glyph outline converted to triangles */
typedef struct {
	uint16 num_points_total; /* total number of points, including generated points */
	uint16 num_points_orig; /* number of the original points from TTF file */
	uint16 num_indices_total;
	uint16 num_indices_convex;
	uint16 num_indices_concave;
	uint16 num_indices_solid;
	uint16 *end_points; /* straight from TTF */
	PointIndex *indices; /* 1. convex curves 2. concave curves 3. solid triangles */
	PointCoord *points; /* First come the points from TTF file in the original order, then additional generated points (2 floats per point) */
	PointFlag *flags; /* on-curve flags */
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
	float advance_x;
	/* todo */
} GlyphMetrics;

#define HAS_BIG_CMAP(font) ((font)->num_glyphs > 0x10000)
#define IS_SIMPLE_GLYPH(glyph) ((glyph)->num_parts == 0)
#define COMPOSITE_GLYPH_SIZE(num_parts) (( 4+(num_parts)*(4+6*sizeof(float)) ))

typedef struct {
	GlyphMetrics *metrics;
	SimpleGlyph **glyphs; /* Array of pointers to CompositeGlyph and SimpleGlyph */
	union {
		/* Array of integers. Maps unicode to glyph index.
		The array has exactly UNICODE_MAX elements allocated (though most of them are usually zeros/wasted)
		If num_glyphs > 0x10000, use 'big'. Otherwise, use 'small' */
		uint16 *small;
		uint32 *big;
	} cmap;
	void *all_glyphs; /* SimpleGlyphs and composite glyphs */
	PointCoord *all_points;
	PointIndex *all_indices;
	PointFlag *all_flags;
	size_t total_points;
	size_t total_indices;
	uint32 gl_buffers[4]; /* vao, vbo, ibo, another vbo */
	uint32 num_glyphs; /* sizeof of glyphs array */
} Font;

/* GLSL font rendering algorithm:
1. Read the following from a TTF font file:
	- Table that maps unicode characters to glyph indices (cmap)
	- Bezier curves2
	- Metrics
2. Triangulate glyphs
3. Compile & link GLSL program
4. Layout text (output glyph index/position tuples)
5. Sort glyphs by indices
6. Upload uniforms
7. Draw
*/

int set_cmap_entry( Font *font, uint32 unicode, uint32 glyph_index );
uint32 get_cmap_entry( Font *font, uint32 unicode );
void destroy_font( Font *font );

/* Merges all vertex & index arrays together so that every glyph can be put into the same VBO
Returns 0 if failure, 1 if success */
int merge_glyph_data( Font *font );

#endif
