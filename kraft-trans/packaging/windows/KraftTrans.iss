; Inno-Setup-Skript für KRAFT TRANS (VST3 + Standalone, 64 Bit)
; Aufruf: iscc /DAppVersion=0.1.0 /DBuildDir=<...>\build\KraftTrans_artefacts\Release /O<ausgabe> KraftTrans.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\KraftTrans_artefacts\Release"
#endif

[Setup]
AppId={{87BA8931-2431-44AA-B566-0383C2785007}
AppName=KRAFT TRANS
AppVersion={#AppVersion}
AppVerName=KRAFT TRANS {#AppVersion}
AppPublisher=Andreas Engel, Darmstadt, Deutschland
DefaultDirName={autopf}\KRAFT TRANS
DefaultGroupName=KRAFT TRANS
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=KraftTrans-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=KRAFT TRANS
UninstallDisplayIcon={app}\KRAFT TRANS.exe

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
Source: "{#BuildDir}\VST3\KRAFT TRANS.vst3\*"; DestDir: "{commoncf64}\VST3\KRAFT TRANS.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\KRAFT TRANS.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\KRAFT TRANS"; Filename: "{app}\KRAFT TRANS.exe"; Components: standalone
Name: "{autodesktop}\KRAFT TRANS";  Filename: "{app}\KRAFT TRANS.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\KRAFT TRANS.exe"; Description: "{cm:LaunchProgram,KRAFT TRANS}"; Flags: nowait postinstall skipifsilent; Components: standalone
