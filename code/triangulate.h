#ifndef _TRIANGULATE_H
#define _TRIANGULATE_H
#include "font_data.h"

enum {
	/* memory limits */
	MAX_CONTOURS = 128, /* max contours per glyph (FreeMono.ttf has 120) */
	MAX_HOLES = 32, /* max holes per polygon */
	MAX_POINTS = 512 /* max points per contour (FreeMono.ttf has 480) */
};

/* Returns 0 on failure */
int triangulate_contours( GlyphTriangles gt[1], uint8 point_flags[MAX_POINTS], float points[2*MAX_POINTS], uint16 end_points[], uint32 num_contours );

#endif
