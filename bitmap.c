#include <linux/buffer_head.h>
#include <asm-generic/bitops/find.h>
#include "simplefs.h"

struct buffer_head *bitmap_load(struct super_block *sb, sector_t block) {
	struct buffer_head *bh = sb_bread(sb, block);
	if (bh)
		
		printk(KERN_INFO "bitmap_load: %d\n", *(int *)bh->b_data);
	else
		printk(KERN_INFO "bitmap_load failed\n");
	return bh;
}

static int bitmap_alloc_bit(struct buffer_head *bh) {
	int nr = -1;

	nr = find_first_zero_bit((unsigned long *)bh->b_data, bh->b_size * 8);
	//int nr = minix_find_first_zero_bit((unsigned long *)bh->b_data, bh->b_size * 8);
	if (nr < 0) {
		printk(KERN_ERR "bitmap_alloc_bit failed\n");
		goto out;
	}
	set_bit(nr, (unsigned long *)bh->b_data);

	//minix_set_bit(nr, (unsigned long *)bh->b_data);
	mark_buffer_dirty(bh);
	printk(KERN_WARNING "bitmap_alloc_bit ok: %d\n", nr);
 out:
	return nr;
}

static void bitmap_free_bit(struct buffer_head *bh, int nr) {
	clear_bit(nr, (unsigned long *)bh->b_data);
	//minix_test_and_clear_bit(nr, (unsigned long *)bh->b_data);
	mark_buffer_dirty(bh);
	printk(KERN_WARNING "bitmap_free_bit ok: %d\n", nr);
}

long bitmap_alloc_inode(struct super_block *sb) {
	struct simplefs_super_info *sbi = sb->s_fs_info;
	int i;
	long ino = -1;

	for (i = 0; i < sbi->raw_super.s_inode_bitmap_blknr; i++) {
		ino = bitmap_alloc_bit(sbi->s_bitmaps[i]);
		if (ino >= 0) {
			ino += i * SIMPLEFS_BLOCKSIZE * 8;
			break;
		}
	}
	printk(KERN_WARNING "bitmap_alloc_inode ok: %ld\n", ino);
	return ino;
}

void bitmap_free_inode(struct super_block *sb, long ino) {
	struct simplefs_super_info *sbi = sb->s_fs_info;
	int i;

	i = ino / (SIMPLEFS_BLOCKSIZE * 8);
	ino -= (i * SIMPLEFS_BLOCKSIZE * 8);
	bitmap_free_bit(sbi->s_bitmaps[i], ino);
	printk(KERN_WARNING "bitmap_free_inode ok: %ld\n", ino);
}


long bitmap_alloc_block(struct super_block *sb) {
	struct simplefs_super_info *sbi = sb->s_fs_info;
	int i, ino_bitmap_blknr, bno_bitmap_blknr;
	long bno = -1, real_bno = -1;

	ino_bitmap_blknr = sbi->raw_super.s_inode_bitmap_blknr;
	bno_bitmap_blknr = sbi->raw_super.s_block_bitmap_blknr;
	for (i = 0; i < bno_bitmap_blknr; i++) {
		bno = bitmap_alloc_bit(sbi->s_bitmaps[i + ino_bitmap_blknr]);
		if (bno >= 0) {
			bno += i * SIMPLEFS_BLOCKSIZE * 8;
			real_bno = bno + SIMPLEFS_SUPER_BNO + 1 + ino_bitmap_blknr
				+ bno_bitmap_blknr + sbi->raw_super.s_inode_blknr;
			break;
		}
	}
	printk(KERN_WARNING "bitmap_alloc_block ok: %ld\n", real_bno);
	return real_bno;
}

void bitmap_free_block(struct super_block *sb, long real_bno) {
	struct simplefs_super_info *sbi = sb->s_fs_info;
	int i, ino_bitmap_blknr, bno_bitmap_blknr;
	long bno = -1;

	ino_bitmap_blknr = sbi->raw_super.s_inode_bitmap_blknr;
	bno_bitmap_blknr = sbi->raw_super.s_block_bitmap_blknr;
	bno = real_bno - sbi->raw_super.s_inode_blknr -
		ino_bitmap_blknr - bno_bitmap_blknr - SIMPLEFS_SUPER_BNO - 1;
	i = bno / (SIMPLEFS_BLOCKSIZE * 8);
	bno -= (i * SIMPLEFS_BLOCKSIZE * 8);
	bitmap_free_bit(sbi->s_bitmaps[i + ino_bitmap_blknr], bno);
	printk(KERN_WARNING "bitmap_free_block ok: %ld -> %ld\n", real_bno, bno);
}
