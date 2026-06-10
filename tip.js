const path = require('node:path');
const os = require('node:os');

function loadLib(lib) {
    const filePath = path.join(os.homedir(), 'langit', 'tip', lib + '.js');
    try {
        // Dynamically require the file path directly
        return require(filePath);
    } catch (error) {
        console.error(`Error loading module "${lib}":`, error.message);
        return null;
    }
}

// Usage:
const echoMain = loadLib('echo');
if (typeof echoMain === 'function') {
    echoMain("Hello World!");
}
