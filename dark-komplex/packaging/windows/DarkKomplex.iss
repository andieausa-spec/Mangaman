; Inno-Setup-Skript für DARK KOMPLEX (VST3 + Standalone, 64 Bit)
; Aufruf: iscc /DAppVersion=0.1.0 /DBuildDir=<...>\build\DarkKomplex_artefacts\Release /O<ausgabe> DarkKomplex.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\DarkKomplex_artefacts\Release"
#endif

[Setup]
AppId={{A3C7E1F4-58B2-4D9A-9E61-2F0B7C4D8E15}
AppName=DARK KOMPLEX
AppVersion={#AppVersion}
AppVerName=DARK KOMPLEX {#AppVersion}
AppPublisher=Andreas Engel, Darmstadt, Deutschland
DefaultDirName={autopf}\DARK KOMPLEX
DefaultGroupName=DARK KOMPLEX
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=DarkKomplex-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=DARK KOMPLEX
UninstallDisplayIcon={app}\DARK KOMPLEX.exe

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

[InstallDelete]
; Frühere Ausgabe unter dem Arbeitsnamen DEPECHE MACHINE entfernen
Type: filesandordirs; Name: "{commoncf64}\VST3\DEPECHE MACHINE.vst3"
Type: filesandordirs; Name: "{autopf}\DEPECHE MACHINE"
Type: files; Name: "{autoprograms}\DEPECHE MACHINE.lnk"
Type: files; Name: "{autodesktop}\DEPECHE MACHINE.lnk"

[Files]
Source: "{#BuildDir}\VST3\DARK KOMPLEX.vst3\*"; DestDir: "{commoncf64}\VST3\DARK KOMPLEX.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\DARK KOMPLEX.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\DARK KOMPLEX"; Filename: "{app}\DARK KOMPLEX.exe"; Components: standalone
Name: "{autodesktop}\DARK KOMPLEX";  Filename: "{app}\DARK KOMPLEX.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\DARK KOMPLEX.exe"; Description: "{cm:LaunchProgram,DARK KOMPLEX}"; Flags: nowait postinstall skipifsilent; Components: standalone
