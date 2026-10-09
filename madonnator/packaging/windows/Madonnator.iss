; Inno-Setup-Skript für MADONNATOR (VST3 + Standalone, 64 Bit)
; Aufruf: iscc /DAppVersion=0.1.0 /DBuildDir=<...>\build\Madonnator_artefacts\Release /O<ausgabe> Madonnator.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\Madonnator_artefacts\Release"
#endif

[Setup]
AppId={{B836CC5F-9D58-4A50-A43D-91A459FA7D74}
AppName=MADONNATOR
AppVersion={#AppVersion}
AppVerName=MADONNATOR {#AppVersion}
AppPublisher=Andreas Engel, Darmstadt, Deutschland
DefaultDirName={autopf}\MADONNATOR
DefaultGroupName=MADONNATOR
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=Madonnator-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=MADONNATOR
UninstallDisplayIcon={app}\MADONNATOR.exe

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Alles (VST3-Instrument und Standalone-Programm)"
Name: "custom"; Description: "Benutzerdefiniert"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3-Instrument (Cubase, Ableton Live, Bitwig, Reaper …)"; Types: full custom
Name: "standalone"; Description: "Standalone-Programm (ohne DAW)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Components: standalone; Flags: unchecked

[Files]
Source: "{#BuildDir}\VST3\MADONNATOR.vst3\*"; DestDir: "{commoncf64}\VST3\MADONNATOR.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\MADONNATOR.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\MADONNATOR"; Filename: "{app}\MADONNATOR.exe"; Components: standalone
Name: "{autodesktop}\MADONNATOR";  Filename: "{app}\MADONNATOR.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\MADONNATOR.exe"; Description: "{cm:LaunchProgram,MADONNATOR}"; Flags: nowait postinstall skipifsilent; Components: standalone
