#ifndef VFS_H
#define VFS_H

#include "../../cpu/types.h"


/* VFS — thin path-resolution layer over KFS.
 * All paths are absolute (start with '/').
 * Max path length: 128 chars.
 */

/* List directory contents to screen. */
void vfs_ls(const char *path);

/* Print file contents to screen. */
void vfs_cat(const char *path);

/* Create an empty file. Returns 0 on success. */
int vfs_touch(const char *path);

/* Create a directory. Returns 0 on success. */
int vfs_mkdir(const char *path);

/* Write string `data` to file at `path` (overwrites). Returns 0 on success. */
int vfs_write(const char *path, const char *data);

/* Remove a file or empty directory. Returns 0 on success. */
int vfs_rm(const char *path);

/* Programmatic API for GUI Directory Listing 
 * Fills `names` (array of 28-byte strings) and `types` with up to `max_items`.
 * Returns the number of items populated, or -1 if path not found. */
int vfs_list_dir(const char *path, char names[][28], u8 types[], int max_items);

/* Programmatic API to read file contents directly into a buffer.
 * Returns bytes read, or -1 if path not found or not a file. */
int vfs_read_into(const char *path, char *buf, int max_len);

#endif
