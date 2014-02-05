#ifndef _linkedlist_H
#define _linkedlist_H
#include "types.h"

typedef uint16 LLNodeID; /* sets a hard limit on linked list size */
#define LL_BAD_INDEX 0xFFFF

typedef struct {
	LLNodeID prev, next;
} LLNode;

typedef struct {
	LLNode *pool; /* pointer to the node pool */
	LLNodeID
	num_nodes, /* how many nodes in the "full" list */
	root_index_empty, /* root node of the "empty" list. Equal to LL_BAD_INDEX if the list is full */
	root_index, /* root node of the "full" list. Use this to iterate the list. Equal to LL_BAD_INDEX if the list is empty */
	pool_size; /* maximum number of nodes in either list */
} LinkedList;

/* Used to create an empty list. Can use any memory (stack/heap/whatever)
Note: the actual data of nodes' is kept in a separate array and accessed via node indices */
void init_list( LinkedList list[1], LLNode *pool, LLNodeID max_nodes );

/* Removes a node from the list */
void pop_node( LinkedList list[1], LLNodeID node );

/* Moves a node at given index from the "empty" list to "full" list. The given index MUST belong to the "empty" list. Returns LL_BAD_INDEX if the list is already full */
LLNodeID add_node_x( LinkedList list[1], LLNodeID index );

/* Adds a new node. Returns LL_BAD_INDEX on failure */
#define add_node( list ) add_node_x( (list), (list)->root_index_empty )

#define LL_HAS_NODES( list ) (( (list).num_nodes != 0 ))
#define LL_PREV( list, node_index ) ((list).pool[(node_index)].prev)
#define LL_NEXT( list, node_index ) ((list).pool[(node_index)].next)

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
