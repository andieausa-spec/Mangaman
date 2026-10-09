; Inno-Setup-Skript für TRENDY ANDY (VST3 + Standalone, 64 Bit)
; Aufruf: iscc /DAppVersion=0.6.0 /DBuildDir=<...>\build\Mangaman_artefacts\Release /O<ausgabe> Mangaman.iss

#ifndef AppVersion
  #define AppVersion "0.6.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\Mangaman_artefacts\Release"
#endif

[Setup]
AppId={{62DA37F7-F9F3-4689-B0E1-AAC52A4742AE}
AppName=TRENDY ANDY
AppVersion={#AppVersion}
AppVerName=TRENDY ANDY {#AppVersion}
AppPublisher=Andreas Engel, Darmstadt, Deutschland
DefaultDirName={autopf}\TRENDY ANDY
DefaultGroupName=TRENDY ANDY
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=TrendyAndy-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=TRENDY ANDY
UninstallDisplayIcon={app}\TRENDY ANDY.exe

[InstallDelete]
; Vorgaenger "Mangaman" (gleiche Plugin-Kennung) entfernen
Type: filesandordirs; Name: "{commoncf64}\VST3\Mangaman.vst3"
Type: filesandordirs; Name: "{autopf}\Mangaman"
Type: files; Name: "{autoprograms}\Mangaman.lnk"
Type: files; Name: "{autodesktop}\Mangaman.lnk"

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Alles (VST3-Plugin und Standalone-Programm)"
Name: "custom"; Description: "Benutzerdefiniert"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3-Plugin (Cubase, Ableton Live, Bitwig, Reaper …)"; Types: full custom
Name: "standalone"; Description: "Standalone-Programm (ohne DAW)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Components: standalone; Flags: unchecked

[Files]
Source: "{#BuildDir}\VST3\TRENDY ANDY.vst3\*"; DestDir: "{commoncf64}\VST3\TRENDY ANDY.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\TRENDY ANDY.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\TRENDY ANDY"; Filename: "{app}\TRENDY ANDY.exe"; Components: standalone
Name: "{autodesktop}\TRENDY ANDY";  Filename: "{app}\TRENDY ANDY.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\TRENDY ANDY.exe"; Description: "{cm:LaunchProgram,TRENDY ANDY}"; Flags: nowait postinstall skipifsilent; Components: standalone
