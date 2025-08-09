# 🔧 Matrix Debugging Framework Documentation

## 📋 Overview

The Matrix Debugging Framework is a comprehensive system for debugging and comparing OpenGL matrix operations between native (OpenGL) and web (WebGL) builds of the HexTrail screensaver. This framework ensures mathematical consistency across platforms.

## 🏗️ Architecture Overview

```
matrix_debug.sh (Main Orchestrator)
├── build_web.sh (WebGL Build Script)
├── enhanced_compare.sh (Advanced Comparison)
├── auto_probe_web.sh (Web Debugging Automation)
└── Core Debug Files
    ├── matrix_debug.c (Debug Implementation)
    ├── matrix_debug.h (Debug Headers)
    └── xscreensaver_web.c (WebGL Compatibility Layer)
```

## 📁 Script Relationships & Dependencies

### 1. **matrix_debug.sh** - Main Orchestrator 🎯
**Purpose**: User-friendly interface for all matrix debugging operations

**Commands**:
- `compare` - Builds both versions and uses other scripts for comparison
- `hextrail-web` - Builds WebGL version with debugging
- `hextrail-native` - Builds native version with debugging
- `clean` - Cleans all build files
- `help` - Shows usage information

**Dependencies**:
- ✅ **build_web.sh** - Called for WebGL builds
- ✅ **auto_probe_web.sh** - Called for advanced web debugging (required)
- ✅ **matrix_debug.c/h** - Core debugging implementation
- ✅ **hextrail.c** - Modified with conditional debugging code

**Key Features**:
- Dependency checking (gcc, emcc, required files)
- Automatic Emscripten environment sourcing
- Makefile modification for native builds
- **Script orchestration** - Uses other scripts (required for full functionality)

### 2. **build_web.sh** - WebGL Build Script 🌐
**Purpose**: Compiles HexTrail for WebGL using Emscripten

**Dependencies**:
- ✅ **matrix_debug.c** - Included in WebGL build
- ✅ **xscreensaver_web.c** - WebGL compatibility layer
- ✅ **hextrail_web_main.c** - WebGL entry point
- ✅ **web/template.html** - HTML template

**Key Features**:
- Emscripten compilation with WebGL2 support
- Matrix debugging enabled via `-DMATRIX_DEBUG`
- Memory debugging options (`-memory` flag)
- GL error hunting mode (`-debug` flag)
- Custom HTML template integration

**Build Process**:
```bash
emcc [flags] hextrail_web_main.c matrix_debug.c xscreensaver_web.c [other files] -o index.html
```

### 4. **auto_probe_web.sh** - Web Debug Output Extraction 🤖
**Purpose**: Automated debug output capture and extraction tools

**Dependencies**:
- ✅ **curl** - For web page probing
- ✅ **build_web.sh** - For web server management (must be run first)

**Key Features**:
- **Debug Output Extraction**:
  - Curl-based content extraction
  - JavaScript injection capabilities
  - Browser automation tools
  - Multiple extraction methods

- **Tool Generation**:
  - Creates extraction scripts
  - Generates browser automation pages
  - Provides manual extraction options

**Commands**:
- `./auto_probe_web.sh` - Run debug output extraction (requires existing web server)

## 🔄 Data Flow & Interactions

### Build Process Flow:
```
matrix_debug.sh hextrail-web
    ↓
build_web.sh
    ↓
emcc compilation with matrix_debug.c
    ↓
build_web/index.html (WebGL version)
```

```
matrix_debug.sh hextrail-native
    ↓
Makefile modification (injects matrix_debug.o)
    ↓
gcc compilation with matrix_debug.c
    ↓
build_native_debug/hextrail_debug (Native version)
```

### Comparison Process Flow:
```
matrix_debug.sh compare
    ↓
[Build both versions]
    ↓
[Run native hextrail and capture debug output]
    ↓
auto_probe_web.sh (required)
    ↓
[Intelligent matrix operation comparison (integrated)]
    ↓
[Generate comparison report]
```

### Web Debugging Flow:
```
auto_probe_web.sh
    ↓
[Check for existing servers]
    ↓
[Start web server if needed]
    ↓
[Create extraction tools]
    ↓
[Probe webpage with curl]
    ↓
[Generate browser automation]
    ↓
[Clean up on exit]
```

## 🛠️ Core Debug Files

### **matrix_debug.c** - Debug Implementation
**Purpose**: Intercepts and logs OpenGL/GLU matrix operations

**Key Features**:
- Function pointer wrapping for OpenGL calls
- Platform-specific implementation (native vs WebGL)
- Frame limiting for readable output
- Matrix state tracking
- Debug logging with timestamps

**Wrapped Functions**:
- `glMatrixMode`, `glLoadIdentity`
- `glTranslatef`, `glRotatef`, `glScalef`
- `glPushMatrix`, `glPopMatrix`
- `glMultMatrixf`, `glViewport`
- `gluPerspective`, `gluLookAt`
- `glMultMatrixd`, `glTranslated`

### **matrix_debug.h** - Debug Headers
**Purpose**: Declares debug functions and provides macro redirections

**Key Features**:
- Conditional compilation for native vs WebGL
- Macro redirections for OpenGL calls
- Platform-specific declarations
- Debug state management

### **xscreensaver_web.c** - WebGL Compatibility Layer
**Purpose**: Provides WebGL equivalents for OpenGL functions

**Key Features**:
- WebGL 2.0 API mapping
- Type conversion (GLdouble → GLfloat)
- Matrix stack management
- Emscripten integration

## 🔧 Architectural Improvements

### **Fixed Issues:**

1. **✅ Eliminated Duplication:**
   - Web server management now centralized in `auto_probe_web.sh`
   - WebGL output capture logic unified
   - Build logic consolidated

2. **✅ Corrected Dependencies:**
   - `matrix_debug.sh` now properly calls other scripts
   - `enhanced_compare.sh` no longer depends on `matrix_debug.sh`
   - Each script can operate independently

3. **✅ Improved Modularity:**
   - Scripts can be used individually or together
   - Graceful fallbacks when scripts are missing
   - Clear separation of concerns

### **New Architecture:**
```
matrix_debug.sh (Orchestrator)
├── build_web.sh (WebGL Build)
├── auto_probe_web.sh (Web Debugging)
└── [Intelligent comparison logic (integrated)]

Each script can also run independently!
```

## 🎯 Usage Examples

### Basic Comparison:
```bash
# Compare native vs WebGL matrix operations (uses all available scripts)
./matrix_debug.sh compare
```

### Individual Builds:
```bash
# Build WebGL version with debugging
./matrix_debug.sh hextrail-web

# Build native version with debugging
./matrix_debug.sh hextrail-native
```

### Advanced Web Debugging:
```bash
# Build WebGL version with auto-start server
./build_web.sh -matrix-debug

# Extract debug output (after server is running)
./auto_probe_web.sh
```

### Direct Script Usage:
```bash
# Build WebGL with memory debugging
./build_web.sh -memory

# Build WebGL without server (manual server start)
./build_web.sh -matrix-debug -noserver
```

## 🔧 Configuration & Customization

### Environment Variables:
- `WEBGL_AVAILABLE` - Controls WebGL build availability
- `PYTHON_AVAILABLE` - Controls web server functionality
- `DEBUG_MODE` - Enables additional debug output

### Build Flags:
- `-DMATRIX_DEBUG` - Enables matrix debugging
- `-DWEB_BUILD` - Indicates WebGL build
- `-DFINDBUG_MODE` - Enables GL error hunting
- `-g` - Enables debug symbols

### Output Directories:
- `build_web/` - WebGL build output
- `build_native_debug/` - Native build output
- `matrix_debug_outputs/` - Comparison results

## 🚨 Troubleshooting

### Common Issues:

1. **"emcc not found"**
   - Solution: Install Emscripten or source emsdk environment
   - Check: `source ~/src/emsdk/emsdk_env.sh`

2. **"Web server already running"**
   - Solution: Use `./auto_probe_web.sh cleanup`
   - Check: `lsof -i :8000` for port conflicts

3. **"Matrix operations differ"**
   - Check: Platform-specific floating-point precision
   - Verify: Both versions use same matrix stack operations

4. **"Cannot access iframe content"**
   - Cause: Browser CORS restrictions
   - Solution: Use manual debug panel capture

### Debug Output Locations:
- Native: `matrix_debug_outputs/native_output.txt`
- WebGL: `matrix_debug_outputs/webgl_output.txt`
- Differences: `matrix_debug_outputs/matrix_diff.txt`

## 🎉 Success Criteria

The framework is working correctly when:
- ✅ Both native and WebGL builds compile successfully
- ✅ Matrix operations are logged consistently
- ✅ Web server starts and serves files correctly
- ✅ Debug output can be captured from both platforms
- ✅ Matrix operations match between platforms
- ✅ All cleanup operations work properly

## 🔮 Future Enhancements

Potential improvements:
- Automated browser testing with Selenium
- Real-time matrix operation streaming
- Visual matrix state visualization
- Performance profiling integration
- Cross-platform matrix validation
- Automated regression testing

---

**Last Updated**: $(date)
**Framework Version**: 1.0
**Compatibility**: Linux, WebGL 2.0, OpenGL 2.1+
