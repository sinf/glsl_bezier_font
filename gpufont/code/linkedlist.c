#include <assert.h>
#include "linkedlist.h"

void init_list( LinkedList list[1], LLNode pool[], LLNodeID first_node, LLNodeID last_node )
{
	list->pool = pool;
	list->free_root_p = &list->free_root;
	list->length = 0;
	list->root = LL_BAD_INDEX;
	list->free_root = LL_BAD_INDEX;
	
	if ( last_node >= first_node && last_node != LL_BAD_INDEX )
	{
		LLNodeID n;
		
		list->free_root = first_node;
		
		pool[ first_node ].prev = last_node;
		pool[ first_node ].next = first_node + 1;
		
		pool[ last_node ].prev = last_node - 1;
		pool[ last_node ].next = first_node;
		
		for( n=first_node+1; n<last_node; n++ ) {
			pool[n].prev = n - 1;
			pool[n].next = n + 1;
		}
	}
}

LLNodeID unlink_node( LLNode pool[], LLNodeID root[1], LLNodeID node_index )
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

LLNodeID link_node( LLNode pool[], LLNodeID root_index[1], LLNodeID node_index )
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

LLNodeID add_node( LinkedList list[1], LLNodeID before_this_node )
{
	if ( *list->free_root_p == LL_BAD_INDEX )
		return LL_BAD_INDEX;
	
	list->length += 1;
	
	return link_node( list->pool,
		( before_this_node == LL_BAD_INDEX ) ? &list->root : &before_this_node,
		unlink_node( list->pool, list->free_root_p, *list->free_root_p )
	);
}

void pop_node( LinkedList list[1], LLNodeID node )
{
	assert( list->length > 0 );
	list->length -= 1;
	
	/* Unlink a node from the "full" list and link_node that node to the "empty" list */
	link_node( list->pool, list->free_root_p, unlink_node( list->pool, &list->root, node ) );
}
