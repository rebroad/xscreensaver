#!/bin/bash

# Script to capture debug output from the WebGL hextrail page
# This script will extract debug output from the webpage's debug panel

echo "🌐 WebGL Matrix Debug Output Capture"
echo "====================================="
echo ""

# Check if server is running
if ! curl -s http://localhost:8000 > /dev/null 2>&1; then
    echo "❌ Error: Web server not running on localhost:8000"
    echo "   Please start the server with: cd build_web && python3 -m http.server 8000"
    exit 1
fi

echo "✅ Web server is running on localhost:8000"
echo ""

# Function to extract debug output from the page
extract_debug_output() {
    echo "📊 Capturing debug output from webpage..."
    echo ""
    
    # Use curl to get the page and extract debug log content
    # Note: This is a simplified approach - in practice, you'd need to interact with the page
    # to trigger the WebAssembly module and capture its output
    
    echo "🔍 To capture debug output:"
    echo "1. Open http://localhost:8000 in your browser"
    echo "2. Click the 'Debug' button in the top-right corner"
    echo "3. The debug panel will show matrix debug output from the WebAssembly module"
    echo "4. Look for lines starting with [timestamp] in teal (stdout) or red (stderr)"
    echo ""
    echo "📋 Expected matrix debug output should include:"
    echo "   - glMatrixMode() calls"
    echo "   - glLoadIdentity() calls" 
    echo "   - gluPerspective() calls"
    echo "   - gluLookAt() calls"
    echo "   - glTranslatef() calls"
    echo "   - glRotatef() calls"
    echo "   - glPushMatrix() / glPopMatrix() calls"
    echo ""
    echo "🎯 The debug panel captures both stdout (teal) and stderr (red) from our matrix_debug.c"
    echo ""
}

# Function to compare with native output
compare_outputs() {
    echo "🔄 To compare with native output:"
    echo "1. Run: ./matrix_debug.sh hextrail-native"
    echo "2. Run: cd build_native_debug && timeout 10s ./hextrail_debug 2>&1 | head -50"
    echo "3. Compare the matrix operations between native and WebGL versions"
    echo ""
}

# Main execution
extract_debug_output
compare_outputs

echo "🚀 Ready to test! Open http://localhost:8000 in your browser"
echo "   and click the 'Debug' button to see matrix debug output."
