#!/bin/bash

# Curl-based debug output extraction
echo "🔍 Attempting to extract debug output via curl..."

# Get the port from temp file or default
PORT=8000
if [ -f "/tmp/hextrail_web_port.txt" ]; then
    PORT=$(cat /tmp/hextrail_web_port.txt)
fi

echo "🌐 Using port: $PORT"

# First, get the main page
curl -s http://localhost:$PORT > /tmp/hextrail_page.html

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
