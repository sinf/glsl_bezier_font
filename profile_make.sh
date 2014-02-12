#!/bin/bash
# This script is supposed to compile a very highly optimized binary
# The final binary will still have debug symbols to allow easy disassembly and inspection of important functions

make clean

# -flto : Link time optimization. Allows small functions to be inlined across modules for example
# -O3 : Enables many things but most importantly SSE vectorization
# -ffast-math : Allows the use of hardware math instructions by disregarding precision
# -mfpmath=both : Allows useful legacy instructions such as FSINCOS to be mixed in
# -ffunction-sections, -fdata-sections : Makes it easy for the linker to trim unused junk
# -fprofile-generate, -fprofile-use: Profile-guided optimization steps.
# NDEBUG : disables assert() from <assert.h>

C="-flto -O3 -ffast-math -mfpmath=both -ffunction-sections -fdata-sections -g -DNDEBUG"
L="-flto"
PASS1="-fprofile-generate"
PASS2="-fprofile-use"

inspect() {
	echo "MD5 sum:" `md5sum $1`
	echo "Filesize:" `du -b $1`
	echo "Library dependencies:" `ldd $1| wc -l`
	echo "External symbols:" `nm -D $1 | wc -l`
}

export CFLAGS="$C $PASS1"
export LDFLAGS="$L $PASS1"
make

# Now play with the program.
# Try to access every code path and feature
# to generate good optimization hints
./prog $*
mv ./prog ./prog.old

make clean

export CFLAGS="$C $PASS2"
export LDFLAGS="$L $PASS2 -Wl,-O5,--gc-sections"
make

echo
echo "Before:"
inspect ./prog.old

echo
echo "After:"
inspect ./prog

rm -f ./prog.old ./build/*.gcda
