#include "clothesfs.hh"
#include <stdlib.h>
#include <stdio.h>

ClothesFS::ClothesFS()
    : m_phys(NULL)
{
}

ClothesFS::~ClothesFS()
{
}

bool ClothesFS::detect()
{
    if (m_phys == NULL) {
        return false;
    }

    char buf[m_phys->sectorSize()];
    if (!m_phys->read((uint8_t*)buf, 1, 0, 0)) {
        return false;
    }

    return (
           buf[0] == 0x4
        && buf[1] == 0x2
        && buf[2] == 0x24
        && buf[3] == 'C'
        && buf[4] == 'L'
        && buf[5] == 'O'
        );
}

bool ClothesFS::format()
{
    char buf[m_phys->sectorSize()];
    for (uint32_t i = 0; i < m_phys->sectorSize(); ++i) {
        buf[i] = 0;
    }
    buf[0] = 0x4;
    buf[1] = 0x2;
    buf[2] = 0x24;
    buf[3] = 'C';
    buf[4] = 'L';
    buf[5] = 'O';

    return m_phys->write((uint8_t*)buf, 1, 0, 0);
}

bool ClothesFS::addFile(const char *name, const char *contents, uint32_t size)
{
    return false;
}

uint32_t ClothesListing::entires() const
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
