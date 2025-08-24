@echo off
setlocal enabledelayedexpansion
echo Compilando ScreenHighlighter...

REM Verificar si g++ está disponible
where g++ >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: g++ no encontrado. Por favor instala MinGW-w64 o MSYS2.
    echo Puedes descargarlo desde: https://www.mingw-w64.org/
    exit /b 1
)

REM Verificar si windres está disponible para el manifiesto
where windres >nul 2>nul
if %errorlevel% neq 0 (
    echo Advertencia: windres no encontrado. El programa se compilará sin manifiesto.
    echo Esto significa que no solicitará permisos de administrador automáticamente.
)

REM Crear archivo .res para el manifiesto e icono
echo #include ^<windows.h^> > temp.rc
echo 1 24 "ScreenHighlighter.manifest" >> temp.rc
echo IDI_ICON1 ICON "misc01.ico" >> temp.rc
windres temp.rc temp.o 2>nul

REM Compilar el programa completamente estático (standalone) con warnings habilitados
if exist temp.o (
    g++ -std=c++17 -O3 -DNDEBUG -DWIN32_LEAN_AND_MEAN -mwindows -static -static-libgcc -static-libstdc++ -Wall -Wextra -Wpedantic -Werror -o ScreenHighlighter.exe main.cpp temp.o -luser32 -lgdi32 -lkernel32 -lshell32 -lole32 -luuid -loleaut32 -lwinmm -lmsimg32
    if !errorlevel! equ 0 (
        del temp.rc temp.o 2>nul
        goto :success
    ) else (
        goto :error
    )
) else (
    echo Advertencia: No se pudo crear el manifiesto, compilando sin él...
    g++ -std=c++17 -O3 -DNDEBUG -DWIN32_LEAN_AND_MEAN -mwindows -static -static-libgcc -static-libstdc++ -Wall -Wextra -Wpedantic -Werror -o ScreenHighlighter.exe main.cpp -luser32 -lgdi32 -lkernel32 -lshell32 -lole32 -luuid -loleaut32 -lwinmm -lmsimg32
    if !errorlevel! equ 0 (
        goto :success
    ) else (
        goto :error
    )
)

:success
    echo.
    echo ✅ Compilación exitosa! Se creó ScreenHighlighter.exe
    echo.
    REM Mostrar información del ejecutable
    for %%A in (ScreenHighlighter.exe) do (
        echo 📁 Archivo: %%~nxA
        echo 📏 Tamaño: %%~zA bytes
        echo 🎯 Tipo: Ejecutable standalone (completamente estático)
    )
    echo.
    echo 🚀 Para ejecutar el programa:
    echo   ScreenHighlighter.exe
    echo.
    echo 📋 Uso:
    echo   - Presiona Shift+Alt+X para activar el highlight
    echo   - Usa el scroll del mouse para hacer zoom
    echo   - Haz clic para finalizar la selección
    echo   - Usa el menú del icono en la bandeja para salir
    echo.
    echo 💡 El programa es completamente autocontenible y no necesita dependencias externas.
    echo.
    goto :end

:error
    echo.
    echo ❌ Error en la compilación!
    echo.

:end

