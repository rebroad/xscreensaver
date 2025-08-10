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

    // Increase timeout to 30 seconds for WebGL initialization
    await page.goto(`http://localhost:${port}`, {
      waitUntil: 'domcontentloaded',
      timeout: 30000
    });

    console.log('✅ Page loaded successfully');

    // Wait for WebGL initialization and matrix debug output
    console.log('⏳ Waiting for WebGL initialization and matrix debug output...');
    await new Promise(resolve => setTimeout(resolve, 5000));

    // Check if debug panel exists
    const debugPanelExists = await page.evaluate(() => {
      const debugPanel = document.getElementById('debug-panel');
      const debugLog = document.getElementById('debug-log');
      return {
        panelExists: !!debugPanel,
        logExists: !!debugLog,
        panelVisible: debugPanel ? debugPanel.style.display !== 'none' : false
      };
    });

    console.log('🔍 Debug panel status:', debugPanelExists);

    // Extract debug panel content
    const debugContent = await page.evaluate(() => {
      const debugLog = document.getElementById('debug-log');
      if (!debugLog) {
        return 'No debug log element found';
      }
      
      const content = debugLog.innerHTML;
      if (!content || content.trim() === '') {
        return 'Debug log is empty - no matrix debug output captured';
      }
      
      return content;
    });

    console.log('📋 Raw debug content length:', debugContent.length);

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
    console.log('\n📁 Full output saved to: matrix_debug_outputs/web_debug_output.txt');

  } catch (error) {
    console.log(`❌ Error: ${error.message}`);
    console.log('💡 This might be due to:');
    console.log('   - WebGL page taking too long to load');
    console.log('   - Matrix debug not being initialized properly');
    console.log('   - Browser compatibility issues');
  }

  await browser.close();
})();
