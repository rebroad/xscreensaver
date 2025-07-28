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
    echo -e "  ${GREEN}test${NC}          - Build and run matrix test program (both native and WebGL)"
    echo -e "  ${GREEN}compare${NC}       - Build both versions and compare matrix outputs"
    echo -e "  ${GREEN}hextrail-web${NC}  - Build hextrail for WebGL with matrix debugging"
    echo -e "  ${GREEN}hextrail-native${NC} - Build hextrail for native with matrix debugging"
    echo -e "  ${GREEN}clean${NC}         - Clean all build files"
    echo -e "  ${GREEN}help${NC}          - Show this help message"
    echo ""
    echo -e "${CYAN}Examples:${NC}"
    echo -e "  $0 test          # Test the matrix debugging framework"
    echo -e "  $0 compare       # Compare native vs WebGL matrix operations"
    echo -e "  $0 hextrail-web  # Build WebGL hextrail with debugging"
    echo -e "  $0 hextrail-native # Build native hextrail with debugging"
    echo ""
    echo -e "${YELLOW}💡 Tip: Run '$0 test' first to verify the framework works!${NC}"
}

# Function to check dependencies
check_dependencies() {
    echo -e "${BLUE}🔍 Checking dependencies...${NC}"
    
    # Check for gcc
    if ! command -v gcc &> /dev/null; then
        echo -e "${RED}❌ gcc not found. Please install gcc.${NC}"
        exit 1
    fi
    
    # Check for emcc (Emscripten)
    if ! command -v emcc &> /dev/null; then
        echo -e "${YELLOW}⚠️  emcc not found. WebGL builds will not work.${NC}"
        echo -e "${YELLOW}💡 Install with: git clone https://github.com/emscripten-core/emsdk.git && ./emsdk install latest && ./emsdk activate latest${NC}"
        WEBGL_AVAILABLE=false
    else
        WEBGL_AVAILABLE=true
    fi
    
    # Check for required files
    if [ ! -f "matrix_test.c" ] || [ ! -f "matrix_debug.c" ] || [ ! -f "matrix_debug.h" ]; then
        echo -e "${RED}❌ Required files missing: matrix_test.c, matrix_debug.c, matrix_debug.h${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✅ Dependencies check complete${NC}"
}

# Function to build and run matrix test
run_matrix_test() {
    echo -e "${BLUE}🧪 Building and running matrix test...${NC}"
    
    # Build native version
    echo -e "${YELLOW}📦 Building native version...${NC}"
    make -f Makefile.matrix_test native
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Native build successful${NC}"
        
        # Run native test
        echo -e "${YELLOW}🚀 Running native test...${NC}"
        echo -e "${CYAN}=== NATIVE OUTPUT ===${NC}"
        ./matrix_test_native
        echo -e "${CYAN}=====================${NC}"
    else
        echo -e "${RED}❌ Native build failed${NC}"
        return 1
    fi
    
    # Build WebGL version if available
    if [ "$WEBGL_AVAILABLE" = true ]; then
        echo -e "${YELLOW}📦 Building WebGL version...${NC}"
        make -f Makefile.matrix_test webgl
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✅ WebGL build successful${NC}"
            echo -e "${YELLOW}🌐 WebGL files created:${NC}"
            echo -e "   - matrix_test_webgl.html"
            echo -e "   - matrix_test_webgl.js"
            echo -e "   - matrix_test_webgl.wasm"
            echo -e "${CYAN}💡 Open matrix_test_webgl.html in your browser to run the WebGL test${NC}"
        else
            echo -e "${RED}❌ WebGL build failed${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️  Skipping WebGL build (emcc not available)${NC}"
    fi
}

# Function to compare native vs WebGL outputs
compare_outputs() {
    echo -e "${BLUE}🔍 Comparing native vs WebGL matrix outputs...${NC}"
    
    # Build both versions
    echo -e "${YELLOW}📦 Building both versions...${NC}"
    make -f Makefile.matrix_test native
    
    if [ "$WEBGL_AVAILABLE" = true ]; then
        make -f Makefile.matrix_test webgl
    else
        echo -e "${RED}❌ Cannot compare: WebGL build not available (emcc missing)${NC}"
        return 1
    fi
    
    # Create output directories
    mkdir -p matrix_debug_outputs
    
    # Run native and capture output
    echo -e "${YELLOW}📊 Capturing native output...${NC}"
    ./matrix_test_native > matrix_debug_outputs/native_output.txt 2>&1
    
    # Run WebGL and capture output (this is trickier)
    echo -e "${YELLOW}📊 Capturing WebGL output...${NC}"
    echo -e "${CYAN}💡 To capture WebGL output:${NC}"
    echo -e "   1. Open matrix_test_webgl.html in your browser"
    echo -e "   2. Open browser developer tools (F12)"
    echo -e "   3. Go to Console tab"
    echo -e "   4. Copy the console output"
    echo -e "   5. Save it to: matrix_debug_outputs/webgl_output.txt"
    echo ""
    echo -e "${YELLOW}⏳ Waiting for you to capture WebGL output...${NC}"
    echo -e "${CYAN}Press Enter when you've saved the WebGL output to matrix_debug_outputs/webgl_output.txt${NC}"
    read -p ""
    
    # Check if WebGL output exists
    if [ ! -f "matrix_debug_outputs/webgl_output.txt" ]; then
        echo -e "${RED}❌ WebGL output file not found: matrix_debug_outputs/webgl_output.txt${NC}"
        echo -e "${YELLOW}💡 Please capture the WebGL console output and save it to that file${NC}"
        return 1
    fi
    
    # Compare outputs
    echo -e "${YELLOW}🔍 Comparing outputs...${NC}"
    if diff matrix_debug_outputs/native_output.txt matrix_debug_outputs/webgl_output.txt > matrix_debug_outputs/diff.txt; then
        echo -e "${GREEN}✅ Outputs are identical! Matrix operations match perfectly.${NC}"
    else
        echo -e "${RED}❌ Outputs differ! Check matrix_debug_outputs/diff.txt for details.${NC}"
        echo -e "${YELLOW}📋 Differences found:${NC}"
        cat matrix_debug_outputs/diff.txt
    fi
    
    echo -e "${CYAN}📁 Output files saved in: matrix_debug_outputs/${NC}"
}

# Function to build hextrail for WebGL
build_hextrail_web() {
    echo -e "${BLUE}🌐 Building hextrail for WebGL with matrix debugging...${NC}"
    
    if [ ! -f "build_web.sh" ]; then
        echo -e "${RED}❌ build_web.sh not found${NC}"
        return 1
    fi
    
    # Run the WebGL build script
    ./build_web.sh
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ WebGL hextrail build successful!${NC}"
        echo -e "${CYAN}📁 Output files in: build_web/${NC}"
        echo -e "${YELLOW}🌐 To run: open build_web/index.html in your browser${NC}"
    else
        echo -e "${RED}❌ WebGL hextrail build failed${NC}"
    fi
}

# Function to build hextrail for native
build_hextrail_native() {
    echo -e "${BLUE}🖥️  Building hextrail for native with matrix debugging...${NC}"
    
    if [ ! -f "build_native_debug.sh" ]; then
        echo -e "${RED}❌ build_native_debug.sh not found${NC}"
        return 1
    fi
    
    # Run the native build script
    ./build_native_debug.sh
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Native hextrail build successful!${NC}"
        echo -e "${CYAN}📁 Output file: build_native_debug/hextrail_debug${NC}"
        echo -e "${YELLOW}🚀 To run: cd build_native_debug && ./hextrail_debug${NC}"
    else
        echo -e "${RED}❌ Native hextrail build failed${NC}"
    fi
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
        "test")
            run_matrix_test
            ;;
        "compare")
            compare_outputs
            ;;
        "hextrail-web")
            build_hextrail_web
            ;;
        "hextrail-native")
            build_hextrail_native
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