#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/statfs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "clothesfs.h"

static struct kmem_cache *clothesfs_inode_cachep;


static inline struct clothesfs_inode_info *CLOTHESFS_I(struct inode *inode)
{
        return container_of(inode, struct clothesfs_inode_info, vfs_inode);
}

static inline struct clothesfs_sb_info *CLOTHESFS_SB(struct super_block *sb)
{
        return sb->s_fs_info;
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

static int clothesfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
        struct super_block *sb = dentry->d_sb;
	struct clothesfs_sb_info *sbi;

	sbi = CLOTHESFS_SB(sb);

	buf->f_type = CLOTHESFS_SUPER_MAGIC;
        buf->f_namelen = CLOTHESFS_MAX_FN_LEN;
        buf->f_bsize = sbi->blocksize;
        buf->f_bfree = buf->f_bavail = buf->f_ffree;
        buf->f_blocks = sbi->size / sbi->blocksize;
        buf->f_fsid.val[0] = (u32)sbi->vol_id;
        buf->f_fsid.val[1] = (u32)(sbi->vol_id >> 32);

	return 0;
}

static const struct super_operations clothesfs_super_ops = {
        .alloc_inode    = clothesfs_alloc_inode,
        .destroy_inode  = clothesfs_destroy_inode,
        .statfs         = clothesfs_statfs,
        .remount_fs     = clothesfs_remount,
};

static int clothesfs_read_super(struct super_block *sb,
	struct clothesfs_sb_info *sbi, struct clothesfs_super_block *csb)
{
	int error = -EINVAL;

	sbi->blocksize = le16_to_cpu(csb->blocksize);
	sbi->flags = csb->flags;
	sbi->grpindex = csb->grpindex;
	sbi->vol_id = le64_to_cpu(csb->vol_id);
	sbi->size = le64_to_cpu(csb->size);
	sbi->root = le32_to_cpu(csb->root);
	sbi->used = le32_to_cpu(csb->used);
	sbi->journal1 = le32_to_cpu(csb->journal1);
	sbi->journal2 = le32_to_cpu(csb->journal2);
	sbi->freechain = le32_to_cpu(csb->freechain);
	memcpy(sbi->name, csb->name, 32);
	sbi->name[32] = 0;

	if (!sbi->vol_id) {
		clothesfs_msg(sb, KERN_ERR, "Invalid volume id: %x", sbi->vol_id);
		goto out;
	}
	clothesfs_msg(sb, KERN_ERR, "DBG volume id: %x", sbi->vol_id);
	if (!sbi->root) {
		clothesfs_msg(sb, KERN_ERR, "Invalid root position: %x != %x", sbi->root, csb->root);
		goto out;
	}
	clothesfs_msg(sb, KERN_ERR, "DBG root position: %x", sbi->root);
	if (!sbi->size) {
		clothesfs_msg(sb, KERN_ERR, "Invalid size: %lu", sbi->size);
		goto out;
	}
	clothesfs_msg(sb, KERN_ERR, "DBG size: %lu", sbi->size);

	clothesfs_msg(sb, KERN_ERR, "DBG name: %s", sbi->name);

	error = 0;
out:
	return error;
}

/*
static const struct inode_operations clothesfs_dir_inode_operations = {
        .lookup = clothesfs_lookup,
};
*/


static struct inode *clothesfs_get_root(struct super_block *sb, int pos)
{
	struct clothesfs_inode_info *ci;
	umode_t mode;
	struct inode *inode = iget_locked(sb, pos);

	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (!(inode->i_state & I_NEW))
		return inode;


	inode->i_ino = get_next_ino();
	mode = S_IFDIR | 0755;

	set_nlink(inode, 1);
	inode->i_size = 0;
        inode->i_mtime.tv_sec = inode->i_atime.tv_sec = inode->i_ctime.tv_sec = 0;
        inode->i_mtime.tv_nsec = inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = 0;
	//inode->i_op = 

	ci = CLOTHESFS_I(inode);

	unlock_new_inode(inode);
	return inode;
}

int clothesfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct clothesfs_sb_info *sbi;
	struct clothesfs_super_block *csb;
	struct buffer_head *bh;
	struct inode *inode;
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
        sb->s_magic = le32_to_cpu(csb->id);

	if (sb->s_magic != CLOTHESFS_SUPER_MAGIC)
		goto cant_find_clothes;

	error = clothesfs_read_super(sb, sbi, csb);
	if (error)
		goto out_fail;

	brelse(bh);

	sb->s_maxbytes = MAX_LFS_FILESIZE;
	if (sb->s_blocksize != sbi->blocksize) {
		clothesfs_msg(sb, KERN_ERR, "Blocksize: %d != %d\n", sb->s_blocksize, sbi->blocksize);
		// TODO
	}

	inode = clothesfs_get_root(sb, csb->root);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		error = -ENOMEM;
		goto out_fail;
	}

	clothesfs_msg(sb, KERN_ERR, "All ok");
	goto out;

cant_find_clothes:
	clothesfs_msg(sb, KERN_ERR, "Can't find ClothesFS filesystem %x != %x != %x", sb->s_magic, csb->id, CLOTHESFS_SUPER_MAGIC);

out_fail:
	if (error == 0) error = -EINVAL;
	sb->s_fs_info = NULL;
	kfree(sbi);

out:
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
