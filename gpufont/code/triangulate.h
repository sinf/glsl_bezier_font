#ifndef _TRIANGULATE_H
#define _TRIANGULATE_H

/* memory limits */
enum {
	MAX_GLYPH_CONTOURS = 128, /* Max contours per glyph (FreeMono.ttf has 120) */
	MAX_GLYPH_POINTS = 2048, /* Max total per (simple) glyph (FreeMono.ttf has up to 480). Includes generated points */
	MAX_GLYPH_TRI_INDICES = 4096 /* Max triangle indices */
};

/* error codes */
typedef enum {
	TR_SUCCESS=0,
	TR_POINTS_LIMIT, /* too many points */
	TR_INDICES_LIMIT, /* too many indices */
	TR_ALLOC_FAIL /* calloc/malloc failed */
} TrError;

struct GlyphTriangles;
struct Triangulator;

struct Triangulator *triangulator_begin( void );
void triangulator_end( struct Triangulator * );

/* Before calling triangulate_contours()
gt->end_points must not be NULL
gt->points must be allocated to 2*MAX_GLYPH_POINTS elements
gt->flags must be allocated to MAX_GLYPH_POINTS elements
Other fields in gt must also have been initialized
*/
TrError triangulate_contours( struct Triangulator *, struct GlyphTriangles *gt );

#endif
