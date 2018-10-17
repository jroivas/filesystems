#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/statfs.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "clothesfs.h"

static struct kmem_cache *clothesfs_inode_cachep;

static struct inode *clothesfs_get_inode(struct super_block *sb, unsigned int pos, unsigned int entry_type, size_t entry_size);

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

	u64 id = huge_encode_dev(sb->s_bdev->bd_dev);

	clothesfs_msg(sb, KERN_ERR, "Statfs ");
	sbi = CLOTHESFS_SB(sb);

	buf->f_type = CLOTHESFS_SUPER_MAGIC;
        buf->f_namelen = CLOTHESFS_MAX_FN_LEN;
        buf->f_bsize = sbi->blocksize;
        buf->f_bfree = buf->f_bavail = buf->f_ffree;
        buf->f_blocks = sbi->size / sbi->blocksize;
        buf->f_fsid.val[0] = (u32)id;
        buf->f_fsid.val[1] = (u32)(id >> 32);
        //buf->f_fsid.val[0] = (u32)sbi->vol_id;
        //buf->f_fsid.val[1] = (u32)(sbi->vol_id >> 32);

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

static int clothesfs_block_read(struct super_block *sb, unsigned int pos,
	void *buf, size_t buflen)
{
	struct buffer_head *bh;
	unsigned int offset;
	size_t block_cnt;

	block_cnt = buflen / CLOTHESFS_BASE_BLOCK_SIZE;
	offset = 0;
	clothesfs_msg(sb, KERN_ERR, "Read from %x, %d bytes", pos, buflen);

	while (block_cnt > 0) {
		bh = sb_bread(sb, pos);
		if (bh == NULL)
			return -EIO;
		memcpy(buf, bh->b_data + offset, buflen);
		brelse(bh);

		offset += CLOTHESFS_BASE_BLOCK_SIZE;
		pos++;
		block_cnt--;
	}

	return 0;
}

static int clothesfs_emit_dir_block(struct dir_context *ctx, struct super_block *sb, struct clothesfs_meta_block *meta)
{
	int error = 0;
	char buf[sb->s_blocksize];
	struct clothesfs_meta_block *entry_meta;

	unsigned int ipos = 0;
	unsigned int type;
	int i;
	char *fname;
	unsigned int namelen;
	int index;
	struct inode *inode;
	namelen = meta->namelen;
	index = le32_to_cpu(namelen) / 4;
	clothesfs_msg(sb, KERN_ERR, "FF: %d, %d, %d", index, namelen, meta->namelen);

	if (namelen % 4 != 0)
		++index;

	clothesfs_msg(sb, KERN_ERR, "Iter from: %d, %d", index, namelen);
	clothesfs_msg(sb, KERN_ERR, "Folder name: %s", meta->name);
	for (i = index; i < 123; ++i) {
		ipos = meta->payload[i];
		clothesfs_msg(sb, KERN_ERR, "Pos: %d, %d", i, ipos);
		if (ipos == 0)
			break;

		ctx->pos = ipos;
		error = clothesfs_block_read(sb, ipos, &buf, sb->s_blocksize);
		if (error)
			goto out;

		entry_meta = (struct clothesfs_meta_block *)buf;
		if (entry_meta->type == CLOTHESFS_META_DIR
			|| entry_meta->type == CLOTHESFS_META_DIR_EXT) {
			type = DT_DIR;
		} else if (entry_meta->type == CLOTHESFS_META_FILE
			|| entry_meta->type == CLOTHESFS_META_FILE_EXT) {
			type = DT_REG;
		} else {
			clothesfs_msg(sb, KERN_ERR, "Invalid entry type %x", entry_meta->type);
			error = -EINVAL;
			goto out;
		}
		if (entry_meta->namelen <= 0) {
			clothesfs_msg(sb, KERN_ERR, "Invalid name len: %d", entry_meta->namelen);
			error = -EINVAL;
			goto out;
		}

		fname = kzalloc(entry_meta->namelen + 1, GFP_KERNEL);
		if (fname == NULL) {
			error = -ENOMEM;
			goto out;
		}
		memcpy(fname, entry_meta->name, entry_meta->namelen);
		fname[entry_meta->namelen] = 0;

		inode = clothesfs_get_inode(sb, ipos, entry_meta->type, entry_meta->size);
		if (inode == NULL) {
			error = -EINVAL;
			goto out;
		}

		clothesfs_msg(sb, KERN_ERR, "DIRFound: %s, %d, inode: %d", fname, ipos, inode->i_ino);
		if (!dir_emit(ctx, fname, entry_meta->namelen, inode->i_ino, type)) {
			clothesfs_msg(sb, KERN_ERR, "Invalid entry at: %d", i);
			error = -EINVAL;
			goto out;
		}
	}

out:
	clothesfs_msg(sb, KERN_ERR, "Return err: %d", error);
	return error;
}

static int clothesfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *i = file_inode(file);
	unsigned long offset;
	struct clothesfs_inode_info *ci;
	int error;
	unsigned short id;
	char buf[i->i_sb->s_blocksize];
	struct clothesfs_meta_block *meta;

	ci = CLOTHESFS_I(i);

	clothesfs_msg(i->i_sb, KERN_ERR, "--- Read from %x %d", ci->pos, i->i_ino);
	offset = ci->pos;

	while (offset < CLOTHESFS_SB(i->i_sb)->size / i->i_sb->s_blocksize) {
		clothesfs_msg(i->i_sb, KERN_ERR, "Offs %d", offset);
		ctx->pos = offset;

		error = clothesfs_block_read(i->i_sb, offset, &buf, i->i_sb->s_blocksize);
		if (error)
			goto readdir_error;
		error = -ENOENT;
		meta = (struct clothesfs_meta_block *)&buf;

		id = le16_to_cpu(meta->id);
		clothesfs_msg(i->i_sb, KERN_ERR, "Got id %x", id);
		if (id != CLOTHESFS_META_ID)
			goto readdir_error;

		clothesfs_msg(i->i_sb, KERN_ERR, "Got type %x", meta->type);
		if (meta->type != CLOTHESFS_META_DIR && meta->type != CLOTHESFS_META_DIR_EXT)
			break;

		clothesfs_msg(i->i_sb, KERN_ERR, "Got attr %x", meta->attrib);
		clothesfs_msg(i->i_sb, KERN_ERR, "Got len %x", meta->namelen);

		error = clothesfs_emit_dir_block(ctx, i->i_sb, meta);
		if (error)
			goto readdir_error;

		// No continuation block
		if (meta->ptr == 0)
			break;
		offset = meta->ptr;
	}

	return 0;

readdir_error:
	clothesfs_msg(i->i_sb, KERN_ERR, "Got error %d", error);
	return error;
}

static int clothesfs_find_entry_from_dir(struct super_block *sb, struct clothesfs_meta_block *meta, const struct qstr *child, struct inode **inode)
{
	int error = 0;
	char buf[sb->s_blocksize];
	struct clothesfs_meta_block *entry_meta;

	unsigned int ipos = 0;
	unsigned int type;
	int i;
	unsigned int namelen;
	int index;
	namelen = meta->namelen;
	index = le32_to_cpu(namelen) / 4;

	if (namelen % 4 != 0)
		++index;

	for (i = index; i < 123; ++i) {
		ipos = meta->payload[i];
		if (ipos == 0)
			break;

		error = clothesfs_block_read(sb, ipos, &buf, sb->s_blocksize);
		entry_meta = (struct clothesfs_meta_block *)buf;
		if (entry_meta->type == CLOTHESFS_META_DIR
			|| entry_meta->type == CLOTHESFS_META_DIR_EXT)
			type = DT_DIR;
		else if (entry_meta->type == CLOTHESFS_META_FILE
			|| entry_meta->type == CLOTHESFS_META_FILE_EXT)
			type = DT_REG;
		else {
			clothesfs_msg(sb, KERN_ERR, "Invalid entry type %x", entry_meta->type);
			goto error_out;
		}
		if (entry_meta->namelen <= 0) {
			clothesfs_msg(sb, KERN_ERR, "Invalid name len: %d", entry_meta->namelen);
			goto error_out;
		}

		clothesfs_msg(sb, KERN_ERR, "Name: %s != %s", entry_meta->name, child->name);
		clothesfs_msg(sb, KERN_ERR, "Lens: %d != %d", entry_meta->namelen, child->len);
		if (entry_meta->namelen != child->len)
			continue;
		if (strncmp(entry_meta->name, child->name, entry_meta->namelen) != 0)
			continue;


		error = 0;
		*inode = clothesfs_get_inode(sb, ipos, entry_meta->type, entry_meta->size);
		goto out;
	}

error_out:
	error = -EINVAL;

out:
	return error;
}

struct inode *clothesfs_find_entry(struct inode *inode, const struct qstr *child)
{
	int error = 0;
	struct super_block *sb = inode->i_sb;
	struct clothesfs_inode_info *ci;
	char buf[sb->s_blocksize];
	struct clothesfs_meta_block *meta;
	unsigned long offset;
	unsigned short id;
	struct inode *child_inode = NULL;

	ci = CLOTHESFS_I(inode);

	offset = ci->pos;

	while (offset < CLOTHESFS_SB(sb)->size / sb->s_blocksize) {
		clothesfs_msg(sb, KERN_ERR, "Offs %d", offset);

		error = clothesfs_block_read(sb, offset, &buf, sb->s_blocksize);
		if (error)
			goto find_entry_error;
		error = -ENOENT;
		meta = (struct clothesfs_meta_block *)&buf;

		id = le16_to_cpu(meta->id);
		if (id != CLOTHESFS_META_ID)
			goto find_entry_error;

		if (meta->type != CLOTHESFS_META_DIR && meta->type != CLOTHESFS_META_DIR_EXT)
			break;

		error = clothesfs_find_entry_from_dir(sb, meta, child, &child_inode);
		if (error)
			goto find_entry_error;

		if (child_inode != NULL)
			goto find_entry_out;

		// No continuation block
		if (meta->ptr == 0)
			break;
		offset = meta->ptr;
	}

find_entry_error:
	clothesfs_msg(sb, KERN_ERR, "Got error %d", error);

find_entry_out:
	return child_inode;
}

static struct dentry *clothesfs_lookup(struct inode *dir,
	struct dentry *dentry, unsigned int flags)
{
	int error = 0;
	struct clothesfs_inode_info *ci;
	struct inode *inode;
	struct dentry *alias;

	ci = CLOTHESFS_I(dir);

	clothesfs_msg(dir->i_sb, KERN_ERR, "LOOKUP: %d %d %s", dir->i_ino, ci->pos, dentry->d_name.name);
	//clothesfs_msg(dir->i_sb, KERN_ERR, "LName: %s", dentry->d_name.name);

	inode = clothesfs_find_entry(dir, &dentry->d_name);
	if (inode == NULL)
		goto lookup_error;

	clothesfs_msg(dir->i_sb, KERN_ERR, "LFOFound: %d %s", inode->i_ino, dentry->d_name.name);

	alias = d_find_alias(inode);
	return d_splice_alias(inode, dentry);

lookup_error:
	clothesfs_msg(dir->i_sb, KERN_ERR, "LUP err %d", error);
	return ERR_PTR(error);
}

static int clothesfs_readpage(struct file *file, struct page *page)
{
	return -EINVAL;
}

static const struct file_operations clothesfs_dir_operations = {
        .read           = generic_read_dir,
        .iterate_shared = clothesfs_readdir,
        .llseek         = generic_file_llseek,
};

static const struct inode_operations clothesfs_dir_inode_operations = {
        .lookup         = clothesfs_lookup,
};

static const struct address_space_operations clothesfs_aops = {
        .readpage       = clothesfs_readpage
};

static struct inode *clothesfs_get_inode(struct super_block *sb, unsigned int pos, unsigned int entry_type, size_t entry_size)
{
	struct clothesfs_inode_info *ci;
	struct inode *inode = iget_locked(sb, pos);
	clothesfs_msg(sb, KERN_ERR, "+++ INODE: %d %d", pos, inode->i_ino);

	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (!(inode->i_state & I_NEW))
		return inode;

	//inode->i_ino = get_next_ino();
	inode->i_sb = sb;

	set_nlink(inode, 1);
	inode->i_size = entry_size;
        inode->i_mtime.tv_sec = inode->i_atime.tv_sec = inode->i_ctime.tv_sec = 0;
        inode->i_mtime.tv_nsec = inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = 0;
	if (entry_type == CLOTHESFS_META_DIR) {
		inode->i_op = &clothesfs_dir_inode_operations;
		inode->i_fop = &clothesfs_dir_operations;
		inode->i_mode = S_IFDIR | 0755;
	} else {
		inode->i_fop = &generic_ro_fops;
		inode->i_data.a_ops = &clothesfs_aops;
		inode->i_mode = S_IFREG | 0644;
	}

	ci = CLOTHESFS_I(inode);
	ci->pos = pos;

	unlock_new_inode(inode);
	return inode;
}

static struct inode *clothesfs_get_root(struct super_block *sb, unsigned int pos)
{
	return clothesfs_get_inode(sb, pos, CLOTHESFS_META_DIR, 0);
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
	brelse(bh);

	if (sb->s_magic != CLOTHESFS_SUPER_MAGIC)
		goto cant_find_clothes;

	error = clothesfs_read_super(sb, sbi, csb);
	if (error)
		goto out_fail;

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
