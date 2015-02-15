cmake_minimum_required(VERSION 2.8)
add_definitions(
    -fstack-protector-strong
    -Wstack-protector
    -D_FORTIFY_SOURCE=2
    -Wall
    -g
)
add_executable(clothes
    main.cpp
    clothesfs.cpp
    )

add_executable(fat
    fatmain.cpp
    fat.cpp
    )
