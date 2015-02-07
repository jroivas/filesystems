#include <iostream>

#include "fat.hh"

int main(int argc, char **argv)
{
    FATPhys phys(argv[1], 1024 * 10);

    FAT fat(&phys);

    bool res = fat.readBootRecord();
    if (!res) return 1;

    printf("%u\n", fat.readFat(2));
    FATInfo *info = fat.readRootDir();
    while (info != NULL) {
        if (info->m_name == "test.txt") {
            if (fat.readFile(info))
                printf("DATA: %s\n", info->m_data);
        }
        info = info->m_next;
    }
    fat.readDir(18);
}
