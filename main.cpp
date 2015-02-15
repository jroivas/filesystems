#include "clothesfs.hh"
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

class FilePhys : public ClothesPhys
{
public:
    FilePhys(std::string fname, uint32_t maxsize)
    {
        m_fp = fopen(fname.c_str(), "r+");
        if (m_fp == NULL) {
            m_fp = fopen(fname.c_str(), "w+");
        }
        m_size = maxsize;
    }
    ~FilePhys()
    {
        fclose(m_fp);
    }

    virtual bool read(
        uint8_t *buffer,
        uint32_t sectors,
        uint32_t pos,
        uint32_t pos_hi)
    {
        if (pos >= m_size) {
            return false;
        }
        int res = fseek(m_fp, pos, SEEK_SET);

        for (uint32_t i = 0; i < sectorSize(); ++i) {
            buffer[i] = 0;
        }

        if (res == 0) {
            res = fread(buffer, 1, sectorSize(), m_fp);
        }
        return true;
    }

    virtual bool write(
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

    virtual uint64_t size() const
    {
        return m_size;
    }

    virtual uint32_t sectorSize() const
    {
        return 512;
    }

protected:
    FILE *m_fp;
    uint32_t m_size;
};

int main(int argc, char **argv)
{
    FilePhys phys("test.img", 1024 * 40);

    ClothesFS cloth;
    cloth.setPhysical(&phys);
    cloth.format("My impressive volume");
    const char *data = "This is\ntest file\n with contents...\n";
    bool res = cloth.addFile(1, "test", data, strlen(data));
    cloth.addDir(1, "tmp");
    res = cloth.addFile(1, "dummy", "4dummy2", 7);
    FILE *tmpf = fopen("test.md", "r");
    char *fdata = new char[7000];
    if (tmpf != NULL) {
        size_t cnt = fread(fdata, 1, 7000, tmpf);
        fclose(tmpf);
        res = cloth.addFile(1, "test.md", fdata, cnt);
    }
    cloth.addDir(1, "folder");
    res = cloth.addFile(4, "fileinfolder", "data42.", 7);

    printf("ok: %d %d\n", cloth.detect(), res);

    ClothesFS::Iterator iter = cloth.list(1);
    while (iter.ok()) {
        char type = 'F';
        if (iter.type() & ClothesFS::META_DIR) {
            type = 'D';
        }
        printf("%c  %5lu   %s   %u\n", type, iter.size(), iter.name().c_str(), iter.block());
#if 0
        if (iter.size() < 100) {
            char daa[200];
            uint64_t getd = iter.read((uint8_t*)daa, 200);
            for (uint64_t a = 0; a < getd; ++a) {
                printf("%c", daa[a]);
            }
            printf("\n");
        }
#endif
        if (!iter.next()) break;
    }
}
