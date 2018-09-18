#ifndef __FILESYSTEM_PHYS_HH
#define __FILESYSTEM_PHYS_HH

#include <stdint.h>

class FilesystemPhys
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

#endif
