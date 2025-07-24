#!/bin/bash
# Simple Windows build script with header conflict workaround

set -e

echo "Building DigiByte for Windows (minimal build)..."

# Clean
make clean 2>/dev/null || true

# Set up environment to avoid header conflicts
export CPPFLAGS="-D_WIN32_WINNT=0x0601 -DWIN32 -D_WINDOWS"
export CXXFLAGS="-O2 -w"
export LDFLAGS="-static -static-libgcc -static-libstdc++"

# Configure minimal build
./configure \
    --host=x86_64-w64-mingw32 \
    --disable-wallet \
    --without-gui \
    --disable-tests \
    --disable-bench \
    --without-miniupnpc \
    --disable-zmq \
    --without-libs \
    --enable-reduce-exports

# Build only the daemon
echo "Building digibyted.exe..."
cd src

# Compile critical files individually with isolated headers
echo "Compiling digibyted..."
x86_64-w64-mingw32-g++ -c digibyted.cpp -o digibyted.o ${CPPFLAGS} ${CXXFLAGS} -I. -I../src -DHAVE_CONFIG_H

echo "Building libraries..."
make libdigibyte_util.a libdigibyte_common.a libdigibyte_consensus.a libdigibyte_node.a -j4

echo "Linking digibyted.exe..."
x86_64-w64-mingw32-g++ -o digibyted.exe \
    digibyted.o \
    init/digibyted-digibyted.o \
    libdigibyte_node.a \
    libdigibyte_common.a \
    libdigibyte_util.a \
    libdigibyte_consensus.a \
    crypto/libdigibyte_crypto_base.a \
    leveldb/libleveldb.a \
    crc32c/libcrc32c.a \
    secp256k1/.libs/libsecp256k1.a \
    -lws2_32 -lshlwapi -liphlpapi -ladvapi32 -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lodbc32 -lodbccp32 \
    ${LDFLAGS}

echo "Build complete!"
ls -la digibyted.exe