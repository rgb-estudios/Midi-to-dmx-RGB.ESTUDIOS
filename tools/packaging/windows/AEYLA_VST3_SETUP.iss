#ifndef SourceVST3
  #error SourceVST3 must point to the staged AeylaVisualDmx.vst3 bundle
#endif

#ifndef ReadmePath
  #error ReadmePath must point to README_INSTALLERS_ES.txt
#endif

#ifndef ManifestPath
  #error ManifestPath must point to BUILD_MANIFEST.json
#endif

#ifndef OutputDir
  #error OutputDir must point to the installer output directory
#endif

#ifndef AppVersion
  #define AppVersion "0.3.3-alpha"
#endif

#ifndef AppVersionNumeric
  #define AppVersionNumeric "0.3.3.0"
#endif

#ifndef BuildSha
  #define BuildSha "unknown"
#endif

[Setup]
AppId={{B96B62A5-C7E8-4B7D-A2D4-691508C169F7}
AppName=AEYLA Visual DMX
AppVersion={#AppVersion}
AppVerName=AEYLA Visual DMX {#AppVersion}
AppPublisher=RGB Estudios
AppPublisherURL=https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS
AppSupportURL=https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS/issues
AppUpdatesURL=https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS/releases
DefaultDirName={autopf}\RGB Estudios\AEYLA Visual DMX
DefaultGroupName=AEYLA Visual DMX
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=AEYLA-{#AppVersion}-Windows-x64-Setup-UNSIGNED
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
Uninstallable=yes
UninstallDisplayName=AEYLA Visual DMX {#AppVersion} VST3
VersionInfoVersion={#AppVersionNumeric}
VersionInfoCompany=RGB Estudios
VersionInfoDescription=AEYLA Visual DMX VST3 installer
VersionInfoProductName=AEYLA Visual DMX
VersionInfoProductVersion={#AppVersionNumeric}
VersionInfoProductTextVersion={#AppVersion} ({#BuildSha})
VersionInfoTextVersion={#AppVersion} ({#BuildSha})

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Files]
Source: "{#SourceVST3}\*"; DestDir: "{commoncf64}\VST3\AeylaVisualDmx.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ReadmePath}"; DestDir: "{app}"; DestName: "LEEME_AEYLA.txt"; Flags: ignoreversion
Source: "{#ManifestPath}"; DestDir: "{app}"; DestName: "BUILD_MANIFEST.json"; Flags: ignoreversion

[Icons]
Name: "{group}\Leer antes de usar AEYLA"; Filename: "{app}\LEEME_AEYLA.txt"
Name: "{group}\Desinstalar AEYLA Visual DMX"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\AeylaVisualDmx.vst3"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
  if SuppressibleMsgBox(
    'Este instalador contiene una version ALPHA sin firma digital. ' +
    'Cierra REAPER y Ableton antes de continuar. La salida DMX comienza ' +
    'desarmada y este build no esta autorizado para depender de el en show.',
    mbInformation,
    MB_OKCANCEL,
    IDOK
  ) = IDCANCEL then
    Result := False;
end;
