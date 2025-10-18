#!/bin/bash

# Compare Web Builds Script
# This script builds HexTrail with and without matrix debug and compares the outputs

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔍 HexTrail Build Comparison Script${NC}"
echo -e "${BLUE}====================================${NC}"
echo ""

# Create comparison directories
NORMAL_OUTPUT_DIR="matrix_debug_outputs/normal_build"
DEBUG_OUTPUT_DIR="matrix_debug_outputs/debug_build"

echo -e "${CYAN}📁 Output directories:${NC}"
echo -e "   Normal build: $NORMAL_OUTPUT_DIR"
echo -e "   Debug build:  $DEBUG_OUTPUT_DIR"
echo ""

# Step 1: Build without matrix debug
echo -e "${YELLOW}🔨 Step 1: Building without matrix debug...${NC}"
./build_web.sh -noserver
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Normal build completed${NC}"
echo ""

# Step 2: Capture debug output from normal build
echo -e "${YELLOW}🔍 Step 2: Capturing debug output from normal build...${NC}"
./probe_web.sh 8000 "$NORMAL_OUTPUT_DIR"
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Failed to capture normal build output!${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Normal build output captured${NC}"
echo ""

# Step 3: Build with matrix debug
echo -e "${YELLOW}🔨 Step 3: Building with matrix debug...${NC}"
./build_web.sh -matrix-debug -noserver
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Debug build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Debug build completed${NC}"
echo ""

# Step 4: Capture debug output from debug build
echo -e "${YELLOW}🔍 Step 4: Capturing debug output from debug build...${NC}"
./probe_web.sh 8000 "$DEBUG_OUTPUT_DIR"
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Failed to capture debug build output!${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Debug build output captured${NC}"
echo ""

# Step 5: Compare the outputs
echo -e "${YELLOW}🔍 Step 5: Comparing outputs...${NC}"

if [ ! -f "$NORMAL_OUTPUT_DIR/web_debug_output.txt" ]; then
    echo -e "${RED}❌ Normal build output file not found!${NC}"
    exit 1
fi

if [ ! -f "$DEBUG_OUTPUT_DIR/web_debug_output.txt" ]; then
    echo -e "${RED}❌ Debug build output file not found!${NC}"
    exit 1
fi

# Extract matrix operations from both files
echo -e "${CYAN}📊 Extracting matrix operations...${NC}"
grep -E "(glMatrixMode|glLoadIdentity|gluPerspective|gluLookAt|glTranslatef|glRotatef|glScalef|glPushMatrix|glPopMatrix)" "$NORMAL_OUTPUT_DIR/web_debug_output.txt" > "$NORMAL_OUTPUT_DIR/matrix_ops.txt"
grep -E "(glMatrixMode|glLoadIdentity|gluPerspective|gluLookAt|glTranslatef|glRotatef|glScalef|glPushMatrix|glPopMatrix)" "$DEBUG_OUTPUT_DIR/web_debug_output.txt" > "$DEBUG_OUTPUT_DIR/matrix_ops.txt"

# Compare the matrix operations
echo -e "${CYAN}🔍 Comparing matrix operations...${NC}"
if diff "$NORMAL_OUTPUT_DIR/matrix_ops.txt" "$DEBUG_OUTPUT_DIR/matrix_ops.txt" > "matrix_debug_outputs/matrix_ops_diff.txt"; then
    echo -e "${GREEN}✅ Matrix operations are identical!${NC}"
else
    echo -e "${YELLOW}⚠️  Matrix operations differ!${NC}"
    echo -e "${CYAN}📁 Differences saved to: matrix_debug_outputs/matrix_ops_diff.txt${NC}"
    echo ""
    echo -e "${CYAN}📊 Summary:${NC}"
    echo -e "   Normal build operations: $(wc -l < "$NORMAL_OUTPUT_DIR/matrix_ops.txt")"
    echo -e "   Debug build operations:  $(wc -l < "$DEBUG_OUTPUT_DIR/matrix_ops.txt")"
    echo ""
    echo -e "${YELLOW}💡 To view differences:${NC}"
    echo -e "   cat matrix_debug_outputs/matrix_ops_diff.txt"
fi

# Also compare the full outputs for any other differences
echo -e "${CYAN}🔍 Comparing full outputs...${NC}"
if diff "$NORMAL_OUTPUT_DIR/web_debug_output.txt" "$DEBUG_OUTPUT_DIR/web_debug_output.txt" > "matrix_debug_outputs/full_diff.txt"; then
    echo -e "${GREEN}✅ Full outputs are identical!${NC}"
else
    echo -e "${YELLOW}⚠️  Full outputs differ!${NC}"
    echo -e "${CYAN}📁 Full differences saved to: matrix_debug_outputs/full_diff.txt${NC}"
    echo ""
    echo -e "${CYAN}📊 Summary:${NC}"
    echo -e "   Normal build lines: $(wc -l < "$NORMAL_OUTPUT_DIR/web_debug_output.txt")"
    echo -e "   Debug build lines:  $(wc -l < "$DEBUG_OUTPUT_DIR/web_debug_output.txt")"
fi

echo ""
echo -e "${GREEN}✅ Comparison complete!${NC}"
echo -e "${CYAN}📁 Check matrix_debug_outputs/ for all results${NC}"
