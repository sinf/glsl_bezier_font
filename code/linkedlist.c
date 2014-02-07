#include <assert.h>
#include "linkedlist.h"

void init_list( LinkedList list[1], LLNode pool[], LLNodeID max_nodes )
{
	LLNodeID n;
	for( n=0; n<max_nodes; n++ ) {
		pool[n].prev = ( n + max_nodes - 1 ) % max_nodes;
		pool[n].next = ( n + 1 ) % max_nodes;
	}
	
	list->pool = pool;
	list->length = 0;
	list->root_of_empty = 0;
	list->root = LL_BAD_INDEX;
	list->pool_size = max_nodes;
}

static LLNodeID unlink( LLNode pool[], LLNodeID root[1], LLNodeID node_index )
{
	LLNode *node = pool + node_index;
	if ( node->prev == node->next ) {
		/* The list had only 1 node before but now it becomes completely empty */
		*root = LL_BAD_INDEX;
	} else {
		pool[ node->prev ].next = node->next;
		pool[ node->next ].prev = node->prev;
		if ( *root == node_index )
			*root = node->next;
	}
	return node_index;
}

static LLNodeID link( LLNode pool[], LLNodeID root_index[1], LLNodeID node_index )
{
	LLNode *node = pool + node_index;
	
	if ( *root_index == LL_BAD_INDEX )
	{
		/* List was empty */
		node->prev = node_index;
		node->next = node_index;
	}
	else
	{
		LLNode *root = pool + *root_index;
		pool[ root->prev ].next = node_index;
		node->prev = root->prev;
		node->next = *root_index;
		root->prev = node_index;
	}
	
	return *root_index = node_index;
}

LLNodeID add_node_x( LinkedList list[1], LLNodeID index )
{
	if ( list->length < list->pool_size ) {
		list->length += 1;
		return link( list->pool, &list->root, unlink( list->pool, &list->root_of_empty, index ) );
	}
	return LL_BAD_INDEX;
}

LLNodeID add_node_before( LinkedList list[1], LLNodeID before )
{
	LLNodeID new = LL_BAD_INDEX;
	if ( list->length < list->pool_size ) {
		list->length += 1;
		new = link( list->pool, &before, unlink( list->pool, &list->root_of_empty, list->root_of_empty ) );
	}
	return new;
}

void pop_node( LinkedList list[1], LLNodeID node )
{
	assert( list->length > 0 );
	list->length -= 1;
	
	/* Unlink a node from the "full" list and link that node to the "empty" list */
	link( list->pool, &list->root_of_empty,
		unlink( list->pool, &list->root, node ) );
}


