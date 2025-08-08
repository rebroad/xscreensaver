#!/bin/bash

# Automated Web Probing Script for Matrix Debug Output
# This script attempts to automatically extract debug output from the webpage

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

# Function to check if web server is running and start if needed
check_and_start_web_server() {
    echo -e "${YELLOW}🔍 Checking web server...${NC}"
    
    # Check if build_web directory exists
    if [ ! -d "build_web" ]; then
        echo -e "${RED}❌ build_web directory not found${NC}"
        echo -e "${YELLOW}💡 Please build WebGL version first: ./matrix_debug.sh hextrail-web${NC}"
        return 1
    fi
    
    # Check if index.html exists in build_web
    if [ ! -f "build_web/index.html" ]; then
        echo -e "${RED}❌ build_web/index.html not found${NC}"
        echo -e "${YELLOW}💡 Please build WebGL version first: ./matrix_debug.sh hextrail-web${NC}"
        return 1
    fi
    
    # Check if there's already a server running on any port serving our files (quick check)
    echo -e "${YELLOW}🔍 Checking for existing hextrail servers...${NC}"
    for test_port in 8000 8001 8002 8003 8004 8005; do
        if timeout 2 curl -s http://localhost:$test_port > /dev/null 2>&1; then
            # Test if it's serving our hextrail page
            if timeout 2 curl -s http://localhost:$test_port | grep -q "HexTrail"; then
                echo -e "${YELLOW}🔍 Found server on port $test_port, verifying directory...${NC}"
                
                # Use lsof to verify the server is serving from the correct directory
                SERVER_PIDS=$(lsof -ti:$test_port 2>/dev/null)
                if [ ! -z "$SERVER_PIDS" ]; then
                    for pid in $SERVER_PIDS; do
                        SERVER_CWD=$(lsof -p $pid 2>/dev/null | grep cwd | awk '{print $NF}')
                        if [ "$SERVER_CWD" = "$(pwd)/build_web" ]; then
                            echo -e "${GREEN}✅ Web server already running on localhost:$test_port (serving from correct directory)${NC}"
                            PORT=$test_port
                            echo $PORT > /tmp/hextrail_web_port.txt
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
    
    # Clean up any stale python http servers that might be blocking ports
    echo -e "${YELLOW}🧹 Checking for stale web servers...${NC}"
    STALE_PIDS=$(ps aux | grep "python3 -m http.server" | grep -v grep | awk '{print $2}')
    if [ ! -z "$STALE_PIDS" ]; then
        echo -e "${YELLOW}🔍 Found stale web server processes: $STALE_PIDS${NC}"
        for pid in $STALE_PIDS; do
            # Check if this server is actually responding
            if ! timeout 2 curl -s http://localhost:8000 > /dev/null 2>&1; then
                echo -e "${YELLOW}💀 Killing stale server process $pid (not responding)${NC}"
                kill $pid 2>/dev/null
                sleep 1
                # Force kill if still running
                if kill -0 $pid 2>/dev/null; then
                    echo -e "${YELLOW}💀 Force killing stubborn process $pid${NC}"
                    kill -9 $pid 2>/dev/null
                fi
            else
                echo -e "${YELLOW}⚠️  Server process $pid is responding, leaving it alone${NC}"
            fi
        done
    fi
    
    # Find an available port starting from 8000 (quick check)
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
        return 1
    fi
    
    # No server found, start one
    echo -e "${YELLOW}🚀 Starting web server on port $PORT...${NC}"
    
    if command -v python3 &> /dev/null; then
        cd build_web && python3 -m http.server $PORT > /dev/null 2>&1 &
        SERVER_PID=$!
        sleep 2
        
        # Check if server started successfully
        if kill -0 $SERVER_PID 2>/dev/null && curl -s http://localhost:$PORT > /dev/null 2>&1; then
            echo -e "${GREEN}✅ Web server started successfully (PID: $SERVER_PID, Port: $PORT)${NC}"
            echo $PORT > /tmp/hextrail_web_port.txt
            echo $SERVER_PID > /tmp/hextrail_web_pid.txt
            return 0
        else
            echo -e "${RED}❌ Failed to start web server${NC}"
            return 1
        fi
    else
        echo -e "${RED}❌ python3 not found - cannot start web server${NC}"
        return 1
    fi
}

# Function to create a JavaScript injection page
create_injection_page() {
    echo -e "${YELLOW}🔧 Creating JavaScript injection page...${NC}"
    
    cat > matrix_debug_outputs/inject_debug.js << 'EOF'
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

    echo -e "${GREEN}✅ Injection script created: matrix_debug_outputs/inject_debug.js${NC}"
}

# Function to create a curl-based extraction script
create_curl_extractor() {
    echo -e "${YELLOW}🔧 Creating curl-based extraction script...${NC}"
    
    # Get the port from the temp file or default to 8000
    PORT=8000
    if [ -f "/tmp/hextrail_web_port.txt" ]; then
        PORT=$(cat /tmp/hextrail_web_port.txt)
    fi
    
    cat > matrix_debug_outputs/curl_extract.sh << EOF
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

    chmod +x matrix_debug_outputs/curl_extract.sh
    echo -e "${GREEN}✅ Curl extractor created: matrix_debug_outputs/curl_extract.sh${NC}"
}

# Function to create a browser automation script
create_browser_automation() {
    echo -e "${YELLOW}🔧 Creating browser automation script...${NC}"
    
    # Get the port from the temp file or default to 8000
    PORT=8000
    if [ -f "/tmp/hextrail_web_port.txt" ]; then
        PORT=$(cat /tmp/hextrail_web_port.txt)
    fi
    
    cat > matrix_debug_outputs/automate_browser.html << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Browser Automation for Debug Extraction</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .status { padding: 10px; margin: 10px 0; border-radius: 5px; }
        .success { background: #d4edda; color: #155724; }
        .error { background: #f8d7da; color: #721c24; }
        .info { background: #d1ecf1; color: #0c5460; }
        #output { background: #f8f9fa; padding: 15px; border: 1px solid #dee2e6; border-radius: 5px; white-space: pre-wrap; font-family: monospace; }
        button { padding: 10px 20px; margin: 5px; cursor: pointer; }
    </style>
</head>
<body>
    <h1>🤖 Browser Automation for Debug Extraction</h1>
    
    <div id="status" class="status info">Ready to extract debug output...</div>
    
    <div>
        <button onclick="startExtraction()">🚀 Start Extraction</button>
        <button onclick="clearOutput()">🧹 Clear Output</button>
        <button onclick="copyToClipboard()">📋 Copy to Clipboard</button>
    </div>
    
    <div id="output">Click "Start Extraction" to begin...</div>
    
    <script>
        let extractionInterval;
        let debugOutput = '';
        
        function updateStatus(message, type = 'info') {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = `status ${type}`;
        }
        
        function addOutput(text) {
            const output = document.getElementById('output');
            output.textContent += text + '\n';
            output.scrollTop = output.scrollHeight;
        }
        
        function clearOutput() {
            document.getElementById('output').textContent = '';
            debugOutput = '';
        }
        
        function copyToClipboard() {
            const output = document.getElementById('output').textContent;
            navigator.clipboard.writeText(output).then(() => {
                updateStatus('✅ Output copied to clipboard!', 'success');
            }).catch(() => {
                updateStatus('❌ Failed to copy to clipboard', 'error');
            });
        }
        
        function startExtraction() {
            updateStatus('🚀 Starting debug extraction...', 'info');
            addOutput('=== DEBUG EXTRACTION STARTED ===');
            addOutput('Timestamp: ' + new Date().toISOString());
            addOutput('');
            
            // Step 1: Open hextrail page in new window
            addOutput('Step 1: Opening hextrail page...');
            const hextrailWindow = window.open('http://localhost:$PORT', 'hextrail', 'width=1200,height=800');
            
            if (!hextrailWindow) {
                updateStatus('❌ Failed to open hextrail page (popup blocked?)', 'error');
                addOutput('ERROR: Popup blocked or failed to open hextrail page');
                return;
            }
            
            // Step 2: Wait for page to load and inject our script
            setTimeout(() => {
                addOutput('Step 2: Injecting debug extraction script...');
                
                try {
                    // Inject our extraction script
                    const script = hextrailWindow.document.createElement('script');
                    script.textContent = `
                        console.log('🔍 Debug extraction script injected');
                        
                        // Function to extract debug output
                        function extractDebugOutput() {
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
                                
                                // Extract content
                                const content = debugLog.innerHTML;
                                console.log('📊 Debug content extracted:', content);
                                
                                // Send to parent window
                                window.opener.postMessage({
                                    type: 'debug_output',
                                    content: content
                                }, '*');
                                
                                return content;
                            } else {
                                console.log('❌ Debug panel not found');
                                return null;
                            }
                        }
                        
                        // Auto-extract every 2 seconds
                        setInterval(extractDebugOutput, 2000);
                        
                        // Initial extraction
                        setTimeout(extractDebugOutput, 1000);
                    `;
                    
                    hextrailWindow.document.head.appendChild(script);
                    addOutput('✅ Script injected successfully');
                    
                } catch (error) {
                    addOutput('ERROR: Failed to inject script: ' + error.message);
                    updateStatus('❌ Script injection failed', 'error');
                }
                
            }, 3000);
            
            // Step 3: Listen for messages from hextrail page
            window.addEventListener('message', (event) => {
                if (event.data.type === 'debug_output') {
                    const content = event.data.content;
                    if (content && content !== debugOutput) {
                        debugOutput = content;
                        addOutput('📊 New debug output received:');
                        addOutput(content);
                        addOutput('---');
                    }
                }
            });
            
            // Step 4: Auto-stop after 30 seconds
            setTimeout(() => {
                addOutput('⏰ Auto-stopping extraction after 30 seconds...');
                if (hextrailWindow && !hextrailWindow.closed) {
                    hextrailWindow.close();
                }
                updateStatus('✅ Extraction complete! Check output above.', 'success');
            }, 30000);
        }
    </script>
</body>
</html>
EOF

    echo -e "${GREEN}✅ Browser automation page created: matrix_debug_outputs/automate_browser.html${NC}"
}

# Function to cleanup web servers
cleanup_web_servers() {
    echo -e "${YELLOW}🧹 Cleaning up web servers...${NC}"
    
    # Kill any servers we started
    if [ -f "/tmp/hextrail_web_pid.txt" ]; then
        SERVER_PID=$(cat /tmp/hextrail_web_pid.txt)
        if kill -0 $SERVER_PID 2>/dev/null; then
            echo -e "${YELLOW}💀 Killing web server process $SERVER_PID${NC}"
            kill $SERVER_PID 2>/dev/null
            sleep 1
            # Force kill if still running
            if kill -0 $SERVER_PID 2>/dev/null; then
                echo -e "${YELLOW}💀 Force killing stubborn process $SERVER_PID${NC}"
                kill -9 $SERVER_PID 2>/dev/null
            fi
        fi
        rm -f /tmp/hextrail_web_pid.txt
    fi
    
    # Clean up port file
    rm -f /tmp/hextrail_web_port.txt
    
    echo -e "${GREEN}✅ Web server cleanup complete${NC}"
}

# Function to run the automated extraction
run_automated_extraction() {
    echo -e "${YELLOW}🤖 Running automated extraction...${NC}"
    
    # Create output directory
    mkdir -p matrix_debug_outputs
    
    # Create extraction tools
    create_injection_page
    create_curl_extractor
    create_browser_automation
    
    echo ""
    echo -e "${CYAN}📋 Automated extraction tools created:${NC}"
    echo -e "   1. matrix_debug_outputs/inject_debug.js - JavaScript injection script"
    echo -e "   2. matrix_debug_outputs/curl_extract.sh - Curl-based extraction"
    echo -e "   3. matrix_debug_outputs/automate_browser.html - Browser automation page"
    echo ""
    
    # Try curl extraction first
    echo -e "${YELLOW}🔍 Attempting curl-based extraction...${NC}"
    ./matrix_debug_outputs/curl_extract.sh
    
    echo ""
    echo -e "${CYAN}🚀 For full automation, open: matrix_debug_outputs/automate_browser.html${NC}"
    echo -e "${YELLOW}💡 This will open the hextrail page and automatically extract debug output${NC}"
    echo ""
    
    # Check if we can open the automation page
    if command -v xdg-open &> /dev/null; then
        echo -e "${YELLOW}🔧 Opening browser automation page...${NC}"
        xdg-open matrix_debug_outputs/automate_browser.html
    elif command -v open &> /dev/null; then
        echo -e "${YELLOW}🔧 Opening browser automation page...${NC}"
        open matrix_debug_outputs/automate_browser.html
    else
        echo -e "${YELLOW}💡 Please manually open: matrix_debug_outputs/automate_browser.html${NC}"
    fi
}

# Main execution
main() {
    # Check for cleanup command
    if [ "$1" = "cleanup" ]; then
        cleanup_web_servers
        exit 0
    fi
    
    echo -e "${BLUE}🚀 Starting automated web debug output probe...${NC}"
    echo ""
    
    # Check and start web server if needed
    if ! check_and_start_web_server; then
        exit 1
    fi
    
    # Run automated extraction
    run_automated_extraction
    
    echo ""
    echo -e "${GREEN}✅ Automated extraction setup complete!${NC}"
    echo -e "${CYAN}📁 Check matrix_debug_outputs/ for extraction tools and results${NC}"
    echo ""
    echo -e "${YELLOW}💡 To cleanup web servers later, run: $0 cleanup${NC}"
}

# Set up cleanup trap (only if not running cleanup command)
if [ "$1" != "cleanup" ]; then
    trap cleanup_web_servers EXIT
fi

# Run main function
main "$@"
