#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <math.h>
#define M_PI 3.14159265358979323846
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <optional>
// Headers de seguridad para verificación de permisos
#include <windows.h>

// Optimizaciones de compilador para máxima performance
// Nota: Los pragmas específicos de MSVC no son compatibles con g++

// Incluir librería de controles comunes para sliders
// #pragma comment(lib, "comctl32.lib") // No soportado por g++

// ============================================================================
// CLASES RAII PARA GESTIÓN SEGURA DE RECURSOS GDI
// ============================================================================

// Clase RAII para HBITMAP - gestión automática de memoria
class ScopedBitmap {
    HBITMAP handle_;
public:
    ScopedBitmap() : handle_(nullptr) {}
    explicit ScopedBitmap(HBITMAP h) : handle_(h) {}
    ~ScopedBitmap() { 
        if (handle_) DeleteObject(handle_); 
    }
    
    // Constructor de movimiento
    ScopedBitmap(ScopedBitmap&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    // Operador de asignación de movimiento
    ScopedBitmap& operator=(ScopedBitmap&& other) noexcept {
        if (this != &other) {
            if (handle_) DeleteObject(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    // Eliminar copia
    ScopedBitmap(const ScopedBitmap&) = delete;
    ScopedBitmap& operator=(const ScopedBitmap&) = delete;
    
    // Operadores de conversión
    operator HBITMAP() const { return handle_; }
    HBITMAP get() const { return handle_; }
    
    // Liberar el handle (transferir propiedad)
    HBITMAP release() {
        HBITMAP h = handle_;
        handle_ = nullptr;
        return h;
    }
    
    // Verificar si es válido
    explicit operator bool() const { return handle_ != nullptr; }
    
    // Asignar nuevo handle
    void reset(HBITMAP h = nullptr) {
        if (handle_) DeleteObject(handle_);
        handle_ = h;
    }
};

// Clase RAII para HDC - gestión automática de contexto de dispositivo
class ScopedDC {
    HDC handle_;
    bool should_delete_;
public:
    ScopedDC() : handle_(nullptr), should_delete_(false) {}
    explicit ScopedDC(HDC h, bool delete_on_destroy = false) 
        : handle_(h), should_delete_(delete_on_destroy) {}
    ~ScopedDC() { 
        if (handle_ && should_delete_) DeleteDC(handle_); 
    }
    
    // Constructor de movimiento
    ScopedDC(ScopedDC&& other) noexcept 
        : handle_(other.handle_), should_delete_(other.should_delete_) {
        other.handle_ = nullptr;
        other.should_delete_ = false;
    }
    
    // Operador de asignación de movimiento
    ScopedDC& operator=(ScopedDC&& other) noexcept {
        if (this != &other) {
            if (handle_ && should_delete_) DeleteDC(handle_);
            handle_ = other.handle_;
            should_delete_ = other.should_delete_;
            other.handle_ = nullptr;
            other.should_delete_ = false;
        }
        return *this;
    }
    
    // Eliminar copia
    ScopedDC(const ScopedDC&) = delete;
    ScopedDC& operator=(const ScopedDC&) = delete;
    
    // Operadores de conversión
    operator HDC() const { return handle_; }
    HDC get() const { return handle_; }
    
    // Liberar el handle (transferir propiedad)
    HDC release() {
        HDC h = handle_;
        handle_ = nullptr;
        should_delete_ = false;
        return h;
    }
    
    // Verificar si es válido
    explicit operator bool() const { return handle_ != nullptr; }
    
    // Asignar nuevo handle
    void reset(HDC h = nullptr, bool delete_on_destroy = false) {
        if (handle_ && should_delete_) DeleteDC(handle_);
        handle_ = h;
        should_delete_ = delete_on_destroy;
    }
};

// Clase RAII para HICON - gestión automática de iconos
class ScopedIcon {
    HICON handle_;
public:
    ScopedIcon() : handle_(nullptr) {}
    explicit ScopedIcon(HICON h) : handle_(h) {}
    ~ScopedIcon() { 
        if (handle_) DestroyIcon(handle_); 
    }
    
    // Constructor de movimiento
    ScopedIcon(ScopedIcon&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    // Operador de asignación de movimiento
    ScopedIcon& operator=(ScopedIcon&& other) noexcept {
        if (this != &other) {
            if (handle_) DestroyIcon(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    // Eliminar copia
    ScopedIcon(const ScopedIcon&) = delete;
    ScopedIcon& operator=(const ScopedIcon&) = delete;
    
    // Operadores de conversión
    operator HICON() const { return handle_; }
    HICON get() const { return handle_; }
    
    // Liberar el handle (transferir propiedad)
    HICON release() {
        HICON h = handle_;
        handle_ = nullptr;
        return h;
    }
    
    // Verificar si es válido
    explicit operator bool() const { return handle_ != nullptr; }
    
    // Asignar nuevo handle
    void reset(HICON h = nullptr) {
        if (handle_) DestroyIcon(handle_);
        handle_ = h;
    }
};

// Clase RAII para HBRUSH - gestión automática de pinceles
class ScopedBrush {
    HBRUSH handle_;
    bool should_delete_;
public:
    ScopedBrush() : handle_(nullptr), should_delete_(false) {}
    explicit ScopedBrush(HBRUSH h, bool delete_on_destroy = false) 
        : handle_(h), should_delete_(delete_on_destroy) {}
    ~ScopedBrush() { 
        if (handle_ && should_delete_) DeleteObject(handle_); 
    }
    
    // Constructor de movimiento
    ScopedBrush(ScopedBrush&& other) noexcept 
        : handle_(other.handle_), should_delete_(other.should_delete_) {
        other.handle_ = nullptr;
        other.should_delete_ = false;
    }
    
    // Operador de asignación de movimiento
    ScopedBrush& operator=(ScopedBrush&& other) noexcept {
        if (this != &other) {
            if (handle_ && should_delete_) DeleteObject(handle_);
            handle_ = other.handle_;
            should_delete_ = other.should_delete_;
            other.handle_ = nullptr;
            other.should_delete_ = false;
        }
        return *this;
    }
    
    // Eliminar copia
    ScopedBrush(const ScopedBrush&) = delete;
    ScopedBrush& operator=(const ScopedBrush&) = delete;
    
    // Operadores de conversión
    operator HBRUSH() const { return handle_; }
    HBRUSH get() const { return handle_; }
    
    // Liberar el handle (transferir propiedad)
    HBRUSH release() {
        HBRUSH h = handle_;
        handle_ = nullptr; 
        should_delete_ = false;
        return h; 
    }
    
    // Verificar si es válido
    explicit operator bool() const { return handle_ != nullptr; }
    
    // Asignar nuevo handle
    void reset(HBRUSH h = nullptr, bool delete_on_destroy = false) {
        if (handle_ && should_delete_) DeleteObject(handle_);
        handle_ = h;
        should_delete_ = false;
    }
};

// Clase RAII para HPEN - gestión automática de plumas
class ScopedPen {
    HPEN handle_;
public:
    ScopedPen() : handle_(nullptr) {}
    explicit ScopedPen(HPEN h) : handle_(h) {}
    ~ScopedPen() { 
        if (handle_) DeleteObject(handle_); 
    }
    
    // Constructor de movimiento
    ScopedPen(ScopedPen&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    // Operador de asignación de movimiento
    ScopedPen& operator=(ScopedPen&& other) noexcept {
        if (this != &other) {
            if (handle_) DeleteObject(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    // Eliminar copia
    ScopedPen(const ScopedPen&) = delete;
    ScopedPen& operator=(const ScopedPen&) = delete;
    
    // Operadores de conversión
    operator HPEN() const { return handle_; }
    HPEN get() const { return handle_; }
    
    // Liberar el handle (transferir propiedad)
    HPEN release() {
        HPEN h = handle_;
        handle_ = nullptr;
        return h;
    }
    
    // Verificar si es válido
    explicit operator bool() const { return handle_ != nullptr; }
    
    // Asignar nuevo handle
    void reset(HPEN h = nullptr) {
        if (handle_) DeleteObject(handle_);
        handle_ = h;
    }
};

// Clase RAII para HFONT - gestión automática de fuentes
class ScopedFont {
    HFONT handle_;
public:
    ScopedFont() : handle_(nullptr) {}
    explicit ScopedFont(HFONT h) : handle_(h) {}
    ~ScopedFont() { 
        if (handle_) DeleteObject(handle_); 
    }
    
    // Constructor de movimiento
    ScopedFont(ScopedFont&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    // Operador de asignación de movimiento
    ScopedFont& operator=(ScopedFont&& other) noexcept {
        if (this != &other) {
            if (handle_) DeleteObject(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    // Eliminar copia
    ScopedFont(const ScopedFont&) = delete;
    ScopedFont& operator=(const ScopedFont&) = delete;
    
    // Operadores de conversión
    operator HFONT() const { return handle_; }
    HFONT get() const { return handle_; }
    
    // Liberar el handle (transferir propiedad)
    HFONT release() {
        HFONT h = handle_;
        handle_ = nullptr;
        return h;
    }
    
    // Verificar si es válido
    explicit operator bool() const { return handle_ != nullptr; }
    
    // Asignar nuevo handle
    void reset(HFONT h = nullptr) {
        if (handle_) DeleteObject(handle_);
        handle_ = h;
    }
};

// ============================================================================
// ENUMERACIONES TIPO-SEGURAS PARA REEMPLAZAR MAGIC NUMBERS
// ============================================================================

// Enumeración para herramientas de dibujo (reemplaza magic numbers)
enum class DrawingTool : uint8_t {
    None = 0,
    Line = 1,
    Arrow = 2,
    Rectangle = 3,
    Text = 4,        // Mantenido para compatibilidad
    Highlighter = 5
};

// Enumeración para tipos de mensajes personalizados
enum class CustomMessage : UINT {
    Taskbar = WM_USER + 1,
    TaskbarRestored = WM_USER + 2
};

// ============================================================================
// CONSTANTES Y CONFIGURACIÓN
// ============================================================================

// Configuración optimizada integrada con constantes de performance
constexpr UINT WM_TASKBAR = static_cast<UINT>(CustomMessage::Taskbar);
constexpr UINT WM_TASKBAR_RESTORED = static_cast<UINT>(CustomMessage::TaskbarRestored);
constexpr int TRAY_ICON_ID = 1;
constexpr int TRAY_ICON_SMALL = 16;
constexpr int TRAY_ICON_LARGE = 32;
constexpr char TRAY_TOOLTIP_TEXT[] = "Screen Highlighter";
constexpr int MENU_ACTIVATE_ID = 1001;
constexpr int MENU_SETTINGS_ID = 1002;
constexpr int MENU_EXIT_ID = 1003;
constexpr int MENU_SEPARATOR_ID = 1003;
constexpr wchar_t MENU_ACTIVATE_TEXT[] = L"Activate Highlight (Shift+Alt+X)";
constexpr wchar_t MENU_EXIT_TEXT[] = L"Exit";
constexpr wchar_t MENU_SEPARATOR_TEXT[] = L"";

// Nombre del archivo de configuración
constexpr const char* CONFIG_FILE = "ScreenHighlighter.ini";

// ============================================================================
// VARIABLES GLOBALES CON MEJORAS DE SEGURIDAD
// ============================================================================

// Variables globales para el estado del programa
std::atomic<bool> running(true);
std::atomic<bool> overlay_active(false);
std::atomic<bool> selection_mode(false);
std::atomic<bool> needsRedraw(false);
std::atomic<int> start_x(0);
std::atomic<int> start_y(0);
std::atomic<int> end_x(0);
std::atomic<int> end_y(0);

// Variables para zoom eficiente (captura única)
std::atomic<float> zoom_factor(1.0f);
std::atomic<bool> zoom_active(false);
std::atomic<int> zoom_center_x(0);
std::atomic<int> zoom_center_y(0);

// Reemplazar HBITMAP y HDC raw con clases RAII
ScopedBitmap hZoomedBitmap;
ScopedDC hZoomedDC;

// Variables para texto en zoom
std::atomic<bool> text_input_mode(false);
std::atomic<bool> text_cursor_visible(true);
std::atomic<int> text_cursor_pos(0);
// Variables para selección de texto
std::atomic<int> text_selection_start(-1);
std::atomic<int> text_selection_end(-1);
std::atomic<bool> text_selection_active(false);
// Sistema simple para manejar imágenes del clipboard
std::vector<ScopedBitmap> clipboard_images;
std::vector<std::wstring> image_markers; // Marcadores en el texto
// El texto se maneja en una variable global simple (no atómica)
std::wstring zoom_text;

// Variables para configuración
std::atomic<bool> settings_overlay_active(false);
std::atomic<int> hotkey_shift_alt_x(1); // 1 = habilitado, 0 = deshabilitado
std::atomic<int> overlay_opacity(178); // 0-255
std::atomic<int> zoom_min_factor(50); // 0.5x = 50
std::atomic<int> zoom_max_factor(500); // 5.0x = 500
std::atomic<int> text_cursor_blink_speed(500); // ms
std::atomic<int> region_border_thickness(2); // píxeles
std::atomic<int> region_border_color(0x00FF00); // Verde por defecto

// Variables para scroll vertical de la ventana de configuración
std::atomic<int> scroll_pos(0);
std::atomic<int> scroll_max(1000);  // Contenido total alto

// Variables para herramientas de dibujo
std::atomic<DrawingTool> current_drawing_tool{DrawingTool::None};
std::atomic<int> drawing_color(RGB(255, 0, 0)); // Color rojo por defecto
std::atomic<int> drawing_thickness(3); // Grosor de línea
std::atomic<bool> drawing_fill(false); // Relleno para formas
std::atomic<int> drawing_start_x(-1);
std::atomic<int> drawing_start_y(-1);
std::atomic<bool> drawing_active(false);

// Variables para captura de pantalla
std::atomic<bool> screenshot_mode(false);
std::atomic<int> screenshot_start_x(-1);
std::atomic<int> screenshot_start_y(-1);
std::atomic<int> screenshot_end_x(-1);
std::atomic<int> screenshot_end_y(-1);
    // Variables de texto eliminadas

// Estructura para elementos dibujados
struct DrawingElement {
    DrawingTool tool_type;
    int x1, y1, x2, y2;
    COLORREF color;
    int thickness;
    bool filled;
    std::wstring text; // Para texto libre
    
    DrawingElement(DrawingTool type, int x1, int y1, int x2, int y2, COLORREF col, int thick, bool fill, const std::wstring& txt = L"")
        : tool_type(type), x1(x1), y1(y1), x2(x2), y2(y2), color(col), thickness(thick), filled(fill), text(txt) {}
};

std::vector<DrawingElement> drawing_elements;

// Variables para el system tray
NOTIFYICONDATA nid;
HWND hMainWnd;
bool systemTrayInitialized = false;
// WM_TASKBAR ya está definido arriba

// Variables para monitoreo de explorer.exe y restauración del system tray
static DWORD explorerProcessId = 0;
static HANDLE explorerProcessHandle = NULL;
static std::thread explorerMonitorThread;
static std::atomic<bool> explorerMonitorRunning(false);
static std::atomic<bool> systemTrayRestorationNeeded(false);

// Estructura para almacenar rectángulos
struct ScreenRectangle {
    int x1, y1, x2, y2;
    ScreenRectangle(int x1, int y1, int x2, int y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}
};

std::vector<ScreenRectangle> screenRectangles;

// Variable global para el handle de la ventana overlay
HWND hCurrentOverlay = NULL;

// Función para cargar configuración desde archivo .ini
void LoadConfiguration() {
    std::ifstream file(CONFIG_FILE);
    if (!file.is_open()) {
        // Si no existe el archivo, usar valores por defecto
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';') continue; // Ignorar líneas vacías y comentarios
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Eliminar espacios en blanco
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            try {
                if (key == "overlay_opacity") {
                    overlay_opacity.store(std::stoi(value));
                } else if (key == "zoom_min_factor") {
                    zoom_min_factor.store(std::stoi(value));
                } else if (key == "zoom_max_factor") {
                    zoom_max_factor.store(std::stoi(value));
                } else if (key == "text_cursor_blink_speed") {
                    text_cursor_blink_speed.store(std::stoi(value));
                } else if (key == "region_border_thickness") {
                    region_border_thickness.store(std::stoi(value));
                } else if (key == "region_border_color") {
                    region_border_color.store(std::stoi(value));
                } else if (key == "hotkey_shift_alt_x") {
                    hotkey_shift_alt_x.store(std::stoi(value));
                }
            } catch (const std::exception&) {
                // Ignorar valores inválidos
            }
        }
    }
    
    file.close();
}

// Función para guardar configuración en archivo .ini
void SaveConfiguration() {
    std::ofstream file(CONFIG_FILE);
    if (!file.is_open()) {
        return;
    }
    
    file << "; Screen Highlighter Configuration File" << std::endl;
    file << "; Generated automatically - Do not edit manually" << std::endl;
    file << std::endl;
    
    file << "overlay_opacity=" << overlay_opacity.load() << std::endl;
    file << "zoom_min_factor=" << zoom_min_factor.load() << std::endl;
    file << "zoom_max_factor=" << zoom_max_factor.load() << std::endl;
    file << "text_cursor_blink_speed=" << text_cursor_blink_speed.load() << std::endl;
    file << "region_border_thickness=" << region_border_thickness.load() << std::endl;
    file << "region_border_color=" << region_border_color.load() << std::endl;
    file << "hotkey_shift_alt_x=" << hotkey_shift_alt_x.load() << std::endl;
    
    file.close();
}

// Función para verificar si hay una imagen en el clipboard
bool CheckClipboardForImage() {
    if (OpenClipboard(NULL)) {
        bool hasImage = IsClipboardFormatAvailable(CF_BITMAP) || 
                       IsClipboardFormatAvailable(CF_DIB) || 
                       IsClipboardFormatAvailable(CF_DIBV5);
        CloseClipboard();
        return hasImage;
    }
    return false;
}

// Función para capturar imagen del clipboard de manera simple y segura
bool AddImageElement() {
    if (!OpenClipboard(NULL)) {
        return false;
    }
    
    bool success = false;
    
    // Verificar si hay un bitmap en el clipboard
    if (IsClipboardFormatAvailable(CF_BITMAP)) {
        HBITMAP hClipboardBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
        if (hClipboardBitmap) {
            // Crear una copia simple del bitmap
            HDC hdcScreen = GetDC(NULL);
            if (hdcScreen) {
                HDC hdcMem = CreateCompatibleDC(hdcScreen);
                if (hdcMem) {
                    BITMAP bm;
                    if (GetObject(hClipboardBitmap, sizeof(BITMAP), &bm) > 0) {
                        // Verificar dimensiones razonables
                        if (bm.bmWidth > 0 && bm.bmHeight > 0 && 
                            bm.bmWidth <= 2000 && bm.bmHeight <= 2000) {
                            
                            // Crear nuevo bitmap compatible
                            HBITMAP hNewBitmap = CreateCompatibleBitmap(hdcScreen, bm.bmWidth, bm.bmHeight);
                            if (hNewBitmap) {
                                HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hNewBitmap);
                                
                                // Crear DC temporal para el bitmap del clipboard
                                HDC hdcClipboard = CreateCompatibleDC(hdcScreen);
                                if (hdcClipboard) {
                                    HBITMAP hOldClipboard = (HBITMAP)SelectObject(hdcClipboard, hClipboardBitmap);
                                    
                                    // Copiar el bitmap del clipboard al nuevo bitmap
                                    if (BitBlt(hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, 
                                              hdcClipboard, 0, 0, SRCCOPY)) {
                                        
                                        // Agregar a la lista de imágenes
                                        clipboard_images.push_back(ScopedBitmap(hNewBitmap));
                                        
                                        // Agregar marcador de imagen en el texto (índice correcto) - optimizado
                                        const int imageIndex = (int)clipboard_images.size() - 1;
                                        // Pre-reservar espacio para evitar reasignaciones
                                        zoom_text.reserve(zoom_text.length() + 20);
                                        
                                        // Construir marcador de manera eficiente
                                        wchar_t markerBuffer[32];
                                        int markerLen = swprintf_s(markerBuffer, L"[IMAGE_%d]", imageIndex);
                                        if (markerLen > 0) {
                                            zoom_text.insert(text_cursor_pos.load(), markerBuffer, markerLen);
                                            text_cursor_pos.store(text_cursor_pos.load() + markerLen);
                                        }
                                        
                                        // Calcular cuántos saltos de línea necesita el texto para estar justo debajo de la imagen
                                        int imageHeight = bm.bmHeight;
                                        int lineHeight = 20; // Altura estándar de línea de texto
                                        int totalLines = (imageHeight + lineHeight - 1) / lineHeight; // Redondear hacia arriba
                                        
                                        // AJUSTE ESPECÍFICO PARA IMÁGENES:
                                        // 1. El texto sube 4 líneas más (reducir en 6 líneas en total)
                                        // 2. Insertar solo los saltos de línea necesarios para el texto
                                        // 3. El cursor se posiciona visualmente 2 líneas más abajo (en el renderizado)
                                        int textLines = std::max(0, totalLines - 6); // Texto sube 4 líneas más
                                        
                                        // Insertar solo los saltos de línea necesarios para el texto
                                        for (int i = 0; i < textLines; i++) {
                                            zoom_text.insert(text_cursor_pos.load(), L"\n");
                                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                                        }
                                        
                                        // NO insertar saltos adicionales aquí - el renderizado manejará la posición visual del cursor
                                        // El cursor y texto permanecen sincronizados, pero el renderizado mostrará
                                        // el cursor 2 líneas más abajo visualmente
                                        
                                        // Ahora el texto está 4 líneas más arriba de donde normalmente estaría
                                        // y el cursor está sincronizado con el texto
                                        
                                        // El cursor está ahora en la posición correcta después de la imagen
                                        // y el texto tiene los saltos de línea necesarios para que esté debajo
                                        // Forzar redibujado para actualizar la posición del cursor
                                        needsRedraw.store(true);
                                        
                                        success = true;
                                    } else {
                                        clipboard_images.back().reset();
                                    }
                                    
                                    // Limpiar DC temporal
                                    SelectObject(hdcClipboard, hOldClipboard);
                                    DeleteDC(hdcClipboard);
                                } else {
                                    clipboard_images.back().reset();
                                }
                                
                                SelectObject(hdcMem, hOldBitmap);
                            }
                        }
                    }
                    DeleteDC(hdcMem);
                }
                ReleaseDC(NULL, hdcScreen);
            }
        }
    }
    
    CloseClipboard();
    return success;
}



// Función para cargar el icono desde archivo
HICON LoadIconFromFile(int size) {
    // Cargar el icono desde el archivo misc01.ico
    HICON hIcon = (HICON)LoadImageW(
        GetModuleHandle(NULL),           // Handle del módulo actual
        L"misc01.ico",                   // Nombre del archivo de icono
        IMAGE_ICON,                      // Tipo de imagen
        size,                            // Ancho deseado
        size,                            // Alto deseado
        LR_LOADFROMFILE                  // Cargar desde archivo
    );
    
    // Si falla la carga, usar un icono del sistema como fallback
    if (!hIcon) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    return hIcon;
}

// Función para agregar el icono al system tray
bool AddToSystemTray() {
    printf("  🖼️ Configurando icono del system tray...\n");
    
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hMainWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TASKBAR;
    
    // Cargar icono con gestión automática de memoria
    printf("  🖼️ Cargando icono personalizado...\n");
    ScopedIcon hIcon(LoadIconFromFile(TRAY_ICON_SMALL));
    if (!hIcon) {
        // Si falla la carga del icono, usar un icono por defecto del sistema
        printf("  ⚠️ Usando icono por defecto del sistema\n");
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    } else {
        printf("  ✅ Icono personalizado cargado\n");
        nid.hIcon = hIcon.release(); // Transferir propiedad al nid
    }
    
    strcpy_s(nid.szTip, TRAY_TOOLTIP_TEXT);
    printf("  💬 Tooltip configurado: %s\n", TRAY_TOOLTIP_TEXT);
    
    printf("  🔧 Agregando icono al system tray...\n");
    if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
        // Manejar error de notificación
        printf("  ❌ Error al agregar icono al system tray\n");
        OutputDebugStringW(L"Error al agregar icono al system tray\n");
        return false;
    }
    
    printf("  ✅ Icono agregado exitosamente al system tray\n");
    systemTrayInitialized = true;
    return true;
}

// Función para remover el icono del system tray
void RemoveFromSystemTray() {
    if (!Shell_NotifyIcon(NIM_DELETE, &nid)) {
        // Manejar error de eliminación
        OutputDebugStringW(L"Error al remover icono del system tray\n");
    }
    
    // Limpiar el icono si existe
    if (nid.hIcon) {
        DestroyIcon(nid.hIcon);
        nid.hIcon = nullptr;
    }
}

// Función para restaurar el icono del system tray
bool RestoreSystemTrayIcon() {
    printf("🔄 Restaurando icono del system tray...\n");
    
    // Verificar si el icono ya está en el system tray
    if (systemTrayInitialized) {
        // Intentar restaurar el icono existente
        if (Shell_NotifyIcon(NIM_MODIFY, &nid)) {
            printf("✅ Icono del system tray restaurado exitosamente\n");
            return true;
        }
    }
    
    // Si no se puede restaurar, agregar uno nuevo
    printf("🆕 Agregando nuevo icono al system tray...\n");
    return AddToSystemTray();
}

// Función para monitorear el proceso explorer.exe
void MonitorExplorerProcess() {
    printf("🔍 Iniciando monitoreo de explorer.exe...\n");
    
    // Obtener el PID de explorer.exe
    HWND shellTrayWnd = FindWindowW(L"Shell_TrayWnd", NULL);
    if (shellTrayWnd) {
        GetWindowThreadProcessId(shellTrayWnd, &explorerProcessId);
        printf("📱 Explorer.exe PID: %lu\n", explorerProcessId);
        
        if (explorerProcessId > 0) {
            explorerProcessHandle = OpenProcess(SYNCHRONIZE, FALSE, explorerProcessId);
            if (explorerProcessHandle) {
                printf("✅ Monitoreo de explorer.exe iniciado\n");
                
                // Monitorear continuamente
                while (explorerMonitorRunning.load()) {
                    // Esperar a que explorer.exe termine
                    DWORD waitResult = WaitForSingleObject(explorerProcessHandle, 1000); // 1 segundo
                    
                    if (waitResult == WAIT_OBJECT_0) {
                        // Explorer.exe terminó
                        printf("⚠️ Explorer.exe terminó - Restaurando system tray...\n");
                        systemTrayRestorationNeeded.store(true);
                        
                        // Esperar a que explorer.exe se reinicie
                        Sleep(2000); // 2 segundos
                        
                        // Restaurar el icono del system tray
                        if (RestoreSystemTrayIcon()) {
                            printf("✅ System tray restaurado después del reinicio de explorer.exe\n");
                        } else {
                            printf("❌ Error al restaurar system tray\n");
                        }
                        
                        // Reiniciar el monitoreo
                        CloseHandle(explorerProcessHandle);
                        explorerProcessHandle = NULL;
                        
                        // Buscar el nuevo proceso de explorer.exe
                        Sleep(1000); // 1 segundo
                        shellTrayWnd = FindWindowW(L"Shell_TrayWnd", NULL);
                        if (shellTrayWnd) {
                            GetWindowThreadProcessId(shellTrayWnd, &explorerProcessId);
                            if (explorerProcessId > 0) {
                                explorerProcessHandle = OpenProcess(SYNCHRONIZE, FALSE, explorerProcessId);
                                if (explorerProcessHandle) {
                                    printf("✅ Monitoreo de explorer.exe reiniciado\n");
                                }
                            }
                        }
                    }
                    
                    // Verificar si el icono del system tray sigue visible
                    if (!systemTrayRestorationNeeded.load()) {
                        // Verificar si el icono está visible
                        if (systemTrayInitialized) {
                            // Enviar mensaje de prueba al system tray
                            if (!Shell_NotifyIcon(NIM_MODIFY, &nid)) {
                                printf("⚠️ Icono del system tray no responde - Restaurando...\n");
                                if (RestoreSystemTrayIcon()) {
                                    printf("✅ System tray restaurado\n");
                                }
                            }
                        }
                    }
                }
                
                if (explorerProcessHandle) {
                    CloseHandle(explorerProcessHandle);
                    explorerProcessHandle = NULL;
                }
            }
        }
    }
    
    printf("🔍 Monitoreo de explorer.exe terminado\n");
}

// Función para iniciar el monitoreo de explorer.exe
void StartExplorerMonitoring() {
    if (!explorerMonitorRunning.load()) {
        printf("🚀 Iniciando monitoreo de explorer.exe...\n");
        explorerMonitorRunning.store(true);
        explorerMonitorThread = std::thread(MonitorExplorerProcess);
        printf("✅ Monitoreo de explorer.exe iniciado\n");
    }
}

// Función para detener el monitoreo de explorer.exe
void StopExplorerMonitoring() {
    if (explorerMonitorRunning.load()) {
        printf("🛑 Deteniendo monitoreo de explorer.exe...\n");
        explorerMonitorRunning.store(false);
        
        if (explorerMonitorThread.joinable()) {
            explorerMonitorThread.join();
        }
        
        if (explorerProcessHandle) {
            CloseHandle(explorerProcessHandle);
            explorerProcessHandle = NULL;
        }
        
        printf("✅ Monitoreo de explorer.exe detenido\n");
    }
}

// Función para mostrar el menú contextual del system tray
void ShowTrayMenu() {
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, MENU_ACTIVATE_ID, MENU_ACTIVATE_TEXT);
            AppendMenuW(hMenu, MF_STRING, MENU_SETTINGS_ID, L"⚙️ Settings");
    AppendMenuW(hMenu, MF_SEPARATOR, MENU_SEPARATOR_ID, MENU_SEPARATOR_TEXT);
    AppendMenuW(hMenu, MF_STRING, MENU_EXIT_ID, MENU_EXIT_TEXT);
    
    SetForegroundWindow(hMainWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hMainWnd, NULL);
    DestroyMenu(hMenu);
}

// Función para dibujar línea
void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness) {
    ScopedPen hPen(CreatePen(PS_SOLID, thickness, color));
    if (!hPen) return; // Verificar que se creó correctamente
    
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    
    SelectObject(hdc, hOldPen);
    // hPen se limpia automáticamente al salir del scope
}

// Función para dibujar flecha
void DrawArrow(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness) {
    // Calcular dirección de la flecha (desde x1,y1 hacia x2,y2)
    double angle = atan2(y2 - y1, x2 - x1);
    int arrowLength = thickness * 6; // Puntas más largas para mejor visibilidad
    int arrowAngle = 35; // Grados más amplios para puntas más visibles
    
    // Calcular el punto donde termina la línea principal (antes de las puntas)
    // La línea principal debe terminar antes de las puntas para que se vea bien
    int lineEndX = x2 - (int)(arrowLength * cos(angle));
    int lineEndY = y2 - (int)(arrowLength * sin(angle));
    
    // Dibujar línea principal (desde x1,y1 hasta antes de las puntas)
    DrawLine(hdc, x1, y1, lineEndX, lineEndY, color, thickness);
    
    // Puntas de la flecha (desde el punto final hacia atrás)
    int arrowX1 = x2 - (int)(arrowLength * cos(angle - arrowAngle * M_PI / 180));
    int arrowY1 = y2 - (int)(arrowLength * sin(angle - arrowAngle * M_PI / 180));
    int arrowX2 = x2 - (int)(arrowLength * cos(angle + arrowAngle * M_PI / 180));
    int arrowY2 = y2 - (int)(arrowLength * sin(angle + arrowAngle * M_PI / 180));
    
    // Dibujar puntas de la flecha
    DrawLine(hdc, x2, y2, arrowX1, arrowY1, color, thickness);
    DrawLine(hdc, x2, y2, arrowX2, arrowY2, color, thickness);
}

// Función para dibujar rectángulo
void DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness, bool filled) {
    ScopedPen hPen(CreatePen(PS_SOLID, thickness, color));
    if (!hPen) return; // Verificar que se creó correctamente
    
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    
    // Crear pincel solo si es necesario (filled = true)
    ScopedBrush hBrush;
    if (filled) {
        hBrush.reset(CreateSolidBrush(color), true);
        if (!hBrush) return; // Verificar que se creó correctamente
    }
    
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, filled ? hBrush : GetStockObject(NULL_BRUSH));
    
    Rectangle(hdc, x1, y1, x2, y2);
    
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    // hPen y hBrush se limpian automáticamente al salir del scope
}

// Función para reproducir sonido de captura
void PlayScreenshotSound() {
    // Reproducir un beep más fuerte y agudo para indicar captura exitosa
    // Frecuencia más alta (1200Hz) y duración más larga (300ms) para mayor notoriedad
    Beep(2400, 800); // 1200Hz por 300ms
}

// Función auxiliar para guardar bitmap como archivo BMP
bool SaveBitmapToFile(HBITMAP hBitmap, const std::wstring& filePath) {
    // Obtener información del bitmap
    BITMAP bm;
    if (!GetObject(hBitmap, sizeof(BITMAP), &bm)) {
        return false;
    }
    
    // Crear DC de memoria
    HDC hMemDC = CreateCompatibleDC(NULL);
    if (!hMemDC) {
        return false;
    }
    
    // Seleccionar el bitmap
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    
    // Preparar estructura BITMAPINFO
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24; // 24 bits por píxel
    bmi.bmiHeader.biCompression = BI_RGB;
    
    // Calcular tamaño del buffer de píxeles
    int bytesPerRow = ((bm.bmWidth * 24 + 31) / 32) * 4; // Alineado a 4 bytes
    int bufferSize = bytesPerRow * bm.bmHeight;
    
    // Crear buffer para los píxeles
    std::vector<BYTE> pixelBuffer(bufferSize);
    
    // Obtener los píxeles del bitmap
    if (!GetDIBits(hMemDC, hBitmap, 0, bm.bmHeight, pixelBuffer.data(), &bmi, DIB_RGB_COLORS)) {
        SelectObject(hMemDC, hOldBitmap);
        DeleteDC(hMemDC);
        return false;
    }
    
    // Crear archivo (convertir wstring a string para std::ofstream)
    std::string filePathStr(filePath.begin(), filePath.end());
    std::ofstream file(filePathStr, std::ios::binary);
    if (!file.is_open()) {
        SelectObject(hMemDC, hOldBitmap);
        DeleteDC(hMemDC);
        return false;
    }
    
    // Escribir encabezado BMP
    BITMAPFILEHEADER bfh;
    ZeroMemory(&bfh, sizeof(bfh));
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bufferSize;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    
    // Escribir encabezado del archivo
    file.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    
    // Escribir encabezado de información
    file.write(reinterpret_cast<const char*>(&bmi.bmiHeader), sizeof(BITMAPINFOHEADER));
    
    // Escribir datos de píxeles (de arriba hacia abajo para capturas de pantalla)
    for (int y = 0; y < bm.bmHeight; y++) {
        file.write(reinterpret_cast<const char*>(&pixelBuffer[y * bytesPerRow]), bytesPerRow);
    }
    
    // Cerrar archivo
    file.close();
    
    // Limpiar recursos
    SelectObject(hMemDC, hOldBitmap);
    DeleteDC(hMemDC);
    
    return true;
}

// Función para guardar captura en el directorio del ejecutable
void SaveScreenshotToDownloads(HBITMAP hBitmap, int x1, int y1, int x2, int y2) {
    (void)x1; (void)y1; (void)x2; (void)y2; // Parámetros no utilizados
    // Obtener el directorio del ejecutable
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exePathStr = std::wstring(exePath);
    size_t lastSlash = exePathStr.find_last_of(L"\\/");
    std::wstring exeDir = exePathStr.substr(0, lastSlash + 1);
    
    // Generar nombre de archivo con formato dd_MM_yyyy-HH-mm-ss.png
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Formatear fecha y hora con ceros a la izquierda
    wchar_t filename[256];
    swprintf_s(filename, L"%02d_%02d_%04d-%02d-%02d-%02d.png", 
               st.wDay, st.wMonth, st.wYear, 
               st.wHour, st.wMinute, st.wSecond);
    
    std::wstring fullPath = exeDir + std::wstring(filename);
    
    // Usar directamente el bitmap capturado (ya contiene la región correcta)
    if (SaveBitmapToFile(hBitmap, fullPath)) {
        // Éxito: archivo guardado correctamente
    } else {
        // Error: no se pudo guardar el archivo
    }
}

// Función para capturar región de pantalla
void CaptureScreenRegion(int x1, int y1, int x2, int y2) {
    // Asegurar coordenadas correctas
    int left = std::min(x1, x2);
    int top = std::min(y1, y2);
    int right = std::max(x1, x2);
    int bottom = std::max(y1, y2);
    
    // Verificar tamaño mínimo
    if ((right - left) < 5 || (bottom - top) < 5) {
        return; // Región muy pequeña
    }
    
    // Capturar la pantalla (excluyendo el borde de selección)
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    
    // Reducir ligeramente la región para excluir el borde de selección
    int borderOffset = 2; // 2 píxeles de margen para excluir el borde
    int captureLeft = left + borderOffset;
    int captureTop = top + borderOffset;
    int captureRight = right - borderOffset;
    int captureBottom = bottom - borderOffset;
    
    // Asegurar que la región de captura sea válida
    if (captureRight <= captureLeft || captureBottom <= captureTop) {
        ReleaseDC(NULL, hScreenDC);
        return;
    }
    
    int width = captureRight - captureLeft;
    int height = captureBottom - captureTop;
    
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    
    // Copiar la región de la pantalla (sin el borde de selección)
    BitBlt(hMemDC, 0, 0, width, height, hScreenDC, captureLeft, captureTop, SRCCOPY);
    
    // Copiar al clipboard
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hBitmap);
        CloseClipboard();
    }
    
    // Guardar en el directorio del ejecutable
    SaveScreenshotToDownloads(hBitmap, left, top, right, bottom);
    
    // Reproducir sonido de confirmación
    PlayScreenshotSound();
    
    // Limpiar recursos
    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
}

// Función para dibujar resaltador (capa amarilla casi opaca)
void DrawHighlighter(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    (void)color; // Parámetro no utilizado
    // Color amarillo casi opaco (solo 10% transparente)
    // Usar un amarillo muy opaco para máxima visibilidad
    COLORREF yellowColor = RGB(255, 255, 25); // Amarillo casi opaco
    
    // Crear pincel amarillo casi opaco
    ScopedBrush hBrush(CreateSolidBrush(yellowColor), true);
    if (!hBrush) return; // Verificar que se creó correctamente
    
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    
    // Configurar modo de mezcla para crear transparencia real
    // R2_MASKPEN crea una capa transparente real sin invertir colores
    int oldROP = SetROP2(hdc, R2_MASKPEN);
    
    // Dibujar rectángulo que crea una capa casi opaca
    Rectangle(hdc, x1, y1, x2, y2);
    
    // Restaurar modo de mezcla original
    SetROP2(hdc, oldROP);
    
    SelectObject(hdc, hOldBrush);
    // hBrush se limpia automáticamente al salir del scope
}

// Función para dibujar el overlay
void DrawOverlay(HDC hdc, int width, int height) {
    // Configurar transparencia para que el magenta (255, 0, 255) y cian (0, 255, 255) sean completamente transparentes
    if (hCurrentOverlay) {
        // Usar LWA_COLORKEY para hacer transparentes tanto el magenta como el cian
        // El overlay negro tendrá la opacidad configurada por el usuario
        int userOpacity = overlay_opacity.load();
        
        // Cuando hay zoom activo, hacer el overlay completamente opaco
        if (zoom_active.load()) {
            userOpacity = 255; // Completamente opaco
        }
        
        // Nota: LWA_COLORKEY solo soporta un color a la vez, pero podemos usar el cian
        // que es el color que se usa cuando drawing_active es true
        COLORREF transparentColor = drawing_active.load() ? RGB(0, 255, 255) : RGB(255, 0, 255);
        SetLayeredWindowAttributes(hCurrentOverlay, transparentColor, userOpacity, LWA_COLORKEY | LWA_ALPHA);
    }
    
    // Crear DC de memoria para doble buffering
    ScopedDC hMemDC(CreateCompatibleDC(hdc), true);
    if (!hMemDC) return; // Verificar que se creó correctamente
    
    ScopedBitmap hBitmap(CreateCompatibleBitmap(hdc, width, height));
    if (!hBitmap) return; // Verificar que se creó correctamente
    
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    
    // Dibujar overlay negro en toda la pantalla
    ScopedBrush hOverlayBrush(CreateSolidBrush(RGB(0, 0, 0)), true);
    if (!hOverlayBrush) return; // Verificar que se creó correctamente
    
    RECT fullRect = {0, 0, width, height};
    FillRect(hMemDC, &fullRect, hOverlayBrush);
    
    // Las regiones son completamente transparentes (sin color de overlay)
    if (!screenRectangles.empty() || (selection_mode.load() && start_x.load() != -1)) {
        // Cuando drawing_active es true, usar un color que bloquee la interactividad pero sea transparente
        // Cuando drawing_active es false, usar magenta que es completamente transparente
        COLORREF regionColor = drawing_active.load() ? RGB(0, 255, 255) : RGB(255, 0, 255);
        ScopedBrush hRegionBrush(CreateSolidBrush(regionColor), true);
        if (!hRegionBrush) return; // Verificar que se creó correctamente
        
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, hRegionBrush);
        
        // Regiones guardadas - completamente transparentes
        if (zoom_active.load()) {
            // Con zoom activo: solo mostrar la última región (la que tiene zoom)
            // Las demás regiones desaparecen para enfocar solo en el zoom
        } else {
            // Sin zoom: las regiones son completamente transparentes (sin overlay)
            for (const auto& rect : screenRectangles) {
                RECT r = {rect.x1, rect.y1, rect.x2, rect.y2};
                FillRect(hMemDC, &r, hRegionBrush); // Color transparente con LWA_COLORKEY
            }
        }
        
        // Región en tiempo real - solo mostrar durante la selección activa
        if (selection_mode.load() && start_x.load() != -1 && !drawing_active.load()) {
            int x1 = std::min(start_x.load(), end_x.load());
            int y1 = std::min(start_y.load(), end_y.load());
            int x2 = std::max(start_x.load(), end_x.load());
            int y2 = std::max(start_y.load(), end_y.load());
            RECT liveRect = {x1, y1, x2, y2};
            FillRect(hMemDC, &liveRect, hRegionBrush);
        }
        
        SelectObject(hMemDC, hOldBrush);
        // hRegionBrush se limpia automáticamente al salir del scope
    }
    
    // Dibujar zoom de la última región (si está activo)
    if (zoom_active.load() && !screenRectangles.empty() && hZoomedDC && hZoomedBitmap) {
        const auto& lastRect = screenRectangles.back();
        float zoom = zoom_factor.load();
        
        int originalWidth = lastRect.x2 - lastRect.x1;
        int originalHeight = lastRect.y2 - lastRect.y1;
        int zoomedWidth = (int)(originalWidth * zoom);
        int zoomedHeight = (int)(originalHeight * zoom);
        
        // Centrar el zoom
        int centerX = zoom_center_x.load();
        int centerY = zoom_center_y.load();
        int zoomX = centerX - zoomedWidth / 2;
        int zoomY = centerY - zoomedHeight / 2;
        
        // Debug: mostrar información del zoom
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"DEBUG: Zoom activo - Original: %dx%d, Zoom: %.2f, Zoomed: %dx%d\n", 
                   originalWidth, originalHeight, zoom, zoomedWidth, zoomedHeight);
        OutputDebugStringW(debugMsg);
        
        // Primero dibujar un fondo blanco sólido para el zoom (brillo normal)
        RECT zoomRect = {zoomX, zoomY, zoomX + zoomedWidth, zoomY + zoomedHeight};
        ScopedBrush hWhiteBrush(CreateSolidBrush(RGB(255, 255, 255)), true);
        if (!hWhiteBrush) return; // Verificar que se creó correctamente
        
        FillRect(hMemDC, &zoomRect, hWhiteBrush);
        
        // Debug: verificar que el StretchBlt funcione
        // IMPORTANTE: Seleccionar el bitmap antes de hacer StretchBlt
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hZoomedDC.get(), hZoomedBitmap.get());
        
        BOOL stretchResult = StretchBlt(hMemDC, zoomX, zoomY, zoomedWidth, zoomedHeight,
                                       hZoomedDC.get(), 0, 0, originalWidth, originalHeight, SRCCOPY);
        if (!stretchResult) {
            wchar_t errorMsg[256];
            swprintf_s(errorMsg, L"DEBUG: StretchBlt falló - Error: %d\n", GetLastError());
            OutputDebugStringW(errorMsg);
        } else {
            OutputDebugStringW(L"DEBUG: StretchBlt exitoso\n");
        }
        
        // Restaurar el bitmap anterior
        SelectObject(hZoomedDC.get(), hOldBitmap);
        
        // Dibujar texto debajo de la región con zoom
        if (!zoom_text.empty()) {
                    // Configurar fuente para el texto (con cache estático para performance)
        static HFONT hCachedFontZoom = NULL;
        // static HFONT hOldCachedFontZoom = NULL; // Variable no utilizada
        if (!hCachedFontZoom) {
            hCachedFontZoom = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            // hOldCachedFontZoom = (HFONT)SelectObject(hMemDC, hCachedFontZoom); // Variable no utilizada
        }
            
            // Color del texto
            SetTextColor(hMemDC, RGB(255, 255, 255)); // Texto blanco
            SetBkMode(hMemDC, TRANSPARENT); // Fondo transparente
            
            // Calcular tamaño del texto para ajustar el textbox dinámicamente
            SIZE textSize;
            GetTextExtentPoint32W(hMemDC, zoom_text.c_str(), zoom_text.length(), &textSize);
            
            // Calcular dimensiones del textbox ajustado al contenido más ancho
            int textBoxWidth = 100; // Ancho mínimo para estabilidad
            
            // Calcular el ancho máximo de todas las líneas
            std::wstring widthText = zoom_text;
            size_t widthPos = 0;
            size_t widthNewlinePos;
            while ((widthNewlinePos = widthText.find(L'\n', widthPos)) != std::wstring::npos) {
                // Calcular ancho de la línea actual
                std::wstring widthLine = widthText.substr(widthPos, widthNewlinePos - widthPos);
                if (!widthLine.empty()) {
                    SIZE widthLineSize;
                    GetTextExtentPoint32W(hMemDC, widthLine.c_str(), widthLine.length(), &widthLineSize);
                    textBoxWidth = std::max(textBoxWidth, (int)(widthLineSize.cx + 40)); // +40px de padding para seguridad
                }
                widthPos = widthNewlinePos + 1;
            }
            
            // Calcular ancho de la última línea (si no termina en \n)
            if (widthPos < widthText.length()) {
                std::wstring widthLastLine = widthText.substr(widthPos);
                if (!widthLastLine.empty()) {
                    SIZE widthLastLineSize;
                    GetTextExtentPoint32W(hMemDC, widthLastLine.c_str(), widthLastLine.length(), &widthLastLineSize);
                    textBoxWidth = std::max(textBoxWidth, (int)(widthLastLineSize.cx + 40)); // +40px de padding para seguridad
                }
            }
            
            // Asegurar ancho mínimo y máximo para estabilidad (optimizado)
            textBoxWidth = std::clamp(textBoxWidth, 100, 1200); // Mínimo 100px, máximo 1200px
            
            // Calcular altura basada en el número de líneas (comentado por no utilizado)
            // int numLines = 1;
            // size_t newlineCount = 0;
            // size_t countPos = 0;
            // while ((countPos = zoom_text.find(L'\n', countPos)) != std::wstring::npos) {
            //     newlineCount++;
            //     countPos++;
            // }
            // numLines = (int)(newlineCount + 1);
            
            // int totalLineHeight = 20; // Variable no utilizada
            // int textBoxHeight = (int)(numLines * totalLineHeight + 10); // Variable no utilizada
            
            // Centrar el textbox debajo de la región con zoom
            int textBoxX = zoomX + (zoomedWidth - textBoxWidth) / 2;
            int textBoxY = zoomY + zoomedHeight + 5;
            
            // Posición del texto (centrado en el textbox ajustado)
            // RECT textRect = {textBoxX + 10, textBoxY + 5, textBoxX + textBoxWidth - 10, textBoxY + textBoxHeight - 5}; // Variable no utilizada
            
            // Dibujar texto base primero (siempre estable) - manejo manual de saltos de línea
            std::wstring currentText = zoom_text;
            // int currentY = textBoxY + 5; // Variable no utilizada
            // int lineHeight = 20; // Variable no utilizada
            
                        // Función simple para dibujar texto línea por línea
            auto drawTextSimple = [&](int startY) {
                if (zoom_text.empty()) return;
                
                std::wstring currentText = zoom_text;
                int currentY = startY;
                int lineHeight = 20; // Altura de cada línea (se ajusta dinámicamente para imágenes)
                
                size_t pos = 0;
                
                // Procesar el texto línea por línea
                while (pos < currentText.length()) {
                    // Buscar el próximo salto de línea
                    size_t nextNewline = currentText.find(L'\n', pos);
                    if (nextNewline == std::wstring::npos) {
                        nextNewline = currentText.length();
                    }
                    
                    // Obtener la línea actual
                    std::wstring currentLine = currentText.substr(pos, nextNewline - pos);
                    
                    // Verificar si la línea contiene un marcador de imagen
                    if (currentLine.find(L"[IMAGE_") != std::wstring::npos) {
                        // Extraer el índice de la imagen del marcador
                        size_t imageStart = currentLine.find(L"[IMAGE_");
                        size_t imageEnd = currentLine.find(L"]", imageStart);
                        if (imageStart != std::wstring::npos && imageEnd != std::wstring::npos) {
                            std::wstring imageIndexStr = currentLine.substr(imageStart + 7, imageEnd - imageStart - 7);
                            try {
                                int imageIndex = std::stoi(imageIndexStr);
                                if (imageIndex >= 0 && imageIndex < static_cast<int>(clipboard_images.size()) && clipboard_images[imageIndex]) {
                                    // Dibujar la imagen
                                    HBITMAP hImage = clipboard_images[imageIndex].get();
                                    BITMAP bm;
                                    
                                    if (GetObject(hImage, sizeof(BITMAP), &bm) > 0) {
                                        int imgWidth = bm.bmWidth;
                                        int imgHeight = bm.bmHeight;
                                        
                                        // Centrar la imagen horizontalmente
                                        int imgX = textBoxX + (textBoxWidth - imgWidth) / 2;
                                        
                                        // Crear DC temporal para la imagen
                                        HDC hdcImage = CreateCompatibleDC(hMemDC);
                                        if (hdcImage) {
                                            HBITMAP hOldImageBitmap = (HBITMAP)SelectObject(hdcImage, hImage);
                                            
                                            // Dibujar la imagen en su posición
                                            BitBlt(hMemDC, imgX, currentY, imgWidth, imgHeight, hdcImage, 0, 0, SRCCOPY);
                                            
                                            // Limpiar
                                            SelectObject(hdcImage, hOldImageBitmap);
                                            DeleteDC(hdcImage);
                                        }
                                        
                                        // Avanzar la posición Y según la altura de la imagen + espaciado
                                        currentY += imgHeight + 5;
                                    }
                                }
                            } catch (...) {
                                // Si hay error, continuar con la siguiente línea
                                currentY += lineHeight;
                            }
                        } else {
                            // Si no se puede parsear el marcador, continuar
                            currentY += lineHeight;
                        }
                    } else if (!currentLine.empty()) {
                        // Calcular si esta línea tiene texto seleccionado
                        bool hasSelection = false;
                        int selectionStart = -1;
                        int selectionEnd = -1;
                        
                        if (text_selection_active.load()) {
                            int start = std::min(text_selection_start.load(), text_selection_end.load());
                            int end = std::max(text_selection_start.load(), text_selection_end.load());
                            
                            // Verificar si esta línea tiene caracteres específicos seleccionados
                            int lineStart = (int)pos;
                            int lineEnd = (int)(pos + currentLine.length());
                            
                            // Solo seleccionar si hay superposición real
                            if (start < lineEnd && end > lineStart) {
                                hasSelection = true;
                                selectionStart = std::max(0, start - lineStart);
                                selectionEnd = std::min((int)currentLine.length(), end - lineStart);
                                
                                // Solo seleccionar si hay caracteres realmente seleccionados
                                if (selectionStart >= selectionEnd) {
                                    hasSelection = false;
                                }
                            }
                        }
                        
                        if (hasSelection && selectionStart < selectionEnd) {
                            // Dibujar texto con selección parcial
                            // Primero dibujar el texto normal
                            RECT lineRect = {textBoxX + 10, currentY, textBoxX + textBoxWidth - 10, currentY + lineHeight};
                            DrawTextW(hMemDC, currentLine.c_str(), -1, &lineRect, DT_LEFT | DT_TOP);
                            
                            // Ahora dibujar el fondo de selección solo para los caracteres seleccionados
                            if (selectionStart < (int)currentLine.length()) {
                                // Calcular posición X del inicio de la selección
                                std::wstring textBeforeSelection = currentLine.substr(0, selectionStart);
                                SIZE textBeforeSize;
                                GetTextExtentPoint32W(hMemDC, textBeforeSelection.c_str(), textBeforeSelection.length(), &textBeforeSize);
                                
                                // Calcular posición X del final de la selección
                                std::wstring textInSelection = currentLine.substr(selectionStart, selectionEnd - selectionStart);
                                SIZE textInSelectionSize;
                                GetTextExtentPoint32W(hMemDC, textInSelection.c_str(), textInSelection.length(), &textInSelectionSize);
                                
                                // Dibujar fondo azul solo para los caracteres seleccionados
                                RECT selectionRect = {
                                    textBoxX + 10 + (int)textBeforeSize.cx, 
                                    currentY, 
                                    textBoxX + 10 + (int)textBeforeSize.cx + (int)textInSelectionSize.cx, 
                                    currentY + lineHeight
                                };
                                
                                HBRUSH hSelectionBrush = CreateSolidBrush(RGB(0, 120, 215)); // Azul de selección estándar de Windows
                                FillRect(hMemDC, &selectionRect, hSelectionBrush);
                                DeleteObject(hSelectionBrush);
                                
                                // Redibujar solo el texto seleccionado en blanco sobre el fondo azul
                                SetTextColor(hMemDC, RGB(255, 255, 255)); // Texto blanco sobre fondo azul
                                RECT selectedTextRect = {
                                    textBoxX + 10 + (int)textBeforeSize.cx, 
                                    currentY, 
                                    textBoxX + 10 + (int)textBeforeSize.cx + (int)textInSelectionSize.cx, 
                                    currentY + lineHeight
                                };
                                DrawTextW(hMemDC, textInSelection.c_str(), -1, &selectedTextRect, DT_LEFT | DT_TOP);
                                
                                // Restaurar color del texto normal
                                SetTextColor(hMemDC, RGB(255, 255, 255));
                                            }
                                        } else {
                            // Dibujar texto normal sin selección
                            RECT lineRect = {textBoxX + 10, currentY, textBoxX + textBoxWidth - 10, currentY + lineHeight};
                            DrawTextW(hMemDC, currentLine.c_str(), -1, &lineRect, DT_LEFT | DT_TOP);
                                        }
                        
                        currentY += lineHeight; // Línea de texto normal
                                } else {
                        // Línea vacía - usar altura estándar para el texto
                        currentY += lineHeight; // Altura estándar para línea vacía
                            }
                            
                            // Mover a la siguiente línea
                            pos = nextNewline + 1;
                        }
            };
            
            // Dibujar texto simple línea por línea (ahora incluye el renderizado de imágenes mediante marcadores)
            drawTextSimple(textBoxY + 5);
            // Las imágenes se renderizan automáticamente mediante los marcadores [IMAGE_X] en drawTextSimple
            

            
            // CURSOR SIMPLE - Aparece al final del texto como una línea vertical
            if (text_input_mode.load() && text_cursor_visible.load()) {
                // Calcular posición del cursor simulando exactamente el mismo renderizado que el texto
                int cursorX = textBoxX + 10; // Inicio del texto + padding
                int cursorY = textBoxY + 5;
                
                if (text_cursor_pos.load() > 0) {
                    // Simular el renderizado exacto hasta la posición del cursor
                    std::wstring currentText = zoom_text.substr(0, text_cursor_pos.load());
                    int currentY = textBoxY + 5;
                    int lineHeight = 20;
                    size_t pos = 0;
                    size_t targetPos = text_cursor_pos.load();
                    bool foundCursor = false;
                    
                    // Procesar el texto línea por línea igual que drawTextSimple
                    while (pos < currentText.length() && !foundCursor) {
                        // Buscar el próximo salto de línea
                        size_t nextNewline = currentText.find(L'\n', pos);
                        if (nextNewline == std::wstring::npos) {
                            nextNewline = currentText.length();
                        }
                        
                        // Verificar si el cursor está en esta línea
                        if (targetPos >= pos && targetPos <= nextNewline) {
                            // El cursor está en esta línea
                            foundCursor = true;
                            
                            // Obtener la línea actual
                            std::wstring currentLine = currentText.substr(pos, nextNewline - pos);
                            
                            // Verificar si la línea contiene un marcador de imagen
                            if (currentLine.find(L"[IMAGE_") != std::wstring::npos) {
                                // Esta línea tiene una imagen, obtener su altura
                                size_t imageStart = currentLine.find(L"[IMAGE_");
                                size_t imageEnd = currentLine.find(L"]", imageStart);
                                if (imageStart != std::wstring::npos && imageEnd != std::wstring::npos) {
                                    std::wstring imageIndexStr = currentLine.substr(imageStart + 7, imageEnd - imageStart - 7);
                                    try {
                                        int imageIndex = std::stoi(imageIndexStr);
                                        if (imageIndex >= 0 && imageIndex < static_cast<int>(clipboard_images.size()) && clipboard_images[imageIndex]) {
                                            HBITMAP hImage = clipboard_images[imageIndex].get();
                                            BITMAP bm;
                                            if (GetObject(hImage, sizeof(BITMAP), &bm) > 0) {
                                                // El cursor debe estar después de la imagen
                                                cursorY = currentY + bm.bmHeight + 5;
                                                cursorX = textBoxX + 10; // Al inicio de la siguiente línea
                                            }
                                        }
                                    } catch (...) {
                                        cursorY = currentY + 20;
                                        cursorX = textBoxX + 10;
                                    }
                                }
                            } else {
                                // Esta línea es texto normal
                                std::wstring textBeforeCursor = currentLine.substr(0, targetPos - pos);
                                SIZE textSize;
                                GetTextExtentPoint32W(hMemDC, textBeforeCursor.c_str(), textBeforeCursor.length(), &textSize);
                                cursorX = textBoxX + 10 + (int)textSize.cx;
                                cursorY = currentY;
                            }
                        } else {
                            // El cursor no está en esta línea, solo avanzar la posición Y
                            std::wstring currentLine = currentText.substr(pos, nextNewline - pos);
                            
                            if (currentLine.find(L"[IMAGE_") != std::wstring::npos) {
                                // Esta línea tiene una imagen
                                size_t imageStart = currentLine.find(L"[IMAGE_");
                                size_t imageEnd = currentLine.find(L"]", imageStart);
                                if (imageStart != std::wstring::npos && imageEnd != std::wstring::npos) {
                                    std::wstring imageIndexStr = currentLine.substr(imageStart + 7, imageEnd - imageStart - 7);
                                    try {
                                        int imageIndex = std::stoi(imageIndexStr);
                                        if (imageIndex >= 0 && imageIndex < static_cast<int>(clipboard_images.size()) && clipboard_images[imageIndex]) {
                                            HBITMAP hImage = clipboard_images[imageIndex].get();
                                            BITMAP bm;
                                            if (GetObject(hImage, sizeof(BITMAP), &bm) > 0) {
                                                // IMPORTANTE: Actualizar currentY con la altura real de la imagen
                                                currentY += bm.bmHeight + 5;
                                            } else {
                                                currentY += lineHeight; // Fallback si no se puede obtener altura
                                            }
                                        } else {
                                            currentY += lineHeight; // Fallback si no se encuentra la imagen
                                        }
                                    } catch (...) {
                                        currentY += lineHeight; // Fallback si hay error
                                    }
                                } else {
                                    currentY += lineHeight; // Fallback si no se puede parsear
                                }
                            } else {
                                // Línea de texto normal
                                currentY += lineHeight;
                            }
                        }
                        
                        // Mover a la siguiente línea
                        pos = nextNewline + 1;
                    }
                }
                
                // Dibujar cursor vertical simple en la posición correcta
                HPEN hCursorPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                HPEN hOldPen = (HPEN)SelectObject(hMemDC, hCursorPen);
                MoveToEx(hMemDC, cursorX, cursorY, NULL);
                LineTo(hMemDC, cursorX, cursorY + 20); // Altura fija de 20px por línea
                SelectObject(hMemDC, hOldPen);
                DeleteObject(hCursorPen);
            }
            
            // Restaurar fuente (no es necesario con cache estático)
            // SelectObject(hMemDC, hOldCachedFontZoom);
            // DeleteObject(hCachedFontZoom);
        }
    }
    
    // Dibujar texto cuando no hay zoom pero sí está en modo texto
    if (!zoom_active.load() && text_input_mode.load() && !zoom_text.empty()) {
        // Configurar fuente para el texto (con cache estático para performance)
        static HFONT hCachedFont = NULL;
        // static HFONT hOldCachedFont = NULL; // Variable no utilizada
        if (!hCachedFont) {
            hCachedFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            // hOldCachedFont = (HFONT)SelectObject(hMemDC, hCachedFont); // Variable no utilizada
        }
        
        // Color del texto
        SetTextColor(hMemDC, RGB(255, 255, 255)); // Texto blanco
        SetBkMode(hMemDC, TRANSPARENT); // Fondo transparente
        
        // Calcular tamaño del texto para ajustar el textbox dinámicamente
        SIZE textSize;
        GetTextExtentPoint32W(hMemDC, zoom_text.c_str(), zoom_text.length(), &textSize);
        
        // Calcular dimensiones del textbox ajustado al contenido más ancho
        int textBoxWidth = 100; // Ancho mínimo para estabilidad
        
        // Calcular el ancho máximo de todas las líneas
        std::wstring widthText = zoom_text;
        size_t widthPos = 0;
        size_t widthNewlinePos;
        while ((widthNewlinePos = widthText.find(L'\n', widthPos)) != std::string::npos) {
            // Calcular ancho de la línea actual
            std::wstring widthLine = widthText.substr(widthPos, widthNewlinePos - widthPos);
            if (!widthLine.empty()) {
                SIZE widthLineSize;
                GetTextExtentPoint32W(hMemDC, widthLine.c_str(), widthLine.length(), &widthLineSize);
                textBoxWidth = std::max(textBoxWidth, (int)(widthLineSize.cx + 40)); // +40px de padding para seguridad
            }
            widthPos = widthNewlinePos + 1;
        }
        
        // Calcular ancho de la última línea (si no termina en \n)
        if (widthPos < widthText.length()) {
            std::wstring widthLastLine = widthText.substr(widthPos);
            if (!widthLastLine.empty()) {
                SIZE widthLastLineSize;
                GetTextExtentPoint32W(hMemDC, widthLastLine.c_str(), widthLastLine.length(), &widthLastLineSize);
                textBoxWidth = std::max(textBoxWidth, (int)(widthLastLineSize.cx + 40)); // +40px de padding para seguridad
            }
        }
        
        // Asegurar ancho mínimo y máximo para estabilidad
        textBoxWidth = std::max(textBoxWidth, 100);  // Mínimo 100px
        textBoxWidth = std::min(textBoxWidth, 1200); // Máximo 1200px para mostrar texto largo completo
        
        // Calcular altura basada en el número de líneas
        // Calcular altura basada en el número de líneas (comentado por no utilizado)
        // int numLines = 1;
        // size_t newlineCount = 0;
        // size_t countPos = 0;
        // while ((countPos = zoom_text.find(L'\n', countPos)) != std::string::npos) {
        //     newlineCount++;
        //     countPos++;
        // }
        // numLines = (int)(newlineCount + 1);
        
        // int totalLineHeight = 20; // Variable no utilizada
        // int textBoxHeight = (int)(numLines * totalLineHeight + 10); // Variable no utilizada
        
        // Posicionar el textbox debajo de la última región seleccionada
        int textBoxX = 20;
        int textBoxY = 20;
        
        // Si hay regiones, posicionar el texto debajo de la última
        if (!screenRectangles.empty()) {
            const auto& lastRect = screenRectangles.back();
            textBoxX = lastRect.x1;
            textBoxY = lastRect.y2 + 5; // 5px debajo de la región
        }
        
        // Posición del texto (centrado en el textbox ajustado)
        // RECT textRect = {textBoxX + 10, textBoxY + 5, textBoxX + textBoxWidth - 10, textBoxY + textBoxHeight - 5}; // Variable no utilizada
        
        // Dibujar texto línea por línea
        std::wstring currentText = zoom_text;
        int currentY = textBoxY + 5;
        int lineHeight = 20; // Altura de cada línea
        
        size_t pos = 0;
        while (pos < currentText.length()) {
            // Buscar el próximo salto de línea
            size_t nextNewline = currentText.find(L'\n', pos);
            if (nextNewline == std::string::npos) {
                nextNewline = currentText.length();
            }
            
            // Obtener la línea actual
            std::wstring currentLine = currentText.substr(pos, nextNewline - pos);
            
            // Calcular si esta línea tiene texto seleccionado
            bool hasSelection = false;
            int selectionStart = -1;
            int selectionEnd = -1;
            
            if (text_selection_active.load()) {
                int start = std::min(text_selection_start.load(), text_selection_end.load());
                int end = std::max(text_selection_start.load(), text_selection_end.load());
                
                // Verificar si esta línea tiene caracteres específicos seleccionados
                int lineStart = (int)pos;
                int lineEnd = (int)(pos + currentLine.length());
                
                // Solo seleccionar si hay superposición real
                if (start < lineEnd && end > lineStart) {
                    hasSelection = true;
                    selectionStart = std::max(0, start - lineStart);
                    selectionEnd = std::min((int)currentLine.length(), end - lineStart);
                    
                    // Solo seleccionar si hay caracteres realmente seleccionados
                    if (selectionStart >= selectionEnd) {
                        hasSelection = false;
                    }
                }
            }
            
            if (hasSelection && selectionStart < selectionEnd) {
                // Dibujar texto con selección parcial
                // Primero dibujar el texto normal
                RECT lineRect = {textBoxX + 10, currentY, textBoxX + textBoxWidth - 10, currentY + lineHeight};
                DrawTextW(hMemDC, currentLine.c_str(), -1, &lineRect, DT_LEFT | DT_TOP);
                
                // Ahora dibujar el fondo de selección solo para los caracteres seleccionados
                if (selectionStart < (int)currentLine.length()) {
                    // Calcular posición X del inicio de la selección
                    std::wstring textBeforeSelection = currentLine.substr(0, selectionStart);
                    SIZE textBeforeSize;
                    GetTextExtentPoint32W(hMemDC, textBeforeSelection.c_str(), textBeforeSelection.length(), &textBeforeSize);
                    
                    // Calcular posición X del final de la selección
                    std::wstring textInSelection = currentLine.substr(selectionStart, selectionEnd - selectionStart);
                    SIZE textInSelectionSize;
                    GetTextExtentPoint32W(hMemDC, textInSelection.c_str(), textInSelection.length(), &textInSelectionSize);
                    
                    // Dibujar fondo azul solo para los caracteres seleccionados
                    RECT selectionRect = {
                        textBoxX + 10 + (int)textBeforeSize.cx, 
                        currentY, 
                        textBoxX + 10 + (int)textBeforeSize.cx + (int)textInSelectionSize.cx, 
                        currentY + lineHeight
                    };
                    
                    HBRUSH hSelectionBrush = CreateSolidBrush(RGB(0, 120, 215)); // Azul de selección estándar de Windows
                    FillRect(hMemDC, &selectionRect, hSelectionBrush);
                    DeleteObject(hSelectionBrush);
                    
                    // Redibujar solo el texto seleccionado en blanco sobre el fondo azul
                    SetTextColor(hMemDC, RGB(255, 255, 255)); // Texto blanco sobre fondo azul
                    RECT selectedTextRect = {
                        textBoxX + 10 + (int)textBeforeSize.cx, 
                        currentY, 
                        textBoxX + 10 + (int)textBeforeSize.cx + (int)textInSelectionSize.cx, 
                        currentY + lineHeight
                    };
                    DrawTextW(hMemDC, textInSelection.c_str(), -1, &selectedTextRect, DT_LEFT | DT_TOP);
                    
                    // Restaurar color del texto normal
                    SetTextColor(hMemDC, RGB(255, 255, 255));
                                    }
                                } else {
                // Dibujar texto normal sin selección
                RECT lineRect = {textBoxX + 10, currentY, textBoxX + textBoxWidth - 10, currentY + lineHeight};
                DrawTextW(hMemDC, currentLine.c_str(), -1, &lineRect, DT_LEFT | DT_TOP);
                }
                
                // Mover a la siguiente línea
                pos = nextNewline + 1;
            currentY += lineHeight;
        }
        
        // CURSOR SIMPLE - Aparece al final del texto como una línea vertical
        if (text_cursor_visible.load()) {
            // Calcular posición del cursor simulando exactamente el mismo renderizado que el texto
            int cursorX = textBoxX + 10; // Inicio del texto + padding
            int cursorY = textBoxY + 5;
            
            if (text_cursor_pos.load() > 0) {
                // Simular el renderizado exacto hasta la posición del cursor
                std::wstring currentText = zoom_text.substr(0, text_cursor_pos.load());
                int currentY = textBoxY + 5;
                int lineHeight = 20;
                size_t pos = 0;
                size_t targetPos = text_cursor_pos.load();
                bool foundCursor = false;
                
                // Procesar el texto línea por línea igual que drawTextSimple
                while (pos < currentText.length() && !foundCursor) {
                    // Buscar el próximo salto de línea
                    size_t nextNewline = currentText.find(L'\n', pos);
                    if (nextNewline == std::wstring::npos) {
                        nextNewline = currentText.length();
                    }
                    
                    // Verificar si el cursor está en esta línea
                    if (targetPos >= pos && targetPos <= nextNewline) {
                        // El cursor está en esta línea
                        foundCursor = true;
                        
                        // Obtener la línea actual
                        std::wstring currentLine = currentText.substr(pos, nextNewline - pos);
                        
                        // Verificar si la línea contiene un marcador de imagen
                        if (currentLine.find(L"[IMAGE_") != std::wstring::npos) {
                            // Esta línea tiene una imagen, obtener su altura
                            size_t imageStart = currentLine.find(L"[IMAGE_");
                            size_t imageEnd = currentLine.find(L"]", imageStart);
                            if (imageStart != std::wstring::npos && imageEnd != std::wstring::npos) {
                                std::wstring imageIndexStr = currentLine.substr(imageStart + 7, imageEnd - imageStart - 7);
                                try {
                                    int imageIndex = std::stoi(imageIndexStr);
                                    if (imageIndex >= 0 && imageIndex < static_cast<int>(clipboard_images.size()) && clipboard_images[imageIndex]) {
                                        HBITMAP hImage = clipboard_images[imageIndex].get();
                                        BITMAP bm;
                                        if (GetObject(hImage, sizeof(BITMAP), &bm) > 0) {
                                            // El cursor debe estar después de la imagen
                                            cursorY = currentY + bm.bmHeight + 5;
                                            cursorX = textBoxX + 10; // Al inicio de la siguiente línea
                                        }
                                    }
                                } catch (...) {
                                    cursorY = currentY + 20;
                                    cursorX = textBoxX + 10;
                                }
                            }
                        } else {
                            // Esta línea es texto normal
                            std::wstring textBeforeCursor = currentLine.substr(0, targetPos - pos);
                            SIZE textSize;
                            GetTextExtentPoint32W(hMemDC, textBeforeCursor.c_str(), textBeforeCursor.length(), &textSize);
                            cursorX = textBoxX + 10 + (int)textSize.cx;
                            cursorY = currentY;
                        }
                    } else {
                        // El cursor no está en esta línea, solo avanzar la posición Y
                        std::wstring currentLine = currentText.substr(pos, nextNewline - pos);
                        
                        if (currentLine.find(L"[IMAGE_") != std::wstring::npos) {
                            // Esta línea tiene una imagen
                            size_t imageStart = currentLine.find(L"[IMAGE_");
                            size_t imageEnd = currentLine.find(L"]", imageStart);
                            if (imageStart != std::wstring::npos && imageEnd != std::wstring::npos) {
                                std::wstring imageIndexStr = currentLine.substr(imageStart + 7, imageEnd - imageStart - 7);
                                try {
                                    int imageIndex = std::stoi(imageIndexStr);
                                    if (imageIndex >= 0 && imageIndex < static_cast<int>(clipboard_images.size()) && clipboard_images[imageIndex]) {
                                        HBITMAP hImage = clipboard_images[imageIndex].get();
                                        BITMAP bm;
                                        if (GetObject(hImage, sizeof(BITMAP), &bm) > 0) {
                                            // IMPORTANTE: Actualizar currentY con la altura real de la imagen
                                            currentY += bm.bmHeight + 5;
                                        } else {
                                            currentY += lineHeight; // Fallback si no se puede obtener altura
                                        }
                                                                            } else {
                                            currentY += lineHeight; // Fallback si no se encuentra la imagen
                                        }
                                } catch (...) {
                                    currentY += lineHeight; // Fallback si hay error
                                }
                            } else {
                                currentY += lineHeight; // Fallback si no se puede parsear
                            }
                        } else {
                            // Línea de texto normal
                            currentY += lineHeight;
                        }
                    }
                    
                    // Mover a la siguiente línea
                    pos = nextNewline + 1;
                }
            }
            
            // Dibujar cursor vertical simple en la posición correcta
            HPEN hCursorPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            HPEN hOldPen = (HPEN)SelectObject(hMemDC, hCursorPen);
            MoveToEx(hMemDC, cursorX, cursorY, NULL);
            LineTo(hMemDC, cursorX, cursorY + 20); // Altura fija de 20px por línea
            SelectObject(hMemDC, hOldPen);
            DeleteObject(hCursorPen);
        }
        
        // Restaurar fuente (no es necesario con cache estático)
        // SelectObject(hMemDC, hOldCachedFont);
        // DeleteObject(hCachedFont);
    }
    
    DeleteObject(hOverlayBrush);
    
    // Dibujar bordes verdes (solo si hay regiones)
    if (!screenRectangles.empty() || (selection_mode.load() && start_x.load() != -1)) {
        // Usar el grosor del borde configurado por el usuario
        int borderThickness = region_border_thickness.load();
        COLORREF borderColor = region_border_color.load();
        ScopedPen hPen(CreatePen(PS_SOLID, borderThickness, borderColor));
        if (!hPen) return; // Verificar que se creó correctamente
        
        HPEN hOldPen = (HPEN)SelectObject(hMemDC, hPen);
        
        // Función lambda para dibujar rectángulo optimizada
        auto drawRect = [&](int x1, int y1, int x2, int y2) {
            MoveToEx(hMemDC, x1, y1, NULL);
            LineTo(hMemDC, x2, y1);
            LineTo(hMemDC, x2, y2);
            LineTo(hMemDC, x1, y2);
            LineTo(hMemDC, x1, y1);
        };
        
        // Bordes de rectángulos guardados
        if (zoom_active.load() && !screenRectangles.empty()) {
            // Con zoom activo: solo mostrar borde de la última región (con zoom)
            const auto& rect = screenRectangles.back();
            float zoom = zoom_factor.load();
            int originalWidth = rect.x2 - rect.x1;
            int originalHeight = rect.y2 - rect.y1;
            int zoomedWidth = (int)(originalWidth * zoom);
            int zoomedHeight = (int)(originalHeight * zoom);
            
            int centerX = zoom_center_x.load();
            int centerY = zoom_center_y.load();
            int zoomX = centerX - zoomedWidth / 2;
            int zoomY = centerY - zoomedHeight / 2;
            
            drawRect(zoomX, zoomY, zoomX + zoomedWidth, zoomY + zoomedHeight);
        } else {
            // Sin zoom: mostrar bordes de todas las regiones normalmente
            for (const auto& rect : screenRectangles) {
                drawRect(rect.x1, rect.y1, rect.x2, rect.y2);
            }
        }
        
        // Borde del rectángulo en tiempo real
        if (selection_mode.load() && start_x.load() != -1) {
            drawRect(start_x.load(), start_y.load(), end_x.load(), end_y.load());
        }
        
        SelectObject(hMemDC, hOldPen);
        // hPen se limpia automáticamente al salir del scope
    }
    
    // Mostrar indicador de herramienta activa
    if (drawing_active.load()) {
        // Crear fuente para el indicador
        HFONT hIndicatorFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hMemDC, hIndicatorFont);
        
        // Texto del indicador
        std::wstring toolText;
        switch (current_drawing_tool.load()) {
            case DrawingTool::Line: toolText = L"✏️ LINE"; break;
            case DrawingTool::Arrow: toolText = L"🏹 ARROW"; break;
            case DrawingTool::Rectangle: toolText = L"🔲 RECTANGLE"; break;
            // Case 4 (Text) removed
            case DrawingTool::Highlighter: toolText = L"🎨 HIGHLIGHTER"; break;
        }
        
        // Fondo del indicador
        RECT indicatorRect = {20, 20, 200, 50};
        HBRUSH hIndicatorBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hMemDC, &indicatorRect, hIndicatorBrush);
        DeleteObject(hIndicatorBrush);
        
        // Borde del indicador
        HPEN hIndicatorPen = CreatePen(PS_SOLID, 2, drawing_color.load());
        HPEN hOldPen = (HPEN)SelectObject(hMemDC, hIndicatorPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
        Rectangle(hMemDC, indicatorRect.left, indicatorRect.top, indicatorRect.right, indicatorRect.bottom);
        
        // Texto del indicador
        SetTextColor(hMemDC, RGB(255, 255, 255));
        SetBkMode(hMemDC, TRANSPARENT);
        RECT textRect = {25, 25, 195, 45};
        DrawTextW(hMemDC, toolText.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER);
        
        // Restaurar objetos
        SelectObject(hMemDC, hOldFont);
        SelectObject(hMemDC, hOldPen);
        SelectObject(hMemDC, hOldBrush);
        DeleteObject(hIndicatorFont);
        DeleteObject(hIndicatorPen);
    }
    
    // Mostrar indicador del modo captura
    if (screenshot_mode.load()) {
        // Crear fuente para el indicador
        HFONT hIndicatorFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hMemDC, hIndicatorFont);
        
        // Texto del indicador de captura
        std::wstring toolText = L"📸 SCREENSHOT";
        
        // Fondo del indicador
        RECT indicatorRect = {20, 20, 250, 50};
        HBRUSH hIndicatorBrush = CreateSolidBrush(RGB(255, 0, 0));
        FillRect(hMemDC, &indicatorRect, hIndicatorBrush);
        DeleteObject(hIndicatorBrush);
        
        // Borde del indicador
        HPEN hIndicatorPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HPEN hOldPen = (HPEN)SelectObject(hMemDC, hIndicatorPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
        Rectangle(hMemDC, indicatorRect.left, indicatorRect.top, indicatorRect.right, indicatorRect.bottom);
        
        // Texto del indicador
        SetTextColor(hMemDC, RGB(255, 255, 255));
        SetBkMode(hMemDC, TRANSPARENT);
        RECT textRect = {25, 25, 245, 45};
        DrawTextW(hMemDC, toolText.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER);
        
        // Restaurar objetos
        SelectObject(hMemDC, hOldFont);
        SelectObject(hMemDC, hOldPen);
        SelectObject(hMemDC, hOldBrush);
        DeleteObject(hIndicatorFont);
        DeleteObject(hIndicatorPen);
        
        // Mostrar instrucciones
        std::wstring instructionText = L"ESC = Exit | Click + Drag = Select area | Release to capture";
        RECT instructionRect = {20, 55, 400, 75};
        SetTextColor(hMemDC, RGB(255, 255, 0));
        DrawTextW(hMemDC, instructionText.c_str(), -1, &instructionRect, DT_LEFT | DT_TOP);
    }
    

    
    // Dibujar preview del modo captura en tiempo real
    if (screenshot_mode.load() && screenshot_start_x.load() != -1) {
        // Dibujar rectángulo de selección para captura
        int x1 = std::min(screenshot_start_x.load(), screenshot_end_x.load());
        int y1 = std::min(screenshot_start_y.load(), screenshot_end_y.load());
        int x2 = std::max(screenshot_start_x.load(), screenshot_end_x.load());
        int y2 = std::max(screenshot_start_y.load(), screenshot_end_y.load());
        
        // Dibujar rectángulo con líneas cortadas blancas finas (estilo Paint)
        HPEN hDashedPen = CreatePen(PS_DASH, 1, RGB(255, 255, 255));
        HPEN hOldPen = (HPEN)SelectObject(hMemDC, hDashedPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
        
        // Dibujar las cuatro líneas del rectángulo por separado para mejor control
        // Línea superior
        MoveToEx(hMemDC, x1, y1, NULL);
        LineTo(hMemDC, x2, y1);
        
        // Línea derecha
        MoveToEx(hMemDC, x2, y1, NULL);
        LineTo(hMemDC, x2, y2);
        
        // Línea inferior
        MoveToEx(hMemDC, x2, y2, NULL);
        LineTo(hMemDC, x1, y2);
        
        // Línea izquierda
        MoveToEx(hMemDC, x1, y2, NULL);
        LineTo(hMemDC, x1, y1);
        
        // Restaurar objetos
        SelectObject(hMemDC, hOldPen);
        SelectObject(hMemDC, hOldBrush);
        DeleteObject(hDashedPen);
        
        // Mostrar texto indicativo
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hMemDC, hFont);
        
        SetTextColor(hMemDC, RGB(255, 255, 255)); // Texto blanco para mejor visibilidad
        SetBkMode(hMemDC, TRANSPARENT);
        
        std::wstring captionText = L"📸 SCREENSHOT - Release click to capture";
        RECT captionRect = {x1, y1 - 25, x2, y1};
        DrawTextW(hMemDC, captionText.c_str(), -1, &captionRect, DT_CENTER | DT_VCENTER);
        
        SelectObject(hMemDC, hOldFont);
        DeleteObject(hFont);
    }
    
    // Dibujar preview de dibujo en tiempo real
    if (drawing_active.load() && drawing_start_x.load() != -1) {
        // Para la flecha, mantener las coordenadas originales sin intercambiar
        // Para otras herramientas, usar min/max para crear rectángulos
        int x1, y1, x2, y2;
        
        if (current_drawing_tool.load() == DrawingTool::Line || current_drawing_tool.load() == DrawingTool::Arrow) { // Línea o Flecha
            // Mantener coordenadas originales: inicio fijo, final móvil
            x1 = drawing_start_x.load();
            y1 = drawing_start_y.load();
            x2 = end_x.load();
            y2 = end_y.load();
        } else {
            // Para otras herramientas, usar min/max para crear rectángulos
            x1 = std::min(drawing_start_x.load(), end_x.load());
            y1 = std::min(drawing_start_y.load(), end_y.load());
            x2 = std::max(drawing_start_x.load(), end_x.load());
            y2 = std::max(drawing_start_y.load(), end_y.load());
        }
        
        // Declarar variables para el preview de texto (comentadas por no utilizadas)
        // RECT textPreviewRect;
        // HBRUSH hWhiteBrush;
        // HPEN hDottedPen;
        // HPEN hOldPen;
        // HBRUSH hOldBrush;
        // HFONT hPreviewFont;
        // HFONT hOldFont;
        // RECT previewTextRect;
        
        switch (current_drawing_tool.load()) {
            case DrawingTool::Line: // Línea
                DrawLine(hMemDC, x1, y1, x2, y2, drawing_color.load(), drawing_thickness.load());
                break;
            case DrawingTool::Arrow: // Flecha
                DrawArrow(hMemDC, x1, y1, x2, y2, drawing_color.load(), drawing_thickness.load());
                break;
            case DrawingTool::Rectangle: // Rectángulo
                DrawRectangle(hMemDC, x1, y1, x2, y2, drawing_color.load(), drawing_thickness.load(), drawing_fill.load());
                break;
            // Case 4 (Texto) eliminado
            case DrawingTool::Highlighter: // Resaltador
                DrawHighlighter(hMemDC, x1, y1, x2, y2, drawing_color.load());
                break;
        }
    }
    
    // Dibujar todos los elementos de dibujo
    for (const auto& element : drawing_elements) {
        switch (element.tool_type) {
            case DrawingTool::Line: // Línea
                DrawLine(hMemDC, element.x1, element.y1, element.x2, element.y2, element.color, element.thickness);
                break;
            case DrawingTool::Arrow: // Flecha
                DrawArrow(hMemDC, element.x1, element.y1, element.x2, element.y2, element.color, element.thickness);
                break;
            case DrawingTool::Rectangle: // Rectángulo
                DrawRectangle(hMemDC, element.x1, element.y1, element.x2, element.y2, element.color, element.thickness, element.filled);
                break;
            // Case 4 (Texto) eliminado
            case DrawingTool::Highlighter: // Resaltador
                DrawHighlighter(hMemDC, element.x1, element.y1, element.x2, element.y2, element.color);
                break;
        }
    }
    
    // Copiar el resultado al DC principal (doble buffering)
    BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);
    
    // Restaurar bitmap anterior
    SelectObject(hMemDC, hOldBitmap);
    // Los recursos se limpian automáticamente al salir del scope
}

// Función para dibujar la ventana de configuración moderna
void DrawSettingsWindow(HWND hwnd, HDC hdc) {
    // Obtener dimensiones del área cliente
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    
    // Obtener posición actual del scroll
    int scrollY = scroll_pos.load();
    
    // Crear DC de memoria para doble buffering
    ScopedDC hMemDC(CreateCompatibleDC(hdc), true);
    if (!hMemDC) return; // Verificar que se creó correctamente
    
    ScopedBitmap hBitmap(CreateCompatibleBitmap(hdc, width, height));
    if (!hBitmap) return; // Verificar que se creó correctamente
    
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    
    // Fondo negro puro (sin transparencia)
    ScopedBrush hBlackBrush(CreateSolidBrush(RGB(0, 0, 0)), true);
    if (!hBlackBrush) return; // Verificar que se creó correctamente
    
    RECT fullRect = {0, 0, width, height};
    FillRect(hMemDC, &fullRect, hBlackBrush);
    
    // Crear fuentes modernas
    ScopedFont hTitleFont(CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"));
    if (!hTitleFont) return; // Verificar que se creó correctamente
    ScopedFont hSectionFont(CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"));
    if (!hSectionFont) return; // Verificar que se creó correctamente
    ScopedFont hSubsectionFont(CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"));
    if (!hSubsectionFont) return; // Verificar que se creó correctamente
    ScopedFont hOptionFont(CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"));
    if (!hOptionFont) return; // Verificar que se creó correctamente
    
    HFONT hOldFont = (HFONT)SelectObject(hMemDC, hTitleFont);
    SetBkMode(hMemDC, TRANSPARENT);
    
    // HEADER PRINCIPAL con gradiente visual
    int headerY = 30 - scrollY;
    
    // Título principal con efecto de sombra
    SetTextColor(hMemDC, RGB(255, 255, 255)); // Blanco puro
    RECT titleRect = {40, headerY, width - 40, headerY + 50};
    DrawTextW(hMemDC, L"⚙️ SETTINGS", -1, &titleRect, DT_CENTER | DT_TOP);
    
    // Subtítulo
    SelectObject(hMemDC, hSubsectionFont);
    SetTextColor(hMemDC, RGB(100, 150, 255)); // Azul moderno
    RECT subtitleRect = {40, headerY + 45, width - 40, headerY + 70};
    DrawTextW(hMemDC, L"Screen Highlighter Pro", -1, &subtitleRect, DT_CENTER | DT_TOP);
    
    // Línea separadora moderna
    ScopedPen hModernPen(CreatePen(PS_SOLID, 2, RGB(50, 50, 50)));
    if (!hModernPen) return; // Verificar que se creó correctamente
    
    HPEN hOldPen = (HPEN)SelectObject(hMemDC, hModernPen);
    MoveToEx(hMemDC, 60, headerY + 85, NULL);
    LineTo(hMemDC, width - 60, headerY + 85);
    
    int currentY = headerY + 120;
    int sectionSpacing = 60;
    int optionSpacing = 35;
    
    // 🔑 SECCIÓN HOTKEYS
    SelectObject(hMemDC, hSectionFont);
    SetTextColor(hMemDC, RGB(255, 215, 0)); // Dorado
    RECT hotkeyRect = {50, currentY, width - 50, currentY + 35};
    DrawTextW(hMemDC, L"🔑 KEYBOARD SHORTCUTS", -1, &hotkeyRect, DT_LEFT | DT_TOP);
    
    currentY += sectionSpacing;
    SelectObject(hMemDC, hOptionFont);
    SetTextColor(hMemDC, RGB(220, 220, 220)); // Gris claro
    
    // Hotkey 1 con icono y descripción
    RECT hotkey1Rect = {70, currentY, width - 50, currentY + 25};
    DrawTextW(hMemDC, L"⌨️ Shift + Alt + X", -1, &hotkey1Rect, DT_LEFT | DT_TOP);
    currentY += 25;
    RECT hotkey1DescRect = {90, currentY, width - 50, currentY + 20};
    SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
            DrawTextW(hMemDC, L"Activate screen selection mode", -1, &hotkey1DescRect, DT_LEFT | DT_TOP);
    currentY += optionSpacing;
    
    // Herramientas de dibujo
    RECT drawingRect = {70, currentY, width - 50, currentY + 25};
            DrawTextW(hMemDC, L"🎨 DRAWING TOOLS:", -1, &drawingRect, DT_LEFT | DT_TOP);
    currentY += 25;
    
            // Herramientas
        RECT toolsRect = {90, currentY, width - 50, currentY + 20};
        SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
        DrawTextW(hMemDC, L"F1 = Line | F2 = Arrow | F3 = Rectangle | F4 = Highlighter", -1, &toolsRect, DT_LEFT | DT_TOP);
        currentY += 25;
        RECT toolsDescRect = {90, currentY, width - 50, currentY + 20};
        DrawTextW(hMemDC, L"Ctrl+Z = Undo | ESC = Exit drawing mode", -1, &toolsDescRect, DT_LEFT | DT_TOP);
        currentY += 25;
        
        // Captura de pantalla
        RECT screenshotRect = {90, currentY, width - 50, currentY + 20};
        SetTextColor(hMemDC, RGB(255, 150, 150)); // Rosa para destacar
        DrawTextW(hMemDC, L"📸 Ctrl+Enter = Screenshot mode", -1, &screenshotRect, DT_LEFT | DT_TOP);
        currentY += 25;
        RECT screenshotDescRect = {90, currentY, width - 50, currentY + 20};
        SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
        DrawTextW(hMemDC, L"Click + drag to select area, release to capture to clipboard", -1, &screenshotDescRect, DT_LEFT | DT_TOP);
    currentY += optionSpacing;
    

    
    // 🎨 SECCIÓN VISUAL
    currentY += 20;
    SelectObject(hMemDC, hSectionFont);
    SetTextColor(hMemDC, RGB(255, 105, 180)); // Rosa moderno
    RECT visualRect = {50, currentY, width - 50, currentY + 35};
    DrawTextW(hMemDC, L"🎨 VISUAL CUSTOMIZATION", -1, &visualRect, DT_LEFT | DT_TOP);
    
    currentY += sectionSpacing;
    SelectObject(hMemDC, hOptionFont);
    SetTextColor(hMemDC, RGB(220, 220, 220)); // Gris claro
    
    // Opacidad con descripción y controles integrados
    RECT opacityRect = {70, currentY, width - 50, currentY + 25};
    DrawTextW(hMemDC, L"🎭 Overlay Opacity", -1, &opacityRect, DT_LEFT | DT_TOP);
    currentY += 25;
    RECT opacityDescRect = {90, currentY, width - 50, currentY + 20};
    SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
            std::wstring opacityText = L"Controls overlay transparency: " + std::to_wstring(overlay_opacity.load()) + L"/255";
    DrawTextW(hMemDC, opacityText.c_str(), -1, &opacityDescRect, DT_LEFT | DT_TOP);
    
    // Layout de dos columnas: texto a la izquierda, botones a la derecha
    // Texto descriptivo (columna izquierda) - en línea separada
    currentY += 25; // Espacio extra para separar descripción de valor actual
    SetTextColor(hMemDC, RGB(255, 255, 255)); // Blanco para controles
    RECT opacityValueRect = {90, currentY, width - 200, currentY + 25};
            std::wstring opacityValue = L"Current value: " + std::to_wstring(overlay_opacity.load()) + L"/255";
    DrawTextW(hMemDC, opacityValue.c_str(), -1, &opacityValueRect, DT_LEFT | DT_TOP);
    
    // Botones a la derecha (columna derecha) - alineados con el valor actual
    HWND hOpacityMinus = GetDlgItem(hwnd, 2001);
    HWND hOpacityPlus = GetDlgItem(hwnd, 2002);
    if (hOpacityMinus) SetWindowPos(hOpacityMinus, NULL, width - 150, currentY, 30, 25, SWP_NOZORDER);
    if (hOpacityPlus) SetWindowPos(hOpacityPlus, NULL, width - 110, currentY, 30, 25, SWP_NOZORDER);
    
    currentY += optionSpacing;
    
    // 🔍 SECCIÓN ZOOM
    currentY += 20;
    SelectObject(hMemDC, hSectionFont);
    SetTextColor(hMemDC, RGB(0, 255, 127)); // Verde primavera
    RECT zoomRect = {50, currentY, width - 50, currentY + 35};
    DrawTextW(hMemDC, L"🔍 ZOOM CONFIGURATION", -1, &zoomRect, DT_LEFT | DT_TOP);
    
    currentY += sectionSpacing;
    SelectObject(hMemDC, hOptionFont);
    SetTextColor(hMemDC, RGB(220, 220, 220)); // Gris claro
    
    // Zoom mínimo con controles integrados
    RECT zoomMinRect = {70, currentY, width - 50, currentY + 25};
            DrawTextW(hMemDC, L"📏 Minimum Zoom", -1, &zoomMinRect, DT_LEFT | DT_TOP);
    currentY += 25;
    RECT zoomMinDescRect = {90, currentY, width - 50, currentY + 20};
    SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
            std::wstring zoomMinText = L"Lowest allowed zoom factor: " + std::to_wstring(zoom_min_factor.load() / 100.0f) + L"x";
    DrawTextW(hMemDC, zoomMinText.c_str(), -1, &zoomMinDescRect, DT_LEFT | DT_TOP);
    
    // Layout de dos columnas: texto a la izquierda, botones a la derecha
    // Texto descriptivo (columna izquierda) - en línea separada
    currentY += 25; // Espacio extra para separar descripción de valor actual
    SetTextColor(hMemDC, RGB(255, 255, 255)); // Blanco para controles
    RECT zoomMinValueRect = {90, currentY, width - 200, currentY + 25};
    std::wstring zoomMinValue = std::to_wstring(zoom_min_factor.load() / 100.0f);
    zoomMinValue = zoomMinValue.substr(0, zoomMinValue.find('.') + 2);
            std::wstring zoomMinValueText = L"Current value: " + zoomMinValue + L"x";
    DrawTextW(hMemDC, zoomMinValueText.c_str(), -1, &zoomMinValueRect, DT_LEFT | DT_TOP);
    
    // Botones a la derecha (columna derecha) - alineados con el valor actual
    HWND hZoomMinMinus = GetDlgItem(hwnd, 2003);
    HWND hZoomMinPlus = GetDlgItem(hwnd, 2004);
    if (hZoomMinMinus) SetWindowPos(hZoomMinMinus, NULL, width - 150, currentY, 30, 25, SWP_NOZORDER);
    if (hZoomMinPlus) SetWindowPos(hZoomMinPlus, NULL, width - 110, currentY, 30, 25, SWP_NOZORDER);
    
    currentY += optionSpacing;
    
    // Zoom máximo con controles integrados
    SetTextColor(hMemDC, RGB(220, 220, 220)); // Gris claro
    RECT zoomMaxRect = {70, currentY, width - 50, currentY + 25};
            DrawTextW(hMemDC, L"📏 Maximum Zoom", -1, &zoomMaxRect, DT_LEFT | DT_TOP);
    currentY += 25;
    RECT zoomMaxDescRect = {90, currentY, width - 50, currentY + 20};
    SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
            std::wstring zoomMaxText = L"Highest allowed zoom factor: " + std::to_wstring(zoom_max_factor.load() / 100.0f) + L"x";
    DrawTextW(hMemDC, zoomMaxText.c_str(), -1, &zoomMaxDescRect, DT_LEFT | DT_TOP);
    
    // Layout de dos columnas: texto a la izquierda, botones a la derecha
    // Texto descriptivo (columna izquierda) - en línea separada
    currentY += 25; // Espacio extra para separar descripción de valor actual
    SetTextColor(hMemDC, RGB(255, 255, 255)); // Blanco para controles
    RECT zoomMaxValueRect = {90, currentY, width - 200, currentY + 25};
    std::wstring zoomMaxValue = std::to_wstring(zoom_max_factor.load() / 100.0f);
    zoomMaxValue = zoomMaxValue.substr(0, zoomMaxValue.find('.') + 2);
            std::wstring zoomMaxValueText = L"Current value: " + zoomMaxValue + L"x";
    DrawTextW(hMemDC, zoomMaxValueText.c_str(), -1, &zoomMaxValueRect, DT_LEFT | DT_TOP);
    
    // Botones a la derecha (columna derecha) - alineados con el valor actual
    HWND hZoomMaxMinus = GetDlgItem(hwnd, 2005);
    HWND hZoomMaxPlus = GetDlgItem(hwnd, 2006);
    if (hZoomMaxMinus) SetWindowPos(hZoomMaxMinus, NULL, width - 150, currentY, 30, 25, SWP_NOZORDER);
    if (hZoomMaxPlus) SetWindowPos(hZoomMaxPlus, NULL, width - 110, currentY, 30, 25, SWP_NOZORDER);
    
    currentY += optionSpacing;
    
    // 📝 SECCIÓN TEXTO
    currentY += 20;
    SelectObject(hMemDC, hSectionFont);
    SetTextColor(hMemDC, RGB(255, 165, 0)); // Naranja
    RECT textRect = {50, currentY, width - 50, currentY + 35};
    DrawTextW(hMemDC, L"📝 TEXT CONFIGURATION", -1, &textRect, DT_LEFT | DT_TOP);
    
    currentY += sectionSpacing;
    SelectObject(hMemDC, hOptionFont);
    SetTextColor(hMemDC, RGB(220, 220, 220)); // Gris claro
    
    // Velocidad del cursor con controles integrados
    RECT cursorRect = {70, currentY, width - 50, currentY + 25};
            DrawTextW(hMemDC, L"⏱️ Cursor Speed", -1, &cursorRect, DT_LEFT | DT_TOP);
    currentY += 25;
    RECT cursorDescRect = {90, currentY, width - 50, currentY + 20};
    SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
            std::wstring cursorText = L"Controls cursor blink speed: " + std::to_wstring(text_cursor_blink_speed.load()) + L"ms";
    DrawTextW(hMemDC, cursorText.c_str(), -1, &cursorDescRect, DT_LEFT | DT_TOP);
    
    // Layout de dos columnas: texto a la izquierda, botones a la derecha
    // Texto descriptivo (columna izquierda) - en línea separada
    currentY += 25; // Espacio extra para separar descripción de valor actual
    SetTextColor(hMemDC, RGB(255, 255, 255)); // Blanco para controles
    RECT cursorValueRect = {90, currentY, width - 200, currentY + 25};
            std::wstring cursorValueText = L"Current value: " + std::to_wstring(text_cursor_blink_speed.load()) + L"ms";
    DrawTextW(hMemDC, cursorValueText.c_str(), -1, &cursorValueRect, DT_LEFT | DT_TOP);
    
    // Botones a la derecha (columna derecha) - alineados con el valor actual
    HWND hCursorMinus = GetDlgItem(hwnd, 2007);
    HWND hCursorPlus = GetDlgItem(hwnd, 2008);
    if (hCursorMinus) SetWindowPos(hCursorMinus, NULL, width - 150, currentY, 30, 25, SWP_NOZORDER);
    if (hCursorPlus) SetWindowPos(hCursorPlus, NULL, width - 110, currentY, 30, 25, SWP_NOZORDER);
    
    currentY += optionSpacing;
    
    // 🔲 SECCIÓN REGIONES
    currentY += 20;
    SelectObject(hMemDC, hSectionFont);
    SetTextColor(hMemDC, RGB(138, 43, 226)); // Violeta
    RECT regionRect = {50, currentY, width - 50, currentY + 35};
    DrawTextW(hMemDC, L"🔲 REGION CONFIGURATION", -1, &regionRect, DT_LEFT | DT_TOP);
    
    currentY += sectionSpacing;
    SelectObject(hMemDC, hOptionFont);
    SetTextColor(hMemDC, RGB(220, 220, 220)); // Gris claro
    
    // Grosor del borde con controles integrados
    RECT borderRect = {70, currentY, width - 50, currentY + 25};
            DrawTextW(hMemDC, L"📐 Border Thickness", -1, &borderRect, DT_LEFT | DT_TOP);
    currentY += 25;
    RECT borderDescRect = {90, currentY, width - 50, currentY + 20};
    SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
            std::wstring borderText = L"Defines border thickness: " + std::to_wstring(region_border_thickness.load()) + L" pixels";
    DrawTextW(hMemDC, borderText.c_str(), -1, &borderDescRect, DT_LEFT | DT_TOP);
    
    // Layout de dos columnas: texto a la izquierda, botones a la derecha
    // Texto descriptivo (columna izquierda) - en línea separada
    currentY += 25; // Espacio extra para separar descripción de valor actual
    SetTextColor(hMemDC, RGB(255, 255, 255)); // Blanco para controles
    RECT borderValueRect = {90, currentY, width - 200, currentY + 25};
            std::wstring borderValueText = L"Current value: " + std::to_wstring(region_border_thickness.load()) + L" pixels";
    DrawTextW(hMemDC, borderValueText.c_str(), -1, &borderValueRect, DT_LEFT | DT_TOP);
    
    // Botones a la derecha (columna derecha) - alineados con el valor actual
    HWND hBorderMinus = GetDlgItem(hwnd, 2009);
    HWND hBorderPlus = GetDlgItem(hwnd, 2010);
    if (hBorderMinus) SetWindowPos(hBorderMinus, NULL, width - 150, currentY, 30, 25, SWP_NOZORDER);
    if (hBorderPlus) SetWindowPos(hBorderPlus, NULL, width - 110, currentY, 30, 25, SWP_NOZORDER);
    
    currentY += optionSpacing;
    
    // Color del borde con selección visual
    SetTextColor(hMemDC, RGB(220, 220, 220)); // Gris claro
    RECT colorRect = {70, currentY, width - 50, currentY + 25};
            DrawTextW(hMemDC, L"🎨 Border Color", -1, &colorRect, DT_LEFT | DT_TOP);
    currentY += 25;
    RECT colorDescRect = {90, currentY, width - 50, currentY + 20};
    SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
            DrawTextW(hMemDC, L"Select the color for region borders:", -1, &colorDescRect, DT_LEFT | DT_TOP);
    currentY += 25;
    
    // Botones de colores predefinidos
    int colorButtonSize = 35;
    int colorButtonSpacing = 10;
    int colorButtonsStartX = 90;
    int colorButtonsY = currentY;
    
    // Definir colores predefinidos (comentados por no utilizados)
    // COLORREF predefinedColors[] = {
    //     RGB(0, 255, 68),    // Verde (#00FF44) - actual
    //     RGB(255, 0, 0),     // Rojo
    //     RGB(0, 0, 255),     // Azul
    //     RGB(255, 255, 0),   // Amarillo
    //     RGB(255, 0, 255),   // Magenta
    //     RGB(0, 255, 255),   // Cian
    //     RGB(255, 165, 0),   // Naranja
    //     RGB(128, 0, 128),   // Púrpura
    //     RGB(255, 255, 255), // Blanco
    //     RGB(255, 215, 0)    // Dorado
    // };
    
    // Posicionar botones de colores reales
    for (int i = 0; i < 10; i++) {
        int buttonX = colorButtonsStartX + (i * (colorButtonSize + colorButtonSpacing));
        
        // Obtener el botón real y posicionarlo
        HWND hColorButton = GetDlgItem(hwnd, 3000 + i);
        if (hColorButton) {
            SetWindowPos(hColorButton, NULL, buttonX, colorButtonsY, colorButtonSize, colorButtonSize, SWP_NOZORDER);
            
            // Cambiar el color del botón usando WM_CTLCOLORBTN
            // Esto se maneja en el procedimiento de ventana
        }
    }
    
    currentY += colorButtonSize + 15;
    
    // BOTONES DE ACCIÓN
    currentY += 40;
    
    // Línea separadora antes de botones
    MoveToEx(hMemDC, 60, currentY, NULL);
    LineTo(hMemDC, width - 60, currentY);
    currentY += 30;
    
    // Título de la sección de botones
    SetTextColor(hMemDC, RGB(255, 255, 255)); // Blanco
    RECT buttonTitleRect = {50, currentY, width - 50, currentY + 25};
    DrawTextW(hMemDC, L"🎛️ ACTIONS", -1, &buttonTitleRect, DT_CENTER | DT_TOP);
    currentY += 30;
    
    // Posicionar botones de acción - adaptados al tamaño de ventana 700x800
    HWND hApplyButton = GetDlgItem(hwnd, 2020);
    HWND hResetButton = GetDlgItem(hwnd, 2021);
    
    // Botones centrados y adaptados al ancho de la ventana
    int buttonWidth = 150;
    int buttonHeight = 35;
    int buttonSpacing = 50;
    int totalWidth = (buttonWidth * 2) + buttonSpacing;
    int startX = (width - totalWidth) / 2;
    
    if (hApplyButton) SetWindowPos(hApplyButton, NULL, startX, currentY, buttonWidth, buttonHeight, SWP_NOZORDER);
    if (hResetButton) SetWindowPos(hResetButton, NULL, startX + buttonWidth + buttonSpacing, currentY, buttonWidth, buttonHeight, SWP_NOZORDER);
    
    // Mostrar texto descriptivo de los botones
    SetTextColor(hMemDC, RGB(150, 150, 150)); // Gris medio
    RECT applyTextRect = {startX, currentY + buttonHeight + 5, startX + buttonWidth, currentY + buttonHeight + 25};
            DrawTextW(hMemDC, L"💾 Apply", -1, &applyTextRect, DT_CENTER | DT_TOP);
        RECT resetTextRect = {startX + buttonWidth + buttonSpacing, currentY + buttonHeight + 5, startX + buttonWidth + buttonSpacing + buttonWidth, currentY + buttonHeight + 25};
        DrawTextW(hMemDC, L"🔄 Reset", -1, &resetTextRect, DT_CENTER | DT_TOP);
    
    currentY += 60;
    
    // FOOTER con instrucciones modernas
    currentY += 20;
    
    // Línea separadora
    MoveToEx(hMemDC, 60, currentY, NULL);
    LineTo(hMemDC, width - 60, currentY);
    currentY += 30;
    
    // Instrucciones con iconos modernos
    SelectObject(hMemDC, hSubsectionFont);
    SetTextColor(hMemDC, RGB(100, 200, 100)); // Verde suave
    RECT instructionRect = {40, currentY, width - 40, currentY + 30};
    DrawTextW(hMemDC, L"💡 Controls are integrated in each option", -1, &instructionRect, DT_CENTER | DT_TOP);
    
    currentY += 35;
    SetTextColor(hMemDC, RGB(100, 150, 255)); // Azul suave
    RECT instructionRect2 = {40, currentY, width - 40, currentY + 30};
    DrawTextW(hMemDC, L"💾 Use [-] and [+] buttons to adjust each value", -1, &instructionRect2, DT_CENTER | DT_TOP);
    
    // Restaurar objetos
    SelectObject(hMemDC, hOldFont);
    SelectObject(hMemDC, hOldPen);
    // Las fuentes se limpian automáticamente al salir del scope
    
    // Actualizar scroll_max basado en el contenido real - incluir completamente los botones
    scroll_max.store(currentY + 200); // Agregar 200 píxeles de margen para asegurar acceso completo
    
    // Copiar el resultado al DC principal
    BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);
    
    // Restaurar bitmap anterior
    SelectObject(hMemDC, hOldBitmap);
    // Los recursos se limpian automáticamente al salir del scope
}

// Función para capturar la región para zoom (captura única optimizada)
void CaptureZoomRegion(const ScreenRectangle& rect) {
    // Limpiar recursos anteriores (las clases RAII se encargan automáticamente)
    hZoomedBitmap.reset();
    hZoomedDC.reset();
    
    // Capturar la pantalla de la región
    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) return; // Verificar que se obtuvo el DC
    
    // Crear nuevo DC de memoria
    hZoomedDC.reset(CreateCompatibleDC(hScreenDC), true);
    if (!hZoomedDC) {
        ReleaseDC(NULL, hScreenDC);
        return; // Verificar que se creó correctamente
    }
    
    int width = rect.x2 - rect.x1;
    int height = rect.y2 - rect.y1;
    
    // Crear nuevo bitmap
    hZoomedBitmap.reset(CreateCompatibleBitmap(hScreenDC, width, height));
    if (!hZoomedBitmap) {
        ReleaseDC(NULL, hScreenDC);
        return; // Verificar que se creó correctamente
    }
    
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hZoomedDC.get(), hZoomedBitmap.get());
    
    // Capturar la región original de la pantalla
    BOOL captureResult = BitBlt(hZoomedDC.get(), 0, 0, width, height, hScreenDC, rect.x1, rect.y1, SRCCOPY);
    if (!captureResult) {
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"DEBUG: Captura falló - Error: %d\n", GetLastError());
        OutputDebugStringW(errorMsg);
    } else {
        wchar_t successMsg[256];
        swprintf_s(successMsg, L"DEBUG: Captura exitosa - Región: %dx%d en (%d,%d)\n", width, height, rect.x1, rect.y1);
        OutputDebugStringW(successMsg);
    }
    
    // Restaurar el bitmap anterior
    SelectObject(hZoomedDC.get(), hOldBitmap);
    
    ReleaseDC(NULL, hScreenDC);
    
    // Configurar centro de zoom - X centrado, Y 20% arriba del centro
    zoom_center_x.store((rect.x1 + rect.x2) / 2);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int centerY = screenHeight / 2;
    int offsetY = (int)(centerY * 0.2); // 20% arriba del centro
    zoom_center_y.store(centerY - offsetY);
    
    // Inicializar texto vacío para la nueva región
    zoom_text.clear();
    text_cursor_pos.store(0);
    text_input_mode.store(false);
}

// Función para limpiar recursos de zoom
void CleanupZoomResources() {
    // Limpiar recursos de zoom (las clases RAII se encargan automáticamente)
    hZoomedBitmap.reset();
    hZoomedDC.reset();
    
    zoom_active.store(false);
    zoom_factor.store(1.0f);
    
    // Limpiar también el texto y modo de edición
    text_input_mode.store(false);
    zoom_text.clear();
    text_cursor_pos.store(0);
    
    // Limpiar selección de texto
    text_selection_active.store(false);
    text_selection_start.store(-1);
    text_selection_end.store(-1);
    
    // Limpiar marcadores de imagen
    image_markers.clear();
    
    // Limpiar imágenes del clipboard (las clases RAII se encargan automáticamente)
    clipboard_images.clear();
    
    // Limpiar elementos de dibujo
    drawing_elements.clear();
    drawing_active.store(false);
    current_drawing_tool.store(DrawingTool::None);
    drawing_start_x.store(-1);
    drawing_start_y.store(-1);
}

// Función para crear y mostrar el overlay
void ShowOverlay() {
    if (overlay_active.load()) return;
    
    overlay_active.store(true);
    selection_mode.store(true);
    needsRedraw.store(true);
    screenRectangles.clear();
    
    // Inicializar coordenadas de selección con -1 (sin selección)
    start_x.store(-1);
    start_y.store(-1);
    end_x.store(-1);
    end_y.store(-1);
    
    // Obtener dimensiones de la pantalla
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Crear ventana overlay con procedimiento personalizado
    // Usar WS_EX_TRANSPARENT para permitir que los eventos pasen a través de áreas transparentes
    HWND hOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        L"ScreenHighlighterOverlayClass",
        L"Screen Highlighter Overlay",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (!hOverlay) return;
    
    // Asignar handle global para poder cambiar atributos dinámicamente
    hCurrentOverlay = hOverlay;
    
    // Hacer la ventana semitransparente inicialmente
    SetLayeredWindowAttributes(hOverlay, 0, 128, LWA_ALPHA);
    
    // Mostrar la ventana
    ShowWindow(hOverlay, SW_SHOW);
    
    // Obtener DC
    HDC hdc = GetDC(hOverlay);
    
    // Thread para hacer parpadear el cursor del texto
    std::thread cursor_thread([]() {
        while (overlay_active.load()) {
            if (text_input_mode.load()) {
                text_cursor_visible.store(!text_cursor_visible.load());
                            // Solo redibujar si hay texto visible (no redibujar si está vacío)
            if (!zoom_text.empty()) {
                needsRedraw.store(true);
            }
        }
        // Usar la velocidad del cursor configurada por el usuario
            Sleep(text_cursor_blink_speed.load()); // Parpadear según configuración
        }
    });
    cursor_thread.detach();
    

    
    // Bucle principal del overlay
    while (overlay_active.load()) {
        // Solo dibujar si es necesario
        if (needsRedraw.load()) {
            DrawOverlay(hdc, screenWidth, screenHeight);
            needsRedraw.store(false);
        }
        
        // Procesar mensajes
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                overlay_active.store(false);
                break;
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Pausa más larga si no hay cambios
        Sleep(needsRedraw.load() ? 16 : 50);
    }
    
    // Limpiar recursos
    ReleaseDC(hOverlay, hdc);
    DestroyWindow(hOverlay);
    
    // Limpiar variable global
    hCurrentOverlay = NULL;
    
    // Limpiar recursos de zoom al salir del overlay
    CleanupZoomResources();
}

// Declaraciones forward
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateSettingsLabels(HWND hwnd);
void UpdateSliderPositions(HWND hwnd);
void ApplyConfigurationChanges();
void ResetToDefaultSettings();
void LoadConfiguration();
void SaveConfiguration();

// Declaraciones forward para herramientas de dibujo
void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness);
void DrawArrow(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness);
void DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness, bool filled);
void DrawHighlighter(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color);

// Declaraciones forward para captura de pantalla
void PlayScreenshotSound();
void SaveScreenshotToDownloads(HBITMAP hBitmap, int x1, int y1, int x2, int y2);
bool SaveBitmapToFile(HBITMAP hBitmap, const std::wstring& filePath);
void CaptureScreenRegion(int x1, int y1, int x2, int y2);

// Función para mostrar la ventana de configuración
void ShowSettingsOverlay() {
    if (settings_overlay_active.load()) return;
    
    settings_overlay_active.store(true);
    
    // Obtener dimensiones de la pantalla
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Crear ventana de configuración con diseño black amoled y scroll
    HWND hSettingsWindow = CreateWindowExW(
        WS_EX_TOPMOST,
        L"ScreenHighlighterSettingsClass",
        L"⚙️ Screen Highlighter - Settings",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_VSCROLL,
        0, 0, 700, screenHeight,  // Altura temporal, se ajustará después
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (!hSettingsWindow) return;
    
    // Obtener la altura del taskbar para calcular la altura disponible
    RECT taskbarRect;
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    int taskbarHeight = 0;
    if (hTaskbar && GetWindowRect(hTaskbar, &taskbarRect)) {
        taskbarHeight = taskbarRect.bottom - taskbarRect.top;
    }
    
    // Calcular altura disponible (pantalla completa menos taskbar)
    int availableHeight = screenHeight - taskbarHeight;
    
    // Centrar la ventana horizontalmente, alinear al tope verticalmente
    int windowX = (screenWidth - 700) / 2;
    int windowY = 0; // Alinear al tope de la pantalla
    
    // Configurar la ventana con altura completa disponible
    SetWindowPos(hSettingsWindow, HWND_TOPMOST, windowX, windowY, 700, availableHeight, SWP_SHOWWINDOW);
    
    // Configurar procedimiento de ventana para la configuración
    SetWindowLongPtr(hSettingsWindow, GWLP_WNDPROC, (LONG_PTR)SettingsWndProc);
    
    // Bucle principal de configuración
    while (settings_overlay_active.load()) {
        // Procesar mensajes
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT || msg.message == WM_CLOSE) {
                settings_overlay_active.store(false);
                break;
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        Sleep(16); // 60 FPS
    }
    
    // Limpiar recursos
    DestroyWindow(hSettingsWindow);
}

// Función para manejar eventos del mouse en el overlay
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_LBUTTONDOWN:
            if (screenshot_mode.load()) {
                // Modo captura de pantalla
                screenshot_start_x.store(LOWORD(lParam));
                screenshot_start_y.store(HIWORD(lParam));
                screenshot_end_x.store(LOWORD(lParam));
                screenshot_end_y.store(HIWORD(lParam));
                needsRedraw.store(true);
            } else if (drawing_active.load()) {
                // Modo dibujo activo - las regiones no son interactivas
                drawing_start_x.store(LOWORD(lParam));
                drawing_start_y.store(HIWORD(lParam));
                end_x.store(LOWORD(lParam));
                end_y.store(HIWORD(lParam));
                needsRedraw.store(true);
            } else if (selection_mode.load() && !drawing_active.load()) {
                // Solo permitir selección si no hay herramienta de dibujo activa
                start_x.store(LOWORD(lParam));
                start_y.store(HIWORD(lParam));
                end_x.store(start_x.load());
                end_y.store(start_y.load());
                needsRedraw.store(true);
            }
            break;
            
        case WM_MOUSEMOVE:
            if (screenshot_mode.load() && screenshot_start_x.load() != -1) {
                // Modo captura - mostrar preview
                screenshot_end_x.store(LOWORD(lParam));
                screenshot_end_y.store(HIWORD(lParam));
                needsRedraw.store(true);
            } else if (drawing_active.load() && drawing_start_x.load() != -1) {
                // Modo dibujo - mostrar preview
                end_x.store(LOWORD(lParam));
                end_y.store(HIWORD(lParam));
                needsRedraw.store(true);
            } else if (selection_mode.load() && start_x.load() != -1 && !drawing_active.load()) {
                // Solo permitir selección si no hay herramienta de dibujo activa
                end_x.store(LOWORD(lParam));
                end_y.store(HIWORD(lParam));
                needsRedraw.store(true);
            }
            break;
            
        case WM_MOUSEWHEEL:
            // Zoom in/out en la última región seleccionada (captura única optimizada)
            if (!screenRectangles.empty() && !drawing_active.load()) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                float zoomChange = (delta > 0) ? 1.1f : 0.9f; // 10% de cambio por scroll
                
                // Si no hay zoom activo, capturar la región una sola vez
                if (!zoom_active.load()) {
                    CaptureZoomRegion(screenRectangles.back());
                    zoom_active.store(true);
                }
                
                // Aplicar cambio de zoom con límites y centrado inmediato
                float newZoom = zoom_factor.load() * zoomChange;
                if (newZoom >= 0.5f && newZoom <= 5.0f) {
                    // Calcular el centro de la pantalla
                    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                    int screenCenterX = screenWidth / 2;
                    int screenCenterY = screenHeight / 2;
                    
                    // Centrado inmediato: X en el centro, Y 20% arriba del centro
                    zoom_factor.store(newZoom);
                    zoom_center_x.store(screenCenterX);
                    int offsetY = (int)(screenCenterY * 0.2); // 20% arriba del centro
                    zoom_center_y.store(screenCenterY - offsetY);
                    
                    needsRedraw.store(true);
                }
            }
            break;
            
        case WM_LBUTTONUP:
            if (screenshot_mode.load() && screenshot_start_x.load() != -1) {
                // Finalizar captura de pantalla
                int x1 = std::min(screenshot_start_x.load(), screenshot_end_x.load());
                int y1 = std::min(screenshot_start_y.load(), screenshot_end_y.load());
                int x2 = std::max(screenshot_start_x.load(), screenshot_end_x.load());
                int y2 = std::max(screenshot_start_y.load(), screenshot_end_y.load());
                
                // Verificar tamaño mínimo
                if ((x2 - x1) >= 5 && (y2 - y1) >= 5) {
                    // Realizar la captura
                    CaptureScreenRegion(x1, y1, x2, y2);
                }
                
                // Salir del modo captura
                screenshot_mode.store(false);
                screenshot_start_x.store(-1);
                screenshot_start_y.store(-1);
                screenshot_end_x.store(-1);
                screenshot_end_y.store(-1);
                needsRedraw.store(true);
            } else if (drawing_active.load() && drawing_start_x.load() != -1) {
                // Finalizar dibujo
                int x1, y1, x2, y2;
                
                if (current_drawing_tool.load() == DrawingTool::Line || current_drawing_tool.load() == DrawingTool::Arrow) { // Línea o Flecha
                    // Para línea y flecha, mantener coordenadas originales sin intercambiar
                    x1 = drawing_start_x.load();
                    y1 = drawing_start_y.load();
                    x2 = end_x.load();
                    y2 = end_y.load();
                } else {
                    // Para otras herramientas, usar min/max para crear rectángulos
                    x1 = std::min(drawing_start_x.load(), end_x.load());
                    y1 = std::min(drawing_start_y.load(), end_y.load());
                    x2 = std::max(drawing_start_x.load(), end_x.load());
                    y2 = std::max(drawing_start_y.load(), end_y.load());
                }
                
                // Verificar tamaño mínimo
                bool isValidSize = false;
                
                if (current_drawing_tool.load() == DrawingTool::Line || current_drawing_tool.load() == DrawingTool::Arrow) { // Línea o Flecha
                    // Para línea y flecha, verificar distancia mínima entre puntos
                    int distance = (int)sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
                    isValidSize = (distance >= 10); // Distancia mínima de 10 píxeles
                } else {
                    // Para otras herramientas, verificar dimensiones del rectángulo
                    isValidSize = ((x2 - x1) >= 5 && (y2 - y1) >= 5);
                }
                
                if (isValidSize) {
                    // Agregar elemento de dibujo
                    drawing_elements.emplace_back(
                        current_drawing_tool.load(),
                        x1, y1, x2, y2,
                        drawing_color.load(),
                        drawing_thickness.load(),
                        drawing_fill.load()
                    );
                }
                
                // Mantener herramienta activa para dibujar más elementos
                drawing_start_x.store(-1);
                drawing_start_y.store(-1);
                // NO resetear drawing_active ni current_drawing_tool
                // drawing_active.store(false);        // COMENTADO
                // current_drawing_tool.store(0);     // COMENTADO
                needsRedraw.store(true);
            } else if (selection_mode.load() && start_x.load() != -1) {
                // Calcular coordenadas de la región
                int x1 = std::min(start_x.load(), end_x.load());
                int y1 = std::min(start_y.load(), end_y.load());
                int x2 = std::max(start_x.load(), end_x.load());
                int y2 = std::max(start_y.load(), end_y.load());
                
                // Verificar que la región tiene un tamaño mínimo (al menos 5x5 píxeles)
                if ((x2 - x1) >= 5 && (y2 - y1) >= 5) {
                    // Permitir superposición pero ajustar la región para evitar problemas
                    // Estrategia: Crear regiones ligeramente separadas si están muy cerca
                    
                    bool needsAdjustment = false;
                    int adjustedX1 = x1, adjustedY1 = y1, adjustedX2 = x2, adjustedY2 = y2;
                    
                    // Verificar proximidad con regiones existentes (optimizado)
                    for (const auto& rect : screenRectangles) {
                        // Verificación rápida de superposición
                        if (x2 > rect.x1 && x1 < rect.x2 && y2 > rect.y1 && y1 < rect.y2) {
                            // Hay superposición - ajustar con separación mínima
                            constexpr int SEPARATION = 2;
                            
                            // Determinar mejor dirección para ajuste (menos movimiento)
                            int leftGap = rect.x1 - x2;
                            int rightGap = x1 - rect.x2;
                            int topGap = rect.y1 - y2;
                            int bottomGap = y1 - rect.y2;
                            
                            // Encontrar el gap más pequeño (ajuste mínimo)
                            int minGap = std::min({abs(leftGap), abs(rightGap), abs(topGap), abs(bottomGap)});
                            
                            if (abs(leftGap) == minGap && leftGap >= -SEPARATION) {
                                // Mover a la izquierda
                                int width = x2 - x1;
                                adjustedX2 = rect.x1 - SEPARATION;
                                adjustedX1 = adjustedX2 - width;
                                needsAdjustment = true;
                            } else if (abs(rightGap) == minGap && rightGap >= -SEPARATION) {
                                // Mover a la derecha
                                int width = x2 - x1;
                                adjustedX1 = rect.x2 + SEPARATION;
                                adjustedX2 = adjustedX1 + width;
                                needsAdjustment = true;
                            } else if (abs(topGap) == minGap && topGap >= -SEPARATION) {
                                // Mover arriba
                                int height = y2 - y1;
                                adjustedY2 = rect.y1 - SEPARATION;
                                adjustedY1 = adjustedY2 - height;
                                needsAdjustment = true;
                            } else if (abs(bottomGap) == minGap && bottomGap >= -SEPARATION) {
                                // Mover abajo
                                int height = y2 - y1;
                                adjustedY1 = rect.y2 + SEPARATION;
                                adjustedY2 = adjustedY1 + height;
                                needsAdjustment = true;
                            }
                            break; // Solo ajustar por la primera superposición encontrada
                        }
                    }
                    
                    // Agregar la región (original o ajustada)
                    if (needsAdjustment) {
                        screenRectangles.emplace_back(adjustedX1, adjustedY1, adjustedX2, adjustedY2);
                    } else {
                        screenRectangles.emplace_back(x1, y1, x2, y2);
                    }
                }
                
                // Resetear selección (usar -1 para indicar "no hay selección")
                start_x.store(-1);
                start_y.store(-1);
                end_x.store(-1);
                end_y.store(-1);
                needsRedraw.store(true);
            }
            break;
            
        case WM_KEYDOWN:
            // Manejar teclas F1-F5 para herramientas de dibujo (siempre disponibles)
            if (wParam == VK_F1) {
                // F1 - Activar herramienta Línea
                current_drawing_tool.store(DrawingTool::Line);
                drawing_active.store(true);
                needsRedraw.store(true);
                return 0;
            } else if (wParam == VK_F2) {
                // F2 - Activar herramienta Flecha
                current_drawing_tool.store(DrawingTool::Arrow);
                drawing_active.store(true);
                needsRedraw.store(true);
                return 0;
            } else if (wParam == VK_F3) {
                // F3 - Activar herramienta Rectángulo
                current_drawing_tool.store(DrawingTool::Rectangle);
                drawing_active.store(true);
                needsRedraw.store(true);
                return 0;
            // F4 (Texto) eliminado
            } else if (wParam == VK_F4) {
                // F4 - Activar herramienta Resaltador
                current_drawing_tool.store(DrawingTool::Highlighter);
                drawing_active.store(true);
                needsRedraw.store(true);
                return 0;
            } else if (wParam == VK_ESCAPE) {
                if (screenshot_mode.load()) {
                    // Salir del modo captura
                    screenshot_mode.store(false);
                    screenshot_start_x.store(-1);
                    screenshot_start_y.store(-1);
                    screenshot_end_x.store(-1);
                    screenshot_end_y.store(-1);
                    needsRedraw.store(true);
                } else if (drawing_active.load()) {
                    // Salir del modo dibujo
                    drawing_active.store(false);
                    current_drawing_tool.store(DrawingTool::None);
                    drawing_start_x.store(-1);
                    drawing_start_y.store(-1);
                    needsRedraw.store(true);
                } else if (zoom_active.load()) {
                    if (text_input_mode.load()) {
                        // Primer Escape: salir del modo texto
                        text_input_mode.store(false);
                        needsRedraw.store(true);
                    } else {
                        // Segundo Escape: salir del zoom
                        CleanupZoomResources();
                        needsRedraw.store(true);
                    }
                } else {
                    // Tercer Escape: salir del overlay completamente
                    overlay_active.store(false);
                }
            } else if (wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // Ctrl+Z para deshacer
                if (!drawing_elements.empty()) {
                    // Deshacer último elemento de dibujo
                    drawing_elements.pop_back();
                    needsRedraw.store(true);
                } else if (!screenRectangles.empty()) {
                    screenRectangles.pop_back();
                    // Si era la última región con zoom, limpiar recursos
                    if (screenRectangles.empty() || zoom_active.load()) {
                        CleanupZoomResources();
                    }
                    needsRedraw.store(true);
                }
            // Ctrl+Enter para captura de pantalla
            } else if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // Activar modo captura de pantalla
                screenshot_mode.store(true);
                drawing_active.store(false);
                current_drawing_tool.store(DrawingTool::None);
                text_input_mode.store(false);
                needsRedraw.store(true);
                return 0;
            } else if (wParam == 'T' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // Ctrl+T para activar modo texto (con o sin zoom)
                text_input_mode.store(true);
                needsRedraw.store(true);
            } else if (text_input_mode.load()) {
                // Manejo de texto cuando está en modo edición
                // Las teclas F1-F5 también funcionan en modo texto para herramientas de dibujo
                if (wParam == VK_F1) {
                    // F1 - Activar herramienta Línea
                    current_drawing_tool.store(DrawingTool::Line);
                    drawing_active.store(true);
                    text_input_mode.store(false);
                    needsRedraw.store(true);
                    return 0;
                } else if (wParam == VK_F2) {
                    // F2 - Activar herramienta Flecha
                    current_drawing_tool.store(DrawingTool::Arrow);
                    drawing_active.store(true);
                    text_input_mode.store(false);
                    needsRedraw.store(true);
                    return 0;
                } else if (wParam == VK_F3) {
                    // F3 - Activar herramienta Rectángulo
                    current_drawing_tool.store(DrawingTool::Rectangle);
                    drawing_active.store(true);
                    text_input_mode.store(false);
                    needsRedraw.store(true);
                    return 0;
                // F4 (Texto) eliminado
                } else if (wParam == VK_F4) {
                    // F4 - Activar herramienta Resaltador
                    current_drawing_tool.store(DrawingTool::Highlighter);
                    drawing_active.store(true);
                    text_input_mode.store(false);
                    needsRedraw.store(true);
                    return 0;
                }
                
                switch (wParam) {
                    case VK_RETURN:
                        // Enter para salto de línea
                        zoom_text.insert(text_cursor_pos.load(), 1, L'\n');
                        text_cursor_pos.store(text_cursor_pos.load() + 1);
                        
                        // Forzar actualización inmediata del cursor
                        needsRedraw.store(true);
                        
                        // Forzar actualización del cursor parpadeante
                        text_cursor_visible.store(true);
                        break;
                        
                    case VK_BACK:
                        // Backspace para borrar
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Ctrl+Backspace: borrar palabra completa
                            if (!zoom_text.empty() && text_cursor_pos.load() > 0) {
                                int pos = text_cursor_pos.load();
                                int startPos = pos;
                                
                                // Retroceder hasta encontrar el inicio de la palabra
                                while (startPos > 0 && (zoom_text[startPos - 1] == ' ' || zoom_text[startPos - 1] == '\n')) {
                                    startPos--;
                                }
                                while (startPos > 0 && zoom_text[startPos - 1] != ' ' && zoom_text[startPos - 1] != '\n') {
                                    startPos--;
                                }
                                
                                if (startPos < pos) {
                                    zoom_text.erase(startPos, pos - startPos);
                                    text_cursor_pos.store(startPos);
                                    needsRedraw.store(true);
                                }
                            }
                        } else {
                            // Backspace normal: borrar un carácter
                            if (!zoom_text.empty() && text_cursor_pos.load() > 0) {
                                zoom_text.erase(text_cursor_pos.load() - 1, 1);
                                text_cursor_pos.store(text_cursor_pos.load() - 1);
                                needsRedraw.store(true);
                            }
                        }
                        break;
                        
                    case VK_LEFT:
                        // Flecha izquierda
                        if (text_cursor_pos.load() > 0) {
                            if (GetKeyState(VK_SHIFT) & 0x8000) {
                                // Shift + Flecha: seleccionar texto carácter por carácter
                                if (!text_selection_active.load()) {
                                    // Iniciar nueva selección
                                    text_selection_start.store(text_cursor_pos.load());
                                    text_selection_end.store(text_cursor_pos.load() - 1);
                                    text_selection_active.store(true);
                                    // Debug deshabilitado para producción
                                } else {
                                    // Extender selección existente hacia la izquierda
                                text_selection_end.store(text_cursor_pos.load() - 1);
                                    // Debug deshabilitado para producción
                                }
                            } else {
                                // Solo flecha: cancelar selección
                                text_selection_active.store(false);
                                text_selection_start.store(-1);
                                text_selection_end.store(-1);
                            }
                            text_cursor_pos.store(text_cursor_pos.load() - 1);
                            // Debug deshabilitado para producción
                            needsRedraw.store(true);
                        }
                        break;
                        
                    case VK_RIGHT:
                        // Flecha derecha
                        if (text_cursor_pos.load() < static_cast<int>(zoom_text.length())) {
                            if (GetKeyState(VK_SHIFT) & 0x8000) {
                                // Shift + Flecha: seleccionar texto carácter por carácter
                                if (!text_selection_active.load()) {
                                    // Iniciar nueva selección
                                    text_selection_start.store(text_cursor_pos.load());
                                    text_selection_end.store(text_cursor_pos.load() + 1);
                                    text_selection_active.store(true);
                                    // Debug deshabilitado para producción
                                } else {
                                    // Extender selección existente hacia la derecha
                                text_selection_end.store(text_cursor_pos.load() + 1);
                                    // Debug deshabilitado para producción
                                }
                            } else {
                                // Solo flecha: cancelar selección
                                text_selection_active.store(false);
                                text_selection_start.store(-1);
                                text_selection_end.store(-1);
                            }
                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                            // Debug deshabilitado para producción
                            needsRedraw.store(true);
                        }
                        break;
                        
                    case VK_HOME:
                        // Home
                        if (GetKeyState(VK_CONTROL) & 0x8000 && GetKeyState(VK_SHIFT) & 0x8000) {
                            // Ctrl+Shift+Home: seleccionar desde el cursor hasta el inicio del documento
                            if (!text_selection_active.load()) {
                                text_selection_start.store(text_cursor_pos.load());
                                text_selection_active.store(true);
                            }
                            text_selection_end.store(0);
                            text_cursor_pos.store(0);
                        } else if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Ctrl+Home: ir al inicio del documento
                            text_cursor_pos.store(0);
                            text_selection_active.store(false);
                            text_selection_start.store(-1);
                            text_selection_end.store(-1);
                        } else if (GetKeyState(VK_SHIFT) & 0x8000) {
                            // Shift+Home: seleccionar desde el cursor hasta el inicio de la línea
                            if (!text_selection_active.load()) {
                                text_selection_start.store(text_cursor_pos.load());
                                text_selection_active.store(true);
                            }
                            int pos = text_cursor_pos.load();
                            while (pos > 0 && zoom_text[pos - 1] != '\n') {
                                pos--;
                            }
                            text_selection_end.store(pos);
                            text_cursor_pos.store(pos);
                            needsRedraw.store(true);
                        } else {
                            // Solo Home: ir al inicio de la línea actual
                            int pos = text_cursor_pos.load();
                            while (pos > 0 && zoom_text[pos - 1] != '\n') {
                                pos--;
                            }
                            text_cursor_pos.store(pos);
                            text_selection_active.store(false);
                            text_selection_start.store(-1);
                            text_selection_end.store(-1);
                        }
                        needsRedraw.store(true);
                        break;
                        
                    case VK_END:
                        // End
                        if (GetKeyState(VK_CONTROL) & 0x8000 && GetKeyState(VK_SHIFT) & 0x8000) {
                            // Ctrl+Shift+End: seleccionar desde el cursor hasta el final del documento
                            if (!text_selection_active.load()) {
                                text_selection_start.store(text_cursor_pos.load());
                                text_selection_active.store(true);
                            }
                            text_selection_end.store(static_cast<int>(zoom_text.length()));
                            text_cursor_pos.store(static_cast<int>(zoom_text.length()));
                        } else if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Ctrl+End: ir al final del documento
                            text_cursor_pos.store(static_cast<int>(zoom_text.length()));
                            text_selection_active.store(false);
                            text_selection_start.store(-1);
                            text_selection_end.store(-1);
                        } else if (GetKeyState(VK_SHIFT) & 0x8000) {
                            // Shift+End: seleccionar desde el cursor hasta el final de la línea
                            if (!text_selection_active.load()) {
                                text_selection_start.store(text_cursor_pos.load());
                                text_selection_active.store(true);
                            }
                            int pos = text_cursor_pos.load();
                            while (pos < static_cast<int>(zoom_text.length()) && zoom_text[pos] != '\n') {
                                pos++;
                            }
                            text_selection_end.store(pos);
                            text_cursor_pos.store(pos);
                            needsRedraw.store(true);
                        } else {
                            // Solo End: ir al final de la línea actual
                            int pos = text_cursor_pos.load();
                            while (pos < static_cast<int>(zoom_text.length()) && zoom_text[pos] != '\n') {
                                pos++;
                            }
                            text_cursor_pos.store(pos);
                            text_selection_active.store(false);
                            text_selection_start.store(-1);
                            text_selection_end.store(-1);
                        }
                        needsRedraw.store(true);
                        break;
                        
                    case VK_UP:
                        // Flecha arriba
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Ctrl+Up: ir a la línea anterior
                            int pos = text_cursor_pos.load();
                            int currentLineStart = pos;
                            while (currentLineStart > 0 && zoom_text[currentLineStart - 1] != '\n') {
                                currentLineStart--;
                            }
                            if (currentLineStart > 0) {
                                int prevLineStart = currentLineStart - 1;
                                while (prevLineStart > 0 && zoom_text[prevLineStart - 1] != '\n') {
                                    prevLineStart--;
                                }
                                int targetPos = prevLineStart + std::min(pos - currentLineStart, 
                                    (currentLineStart - prevLineStart - 1));
                                text_cursor_pos.store(targetPos);
                                text_selection_active.store(false);
                                text_selection_start.store(-1);
                                text_selection_end.store(-1);
                                needsRedraw.store(true);
                            }
                        } else if (GetKeyState(VK_SHIFT) & 0x8000) {
                            // Shift+Up: seleccionar hacia arriba
                            if (!text_selection_active.load()) {
                                text_selection_start.store(text_cursor_pos.load());
                                text_selection_active.store(true);
                            }
                            // Mover cursor hacia arriba
                            int pos = text_cursor_pos.load();
                            int currentLineStart = pos;
                            while (currentLineStart > 0 && zoom_text[currentLineStart - 1] != '\n') {
                                currentLineStart--;
                            }
                            if (currentLineStart > 0) {
                                int prevLineStart = currentLineStart - 1;
                                while (prevLineStart > 0 && zoom_text[prevLineStart - 1] != '\n') {
                                    prevLineStart--;
                                }
                                int targetPos = prevLineStart + std::min(pos - currentLineStart, 
                                    (currentLineStart - prevLineStart - 1));
                                text_cursor_pos.store(targetPos);
                                text_selection_end.store(targetPos);
                                needsRedraw.store(true);
                            }
                        } else {
                            // Solo Up: ir a la línea anterior
                            int pos = text_cursor_pos.load();
                            int currentLineStart = pos;
                            while (currentLineStart > 0 && zoom_text[currentLineStart - 1] != '\n') {
                                currentLineStart--;
                            }
                            if (currentLineStart > 0) {
                                int prevLineStart = currentLineStart - 1;
                                while (prevLineStart > 0 && zoom_text[prevLineStart - 1] != '\n') {
                                    prevLineStart--;
                                }
                                int targetPos = prevLineStart + std::min(pos - currentLineStart, 
                                    (currentLineStart - prevLineStart - 1));
                                text_cursor_pos.store(targetPos);
                                text_selection_active.store(false);
                                text_selection_start.store(-1);
                                text_selection_end.store(-1);
                                needsRedraw.store(true);
                            }
                        }
                        break;
                        
                    case VK_DOWN:
                        // Flecha abajo
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Ctrl+Down: ir a la línea siguiente
                            int pos = text_cursor_pos.load();
                            int currentLineStart = pos;
                            while (currentLineStart > 0 && zoom_text[currentLineStart - 1] != '\n') {
                                currentLineStart--;
                            }
                            int currentLineEnd = pos;
                            while (currentLineEnd < static_cast<int>(zoom_text.length()) && zoom_text[currentLineEnd] != '\n') {
                                currentLineEnd++;
                            }
                            if (currentLineEnd < static_cast<int>(zoom_text.length())) {
                                int nextLineStart = currentLineEnd + 1;
                                int nextLineEnd = nextLineStart;
                                while (nextLineEnd < static_cast<int>(zoom_text.length()) && zoom_text[nextLineEnd] != '\n') {
                                    nextLineEnd++;
                                }
                                int targetPos = nextLineStart + std::min(pos - currentLineStart, 
                                    (nextLineEnd - nextLineStart));
                                text_cursor_pos.store(targetPos);
                                text_selection_active.store(false);
                                text_selection_start.store(-1);
                                text_selection_end.store(-1);
                                needsRedraw.store(true);
                            }
                        } else if (GetKeyState(VK_SHIFT) & 0x8000) {
                            // Shift+Down: seleccionar hacia abajo
                            if (!text_selection_active.load()) {
                                text_selection_start.store(text_cursor_pos.load());
                                text_selection_active.store(true);
                            }
                            // Mover cursor hacia abajo
                            int pos = text_cursor_pos.load();
                            int currentLineStart = pos;
                            while (currentLineStart > 0 && zoom_text[currentLineStart - 1] != '\n') {
                                currentLineStart--;
                            }
                            int currentLineEnd = pos;
                            while (currentLineEnd < static_cast<int>(zoom_text.length()) && zoom_text[currentLineEnd] != '\n') {
                                currentLineEnd++;
                            }
                            if (currentLineEnd < static_cast<int>(zoom_text.length())) {
                                int nextLineStart = currentLineEnd + 1;
                                int nextLineEnd = nextLineStart;
                                while (nextLineEnd < static_cast<int>(zoom_text.length()) && zoom_text[nextLineEnd] != '\n') {
                                    nextLineEnd++;
                                }
                                int targetPos = nextLineStart + std::min(pos - currentLineStart, 
                                    (nextLineEnd - nextLineStart));
                                text_cursor_pos.store(targetPos);
                                text_selection_end.store(targetPos);
                                needsRedraw.store(true);
                            }
                        } else {
                            // Solo Down: ir a la línea siguiente
                            int pos = text_cursor_pos.load();
                            int currentLineStart = pos;
                            while (currentLineStart > 0 && zoom_text[currentLineStart - 1] != '\n') {
                                currentLineStart--;
                            }
                            int currentLineEnd = pos;
                            while (currentLineEnd < static_cast<int>(zoom_text.length()) && zoom_text[currentLineEnd] != '\n') {
                                currentLineEnd++;
                            }
                            if (currentLineEnd < static_cast<int>(zoom_text.length())) {
                                int nextLineStart = currentLineEnd + 1;
                                int nextLineEnd = nextLineStart;
                                while (nextLineEnd < static_cast<int>(zoom_text.length()) && zoom_text[nextLineEnd] != '\n') {
                                    nextLineEnd++;
                                }
                                int targetPos = nextLineStart + std::min(pos - currentLineStart, 
                                    (nextLineEnd - nextLineStart));
                                text_cursor_pos.store(targetPos);
                                text_selection_end.store(targetPos);
                                needsRedraw.store(true);
                            }
                        }
                        break;
                        
                    case VK_DELETE:
                        // Delete
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Ctrl+Delete: borrar palabra completa hacia adelante
                            if (!zoom_text.empty() && text_cursor_pos.load() < static_cast<int>(zoom_text.length())) {
                                int pos = text_cursor_pos.load();
                                int endPos = pos;
                                
                                // Avanzar hasta encontrar el final de la palabra
                                while (endPos < static_cast<int>(zoom_text.length()) && zoom_text[endPos] != ' ' && zoom_text[endPos] != '\n') {
                                    endPos++;
                                }
                                while (endPos < static_cast<int>(zoom_text.length()) && (zoom_text[endPos] == ' ' || zoom_text[endPos] == '\n')) {
                                    endPos++;
                                }
                                
                                if (endPos > pos) {
                                    zoom_text.erase(pos, endPos - pos);
                                    needsRedraw.store(true);
                                }
                            }
                        } else {
                            // Delete normal: borrar un carácter
                            if (!zoom_text.empty() && text_cursor_pos.load() < static_cast<int>(zoom_text.length())) {
                                zoom_text.erase(text_cursor_pos.load(), 1);
                                needsRedraw.store(true);
                            }
                        }
                        break;
                        

                        
                    case 'C':
                        // Ctrl+C para copiar texto seleccionado o todo el texto
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            if (!zoom_text.empty() && OpenClipboard(hCurrentOverlay)) {
                                EmptyClipboard();
                                
                                std::wstring textToCopy;
                                if (text_selection_active.load()) {
                                    // Copiar solo el texto seleccionado
                                    int start = std::min(text_selection_start.load(), text_selection_end.load());
                                    int end = std::max(text_selection_start.load(), text_selection_end.load());
                                    if (start >= 0 && end < static_cast<int>(zoom_text.length()) && start < end) {
                                        textToCopy = zoom_text.substr(start, end - start);
                                    }
                                } else {
                                    // Copiar todo el texto
                                    textToCopy = zoom_text;
                                }
                                
                                if (!textToCopy.empty()) {
                                    size_t textLength = textToCopy.length();
                                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (textLength + 1) * sizeof(wchar_t));
                                    if (hMem) {
                                        wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
                                        wcscpy_s(pMem, textLength + 1, textToCopy.c_str());
                                        GlobalUnlock(hMem);
                                        SetClipboardData(CF_UNICODETEXT, hMem);
                                    }
                                }
                                CloseClipboard();
                            }
                        } else {
                            // C normal - aplicar conversión a minúsculas
                            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                            wchar_t character = 'c'; // Por defecto minúscula
                            if (shiftPressed) {
                                character = 'C'; // Mayúscula si Shift está presionado
                            }
                            zoom_text.insert(text_cursor_pos.load(), 1, character);
                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                            needsRedraw.store(true);
                        }
                        break;
                        
                    case 'X':
                        // Ctrl+X para cortar texto seleccionado
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            if (text_selection_active.load() && !zoom_text.empty()) {
                                // Copiar texto seleccionado al portapapeles
                                int start = std::min(text_selection_start.load(), text_selection_end.load());
                                int end = std::max(text_selection_start.load(), text_selection_end.load());
                                if (start >= 0 && end < static_cast<int>(zoom_text.length()) && start < end) {
                                    std::wstring textToCut = zoom_text.substr(start, end - start);
                                    
                                    if (OpenClipboard(hCurrentOverlay)) {
                                        EmptyClipboard();
                                        size_t textLength = textToCut.length();
                                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (textLength + 1) * sizeof(wchar_t));
                                        if (hMem) {
                                            wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
                                            wcscpy_s(pMem, textLength + 1, textToCut.c_str());
                                            GlobalUnlock(hMem);
                                            SetClipboardData(CF_UNICODETEXT, hMem);
                                        }
                                        CloseClipboard();
                                        
                                        // Eliminar texto seleccionado
                                        zoom_text.erase(start, end - start);
                                        text_cursor_pos.store(start);
                                        text_selection_active.store(false);
                                        needsRedraw.store(true);
                                    }
                                }
                            }
                        } else {
                            // X normal - aplicar conversión a minúsculas
                            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                            wchar_t character = 'x'; // Por defecto minúscula
                            if (shiftPressed) {
                                character = 'X'; // Mayúscula si Shift está presionado
                            }
                            zoom_text.insert(text_cursor_pos.load(), 1, character);
                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                            needsRedraw.store(true);
                        }
                        break;
                        
                    case 'V':
                        // Ctrl+V para pegar texto o imagen
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                                                    // Primero verificar si hay una imagen en el clipboard
                        if (CheckClipboardForImage()) {
                            // Hay una imagen - intentar capturarla
                            if (AddImageElement()) {
                                needsRedraw.store(true);
                            } else {
                                // Si falla, insertar marcador de error
                                zoom_text.insert(text_cursor_pos.load(), L"[ERROR IMAGEN]");
                                text_cursor_pos.store(text_cursor_pos.load() + 15);
                                needsRedraw.store(true);
                            }
                        } else {
                                // No hay imagen, intentar pegar texto
                                if (OpenClipboard(hCurrentOverlay)) {
                                    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                                    if (hData) {
                                        wchar_t* pszText = (wchar_t*)GlobalLock(hData);
                                        if (pszText) {
                                            std::wstring clipboardText(pszText);
                                            zoom_text.insert(text_cursor_pos.load(), clipboardText);
                                            text_cursor_pos.store(text_cursor_pos.load() + clipboardText.length());
                                            needsRedraw.store(true);
                                            GlobalUnlock(hData);
                                        }
                                    }
                                    CloseClipboard();
                                }
                            }
                        } else {
                            // V normal - aplicar conversión a minúsculas
                            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                            wchar_t character = 'v'; // Por defecto minúscula
                            if (shiftPressed) {
                                character = 'V'; // Mayúscula si Shift está presionado
                            }
                            zoom_text.insert(text_cursor_pos.load(), 1, character);
                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                            needsRedraw.store(true);
                        }
                        break;
                        
                    case 'A':
                        // Ctrl+A para seleccionar todo el texto
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            text_selection_start.store(0);
                            text_selection_end.store(zoom_text.length());
                            text_selection_active.store(true);
                            text_cursor_pos.store(zoom_text.length());
                            needsRedraw.store(true);
                        } else {
                            // A normal - aplicar conversión a minúsculas
                            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                            wchar_t character = 'a'; // Por defecto minúscula
                            if (shiftPressed) {
                                character = 'A'; // Mayúscula si Shift está presionado
                            }
                            zoom_text.insert(text_cursor_pos.load(), 1, character);
                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                            needsRedraw.store(true);
                        }
                        break;
                        
                    case 'Z':
                        // Ctrl+Z para deshacer (simplificado)
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Por ahora solo limpia la selección
                            text_selection_active.store(false);
                            text_selection_start.store(-1);
                            text_selection_end.store(-1);
                            needsRedraw.store(true);
                        } else {
                            // Z normal - aplicar conversión a minúsculas
                            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                            wchar_t character = 'z'; // Por defecto minúscula
                            if (shiftPressed) {
                                character = 'Z'; // Mayúscula si Shift está presionado
                            }
                            zoom_text.insert(text_cursor_pos.load(), 1, character);
                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                            needsRedraw.store(true);
                        }
                        break;
                        

                        
                    default:
                        // Caracteres imprimibles básicos (ASCII) y especiales
                        if (wParam >= 32) {
                            
                            // Detectar si Shift está presionado
                            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                            
                            // Obtener el carácter real del teclado
                            wchar_t character = (wchar_t)wParam;
                            
                            // Mapeo de caracteres especiales comunes
                            switch (wParam) {
                                case 186: // ; : (tecla punto y coma)
                                    character = shiftPressed ? L':' : L';';
                                    break;
                                case 188: // , < (tecla coma)
                                    character = shiftPressed ? L'<' : L',';
                                    break;
                                case 190: // . > (tecla punto)
                                    character = shiftPressed ? L'>' : L'.';
                                    break;
                                case 191: // / ? (tecla barra)
                                    character = shiftPressed ? L'?' : L'/';
                                    break;
                                case 192: // ` ~ (tecla acento grave)
                                    character = shiftPressed ? L'~' : L'`';
                                    break;
                                case 219: // [ { (tecla corchete)
                                    character = shiftPressed ? L'{' : L'[';
                                    break;
                                case 220: // \ | (tecla barra invertida)
                                    character = shiftPressed ? L'|' : L'\\';
                                    break;
                                case 221: // ] } (tecla corchete)
                                    character = shiftPressed ? L'}' : L']';
                                    break;
                                case 222: // ' " (tecla apóstrofe)
                                    character = shiftPressed ? L'"' : L'\'';
                                    break;
                                case 189: // - _ (tecla guión)
                                    character = shiftPressed ? L'_' : L'-';
                                    break;
                                case 187: // = + (tecla igual)
                                    character = shiftPressed ? L'+' : L'=';
                                    break;
                                default:
                                    // Para otros caracteres, usar el valor directo
                                    if (!shiftPressed && character >= L'A' && character <= L'Z') {
                                        character = character + 32; // Convertir a minúscula
                                    }
                                    break;
                            }
                            
                            zoom_text.insert(text_cursor_pos.load(), 1, character);
                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                            needsRedraw.store(true);
                        }
                        // Caracteres internacionales y UTF-8
                        else if (wParam >= 128) {
                            // Insertar caracteres Unicode/UTF-8 directamente
                            wchar_t character = (wchar_t)wParam;
                            
                            // Aplicar conversión a minúsculas
                            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                            if (!shiftPressed) {
                                // Convertir caracteres Unicode/UTF-8 a minúsculas cuando sea posible
                                if (character >= L'A' && character <= L'Z') {
                                    character = character + 32; // Convertir a minúscula
                                } else {
                                    // Convertir caracteres españoles comunes a minúsculas
                                    switch (character) {
                                        case L'Á': character = L'á'; break;
                                        case L'É': character = L'é'; break;
                                        case L'Í': character = L'í'; break;
                                        case L'Ó': character = L'ó'; break;
                                        case L'Ú': character = L'ú'; break;
                                        case L'Ñ': character = L'ñ'; break;
                                        case L'Ü': character = L'ü'; break;
                                    }
                                }
                            }
                            
                            zoom_text.insert(text_cursor_pos.load(), 1, character);
                            text_cursor_pos.store(text_cursor_pos.load() + 1);
                            needsRedraw.store(true);
                        }
                        break;
                }
            }
            break;
            
        case WM_DESTROY:
            overlay_active.store(false);
            break;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Función para manejar eventos de la ventana de configuración
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // static HWND hOpacitySlider, hZoomMinSlider, hZoomMaxSlider, hCursorSlider, hBorderSlider; // Variables no utilizadas
    // static HWND hOpacityLabel, hZoomMinLabel, hZoomMaxLabel, hCursorLabel, hBorderLabel; // Variables no utilizadas
    
    switch (uMsg) {
        case WM_CREATE: {
            // Crear botones funcionales para configuración
            // Los botones se posicionarán dinámicamente en DrawSettingsWindow
            
            // Botones de Opacidad
            CreateWindowW(L"BUTTON", L"-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2001, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", L"+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2002, GetModuleHandle(NULL), NULL);
            
            // Botones de Zoom Mínimo
            CreateWindowW(L"BUTTON", L"-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2003, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", L"+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2004, GetModuleHandle(NULL), NULL);
            
            // Botones de Zoom Máximo
            CreateWindowW(L"BUTTON", L"-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2005, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", L"+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2006, GetModuleHandle(NULL), NULL);
            
            // Botones de Velocidad del Cursor
            CreateWindowW(L"BUTTON", L"-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2007, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", L"+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2008, GetModuleHandle(NULL), NULL);
            
            // Botones de Grosor del Borde
            CreateWindowW(L"BUTTON", L"-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2009, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", L"+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 30, 25, hwnd, (HMENU)2010, GetModuleHandle(NULL), NULL);
            
            // Botones de colores predefinidos con owner-draw
            for (int i = 0; i < 10; i++) {
                CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                    0, 0, 35, 35, hwnd, (HMENU)(UINT_PTR)(3000 + i), GetModuleHandle(NULL), NULL);
            }
            
            // Botones de acción
            CreateWindowW(L"BUTTON", L"💾 Aplicar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 100, 35, hwnd, (HMENU)2020, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", L"🔄 Resetear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 100, 35, hwnd, (HMENU)2021, GetModuleHandle(NULL), NULL);
            
            // Configurar scroll vertical
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
            si.nMin = 0;
            si.nMax = scroll_max.load();
            si.nPage = 600; // Altura visible de la ventana
            si.nPos = 0;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            return 0;
        }
        
        case WM_VSCROLL: {
            // Manejar scroll vertical
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            
            int newPos = si.nPos;
            
            switch (LOWORD(wParam)) {
                case SB_LINEUP:
                    newPos -= 10;
                    break;
                case SB_LINEDOWN:
                    newPos += 10;
                    break;
                case SB_PAGEUP:
                    newPos -= si.nPage;
                    break;
                case SB_PAGEDOWN:
                    newPos += si.nPage;
                    break;
                case SB_THUMBTRACK:
                    newPos = HIWORD(wParam);
                    break;
            }
            
            // Limitar el scroll
            if (newPos < 0) newPos = 0;
            if (newPos > si.nMax - (int)si.nPage) newPos = si.nMax - si.nPage;
            
            // Actualizar posición
            scroll_pos.store(newPos);
            si.nPos = newPos;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            
            // Redibujar para reposicionar botones
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        
        case WM_MOUSEWHEEL: {
            // Scroll con rueda del mouse
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int scrollAmount = (delta > 0) ? -60 : 60;  // Scroll hacia arriba o abajo
            
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            
            int newPos = si.nPos + scrollAmount;
            
            // Limitar el scroll
            if (newPos < 0) newPos = 0;
            if (newPos > si.nMax - (int)si.nPage) newPos = si.nMax - si.nPage;
            
            // Actualizar posición
            scroll_pos.store(newPos);
            si.nPos = newPos;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            
            // Redibujar para reposicionar botones
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        

        
        case WM_KEYDOWN: {
            // Salir con ESC
            if (wParam == VK_ESCAPE) {
                settings_overlay_active.store(false);
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        

        
        case WM_DRAWITEM: {
            // Dibujar botones de colores owner-drawn
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            
            // Solo dibujar botones de colores (IDs 3000-3009)
            if (pdis->CtlID >= 3000 && pdis->CtlID <= 3009) {
                // Definir colores predefinidos
                COLORREF predefinedColors[] = {
                    RGB(0, 255, 68),    // Verde (#00FF44)
                    RGB(255, 0, 0),     // Rojo
                    RGB(0, 0, 255),     // Azul
                    RGB(255, 255, 0),   // Amarillo
                    RGB(255, 0, 255),   // Magenta
                    RGB(0, 255, 255),   // Cian
                    RGB(255, 165, 0),   // Naranja
                    RGB(128, 0, 128),   // Púrpura
                    RGB(255, 255, 255), // Blanco
                    RGB(255, 215, 0)    // Dorado
                };
                
                int colorIndex = pdis->CtlID - 3000;
                COLORREF buttonColor = predefinedColors[colorIndex];
                
                // Crear pincel del color del botón
                HBRUSH hBrush = CreateSolidBrush(buttonColor);
                
                // Rellenar el botón con el color
                RECT rect = pdis->rcItem;
                FillRect(pdis->hDC, &rect, hBrush);
                
                // Determinar el color y grosor del borde
                COLORREF borderColor;
                int borderWidth;
                
                if (buttonColor == static_cast<COLORREF>(region_border_color.load())) {
                    // Botón seleccionado - borde grueso blanco
                    borderColor = RGB(255, 255, 255);
                    borderWidth = 3;
                } else {
                    // Botón normal - borde delgado gris
                    borderColor = RGB(100, 100, 100);
                    borderWidth = 1;
                }
                
                // Dibujar el borde
                HPEN hPen = CreatePen(PS_SOLID, borderWidth, borderColor);
                HPEN hOldPen = (HPEN)SelectObject(pdis->hDC, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(pdis->hDC, GetStockObject(NULL_BRUSH));
                
                Rectangle(pdis->hDC, rect.left, rect.top, rect.right, rect.bottom);
                
                // Si está seleccionado, agregar un indicador adicional
                if (buttonColor == static_cast<COLORREF>(region_border_color.load())) {
                    // Dibujar un pequeño círculo en el centro
                    HBRUSH hWhiteBrush = CreateSolidBrush(RGB(255, 255, 255));
                    HBRUSH hOldIndicatorBrush = (HBRUSH)SelectObject(pdis->hDC, hWhiteBrush);
                    
                    int centerX = (rect.left + rect.right) / 2;
                    int centerY = (rect.top + rect.bottom) / 2;
                    int radius = 6;
                    
                    Ellipse(pdis->hDC, centerX - radius, centerY - radius, centerX + radius, centerY + radius);
                    
                    SelectObject(pdis->hDC, hOldIndicatorBrush);
                    DeleteObject(hWhiteBrush);
                }
                
                // Limpiar recursos
                SelectObject(pdis->hDC, hOldPen);
                SelectObject(pdis->hDC, hOldBrush);
                DeleteObject(hPen);
                DeleteObject(hBrush);
                
                return TRUE;
            }
            break;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawSettingsWindow(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_SIZE: {
            // Redibujar cuando cambie el tamaño de la ventana
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        
        case WM_CLOSE: {
            settings_overlay_active.store(false);
            DestroyWindow(hwnd);
            return 0;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                // Botones de Opacidad
                case 2001: { // Botón - para opacidad
                    int currentValue = overlay_opacity.load();
                    if (currentValue > 0) {
                        overlay_opacity.store(currentValue - 10);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                case 2002: { // Botón + para opacidad
                    int currentValue = overlay_opacity.load();
                    if (currentValue < 255) {
                        overlay_opacity.store(currentValue + 10);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                // Botones de Zoom Mínimo
                case 2003: { // Botón - para zoom mín
                    int currentValue = zoom_min_factor.load();
                    if (currentValue > 10) {
                        zoom_min_factor.store(currentValue - 5);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                case 2004: { // Botón + para zoom mín
                    int currentValue = zoom_min_factor.load();
                    if (currentValue < 500) {
                        zoom_min_factor.store(currentValue + 5);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                // Botones de Zoom Máximo
                case 2005: { // Botón - para zoom máx
                    int currentValue = zoom_max_factor.load();
                    if (currentValue > 50) {
                        zoom_max_factor.store(currentValue - 50);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                case 2006: { // Botón + para zoom máx
                    int currentValue = zoom_max_factor.load();
                    if (currentValue < 1000) {
                        zoom_max_factor.store(currentValue + 50);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                // Botones de Velocidad del Cursor
                case 2007: { // Botón - para cursor
                    int currentValue = text_cursor_blink_speed.load();
                    if (currentValue > 100) {
                        text_cursor_blink_speed.store(currentValue - 50);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                case 2008: { // Botón + para cursor
                    int currentValue = text_cursor_blink_speed.load();
                    if (currentValue < 2000) {
                        text_cursor_blink_speed.store(currentValue + 50);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                // Botones de Grosor del Borde
                case 2009: { // Botón - para borde
                    int currentValue = region_border_thickness.load();
                    if (currentValue > 1) {
                        region_border_thickness.store(currentValue - 1);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                case 2010: { // Botón + para borde
                    int currentValue = region_border_thickness.load();
                    if (currentValue < 10) {
                        region_border_thickness.store(currentValue + 1);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                // Botones de colores predefinidos
                case 3000: // Verde
                    region_border_color.store(RGB(0, 255, 68));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3001: // Rojo
                    region_border_color.store(RGB(255, 0, 0));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3002: // Azul
                    region_border_color.store(RGB(0, 0, 255));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3003: // Amarillo
                    region_border_color.store(RGB(255, 255, 0));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3004: // Magenta
                    region_border_color.store(RGB(255, 0, 255));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3005: // Cian
                    region_border_color.store(RGB(0, 255, 255));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3006: // Naranja
                    region_border_color.store(RGB(255, 165, 0));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3007: // Púrpura
                    region_border_color.store(RGB(128, 0, 128));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3008: // Blanco
                    region_border_color.store(RGB(255, 255, 255));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3009: // Dorado
                    region_border_color.store(RGB(255, 215, 0));
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                
                // Botones de acción
                case 2020: { // Aplicar Cambios
                    // Aplicar los cambios actuales a la configuración del programa
                    ApplyConfigurationChanges();
                    
                    // Mostrar confirmación
                    MessageBoxW(hwnd, L"✅ Changes applied successfully!\n\nThe changes have been applied to the main overlay.", L"Settings", MB_OK | MB_ICONINFORMATION);
                    break;
                }
                case 2021: { // Resetear
                    // Resetear valores por defecto
                    overlay_opacity.store(178);
                    zoom_min_factor.store(50);
                    zoom_max_factor.store(500);
                    text_cursor_blink_speed.store(500);
                    region_border_thickness.store(2);
                    
                    MessageBoxW(hwnd, L"🔄 Settings reset to default values!", L"Settings", MB_OK | MB_ICONINFORMATION);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
            }
            return 0;
        }
        
        case WM_DESTROY: {
            settings_overlay_active.store(false);
            return 0;
        }
        
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Función para actualizar las etiquetas de configuración
void UpdateSettingsLabels(HWND hwnd) {
    // Buscar las etiquetas por ID
    HWND hOpacityLabel = GetDlgItem(hwnd, 1002);
    HWND hZoomMinLabel = GetDlgItem(hwnd, 1004);
    HWND hZoomMaxLabel = GetDlgItem(hwnd, 1006);
    HWND hCursorLabel = GetDlgItem(hwnd, 1008);
    HWND hBorderLabel = GetDlgItem(hwnd, 1010);
    
    if (hOpacityLabel) {
        std::wstring text = L"Valor: " + std::to_wstring(overlay_opacity.load());
        SetWindowTextW(hOpacityLabel, text.c_str());
    }
    
    if (hZoomMinLabel) {
        std::wstring text = L"Valor: " + std::to_wstring(zoom_min_factor.load() / 100.0f) + L"x";
        SetWindowTextW(hZoomMinLabel, text.c_str());
    }
    
    if (hZoomMaxLabel) {
        std::wstring text = L"Valor: " + std::to_wstring(zoom_max_factor.load() / 100.0f) + L"x";
        SetWindowTextW(hZoomMaxLabel, text.c_str());
    }
    
    if (hCursorLabel) {
        std::wstring text = L"Valor: " + std::to_wstring(text_cursor_blink_speed.load()) + L"ms";
        SetWindowTextW(hCursorLabel, text.c_str());
    }
    
    if (hBorderLabel) {
        std::wstring text = L"Valor: " + std::to_wstring(region_border_thickness.load()) + L"px";
        SetWindowTextW(hBorderLabel, text.c_str());
    }
}

// Función para actualizar las posiciones de los sliders
void UpdateSliderPositions(HWND hwnd) {
    HWND hOpacitySlider = GetDlgItem(hwnd, 1001);
    HWND hZoomMinSlider = GetDlgItem(hwnd, 1003);
    HWND hZoomMaxSlider = GetDlgItem(hwnd, 1005);
    HWND hCursorSlider = GetDlgItem(hwnd, 1007);
    HWND hBorderSlider = GetDlgItem(hwnd, 1009);
    
    if (hOpacitySlider) {
        SendMessage(hOpacitySlider, TBM_SETPOS, TRUE, overlay_opacity.load());
    }
    
    if (hZoomMinSlider) {
        SendMessage(hZoomMinSlider, TBM_SETPOS, TRUE, zoom_min_factor.load() / 10);
    }
    
    if (hZoomMaxSlider) {
        SendMessage(hZoomMaxSlider, TBM_SETPOS, TRUE, zoom_max_factor.load() / 10);
    }
    
    if (hCursorSlider) {
        SendMessage(hCursorSlider, TBM_SETPOS, TRUE, text_cursor_blink_speed.load() / 10);
    }
    
    if (hBorderSlider) {
        SendMessage(hBorderSlider, TBM_SETPOS, TRUE, region_border_thickness.load());
    }
}

// Función para aplicar cambios de configuración a todas las ventanas activas
void ApplyConfigurationChanges() {
    // Forzar redibujada del overlay principal para aplicar todos los cambios
    // La opacidad se aplica automáticamente en DrawOverlay usando overlay_opacity
    needsRedraw.store(true);
    
    // Aplicar cambios a la ventana de configuración si está activa
    if (settings_overlay_active.load()) {
        // Forzar redibujada de la ventana de configuración
        HWND hSettingsWnd = FindWindowW(L"ScreenHighlighterSettingsClass", NULL);
        if (hSettingsWnd) {
            InvalidateRect(hSettingsWnd, NULL, TRUE);
        }
    }
    
    // Guardar configuración en archivo .ini
    SaveConfiguration();
}

// Función para resetear a valores por defecto
void ResetToDefaultSettings() {
    overlay_opacity.store(178);
    zoom_min_factor.store(50);      // 0.5x
    zoom_max_factor.store(500);     // 5.0x
    text_cursor_blink_speed.store(500);
    region_border_thickness.store(2);
    region_border_color.store(0x00FF44);
    
    // Guardar configuración reseteada en archivo .ini
    SaveConfiguration();
}

// Función para registrar hotkeys
bool RegisterHotkeys() {
    printf("  🔑 Registrando hotkey Shift+Alt+X...\n");
    
    // Shift+Alt+X
    if (!RegisterHotKey(hMainWnd, 1, MOD_SHIFT | MOD_ALT, 'X')) {
        printf("  ❌ Error al registrar hotkey Shift+Alt+X\n");
        return false;
    }
    
    printf("  ✅ Hotkey Shift+Alt+X registrado exitosamente\n");
    return true;
}

// Función para procesar mensajes de la ventana principal
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
                case WM_HOTKEY:
            printf("🔥 Hotkey recibido: %d\n", (int)wParam);
            switch (wParam) {
                case 1: // Shift+Alt+X
                    printf("🎯 Activando overlay (Shift+Alt+X)\n");
                    if (!overlay_active.load()) {
                        printf("🚀 Iniciando thread de overlay...\n");
                        std::thread overlay_thread(ShowOverlay);
                        overlay_thread.detach();
                    } else {
                        printf("⚠️ Overlay ya está activo\n");
                    }
                    break;
                default:
                    printf("❓ Hotkey desconocido: %d\n", (int)wParam);
                    break;
            }
            break;
            
        case WM_TASKBAR:
            switch (lParam) {
                case WM_RBUTTONUP:
                    ShowTrayMenu();
                    break;
                case WM_LBUTTONDBLCLK:
                    if (!overlay_active.load()) {
                        std::thread overlay_thread(ShowOverlay);
                        overlay_thread.detach();
                    }
                    break;
            }
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case MENU_ACTIVATE_ID: // Activar Highlight
                    if (!overlay_active.load()) {
                        std::thread overlay_thread(ShowOverlay);
                        overlay_thread.detach();
                    }
                    break;
                    
                case MENU_SETTINGS_ID: // Configuración
                    ShowSettingsOverlay();
                    break;
                    
                case MENU_EXIT_ID: // Salir
                    running.store(false);
                    PostQuitMessage(0);
                    break;
            }
            break;
            
        case WM_TIMER:
            if (wParam == 1) { // Timer de verificación del system tray
                printf("⏰ Verificando estado del system tray...\n");
                
                // Verificar si el icono del system tray está visible
                if (systemTrayInitialized) {
                    // Enviar mensaje de prueba al system tray
                    if (!Shell_NotifyIcon(NIM_MODIFY, &nid)) {
                        printf("⚠️ Icono del system tray no responde - Restaurando...\n");
                        if (RestoreSystemTrayIcon()) {
                            printf("✅ System tray restaurado por timer\n");
                        } else {
                            printf("❌ Error al restaurar system tray por timer\n");
                        }
                    } else {
                        printf("✅ System tray responde correctamente\n");
                    }
                }
            }
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Función principal sin ventana de consola
// Función para verificar si la aplicación tiene permisos de administrador
bool IsRunningAsAdministrator() {
    // Método simple y compatible con MinGW
    HANDLE hToken = NULL;
    TOKEN_ELEVATION elevation;
    DWORD size = sizeof(TOKEN_ELEVATION);
    bool isAdmin = false;
    
    // Abrir token del proceso actual
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        // Verificar si el token tiene elevación
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isAdmin = (elevation.TokenIsElevated != 0);
        }
        CloseHandle(hToken);
    }
    
    return isAdmin;
}

// Función para solicitar elevación de privilegios
bool RequestAdminPrivileges() {
    // Obtener el nombre del ejecutable actual
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    
    // Crear estructura para ShellExecute
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb = L"runas";  // Solicitar ejecución como administrador
    sei.lpFile = exePath;
    sei.nShow = SW_NORMAL;
    
    // Ejecutar como administrador
    if (ShellExecuteExW(&sei)) {
        return true;
    }
    
    return false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow; // Parámetros no utilizados
    
    printf("🚀 Iniciando Screen Highlighter...\n");
    
    // Verificar permisos de administrador
    if (!IsRunningAsAdministrator()) {
        printf("⚠️ La aplicación requiere permisos de administrador\n");
        printf("🔐 Solicitando elevación de privilegios...\n");
        
        if (RequestAdminPrivileges()) {
            printf("✅ Permisos de administrador solicitados exitosamente\n");
            printf("🔄 Cerrando instancia actual...\n");
            return 0; // Cerrar esta instancia
        } else {
            printf("❌ No se pudieron obtener permisos de administrador\n");
            MessageBoxW(NULL, 
                L"Screen Highlighter requiere permisos de administrador para funcionar correctamente.\n\n"
                L"Por favor, ejecuta la aplicación como administrador.",
                L"Permisos Requeridos", 
                MB_OK | MB_ICONWARNING);
            return 1;
        }
    }
    
    printf("✅ Permisos de administrador verificados\n");
    
    // Cargar configuración desde archivo .ini al inicio
    LoadConfiguration();
    printf("✅ Configuración cargada\n");
                
    // Crear una ventana oculta para manejar mensajes
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ScreenHighlighterClass";
    wc.hIcon = LoadIconFromFile(TRAY_ICON_LARGE);
    wc.hIconSm = LoadIconFromFile(TRAY_ICON_SMALL);
    
                if (!RegisterClassExW(&wc)) {
        printf("❌ Error al registrar clase principal\n");
        return 1;
    }
    printf("✅ Clase principal registrada\n");
    
    // Registrar clase para el overlay
    WNDCLASSEXW wcOverlay = {};
    wcOverlay.cbSize = sizeof(WNDCLASSEXW);
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.lpszClassName = L"ScreenHighlighterOverlayClass";
    wcOverlay.hCursor = LoadCursor(NULL, IDC_CROSS);
    
    if (!RegisterClassExW(&wcOverlay)) {
        printf("❌ Error al registrar clase overlay\n");
        return 1;
    }
    printf("✅ Clase overlay registrada\n");
    
    // Registrar clase para la ventana de configuración
    WNDCLASSEXW wcSettings = {};
    wcSettings.cbSize = sizeof(WNDCLASSEXW);
    wcSettings.lpfnWndProc = SettingsWndProc;
    wcSettings.hInstance = hInstance;
    wcSettings.lpszClassName = L"ScreenHighlighterSettingsClass";
    wcSettings.hCursor = LoadCursor(NULL, IDC_ARROW); // Cursor normal
    wcSettings.hIcon = LoadIconFromFile(TRAY_ICON_SMALL);
    wcSettings.hIconSm = LoadIconFromFile(TRAY_ICON_SMALL);
    
    if (!RegisterClassExW(&wcSettings)) {
        printf("❌ Error al registrar clase settings\n");
        return 1;
    }
    printf("✅ Clase settings registrada\n");
    
    hMainWnd = CreateWindowExW(
        0,
        L"ScreenHighlighterClass",
        L"Screen Highlighter",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hMainWnd) {
        printf("❌ Error al crear ventana principal\n");
        return 1;
    }
    printf("✅ Ventana principal creada\n");
    
    // Ocultar la ventana
    ShowWindow(hMainWnd, SW_HIDE);
    
    // Agregar icono al system tray
    printf("🔧 Agregando icono al system tray...\n");
    if (!AddToSystemTray()) {
        printf("❌ Error al agregar icono al system tray\n");
        MessageBoxW(NULL, L"Error al agregar icono al system tray", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    printf("✅ Icono agregado al system tray\n");
    
    // Registrar hotkeys
    printf("🔧 Registrando hotkeys...\n");
    if (!RegisterHotkeys()) {
        printf("❌ Error al registrar hotkeys\n");
        MessageBoxW(NULL, L"Error al registrar hotkeys", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    printf("✅ Hotkeys registrados\n");
    
    // Iniciar monitoreo de explorer.exe para restauración automática del system tray
    printf("🔍 Iniciando monitoreo de explorer.exe...\n");
    StartExplorerMonitoring();
    printf("✅ Monitoreo de explorer.exe iniciado\n");
    
    // Configurar timer para verificar periódicamente el system tray
    SetTimer(hMainWnd, 1, 30000, NULL); // Verificar cada 30 segundos
    printf("⏰ Timer de verificación del system tray configurado\n");
    
    // Bucle principal del mensaje
    printf("🔄 Iniciando bucle principal de mensajes...\n");
    printf("💡 Presiona Shift+Alt+X para activar el highlight\n");
    printf("💡 Busca el icono verde en el system tray\n");
    
    MSG msg;
    while (running.load() && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Limpiar hotkeys
    UnregisterHotKey(hMainWnd, 1);
    
    // Limpiar timer
    KillTimer(hMainWnd, 1);
    
    // Detener monitoreo de explorer.exe
    printf("🛑 Deteniendo monitoreo de explorer.exe...\n");
    StopExplorerMonitoring();
    
    // Remover del system tray
    RemoveFromSystemTray();
    
    return 0;
}
