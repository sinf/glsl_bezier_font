#ifndef _BINARYTREE_H
#define _BINARYTREE_H
#include "types.h"

/* This module implements a 16-tree that maps integers to integers.
It can be at most 8 levels deep (for a 32-bit integer).
I call it a "nibble tree" because of the way it is searched.
Intented to be used by font_file.c for character code to glyph index translation */

/* This structure must be zero-initialized before it can be used !! */
typedef struct {
	uint32 *data; /* can be free'd */
	uint32 data_len; /* how many uint32's have been allocated */
	uint32 next_offset; /* where to add the next node */
} NibTree;

/* Returns 0 if memory allocation failed, otherwise 1 */
int nibtree_set( NibTree b[1], uint32 key, uint32 value );

/* Retrieves value for given key. Returns 0 if key wasn't found */
uint32 nibtree_get( NibTree b[1], uint32 key );

#endif
