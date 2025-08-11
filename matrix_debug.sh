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
    echo -e "${BLUE}🔧 Matrix Debugging Framework with Validation${NC}"
    echo -e "${YELLOW}Usage: $0 [COMMAND]${NC}"
    echo ""
    echo -e "${CYAN}Available Commands:${NC}"
    echo -e "  ${GREEN}compare${NC}       - Build both versions and compare matrix outputs with validation"
    echo -e "  ${GREEN}web${NC}           - Build hextrail for WebGL with matrix debugging & validation"
    echo -e "  ${GREEN}native${NC}        - Build hextrail for native with matrix debugging & validation"
    echo -e "  ${GREEN}clean${NC}         - Clean all build files"
    echo -e "  ${GREEN}help${NC}          - Show this help message"
    echo ""
    echo -e "${CYAN}Examples:${NC}"
    echo -e "  $0 compare       # Compare native vs WebGL with deterministic random & validation"
    echo -e "  $0 web           # Build WebGL hextrail with matrix validation"
    echo -e "  $0 native        # Build native hextrail with matrix validation"
    echo ""
    echo -e "${PURPLE}🎯 Features: Deterministic random, matrix validation, before/after debugging${NC}"
    echo -e "${PURPLE}💡 Tip: Run '$0 compare' for pixel-perfect native vs WebGL comparison!${NC}"
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
    if [ ! -f "matrix_debug.c" ] || [ ! -f "matrix_debug.h" ]; then
        echo -e "${RED}❌ Required files missing: matrix_debug.c, matrix_debug.h${NC}"
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

    # Build web version and capture server port
    echo -e "${YELLOW}📦 Building web version and capturing server port...${NC}"
    WEB_BUILD_OUTPUT=$(build_hextrail "web" 2>&1)
    echo "$WEB_BUILD_OUTPUT"  # Still show the output to user

    # Extract port from build output
    WEB_SERVER_PORT=$(echo "$WEB_BUILD_OUTPUT" | grep "WEBSERVER_PORT:" | cut -d: -f2)
    if [ -z "$WEB_SERVER_PORT" ]; then
        echo -e "${RED}❌ Failed to detect web server port${NC}"
        return 1
    fi
    echo -e "${GREEN}🔍 Detected web server port: $WEB_SERVER_PORT${NC}"

    # Create output directory
    mkdir -p matrix_debug_outputs

    # Capture native debug output
    echo -e "${YELLOW}📊 Capturing native debug output...${NC}"
    if [ -f "build_native_debug/hextrail_debug" ]; then
        echo -e "${YELLOW}⏳ Running native hextrail for 10 seconds...${NC}"
        # Keep it simple: run windowed with timeout and capture all output
        set +e
        # Calculate screen geometry for native window positioning (right side)
        export screen_width=$(xrandr --current | grep '*' | uniq | awk '{print $1}' | cut -d 'x' -f1 | head -1)
        export screen_height=$(xrandr --current | grep '*' | uniq | awk '{print $1}' | cut -d 'x' -f2 | head -1)
        [ -z "$screen_width" ] && screen_width=1920
        [ -z "$screen_height" ] && screen_height=1080
        export window_width=$((screen_width / 2))
        export indow_height=$((screen_height - 100))
        export right_x=$((screen_width / 2))  # Position on right side
        export y_pos=50

        # Start browser positioning in background
        (
            title_pattern="HexTrail - XScreenSaver Web Version"
            echo -e "${CYAN}🔎 Searching for browser window by title pattern: $title_pattern${NC}"

            # Capture current window list for debugging
            mkdir -p matrix_debug_outputs
            {
                echo "=== wmctrl -lx ===";
                wmctrl -lx 2>/dev/null || echo "(wmctrl not available)";
                echo "\n=== wmctrl -l ===";
                wmctrl -l 2>/dev/null || true;
            } > matrix_debug_outputs/window_list.txt
            echo -e "${CYAN}📝 Saved window list to: matrix_debug_outputs/window_list.txt${NC}"
            # Also print likely candidates to console
            if command -v wmctrl >/dev/null 2>&1; then
                echo -e "${CYAN}🔎 Candidate windows (wmctrl -l filtered):${NC}"
                wmctrl -l | grep -Ei "HexTrail|localhost|Chrome|Chromium|Firefox" | head -n 10 || true
            fi

            if ! command -v xdotool >/dev/null 2>&1 && ! command -v wmctrl >/dev/null 2>&1; then
                echo -e "${YELLOW}⚠️  xdotool/wmctrl not found; cannot reposition browser automatically${NC}"
                exit 0
            fi

            # Add a longer initial delay to allow browser window to appear
            echo -e "${CYAN}⏳ Waiting for browser window to appear...${NC}"
            sleep 1

            # Use a more aggressive detection loop - very short delays initially
            for i in $(seq 1 30); do
                if command -v xdotool >/dev/null 2>&1; then
                    echo -e "${CYAN}🔎 xdotool search --name \"$title_pattern\" (attempt $i)${NC}"
                    bid=$(xdotool search --name "$title_pattern" | head -n1 || true)
                    if [ -n "$bid" ]; then
                        echo -e "${GREEN}✅ Found window id via xdotool: $bid${NC}"
                        xdotool windowmove "$bid" "$left_x" "$y_pos" 2>/dev/null || true
                        xdotool windowsize "$bid" "$window_width" "$window_height" 2>/dev/null || true
                        break
                    fi
                fi
                if command -v wmctrl >/dev/null 2>&1; then
                    echo -e "${CYAN}🔎 wmctrl -l | grep -iE \"$title_pattern\" (attempt $i)${NC}"
                    if wmctrl -l | grep -qiE "$title_pattern"; then
                        wline=$(wmctrl -l | grep -iE "$title_pattern" | head -n1)
                        echo -e "${GREEN}✅ Found window line via wmctrl: $wline${NC}"
                        wid=$(echo "$wline" | awk '{print $1}')
                        if [ -n "$wid" ]; then
                            wmctrl -i -r "$wid" -e 0,$left_x,$y_pos,$window_width,$window_height 2>/dev/null || true
                            break
                        fi
                    fi
                fi

                # Use very short delays for the first 20 iterations, then longer delays
                if [ $i -lt 20 ]; then
                    sleep 0.05  # 50ms delay for first 20 attempts
                elif [ $i -lt 40 ]; then
                    sleep 0.1   # 100ms delay for next 20 attempts
                else
                    sleep 0.25  # 250ms delay for remaining attempts
                fi
            done

            if [ "$i" = "120" ]; then
                echo -e "${YELLOW}⚠️  Did not find a matching browser window. See matrix_debug_outputs/window_list.txt for titles/classes.${NC}"
                if [ "${XDG_SESSION_TYPE:-}" = "wayland" ]; then
                    echo -e "${YELLOW}⚠️  Wayland session detected; window movement may not be supported by xdotool/wmctrl.${NC}"
                fi
            fi

            # Store browser window ID for later closing
            if command -v xdotool >/dev/null 2>&1; then
                BROWSER_WINDOW_ID=$(xdotool search --name "$title_pattern" | head -n1 || true)
            elif command -v wmctrl >/dev/null 2>&1; then
                BROWSER_WINDOW_LINE=$(wmctrl -l | grep -iE "$title_pattern" | head -n1)
                BROWSER_WINDOW_ID=$(echo "$BROWSER_WINDOW_LINE" | awk '{print $1}')
            fi

            # Export browser window ID for parent shell
            if [ -n "$BROWSER_WINDOW_ID" ]; then
                echo "export BROWSER_WINDOW_ID=$BROWSER_WINDOW_ID" > /tmp/matrix_debug_browser_id.sh
            fi
        ) &

        # Run native hextrail and capture output
        (cd build_native_debug && timeout 10s ./hextrail_debug -window -xrm "*geometry:800x600+${right_x}+${y_pos}" > ../matrix_debug_outputs/native_output.txt 2>&1)
        native_status=$?

        # Close browser window when native hextrail ends
        if [ -f /tmp/matrix_debug_browser_id.sh ]; then
            source /tmp/matrix_debug_browser_id.sh
            if [ -n "$BROWSER_WINDOW_ID" ]; then
                if command -v xdotool >/dev/null 2>&1; then
                    xdotool windowclose "$BROWSER_WINDOW_ID" 2>/dev/null || true
                elif command -v wmctrl >/dev/null 2>&1; then
                    wmctrl -i -c "$BROWSER_WINDOW_ID" 2>/dev/null || true
                fi
            fi
            rm -f /tmp/matrix_debug_browser_id.sh
        fi

        set -e
        if [ $native_status -eq 124 ]; then
            # timeout exit code 124 means it timed out as expected; still treat as success
            echo -e "${GREEN}✅ Native output captured (timed): matrix_debug_outputs/native_output.txt${NC}"
        elif [ $native_status -eq 0 ]; then
            echo -e "${GREEN}✅ Native output captured: matrix_debug_outputs/native_output.txt${NC}"
        else
            echo -e "${RED}❌ Failed to capture native output${NC}"
            return 1
        fi
    else
        echo -e "${RED}❌ Native hextrail not found: build_native_debug/hextrail_debug${NC}"
        return 1
    fi

    # Use auto_probe_web.sh for web debugging if available to capture output
    if [ -f "auto_probe_web.sh" ]; then
        echo -e "${YELLOW}🤖 Using auto_probe_web.sh for web debugging on port $WEB_SERVER_PORT...${NC}"
        # Add a small delay to ensure window positioning has completed
        sleep 2
        ./auto_probe_web.sh $WEB_SERVER_PORT
    else
        echo -e "${YELLOW}⚠️  auto_probe_web.sh not found; skipping automated web capture${NC}"
    fi

    # Perform intelligent comparison of matrix operations
    echo -e "${YELLOW}🔍 Performing intelligent matrix operation comparison...${NC}"
    compare_matrix_operations_intelligently

    echo -e "${GREEN}✅ Comparison complete! Check matrix_debug_outputs/ for results${NC}"
}

# Function to compare matrix operations intelligently (from enhanced_compare.sh)
compare_matrix_operations_intelligently() {
    echo -e "${YELLOW}🔍 Comparing outputs intelligently...${NC}"

    if [ ! -f "matrix_debug_outputs/native_output.txt" ] || [ ! -f "matrix_debug_outputs/web_debug_output.txt" ]; then
        echo -e "${RED}❌ Missing output files for comparison${NC}"
        return 1
    fi

    # Extract key matrix operations for comparison
    echo -e "${YELLOW}📊 Extracting key matrix operations...${NC}"

    # Extract matrix operations from native output
    grep -E "(glMatrixMode|glLoadIdentity|gluPerspective|gluLookAt|glTranslatef|glRotatef|glScalef|glPushMatrix|glPopMatrix)" matrix_debug_outputs/native_output.txt > matrix_debug_outputs/native_matrix_ops.txt

    # Extract matrix operations from WebGL output
    grep -E "(glMatrixMode|glLoadIdentity|gluPerspective|gluLookAt|glTranslatef|glRotatef|glScalef|glPushMatrix|glPopMatrix)" matrix_debug_outputs/web_debug_output.txt > matrix_debug_outputs/webgl_matrix_ops.txt

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

        # Open meld to show differences
        if command -v meld &> /dev/null; then
            echo -e "${BLUE}🔍 Opening meld to show differences...${NC}"
            meld matrix_debug_outputs/native_matrix_ops.txt matrix_debug_outputs/webgl_matrix_ops.txt &
        else
            echo -e "${YELLOW}⚠️  meld not found. Install it to view differences graphically${NC}"
            echo -e "${CYAN}💡 Differences saved to: matrix_debug_outputs/matrix_diff.txt${NC}"
        fi
    fi

    echo ""
    echo -e "${CYAN}📁 All output files saved in: matrix_debug_outputs/${NC}"
    echo -e "   - native_output.txt: Full native debug output"
    echo -e "   - web_debug_output.txt: Full WebGL debug output"
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
        ./$build_script -matrix-debug
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
        "web"|"hextrail-web")
            build_hextrail "web"
            ;;
        "native"|"hextrail-native")
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
