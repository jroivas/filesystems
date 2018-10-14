#ifndef _CLOTHESFS_H
#define _CLOTHESFS_H

#include <linux/fs.h>
#include <asm/byteorder.h>

#define CLOTHESFS_SUPER_MAGIC 0x00420041

struct clothesfs_super_block {
	__u64 jump1;
	__u64 jump2;
	__u64 jump3;
	__u64 jump4;
	__u32 id;
	__u16 blocksize;
	__u8  flags;
	__u8  grpindex;
	__u64 vol_id;
	__u64 size;
	__u8  name[32];
	__u64 group_id;
	__u32 root;
	__u32 used;
	__u32 journal1;
	__u32 journal2;
	__u32 freechain;
};

struct clothesfs_sb_info {
	unsigned short blocksize;
	unsigned char flags;
	unsigned char grpindex;
	unsigned long vol_id;
	unsigned long size;
	char name[32];
	unsigned long group_id;
	unsigned int root;
	unsigned int used;
	unsigned int journal1;
	unsigned int journal2;
	unsigned int freechain;

        struct mutex s_lock;
	struct clothesfs_super_block *super;
};

struct clothesfs_inode_info {
	struct inode vfs_inode;
	// TODO
};


void clothesfs_msg(struct super_block *sb, const char *level, const char *fmt, ...);


#endif /* !_CLOTHESFS_H */
