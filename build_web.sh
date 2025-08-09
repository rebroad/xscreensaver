#!/bin/bash

# HexTrail Web Build Script
# This script compiles hextrail for the web using emscripten

set -e

# Parse command line arguments
DEBUG_MODE=false
MEMORY_DEBUG=false
MATRIX_DEBUG=false
START_SERVER=true  # Start server by default
while [[ $# -gt 0 ]]; do
    case $1 in
        -debug)
            DEBUG_MODE=true
            shift
            ;;
        -memory)
            MEMORY_DEBUG=true
            shift
            ;;
        -matrix-debug)
            MATRIX_DEBUG=true
            shift
            ;;
        -noserver)
            START_SERVER=false
            shift
            ;;
        *)
            echo "Usage: $0 [-debug] [-memory] [-noserver]"
            echo "  -debug: Enable FINDBUG mode for GL error hunting"
            echo "  -memory: Enable memory debugging and leak detection"
            echo "  -noserver: Don't start web server automatically (default: start server)"
            exit 1
            ;;
    esac
done

# Get the repository root directory (same directory as this script)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check for emcc (Emscripten) first, source emsdk only if needed
if ! command -v emcc &> /dev/null; then
    echo -e "${YELLOW}⚠️  emcc not found, trying to source emsdk environment...${NC}"

    # Try to source emsdk environment
    if [ -f "$HOME/src/emsdk/emsdk_env.sh" ]; then
        source "$HOME/src/emsdk/emsdk_env.sh"
        echo -e "${GREEN}✅ Sourced emsdk from $HOME/src/emsdk/emsdk_env.sh${NC}"
    elif [ -f "$REPO_ROOT/emsdk/emsdk_env.sh" ]; then
        source "$REPO_ROOT/emsdk/emsdk_env.sh"
        echo -e "${GREEN}✅ Sourced emsdk from $REPO_ROOT/emsdk/emsdk_env.sh${NC}"
    else
        echo -e "${YELLOW}⚠️  emsdk_env.sh not found. Make sure emscripten is in your PATH.${NC}"
    fi
else
    echo -e "${GREEN}✅ emcc found in PATH: $(which emcc)${NC}"
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

UTILS_DIR="$REPO_ROOT/utils"
GLX_DIR="$REPO_ROOT/hacks/glx"
HACKS_DIR="$REPO_ROOT/hacks"
JWXYZ_DIR="$REPO_ROOT/jwxyz"


# Function to start web server
start_web_server() {
    echo -e "${YELLOW}🔍 Checking for existing web servers...${NC}"

    # Get the absolute path of our build_web directory
    EXPECTED_DIR="$(pwd)/build_web"

    # Check if there's already a server running on any port serving our files
    for test_port in 8000 8001 8002 8003 8004 8005; do
        if timeout 2 curl -s http://localhost:$test_port > /dev/null 2>&1; then
            # Test if it's serving our hextrail page
            if timeout 2 curl -s http://localhost:$test_port | grep -q "HexTrail"; then
                echo -e "${YELLOW}🔍 Found HexTrail server on port $test_port, verifying directory...${NC}"

                # Check if the server is serving from our build_web directory
                SERVER_PIDS=$(lsof -ti:$test_port 2>/dev/null)
                if [ ! -z "$SERVER_PIDS" ]; then
                    for pid in $SERVER_PIDS; do
                        SERVER_CWD=$(lsof -p $pid 2>/dev/null | grep cwd | awk '{print $NF}')
                        if [ "$SERVER_CWD" = "$EXPECTED_DIR" ]; then
                            echo -e "${GREEN}✅ Web server already running on localhost:$test_port (serving from correct directory)${NC}"
                            echo -e "${CYAN}🌐 Open: http://localhost:$test_port${NC}"
                            return 0
                        else
                            echo -e "${YELLOW}⚠️  Server on port $test_port serving from: $SERVER_CWD (not our build_web)${NC}"
                        fi
                    done
                else
                    echo -e "${YELLOW}⚠️  Could not determine server directory for port $test_port${NC}"
                fi
            fi
        fi
    done

    # Find an available port starting from 8000
    echo -e "${YELLOW}🔍 Finding available port...${NC}"
    PORT=8000
    for test_port in 8000 8001 8002 8003 8004 8005 8006 8007 8008 8009 8010; do
        if ! timeout 2 curl -s http://localhost:$test_port > /dev/null 2>&1; then
            PORT=$test_port
            break
        fi
    done

    if [ $PORT -gt 8010 ]; then
        echo -e "${RED}❌ Could not find available port between 8000-8010${NC}"
        echo -e "${YELLOW}💡 Please manually start server: cd build_web && python3 -m http.server 8000${NC}"
        return 1
    fi

    # Start web server
    echo -e "${YELLOW}🚀 Starting web server on port $PORT...${NC}"

    if command -v python3 &> /dev/null; then
        cd build_web && python3 -m http.server $PORT > /dev/null 2>&1 &
        SERVER_PID=$!
        cd ..
        sleep 2

        # Check if server started successfully
        if kill -0 $SERVER_PID 2>/dev/null && curl -s http://localhost:$PORT > /dev/null 2>&1; then
            echo -e "${GREEN}✅ Web server started successfully (PID: $SERVER_PID, Port: $PORT)${NC}"
            echo -e "${CYAN}🌐 Open: http://localhost:$PORT${NC}"
            echo -e "${YELLOW}💡 Server will run in background. To stop: kill $SERVER_PID${NC}"

            # Store server info for potential cleanup
            echo $PORT > /tmp/hextrail_web_port.txt
            echo $SERVER_PID > /tmp/hextrail_web_pid.txt
        else
            echo -e "${RED}❌ Failed to start web server${NC}"
            echo -e "${YELLOW}💡 Please manually start server: cd build_web && python3 -m http.server 8000${NC}"
            return 1
        fi
    else
        echo -e "${RED}❌ python3 not found - cannot start web server${NC}"
        echo -e "${YELLOW}💡 Please install python3 or manually serve the files${NC}"
        return 1
    fi
}

echo -e "${BLUE}🚀 Building HexTrail for Web with Emscripten${NC}"
if [ "$DEBUG_MODE" = true ]; then
    echo -e "${YELLOW}🐛 FINDBUG mode enabled - GL error checking will be active${NC}"
fi
if [ "$MEMORY_DEBUG" = true ]; then
    echo -e "${YELLOW}🧠 Memory debugging enabled - leak detection and profiling active${NC}"
fi
echo -e "${YELLOW}📁 Repository root: $REPO_ROOT${NC}"
echo -e "${YELLOW}📁 Utils directory: $UTILS_DIR${NC}"
echo -e "${YELLOW}📁 GLX directory: $GLX_DIR${NC}"
echo -e "${YELLOW}📁 JWXYZ directory: $JWXYZ_DIR${NC}"

# Check if emscripten is available
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}❌ Emscripten not found. Please install emscripten first.${NC}"
    echo -e "${YELLOW}💡 You can install it with: git clone https://github.com/emscripten-core/emsdk.git && ./emsdk install latest && ./emsdk activate latest${NC}"
    exit 1
fi

# Check if we're in the right directory
if [ ! -f "hacks/glx/hextrail_web_main.c" ]; then
    echo -e "${RED}❌ hextrail_web_main.c not found. Please run this script from the project root directory.${NC}"
    exit 1
fi

# Create build directory
mkdir -p build_web
cd build_web

echo -e "${YELLOW}📦 Compiling HexTrail...${NC}"

# Build emcc command with conditional debug flags
EMCC_ARGS=(
    -DSTANDALONE
    -DUSE_GL
    -DHAVE_CONFIG_H
    -DWEB_BUILD
    -DHAVE_JWXYZ
    -s USE_WEBGL2=1
    -s FULL_ES3=1
    -s ALLOW_MEMORY_GROWTH=1
    -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPU8']
    -s EXPORTED_FUNCTIONS=['_main','_init_hextrail','_draw_hextrail','_reshape_hextrail_wrapper','_free_hextrail','_set_speed','_set_thickness','_set_spin','_set_wander','_stop_rendering','_start_rendering','_handle_mouse_drag','_handle_mouse_wheel','_handle_keypress','_set_debug_level','_track_memory_allocation','_track_memory_free','_print_memory_stats']
    -s MIN_WEBGL_VERSION=2
    -O3
    -s RETAIN_COMPILER_SETTINGS=0
    -I$JWXYZ_DIR
    -I.
    -I$REPO_ROOT
    -I$HACKS_DIR
    -I$UTILS_DIR
    -I$GLX_DIR
    $GLX_DIR/hextrail_web_main.c
    $UTILS_DIR/colors.c
    $UTILS_DIR/hsv.c
    $UTILS_DIR/yarandom.c
    $UTILS_DIR/usleep.c
    $HACKS_DIR/screenhack.c
    $GLX_DIR/rotator.c
    $GLX_DIR/gltrackball.c
    $GLX_DIR/normals.c
    $JWXYZ_DIR/jwxyz-timers.c
    -o index.html
    --shell-file $REPO_ROOT/web/template.html
)

# Add debug flags if requested
if [ "$DEBUG_MODE" = true ]; then
    EMCC_ARGS=(-DFINDBUG_MODE "${EMCC_ARGS[@]}")
fi

# Add memory debugging flags if requested
if [ "$MEMORY_DEBUG" = true ]; then
    EMCC_ARGS=(
        -g
        -s ASSERTIONS=1
        -s SAFE_HEAP=1
        -s STACK_OVERFLOW_CHECK=1
        -s INITIAL_MEMORY=16777216     # 16MB
        -s MAXIMUM_MEMORY=268435456    # 256MB
        -s MEMORY_GROWTH_STEP=16777216 # 16MB
        -s ABORTING_MALLOC=0
        "${EMCC_ARGS[@]}"
    )
fi

# Add matrix debugging flag and source file if requested
if [ "$MATRIX_DEBUG" = true ]; then
    EMCC_ARGS=(-DMATRIX_DEBUG "$REPO_ROOT/matrix_debug.c" "${EMCC_ARGS[@]}")
fi

# Compile with emscripten using custom HTML template
emcc "${EMCC_ARGS[@]}"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Build successful!${NC}"
    echo -e "${BLUE}📁 Output files:${NC}"
    echo -e "   - index.html (main HTML file)"
    echo -e "   - index.js (JavaScript module)"
    echo -e "   - index.wasm (WebAssembly binary)"

    # Start web server (unless disabled)
    if [ "$START_SERVER" = true ]; then
        start_web_server
    else
        echo -e "${YELLOW}🌐 To run locally:${NC}"
        echo -e "   cd build_web"
        echo -e "   python3 -m http.server 8000"
        echo -e "   Then open http://localhost:8000"
        echo -e "${CYAN}💡 Server start disabled with -noserver flag${NC}"
    fi

    echo -e "${GREEN}📋 Files in build_web directory${NC}"

else
    echo -e "${RED}❌ Build failed!${NC}"
    exit 1
fi
