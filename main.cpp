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
    FilePhys phys("test.img", 1024 * 10);

    ClothesFS cloth;
    cloth.setPhysical(&phys);
    cloth.format("My impressive volume");
    const char *data = "This is\ntest file\n with contents...\n";
    bool res = cloth.addFile(1, "test", data, strlen(data));

    printf("ok: %d %d\n", cloth.detect(), res);
}
