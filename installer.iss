; Inno Setup 6 — установщик Engine Design (wxWidgets GUI)
;
; Подготовка файлов (Release, из корня репозитория):
;   cmake --build build --config Release
;   cmake --install build --config Release --prefix build/install
;
; Сборка .exe установщика:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss
;   или: cmake --build build --config Release --target installer_bundle
;
; Переопределить каталог вывода (опционально):
;   ISCC /DOutputSubDir=dist\setup installer.iss

#ifndef OutputSubDir
  #define OutputSubDir "build\installer"
#endif

#define MyAppName      "Engine Design"
#define MyAppNameShort "EngineDesign"
#define MyAppVersion   "1.0.0"
#define MyAppPublisher "Engine Design"
#define MyAppExeName   "engine_gui.exe"
#define MyAppDirName   "Engine Design"
#define MyAppIconFile  "assets\icon\EngineDesign.ico"
#define MyAppUrl       "https://github.com/"

[Setup]
AppId={{D6A4E0A1-7C5C-4F5A-9E77-9F1A26D5B3A1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppUrl}
AppSupportURL={#MyAppUrl}
AppUpdatesURL={#MyAppUrl}
DefaultDirName={autopf64}\{#MyAppDirName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
DisableProgramGroupPage=no
OutputDir={#OutputSubDir}
OutputBaseFilename=EngineDesign_Setup_{#MyAppVersion}
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile={#MyAppIconFile}
WizardStyle=modern
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
CloseApplicationsFilter=engine_gui.exe
RestartApplications=no
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} — установщик
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
MinVersion=10.0
SetupLogging=yes
UsePreviousAppDir=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "build\install\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyAppIconFile}"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "{#MyAppName}"; IconFilename: "{app}\EngineDesign.ico"; IconIndex: 0
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "{#MyAppName}"; Tasks: desktopicon; IconFilename: "{app}\EngineDesign.ico"; IconIndex: 0
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

[Registry]
Root: HKCR; Subkey: ".edp"; ValueType: string; ValueName: ""; ValueData: "EngineDesign.Project"; Flags: uninsdeletevalue
Root: HKCR; Subkey: "EngineDesign.Project"; ValueType: string; ValueName: ""; ValueData: "Проект Engine Design (*.edp)"; Flags: uninsdeletekey
Root: HKCR; Subkey: "EngineDesign.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\EngineDesign.ico,0"
Root: HKCR; Subkey: "EngineDesign.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

Root: HKCR; Subkey: ".eds"; ValueType: string; ValueName: ""; ValueData: "EngineDesign.State"; Flags: uninsdeletevalue
Root: HKCR; Subkey: "EngineDesign.State"; ValueType: string; ValueName: ""; ValueData: "Данные проекта Engine Design (*.eds)"; Flags: uninsdeletekey
Root: HKCR; Subkey: "EngineDesign.State\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\EngineDesign.ico,0"
Root: HKCR; Subkey: "EngineDesign.State\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Запустить {#MyAppName}"; Flags: nowait postinstall skipifsilent
