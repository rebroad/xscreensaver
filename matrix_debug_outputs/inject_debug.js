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
