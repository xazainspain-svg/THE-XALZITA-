; Instalador de Xaza FX (90 plugins VST3). Se compila en CI con: iscc installer.iss /DSrcDir=<carpeta con los .vst3>
#ifndef SrcDir
  #define SrcDir "vst3"
#endif

[Setup]
AppName=Xaza FX (90 plugins VST3)
AppVersion=1.0.0
AppPublisher=xaza
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
CreateUninstallRegKey=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=Xaza-FX-90-Instalador
OutputDir=.
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SrcDir}\*"; DestDir: "{commoncf64}\VST3"; Flags: recursesubdirs createallsubdirs ignoreversion
