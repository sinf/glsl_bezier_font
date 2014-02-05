#ifndef _linkedlist_H
#define _linkedlist_H
#include "types.h"

typedef uint8 LLNodeID; /* sets a hard limit on linked list size */
#define LL_BAD_INDEX (~((LL_NodeID)0))

typedef struct {
	LLNodeID prev, next;
} LLNode;

typedef struct {
	LLNode *pool; /* pointer to the node pool */
	LLNodeID num_empty, /* how many nodes in the 'empty' list */
	num_full, /* how many nodes in the 'full' list */
	root_empty, /* root node index in the empty list */
	root_full, /* root node index in the full list */
	pool_size; /* maximum number of nodes in either list */
} LinkedList;

/* Used to create an empty list. Can use any memory (stack/heap/whatever)
Note: the actual data of nodes' is kept in a separate array and accessed via node indices */
void init_list( LinkedList list[1], LLNode *pool, LLNodeID max_nodes );

/* Removes a node from the list */
void pop_node( LinkedList list[1], LLNodeID node );

/* Adds a node. Returns LL_BAD_INDEX on failure */
LLNodeID add_node( LinkedList list[1] );

/**
void example()
{
	LLNode pool[1000]
	int data[1000]
	LinkedList list;
	LLNodeID n;
	
	init_list( &list, pool, 1000 )
	n = add_node( &list )
	if ( n == LL_BAD_INDEX ) {
		// not enough space in the pool
		exit with an error code
	}
	data[n] = 0
	
	...
	
	int x = data[n]
	pop_node( n )
}
**/

#endif
