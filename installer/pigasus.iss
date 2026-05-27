; Pigasus — Inno Setup script.
;
; Compiles a Pigasus-Setup.exe that installs the VST3 plugin into the system
; VST3 folder (C:\Program Files\Common Files\VST3\). Standard double-click
; install, and the uninstaller cleans the bundle back out.

#define MyAppName       "Pigasus"
#define MyAppVersion    "0.1.0"
#define MyAppPublisher  "AnperAudio"
#define MyAppURL        "https://github.com/PerekhodovAnton/PIGASUS"
#define VST3Source      "..\build\Pigasus_artefacts\Release\VST3\Pigasus.vst3"

[Setup]
; Fixed AppId so upgrades replace earlier versions cleanly.
AppId={{8E4A2C1B-3F5D-4A7E-9C2B-1D6F0E8B5A3C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DefaultGroupName={#MyAppName}
OutputBaseFilename=Pigasus-{#MyAppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayName={#MyAppName} {#MyAppVersion}
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#VST3Source}\*"; DestDir: "{app}\Pigasus.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Pigasus.vst3"
