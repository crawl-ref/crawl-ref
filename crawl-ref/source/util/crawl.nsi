# please run with -DVERSION="xxx"
# !define VERSION "0.7.0"
!define DCSS "Dungeon Crawl Stone Soup"
SetCompressor /SOLID lzma

!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY "Software\Crawl"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME ""
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "Software\Crawl"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME ""
!define MULTIUSER_INSTALLMODE_INSTDIR "Crawl"
!include "MultiUser.nsh"
!include "MUI2.nsh"

Name "${DCSS} ${VERSION}"
Outfile "crawl-${VERSION}.setup.exe"
InstallDir "$PROGRAMFILES\Crawl"
InstallDirRegKey HKCU "Software\Crawl" "InstallDir"
RequestExecutionLevel user
XPStyle on
!define MUI_ICON util\crawl.ico

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section ""
  SetOutPath $INSTDIR
  File /r /x "*~" "crawl-win\*"
  #%NSIS_INSTALL_FILES

  WriteUninstaller $INSTDIR\uninst.exe

  WriteRegStr SHCTX "Software\Crawl" "" $INSTDIR

  CreateDirectory "$SMPROGRAMS\${DCSS}"
  CreateShortCut "$SMPROGRAMS\${DCSS}\Dungeon Crawl - console.lnk" "$INSTDIR\crawl.exe"
  CreateShortCut "$SMPROGRAMS\${DCSS}\Dungeon Crawl - tiles.lnk" "$INSTDIR\crawl-tiles.exe"
  CreateShortCut "$SMPROGRAMS\${DCSS}\Uninstall DCSS.lnk" "$INSTDIR\uninst.exe"

  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "DisplayName" "${DCSS}"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "DisplayVersion" "${VERSION}"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "DisplayIcon" "$INSTDIR\crawl.exe"
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "NoModify" 1
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "NoRepair" 1
SectionEnd

Function .onInit
  !insertmacro MULTIUSER_INIT
FunctionEnd

Section "Uninstall"
  Delete $INSTDIR\uninst.exe
  RMDir /r $INSTDIR

  Delete "$SMPROGRAMS\${DCSS}\Uninstall DCSS.lnk"
  Delete "$SMPROGRAMS\${DCSS}\Dungeon Crawl - console.lnk"
  Delete "$SMPROGRAMS\${DCSS}\Dungeon Crawl - tiles.lnk"
  RMDir "$SMPROGRAMS\${DCSS}"

  DeleteRegKey SHCTX "Software\Crawl"
  DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl"
SectionEnd

Function un.onInit
  !insertmacro MULTIUSER_UNINIT
FunctionEnd
