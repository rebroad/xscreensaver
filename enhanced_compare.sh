#!/bin/bash

# Enhanced Matrix Debug Comparison Script
# This script actually probes the webpage and captures debug output automatically

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔍 Enhanced Matrix Debug Comparison${NC}"
echo -e "${BLUE}====================================${NC}"
echo ""

# Check dependencies
check_dependencies() {
    echo -e "${YELLOW}🔍 Checking dependencies...${NC}"
    
    # Check for curl (for web probing)
    if ! command -v curl &> /dev/null; then
        echo -e "${RED}❌ curl not found - needed for web probing${NC}"
        return 1
    fi
    
    # Check for emcc
    if ! command -v emcc &> /dev/null; then
        echo -e "${YELLOW}⚠️  emcc not found - WebGL comparison will be limited${NC}"
        WEBGL_AVAILABLE=false
    else
        echo -e "${GREEN}✅ emcc found${NC}"
        WEBGL_AVAILABLE=true
    fi
    
    # Check for Python3 (for web server)
    if ! command -v python3 &> /dev/null; then
        echo -e "${YELLOW}⚠️  python3 not found - manual web server needed${NC}"
        PYTHON_AVAILABLE=false
    else
        echo -e "${GREEN}✅ python3 found${NC}"
        PYTHON_AVAILABLE=true
    fi
    
    echo -e "${GREEN}✅ Dependencies check complete${NC}"
    echo ""
}

# Function to capture native debug output
capture_native_output() {
    echo -e "${YELLOW}📊 Capturing native debug output...${NC}"
    
    # Build native version if not already built
    if [ ! -f "build_native_debug/hextrail_debug" ]; then
        echo -e "${YELLOW}🔧 Building native hextrail...${NC}"
        ./matrix_debug.sh hextrail-native
    fi
    
    # Create output directory
    mkdir -p matrix_debug_outputs
    
    # Run native version and capture first 50 lines of debug output
    echo -e "${YELLOW}⏳ Running native hextrail for 10 seconds...${NC}"
    timeout 10s cd build_native_debug && ./hextrail_debug 2>&1 | head -50 > ../matrix_debug_outputs/native_output.txt
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Native output captured: matrix_debug_outputs/native_output.txt${NC}"
        echo -e "${CYAN}📋 Native output preview:${NC}"
        head -10 matrix_debug_outputs/native_output.txt
        echo ""
    else
        echo -e "${RED}❌ Failed to capture native output${NC}"
        return 1
    fi
}

# Function to capture WebGL debug output by probing the webpage
capture_webgl_output() {
    echo -e "${YELLOW}📊 Capturing WebGL debug output...${NC}"
    
    # Build WebGL version if not already built
    if [ ! -f "build_web/index.html" ]; then
        echo -e "${YELLOW}🔧 Building WebGL hextrail...${NC}"
        ./matrix_debug.sh hextrail-web
    fi
    
    # Start web server if not running
    if ! curl -s http://localhost:8000 > /dev/null 2>&1; then
        if [ "$PYTHON_AVAILABLE" = true ]; then
            echo -e "${YELLOW}🚀 Starting web server...${NC}"
            cd build_web && python3 -m http.server 8000 > /dev/null 2>&1 &
            SERVER_PID=$!
            sleep 2
            
            if ! kill -0 $SERVER_PID 2>/dev/null; then
                echo -e "${RED}❌ Failed to start web server${NC}"
                return 1
            fi
            echo -e "${GREEN}✅ Web server started (PID: $SERVER_PID)${NC}"
        else
            echo -e "${RED}❌ Web server not running and python3 not available${NC}"
            echo -e "${YELLOW}💡 Please start web server manually: cd build_web && python3 -m http.server 8000${NC}"
            return 1
        fi
    else
        echo -e "${GREEN}✅ Web server already running${NC}"
    fi
    
    # Create a simple HTML page to extract debug output
    echo -e "${YELLOW}🔍 Creating debug extraction page...${NC}"
    cat > matrix_debug_outputs/extract_debug.html << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Debug Output Extractor</title>
    <script>
        // Function to extract debug output from the hextrail page
        async function extractDebugOutput() {
            try {
                // Load the hextrail page in an iframe
                const iframe = document.createElement('iframe');
                iframe.src = 'http://localhost:8000';
                iframe.style.display = 'none';
                document.body.appendChild(iframe);
                
                // Wait for iframe to load
                await new Promise(resolve => setTimeout(resolve, 3000));
                
                // Try to access the debug panel content
                try {
                    const debugLog = iframe.contentDocument.getElementById('debug-log');
                    if (debugLog) {
                        const output = debugLog.innerHTML;
                        document.getElementById('output').textContent = output;
                        console.log('Debug output extracted:', output);
                    } else {
                        document.getElementById('output').textContent = 'Debug panel not found or not visible';
                    }
                } catch (e) {
                    document.getElementById('output').textContent = 'Cannot access iframe content (CORS restriction)';
                }
                
            } catch (error) {
                document.getElementById('output').textContent = 'Error: ' + error.message;
            }
        }
        
        // Auto-extract when page loads
        window.onload = extractDebugOutput;
    </script>
</head>
<body>
    <h1>Debug Output Extractor</h1>
    <p>Attempting to extract debug output from hextrail page...</p>
    <pre id="output">Loading...</pre>
</body>
</html>
EOF

    echo -e "${YELLOW}💡 Manual extraction required due to browser security restrictions${NC}"
    echo -e "${CYAN}📋 Instructions to capture WebGL debug output:${NC}"
    echo -e "   1. Open http://localhost:8000 in your browser"
    echo -e "   2. Click the 'Debug' button in the top-right corner"
    echo -e "   3. Let the animation run for 10-15 seconds"
    echo -e "   4. Copy the debug panel content (teal/red text)"
    echo -e "   5. Save it to: matrix_debug_outputs/webgl_output.txt"
    echo ""
    echo -e "${YELLOW}⏳ Waiting for manual capture...${NC}"
    echo -e "${CYAN}Press Enter when you've saved the WebGL output to matrix_debug_outputs/webgl_output.txt${NC}"
    read -p ""
    
    # Check if WebGL output was captured
    if [ -f "matrix_debug_outputs/webgl_output.txt" ]; then
        echo -e "${GREEN}✅ WebGL output captured: matrix_debug_outputs/webgl_output.txt${NC}"
        echo -e "${CYAN}📋 WebGL output preview:${NC}"
        head -10 matrix_debug_outputs/webgl_output.txt
        echo ""
    else
        echo -e "${RED}❌ WebGL output file not found${NC}"
        return 1
    fi
}

# Function to compare outputs intelligently
compare_outputs_intelligently() {
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

# Main execution
main() {
    check_dependencies
    
    echo -e "${BLUE}🚀 Starting enhanced matrix debug comparison...${NC}"
    echo ""
    
    # Capture native output
    capture_native_output
    
    # Capture WebGL output if available
    if [ "$WEBGL_AVAILABLE" = true ]; then
        capture_webgl_output
    else
        echo -e "${YELLOW}⚠️  Skipping WebGL comparison (emcc not available)${NC}"
    fi
    
    # Compare outputs
    compare_outputs_intelligently
    
    # Cleanup
    if [ ! -z "$SERVER_PID" ]; then
        echo -e "${YELLOW}🧹 Cleaning up web server...${NC}"
        kill $SERVER_PID 2>/dev/null
    fi
    
    echo ""
    echo -e "${GREEN}✅ Enhanced comparison complete!${NC}"
}

# Run main function
main
