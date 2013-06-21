// Beneath C Level - Rootkit tutorial
//
// hook.c - The function hook handling code
//
// Author: Nick Newson
// Website: http://beneathclevel.blogspot.co.uk/
//
// Tab size = 4

#include "hook.h"

#include <linux/list.h>
#include <linux/slab.h>

// Type Declarations
typedef struct list_head			list_head;

// Type Definitions
struct km_hook
{
	list_head						list;

	void*							original_func;
	unsigned						prolog_offset;

	void*							new_func;
	void*							trampoline_func;
};

// Globals
static								LIST_HEAD( g_hooks );

// Function Definitions
km_hook*
km_hook_init(
	void* const				original_func,
	const unsigned			prolog_offset,
	void* const				new_func 
)
{
	km_hook*				hook = kmalloc( sizeof( km_hook ), GFP_KERNEL );

	// Setup the hook structure
	hook->original_func = original_func;
	hook->prolog_offset = prolog_offset;

	hook->new_func = new_func;
	hook->trampoline_func = NULL;

	list_add( &g_hooks, &hook->list );

	return hook;
}

void
km_hook_release(
	km_hook* const			hook
)
{
	if ( hook->trampoline_func )
	{
		km_hook_remove( hook );
	}

	list_del( &hook->list );
	kfree( hook );
}

void
km_hook_release_all(
	void
)
{
	km_hook*				hook;

	// Since we're going to free the list entry we can't use the standard list iterator
	for ( hook = list_entry( g_hooks.next, km_hook, list ) ; &hook->list != &g_hooks ; )
	{
		km_hook*			old_hook = hook;
		hook = list_entry( hook->list.next, km_hook, list );

		km_hook_release( old_hook );
	}
}

void
km_hook_apply(
	km_hook* const			hook
)
{
}

void
km_hook_apply_all(
	void
)
{
	km_hook*				hook;

    list_for_each_entry( hook, &g_hooks, list ) 
	{
		km_hook_apply( hook );
	}
}

void
km_hook_remove(
	km_hook* const			hook
)
{
	hook->trampoline_func = NULL;
}

void
km_hook_remove_all(
	void
)
{
	km_hook*				hook;

    list_for_each_entry( hook, &g_hooks, list ) 
	{
		km_hook_remove( hook );
	}
}
