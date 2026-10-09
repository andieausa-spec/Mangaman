; Inno-Setup-Skript für TRENDY DELAY (VST3 + Standalone, 64 Bit)
; Aufruf: iscc /DAppVersion=0.1.0 /DBuildDir=<...>\build\TrendyDelay_artefacts\Release /O<ausgabe> TrendyDelay.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\TrendyDelay_artefacts\Release"
#endif

[Setup]
AppId={{B3E5C1A4-7D2F-4E8B-9A61-5C0F2D7E4B19}
AppName=TRENDY DELAY
AppVersion={#AppVersion}
AppVerName=TRENDY DELAY {#AppVersion}
AppPublisher=Andreas Engel, Darmstadt, Deutschland
DefaultDirName={autopf}\TRENDY DELAY
DefaultGroupName=TRENDY DELAY
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=TrendyDelay-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=TRENDY DELAY
UninstallDisplayIcon={app}\TRENDY DELAY.exe

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Alles (VST3-Effekt und Standalone-Programm)"
Name: "custom"; Description: "Benutzerdefiniert"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3-Effekt (Cubase, Ableton Live, Bitwig, Reaper …)"; Types: full custom
Name: "standalone"; Description: "Standalone-Programm (ohne DAW)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Components: standalone; Flags: unchecked

[Files]
Source: "{#BuildDir}\VST3\TRENDY DELAY.vst3\*"; DestDir: "{commoncf64}\VST3\TRENDY DELAY.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\TRENDY DELAY.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\TRENDY DELAY"; Filename: "{app}\TRENDY DELAY.exe"; Components: standalone
Name: "{autodesktop}\TRENDY DELAY";  Filename: "{app}\TRENDY DELAY.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\TRENDY DELAY.exe"; Description: "{cm:LaunchProgram,TRENDY DELAY}"; Flags: nowait postinstall skipifsilent; Components: standalone
