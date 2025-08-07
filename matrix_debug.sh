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

    # Check if we have a web server available
    if command -v python3 &> /dev/null; then
        echo -e "${GREEN}🚀 Starting Python web server...${NC}"

        # Find an available port
        PORT=8000
        while netstat -tuln 2>/dev/null | grep -q ":$PORT " || ss -tuln 2>/dev/null | grep -q ":$PORT "; do
            PORT=$((PORT + 1))
            if [ $PORT -gt 8100 ]; then
                echo -e "${RED}❌ Could not find available port between 8000-8100${NC}"
                break
            fi
        done

        echo -e "${YELLOW}💡 WebGL test will be available at: http://localhost:$PORT/matrix_test_webgl.html${NC}"
        echo -e "${YELLOW}📋 Instructions:${NC}"
        echo -e "   1. Open http://localhost:$PORT/matrix_test_webgl.html in your browser"
        echo -e "   2. Open browser developer tools (F12)"
        echo -e "   3. Go to Console tab"
        echo -e "   4. Copy the console output"
        echo -e "   5. Save it to: matrix_debug_outputs/webgl_output.txt"
        echo ""
        echo -e "${YELLOW}⏳ Starting web server on port $PORT...${NC}"
        python3 -m http.server $PORT > /dev/null 2>&1 &
        SERVER_PID=$!

        # Wait a moment for server to start
        sleep 1

        # Check if server started successfully
        if kill -0 $SERVER_PID 2>/dev/null; then
            echo -e "${GREEN}✅ Web server started (PID: $SERVER_PID, Port: $PORT)${NC}"
            echo -e "${YELLOW}💡 Server will be stopped when you press Enter${NC}"
            echo ""
            echo -e "${YELLOW}⏳ Waiting for you to capture WebGL output...${NC}"
            echo -e "${CYAN}Press Enter when you've saved the WebGL output to matrix_debug_outputs/webgl_output.txt${NC}"
            read -p ""

            # Stop the web server
            if kill $SERVER_PID 2>/dev/null; then
                echo -e "${GREEN}✅ Web server stopped${NC}"
            fi
        else
            echo -e "${RED}❌ Failed to start web server${NC}"
            echo -e "${YELLOW}💡 Falling back to manual instructions...${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️  Python3 not found. You'll need to serve the files manually.${NC}"
        echo -e "${YELLOW}💡 Options:${NC}"
        echo -e "   1. Install Python3 and run: python3 -m http.server 8000"
        echo -e "   2. Use Node.js: npx http-server"
        echo -e "   3. Open matrix_test_webgl.html directly as a file (may not work due to CORS)"
        echo ""
        echo -e "${YELLOW}📁 File location: $(pwd)/matrix_test_webgl.html${NC}"
        echo -e "${YELLOW}💡 To capture WebGL output:${NC}"
        echo -e "   1. Open matrix_test_webgl.html in your browser"
        echo -e "   2. Open browser developer tools (F12)"
        echo -e "   3. Go to Console tab"
        echo -e "   4. Copy the console output"
        echo -e "   5. Save it to: matrix_debug_outputs/webgl_output.txt"
        echo ""
        echo -e "${YELLOW}⏳ Waiting for you to capture WebGL output...${NC}"
        echo -e "${CYAN}Press Enter when you've saved the WebGL output to matrix_debug_outputs/webgl_output.txt${NC}"
        read -p ""
    fi

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

    # Temporarily modify hextrail.c to include matrix debugging
    echo -e "${YELLOW}🔧 Adding matrix debugging to hextrail.c...${NC}"
    cp hacks/glx/hextrail.c hacks/glx/hextrail.c.backup
    echo '#include "../../matrix_debug.h"' > hacks/glx/hextrail.c.tmp
    cat hacks/glx/hextrail.c >> hacks/glx/hextrail.c.tmp
    mv hacks/glx/hextrail.c.tmp hacks/glx/hextrail.c

    # Build based on type
    if [ "$build_type" = "web" ]; then
        if [ ! -f "$build_script" ]; then
            echo -e "${RED}❌ $build_script not found${NC}"
            mv hacks/glx/hextrail.c.backup hacks/glx/hextrail.c
            return 1
        fi
        ./$build_script
    else
        # Native build - incorporate logic directly
        build_native_hextrail
    fi

    # Restore original hextrail.c
    echo -e "${YELLOW}🧹 Restoring original hextrail.c...${NC}"
    mv hacks/glx/hextrail.c.backup hacks/glx/hextrail.c

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

    # Add matrix_debug.o to the hextrail target
    sed -i 's|hextrail: hextrail.o|hextrail: hextrail.o ../../matrix_debug.o|' ../hacks/glx/Makefile

    # Add matrix_debug.o compilation rule
    echo "" >> ../hacks/glx/Makefile
    echo "../../matrix_debug.o: ../../matrix_debug.c ../../matrix_debug.h" >> ../hacks/glx/Makefile
    echo -e "\t\$(CC) \$(CFLAGS) -DMATRIX_DEBUG -c \$< -o \$@" >> ../hacks/glx/Makefile

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
