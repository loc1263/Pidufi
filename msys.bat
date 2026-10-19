@echo off
rem Localiza MSYS2 (entorno UCRT64) y deja su ruta en la variable MSYS.
rem Orden de busqueda: 1) la variable MSYS, si ya esta definida; 2) C:\msys64 ... F:\msys64;
rem 3) el g++ que haya en el PATH. Para usar otra ruta: set MSYS=X:\ruta\msys64\ucrt64
if defined MSYS if exist "%MSYS%\bin\g++.exe" exit /b 0
set MSYS=
for %%D in (C D E F) do if not defined MSYS if exist "%%D:\msys64\ucrt64\bin\g++.exe" set "MSYS=%%D:\msys64\ucrt64"
if not defined MSYS for /f "delims=" %%G in ('where g++ 2^>nul') do if not defined MSYS for %%I in ("%%~dpG..") do if exist "%%~fI\include\mupdf\fitz.h" set "MSYS=%%~fI"
if not defined MSYS (
    echo No se encontro MSYS2 UCRT64 con MuPDF.
    echo Instala MSYS2 y ejecuta: pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libmupdf
    echo o define la ruta a mano: set MSYS=X:\ruta\msys64\ucrt64
    exit /b 1
)
exit /b 0
