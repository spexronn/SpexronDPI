[Setup]
AppName=SpexronDPI
AppVersion=1.0.0
DefaultDirName={autopf}\SpexronDPI
DefaultGroupName=SpexronDPI
UninstallDisplayIcon={app}\SpexronDPI.exe
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
OutputBaseFilename=SpexronDPI-Setup
AllowNoIcons=yes
DisableDirPage=no
DisableProgramGroupPage=no
UsePreviousAppDir=no
UsePreviousGroup=no
DirExistsWarning=no
ShowLanguageDialog=auto

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "tr"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[CustomMessages]
en.AlreadyInstalled=SpexronDPI is already installed on your system! Please uninstall the existing version first.
tr.AlreadyInstalled=SpexronDPI sisteminizde zaten kurulu! Lütfen yeni sürümü kurmadan önce mevcut olanı Denetim Masası'ndan veya klasöründen kaldırın.
de.AlreadyInstalled=SpexronDPI ist bereits auf Ihrem System installiert! Bitte deinstallieren Sie zuerst die vorhandene Version.
es.AlreadyInstalled=¡SpexronDPI ya está instalado en su sistema! Desinstale la versión existente primero.
ru.AlreadyInstalled=SpexronDPI уже установлен в вашей системе! Пожалуйста, сначала удалите существующую версию.

[Messages]
tr.PreviousInstallNotCompleted=Sisteminizde (başka bir uygulamadan veya Windows'tan kaynaklı) bekleyen bir yeniden başlatma işlemi var.
tr.PrepareToInstallNeedsRestart=SpexronDPI çekirdek ağ kancalarının (Network Hooks) temiz bir şekilde kurulabilmesi için bilgisayarınızı yeniden başlatmanız şiddetle tavsiye edilir.%n%nBilgisayarı şimdi yeniden başlatmak ister misiniz?

en.PreviousInstallNotCompleted=There is a pending restart on your system (from another application or Windows).
en.PrepareToInstallNeedsRestart=It is highly recommended to restart your computer so that SpexronDPI network hooks can be installed cleanly.%n%nDo you want to restart the computer now?

de.PreviousInstallNotCompleted=Auf Ihrem System steht ein Neustart aus (von einer anderen Anwendung oder von Windows).
de.PrepareToInstallNeedsRestart=Es wird dringend empfohlen, den Computer neu zu starten, damit die SpexronDPI-Netzwerk-Hooks sauber installiert werden können.%n%nMöchten Sie den Computer jetzt neu starten?

es.PreviousInstallNotCompleted=Hay un reinicio pendiente en su sistema (de otra aplicación o de Windows).
es.PrepareToInstallNeedsRestart=Se recomienda encarecidamente reiniciar su computadora para que los hooks de red de SpexronDPI se puedan instalar limpiamente.%n%n¿Desea reiniciar la computadora ahora?

ru.PreviousInstallNotCompleted=В вашей системе ожидается перезагрузка (от другого приложения или Windows).
ru.PrepareToInstallNeedsRestart=Настоятельно рекомендуется перезагрузить компьютер, чтобы сетевые хуки SpexronDPI могли быть чисто установлены.%n%nХотите ли вы перезагрузить компьютер сейчас?

[Code]
const
  WM_CLOSE = $0010;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  hWnd: HWND;
begin
  if CurUninstallStep = usUninstall then
  begin
    hWnd := FindWindowByWindowName('SpexronDPI');
    if hWnd <> 0 then
    begin
      SendMessage(hWnd, WM_CLOSE, 0, 0);
      Sleep(1000);
    end;
  end;
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if FileExists(ExpandConstant('{pf}\SpexronDPI\SpexronDPI.exe')) then
  begin
    MsgBox(CustomMessage('AlreadyInstalled'), mbError, MB_OK);
    Result := False;
  end;
end;

[Registry]
Root: HKCU; Subkey: "SOFTWARE\SpexronDPI"; ValueType: string; ValueName: "Language"; ValueData: "{language}"; Flags: uninsdeletevalue

[Files]
Source: "..\build\bin\Release\SpexronDPI.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\third_party\windivert\WinDivert.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\third_party\windivert\WinDivert64.sys"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{app}\uninstaller"; Filename: "{uninstallexe}"

[Run]
Filename: "schtasks"; Parameters: "/create /tn SpexronDPI_AutoStart /tr ""\""{app}\SpexronDPI.exe\"" --startup"" /sc onlogon /rl highest /f"; Flags: runhidden
Filename: "schtasks"; Parameters: "/run /tn SpexronDPI_AutoStart"; Flags: runhidden

[UninstallRun]
Filename: "schtasks"; Parameters: "/end /tn SpexronDPI_AutoStart"; Flags: runhidden
Filename: "schtasks"; Parameters: "/delete /tn SpexronDPI_AutoStart /f"; Flags: runhidden
Filename: "taskkill"; Parameters: "/F /IM SpexronDPI.exe /T"; Flags: runhidden
Filename: "sc"; Parameters: "delete WinDivert"; Flags: runhidden

[UninstallDelete]
Type: files; Name: "{app}\SpexronDPI_Engine.log"
Type: files; Name: "{app}\SpexronDPI_Config.json"
Type: dirifempty; Name: "{app}"
