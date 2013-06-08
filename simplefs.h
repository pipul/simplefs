#ifndef __FS_SIMPLEFS_H__
#define __FS_SIMPLEFS_H__



/*
 * linux/fs/sfs/file.c
 *
 * Copyright (C) 2013
 * fangdong@pipul.org
 */

#include <linux/pagemap.h>
#include <linux/fs.h>

#define SIMPLEFS_ROOT_INO 0

#define SIMPLEFS_SUPER_BNO 0
#define SIMPLEFS_BITMAP_BNO 1

#define SIMPLEFS_BLOCKSIZE 512
#define SIMPLEFS_BLOCKBITS 9

struct simplefs_super {
	__le32 s_inode_bitmap_blknr;
	__le32 s_inode_blknr;
	__le32 s_free_inodes_count;

	__le32 s_block_bitmap_blknr;
	__le32 s_block_blknr;
	__le32 s_free_blocks_count;
};

struct simplefs_super_info {
	struct buffer_head *s_sb;
	struct buffer_head **s_bitmaps;
	struct simplefs_super raw_super;
};


#define SIMPLEFS_BLOCKS_PER_INODE 15
struct simplefs_inode {
	__le32 i_size;
	__le32 i_time;
	__le32 i_mode;
	__le32 i_nlink;
	__le32 i_data[SIMPLEFS_BLOCKS_PER_INODE];
};

struct simplefs_inode *simplefs_iget_raw(struct super_block *sb,
					 long ino, struct buffer_head **bh);
struct inode *simplefs_iget(struct super_block *sb, long ino);
void simplefs_truncate(struct inode *inode);
int simplefs_free_inode(struct inode *inode);
int simplefs_get_block(struct inode *inode,
		       sector_t block, struct buffer_head *bh_result, int create);
int simplefs_sync_inode(struct inode *inode);


static inline int inode_last_bytes(struct inode *inode, unsigned long page_nr) {
	unsigned last_bytes = PAGE_CACHE_SIZE;

	if (page_nr == (inode->i_size >> PAGE_CACHE_SHIFT))
		last_bytes = inode->i_size & (PAGE_CACHE_SIZE - 1);
	return last_bytes;
}


static inline unsigned long inode_pages(struct inode *inode) {
	return (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
}

void simplefs_put_page(struct page *page);
struct page *simplefs_get_page(struct inode *dir, unsigned long n);
struct simplefs_dentry *simplefs_find_dentry(struct inode *inode,
					     struct dentry *dentry, struct page **rs_page);
int simplefs_insert_dentry(struct dentry *dentry, struct inode *inode);
int simplefs_delete_dentry(struct simplefs_dentry *de, struct page *page);


#define SIMPLEFS_NAME_LEN 20

#define SIMPLEFS_DENTRY_UNUSED 0x01
#define SIMPLEFS_DENTRY_INUSED 0x02

struct simplefs_dentry {
	__le32 inode;
	__le32 flags;
        char name[SIMPLEFS_NAME_LEN];
};
#define SIMPLEFS_DENTRY_SIZE sizeof(struct simplefs_dentry)
#define SIMPLEFS_INODES_PER_BLOCK (SIMPLEFS_BLOCKSIZE / sizeof(struct simplefs_inode))
#define SIMPLEFS_DENTRYS_PER_BLOCK (SIMPLEFS_BLOCKSIZE / sizeof(struct simplefs_dentry))



/* bitmap.c */


struct buffer_head *bitmap_load(struct super_block *sb, sector_t block);
long bitmap_alloc_inode(struct super_block *sb);
void bitmap_free_inode(struct super_block *sb, long ino);
long bitmap_alloc_block(struct super_block *sb);
void bitmap_free_block(struct super_block *sb, long bno);




/*
 * Inodes and files operations
 */

/* dir.c */
extern const struct file_operations simplefs_dir_operations;
extern const struct inode_operations simplefs_dir_inode_operations;

/* file.c */
extern const struct file_operations simplefs_file_operations;
extern const struct inode_operations simplefs_file_inode_operations;


/* inode.c */
extern const struct address_space_operations simplefs_aops;


#endif /* __FS_SIMPLEFS_H__ */
