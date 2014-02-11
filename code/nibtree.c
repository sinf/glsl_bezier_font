#include <stdlib.h>
#include <string.h>
#include "nibtree.h"

static int initialize( NibTree tree[1] )
{
	uint32 len = 1024;
	
	tree->data = malloc( len * 4 );
	tree->data_len = len;
	tree->next_offset = 16;
	
	if ( !tree->data )
		return 0;
	
	/* clear the root branch with invalid offsets */
	memset( tree->data, 0, 16 * 4 );
	
	return 1;
}

static uint32 add_node( NibTree tree[1] )
{
	uint32 pos = tree->next_offset;
	uint32 end = tree->next_offset + 16;
	
	while( end > tree->data_len )
	{
		uint32 new_len = 2 * tree->data_len;
		uint32 *new_data;
		
		new_data = realloc( tree->data, new_len * 4 );
		if ( !new_data )
			return 0;
		
		tree->data = new_data;
		tree->data_len = new_len;
	}
	
	tree->next_offset = end;
	return pos;
}

int nibtree_set( NibTree tree[1], uint32 key, uint32 new_value )
{
	uint32 *node;
	uint32 nibble_pos = 28;
	uint32 nibble, offset;
	
	if ( !tree->data ) {
		if ( !initialize( tree ) )
			return 0;
	}
	
	node = tree->data;
	
	do {
		nibble = ( key >> nibble_pos ) & 0xF;
		nibble_pos -= 4;
		offset = node[ nibble ];
		
		if ( !offset )
		{
			/* Branch doesn't exist so create it */
			
			offset = add_node( tree );
			
			if ( !offset )
				return 0;
			
			node[ nibble ] = offset;
			memset( tree->data + offset, 0, 16 * 4 );
		}
		
		node = tree->data + offset;
	} while( nibble_pos );
	
	node[ key & 0xF ] = new_value;
	return 1;
}

uint32 nibtree_get( NibTree tree[1], uint32 key )
{
	uint32 *node = tree->data;
	uint32 nibble_pos = 28;
	uint32 nibble, offset;
	
	if ( !node )
		return 0;
	
	do {
		nibble = ( key >> nibble_pos ) & 0xF;
		nibble_pos -= 4;
		offset = node[ nibble ];
		
		if ( !offset )
			return 0;
		
		node = tree->data + offset;
	} while( nibble_pos );
	
	return node[ key & 0xF ];
}
