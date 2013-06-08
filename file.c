/*
 * linux/fs/sfs/file.c
 *
 * Copyright (C) 2013
 * fangdong@pipul.org
 */

#include "simplefs.h"

const struct file_operations simplefs_file_operations = {
	.llseek = generic_file_llseek,
	.read = do_sync_read,
	.write = do_sync_write,
	.aio_read = generic_file_aio_read,
	.aio_write = generic_file_aio_write,
	.mmap = generic_file_mmap,
	.fsync = simple_fsync,
};


const struct inode_operations simplefs_file_inode_operations = {
	.truncate = simplefs_truncate,
};
