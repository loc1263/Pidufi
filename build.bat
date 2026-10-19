@echo off
setlocal
rem Compila Pidufi con MinGW (MSYS2 UCRT64) + MuPDF. msys.bat busca MSYS2 (ver ese archivo si esta en otra ruta).
call "%~dp0msys.bat"
if errorlevel 1 exit /b 1
set PATH=%MSYS%\bin;%PATH%

set CXXFLAGS=-std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN -I%MSYS%\include
set LDFLAGS=-municode -mwindows -s -Wl,--gc-sections -static-libgcc -static-libstdc++ -L%MSYS%\lib
set LIBS=-Wl,-Bstatic -lmupdf -lgumbo -ljbig2dec -ljpeg -lfreetype -lharfbuzz -lpng16 -lbz2 -lbrotlienc -lbrotlidec -lbrotlicommon -lgraphite2 -lz -lglib-2.0 -lintl -lpcre2-8 -Wl,-Bdynamic -lopenjp2 -lcomdlg32 -lcomctl32 -lshell32 -lgdi32 -luser32 -lusp10 -lrpcrt4 -ldwrite -lws2_32 -lole32 -lwinmm -lshlwapi -luuid

windres res\pidufi.rc -O coff -o res\pidufi.o
if errorlevel 1 (
    echo Error al compilar los recursos.
    exit /b 1
)

g++ %CXXFLAGS% -ffunction-sections -fdata-sections src\*.cpp res\pidufi.o -o pidufi.exe %LDFLAGS% %LIBS%
if errorlevel 1 (
    echo Error al compilar.
    exit /b 1
)

copy /y "%MSYS%\bin\libopenjp2-7.dll" . >nul
echo Listo: pidufi.exe
