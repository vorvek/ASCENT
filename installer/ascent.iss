; ASCENT setup script (Inno Setup 6.5 or later). docs/install.md describes it.
;
; Build it with installer/build.sh: the script expects the files of
; "cmake --install" in the folder STAGE (default ..\build-rel\stage).
;
; The setup program asks for the game disc (a drive, a copy of the disc or
; a disc image), copies the program, runs ascent-import to make the data
; folder, downloads the 1.3 patch files from the content server, and
; downloads the upscaled content that the user chooses. An installed
; Descent to Undermountain is not a source: it has only a part of the files.
;
; Command line options (also for a silent install):
;   /SOURCE=path      the game disc (folder, drive, .cue, .bin or .iso)
;   /PATCH=path       a local 1.3 patch archive or folder (no download)
;   /CONTENTURL=url   another content server (tests)
;   and the standard options: /LANG=, /DIR=, /TASKS=, /SILENT, /VERYSILENT, ...
;
; The setup program is in English, Spanish, German, French and Italian. The
; Spanish text of the game (task langes, pack lang_es) is checked by default
; in the Spanish setup program, which then also writes default.cfg
; (Language=es) into the installation folder.

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
#ifndef LANG_ES_KB
  #define LANG_ES_KB "2775"
#endif
; The content server: the one place is installer/content.ini.
#define BaseUrl ReadIni(SourcePath + "content.ini", "content", "base_url", "")

[Setup]
AppId={{6C2E9A57-3B1D-4E8F-9A40-5D1E0C7A2B13}
AppName=ASCENT-DTU
AppVersion={#APPVERSION}
AppVerName=ASCENT-DTU {#APPVERSION}
AppPublisher=ASCENT-DTU project
DefaultDirName={userpf}\ASCENT-DTU
DisableProgramGroupPage=yes
DefaultGroupName=ASCENT-DTU
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OUTDIR}
OutputBaseFilename=ascent-dtu-setup-{#APPVERSION}
SetupIconFile=res\ascent.ico
UninstallDisplayIcon={app}\data\DTU.ICO
UninstallDisplayName=ASCENT-DTU
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; The data folder (about 420 MB) is made by ascent-import, not by [Files].
ExtraDiskSpaceRequired=440000000
MinVersion=10.0
; The first page: the language of the setup program (preselected from the
; language of Windows).
ShowLanguageDialog=yes

; The language of the setup program. The texts of this script are in
; [CustomMessages] below, one block for each language.
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Tasks]
Name: "startmenu"; Description: "{cm:TaskStartMenu}"; GroupDescription: "{cm:GroupShortcuts}"
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; GroupDescription: "{cm:GroupShortcuts}"; Flags: unchecked
Name: "hdtextures"; Description: "{cm:TaskTextures,{#TEXTURES_MB}}"; GroupDescription: "{cm:GroupContent}"; Check: HasContentServer
Name: "hdmovies"; Description: "{cm:TaskMovies,{#MOVIES_MB}}"; GroupDescription: "{cm:GroupContent}"; Flags: unchecked; Check: HasContentServer
; The Spanish text of the game: checked when the setup program is in Spanish
Name: "langes"; Description: "{cm:TaskLangEs,{#LANG_ES_KB}}"; GroupDescription: "{cm:GroupContent}"; Languages: spanish; Check: HasContentServer
Name: "langes"; Description: "{cm:TaskLangEs,{#LANG_ES_KB}}"; GroupDescription: "{cm:GroupContent}"; Languages: english german french italian; Flags: unchecked; Check: HasContentServer

[Files]
Source: "{#STAGE}\ascent.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\ascent-import.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\ascent-content.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\content.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#STAGE}\LICENSE-*.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; The icon of the game comes from the disc (ascent-import copies DTU.ICO).
; The setup program does not contain it.
Name: "{autoprograms}\ASCENT-DTU\ASCENT-DTU"; Filename: "{app}\ascent.exe"; WorkingDir: "{app}"; IconFilename: "{app}\data\DTU.ICO"; Tasks: startmenu
Name: "{autoprograms}\ASCENT-DTU\{cm:ShortcutContent}"; Filename: "{app}\ascent-content.exe"; WorkingDir: "{app}"; Tasks: startmenu
Name: "{autodesktop}\ASCENT-DTU"; Filename: "{app}\ascent.exe"; WorkingDir: "{app}"; IconFilename: "{app}\data\DTU.ICO"; Tasks: desktopicon

[Run]
Filename: "{app}\ascent.exe"; WorkingDir: "{app}"; Description: "{cm:RunPlay}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The files that ascent-import, ascent-content and the [Code] made
Type: filesandordirs; Name: "{app}\data"
Type: filesandordirs; Name: "{app}\hd"
Type: filesandordirs; Name: "{app}\content"
Type: files; Name: "{app}\default.cfg"
Type: dirifempty; Name: "{app}"

; The texts of this script. %1 is a value that the script gives.
; Every language has every message.
[CustomMessages]
english.GroupShortcuts=Shortcuts:
english.TaskStartMenu=Create a &Start menu entry
english.TaskDesktop=Create a &desktop shortcut
english.GroupContent=Downloadable content (can also be added later with ASCENT-DTU Content):
english.TaskTextures=Upscaled textures and interface (for the GPU renderer; download, about %1 MB)
english.TaskMovies=Upscaled movies (for the GPU renderer; download, about %1 MB)
english.TaskLangEs=Spanish translation of the game text (download, about %1 KB)
english.ShortcutContent=ASCENT-DTU Content
english.RunPlay=Play ASCENT-DTU
english.DiscPageTitle=Game disc
english.DiscPageDesc=ASCENT-DTU needs the disc of Descent to Undermountain.
english.DiscPageText=Select the location of the game disc. A CD drive, a folder with a copy of the disc, or a disc image (an .iso file, or a .bin and .cue pair) work. Setup copies the game files into the installation folder.
english.DiscLabel=Game disc:
english.ButtonFolder=&Folder...
english.ButtonImage=&Image...
english.BrowseFolderPrompt=Select the location of the game disc:
english.BrowseImageTitle=Choose the disc image
english.ImageFilter=Disc images (*.cue;*.bin;*.iso)|*.cue;*.bin;*.iso|All files|*.*
english.ProgressTitle=Installing the game files
english.ProgressDesc=Please wait while Setup copies the files of Descent to Undermountain.
english.ErrNoSource=Select the location of the game disc: a CD drive, a copy of the disc, or a disc image.
english.ErrCannotStart=Cannot start %1.
english.ErrReadStatus=The game files cannot be read (ascent-import status %1).
english.ErrNoPatchUrl=The game files do not have the 1.3 patch, and this setup program has no download address for it. Start Setup with /PATCH=<the 1.3 patch archive>.
english.InfoPatched=The game files have the 1.3 patch.
english.InfoUnpatched=The game files do not have the 1.3 patch: Setup downloads its files.
english.StepPatchDownload=Downloading the files of the 1.3 patch...
english.ErrPatchDownload=The files of the 1.3 patch cannot be downloaded.
english.StepImport=Copying the game files...
english.ErrImport=The game files cannot be copied.
english.PackTextures=Upscaled textures and interface
english.PackMovies=Upscaled movies
english.PackLangEs=Spanish translation of the game text
english.StepPack=Downloading: %1...
english.ErrPack=This content cannot be installed now: %1. The game works without it; install it later with ASCENT-DTU Content in the Start menu.
english.UninstallAsk=Remove your saved games and settings of ASCENT-DTU too?

spanish.GroupShortcuts=Accesos directos:
spanish.TaskStartMenu=Crear una entrada en el menú &Inicio
spanish.TaskDesktop=Crear un acceso directo en el &escritorio
spanish.GroupContent=Contenido descargable (también se puede añadir más tarde con Contenido de ASCENT-DTU):
spanish.TaskTextures=Texturas e IU en alta resolución (para el renderizador por GPU; descarga de unos %1 MB)
spanish.TaskMovies=Vídeos en alta resolución (para el renderizador por GPU; descarga de unos %1 MB)
spanish.TaskLangEs=Traducción al español de los textos del juego (descarga de unos %1 KB)
spanish.ShortcutContent=Contenido de ASCENT-DTU
spanish.RunPlay=Jugar a ASCENT-DTU
spanish.DiscPageTitle=Disco del juego
spanish.DiscPageDesc=ASCENT-DTU necesita el disco de Descent to Undermountain.
spanish.DiscPageText=Seleccione la ubicación del disco del juego. Sirve una unidad de CD, una carpeta con una copia del disco o una imagen de disco (un archivo .iso, o un par de archivos .bin y .cue). El programa de instalación copia los archivos del juego en la carpeta de instalación.
spanish.DiscLabel=Disco del juego:
spanish.ButtonFolder=&Carpeta...
spanish.ButtonImage=&Imagen...
spanish.BrowseFolderPrompt=Seleccione la ubicación del disco del juego:
spanish.BrowseImageTitle=Elija la imagen del disco
spanish.ImageFilter=Imágenes de disco (*.cue;*.bin;*.iso)|*.cue;*.bin;*.iso|Todos los archivos|*.*
spanish.ProgressTitle=Instalando los archivos del juego
spanish.ProgressDesc=Espere mientras el programa de instalación copia los archivos de Descent to Undermountain.
spanish.ErrNoSource=Seleccione la ubicación del disco del juego: una unidad de CD, una copia del disco o una imagen de disco.
spanish.ErrCannotStart=No se puede iniciar %1.
spanish.ErrReadStatus=No se pueden leer los archivos del juego (estado de ascent-import: %1).
spanish.ErrNoPatchUrl=Los archivos del juego no tienen el parche 1.3 y este programa de instalación no tiene ninguna dirección para descargarlo. Inicie la instalación con /PATCH=<el archivo del parche 1.3>.
spanish.InfoPatched=Los archivos del juego tienen el parche 1.3.
spanish.InfoUnpatched=Los archivos del juego no tienen el parche 1.3: el programa de instalación descargará sus archivos.
spanish.StepPatchDownload=Descargando los archivos del parche 1.3...
spanish.ErrPatchDownload=No se pueden descargar los archivos del parche 1.3.
spanish.StepImport=Copiando los archivos del juego...
spanish.ErrImport=No se pueden copiar los archivos del juego.
spanish.PackTextures=Texturas e IU en alta resolución
spanish.PackMovies=Vídeos en alta resolución
spanish.PackLangEs=Traducción al español de los textos del juego
spanish.StepPack=Descargando: %1...
spanish.ErrPack=Este contenido no se puede instalar ahora: %1. El juego funciona sin él; puede instalarlo más tarde con Contenido de ASCENT-DTU en el menú Inicio.
spanish.UninstallAsk=¿Eliminar también las partidas guardadas y la configuración de ASCENT-DTU?

german.GroupShortcuts=Verknüpfungen:
german.TaskStartMenu=Eintrag im &Startmenü erstellen
german.TaskDesktop=Verknüpfung auf dem &Desktop erstellen
german.GroupContent=Herunterladbare Inhalte (auch später mit ASCENT-DTU Inhalte installierbar):
german.TaskTextures=Hochskalierte Texturen und Oberfläche (für den GPU-Renderer; Download, etwa %1 MB)
german.TaskMovies=Hochskalierte Videos (für den GPU-Renderer; Download, etwa %1 MB)
german.TaskLangEs=Spanische Übersetzung der Spieltexte (Download, etwa %1 KB)
german.ShortcutContent=ASCENT-DTU Inhalte
german.RunPlay=ASCENT-DTU spielen
german.DiscPageTitle=Spiel-CD
german.DiscPageDesc=ASCENT-DTU benötigt die CD von Descent to Undermountain.
german.DiscPageText=Wählen Sie den Ort der Spiel-CD. Möglich sind ein CD-Laufwerk, ein Ordner mit einer Kopie der CD oder ein CD-Abbild (eine .iso-Datei oder ein Paar aus .bin- und .cue-Datei). Setup kopiert die Spieldateien in den Installationsordner.
german.DiscLabel=Spiel-CD:
german.ButtonFolder=&Ordner...
german.ButtonImage=A&bbild...
german.BrowseFolderPrompt=Wählen Sie den Ort der Spiel-CD:
german.BrowseImageTitle=CD-Abbild auswählen
german.ImageFilter=CD-Abbilder (*.cue;*.bin;*.iso)|*.cue;*.bin;*.iso|Alle Dateien|*.*
german.ProgressTitle=Die Spieldateien werden installiert
german.ProgressDesc=Bitte warten Sie, während Setup die Dateien von Descent to Undermountain kopiert.
german.ErrNoSource=Wählen Sie den Ort der Spiel-CD: ein CD-Laufwerk, eine Kopie der CD oder ein CD-Abbild.
german.ErrCannotStart=%1 kann nicht gestartet werden.
german.ErrReadStatus=Die Spieldateien können nicht gelesen werden (Status von ascent-import: %1).
german.ErrNoPatchUrl=Den Spieldateien fehlt der Patch 1.3, und dieses Setup-Programm hat keine Download-Adresse dafür. Starten Sie Setup mit /PATCH=<Archiv des Patches 1.3>.
german.InfoPatched=Die Spieldateien enthalten den Patch 1.3.
german.InfoUnpatched=Den Spieldateien fehlt der Patch 1.3: Setup lädt seine Dateien herunter.
german.StepPatchDownload=Die Dateien des Patches 1.3 werden heruntergeladen...
german.ErrPatchDownload=Die Dateien des Patches 1.3 können nicht heruntergeladen werden.
german.StepImport=Die Spieldateien werden kopiert...
german.ErrImport=Die Spieldateien können nicht kopiert werden.
german.PackTextures=Hochskalierte Texturen und Oberfläche
german.PackMovies=Hochskalierte Videos
german.PackLangEs=Spanische Übersetzung der Spieltexte
german.StepPack=Download: %1...
german.ErrPack=Dieser Inhalt kann jetzt nicht installiert werden: %1. Das Spiel funktioniert auch ohne ihn; Sie können ihn später mit ASCENT-DTU Inhalte im Startmenü installieren.
german.UninstallAsk=Auch die Spielstände und Einstellungen von ASCENT-DTU entfernen?

french.GroupShortcuts=Raccourcis :
french.TaskStartMenu=Créer une entrée dans le menu &Démarrer
french.TaskDesktop=Créer un raccourci sur le &bureau
french.GroupContent=Contenu téléchargeable (peut aussi être ajouté plus tard avec Contenu ASCENT-DTU) :
french.TaskTextures=Textures et interface en haute résolution (pour le rendu GPU ; téléchargement d'environ %1 Mo)
french.TaskMovies=Vidéos en haute résolution (pour le rendu GPU ; téléchargement d'environ %1 Mo)
french.TaskLangEs=Traduction espagnole des textes du jeu (téléchargement d'environ %1 Ko)
french.ShortcutContent=Contenu ASCENT-DTU
french.RunPlay=Jouer à ASCENT-DTU
french.DiscPageTitle=Disque du jeu
french.DiscPageDesc=ASCENT-DTU a besoin du disque de Descent to Undermountain.
french.DiscPageText=Sélectionnez l'emplacement du disque du jeu : un lecteur de CD, un dossier contenant une copie du disque, ou une image de disque (un fichier .iso, ou une paire de fichiers .bin et .cue). L'assistant copie les fichiers du jeu dans le dossier d'installation.
french.DiscLabel=Disque du jeu :
french.ButtonFolder=&Dossier...
french.ButtonImage=&Image...
french.BrowseFolderPrompt=Sélectionnez l'emplacement du disque du jeu :
french.BrowseImageTitle=Choisir l'image du disque
french.ImageFilter=Images de disque (*.cue;*.bin;*.iso)|*.cue;*.bin;*.iso|Tous les fichiers|*.*
french.ProgressTitle=Installation des fichiers du jeu
french.ProgressDesc=Veuillez patienter pendant que l'assistant copie les fichiers de Descent to Undermountain.
french.ErrNoSource=Sélectionnez l'emplacement du disque du jeu : un lecteur de CD, une copie du disque ou une image de disque.
french.ErrCannotStart=Impossible de lancer %1.
french.ErrReadStatus=Impossible de lire les fichiers du jeu (code de sortie d'ascent-import : %1).
french.ErrNoPatchUrl=Les fichiers du jeu n'ont pas le patch 1.3, et ce programme d'installation n'a pas d'adresse pour le télécharger. Lancez l'installation avec /PATCH=<l'archive du patch 1.3>.
french.InfoPatched=Les fichiers du jeu ont le patch 1.3.
french.InfoUnpatched=Les fichiers du jeu n'ont pas le patch 1.3 : l'assistant va télécharger ses fichiers.
french.StepPatchDownload=Téléchargement des fichiers du patch 1.3...
french.ErrPatchDownload=Impossible de télécharger les fichiers du patch 1.3.
french.StepImport=Copie des fichiers du jeu...
french.ErrImport=Impossible de copier les fichiers du jeu.
french.PackTextures=Textures et interface en haute résolution
french.PackMovies=Vidéos en haute résolution
french.PackLangEs=Traduction espagnole des textes du jeu
french.StepPack=Téléchargement : %1...
french.ErrPack=Ce contenu ne peut pas être installé maintenant : %1. Le jeu fonctionne sans lui ; vous pourrez l'installer plus tard avec Contenu ASCENT-DTU dans le menu Démarrer.
french.UninstallAsk=Supprimer aussi les parties sauvegardées et les paramètres d'ASCENT-DTU ?

italian.GroupShortcuts=Collegamenti:
italian.TaskStartMenu=Crea una voce nel menu &Start
italian.TaskDesktop=Crea un collegamento sul &desktop
italian.GroupContent=Contenuti scaricabili (si possono aggiungere anche in seguito con Contenuti ASCENT-DTU):
italian.TaskTextures=Texture e interfaccia ad alta risoluzione (per il renderer GPU; download di circa %1 MB)
italian.TaskMovies=Filmati ad alta risoluzione (per il renderer GPU; download di circa %1 MB)
italian.TaskLangEs=Traduzione spagnola dei testi del gioco (download di circa %1 KB)
italian.ShortcutContent=Contenuti ASCENT-DTU
italian.RunPlay=Gioca ad ASCENT-DTU
italian.DiscPageTitle=Disco del gioco
italian.DiscPageDesc=ASCENT-DTU richiede il disco di Descent to Undermountain.
italian.DiscPageText=Selezionare la posizione del disco del gioco. Vanno bene un'unità CD, una cartella con una copia del disco o un'immagine del disco (un file .iso, oppure una coppia di file .bin e .cue). Il programma di installazione copia i file del gioco nella cartella di installazione.
italian.DiscLabel=Disco del gioco:
italian.ButtonFolder=&Cartella...
italian.ButtonImage=&Immagine...
italian.BrowseFolderPrompt=Selezionare la posizione del disco del gioco:
italian.BrowseImageTitle=Scegliere l'immagine del disco
italian.ImageFilter=Immagini disco (*.cue;*.bin;*.iso)|*.cue;*.bin;*.iso|Tutti i file|*.*
italian.ProgressTitle=Installazione dei file del gioco
italian.ProgressDesc=Attendere mentre il programma di installazione copia i file di Descent to Undermountain.
italian.ErrNoSource=Selezionare la posizione del disco del gioco: un'unità CD, una copia del disco o un'immagine del disco.
italian.ErrCannotStart=Impossibile avviare %1.
italian.ErrReadStatus=Impossibile leggere i file del gioco (stato di ascent-import: %1).
italian.ErrNoPatchUrl=I file del gioco non hanno la patch 1.3 e questo programma di installazione non ha un indirizzo da cui scaricarla. Avviare l'installazione con /PATCH=<l'archivio della patch 1.3>.
italian.InfoPatched=I file del gioco hanno la patch 1.3.
italian.InfoUnpatched=I file del gioco non hanno la patch 1.3: il programma di installazione ne scarica i file.
italian.StepPatchDownload=Download dei file della patch 1.3...
italian.ErrPatchDownload=Impossibile scaricare i file della patch 1.3.
italian.StepImport=Copia dei file del gioco...
italian.ErrImport=Impossibile copiare i file del gioco.
italian.PackTextures=Texture e interfaccia ad alta risoluzione
italian.PackMovies=Filmati ad alta risoluzione
italian.PackLangEs=Traduzione spagnola dei testi del gioco
italian.StepPack=Download: %1...
italian.ErrPack=Questo contenuto non può essere installato ora: %1. Il gioco funziona anche senza; è possibile installarlo in seguito con Contenuti ASCENT-DTU nel menu Start.
italian.UninstallAsk=Rimuovere anche le partite salvate e le impostazioni di ASCENT-DTU?

[Code]
var
  SourcePage: TWizardPage;
  SourceEdit: TNewEdit;
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
    ToolError := FmtMessage(CustomMessage('ErrCannotStart'), [Exe]);
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

procedure BrowseFolder(Edit: TNewEdit);
var
  Dir: String;
begin
  Dir := Edit.Text;
  if BrowseForFolder(CustomMessage('BrowseFolderPrompt'), Dir, False) then
    Edit.Text := Dir;
end;

procedure BrowseImage(Edit: TNewEdit);
var
  F: String;
begin
  F := Edit.Text;
  if GetOpenFileName(CustomMessage('BrowseImageTitle'), F, '', CustomMessage('ImageFilter'), 'cue') then
    Edit.Text := F;
end;

procedure SourceFolderClick(Sender: TObject); begin BrowseFolder(SourceEdit); end;
procedure SourceImageClick(Sender: TObject); begin BrowseImage(SourceEdit); end;

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
  SourcePage := CreateCustomPage(wpSelectDir, CustomMessage('DiscPageTitle'),
    CustomMessage('DiscPageDesc'));
  L := AddLabel(CustomMessage('DiscPageText'), 0);
  T := L.Top + L.Height + ScaleY(10);
  W := SourcePage.SurfaceWidth - 2 * ScaleX(96);
  AddLabel(CustomMessage('DiscLabel'), T);
  T := T + ScaleY(18);
  SourceEdit := TNewEdit.Create(SourcePage);
  SourceEdit.Parent := SourcePage.Surface;
  SourceEdit.Top := T;
  SourceEdit.Width := W - ScaleX(6);
  SourceEdit.Text := ExpandConstant('{param:SOURCE|}');
  AddButton(CustomMessage('ButtonFolder'), W, T - ScaleY(1), @SourceFolderClick);
  AddButton(CustomMessage('ButtonImage'), W + ScaleX(96), T - ScaleY(1), @SourceImageClick);
  T := T + ScaleY(36);
  SourceInfo := AddLabel('', T);
  ProgressPage := CreateOutputProgressPage(CustomMessage('ProgressTitle'), CustomMessage('ProgressDesc'));
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
    Result := CustomMessage('ErrNoSource');
    Exit;
  end;
  ExtractTemporaryFile('ascent-import.exe');
  Params := '--check --source ' + Quote(Trim(SourceEdit.Text));
  Log('Check: ' + Params);
  if not ExecAndCaptureOutput(ExpandConstant('{tmp}\ascent-import.exe'), Params, '', SW_HIDE, ewWaitUntilTerminated, R, Output) then begin
    Result := FmtMessage(CustomMessage('ErrCannotStart'), ['ascent-import.exe']);
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
    Result := FmtMessage(CustomMessage('ErrReadStatus'), [IntToStr(R)]);
  if (Result = '') and not SourcePatched and (ExpandConstant('{param:PATCH|}') = '') and not HasContentServer() then
    Result := CustomMessage('ErrNoPatchUrl');
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
      SourceInfo.Caption := CustomMessage('InfoPatched')
    else
      SourceInfo.Caption := CustomMessage('InfoUnpatched');
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
        CustomMessage('StepPatchDownload'));
      if R <> 0 then begin
        Result := CustomMessage('ErrPatchDownload') + #13#10#13#10 + ToolError;
        Exit;
      end;
    end;
    Params := '--source ' + Quote(Trim(SourceEdit.Text)) + ' --dest ' + Quote(ExpandConstant('{app}\data'));
    if not SourcePatched then
      Params := Params + ' --patch ' + Quote(Patch);
    R := RunTool(ExpandConstant('{tmp}\ascent-import.exe'), Params, CustomMessage('StepImport'));
    if R <> 0 then begin
      Result := CustomMessage('ErrImport') + #13#10#13#10 + ToolError;
      DelTree(ExpandConstant('{app}\data'), True, True, True);
    end;
  finally
    ProgressPage.Hide();
  end;
end;

// ---------------------------------------------------------------------
// After the files: the content packs (a failure is only a warning)

// What: the name of the content (a custom message). True if installed.
function InstallPacks(const Packs, What: String): Boolean;
var
  R: Integer;
begin
  R := RunTool(ExpandConstant('{app}\ascent-content.exe'),
    '--root ' + Quote(ExpandConstant('{app}')) + ' --url ' + Quote(ContentUrl()) + ' install ' + Packs,
    FmtMessage(CustomMessage('StepPack'), [CustomMessage(What)]));
  Result := R = 0;
  if not Result then
    SuppressibleMsgBox(FmtMessage(CustomMessage('ErrPack'), [CustomMessage(What)]) + #13#10#13#10 + ToolError,
      mbError, MB_OK, IDOK);
end;

// The Spanish setup program with the Spanish text makes Spanish the default
// language of the game: default.cfg in the installation folder (dtuopts.c).
// The game reads it before ascent.cfg of the user, so a language that the
// player chose stays.
procedure SetDefaultLanguage(const Code: String);
begin
  if not SaveStringToFile(ExpandConstant('{app}\default.cfg'), 'Language=' + Code + #13#10, False) then
    Log('Cannot write default.cfg');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    if ExpandConstant('{param:CONTENTURL|}') <> '' then
      SetIniString('content', 'base_url', ExpandConstant('{param:CONTENTURL|}'), ExpandConstant('{app}\content.ini'));
    if WizardIsTaskSelected('hdtextures') or WizardIsTaskSelected('hdmovies') or WizardIsTaskSelected('langes') then begin
      ProgressPage.Show();
      try
        if WizardIsTaskSelected('langes') then
          if InstallPacks('lang_es', 'PackLangEs') and (ActiveLanguage() = 'spanish') then
            SetDefaultLanguage('es');
        if WizardIsTaskSelected('hdtextures') then
          InstallPacks('textures ui', 'PackTextures');
        if WizardIsTaskSelected('hdmovies') then
          InstallPacks('movies', 'PackMovies');
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
    Dir := ExpandConstant('{userappdata}\ASCENT-DTU');
    if DirExists(Dir) then
      if SuppressibleMsgBox(CustomMessage('UninstallAsk') + #13#10#13#10 + Dir,
          mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDNO) = IDYES then
        DelTree(Dir, True, True, True);
  end;
end;
