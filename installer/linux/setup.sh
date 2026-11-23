#!/bin/sh
# ASCENT-DTU setup for Linux (docs/install.md).
#
#   ./setup.sh [options]              install
#   ./setup.sh --uninstall [--purge]  remove an installation
#
# The setup copies the program, makes the data folder from the game disc
# with ascent-import and the 1.3 patch of the content server, installs the
# optional content packs, and makes the icon, the menu entry and the
# command "ascent-dtu". It asks each question unless the answer is an
# option. Run ./setup.sh --help for the options.
#
# It needs a POSIX shell, coreutils, curl or wget, and unzip.
set -eu
umask 022

TAB=$(printf '\t')
BS="\\"
NL='
'
SETUP_VERSION=1
RECORD_NAME=install-record.txt
PACK_ORDER="lang_es textures ui movies"

HERE=$(cd "$(dirname "$0")" && pwd)

# ---------------------------------------------------------------------
# Messages

say() { printf '%s\n' "$*"; }
warn() { printf 'Warning: %s\n' "$*" >&2; }
die() {
	printf 'Error: %s\n' "$*" >&2
	exit 1
}

usage() {
	cat <<'EOF'
Usage: ./setup.sh [options]

Install ASCENT-DTU (Descent to Undermountain). The setup needs the game disc:
a mounted CD, a copy of the disc in a folder, or a disc image (.iso, .cue/.bin).

Options (each one answers a question of the setup):
  --prefix DIR       the installation folder
                     (default: ~/.local/share/ASCENT-DTU/game, or
                     /opt/ascent-dtu when run as root)
  --source PATH      the game disc: a folder or an image
  --lang en|es       the language of the game (es needs the Spanish pack)
  --textures yes|no  the upscaled textures and interface (download)
  --movies yes|no    the upscaled movies (download)
  --spanish yes|no   the Spanish text pack (download; yes with --lang es)
  --yes              no questions: take the default of each answer not given
  --url URL          another content server (the folder of manifest.txt)
  --patch PATH       the 1.3 patch archive or folder, instead of the download

  --uninstall        remove the installation (asks first; --yes does not ask)
  --purge            with --uninstall: also remove the saves and settings
  --help             this text
EOF
}

# ---------------------------------------------------------------------
# Helpers

# yesno VALUE: prints yes or no, or fails.
yesno() {
	case $1 in
		[Yy]|[Yy][Ee][Ss]|1|[Tt][Rr][Uu][Ee]) echo yes ;;
		[Nn]|[Nn][Oo]|0|[Ff][Aa][Ll][Ss][Ee]) echo no ;;
		*) return 1 ;;
	esac
}

# ask PROMPT DEFAULT: the answer in ANSWER (the default with --yes or an
# empty line).
ask() {
	if [ "$OPT_YES" = yes ]; then
		ANSWER=$2
		return 0
	fi
	if [ -n "$2" ]; then
		printf '%s [%s]: ' "$1" "$2"
	else
		printf '%s: ' "$1"
	fi
	if ! IFS= read -r ANSWER; then
		echo
		die "No answer (end of input). Give the answers as options, or use --yes."
	fi
	[ -n "$ANSWER" ] || ANSWER=$2
}

# ask_yn PROMPT DEFAULT(yes|no): ANSWER is yes or no.
ask_yn() {
	while :; do
		ask "$1 (yes/no)" "$2"
		if ANSWER=$(yesno "$ANSWER"); then
			return 0
		fi
		say "Please answer yes or no."
	done
}

# abspath PATH: the absolute path, with ~ expanded (the path need not exist).
abspath() {
	_p=$1
	# shellcheck disable=SC2088
	case $_p in
		"~") _p=$HOME ;;
		"~/"*) _p=$HOME/${_p#"~/"} ;;
	esac
	case $_p in
		/*) ;;
		*) _p=$(pwd)/$_p ;;
	esac
	# remove the "/" at the end and the "/./" parts
	while :; do
		case $_p in
			*/./*) _p=$(printf '%s' "$_p" | sed 's|/\./|/|g') ;;
			*/.) _p=${_p%/.} ;;
			?*/) _p=${_p%/} ;;
			*) break ;;
		esac
	done
	printf '%s' "$_p"
}

# existing_parent PATH: the nearest folder of PATH that exists.
existing_parent() {
	_p=$1
	while [ ! -d "$_p" ]; do
		_p=$(dirname "$_p")
	done
	printf '%s' "$_p"
}

# free_kb DIR: the free space of the file system of DIR in KB.
free_kb() {
	df -Pk "$1" | awk 'NR == 2 { print $4 }'
}

mb() {
	echo $((($1 + 524287) / 1048576))
}

filesize() {
	wc -c <"$1" | tr -d ' '
}

sha256() {
	sha256sum "$1" | cut -d ' ' -f 1
}

# sq STRING: STRING quoted for sh with single quotes.
sq() {
	printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"
}

# safe_rm_tree DIR BASE: rm -rf DIR only if DIR is strictly inside BASE.
safe_rm_tree() {
	[ -n "$1" ] && [ -n "$2" ] || return 0
	case $1 in
		*/../*|*/..) warn "Not removed (path with ..): $1"; return 0 ;;
	esac
	case $1 in
		"$2"/?*) rm -rf -- "$1" ;;
		*) warn "Not removed (outside $2): $1" ;;
	esac
}

ini_get() {
	awk -F= -v k="$1" '
		/^[ \t]*[;#]/ { next }
		/^\[/ { s = ($0 ~ /^\[content\]/); next }
		s && $1 == k { sub(/^[^=]*=/, ""); sub(/\r$/, ""); print; exit }' "$2"
}

# ---------------------------------------------------------------------
# Options

OPT_PREFIX=
OPT_SOURCE=
OPT_LANG=
OPT_TEXTURES=
OPT_MOVIES=
OPT_SPANISH=
OPT_YES=no
OPT_URL=${ASCENT_CONTENT_URL:-}
OPT_PATCH=
OPT_UNINSTALL=no
OPT_PURGE=no

need_arg() {
	[ "$2" -ge 2 ] || die "The option $1 needs a value."
}

while [ $# -gt 0 ]; do
	opt=$1
	val=
	case $opt in
		--*=*) val=${opt#*=}; opt=${opt%%=*}; shift; set -- "$opt" "$val" "$@" ;;
	esac
	case $1 in
		--prefix) need_arg "$1" $#; OPT_PREFIX=$2; shift 2 ;;
		--source) need_arg "$1" $#; OPT_SOURCE=$2; shift 2 ;;
		--lang) need_arg "$1" $#; OPT_LANG=$2; shift 2 ;;
		--textures) need_arg "$1" $#; OPT_TEXTURES=$(yesno "$2") || die "--textures takes yes or no."; shift 2 ;;
		--movies) need_arg "$1" $#; OPT_MOVIES=$(yesno "$2") || die "--movies takes yes or no."; shift 2 ;;
		--spanish) need_arg "$1" $#; OPT_SPANISH=$(yesno "$2") || die "--spanish takes yes or no."; shift 2 ;;
		--url) need_arg "$1" $#; OPT_URL=$2; shift 2 ;;
		--patch) need_arg "$1" $#; OPT_PATCH=$2; shift 2 ;;
		--yes|-y) OPT_YES=yes; shift ;;
		--uninstall) OPT_UNINSTALL=yes; shift ;;
		--purge) OPT_PURGE=yes; shift ;;
		--help|-h) usage; exit 0 ;;
		*) usage >&2; die "Unknown option: $1" ;;
	esac
done
case $OPT_LANG in
	''|en|es) ;;
	*) die "--lang takes en or es." ;;
esac

[ -n "${HOME:-}" ] || die "HOME is not set."
DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
case $DATA_HOME in
	/*) ;;
	*) DATA_HOME=$HOME/.local/share ;;	# the XDG rule: a relative value is ignored
esac
USERDIR=$DATA_HOME/ASCENT-DTU		# SDL_GetPrefPath of the game: saves and settings
if [ "$(id -u)" -eq 0 ]; then
	IS_ROOT=yes
	DEFAULT_PREFIX=/opt/ascent-dtu
else
	IS_ROOT=no
	DEFAULT_PREFIX=$USERDIR/game
fi

# The folders of a user install and of a system install.
set_mode_dirs() {
	case $PREFIX/ in
		"$HOME"/*) SYSTEM=no ;;
		*) SYSTEM=$IS_ROOT ;;
	esac
	if [ "$SYSTEM" = yes ]; then
		BIN_DIR=/usr/local/bin
		APP_DIR=/usr/local/share/applications
	else
		BIN_DIR=$HOME/.local/bin
		APP_DIR=$DATA_HOME/applications
	fi
}

TMPD=
STAGING=
PACK_LIST=
PACK_DEST=
INSTALLING=no
cleanup() {
	rc=$?
	trap - EXIT INT TERM HUP
	# a pack that was being moved into place: remove its files
	if [ -n "$PACK_LIST" ] && [ -f "$PACK_LIST" ] && [ -n "$PACK_DEST" ]; then
		while IFS= read -r rel; do
			rm -f -- "$PACK_DEST/$rel"
		done <"$PACK_LIST"
	fi
	[ -z "$STAGING" ] || safe_rm_tree "$STAGING" "$PREFIX/content"
	[ -z "$TMPD" ] || rm -rf -- "$TMPD"
	if [ "$rc" -ne 0 ] && [ "$INSTALLING" = yes ]; then
		printf '\nThe setup stopped. Run it again with the same answers: a download goes on where it stopped.\n' >&2
		[ -z "${LOG:-}" ] || printf 'The log: %s\n' "$LOG" >&2
	fi
	exit "$rc"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

TMPD=$(mktemp -d "${TMPDIR:-/tmp}/ascent-setup.XXXXXX")

# ---------------------------------------------------------------------
# Uninstall

if [ "$OPT_UNINSTALL" = yes ]; then
	if [ -n "$OPT_PREFIX" ]; then
		PREFIX=$(abspath "$OPT_PREFIX")
	elif [ -f "$HERE/$RECORD_NAME" ]; then
		PREFIX=$HERE
	else
		PREFIX=$DEFAULT_PREFIX
	fi
	RECORD=$PREFIX/$RECORD_NAME
	[ -f "$RECORD" ] || die "No installation of ASCENT-DTU in $PREFIX (no $RECORD_NAME). Give the folder with --prefix."
	head -n 1 "$RECORD" | grep -q '^ascent-dtu-install ' || die "$RECORD is not an install record of ASCENT-DTU."
	set_mode_dirs

	# Check each entry before anything is removed.
	: >"$TMPD/files"
	: >"$TMPD/trees"
	: >"$TMPD/rmdirs"
	while IFS= read -r line; do
		kind=${line%% *}
		path=${line#* }
		case $kind in
			ascent-dtu-install|prefix|'#'*) continue ;;
		esac
		case $path in
			/*) ;;
			*) die "The install record has a path that is not absolute: $path" ;;
		esac
		case $path in
			*/../*|*/..|*/./*) die "The install record has a path with . or ..: $path" ;;
		esac
		case $kind in
			file)
				case $path in
					"$PREFIX"/?*|"$BIN_DIR/ascent-dtu"|"$APP_DIR/ascent-dtu.desktop") ;;
					*) die "The install record has a file outside the installation: $path" ;;
				esac
				printf '%s\n' "$path" >>"$TMPD/files" ;;
			tree)
				case $path in
					"$PREFIX"/?*) ;;
					*) die "The install record has a folder outside the installation: $path" ;;
				esac
				printf '%s\n' "$path" >>"$TMPD/trees" ;;
			rmdir)		# a folder that the setup made: removed if empty
				printf '%s\n' "$path" >>"$TMPD/rmdirs" ;;
			*) die "The install record has an unknown line: $line" ;;
		esac
	done <"$RECORD"

	say "ASCENT-DTU is installed in $PREFIX."
	say "The setup will remove:"
	while IFS= read -r p; do
		[ -e "$p" ] && say "  $p (folder)"
	done <"$TMPD/trees"
	while IFS= read -r p; do
		if [ -e "$p" ] || [ -L "$p" ]; then say "  $p"; fi
	done <"$TMPD/files"
	if [ "$OPT_PURGE" = yes ]; then
		say "  $USERDIR (your saves and settings)"
	else
		say "Your saves and settings stay in $USERDIR."
	fi
	if [ "$OPT_YES" = yes ]; then ANSWER=yes; else ask_yn "Remove these files?" no; fi
	[ "$ANSWER" = yes ] || { say "Nothing was removed."; exit 0; }

	while IFS= read -r p; do
		safe_rm_tree "$p" "$PREFIX"
	done <"$TMPD/trees"
	while IFS= read -r p; do
		rm -f -- "$p"
	done <"$TMPD/files"
	rm -f -- "$RECORD"
	if rmdir -- "$PREFIX" 2>/dev/null; then :; else
		[ ! -d "$PREFIX" ] || say "The folder $PREFIX stays: it has files that the setup did not make."
	fi
	if [ "$OPT_PURGE" = yes ]; then
		case $USERDIR in
			*/ASCENT-DTU) [ ! -d "$USERDIR" ] || rm -rf -- "$USERDIR" ;;
		esac
	fi
	# the folders that the setup made, the deepest first
	tac "$TMPD/rmdirs" | while IFS= read -r p; do
		rmdir -- "$p" 2>/dev/null || :
	done
	if [ "$OPT_PURGE" = yes ]; then
		say "Removed ASCENT-DTU, its saves and its settings."
	else
		say "Removed ASCENT-DTU. The saves and settings are in $USERDIR."
	fi
	exit 0
fi

# ---------------------------------------------------------------------
# Install: the program files of the release

for f in ascent ascent-import content.ini; do
	[ -f "$HERE/$f" ] || die "$HERE has no $f. Run setup.sh in the unpacked release folder."
done
IMPORT=$HERE/ascent-import
[ -x "$IMPORT" ] || die "$IMPORT cannot be run (check that the folder is not mounted noexec)."
command -v unzip >/dev/null 2>&1 || die "The setup needs unzip. Install the package unzip."
command -v sha256sum >/dev/null 2>&1 || die "The setup needs sha256sum (coreutils)."
if command -v curl >/dev/null 2>&1; then
	FETCH=curl
elif command -v wget >/dev/null 2>&1; then
	FETCH=wget
else
	die "The setup needs curl or wget. Install one of them."
fi

BASE_URL=$OPT_URL
[ -n "$BASE_URL" ] || BASE_URL=$(ini_get base_url "$HERE/content.ini")
case $BASE_URL in
	''|*/) ;;
	*) BASE_URL=$BASE_URL/ ;;
esac
HDR_NAME=$(ini_get client_header "$HERE/content.ini")
HDR_VALUE=$(ini_get client_token "$HERE/content.ini")
USER_AGENT=$(ini_get user_agent "$HERE/content.ini")
[ -n "$USER_AGENT" ] || USER_AGENT=ASCENT-Setup/1.0

say "ASCENT-DTU setup"
say ""

# ---------------------------------------------------------------------
# 1. The installation folder

while :; do
	ask "Installation folder" "${OPT_PREFIX:-$DEFAULT_PREFIX}"
	PREFIX=$(abspath "$ANSWER")
	OPT_PREFIX=
	problem=
	# shellcheck disable=SC1003
	case $PREFIX in
		*"$NL"*|*'"'*|*'`'*|*'$'*|*'\'*|*'%'*) problem="The folder name must not have a newline or one of the characters \" \` \$ \\ %." ;;
		/|/usr|/usr/local|/opt|/bin|/etc|/home|/tmp|/var) problem="The folder $PREFIX is a system folder. Choose a folder for the game." ;;
		"$HOME"|"$HOME/.local"|"$DATA_HOME"|"$USERDIR") problem="Choose a folder only for the program, for example $DEFAULT_PREFIX." ;;
		"$HERE") problem="This is the unpacked release folder. Choose another folder." ;;
	esac
	if [ -z "$problem" ] && [ -e "$PREFIX" ] && [ ! -d "$PREFIX" ]; then
		problem="$PREFIX is a file."
	fi
	if [ -z "$problem" ] && [ -d "$PREFIX" ] && [ ! -f "$PREFIX/$RECORD_NAME" ] && [ -n "$(ls -A "$PREFIX")" ]; then
		problem="The folder $PREFIX is not empty and has no installation of ASCENT-DTU. Choose an empty or a new folder."
	fi
	if [ -z "$problem" ]; then
		parent=$(existing_parent "$PREFIX")
		if [ ! -w "$parent" ]; then
			problem="You cannot write to $parent. Choose a folder in your home folder, or run the setup as root (sudo) for a system install."
		fi
	fi
	[ -n "$problem" ] || break
	[ "$OPT_YES" = no ] || die "$problem"
	say "$problem"
done
set_mode_dirs
RECORD=$PREFIX/$RECORD_NAME
UPDATE=no
[ ! -f "$RECORD" ] || UPDATE=yes

# ---------------------------------------------------------------------
# 2. The game disc

# check_source PATH: runs ascent-import --check. Sets SRC_ERROR,
# SRC_PATCHED and SRC_BYTES.
check_source() {
	SRC_ERROR=
	SRC_PATCHED=no
	SRC_BYTES=0
	rc=0
	"$IMPORT" --check --source "$1" >"$TMPD/check.txt" 2>&1 || rc=$?
	while IFS= read -r line; do
		case $line in
			"ERROR: "*) SRC_ERROR=${line#ERROR: } ;;
			"PATCHED yes") SRC_PATCHED=yes ;;
			"FILES "*)
				# shellcheck disable=SC2086
				set -- $line
				SRC_BYTES=${3:-0} ;;
		esac
	done <"$TMPD/check.txt"
	if [ -z "$SRC_ERROR" ] && [ "$rc" -ne 0 ]; then
		SRC_ERROR="The game files cannot be read (ascent-import status $rc)."
	fi
}

say ""
say "The setup needs the game disc of Descent to Undermountain: a mounted CD,"
say "a copy of the disc in a folder, or a disc image (.iso, or .cue with .bin)."
say "An installed game is not enough: it does not have all the files."
while :; do
	ask "Game disc" "$OPT_SOURCE"
	OPT_SOURCE=
	if [ -z "$ANSWER" ]; then
		problem="Give the path of the game disc."
	else
		SOURCE=$(abspath "$ANSWER")
		if [ ! -e "$SOURCE" ]; then
			problem="$SOURCE does not exist."
		else
			check_source "$SOURCE"
			problem=$SRC_ERROR
		fi
	fi
	[ -n "$problem" ] || break
	[ "$OPT_YES" = no ] || die "$problem"
	say "$problem"
done
if [ "$SRC_PATCHED" = yes ]; then
	say "The disc files have the 1.3 patch."
else
	say "Good: the game disc. The setup adds the files of the 1.3 patch."
fi

# ---------------------------------------------------------------------
# 3. The content server and the choices

# fetch URL OUT: download URL to OUT; continues a partial OUT.
fetch() {
	case $1 in
		file://*)
			if [ "$FETCH" = wget ]; then
				cp -f -- "${1#file://}" "$2"
				return
			fi ;;
	esac
	if [ "$FETCH" = curl ]; then
		set -- "$1" "$2" -fL --connect-timeout 20 -A "$USER_AGENT" -C - -o "$2"
		[ -z "$HDR_NAME" ] || [ -z "$HDR_VALUE" ] || set -- "$@" -H "$HDR_NAME: $HDR_VALUE"
		if [ -t 2 ]; then set -- "$@" --progress-bar; else set -- "$@" -sS; fi
		url=$1
		shift 2
		curl "$@" -- "$url"
	else
		set -- "$1" "$2" -c -U "$USER_AGENT" -O "$2" -T 60
		[ -z "$HDR_NAME" ] || [ -z "$HDR_VALUE" ] || set -- "$@" --header="$HDR_NAME: $HDR_VALUE"
		if [ -t 2 ]; then set -- "$@" -q --show-progress; else set -- "$@" -nv; fi
		url=$1
		shift 2
		wget "$@" -- "$url"
	fi
}

MANIFEST=$TMPD/manifest.txt
HAVE_MANIFEST=no
if [ -n "$BASE_URL" ]; then
	say ""
	say "Reading the list of the content server ($BASE_URL)."
	rm -f -- "$MANIFEST"
	if fetch "${BASE_URL}manifest.txt" "$MANIFEST" >"$TMPD/fetch.txt" 2>&1 && grep -q '^# ASCENT content manifest' "$MANIFEST"; then
		HAVE_MANIFEST=yes
	else
		cat "$TMPD/fetch.txt" >&2 || :
		warn "The content server cannot be reached."
	fi
fi
if [ "$HAVE_MANIFEST" = no ] && [ "$SRC_PATCHED" = no ] && [ -z "$OPT_PATCH" ]; then
	die "The setup needs the 1.3 patch from the content server. Check the Internet connection, or give the patch with --patch."
fi

# pack_info NAME: P_VERSION, P_TARGET, P_FILE, P_SIZE, P_SHA, P_TITLE from
# the manifest. Fails if the pack is not listed or its line is not valid.
pack_info() {
	[ "$HAVE_MANIFEST" = yes ] || return 1
	line=$(awk -F'\t' -v n="$1" '!/^#/ && $1 == n && NF >= 8 { print; exit }' "$MANIFEST")
	[ -n "$line" ] || return 1
	IFS=$TAB read -r _n P_VERSION P_TARGET P_FILE P_SIZE P_SHA _files P_TITLE <<EOF
$line
EOF
	case $P_SIZE in ''|*[!0-9]*) return 1 ;; esac
	case $P_SHA in *[!0-9a-f]*) return 1 ;; esac
	[ ${#P_SHA} -eq 64 ] || return 1
	case $P_FILE in ''|*[!A-Za-z0-9._-]*|.*) return 1 ;; esac
	case $P_VERSION in ''|*[!A-Za-z0-9._-]*) return 1 ;; esac
	return 0
}

pack_size() {
	if pack_info "$1"; then echo "$P_SIZE"; else echo 0; fi
}

# The language
say ""
case ${LC_ALL:-${LANG:-}} in
	es*) lang_default=es ;;
	*) lang_default=en ;;
esac
while :; do
	ask "Language of the game: en (English) or es (Spanish)" "${OPT_LANG:-$lang_default}"
	case $ANSWER in
		en|es) GAME_LANG=$ANSWER; break ;;
	esac
	[ "$OPT_YES" = no ] || die "The language is en or es."
	say "Please answer en or es."
done

# The packs
WANT_TEX=no
WANT_MOV=no
WANT_ES=no
if [ "$HAVE_MANIFEST" = yes ]; then
	tex_size=$(($(pack_size textures) + $(pack_size ui)))
	mov_size=$(pack_size movies)
	es_size=$(pack_size lang_es)
	if [ "$tex_size" -gt 0 ]; then
		if [ -n "$OPT_TEXTURES" ]; then WANT_TEX=$OPT_TEXTURES; else
			ask_yn "Upscaled textures and interface, for the GPU renderer (download, $(mb "$tex_size") MB)?" yes
			WANT_TEX=$ANSWER
		fi
	fi
	if [ "$mov_size" -gt 0 ]; then
		if [ -n "$OPT_MOVIES" ]; then WANT_MOV=$OPT_MOVIES; else
			ask_yn "Upscaled movies, for the GPU renderer (download, $(mb "$mov_size") MB)?" no
			WANT_MOV=$ANSWER
		fi
	fi
	if [ "$es_size" -gt 0 ]; then
		if [ "$GAME_LANG" = es ]; then
			[ "$OPT_SPANISH" != no ] || say "Spanish needs the Spanish text pack: the setup installs it."
			WANT_ES=yes
		elif [ -n "$OPT_SPANISH" ]; then WANT_ES=$OPT_SPANISH; else
			ask_yn "Spanish translation of the game text (download, $(((es_size + 1023) / 1024)) KB)?" no
			WANT_ES=$ANSWER
		fi
	fi
fi
if [ "$GAME_LANG" = es ] && [ "$WANT_ES" = no ]; then
	warn "The content server has no Spanish text pack: the game starts in English."
	GAME_LANG=en
fi

PACKS=
[ "$WANT_ES" = no ] || PACKS="$PACKS lang_es"
[ "$WANT_TEX" = no ] || PACKS="$PACKS textures ui"
[ "$WANT_MOV" = no ] || PACKS="$PACKS movies"

# ---------------------------------------------------------------------
# 4. The summary and the free space

need=$((SRC_BYTES + 64 * 1048576))
if [ "$SRC_PATCHED" = no ] && [ -z "$OPT_PATCH" ]; then
	pack_info patch13 || die "The content server has no 1.3 patch (pack patch13)."
	need=$((need + P_SIZE))
fi
for p in $PACKS; do
	need=$((need + 2 * $(pack_size "$p")))	# the download and the files
done
parent=$(existing_parent "$PREFIX")
free=$(free_kb "$parent")

say ""
say "Ready to install:"
say "  Folder:    $PREFIX"
[ "$UPDATE" = no ] || say "             (an update of the installation in this folder)"
say "  Game disc: $SOURCE"
say "  Language:  $GAME_LANG"
say "  Downloads:${PACKS:- none}"
say "  Space:     about $(mb "$need") MB needed, $((free / 1024)) MB free"
if [ "$SYSTEM" = yes ]; then
	say "  For all users: the command $BIN_DIR/ascent-dtu and the menu entry in $APP_DIR"
else
	say "  The command ~/.local/bin/ascent-dtu and the menu entry in $APP_DIR"
fi
say "  Saves and settings go to the folder of each user: $USERDIR"
if [ "$free" -lt $((need / 1024)) ]; then
	die "Not enough free space in $parent: $(mb "$need") MB needed, $((free / 1024)) MB free."
fi
ask_yn "Install now?" yes
[ "$ANSWER" = yes ] || { say "Nothing was installed."; exit 0; }

# ---------------------------------------------------------------------
# 5. The program files

INSTALLING=yes
# missing_dirs DIR: DIR and its folders that do not exist, the top one first
missing_dirs() {
	_d=$1
	_l=
	while [ ! -d "$_d" ]; do
		_l=$_d$NL$_l
		_d=$(dirname "$_d")
	done
	printf '%s' "$_l"
}
NEW_DIRS=$(missing_dirs "$PREFIX")
mkdir -p -- "$PREFIX" || die "Cannot make the folder $PREFIX."
LOG=$PREFIX/setup.log
: >"$LOG"
log() { printf '%s\n' "$*" >>"$LOG"; }
log "setup.sh $SETUP_VERSION, $(date)"
log "prefix=$PREFIX source=$SOURCE lang=$GAME_LANG packs=$PACKS url=$BASE_URL"

# The install record: one line for each file or folder that the setup
# made: "file PATH", "tree PATH" (a folder with all its files) or "rmdir
# PATH" (a folder, removed if it is empty). --uninstall removes exactly
# these.
if [ "$UPDATE" = no ]; then
	printf 'ascent-dtu-install %s\nprefix %s\n' "$SETUP_VERSION" "$PREFIX" >"$RECORD"
fi
rec() {
	grep -qxF -- "$1 $2" "$RECORD" || printf '%s %s\n' "$1" "$2" >>"$RECORD"
}
# rec_dirs LIST: "rmdir" lines for the folders of LIST (from missing_dirs)
rec_dirs() {
	printf '%s\n' "$1" | while IFS= read -r d; do
		[ -z "$d" ] || [ ! -d "$d" ] || rec rmdir "$d"
	done
}
rec_dirs "$NEW_DIRS"
rec file "$RECORD"
rec file "$LOG"

# install_file SRC NAME MODE
install_file() {
	cp -f -- "$1" "$PREFIX/.$2.new"
	chmod "$3" "$PREFIX/.$2.new"
	mv -f -- "$PREFIX/.$2.new" "$PREFIX/$2"
	rec file "$PREFIX/$2"
}

say ""
say "Copying the program."
install_file "$HERE/ascent" ascent 755
install_file "$HERE/ascent-import" ascent-import 755
install_file "$HERE/$(basename "$0")" setup.sh 755
for f in README.txt LICENSE-PARALLAX.txt LICENSE-SDL3.txt; do
	[ ! -f "$HERE/$f" ] || install_file "$HERE/$f" "$f" 644
done
# content.ini with the address that the setup used
awk -v u="$BASE_URL" '/^base_url=/ { print "base_url=" u; next } { print }' "$HERE/content.ini" >"$TMPD/content.ini"
install_file "$TMPD/content.ini" content.ini 644

# ---------------------------------------------------------------------
# 6. Downloads

CONTENT=$PREFIX/content
DL_DIR=$CONTENT/download
rec tree "$CONTENT"

# download NAME: the pack NAME of the manifest to $DL_DIR/<file>, checked
# against the size and the SHA-256. Sets DL_FILE.
download() {
	pack_info "$1" || { warn "The content server has no valid entry for $1."; return 1; }
	mkdir -p -- "$DL_DIR"
	DL_FILE=$DL_DIR/$P_FILE
	part=$DL_FILE.part
	if [ -f "$DL_FILE" ] && [ "$(filesize "$DL_FILE")" -eq "$P_SIZE" ] && [ "$(sha256 "$DL_FILE")" = "$P_SHA" ]; then
		return 0
	fi
	rm -f -- "$DL_FILE"
	if [ -f "$part" ] && [ "$(filesize "$part")" -gt "$P_SIZE" ]; then
		rm -f -- "$part"
	fi
	free=$(free_kb "$DL_DIR")
	have=0
	[ ! -f "$part" ] || have=$(filesize "$part")
	if [ "$free" -lt $(((P_SIZE - have) / 1024 + 1024)) ]; then
		warn "Not enough free space for $P_TITLE: $(mb "$P_SIZE") MB."
		return 1
	fi
	say "Downloading $P_TITLE ($(mb "$P_SIZE") MB)."
	tries=0
	while :; do
		have=0
		[ ! -f "$part" ] || have=$(filesize "$part")
		if [ "$have" -lt "$P_SIZE" ]; then
			[ "$have" -eq 0 ] || say "  continuing at $(mb "$have") MB"
			if fetch "${BASE_URL}$P_FILE" "$part" 2>>"$TMPD/fetch.txt"; then :; else
				log "download of $P_FILE failed (try $((tries + 1)))"
			fi
		fi
		have=0
		[ ! -f "$part" ] || have=$(filesize "$part")
		[ "$have" -lt "$P_SIZE" ] || break
		tries=$((tries + 1))
		if [ "$tries" -ge 3 ]; then
			tail -n 3 "$TMPD/fetch.txt" >&2 || :
			warn "The download of $P_TITLE stopped at $(mb "$have") of $(mb "$P_SIZE") MB. Run the setup again to continue it."
			return 1
		fi
		sleep 2
	done
	if [ "$have" -ne "$P_SIZE" ] || [ "$(sha256 "$part")" != "$P_SHA" ]; then
		rm -f -- "$part"
		warn "The download of $P_TITLE is damaged (size or SHA-256 differs from the list of the server). It was deleted."
		return 1
	fi
	mv -f -- "$part" "$DL_FILE"
	log "downloaded $P_FILE $P_SIZE $P_SHA"
	return 0
}

# ---------------------------------------------------------------------
# 7. The data folder (ascent-import)

PATCH_ARG=
if [ "$SRC_PATCHED" = no ]; then
	if [ -n "$OPT_PATCH" ]; then
		PATCH_ARG=$(abspath "$OPT_PATCH")
	else
		download patch13 || die "The 1.3 patch cannot be downloaded."
		PATCH_ARG=$DL_FILE
	fi
fi

# progress: shows the PROGRESS lines of a tool and keeps the others in the log.
progress() {
	last=-1
	while IFS= read -r line; do
		case $line in
			"PROGRESS "*)
				rest=${line#PROGRESS }
				pct=${rest%% *}
				text=${rest#* }
				case $pct in ''|*[!0-9]*) continue ;; esac
				if [ -t 1 ]; then
					printf '\r  %3d%%  %-60.60s' "$pct" "$text"
				elif [ $((pct / 10)) -ne "$last" ]; then
					last=$((pct / 10))
					printf '  %3d%%  %s\n' "$pct" "$text"
				fi ;;
			"ERROR: "*)
				log "$line"
				printf '%s\n' "${line#ERROR: }" >"$TMPD/import-error.txt" ;;
			*) log "$line" ;;
		esac
	done
	[ ! -t 1 ] || printf '\n'
}

say ""
say "Making the data folder from the game disc."
DATA=$PREFIX/data
rec tree "$DATA"
set -- --source "$SOURCE" --dest "$DATA"
[ -z "$PATCH_ARG" ] || set -- "$@" --patch "$PATCH_ARG"
rm -f -- "$TMPD/import-error.txt"
{
	rc=0
	"$IMPORT" "$@" 2>&1 || rc=$?
	echo "$rc" >"$TMPD/import-rc"
} | progress
rc=$(cat "$TMPD/import-rc")
if [ "$rc" -ne 0 ]; then
	safe_rm_tree "$DATA" "$PREFIX"
	msg="ascent-import status $rc"
	[ ! -f "$TMPD/import-error.txt" ] || msg=$(cat "$TMPD/import-error.txt")
	die "The data folder cannot be made: $msg"
fi
if [ -n "$PATCH_ARG" ] && [ -z "$OPT_PATCH" ]; then
	rm -f -- "$PATCH_ARG"
fi
say "The data folder is ready."

# ---------------------------------------------------------------------
# 8. The content packs

HD=$PREFIX/hd
INSTALLED=$CONTENT/installed

# remove_pack_files NAME: removes the files of the installed pack NAME.
remove_pack_files() {
	st=$INSTALLED/$1.txt
	[ -f "$st" ] || return 0
	awk 'length($0) > 66 && substr($0, 65, 1) == " " {
		rest = substr($0, 66); sub(/^[0-9]+ /, "", rest); print rest }' "$st" >"$TMPD/old.txt"
	while IFS= read -r rel; do
		case $rel in /*|../*|*/../*) continue ;; esac
		rm -f -- "$HD/$rel"
	done <"$TMPD/old.txt"
	rm -f -- "$st"
}

# install_pack NAME: download, unpack into a staging folder, check each
# file, then move the files into hd and write content/installed/NAME.txt
# (the format of ascent-content). Fails without a half-installed pack.
install_pack() {
	pack_info "$1" || { warn "The content server has no valid entry for $1."; return 1; }
	if [ "$P_TARGET" != hd ]; then
		warn "The pack $1 has the target $P_TARGET; the setup installs only hd packs."
		return 1
	fi
	st=$INSTALLED/$1.txt
	if [ -f "$st" ] && grep -qx "version=$P_VERSION" "$st"; then
		say "$P_TITLE: version $P_VERSION is installed."
		return 0
	fi
	download "$1" || return 1
	zip=$DL_FILE
	title=$P_TITLE
	version=$P_VERSION
	say "Installing $title."

	# The names in the archive: no absolute path, no "..".
	if ! unzip -Z1 "$zip" >"$TMPD/entries.txt" 2>>"$LOG"; then
		rm -f -- "$zip"
		warn "The pack $1 is not a ZIP archive. It was deleted."
		return 1
	fi
	if grep -qE '^/|(^|/)\.\.(/|$)' "$TMPD/entries.txt" || grep -qF -- "$BS" "$TMPD/entries.txt"; then
		rm -f -- "$zip"
		warn "The pack $1 has a file name outside its folder. It was deleted."
		return 1
	fi
	STAGING=$CONTENT/staging-$1
	safe_rm_tree "$STAGING" "$CONTENT"
	mkdir -p -- "$STAGING"
	if ! unzip -qq -o "$zip" -d "$STAGING" 2>>"$LOG"; then
		safe_rm_tree "$STAGING" "$CONTENT"; STAGING=
		warn "The pack $1 cannot be unpacked (disk full?)."
		return 1
	fi
	S=$STAGING
	# readable for all users (a system install), whatever the archive says
	chmod -R u+rwX,go=rX -- "$S"
	if [ ! -f "$S/_files.txt" ]; then
		safe_rm_tree "$STAGING" "$CONTENT"; STAGING=
		rm -f -- "$zip"
		warn "The pack $1 has no file list."
		return 1
	fi
	tr -d '\r' <"$S/_files.txt" >"$TMPD/files.txt"
	: >"$TMPD/links.txt"
	[ ! -f "$S/_links.txt" ] || tr -d '\r' <"$S/_links.txt" >"$TMPD/links.txt"
	# the paths of the file list, and a list for sha256sum -c of the
	# stored files (the links are checked through their stored file)
	awk 'length($0) > 66 && substr($0, 65, 1) == " " {
		rest = substr($0, 66); sub(/^[0-9]+ /, "", rest); print rest }' "$TMPD/files.txt" >"$TMPD/paths.txt"
	if grep -qE '^/|(^|/)\.\.(/|$)' "$TMPD/paths.txt" "$TMPD/links.txt"; then
		safe_rm_tree "$STAGING" "$CONTENT"; STAGING=
		rm -f -- "$zip"
		warn "The file list of the pack $1 has a path outside its folder."
		return 1
	fi
	# every file of the archive must be in the file list
	grep -v '/$' "$TMPD/entries.txt" | grep -v '^_[a-z]*\.txt$' | sort >"$TMPD/e.sorted" || :
	sort "$TMPD/paths.txt" >"$TMPD/p.sorted"
	if [ -n "$(comm -23 "$TMPD/e.sorted" "$TMPD/p.sorted" | head -n 1)" ]; then
		safe_rm_tree "$STAGING" "$CONTENT"; STAGING=
		rm -f -- "$zip"
		warn "The pack $1 has files that are not in its file list."
		return 1
	fi
	# the links: each link and its stored file have the same SHA-256 in the list
	if ! awk -F'\t' '
		FILENAME == ARGV[1] { rest = substr($0, 66); sub(/^[0-9]+ /, "", rest); h[rest] = substr($0, 1, 64); next }
		NF >= 2 && (!($1 in h) || !($2 in h) || h[$1] != h[$2]) { bad = 1; exit }
		END { exit bad }' "$TMPD/files.txt" "$TMPD/links.txt"; then
		safe_rm_tree "$STAGING" "$CONTENT"; STAGING=
		rm -f -- "$zip"
		warn "The link list of the pack $1 does not match its file list."
		return 1
	fi
	awk -F'\t' '
		FILENAME == ARGV[1] { if (NF >= 2) link[$1] = 1; next }
		length($0) > 66 && substr($0, 65, 1) == " " {
			rest = substr($0, 66); sub(/^[0-9]+ /, "", rest)
			if (!(rest in link)) print substr($0, 1, 64) "  " rest }' "$TMPD/links.txt" "$TMPD/files.txt" >"$TMPD/check.sha"
	say "  checking the files"
	if ! (cd "$S" && sha256sum --quiet --strict -c "$TMPD/check.sha") >>"$LOG" 2>&1; then
		safe_rm_tree "$STAGING" "$CONTENT"; STAGING=
		rm -f -- "$zip"
		warn "Files of the pack $1 do not match its file list (see $LOG)."
		return 1
	fi
	# the duplicates: hard links (or copies) of a stored file
	if [ -s "$TMPD/links.txt" ]; then
		say "  making $(wc -l <"$TMPD/links.txt" | tr -d ' ') links"
		awk -F'\t' 'NF >= 2 { d = $1; if (sub(/\/[^\/]*$/, "", d)) print d }' "$TMPD/links.txt" | sort -u >"$TMPD/dirs.txt"
		while IFS= read -r d; do
			mkdir -p -- "$S/$d"
		done <"$TMPD/dirs.txt"
		while IFS=$TAB read -r a b; do
			[ -n "$b" ] || continue
			ln -f -- "$S/$b" "$S/$a" 2>/dev/null || cp -f -- "$S/$b" "$S/$a" || {
				safe_rm_tree "$STAGING" "$CONTENT"; STAGING=
				warn "Cannot make the files of the pack $1 (disk full?)."
				return 1
			}
		done <"$TMPD/links.txt"
	fi
	rm -f -- "$S/_files.txt" "$S/_links.txt" "$S/_pack.txt"
	# Move into place. An interruption here removes the files of this
	# pack again (cleanup).
	remove_pack_files "$1"
	mkdir -p -- "$HD" "$INSTALLED"
	rec tree "$HD"
	PACK_LIST=$TMPD/paths.txt
	PACK_DEST=$HD
	if ! cp -alf -- "$S/." "$HD/" 2>/dev/null; then
		if ! cp -af -- "$S/." "$HD/"; then
			while IFS= read -r rel; do rm -f -- "$HD/$rel"; done <"$TMPD/paths.txt"
			PACK_LIST=
			safe_rm_tree "$STAGING" "$CONTENT"; STAGING=
			warn "Cannot copy the files of the pack $1 (disk full?)."
			return 1
		fi
	fi
	{
		printf 'version=%s\ntarget=%s\ntitle=%s\n' "$version" hd "$title"
		cat "$TMPD/files.txt"
	} >"$st.new" && mv -f -- "$st.new" "$st"
	PACK_LIST=
	safe_rm_tree "$STAGING" "$CONTENT"
	STAGING=
	rm -f -- "$zip"
	rmdir -- "$DL_DIR" 2>/dev/null || :
	log "installed $1 version $version"
	say "  installed"
	return 0
}

FAILED=
ES_OK=no
if [ -n "$PACKS" ]; then
	say ""
	for p in $PACK_ORDER; do
		case " $PACKS " in *" $p "*) ;; *) continue ;; esac
		if install_pack "$p"; then
			[ "$p" != lang_es ] || ES_OK=yes
		else
			FAILED="$FAILED $p"
		fi
	done
fi
rmdir -- "$DL_DIR" 2>/dev/null || :

# The default language of the game: default.cfg next to the program. The
# game reads it before ascent.cfg of the player, so a language that the
# player chose in the game stays (dtuopts.c).
if [ "$GAME_LANG" = es ] && [ "$ES_OK" = yes ]; then
	printf 'Language=es\n' >"$PREFIX/default.cfg"
	rec file "$PREFIX/default.cfg"
else
	rm -f -- "$PREFIX/default.cfg"
	[ "$GAME_LANG" = en ] || warn "The Spanish text pack is not installed: the game starts in English."
fi

# ---------------------------------------------------------------------
# 9. The icon, the command and the menu entry

ICON=
ico=$(find "$DATA" -maxdepth 1 -iname dtu.ico | head -n 1)
if [ -n "$ico" ]; then
	png=$PREFIX/ascent-dtu.png
	rm -f -- "$png"
	if command -v icotool >/dev/null 2>&1; then
		# the second image has 256 colors
		icotool -x -i 2 -o "$png" "$ico" 2>/dev/null || icotool -x -i 1 -o "$png" "$ico" 2>/dev/null || :
	fi
	if [ ! -s "$png" ] && command -v magick >/dev/null 2>&1; then
		magick "${ico}[1]" "$png" 2>/dev/null || magick "${ico}[0]" "$png" 2>/dev/null || :
	fi
	if [ ! -s "$png" ] && command -v convert >/dev/null 2>&1; then
		convert "${ico}[1]" "$png" 2>/dev/null || convert "${ico}[0]" "$png" 2>/dev/null || :
	fi
	if [ -s "$png" ]; then
		ICON=$png
		rec file "$png"
	else
		rm -f -- "$png"
		say "No icon: the setup needs icotool (package icoutils) or ImageMagick to make it from DTU.ICO."
	fi
fi

# The launcher: the game starts in the folder of the user, where the
# screenshots go.
LAUNCHER=$PREFIX/ascent-dtu
# shellcheck disable=SC2016
{
	printf '#!/bin/sh\n# ASCENT-DTU (made by setup.sh)\n'
	printf 'dir=${XDG_DATA_HOME:-$HOME/.local/share}/ASCENT-DTU\n'
	printf 'mkdir -p "$dir" && cd "$dir" || exit 1\n'
	printf 'exec %s/ascent "$@"\n' "$(sq "$PREFIX")"
} >"$LAUNCHER.new"
chmod 755 "$LAUNCHER.new"
mv -f -- "$LAUNCHER.new" "$LAUNCHER"
rec file "$LAUNCHER"

LINK=$BIN_DIR/ascent-dtu
NEW_DIRS=$(missing_dirs "$BIN_DIR")
if { [ -e "$LINK" ] || [ -L "$LINK" ]; } && ! { [ -L "$LINK" ] && [ "$(readlink "$LINK")" = "$LAUNCHER" ]; }; then
	warn "$LINK exists and is not of this installation: it stays. Start the game with $LAUNCHER."
	LINK=
elif mkdir -p -- "$BIN_DIR" 2>/dev/null && ln -sf -- "$LAUNCHER" "$LINK" 2>/dev/null; then
	rec_dirs "$NEW_DIRS"
	rec file "$LINK"
else
	warn "Cannot make $LINK. Start the game with $LAUNCHER."
	LINK=
fi

DESKTOP=$APP_DIR/ascent-dtu.desktop
NEW_DIRS=$(missing_dirs "$APP_DIR")
if mkdir -p -- "$APP_DIR" 2>/dev/null; then
	rec_dirs "$NEW_DIRS"
	{
		printf '[Desktop Entry]\nType=Application\nName=ASCENT-DTU\n'
		printf 'GenericName=Role-playing game\n'
		printf 'Comment=Descent to Undermountain (source port)\n'
		printf 'Exec="%s"\n' "$LAUNCHER"
		[ -z "$ICON" ] || printf 'Icon=%s\n' "$ICON"
		printf 'Terminal=false\nCategories=Game;RolePlaying;\n'
	} >"$DESKTOP.new" && mv -f -- "$DESKTOP.new" "$DESKTOP" && rec file "$DESKTOP"
else
	warn "Cannot make the menu entry in $APP_DIR."
fi
INSTALLING=no

# ---------------------------------------------------------------------
# 10. Done

say ""
say "ASCENT-DTU is installed in $PREFIX."
if [ -n "$LINK" ]; then
	say "Start it with the command ascent-dtu, or from the menu of the desktop (Games)."
	case ":$PATH:" in
		*":$BIN_DIR:"*) ;;
		*) say "Note: $BIN_DIR is not in your PATH. Add it, or start the game with $LAUNCHER." ;;
	esac
fi
say "Saves and settings: $USERDIR"
[ -z "$FAILED" ] || warn "These packs are not installed:$FAILED. The game works without them. Run the setup again to retry."
say "To remove the game: $PREFIX/setup.sh --uninstall"
