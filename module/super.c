#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "clothesfs.h"

static struct kmem_cache *clothesfs_inode_cachep;


static inline struct clothesfs_inode_info *CLOTHESFS_I(struct inode *inode)
{
        return container_of(inode, struct clothesfs_inode_info, vfs_inode);
}

static struct inode *clothesfs_alloc_inode(struct super_block *sb)
{
        struct clothesfs_inode_info *inode;

        inode = kmem_cache_alloc(clothesfs_inode_cachep, GFP_KERNEL);
        return inode ? &inode->vfs_inode : NULL;
}

static void clothesfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(clothesfs_inode_cachep, CLOTHESFS_I(inode));
}

static void clothesfs_destroy_inode(struct inode *inode)
{
        call_rcu(&inode->i_rcu, clothesfs_i_callback);
}

static int clothesfs_remount(struct super_block *sb, int *flags, char *data)
{
        sync_filesystem(sb);
        *flags |= SB_RDONLY;
        return 0;
}


static const struct super_operations clothesfs_super_ops = {
        .alloc_inode    = clothesfs_alloc_inode,
        .destroy_inode  = clothesfs_destroy_inode,
/*
        .statfs         = clothesfs_statfs,
*/
        .remount_fs     = clothesfs_remount,
};

int clothesfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct clothesfs_sb_info *sbi;
	struct clothesfs_super_block *csb;
	struct buffer_head *bh;
	long error;

        sbi = kzalloc(sizeof(struct clothesfs_sb_info), GFP_KERNEL);
        if (!sbi)
                return -ENOMEM;

        sb->s_fs_info = sbi;

        sb->s_flags |= SB_RDONLY;
	sb->s_flags |= SB_NOATIME;
        sb->s_flags |= SB_NODIRATIME;

        sb->s_op = &clothesfs_super_ops;
        mutex_init(&sbi->s_lock);


	sb_min_blocksize(sb, 512);
	bh = sb_bread(sb, 0);
	error = -EIO;
        if (bh == NULL) {
		clothesfs_msg(sb, KERN_ERR, "unable to read boot sector");
		goto out_fail;
        }

	csb = (struct clothesfs_super_block *)(bh->b_data);
	sbi->super = csb;
        sb->s_magic = be32_to_cpu(csb->id);

	if (sb->s_magic != CLOTHESFS_SUPER_MAGIC)
		goto cant_find_clothes;

	//clothesfs_read_

	goto temp_fail;
	return 0;

temp_fail:
	clothesfs_msg(sb, KERN_ERR, "Temporary fail, all ok");
	goto out_fail;

cant_find_clothes:
	clothesfs_msg(sb, KERN_ERR, "Can't find ClothesFS filesystem %x != %x != %x", sb->s_magic, csb->id, CLOTHESFS_SUPER_MAGIC);

out_fail:
        sb->s_fs_info = NULL;
        kfree(sbi);
        return error;
}


static struct dentry *clothesfs_mount(struct file_system_type *fs_type,
			int flags, const char *dev_name,
                        void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, clothesfs_fill_super);
}

static void clothesfs_init_once(void *_inode)
{
        struct clothesfs_inode_info *inode = _inode;
        inode_init_once(&inode->vfs_inode);
}

static struct file_system_type clothesfs_fs_type = {
        .owner          = THIS_MODULE,
        .name           = "clothesfs",
        .mount          = clothesfs_mount,
        .kill_sb        = kill_block_super,
        .fs_flags       = FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("clothesfs");

static int __init init_clothesfs_fs(void)
{
        clothesfs_inode_cachep =
                kmem_cache_create("clothesfs_inode_cache",
                                  sizeof(struct clothesfs_inode_info), 0,
                                  SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD |
                                  SLAB_ACCOUNT, clothesfs_init_once);

        if (!clothesfs_inode_cachep) {
                pr_err("Failed to initialise inode cache\n");
                return -ENOMEM;
        }

	return register_filesystem(&clothesfs_fs_type);
}

static void __exit exit_clothesfs_fs(void)
{
	unregister_filesystem(&clothesfs_fs_type);
}

module_init(init_clothesfs_fs)
module_exit(exit_clothesfs_fs)


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jouni Roivas <jroivas@iki.fi>");
MODULE_DESCRIPTION("ClothesFS filesystem support (read-only)");
