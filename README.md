# UnnamedHighlighter

A professional screen highlighting and screenshot tool for Windows.

## 🚀 **Main Features**

### **Highlighting and Zoom:**
- **Activation**: `Shift+Alt+X`
- **Zoom**: Mouse scroll in selected regions
- **Overlay**: Black layer with transparent regions

### **Drawing Tools (F1-F4):**
- **F1**: Line
- **F2**: Arrow  
- **F3**: Rectangle
- **F5**: Highlighter
- **Usage**: Inside selected regions

### **Screenshot Capture:**
- **Activation**: `Ctrl+Enter`
- **Selection**: White dashed lines
- **Saving**: Local directory + Clipboard
- **Format**: `dd_MM_yyyy-HH-mm-ss.png`

### **Advanced Text Editor:**
- **Activation**: `Ctrl+T`
- **Location**: Inside regions or on screen
- **Color**: Pure white text
- **Image Support**: Paste images with `Ctrl+V` (supports clipboard images)
- **Text Selection**: Character-by-character selection with `Shift + Arrow Keys`
- **Smart Positioning**: Text and cursor automatically positioned below images
- **Visual Selection**: Blue background highlighting for selected text
- **Multi-line Support**: Full text editing with line breaks
- **Image Integration**: Seamless text and image mixing
- **Cursor Intelligence**: Smart cursor positioning that considers image heights

## 📋 **Installation**

### **1. Compile:**
```bash
.\build.bat
```

### **2. Run:**
```bash
.\ScreenHighlighter.exe
```

### **3. Ready!** The program is completely standalone and portable

## 🎯 **Quick Usage**

1. **Activate**: `Shift+Alt+X`
2. **Select region**: Click and drag
3. **Zoom**: Mouse scroll in the region
4. **Tools**: F1-F4 to draw
5. **Capture**: `Ctrl+Enter` for screenshots
6. **Text**: `Ctrl+T` to add text
7. **Images**: `Ctrl+V` to paste clipboard images
8. **Text Selection**: `Shift + Arrow Keys` for character selection

## 📝 **Advanced Text Editor Usage**

### **Text Operations:**
- **Type**: Just start typing in text mode
- **Line Breaks**: Press `Enter` for new lines
- **Navigation**: Use arrow keys to move cursor
- **Selection**: Hold `Shift` + arrow keys for text selection

### **Image Integration:**
- **Paste Images**: Copy any image to clipboard, then `Ctrl+V` in text mode
- **Smart Layout**: Text automatically positions below images
- **Mixed Content**: Seamlessly mix text and images in any order
- **Auto-sizing**: Images maintain their original dimensions

### **Selection Features:**
- **Character Selection**: `Shift + ←/→` for precise character selection
- **Line Selection**: `Shift + Home/End` for line selection
- **Select All**: `Ctrl+A` to select entire text
- **Visual Feedback**: Selected text highlighted in blue with white text

## 🔧 **Project Files**

### **Core Files:**
- **`main.cpp`** - Source code (C++17)
- **`build.bat`** - Build script with strict warnings enabled
- **`ScreenHighlighter.exe`** - Compiled executable (standalone)

### **Configuration & Resources:**
- **`ScreenHighlighter.ini`** - User configuration
- **`misc01.ico`** - Application icon
- **`ScreenHighlighter.manifest`** - Administrator permissions manifest

### **Development:**
- **`.gitignore`** - Git ignore patterns for build artifacts
- **`README.md`** - Project documentation

## 📁 **Screenshot Location**

Screenshots are saved in the executable directory with format:
```
22_08_2025-15-30-45.png
```

## 🚀 **Portability & Development**

**ScreenHighlighter is a completely standalone executable:**
- ✅ **No installation required**: Can run directly from any folder
- ✅ **No dependencies**: All required libraries are included
- ✅ **Cross-system compatible**: Works on any Windows 10/11 system
- ✅ **USB portable**: Can be carried on USB drives
- ✅ **Network deployment**: Easy to deploy across multiple computers

**Development-friendly:**
- ✅ **Source code included**: Full C++17 implementation
- ✅ **Build automation**: `build.bat` with strict compiler warnings
- ✅ **Version control ready**: `.gitignore` configured for build artifacts
- ✅ **Clean project structure**: Organized file layout

## 🗑️ **Uninstallation**

Simply delete the `ScreenHighlighter.exe` file. No system files are modified, making it completely safe to remove.

## ⚡ **Keyboard Shortcuts**

| Key | Function |
|-----|----------|
| `Shift+Alt+X` | Toggle highlighter on/off |
| `Ctrl+Enter` | Screenshot mode |
| `Ctrl+T` | Text mode |
| `Ctrl+V` | Paste clipboard images |
| `Shift + ←/→` | Character-by-character text selection |
| `Shift + Home/End` | Select to beginning/end of line |
| `Ctrl+A` | Select all text |
| `F1` | Line tool |
| `F2` | Arrow tool |
| `F3` | Rectangle tool |
| `F5` | Highlighter tool |
| `ESC` | Exit current mode |

## 🎨 **Technical Features**

- ✅ **Automatic permissions**: Automatic UAC elevation when needed
- ✅ **Custom icon**: Integrated in executable
- ✅ **Clean captures**: No selection borders
- ✅ **Correct orientation**: No image flipping
- ✅ **Consistent format**: Date-based naming
- ✅ **Local storage**: No system dependencies
- ✅ **Standalone executable**: No external dependencies required
- ✅ **Portable**: Can be moved to any Windows system
- ✅ **Self-contained**: All libraries included in the executable
- ✅ **Source code included**: Full C++17 source code available
- ✅ **Build system**: Automated build script with strict warnings

## 🚀 **Performance Optimizations**

- ✅ **Compiler optimizations**: Maximum optimization flags enabled
- ✅ **Inline expansion**: Aggressive function inlining
- ✅ **Intrinsic functions**: Compiler-optimized function calls
- ✅ **Font caching**: Static font objects to avoid recreation
- ✅ **Memory management**: Pre-reserved memory for strings
- ✅ **Efficient algorithms**: Optimized text processing and image handling
- ✅ **Resource optimization**: Reduced GDI resource usage

## 🔨 **Development & Building**

### **Requirements:**
- **Compiler**: MinGW-w64 or MSYS2 with g++ support
- **C++ Standard**: C++17 or higher
- **OS**: Windows 10/11

### **Build Process:**
```bash
# Compile with strict warnings enabled
.\build.bat

# The script will:
# - Check for g++ availability
# - Compile with -Wall -Wextra -Wpedantic -Werror
# - Create standalone executable
# - Show compilation status
```

### **Build Features:**
- **Strict warnings**: All warnings treated as errors
- **Optimization**: Maximum performance optimization flags
- **Static linking**: No external dependencies
- **Standalone**: Single executable file output

---

## 👨‍💻 **Developer**

**Unnamed10110**

📧 **Contact:**
- trojan.v6@gmail.com
- sergiobritos10110@gmail.com

## 💡 **Inspiration**

This program was inspired by **xshare** but designed to work in **real-time** for Windows systems.

## 🔄 **Recent Updates**

### **Text Editor Enhancements:**
- ✨ **Image Support**: Full clipboard image pasting support
- ✨ **Smart Text Positioning**: Automatic text positioning below images
- ✨ **Character Selection**: Precise character-by-character text selection
- ✨ **Visual Feedback**: Blue highlighting for selected text
- ✨ **Cursor Optimization**: Smart cursor positioning considering image heights

### **Performance Improvements:**
- ⚡ **Compiler Optimizations**: Maximum performance flags enabled
- ⚡ **Resource Caching**: Font objects cached for better performance
- ⚡ **Memory Efficiency**: Optimized string handling and memory management
- ⚡ **Algorithm Optimization**: Faster text processing and image handling

### **Development & Build System:**
- 🔧 **Build Automation**: `build.bat` with strict compiler warnings
- 🔧 **Code Quality**: All warnings treated as errors (-Werror)
- 🔧 **Project Structure**: Clean organization with `.gitignore`
- 🔧 **Source Code**: Full C++17 implementation included
- 🔧 **Bug Documentation**: Known issues documented with workarounds

---

## 🐛 **Known Issues & Bugs**

### **Highlighting and Image Pasting Bug:**
- **Issue**: Highlighting and pasting images do not work correctly when not in zoom mode
- **Status**: 🟡 **Pending Fix** - Bug identified, solution in progress
- **Workaround**: Use zoom mode (`Shift+Alt+X` → select region → mouse scroll) for full functionality
- **Affected Features**: 
  - Text highlighting with `Shift + Arrow Keys`
  - Image pasting with `Ctrl+V`
  - Text editor functionality

### **Technical Details:**
This bug occurs because the highlighting and image pasting systems are designed to work within the zoom context where the screen region is properly captured and managed. When operating outside zoom mode, the necessary screen context is not available, causing these features to malfunction.

---

**Developed for professional and personal use** 🎯✨
