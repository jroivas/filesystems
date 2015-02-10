#ifndef __CLOTHESFS_HH
#define __CLOTHESFS_HH

#include <stdint.h>
#include <stddef.h>

class ClothesPhys
{
public:
    virtual bool read(
        uint8_t *buffer,
        uint32_t sectors,
        uint32_t pos,
        uint32_t pos_hi) = 0;
    virtual bool write(
        uint8_t *buffer,
        uint32_t sectors,
        uint32_t pos,
        uint32_t pos_hi) = 0;

    virtual uint64_t size() const = 0;
    virtual uint32_t sectorSize() const = 0;
};

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
    ClothesFS();
    ~ClothesFS();

    void setPhysical(ClothesPhys *phys)
    {
        m_phys = phys;
    }

    uint32_t dataToNum(uint8_t *buf, int start, int cnt);
    void numToData(uint64_t num, uint8_t *buf, int start, int cnt);
    bool detect();
    bool format(const char *volid);
    bool addFile(
        uint32_t parent,
        const char *name,
        const char *contents,
        uint64_t size);

protected:
    uint32_t takeFreeBlock();
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
    bool updateMeta(uint32_t index, uint8_t *name, uint64_t size);

    uint8_t baseType(uint8_t type) const;
    bool validType(uint8_t type, uint8_t valid) const;

    ClothesPhys *m_phys;
    uint32_t m_blocksize;
    uint32_t m_blocks;
    uint32_t m_freechain;
    uint32_t m_block_in_sectors;
};

#endif
