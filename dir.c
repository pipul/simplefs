/*
 * linux/fs/sfs/file.c
 *
 * Copyright (C) 2013
 * fangdong@pipul.org
 */

#include <linux/buffer_head.h>
#include <asm-generic/errno-base.h>
#include "simplefs.h"

static int simplefs_readdir(struct file *filp, void *dirent, filldir_t filldir) {
	unsigned long pos = filp->f_pos;
	struct inode *inode = filp->f_path.dentry->d_inode;
	unsigned offset = pos & ~PAGE_CACHE_MASK;
	unsigned long n = pos >> PAGE_CACHE_SHIFT;
	unsigned long npages = inode_pages(inode);

	printk(KERN_INFO "simplefs_readdir\n");
	pos = (pos + SIMPLEFS_DENTRY_SIZE - 1) & ~(SIMPLEFS_DENTRY_SIZE - 1);
	if (pos >= inode->i_size)
		goto done;
	for ( ; n < npages; n++, offset = 0) {
		char *p, *kaddr, *limit;
		struct page *page = simplefs_get_page(inode, n);
		if (IS_ERR(page))
			continue;
		kaddr = (char *)page_address(page);
		p = kaddr + offset;
		limit = kaddr + inode_last_bytes(inode, n) - SIMPLEFS_DENTRY_SIZE;
		for ( ; p <= limit; p = p + SIMPLEFS_DENTRY_SIZE) {
			struct simplefs_dentry *de = (struct simplefs_dentry *)p;
			if (de->flags == SIMPLEFS_DENTRY_UNUSED)
				continue;
			if (de->inode) {
				int over;
				unsigned len = strnlen(de->name, SIMPLEFS_NAME_LEN);
				offset = p - kaddr;
				
				over = filldir(dirent, de->name, len,
					       (n << PAGE_CACHE_SHIFT) | offset, de->inode, DT_UNKNOWN);
				if (over) {
					simplefs_put_page(page);
					goto done;
				}
			}
		}
		simplefs_put_page(page);
	}
 done:
	filp->f_pos = (n << PAGE_CACHE_SHIFT) | offset;
	return 0;
}



const struct file_operations simplefs_dir_operations = {
	.llseek = generic_file_llseek,
	.read = generic_read_dir,
	.readdir = simplefs_readdir,
	.fsync = simple_fsync,
};








static void ext2_iput(struct dentry *dentry, struct inode *inode) {
	printk(KERN_INFO "d_iput %s %d\n", dentry->d_name.name, dentry->d_count.counter);
	printk(KERN_INFO "d_iput inode %d\n", inode->i_count.counter);
	iput(inode);
}

static int ext2_delete(struct dentry *dentry) {
	printk(KERN_INFO "d_delete %s\n", dentry->d_name.name);
	return 0;
}

const struct dentry_operations d_op = {
	.d_iput = ext2_iput,
	.d_delete = ext2_delete,
};

static struct dentry *simplefs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd) {
	struct inode *inode = NULL;
	struct page *pagep = NULL;
	struct simplefs_dentry *raw_de;
	long ino;

	dentry->d_op = dir->i_sb->s_root->d_op;
	if (dentry->d_name.len > SIMPLEFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);
	raw_de = simplefs_find_dentry(dir, dentry, &pagep);
	if (raw_de) {
		ino = raw_de->inode;
		simplefs_put_page(pagep);

		inode = simplefs_iget(dir->i_sb, ino);
		if (IS_ERR(inode))
			return ERR_CAST(inode);
	}
	//dentry->d_op = &d_op;
	if (inode) {
		printk(KERN_INFO "simplefs_lookup: dentry %d", dentry->d_count.counter);
		printk(KERN_INFO "simplefs_lookup: inode %ld %ld %d %d", inode->i_ino, inode->i_state, inode->i_count.counter, inode->i_nlink);
	}

	return d_splice_alias(inode, dentry);
}

static int simplefs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd) {
	struct inode *inode = NULL;
	long ino;
	int err = 0;

	ino = bitmap_alloc_inode(dir->i_sb);
	if (ino < 0)
		return -EIO;
	inode = simplefs_iget(dir->i_sb, ino);
	if (IS_ERR(inode))
		return -EIO;

	inode->i_state = 0;
	inode->i_mode = mode | S_IFREG;
	inode->i_nlink = 1;
	inode->i_size = inode->i_blocks = inode->i_bytes = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode);
	err = simplefs_insert_dentry(dentry, inode);
	if (!err) {
		//dentry->d_op = &d_op;
		d_instantiate(dentry, inode);
		if (inode) {
			printk(KERN_INFO "simplefs_create: dentry %d", dentry->d_count.counter);
			printk(KERN_INFO "simplefs_create: inode %ld %ld %d %d", inode->i_ino, inode->i_state, inode->i_count.counter, inode->i_nlink);
		}

		return 0;
	}
	inode_dec_link_count(inode);
	iput(inode);
	return err;
}

/*
static int simplefs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry) {
	struct inode *inode = old_dentry->d_inode;

	inode->i_ctime = CURRENT_TIME_SEC;
	inode_inc_link_count(inode);
	atomic_inc(&inode->i_count);

	
}

*/


static int simplefs_unlink(struct inode *dir, struct dentry *dentry) {
	struct page *rs_page = NULL;
	struct simplefs_dentry *raw_de;
	struct inode *inode = dentry->d_inode;
	int err = -ENOENT;
	
	printk(KERN_INFO "simplefs_unlink %s %d\n", dentry->d_name.name, dentry->d_count.counter);
	printk(KERN_INFO "simplefs_unlink %d %d \n", inode->i_count.counter, inode->i_nlink);
	raw_de = simplefs_find_dentry(dir, dentry, &rs_page);
	if (!raw_de)
		goto out;
	err = simplefs_delete_dentry(raw_de, rs_page);
	if (err)
		goto out;
	inode->i_ctime = dir->i_ctime;
	inode_dec_link_count(inode);
	printk(KERN_INFO "simplefs_unlink %s %d\n", dentry->d_name.name, dentry->d_count.counter);
	printk(KERN_INFO "simplefs_unlink %d %d \n", inode->i_count.counter, inode->i_nlink);
 out:
	return err;
}


static int simplefs_mkdir(struct inode *dir, struct dentry *dentry, int mode) {
	struct inode *inode = NULL;
	long ino;
	int err = 0;

	printk(KERN_INFO "simplefs_mkdir: %s %d\n", dentry->d_name.name, mode);
	inode_inc_link_count(dir);
	
	ino = bitmap_alloc_inode(dir->i_sb);
	if (ino < 0)
		return -EIO;
	inode = simplefs_iget(dir->i_sb, ino);
	if (IS_ERR(inode))
		return -EIO;

	// set inode operations
	inode->i_op = &simplefs_file_inode_operations;
	inode->i_fop = &simplefs_file_operations;
	inode->i_mapping->a_ops = &simplefs_aops;

	inode->i_mode = mode | S_IFDIR;
	inode->i_nlink = 2;
	inode->i_size = inode->i_blocks = inode->i_bytes = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;

	mark_inode_dirty(inode);
	err = simplefs_insert_dentry(dentry, inode);
	if (!err) {
		d_instantiate(dentry, inode);
		return 0;
	}

	inode_dec_link_count(inode);
	inode_dec_link_count(inode);
	iput(inode);
	inode_dec_link_count(dir);
	return err;
}


static int simplefs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry) {
	struct inode *inode = old_dentry->d_inode;
	int err;

	inode->i_ctime = CURRENT_TIME_SEC;
	inode_inc_link_count(inode);
	atomic_inc(&inode->i_count);
	err = simplefs_insert_dentry(dentry, inode);
	if (!err) {
		d_instantiate(dentry, inode);
		return 0;
	}
	inode_dec_link_count(inode);
	iput(inode);
	return err;
}

static int simplefs_rmdir(struct inode *dir, struct dentry *dentry) {
	return -ENOTEMPTY;
}

const struct inode_operations simplefs_dir_inode_operations = {
	.create = simplefs_create,
	.lookup = simplefs_lookup,
	.link = simplefs_link,
	.unlink = simplefs_unlink,
	.mkdir = simplefs_mkdir,
	.rmdir = simplefs_rmdir,
	.rename = NULL,
};
