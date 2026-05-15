; ZiOvpo PZ Antivirus installer (Inno Setup 6)
;
; Перед ISCC положите vc_redist.x64.exe в installer/third_party/
; https://aka.ms/vs/17/release/vc_redist.x64.exe

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
Uninstallable=yes
CreateUninstallRegKey=yes
UninstallDisplayName={#MyAppDisplayName}
UsePreviousAppDir=yes
OutputDir=..\dist
OutputBaseFilename=ZiOvpoPz-Setup_{#MyVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.17763
UninstallDisplayIcon={app}\TrayApp.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SrcDir}\TrayApp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\ZiOvpoPzService.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\data\default.avdb"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "{#SrcDir}\data\public_cert.pem"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "InstallService.ps1"; DestDir: "{app}\installer"; Flags: ignoreversion
Source: "UninstallService.ps1"; DestDir: "{app}\installer"; Flags: ignoreversion
Source: "third_party\vc_redist.x64.exe"; Flags: dontcopy

[Icons]
Name: "{group}\{#MyAppDisplayName}"; Filename: "{app}\TrayApp.exe"
Name: "{group}\{cm:UninstallProgram,{#MyAppDisplayName}}"; Filename: "{uninstallexe}"

[Code]

function NotSilentSetup: Boolean;
var
  T: string;
begin
  T := UpperCase(GetCmdTail);
  Result := (Pos('/VERYSILENT', T) = 0) and (Pos('/SILENT', T) = 0);
end;

function PowerShellExe: string;
begin
  Result := ExpandConstant('{sysnative}\WindowsPowerShell\v1.0\powershell.exe');
  if not FileExists(Result) then
    Result := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
end;

procedure InstallVcRedistributableSilent;
var
  TmpExe: string;
  RC: Integer;
  HadRuntime: Boolean;
begin
  HadRuntime :=
    RegKeyExists(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64');

  ExtractTemporaryFile('vc_redist.x64.exe');
  TmpExe := ExpandConstant('{tmp}\vc_redist.x64.exe');
    if not FileExists(TmpExe) then
    begin
      Log('vc_redist.x64.exe not found after extract');
      if NotSilentSetup then
        MsgBox('В пакете отсутствует vc_redist.x64.exe (ошибка сборки установщика).', mbError, MB_OK);
    Exit;
  end;

  if Exec(TmpExe, '/install /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, RC) then
  begin
    if (not HadRuntime) and ((RC = 0) or (RC = 3010)) then
      RegWriteDWordValue(HKLM64, 'SOFTWARE\ZiOvpoPzInstaller', 'ShouldRemoveVcRedist', 1);
    if (RC <> 0) and (RC <> 3010) and NotSilentSetup then
      MsgBox('Microsoft VC++ Redistributable вернул код ' + IntToStr(RC) + '. Приложение может не запуститься без рантайма.', mbInformation, MB_OK);
  end;
end;

procedure InstallWindowsService;
var
  RC: Integer;
  Script: string;
  Params: string;
  Ps: string;
begin
  Script := ExpandConstant('{app}\installer\InstallService.ps1');
    if not FileExists(Script) then
    begin
      if NotSilentSetup then
        MsgBox('Не найден InstallService.ps1 в ' + Script, mbError, MB_OK);
    Exit;
  end;

  Ps := PowerShellExe;
  Params :=
    '-NoProfile -ExecutionPolicy Bypass -File "' + Script + '" -ServiceAppDir "' +
    ExpandConstant('{app}') + '" -ServiceName "{#ServiceNameStr}" -DisplayName "{#ServiceDispStr}"';

  if Exec(Ps, Params, '', SW_HIDE, ewWaitUntilTerminated, RC) then
  begin
    if (RC <> 0) and NotSilentSetup then
      MsgBox(
        'Не удалось зарегистрировать или запустить службу (код ' + IntToStr(RC) + ').' + #13#10 +
        'Смотрите лог: ' + ExpandConstant('{commonappdata}\ZiOvpoPz\install-service.log'),
        mbError,
        MB_OK);
  end;
end;

procedure RunUninstallServiceScript;
var
  RC: Integer;
  Script: string;
  Params: string;
  Ps: string;
begin
  Script := ExpandConstant('{app}\installer\UninstallService.ps1');
  if not FileExists(Script) then
    Exit;
  Ps := PowerShellExe;
  Params := '-NoProfile -ExecutionPolicy Bypass -File "' + Script + '" -ServiceName "{#ServiceNameStr}"';
  Exec(Ps, Params, '', SW_HIDE, ewWaitUntilTerminated, RC);
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
    RunUninstallServiceScript;
    MaybeUninstallVcRedist;
    CleanupInstallerRegistryKeys;
  end;
end;
