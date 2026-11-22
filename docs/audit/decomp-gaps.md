# Gaps against three decompilation notes (2026-11-22)

This check compares the port with three notes of the DTU 1.3 decompilation
project: the conversation input (`recovered/dialogue.md`), the HUD display
area calls (`analysis/hud-calls.md`) and the Blackstaff frame timing
(`recovered/town.md`). For each rule of a note the table gives the original
address, the place in the port and the status. The evidence is the note, its
C reconstruction in `recovered/src` and the Ghidra C of the function.

Status: **same**, **different** or **missing** (before this check), then the
commit of the fix. **N/A** marks a note item with no counterpart in the port:
engine plumbing such as queues and method tables, or code with no use in the
retail game.

## Conversation input (dialogue.md, dialogue.c)

| # | Item | Original | Port | Status | Evidence |
|---|---|---|---|---|---|
| D1 | A choice opens with the focus on the replies, selection -1 and no reply in the area | 0xB2280 | `dtu_dlg_start_choice` (dtudlg.c) | same (the area count since 6ba4143) | dialogue.c state; Dlg_first and Dlg_visible start at 0 |
| D2 | The input function follows the reply count: replies, no reply, text | 0xB2280, 0xB28F0, 0xB2B6F, 0xB2BED | `dtu_dlg_poll` | same | the tests of Dlg_n_replies and Dlg_focus_replies |
| D3 | Up and Down: keypad and extended codes; the selection stops at the first and the last reply; from -1 both give reply 1 | 0xB2988, 0xB2A1F | `dtu_dlg_poll` KEY_UP/KEY_PAD8, KEY_DOWN/KEY_PAD2 | same | dialogue.c cases 0x48/0xC8, 0x50/0xD0 |
| D4 | Enter with the focus on the replies closes with the selection, also with -1 (only possible in the first frame); 0xA7ADC then ends the run as for Escape | 0xB28F0, 0xB23C6, 0xA7ADC | `dtu_dlg_poll` | different: Enter with -1 did nothing. Fixed 6ba4143 (returns -2, which 0xA7ADC treats the same) | dialogue.md "Enter on the very first frame"; 000a7adc.c: any negative result with replies calls 0xA7A57 |
| D5 | A frame with no key and the focus on the replies makes the selection reply 1 | 0xB28F0 end, 0xB29A1 | `dtu_dlg_poll` | missing: the first reply was not selected until Up or Down. Fixed 6ba4143 | dialogue.c `keep_reply_visible` runs after every REPLIES frame |
| D6 | The reply area pages: a selection above the area starts the area there and adds later, then earlier replies; below, earlier first. Each reply takes its lines plus one; an exact fit fits; the selection always has its place | 0xB29A1, 0xB2ABC, 0x3D498C, 0x3D4984 | `keep_reply_visible`, `dtu_dlg_draw` | missing: the replies were drawn from reply 1 and a selection below the area was not shown. Fixed 6ba4143 | dialogue.c `extend_visible`; dialogue.md "Its reply drawing loop starts at reply zero" |
| D7 | No reply: Enter closes with 0 | 0xB2B6F | `dtu_dlg_poll` | same | dialogue.c case 0x1C, dispatch CONTINUE |
| D8 | Focus on the text: Enter only redraws, also with no reply | 0xB2BED | `dtu_dlg_poll` | different: with no reply Enter went on before the focus test. Fixed f1c7501 | dialogue.md "ASCENT also handles Enter on an empty reply list before checking focus" |
| D9 | No reply and the focus on the replies: Up and Down do nothing | 0xB2B6F | `dtu_dlg_poll` | different: they scrolled the text. Fixed f1c7501 | dialogue.c: Up/Down act only in REPLIES or TEXT |
| D10 | Focus on the text: Up and Down scroll one font height, clamped at the top and the end | 0xB2BED | `dtu_dlg_poll`, clamp in `dtu_dlg_draw` | same (one line of the game font) | dialogue.c `text_step`, `text_min` |
| D11 | Space and Tab move the focus in all three functions | 0xB2A47, 0xB2BB5, 0xB2CEB | `dtu_dlg_poll` | same | dialogue.c cases 0x0F, 0x39 |
| D12 | Escape closes with -2 in all three functions | 0xB2D01 | `dtu_dlg_poll` | same | dialogue.c case 0x01 |
| D13 | Backtick: a screenshot, then the redraw | 0xB297E, 0x80DA1 | `dtu_dlg_poll` KEY_LAPOSTRO | same | dialogue.c case 0x29 |
| D14 | The first mouse poll centers the cursor and installs the poller that keeps the cursor in the window | 0xB2433, 0xB249C | `mouse_poll` | different (port decision): the port has its own mouse for the window (header of dtudlg.c) and neither centers nor confines the cursor | dialogue.md table |
| D15 | Close clears the active flag; the wait loop ends on a result or on the flag | 0xB23C6, 0xB2FA2 | `chosen`, `dtu_script_frame` (dtuscript.c) | same | 000b2fa2.c |
| D16 | Redraw, reply list and "Press Enter..." renderers | 0xB2D0D, 0xB2D90, 0xB2E91 | `dtu_dlg_draw` | same (the list starts at the first reply of the area since 6ba4143) | dtudlg.c comments |
| D17 | No number keys | 0xB28F0 | `digit_reply` | different (port extension, documented in dtudlg.c) | dialogue.md "an explicit port extension" |
| D18 | 0xA7ADC: a negative result ends the run when there are replies, else the script goes on | 0xA7ADC, 0xA7A57 | dtuscript.c `pending_reply` | same | 000a7adc.c |
| D19 | Optional callback of the wait loop, always null | 0xB2FD9, 0xA7ADF | none | N/A (the port has no callback; same effect) | dialogue.md "Indirect calls" |
| D20 | Integer queues of SFX, speech and music; list release callback | 0xAEB04, 0xA13EA, 0x2F01F | engine/main audio | N/A (port audio has its own structures) | dialogue.md |
| D21 | Software cursor method slots and the idle stubs after shutdown | 0x1BD51C, 0xB0F92, 0xB0FEA | dtudisp.c cursor | N/A | dialogue.md |
| D22 | Editor buttons and keypad command bindings | 0xF8878, 0xFA910, 0x146C70 | none | N/A (no use in the retail game is known) | dialogue.md "Editor button provenance" |

## HUD display areas (hud-calls.md, hud.c)

| # | Item | Original | Port | Status | Evidence |
|---|---|---|---|---|---|
| H1 | Restore method +0x48: interface picture or clear color | 0x9C38F, 0x9C1F2 | `area_restore` (dtudisp.c) | same | hud.c `restore[]`; docs/re/ingame-ui.md |
| H2 | Print method +0x50 from the seven-entry table: icon, centered text, positioned text, hit points, attribute, portrait, experience | 0x144D48, 0x9BD8A, 0x9B95E, 0x9B761, 0x9B241, 0x9AF8E, 0x9BB7F, 0x9B503 | `area_icon`, `area_text`, `draw_main_areas`, `area_hit_points`, portrait blit, `draw_info_areas` | same for types 0, 1, 2, 3, 5, 6; type 4 (attribute) has no port function and no area of the port uses it | hud.c `print[]`; ingame-ui.md "4: not read" |
| H3 | The hit point area is always type 3 | 0x32542, 0x9B241 | `area_hit_points` | same | hud-calls.md |
| H4 | The name: type 1 (centered) in high resolution; low resolution type 2 (positioned) in mode 0, type 1 in modes 3, 5, 6 | 0x32624, 0x99810, 0x3263B | `draw_main_areas`, `draw_info_areas` | same | dtudisp.c "Low resolution prints it at x 1 (area type 2), high resolution centers it" |
| H5 | Hit method +0x54: half-open rectangle with signed 16-bit fields | 0x9C00B | `in_rect` | same | hud.c `dtu_hud_area_hit` |
| H6 | Overlay +0x4C and border +0x58 methods | 0x9C537, 0x9C890 | `area_border` | same for the border; the overlay method has no port use found | hud.c |
| H7 | Teardown: inactive methods, hit returns false | 0x9A7DB, 0x9C1D7 | none | N/A (the port draws and tests only the areas of the current mode) | hud.c `dtu_hud_area_unbind` |
| H8 | Mode 2: the absent hit point area is tested after a click | 0x2FF58, 0x300E7 | none | N/A: modes 1 and 2 are invalid (docs/re/ingame-ui.md) | hud-calls.md |
| H9 | High resolution mode 6: the hover loop of the ten absent hot key areas finds nothing | 0x313DA, 0x31412 | `hot_keys_visible` | same (no hot key areas in high resolution mode 6) | dtudisp.c `hot_keys_visible` |
| H10 | Areas of 0x5C bytes; ten inventory and ten hot key areas, four statistics arrays of four | 0x9ADAA, 0x992DB | dtudisp.c tables | same | docs/re/ingame-ui.md "An area is a record of 0x5C bytes" |
| H11 | Two tests use the method of a fixed statistics area with a variable receiver | 0x321B9, 0x32207 | not found | not verified: what the two tests decide was not traced | hud-calls.md |

## Blackstaff frame timing (town.md, town.c)

| # | Item | Original | Port | Status | Evidence |
|---|---|---|---|---|---|
| T1 | The frame callback draws the torches first and updates the mouth last | 0x830D4, 0x96263 | `bs_draw`, `bs_frame` (dtufe_town.c) | same in effect: the port updates in `bs_frame` and draws everything in `bs_draw` | town.c `dtu_town_tick` |
| T2 | Prepare the text; the start is 2 s ahead | 0x8313C | `bs_init` | same | town.c case START |
| T3 | At the start time the scroll begins; the first pixel waits for the next frame | 0x83278 | `bs_frame` | different: the pixel came from the elapsed time. Fixed 8ecc757 | 00083278.c |
| T4 | One pixel when due; the next 0x2800 after a fresh clock read; a late frame moves one pixel | 0x8329A, 0x13E90C | `bs_frame` | different: the pixel was (now - start) / 0x2800, so a late frame jumped. Fixed 8ecc757 | 0008329a.c |
| T5 | At the end of the text: SMusFade(100) in the same frame, then a hold of 10 s (0xA0000) | 0x8329A | `bs_frame` | different: the port faded after the hold. Fixed 8ecc757 | 0008329a.c calls SMusFade after the deadline store |
| T6 | After the hold: release the text, then the empty phase | 0x83386, 0x8339E, 0x83A75 | `bs_frame` state 3, `bs_draw` | same for the text; whether the release also clears the `~p` picture was not checked | town.c cases HOLD, CLEANUP |
| T7 | A deadline that overflows to a negative clock becomes the interval itself | 0x8329A | none | different, not done: the game clock of the port would need about 9 hours in the screen to reach it | town.c `deadline` |
| T8 | Null-phase fallback: the hidden animation of Khelben's face | 0x83A93 | none (dtufe_town.c header) | missing; the note finds no path to it in a normal visit | town.md |
| T9 | The latch 0x13E91D and the cursor band test (318, 205) for T8 | 0x81D08, 0x13E91D | none | missing with T8 | town.c `dtu_town_face_gate` |
| T10 | Each pixel step hides the cursor around the scroll blit when they overlap | 0x8329A, 0x81704, 0x8182A | none | N/A: the port draws the whole screen and the cursor each frame | 0008329a.c |

The port keeps one extension in this screen: a frame longer than 0.1 s
(a picture load, a paused window) moves the 2 s start, the pixel time, the
10 s hold and the mouth time forward by the rest (`BS_MAX_STEP`).

## Open items

| Item | Size | Note |
|---|---|---|
| T8, T9: Khelben's hidden face animation and its cursor latch | medium, 1 to 2 days | The note finds no normal path to it, and its colors on the screen are not verified. Needs runtime evidence of reachability first. |
| D14: centering and confining the cursor in the conversation window | small, half a day | Conflicts with the mouse of the port; only with `Dtu_fe_original_input`. |
| H11: the two fixed statistics area tests | small to trace | Trace 0x321B9 and 0x32207 before a change. |
| H2: area type 4 (attribute) | small | Only if an area of 1.3 is found that uses it. |
| T7: the negative deadline | trivial | No visible effect in practice. |
