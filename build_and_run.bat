@echo off
setlocal

echo ===============================
echo OmniSim Engine - Build ^& Run
echo ===============================

set "BUILD_DIR=build"
set "EXEC=omnisim_cli.exe"
set "SIM_ARG=%~1"

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

if exist "Debug\%EXEC%" (
    set "RUN_PATH=Debug\%EXEC%"
) else if exist "Release\%EXEC%" (
    set "RUN_PATH=Release\%EXEC%"
) else if exist "RelWithDebInfo\%EXEC%" (
    set "RUN_PATH=RelWithDebInfo\%EXEC%"
) else if exist "MinSizeRel\%EXEC%" (
    set "RUN_PATH=MinSizeRel\%EXEC%"
) else if exist "%EXEC%" (
    set "RUN_PATH=%EXEC%"
) else (
    echo ERROR: No se encontro el ejecutable %EXEC%.
    popd
    pause
    exit /b 1
)

echo.
echo Ejecutando %RUN_PATH%...
if "%SIM_ARG%"=="" (
    "%RUN_PATH%"
) else (
    "%RUN_PATH%" "%SIM_ARG%"
)

set "APP_EXIT=%ERRORLEVEL%"
popd

echo.
echo Proceso finalizado con codigo %APP_EXIT%.
pause
exit /b %APP_EXIT%

endlocal
