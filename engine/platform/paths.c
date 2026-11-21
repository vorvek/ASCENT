/*
 * ASCENT platform layer: the directories of an installed game.
 *
 * This file is new. The DOS game used the current directory for all its
 * files. An installed port reads the game data next to the executable and
 * writes the files of the player in a directory of the user.
 */
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "platform.h"

// Copy a directory name without the separator at the end.
static int copy_dir(char *out, int size, const char *dir)
{
	int n;

	if (out == NULL || size <= 0)
		return 0;
	out[0] = 0;
	if (dir == NULL || dir[0] == 0 || (int) strlen(dir) >= size)
		return 0;
	strcpy(out, dir);
	n = (int) strlen(out);
	while (n > 1 && (out[n - 1] == '/' || out[n - 1] == '\\'))
		out[--n] = 0;
	return 1;
}

int plat_base_dir(char *out, int size)
{
	return copy_dir(out, size, SDL_GetBasePath());		// SDL owns the string
}

int plat_pref_dir(const char *app, char *out, int size)
{
	char *dir = SDL_GetPrefPath(NULL, app);	// creates the directory
	int ok = copy_dir(out, size, dir);

	SDL_free(dir);
	return ok;
}

void plat_message_box(const char *title, const char *text)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, NULL);
}
