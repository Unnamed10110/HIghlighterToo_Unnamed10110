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
#include <unordered_map>
#include <mutex>
#include <cstdarg>
#include <expected>
// Headers para soporte de GIFs
#include <gdiplus.h>
#include <objidl.h>
#include <shlwapi.h>

// windows.h define macros min/max que rompen std::min / std::max.
// NOMINMAX las desactiva. Tambien se define en CMakeLists, esto es un respaldo
// por si se compila main.cpp directamente.
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Optimizaciones de compilador para máxima performance
// Nota: Los pragmas específicos de MSVC no son compatibles con g++

// Incluir librería de controles comunes para sliders
// #pragma comment(lib, "comctl32.lib") // No soportado por g++

// ============================================================================
// DIAGNOSTICO
// ============================================================================
// El binario que se distribuye es una aplicacion GUI sin consola, por lo que los
// LogDebug() que habia repartidos por el archivo escribian a un stdout invalido:
// no habia diagnostico util en la compilacion que realmente se usa.
//
// LogDebug envia el mensaje al depurador (visible con DebugView o desde el IDE)
// en compilaciones de depuracion, y desaparece por completo en release.
#ifdef DEBUG_BUILD
inline void LogDebug(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}
#else
inline void LogDebug(const char*, ...) {}
#endif

// ============================================================================
// CICLO DE VIDA DE GDI+
// ============================================================================
// GDI+ se inicializa una sola vez en WinMain y se cierra al salir. Antes se
// inicializaba de forma diferida dentro de LoadGifFromFile y nunca se llamaba a
// GdiplusShutdown. Se usa para decodificar GIFs y para codificar PNG.
static ULONG_PTR g_gdiplusToken = 0;
static bool g_gdiplusReady = false;

bool InitializeGdiPlus() {
    if (g_gdiplusReady) return true;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, NULL) != Gdiplus::Ok) {
        return false;
    }
    g_gdiplusReady = true;
    return true;
}

void ShutdownGdiPlus() {
    if (g_gdiplusReady) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusReady = false;
        g_gdiplusToken = 0;
    }
}

// ============================================================================
// CLASES RAII OPTIMIZADAS PARA GESTIÓN SEGURA DE RECURSOS GDI
// ============================================================================

// Cache estático para recursos GDI comúnmente usados (optimización de performance)
namespace GdiCache {
    // Declaración forward
    struct TextLine;
    
    // Fuentes cacheadas
    static HFONT hCachedFontZoom = nullptr;
    static HFONT hCachedFont = nullptr;
    static HFONT hCachedFontIndicator = nullptr;
    static HFONT hCachedFontScreenshot = nullptr;
    
    // Pinceles cacheados para colores comunes
    static HBRUSH hBlackBrush = nullptr;
    static HBRUSH hWhiteBrush = nullptr;
    static HBRUSH hSelectionBrush = nullptr;
    static HBRUSH hOverlayBrush = nullptr;
    
    // Origen cacheado para AlphaBlend: un DIB de 1x1 y 32 bpp cuyo pixel se
    // reescribe con el color pedido y se estira al tamaño del rectangulo. Evita
    // crear y destruir un bitmap por cada trazo del resaltador.
    static HDC hBlendDC = nullptr;
    static HBITMAP hBlendBitmap = nullptr;
    static DWORD* pBlendPixel = nullptr;

    // Lápices cacheados para colores comunes
    static HPEN hWhitePen = nullptr;
    static HPEN hCursorPen = nullptr;
    static HPEN hModernPen = nullptr;
    
    // Cache de mediciones de texto.
    //
    // La clave incluye la fuente: antes solo era el texto, asi que una medicion
    // hecha con una fuente se reutilizaba con otra y devolvia un ancho erroneo.
    // El tamaño esta acotado porque al escribir se genera una entrada por cada
    // prefijo del texto y antes no se descartaba ninguna.
    struct TextSizeKey {
        std::wstring text;
        HFONT font;
        bool operator==(const TextSizeKey& other) const {
            return font == other.font && text == other.text;
        }
    };
    struct TextSizeKeyHash {
        size_t operator()(const TextSizeKey& k) const {
            return std::hash<std::wstring>{}(k.text) ^
                   (std::hash<void*>{}(reinterpret_cast<void*>(k.font)) << 1);
        }
    };
    constexpr size_t kMaxCacheEntries = 512;
    static std::unordered_map<TextSizeKey, SIZE, TextSizeKeyHash> textSizeCache;
    
    // NOTA: se elimino el cache de dimensiones de bitmaps. Estaba indexado por
    // HBITMAP y GDI reutiliza los handles, por lo que un handle liberado y vuelto a
    // asignar devolvia las dimensiones del bitmap anterior. GetObject() es barato.
    
    // Cache para layout de texto (optimización de performance)
    struct TextLayout {
        std::vector<TextLine> lines;
        int totalHeight;
        int maxWidth;
    };
    static std::unordered_map<std::wstring, TextLayout> textLayoutCache;
    
    // Función optimizada para procesar texto línea por línea (reduce substr() calls)
    
    // Definición de TextLine dentro del namespace
    struct TextLine {
        std::wstring content;
        size_t startPos;
        size_t endPos;
    };
    
    std::vector<TextLine> ProcessTextLines(const std::wstring& text) {
        std::vector<TextLine> lines;
        size_t pos = 0;
        
        while (pos < text.length()) {
            size_t nextNewline = text.find(L'\n', pos);
            if (nextNewline == std::wstring::npos) {
                nextNewline = text.length();
            }
            
            TextLine line;
            line.startPos = pos;
            line.endPos = nextNewline;
            line.content = text.substr(pos, nextNewline - pos);
            lines.push_back(line);
            
            pos = nextNewline + 1;
        }
        
        return lines;
    }
    
    // NOTA: TextRender::BuildLayout es quien calcula el layout real (incluyendo
    // altura de imagenes y GIFs) y se usa una sola vez por frame. Esta version
    // simple queda por compatibilidad, pero no esta en el camino de dibujo.
    TextLayout GetTextLayoutCached(const std::wstring& text) {
        auto it = textLayoutCache.find(text);
        if (it != textLayoutCache.end()) {
            return it->second;
        }
        
        TextLayout layout;
        layout.lines = ProcessTextLines(text);
        layout.totalHeight = 0;
        layout.maxWidth = 0;
        
        for (const auto& line : layout.lines) {
            layout.totalHeight += 20; // Altura estándar por línea
            // El ancho máximo se calculará cuando sea necesario con GetTextSizeCached
        }
        
        if (textLayoutCache.size() >= kMaxCacheEntries) {
            textLayoutCache.clear();
        }
        textLayoutCache[text] = layout;
        return layout;
    }
    
    // Función optimizada para obtener tamaño de texto con cache
    SIZE GetTextSizeCached(HDC hdc, const std::wstring& text) {
        const TextSizeKey key{text, (HFONT)GetCurrentObject(hdc, OBJ_FONT)};

        auto it = textSizeCache.find(key);
        if (it != textSizeCache.end()) {
            return it->second;
        }

        SIZE size = {0, 0};
        GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.length()), &size);

        // Descartar todo al llegar al limite. Politica simple, pero el cache se
        // rellena en un frame y evita el crecimiento sin fin que habia antes.
        if (textSizeCache.size() >= kMaxCacheEntries) {
            textSizeCache.clear();
        }
        textSizeCache[key] = size;
        return size;
    }
    
    // Dimensiones de un bitmap. Sin cache a proposito: ver la nota de
    // bitmapSizeCache arriba. GetObject() es una consulta local muy barata.
    SIZE GetBitmapSize(HBITMAP hBitmap) {
        BITMAP bm;
        SIZE size = {0, 0};
        if (GetObject(hBitmap, sizeof(BITMAP), &bm) > 0) {
            size.cx = bm.bmWidth;
            size.cy = bm.bmHeight;
        }
        return size;
    }
    
    // Limpiar cache de texto cuando sea necesario
    void ClearTextCache() {
        textSizeCache.clear();
        textLayoutCache.clear();
    }
    
    // Clase helper para optimizar SelectObject calls (reduce overhead)
    class GdiObjectManager {
        HDC hdc_;
        HGDIOBJ oldObjects_[10];
        int count_;
        
    public:
        GdiObjectManager(HDC hdc) : hdc_(hdc), count_(0) {}
        
        ~GdiObjectManager() {
            // Restaurar todos los objetos en orden inverso
            for (int i = count_ - 1; i >= 0; --i) {
                SelectObject(hdc_, oldObjects_[i]);
            }
        }
        
        HGDIOBJ Select(HGDIOBJ obj) {
            if (count_ < 10) {
                oldObjects_[count_] = SelectObject(hdc_, obj);
                count_++;
                return obj;
            }
            return SelectObject(hdc_, obj);
        }
        
        // Método para restaurar manualmente si es necesario
        void Restore() {
            for (int i = count_ - 1; i >= 0; --i) {
                SelectObject(hdc_, oldObjects_[i]);
            }
            count_ = 0;
        }
    };
    
    // Cache de lapices por (color, grosor). Antes DrawLine creaba y destruia un
    // HPEN en cada llamada, y DrawArrow llama a DrawLine tres veces por flecha,
    // por cada elemento y por cada frame.
    static std::unordered_map<uint64_t, HPEN> penCache;

    HPEN GetPenCached(COLORREF color, int thickness) {
        thickness = std::clamp(thickness, 1, 100);
        const uint64_t key = (static_cast<uint64_t>(color) << 32) |
                              static_cast<uint64_t>(static_cast<uint32_t>(thickness));

        auto it = penCache.find(key);
        if (it != penCache.end()) {
            return it->second;
        }

        HPEN pen = CreatePen(PS_SOLID, thickness, color);
        if (pen) {
            if (penCache.size() >= 256) {
                for (auto& entry : penCache) DeleteObject(entry.second);
                penCache.clear();
            }
            penCache[key] = pen;
        }
        return pen;
    }

    // Inicializar cache
    void InitializeCache() {
        if (!hCachedFontZoom) {
            hCachedFontZoom = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        }
        if (!hCachedFont) {
            hCachedFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        }
        if (!hCachedFontIndicator) {
            hCachedFontIndicator = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        }
        if (!hCachedFontScreenshot) {
            hCachedFontScreenshot = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        }
        
        if (!hBlackBrush) {
            hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
        }
        if (!hWhiteBrush) {
            hWhiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        }
        if (!hSelectionBrush) {
            hSelectionBrush = CreateSolidBrush(RGB(0, 120, 215));
        }
        if (!hOverlayBrush) {
            hOverlayBrush = CreateSolidBrush(RGB(0, 0, 0));
        }
        
        if (!hBlendDC) {
            hBlendDC = CreateCompatibleDC(NULL);
            if (hBlendDC) {
                BITMAPINFO bi;
                ZeroMemory(&bi, sizeof(bi));
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = 1;
                bi.bmiHeader.biHeight = 1;
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;

                void* bits = nullptr;
                hBlendBitmap = CreateDIBSection(hBlendDC, &bi, DIB_RGB_COLORS,
                                                &bits, NULL, 0);
                if (hBlendBitmap) {
                    pBlendPixel = static_cast<DWORD*>(bits);
                    SelectObject(hBlendDC, hBlendBitmap);
                } else {
                    DeleteDC(hBlendDC);
                    hBlendDC = nullptr;
                }
            }
        }

        if (!hWhitePen) {
            hWhitePen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        }
        if (!hCursorPen) {
            hCursorPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        }
        if (!hModernPen) {
            hModernPen = CreatePen(PS_SOLID, 2, RGB(50, 50, 50));
        }
    }
    
    // Limpiar cache
    void CleanupCache() {
        if (hCachedFontZoom) { DeleteObject(hCachedFontZoom); hCachedFontZoom = nullptr; }
        if (hCachedFont) { DeleteObject(hCachedFont); hCachedFont = nullptr; }
        if (hCachedFontIndicator) { DeleteObject(hCachedFontIndicator); hCachedFontIndicator = nullptr; }
        if (hCachedFontScreenshot) { DeleteObject(hCachedFontScreenshot); hCachedFontScreenshot = nullptr; }
        
        if (hBlackBrush) { DeleteObject(hBlackBrush); hBlackBrush = nullptr; }
        if (hWhiteBrush) { DeleteObject(hWhiteBrush); hWhiteBrush = nullptr; }
        if (hSelectionBrush) { DeleteObject(hSelectionBrush); hSelectionBrush = nullptr; }
        if (hOverlayBrush) { DeleteObject(hOverlayBrush); hOverlayBrush = nullptr; }
        
        if (hBlendBitmap) { DeleteObject(hBlendBitmap); hBlendBitmap = nullptr; }
        if (hBlendDC) { DeleteDC(hBlendDC); hBlendDC = nullptr; }
        pBlendPixel = nullptr;

        for (auto& entry : penCache) DeleteObject(entry.second);
        penCache.clear();

        if (hWhitePen) { DeleteObject(hWhitePen); hWhitePen = nullptr; }
        if (hCursorPen) { DeleteObject(hCursorPen); hCursorPen = nullptr; }
        if (hModernPen) { DeleteObject(hModernPen); hModernPen = nullptr; }
    }
}

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
        should_delete_ = delete_on_destroy;
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
// UTILIDADES MODERNAS CON C++23
// ============================================================================
// Aqui vivia una capa de ayudantes basados en std::expected (CreateWindowSafe,
// LoadResourceSafe, ReadFileSafe, WriteFileSafe, ReadRegistryStringSafe). Nunca
// se llamo a ninguno: solo existian sus definiciones. Se eliminaron por ser
// codigo muerto. ReadFileSafe ademas contenia el mismo error de truncar wchar_t
// a char que se corrigio en el guardado de capturas.

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
    Highlighter = 5,
    Ellipse = 6,     // F5 - elipse
    Pen = 7,         // F6 - trazo libre
    Redact = 8,      // F7 - pixelar (ocultar informacion sensible)
    Step = 9         // F8 - numero de paso
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
constexpr int MENU_ENABLE_AUTOSTART_ID = 1003;
constexpr int MENU_DISABLE_AUTOSTART_ID = 1004;
constexpr int MENU_EXIT_ID = 1005;
constexpr int MENU_SEPARATOR_ID = 1006;
constexpr int MENU_LOW_RESOURCES_ID = 1008;
constexpr int MENU_HIGH_PERFORMANCE_ID = 1009;
constexpr wchar_t MENU_ACTIVATE_TEXT[] = L"Activate Highlight (Shift+Alt+X)";
constexpr wchar_t MENU_EXIT_TEXT[] = L"Exit";
constexpr wchar_t MENU_SEPARATOR_TEXT[] = L"";

// Nombre del archivo de configuración
// Nombre del archivo de configuracion. La ruta completa la resuelve
// GetConfigFilePath(): antes esta ruta era relativa y se resolvia contra el
// directorio de trabajo, que al iniciar desde el registro (Run) es system32.
constexpr const wchar_t* CONFIG_FILE_NAME = L"ScreenHighlighter.ini";

// ============================================================================
// VARIABLES GLOBALES CON MEJORAS DE SEGURIDAD
// ============================================================================

// Variables globales para el estado del programa
std::atomic<bool> running(true);
std::atomic<bool> overlay_active(false);
std::atomic<bool> selection_mode(false);
// La antigua bandera needsRedraw se elimino: el repintado ahora lo pide
// RequestOverlayRedraw() via InvalidateRect y lo entrega WM_PAINT, en lugar de
// que un bucle la consultara cada 16-50 ms.
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
// ----------------------------------------------------------------------------
// Candado del estado de anotaciones.
//
// zoom_text, screenRectangles, drawing_elements, clipboard_images, image_markers
// y gif_elements son objetos NO atomicos que se tocan desde tres hilos: el hilo
// principal (WindowProc y el menu del tray), el hilo del overlay (ShowOverlay y
// OverlayWndProc) y antes tambien un hilo suelto de parpadeo del cursor.
//
// Con -O0 cada acceso releia la memoria y las carreras parecian inofensivas; con
// -O2 el compilador mantiene los valores en registros y los saca de los bucles, lo
// que explica el sintoma conocido de "el modo Release no funciona". Todo acceso a
// esas variables debe tomar este candado.
// ----------------------------------------------------------------------------
std::mutex g_annotationMutex;

std::vector<ScopedBitmap> clipboard_images;
std::vector<std::wstring> image_markers; // Marcadores en el texto

// El texto se maneja en una variable global simple (no atómica)
std::wstring zoom_text;
// Variable para almacenar el texto antes del zoom (para copiarlo la primera vez)
std::wstring text_before_zoom;

// Variables para configuración
std::atomic<bool> settings_overlay_active(false);
std::atomic<int> hotkey_shift_alt_x(1); // 1 = habilitado, 0 = deshabilitado
std::atomic<int> overlay_opacity(178); // 0-255
std::atomic<int> zoom_min_factor(50); // 0.5x = 50
std::atomic<int> zoom_max_factor(500); // 5.0x = 500
std::atomic<int> text_cursor_blink_speed(500); // ms
std::atomic<int> region_border_thickness(2); // píxeles
std::atomic<int> region_border_color(0x00FF00); // Verde por defecto
// Opacidad del resaltador (1-255). 110 deja ver el contenido de abajo.
std::atomic<int> highlighter_alpha(110);

// Carpeta donde se guardan las capturas. Vacia = carpeta por defecto
// (%USERPROFILE%\Pictures\Screenshots). Configurable desde el .ini.
std::wstring screenshot_folder;
// Formato de guardado: "png" (por defecto) o "bmp".
std::wstring screenshot_format = L"png";

// Modos de recursos
enum class ResourceMode {
    Normal = 0,
    LowResources = 1,
    HighPerformance = 2
};

std::atomic<ResourceMode> current_resource_mode(ResourceMode::Normal);

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
    std::wstring text;           // Para texto libre
    std::vector<POINT> points;   // Trazo del lapiz (DrawingTool::Pen)
    int step_number = 0;         // Numero mostrado por DrawingTool::Step

    DrawingElement(DrawingTool type, int x1, int y1, int x2, int y2, COLORREF col, int thick, bool fill, const std::wstring& txt = L"")
        : tool_type(type), x1(x1), y1(y1), x2(x2), y2(y2), color(col), thickness(thick), filled(fill), text(txt) {}
};

// Estructura para elementos GIF
struct GifElement {
    std::wstring file_path;           // Ruta del archivo GIF
    std::vector<ScopedBitmap> frames; // Frames del GIF
    std::vector<int> delays;          // Delays originales entre frames (ms)
    std::vector<int> adjusted_delays; // Delays ajustados por el usuario
    int current_frame;                // Frame actual
    int x, y;                         // Posición en pantalla
    int width, height;                // Dimensiones del GIF
    DWORD last_frame_time;            // Último cambio de frame
    bool is_playing;                  // Si está reproduciéndose
    float speed_multiplier;           // Multiplicador de velocidad (1.0 = velocidad original)
    
    GifElement(const std::wstring& path, int pos_x, int pos_y, int w, int h)
        : file_path(path), current_frame(0), x(pos_x), y(pos_y), width(w), height(h), 
          last_frame_time(0), is_playing(true), speed_multiplier(1.0f) {}
    
    // Función para ajustar la velocidad de reproducción
    void setSpeed(float speed) {
        speed_multiplier = speed;
        adjusted_delays.clear();
        
        // Calcular delays ajustados
        for (int original_delay : delays) {
            int adjusted_delay = static_cast<int>(original_delay / speed_multiplier);
            adjusted_delays.push_back(adjusted_delay);
        }
    }
    
    // Función para obtener el delay actual (ajustado o original)
    int getCurrentDelay() const {
        if (!adjusted_delays.empty() && speed_multiplier != 1.0f) {
            return adjusted_delays[current_frame];
        }
        return delays[current_frame];
    }
};

std::vector<DrawingElement> drawing_elements;

// Pila de rehacer. Ctrl+Z mueve el ultimo elemento aqui y Ctrl+Y / Ctrl+Shift+Z
// lo devuelve. El README ya prometia "Undo/Redo" pero solo existia el deshacer.
std::vector<DrawingElement> redo_stack;

// Trazo del lapiz en curso (se acumula mientras el boton esta presionado).
std::vector<POINT> pen_stroke;

// Contador de los numeros de paso (DrawingTool::Step).
std::atomic<int> step_counter(1);
std::vector<GifElement> gif_elements; // Elementos GIF para reproducción

// Variables para el system tray
NOTIFYICONDATA nid;
HWND hMainWnd;
// Atomico: lo consulta y actualiza tambien el hilo monitor de explorer.exe.
std::atomic<bool> systemTrayInitialized{false};
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

// Declaración forward
void PreAllocateVectors();

// Aplicar configuraciones según el modo de recursos seleccionado
void ApplyResourceModeSettings() {
    ResourceMode mode = current_resource_mode.load();
    
    switch (mode) {
        case ResourceMode::LowResources:
            LogDebug("🔋 Applying Low Resources Mode settings...\n");
            
            // Liberar los caches de texto: es memoria recuperable y no es
            // contenido del usuario.
            GdiCache::ClearTextCache();
            
            // IMPORTANTE: aqui se llamaba a .clear() sobre drawing_elements,
            // clipboard_images y gif_elements, es decir cambiar de "modo de
            // recursos" borraba las anotaciones del usuario. Reservar capacidad
            // nunca justifica perder datos, asi que solo se reserva.
            clipboard_images.reserve(10);
            image_markers.reserve(10);
            drawing_elements.reserve(25);
            gif_elements.reserve(5);
            screenRectangles.reserve(5);
            
            // Configurar para menor uso de CPU
            text_cursor_blink_speed.store(750); // Más lento
            
            LogDebug("✅ Low Resources Mode applied\n");
            break;
            
        case ResourceMode::HighPerformance:
            LogDebug("⚡ Applying High Performance Mode settings...\n");
            
            // Igual que en LowResources: no se borran las anotaciones del usuario,
            // solo se reserva capacidad.
            clipboard_images.reserve(200);
            image_markers.reserve(200);
            drawing_elements.reserve(500);
            gif_elements.reserve(50);
            screenRectangles.reserve(100);
            
            // Configurar para máximo rendimiento
            text_cursor_blink_speed.store(250); // Más rápido
            
            LogDebug("✅ High Performance Mode applied\n");
            break;
            
        case ResourceMode::Normal:
        default:
            LogDebug("🔧 Applying Normal Mode settings...\n");
            
            // Aplicar configuraciones estándar (las originales)
            PreAllocateVectors();
            text_cursor_blink_speed.store(500); // Velocidad normal
            
            LogDebug("✅ Normal Mode applied\n");
            break;
    }
}

// Pre-allocar vectores para mejor performance (optimización)
void PreAllocateVectors() {
    clipboard_images.reserve(50);  // Pre-allocar para hasta 50 imágenes
    image_markers.reserve(50);     // Pre-allocar para hasta 50 marcadores
    drawing_elements.reserve(100); // Pre-allocar para hasta 100 elementos de dibujo
    gif_elements.reserve(20);      // Pre-allocar para hasta 20 GIFs
    screenRectangles.reserve(20);  // Pre-allocar para hasta 20 rectángulos
}

// Handle de la ventana overlay. Atomico: lo escribe el hilo del overlay y lo
// leen el hilo principal y DrawOverlay.
std::atomic<HWND> hCurrentOverlay{nullptr};

// ----------------------------------------------------------------------------
// Geometria del escritorio virtual
// ----------------------------------------------------------------------------
// SM_CXSCREEN / SM_CYSCREEN describen SOLO el monitor primario. El escritorio
// virtual abarca todos los monitores y su origen puede ser negativo (un monitor
// a la izquierda o arriba del primario).
int VirtualScreenLeft()   { return GetSystemMetrics(SM_XVIRTUALSCREEN); }
int VirtualScreenTop()    { return GetSystemMetrics(SM_YVIRTUALSCREEN); }
int VirtualScreenWidth()  { return GetSystemMetrics(SM_CXVIRTUALSCREEN); }
int VirtualScreenHeight() { return GetSystemMetrics(SM_CYVIRTUALSCREEN); }

// El overlay cubre todo el escritorio virtual, asi que sus coordenadas de cliente
// estan desplazadas respecto a las de pantalla por el origen virtual.
int ClientToScreenX(int clientX) { return clientX + VirtualScreenLeft(); }
int ClientToScreenY(int clientY) { return clientY + VirtualScreenTop(); }

// Habilita conciencia de DPI por monitor en tiempo de ejecucion.
//
// El manifiesto ya lo declara, pero se hace tambien aqui por si el binario se
// ejecuta sin manifiesto incrustado. Se carga dinamicamente para no romper la
// compatibilidad con Windows 7/8, donde estas funciones no existen.
void EnablePerMonitorDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef BOOL (WINAPI *SetCtxFn)(void*);
        SetCtxFn setCtx = (SetCtxFn)(void*)GetProcAddress(
            user32, "SetProcessDpiAwarenessContext");
        if (setCtx) {
            // -4 == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            if (setCtx((void*)(INT_PTR)-4)) return;
            // -3 == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE
            if (setCtx((void*)(INT_PTR)-3)) return;
        }
    }

    // Respaldo para Windows 8.1 / 10 antiguos.
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        typedef HRESULT (WINAPI *SetAwarenessFn)(int);
        SetAwarenessFn setAwareness = (SetAwarenessFn)(void*)GetProcAddress(
            shcore, "SetProcessDpiAwareness");
        if (setAwareness) {
            setAwareness(2); // PROCESS_PER_MONITOR_DPI_AWARE
            FreeLibrary(shcore);
            return;
        }
        FreeLibrary(shcore);
    }

    // Ultimo respaldo (Vista+).
    SetProcessDPIAware();
}

// Identificadores de los timers de la ventana overlay.
constexpr UINT_PTR TIMER_CURSOR_BLINK = 1;  // parpadeo del cursor de texto
constexpr UINT_PTR TIMER_GIF_FRAMES = 2;    // avance de frames de los GIFs

// Pide un repintado del overlay.
//
// Reemplaza a needsRedraw.store(true): antes un bucle consultaba esa bandera cada
// 16-50 ms; ahora se invalida la ventana y Windows entrega un WM_PAINT, por lo que
// el hilo puede quedarse bloqueado en GetMessage cuando no hay nada que hacer.
// bErase = FALSE porque WM_ERASEBKGND se ignora (se pinta con doble buffer).
void RequestOverlayRedraw() {
    HWND overlay = hCurrentOverlay.load();
    if (overlay) {
        InvalidateRect(overlay, NULL, FALSE);
    }
}

// Función para cargar configuración desde archivo .ini
// Devuelve la ruta absoluta del archivo de configuracion, en
// %APPDATA%\ScreenHighlighter\. Se usa APPDATA y no la carpeta del ejecutable
// porque la aplicacion corre elevada y puede estar instalada en Program Files,
// donde la escritura falla.
std::wstring GetConfigFilePath() {
    static std::wstring cached;
    if (!cached.empty()) return cached;

    PWSTR appDataPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath))) {
        std::wstring dir = std::wstring(appDataPath) + L"\\ScreenHighlighter";
        CoTaskMemFree(appDataPath);
        SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
        cached = dir + L"\\" + CONFIG_FILE_NAME;
    } else {
        // Respaldo: junto al ejecutable.
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring exePathStr(exePath);
        size_t lastSlash = exePathStr.find_last_of(L"\\/");
        cached = exePathStr.substr(0, lastSlash + 1) + CONFIG_FILE_NAME;
    }
    return cached;
}

// Si existe un .ini de una version anterior junto al ejecutable y todavia no hay
// uno en APPDATA, se copia para no perder la configuracion del usuario.
void MigrateLegacyConfig() {
    const std::wstring target = GetConfigFilePath();
    if (GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) return;

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exePathStr(exePath);
    size_t lastSlash = exePathStr.find_last_of(L"\\/");
    const std::wstring legacy = exePathStr.substr(0, lastSlash + 1) + CONFIG_FILE_NAME;

    if (GetFileAttributesW(legacy.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CopyFileW(legacy.c_str(), target.c_str(), TRUE);
    }
}

void LoadConfiguration() {
    std::ifstream file(GetConfigFilePath().c_str());
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
                } else if (key == "drawing_color") {
                    drawing_color.store(std::stoi(value));
                } else if (key == "drawing_thickness") {
                    drawing_thickness.store(std::clamp(std::stoi(value), 1, 50));
                } else if (key == "drawing_fill") {
                    drawing_fill.store(std::stoi(value) != 0);
                } else if (key == "highlighter_alpha") {
                    highlighter_alpha.store(std::clamp(std::stoi(value), 1, 255));
                } else if (key == "screenshot_format") {
                    screenshot_format = (value == "bmp") ? L"bmp" : L"png";
                } else if (key == "screenshot_folder") {
                    // El .ini se lee como texto estrecho; se convierte con la
                    // pagina de codigos activa para soportar rutas con acentos.
                    if (value.empty()) {
                        screenshot_folder.clear();
                    } else {
                        int needed = MultiByteToWideChar(CP_ACP, 0, value.c_str(),
                                                         -1, NULL, 0);
                        if (needed > 0) {
                            std::wstring wide(needed - 1, L'\0');
                            MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1,
                                                &wide[0], needed);
                            screenshot_folder = wide;
                        }
                    }
                } else if (key == "resource_mode") {
                    int mode = std::stoi(value);
                    if (mode >= 0 && mode <= 2) {
                        current_resource_mode.store(static_cast<ResourceMode>(mode));
                    }
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
    std::ofstream file(GetConfigFilePath().c_str());
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
    file << "resource_mode=" << static_cast<int>(current_resource_mode.load()) << std::endl;

    // Estas cinco no se guardaban, por lo que el color, el grosor y el relleno
    // elegidos por el usuario se perdian en cada reinicio.
    file << "drawing_color=" << drawing_color.load() << std::endl;
    file << "drawing_thickness=" << drawing_thickness.load() << std::endl;
    file << "drawing_fill=" << (drawing_fill.load() ? 1 : 0) << std::endl;
    file << "highlighter_alpha=" << highlighter_alpha.load() << std::endl;

    // screenshot_format solo contiene "png" o "bmp", pero se convierte igual con
    // la API en vez de truncar wchar_t a char, que es el patron que se corrigio
    // en el resto del archivo.
    file << "screenshot_format=" << (screenshot_format == L"bmp" ? "bmp" : "png")
         << std::endl;

    {
        std::string narrowFolder;
        if (!screenshot_folder.empty()) {
            int needed = WideCharToMultiByte(CP_ACP, 0, screenshot_folder.c_str(),
                                             -1, NULL, 0, NULL, NULL);
            if (needed > 0) {
                narrowFolder.resize(needed - 1);
                WideCharToMultiByte(CP_ACP, 0, screenshot_folder.c_str(), -1,
                                    &narrowFolder[0], needed, NULL, NULL);
            }
        }
        file << "screenshot_folder=" << narrowFolder << std::endl;
    }
    
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

// Declaración forward para LoadGifFromFile
std::expected<GifElement, std::string> LoadGifFromFile(const std::wstring& filePath, int x, int y);

// Función para verificar si hay un GIF en el clipboard
bool CheckClipboardForGif() {
    if (OpenClipboard(NULL)) {
        // Verificar formatos de archivo
        bool hasGif = IsClipboardFormatAvailable(CF_HDROP) || 
                      IsClipboardFormatAvailable(CF_TEXT) ||
                      IsClipboardFormatAvailable(CF_UNICODETEXT);
        
        if (hasGif) {
            // Verificar si hay archivos GIF
            if (IsClipboardFormatAvailable(CF_HDROP)) {
                HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
                if (hDrop) {
                    UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
                    for (UINT i = 0; i < fileCount; i++) {
                        wchar_t filePath[MAX_PATH];
                        if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                            std::wstring path(filePath);
                            if (path.length() > 4 && 
                                (path.substr(path.length() - 4) == L".gif" || 
                                 path.substr(path.length() - 4) == L".GIF")) {
                                CloseClipboard();
                                return true;
                            }
                        }
                    }
                }
            }
        }
        CloseClipboard();
    }
    return false;
}

// Función para agregar GIF desde el clipboard
// Definida mas abajo, junto al resto del manejo de timers del overlay.
void EnsureGifTimer();

bool AddGifElement() {
    if (!OpenClipboard(NULL)) {
        return false;
    }
    
    bool success = false;
    
    // Verificar si hay archivos en el clipboard
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
        if (hDrop) {
            UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < fileCount; i++) {
                wchar_t filePath[MAX_PATH];
                if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                    std::wstring path(filePath);
                    if (path.length() > 4 && 
                        (path.substr(path.length() - 4) == L".gif" || 
                         path.substr(path.length() - 4) == L".GIF")) {
                        
                        // Cargar GIF desde archivo
                        auto gifResult = LoadGifFromFile(path, 0, 0);
                        if (gifResult.has_value()) {
                            GifElement& gifElement = gifResult.value();
                            
                            // Posicionar el GIF arriba de la región de texto
                            gifElement.x = 50; // Margen izquierdo
                            gifElement.y = 50; // Margen superior
                            
                            // Agregar a la lista de elementos GIF
                            gif_elements.push_back(std::move(gifElement));
                            
                            // Agregar marcador en el texto
                            const int gifIndex = (int)gif_elements.size() - 1;
                            zoom_text.reserve(zoom_text.length() + 20);
                            
                            wchar_t markerBuffer[32];
                            int markerLen = swprintf_s(markerBuffer, L"[GIF_%d]", gifIndex);
                            if (markerLen > 0) {
                                zoom_text.insert(text_cursor_pos.load(), markerBuffer, markerLen);
                                text_cursor_pos.store(text_cursor_pos.load() + markerLen);
                            }
                            
                            // Calcular cuántos saltos de línea necesita el texto para estar justo debajo del GIF
                            int gifHeight = gifElement.height;
                            int lineHeight = 20; // Altura estándar de línea de texto
                            int totalLines = (gifHeight + lineHeight - 1) / lineHeight; // Redondear hacia arriba
                            
                            // AJUSTE ESPECÍFICO PARA GIFs (texto y cursor sincronizados):
                            // 1. El texto sube 8 líneas más (totalLines - 21)
                            // 2. Insertar solo los saltos de línea necesarios para el texto
                            // 3. El cursor y el texto quedan en la MISMA LÍNEA, justo debajo del GIF
                            int textLines = std::max(0, totalLines - 21); // Texto sube 8 líneas más
                            
                            // Insertar solo los saltos de línea necesarios para el texto
                            for (int j = 0; j < textLines; j++) {
                                zoom_text.insert(text_cursor_pos.load(), L"\n");
                                text_cursor_pos.store(text_cursor_pos.load() + 1);
                            }
                            
                            // NO insertar líneas adicionales - el cursor y texto quedan sincronizados
                            // en la misma línea, justo debajo del GIF
                            
                            // El cursor ahora está en la posición correcta después del GIF
                            // SIGUE LA MISMA LÓGICA QUE LAS IMÁGENES:
                            // - Se inserta el marcador [GIF_X] y el cursor avanza
                            // - Se insertan los saltos de línea necesarios y el cursor avanza
                            // - El cursor queda posicionado exactamente donde debe estar para escribir
                            
                            // El texto sube 8 líneas más (totalLines - 21)
                            // y el cursor queda sincronizado con el texto en la misma línea
                            
                            // Arrancar la animacion: ya no hay un bucle que
                            // llame a UpdateGifFrames en cada iteracion.
                            EnsureGifTimer();

                            RequestOverlayRedraw();
                            success = true;
                            break; // Solo procesar el primer GIF
                        }
                    }
                }
            }
        }
    }
    
    CloseClipboard();
    return success;
}

// Función para cargar GIF desde archivo
std::expected<GifElement, std::string> LoadGifFromFile(const std::wstring& filePath, int x, int y) {
    try {
        // GDI+ ya fue inicializado por WinMain (InitializeGdiPlus).
        if (!g_gdiplusReady) {
            return std::unexpected("GDI+ no esta inicializado");
        }
        
        // Cargar imagen desde archivo
        Gdiplus::Image* image = Gdiplus::Image::FromFile(filePath.c_str());
        if (!image) {
            return std::unexpected("No se pudo cargar la imagen GIF");
        }
        
        // Verificar que sea un GIF
        if (image->GetFrameDimensionsCount() == 0) {
            delete image;
            return std::unexpected("The file is not a valid GIF");
        }
        
        // Obtener dimensiones del frame
        GUID frameDimension = Gdiplus::FrameDimensionTime;
        int frameCount = image->GetFrameCount(&frameDimension);
        
        if (frameCount <= 0) {
            delete image;
            return std::unexpected("No se pudieron obtener los frames del GIF");
        }
        
        // Crear elemento GIF
        GifElement gifElement(filePath, x, y, image->GetWidth(), image->GetHeight());
        
        // Obtener delays de los frames del GIF
        // Intentar extraer los delays originales usando GDI+
        // Nota: Usamos valores numéricos directamente ya que las constantes pueden no estar disponibles
        UINT size = image->GetPropertyItemSize(0x5100); // PropertyTagFrameDelay = 0x5100
        if (size > 0) {
            Gdiplus::PropertyItem* propertyItem = (Gdiplus::PropertyItem*)malloc(size);
            if (propertyItem) {
                if (image->GetPropertyItem(0x5100, size, propertyItem) == Gdiplus::Ok) {
                    // Los delays en GIFs están en centésimas de segundo (1/100 seg)
                    for (int i = 0; i < frameCount; i++) {
                        int delay = ((int*)propertyItem->value)[i] * 10; // Convertir a milisegundos
                        gifElement.delays.push_back(delay);
                    }
                } else {
                    // Si falla, usar delays por defecto
                    for (int i = 0; i < frameCount; i++) {
                        gifElement.delays.push_back(100); // 100ms por defecto
                    }
                }
                free(propertyItem);
            } else {
                // Si no se puede asignar memoria, usar delays por defecto
                for (int i = 0; i < frameCount; i++) {
                    gifElement.delays.push_back(100); // 100ms por defecto
                }
            }
        } else {
            // Si no hay información de delays, usar delays por defecto
            for (int i = 0; i < frameCount; i++) {
                gifElement.delays.push_back(100); // 100ms por defecto
            }
        }
        
        // Debug: mostrar información de los delays extraídos
        #ifdef DEBUG_BUILD
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"GIF cargado: %zu frames, delays: ", gifElement.delays.size());
        OutputDebugStringW(debugMsg);
        
        for (size_t i = 0; i < gifElement.delays.size(); i++) {
            wchar_t delayMsg[64];
            swprintf_s(delayMsg, L"%dms ", gifElement.delays[i]);
            OutputDebugStringW(delayMsg);
        }
        OutputDebugStringW(L"\n");
        #endif
        

        
        // Convertir frames a bitmaps
        for (int i = 0; i < frameCount; i++) {
            image->SelectActiveFrame(&frameDimension, i);
            
            // Crear bitmap del frame
            Gdiplus::Bitmap* frameBitmap = new Gdiplus::Bitmap(image->GetWidth(), image->GetHeight());
            Gdiplus::Graphics graphics(frameBitmap);
            graphics.DrawImage(image, 0, 0);
            
            // Convertir a HBITMAP
            HBITMAP hBitmap;
            frameBitmap->GetHBITMAP(Gdiplus::Color::Transparent, &hBitmap);
            
            if (hBitmap) {
                gifElement.frames.push_back(ScopedBitmap(hBitmap));
            }
            
            delete frameBitmap;
        }
        
        delete image;
        
        if (gifElement.frames.empty()) {
            return std::unexpected("No se pudieron extraer los frames del GIF");
        }
        
        return gifElement;
        
    } catch (const std::exception& e) {
        return std::unexpected("Error al cargar GIF: " + std::string(e.what()));
    }
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
                                        RequestOverlayRedraw();
                                        
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
    LogDebug("  🖼️ Configuring system tray icon...\n");
    
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hMainWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TASKBAR;
    
    // Cargar icono con gestión automática de memoria
    LogDebug("  🖼️ Cargando icono personalizado...\n");
    ScopedIcon hIcon(LoadIconFromFile(TRAY_ICON_SMALL));
    if (!hIcon) {
        // Si falla la carga del icono, usar un icono por defecto del sistema
        LogDebug("  ⚠️ Usando icono por defecto del sistema\n");
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    } else {
        LogDebug("  ✅ Icono personalizado cargado\n");
        nid.hIcon = hIcon.release(); // Transferir propiedad al nid
    }
    
    strcpy_s(nid.szTip, TRAY_TOOLTIP_TEXT);
    LogDebug("  💬 Tooltip configured: %s\n", TRAY_TOOLTIP_TEXT);
    
    LogDebug("  🔧 Agregando icono al system tray...\n");
    if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
        // Manejar error de notificación
        LogDebug("  ❌ Error al agregar icono al system tray\n");
        OutputDebugStringW(L"Error al agregar icono al system tray\n");
        return false;
    }
    
    LogDebug("  ✅ Icono agregado exitosamente al system tray\n");
    systemTrayInitialized = true;
    return true;
}

// Función para remover el icono del system tray
void RemoveFromSystemTray() {
    if (!Shell_NotifyIcon(NIM_DELETE, &nid)) {
        // Manejar error de eliminación
        OutputDebugStringW(L"Error al remover icono del system tray\nimage.png");
    }
    
    // Limpiar el icono si existe
    if (nid.hIcon) {
        DestroyIcon(nid.hIcon);
        nid.hIcon = nullptr;
    }
}

// Función para restaurar el icono del system tray
bool RestoreSystemTrayIcon() {
    LogDebug("🔄 Restaurando icono del system tray...\n");
    
    // Verificar si el icono ya está en el system tray
    if (systemTrayInitialized) {
        // Intentar restaurar el icono existente
        if (Shell_NotifyIcon(NIM_MODIFY, &nid)) {
            LogDebug("✅ Icono del system tray restaurado exitosamente\n");
            return true;
        }
    }
    
    // Si no se puede restaurar, agregar uno nuevo
    LogDebug("🆕 Agregando nuevo icono al system tray...\n");
    return AddToSystemTray();
}

// Función para monitorear el proceso explorer.exe
void MonitorExplorerProcess() {
    LogDebug("🔍 Iniciando monitoreo de explorer.exe...\n");
    
    // Obtener el PID de explorer.exe
    HWND shellTrayWnd = FindWindowW(L"Shell_TrayWnd", NULL);
    if (shellTrayWnd) {
        GetWindowThreadProcessId(shellTrayWnd, &explorerProcessId);
        LogDebug("📱 Explorer.exe PID: %lu\n", explorerProcessId);
        
        if (explorerProcessId > 0) {
            explorerProcessHandle = OpenProcess(SYNCHRONIZE, FALSE, explorerProcessId);
            if (explorerProcessHandle) {
                LogDebug("✅ Monitoreo de explorer.exe iniciado\n");
                
                // Monitorear continuamente
                while (explorerMonitorRunning.load()) {
                    // Esperar a que explorer.exe termine
                    DWORD waitResult = WaitForSingleObject(explorerProcessHandle, 1000); // 1 segundo
                    
                    if (waitResult == WAIT_OBJECT_0) {
                        // Explorer.exe terminó
                        LogDebug("⚠️ Explorer.exe terminated - Restoring system tray...\n");
                        systemTrayRestorationNeeded.store(true);
                        
                        // Esperar a que explorer.exe se reinicie
                        Sleep(2000); // 2 segundos
                        
                        // Restaurar el icono del system tray
                        if (RestoreSystemTrayIcon()) {
                            LogDebug("✅ System tray restored after explorer.exe restart\n");
                        } else {
                            LogDebug("❌ Error al restaurar system tray\n");
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
                                    LogDebug("✅ Monitoreo de explorer.exe reiniciado\n");
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
                                LogDebug("⚠️ Icono del system tray no responde - Restaurando...\n");
                                if (RestoreSystemTrayIcon()) {
                                    LogDebug("✅ System tray restaurado\n");
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
    
    LogDebug("🔍 Monitoreo de explorer.exe terminado\n");
}

// Función para iniciar el monitoreo de explorer.exe
void StartExplorerMonitoring() {
    if (!explorerMonitorRunning.load()) {
        LogDebug("🚀 Iniciando monitoreo de explorer.exe...\n");
        explorerMonitorRunning.store(true);
        explorerMonitorThread = std::thread(MonitorExplorerProcess);
        LogDebug("✅ Monitoreo de explorer.exe iniciado\n");
    }
}

// Función para detener el monitoreo de explorer.exe
void StopExplorerMonitoring() {
    if (explorerMonitorRunning.load()) {
        LogDebug("🛑 Deteniendo monitoreo de explorer.exe...\n");
        explorerMonitorRunning.store(false);
        
        if (explorerMonitorThread.joinable()) {
            explorerMonitorThread.join();
        }
        
        if (explorerProcessHandle) {
            CloseHandle(explorerProcessHandle);
            explorerProcessHandle = NULL;
        }
        
        LogDebug("✅ Monitoreo de explorer.exe detenido\n");
    }
}

// Declaraciones adelantadas
bool IsRunningAsAdministrator();
void ShowAutoStartStatus();

// Función para verificar si la aplicación está configurada para auto-ejecutarse
bool IsAutoStartEnabled() {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, 
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
        0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        LogDebug("❌ Error opening registry key to verify auto-start: %ld\n", result);
        return false;
    }
    
    wchar_t valueData[MAX_PATH];
    DWORD dataSize = sizeof(valueData);
    DWORD dataType = REG_SZ;
    
    // Verificar si existe el valor "Screen Highlighter"
    result = RegQueryValueExW(hKey, L"Screen Highlighter", NULL, &dataType, 
                             (LPBYTE)valueData, &dataSize);
    
    RegCloseKey(hKey);
    
    if (result == ERROR_SUCCESS) {
        LogDebug("✅ Auto-start value found: %ls\n", valueData);
        return true;
    } else if (result == ERROR_FILE_NOT_FOUND) {
        LogDebug("ℹ️ Auto-start value not found\n");
        return false;
    } else {
        LogDebug("❌ Error reading auto-start value: %ld\n", result);
        return false;
    }
}

// Función para habilitar la auto-ejecución al iniciar sesión
bool EnableAutoStart() {
    LogDebug("🔧 Attempting to enable auto-start...\n");
    
    // Verificar permisos de administrador
    if (!IsRunningAsAdministrator()) {
        LogDebug("❌ Administrator privileges required to configure auto-start\n");
        return false;
    }
    
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, 
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
        0, KEY_WRITE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        LogDebug("❌ Error opening registry key for auto-start: %ld\n", result);
        return false;
    }
    
    // Obtener la ruta completa del ejecutable
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        LogDebug("❌ Error al obtener ruta del ejecutable: %ld\n", GetLastError());
        RegCloseKey(hKey);
        return false;
    }
    
    LogDebug("📁 Ruta del ejecutable: %ls\n", exePath);
    
    // Verificar que el archivo existe
    DWORD fileAttributes = GetFileAttributesW(exePath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        LogDebug("❌ El archivo ejecutable no existe o no es accesible: %ld\n", GetLastError());
        RegCloseKey(hKey);
        return false;
    }
    
    // Crear la entrada en el registro
    result = RegSetValueExW(hKey, L"Screen Highlighter", 0, REG_SZ, 
        (const BYTE*)exePath, (wcslen(exePath) + 1) * sizeof(wchar_t));
    
    RegCloseKey(hKey);
    
    if (result == ERROR_SUCCESS) {
        LogDebug("✅ Auto-start enabled successfully in registry\n");
        
        // Verificar que se escribió correctamente
        if (IsAutoStartEnabled()) {
            LogDebug("✅ Verification successful: auto-start is enabled\n");
            return true;
        } else {
            LogDebug("⚠️ Auto-start was written but cannot be verified\n");
            return false;
        }
    } else {
        LogDebug("❌ Error configuring auto-start in registry: %ld\n", result);
        return false;
    }
}

// Función para deshabilitar la auto-ejecución al iniciar sesión
bool DisableAutoStart() {
    LogDebug("🔧 Attempting to disable auto-start...\n");
    
    // Verificar permisos de administrador
    if (!IsRunningAsAdministrator()) {
        LogDebug("❌ Administrator privileges required to configure auto-start\n");
        return false;
    }
    
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, 
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
        0, KEY_WRITE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        LogDebug("❌ Error opening registry key for auto-start: %ld\n", result);
        return false;
    }
    
    // Eliminar la entrada del registro
    result = RegDeleteValueW(hKey, L"Screen Highlighter");
    
    RegCloseKey(hKey);
    
    if (result == ERROR_SUCCESS) {
        LogDebug("✅ Auto-start disabled successfully from registry\n");
        
        // Verificar que se eliminó correctamente
        if (!IsAutoStartEnabled()) {
            LogDebug("✅ Verification successful: auto-start is disabled\n");
            return true;
        } else {
            LogDebug("⚠️ Auto-start was removed but cannot be verified\n");
            return false;
        }
    } else if (result == ERROR_FILE_NOT_FOUND) {
        LogDebug("ℹ️ Auto-start was already disabled\n");
        return true;
    } else {
        LogDebug("❌ Error disabling auto-start from registry: %ld\n", result);
        return false;
    }
}

// Función para mostrar el menú contextual del system tray
void ShowTrayMenu() {
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, MENU_ACTIVATE_ID, MENU_ACTIVATE_TEXT);
    AppendMenuW(hMenu, MF_STRING, MENU_SETTINGS_ID, L"⚙️ Settings");
    
    // Agregar opciones de modo de recursos
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Submenu para modos de recursos
    HMENU hResourceMenu = CreatePopupMenu();
    
    // Marcar el modo actual
    ResourceMode currentMode = current_resource_mode.load();
    UINT normalFlag = (currentMode == ResourceMode::Normal) ? MF_CHECKED : MF_UNCHECKED;
    UINT lowFlag = (currentMode == ResourceMode::LowResources) ? MF_CHECKED : MF_UNCHECKED;
    UINT highFlag = (currentMode == ResourceMode::HighPerformance) ? MF_CHECKED : MF_UNCHECKED;
    
    AppendMenuW(hResourceMenu, MF_STRING | normalFlag, 1010, L"🔧 Normal Mode");
    AppendMenuW(hResourceMenu, MF_STRING | lowFlag, MENU_LOW_RESOURCES_ID, L"🔋 Low Resources Mode");
    AppendMenuW(hResourceMenu, MF_STRING | highFlag, MENU_HIGH_PERFORMANCE_ID, L"⚡ High Performance Mode");
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hResourceMenu, L"🎯 Resource Mode");
    
    // Agregar opciones de auto-ejecución
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    if (IsAutoStartEnabled()) {
        AppendMenuW(hMenu, MF_STRING, MENU_DISABLE_AUTOSTART_ID, L"🚫 Disable Auto-Start");
    } else {
        AppendMenuW(hMenu, MF_STRING, MENU_ENABLE_AUTOSTART_ID, L"✅ Enable Auto-Start");
    }
    
    // Add option to show detailed status
    AppendMenuW(hMenu, MF_STRING, 1007, L"🔍 Auto-Start Status");
    
    AppendMenuW(hMenu, MF_SEPARATOR, MENU_SEPARATOR_ID, MENU_SEPARATOR_TEXT);
    AppendMenuW(hMenu, MF_STRING, MENU_EXIT_ID, MENU_EXIT_TEXT);
    
    SetForegroundWindow(hMainWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hMainWnd, NULL);
    DestroyMenu(hMenu);
}

// Declaraciones adelantadas: CaptureScreenRegion redibuja las anotaciones sobre
// la captura, y estas primitivas se definen mas abajo en este archivo.
void DrawHighlighter(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color);
void DrawEllipseShape(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color,
                      int thickness, bool filled);
void DrawPenStroke(HDC hdc, const std::vector<POINT>& points, COLORREF color,
                   int thickness);
void DrawStepMarker(HDC hdc, int x, int y, int number, COLORREF color, int thickness);
void DrawRedaction(HDC hdcDest, HDC hdcSrc, int x1, int y1, int x2, int y2,
                   int blockSize);

// Función para dibujar línea (optimizada)
void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness) {
    // Lapiz cacheado: antes se creaba y destruia uno en cada llamada.
    HPEN hPen = GdiCache::GetPenCached(color, thickness);
    if (!hPen) return;

    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, hOldPen);
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

// Función para dibujar rectángulo (optimizada)
void DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness, bool filled) {
    // Lapiz cacheado (ver GdiCache::GetPenCached).
    HPEN hPen = GdiCache::GetPenCached(color, thickness);
    if (!hPen) return;

    // El pincel solo se necesita si hay relleno.
    ScopedBrush hBrush;
    if (filled) {
        hBrush.reset(CreateSolidBrush(color), true);
        if (!hBrush) return;
    }

    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, filled ? (HGDIOBJ)hBrush.get()
                                                 : GetStockObject(NULL_BRUSH));

    Rectangle(hdc, x1, y1, x2, y2);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
}

// Función para reproducir sonido de captura
// Sonido de confirmacion de captura.
//
// Antes: Beep(2400, 800). Beep() es sincronico, por lo que congelaba el hilo
// (y con el la interfaz) durante 800 ms en cada captura. PlaySound con SND_ASYNC
// retorna de inmediato.
void PlayScreenshotSound() {
    PlaySoundW(L"SystemAsterisk", NULL,
               SND_ALIAS | SND_ASYNC | SND_NODEFAULT | SND_NOWAIT);
}

// Función auxiliar para guardar bitmap como archivo BMP
// Obtiene el CLSID del codificador GDI+ para un tipo MIME ("image/png", "image/bmp").
bool GetEncoderClsid(const wchar_t* mimeType, CLSID* clsid) {
    UINT num = 0, size = 0;
    if (Gdiplus::GetImageEncodersSize(&num, &size) != Gdiplus::Ok || size == 0) {
        return false;
    }

    std::vector<BYTE> buffer(size);
    Gdiplus::ImageCodecInfo* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(num, size, codecs) != Gdiplus::Ok) {
        return false;
    }

    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(codecs[i].MimeType, mimeType) == 0) {
            *clsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

// Guarda un HBITMAP como PNG real usando GDI+.
//
// Antes las capturas se escribian con SaveBitmapToFile, que emite un BMP, pero el
// nombre de archivo terminaba en .png: cada "captura.png" era en realidad un BMP mal
// etiquetado (~6 MB en 1080p, ~25 MB en 4K) que los visores estrictos rechazan.
// GDI+ recibe la ruta como wchar_t, por lo que tampoco hace falta convertir a
// std::string (esa conversion truncaba los acentos de la ruta).
bool SaveBitmapAsPng(HBITMAP hBitmap, const std::wstring& filePath) {
    if (!hBitmap || !g_gdiplusReady) return false;

    CLSID pngClsid;
    if (!GetEncoderClsid(L"image/png", &pngClsid)) return false;

    // FromHBITMAP copia los pixeles, asi que el bitmap original sigue siendo del
    // llamador y puede liberarse despues.
    Gdiplus::Bitmap bitmap(hBitmap, static_cast<HPALETTE>(nullptr));
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return false;

    return bitmap.Save(filePath.c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
}

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
    
    // std::ofstream acepta wchar_t* en Windows. Antes se hacia
    // std::string(filePath.begin(), filePath.end()), que trunca cada wchar_t a char
    // y rompia cualquier ruta con acentos (p. ej. C:\\Users\\Jose\\...).
    std::ofstream file(filePath.c_str(), std::ios::binary);
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
// Devuelve la carpeta destino de las capturas, creandola si hace falta.
//
// Antes se guardaba junto al .exe. La aplicacion corre elevada y puede estar
// instalada en Program Files, donde la escritura falla; por eso ahora se usa por
// defecto la carpeta de imagenes del usuario.
std::wstring GetScreenshotFolder() {
    if (!screenshot_folder.empty()) {
        SHCreateDirectoryExW(NULL, screenshot_folder.c_str(), NULL);
        return screenshot_folder;
    }

    PWSTR picturesPath = nullptr;
    std::wstring folder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &picturesPath))) {
        folder = std::wstring(picturesPath) + L"\\Screenshots";
        CoTaskMemFree(picturesPath);
    } else {
        // Respaldo: la carpeta del ejecutable.
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring exePathStr(exePath);
        size_t lastSlash = exePathStr.find_last_of(L"\\/");
        folder = exePathStr.substr(0, lastSlash);
    }

    SHCreateDirectoryExW(NULL, folder.c_str(), NULL);
    return folder;
}

// Guarda la captura en disco.
//
// Correcciones respecto a la version anterior:
//  - Escribe un PNG real. Antes se generaba un BMP con extension .png, es decir
//    cada "captura.png" era un BMP mal etiquetado (~6 MB en 1080p, ~25 MB en 4K).
//  - Incluye milisegundos y un contador anticolision: con resolucion de 1 segundo
//    dos capturas consecutivas se sobreescribian.
//  - Informa el fallo. Antes las dos ramas del if estaban vacias, asi que un
//    error de escritura pasaba totalmente inadvertido.
void SaveScreenshotToDownloads(HBITMAP hBitmap, int x1, int y1, int x2, int y2) {
    (void)x1; (void)y1; (void)x2; (void)y2; // Parámetros no utilizados

    const bool useBmp = (screenshot_format == L"bmp");
    const wchar_t* ext = useBmp ? L"bmp" : L"png";
    const std::wstring folder = GetScreenshotFolder();

    SYSTEMTIME st;
    GetLocalTime(&st);

    std::wstring fullPath;
    wchar_t filename[256];

    for (int attempt = 0; attempt < 1000; ++attempt) {
        if (attempt == 0) {
            swprintf_s(filename, L"%02d_%02d_%04d-%02d-%02d-%02d_%03d.%s",
                       st.wDay, st.wMonth, st.wYear,
                       st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, ext);
        } else {
            swprintf_s(filename, L"%02d_%02d_%04d-%02d-%02d-%02d_%03d_%d.%s",
                       st.wDay, st.wMonth, st.wYear,
                       st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                       attempt, ext);
        }
        fullPath = folder + L"\\" + filename;
        if (GetFileAttributesW(fullPath.c_str()) == INVALID_FILE_ATTRIBUTES) break;
    }

    const bool saved = useBmp ? SaveBitmapToFile(hBitmap, fullPath)
                              : SaveBitmapAsPng(hBitmap, fullPath);

    if (!saved) {
        OutputDebugStringW((L"Screen Highlighter: fallo al guardar " + fullPath +
                            L"\n").c_str());
        MessageBoxW(NULL,
            (L"No se pudo guardar la captura en:\n\n" + fullPath +
             L"\n\nVerifique los permisos de la carpeta.").c_str(),
            L"Screen Highlighter", MB_OK | MB_ICONWARNING);
    }
}

// Función para capturar región de pantalla
// Captura una region de la pantalla, la copia al portapapeles y la guarda.
//
// Correcciones respecto a la version anterior:
//  - A2: se hacia SetClipboardData(CF_BITMAP, hBitmap) y despues
//    DeleteObject(hBitmap). SetClipboardData transfiere la propiedad del handle al
//    portapapeles, asi que el DeleteObject destruia el bitmap que el portapapeles
//    acababa de recibir: al pegar no aparecia nada o aparecia basura. Ademas el
//    bitmap seguia seleccionado en el DC de memoria, lo que tampoco es valido.
//    Ahora se deselecciona primero y se entrega una copia independiente.
//  - A9: el overlay (topmost y layered) seguia visible durante el BitBlt, por lo
//    que el tinte oscuro quedaba grabado en la captura. Ahora se oculta el overlay,
//    se captura el escritorio limpio y se vuelve a mostrar.
//  - A8: se corrigio la fuga del DC de memoria en el retorno temprano.
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

    // Reducir ligeramente la región para excluir el borde de selección
    const int borderOffset = 2;
    int captureLeft = left + borderOffset;
    int captureTop = top + borderOffset;
    int captureRight = right - borderOffset;
    int captureBottom = bottom - borderOffset;

    if (captureRight <= captureLeft || captureBottom <= captureTop) {
        return;
    }

    const int width = captureRight - captureLeft;
    const int height = captureBottom - captureTop;

    // Ocultar el overlay para que su tinte no quede grabado en la captura.
    HWND overlay = hCurrentOverlay.load();
    bool overlayWasVisible = false;
    if (overlay && IsWindowVisible(overlay)) {
        overlayWasVisible = true;
        ShowWindow(overlay, SW_HIDE);
        // Dar tiempo a que DWM recomponga el escritorio sin el overlay.
        UpdateWindow(GetDesktopWindow());
        Sleep(30);
    }

    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) {
        if (overlayWasVisible) ShowWindow(overlay, SW_SHOW);
        return;
    }

    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) {
        ReleaseDC(NULL, hScreenDC);
        if (overlayWasVisible) ShowWindow(overlay, SW_SHOW);
        return;
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    if (!hBitmap) {
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreenDC);
        if (overlayWasVisible) ShowWindow(overlay, SW_SHOW);
        return;
    }

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    // Copiar el escritorio limpio (sin overlay).
    // Las coordenadas vienen en espacio de cliente del overlay; el DC de pantalla
    // usa coordenadas de pantalla, que difieren por el origen virtual.
    BitBlt(hMemDC, 0, 0, width, height, hScreenDC,
           ClientToScreenX(captureLeft), ClientToScreenY(captureTop), SRCCOPY);

    // Volver a dibujar las anotaciones sobre la captura, trasladadas al origen de
    // la region. Asi la captura conserva los dibujos pero no el tinte del overlay.
    {
        std::lock_guard<std::mutex> lock(g_annotationMutex);
        SetViewportOrgEx(hMemDC, -captureLeft, -captureTop, NULL);
        for (const auto& element : drawing_elements) {
            switch (element.tool_type) {
                case DrawingTool::Line:
                    DrawLine(hMemDC, element.x1, element.y1, element.x2, element.y2,
                             element.color, element.thickness);
                    break;
                case DrawingTool::Arrow:
                    DrawArrow(hMemDC, element.x1, element.y1, element.x2, element.y2,
                              element.color, element.thickness);
                    break;
                case DrawingTool::Rectangle:
                    DrawRectangle(hMemDC, element.x1, element.y1, element.x2, element.y2,
                                  element.color, element.thickness, element.filled);
                    break;
                case DrawingTool::Highlighter:
                    DrawHighlighter(hMemDC, element.x1, element.y1, element.x2, element.y2,
                                    element.color);
                    break;
                case DrawingTool::Ellipse:
                    DrawEllipseShape(hMemDC, element.x1, element.y1, element.x2, element.y2,
                                     element.color, element.thickness, element.filled);
                    break;
                case DrawingTool::Pen:
                    DrawPenStroke(hMemDC, element.points, element.color, element.thickness);
                    break;
                case DrawingTool::Redact:
                    // Se pixela desde la captura limpia recien tomada, no desde la
                    // copia de apertura, para reflejar el contenido actual.
                    DrawRedaction(hMemDC, hMemDC, element.x1, element.y1,
                                  element.x2, element.y2, element.thickness * 4);
                    break;
                case DrawingTool::Step:
                    DrawStepMarker(hMemDC, element.x1, element.y1, element.step_number,
                                   element.color, element.thickness);
                    break;
                default:
                    break;
            }
        }
        SetViewportOrgEx(hMemDC, 0, 0, NULL);
    }

    // Restaurar el overlay antes de las operaciones lentas (guardado en disco).
    if (overlayWasVisible) ShowWindow(overlay, SW_SHOW);

    // IMPORTANTE: deseleccionar el bitmap del DC antes de usarlo o publicarlo.
    SelectObject(hMemDC, hOldBitmap);

    // Guardar en disco (usa el bitmap, no lo consume).
    SaveScreenshotToDownloads(hBitmap, left, top, right, bottom);

    // Copiar al portapapeles una COPIA independiente: el portapapeles se vuelve
    // dueño de lo que recibe, y este lado sigue siendo dueño de hBitmap.
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HBITMAP hClipboardCopy = (HBITMAP)CopyImage(hBitmap, IMAGE_BITMAP, 0, 0, 0);
        if (hClipboardCopy) {
            if (!SetClipboardData(CF_BITMAP, hClipboardCopy)) {
                DeleteObject(hClipboardCopy); // el portapapeles no lo acepto
            }
        }
        CloseClipboard();
    }

    // Reproducir sonido de confirmación (asincrono)
    PlayScreenshotSound();

    // Limpiar recursos propios
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
}

// Dibuja una elipse. Espeja a DrawRectangle.
void DrawEllipseShape(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color,
                      int thickness, bool filled) {
    HPEN hPen = GdiCache::GetPenCached(color, thickness);
    if (!hPen) return;

    ScopedBrush hBrush;
    if (filled) {
        hBrush.reset(CreateSolidBrush(color), true);
        if (!hBrush) return;
    }

    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, filled ? (HGDIOBJ)hBrush.get()
                                                 : GetStockObject(NULL_BRUSH));
    Ellipse(hdc, x1, y1, x2, y2);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
}

// Dibuja un trazo libre.
void DrawPenStroke(HDC hdc, const std::vector<POINT>& points, COLORREF color,
                   int thickness) {
    if (points.size() < 2) {
        // Un solo punto: dibujar un punto grueso para que se vea algo.
        if (points.size() == 1) {
            HPEN hPen = GdiCache::GetPenCached(color, thickness);
            if (!hPen) return;
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, points[0].x, points[0].y, NULL);
            LineTo(hdc, points[0].x + 1, points[0].y);
            SelectObject(hdc, hOldPen);
        }
        return;
    }

    HPEN hPen = GdiCache::GetPenCached(color, thickness);
    if (!hPen) return;

    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    // Polyline dibuja todo el trazo en una sola llamada a GDI.
    Polyline(hdc, points.data(), static_cast<int>(points.size()));
    SelectObject(hdc, hOldPen);
}

// Dibuja un marcador de paso: circulo relleno con un numero centrado.
void DrawStepMarker(HDC hdc, int x, int y, int number, COLORREF color, int thickness) {
    const int radius = std::clamp(10 + thickness * 2, 12, 40);

    ScopedBrush hBrush(CreateSolidBrush(color), true);
    if (!hBrush) return;

    HPEN hPen = GdiCache::GetPenCached(RGB(255, 255, 255), 2);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, (HGDIOBJ)hBrush.get());

    Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);

    wchar_t label[16];
    swprintf_s(label, L"%d", number);

    HFONT hOldFont = (HFONT)SelectObject(hdc, GdiCache::hCachedFontIndicator);
    const int oldMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(hdc, RGB(255, 255, 255));

    RECT r = {x - radius, y - radius, x + radius, y + radius};
    DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldMode);
    SelectObject(hdc, hOldFont);
}

// Pixela una region para ocultar informacion sensible.
//
// Reduce el area a 1/blockSize con StretchBlt y la vuelve a ampliar, lo que
// produce bloques grandes. Es la forma mas barata de censurar con GDI puro y no
// se puede revertir, a diferencia de un desenfoque suave.
//
// hdcSrc debe contener pixeles reales del escritorio (la copia limpia), porque en
// el back buffer del overlay esa zona es color clave, no contenido.
void DrawRedaction(HDC hdcDest, HDC hdcSrc, int x1, int y1, int x2, int y2,
                   int blockSize) {
    const int left = std::min(x1, x2);
    const int top = std::min(y1, y2);
    const int width = std::abs(x2 - x1);
    const int height = std::abs(y2 - y1);
    if (width <= 0 || height <= 0) return;

    if (!hdcSrc) {
        // Sin fuente disponible: rellenar en negro. Censurar de mas es preferible
        // a dejar el contenido visible.
        RECT r = {left, top, left + width, top + height};
        FillRect(hdcDest, &r, GdiCache::hBlackBrush);
        return;
    }

    blockSize = std::clamp(blockSize, 2, 64);
    const int smallW = std::max(1, width / blockSize);
    const int smallH = std::max(1, height / blockSize);

    HDC hTmpDC = CreateCompatibleDC(hdcSrc);
    if (!hTmpDC) return;

    HBITMAP hTmpBmp = CreateCompatibleBitmap(hdcSrc, smallW, smallH);
    if (!hTmpBmp) {
        DeleteDC(hTmpDC);
        return;
    }

    HBITMAP hOldTmp = (HBITMAP)SelectObject(hTmpDC, hTmpBmp);

    // Reducir (promediando) y luego ampliar sin suavizado.
    SetStretchBltMode(hTmpDC, HALFTONE);
    SetBrushOrgEx(hTmpDC, 0, 0, NULL);
    StretchBlt(hTmpDC, 0, 0, smallW, smallH,
               hdcSrc, left, top, width, height, SRCCOPY);

    SetStretchBltMode(hdcDest, COLORONCOLOR);
    StretchBlt(hdcDest, left, top, width, height,
               hTmpDC, 0, 0, smallW, smallH, SRCCOPY);

    SelectObject(hTmpDC, hOldTmp);
    DeleteObject(hTmpBmp);
    DeleteDC(hTmpDC);
}

// Función para dibujar resaltador (capa amarilla casi opaca)
// Dibuja una capa translucida (resaltador).
//
// Antes se usaba SetROP2(hdc, R2_MASKPEN), que hace un AND bit a bit entre el
// color y el destino. Fuera del modo zoom el destino es el overlay negro, y
// amarillo AND negro = negro: el resaltado era literalmente invisible. Dentro del
// zoom el destino es la captura de pantalla (pixeles claros), por lo que ahi si se
// veia. Eso explica el bug conocido "el resaltador no funciona fuera del zoom".
//
// Ahora se usa AlphaBlend, que mezcla de verdad contra cualquier fondo, y se
// respeta el color elegido por el usuario (antes se descartaba con (void)color y
// el resaltador era siempre amarillo).
void DrawHighlighter(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    const int left = std::min(x1, x2);
    const int top = std::min(y1, y2);
    const int width = std::abs(x2 - x1);
    const int height = std::abs(y2 - y1);
    if (width <= 0 || height <= 0) return;

    const BYTE alpha = static_cast<BYTE>(
        std::clamp(highlighter_alpha.load(), 1, 255));

    if (GdiCache::hBlendDC && GdiCache::pBlendPixel) {
        // AlphaBlend con AlphaFormat = 0 ignora el canal alfa del origen y usa
        // SourceConstantAlpha, asi que basta un origen de 1x1 estirado.
        // El DIB es BGRA en memoria.
        *GdiCache::pBlendPixel = (static_cast<DWORD>(GetRValue(color)) << 16) |
                                 (static_cast<DWORD>(GetGValue(color)) << 8) |
                                  static_cast<DWORD>(GetBValue(color)) |
                                 (0xFFu << 24);

        BLENDFUNCTION blend;
        blend.BlendOp = AC_SRC_OVER;
        blend.BlendFlags = 0;
        blend.SourceConstantAlpha = alpha;
        blend.AlphaFormat = 0;

        if (AlphaBlend(hdc, left, top, width, height,
                       GdiCache::hBlendDC, 0, 0, 1, 1, blend)) {
            return;
        }
    }

    // Respaldo si AlphaBlend no esta disponible: relleno opaco. Sigue siendo
    // visible sobre cualquier fondo, que es lo que fallaba antes.
    ScopedBrush hBrush(CreateSolidBrush(color), true);
    if (!hBrush) return;
    RECT r = {left, top, left + width, top + height};
    FillRect(hdc, &r, hBrush);
}

// ============================================================================
// RENDERIZADO DE TEXTO DE ANOTACION (unificado)
// ============================================================================
// Antes existian dos copias casi identicas de este codigo dentro de DrawOverlay,
// una para el modo zoom y otra para el modo normal, y habian divergido: solo la
// copia del zoom dibujaba los marcadores [IMAGE_n]/[GIF_n], y la copia normal
// exigia text_input_mode para mostrar algo. De ahi venian los dos bugs conocidos
// ("pegar imagen no funciona fuera del zoom" y el texto que desaparecia).

namespace TextRender {

constexpr int kLineHeight = 20;     // altura de una linea de texto
constexpr int kPadding = 10;        // margen horizontal dentro del cuadro
constexpr int kMinBoxWidth = 100;
constexpr int kMaxBoxWidth = 1200;

// Una linea ya medida y posicionada.
struct LineBox {
    std::wstring content;
    size_t startPos = 0;    // offset en zoom_text donde empieza la linea
    size_t endPos = 0;      // offset donde termina (antes del salto de linea)
    int y = 0;              // posicion Y relativa al inicio del cuadro
    int height = kLineHeight;
    int imageIndex = -1;    // >= 0 si la linea representa una imagen
    int gifIndex = -1;      // >= 0 si la linea representa un GIF
};

// Extrae el indice de un marcador tipo "[IMAGE_3]" o "[GIF_2]".
// Devuelve -1 si la linea no contiene ese marcador o no se puede interpretar.
int ParseMarker(const std::wstring& line, const wchar_t* prefix, size_t prefixLen) {
    const size_t start = line.find(prefix);
    if (start == std::wstring::npos) return -1;
    const size_t end = line.find(L']', start);
    if (end == std::wstring::npos) return -1;

    const std::wstring digits = line.substr(start + prefixLen, end - start - prefixLen);
    if (digits.empty()) return -1;
    for (wchar_t c : digits) {
        if (c < L'0' || c > L'9') return -1;   // evita depender de excepciones
    }
    try {
        return std::stoi(digits);
    } catch (...) {
        return -1;
    }
}

// Construye el layout completo del texto: una pasada, reutilizada despues para
// dibujar, para el resaltado de seleccion y para ubicar el cursor. Antes el texto
// se recorria tres veces por frame (ancho, dibujo y cursor).
//
// Requiere que el llamador ya tenga tomado g_annotationMutex.
std::vector<LineBox> BuildLayout(HDC hdc, const std::wstring& text, int* outBoxWidth) {
    std::vector<LineBox> boxes;
    int boxWidth = kMinBoxWidth;

    const std::vector<GdiCache::TextLine> lines = GdiCache::ProcessTextLines(text);
    boxes.reserve(lines.size());

    int y = 0;
    for (const auto& line : lines) {
        LineBox box;
        box.content = line.content;
        box.startPos = line.startPos;
        box.endPos = line.endPos;
        box.y = y;
        box.imageIndex = ParseMarker(line.content, L"[IMAGE_", 7);
        box.gifIndex = (box.imageIndex >= 0)
                           ? -1
                           : ParseMarker(line.content, L"[GIF_", 5);

        if (box.imageIndex >= 0 &&
            box.imageIndex < static_cast<int>(clipboard_images.size()) &&
            clipboard_images[box.imageIndex]) {
            const SIZE sz = GdiCache::GetBitmapSize(clipboard_images[box.imageIndex].get());
            if (sz.cx > 0 && sz.cy > 0) {
                box.height = static_cast<int>(sz.cy) + 5;
                boxWidth = std::max(boxWidth, static_cast<int>(sz.cx) + 2 * kPadding);
            } else {
                box.imageIndex = -1;   // bitmap invalido: tratar como texto
            }
        } else if (box.gifIndex >= 0 &&
                   box.gifIndex < static_cast<int>(gif_elements.size()) &&
                   !gif_elements[box.gifIndex].frames.empty()) {
            const GifElement& gif = gif_elements[box.gifIndex];
            box.height = gif.height + 5;
            boxWidth = std::max(boxWidth, gif.width + 2 * kPadding);
        } else {
            box.imageIndex = -1;
            box.gifIndex = -1;
            if (!box.content.empty()) {
                const SIZE sz = GdiCache::GetTextSizeCached(hdc, box.content);
                boxWidth = std::max(boxWidth, static_cast<int>(sz.cx) + 4 * kPadding);
            }
        }

        y += box.height;
        boxes.push_back(std::move(box));
    }

    if (outBoxWidth) *outBoxWidth = std::clamp(boxWidth, kMinBoxWidth, kMaxBoxWidth);
    return boxes;
}

// Dibuja una imagen o un frame de GIF ya seleccionado en un DC temporal.
void BlitBitmap(HDC hdc, HBITMAP hBitmap, int x, int y, int w, int h) {
    if (!hBitmap || w <= 0 || h <= 0) return;
    HDC hTmp = CreateCompatibleDC(hdc);
    if (!hTmp) return;
    HBITMAP hOld = (HBITMAP)SelectObject(hTmp, hBitmap);
    BitBlt(hdc, x, y, w, h, hTmp, 0, 0, SRCCOPY);
    SelectObject(hTmp, hOld);
    DeleteDC(hTmp);
}

// Dibuja el texto de anotacion, sus imagenes/GIFs incrustados, el resaltado de
// seleccion y el cursor.
//
//   anchorX/anchorY : esquina superior izquierda del cuadro de texto.
//   anchorWidth     : si es > 0, el cuadro se centra dentro de
//                     [anchorX, anchorX + anchorWidth] (modo zoom). Si es 0, el
//                     cuadro se alinea a la izquierda en anchorX (modo normal).
//
// Requiere que el llamador ya tenga tomado g_annotationMutex.
void RenderAnnotationText(HDC hdc, HFONT font, int anchorX, int anchorY, int anchorWidth) {
    if (zoom_text.empty()) return;

    HFONT hOldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    int boxWidth = kMinBoxWidth;
    const std::vector<LineBox> boxes = BuildLayout(hdc, zoom_text, &boxWidth);

    const int boxX = (anchorWidth > 0) ? anchorX + (anchorWidth - boxWidth) / 2
                                       : anchorX;
    const int boxY = anchorY;
    const int textX = boxX + kPadding;

    // Rango de seleccion normalizado.
    bool hasSelection = false;
    int selStart = 0, selEnd = 0;
    if (text_selection_active.load()) {
        selStart = std::min(text_selection_start.load(), text_selection_end.load());
        selEnd = std::max(text_selection_start.load(), text_selection_end.load());
        hasSelection = (selStart >= 0 && selEnd > selStart);
    }

    for (const auto& box : boxes) {
        const int lineY = boxY + box.y;

        if (box.imageIndex >= 0) {
            const HBITMAP hImage = clipboard_images[box.imageIndex].get();
            const SIZE sz = GdiCache::GetBitmapSize(hImage);
            BlitBitmap(hdc, hImage, boxX + (boxWidth - static_cast<int>(sz.cx)) / 2,
                       lineY, static_cast<int>(sz.cx), static_cast<int>(sz.cy));
            continue;
        }

        if (box.gifIndex >= 0) {
            const GifElement& gif = gif_elements[box.gifIndex];
            const int frame = std::clamp(gif.current_frame, 0,
                                         static_cast<int>(gif.frames.size()) - 1);
            BlitBitmap(hdc, gif.frames[frame].get(),
                       boxX + (boxWidth - gif.width) / 2, lineY,
                       gif.width, gif.height);
            continue;
        }

        if (box.content.empty()) continue;

        RECT lineRect = {textX, lineY, boxX + boxWidth - kPadding, lineY + box.height};
        DrawTextW(hdc, box.content.c_str(), -1, &lineRect, DT_LEFT | DT_TOP | DT_NOPREFIX);

        // Resaltado de la parte seleccionada de esta linea.
        if (hasSelection) {
            const int lineStart = static_cast<int>(box.startPos);
            const int lineEnd = static_cast<int>(box.endPos);
            if (selStart < lineEnd && selEnd > lineStart) {
                const int from = std::clamp(selStart - lineStart, 0,
                                            static_cast<int>(box.content.length()));
                const int to = std::clamp(selEnd - lineStart, 0,
                                          static_cast<int>(box.content.length()));
                if (from < to) {
                    const SIZE before = GdiCache::GetTextSizeCached(
                        hdc, box.content.substr(0, from));
                    const std::wstring selected = box.content.substr(from, to - from);
                    const SIZE selSize = GdiCache::GetTextSizeCached(hdc, selected);

                    RECT selRect = {textX + static_cast<int>(before.cx), lineY,
                                    textX + static_cast<int>(before.cx) +
                                        static_cast<int>(selSize.cx),
                                    lineY + box.height};
                    FillRect(hdc, &selRect, GdiCache::hSelectionBrush);
                    DrawTextW(hdc, selected.c_str(), -1, &selRect,
                              DT_LEFT | DT_TOP | DT_NOPREFIX);
                }
            }
        }
    }

    // Cursor. Se ubica con el MISMO layout que se acaba de dibujar, por lo que ya
    // no se desincroniza en las lineas que contienen imagenes (antes el recorrido
    // del cursor asumia 50 px fijos de alto para las imagenes).
    if (text_input_mode.load() && text_cursor_visible.load()) {
        const int caret = std::clamp(text_cursor_pos.load(), 0,
                                     static_cast<int>(zoom_text.length()));
        int cursorX = textX;
        int cursorY = boxY;

        for (const auto& box : boxes) {
            const int lineStart = static_cast<int>(box.startPos);
            const int lineEnd = static_cast<int>(box.endPos);
            if (caret >= lineStart && caret <= lineEnd) {
                if (box.imageIndex >= 0 || box.gifIndex >= 0) {
                    // Despues de una imagen el cursor va al inicio de la siguiente linea.
                    cursorX = textX;
                    cursorY = boxY + box.y + box.height;
                } else {
                    const SIZE upTo = GdiCache::GetTextSizeCached(
                        hdc, box.content.substr(0, caret - lineStart));
                    cursorX = textX + static_cast<int>(upTo.cx);
                    cursorY = boxY + box.y;
                }
                break;
            }
            // El cursor esta mas abajo: seguir acumulando.
            cursorY = boxY + box.y + box.height;
        }

        HPEN hOldPen = (HPEN)SelectObject(hdc, GdiCache::hCursorPen);
        MoveToEx(hdc, cursorX, cursorY, NULL);
        LineTo(hdc, cursorX, cursorY + kLineHeight);
        SelectObject(hdc, hOldPen);
    }

    SelectObject(hdc, hOldFont);
}

} // namespace TextRender

// ----------------------------------------------------------------------------
// Copia del escritorio limpio
// ----------------------------------------------------------------------------
// Se toma justo antes de mostrar el overlay, por lo que contiene el escritorio
// SIN el tinte del overlay. Es la fuente que necesita la herramienta de pixelado:
// el back buffer no sirve porque ahi las regiones son color clave, no pixeles
// reales del escritorio.
static HDC g_cleanDesktopDC = nullptr;
static HBITMAP g_cleanDesktopBitmap = nullptr;
static HBITMAP g_cleanDesktopOldBitmap = nullptr;
static int g_cleanDesktopWidth = 0;
static int g_cleanDesktopHeight = 0;

void ReleaseCleanDesktop() {
    if (g_cleanDesktopDC) {
        if (g_cleanDesktopOldBitmap) {
            SelectObject(g_cleanDesktopDC, g_cleanDesktopOldBitmap);
        }
        DeleteDC(g_cleanDesktopDC);
        g_cleanDesktopDC = nullptr;
    }
    if (g_cleanDesktopBitmap) {
        DeleteObject(g_cleanDesktopBitmap);
        g_cleanDesktopBitmap = nullptr;
    }
    g_cleanDesktopOldBitmap = nullptr;
    g_cleanDesktopWidth = 0;
    g_cleanDesktopHeight = 0;
}

// Captura el escritorio virtual completo. Las coordenadas del bitmap resultante
// coinciden con las de cliente del overlay.
bool CaptureCleanDesktop(int width, int height) {
    ReleaseCleanDesktop();
    if (width <= 0 || height <= 0) return false;

    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) return false;

    HDC dc = CreateCompatibleDC(hScreenDC);
    if (!dc) {
        ReleaseDC(NULL, hScreenDC);
        return false;
    }

    HBITMAP bmp = CreateCompatibleBitmap(hScreenDC, width, height);
    if (!bmp) {
        DeleteDC(dc);
        ReleaseDC(NULL, hScreenDC);
        return false;
    }

    g_cleanDesktopOldBitmap = (HBITMAP)SelectObject(dc, bmp);
    BitBlt(dc, 0, 0, width, height, hScreenDC,
           VirtualScreenLeft(), VirtualScreenTop(), SRCCOPY);

    ReleaseDC(NULL, hScreenDC);

    g_cleanDesktopDC = dc;
    g_cleanDesktopBitmap = bmp;
    g_cleanDesktopWidth = width;
    g_cleanDesktopHeight = height;
    return true;
}

// ----------------------------------------------------------------------------
// Back buffer del overlay (doble buffering)
// ----------------------------------------------------------------------------
// Antes DrawOverlay creaba y destruia el DC y el bitmap de pantalla completa en
// CADA frame (~8 MB en 1080p, ~33 MB en 4K). Ahora se conservan y solo se
// reconstruyen cuando cambia el tamaño.
static HDC g_overlayBackDC = nullptr;
static HBITMAP g_overlayBackBitmap = nullptr;
static HBITMAP g_overlayBackOldBitmap = nullptr;
static int g_overlayBackWidth = 0;
static int g_overlayBackHeight = 0;

void ReleaseOverlayBackBuffer() {
    if (g_overlayBackDC) {
        if (g_overlayBackOldBitmap) {
            SelectObject(g_overlayBackDC, g_overlayBackOldBitmap);
        }
        DeleteDC(g_overlayBackDC);
        g_overlayBackDC = nullptr;
    }
    if (g_overlayBackBitmap) {
        DeleteObject(g_overlayBackBitmap);
        g_overlayBackBitmap = nullptr;
    }
    g_overlayBackOldBitmap = nullptr;
    g_overlayBackWidth = 0;
    g_overlayBackHeight = 0;
}

// Devuelve el DC del back buffer, creandolo solo si no existe o si cambio el
// tamaño. El bitmap queda seleccionado permanentemente en el DC.
HDC AcquireOverlayBackBuffer(HDC hdc, int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;

    if (g_overlayBackDC && g_overlayBackWidth == width && g_overlayBackHeight == height) {
        return g_overlayBackDC;
    }

    ReleaseOverlayBackBuffer();

    HDC dc = CreateCompatibleDC(hdc);
    if (!dc) return nullptr;

    HBITMAP bmp = CreateCompatibleBitmap(hdc, width, height);
    if (!bmp) {
        DeleteDC(dc);
        return nullptr;
    }

    g_overlayBackDC = dc;
    g_overlayBackBitmap = bmp;
    g_overlayBackOldBitmap = (HBITMAP)SelectObject(dc, bmp);
    g_overlayBackWidth = width;
    g_overlayBackHeight = height;
    return dc;
}

// ----------------------------------------------------------------------------
// Selector de color y grosor sobre el overlay
// ----------------------------------------------------------------------------
// La geometria se define una sola vez y la usan tanto el dibujado como la
// deteccion de clics, para que no puedan desincronizarse.
namespace Toolbar {

constexpr int kX = 20;
constexpr int kY = 55;          // justo debajo del indicador de herramienta
constexpr int kSwatchSize = 26;
constexpr int kGap = 4;

// OJO: el overlay usa LWA_COLORKEY con magenta RGB(255,0,255) y cian
// RGB(0,255,255) como colores transparentes, asi que la paleta NO puede incluir
// exactamente esos dos valores: la muestra se volveria invisible y lo dibujado
// con ese color desapareceria.
constexpr COLORREF kColors[] = {
    RGB(255, 0, 0),      // rojo
    RGB(255, 128, 0),    // naranja
    RGB(255, 255, 0),    // amarillo
    RGB(0, 200, 0),      // verde
    RGB(0, 160, 255),    // azul
    RGB(160, 0, 255),    // violeta
    RGB(255, 255, 255),  // blanco
    RGB(0, 0, 0)         // negro
};
constexpr int kColorCount = static_cast<int>(sizeof(kColors) / sizeof(kColors[0]));

constexpr int kThicknessValues[] = {2, 4, 8, 14};
constexpr int kThicknessCount =
    static_cast<int>(sizeof(kThicknessValues) / sizeof(kThicknessValues[0]));

constexpr int kThicknessRowY = kY + kSwatchSize + kGap;

RECT ColorRect(int index) {
    const int left = kX + index * (kSwatchSize + kGap);
    return RECT{left, kY, left + kSwatchSize, kY + kSwatchSize};
}

RECT ThicknessRect(int index) {
    const int left = kX + index * (kSwatchSize + kGap);
    return RECT{left, kThicknessRowY, left + kSwatchSize,
                kThicknessRowY + kSwatchSize};
}

void Draw(HDC hdc) {
    const COLORREF activeColor = static_cast<COLORREF>(drawing_color.load());
    const int activeThickness = drawing_thickness.load();

    // Fila de colores.
    for (int i = 0; i < kColorCount; ++i) {
        RECT r = ColorRect(i);

        ScopedBrush brush(CreateSolidBrush(kColors[i]), true);
        if (brush) FillRect(hdc, &r, brush);

        // La muestra activa lleva un borde blanco mas grueso.
        const bool active = (kColors[i] == activeColor);
        HPEN pen = GdiCache::GetPenCached(active ? RGB(255, 255, 255)
                                                 : RGB(90, 90, 90),
                                         active ? 3 : 1);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
    }

    // Fila de grosores: cada boton muestra una linea de su propio grosor.
    for (int i = 0; i < kThicknessCount; ++i) {
        RECT r = ThicknessRect(i);
        FillRect(hdc, &r, GdiCache::hBlackBrush);

        const bool active = (kThicknessValues[i] == activeThickness);
        HPEN borderPen = GdiCache::GetPenCached(active ? RGB(255, 255, 255)
                                                       : RGB(90, 90, 90),
                                               active ? 3 : 1);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);

        HPEN samplePen = GdiCache::GetPenCached(activeColor, kThicknessValues[i]);
        oldPen = (HPEN)SelectObject(hdc, samplePen);
        const int midY = (r.top + r.bottom) / 2;
        MoveToEx(hdc, r.left + 5, midY, NULL);
        LineTo(hdc, r.right - 5, midY);
        SelectObject(hdc, oldPen);
    }
}

// Devuelve true si el clic fue consumido por el selector, para que no se
// interprete tambien como el inicio de un trazo.
bool HitTest(int x, int y) {
    POINT p = {x, y};

    for (int i = 0; i < kColorCount; ++i) {
        RECT r = ColorRect(i);
        if (PtInRect(&r, p)) {
            drawing_color.store(static_cast<int>(kColors[i]));
            SaveConfiguration();
            return true;
        }
    }

    for (int i = 0; i < kThicknessCount; ++i) {
        RECT r = ThicknessRect(i);
        if (PtInRect(&r, p)) {
            drawing_thickness.store(kThicknessValues[i]);
            SaveConfiguration();
            return true;
        }
    }

    return false;
}

} // namespace Toolbar

// Función para dibujar el overlay
void DrawOverlay(HDC hdc, int width, int height) {
    // Este frame lee zoom_text, screenRectangles, drawing_elements,
    // clipboard_images y gif_elements, todos objetos no atomicos compartidos con
    // el hilo principal. Se toma el candado una sola vez por frame.
    std::lock_guard<std::mutex> annotationLock(g_annotationMutex);

    // Configurar transparencia para que el magenta (255, 0, 255) y cian (0, 255, 255) sean completamente transparentes
    //
    // W2.3: esta llamada se hacia en cada frame, obligando a DWM a reevaluar toda
    // la superficie layered aunque nada hubiera cambiado. Ahora solo se llama
    // cuando el color clave o la opacidad efectivamente cambian.
    if (hCurrentOverlay.load()) {
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

        static COLORREF lastTransparentColor = CLR_INVALID;
        static int lastOpacity = -1;
        if (transparentColor != lastTransparentColor || userOpacity != lastOpacity) {
            SetLayeredWindowAttributes(hCurrentOverlay.load(), transparentColor,
                                       static_cast<BYTE>(userOpacity),
                                       LWA_COLORKEY | LWA_ALPHA);
            lastTransparentColor = transparentColor;
            lastOpacity = userOpacity;
        }
    }

    // Back buffer cacheado. Antes se creaba y destruia un DC y un bitmap de
    // pantalla completa en CADA frame (~8 MB en 1080p, ~33 MB en 4K).
    HDC hMemDC = AcquireOverlayBackBuffer(hdc, width, height);
    if (!hMemDC) return;
            // Dibujar overlay negro en toda la pantalla (usando cache optimizado)
    RECT fullRect = {0, 0, width, height};
        FillRect(hMemDC, &fullRect, GdiCache::hOverlayBrush);
    
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
        
        // Debug del zoom: solo en compilaciones de depuracion. Antes se emitia
        // en CADA frame, y OutputDebugStringW toma un mutex global del sistema.
        #ifdef DEBUG_BUILD
        {
            wchar_t debugMsg[256];
            swprintf_s(debugMsg, L"DEBUG: Zoom active - Original: %dx%d, Zoom: %.2f, Zoomed: %dx%d\n",
                       originalWidth, originalHeight, zoom, zoomedWidth, zoomedHeight);
            OutputDebugStringW(debugMsg);
        }
        #endif
        // Primero dibujar un fondo blanco sólido para el zoom (brillo normal)
        RECT zoomRect = {zoomX, zoomY, zoomX + zoomedWidth, zoomY + zoomedHeight};
        FillRect(hMemDC, &zoomRect, GdiCache::hWhiteBrush);
        
        // IMPORTANTE: Seleccionar el bitmap antes de hacer StretchBlt
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hZoomedDC.get(), hZoomedBitmap.get());

        BOOL stretchResult = StretchBlt(hMemDC, zoomX, zoomY, zoomedWidth, zoomedHeight,
                                       hZoomedDC.get(), 0, 0, originalWidth, originalHeight, SRCCOPY);
        // El resultado se registraba en cada frame, incluso en el camino de exito.
        #ifdef DEBUG_BUILD
        if (!stretchResult) {
            wchar_t errorMsg[256];
            swprintf_s(errorMsg, L"DEBUG: StretchBlt failed - Error: %d\n", (int)GetLastError());
            OutputDebugStringW(errorMsg);
        }
        #else
        (void)stretchResult;
        #endif
        // Restaurar el bitmap anterior
        SelectObject(hZoomedDC.get(), hOldBitmap);
        
        // Dibujar texto debajo de la región con zoom.
        // El cuadro se centra horizontalmente bajo la región ampliada.
        TextRender::RenderAnnotationText(hMemDC, GdiCache::hCachedFontZoom,
                                         zoomX, zoomY + zoomedHeight + 5,
                                         zoomedWidth);
    }
    
    // Dibujar texto cuando no hay zoom.
    //
    // La condicion ya no exige text_input_mode: antes el texto desaparecia al
    // salir del modo de edicion, mientras que en modo zoom bastaba con que el
    // texto no estuviera vacio. Ahora ambos modos se comportan igual.
    if (!zoom_active.load() && !zoom_text.empty()) {
        // Debajo de la ultima region seleccionada, alineado a su borde izquierdo.
        int anchorX = 20;
        int anchorY = 20;
        if (!screenRectangles.empty()) {
            const auto& lastRect = screenRectangles.back();
            anchorX = lastRect.x1;
            anchorY = lastRect.y2 + 5;
        }
        TextRender::RenderAnnotationText(hMemDC, GdiCache::hCachedFont,
                                         anchorX, anchorY, 0);
    }
    
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
        // Usar fuente cacheada para mejor performance
        HFONT hOldFont = (HFONT)SelectObject(hMemDC, GdiCache::hCachedFontIndicator);
        
        // Texto del indicador
        std::wstring toolText;
        switch (current_drawing_tool.load()) {
            case DrawingTool::Line: toolText = L"✏️ LINE"; break;
            case DrawingTool::Arrow: toolText = L"🏹 ARROW"; break;
            case DrawingTool::Rectangle: toolText = L"🔲 RECTANGLE"; break;
            // Case 4 (Text) removed
            case DrawingTool::Highlighter: toolText = L"🎨 HIGHLIGHTER"; break;
            case DrawingTool::Ellipse: toolText = L"⭕ ELLIPSE"; break;
            case DrawingTool::Pen: toolText = L"🖊️ PEN"; break;
            case DrawingTool::Redact: toolText = L"🔒 REDACT"; break;
            case DrawingTool::Step: toolText = L"①  STEP"; break;
        }
        
        // Fondo del indicador
        RECT indicatorRect = {20, 20, 200, 50};
        FillRect(hMemDC, &indicatorRect, GdiCache::hBlackBrush);
        
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
        // DeleteObject(hIndicatorFont); // Comentado - usando cache estático
        DeleteObject(hIndicatorPen);

        // Selector de color y grosor, justo debajo del indicador.
        Toolbar::Draw(hMemDC);
    }
    
    // Mostrar indicador del modo captura
    if (screenshot_mode.load()) {
        // Usar fuente cacheada para mejor performance
        HFONT hOldFont = (HFONT)SelectObject(hMemDC, GdiCache::hCachedFontIndicator);
        
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
        // DeleteObject(hIndicatorFont); // Comentado - usando cache estático
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
        HFONT hOldFont = (HFONT)SelectObject(hMemDC, GdiCache::hCachedFontScreenshot);
        
        SetTextColor(hMemDC, RGB(255, 255, 255)); // Texto blanco para mejor visibilidad
        SetBkMode(hMemDC, TRANSPARENT);
        
        std::wstring captionText = L"📸 SCREENSHOT - Release click to capture";
        RECT captionRect = {x1, y1 - 25, x2, y1};
        DrawTextW(hMemDC, captionText.c_str(), -1, &captionRect, DT_CENTER | DT_VCENTER);
        
        SelectObject(hMemDC, hOldFont);
    }
    
    // Dibujar preview de dibujo en tiempo real (funciona también durante el zoom)
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
            case DrawingTool::Ellipse: // Elipse
                DrawEllipseShape(hMemDC, x1, y1, x2, y2, drawing_color.load(),
                                 drawing_thickness.load(), drawing_fill.load());
                break;
            case DrawingTool::Redact: // Pixelado
                DrawRedaction(hMemDC, g_cleanDesktopDC, x1, y1, x2, y2,
                              drawing_thickness.load() * 4);
                break;
            default:
                break;
        }
    }
    
    // Dibujar todos los elementos de dibujo (funciona también durante el zoom)
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
            case DrawingTool::Ellipse: // Elipse
                DrawEllipseShape(hMemDC, element.x1, element.y1, element.x2, element.y2,
                                 element.color, element.thickness, element.filled);
                break;
            case DrawingTool::Pen: // Trazo libre
                DrawPenStroke(hMemDC, element.points, element.color, element.thickness);
                break;
            case DrawingTool::Redact: // Pixelado
                DrawRedaction(hMemDC, g_cleanDesktopDC, element.x1, element.y1,
                              element.x2, element.y2, element.thickness * 4);
                break;
            case DrawingTool::Step: // Numero de paso
                DrawStepMarker(hMemDC, element.x1, element.y1, element.step_number,
                               element.color, element.thickness);
                break;
            default:
                break;
        }
    }

    // Trazo del lapiz en curso (aun no confirmado).
    if (drawing_active.load() && current_drawing_tool.load() == DrawingTool::Pen &&
        !pen_stroke.empty()) {
        DrawPenStroke(hMemDC, pen_stroke, drawing_color.load(), drawing_thickness.load());
    }
    
    // Copiar el resultado al DC principal (doble buffering)
    BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);

    // El bitmap sigue seleccionado en el back buffer cacheado; se libera en
    // ReleaseOverlayBackBuffer al cerrar el overlay.
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
    
    // Fondo negro puro (sin transparencia) - usando cache optimizado
    RECT fullRect = {0, 0, width, height};
    FillRect(hMemDC, &fullRect, GdiCache::hBlackBrush);
    
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
    
    // Línea separadora moderna (usando cache optimizado)
    HPEN hOldPen = (HPEN)SelectObject(hMemDC, GdiCache::hModernPen);
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
        RECT tools2Rect = {90, currentY, width - 50, currentY + 20};
        DrawTextW(hMemDC, L"F5 = Ellipse | F6 = Pen | F7 = Redact (pixelate) | F8 = Step number", -1, &tools2Rect, DT_LEFT | DT_TOP);
        currentY += 25;
        RECT toolsDescRect = {90, currentY, width - 50, currentY + 20};
        DrawTextW(hMemDC, L"Ctrl+Z = Undo | Ctrl+Y = Redo | ESC = Exit drawing mode", -1, &toolsDescRect, DT_LEFT | DT_TOP);
        currentY += 25;
        
        // Captura de pantalla
        RECT screenshotRect = {90, currentY, width - 50, currentY + 20};
        SetTextColor(hMemDC, RGB(255, 150, 150)); // Rosa para destacar
        DrawTextW(hMemDC, L"📸 Shift+Alt+X (overlay active) = Screenshot mode", -1, &screenshotRect, DT_LEFT | DT_TOP);
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
    BOOL captureResult = BitBlt(hZoomedDC.get(), 0, 0, width, height, hScreenDC,
                                ClientToScreenX(rect.x1), ClientToScreenY(rect.y1), SRCCOPY);
    #ifdef DEBUG_BUILD
    if (!captureResult) {
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"DEBUG: Capture failed - Error: %d\n", (int)GetLastError());
        OutputDebugStringW(errorMsg);
    }
    #else
    (void)captureResult;
    #endif
    // Restaurar el bitmap anterior
    SelectObject(hZoomedDC.get(), hOldBitmap);
    
    ReleaseDC(NULL, hScreenDC);
    
    // Configurar centro de zoom - X centrado, Y 20% arriba del centro.
    //
    // W3.3: se centra en el monitor donde esta la region, no en el primario. Antes
    // se usaba SM_CYSCREEN, por lo que al ampliar una region de un monitor
    // secundario el zoom aparecia centrado en el primario.
    zoom_center_x.store((rect.x1 + rect.x2) / 2);

    POINT regionCenter = {ClientToScreenX((rect.x1 + rect.x2) / 2),
                          ClientToScreenY((rect.y1 + rect.y2) / 2)};
    HMONITOR hMonitor = MonitorFromPoint(regionCenter, MONITOR_DEFAULTTONEAREST);

    int monitorTop = 0;
    int monitorHeight = VirtualScreenHeight();
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (hMonitor && GetMonitorInfoW(hMonitor, &mi)) {
        // De vuelta a coordenadas de cliente del overlay.
        monitorTop = mi.rcMonitor.top - VirtualScreenTop();
        monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }

    int centerY = monitorTop + monitorHeight / 2;
    int offsetY = (int)((monitorHeight / 2) * 0.2); // 20% arriba del centro
    zoom_center_y.store(centerY - offsetY);
    
    // Copiar el texto actual antes de borrarlo (solo la primera vez que se hace zoom)
    if (!zoom_active.load() && !zoom_text.empty()) {
        // Es la primera vez que se hace zoom y hay texto escrito
        text_before_zoom = zoom_text; // Guardar el texto actual
        // Copiar el texto al zoom_text para que aparezca en el zoom
        // (el texto ya está en zoom_text, así que no necesitamos hacer nada más)
    } else if (!zoom_active.load() && zoom_text.empty() && !text_before_zoom.empty()) {
        // Es la primera vez que se hace zoom, no hay texto actual, pero sí había texto antes
        zoom_text = text_before_zoom; // Restaurar el texto anterior
        text_cursor_pos.store(text_before_zoom.length()); // Posicionar cursor al final
    } else if (!zoom_active.load()) {
        // Es la primera vez que se hace zoom y no hay texto
        zoom_text.clear();
        text_cursor_pos.store(0);
    }
    // Si ya hay zoom activo, mantener el texto actual sin cambios
    
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
    // Limpiar el texto guardado antes del zoom
    text_before_zoom.clear();
    
    // Limpiar selección de texto
    text_selection_active.store(false);
    text_selection_start.store(-1);
    text_selection_end.store(-1);
    
    // Limpiar marcadores de imagen
    image_markers.clear();
    
    // Limpiar imágenes del clipboard (las clases RAII se encargan automáticamente)
    clipboard_images.clear();
    
    // Limpiar elementos GIF
    gif_elements.clear();
    
    // Limpiar elementos de dibujo
    drawing_elements.clear();
    drawing_active.store(false);
    current_drawing_tool.store(DrawingTool::None);
    drawing_start_x.store(-1);
    drawing_start_y.store(-1);
}

// Función para actualizar frames de GIFs con velocidad original
// Arranca el timer de animacion de GIFs si hace falta. Se llama al agregar un GIF.
void EnsureGifTimer() {
    HWND overlay = hCurrentOverlay.load();
    if (overlay) {
        // ~30 fps. El handler mata el timer solo cuando ya no hay GIFs.
        SetTimer(overlay, TIMER_GIF_FRAMES, 33, NULL);
    }
}

void UpdateGifFrames() {
    DWORD currentTime = GetTickCount();
    
            for (auto& gif : gif_elements) {
            if (gif.is_playing && !gif.frames.empty() && !gif.delays.empty()) {
                // Verificar si es momento de cambiar al siguiente frame
                int currentDelay = gif.getCurrentDelay();
                if (currentTime - gif.last_frame_time >= currentDelay) {
                    // Cambiar al siguiente frame
                    gif.current_frame = (gif.current_frame + 1) % gif.frames.size();
                    gif.last_frame_time = currentTime;
                    
                    // Forzar redibujado para mostrar el nuevo frame
                    RequestOverlayRedraw();
                    
                    // Debug: mostrar información del frame actual
                    #ifdef DEBUG_BUILD
                    wchar_t debugMsg[256];
                    swprintf_s(debugMsg, L"GIF Frame: %d/%zu, Delay: %dms (Original: %dms, Speed: %.2fx)\n", 
                               gif.current_frame + 1, gif.frames.size(), currentDelay, 
                               gif.delays[gif.current_frame], gif.speed_multiplier);
                    OutputDebugStringW(debugMsg);
                    #endif
                }
            }
        }
}

// Función para crear y mostrar el overlay
// Función para crear y mostrar el overlay
void ShowOverlay() {
    if (overlay_active.load()) return;

    overlay_active.store(true);
    selection_mode.store(true);

    {
        std::lock_guard<std::mutex> lock(g_annotationMutex);
        screenRectangles.clear();

        // Tambien se limpian los dibujos. Antes solo se borraba screenRectangles,
        // asi que al reabrir el overlay reaparecian las lineas y flechas de la
        // sesion anterior flotando sobre una pantalla distinta y sin sus regiones.
        drawing_elements.clear();
        redo_stack.clear();
        pen_stroke.clear();
    }
    step_counter.store(1);

    // Inicializar coordenadas de selección con -1 (sin selección)
    start_x.store(-1);
    start_y.store(-1);
    end_x.store(-1);
    end_y.store(-1);

    // Dimensiones del ESCRITORIO VIRTUAL, no del monitor primario: antes el
    // overlay se creaba en (0, 0, SM_CXSCREEN, SM_CYSCREEN) y por eso nunca podia
    // cubrir un segundo monitor.
    int screenLeft = VirtualScreenLeft();
    int screenTop = VirtualScreenTop();
    int screenWidth = VirtualScreenWidth();
    int screenHeight = VirtualScreenHeight();

    // Crear ventana overlay con procedimiento personalizado
    HWND hOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        L"ScreenHighlighterOverlayClass",
        L"Screen Highlighter Overlay",
        WS_POPUP,
        screenLeft, screenTop, screenWidth, screenHeight,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!hOverlay) {
        overlay_active.store(false);
        return;
    }

    // Asignar handle global para poder cambiar atributos dinámicamente
    hCurrentOverlay.store(hOverlay);

    // Hacer la ventana semitransparente inicialmente
    SetLayeredWindowAttributes(hOverlay, 0, 128, LWA_ALPHA);

    // Copia del escritorio limpio ANTES de mostrar el overlay, para que no quede
    // capturado su propio tinte. Es la fuente de la herramienta de pixelado.
    CaptureCleanDesktop(screenWidth, screenHeight);

    // Mostrar la ventana (genera el primer WM_PAINT)
    ShowWindow(hOverlay, SW_SHOW);

    // Parpadeo del cursor de texto.
    //
    // Antes esto era un std::thread suelto que leia zoom_text.empty() mientras el
    // hilo del overlay modificaba esa misma cadena: una carrera de datos real. Un
    // timer de ventana entrega el evento en el hilo del overlay, sin carrera y sin
    // un hilo extra.
    SetTimer(hOverlay, TIMER_CURSOR_BLINK,
             std::clamp(text_cursor_blink_speed.load(), 100, 2000), NULL);

    // Bucle principal del overlay.
    //
    // GetMessage BLOQUEA hasta que llega un mensaje, por lo que con el overlay
    // abierto y quieto el consumo de CPU es practicamente nulo. Antes este bucle
    // hacia Sleep(16..50) indefinidamente.
    //
    // Las rutas que cierran el overlay (ESC, WM_DESTROY) ponen overlay_active en
    // false desde OverlayWndProc, es decir dentro de DispatchMessage y en este
    // mismo hilo, asi que la condicion se evalua justo despues y no hace falta
    // despertar el bucle artificialmente.
    MSG msg;
    while (overlay_active.load()) {
        if (!GetMessage(&msg, NULL, 0, 0)) {
            break; // WM_QUIT
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Limpiar recursos
    KillTimer(hOverlay, TIMER_CURSOR_BLINK);
    KillTimer(hOverlay, TIMER_GIF_FRAMES);

    // Limpiar variable global antes de destruir la ventana, para que ningun
    // RequestOverlayRedraw use un handle ya invalido.
    hCurrentOverlay.store(nullptr);

    DestroyWindow(hOverlay);

    // Liberar el back buffer del overlay y la copia del escritorio
    ReleaseOverlayBackBuffer();
    ReleaseCleanDesktop();

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
    
    // Dimensiones del monitor donde esta el puntero, para que la ventana de
    // configuracion no aparezca siempre en el primario.
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int screenOriginX = 0;
    int screenOriginY = 0;
    {
        POINT cursorPos;
        if (GetCursorPos(&cursorPos)) {
            HMONITOR hMonitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            if (hMonitor && GetMonitorInfoW(hMonitor, &mi)) {
                screenOriginX = mi.rcMonitor.left;
                screenOriginY = mi.rcMonitor.top;
                screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
                screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
            }
        }
    }

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
    
    // Centrar la ventana horizontalmente, alinear al tope verticalmente,
    // relativo al monitor elegido.
    int windowX = screenOriginX + (screenWidth - 700) / 2;
    int windowY = screenOriginY; // Alinear al tope de ese monitor
    
    // Configurar la ventana con altura completa disponible
    SetWindowPos(hSettingsWindow, HWND_TOPMOST, windowX, windowY, 700, availableHeight, SWP_SHOWWINDOW);
    
    // Configurar procedimiento de ventana para la configuración
    SetWindowLongPtr(hSettingsWindow, GWLP_WNDPROC, (LONG_PTR)SettingsWndProc);
    
    // Bucle principal de configuración.
    //
    // Igual que en el overlay: antes esto hacia Sleep(16) indefinidamente, o sea
    // 60 despertares por segundo mientras la ventana estaba abierta sin que el
    // usuario tocara nada. GetMessage bloquea hasta que hay algo que procesar.
    //
    // Las rutas que cierran esta ventana ponen settings_overlay_active en false
    // desde SettingsWndProc, dentro de DispatchMessage y en este mismo hilo, por
    // lo que la condicion se reevalua inmediatamente despues.
    MSG msg;
    while (settings_overlay_active.load()) {
        if (!GetMessage(&msg, NULL, 0, 0)) {
            break; // WM_QUIT
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    // Limpiar recursos
    DestroyWindow(hSettingsWindow);
}

// Función para manejar eventos del mouse en el overlay
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        // Todo el dibujado pasa por aqui. Antes el overlay no tenia handler de
        // WM_PAINT: un bucle aparte llamaba a DrawOverlay sobre un HDC obtenido
        // con GetDC y mantenido abierto durante toda la vida de la ventana.
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            DrawOverlay(hdc, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hwnd, &ps);
            return 0;
        }

        // El fondo lo pinta DrawOverlay por completo en su back buffer; dejar que
        // Windows lo borre antes solo produciria parpadeo.
        case WM_ERASEBKGND:
            return 1;

        case WM_TIMER:
            if (wParam == TIMER_CURSOR_BLINK) {
                // El hilo anterior releia la velocidad en cada vuelta. Un timer de
                // periodo fijo ignoraria un cambio hecho en configuracion hasta
                // reabrir el overlay, asi que se reajusta cuando cambia.
                static int lastBlinkSpeed = -1;
                const int desiredSpeed = std::clamp(text_cursor_blink_speed.load(),
                                                    100, 2000);
                if (desiredSpeed != lastBlinkSpeed) {
                    lastBlinkSpeed = desiredSpeed;
                    SetTimer(hwnd, TIMER_CURSOR_BLINK, desiredSpeed, NULL);
                }

                if (text_input_mode.load()) {
                    text_cursor_visible.store(!text_cursor_visible.load());
                    bool hasText;
                    {
                        std::lock_guard<std::mutex> lock(g_annotationMutex);
                        hasText = !zoom_text.empty();
                    }
                    if (hasText) {
                        RequestOverlayRedraw();
                    }
                }
            } else if (wParam == TIMER_GIF_FRAMES) {
                bool anyGifs;
                {
                    std::lock_guard<std::mutex> lock(g_annotationMutex);
                    anyGifs = !gif_elements.empty();
                }
                if (anyGifs) {
                    UpdateGifFrames();
                } else {
                    // Sin GIFs no hace falta seguir despertando.
                    KillTimer(hwnd, TIMER_GIF_FRAMES);
                }
            }
            return 0;

        // Reconstruir el overlay si cambia la resolucion o la disposicion de
        // monitores mientras esta abierto.
        case WM_DISPLAYCHANGE: {
            ReleaseOverlayBackBuffer();
            // Reajustar el overlay al nuevo escritorio virtual y volver a
            // capturarlo, si no el pixelado usaria una copia obsoleta.
            const int vw = VirtualScreenWidth();
            const int vh = VirtualScreenHeight();
            SetWindowPos(hwnd, HWND_TOPMOST, VirtualScreenLeft(), VirtualScreenTop(),
                         vw, vh, SWP_NOACTIVATE);
            CaptureCleanDesktop(vw, vh);
            RequestOverlayRedraw();
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (screenshot_mode.load()) {
                // Modo captura de pantalla
                screenshot_start_x.store(GET_X_LPARAM(lParam));
                screenshot_start_y.store(GET_Y_LPARAM(lParam));
                screenshot_end_x.store(GET_X_LPARAM(lParam));
                screenshot_end_y.store(GET_Y_LPARAM(lParam));
                RequestOverlayRedraw();
            } else if (drawing_active.load()) {
                const int mx = GET_X_LPARAM(lParam);
                const int my = GET_Y_LPARAM(lParam);
                const DrawingTool tool = current_drawing_tool.load();

                // El selector se dibuja encima, asi que tiene prioridad sobre el
                // inicio de un trazo.
                if (Toolbar::HitTest(mx, my)) {
                    RequestOverlayRedraw();
                    break;
                }

                if (tool == DrawingTool::Step) {
                    // El marcador de paso se coloca con un solo clic, sin arrastrar.
                    std::lock_guard<std::mutex> lock(g_annotationMutex);
                    DrawingElement marker(DrawingTool::Step, mx, my, mx, my,
                                          drawing_color.load(),
                                          drawing_thickness.load(), true);
                    marker.step_number = step_counter.fetch_add(1);
                    drawing_elements.push_back(std::move(marker));
                    redo_stack.clear();
                    RequestOverlayRedraw();
                    break;
                }

                if (tool == DrawingTool::Pen) {
                    // Comenzar un trazo libre.
                    std::lock_guard<std::mutex> lock(g_annotationMutex);
                    pen_stroke.clear();
                    pen_stroke.push_back(POINT{mx, my});
                }

                // Modo dibujo activo - las regiones no son interactivas
                drawing_start_x.store(mx);
                drawing_start_y.store(my);
                end_x.store(mx);
                end_y.store(my);
                RequestOverlayRedraw();
            } else if (selection_mode.load() && !drawing_active.load()) {
                // Solo permitir selección si no hay herramienta de dibujo activa
                start_x.store(GET_X_LPARAM(lParam));
                start_y.store(GET_Y_LPARAM(lParam));
                end_x.store(start_x.load());
                end_y.store(start_y.load());
                RequestOverlayRedraw();
            }
            break;
            
        case WM_MOUSEMOVE:
            if (screenshot_mode.load() && screenshot_start_x.load() != -1) {
                // Modo captura - mostrar preview
                screenshot_end_x.store(GET_X_LPARAM(lParam));
                screenshot_end_y.store(GET_Y_LPARAM(lParam));
                RequestOverlayRedraw();
            } else if (drawing_active.load() && drawing_start_x.load() != -1) {
                const int mx = GET_X_LPARAM(lParam);
                const int my = GET_Y_LPARAM(lParam);

                if (current_drawing_tool.load() == DrawingTool::Pen) {
                    std::lock_guard<std::mutex> lock(g_annotationMutex);
                    // Descartar puntos casi repetidos: reduce el tamaño del trazo
                    // sin que se note en pantalla.
                    if (pen_stroke.empty() ||
                        std::abs(pen_stroke.back().x - mx) > 1 ||
                        std::abs(pen_stroke.back().y - my) > 1) {
                        pen_stroke.push_back(POINT{mx, my});
                    }
                }

                // Modo dibujo - mostrar preview
                end_x.store(mx);
                end_y.store(my);
                RequestOverlayRedraw();
            } else if (selection_mode.load() && start_x.load() != -1 && !drawing_active.load()) {
                // Solo permitir selección si no hay herramienta de dibujo activa
                end_x.store(GET_X_LPARAM(lParam));
                end_y.store(GET_Y_LPARAM(lParam));
                RequestOverlayRedraw();
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
                    // Centrar en el monitor donde esta el puntero, no en el
                    // primario: antes se usaba SM_CXSCREEN/SM_CYSCREEN, asi que al
                    // ampliar en un monitor secundario el zoom saltaba al primario.
                    // OJO: a diferencia de WM_LBUTTONDOWN y WM_MOUSEMOVE,
                    // WM_MOUSEWHEEL entrega lParam ya en coordenadas de PANTALLA,
                    // por lo que no hay que convertirlo.
                    POINT screenPos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

                    int centerX = VirtualScreenWidth() / 2;
                    int centerY = VirtualScreenHeight() / 2;

                    HMONITOR hMonitor = MonitorFromPoint(screenPos, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi;
                    mi.cbSize = sizeof(mi);
                    if (hMonitor && GetMonitorInfoW(hMonitor, &mi)) {
                        centerX = (mi.rcMonitor.left + mi.rcMonitor.right) / 2 - VirtualScreenLeft();
                        centerY = (mi.rcMonitor.top + mi.rcMonitor.bottom) / 2 - VirtualScreenTop();
                    }

                    // Centrado inmediato: X en el centro, Y 20% arriba del centro
                    zoom_factor.store(newZoom);
                    zoom_center_x.store(centerX);
                    const int monitorHalfHeight = (hMonitor && GetMonitorInfoW(hMonitor, &mi))
                        ? (mi.rcMonitor.bottom - mi.rcMonitor.top) / 2
                        : VirtualScreenHeight() / 2;
                    zoom_center_y.store(centerY - (int)(monitorHalfHeight * 0.2));

                    RequestOverlayRedraw();
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
                RequestOverlayRedraw();
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
                // El lapiz se confirma con su lista de puntos, no con un
                // rectangulo delimitador.
                if (current_drawing_tool.load() == DrawingTool::Pen) {
                    std::lock_guard<std::mutex> lock(g_annotationMutex);
                    if (pen_stroke.size() >= 2) {
                        DrawingElement stroke(DrawingTool::Pen,
                                              pen_stroke.front().x, pen_stroke.front().y,
                                              pen_stroke.back().x, pen_stroke.back().y,
                                              drawing_color.load(),
                                              drawing_thickness.load(), false);
                        stroke.points = pen_stroke;
                        drawing_elements.push_back(std::move(stroke));
                        redo_stack.clear();
                    }
                    pen_stroke.clear();
                    drawing_start_x.store(-1);
                    drawing_start_y.store(-1);
                    RequestOverlayRedraw();
                    break;
                }

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

                    // Un elemento nuevo descarta el historial de rehacer.
                    redo_stack.clear();
                }
                
                // Mantener herramienta activa para dibujar más elementos
                drawing_start_x.store(-1);
                drawing_start_y.store(-1);
                // NO resetear drawing_active ni current_drawing_tool
                // drawing_active.store(false);        // COMENTADO
                // current_drawing_tool.store(0);     // COMENTADO
                RequestOverlayRedraw();
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
                RequestOverlayRedraw();
            }
            break;
            
        // Entrada de texto real. TranslateMessage (llamado en el bucle del
        // overlay) convierte las pulsaciones en WM_CHAR ya resueltas segun la
        // distribucion del teclado, por lo que este handler soporta acentos,
        // teclas muertas, AltGr e IME sin tablas propias.
        case WM_CHAR: {
            if (!text_input_mode.load()) break;

            const wchar_t ch = static_cast<wchar_t>(wParam);

            // Descartar caracteres de control: Ctrl+letra llega como 1..26, y
            // Enter, Tab, Escape y Retroceso ya se manejan en WM_KEYDOWN.
            if (ch < 32 || ch == 127) break;

            {
                std::lock_guard<std::mutex> lock(g_annotationMutex);

                // Si hay seleccion activa, escribir la reemplaza.
                if (text_selection_active.load()) {
                    int selStart = std::min(text_selection_start.load(),
                                            text_selection_end.load());
                    int selEnd = std::max(text_selection_start.load(),
                                          text_selection_end.load());
                    selStart = std::clamp(selStart, 0, static_cast<int>(zoom_text.length()));
                    selEnd = std::clamp(selEnd, 0, static_cast<int>(zoom_text.length()));
                    if (selStart < selEnd) {
                        zoom_text.erase(selStart, selEnd - selStart);
                        text_cursor_pos.store(selStart);
                    }
                    text_selection_active.store(false);
                    text_selection_start.store(-1);
                    text_selection_end.store(-1);
                }

                const int pos = std::clamp(text_cursor_pos.load(), 0,
                                           static_cast<int>(zoom_text.length()));
                zoom_text.insert(pos, 1, ch);
                text_cursor_pos.store(pos + 1);
            }

            RequestOverlayRedraw();
            break;
        }

        case WM_KEYDOWN:
            // Manejar teclas F1-F5 para herramientas de dibujo (siempre disponibles)
            if (wParam == VK_F1) {
                // F1 - Activar herramienta Línea
                current_drawing_tool.store(DrawingTool::Line);
                drawing_active.store(true);
                RequestOverlayRedraw();
                return 0;
            } else if (wParam == VK_F2) {
                // F2 - Activar herramienta Flecha
                current_drawing_tool.store(DrawingTool::Arrow);
                drawing_active.store(true);
                RequestOverlayRedraw();
                return 0;
            } else if (wParam == VK_F3) {
                // F3 - Activar herramienta Rectángulo
                current_drawing_tool.store(DrawingTool::Rectangle);
                drawing_active.store(true);
                RequestOverlayRedraw();
                return 0;
            // F4 (Texto) eliminado
            } else if (wParam == VK_F4) {
                // F4 - Activar herramienta Resaltador
                current_drawing_tool.store(DrawingTool::Highlighter);
                drawing_active.store(true);
                RequestOverlayRedraw();
                return 0;
            } else if (wParam == VK_F5) {
                // F5 - Elipse
                current_drawing_tool.store(DrawingTool::Ellipse);
                drawing_active.store(true);
                RequestOverlayRedraw();
                return 0;
            } else if (wParam == VK_F6) {
                // F6 - Lapiz (trazo libre)
                current_drawing_tool.store(DrawingTool::Pen);
                drawing_active.store(true);
                RequestOverlayRedraw();
                return 0;
            } else if (wParam == VK_F7) {
                // F7 - Pixelar (ocultar informacion sensible)
                current_drawing_tool.store(DrawingTool::Redact);
                drawing_active.store(true);
                RequestOverlayRedraw();
                return 0;
            } else if (wParam == VK_F8) {
                // F8 - Numero de paso
                current_drawing_tool.store(DrawingTool::Step);
                drawing_active.store(true);
                RequestOverlayRedraw();
                return 0;
            } else if (wParam == VK_ESCAPE) {
                if (screenshot_mode.load()) {
                    // Salir del modo captura
                    screenshot_mode.store(false);
                    screenshot_start_x.store(-1);
                    screenshot_start_y.store(-1);
                    screenshot_end_x.store(-1);
                    screenshot_end_y.store(-1);
                    RequestOverlayRedraw();
                } else if (drawing_active.load()) {
                    // Salir del modo dibujo
                    drawing_active.store(false);
                    current_drawing_tool.store(DrawingTool::None);
                    drawing_start_x.store(-1);
                    drawing_start_y.store(-1);
                    RequestOverlayRedraw();
                } else if (zoom_active.load()) {
                    if (text_input_mode.load()) {
                        // Primer Escape: salir del modo texto
                        text_input_mode.store(false);
                        RequestOverlayRedraw();
                    } else {
                        // Segundo Escape: salir del zoom
                        CleanupZoomResources();
                        RequestOverlayRedraw();
                    }
                } else {
                    // Tercer Escape: salir del overlay completamente
                    overlay_active.store(false);
                }
            } else if (wParam == 'Y' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // Ctrl+Y para rehacer
                if (!redo_stack.empty()) {
                    drawing_elements.push_back(std::move(redo_stack.back()));
                    redo_stack.pop_back();
                    RequestOverlayRedraw();
                }
            } else if (wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000) &&
                       (GetKeyState(VK_SHIFT) & 0x8000)) {
                // Ctrl+Shift+Z: rehacer (alternativa habitual a Ctrl+Y)
                if (!redo_stack.empty()) {
                    drawing_elements.push_back(std::move(redo_stack.back()));
                    redo_stack.pop_back();
                    RequestOverlayRedraw();
                }
            } else if (wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // Ctrl+Z para deshacer
                if (!drawing_elements.empty()) {
                    // El elemento deshecho pasa a la pila de rehacer.
                    redo_stack.push_back(std::move(drawing_elements.back()));
                    drawing_elements.pop_back();
                    RequestOverlayRedraw();
                } else if (!screenRectangles.empty()) {
                    screenRectangles.pop_back();
                    // Si era la última región con zoom, limpiar recursos
                    if (screenRectangles.empty() || zoom_active.load()) {
                        CleanupZoomResources();
                    }
                    RequestOverlayRedraw();
                }
            // Ctrl+Enter para captura de pantalla (ahora deshabilitado - usar Shift+Alt+X cuando overlay está activo)
            } else if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // Deshabilitado: ahora usar Shift+Alt+X cuando el overlay está activo
                // Mantener para compatibilidad pero no hacer nada
                LogDebug("ℹ️ Ctrl+Enter disabled - use Shift+Alt+X when overlay is active\n");
                return 0;
            } else if (wParam == 'T' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // Ctrl+T para activar modo texto (con o sin zoom)
                text_input_mode.store(true);
                RequestOverlayRedraw();
            } else if (text_input_mode.load()) {
                // Manejo de texto cuando está en modo edición
                // Las teclas F1-F5 también funcionan en modo texto para herramientas de dibujo
                if (wParam == VK_F1) {
                    // F1 - Activar herramienta Línea
                    current_drawing_tool.store(DrawingTool::Line);
                    drawing_active.store(true);
                    text_input_mode.store(false);
                    RequestOverlayRedraw();
                    return 0;
                } else if (wParam == VK_F2) {
                    // F2 - Activar herramienta Flecha
                    current_drawing_tool.store(DrawingTool::Arrow);
                    drawing_active.store(true);
                    text_input_mode.store(false);
                    RequestOverlayRedraw();
                    return 0;
                } else if (wParam == VK_F3) {
                    // F3 - Activar herramienta Rectángulo
                    current_drawing_tool.store(DrawingTool::Rectangle);
                    drawing_active.store(true);
                    text_input_mode.store(false);
                    RequestOverlayRedraw();
                    return 0;
                // F4 (Texto) eliminado
                } else if (wParam == VK_F4) {
                    // F4 - Activar herramienta Resaltador
                    current_drawing_tool.store(DrawingTool::Highlighter);
                    drawing_active.store(true);
                    text_input_mode.store(false);
                    RequestOverlayRedraw();
                    return 0;
                }
                
                switch (wParam) {
                    case VK_RETURN:
                        // Enter para salto de línea
                        zoom_text.insert(text_cursor_pos.load(), 1, L'\n');
                        text_cursor_pos.store(text_cursor_pos.load() + 1);
                        
                        // Forzar actualización inmediata del cursor
                        RequestOverlayRedraw();
                        
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
                                    RequestOverlayRedraw();
                                }
                            }
                        } else {
                            // Backspace normal: borrar un carácter
                            if (!zoom_text.empty() && text_cursor_pos.load() > 0) {
                                zoom_text.erase(text_cursor_pos.load() - 1, 1);
                                text_cursor_pos.store(text_cursor_pos.load() - 1);
                                RequestOverlayRedraw();
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
                            RequestOverlayRedraw();
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
                            RequestOverlayRedraw();
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
                            RequestOverlayRedraw();
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
                        RequestOverlayRedraw();
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
                            RequestOverlayRedraw();
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
                        RequestOverlayRedraw();
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
                                RequestOverlayRedraw();
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
                                RequestOverlayRedraw();
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
                                RequestOverlayRedraw();
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
                                RequestOverlayRedraw();
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
                                RequestOverlayRedraw();
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
                                RequestOverlayRedraw();
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
                                    RequestOverlayRedraw();
                                }
                            }
                        } else {
                            // Delete normal: borrar un carácter
                            if (!zoom_text.empty() && text_cursor_pos.load() < static_cast<int>(zoom_text.length())) {
                                zoom_text.erase(text_cursor_pos.load(), 1);
                                RequestOverlayRedraw();
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
                        }
                        // Sin Ctrl esta tecla es texto normal: la inserta WM_CHAR, que
                        // respeta la distribucion del teclado, Shift, Bloq Mayus, AltGr,
                        // teclas muertas e IME.
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
                                        RequestOverlayRedraw();
                                    }
                                }
                            }
                        }
                        // Sin Ctrl esta tecla es texto normal: la inserta WM_CHAR, que
                        // respeta la distribucion del teclado, Shift, Bloq Mayus, AltGr,
                        // teclas muertas e IME.
                        break;
                        
                    case 'V':
                        // Ctrl+V para pegar texto, imagen o GIF
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Primero verificar si hay un GIF en el clipboard
                            if (CheckClipboardForGif()) {
                                // Hay un GIF - intentar cargarlo
                                if (AddGifElement()) {
                                    RequestOverlayRedraw();
                                } else {
                                    // Si falla, insertar marcador de error
                                    zoom_text.insert(text_cursor_pos.load(), L"[ERROR GIF]");
                                    text_cursor_pos.store(text_cursor_pos.load() + 10);
                                    RequestOverlayRedraw();
                                }
                            } else if (CheckClipboardForImage()) {
                                // Hay una imagen - intentar capturarla
                                if (AddImageElement()) {
                                    RequestOverlayRedraw();
                                } else {
                                    // Si falla, insertar marcador de error
                                    zoom_text.insert(text_cursor_pos.load(), L"[ERROR IMAGEN]");
                                    text_cursor_pos.store(text_cursor_pos.load() + 15);
                                    RequestOverlayRedraw();
                                }
                            } else {
                                // No hay imagen ni GIF, intentar pegar texto
                                if (OpenClipboard(hCurrentOverlay)) {
                                    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                                    if (hData) {
                                        wchar_t* pszText = (wchar_t*)GlobalLock(hData);
                                        if (pszText) {
                                            std::wstring clipboardText(pszText);
                                            zoom_text.insert(text_cursor_pos.load(), clipboardText);
                                            text_cursor_pos.store(text_cursor_pos.load() + clipboardText.length());
                                            RequestOverlayRedraw();
                                            GlobalUnlock(hData);
                                        }
                                    }
                                    CloseClipboard();
                                }
                            }
                        }
                        // Sin Ctrl esta tecla es texto normal: la inserta WM_CHAR, que
                        // respeta la distribucion del teclado, Shift, Bloq Mayus, AltGr,
                        // teclas muertas e IME.
                        break;
                        
                    case 'A':
                        // Ctrl+A para seleccionar todo el texto
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            text_selection_start.store(0);
                            text_selection_end.store(zoom_text.length());
                            text_selection_active.store(true);
                            text_cursor_pos.store(zoom_text.length());
                            RequestOverlayRedraw();
                        }
                        // Sin Ctrl esta tecla es texto normal: la inserta WM_CHAR, que
                        // respeta la distribucion del teclado, Shift, Bloq Mayus, AltGr,
                        // teclas muertas e IME.
                        break;
                        
                    case 'Z':
                        // Ctrl+Z para deshacer (simplificado)
                        if (GetKeyState(VK_CONTROL) & 0x8000) {
                            // Por ahora solo limpia la selección
                            text_selection_active.store(false);
                            text_selection_start.store(-1);
                            text_selection_end.store(-1);
                            RequestOverlayRedraw();
                        }
                        // Sin Ctrl esta tecla es texto normal: la inserta WM_CHAR, que
                        // respeta la distribucion del teclado, Shift, Bloq Mayus, AltGr,
                        // teclas muertas e IME.
                        break;
                        

                        
                    default:
                        // Las teclas imprimibles se insertan en WM_CHAR.
                        //
                        // Aqui habia una tabla VK->caracter escrita a mano que
                        // asumia teclado US QWERTY (VK 186 == ';'), por lo que en
                        // una distribucion espanola o latinoamericana la puntuacion
                        // salia equivocada. Su rama de "caracteres internacionales"
                        // era codigo muerto: estaba en
                        //   if (wParam >= 32) {...} else if (wParam >= 128) {...}
                        // y como 128 >= 32 nunca se alcanzaba, por lo que las
                        // vocales acentuadas jamas llegaban a insertarse.
                        //
                        // WM_CHAR ya recibe el caracter correcto de Windows, con
                        // distribucion, Shift, Bloq Mayus, AltGr, teclas muertas e
                        // IME resueltos.
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
    RequestOverlayRedraw();
    
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
    LogDebug("  🔑 Registrando hotkey Shift+Alt+X...\n");
    
    // Shift+Alt+X
    if (!RegisterHotKey(hMainWnd, 1, MOD_SHIFT | MOD_ALT, 'X')) {
        LogDebug("  ❌ Error al registrar hotkey Shift+Alt+X\n");
        return false;
    }
    
    LogDebug("  ✅ Hotkey Shift+Alt+X registrado exitosamente\n");
    return true;
}

// Función para procesar mensajes de la ventana principal
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
                case WM_HOTKEY:
            LogDebug("🔥 Hotkey recibido: %d\n", (int)wParam);
            switch (wParam) {
                case 1: // Shift+Alt+X
                    LogDebug("🎯 Hotkey Shift+Alt+X presionado\n");
                    if (!overlay_active.load()) {
                        LogDebug("🚀 Iniciando thread de overlay...\n");
                        std::thread overlay_thread(ShowOverlay);
                        overlay_thread.detach();
                    } else {
                        LogDebug("📸 Overlay active - Activating screenshot mode\n");
                        // Activar modo captura de pantalla cuando el overlay ya está activo
                        screenshot_mode.store(true);
                        drawing_active.store(false);
                        current_drawing_tool.store(DrawingTool::None);
                        text_input_mode.store(false);
                        RequestOverlayRedraw();
                    }
                    break;
                default:
                    LogDebug("❓ Hotkey desconocido: %d\n", (int)wParam);
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
                    
                case MENU_ENABLE_AUTOSTART_ID: // Habilitar Auto-Inicio
                    LogDebug("🚀 Enabling auto-start on login...\n");
                    if (EnableAutoStart()) {
                        MessageBoxW(hMainWnd, 
                            L"Auto-start enabled successfully.\n\n"
                            L"The application will run automatically every time you log into Windows.",
                            L"Auto-Start Enabled", 
                            MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hMainWnd, 
                            L"Error enabling auto-start.\n\n"
                            L"Please verify that you have administrator privileges.",
                            L"Error", 
                            MB_OK | MB_ICONERROR);
                    }
                    break;
                    
                case MENU_DISABLE_AUTOSTART_ID: // Deshabilitar Auto-Inicio
                    LogDebug("🚫 Disabling auto-start on login...\n");
                    if (DisableAutoStart()) {
                        MessageBoxW(hMainWnd, 
                            L"Auto-start disabled successfully.\n\n"
                            L"The application will no longer run automatically when logging in.",
                            L"Auto-Start Disabled", 
                            MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hMainWnd, 
                            L"Error disabling auto-start.",
                            L"Error", 
                            MB_OK | MB_ICONERROR);
                    }
                    break;
                    
                case 1007: // Estado del Auto-Inicio
                    LogDebug("🔍 Showing detailed auto-start status...\n");
                    ShowAutoStartStatus();
                    MessageBoxW(hMainWnd, 
                        L"Detailed auto-start status information has been displayed in the console.\n\n"
                        L"If you don't see the console, run the program with the 'Debug with console' option.",
                        L"Auto-Start Status", 
                        MB_OK | MB_ICONINFORMATION);
                    break;
                    
                case 1010: // Normal Mode
                    current_resource_mode.store(ResourceMode::Normal);
                    ApplyResourceModeSettings();
                    SaveConfiguration();
                    MessageBoxW(hMainWnd, 
                        L"Normal Mode activated.\n\n"
                        L"Standard resource usage and performance settings applied.",
                        L"Resource Mode", 
                        MB_OK | MB_ICONINFORMATION);
                    break;
                    
                case MENU_LOW_RESOURCES_ID: // Low Resources Mode
                    current_resource_mode.store(ResourceMode::LowResources);
                    ApplyResourceModeSettings();
                    SaveConfiguration();
                    MessageBoxW(hMainWnd, 
                        L"Low Resources Mode activated.\n\n"
                        L"Optimized for minimal CPU, memory and resource usage.\n"
                        L"• Reduced cache sizes\n"
                        L"• Lower refresh rates\n"
                        L"• Simplified rendering\n"
                        L"• Memory optimizations",
                        L"Low Resources Mode", 
                        MB_OK | MB_ICONINFORMATION);
                    break;
                    
                case MENU_HIGH_PERFORMANCE_ID: // High Performance Mode
                    current_resource_mode.store(ResourceMode::HighPerformance);
                    ApplyResourceModeSettings();
                    SaveConfiguration();
                    MessageBoxW(hMainWnd, 
                        L"High Performance Mode activated.\n\n"
                        L"Maximum performance and responsiveness.\n"
                        L"• Pre-allocated memory pools\n"
                        L"• Larger cache sizes\n"
                        L"• Higher refresh rates\n"
                        L"• Advanced optimizations\n"
                        L"• Priority resource allocation",
                        L"High Performance Mode", 
                        MB_OK | MB_ICONINFORMATION);
                    break;
                    
                case MENU_EXIT_ID: // Salir
                    running.store(false);
                    PostQuitMessage(0);
                    break;
            }
            break;
            
        case WM_TIMER:
            if (wParam == 1) { // Timer de verificación del system tray
                LogDebug("⏰ Verificando estado del system tray...\n");
                
                // Verificar si el icono del system tray está visible
                if (systemTrayInitialized) {
                    // Enviar mensaje de prueba al system tray
                    if (!Shell_NotifyIcon(NIM_MODIFY, &nid)) {
                        LogDebug("⚠️ Icono del system tray no responde - Restaurando...\n");
                        if (RestoreSystemTrayIcon()) {
                            LogDebug("✅ System tray restaurado por timer\n");
                        } else {
                            LogDebug("❌ Error al restaurar system tray por timer\n");
                        }
                    } else {
                        LogDebug("✅ System tray responde correctamente\n");
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

// Función para mostrar información detallada del estado del auto-inicio
void ShowAutoStartStatus() {
    LogDebug("\n🔍 === ESTADO DETALLADO DEL AUTO-INICIO ===\n");
    
    // Verificar permisos de administrador
    bool isAdmin = IsRunningAsAdministrator();
    LogDebug("👤 Administrator privileges: %s\n", isAdmin ? "✅ YES" : "❌ NO");
    
    // Verificar si está habilitado
    bool isEnabled = IsAutoStartEnabled();
    LogDebug("🚀 Auto-start enabled: %s\n", isEnabled ? "✅ YES" : "❌ NO");
    
    // Mostrar información del registro
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, 
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
        0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        LogDebug("📋 Clave del registro: ✅ Accesible\n");
        
        if (isEnabled) {
            wchar_t valueData[MAX_PATH];
            DWORD dataSize = sizeof(valueData);
            DWORD dataType = REG_SZ;
            
            result = RegQueryValueExW(hKey, L"Screen Highlighter", NULL, &dataType, 
                                     (LPBYTE)valueData, &dataSize);
            
            if (result == ERROR_SUCCESS) {
                LogDebug("📁 Ruta en el registro: %ls\n", valueData);
                
                // Verificar si el archivo existe
                DWORD fileAttributes = GetFileAttributesW(valueData);
                if (fileAttributes != INVALID_FILE_ATTRIBUTES) {
                    LogDebug("✅ Archivo ejecutable: Existe y es accesible\n");
                } else {
                    LogDebug("❌ Archivo ejecutable: No existe o no es accesible\n");
                }
            } else {
                LogDebug("❌ Error al leer valor del registro: %ld\n", result);
            }
        }
        
        RegCloseKey(hKey);
    } else {
        LogDebug("📋 Clave del registro: ❌ No accesible (Error: %ld)\n", result);
    }
    
    // Mostrar ruta actual del ejecutable
    wchar_t currentExePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, currentExePath, MAX_PATH) > 0) {
        LogDebug("📁 Ruta actual del ejecutable: %ls\n", currentExePath);
    } else {
        LogDebug("❌ No se pudo obtener la ruta del ejecutable\n");
    }
    
    LogDebug("===========================================\n\n");
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow; // Parámetros no utilizados
    
    // Debe hacerse antes de crear cualquier ventana o consultar metricas.
    EnablePerMonitorDpiAwareness();

    LogDebug("🚀 Iniciando Screen Highlighter...\n");
    
    // Verificar permisos de administrador
    if (!IsRunningAsAdministrator()) {
        LogDebug("⚠️ Application requires administrator privileges\n");
        LogDebug("🔐 Requesting privilege elevation...\n");
        
        if (RequestAdminPrivileges()) {
            LogDebug("✅ Administrator privileges requested successfully\n");
            LogDebug("🔄 Closing current instance...\n");
            return 0; // Cerrar esta instancia
        } else {
            LogDebug("❌ Could not obtain administrator privileges\n");
            MessageBoxW(NULL, 
                L"Screen Highlighter requires administrator privileges to function correctly.\n\n"
                L"Please run the application as administrator.",
                L"Privileges Required", 
                MB_OK | MB_ICONWARNING);
            return 1;
        }
    }
    
    LogDebug("✅ Administrator privileges verified\n");
    
    // Cargar configuración desde archivo .ini al inicio
    MigrateLegacyConfig();
    LoadConfiguration();
    LogDebug("✅ Configuration loaded\n");
    
    // Aplicar configuraciones del modo de recursos
    LogDebug("🎯 Aplicando configuraciones del modo de recursos...\n");
    ApplyResourceModeSettings();
    
    // Inicializar GDI+ (decodificacion de GIF y codificacion de PNG)
    if (!InitializeGdiPlus()) {
        LogDebug("No se pudo inicializar GDI+: sin soporte de GIF ni guardado PNG\n");
    }

    // Inicializar cache de recursos GDI para optimización de performance
    LogDebug("🔧 Inicializando cache de recursos GDI...\n");
    GdiCache::InitializeCache();
    LogDebug("✅ Cache de recursos GDI inicializado\n");
    
    // Pre-allocar vectores para mejor performance
    LogDebug("🔧 Pre-allocando vectores...\n");
    PreAllocateVectors();
    LogDebug("✅ Vectores pre-allocados\n");
                
    // Crear una ventana oculta para manejar mensajes
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ScreenHighlighterClass";
    wc.hIcon = LoadIconFromFile(TRAY_ICON_LARGE);
    wc.hIconSm = LoadIconFromFile(TRAY_ICON_SMALL);
    
                if (!RegisterClassExW(&wc)) {
        LogDebug("❌ Error al registrar clase principal\n");
        return 1;
    }
    LogDebug("✅ Clase principal registrada\n");
    
    // Registrar clase para el overlay
    WNDCLASSEXW wcOverlay = {};
    wcOverlay.cbSize = sizeof(WNDCLASSEXW);
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.lpszClassName = L"ScreenHighlighterOverlayClass";
    wcOverlay.hCursor = LoadCursor(NULL, IDC_CROSS);
    
    if (!RegisterClassExW(&wcOverlay)) {
        LogDebug("❌ Error al registrar clase overlay\n");
        return 1;
    }
    LogDebug("✅ Clase overlay registrada\n");
    
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
        LogDebug("❌ Error al registrar clase settings\n");
        return 1;
    }
    LogDebug("✅ Clase settings registrada\n");
    
    hMainWnd = CreateWindowExW(
        0,
        L"ScreenHighlighterClass",
        L"Screen Highlighter",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hMainWnd) {
        LogDebug("❌ Error al crear ventana principal\n");
        return 1;
    }
    LogDebug("✅ Ventana principal creada\n");
    
    // Ocultar la ventana
    ShowWindow(hMainWnd, SW_HIDE);
    
    // Agregar icono al system tray
    LogDebug("🔧 Agregando icono al system tray...\n");
    if (!AddToSystemTray()) {
        LogDebug("❌ Error al agregar icono al system tray\n");
        MessageBoxW(NULL, L"Error al agregar icono al system tray", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    LogDebug("✅ Icono agregado al system tray\n");
    
    // Registrar hotkeys
    LogDebug("🔧 Registrando hotkeys...\n");
    if (!RegisterHotkeys()) {
        LogDebug("❌ Error al registrar hotkeys\n");
        MessageBoxW(NULL, L"Error al registrar hotkeys", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    LogDebug("✅ Hotkeys registrados\n");
    
    // Verificar estado de auto-ejecución
    if (IsAutoStartEnabled()) {
        LogDebug("🚀 Auto-start on login: ENABLED\n");
    } else {
        LogDebug("🚫 Auto-start on login: DISABLED\n");
    }
    
    // Mostrar estado detallado del auto-inicio
    ShowAutoStartStatus();
    
    // Iniciar monitoreo de explorer.exe para restauración automática del system tray
    LogDebug("🔍 Iniciando monitoreo de explorer.exe...\n");
    StartExplorerMonitoring();
    LogDebug("✅ Monitoreo de explorer.exe iniciado\n");
    
    // Configurar timer para verificar periódicamente el system tray
    SetTimer(hMainWnd, 1, 30000, NULL); // Verificar cada 30 segundos
    LogDebug("⏰ System tray verification timer configured\n");
    
    // Bucle principal del mensaje
    LogDebug("🔄 Starting main message loop...\n");
    LogDebug("💡 Press Shift+Alt+X to activate highlight\n");
    LogDebug("💡 Look for the green icon in the system tray\n");
    
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
    LogDebug("🛑 Deteniendo monitoreo de explorer.exe...\n");
    StopExplorerMonitoring();
    
    // Remover del system tray
    RemoveFromSystemTray();
    
    // Limpiar cache de recursos GDI
    LogDebug("🧹 Limpiando cache de recursos GDI...\n");
    GdiCache::CleanupCache();

    // Cerrar GDI+ despues de liberar todo lo que pueda depender de el.
    ShutdownGdiPlus();
    LogDebug("✅ Cache de recursos GDI limpiado\n");
    
    return 0;
}
