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

#if 0
class ClothesListing
{
public:
    ClothesListing(uint8_t *data, uint32_t size)
        : m_data(data),
        m_size(size),
        m_items(NULL),
        m_continues(false)
    {
        parse();
    }
    ~ClothesListing();

    uint32_t entries() const;

    uint8_t type(uint32_t index) const;
    const char *filename(uint32_t index) const;
    const char *contents(uint32_t index) const;
    uint32_t size(uint32_t index) const;

    bool write(uint8_t *data, uint32_t size);
    bool continues() const
    {
        return m_continues;
    }

protected:
    bool parse();
    bool detect();
    uint32_t parseFile(uint32_t pos, uint8_t type);
    uint32_t parseItem(uint8_t type, uint32_t pos);

    class Item {
    public:
        Item()
            :
            type(0),
            name(NULL),
            inode(0),
            next(NULL)
        {}

        uint8_t type;
        char *name;
        uint32_t inode;
        //const char *contents;
        //uint32_t size;

        Item *next;
    };
    uint8_t *m_data;
    uint32_t m_size;
    Item *m_items;
    bool m_continues;

    void append(Item *item);
};
#endif

class ClothesFS
{
public:
    ClothesFS();
    ~ClothesFS();

    void setPhysical(ClothesPhys *phys)
    {
        m_phys = phys;
    }

    uint32_t dataToNum(uint8_t *buf, int start, int cnt);
    void numToData(uint64_t num, uint8_t *buf, int start, int cnt);
    bool detect();
    bool format();
    bool addFile(const char *name, const char *contents, uint32_t size);

protected:
    uint32_t takeFreeBlock();
    bool formatBlock(uint32_t num, uint32_t next);
    uint32_t formatBlocks();
    bool getBlock(uint32_t index, uint8_t *buffer);
    bool putBlock(uint32_t index, uint8_t *buffer);

    ClothesPhys *m_phys;
    uint32_t m_blocksize;
    uint32_t m_blocks;
    uint32_t m_freechain;
    uint32_t m_block_in_sectors;
};

#endif
