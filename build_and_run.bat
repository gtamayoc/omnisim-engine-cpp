@echo off
setlocal

echo ===============================
echo OmniSim Engine - Build ^& Run
echo ===============================

set "BUILD_DIR=build"
set "EXEC_GUI=omnisim_projectile_2d.exe"
set "EXEC_CLI=omnisim_cli.exe"
set "SIM_ARG=%~1"
if "%SIM_ARG%"=="" (
    set "SIM_ARG=projectile"
)

where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake no esta en PATH.
    echo Instala CMake o agrega su carpeta bin al PATH.
    pause
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    echo Creando carpeta build...
    mkdir "%BUILD_DIR%"
    if %ERRORLEVEL% neq 0 (
        echo ERROR: No se pudo crear la carpeta build.
        pause
        exit /b 1
    )
)

pushd "%BUILD_DIR%"

echo.
echo Generando proyecto con CMake...
cmake ..
if %ERRORLEVEL% neq 0 (
    echo ERROR: Fallo en configuracion de CMake.
    popd
    pause
    exit /b 1
)

echo.
echo Compilando proyecto...
cmake --build .
if %ERRORLEVEL% neq 0 (
    echo ERROR: Fallo en compilacion.
    popd
    pause
    exit /b 1
)

set "RUN_PATH="
set "RUN_MODE=GUI"

if exist "Debug\%EXEC_GUI%" (
    set "RUN_PATH=Debug\%EXEC_GUI%"
) else if exist "Release\%EXEC_GUI%" (
    set "RUN_PATH=Release\%EXEC_GUI%"
) else if exist "RelWithDebInfo\%EXEC_GUI%" (
    set "RUN_PATH=RelWithDebInfo\%EXEC_GUI%"
) else if exist "MinSizeRel\%EXEC_GUI%" (
    set "RUN_PATH=MinSizeRel\%EXEC_GUI%"
) else if exist "%EXEC_GUI%" (
    set "RUN_PATH=%EXEC_GUI%"
) else if exist "Debug\%EXEC_CLI%" (
    set "RUN_PATH=Debug\%EXEC_CLI%"
    set "RUN_MODE=CLI"
) else if exist "Release\%EXEC_CLI%" (
    set "RUN_PATH=Release\%EXEC_CLI%"
    set "RUN_MODE=CLI"
) else if exist "RelWithDebInfo\%EXEC_CLI%" (
    set "RUN_PATH=RelWithDebInfo\%EXEC_CLI%"
    set "RUN_MODE=CLI"
) else if exist "MinSizeRel\%EXEC_CLI%" (
    set "RUN_PATH=MinSizeRel\%EXEC_CLI%"
    set "RUN_MODE=CLI"
) else if exist "%EXEC_CLI%" (
    set "RUN_PATH=%EXEC_CLI%"
    set "RUN_MODE=CLI"
) else (
    echo ERROR: No se encontro ni %EXEC_GUI% ni %EXEC_CLI%.
    popd
    pause
    exit /b 1
)

echo.
if "%RUN_MODE%"=="GUI" (
    echo Ejecutando interfaz 2D: %RUN_PATH%
    "%RUN_PATH%"
) else (
    echo Ejecutando CLI: %RUN_PATH% %SIM_ARG%
    "%RUN_PATH%" "%SIM_ARG%"
)

set "APP_EXIT=%ERRORLEVEL%"
popd

echo.
echo Proceso finalizado con codigo %APP_EXIT%.
pause
exit /b %APP_EXIT%

endlocal
