#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

static struct dentry *clothesfs_mount(struct file_system_type *fs_type,
			int flags, const char *dev_name,
                        void *data)
{
	struct dentry *ret = ERR_PTR(-EINVAL);
	return ret;
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
