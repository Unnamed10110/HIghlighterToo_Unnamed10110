@echo off
echo ========================================
echo    Screen Highlighter Debug Silencioso
echo ========================================
echo.
echo MODO DEBUG SILENCIOSO:
echo - Funcionalidad debug completa
echo - Sin ventana de consola
echo - Hotkeys y system tray funcionan
echo - Aplicación optimizada para uso diario
echo - Requiere permisos de administrador (UAC automático)
echo.

REM Verificar si CMake está disponible
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake no está disponible en el PATH
    echo Por favor instala CMake y agrégalo al PATH
    pause
    exit /b 1
)

REM Crear directorio de build
if not exist "build_debug_silent" mkdir build_debug_silent
cd build_debug_silent

REM Configurar proyecto en modo debug silencioso
echo Configurando proyecto en modo debug silencioso...
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDEBUG_MODE=ON -DENABLE_CONSOLE=OFF -DSILENT_DEBUG=ON
if errorlevel 1 (
    echo ERROR: Falló la configuración de CMake
    pause
    exit /b 1
)

REM Compilar proyecto
echo.
echo Compilando proyecto en modo debug silencioso...
cmake --build . --config Debug
if errorlevel 1 (
    echo ERROR: Falló la compilación
    pause
    exit /b 1
)

REM Verificar que el ejecutable se creó
if exist "bin\ScreenHighlighter.exe" (
    echo.
    echo ========================================
    echo    COMPILACIÓN DEBUG SILENCIOSO EXITOSA!
    echo ========================================
    echo.
    echo Ejecutable creado en: build_debug_silent\bin\ScreenHighlighter.exe
    echo.
    echo CARACTERÍSTICAS:
echo ✅ Funcionalidad debug completa
echo ✅ Sin ventana de consola
echo ✅ Hotkeys funcionan (Shift+Alt+X)
echo ✅ System tray funciona
echo ✅ Aplicación optimizada para uso diario
echo ✅ Permisos de administrador automáticos
echo.
    echo ¿Deseas ejecutar la aplicación? (S/N)
    set /p choice=
    if /i "%choice%"=="S" (
        echo.
        echo 🚀 Ejecutando Screen Highlighter en modo debug silencioso...
        echo.
        echo 💡 Presiona Shift+Alt+X para activar el highlight
        echo 💡 Busca el icono en el system tray
        echo.
        start "" "bin\ScreenHighlighter.exe"
        echo.
        echo ✅ Aplicación ejecutándose en segundo plano
        echo.
        cd ..
    )
) else (
    echo ERROR: No se encontró el ejecutable
)

echo.
pause
