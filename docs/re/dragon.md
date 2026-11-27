# The dragon of DROW2 (Tamaramagil)

Code: `engine/main/dtudragon.c`, `dtudragon.h`. Setting: the line "The dragon
Tamaramagil" of the group "Restored and new content" of the Improvements
menu (`Dtu_restored_dragon`, `RestoredDragon` in `ascent.cfg`), on by
default. The options `-origobj` and `-origtamaramagil` clear
`Dtu_fix_tamaramagil` for one run. The encounter is on when both are on.
Tests: the cases `dragon_*` of `tools/questtest.py`.

Section 1 has the facts of 1.3, checked in the files. The rest is the design
of the port. None of it is in DTUGAME.EXE.

## 1. What 1.3 has (checked)

Level 21 of `DTU.MSN` is DROW2 ("Temple of the Drow", 800 segments, the
limit of the visibility matrix).

- The creature type `Dragon` (type 1 of the tables of the five drow levels):
  `model=dragn2.pof`, `max_hit_points=120`, `thaco=9`, `armor_class=-4`,
  `score_value=8000`, `FIRE_IMMUNE`, `NO_SCARE`, `weapon_type=-37`,
  `attack_type=melee`. After the table fixup and the creature setup the game
  has 120 hit points, armor class -3, THAC0 7, 8000 experience (the log line
  `dragon: type 1 Dragon: ...` shows the values before the port changes them).
- No level file has an object of the type, and no code makes one.
- `dragn2.pof` (`DROW.EGG`): 48 submodels, 126 frames, one gun point on
  submodel 15 (the jaw), fire frames 56 and 72. The mesh of the stand frame
  is 286 wide (the wings), 92.5 high and 225 long. Checked with
  `tools/model.py`:
  - STAND 0 to 3: the wings are raised in a V.
  - WALK 4 to 19.
  - ATTACK 44 to 59: the wings go straight up (the mesh is 164 high in frame
    52) and the right front claw strikes forward in frame 56. The weapon box
    of the model (box 2) follows that claw.
  - ATTACK2 60 to 75: the head dips (62), rears up and back (64 to 66, the
    jaw 55 above the origin and pointing 45 degrees down), then lunges
    down and forward (68 to 72). A bite.
  - State 11, 108 to 123: she swings her body and whips the tail around to
    the front. The second weapon box (box 4) follows the tail tip. It is not
    a wing spread. The port does not use it.
  - DIE 84 to 95. FIDGET 96 to 107: the head looks left and right.
  - The rest pose (frame -1, no joint angles): the legs straight, the neck
    and the tail level, the wings folded flat. 62 high.
  - There is no sitting frame and no frame with fire.
  - The body center of the frames (the place of the object, 0x729C1) moves
    up to 34 between frames: -31.6 in STAND, -2.1 in the first frame of the
    attacks, because the box follows the tail.
  - The torso box has radius 70.8, the body radius of the rest pose is 0.
- Her room: x -90 to 90, z -197.5 to -7.5, floor -110, ceiling -45 (180 x 65
  x 190). The middle is segment 394. Each of its four doorways is 20 high.
  - South: the doorway of segment 473 (60 wide), the corridor 474, the door
    of segment 475 (walls 164 and 165), the corridor 477 to 481. At its end
    (segment 481) the east alcove 482 has the lever (trigger 20 on side 2;
    door walls 133 and 132, not locked) and the west alcove 483 is empty
    (door walls 131 and 130, locked; trigger 29 on the floor of 481 takes
    MISC 115, which no object has, and runs trap 15 on that door). Trigger
    22 (segment 480) is the sign "Use only as a last resort. Retribution
    could be devastating."
  - West: the hoard, segments 783 to 789, 782 and 409, behind the door of
    segment 449 side 4 (walls 136 and 137, flags 0x18, keys 1). Trigger 27
    (the floor of segment 448) takes MISC 42 (Dragon Key) and runs trap 13
    (the lock). Message 22: "You use the dragon's key to unlock the door."
    The objects 50 to 60 are its treasure: 3 diamonds, Plate Mail +2, Chain
    Mail +2, Bracers AC4, Leather +2, Bastard Sword +1, Mace +2, Dagger +2,
    Scimitar +1.
  - East: the room of segments 790 to 796, 347, 476 and 511 behind the door
    of segment 398 side 4 (walls 138 and 139, not locked). Trigger 25
    (segment 790), message 19: "Uh oh, baby dragons."
  - North: the cell room (door walls 134 and 135, the corridor 408 with
    trigger 26 and message 20, "victims for the dragon were held here"; the
    cells 399, 401, 407, 402, 779 to 781). It is empty.
  - The ceiling over segment 394 has a hatch (segment 445 side 1, opened by
    trap 12) to segment 446, 60 x 20 x 60, which has no other way out.
- The four hatchlings (objects 48, 49, 61, 63, type `hatchlng`) stand in her
  room (segments 430, 436, 471, 391), hostile. After the setup: 50 hit
  points, armor class 0, THAC0 7, 7000 experience each, mage level 4, flying.
  They cast Fireball, Lightning Bolt, Magic Missile, Melf's Acid Arrows, Hold
  Person, Fear and others.
- The lever, trigger 20: trap 12 (the hatch) and, by the level rule 0x5A855,
  script 213 of object 42. Script 213 narrates "You hear the flapping of
  wings coming from down the hall...", sets global flag 107 and sends the
  four hatchlings out of the level. Flag 107 is the release of the red
  dragons: the people of Waterdeep talk about the burned Castle Ward and the
  market doubles its prices (0x86F5F).
- Script 210 is her dialogue. She asks the player to throw the lever "in the
  corridor from whence you came" and promises the key of her hoard. After
  flag 107 she says "Why thank you!" and each reply makes her hostile.
  `visited(seg 783)` makes her say "YOU HAVE TOUCHED MY HOARD!" and turn
  hostile. No statement of the script gives the key, and no object of the
  game has MISC 42.
- The three table lines MISC 42, 43 and 44 are "Dragon Key". DROW2 uses 42.

## 2. The encounter of the port

### The model

She is drawn at 0.55 of the size of the file (`DR_SCALE`). At the full size
the model is larger than the room in each direction. The rule for the
scale: her head stays below the ceiling (65 above the floor) in each frame
that the port uses and is in the view of a player who stands in front of
her; the wings may pass through the ceiling. The frame that sets the limit
is the rear of ATTACK2 (the bite and the wind-up of the breath), head top
109.7 in the file: 0.59 would touch the ceiling.

Heights above the floor and the half width at 0.55 (`tools/model.py`):

| Frames | Head, lowest to highest | Wing top | Half width |
|---|---|---|---|
| STAND 0 to 3 (0 is the talk pose) | 11.7 to 19.8 | 50.9 | 79.3 |
| WALK 4 to 19 | to 26.7 | 57.6 | 79.0 |
| ATTACK 44 to 59 (claws) | 0.9 to 20.6 | 90.4 (frame 52) | 94.4 (frame 57) |
| ATTACK2 60 to 75 (bite, breath) | 3.2 to 60.3 (frames 64 to 66) | 69.2 (frame 63) | 86.6 |

What passes through the room at 0.55: the wing tops in the frames 48 to 54
of the claw attack (up to 25 above the ceiling, about a third of a second)
and in frame 63 of ATTACK2 (4); the wing tips in the frames 56 to 58 of the
claw attack (4 beyond a side wall when she stands in the middle and faces
north or south); the tail tip (97 behind her position, 107 in the rear)
when she faces east or west. The head and the body do not.

`Polyobj_scale_model` and `Polyobj_scale` (`polyobj.h`) scale the draw of
one model in both renderers; `dtudragon.c` scales the arrays of the model
(boxes, centers, radii, gun points) one time for a load. The lengths of the
code that depend on her size are written in the units of the file
(`DR_S()`).

Each frame gets one body center, 27.5 of the file (15.1) in front of the
origin of the model, so the model does not jump when the animation changes.
That point is her position and the axis of her collision cylinder: radius
29.4 (from the hips to the tip of the snout), from the floor to 33 above it.
The swing of the player and the use ray test a cylinder about the position
of an object (0x4F7D5), so the cylinder must stand there. The tail and the
wings are outside of it; the wings are above the head of the player.

The move of the player tests only the objects of the segments that it goes
through, and she is in the list of one segment, so in the first version the
player walked into her from most sides. Each frame a player whose axis is in
the cylinder is moved out of it (`keep_player_out`).

While she is peaceful she stands still in the first frame of STAND: her
head is 12 to 20 above the floor, at the eyes of the player (9.25), and
looks at a player in front of her. The model has no sitting frame, and its
rest pose (frame -1) has the head 19 to 27 above the floor, over the view
of a player who stands at her chest. When she turns hostile the STAND
animation runs.

Her name in the messages (the hit lines, the kill line, the lines of this
file) is "Tamaramagil" from the start (`DTU_DRAGON_NAME`), not the type name
"Red Dragon" of the text file. She says the name in her first speech.

### Her values

A red dragon of age 6 of AD&D 2nd edition: 90 hit points, armor class -5,
THAC0 7, 15000 experience, fire immune, magic resistance 35 percent.

- Magic resistance: a d100 roll against 35 for each spell that another
  object casts on her (`dtu_dragon_spell_resisted`, called by `apply_spell`
  before the effect). A roll of 35 or less: the spell has no effect, and the
  player reads "Tamaramagil resists NAME." Log: `dragon: magic resistance ...`.
- Two claws, 1d10 each, or one bite of 3d10. One melee action each 2 seconds
  (an attack of 1 second and a pause of 1 second). Each attack has its own
  attack roll. She bites in 30 percent of the actions in which both reach,
  and always when only the bite reaches.
- The reach is not measured from her position. The claws: from the weapon
  box of frame 56 (the right front claw), the radius of the box (6), the
  radius of the player (2.5) and 27.5 (38.25 in all). That is a player who
  touches her in the front half, to about 6 beyond the touch in front. The
  bite: from the tip of her snout in frame 72, 24.5. That is a player in
  front of her to about 12 beyond the touch.
- The breath: 12d10+6 of fire in a cone from her jaws (half angle 20
  degrees, 120 long), a save against breath (category 3) for half, then the
  fire protections of the player as for each fire damage (Resist Fire, also
  from the Ring of Coolness and the Potion of Fire Resistance: half;
  Protection From Fire: its pool). No limit of uses; 15 seconds from the end
  of one breath to the next one, and 4 seconds from the start of the fight
  to the first one. The 2nd edition rule is one breath each three rounds
  with no limit; the pause is the choice of the port for a fight in real
  time.
- The wind-up of the breath is 2 seconds: she roars, rears (the frames 60 to
  65 of ATTACK2), flames come from her jaws, and the player reads "Tamaramagil rears and draws a deep breath." She turns with the player for the first
  second. Then the aim is fixed at the place of the player, and the player
  has one second to leave the cone or the line of sight. The stream lasts 1
  second and hurts one time. It is drawn as 240 small flames in a second
  along the cone (the fire burst of a fire hit, vclip 11, and one fire ball
  of the Fireball missile, vclip 14, for each 8), each at most 6.0 in size, so
  that they overlap from her jaws to the front of the stream. The first
  version used the `weapon_vclip` of Burning Hands (108): that is the
  texture of the model `burn.pof`, a full square with no transparent rim,
  so each flame was a square. A wall between her jaws and the player stops it
  (the door frames of the four doorways are the cover of the room).
  The offset of a flame from the axis is a random point of a cube of
  0.6 times the width of the cone, each component squared with its sign
  (`flame_offset`), so the flames are denser near the axis. The version
  before put each flame at a unit vector, on a sphere around the axis
  point: from the front the stream was a ring of separate bursts with no
  fire in the middle. The offset takes the same three `rand()` calls as
  `make_random_vector`, so the rolls after the breath (the save) do not
  change. In the GPU renderer with the upscaled set of version 8 the
  flames have a soft edge (docs/hd_textures.md, "The fire flipbooks").

### Her AI

`dtu_dragon_ai_frame` runs in place of the AI of the game while she lives,
and the creature physics of the game does not move her. The death is the
death of the game.

- Peaceful: she rests and turns slowly to the player.
- Hostile: she turns to the player at 112 degrees in a second and walks
  toward the player at 16.5 in a second while the claws do not reach. Her
  position stays within 8 (x) and 14 (z) of the middle of the room, so she
  cannot leave it (each doorway is 20 high, her cylinder is 33 high and 59
  wide). From there her attacks reach about 55 (x) and 61 (z) of the 90 and
  95 from the middle to the walls; the breath reaches the rest. The player
  has 52 or more between her cylinder and a side wall.
- She does not turn or walk during an attack.
- An effect that holds her (Hold Monster, Sleep) stops her.
- Her state (the action, the time to the next breath) is not in a level
  state: after a load she starts with the choice of an action.

### The level

- She stands in the middle of her room, peaceful, with script 210 and the
  Dragon Key (MISC 42), which drops on the floor in front of her at her
  death. A thief can pick the lock of the door of the hoard, as in 1.3.
- The hatchlings start in the east room, the hatchery. Its door (walls 138
  and 139) is locked and no skill or spell opens it. Behind the locked door
  the AI of the game does not run for them. (With the AI of the game they do
  not sense the player through the closed door either: checked with
  `-dragonhatchai`, with and without Improved AI.)
- The door of the alcove of the lever (walls 133 and 132) is locked and no
  skill or spell opens it. Its key lies on the floor of cell 407 behind her:
  MISC 43, which gets the name "Jailer's Key" in each level. A player with
  the key at the end of the corridor (segment 481) opens the door.
- Her dialogue names the key: three strings of script 210 are replaced
  (`dtu_dragon_script_text`; the port catalog has the Spanish text).
- The lever: the door of the hatchery opens and stays open (no automatic
  close; the port opens it again if it is closed). She laughs at the player
  (a message of the level) and is hostile. She does not give the key. Global
  flag 107 is not set: the dragons do not leave, and the market keeps its
  prices. Script 213 does not run. The same happens when she fights already.
- The hatchlings fly to the four segments of her room where the level file
  has them and attack: two of them at one time while she lives
  (`DR_HATCH_AT_ONCE`), the next one when one dies, all of them when she is
  dead. A hatchling casts Fireball at level 6 (6d6, radius 40; 25 in a
  test); four of them and her breath at one time would leave no way to live.
- How the two are held to two:
  - Before the lever nothing reaches a hatchling. The door of the hatchery
    is locked with a lock that no thief skill and no spell opens (keys 4),
    no creature opens a locked door, a closed door stops sight, missiles and
    the area of a spell (a Fireball at the door hurts none of them), and the
    AI of the game does not run for them. If one is hurt all the same, the
    hatchery opens as by the lever and she attacks, with a line of her own.
  - After the lever the AI of the game does not run for a hatchling that
    waits. It is not helpless: when it is hurt, or when the player comes
    within 70 of it with a free line, it joins the fight at once, over the
    two. So the player who runs into the hatchery fights the two that wait
    as well.
  - The global flags 243 to 246 say which hatchling has left the hatchery.
    They are in the save game, so a save and a load, or a way out of the
    level and back, in the middle of the fight change nothing.
- The hatch of the ceiling stays closed. In 1.3 it is the way out for the
  hatchlings. Here they do not leave, the room above it has no other way
  out, and a flying caster in it would be out of the reach of a melee
  player.
- The door of her hoard: if it is not locked or it opens while she lives, is
  at peace and has a free line to the player, she says a line about the
  theft and is hostile.
- The treasure: one table in `dtudragon.c` (`Dr_treasure`) adds Plate Mail
  +3, a Girdle of Hill Giant Strength, a Ring of Coolness, 2 Potions of Fire
  Resistance, 7 diamonds and 7000 gold to the hoard. Six places of the table
  are free: with the setting "New gear" `dtu_gear_dragon_treasure` fills them
  in order with the six items of `Gear_dragon` (dtugear.c: the Frost Brand,
  the Cloak of Displacement, the Sash of the Magi, the Belt of Dwarvenkind,
  the Ring of Regeneration, the Plate Mail of Fire Resistance;
  docs/re/objects.md, "The treasury of Tamaramagil"). The Flame Tongue of
  the first version of the table is gone: one lies in a cell of the troll
  wing of DROW2 (objects.md, "Places and market").
- The key and the treasure are placed one time: at the load of a new level,
  or at the restore of a level state when global flag 241 is not set (a
  state of 1.3 or of an older port). Global flag 242: the lever is thrown.
  The scripts of 1.3 use the flags 1 to 160.
- A level state in which the lever is thrown already (global flag 107 of
  1.3, or trigger 20 with a fire count in a state of an older port): no door
  is locked, no key is placed, and the lever is not an event of the visit.
  The treasure is added.

### With the setting off

DROW2 is as 1.3 ships it: no dragon, the hatchlings in her room, the lever
opens the hatch and runs script 213. An object for her in a level state goes
away, and the locks of the alcove and of the hatchery are taken off. The key
and the added treasure of a level state stay.

## 3. Old and new text

| Place | 1.3 | Port |
|---|---|---|
| Script 210, offset 3390 | In the corridor from whence you came, there is a lever that will release my children. If you pull it, I shall be grateful. I shall give you the key to my horde, and let you have all my treasures. (...) | In the corridor from whence you came, there is a lever that will release my children. The drow keep it behind a locked door, and its key lies in the cells behind me, where they held the wretches they fed to me. Take the key, open that door and throw the lever, and I shall be grateful. I shall give you the key to my hoard, and let you have all my treasures. (...) |
| Script 210, offset 627 | You have not yet thrown the lever. Oh, that the fate of my loved ones (...) | You have not yet thrown the lever. Its door is locked, and the key lies in the cells behind me. Oh, that the fate of my loved ones (...) |
| Script 210, offset 945 | (...) could you not do a small favor for me? Find the lever to the cage that imprisons my offspring (...) | (...) could you not do a small favor for me? Take the key from the cells behind me, find the lever to the cage that imprisons my offspring (...) |
| The lever | Script 213: You hear the flapping of wings coming from down the hall...it quickly fades into silence. | A heavy door grinds open far down the hall, and a deep laugh rolls through the corridor: "Little fool! That was no cage, it was my hatchery, and you have opened it. The hoard stays mine. Come, my children, our dinner has let itself in!" |
| The door of the hoard | (none; script 210 after `visited(seg 783)`: YOU HAVE TOUCHED MY HOARD!) | Tamaramagil's roar shakes the hall: "THIEF! You dare open my hoard before my very eyes? My riches are MINE! You shall burn for this!" |
| The door of the alcove | (not locked) | You use the jailer's key to unlock the door. |

## 4. Limits

- The collision is one cylinder. The player walks through her tail and
  under her wing tips.
- She does not use the tail animation (state 11), and she has no wing
  buffet, no fear aura and no spells of the 2nd edition dragon.
- She breathes only at the player. Other creatures are not hurt.
- Her attacks hurt only the player, by a reach test of her own and not by
  the weapon test of the game.
- A player who shoots from the south corridor beyond 120 from her jaws is
  out of the reach of the breath; she cannot follow.
