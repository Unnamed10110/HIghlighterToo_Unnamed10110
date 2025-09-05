cmake_minimum_required(VERSION 3.16)
project(ScreenHighlighter VERSION 1.0.0 LANGUAGES CXX)

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

# Crear ejecutable principal
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

# Configurar propiedades del ejecutable
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "ScreenHighlighter"
    WIN32_EXECUTABLE TRUE
)

# Vincular manifest para permisos de administrador
if(WIN32)
    # Configuraciones específicas para MSVC
    if(MSVC)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/MANIFEST:NO"
        )
    endif()
    
    # Agregar manifest como recurso (solo para MSVC)
    if(MSVC)
        target_sources(${PROJECT_NAME} PRIVATE
            "${CMAKE_BINARY_DIR}/admin_manifest.manifest"
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

# Configurar instalación
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Configurar archivos de configuración - copiar a la raíz
configure_file(
    "${CMAKE_SOURCE_DIR}/config/ScreenHighlighter.ini.in"
    "${CMAKE_SOURCE_DIR}/ScreenHighlighter.ini"
    @ONLY
)

# Configurar manifest para permisos de administrador
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
