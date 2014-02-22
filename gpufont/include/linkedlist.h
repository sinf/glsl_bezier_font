#ifndef _LINKEDLIST_H
#define _LINKEDLIST_H
#include <stddef.h>

typedef unsigned short LLNodeID; /* sets a hard limit on linked list size */
#define LL_BAD_INDEX 0xFFFF

/* Note: the list is cyclic. Both prev and next will point to the same node if the list has only one node */
typedef struct {
	LLNodeID prev, next;
} LLNode;

typedef struct {
	LLNode *pool; /* pointer to the node pool */
	LLNodeID *free_root_p; /* Pointer to root index of the "empty" list. Can be used to make multiple lists share the same pool. Equals LL_BAD_INDEX if the pool has no free slots */
	LLNodeID
	root, /* root node of the "full" list. Use this to iterate the list. Equal to LL_BAD_INDEX if the list is empty. Start iterating the list from this node */
	length, /* how many nodes in the "full" list */
	free_root; /* If this list is the only user of the pool, then root_of_empty points to this. Otherwise this is unused */
} LinkedList;

/* Used to create an empty list. Can use any memory (stack/heap/whatever)
Note: the actual data of nodes' is kept in a separate array and accessed via node indices
To use the entire pool, set first_node=0 and last_node=(size of the pool minus one) */
void init_list( LinkedList list[1], LLNode pool[], size_t first_node, size_t last_node );

/* Removes a node from the list */
void pop_node( LinkedList list[1], LLNodeID node );

/* Moves a node from the "free" list to the "used" list. The new node is linked before <before_this_node> if it isn't LL_BAD_INDEX.
Returns LL_BAD_INDEX if the "free" list is empty (=the pool is exhausted) */
LLNodeID add_node( LinkedList list[1], LLNodeID before_this_node );

/* Used to get next and previous node indices */
#define LL_PREV( list, node_index ) ((list).pool[(node_index)].prev)
#define LL_NEXT( list, node_index ) ((list).pool[(node_index)].next)

/* mostly internal use only */
LLNodeID unlink_node( LLNode pool[], LLNodeID root[1], LLNodeID used_node );
LLNodeID link_node( LLNode pool[], LLNodeID root[1], LLNodeID unused_node );

#endif
