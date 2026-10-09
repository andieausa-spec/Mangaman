; Inno-Setup-Skript für JARRE MACHINE (VST3 + Standalone, 64 Bit)
; Aufruf: iscc /DAppVersion=0.1.0 /DBuildDir=<...>\build\JarreMachine_artefacts\Release /O<ausgabe> JarreMachine.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\JarreMachine_artefacts\Release"
#endif

[Setup]
AppId={{6F1A2D9C-3B7E-4C58-A0E4-9D2B7F1C5A83}
AppName=JARRE MACHINE
AppVersion={#AppVersion}
AppVerName=JARRE MACHINE {#AppVersion}
AppPublisher=Andreas Engel, Darmstadt, Deutschland
DefaultDirName={autopf}\JARRE MACHINE
DefaultGroupName=JARRE MACHINE
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=JarreMachine-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=JARRE MACHINE
UninstallDisplayIcon={app}\JARRE MACHINE.exe

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
Source: "{#BuildDir}\VST3\JARRE MACHINE.vst3\*"; DestDir: "{commoncf64}\VST3\JARRE MACHINE.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\JARRE MACHINE.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\JARRE MACHINE"; Filename: "{app}\JARRE MACHINE.exe"; Components: standalone
Name: "{autodesktop}\JARRE MACHINE";  Filename: "{app}\JARRE MACHINE.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\JARRE MACHINE.exe"; Description: "{cm:LaunchProgram,JARRE MACHINE}"; Flags: nowait postinstall skipifsilent; Components: standalone
