#include <iostream>

#include "fat.hh"

int main(int argc, char **argv)
{
    if (argc <= 2) {
        printf("Usage: %s image file\n", argv[0]);
        return 1;
    }
    FATPhys phys(argv[1], 1024 * 10);

    FAT fat(&phys);

    bool res = fat.readBootRecord();
    if (!res) return 1;

    FATInfo *item = fat.getItem(argv[2]);
    if (item != NULL) {
        if (item ->m_data != NULL) {
            std::string res;
            uint32_t c = item->m_size;
            uint32_t i = 0;
            while (c > 0) {
                res += item->m_data[i];
                --c;
                ++i;
            }
            printf("GOT DATA: %s\n", res.c_str());
        } else {
            fat.listDir(item);
        }
    } else {
        printf("NULL ITEM\n");
    }
}
