#include "fs/fat.hh"

bool FAT::readBootRecord()
{
    uint8_t *buf = new uint8_t[m_phys->sectorSize()];
    if (buf == NULL) return false;

    bool res = m_phys->read(buf, 1, 0, 0);
    if (!res) return false;

    res = parseBootRecord(buf);

    delete[] buf;

    return res;
}

uint32_t FAT::dataToNum(uint8_t *buf, int start, int cnt)
{
    uint32_t res = 0;
    int n = 0;
    for (int i = start; i < start + cnt; ++i) {
        res += buf[i] << n;
        n += 8;
    }
    return res;
}

void FAT::print()
{
    printf("Identifier         : %s\n", m_identifier.c_str());
    printf("Bytes per sector   : %d\n", m_bytes_per_sector);
    printf("Sectors per cluster: %d\n", m_sectors_per_cluster);
    printf("Reserved           : %d\n", m_reserved);
    printf("FATs               : %d\n", m_fats);
    printf("Directory entries  : %d\n", m_dir_entries);
    printf("Sectors            : %d\n", m_sectors==0?m_large_sectors:m_sectors);
    printf("Media type         : %s\n", m_media_desc==0xf8?"Hard disk":"Other");
    printf("Sectors per FAT    : %d\n", m_sectors_per_fat);
    printf("Sectors per track  : %d\n", m_sectors_per_track);
    printf("Heads              : %d\n", m_heads);
    printf("Hidden             : %d\n", m_hidden);
    printf("Type               : FAT%d\n", m_type);
    printf("Fat entries        : %u\n", m_fat_entries);

    if (m_ext) {
        printf("Label              : %s\n", m_label.c_str());
        printf("Serial             : ");
        printf("%X", (m_serial >> 24) & 0xFF);
        printf("%X-", (m_serial >> 16) & 0xFF);
        printf("%X", (m_serial >> 8) & 0xFF);
        printf("%X\n", m_serial & 0xFF);
    }
}

bool FAT::parseBootRecord(uint8_t *buf)
{
    if (!(buf[0] == 0xEB
        && buf[1] == 0x3C
        && buf[2] == 0x90))
        return false;

    m_vol_base = 0;
    m_identifier = "";
    for (int i = 3; i < 3+8; ++i) {
        m_identifier += (const char)buf[i];
    }

    m_bytes_per_sector = dataToNum(buf, 11, 2);
    m_sectors_per_cluster = (uint32_t)buf[13];
    m_reserved = dataToNum(buf, 14, 2);
    m_fats = (uint32_t)buf[16];
    m_dir_entries = dataToNum(buf, 17, 2);
    m_sectors = dataToNum(buf, 19, 2);
    m_media_desc = (uint32_t)buf[21];
    m_sectors_per_fat = dataToNum(buf, 22, 2);
    m_sectors_per_track = dataToNum(buf, 24, 2);
    m_heads = dataToNum(buf, 26, 2);
    m_hidden = dataToNum(buf, 28, 4);
    m_large_sectors = dataToNum(buf, 32, 4);

    if (buf[38] == 0x28
        || buf[38] == 0x29) {
        // Extended FAT16 / FAT 12
        m_ext = true;
        m_serial = dataToNum(buf, 39, 4);
        m_label = "";
        for (int i = 43; i < 43 + 11; ++i) {
            m_label += (const char)buf[i];
        }
    } else {
        m_ext = false;
    }
    uint32_t root_dir_sectors = (m_dir_entries * 32 + m_bytes_per_sector - 1) / m_bytes_per_sector;
    m_data_base = m_reserved + m_fats * m_sectors_per_fat;
    uint32_t data_sectors = m_sectors==0?m_large_sectors:m_sectors - (m_reserved + m_fats * m_sectors_per_fat + root_dir_sectors);
    uint32_t clusters = data_sectors / m_sectors_per_cluster;

    m_fat_entries = clusters + 2;
    m_type = 12;
    if (clusters >= 4096U) {
        m_type = 16;
    }
    if (clusters >= 65526U) {
        m_type = 32;
    }
    m_fat_base = m_vol_base + m_reserved;

    return true;
}

uint32_t FAT::sectorSize()
{
    return 512;
}

uint32_t FAT::solveSector(uint32_t relative_cluster)
{
    if (relative_cluster == 0) {
        return m_data_base;
    }
    return m_data_base + 32 + (relative_cluster - 2) * m_sectors_per_cluster;
}

FATInfo *FAT::readDir(uint32_t cluster)
{
    uint32_t sector = solveSector(cluster);

    uint8_t data[m_phys->sectorSize()];
    if (!m_phys->read(data, 1, sector * m_phys->sectorSize(), 0)) {
        return NULL;
    }

    uint8_t *pos = data;
    std::string long_temp = "";

    FATInfo *first_info = NULL;
    FATInfo *cur_info = NULL;
    for (uint32_t i = 0; i < m_dir_entries; ++i) {
        if (*pos == 0) break;
        if (*pos != 0 && *pos != 0xe5) {
            if (*(pos + 11) == 0xf) {
                std::string prev = long_temp;
                long_temp = "";
                for (int dd = 1; dd <= 10; ++dd) {
                    const char c = *(pos + dd);
                    if (c > 0 && c < 0xFF) {
                        long_temp += c;
                    }
                }
                for (int dd = 14; dd <= 25; ++dd) {
                    const char c = *(pos + dd);
                    if (c > 0 && c < 0xFF) {
                        long_temp += c;
                    }
                }
                for (int dd = 28; dd <= 31; ++dd) {
                    const char c = *(pos + dd);
                    if (c > 0 && c < 0xFF) {
                        long_temp += c;
                    }
                }
                long_temp = long_temp + prev;
            } else {
                if (long_temp.empty()) {
                    for (int dd = 0; dd <= 11; ++dd) {
                        long_temp += (const char)*(pos + dd);
                    }
                }
                FATInfo *info = new FATInfo(
                    long_temp,
                    dataToNum(pos, 11, 1),
                    dataToNum(pos, 28, 4),
                    dataToNum(pos, 26, 2)
                    );
                if (first_info == NULL) {
                    first_info = info;
                }
                if (cur_info != NULL) {
                    cur_info->m_next = info;
                }
                cur_info = info;
                long_temp = "";
            }
        }
        pos += 32;

        if (pos >= data + m_phys->sectorSize()) {
            ++sector;
            if (!m_phys->read(data, 1, sector * m_phys->sectorSize(), 0)) {
                return NULL;
            }
            pos = data;
        }
    }

    return first_info;
}

FATInfo *FAT::readRootDir()
{
    return readDir(0);
}

bool FAT::readFile(FATInfo *info)
{
    uint32_t sector = solveSector(info->m_pos);

    uint32_t cnt = info->m_size;
    info->m_data = new uint8_t[cnt];
    uint8_t data[m_phys->sectorSize()];

    if (!m_phys->read(data, 1, sector * m_phys->sectorSize(), 0)) {
        return false;
    }
    uint32_t pos = 0;
    uint8_t *dst = info->m_data;
    while (cnt > 0) {
        *dst = data[pos];
        ++dst;
        --cnt;
        ++pos;
        if (pos >= 512) {
            ++sector;
            if (!m_phys->read(data, 1, sector * m_phys->sectorSize(), 0)) {
                return false;
            }
            pos = 0;
        }
    }

    return true;
}

std::string FAT::getPartialName(std::string name, int part)
{
    std::string res;
    int current = 1;
    for (size_t i = 0; i < name.size(); ++i) {
        if (name[i] != '/') {
            if (part == current)
                res += name[i];
        } else {
            current += 1;
        }
    }
    return res;
}

FATInfo *FAT::getItem(const char *name)
{
    std::string paths = name;
    uint32_t curindex = 1;
    std::string partname = getPartialName(name, curindex);

    FATInfo *main = readRootDir();
    if (partname == ".") return main;
    FATInfo *item = main;
    while (item != NULL) {
        if (item->m_name == partname) {
            if (item->m_attr & FATInfo::T_ARCH) {
                readFile(item);
                return item;
            }
            else if (item->m_attr & FATInfo::T_DIR) {
                ++curindex;
                partname = getPartialName(name, curindex);
                if (partname.empty()) {
                    return item;
                }

                item = readDir(item->m_pos);
            }
        }
        item = item->m_next;
    }

    return NULL;
}

void FAT::listDir(FATInfo *info)
{
    if (info == NULL) return;

    FATInfo *item = readDir(info->m_pos);
    while (item != NULL) {
        char t = 'F';
        if (item->m_attr & FATInfo::T_DIR) t = 'D';
        printf("%c %10d %s\n", t, item->m_size, item->m_name.c_str());
        item = item->m_next;
    }
}
