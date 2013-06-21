// Beneath C Level - Rootkit tutorial
//
// hook.h - The header file for the function hooking code
//
// Author: Nick Newson
// Website: http://beneathclevel.blogspot.co.uk/
//
// Tab size = 4

#ifndef __KM_HOOK_H__
#define __KM_HOOK_H__

// Type Declarations
typedef struct km_hook				km_hook;

// Function Declarations
km_hook*							km_hook_init(
										void* const				original_func,
										const unsigned			prolog_offset,
										void* const				new_func );

void								km_hook_release(
										km_hook* const			hook );

void								km_hook_release_all(
										void );

									// The calling code should ensure cr0 write protection disable/enable sandwiches these functions
void								km_hook_apply(
										km_hook* const			hook );

void								km_hook_apply_all(
										void );

void								km_hook_remove(
										km_hook* const			hook );

void								km_hook_remove_all(
										void );

#endif // __KM_HOOK_H__ 
