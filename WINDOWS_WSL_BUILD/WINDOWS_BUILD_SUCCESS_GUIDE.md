# DigiByte Windows Cross-Compilation Build Guide - Successful Method

This guide documents the successful method for building DigiByte Windows executables from WSL (Windows Subsystem for Linux) using cross-compilation with MinGW-w64.

## Overview

Building Windows executables for DigiByte from WSL can be challenging due to header conflicts between the Linux system headers and MinGW headers. This guide provides a proven method that successfully builds the Windows executables by using the depends system properly.

## Prerequisites

### System Requirements
- Windows 10/11 with WSL2 installed
- Ubuntu 20.04 or later in WSL
- At least 8GB RAM
- At least 10GB free disk space

### Required Packages

Install the following packages in WSL:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    curl \
    git \
    ca-certificates \
    ccache \
    g++-mingw-w64-x86-64-posix \
    mingw-w64-x86-64-dev \
    nsis
```

## Step-by-Step Build Process

### 1. Clone and Prepare Repository

```bash
# Clone the repository (if not already done)
git clone https://github.com/DigiByte-Core/digibyte.git
cd digibyte

# Checkout the desired branch
git checkout feature/bitcoin-v26.2-merge

# Pull latest changes
git pull origin feature/bitcoin-v26.2-merge
```

### 2. Clean Previous Build Artifacts

**IMPORTANT**: Always start with a clean build environment to avoid conflicts.

```bash
# Clean main build directory
make clean

# Clean depends directory
cd depends
make clean
cd ..
```

### 3. Build Dependencies Using the Depends System

The key to success is properly building dependencies through the depends system, which isolates them from system headers.

```bash
cd depends

# Remove WSL paths to avoid conflicts
PATH=$(echo "$PATH" | sed -e 's/:\/mnt.*//g')

# Build dependencies for Windows
# Note: This step takes 10-30 minutes depending on your system
make HOST=x86_64-w64-mingw32 -j$(nproc)

cd ..
```

**Important Notes:**
- The `PATH` modification removes WSL-specific paths that can cause conflicts
- Use `-j$(nproc)` to utilize all CPU cores for faster building
- If the build times out, use `-j1` for single-threaded building

### 4. Generate Build Scripts

```bash
./autogen.sh
```

### 5. Configure the Build

Configure with the Windows cross-compilation toolchain:

```bash
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site \
./configure --prefix=/ \
    --disable-wallet \
    --without-gui \
    --disable-tests \
    --disable-bench
```

**Configuration Options Explained:**
- `CONFIG_SITE`: Points to the depends configuration
- `--prefix=/`: Sets installation prefix
- `--disable-wallet`: Builds without wallet support (optional, remove for full build)
- `--without-gui`: Builds without Qt GUI
- `--disable-tests`: Skips test compilation
- `--disable-bench`: Skips benchmark compilation

### 6. Build the Executables

```bash
make -j$(nproc)
```

This will build:
- `src/digibyted.exe` - Main daemon
- `src/digibyte-cli.exe` - Command-line interface
- `src/digibyte-tx.exe` - Transaction utility
- `src/digibyte-util.exe` - General utility

### 7. Copy Executables to Windows

```bash
# Create destination directory
mkdir -p /mnt/c/Users/$USER/DigiByte-Windows/bin

# Copy executables
cp src/*.exe /mnt/c/Users/$USER/DigiByte-Windows/bin/
```

## Common Issues and Solutions

### Issue 1: Header File Conflicts

**Symptoms:**
```
error: conflicting declaration 'typedef long long unsigned int uintptr_t'
error: static assertion failed: CompactSize only supports 64-bit integers
```

**Solution:** 
- Ensure you're using the depends system properly
- Clean all build artifacts before building
- Remove WSL paths from PATH environment variable

### Issue 2: Boost Configuration Errors

**Symptoms:**
```
/bin/sh: 1: Syntax error: "(" unexpected
configure: error: Boost is not available!
```

**Solution:**
- Let the depends system build Boost (don't use NO_BOOST=1)
- Ensure all dependencies are built before configuring

### Issue 3: Build Timeouts

**Symptoms:**
```
Command timed out after X minutes
```

**Solution:**
- Use fewer parallel jobs: `make -j1` instead of `make -j$(nproc)`
- Build in stages if necessary
- Increase timeout if using automation tools

### Issue 4: Missing Dependencies

**Symptoms:**
```
configure: error: libevent not found
```

**Solution:**
- Ensure all packages from prerequisites are installed
- Let depends system build all dependencies
- Don't skip dependency building with NO_* flags unless necessary

## Build Options

### Minimal Build (Fastest)

For a minimal build without wallet, GUI, or tests:

```bash
cd depends
PATH=$(echo "$PATH" | sed -e 's/:\/mnt.*//g') \
make HOST=x86_64-w64-mingw32 NO_QT=1 NO_WALLET=1 -j$(nproc)
cd ..

CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site \
./configure --prefix=/ --disable-wallet --without-gui --disable-tests --disable-bench

make -j$(nproc)
```

### Full Build (All Features)

For a complete build with wallet and all features:

```bash
cd depends
PATH=$(echo "$PATH" | sed -e 's/:\/mnt.*//g') \
make HOST=x86_64-w64-mingw32 -j$(nproc)
cd ..

CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site \
./configure --prefix=/

make -j$(nproc)
```

## Verification

After building, verify the executables:

```bash
# Check file existence and size
ls -la src/*.exe

# Expected output:
# -rwxrwxrwx 1 user user  ~4MB digibyte-cli.exe
# -rwxrwxrwx 1 user user  ~6MB digibyte-tx.exe
# -rwxrwxrwx 1 user user  ~4MB digibyte-util.exe
# -rwxrwxrwx 1 user user ~16MB digibyted.exe
```

## Important Technical Details

### Why This Method Works

1. **Depends System Isolation**: The depends system builds all dependencies in an isolated environment, avoiding conflicts with system headers.

2. **PATH Sanitization**: Removing WSL paths prevents the build system from finding incompatible Windows libraries.

3. **Cross-Compilation Toolchain**: Using `x86_64-w64-mingw32` provides the correct headers and libraries for Windows.

4. **CONFIG_SITE**: This file contains all the necessary configuration for finding the cross-compiled dependencies.

### What NOT to Do

1. **Don't build outside the depends system** - This leads to header conflicts
2. **Don't mix system libraries with cross-compiled ones**
3. **Don't skip the PATH sanitization step**
4. **Don't use gcc/g++ directly** - Always use the mingw toolchain

## Testing the Built Executables

The built executables can be run on Windows. Basic testing:

```cmd
# In Windows Command Prompt or PowerShell
cd C:\Users\%USERNAME%\DigiByte-Windows\bin

# Check version
digibyted.exe --version

# Run with testnet (safer for testing)
digibyted.exe -testnet -printtoconsole
```

## Conclusion

This method successfully builds Windows executables for DigiByte by:
1. Using the depends system to isolate dependencies
2. Properly configuring the cross-compilation environment
3. Avoiding header conflicts through PATH sanitization

Following these steps exactly should result in successful builds. The key is patience during the dependency building phase and ensuring a clean build environment.

## Additional Resources

- [Bitcoin Core Windows Build Guide](https://github.com/bitcoin/bitcoin/blob/master/doc/build-windows.md)
- [MinGW-w64 Documentation](https://www.mingw-w64.org/)
- [WSL Documentation](https://docs.microsoft.com/en-us/windows/wsl/)

---

*Last tested: July 2024 with DigiByte v8.26.0 on WSL2 Ubuntu 22.04*