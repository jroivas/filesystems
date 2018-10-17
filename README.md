# Filesystems

Filesystem implementations.

Includes my own filesystem ClothesFS, used by [C++ kernel](https://github.com/jroivas/cxx-kernel/tree/master/src/fs).

Documentation [here](doc/clothes.md)


## Building

You need cmake, gcc and make:

    mkdir build
    cd build
    cmake ..
    make

## Running

To run example test app, just write in `build` folder:

    ./clothes

It should create `test.img` and list it's contents.


## Kernel module

There's read-only [Linux kernel module](module/) implementation.
