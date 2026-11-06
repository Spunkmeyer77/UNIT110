; Inno Setup script for the Windows installer.
; Build:  ISCC /DAppVersion=0.1.0 installer\windows\UNIT110.iss
; (paths are relative to this file; expects a Release build in ..\..\build)

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define Artefacts "..\..\build\UNIT110_artefacts\Release"

[Setup]
AppId={{7C2D5E91-3B44-4F0A-9E61-0D1A6B2F8C53}
AppName=UNIT 110
AppVersion={#AppVersion}
AppVerName=UNIT 110 {#AppVersion}
AppPublisher=Transistor Labs
DefaultDirName={autopf}\Transistor Labs\UNIT 110
DefaultGroupName=Transistor Labs
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=UNIT110-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=UNIT 110

[Types]
Name: "full";   Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live, Bitwig, Reaper, Cubase...)"; Types: full custom
Name: "app";  Description: "Standalone app";                                         Types: full custom

[Files]
Source: "{#Artefacts}\VST3\UNIT 110.vst3\*"; DestDir: "{commoncf64}\VST3\UNIT 110.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\UNIT 110.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\UNIT 110"; Filename: "{app}\UNIT 110.exe"; Components: app

[Messages]
FinishedLabel=UNIT 110 is installed.%n%nRescan plug-ins in your DAW (Ableton Live: Settings > Plug-Ins > Rescan).
