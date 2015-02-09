#include "clothesfs.hh"
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

static uint32_t header_begin = 8 * 4;

ClothesFS::ClothesFS()
    : m_phys(NULL),
    m_blocksize(512)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    srand(tv.tv_sec + tv.tv_usec);
}

ClothesFS::~ClothesFS()
{
}

uint32_t ClothesFS::dataToNum(uint8_t *buf, int start, int cnt)
{
    uint32_t res = 0;
    int n = 0;
    for (int i = start; i < start + cnt; ++i) {
        res += buf[i] << n;
        n += 8;
    }
    return res;
}

void ClothesFS::numToData(uint64_t num, uint8_t *buf, int start, int cnt)
{
    int n = 0;
    for (int i = start; i < start + cnt; ++i) {
        buf[i] = (num >> n) & 0xFF;
        n += 8;
    }
}

bool ClothesFS::detect()
{
    if (m_phys == NULL) {
        return false;
    }

    uint8_t buf[m_phys->sectorSize()];
    if (!m_phys->read(buf, 1, 0, 0)) {
        return false;
    }

    m_blocksize = dataToNum(buf, header_begin + 4, 2);

    return (
           buf[header_begin + 0] == 0x00
        && buf[header_begin + 1] == 0x42
        && buf[header_begin + 2] == 0x00
        && buf[header_begin + 3] == 0x42);
}

bool ClothesFS::getBlock(uint32_t index, uint8_t *data)
{
    for (uint32_t block = 0; block < m_block_in_sectors; --block) {
        uint64_t pos = index * m_blocksize;
        pos += block * m_phys->sectorSize();

        if (!m_phys->read(
                data + m_phys->sectorSize() * block,
                1,
                pos & 0xFFFFFFFF,
                (pos >> 32) & 0xFFFFFFFF)) {
            return false;
        }
    }

    return true;
}

bool ClothesFS::putBlock(uint32_t index, uint8_t *data)
{
    for (uint32_t block = 0; block < m_block_in_sectors; --block) {
        uint64_t pos = index * m_blocksize;
        pos += block * m_phys->sectorSize();

        if (!m_phys->write(
                data + m_phys->sectorSize() * block,
                1,
                pos & 0xFFFFFFFF,
                (pos >> 32) & 0xFFFFFFFF)) {
            return false;
        }
    }

    return true;
}


bool ClothesFS::formatBlock(uint32_t num, uint32_t next)
{
    uint8_t buf[m_blocksize];
    for (uint32_t i = 0; i < m_blocksize; ++i) {
        buf[i] = 0;
    }

    numToData(0x42, buf, 0, 4);
    numToData(next, buf, m_blocksize - 4, 4);

    return putBlock(num, buf);
#if 0
    uint8_t buf[m_phys->sectorSize()];
    for (uint32_t i = 0; i < m_phys->sectorSize(); ++i) {
        buf[i] = 0;
    }

    uint32_t blocks = m_block_in_sectors;
    numToData(next, buf, m_phys->sectorSize() - 5, 4);

    while (blocks > 0) {
        if (blocks == 1) {
            numToData(0x00000042, buf, 0, 4);
        }
        uint64_t index = num * m_blocksize;
        index += (blocks - 1) * m_phys->sectorSize();

        if (!m_phys->write(
                buf, 1,
                index & 0xFFFFFFFF,
                (index >> 32) & 0xFFFFFFFF)) {
            return false;
        }
        numToData(0, buf, m_phys->sectorSize() - 5, 4);
        --blocks;
    }

    return true;
#endif
}

uint32_t ClothesFS::formatBlocks()
{
    uint32_t next = 0;
    uint32_t start = 2;
    for (uint32_t i = m_blocks - 1; i >= start; --i) {
        if (!formatBlock(i, next)) {
            return 0;
        }
        next = i;
    }
    return start;
}

bool ClothesFS::format()
{
    uint8_t buf[m_phys->sectorSize()];
    for (uint32_t i = 0; i < m_phys->sectorSize(); ++i) {
        buf[i] = 0;
    }

    buf[header_begin + 0] = 0x00;
    buf[header_begin + 1] = 0x42;
    buf[header_begin + 2] = 0x00;
    buf[header_begin + 3] = 0x42;

    uint32_t pos = header_begin + 4;
    numToData(m_blocksize, buf, pos, 2);

    pos += 2;

    //flags
    buf[pos] = 0x0;
    //grpindex
    buf[pos + 1] = 0x0;

    pos += 2;

    // vol id
    for (int i = 0; i < 8; ++i) {
        buf[pos + i] = rand() % 0xFF;
    }
    pos += 8;

    // size
    numToData(m_phys->size(), buf, pos, 4);
    pos += 8;
    m_blocks = m_phys->size() / m_blocksize;
    m_block_in_sectors = m_blocksize / m_phys->sectorSize();

    // vol name
    buf[pos + 0] = 'V';
    buf[pos + 1] = 'O';
    buf[pos + 2] = 'L';
    buf[pos + 3] = '4';
    buf[pos + 4] = '2';
    pos += 32;

    // Block 1 is root dir
    numToData(1, buf, pos, 4);
    pos += 4;

    uint32_t freechain = formatBlocks();

    // Used
    numToData(2, buf, pos, 4);
    pos += 4;

    // Journal1
    numToData(0, buf, pos, 4);
    pos += 4;

    // Journal2
    numToData(0, buf, pos, 4);
    pos += 4;

    // Freechain
    printf("freepos: %d\n", pos);
    numToData(freechain, buf, pos, 4);
    pos += 4;

    bool res = m_phys->write(buf, 1, 0, 0);

    if (res and m_blocksize > 512) {
        // TODO
    }

    return res;
}

uint32_t ClothesFS::takeFreeBlock()
{
    uint8_t data[m_blocksize];
    uint8_t block[m_blocksize];
    if (!getBlock(0, data)) {
        return 0;
    }

    uint32_t freechain = dataToNum(data, 104, 4);
    printf("Freechain: %u\n", freechain);

    if (!getBlock(freechain, block)) {
        return 0;
    }
    uint32_t index = dataToNum(block, 0, 4);
    uint32_t next_freechain = dataToNum(block, m_blocksize - 4, 4);

    printf("index: %x\n", index);
    printf("Next freechain: %u\n", next_freechain);

    numToData(next_freechain, data, 104, 4);
    if (!putBlock(0, data)) {
        return 0;
    }

    return freechain;
}

bool ClothesFS::addFile(const char *name, const char *contents, uint32_t size)
{
    uint32_t block = takeFreeBlock();
    printf("Blockken: %u\n", block);
    return false;
}

#if 0

uint32_t ClothesListing::entries() const
{
}

const char *ClothesListing::filename(uint32_t index) const
{
}

const char *ClothesListing::contents(uint32_t index) const
{
}

uint32_t ClothesListing::size(uint32_t index) const
{
}

bool ClothesListing::detect()
{
    return (m_data[0] == 0x4
        && m_data[1] == 0x2
        && m_data[2] == 0x42
        && m_data[3] == 'L');
}

void ClothesListing::append(Item *item)
{
    if (m_items == NULL) {
        m_items = item;
    } else {
        Item *d = m_items;
        while (d->next != NULL) {
            d = d->next;
        }
        d->next = item;
    }
}

uint32_t ClothesListing::parseFile(uint32_t pos, uint8_t type)
{
    uint32_t inode = 0;
    inode = m_data[pos];
    inode += (uint32_t)(m_data[pos + 1]) << 8;
    inode += (uint32_t)(m_data[pos + 2]) << 16;
    inode += (uint32_t)(m_data[pos + 3]) << 24;

    pos += 4;

    uint8_t len = 240; //FIXME

    Item *tmp = new Item();
    tmp->type = type;
    tmp->inode = inode;

    tmp->name = (char*)malloc(len + 1);

    for (uint8_t p = 0; p < len; ++p) {
        tmp->name[p] = m_data[pos + p];
    }
    tmp->name[len] = 0;
    pos += len;

    append(tmp);

    return pos;
}

uint32_t ClothesListing::parseItem(uint8_t type, uint32_t pos)
{
    switch (type) {
        case 2:
            pos = parseFile(pos, type);
            //FILE
            break;
        case 6:
            pos = parseFile(pos, type);
            //DIR
            break;
        default:
            break;
    }
    return pos;
}

bool ClothesListing::parse()
{
    if (!detect()) return false;

    uint32_t pos = 4;
    while (pos < m_size - 2) {
        if (m_data[pos] == 0)
                break;
        if (m_data[pos] == 4) {
            ++pos;
            uint8_t type = m_data[pos];
            pos = parseItem(type, pos + 1);
        }
        else if (m_data[pos] == 9) {
            //Continue on next sector
            m_continues = true;
            break;
        }
    }

    return true;
}

bool ClothesListing::write(uint8_t *data, uint32_t size)
{
    if (m_items == NULL
        || m_data == NULL
        || size == 0) {
        return false;
    }

    uint32_t pos = 0;
    data[pos] = 0x4;
    data[pos + 1] = 0x2;
    data[pos + 2] = 0x42;
    data[pos + 3] = 'L';
    pos = 4;

    Item *tmp = m_items;
    while (tmp != NULL) {
        data[pos] = 4;
        ++pos;
        data[pos] = tmp->type;
        ++pos;

        data[pos] = tmp->inode & 0xFF;
        ++pos;
        data[pos] = (tmp->inode >> 8) & 0xFF;
        ++pos;
        data[pos] = (tmp->inode >> 16) & 0xFF;
        ++pos;
        data[pos] = (tmp->inode >> 24) & 0xFF;
        ++pos;

        for (uint8_t i = 0; i < 240; ++i) {
            data[pos] = tmp->name[i];
            ++pos;
        }
        tmp = tmp->next;
        if (tmp == NULL)
            data[pos] = 0;
        if (m_size - pos < 240) {
            data[pos] = 9;
            m_continues = true;
            break;
        }
    }

    return true;
}
#endif
