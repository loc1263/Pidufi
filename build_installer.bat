@echo off
setlocal
rem Compila Pidufi y genera Pidufi-Setup-<version>.exe (requiere NSIS: pacman -S mingw-w64-ucrt-x86_64-nsis)
call "%~dp0msys.bat"
if errorlevel 1 exit /b 1

call "%~dp0version.bat"
call "%~dp0build.bat"
if errorlevel 1 exit /b 1

"%MSYS%\bin\makensis.exe" /V2 /DVERSION=%VERSION% "%~dp0installer\pidufi.nsi"
if errorlevel 1 (
    echo Error al crear el instalador.
    exit /b 1
)
echo Listo: Pidufi-Setup-%VERSION%.exe
