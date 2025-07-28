#!/bin/bash

# Build native hextrail with matrix debugging enabled
# This script uses the existing Makefile and adds our matrix debug framework

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔧 Building Native Hextrail with Matrix Debugging${NC}"

# Check if we're in the right directory
if [ ! -f "hacks/glx/hextrail.c" ]; then
    echo -e "${RED}❌ hextrail.c not found. Please run this script from the project root directory.${NC}"
    exit 1
fi

# Check if matrix debug files exist
if [ ! -f "matrix_debug.h" ] || [ ! -f "matrix_debug.c" ]; then
    echo -e "${RED}❌ matrix_debug.h or matrix_debug.c not found.${NC}"
    exit 1
fi

# Create debug build directory
mkdir -p build_native_debug
cd build_native_debug

echo -e "${YELLOW}📦 Building hextrail with matrix debugging...${NC}"

# Copy our matrix debug files to the build directory
cp ../matrix_debug.h .
cp ../matrix_debug.c .

# Compile our matrix debug framework
echo -e "${YELLOW}🔨 Compiling matrix debug framework...${NC}"
gcc -c -DMATRIX_DEBUG -DUSE_GL -I.. -I../hacks -I../utils -I../hacks/glx matrix_debug.c -o matrix_debug.o

# Build hextrail using the existing Makefile, but with our matrix debugging
echo -e "${YELLOW}🔨 Building hextrail with matrix debugging...${NC}"
cd ../hacks/glx

# Temporarily modify hextrail.c to include our matrix debugging
cp hextrail.c hextrail.c.backup
echo '#include "../../matrix_debug.h"' > hextrail.c.tmp
cat hextrail.c >> hextrail.c.tmp
mv hextrail.c.tmp hextrail.c

# Temporarily modify the Makefile to include our matrix_debug.o
cp Makefile Makefile.backup
sed -i 's|hextrail:	hextrail.o	 normals.o $(HACK_TRACK_OBJS)|hextrail:	hextrail.o	 normals.o $(HACK_TRACK_OBJS) ../../build_native_debug/matrix_debug.o|' Makefile
sed -i 's|$(CC_HACK) -o $@ $@.o	 normals.o $(HACK_TRACK_OBJS) $(HACK_LIBS)|$(CC_HACK) -o $@ $@.o	 normals.o $(HACK_TRACK_OBJS) ../../build_native_debug/matrix_debug.o $(HACK_LIBS)|' Makefile

# Build hextrail with matrix debugging
make hextrail CFLAGS="-DMATRIX_DEBUG -DUSE_GL" CPPFLAGS="-I../../build_native_debug"

# Restore original files
mv hextrail.c.backup hextrail.c
mv Makefile.backup Makefile

# Copy the built binary to our debug directory
cp hextrail ../../build_native_debug/hextrail_debug

cd ../../build_native_debug

if [ -f "hextrail_debug" ]; then
    echo -e "${GREEN}✅ Native hextrail with matrix debugging built successfully!${NC}"
    echo -e "${BLUE}📁 Output: build_native_debug/hextrail_debug${NC}"
    echo -e "${YELLOW}🚀 To run:${NC}"
    echo -e "   cd build_native_debug"
    echo -e "   ./hextrail_debug"
    echo -e "${YELLOW}💡 Matrix operations will be logged to stdout${NC}"
else
    echo -e "${RED}❌ Build failed!${NC}"
    exit 1
fi 