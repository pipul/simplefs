/*
 * linux/fs/sfs/file.c
 *
 * Copyright (C) 2013
 * fangdong@pipul.org
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/buffer_head.h>
#include <asm-generic/bitops/find.h>
#include <linux/writeback.h>
#include "simplefs.h"

MODULE_LICENSE("Dual BSD/GPL");
static struct kmem_cache *simplefs_inode_cachep;

static const struct super_operations simplefs_super_operations;

static void init_once(void *foo) {
	struct inode *inode = foo;
	inode_init_once(inode);
}

static int init_inodecache(void) {
	simplefs_inode_cachep = kmem_cache_create("simplefs_inode_cache",
						  sizeof(struct inode),
						  0, (SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD), init_once);
	if (simplefs_inode_cachep == NULL) {
		return -ENOMEM;
	}
	return 0;
}

static void destroy_inodecache(void) {
	kmem_cache_destroy(simplefs_inode_cachep);
}


static int simplefs_fill_super(struct super_block *sb, void *data, int silent) {
	struct buffer_head *bh;
	struct simplefs_super *rsb;
	struct simplefs_super_info *sbi;
	struct inode *root;
	int i, j, cnt, ret = 0;

	printk(KERN_INFO "simplefs_fill_super\n");
	if (!(bh = sb_bread(sb, SIMPLEFS_SUPER_BNO))) {
		printk(KERN_ERR "Simplefs: unable to read superblock\n");
		return -ENOMEM;
	}
	sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
	if (!sbi) {
		ret = -ENOMEM;
		goto out;
	}
	sbi->s_sb = bh;
	memcpy(&sbi->raw_super, bh->b_data, sizeof(sbi->raw_super));
	sb->s_fs_info = sbi;
	sb->s_blocksize = SIMPLEFS_BLOCKSIZE;
	sb->s_flags = sb->s_flags & ~MS_POSIXACL;

	/*
	 * set up enough so that it can read an inode
	 */
	sb->s_op = &simplefs_super_operations;
	root = simplefs_iget(sb, SIMPLEFS_ROOT_INO);
	if (!root) {
		printk(KERN_ERR "Simplefs: corrupt root inode\n");
		ret = -EINVAL;
		goto failed_root;
	}
	printk(KERN_INFO "simplefs_fill_super -> simplefs_iget ok: %ld\n", root->i_ino);
	
	sb->s_root = d_alloc_root(root);
	if (!sb->s_root) {
		iput(root);
		printk(KERN_ERR "Simplefs: get root dentry failed\n");
		ret = -ENOMEM;
		goto failed_root;
	}
	printk(KERN_INFO "simplefs_fill_super -> d_alloc_root ok\n");

	
	cnt = sbi->raw_super.s_inode_bitmap_blknr + sbi->raw_super.s_block_bitmap_blknr;
	if (!(sbi->s_bitmaps = kzalloc(sizeof(struct buffer_head *) * cnt, GFP_KERNEL)))
		goto failed_bitmap;
	for (i = 0, j = 0; i < cnt; i++) {
		if (!(bh = bitmap_load(sb, SIMPLEFS_SUPER_BNO + 1 + i)))
			goto failed_load;
		sbi->s_bitmaps[j++] = bh;
		printk(KERN_INFO "simplefs_fill_super bitmap ok:%d -> %d\n", cnt, i);
	}
	rsb = &sbi->raw_super;
	printk("fill super ok: (inode %d %d %d) (block %d %d %d)\n",
	       rsb->s_inode_bitmap_blknr, rsb->s_inode_blknr, rsb->s_free_inodes_count,
	       rsb->s_block_bitmap_blknr, rsb->s_block_blknr, rsb->s_free_blocks_count);
	return 0;
 failed_load:
	for (i = 0; i < j; i++) {
		brelse(sbi->s_bitmaps[i]);
	}
 failed_bitmap:
	dput(sb->s_root);
 failed_root:
	kfree(sbi);
 out:
	brelse(bh);
	sbi->s_sb = NULL;
	printk("simplefs get sb failed.\n");
	return ret;
}

static int simplefs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name,
			   void *data, struct vfsmount *mnt) {
	printk(KERN_INFO "simplefs_get_sb\n");
	return get_sb_bdev(fs_type, flags, dev_name, data, simplefs_fill_super, mnt);
}

static void simplefs_put_sb(struct super_block *sb) {
	int i, cnt;
	struct simplefs_super_info *sbi = sb->s_fs_info;
	cnt = sbi->raw_super.s_inode_bitmap_blknr + sbi->raw_super.s_block_bitmap_blknr;
	printk(KERN_INFO "simplefs_put_sb: %d\n", cnt);
	if (sbi->s_sb)
		brelse(sbi->s_sb);
	if (sbi->s_bitmaps) {
		for (i = 0; i < cnt; i++) {
			brelse(sbi->s_bitmaps[i]);
		}
	}
	kfree(sbi);
	sb->s_fs_info = NULL;
}

static struct file_system_type simplefs_fs_type = {
	.owner = THIS_MODULE,
	.name = "simplefs",
	.get_sb = simplefs_get_sb,
	.kill_sb = kill_block_super,
	.fs_flags = FS_REQUIRES_DEV,
};




static int __init simplefs_init(void) {
	int err = init_inodecache();
	if (err)
		return err;
	err = register_filesystem(&simplefs_fs_type);
	if (err)
		goto out;
	printk("Simple file system register ok\n");
	return 0;
 out:
	destroy_inodecache();
	return err;
}

static void __exit simplefs_exit(void) {
	printk("Simple file system unregister\n");
	unregister_filesystem(&simplefs_fs_type);
	destroy_inodecache();
}

static void simplefs_delete_inode(struct inode *inode) {
	printk(KERN_INFO "simplefs_delete_inode: %ld\n", inode->i_ino);
	truncate_inode_pages(&inode->i_data, 0);
	inode->i_size = 0;
	simplefs_truncate(inode);
	simplefs_free_inode(inode);
}

static int simplefs_write_inode(struct inode *inode, struct writeback_control *wbc) {
	int err = 0;
	struct buffer_head *bh;
	struct simplefs_inode *raw_inode;

	printk(KERN_INFO "simplefs_write_inode: %ld\n", inode->i_ino);
	if (!(raw_inode = simplefs_iget_raw(inode->i_sb, inode->i_ino, &bh)))
		return -EIO;
	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_nlink = inode->i_nlink;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_time = inode->i_mtime.tv_sec;
	mark_buffer_dirty(bh);
	if (wbc->sync_mode == WB_SYNC_ALL && buffer_dirty(bh)) {
		sync_dirty_buffer(bh);
	}
	brelse(bh);
	return err;
}

static struct inode *simplefs_alloc_inode(struct super_block *sb) {
	printk(KERN_INFO "simplefs_alloc_inode\n");
	return kmem_cache_alloc(simplefs_inode_cachep, GFP_KERNEL);
}

static void simplefs_destroy_inode(struct inode *inode) {
	printk(KERN_INFO "simplefs_destroy_inode\n");
	kmem_cache_free(simplefs_inode_cachep, inode);
}

static const struct super_operations simplefs_super_operations = {
	.write_inode = simplefs_write_inode,
	.delete_inode = simplefs_delete_inode,
	.alloc_inode = simplefs_alloc_inode,
	.destroy_inode = simplefs_destroy_inode,
	.write_super = NULL,
	.put_super = simplefs_put_sb,
	.statfs = NULL,
	.remount_fs = NULL,
};



module_init(simplefs_init);
module_exit(simplefs_exit);
