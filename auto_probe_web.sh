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
echo -e "${BLUE}===================================${NC}"
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

    # Use our proven puppeteer method
    echo -e "${CYAN}🚀 Extracting debug output with puppeteer...${NC}"
    if extract_debug_with_puppeteer $PORT; then
        echo -e "${GREEN}✅ Debug output successfully extracted!${NC}"
        return 0
    else
        echo -e "${RED}❌ Failed to extract debug output${NC}"
        echo -e "${YELLOW}💡 Please ensure Node.js and puppeteer are installed${NC}"
        return 1
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
