#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <asm/ptrace.h>
#include "simplefs.h"

struct simplefs_inode *simplefs_iget_raw(struct super_block *sb, long ino,
					 struct buffer_head **bh) {
	struct simplefs_super_info *sbi = sb->s_fs_info;
	int block, bitmap_blocks;
	struct simplefs_inode *raw_inode;
	printk(KERN_INFO "simplefs_iget_raw\n");	
	if (ino >= sbi->raw_super.s_inode_blknr * SIMPLEFS_INODES_PER_BLOCK) {
		printk(KERN_ERR "Bad inode number on dev %s: %ld is out of range\n", sb->s_id, (long) ino);
		return NULL;
	}
	bitmap_blocks = sbi->raw_super.s_inode_bitmap_blknr + sbi->raw_super.s_block_bitmap_blknr;
	block = ino / SIMPLEFS_INODES_PER_BLOCK + bitmap_blocks + 1;
	printk(KERN_INFO "simplefs_iget_raw->sb_bread: %d\n", block);
	if (!(*bh = sb_bread(sb, block)))
		return NULL;
	raw_inode = (struct simplefs_inode *)(*bh)->b_data + ino % SIMPLEFS_INODES_PER_BLOCK;
	printk(KERN_INFO "simplefs_iget_raw ok: %ld\n", ino);
	return raw_inode;
}


struct inode *simplefs_inew(struct super_block *sb, long ino) {
	return NULL;
}


struct inode *simplefs_iget(struct super_block *sb, long ino) {
	struct buffer_head *bh;
	struct simplefs_inode *raw_inode;
	struct inode *inode;
	struct simplefs_super_info *sbi;

	printk(KERN_INFO "simplefs_iget %ld\n", ino);
	sbi = sb->s_fs_info;
	if (!(inode = iget_locked(sb, ino))) {
		printk(KERN_ERR "simplefs_iget failed: iget_locked failed\n");
		return NULL;
	}
	if (!(inode->i_state & I_NEW)) {
		printk("%ld is an old inode\n", ino);
		return inode;
	}

	if (!(raw_inode = simplefs_iget_raw(sb, ino, &bh))) {
		printk("simplefs_iget: failed to read raw_inode");
		goto failed_inode;
	}

	printk("simplefs_iget raw_inode info: %ld -> %d %d %d %d", ino, raw_inode->i_mode,
	       raw_inode->i_nlink, raw_inode->i_size, raw_inode->i_time);
	inode->i_mode = raw_inode->i_mode;
	inode->i_nlink = raw_inode->i_nlink;
	inode->i_size = raw_inode->i_size;
	inode->i_blocks = inode->i_size >> SIMPLEFS_BLOCKBITS;
	inode->i_bytes = inode->i_size % SIMPLEFS_BLOCKSIZE;
	inode->i_mtime.tv_sec = inode->i_atime.tv_sec = inode->i_ctime.tv_sec = raw_inode->i_time;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_nsec = 0;
	inode->i_atime.tv_nsec = 0;

	// set inode operations
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &simplefs_file_inode_operations;
		inode->i_fop = &simplefs_file_operations;
		inode->i_mapping->a_ops = &simplefs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &simplefs_dir_inode_operations;
		inode->i_fop = &simplefs_dir_operations;
		inode->i_mapping->a_ops = &simplefs_aops;
	}

	brelse(bh);
	unlock_new_inode(inode);
	return inode;

 failed_inode:
	iget_failed(inode);
	return NULL;
}


void simplefs_truncate(struct inode *inode) {
	struct buffer_head *bh = NULL;
	struct simplefs_inode *raw_inode;
	int i, blocks;

	printk(KERN_INFO "simplefs_truncate start: %ld\n", inode->i_ino);
	blocks = (inode->i_size + SIMPLEFS_BLOCKSIZE - 1) >> (SIMPLEFS_BLOCKBITS);
	if (!(raw_inode = simplefs_iget_raw(inode->i_sb, inode->i_ino, &bh))) {
		printk(KERN_ERR "simplefs_truncate failed: %ld\n", inode->i_ino);
		return;
	}
	for (i = 0; i < blocks; i++) {
		bitmap_free_block(inode->i_sb, raw_inode->i_data[i]);
	}
	inode->i_size = inode->i_blocks = inode->i_bytes = 0;
	raw_inode->i_size = 0;
	
	mark_buffer_dirty(bh);
	brelse(bh);
	printk(KERN_INFO "simplefs_truncate end: %ld\n", inode->i_ino);
	dump_stack();
	return;
}


int simplefs_free_inode(struct inode *inode) {
	bitmap_free_inode(inode->i_sb, inode->i_ino);
	printk(KERN_INFO "simplefs_free_inode ok: %ld\n", inode->i_ino);
	clear_inode(inode);
	return 0;
}

int simplefs_sync_inode(struct inode *inode) {
	struct writeback_control wbc = {
		.sync_mode = WB_SYNC_ALL,
		.nr_to_write = 0, /* sync_fsync did this */
	};
	return sync_inode(inode, &wbc);
}

int simplefs_get_block(struct inode *inode,
			      sector_t block, struct buffer_head *bh_result, int create) {
	struct buffer_head *bh;
	struct simplefs_inode *raw_inode;
	long bno;
	int err = -EIO;
	
	if (block >= SIMPLEFS_BLOCKS_PER_INODE) {
		printk(KERN_ERR "simplefs_get_block failed: too large block number\n");
		return -EINVAL;
	}
	if (!(raw_inode = simplefs_iget_raw(inode->i_sb, inode->i_ino, &bh))) {
		printk(KERN_ERR "simplefs_get_block failed: can't get raw inode\n");
		return -EIO;
	}
	if (create && block == (inode->i_size >> SIMPLEFS_BLOCKBITS)) {
		if ((bno = bitmap_alloc_block(inode->i_sb)) < 0)
			goto bitmap_failed;
		raw_inode->i_data[block] = bno;
		mark_buffer_dirty(bh);
		printk(KERN_INFO "simplefs_get_block create block : %ld %lld %lld %ld\n", inode->i_ino, inode->i_size, block, bno);
	}
	map_bh(bh_result, inode->i_sb, raw_inode->i_data[block]);
	brelse(bh);
	return 0;

 bitmap_failed:
	brelse(bh);
	return err;
}

static int simplefs_readpage(struct file *file, struct page *page) {
	printk(KERN_INFO "simplefs_readpage\n");
	return block_read_full_page(page, simplefs_get_block);
}

static int simplefs_writepage(struct page *page, struct writeback_control *wbc) {
	printk(KERN_INFO "simplefs_writepage\n");
	return block_write_full_page(page, simplefs_get_block, wbc);
}


static int simplefs_write_begin(struct file *file, struct address_space *mapping, loff_t pos,
				unsigned len, unsigned flags, struct page **pagep, void **fsdata) {
	*pagep = NULL;
	printk(KERN_INFO "simplefs_write_begin: pos:%lld len:%d\n", pos, len);
	return block_write_begin(file, mapping, pos, len, flags, pagep, fsdata, simplefs_get_block);
}

static sector_t simplefs_bmap(struct address_space *mapping, sector_t block) {
	printk(KERN_INFO "simplefs_bmap\n");
	return generic_block_bmap(mapping, block, simplefs_get_block);
}


const struct address_space_operations simplefs_aops = {
	.readpage = simplefs_readpage,
	.writepage = simplefs_writepage,
	.sync_page = block_sync_page,
	.write_begin = simplefs_write_begin,
	.write_end = generic_write_end,
	.bmap = simplefs_bmap,
};












void simplefs_put_page(struct page *page) {
	kunmap(page);
	page_cache_release(page);
}

struct page *simplefs_get_page(struct inode *dir, unsigned long n) {
	struct address_space *mapping = dir->i_mapping;
	struct page *page = read_mapping_page(mapping, n, NULL);

	if (!IS_ERR(page)) {
		kmap(page);
		if (!PageUptodate(page))
			goto fail;
	}
	return page;
 fail:
	simplefs_put_page(page);
	return ERR_PTR(-EIO);
}

/*
 * 0 : not same
 * 1 : the same
 */
static inline int namecompare(int len, int maxlen, const char *name, const char *buffer) {
	if (len < maxlen && buffer[len])
		return 0;
	return !memcmp(name, buffer, len);
}

struct simplefs_dentry *simplefs_find_dentry(struct inode *inode,
					     struct dentry *dentry, struct page **rs_page) {
	unsigned long n, npages = inode_pages(inode);

	printk(KERN_INFO "simplefs_find_dentry: %s\n", dentry->d_name.name);
	for (n = 0; n < npages; n++) {
		char *p, *kaddr, *limit;
		struct page *page = simplefs_get_page(inode, n);
		if (IS_ERR(page)) {
			printk(KERN_INFO "page error\n");
			continue;
		}
		p = kaddr = (char *)page_address(page);
		limit = kaddr + inode_last_bytes(inode, n) - SIMPLEFS_DENTRY_SIZE;
		for ( ; p <= limit; p += SIMPLEFS_DENTRY_SIZE) {
			struct simplefs_dentry *de = (struct simplefs_dentry *) p;
			if (de->flags == SIMPLEFS_DENTRY_UNUSED) {
				printk(KERN_INFO "found an unused dentry: %s\n", de->name);
				continue;
			}
			if (namecompare(dentry->d_name.len, SIMPLEFS_NAME_LEN,
					dentry->d_name.name, de->name)) {
				printk(KERN_INFO "match dentry: %s %s\n", dentry->d_name.name, de->name);
				*rs_page = page;
				return de;
			}
		}
		simplefs_put_page(page);
	}
	return NULL;
}

int simplefs_insert_dentry(struct dentry *dentry, struct inode *inode) {
	struct inode *dir = dentry->d_parent->d_inode;
	const char *name = dentry->d_name.name;
	char *kaddr;
	int namelen = dentry->d_name.len;
	struct page *page;
	struct simplefs_dentry de = {};
	int pos, err = 0;

	printk(KERN_INFO "simplefs_insert_dentry\n");
	page = simplefs_get_page(dir, dir->i_size >> PAGE_CACHE_SHIFT);
	if (IS_ERR(page)) {
		printk(KERN_ERR "simplefs_insert_dentry failed: i/o error\n");
		return -EIO;
	}

	lock_page(page);
	kaddr = (char *)page_address(page) + (dir->i_size & ~PAGE_CACHE_MASK);

	pos = page_offset(page) + kaddr - (char *)page_address(page);
	block_write_begin(NULL, page->mapping, pos, sizeof(de), 0, &page, NULL, simplefs_get_block);
	de.inode = inode->i_ino;
	de.flags = SIMPLEFS_DENTRY_INUSED;
	memcpy(&de.name, name, namelen);
	memcpy(kaddr, &de, sizeof(de));
	block_write_end(NULL, page->mapping, pos, sizeof(de), sizeof(de), page, NULL);

	i_size_write(dir, pos + sizeof(de));
	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty(dir);
	
	if (IS_DIRSYNC(dir)) {
		err = write_one_page(page, 1);
		if (!err)
			err = simplefs_sync_inode(dir);
	} else
		unlock_page(page);
	simplefs_put_page(page);

	printk("simplefs insert dentry: %d\n", err);
	return err;
}


int simplefs_delete_dentry(struct simplefs_dentry *de, struct page *page) {
	struct address_space *mapping = page->mapping;
	struct inode *inode = (struct inode *)mapping->host;
	char *kaddr = page_address(page);
	loff_t pos = page_offset(page) + (char *)de - kaddr;
	int err = 0;

	printk(KERN_INFO "simplefs_delete_dentry: %s\n", de->name);
	lock_page(page);
	err = block_write_begin(NULL, mapping, pos, sizeof(*de), 0, &page, NULL, simplefs_get_block);
	BUG_ON(err);

	de->flags = SIMPLEFS_DENTRY_UNUSED;
	block_write_end(NULL, mapping, pos, sizeof(*de), sizeof(*de), page, NULL);
	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode);

	if (IS_DIRSYNC(inode)) {
		err = write_one_page(page, 1);
		if (!err)
			err = simplefs_sync_inode(inode);
	} else
		unlock_page(page);

	simplefs_put_page(page);
	return err;
}
