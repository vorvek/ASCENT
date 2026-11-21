/*
 * ASCENT platform test: show the 256 colors of a PALETTE.256 file.
 *
 * Usage: test_window PALETTE.256 [number of frames]
 * Without a frame count, the program runs until the user closes the window.
 */
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"
#include "fix.h"

#define W 320
#define H 200

int main(int argc, char **argv)
{
	static uint8_t pal[768];
	static uint8_t frame[W * H];
	FILE *f;
	int x, y, frames = -1;

	if (argc < 2) {
		fprintf(stderr, "usage: test_window PALETTE.256 [frames]\n");
		return 2;
	}
	if (argc > 2)
		frames = atoi(argv[2]);

	f = fopen(argv[1], "rb");
	if (!f || fread(pal, 1, 768, f) != 768) {
		fprintf(stderr, "cannot read 768 palette bytes from %s\n", argv[1]);
		return 1;
	}
	fclose(f);

	// 16 by 16 grid of color cells.
	for (y = 0; y < H; y++)
		for (x = 0; x < W; x++)
			frame[y * W + x] = (uint8_t) ((y * 16 / H) * 16 + (x * 16 / W));

	if (plat_init("ASCENT platform test", W, H, 3) != 0) {
		fprintf(stderr, "plat_init failed\n");
		return 1;
	}
	while (plat_pump() && frames != 0) {
		plat_present(frame, W, H, W, pal);
		if (frames > 0)
			frames--;
	}
	printf("fixmul(1.5, 2.0) = %f\n", f2fl(fixmul(fl2f(1.5), i2f(2))));
	plat_shutdown();
	return 0;
}
