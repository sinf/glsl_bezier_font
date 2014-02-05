#include <assert.h>
#include "linkedlist.h"

void init_list( LinkedList list[1], LLNode *pool, LLNodeID max_nodes )
{
	uint32 n;
	for( n=0; n<max_nodes; n++ ) {
		pool[n].prev = ( n + max_nodes - 1 ) % max_nodes;
		pool[n].next = ( n + 1 ) % max_nodes;
	}
	
	list->pool = pool;
	list->num_nodes = 0;
	list->root_index_empty = 0;
	list->root_index = LL_BAD_INDEX;
	list->pool_size = max_nodes;
}

static LLNodeID unlink( LLNode *pool, LLNodeID root_index[1], LLNodeID node_index )
{
	LLNode *node = pool + node_index;
	pool[ node->prev ].next = node->next;
	pool[ node->next ].prev = node->prev;
	*root_index = ( *root_index == node_index ) ? ~0 : node_index;
	return node_index;
}

static LLNodeID link( LLNode *pool, LLNodeID root_index[1], LLNodeID node_index )
{
	LLNode *node = pool + node_index;
	
	if ( *root_index == LL_BAD_INDEX )
	{
		node->prev = node_index;
		node->next = node_index;
	}
	else
	{
		LLNode *root = pool + *root_index;
		node->prev = root->prev;
		node->next = *root_index;
		root->prev = node_index;
	}
	
	return *root_index = node_index;
}

LLNodeID add_node_x( LinkedList list[1], LLNodeID index )
{
	if ( list->num_nodes == list->pool_size )
		return LL_BAD_INDEX;
	
	list->num_nodes += 1;
	
	unlink( list->pool, &list->root_index_empty, index );
	link( list->pool, &list->root_index, index );
	
	return index;
}

void pop_node( LinkedList list[1], LLNodeID node )
{
	assert( list->num_nodes > 0 );
	list->num_nodes -= 1;
	
	/* Unlink a node from the "full" list and link that node to the "empty" list */
	link( list->pool, &list->root_index_empty,
		unlink( list->pool, &list->root_index, node ) );
}
