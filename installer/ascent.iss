; ASCENT setup script (Inno Setup 6.5 or later). docs/install.md describes it.
;
; Build it with installer/build.sh: the script expects the files of
; "cmake --install" in the folder STAGE (default ..\build-rel\stage).
;
; The setup program asks for the game files (the disc, a disc image or an
; installed Descent to Undermountain), copies the program, runs
; ascent-import to make the data folder, downloads the 1.3 patch files from
; the content server when the source has no 1.3 patch, and downloads the
; upscaled content that the user chooses.
;
; Command line options (also for a silent install):
;   /SOURCE=path      the game files (folder, drive, .cue, .bin or .iso)
;   /DISC=path        the disc, when SOURCE is an installed game
;   /PATCH=path       a local 1.3 patch archive or folder (no download)
;   /CONTENTURL=url   another content server (tests)
;   and the standard options: /DIR=, /TASKS=, /SILENT, /VERYSILENT, ...

#ifndef STAGE
  #define STAGE "..\build-rel\stage"
#endif
#ifndef OUTDIR
  #define OUTDIR "..\build-rel\setup"
#endif
#ifndef APPVERSION
  #define APPVERSION "1.0"
#endif
#ifndef TEXTURES_MB
  #define TEXTURES_MB "520"
#endif
#ifndef MOVIES_MB
  #define MOVIES_MB "490"
#endif
; The content server: the one place is installer/content.ini.
#define BaseUrl ReadIni(SourcePath + "content.ini", "content", "base_url", "")

[Setup]
AppId={{6C2E9A57-3B1D-4E8F-9A40-5D1E0C7A2B13}
AppName=ASCENT
AppVersion={#APPVERSION}
AppVerName=ASCENT {#APPVERSION}
AppPublisher=ASCENT project
DefaultDirName={userpf}\ASCENT
DisableProgramGroupPage=yes
DefaultGroupName=ASCENT
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OUTDIR}
OutputBaseFilename=ascent-setup-{#APPVERSION}
SetupIconFile=res\ascent.ico
UninstallDisplayIcon={app}\ascent.exe
UninstallDisplayName=ASCENT
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; The data folder (about 420 MB) is made by ascent-import, not by [Files].
ExtraDiskSpaceRequired=440000000
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "startmenu"; Description: "Create a &Start menu entry"; GroupDescription: "Shortcuts:"
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked
Name: "hdtextures"; Description: "Upscaled textures and interface (download, about {#TEXTURES_MB} MB)"; GroupDescription: "Downloadable content (needs the GPU renderer, can be added later with ASCENT Content):"; Check: HasContentServer
Name: "hdmovies"; Description: "Upscaled movies (download, about {#MOVIES_MB} MB)"; GroupDescription: "Downloadable content (needs the GPU renderer, can be added later with ASCENT Content):"; Flags: unchecked; Check: HasContentServer

[Files]
Source: "{#STAGE}\ascent.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\ascent-import.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\ascent-content.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\content.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\LICENSE-*.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\ASCENT\ASCENT"; Filename: "{app}\ascent.exe"; WorkingDir: "{app}"; Tasks: startmenu
Name: "{autoprograms}\ASCENT\ASCENT Content"; Filename: "{app}\ascent-content.exe"; WorkingDir: "{app}"; Tasks: startmenu
Name: "{autodesktop}\ASCENT"; Filename: "{app}\ascent.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ascent.exe"; WorkingDir: "{app}"; Description: "Play ASCENT"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The files that ascent-import and ascent-content made
Type: filesandordirs; Name: "{app}\data"
Type: filesandordirs; Name: "{app}\hd"
Type: filesandordirs; Name: "{app}\content"
Type: dirifempty; Name: "{app}"

[Code]
var
  SourcePage: TWizardPage;
  SourceEdit, DiscEdit: TNewEdit;
  SourceInfo: TNewStaticText;
  SourcePatched, SourcesChecked: Boolean;
  ProgressPage: TOutputProgressWizardPage;
  ToolError: String;
  ToolStep: String;

function ContentUrl(): String;
begin
  Result := ExpandConstant('{param:CONTENTURL|{#BaseUrl}}');
end;

function HasContentServer(): Boolean;
begin
  Result := ContentUrl() <> '';
end;

// ---------------------------------------------------------------------
// The tools write "PROGRESS <percent> <text>" and "ERROR: <text>" lines.

procedure OnToolLog(const S: String; const Error, FirstLine: Boolean);
var
  Rest: String;
  P, N: Integer;
begin
  if Copy(S, 1, 9) = 'PROGRESS ' then begin
    Rest := Copy(S, 10, MaxInt);
    P := Pos(' ', Rest);
    if P > 0 then begin
      N := StrToIntDef(Copy(Rest, 1, P - 1), 0);
      if ProgressPage <> nil then begin
        ProgressPage.SetText(ToolStep, Copy(Rest, P + 1, MaxInt));
        ProgressPage.SetProgress(N, 100);
      end;
    end;
  end else begin
    Log(S);
    if Copy(S, 1, 7) = 'ERROR: ' then
      ToolError := Copy(S, 8, MaxInt);
  end;
end;

function RunTool(const Exe, Params, Step: String): Integer;
var
  R: Integer;
begin
  ToolError := '';
  ToolStep := Step;
  Log('Run: ' + Exe + ' ' + Params);
  if ProgressPage <> nil then begin
    ProgressPage.SetText(Step, '');
    ProgressPage.SetProgress(0, 100);
  end;
  if not ExecAndLogOutput(Exe, Params, '', SW_HIDE, ewWaitUntilTerminated, R, @OnToolLog) then begin
    R := -1;
    ToolError := 'Cannot start ' + Exe + '.';
  end;
  Log('Exit status: ' + IntToStr(R));
  Result := R;
end;

function Quote(const S: String): String;
begin
  Result := '"' + S + '"';
end;

// ---------------------------------------------------------------------
// The source page

function FindGameDisc(): String;
var
  D: Integer;
  Root: String;
begin
  Result := '';
  for D := Ord('D') to Ord('Z') do begin
    Root := Chr(D) + ':\';
    if FileExists(Root + 'DTU\DTU.MSN') then begin
      Result := Root;
      Exit;
    end;
  end;
end;

procedure BrowseFolder(Edit: TNewEdit);
var
  Dir: String;
begin
  Dir := Edit.Text;
  if BrowseForFolder('Choose the game disc, its DTU folder, or the folder of an installed Descent to Undermountain:', Dir, False) then
    Edit.Text := Dir;
end;

procedure BrowseImage(Edit: TNewEdit);
var
  F: String;
begin
  F := Edit.Text;
  if GetOpenFileName('Choose the disc image', F, '', 'Disc images (*.cue;*.bin;*.iso)|*.cue;*.bin;*.iso|All files|*.*', 'cue') then
    Edit.Text := F;
end;

procedure SourceFolderClick(Sender: TObject); begin BrowseFolder(SourceEdit); end;
procedure SourceImageClick(Sender: TObject); begin BrowseImage(SourceEdit); end;
procedure DiscFolderClick(Sender: TObject); begin BrowseFolder(DiscEdit); end;
procedure DiscImageClick(Sender: TObject); begin BrowseImage(DiscEdit); end;

function AddButton(Caption: String; Left, Top: Integer; Click: TNotifyEvent): TNewButton;
begin
  Result := TNewButton.Create(SourcePage);
  Result.Parent := SourcePage.Surface;
  Result.Caption := Caption;
  Result.Left := Left;
  Result.Top := Top;
  Result.Width := ScaleX(90);
  Result.Height := ScaleY(23);
  Result.OnClick := Click;
end;

function AddLabel(Caption: String; Top: Integer): TNewStaticText;
begin
  Result := TNewStaticText.Create(SourcePage);
  Result.Parent := SourcePage.Surface;
  Result.Caption := Caption;
  Result.Top := Top;
  Result.Width := SourcePage.SurfaceWidth;
  Result.WordWrap := True;
  Result.AutoSize := True;
end;

procedure InitializeWizard();
var
  T, W: Integer;
  L: TNewStaticText;
begin
  SourcePage := CreateCustomPage(wpSelectDir, 'Game files',
    'ASCENT needs the files of Descent to Undermountain.');
  L := AddLabel('Choose the game disc (a CD drive or a copy of the disc), a disc image (.cue, .bin, .iso), ' +
    'or the folder of an installed Descent to Undermountain. Setup copies the game files into the ' +
    'installation folder. The original files are not changed.', 0);
  T := L.Top + L.Height + ScaleY(10);
  W := SourcePage.SurfaceWidth - 2 * ScaleX(96);
  AddLabel('Game files:', T);
  T := T + ScaleY(18);
  SourceEdit := TNewEdit.Create(SourcePage);
  SourceEdit.Parent := SourcePage.Surface;
  SourceEdit.Top := T;
  SourceEdit.Width := W - ScaleX(6);
  SourceEdit.Text := ExpandConstant('{param:SOURCE|}');
  if SourceEdit.Text = '' then
    SourceEdit.Text := FindGameDisc();
  AddButton('&Folder...', W, T - ScaleY(1), @SourceFolderClick);
  AddButton('&Image...', W + ScaleX(96), T - ScaleY(1), @SourceImageClick);
  T := T + ScaleY(36);
  L := AddLabel('The game disc, only when the game files above are an installed game (the installed game has ' +
    'only a part of the files). Leave it empty to look in the CD drives:', T);
  T := L.Top + L.Height + ScaleY(4);
  DiscEdit := TNewEdit.Create(SourcePage);
  DiscEdit.Parent := SourcePage.Surface;
  DiscEdit.Top := T;
  DiscEdit.Width := W - ScaleX(6);
  DiscEdit.Text := ExpandConstant('{param:DISC|}');
  AddButton('F&older...', W, T - ScaleY(1), @DiscFolderClick);
  AddButton('I&mage...', W + ScaleX(96), T - ScaleY(1), @DiscImageClick);
  T := T + ScaleY(36);
  SourceInfo := AddLabel('', T);
  ProgressPage := CreateOutputProgressPage('Installing the game files', 'Please wait while Setup copies the files of Descent to Undermountain.');
end;

// Examine the sources with ascent-import --check. Returns '' if good.
function CheckSources(): String;
var
  Params, Line: String;
  R, I: Integer;
  Output: TExecOutput;
begin
  Result := '';
  SourcePatched := False;
  SourcesChecked := False;
  if Trim(SourceEdit.Text) = '' then begin
    Result := 'Choose the game files: the game disc, a disc image or an installed Descent to Undermountain.';
    Exit;
  end;
  ExtractTemporaryFile('ascent-import.exe');
  Params := '--check --source ' + Quote(Trim(SourceEdit.Text));
  if Trim(DiscEdit.Text) <> '' then
    Params := Params + ' --disc ' + Quote(Trim(DiscEdit.Text));
  Log('Check: ' + Params);
  if not ExecAndCaptureOutput(ExpandConstant('{tmp}\ascent-import.exe'), Params, '', SW_HIDE, ewWaitUntilTerminated, R, Output) then begin
    Result := 'Cannot run ascent-import.exe.';
    Exit;
  end;
  for I := 0 to GetArrayLength(Output.StdOut) - 1 do begin
    Line := Output.StdOut[I];
    Log(Line);
    if Line = 'PATCHED yes' then
      SourcePatched := True;
    if Copy(Line, 1, 7) = 'ERROR: ' then
      Result := Copy(Line, 8, MaxInt);
  end;
  if (Result = '') and (R <> 0) then
    Result := 'The game files cannot be read (ascent-import status ' + IntToStr(R) + ').';
  if (Result = '') and not SourcePatched and (ExpandConstant('{param:PATCH|}') = '') and not HasContentServer() then
    Result := 'The game files do not have the 1.3 patch, and this setup program has no download address for it. ' +
      'Choose an installed game with the 1.3 patch, or start Setup with /PATCH=<the 1.3 patch archive>.';
  SourcesChecked := Result = '';
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Msg: String;
begin
  Result := True;
  if CurPageID = SourcePage.ID then begin
    Msg := CheckSources();
    if Msg <> '' then begin
      MsgBox(Msg, mbError, MB_OK);
      Result := False;
    end else if SourcePatched then
      SourceInfo.Caption := 'The game files have the 1.3 patch.'
    else
      SourceInfo.Caption := 'The game files do not have the 1.3 patch: Setup downloads its files.';
  end;
end;

// ---------------------------------------------------------------------
// Before the files: the import (a failure stops Setup with its message)

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Patch, Params, Url: String;
  R: Integer;
begin
  Result := '';
  if not SourcesChecked then begin
    Result := CheckSources();
    if Result <> '' then
      Exit;
  end;
  ExtractTemporaryFile('ascent-import.exe');
  ExtractTemporaryFile('ascent-content.exe');
  ExtractTemporaryFile('content.ini');
  ProgressPage.Show();
  try
    Patch := ExpandConstant('{param:PATCH|}');
    if not SourcePatched and (Patch = '') then begin
      Url := ContentUrl();
      Patch := ExpandConstant('{tmp}\patch13.zip');
      R := RunTool(ExpandConstant('{tmp}\ascent-content.exe'),
        '--root ' + Quote(ExpandConstant('{tmp}')) + ' --url ' + Quote(Url) + ' download patch13 ' + Quote(Patch),
        'Downloading the files of the 1.3 patch...');
      if R <> 0 then begin
        Result := 'The files of the 1.3 patch cannot be downloaded.' + #13#10#13#10 + ToolError;
        Exit;
      end;
    end;
    Params := '--source ' + Quote(Trim(SourceEdit.Text)) + ' --dest ' + Quote(ExpandConstant('{app}\data'));
    if Trim(DiscEdit.Text) <> '' then
      Params := Params + ' --disc ' + Quote(Trim(DiscEdit.Text));
    if not SourcePatched then
      Params := Params + ' --patch ' + Quote(Patch);
    R := RunTool(ExpandConstant('{tmp}\ascent-import.exe'), Params, 'Copying the game files...');
    if R <> 0 then begin
      Result := 'The game files cannot be copied.' + #13#10#13#10 + ToolError;
      DelTree(ExpandConstant('{app}\data'), True, True, True);
    end;
  finally
    ProgressPage.Hide();
  end;
end;

// ---------------------------------------------------------------------
// After the files: the content packs (a failure is only a warning)

procedure InstallPacks(const Packs, What: String);
var
  R: Integer;
begin
  R := RunTool(ExpandConstant('{app}\ascent-content.exe'),
    '--root ' + Quote(ExpandConstant('{app}')) + ' --url ' + Quote(ContentUrl()) + ' install ' + Packs,
    'Downloading the ' + What + '...');
  if R <> 0 then
    SuppressibleMsgBox('The ' + What + ' cannot be installed now. The game works without them; ' +
      'install them later with ASCENT Content in the Start menu.' + #13#10#13#10 + ToolError, mbError, MB_OK, IDOK);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    if ExpandConstant('{param:CONTENTURL|}') <> '' then
      SetIniString('content', 'base_url', ExpandConstant('{param:CONTENTURL|}'), ExpandConstant('{app}\content.ini'));
    if WizardIsTaskSelected('hdtextures') or WizardIsTaskSelected('hdmovies') then begin
      ProgressPage.Show();
      try
        if WizardIsTaskSelected('hdtextures') then
          InstallPacks('textures ui', 'upscaled textures and interface');
        if WizardIsTaskSelected('hdmovies') then
          InstallPacks('movies', 'upscaled movies');
      finally
        ProgressPage.Hide();
      end;
    end;
  end;
end;

// ---------------------------------------------------------------------
// Uninstall: the saves and the settings of the user stay unless the user
// says to remove them.

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Dir: String;
begin
  if CurUninstallStep = usPostUninstall then begin
    Dir := ExpandConstant('{userappdata}\ASCENT');
    if DirExists(Dir) then
      if SuppressibleMsgBox('Remove your saved games and settings of ASCENT too?' + #13#10#13#10 + Dir,
          mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDNO) = IDYES then
        DelTree(Dir, True, True, True);
  end;
end;
