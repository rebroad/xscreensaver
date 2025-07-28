#!/bin/bash

# Build native hextrail with matrix debugging enabled
# This script compiles hextrail with our matrix debug framework

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

echo -e "${YELLOW}📦 Compiling Hextrail with Matrix Debugging...${NC}"

# Get the original hextrail build command from the Makefile
# We'll extract the compilation flags and add our debug framework

# First, let's get the original hextrail object file
echo -e "${YELLOW}🔨 Compiling hextrail.c with matrix debugging...${NC}"

# Get the original CFLAGS and other variables from the main Makefile
source <(grep -E '^(CC|CFLAGS|CPPFLAGS|LDFLAGS|HACK_LIBS)=' ../Makefile | sed 's/^/export /')

# Compile hextrail.c with matrix debugging
$CC $CFLAGS $CPPFLAGS \
    -DMATRIX_DEBUG \
    -I.. \
    -I../hacks \
    -I../utils \
    -I../hacks/glx \
    -c ../hacks/glx/hextrail.c -o hextrail_debug.o

# Compile our matrix debug framework
echo -e "${YELLOW}🔨 Compiling matrix debug framework...${NC}"
$CC $CFLAGS $CPPFLAGS \
    -DMATRIX_DEBUG \
    -I.. \
    -I../hacks \
    -I../utils \
    -I../hacks/glx \
    -c ../matrix_debug.c -o matrix_debug.o

# Compile normals.c (required by hextrail)
echo -e "${YELLOW}🔨 Compiling normals.c...${NC}"
$CC $CFLAGS $CPPFLAGS \
    -I.. \
    -I../hacks \
    -I../utils \
    -I../hacks/glx \
    -c ../hacks/glx/normals.c -o normals.o

# Compile other required object files
echo -e "${YELLOW}🔨 Compiling required utilities...${NC}"

# Compile colors.c
$CC $CFLAGS $CPPFLAGS \
    -I.. \
    -I../utils \
    -c ../utils/colors.c -o colors.o

# Compile hsv.c
$CC $CFLAGS $CPPFLAGS \
    -I.. \
    -I../utils \
    -c ../utils/hsv.c -o hsv.o

# Compile yarandom.c
$CC $CFLAGS $CPPFLAGS \
    -I.. \
    -I../utils \
    -c ../utils/yarandom.c -o yarandom.o

# Compile usleep.c
$CC $CFLAGS $CPPFLAGS \
    -I.. \
    -I../utils \
    -c ../utils/usleep.c -o usleep.o

# Compile screenhack.c
$CC $CFLAGS $CPPFLAGS \
    -I.. \
    -I../hacks \
    -I../utils \
    -c ../hacks/screenhack.c -o screenhack.o

# Compile rotator.c
$CC $CFLAGS $CPPFLAGS \
    -I.. \
    -I../hacks \
    -I../hacks/glx \
    -c ../hacks/glx/rotator.c -o rotator.o

# Compile gltrackball.c
$CC $CFLAGS $CPPFLAGS \
    -I.. \
    -I../hacks \
    -I../hacks/glx \
    -c ../hacks/glx/gltrackball.c -o gltrackball.o

# Link everything together
echo -e "${YELLOW}🔗 Linking hextrail with matrix debugging...${NC}"
$CC $LDFLAGS \
    -o hextrail_debug \
    hextrail_debug.o \
    matrix_debug.o \
    normals.o \
    colors.o \
    hsv.o \
    yarandom.o \
    usleep.o \
    screenhack.o \
    rotator.o \
    gltrackball.o \
    $HACK_LIBS -ldl

if [ $? -eq 0 ]; then
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