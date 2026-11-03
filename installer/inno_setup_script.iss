; Script do Inno Setup para NativeSX2 (Central de Jogos PS2 Nativos)
; NewPortGame - Cristiano Mariano

#define MyAppName "NativeSX2"
#define MyAppVersion "1.0"
#define MyAppPublisher "NewPortGame"
#define MyAppExeName "NativeSX2.exe"

[Setup]
AppId={{E6F7A189-9A23-4C7D-8912-87B56C71D23E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\NativeSX2
DefaultGroupName=NativeSX2
AllowNoIcons=yes
OutputDir=E:\INSTALADOR_GOW
OutputBaseFilename=Instalador_NativeSX2_PC
SetupIconFile=PACOTE_PORTATIL_NATIVESX2\nativesx2.ico
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=auto

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; Arquivos do jogo e motor compilado
Source: "PACOTE_PORTATIL_NATIVESX2\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\nativesx2.ico"
Name: "{group}\Desinstalar {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\nativesx2.ico"; Tasks: desktopicon

[Registry]
; Associacao automatica de arquivos .ISO ao NativeSX2 no Windows
Root: HKCU; Subkey: "Software\Classes\.iso"; ValueType: string; ValueData: "NativeSX2.iso"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.iso\OpenWithProgids"; ValueType: none; ValueName: "NativeSX2.iso"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\NativeSX2.iso"; ValueType: string; ValueData: "Jogo de PS2 - NativeSX2 (by NewPortGame)"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\NativeSX2.iso\DefaultIcon"; ValueType: string; ValueData: "{app}\nativesx2_iso.ico"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\NativeSX2.iso\shell\open\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "NativeSX2 (by NewPortGame)"
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

; Forcar Alto Desempenho (Placa de Video Dedicada NVIDIA / AMD) no Windows
Root: HKCU; Subkey: "Software\Microsoft\DirectX\UserGpuPreferences"; ValueType: string; ValueName: "{app}\ps2EntryRunner.exe"; ValueData: "GpuPreference=2;"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\DirectX\UserGpuPreferences"; ValueType: string; ValueName: "{app}\{#MyAppExeName}"; ValueData: "GpuPreference=2;"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
