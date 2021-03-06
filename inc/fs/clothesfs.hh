#ifndef __CLOTHESFS_HH
#define __CLOTHESFS_HH

#ifdef LINUX_BUILD
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#else
#include <string.hh>
#include <platform.h>
#endif

#include <fs/filesystem.hh>

#ifdef USE_CUSTOM_STRING
#include <string.hh>
#define STD_STRING_TYPE String
#else
#include <string>
#define STD_STRING_TYPE std::string
#endif
static const uint32_t FS_BLOCKSIZE = 512;

class ClothesFS
{
public:
    enum {
        META_FREE = 0x00,
        META_FILE = 0x02,
        META_DIR = 0x04,
        META_FILE_CONT = 0x08,
        META_DIR_CONT = 0x10,
        META_JOURNAL = 0x80
    };
    enum {
        ATTRIB_NONE = 0x00,
        ATTRIB_EXEC = 0x01,
        ATTRIB_READ = 0x02,
        ATTRIB_WRITE = 0x04,
        ATTRIB_LINK = 0x08,
        ATTRIB_OEXEC = 0x10,
        ATTRIB_OREAD = 0x20,
        ATTRIB_OWRITE = 0x40,
        ATTRIB_SPECIAL = 0x80
    };
    enum {
        PAYLOAD_FREE = 0x00,
        PAYLOAD_USED = 0x01,
        PAYLOAD_FREED = 0x02
    };
    enum {
        ALGO_DISABLED = 0x00,
        ALGO_XOR = 0x01,
        ALGO_CRC = 0x02,
        ALGO_SUMMOD = 0x04
    };

    class Iterator {
        friend class ClothesFS;
    public:
        Iterator()
            : m_ok(false),
            m_block(0),
            m_index(0),
            m_pos(0),
            m_data_block(0),
            m_data_index(0),
            m_fs(nullptr),
            m_parent(nullptr),
            m_data(nullptr)
        {
        }
        Iterator(uint32_t blk, uint32_t index)
            : m_ok(false),
            m_block(blk),
            m_index(index),
            m_pos(0),
            m_data_block(0),
            m_data_index(0),
            m_fs(nullptr),
            m_parent(nullptr),
            m_data(nullptr)
        {
        }
        ~Iterator() {
            m_ok = false;
            if (m_parent != nullptr) {
                delete[] m_parent;
            }
            if (m_data != nullptr) {
                delete[] m_data;
            }
            if (m_content != nullptr) {
                delete[] m_content;
            }
            m_parent = nullptr;
            m_data = nullptr;
            m_content = nullptr;
        }
        Iterator(const Iterator &another)
            : m_ok(false),
            m_pos(0),
            m_data_block(0),
            m_data_index(0),
            m_fs(nullptr),
            m_parent(nullptr),
            m_data(nullptr)
        {
            assign(another);
        }

        Iterator &operator=(const Iterator &another)
        {
            assign(another);
            return *this;
        }

        void assign(const Iterator &another)
        {
            m_fs = another.m_fs;
            m_block = another.m_block;
            m_index = another.m_index;
            m_data_block = another.m_data_block;
            m_data_index = another.m_data_index;
            m_ok = another.m_ok;

            if (m_fs != nullptr) {
                m_parent = new uint8_t[m_fs->m_blocksize];
                m_data = new uint8_t[m_fs->m_blocksize];
                m_content = new uint8_t[m_fs->m_blocksize];

#ifdef LINUX_BUILD
                if (another.m_parent != nullptr) {
                    memmove(m_parent, another.m_parent, m_fs->m_blocksize);
                }
                if (another.m_data != nullptr) {
                    memmove(m_data, another.m_data, m_fs->m_blocksize);
                }
                if (another.m_content != nullptr) {
                    memmove(m_content, another.m_content, m_fs->m_blocksize);
                }
#else
                if (another.m_parent != nullptr) {
                    Mem::move(m_parent, another.m_parent, m_fs->m_blocksize);
                }
                if (another.m_data != nullptr) {
                    Mem::move(m_data, another.m_data, m_fs->m_blocksize);
                }
                if (another.m_content != nullptr) {
                    Mem::move(m_content, another.m_content, m_fs->m_blocksize);
                }
#endif
            }
        }

        bool next();
        inline bool ok() const
        {
            return m_ok;
        }
        inline uint8_t *data()
        {
            return m_data;
        }
        STD_STRING_TYPE name();
        uint32_t nameLen();
        uint64_t size();
        uint8_t type() const;
        uint64_t read(uint8_t *buf, uint64_t cnt);
        uint32_t block() const
        {
            return m_block;
        }
        bool remove();

    protected:
        bool getCurrent();

        bool m_ok;
        uint32_t m_block;
        uint32_t m_index;
        uint64_t m_pos;
        uint32_t m_data_block;
        uint32_t m_data_index;

        ClothesFS *m_fs;
        uint8_t *m_parent;
        uint8_t *m_data;
        uint8_t *m_content;
    };

    ClothesFS();
    ~ClothesFS();

    void setPhysical(FilesystemPhys *phys);
    inline uint32_t blockSize() const
    {
        return m_blocksize;
    }

    static uint32_t dataToNum(uint8_t *buf, int start, int cnt);
    static void numToData(uint64_t num, uint8_t *buf, int start, int cnt);
    bool detect();
    bool format(const char *volid);
    bool addFile(
        uint32_t parent,
        const char *name,
        const char *contents,
        uint64_t size);
    bool addDir(
        uint32_t parent,
        const char *name);
    ClothesFS::Iterator list(
        uint32_t parent);

protected:
    uint32_t takeFreeBlock();
    bool addFreeBlock(uint32_t id);
    bool formatBlock(uint32_t num, uint32_t next);
    uint32_t formatBlocks();
    bool getBlock(uint32_t index, uint8_t *buffer);
    bool putBlock(uint32_t index, uint8_t *buffer);
    void clearBuffer(uint8_t *buf, uint32_t size);

    bool initMeta(uint32_t index,uint8_t type);
    uint32_t initData(uint32_t index, uint8_t type, uint8_t algo);
    bool addToMeta(uint32_t index, uint32_t meta, uint8_t type);
    bool dirContinues(uint32_t index, uint32_t next);
    bool addData(uint32_t meta, const char *contents, uint64_t size);
    bool updateMeta(uint32_t index, const uint8_t *name, uint64_t size);

    uint8_t baseType(uint8_t type) const;
    bool validType(uint8_t type, uint8_t valid) const;

    bool verifySectorSize() const;

    FilesystemPhys *m_phys;
    uint32_t m_blocksize;
    uint32_t m_blocks;
    uint32_t m_freechain;
    uint32_t m_block_in_sectors;
};

#endif
