#pragma once

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string>

class FATPhys
{
public:
    FATPhys(std::string fname, uint32_t maxsize)
    {
        m_fp = fopen(fname.c_str(), "r+");
        int res = fseek(m_fp, 0, SEEK_END);
        if (res == 0) {
            m_size = ftell(m_fp);
        } else {
            m_size = maxsize;
        }
    }
    ~FATPhys()
    {
        fclose(m_fp);
    }

    bool read(
        uint8_t *buffer,
        uint32_t sectors,
        uint32_t pos,
        uint32_t pos_hi)
    {
        if (pos >= m_size) {
            return false;
        }

        for (uint32_t i = 0; i < sectorSize(); ++i) {
            buffer[i] = 0;
        }

        int res = fseek(m_fp, pos, SEEK_SET);
        if (res == 0) {
            res = fread(buffer, 1, sectorSize(), m_fp);
            if (res != (int)sectorSize()) return false;
        } else {
            return false;
        }
        return true;
    }

    bool write(
        uint8_t *buffer,
        uint32_t sectors,
        uint32_t pos,
        uint32_t pos_hi)
    {
        if (pos >= m_size) {
            return false;
        }
        int res = fseek(m_fp, pos, SEEK_SET);

        if (res == 0) {
            res = fwrite(buffer, 1, sectorSize(), m_fp);
        }
        return true;
    }

    uint32_t size()
    {
        return m_size;
    }

    inline uint32_t sectorSize() const
    {
        return 512;
    }

protected:
    FILE *m_fp;
    uint32_t m_size;
};

class FATInfo
{
public:
    enum {
        T_RO = 0x1,
        T_HIDDEN = 0x2,
        T_SYSTEM = 0x4,
        T_VOLID = 0x8,
        T_DIR = 0x10,
        T_ARCH = 0x20
    };
    FATInfo()
        : m_attr(0),
        m_size(0),
        m_pos(0),
        m_data(NULL),
        m_next(NULL)
    {
    }

    FATInfo(
        std::string name,
        uint8_t attr,
        uint32_t size,
        uint32_t pos
        )
        : m_name(name),
        m_attr(attr),
        m_size(size),
        m_pos(pos),
        m_data(NULL),
        m_next(NULL)
    {
    }

    void print()
    {
        std::string attr = "";
        attr += (m_attr & T_RO)?"RO ":"";
        attr += (m_attr & T_HIDDEN)?"HIDDEN ":"";
        attr += (m_attr & T_SYSTEM)?"SYSTEM ":"";
        attr += (m_attr & T_VOLID)?"VolID ":"";
        attr += (m_attr & T_DIR)?"DIR ":"";
        attr += (m_attr & T_ARCH)?"ARCH":"";

        printf("name: %s\n", m_name.c_str());
        printf("size: %u\n", m_size);
        printf("pos : %u\n", m_pos);
        printf("attr: %s\n", attr.c_str());
    }

    std::string m_name;
    uint8_t m_attr;
    uint32_t m_size;
    uint32_t m_pos;
    uint8_t *m_data;

    FATInfo *m_next;
};

class FAT
{
public:
    FAT(FATPhys *phys)
        : m_phys(phys)
    {
    }

    bool readBootRecord();
    FATInfo *readRootDir();
    FATInfo *readDir(uint32_t cluster);
    FATInfo *getItem(const char *name);
    bool readFile(FATInfo *info);
    void print();

protected:
    std::string getPartialName(std::string name, int part);
    uint32_t dataToNum(uint8_t *buf, int start, int cnt);
    bool parseBootRecord(uint8_t *buf);
    uint32_t sectorSize();
    uint32_t solveSector(uint32_t relative_cluster);

    std::string m_identifier;
    std::string m_label;
    FATPhys *m_phys;
    uint32_t m_bytes_per_sector;
    uint32_t m_sectors_per_cluster;
    uint32_t m_reserved;
    uint32_t m_fats;
    uint32_t m_dir_entries;
    uint32_t m_sectors;
    uint32_t m_large_sectors;
    uint32_t m_media_desc;
    uint32_t m_sectors_per_fat;
    uint32_t m_sectors_per_track;
    uint32_t m_heads;
    uint32_t m_hidden;
    uint32_t m_serial;

    uint32_t m_vol_base;
    uint32_t m_fat_base;
    uint32_t m_data_base;
    uint32_t m_fat_entries;
    uint32_t m_type;
    bool m_ext;
};
