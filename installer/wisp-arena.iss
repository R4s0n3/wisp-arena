#ifndef SourceDir
  #define SourceDir "..\dist\wisp-arena-windows"
#endif

#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{F9D7F8D9-7942-42F9-A05E-8F8F7B74B782}
AppName=Wisp Arena
AppVersion={#AppVersion}
AppPublisher=Wisp Arena
DefaultDirName={localappdata}\Programs\Wisp Arena
DefaultGroupName=Wisp Arena
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=wisp-arena-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\wisp-arena.exe

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Wisp Arena"; Filename: "{app}\wisp-arena.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Wisp Arena"; Filename: "{app}\wisp-arena.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\wisp-arena.exe"; Description: "Launch Wisp Arena"; Flags: nowait postinstall skipifsilent
