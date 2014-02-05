#include <assert.h>
#include "linkedlist.h"

void init_list( LinkedList list[1], LLNode *pool, LLNodeID max_nodes )
{
	uint32 n;
	assert( max_nodes != LL_BAD_INDEX );
	
	for( n=0; n<max_nodes; n++ ) {
		empty[n].prev = ( n + max_nodes - 1 ) % max_nodes;
		empty[n].next = ( n + 1 ) % max_nodes;
	}
	
	list->pool = pool;
	list->num_empty = max_nodes;
	list->num_full = 0;
	list->root_empty = 0;
	list->root_full = LL_BAD_INDEX;
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
	if ( *root_index == LL_BAD_INDEX ) {
		node->prev = node_index;
		node->next = node_index;
	} else {
		LLNode *root = pool + *root_index;
		node->prev = root->prev;
		node->next = root_index;
		root->prev = node;
	}
	return *root_index = node_index;
}

LLNodeID add_node( LinkedList list[1] )
{
	assert( list->num_empty + list->num_full == list->pool_size );
	
	if ( !list->num_empty )
		return LL_BAD_INDEX;
	
	list->num_empty -= 1;
	list->num_full += 1;
	
	/* Unlink a node from the "empty" list and link that node to the "full" list */
	return link( list->pool, &list->root_full,
		unlink( list->pool, &list->root_empty, list->root_empty );
}

void pop_node( LinkedList list[1], LLNodeID node )
{
	assert( list->num_full > 0 );
	assert( list->num_empty + list->num_full == list->pool_size );
	
	list->num_empty += 1;
	list->num_full -= 1;
	
	/* Unlink a node from the "full" list and link that node to the "empty" list */
	link( list->pool, &list->root_empty,
		unlink( list->pool, &list->root_full, node ) );
}
