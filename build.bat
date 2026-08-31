@echo off
echo ========================================
echo    Screen Highlighter - Compilacion
echo ========================================
echo.

echo Configurando proyecto...
mkdir build 2>nul
cd build

cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDEBUG_MODE=ON -DENABLE_CONSOLE=OFF -DSILENT_DEBUG=ON

if %ERRORLEVEL% neq 0 (
    echo ❌ Error en la configuración
    pause
    exit /b 1
)

echo.
echo Compilando proyecto...
cmake --build . --config Debug

if %ERRORLEVEL% neq 0 (
    echo ❌ Error en la compilación
    pause
    exit /b 1
)

echo.
echo ========================================
echo    COMPILACIÓN EXITOSA!
echo ========================================
echo.
echo Ejecutable creado en: release\ScreenHighlighter.exe
echo.
echo CARACTERÍSTICAS:
echo ✅ Funcionalidad debug completa
echo ✅ Sin ventana de consola
echo ✅ Hotkeys funcionan (Shift+Alt+X)
echo ✅ System tray funciona
echo ✅ Auto-inicio mejorado
echo ✅ Modo captura con Shift+Alt+X
echo ✅ Permisos de administrador automáticos
echo.
echo ¿Deseas ejecutar la aplicación? (S/N)
set /p choice=
if /i "%choice%"=="S" (
    echo.
    echo 🚀 Ejecutando Screen Highlighter...
    echo.
    echo 💡 Presiona Shift+Alt+X para activar el highlight
    echo 💡 Busca el icono en el system tray
    echo.
    ..\release\ScreenHighlighter.exe
) else (
    echo.
    echo ✅ Compilación completada. Ejecuta release\ScreenHighlighter.exe cuando quieras.
)

pause
