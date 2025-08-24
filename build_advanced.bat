@echo off
echo ========================================
echo    Screen Highlighter Advanced Build
echo ========================================
echo.
echo Selecciona el modo de compilación:
echo.
echo 1. Release (Optimizado, sin consola)
echo 2. Debug (Con consola, para desarrollo)
echo 3. Release con consola (Optimizado + debug)
echo 4. Debug Silencioso (Sin consola, funcionalidad debug)
echo 5. Solo compilar (sin ejecutar)
echo.
set /p choice="Elige una opción (1-5): "

if "%choice%"=="1" goto :release
if "%choice%"=="2" goto :debug
if "%choice%"=="3" goto :release_console
if "%choice%"=="4" goto :debug_silent
if "%choice%"=="5" goto :compile_only
echo Opción inválida
pause
exit /b 1

:release
echo.
echo ========================================
echo    COMPILANDO EN MODO RELEASE
echo ========================================
echo.
if not exist "build" mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DDEBUG_MODE=OFF -DENABLE_CONSOLE=OFF
if errorlevel 1 goto :error
cmake --build . --config Release
if errorlevel 1 goto :error
echo.
echo ✅ COMPILACIÓN RELEASE EXITOSA!
echo.
echo Ejecutable: build\bin\ScreenHighlighter.exe
echo Modo: Release optimizado (sin consola)
echo.
goto :success

:debug
echo.
echo ========================================
echo    COMPILANDO EN MODO DEBUG
echo ========================================
echo.
if not exist "build_debug" mkdir build_debug
cd build_debug
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDEBUG_MODE=ON -DENABLE_CONSOLE=ON
if errorlevel 1 goto :error
cmake --build . --config Debug
if errorlevel 1 goto :error
echo.
echo ✅ COMPILACIÓN DEBUG EXITOSA!
echo.
echo Ejecutable: build_debug\bin\ScreenHighlighter.exe
echo Modo: Debug con consola
echo.
goto :success

:release_console
echo.
echo ========================================
echo    COMPILANDO EN MODO RELEASE + CONSOLA
echo ========================================
echo.
if not exist "build_release_console" mkdir build_release_console
cd build_release_console
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DDEBUG_MODE=OFF -DENABLE_CONSOLE=ON
if errorlevel 1 goto :error
cmake --build . --config Release
if errorlevel 1 goto :error
echo.
echo ✅ COMPILACIÓN RELEASE + CONSOLA EXITOSA!
echo.
echo Ejecutable: build_release_console\bin\ScreenHighlighter.exe
echo Modo: Release optimizado + consola para debug
echo.
goto :success

:debug_silent
echo.
echo ========================================
echo    COMPILANDO EN MODO DEBUG SILENCIOSO
echo ========================================
echo.
if not exist "build_debug_silent" mkdir build_debug_silent
cd build_debug_silent
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDEBUG_MODE=ON -DENABLE_CONSOLE=OFF -DSILENT_DEBUG=ON
if errorlevel 1 goto :error
cmake --build . --config Debug
if errorlevel 1 goto :error
echo.
echo ✅ COMPILACIÓN DEBUG SILENCIOSO EXITOSA!
echo.
echo Ejecutable: build_debug_silent\bin\ScreenHighlighter.exe
echo Modo: Debug silencioso (sin consola, funcionalidad completa)
echo.
goto :success

:compile_only
echo.
echo ========================================
echo    COMPILANDO SOLO (SIN EJECUTAR)
echo ========================================
echo.
if not exist "build" mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DDEBUG_MODE=OFF -DENABLE_CONSOLE=OFF
if errorlevel 1 goto :error
cmake --build . --config Release
if errorlevel 1 goto :error
echo.
echo ✅ COMPILACIÓN EXITOSA!
echo.
echo Ejecutable: build\bin\ScreenHighlighter.exe
echo.
goto :end

:error
echo.
echo ❌ ERROR EN LA COMPILACIÓN
echo.
pause
exit /b 1

:success
echo ¿Deseas ejecutar la aplicación? (S/N)
set /p run_choice=
if /i "%run_choice%"=="S" (
    echo.
    echo 🚀 Ejecutando Screen Highlighter...
    echo.
    if "%choice%"=="1" (
        start "" "bin\ScreenHighlighter.exe"
    ) else if "%choice%"=="2" (
        echo Presiona Ctrl+C para detener
        .\bin\ScreenHighlighter.exe
    ) else if "%choice%"=="3" (
        echo Presiona Ctrl+C para detener
        .\bin\ScreenHighlighter.exe
    ) else if "%choice%"=="4" (
        echo.
        echo 🚀 Ejecutando Screen Highlighter en modo debug silencioso...
        echo.
        echo 💡 Presiona Shift+Alt+X para activar el highlight
echo 💡 Busca el icono en el system tray
echo 💡 La aplicación solicitará permisos de administrador automáticamente
echo.
start "" "bin\ScreenHighlighter.exe"
echo.
echo ✅ Aplicación ejecutándose en segundo plano
echo 🔐 Se solicitarán permisos de administrador (UAC)
echo.
    )
)

:end
echo.
pause
