cmake_minimum_required(VERSION 3.16)
project(ScreenHighlighter VERSION 2.0.0 LANGUAGES CXX)

# Configurar estándar C++
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configurar características específicas de C++23
set(CMAKE_CXX_EXTENSIONS OFF)

# Opciones de compilación
option(DEBUG_MODE "Compilar en modo debug sin mwindows" OFF)
option(ENABLE_CONSOLE "Habilitar consola para mensajes de debug" OFF)
option(SILENT_DEBUG "Modo debug silencioso (sin consola, con debug)" OFF)

# Configuraciones específicas para Windows
if(WIN32)
    # Configurar para aplicación Win32 sin consola
    set(CMAKE_WIN32_EXECUTABLE TRUE)

    # windows.h define macros min/max que rompen std::min / std::max.
    # NOMINMAX las desactiva (obligatorio para compilar con MSVC).
    add_compile_definitions(NOMINMAX)
    
    # Configurar flags de compilación específicos para el compilador
    if(MSVC)
        # Visual Studio - habilitar features de C++23
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /std:c++latest")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /std:c++latest")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
    elseif(MINGW)
        # MinGW - habilitar features de C++23
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++23")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -std=c++23")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
        
        # Solo usar mwindows si no está en modo debug y no se habilita consola
        if(NOT DEBUG_MODE AND NOT ENABLE_CONSOLE AND NOT SILENT_DEBUG)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")
        endif()
    else()
        # Otros compiladores
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -std=c++23")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -std=c++23")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
    endif()
endif()

# Encontrar librerías del sistema
find_package(PkgConfig QUIET)

# Configurar librerías de Windows
if(WIN32)
    # Librerías del sistema Windows
    set(WINDOWS_LIBRARIES
        comctl32
        shell32
        user32
        gdi32
        winmm
        ole32
        uuid
        gdiplus
        shlwapi
        windowscodecs
        msimg32          # AlphaBlend (resaltador translucido)
    )
endif()

# Archivos fuente
set(SOURCES
    main.cpp
)

# Archivos de cabecera
set(HEADERS
    # Agregar aquí archivos de cabecera si los hay
)

# Archivos de recursos (solo para Windows)
if(WIN32)
    set(RESOURCES
        resources.rc
    )
endif()

# El compilador de recursos debe encontrar admin_manifest.manifest, que se genera
# tanto en el directorio fuente como en el de compilacion (windres se ejecuta
# desde el directorio de compilacion, rc.exe resuelve junto al .rc).
if(WIN32)
    include_directories("${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}")
endif()

# Crear ejecutable principal
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

# Configurar propiedades del ejecutable
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "ScreenHighlighter"
    WIN32_EXECUTABLE TRUE
)

# Vincular manifest para permisos de administrador
if(WIN32)
    # El manifiesto se incrusta via resources.rc (entrada RT_MANIFEST), lo que
    # funciona tanto en MinGW como en MSVC. Se desactiva el manifiesto que el
    # linker de MSVC genera por su cuenta para evitar duplicados.
    if(MSVC)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/MANIFEST:NO"
        )
    endif()
endif()

# Vincular librerías
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ${WINDOWS_LIBRARIES})
endif()

# Configurar directorio de salida - compilar directamente en la raíz
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Los generadores multi-configuración (Visual Studio, Ninja Multi-Config) agregan
# el nombre de la configuración a RUNTIME_OUTPUT_DIRECTORY, con lo que el .exe
# terminaba en Release/ o Debug/ en vez de la raíz. Las propiedades por
# configuración NO reciben ese sufijo, así que fijándolas todas el ejecutable
# queda en la raíz con cualquier generador, no solo con MinGW Makefiles.
foreach(SH_CFG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${SH_CFG} "${CMAKE_SOURCE_DIR}"
    )
endforeach()

# Configurar instalación
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Valores por defecto del .ini. Sin este include los marcadores @...@ de la
# plantilla quedaban vacios, generando un ScreenHighlighter.ini con lineas como
# "overlay_opacity=" que la aplicacion no podia interpretar.
include("${CMAKE_SOURCE_DIR}/config/CMakeConfig.cmake")

# Configurar archivos de configuración - copiar a la raíz.
# La aplicación en ejecución usa %APPDATA%\ScreenHighlighter\ScreenHighlighter.ini
# y, en el primer arranque, migra el .ini que encuentre junto al ejecutable.
configure_file(
    "${CMAKE_SOURCE_DIR}/config/ScreenHighlighter.ini.in"
    "${CMAKE_SOURCE_DIR}/ScreenHighlighter.ini"
    @ONLY
)

# Configurar manifest para permisos de administrador.
# Se genera junto a resources.rc para que windres (MinGW) lo encuentre por ruta
# relativa: resources.rc lo incrusta con "1 24 admin_manifest.manifest".
configure_file(
    "${CMAKE_SOURCE_DIR}/config/admin_manifest.manifest.in"
    "${CMAKE_SOURCE_DIR}/admin_manifest.manifest"
    @ONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/config/admin_manifest.manifest.in"
    "${CMAKE_BINARY_DIR}/admin_manifest.manifest"
    @ONLY
)

# Configurar archivos de recursos - copiar a la raíz
if(EXISTS "${CMAKE_SOURCE_DIR}/misc01.ico")
    configure_file(
        "${CMAKE_SOURCE_DIR}/misc01.ico"
        "${CMAKE_SOURCE_DIR}/misc01.ico"
        COPYONLY
    )
endif()

# Configurar para diferentes configuraciones
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_BUILD)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE RELEASE_BUILD)
endif()

# Mensaje de configuración
message(STATUS "Configurando ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Debug mode: ${DEBUG_MODE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "C++ Extensions: ${CMAKE_CXX_EXTENSIONS}")
message(STATUS "Output directory: ${CMAKE_SOURCE_DIR}")
