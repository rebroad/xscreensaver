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

  // Listen for console messages and errors
  page.on('console', msg => {
    console.log('📱 Browser console:', msg.text());
  });

  page.on('pageerror', error => {
    console.log('❌ Browser page error:', error.message);
  });

  page.on('error', error => {
    console.log('❌ Browser error:', error.message);
  });

  try {
    const port = process.argv[2] || '8000';
    console.log(`📍 Loading http://localhost:${port}...`);

    // Increase timeout to 10 seconds for WebGL initialization
    await page.goto(`http://localhost:${port}`, {
      waitUntil: 'domcontentloaded',
      timeout: 10000
    });

    console.log('✅ Page loaded successfully');

    // Check if WebGL is available
    const webglStatus = await page.evaluate(() => {
      const canvas = document.getElementById('canvas');
      if (!canvas) {
        return { error: 'Canvas element not found' };
      }

      const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
      if (!gl) {
        return { error: 'WebGL context not available' };
      }

      return {
        success: true,
        webglVersion: gl.getParameter(gl.VERSION),
        renderer: gl.getParameter(gl.RENDERER)
      };
    });

    console.log('🔍 WebGL status:', webglStatus);

    // Check if the WebAssembly module is loading
    const moduleStatus = await page.evaluate(() => {
      if (typeof Module === 'undefined') {
        return { error: 'Module not defined' };
      }

      if (!Module.onRuntimeInitialized) {
        return { error: 'Module.onRuntimeInitialized not available' };
      }

      return { success: true, moduleExists: true };
    });

    console.log('🔍 Module status:', moduleStatus);

    // Wait for WebGL initialization and matrix debug output
    console.log('⏳ Waiting for WebGL initialization and matrix debug output...');

    // Wait with timeout and check status periodically
    let attempts = 0;
    const maxAttempts = 10;

    while (attempts < maxAttempts) {
      await new Promise(resolve => setTimeout(resolve, 1000));
      attempts++;

      const status = await page.evaluate(() => {
        const debugLog = document.getElementById('debug-log');
        const loadingElement = document.getElementById('loading');

        return {
          debugLogExists: !!debugLog,
          debugLogContent: debugLog ? debugLog.innerHTML.length : 0,
          loadingVisible: loadingElement ? loadingElement.style.display !== 'none' : false,
          moduleInitialized: typeof Module !== 'undefined' && Module.onRuntimeInitialized
        };
      });

      console.log(`⏳ Attempt ${attempts}/${maxAttempts}:`, status);

      if (status.debugLogContent > 0) {
        console.log('✅ Debug output detected!');
        break;
      }
    }

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
