@echo off
setlocal
rem Genera Pidufi-Portable-<version>.zip: carpeta Pidufi lista para copiar y ejecutar (sin instalacion).
call "%~dp0msys.bat"
if errorlevel 1 exit /b 1
call "%~dp0version.bat"
set OUT=%~dp0dist\Pidufi

call "%~dp0build.bat"
if errorlevel 1 exit /b 1

if exist "%~dp0dist" rmdir /s /q "%~dp0dist"
mkdir "%OUT%"
copy /y "%~dp0pidufi.exe" "%OUT%" >nul
copy /y "%MSYS%\bin\libopenjp2-7.dll" "%OUT%" >nul
copy /y "%MSYS%\bin\libwinpthread-1.dll" "%OUT%" >nul
copy /y "%MSYS%\bin\libgcc_s_seh-1.dll" "%OUT%" >nul
copy /y "%~dp0LICENSE" "%OUT%" >nul

powershell -NoProfile -Command "Compress-Archive -Path '%OUT%' -DestinationPath '%~dp0Pidufi-Portable-%VERSION%.zip' -Force"
if errorlevel 1 (
    echo Error al crear el zip.
    exit /b 1
)
echo Listo: Pidufi-Portable-%VERSION%.zip
