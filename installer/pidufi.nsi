; Instalador de Pidufi (NSIS). Instala por usuario, no requiere administrador.
Unicode true
!define APP "Pidufi"
!ifndef VERSION
!define VERSION "1.1.0"  ; normalmente llega desde build_installer.bat (/DVERSION=...)
!endif
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP}"

!include "MUI2.nsh"

Name "${APP}"
OutFile "..\Pidufi-Setup-${VERSION}.exe"
InstallDir "$LOCALAPPDATA\Programs\${APP}"
InstallDirRegKey HKCU "Software\${APP}" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma

!define MUI_FINISHPAGE_RUN "$INSTDIR\pidufi.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Spanish"

Section "Instalar"
    SetOutPath "$INSTDIR"
    File "..\pidufi.exe"
    File "..\libopenjp2-7.dll"
    File "..\LICENSE"
    WriteUninstaller "$INSTDIR\Desinstalar.exe"

    CreateDirectory "$SMPROGRAMS\${APP}"
    CreateShortcut "$SMPROGRAMS\${APP}\${APP}.lnk" "$INSTDIR\pidufi.exe"
    CreateShortcut "$SMPROGRAMS\${APP}\Desinstalar ${APP}.lnk" "$INSTDIR\Desinstalar.exe"
    CreateShortcut "$DESKTOP\${APP}.lnk" "$INSTDIR\pidufi.exe"

    ; Aparece en "Abrir con" para los .pdf (no cambia la aplicacion predeterminada).
    WriteRegStr HKCU "Software\Classes\Applications\pidufi.exe\shell\open\command" "" '"$INSTDIR\pidufi.exe" "%1"'
    WriteRegStr HKCU "Software\Classes\Applications\pidufi.exe\SupportedTypes" ".pdf" ""
    WriteRegStr HKCU "Software\Classes\Applications\pidufi.exe" "FriendlyAppName" "${APP}"

    WriteRegStr HKCU "Software\${APP}" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APP}"
    WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "${APP}"
    WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\pidufi.exe"
    WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Desinstalar.exe"'
    WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\pidufi.exe"
    Delete "$INSTDIR\libopenjp2-7.dll"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\Desinstalar.exe"
    RMDir "$INSTDIR"

    Delete "$SMPROGRAMS\${APP}\${APP}.lnk"
    Delete "$SMPROGRAMS\${APP}\Desinstalar ${APP}.lnk"
    RMDir "$SMPROGRAMS\${APP}"
    Delete "$DESKTOP\${APP}.lnk"

    DeleteRegKey HKCU "Software\Classes\Applications\pidufi.exe"
    DeleteRegKey HKCU "Software\${APP}"
    DeleteRegKey HKCU "${UNINST_KEY}"
SectionEnd
