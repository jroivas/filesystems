/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _CLOTHESFS_H
#define _CLOTHESFS_H

#include <linux/fs.h>
#include <asm/byteorder.h>

#define CLOTHESFS_SUPER_MAGIC     0x41004200
#define CLOTHESFS_MAX_FN_LEN      100
#define CLOTHESFS_BASE_BLOCK_SIZE 512

#define CLOTHESFS_META_ID         0x0042
#define CLOTHESFS_PAYLOAD_ID      0x4242
#define CLOTHESFS_META_FILE       0x02
#define CLOTHESFS_META_DIR        0x04
#define CLOTHESFS_META_FILE_EXT   0x08
#define CLOTHESFS_META_DIR_EXT    0x10

#define CLOTHESFS_PAYLOAD_FREE    0x00
#define CLOTHESFS_PAYLOAD_USED    0x01
#define CLOTHESFS_PAYLOAD_FREED   0x02


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
	__u32 root;
	__u32 used;
	__u32 journal1;
	__u32 journal2;
	__u32 freechain;
} __packed;

struct clothesfs_meta_block {
	__u16 id;
	__u8  type;
	__u8  attrib;
	__u64 size;
	__u32 namelen;
	union {
		__u32 payload[123];
		__u8  name[492];
	};
	__u32 ptr;
} __packed;

struct clothesfs_payload_block {
	__u16 id;
	__u8  type;
	__u8  algo;
	union {
		__u32 check;
		__u8  data[508];
	};
} __packed;

struct clothesfs_sb_info {
	unsigned short blocksize;
	unsigned char flags;
	unsigned char grpindex;
	unsigned long vol_id;
	unsigned long size;
	char name[33];
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
	unsigned int pos;
};


void clothesfs_msg(struct super_block *sb,
	const char *level, const char *fmt, ...);


#endif /* !_CLOTHESFS_H */
