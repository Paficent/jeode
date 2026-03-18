!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WordFunc.nsh"

Name "jeode ${JEODE_VERSION}"
OutFile "jeode-installer.exe"
Unicode True
InstallDir ""
RequestExecutionLevel user
ShowInstDetails show

!define MUI_ICON ".\assets\jeode.ico"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE ValidateMSMDir
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

; All this to detect steam on multiple drives???
Function .onInit
    StrCpy $0 ""
    ReadRegStr $0 HKLM "SOFTWARE\WOW6432Node\Valve\Steam" "InstallPath"
    ${If} $0 == ""
        ReadRegStr $0 HKLM "SOFTWARE\Valve\Steam" "InstallPath"
    ${EndIf}
    ${If} $0 == ""
        ReadRegStr $0 HKCU "SOFTWARE\Valve\Steam" "InstallPath"
    ${EndIf}

    ${If} $0 != ""
        IfFileExists "$0\steamapps\common\My Singing Monsters\MySingingMonsters.exe" 0 +3
            StrCpy $INSTDIR "$0\steamapps\common\My Singing Monsters"
            Return

        StrCpy $1 "$0\steamapps\libraryfolders.vdf"
        IfFileExists "$1" 0 steam_not_found

        FileOpen $2 "$1" r
        ${If} $2 == ""
            Goto steam_not_found
        ${EndIf}

        vdf_loop:
            FileRead $2 $3
            ${If} $3 == ""
                FileClose $2
                Goto steam_not_found
            ${EndIf}

            StrCpy $4 $3
            ${WordFind} $4 '"path"' "E+1" $5
            IfErrors vdf_loop 0

            StrCpy $5 ""
            StrCpy $6 0
            StrCpy $7 0
            StrLen $8 $4
            StrCpy $9 0

            char_loop:
                ${If} $7 >= $8
                    Goto got_value
                ${EndIf}
                StrCpy $R0 $4 1 $7
                ${If} $R0 == '"'
                    IntOp $6 $6 + 1
                    ${If} $6 == 3
                        IntOp $9 $7 + 1
                    ${EndIf}
                    ${If} $6 == 4
                        IntOp $R1 $7 - $9
                        StrCpy $5 $4 $R1 $9
                        Goto got_value
                    ${EndIf}
                ${EndIf}
                IntOp $7 $7 + 1
                Goto char_loop

            got_value:
            ${If} $5 == ""
                Goto vdf_loop
            ${EndIf}

            ${WordReplace} $5 "\\" "\" "+*" $5

            ${If} $5 != $0
                IfFileExists "$5\steamapps\common\My Singing Monsters\MySingingMonsters.exe" 0 vdf_loop
                StrCpy $INSTDIR "$5\steamapps\common\My Singing Monsters"
                FileClose $2
                Return
            ${EndIf}
            Goto vdf_loop
    ${EndIf}

    steam_not_found:
    MessageBox MB_OK \
        "My Singing Monsters could not be found automatically.$\n$\n\
        Please browse to the folder containing MySingingMonsters.exe.$\n\
        (Usually: ...\steamapps\common\My Singing Monsters)"

    nsDialogs::SelectFolderDialog "Select your My Singing Monsters folder" "C:\"
    Pop $0
    ${If} $0 != "error"
    ${AndIf} $0 != ""
        StrCpy $INSTDIR $0
    ${EndIf}
FunctionEnd

Function ValidateMSMDir
    IfFileExists "$INSTDIR\MySingingMonsters.exe" good 0
        MessageBox MB_OK|MB_ICONSTOP \
            "MySingingMonsters.exe was not found in:$\n$INSTDIR$\n$\n\
            Please select the correct My Singing Monsters folder."
        Abort
    good:
FunctionEnd

Section "jeode" SecMain
    SectionIn RO
    SetOutPath "$INSTDIR"
    File "${BUILD_DIR}\winhttp.dll"
    SetOutPath "$INSTDIR\jeode"
    File "${BUILD_DIR}\jeode\libjeode.dll"
    SetOutPath "$INSTDIR"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\winhttp.dll"
    Delete "$INSTDIR\jeode\libjeode.dll"
    Delete "$INSTDIR\jeode\config.json"
    RMDir "$INSTDIR\jeode"
SectionEnd
