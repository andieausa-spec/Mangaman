; Inno-Setup-Skript für DEPECHE MACHINE (VST3 + Standalone, 64 Bit)
; Aufruf: iscc /DAppVersion=0.1.0 /DBuildDir=<...>\build\DepecheMachine_artefacts\Release /O<ausgabe> DepecheMachine.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\DepecheMachine_artefacts\Release"
#endif

[Setup]
AppId={{A3C7E1F4-58B2-4D9A-9E61-2F0B7C4D8E15}
AppName=DEPECHE MACHINE
AppVersion={#AppVersion}
AppVerName=DEPECHE MACHINE {#AppVersion}
AppPublisher=Andreas Engel, Darmstadt, Deutschland
DefaultDirName={autopf}\DEPECHE MACHINE
DefaultGroupName=DEPECHE MACHINE
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=DepecheMachine-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=DEPECHE MACHINE
UninstallDisplayIcon={app}\DEPECHE MACHINE.exe

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
Source: "{#BuildDir}\VST3\DEPECHE MACHINE.vst3\*"; DestDir: "{commoncf64}\VST3\DEPECHE MACHINE.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\DEPECHE MACHINE.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\DEPECHE MACHINE"; Filename: "{app}\DEPECHE MACHINE.exe"; Components: standalone
Name: "{autodesktop}\DEPECHE MACHINE";  Filename: "{app}\DEPECHE MACHINE.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\DEPECHE MACHINE.exe"; Description: "{cm:LaunchProgram,DEPECHE MACHINE}"; Flags: nowait postinstall skipifsilent; Components: standalone
