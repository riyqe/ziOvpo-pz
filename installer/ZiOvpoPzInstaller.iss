; ZiOvpo PZ Antivirus installer (Inno Setup 6)
;
; Компиляция из каталога репозитория:
;   перед ISCC скачайте MSVC Redistributable (x64) в third_party/vc_redist.x64.exe
;   см. официально: https://aka.ms/vs/17/release/vc_redist.x64.exe

#define MyAppDisplayName "ZiOvpo PZ Antivirus"
#define MyAppPublisher "ZiOvpo PZ"
#ifndef MyVersion
#define MyVersion "1.0.0"
#endif
#ifndef SrcDir
#define SrcDir "..\\out"
#endif
#define ServiceNameStr "ZiOvpoPzService"
#define ServiceDispStr "ZiOvpo PZ Service"

[Setup]
AppId={{91C53E70-DE9C-4876-9356-013A52D4B7FA}
AppName={#MyAppDisplayName}
AppVersion={#MyVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\ZiOvpoPz
DefaultGroupName={#MyAppDisplayName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=ZiOvpoPz-Setup_{#MyVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.17763
UninstallDisplayIcon={app}\TrayApp.exe
Compression=lzma2
SolidCompression=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SrcDir}\TrayApp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\ZiOvpoPzService.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\data\default.avdb"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "{#SrcDir}\data\public_cert.pem"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "third_party\vc_redist.x64.exe"; Flags: dontcopy

[Icons]
Name: "{group}\{#MyAppDisplayName}"; Filename: "{app}\TrayApp.exe"

[Code]

function VCRuntimeDetected: Boolean;
begin
  Result := RegKeyExists(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64');
end;

procedure StopAndRemoveService;
var RC: Integer;
begin
  Exec(ExpandConstant('{sysnative}\sc.exe'), ExpandConstant('stop {#ServiceNameStr}'), '', SW_HIDE, ewWaitUntilTerminated, RC);
  Exec(ExpandConstant('{sysnative}\sc.exe'), ExpandConstant('delete {#ServiceNameStr}'), '', SW_HIDE, ewWaitUntilTerminated, RC);
end;

procedure InstallVcRedistributableSilent;
var
  TmpExe: string;
  RC: Integer;
begin
  ExtractTemporaryFile('vc_redist.x64.exe');
  TmpExe := ExpandConstant('{tmp}\vc_redist.x64.exe');

  if VCRuntimeDetected then
    Exit;

  if Exec(TmpExe, '/install /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, RC) then
  begin
    if (RC = 0) or (RC = 3010) then
      RegWriteDWordValue(HKLM64, 'SOFTWARE\ZiOvpoPzInstaller', 'ShouldRemoveVcRedist', 1);
  end;
end;

procedure InstallWindowsService;
var
  RC: Integer;
  CreateArgs: string;
  DispArgs: string;
begin
  StopAndRemoveService;

  CreateArgs :=
    ExpandConstant('create {#ServiceNameStr} ') +
    'binPath= "' + ExpandConstant('{app}\ZiOvpoPzService.exe') + '" start= auto';

  Exec(ExpandConstant('{sysnative}\sc.exe'), CreateArgs, '', SW_HIDE, ewWaitUntilTerminated, RC);

  DispArgs := ExpandConstant('config {#ServiceNameStr} DisplayName= "{#ServiceDispStr}"');
  Exec(ExpandConstant('{sysnative}\sc.exe'), DispArgs, '', SW_HIDE, ewWaitUntilTerminated, RC);

  Exec(
    ExpandConstant('{sysnative}\sc.exe'),
    ExpandConstant('description {#ServiceNameStr} "ZiOvpo PZ background antivirus service."'),
    '',
    SW_HIDE,
    ewWaitUntilTerminated,
    RC
  );

  Exec(ExpandConstant('{sysnative}\sc.exe'), ExpandConstant('start {#ServiceNameStr}'), '', SW_HIDE, ewWaitUntilTerminated, RC);
end;

procedure MaybeUninstallVcRedist;
var
  TmpExe: string;
  RC: Integer;
  Flag: Cardinal;
begin
  if not RegQueryDWordValue(HKLM64, 'SOFTWARE\ZiOvpoPzInstaller', 'ShouldRemoveVcRedist', Flag) then
    Exit;
  if Flag <> 1 then
    Exit;

  ExtractTemporaryFile('vc_redist.x64.exe');
  TmpExe := ExpandConstant('{tmp}\vc_redist.x64.exe');
  if TmpExe <> '' then
    Exec(TmpExe, '/uninstall /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, RC);

  RegDeleteValue(HKLM64, 'SOFTWARE\ZiOvpoPzInstaller', 'ShouldRemoveVcRedist');
end;

procedure CleanupInstallerRegistryKeys;
begin
  if RegKeyExists(HKLM64, 'SOFTWARE\ZiOvpoPzInstaller') then
    RegDeleteKeyIncludingSubkeys(HKLM64, 'SOFTWARE\ZiOvpoPzInstaller');
end;

procedure CurInstallStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    InstallVcRedistributableSilent;
    InstallWindowsService;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    StopAndRemoveService;
    MaybeUninstallVcRedist;
    CleanupInstallerRegistryKeys;
  end;
end;
