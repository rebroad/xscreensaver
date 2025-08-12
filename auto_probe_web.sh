#!/bin/bash

# Automated Web Probing Script for Matrix Debug Output
# Usage: ./auto_probe_web.sh <port_number> [output_directory]
# This script extracts debug output from a HexTrail webpage running on the specified port
# If output_directory is specified, debug output will be saved there instead of matrix_debug_outputs/

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${BLUE}🤖 Automated Web Debug Output Probe${NC}"
echo -e "${BLUE}====================================${NC}"
echo ""

# Parse command line arguments
PORT=$1
OUTPUT_DIR=${2:-matrix_debug_outputs}

echo -e "${CYAN}📁 Output directory: $OUTPUT_DIR${NC}"
echo ""

# Function to verify web server on specified port
verify_web_server() {
    local port=$1

    if [ -z "$port" ]; then
        echo -e "${RED}❌ No port specified${NC}"
        echo -e "${YELLOW}💡 Usage: $0 <port_number> [output_directory]${NC}"
        return 1
    fi

    echo -e "${YELLOW}🔍 Verifying HexTrail web server on port $port...${NC}"

    # Check if build_web directory exists
    if [ ! -d "build_web" ]; then
        echo -e "${RED}❌ build_web directory not found${NC}"
        echo -e "${YELLOW}💡 Please build WebGL version first: ./matrix_debug.sh web${NC}"
        return 1
    fi

    # Check if index.html exists in build_web
    if [ ! -f "build_web/index.html" ]; then
        echo -e "${RED}❌ build_web/index.html not found${NC}"
        echo -e "${YELLOW}💡 Please build WebGL version first: ./matrix_debug.sh web${NC}"
        return 1
    fi

    # Verify server is running and serving HexTrail
    if timeout 2 curl -s http://localhost:$port > /dev/null 2>&1; then
        if timeout 2 curl -s http://localhost:$port | grep -q "HexTrail"; then
            echo -e "${GREEN}✅ Verified HexTrail server on localhost:$port${NC}"
            echo $port > /tmp/hextrail_web_port.txt
            return 0
        else
            echo -e "${RED}❌ Server on port $port is not serving HexTrail content${NC}"
            return 1
        fi
    else
        echo -e "${RED}❌ No server responding on port $port${NC}"
        return 1
    fi
}

# Function to create a JavaScript injection page
create_injection_page() {
    echo -e "${YELLOW}🔧 Creating JavaScript injection page...${NC}"

    mkdir -p "$OUTPUT_DIR"
    cat > "$OUTPUT_DIR/inject_debug.js" << 'EOF'
// JavaScript injection to extract debug output
(function() {
    console.log('🔍 Debug extraction script injected');

    // Wait for page to load
    setTimeout(function() {
        console.log('⏳ Page loaded, looking for debug elements...');

        // Try to find the debug panel
        const debugPanel = document.getElementById('debug-panel');
        const debugLog = document.getElementById('debug-log');
        const debugToggle = document.getElementById('debug-toggle');

        if (debugPanel && debugLog) {
            console.log('✅ Debug panel found');

            // Show debug panel if hidden
            if (debugPanel.style.display === 'none') {
                debugToggle.click();
                console.log('🔘 Debug panel opened');
            }

            // Wait for debug output to accumulate
            setTimeout(function() {
                const debugContent = debugLog.innerHTML;
                console.log('📊 Debug content extracted:');
                console.log('---START DEBUG OUTPUT---');
                console.log(debugContent);
                console.log('---END DEBUG OUTPUT---');

                // Also log to a global variable for external access
                window.extractedDebugOutput = debugContent;

            }, 5000); // Wait 5 seconds for output

        } else {
            console.log('❌ Debug panel not found');
            console.log('Available elements:', document.querySelectorAll('[id*="debug"]'));
        }

    }, 2000); // Wait 2 seconds for page load

})();
EOF

    echo -e "${GREEN}✅ Injection script created: $OUTPUT_DIR/inject_debug.js${NC}"
}

# Function to create a curl-based extraction script
create_curl_extractor() {
    echo -e "${YELLOW}🔧 Creating curl-based extraction script...${NC}"

    # Get the port from the temp file or default to 8000
    PORT=8000
    if [ -f "/tmp/hextrail_web_port.txt" ]; then
        PORT=$(cat /tmp/hextrail_web_port.txt)
    fi

    cat > "$OUTPUT_DIR/curl_extract.sh" << EOF
#!/bin/bash

# Curl-based debug output extraction
echo "🔍 Attempting to extract debug output via curl..."

# Get the port from temp file or default
PORT=$PORT
if [ -f "/tmp/hextrail_web_port.txt" ]; then
    PORT=\$(cat /tmp/hextrail_web_port.txt)
fi

echo "🌐 Using port: \$PORT"

# First, get the main page
curl -s http://localhost:\$PORT > /tmp/hextrail_page.html

# Look for debug-related content
echo "📊 Searching for debug content in page..."
grep -i "debug\|matrix\|glMatrixMode\|glLoadIdentity" /tmp/hextrail_page.html | head -20

# Try to extract any console.log statements
echo ""
echo "📋 Console log statements found:"
grep -o 'console\.log([^)]*)' /tmp/hextrail_page.html | head -10

# Look for any JavaScript variables containing debug info
echo ""
echo "🔍 JavaScript debug variables:"
grep -o 'debug[^;]*' /tmp/hextrail_page.html | head -10

# Clean up
rm -f /tmp/hextrail_page.html
EOF

    chmod +x "$OUTPUT_DIR/curl_extract.sh"
    echo -e "${GREEN}✅ Curl extractor created: $OUTPUT_DIR/curl_extract.sh${NC}"
}

# Function to extract debug output using puppeteer (our proven method)
extract_debug_with_puppeteer() {
    local port=$1
    echo -e "${YELLOW}🤖 Extracting debug output using headless browser...${NC}"

    # Check if Node.js and puppeteer are available
    if ! command -v node &> /dev/null; then
        echo -e "${RED}❌ Node.js not found. Falling back to manual extraction tools.${NC}"
        return 1
    fi

    if ! node -e "require('puppeteer')" &> /dev/null; then
        echo -e "${YELLOW}⚠️ Puppeteer not installed. Installing...${NC}"
        npm install puppeteer &> /dev/null
        if [ $? -ne 0 ]; then
            echo -e "${RED}❌ Failed to install puppeteer. Falling back to manual extraction tools.${NC}"
            return 1
        fi
    fi

    # Create extraction script
    cat > "$OUTPUT_DIR/extract_debug.js" << 'EOF'
const puppeteer = require('puppeteer');

(async () => {
  console.log('🔍 Extracting HexTrail matrix debug output...');

  const browser = await puppeteer.launch({
    headless: true,
    args: [
      '--no-sandbox',
      '--disable-setuid-sandbox',
      '--disable-dev-shm-usage',
      '--disable-gpu',
      '--enable-unsafe-swiftshader'
    ]
  });

  const page = await browser.newPage();

  try {
    const port = process.argv[2] || '8000';
    console.log(`📍 Loading http://localhost:${port}...`);

    await page.goto(`http://localhost:${port}`, {
      waitUntil: 'networkidle0',
      timeout: 10000
    });

    // Wait for initialization
    await new Promise(resolve => setTimeout(resolve, 3000));

    // Extract debug panel content
    const debugContent = await page.evaluate(() => {
      const debugLog = document.getElementById('debug-log');
      return debugLog ? debugLog.innerHTML : 'No debug output found';
    });

    // Clean up HTML and preserve formatting
    const cleanContent = debugContent
      .replace(/<br\s*\/?>/gi, '\n')  // Convert <br> to newlines FIRST
      .replace(/<[^>]*>/g, '')  // Then remove other HTML tags
      .replace(/&gt;/g, '>')   // Decode HTML entities
      .replace(/&lt;/g, '<')
      .replace(/&amp;/g, '&')
      .replace(/\[[\d:\.]+\] /g, '') // Remove timestamps
      .split('\n')
      .filter(line => line.trim())  // Remove empty lines
      .slice(0, 100);  // Limit to first 100 lines

    console.log('✅ Debug output extracted successfully');
    console.log('📋 Sample output (first 20 lines):');
    console.log('=====================================');
    cleanContent.slice(0, 20).forEach(line => console.log(line));
    if (cleanContent.length > 20) {
      console.log(`... and ${cleanContent.length - 20} more lines`);
    }

    // Save to file
    require('fs').writeFileSync('web_debug_output.txt', cleanContent.join('\n'));
    console.log('\n📁 Full output saved to: web_debug_output.txt');

  } catch (error) {
    console.log(`❌ Error: ${error.message}`);
  }

  await browser.close();
})();
EOF

    # Run the extraction
    cd "$OUTPUT_DIR"
    node extract_debug.js $port
    node_exit_code=$?
    cd ..

    if [ $node_exit_code -eq 0 ] && [ -f "$OUTPUT_DIR/web_debug_output.txt" ]; then
        echo -e "${GREEN}✅ Web debug output captured successfully!${NC}"
        echo -e "${CYAN}📁 Output saved to: $OUTPUT_DIR/web_debug_output.txt${NC}"
        return 0
    else
        echo -e "${RED}❌ Failed to capture debug output (Node.js exit code: $node_exit_code)${NC}"
        if [ -f "$OUTPUT_DIR/web_debug_output.txt" ]; then
            echo -e "${YELLOW}⚠️ Debug file exists but may contain incomplete data due to timeout${NC}"
        fi
        return 1
    fi
}

# Function to run the automated extraction
run_automated_extraction() {
    echo -e "${YELLOW}🤖 Running automated extraction...${NC}"

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    # Clean up old WebGL debug output files to avoid confusion
    echo -e "${YELLOW}🧹 Cleaning up old WebGL debug output files...${NC}"
    rm -f "$OUTPUT_DIR/web_debug_output.txt"
    rm -f "$OUTPUT_DIR/webgl_matrix_ops.txt"

    # Get the port from the server
    PORT=$(cat /tmp/hextrail_web_port.txt 2>/dev/null || echo "8000")

    # Try our proven puppeteer method first
    echo -e "${CYAN}🚀 Attempting advanced extraction with puppeteer...${NC}"
    if extract_debug_with_puppeteer $PORT; then
        # Success! No need for fallback tools
        echo -e "${GREEN}✅ Debug output successfully extracted using proven method!${NC}"
        return 0
    fi

    # Fallback to manual tools if puppeteer fails
    echo -e "${YELLOW}⚠️ Puppeteer extraction failed, creating manual extraction tools...${NC}"

    # Create extraction tools
    create_injection_page
    create_curl_extractor

    echo ""
    echo -e "${CYAN}📋 Automated extraction tools created:${NC}"
    echo -e "   1. $OUTPUT_DIR/inject_debug.js - JavaScript injection script"
    echo -e "   2. $OUTPUT_DIR/curl_extract.sh - Curl-based extraction"
    echo ""

    # Try curl extraction first
    echo -e "${YELLOW}🔍 Attempting curl-based extraction...${NC}"
    ./"$OUTPUT_DIR/curl_extract.sh"

    # If we still don't have web_debug_output.txt, warn clearly
    if [ ! -f "$OUTPUT_DIR/web_debug_output.txt" ]; then
        echo -e "${YELLOW}⚠️  Web debug output not captured yet. You can:
 - Ensure Node.js is installed for headless extraction
 - Check the page at http://localhost:$PORT and the presence of #debug-log${NC}"
    fi

    echo ""
    echo -e "${CYAN}🚀 Opening hextrail page directly to verify it works...${NC}"
    echo -e "${YELLOW}💡 This will open the actual hextrail page in your browser${NC}"
    echo ""

    # Open the hextrail page directly first
    if command -v xdg-open &> /dev/null; then
        echo -e "${YELLOW}🔧 Opening hextrail page directly...${NC}"
        xdg-open "http://localhost:$PORT"
    elif command -v open &> /dev/null; then
        echo -e "${YELLOW}🔧 Opening hextrail page directly...${NC}"
        open "http://localhost:$PORT"
    else
        echo -e "${YELLOW}💡 Please manually open: http://localhost:$PORT${NC}"
    fi


}

# Main execution
main() {
    echo -e "${BLUE}🚀 Starting automated web debug output probe...${NC}"
    echo ""

    # Verify web server on specified port
    if ! verify_web_server "$PORT"; then
        exit 1
    fi

    # Run automated extraction
    run_automated_extraction

    echo ""
    echo -e "${GREEN}✅ Automated extraction setup complete!${NC}"
    echo -e "${CYAN}📁 Check $OUTPUT_DIR/ for extraction tools and results${NC}"
    echo ""
    echo -e "${YELLOW}💡 Web server management is now handled by build_web.sh${NC}"
}

# Run main function
main
