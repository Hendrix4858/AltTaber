; AltTaber Inno Setup Script
; Requires Inno Setup 6+

#define MyAppName "AltTaber"
#define MyAppVersion "0.5.0"
#define MyAppPublisher "MrBeanCpp"
#define MyAppURL "https://github.com/Hendrix4858/AltTaber"
#define MyAppExeName "AltTaber.exe"
#define MyAppId "AltTaber.MrBeanCpp"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputBaseFilename={#MyAppName}-v{#MyAppVersion}-Setup
OutputDir=..\build\output
SetupIconFile=..\img\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64os
Compression=lzma2/ultra64
SolidCompression=yes
MinVersion=10.0.10240

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "zh"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[CustomMessages]
en.AppRunningMsg=AltTaber is running. Please close it first.
zh.AppRunningMsg=AltTaber 正在运行，请先关闭它。
en.AppStillRunning=Still running. Click Retry after closing, or Cancel to abort.
zh.AppStillRunning=仍在运行。关闭后请点"重试"，或点"取消"中止。
en.DeleteConfig=Delete configuration and settings?
zh.DeleteConfig=删除配置和设置？

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\build\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{app}\*"

[Code]
function IsAppRunning: Boolean;
begin
  Result := FindWindowByWindowName('AltTaber') <> 0;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  while IsAppRunning do begin
    if MsgBox(CustomMessage('AppRunningMsg') + #13#10 + CustomMessage('AppStillRunning'),
              mbConfirmation, MB_RETRYCANCEL) = IDCANCEL then begin
      Result := 'Setup was cancelled by the user.';
      Exit;
    end;
  end;
end;

function InitializeUninstall: Boolean;
begin
  Result := True;
  while IsAppRunning do begin
    if MsgBox(CustomMessage('AppRunningMsg') + #13#10 + CustomMessage('AppStillRunning'),
              mbConfirmation, MB_RETRYCANCEL) = IDCANCEL then begin
      Result := False;
      Exit;
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ConfigPath: String;
begin
  if CurUninstallStep = usPostUninstall then begin
    ConfigPath := ExpandConstant('{userappdata}\MrBeanCpp\AltTaber');
    if DirExists(ConfigPath) then begin
      if MsgBox(CustomMessage('DeleteConfig'), mbConfirmation, MB_YESNO) = IDYES then
        DelTree(ConfigPath, True, True, True);
    end;
  end;
end;
