/*
 * ASCENT: new file. The script patches of the class gear (Dtu_class_gear,
 * dtugear.h, dtuclassgear.c). Not in DTUGAME.EXE 1.3.
 */
#ifndef _DTUCLASSGEAR_H
#define _DTUCLASSGEAR_H

#include "types.h"

// dtuscript.c, a record of GABE that is decoded: Mirt (script 2) and Nalbas
// (script 5). Returns the record (a new one when it changed; the old one is
// freed), *size its size. Other records stay as they are.
ubyte *dtu_class_gear_patch(int id, ubyte *p, int *size);
// dtuscript.c, patch_aleena: the offers of Aleena (script 4). orig: the size
// of the record of GABE before any patch (16710 for 1.3).
ubyte *dtu_class_gear_aleena(ubyte *p, int *size, int orig);
// The state that the patches read (the setting, the class gear lines): a
// record decoded with another state is decoded again (dtuscript.c).
int dtu_class_gear_key(void);
// dtufe_town.c, the price that the market pays: the most it pays for a
// full item that Aleena sells to a single class mage below the price of
// the market (her wands), or 0 for no limit. Without the limit the mage
// buys a wand from her and sells it to the market for more.
int dtu_class_gear_resale_cap(int type, int id);

#endif
