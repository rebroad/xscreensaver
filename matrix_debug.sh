#!/bin/bash

# Matrix Debugging Framework - User-Friendly Interface
# This script provides easy access to all matrix debugging features

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to show usage
show_usage() {
    echo -e "${BLUE}🔧 Matrix Debugging Framework${NC}"
    echo -e "${YELLOW}Usage: $0 [COMMAND]${NC}"
    echo ""
    echo -e "${CYAN}Available Commands:${NC}"
    echo -e "  ${GREEN}compare${NC}       - Build both versions and compare matrix outputs"
    echo -e "  ${GREEN}hextrail-web${NC}  - Build hextrail for WebGL with matrix debugging"
    echo -e "  ${GREEN}hextrail-native${NC} - Build hextrail for native with matrix debugging"
    echo -e "  ${GREEN}clean${NC}         - Clean all build files"
    echo -e "  ${GREEN}help${NC}          - Show this help message"
    echo ""
    echo -e "${CYAN}Examples:${NC}"
    echo -e "  $0 compare       # Compare native vs WebGL matrix operations"
    echo -e "  $0 hextrail-web  # Build WebGL hextrail with debugging"
    echo -e "  $0 hextrail-native # Build native hextrail with debugging"
    echo ""
    echo -e "${YELLOW}💡 Tip: Run '$0 compare' to debug matrix differences between native and WebGL!${NC}"
}

# Function to check dependencies
check_dependencies() {
    echo -e "${BLUE}🔍 Checking dependencies...${NC}"

    # Check for gcc
    if ! command -v gcc &> /dev/null; then
        echo -e "${RED}❌ gcc not found. Please install gcc.${NC}"
        exit 1
    fi

    # Check for emcc (Emscripten) first
    if ! command -v emcc &> /dev/null; then
        echo -e "${YELLOW}⚠️  emcc not found, trying to source emsdk environment...${NC}"

        # Try to source emsdk environment
        if [ -f "$HOME/src/emsdk/emsdk_env.sh" ]; then
            source "$HOME/src/emsdk/emsdk_env.sh"
            echo -e "${GREEN}✅ Sourced emsdk from $HOME/src/emsdk/emsdk_env.sh${NC}"
        elif [ -f "$(pwd)/emsdk/emsdk_env.sh" ]; then
            source "$(pwd)/emsdk/emsdk_env.sh"
            echo -e "${GREEN}✅ Sourced emsdk from $(pwd)/emsdk/emsdk_env.sh${NC}"
        else
            echo -e "${YELLOW}⚠️  emsdk_env.sh not found. WebGL builds will not work.${NC}"
            echo -e "${YELLOW}💡 Install with: git clone https://github.com/emscripten-core/emsdk.git && ./emsdk install latest && ./emsdk activate latest${NC}"
            WEBGL_AVAILABLE=false
        fi

        # Check again after sourcing
        if ! command -v emcc &> /dev/null; then
            echo -e "${YELLOW}⚠️  emcc still not found after sourcing emsdk. WebGL builds will not work.${NC}"
            WEBGL_AVAILABLE=false
        else
            echo -e "${GREEN}✅ emcc found after sourcing: $(which emcc)${NC}"
            WEBGL_AVAILABLE=true
        fi
    else
        echo -e "${GREEN}✅ emcc found in PATH: $(which emcc)${NC}"
        WEBGL_AVAILABLE=true
    fi

    # Check for required files
    if [ ! -f "matrix_test.c" ] || [ ! -f "matrix_debug.c" ] || [ ! -f "matrix_debug.h" ]; then
        echo -e "${RED}❌ Required files missing: matrix_test.c, matrix_debug.c, matrix_debug.h${NC}"
        exit 1
    fi

    echo -e "${GREEN}✅ Dependencies check complete${NC}"
}

# Function to compare native vs WebGL outputs
compare_outputs() {
    echo -e "${BLUE}🔍 Comparing native vs WebGL matrix outputs...${NC}"

    # Build both versions first
    echo -e "${YELLOW}📦 Building both versions...${NC}"
    build_hextrail "native"
    build_hextrail "web"

    # Create output directory
    mkdir -p matrix_debug_outputs

    # Capture native debug output
    echo -e "${YELLOW}📊 Capturing native debug output...${NC}"
    if [ -f "build_native_debug/hextrail_debug" ]; then
        echo -e "${YELLOW}⏳ Running native hextrail for 10 seconds...${NC}"
        timeout 10s cd build_native_debug && ./hextrail_debug 2>&1 | head -50 > ../matrix_debug_outputs/native_output.txt
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✅ Native output captured: matrix_debug_outputs/native_output.txt${NC}"
        else
            echo -e "${RED}❌ Failed to capture native output${NC}"
            return 1
        fi
    else
        echo -e "${RED}❌ Native hextrail not found: build_native_debug/hextrail_debug${NC}"
        return 1
    fi

    # Use auto_probe_web.sh for web debugging if available
    if [ -f "auto_probe_web.sh" ]; then
        echo -e "${YELLOW}🤖 Using auto_probe_web.sh for web debugging...${NC}"
        ./auto_probe_web.sh
    else
        echo -e "${RED}❌ auto_probe_web.sh not found - required for web debugging${NC}"
        echo -e "${YELLOW}💡 Please ensure auto_probe_web.sh is available for full comparison${NC}"
        return 1
    fi

    # Perform intelligent comparison of matrix operations
    echo -e "${YELLOW}🔍 Performing intelligent matrix operation comparison...${NC}"
    compare_matrix_operations_intelligently

    echo -e "${GREEN}✅ Comparison complete! Check matrix_debug_outputs/ for results${NC}"
}

# Function to compare matrix operations intelligently (from enhanced_compare.sh)
compare_matrix_operations_intelligently() {
    echo -e "${YELLOW}🔍 Comparing outputs intelligently...${NC}"
    
    if [ ! -f "matrix_debug_outputs/native_output.txt" ] || [ ! -f "matrix_debug_outputs/webgl_output.txt" ]; then
        echo -e "${RED}❌ Missing output files for comparison${NC}"
        return 1
    fi
    
    # Extract key matrix operations for comparison
    echo -e "${YELLOW}📊 Extracting key matrix operations...${NC}"
    
    # Extract matrix operations from native output
    grep -E "(glMatrixMode|glLoadIdentity|gluPerspective|gluLookAt|glTranslatef|glRotatef|glScalef|glPushMatrix|glPopMatrix)" matrix_debug_outputs/native_output.txt > matrix_debug_outputs/native_matrix_ops.txt
    
    # Extract matrix operations from WebGL output (assuming HTML format)
    grep -E "(glMatrixMode|glLoadIdentity|gluPerspective|gluLookAt|glTranslatef|glRotatef|glScalef|glPushMatrix|glPopMatrix)" matrix_debug_outputs/webgl_output.txt | sed 's/<[^>]*>//g' > matrix_debug_outputs/webgl_matrix_ops.txt
    
    # Compare the matrix operations
    echo -e "${YELLOW}🔍 Comparing matrix operations...${NC}"
    if diff matrix_debug_outputs/native_matrix_ops.txt matrix_debug_outputs/webgl_matrix_ops.txt > matrix_debug_outputs/matrix_diff.txt; then
        echo -e "${GREEN}✅ Matrix operations are identical! 🎉${NC}"
        echo -e "${CYAN}📋 Both versions perform the same matrix operations in the same order${NC}"
    else
        echo -e "${RED}❌ Matrix operations differ!${NC}"
        echo -e "${YELLOW}📋 Differences found:${NC}"
        cat matrix_debug_outputs/matrix_diff.txt
        echo ""
        echo -e "${CYAN}📊 Summary of differences:${NC}"
        echo -e "   Native operations: $(wc -l < matrix_debug_outputs/native_matrix_ops.txt)"
        echo -e "   WebGL operations:  $(wc -l < matrix_debug_outputs/webgl_matrix_ops.txt)"
    fi
    
    echo ""
    echo -e "${CYAN}📁 All output files saved in: matrix_debug_outputs/${NC}"
    echo -e "   - native_output.txt: Full native debug output"
    echo -e "   - webgl_output.txt: Full WebGL debug output"
    echo -e "   - native_matrix_ops.txt: Native matrix operations only"
    echo -e "   - webgl_matrix_ops.txt: WebGL matrix operations only"
    echo -e "   - matrix_diff.txt: Differences in matrix operations"
}

# Function to build hextrail with matrix debugging
build_hextrail() {
    local build_type="$1"

    if [ "$build_type" = "web" ]; then
        echo -e "${BLUE}🌐 Building hextrail for WebGL with matrix debugging...${NC}"
        build_script="build_web.sh"
        output_dir="build_web"
        run_instructions="🌐 To run: open build_web/index.html in your browser"
    elif [ "$build_type" = "native" ]; then
        echo -e "${BLUE}🖥️  Building hextrail for native with matrix debugging...${NC}"
        build_script=""
        output_dir="build_native_debug"
        run_instructions="🚀 To run: cd build_native_debug && ./hextrail_debug"
    else
        echo -e "${RED}❌ Invalid build type: $build_type${NC}"
        return 1
    fi

    # Build based on type (hextrail.c now has permanent conditional matrix debugging)
    if [ "$build_type" = "web" ]; then
        if [ ! -f "$build_script" ]; then
            echo -e "${RED}❌ $build_script not found${NC}"
            return 1
        fi
        ./$build_script
    else
        # Native build - incorporate logic directly
        build_native_hextrail
    fi

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ $build_type hextrail build successful!${NC}"
        echo -e "${CYAN}📁 Output files in: $output_dir/${NC}"
        echo -e "${YELLOW}$run_instructions${NC}"
    else
        echo -e "${RED}❌ $build_type hextrail build failed${NC}"
    fi
}

# Function to build native hextrail using existing Makefile
build_native_hextrail() {
    echo -e "${YELLOW}📦 Building native hextrail using Makefile...${NC}"

    # Create build directory
    mkdir -p build_native_debug
    cd build_native_debug

    # Temporarily modify the Makefile to include matrix debugging
    echo -e "${YELLOW}🔧 Modifying Makefile for matrix debugging...${NC}"
    cp ../hacks/glx/Makefile ../hacks/glx/Makefile.backup

    # Add matrix_debug.o to the hextrail target dependencies
    sed -i 's|hextrail:	hextrail.o	 normals.o $(HACK_TRACK_OBJS)|hextrail:	hextrail.o	 normals.o ../../matrix_debug.o $(HACK_TRACK_OBJS)|' ../hacks/glx/Makefile

    # Add matrix_debug.o to the link command
    sed -i 's|$(CC_HACK) -o $@ $@.o	 normals.o $(HACK_TRACK_OBJS) $(HACK_LIBS)|$(CC_HACK) -o $@ $@.o	 normals.o ../../matrix_debug.o $(HACK_TRACK_OBJS) $(HACK_LIBS)|' ../hacks/glx/Makefile

    # Add matrix_debug.o compilation rule (only if it doesn't exist)
    if ! grep -q "../../matrix_debug.o:" ../hacks/glx/Makefile; then
        echo "" >> ../hacks/glx/Makefile
        echo "../../matrix_debug.o: ../../matrix_debug.c ../../matrix_debug.h" >> ../hacks/glx/Makefile
        echo -e "\t\$(CC) \$(CFLAGS) -DMATRIX_DEBUG -c \$< -o \$@" >> ../hacks/glx/Makefile
    fi

    # Build using the existing Makefile
    cd ../hacks/glx
    make hextrail CFLAGS="-DMATRIX_DEBUG"

    # Copy the built binary to our build directory
    cp hextrail ../../build_native_debug/hextrail_debug

    # Restore original Makefile
    mv Makefile.backup Makefile

    cd ../..
}

# Function to clean all build files
clean_all() {
    echo -e "${BLUE}🧹 Cleaning all build files...${NC}"

    # Clean matrix test files
    make -f Makefile.matrix_test clean

    # Clean hextrail build directories
    if [ -d "build_web" ]; then
        rm -rf build_web
        echo -e "${GREEN}✅ Cleaned build_web/${NC}"
    fi

    if [ -d "build_native_debug" ]; then
        rm -rf build_native_debug
        echo -e "${GREEN}✅ Cleaned build_native_debug/${NC}"
    fi

    # Clean matrix debug outputs
    if [ -d "matrix_debug_outputs" ]; then
        rm -rf matrix_debug_outputs
        echo -e "${GREEN}✅ Cleaned matrix_debug_outputs/${NC}"
    fi

    echo -e "${GREEN}✅ All build files cleaned!${NC}"
}

# Main script logic
main() {
    # Check dependencies first
    check_dependencies

    # Parse command
    case "${1:-help}" in
        "compare")
            compare_outputs
            ;;
        "hextrail-web")
            build_hextrail "web"
            ;;
        "hextrail-native")
            build_hextrail "native"
            ;;
        "clean")
            clean_all
            ;;
        "help"|*)
            show_usage
            ;;
    esac
}

# Run main function
main "$@"
