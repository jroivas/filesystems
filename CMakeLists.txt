cmake_minimum_required(VERSION 2.8)
add_definitions(
    -fstack-protector-strong
    -Wstack-protector
    -D_FORTIFY_SOURCE=2
    -DLINUX_BUILD=1
    -Wall
    -pedantic
    -std=c++11
    -g
    )
include_directories(
    inc
    )
add_executable(clothes
    main.cpp
    fs/clothesfs.cpp
    )

add_executable(fat
    fatmain.cpp
    fat.cpp
    )
