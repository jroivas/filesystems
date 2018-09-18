# Filesystems

Filesystem implementations.

Includes my own filesystem ClothesFS, used by [C++ kernel](https://github.com/jroivas/cxx-kernel/tree/master/src/fs).

Documentation [here](https://github.com/jroivas/cxx-kernel/blob/master/doc/clothes.md)


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
