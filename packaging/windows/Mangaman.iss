; Inno-Setup-Skript für Mangaman (VST3 + Standalone, 64 Bit)
; Aufruf: iscc /DAppVersion=0.1.0 /DBuildDir=<...>\build\Mangaman_artefacts\Release /O<ausgabe> Mangaman.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\Mangaman_artefacts\Release"
#endif

[Setup]
AppId={{62DA37F7-F9F3-4689-B0E1-AAC52A4742AE}
AppName=Mangaman
AppVersion={#AppVersion}
AppVerName=Mangaman {#AppVersion}
AppPublisher=Andi
DefaultDirName={autopf}\Mangaman
DefaultGroupName=Mangaman
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=Mangaman-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Mangaman
UninstallDisplayIcon={app}\Mangaman.exe

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
Source: "{#BuildDir}\VST3\Mangaman.vst3\*"; DestDir: "{commoncf64}\VST3\Mangaman.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\Mangaman.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\Mangaman"; Filename: "{app}\Mangaman.exe"; Components: standalone
Name: "{autodesktop}\Mangaman";  Filename: "{app}\Mangaman.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\Mangaman.exe"; Description: "{cm:LaunchProgram,Mangaman}"; Flags: nowait postinstall skipifsilent; Components: standalone
