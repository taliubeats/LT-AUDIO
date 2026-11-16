; Inno Setup script for one LT plugin (or all of them).
; ISCC.exe /DAppName="LT TYPE BEATS" /DSourceDir="pkg\VST3\LT TYPE BEATS.vst3" /DOutName="LT-TYPE-BEATS-Installer-windows" /DAppVersion=0.1.0 plugin.iss
; For all plugins: /DSourceDir="pkg\VST3" /DAll=1

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{LT-AUDIO-{#OutName}}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=LT Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
Uninstallable=yes
UninstallDisplayName={#AppName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\..\dist
OutputBaseFilename={#OutName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Messages]
WelcomeLabel2=This will install {#AppName} (VST3) on your computer.%n%nClose Ableton Live before continuing.

[Files]
#ifdef All
Source: "{#SourceDir}\*"; DestDir: "{commoncf64}\VST3"; Flags: ignoreversion recursesubdirs createallsubdirs
#else
Source: "{#SourceDir}\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
