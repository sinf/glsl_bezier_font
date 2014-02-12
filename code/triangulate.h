#ifndef _TRIANGULATE_H
#define _TRIANGULATE_H
#include "font_data.h"
#include "types.h"

/* memory limits */
enum {
	MAX_GLYPH_CONTOURS = 128, /* Max contours per glyph (FreeMono.ttf has 120) */
	MAX_GLYPH_POINTS = 600, /* Max total per (simple) glyph (FreeMono.ttf has up to 480). Includes generated points */
	MAX_GLYPH_TRI_INDICES = 8192 /* Max triangle indices */
};

/* error codes */
typedef enum {
	TR_SUCCESS=0,
	TR_POINTS_LIMIT, /* too many points */
	TR_INDICES_LIMIT, /* too many indices */
	TR_ALLOC_FAIL /* calloc/malloc failed */
} TrError;

void *triangulator_begin( void );
void triangulator_end( void * );

/*
gt->end_points must not be NULL
gt->points must be allocated to 2*MAX_GLYPH_POINTS elements
gt->flags must be allocated to MAX_GLYPH_POINTS elements
Other fields in gt must also have been initialized
*/
TrError triangulate_contours( void *glu_tess_handle, GlyphTriangles gt[1] );

#endif
