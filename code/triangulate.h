#ifndef _TRIANGULATE_H
#define _TRIANGULATE_H
#include "font_data.h"
#include "types.h"

/* memory limits */
enum {
	MAX_GLYPH_CONTOURS = 128, /* Max contours per glyph (FreeMono.ttf has 120) */
	MAX_GLYPH_POINTS = 1024, /* Max total per (simple) glyph (FreeMono.ttf has up to 480). Includes generated points */
	MAX_GLYPH_TRI_INDICES = 2048 /* Max triangle indices */
};

/* error codes */
typedef enum {
	TR_SUCCESS=0,
	TR_POINTS_LIMIT, /* too many points */
	TR_HOLES_LIMIT, /* too many nested contours */
	TR_INDICES_LIMIT, /* too many indices */
	TR_ALLOC_FAIL, /* calloc/malloc failed */
	TR_EMPTY_CONTOUR, /* a contour has no points */
	TR_NONE_ON_CURVE /* a contour does not have any on-curve points */
} TrError;

/* Returns 0 on failure */
TrError triangulate_contours( GlyphTriangles gt[1],
	PointFlag point_flags[MAX_GLYPH_POINTS],
	PointCoord point_coords[2*MAX_GLYPH_POINTS],
	uint16 end_points[],
	uint32 num_contours );

#endif
