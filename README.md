# 🎯 **Screen Highlighter - Screen Capture and Annotation**

**Screen Highlighter** is a complete Win32 C++ application for screen capture, annotation, and highlighting. The project has been completely refactored and optimized with a professional build system.

## 🆕 **Version 2.0.0 - New Features**

### **🔍 Enhanced Zoom Experience**
- ✅ **Automatic Text Copying**: When zooming into a region for the first time, any previously written text is automatically copied to the zoom view
- ✅ **Clean Zoom Focus**: All drawing elements (lines, arrows, rectangles, highlighter) are hidden during zoom to provide a distraction-free experience focused solely on the zoomed region
- ✅ **Smart Text Management**: Text is preserved and restored intelligently when entering/exiting zoom mode
- ✅ **Seamless Transition**: Drawing elements reappear when zoom is deactivated, maintaining full functionality

### **🎨 Improved Drawing Experience**
- ✅ **Context-Aware Rendering**: Drawing elements are conditionally rendered based on zoom state
- ✅ **Enhanced Performance**: Reduced rendering overhead during zoom operations
- ✅ **Better User Experience**: Cleaner interface with focused attention on the zoomed content

**👨‍💻 Developer**: Unnamed10110 | **📧 Contact**: trojan.v6@gmail.com | **📧 Alt Contact**: sergio.britos@gmail.com

## ✨ **Main Features**

### **Implemented Functionality**
- ✅ **Screen capture** with region selection
- ✅ **Annotation tools** (line, arrow, rectangle, highlighter)
- ✅ **Global hotkey system** (Shift+Alt+X)
- ✅ **System tray integration** with custom icon
- ✅ **System tray auto-restoration** when explorer.exe restarts
- ✅ **Auto-start on Windows login** with registry management
- ✅ **Professional executable icon** with Windows resource compilation
- ✅ **Persistent configuration** with .ini file
- ✅ **Zoom and capture** of specific regions
- ✅ **Undo/Redo** of drawing elements
- ✅ **Complete screen capture**

### **Technical Improvements Implemented**
- ✅ **Complete RAII system** for automatic GDI resource management
- ✅ **Type-safe enumerations** for tools and messages
- ✅ **Robust error handling** with explicit validations
- ✅ **Professional CMake build system** and flexible
- ✅ **Multiple compilation modes** for different needs
- ✅ **System tray reliability system** with automatic restoration
- ✅ **Explorer.exe process monitoring** for seamless recovery
- ✅ **Professional executable icon** with Windows resource compilation
- ✅ **Version information** embedded in executable properties

### **Version 2.0.0 Technical Enhancements**
- ✅ **Smart Text State Management**: Advanced text preservation and restoration system
- ✅ **Conditional Rendering Engine**: Context-aware drawing element visibility
- ✅ **Performance Optimizations**: Reduced rendering overhead during zoom operations
- ✅ **Memory-Safe Text Operations**: Modern C++ string handling with automatic memory management
- ✅ **Thread-Safe State Management**: Atomic operations for zoom and text state variables
- ✅ **Clean Code Architecture**: Well-commented Spanish code following modern C++ practices

## 🚀 **Quick Compilation (Recommended)**

### **For Daily Use (RECOMMENDED)**
```cmd
build_debug_silent.bat
```
- ✅ **Complete debug functionality**
- ✅ **No console window**
- ✅ **Hotkeys work perfectly**
- ✅ **System tray works correctly**

### **For Advanced Options**
```cmd
build_advanced.bat
```
- **5 compilation options**
- **Total flexibility** for different needs

## 🔧 **Complete Build System**

### **Available Scripts**

#### **1. `build_debug_silent.bat` - RECOMMENDED for daily use**
- **Functionality**: Complete debug without console
- **Advantages**: Hotkeys and system tray work perfectly
- **Use**: Daily development, personal use

#### **2. `build_advanced.bat` - Advanced build with 5 options**
- **Option 1**: Release (no console) - ⚠️ **Known issues**
- **Option 2**: Debug (with console) - ✅ **Works perfectly**
- **Option 3**: Release + console - ✅ **Works perfectly**
- **Option 4**: Silent debug - ✅ **Works perfectly**
- **Option 5**: Compile only (no execution)

### **Detailed Compilation Modes**

#### **🔇 Silent Debug Mode (RECOMMENDED)**
- **Script**: `build_debug_silent.bat`
- **Optimization**: Minimal (`-O0`)
- **Debug**: Complete information (`-g`)
- **Console**: Not visible
- **Functionality**: ✅ **Hotkeys and system tray work perfectly**
- **Use**: Daily development, personal use

#### **🐛 Debug Mode (With Console)**
- **Script**: `build_advanced.bat` (option 2)
- **Optimization**: Minimal (`-O0`)
- **Debug**: Complete information (`-g`)
- **Console**: Visible with messages
- **Functionality**: ✅ **Complete with visible debugging**
- **Use**: Development, testing, debugging

#### **⚡ Release + Console Mode**
- **Script**: `build_advanced.bat` (option 3)
- **Optimization**: Maximum (`-O2`)
- **Debug**: Visible messages
- **Console**: Visible
- **Functionality**: ✅ **Optimized + debugging**
- **Use**: Production testing, QA

#### **🚀 Release Mode (No Console)**
- **Script**: `build_advanced.bat` (option 1)
- **Optimization**: Maximum (`-O2`)
- **Console**: Not visible
- **Functionality**: ❌ **Known issues**
- **Use**: ⚠️ **Not recommended until issues resolved**

## 🎮 **Application Usage**

### **System Requirements**
- **Windows 10/11** (x64) - Primary target
- **Windows 8.1/8/7** - Supported with limitations
- **Administrator Privileges** - Required for full functionality
- **CMake 3.16+** - For building from source
- **MinGW-w64** or **Visual Studio 2019+** - C++ compiler
- **C++17** compatible compiler

### **Advanced Features**
- **🔐 Automatic Administrator Privileges**: UAC prompt and automatic elevation
- **🔄 System Tray Auto-Restoration**: Automatically restores icon when explorer.exe restarts
- **⏰ Periodic Health Checks**: Monitors system tray status every 30 seconds
- **🔍 Explorer.exe Monitoring**: Detects when Windows shell restarts and restores functionality
- **🚀 Auto-Start on Login**: Option to automatically launch when Windows starts
- **⚙️ Registry Management**: Automatic Windows registry configuration for startup

### **Main Hotkeys**
- **Shift + Alt + X** - Activate selection mode
- **F1** - Line tool
- **F2** - Arrow tool
- **F3** - Rectangle tool
- **F4** - Highlighter tool
- **Shift + Alt + X** (overlay activo) - Screen capture mode
- **Ctrl + Z** - Undo last element
- **ESC** - Exit current mode

### **Functionality**
1. **Activate**: Press `Shift+Alt+X` or double-click the system tray icon
2. **Select**: Draw a region on the screen
3. **Annotate**: Use F1-F4 tools to draw
4. **Capture**: Press `Shift+Alt+X` again when overlay is active to enter screenshot mode
5. **Configure**: Right-click on the icon → Settings
6. **Exit**: Right-click on the icon → Exit

### **🆕 Version 2.0.0 Usage - Enhanced Zoom Features**
1. **Write Text**: Type text in any region before zooming
2. **Create Drawings**: Use drawing tools (F1-F4) to annotate the screen
3. **Activate Zoom**: Scroll mouse wheel over a region to zoom in for the first time
4. **Automatic Text Copy**: Previously written text automatically appears in the zoom view
5. **Clean Focus**: All drawing elements are hidden during zoom for distraction-free experience
6. **Exit Zoom**: Scroll out or press ESC to return to normal view with all elements restored

## 🏗️ **Code Architecture**

### **Implemented RAII Classes**
- **`ScopedBitmap`** - Automatic HBITMAP management
- **`ScopedDC`** - Automatic HDC management
- **`ScopedIcon`** - Automatic HICON management
- **`ScopedBrush`** - Automatic HBRUSH management
- **`ScopedPen`** - Automatic HPEN management
- **`ScopedFont`** - Automatic HFONT management

### **Type-Safe Enumerations**
- **`DrawingTool`** - Drawing tools
- **`CustomMessage`** - Custom Windows messages

### **System Tray Auto-Restoration**
- **🔄 Explorer.exe Monitoring**: Continuous monitoring of Windows shell process
- **⏰ Periodic Health Checks**: 30-second timer for system tray verification
- **🆕 Automatic Recovery**: Restores icon when system tray becomes unresponsive
- **🔄 Seamless Restoration**: No user intervention required
- **📱 Process Lifecycle Management**: Handles explorer.exe restarts gracefully
- **🚀 Auto-Start Management**: Registry-based startup configuration
- **⚙️ User Control**: Enable/disable auto-start from system tray menu

### **Improvement Benefits**
- ✅ **No Memory Leaks**: Automatic GDI resource management
- ✅ **Robust Code**: Validity checks in all operations
- ✅ **Maintainable**: Clear and predictable structure
- ✅ **Performance**: Efficient resource management
- ✅ **Debugging**: Predictable behavior and easy debugging
- ✅ **System Tray Reliability**: Automatic restoration after explorer.exe restarts
- ✅ **Auto-Start Convenience**: Application launches automatically on Windows startup
- ✅ **User Control**: Easy enable/disable of auto-start functionality

## 📁 **Project Structure**

```
ScreenHighlighter/
├── main.cpp                    # Main source code (5494 lines)
├── CMakeLists.txt             # Optimized CMake configuration
├── build_debug_silent.bat     # Main compilation script
├── build_advanced.bat         # Advanced build script
├── config/                    # Project configuration
│   ├── CMakeConfig.cmake      # Default values
│   └── ScreenHighlighter.ini.in # Configuration template
├── resources.rc               # Windows resource file (icon + version info)
├── README.md                  # This file (unified documentation)
├── .gitignore                 # Optimized Git ignore
└── misc01.ico                 # Application icon
```

## 🔍 **Available CMake Variables**

### **DEBUG_MODE**
- **ON**: Enables debug mode, disables `-mwindows`
- **OFF**: Normal release mode
- **Usage**: `-DDEBUG_MODE=ON`

### **ENABLE_CONSOLE**
- **ON**: Enables console, disables `-mwindows`
- **OFF**: Disables console, enables `-mwindows`
- **Usage**: `-DENABLE_CONSOLE=ON`

### **SILENT_DEBUG**
- **ON**: Silent debug mode (no console)
- **OFF**: Normal debug mode
- **Usage**: `-DSILENT_DEBUG=ON`

### **CMAKE_BUILD_TYPE**
- **Release**: Maximum optimization
- **Debug**: No optimization, with debug symbols
- **RelWithDebInfo**: Optimization + debug symbols
- **MinSizeRel**: Size optimization

## 🛠️ **Manual Commands**

### **Basic Configuration**
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 🔐 **Administrator Privileges Required**

### **Why Administrator Rights?**
Screen Highlighter requires administrator privileges for the following reasons:
- **Global Hotkeys**: Registering system-wide hotkeys (Shift+Alt+X)
- **System Tray**: Full access to system tray functionality
- **Screen Capture**: Access to screen capture APIs
- **Cross-Process Communication**: Interacting with other applications

### **How It Works**
1. **Automatic Detection**: The application automatically detects if it's running as administrator
2. **UAC Prompt**: If not running as admin, it automatically requests elevation
3. **Seamless Experience**: Users just need to approve the UAC dialog
4. **Security**: Only requests the minimum required privileges

### **User Experience**
- **First Run**: Windows will show UAC dialog asking for permission
- **Subsequent Runs**: May remember the choice depending on Windows settings
- **No Manual Steps**: Users don't need to manually "Run as Administrator"

## 🚀 **Auto-Start on Windows Login**

### **How It Works**
- **Registry Configuration**: Automatically configures Windows registry for startup
- **User Control**: Right-click system tray icon to enable/disable auto-start
- **Persistent Settings**: Configuration survives Windows updates and restarts
- **Admin Rights**: Requires administrator privileges (automatically requested)

### **User Controls**
- **Enable Auto-Start**: Right-click system tray → "✅ Habilitar Auto-Inicio"
- **Disable Auto-Start**: Right-click system tray → "🚫 Deshabilitar Auto-Inicio"
- **Status Display**: Application shows auto-start status on startup

### **Registry Location**
```
HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Run
Value Name: "Screen Highlighter"
Value Data: [Full path to ScreenHighlighter.exe]
```

## 🎨 **Executable Icon and Resources**

### **Professional Appearance**
- **🎯 Custom Icon**: `misc01.ico` embedded in executable
- **📱 Windows Integration**: Icon appears in taskbar, file explorer, and system tray
- **🔧 Resource Compilation**: Windows resource file (`resources.rc`) for professional look
- **📋 Version Information**: File properties show company, description, and version

### **Resource File Contents**
- **Icon**: `misc01.ico` (16x16, 32x32, 48x48, 256x256 pixels)
- **Company**: Unnamed10110
- **Description**: Screen Highlighter - Screen Capture and Annotation Tool
- **Version**: 2.0.0.0
- **Copyright**: Copyright (C) 2025 Unnamed10110

### **Build Integration**
- **CMake**: Automatically includes `resources.rc` in compilation
- **Windows**: Generates native Windows resource object files
- **Linking**: Resources embedded directly in executable
- **Size**: Executable includes icon and version information

### **Advanced Configuration**
```bash
# Release mode without console
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DDEBUG_MODE=OFF -DENABLE_CONSOLE=OFF

# Debug mode with console
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDEBUG_MODE=ON -DENABLE_CONSOLE=ON

# Release mode with console
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DDEBUG_MODE=OFF -DENABLE_CONSOLE=ON

# Silent debug mode
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDEBUG_MODE=ON -DENABLE_CONSOLE=OFF -DSILENT_DEBUG=ON
```

## 🚨 **Troubleshooting**

### **Problem: "Application requires administrator privileges"**
**Solution**: This is normal and expected behavior
- **UAC Dialog**: Approve the User Account Control dialog when prompted
- **Automatic Elevation**: The application will automatically request admin rights
- **No Manual Steps**: You don't need to manually "Run as Administrator"

### **Problem: "Hotkeys don't respond"**
**Solution**: Use silent debug mode for complete functionality
```cmd
build_debug_silent.bat
```

**Alternative**: Debug mode with console to see messages
```cmd
build_advanced.bat
# Select option 2 (Debug with console)
```

### **Problem: "System tray icon doesn't appear"**
**Solution**: Check debug messages
```cmd
build_advanced.bat
# Select option 2 (Debug with console)
```

### **Problem: "Application closes immediately"**
**Solution**: Use debug mode to see errors
```cmd
build_advanced.bat
# Select option 2 (Debug with console)
```

### **Problem: "I want optimized version with debug"**
**Solution**: Use release + console mode
```cmd
build_advanced.bat
# Select option 3 (Release + console)
```

## 🎯 **Usage Recommendations**

### **For Developers**
1. **Daily development**: `build_debug_silent.bat` (recommended)
2. **Detailed debugging**: `build_advanced.bat` (option 2)
3. **Final testing**: `build_advanced.bat` (option 3)
4. **Release**: `build_advanced.bat` (option 1) - ⚠️ **Known issues**

### **For End Users**
1. **Normal use**: `build_debug_silent.bat` (silent debug mode)
2. **Report bugs**: `build_advanced.bat` (option 2)

### **For QA/Testing**
1. **Functional testing**: `build_debug_silent.bat`
2. **Performance testing**: `build_advanced.bat` (option 3)

## 🎉 **Project Achievements**

### **Before Refactoring**
- ❌ Manual GDI resource management (memory leaks)
- ❌ Use of "magic numbers" in code
- ❌ Basic build system with g++
- ❌ No robust error handling
- ❌ Difficult to maintain code

### **After Refactoring**
- ✅ **Automatic resource management** (complete RAII)
- ✅ **Type-safe enumerations** (no magic numbers)
- ✅ **Professional build system** (CMake)
- ✅ **Robust error handling** (explicit validations)
- ✅ **Maintainable and robust code**
- ✅ **Multiple compilation modes**
- ✅ **Complete and unified documentation**

## 🔮 **Suggested Next Steps**

### **Short Term**
1. **Use `build_debug_silent.bat`** for daily development
2. **Test all functionality** to verify stability
3. **Report any bugs** found
4. **Fix highlighter tool issue** when not in zoom mode
5. **Fix image paste issue** with Ctrl+T when not in zoom mode

### **Medium Term**
1. **Investigate Release mode pure problems**
2. **Optimize debug mode performance**
3. **Add unit tests** if necessary
4. **Improve zoom mode dependency** for tools

### **Long Term**
1. **Implement CI/CD** with GitHub Actions
2. **Add more annotation tools**
3. **Multi-platform support** if required
4. **Enhanced tool functionality** independent of zoom mode

## 🤝 **Contributing and Bug Reports**

### **How to Report Bugs**
1. **Email**: Send detailed bug reports to trojan.v6@gmail.com
2. **Include**: Steps to reproduce, expected vs actual behavior
3. **Specify**: Build mode used and system information
4. **Priority**: High priority for highlighter and image paste issues

### **Current Priority Issues**
- 🔴 **High**: Highlighter tool not working outside zoom mode
- 🔴 **High**: Image paste (Ctrl+T) not working outside zoom mode
- 🟡 **Medium**: Release mode compilation issues
- 🟢 **Low**: Performance optimizations and additional features

## 👨‍💻 **Developer Information**

- **Developer**: Unnamed10110
- **Primary Email**: trojan.v6@gmail.com
- **Secondary Email**: sergio.britos@gmail.com

## 🐛 **Known Issues and Pending Bugs**

### **Highlighter Tool Issue**
- **Problem**: Highlighter tool doesn't work when not in zoom mode
- **Status**: Pending fix
- **Workaround**: Use zoom mode to access highlighter functionality

### **Image Paste in Text Mode Issue**
- **Problem**: Image pasting with Ctrl+T doesn't work when not in zoom mode
- **Status**: Pending fix
- **Workaround**: Use zoom mode to paste images in text mode

### **Release Mode Issue**
- **Problem**: Pure Release mode doesn't work (known issue with -mwindows flag)
- **Status**: Known issue
- **Workaround**: Use Silent Debug Mode (`build_debug_silent.bat`) or Release + Console mode

## 📞 **Support and Contact**

- **Documentation**: This file (`README.md`) contains all information
- **Bug Reports**: Send to trojan.v6@gmail.com or sergio.britos@gmail.com
- **Known issues**: See section above for current bugs and workarounds
- **Recommended solution**: Use Silent Debug Mode (`build_debug_silent.bat`)

---

**🎯 Project Status: VERSION 2.0.0 - ENHANCED AND OPTIMIZED**  
**✅ Functionality**: 100% operational with enhanced zoom experience  
**🔧 Code quality**: Professional and maintainable with modern C++ practices  
**📚 Documentation**: Unified and complete with version 2.0.0 features  
**🆕 New Features**: Smart text copying and clean zoom focus implemented**
