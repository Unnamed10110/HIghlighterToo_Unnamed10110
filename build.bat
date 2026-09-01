@echo off
setlocal

rem Este script se mantiene en ASCII puro a proposito. cmd.exe relee el .bat por
rem desplazamiento de bytes despues de cada linea, y los caracteres multibyte
rem (acentos, iconos) descuadran ese conteo: las lineas siguientes se ejecutan
rem cortadas por la mitad.
rem
rem Tampoco se usa "enabledelayedexpansion": vcvars64.bat manipula rutas que
rem pueden contener "!" y la expansion retardada se las comeria.

rem Raiz del proyecto = carpeta de este .bat, sin la barra final. Se ancla asi
rem para que el script funcione igual invocado desde cualquier directorio.
set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "EXE_NAME=ScreenHighlighter.exe"
set "TARGET=%ROOT%\%EXE_NAME%"
set "BUILD_DIR=%ROOT%\build"

echo ========================================
echo    Screen Highlighter - Compilacion
echo ========================================
echo.
echo Raiz del proyecto: %ROOT%
echo.

rem ---------------------------------------------------------------------------
rem Seleccion del toolchain
rem
rem Antes el generador estaba fijo en "MinGW Makefiles" y el script moria con
rem "CMAKE_MAKE_PROGRAM is not set" en cualquier equipo sin MinGW. Se prefiere
rem MinGW si esta en el PATH, y si no se recurre a MSVC + Ninja, que ya vienen
rem con Visual Studio.
rem ---------------------------------------------------------------------------
rem Las comprobaciones usan "%ERRORLEVEL% neq 0" y no "if errorlevel 1".
rem "if errorlevel N" es una comparacion mayor-o-igual, y cmake --build
rem devuelve -1 cuando ninja falla: con "if errorlevel 1" el fallo pasaba
rem inadvertido y el script anunciaba COMPILACION EXITOSA.

set "GENERATOR="
set "CMAKE_EXTRA="

where g++ >nul 2>&1
if %ERRORLEVEL% neq 0 goto :try_msvc
where mingw32-make >nul 2>&1
if %ERRORLEVEL% equ 0 goto :use_mingw
where make >nul 2>&1
if %ERRORLEVEL% neq 0 goto :try_msvc

:use_mingw
set "GENERATOR=MinGW Makefiles"
echo Toolchain: MinGW (g++)
goto :configure

:try_msvc
echo MinGW no esta en el PATH; buscando Visual Studio...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_toolchain

set "VSPATH="
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%I"
if not defined VSPATH goto :no_toolchain

set "VCVARS=%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" goto :no_toolchain

echo Toolchain: MSVC
echo Visual Studio: %VSPATH%
call "%VCVARS%" >nul 2>&1

rem vcvars64.bat puede "fallar" por avisos internos sin dejar de configurar el
rem entorno, asi que lo que se comprueba es que cl.exe quede accesible.
where cl >nul 2>&1
if %ERRORLEVEL% neq 0 goto :no_toolchain

rem Ninja viene incluido con Visual Studio; si no estuviera, se usa el del PATH.
set "NINJA=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if exist "%NINJA%" goto :have_ninja
set "NINJA="
for /f "delims=" %%I in ('where ninja 2^>nul') do set "NINJA=%%I"
if not defined NINJA goto :no_toolchain

:have_ninja
set "GENERATOR=Ninja"
rem Las comillas van dentro del valor: la ruta de Ninja lleva espacios y sin
rem ellas CMake recibe "C:\Program" y el resto como argumentos sueltos.
set CMAKE_EXTRA=-DCMAKE_MAKE_PROGRAM="%NINJA%"

:configure
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 goto :no_cmake

rem Un cache generado con otro generador hace que CMake aborte. Como el
rem toolchain elegido puede cambiar de un equipo a otro, se descarta.
if not exist "%BUILD_DIR%\CMakeCache.txt" goto :configure_now
findstr /c:"CMAKE_GENERATOR:INTERNAL=%GENERATOR%" "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Cache de CMake de otro generador; se descarta build\
    rd /s /q "%BUILD_DIR%"
)

:configure_now
echo.
echo Configurando proyecto con el generador "%GENERATOR%"...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "%GENERATOR%" %CMAKE_EXTRA% -DCMAKE_BUILD_TYPE=Debug -DDEBUG_MODE=ON -DENABLE_CONSOLE=OFF -DSILENT_DEBUG=ON
if %ERRORLEVEL% neq 0 goto :cmake_error

echo.
echo Compilando proyecto...
cmake --build "%BUILD_DIR%"
if %ERRORLEVEL% neq 0 goto :build_error

rem CMake ya deja el .exe en la raiz (RUNTIME_OUTPUT_DIRECTORY), pero los
rem generadores multi-configuracion y algunos toolchains lo dejan dentro de
rem build\. Si no aparecio en la raiz se busca y se copia, para que el resultado
rem del script sea siempre el mismo: %ROOT%\ScreenHighlighter.exe
rem
rem "for /r" con un nombre literal genera un candidato por cada carpeta sin
rem comprobar si existe, de ahi el "if exist". La condicion sobre %TARGET% se
rem evalua en cada vuelta porque lo que cambia es el archivo, no la ruta.
if not exist "%TARGET%" (
    echo.
    echo El ejecutable no quedo en la raiz; buscando dentro de build\ ...
    for /r "%BUILD_DIR%" %%F in ("%EXE_NAME%") do (
        if not exist "%TARGET%" if exist "%%F" copy /y "%%F" "%TARGET%" >nul
    )
)

if not exist "%TARGET%" goto :missing_exe

echo.
echo ========================================
echo    COMPILACION EXITOSA
echo ========================================
echo.
echo Ejecutable creado en: %TARGET%
echo.
echo Deseas ejecutar la aplicacion? (S/N)
set /p choice=
if /i "%choice%"=="S" (
    echo.
    echo Ejecutando Screen Highlighter...
    echo Presiona Shift+Alt+X para activar el highlight
    echo Busca el icono en el system tray
    start "" "%TARGET%"
) else (
    echo.
    echo Compilacion completada. Ejecuta %EXE_NAME% cuando quieras.
)
goto :end

:no_toolchain
echo.
echo [ERROR] No se encontro ningun compilador de C++.
echo         Instala una de estas dos opciones:
echo           - Visual Studio con la carga de trabajo "Desarrollo para el
echo             escritorio con C++" (incluye MSVC y Ninja), o
echo           - MinGW-w64, con g++ y mingw32-make en el PATH.
goto :fail

:no_cmake
echo.
echo [ERROR] cmake no esta en el PATH. Instalalo desde https://cmake.org
goto :fail

:cmake_error
echo.
echo [ERROR] Fallo la configuracion de CMake
goto :fail

:build_error
echo.
echo [ERROR] Fallo la compilacion
goto :fail

:missing_exe
echo.
echo [ERROR] No se encontro %EXE_NAME% tras la compilacion
goto :fail

:fail
pause
endlocal
exit /b 1

:end
pause
endlocal
