; Encuadre Script installer (Windows x64, Qt6)
; Based on starc_64.nsi from Story Architect

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"
  !include "FileAssociation.nsh"

;--------------------------------
;General

  !define pkgdir "files_64"
  !define regkey "Software\Microsoft\Windows\CurrentVersion\Uninstall\EncuadreScript"

  ;Name and file
  Name "Encuadre Script"
  Caption "Installing Encuadre Script"
  OutFile "encuadre-script-setup.exe"
  BrandingText "Grupo Encuadre Chile"

  ;Default installation folder
  InstallDir "$PROGRAMFILES64\Encuadre Script"
  InstallDirRegKey HKLM "${regkey}" "InstallLocation"

  RequestExecutionLevel admin

;--------------------------------
;Interface Settings

  Function createShortcutsFunction
  CreateDirectory "$SMPROGRAMS\Encuadre Script"
  CreateShortCut "$SMPROGRAMS\Encuadre Script\Encuadre Script.lnk" "$INSTDIR\starcapp.exe"
  CreateShortCut "$SMPROGRAMS\Encuadre Script\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortcut "$DESKTOP\Encuadre Script.lnk" "$INSTDIR\starcapp.exe" "" "$INSTDIR\starcapp.exe" 0
  FunctionEnd

  !define MUI_ICON "..\..\src\app\icon.ico"
  !define MUI_UNICON "..\..\src\app\icon.ico"
  !define MUI_ABORTWARNING
  !define MUI_FINISHPAGE_RUN "$INSTDIR\starcapp.exe"
  !define MUI_FINISHPAGE_SHOWREADME ""
  !define MUI_FINISHPAGE_SHOWREADME_CHECKED
  !define MUI_FINISHPAGE_SHOWREADME_TEXT "Create Desktop and Start Menu Shortcuts"
  !define MUI_FINISHPAGE_SHOWREADME_FUNCTION createShortcutsFunction

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "Spanish"
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "App files section" SecFiles

  WriteRegStr HKLM "${regkey}" "DisplayName" "Encuadre Script"
  WriteRegStr HKLM "${regkey}" "Publisher" "Grupo Encuadre Chile"
  WriteRegStr HKLM "${regkey}" "DisplayVersion" "$%APP_VERSION%"
  WriteRegStr HKLM "${regkey}" "Version" "$%APP_VERSION%"
  WriteRegStr HKLM "${regkey}" "DisplayIcon" "$INSTDIR\starcapp.exe"
  WriteRegStr HKLM "${regkey}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${regkey}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${regkey}" "UrlInfoAbout" "https://prod.encuadre.muxp.art"

  SetOutPath "$INSTDIR"
  File /r "${pkgdir}\"

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ;Register project files extension
  ${registerExtension} "$INSTDIR\starcapp.exe" ".encdre" "Encuadre Script project"

  System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  DeleteRegKey HKLM "${regkey}"

  ;Remove shortcuts, if any
  Delete "$SMPROGRAMS\Encuadre Script\*.*"
  Delete "$DESKTOP\Encuadre Script.lnk"
  RMDir "$SMPROGRAMS\Encuadre Script"

  ;Remove application files
  RMDir /r "$INSTDIR"

  ${unregisterExtension} ".encdre" "Encuadre Script project"

SectionEnd

Function .onInit

  IfSilent 0 +5
  CreateDirectory "$SMPROGRAMS\Encuadre Script"
  CreateShortCut "$SMPROGRAMS\Encuadre Script\Encuadre Script.lnk" "$INSTDIR\starcapp.exe"
  CreateShortCut "$SMPROGRAMS\Encuadre Script\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortcut "$DESKTOP\Encuadre Script.lnk" "$INSTDIR\starcapp.exe" "" "$INSTDIR\starcapp.exe" 0

FunctionEnd
