#ifndef _NIBBLE_TREE_H
#define _NIBBLE_TREE_H
#include <stdint.h>

/* This module implements a 16-tree that maps integers to integers (e.g. it can be at most 8 levels deep for a 32-bit integer)
I call it a "nibble tree" because of the way it is searched.
Intented to be used by font_file.c for character code to glyph index translation */

/* Should be the lowest common size of GlyphIndex and the biggest real world character code */
typedef uint32_t NibValue;

/* This structure must be zero-initialized before it can be used !! */
typedef struct {
	NibValue *data; /* can be free'd */
	NibValue data_len; /* how many NibValues have been allocated */
	NibValue next_offset; /* where to add the next node */
} NibTree;

/* Returns 0 if memory allocation failed, otherwise 1 */
int nibtree_set( NibTree b[1], NibValue key, NibValue value );

/* Retrieves value for given key. Returns 0 if key wasn't found */
NibValue nibtree_get( NibTree b[1], NibValue key );

#endif
