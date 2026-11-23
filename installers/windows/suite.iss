; LT AUDIO - one Windows installer for every plugin, with a checkbox per plugin.
; build_suite_installer.ps1 writes suite_components.iss (one [Components]/[Files]/[InstallDelete] entry per
; plugin found in pkg\VST3) and then runs:  ISCC.exe /DAppVersion=0.1.0 suite.iss
;
; - Full / custom install: tick only the plugins you want.
; - Running it again remembers what you picked; unticking an installed plugin removes it.
; - Windows "Apps" -> LT AUDIO Plugins -> Uninstall removes everything it installed.

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6E2B7C1A-4F3D-4B8E-9C51-1A7D2E0F4C11}
AppName=LT AUDIO Plugins
AppVersion={#AppVersion}
AppVerName=LT AUDIO Plugins {#AppVersion}
AppPublisher=LT Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousSetupType=yes
UsePreviousTasks=yes
Uninstallable=yes
UninstallDisplayName=LT AUDIO Plugins
UninstallFilesDir={commonpf64}\LT Audio\Uninstall
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\..\dist
OutputBaseFilename=LT-AUDIO-Plugins-Setup-windows
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
es.TypeFull=Todos los plugins
es.TypeCustom=Elegir plugins
es.Welcome=Este asistente instala los plugins LT AUDIO (VST3) en C:\Program Files\Common Files\VST3.%n%nEn el siguiente paso eliges qué plugins quieres. Si ya tenías alguno instalado y lo desmarcas, se desinstala.%n%nCierra Ableton (o tu DAW) antes de continuar.
en.TypeFull=All plugins
en.TypeCustom=Choose plugins
en.Welcome=This wizard installs the LT AUDIO plugins (VST3) in C:\Program Files\Common Files\VST3.%n%nOn the next page pick the plugins you want. Unticking a plugin you already had removes it.%n%nClose Ableton (or your DAW) before continuing.

[Messages]
es.WelcomeLabel2={cm:Welcome}
en.WelcomeLabel2={cm:Welcome}

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

#include "suite_components.iss"

[Code]
{ True when this suite was installed before: only then do unticked plugins get removed. }
function IsUpgrade(): Boolean;
var
  Key: String;
begin
  Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{6E2B7C1A-4F3D-4B8E-9C51-1A7D2E0F4C11}_is1';
  Result := RegKeyExists(HKLM, Key) or RegKeyExists(HKCU, Key);
end;
