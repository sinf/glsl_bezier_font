#include <stdlib.h>
#include <string.h>
#include "nibtree.h"

static int initialize( NibTree tree[1] )
{
	NibValue len = 1024;
	
	tree->data = malloc( sizeof( NibValue ) * len );
	tree->data_len = len;
	tree->next_offset = 16;
	
	if ( !tree->data )
		return 0;
	
	/* clear the root branch with invalid offsets */
	memset( tree->data, 0, sizeof( NibValue ) * 16 );
	
	return 1;
}

static NibValue add_node( NibTree tree[1] )
{
	NibValue pos = tree->next_offset;
	NibValue end = tree->next_offset + 16;
	
	while( end > tree->data_len )
	{
		NibValue new_len = 2 * tree->data_len;
		NibValue *new_data;
		
		new_data = realloc( tree->data, sizeof( NibValue ) * new_len );
		if ( !new_data )
			return 0;
		
		tree->data = new_data;
		tree->data_len = new_len;
	}
	
	tree->next_offset = end;
	return pos;
}

int nibtree_set( NibTree tree[1], NibValue key, NibValue new_value )
{
	NibValue *node;
	NibValue nibble_bit_pos = 28;
	NibValue nibble, offset;
	
	if ( !tree->data ) {
		if ( !initialize( tree ) )
			return 0;
	}
	
	node = tree->data;
	
	do {
		nibble = ( key >> nibble_bit_pos ) & 0xF;
		nibble_bit_pos -= 4;
		offset = node[ nibble ];
		
		if ( !offset )
		{
			/* Branch doesn't exist so create it */
			
			size_t parent_index = node - tree->data;
			offset = add_node( tree );
			
			if ( !offset )
				return 0;
			
			tree->data[ parent_index + nibble ] = offset;
			memset( tree->data + offset, 0, sizeof( NibValue ) * 16 );
		}
		
		node = tree->data + offset;
	} while( nibble_bit_pos );
	
	node[ key & 0xF ] = new_value;
	return 1;
}

NibValue nibtree_get( NibTree tree[1], NibValue key )
{
	NibValue *node = tree->data;
	NibValue nibble_pos = 28;
	NibValue nibble, offset;
	
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
