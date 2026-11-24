# Creature behavior in DTUGAME.EXE (ai.c, brain.c, dialogue.c)

This document describes the creature AI of Descent to Undermountain 1.3 as the
executable shows it. It compares the code with the Descent 1 source
(`engine/main/ai.c`, `aistruct.h`, `robot.h`).

All addresses are linear addresses in `gamedata/re/dtu_flat.bin` (base 0x10000).
Numbers with `.0` are 16.16 fixed point values (30.0 is 0x1E0000).

Facts are marked as follows.

- Verified (code): read from the disassembly.
- Verified (data): checked against game files.
- Guess: a reading that the code suggests but does not prove.

Unless a line says "Guess", the statements below are "Verified (code)".

## Coverage

Covered in depth:

- `do_ai_frame` (0x13BF4 to 0x15FBB), every branch.
- The morale function (0x10ADF).
- `init_ai_object` (0x1038B), the animation step (0x11018), the melee test
  (0x1153A), `ai_fire` (0x11B06), `do_firing_stuff` (0x12916),
  `compute_vis_and_vec` (0x12BE8), `ai_do_actual_firing_stuff` (0x1396E), the
  alert function (0x160AB), the to-hit roll (0x2A7F3) and the damage dice
  (0x2AF5E).
- The monster table layout and the brain file (sections near the end).
- The script system in outline (last section).

Not covered:

- `aipath.c` (path creation and `ai_follow_path`, 0x16FD9 to 0x17FFF). Only the
  call sites are named.
- `ai_move_relative_to_player` (0x125B9) and the turn functions, beyond their
  arguments.
- The spell casting functions of creatures (0x92D99, 0x91FDC, 0x9002C).
- The damage function 0x29A9C beyond the calls named here.
- Boss and gate code (0x13407, 0x1374D). It is the Descent 1 code with new
  texts.

## Main differences from Descent 1

1. A creature has an activity byte (object +0x1CC). 0 is inactive, 2 is active.
   An inactive creature only plays idle animations. It becomes active when the
   brain matrix and a ray cast both say that it sees the player.
2. `player_is_visible_from_object` does not cast a ray. It returns the brain
   matrix bit for (creature segment, player segment). So `player_visibility` is
   0 or 1. The Descent 1 value 2 never occurs. The field of view of the monster
   table is passed to the function and ignored.
3. Each creature has a morale value. It is one of three numbers from the
   `$MONSTER_AI` line: the flee value, the attack value, or their mean. Nearly
   every attack decision tests "morale >= attack value".
4. Each creature has a hostile flag (object +0x1B0 bit 22). A creature that is
   not hostile never attacks. Scripts and the morale function set it.
5. The `Ai_transition_table` of Descent 1 is gone. Explicit code picks the goal
   state.
6. There is no `next_fire` timer, no `firing_wait` and no `rapidfire_count`.
   The attack animation paces attacks. A ranged creature fires on a fixed frame
   of the attack animation. A melee creature hits when its weapon polygon
   touches the player during the attack animation.
7. Melee hits use an AD&D to-hit roll (d20 against THAC0 minus armor class) and
   damage dice.
8. Dead creatures with `regeneration` get up again.
9. The time slicing by distance, the awareness decay, the door opener code and
   the mode switch are still the Descent 1 code with small changes.

## Globals

| Address | Size | Name (Descent 1 name where it exists) |
|---|---|---|
| 0x2B0614 | 479 * N | `Objects[]` |
| 0x13D324 | 4 | `Highest_object_index` |
| 0x38E344 | 242 * 40 | `Robot_info[]` |
| 0x13DA54 | 4 | `N_robot_types` |
| 0x153EB0 | 47 * N | `Ai_local_info[]` |
| 0x153A30 | 16 * 8 | `Ai_cloak_info[]`: fix last_time, vector last_position |
| 0x2206A4 | 536 * N | `Segments[]` |
| 0x13CD44 | 4 | `Highest_segment_index` |
| 0x30DF14 | 4 | `ConsoleObject` |
| 0x13B924 | 4 | `Player_num` |
| 0x13CD30 | 4 | `GameTime` |
| 0x13CD2C | 4 | `FrameTime` |
| 0x13CD34 | 4 | `FrameCount` |
| 0x13CDCA | 4 | `Difficulty_level` (0 to 4) |
| 0x167174 | 4 | `Overall_agitation` (0 to 100) |
| 0x167168 | 12 | `Believed_player_pos` |
| 0x16715C | 12 | `Hit_pos` |
| 0x142958 | 4 | `we_have_a_brain` |
| 0x13A820 | 4 | monster type that has the `rat=` keyword |
| 0x13A6B4 | 4 | attack switch. 0 stops all creature attacks |
| 0x13D3FF | 4 | `Player_is_dead` (guess) |
| 0x13D9E0 | 2 * 12 | first frame of each animation state. Entry N + 1 is the end of state N |
| 0x3880D0 | 4 * N | pointer to the animation record of a model |

Statics of `do_ai_frame`: `robptr` 0x1671B4, `objnum` 0x1671D4, `aip` 0x1671D0,
`ailp` 0x1671CC, `dist_to_player` 0x1671C8, `vec_to_player` 0x1671BC,
`vis_vec_pos` 0x167194, `gun_point` 0x1671A0, `player_visibility` 0x13A7B8,
`previous_visibility` 0x13A7BC, `visibility_and_vec_computed` 0x1671AC,
`obj_ref` 0x1671B0 (objnum XOR FrameCount), `circle_distance` 0x1671B8.

Player data is addressed with `Player_num * 0x1B37`. The addresses below are
those of player 0.

| Address | Size | Meaning |
|---|---|---|
| 0x1D3333 | 4 | object number of the player |
| 0x1D338D | 1 | current level number |
| 0x1D339F | 4 | a clock in 16.16 seconds, not a frame counter: 0x4D81A adds `FrameTime` each frame and wraps at 3600.0. The AI tests `(clock + objnum) % N == 0`, which is true in about one game frame of N. This document calls that "every Nth tick". The rate of these tests rises with the frame rate (see models.md, "Timing") |
| 0x1D33BC | 1 | flags. Bit 0: invisible. Bit 2: the creature cannot notice the player from behind (guess: sneaking) |
| 0x1D33C9 | 2 | armor class of the player |
| 0x1D33CB | 2 | compared with the creature level in the morale sum (guess: player level) |
| 0x1D3373 | 4 | start time of the attack animation of the player |

## Names in the executable

The debug texts give the DTU names.

| Table | Address | Entries |
|---|---|---|
| AI state (5 bytes each) | 0x13A73C | NONE, REST, SRCH, LOCK, FLIN, ATAK, RECO, DEAD |
| AI mode (9 bytes each) | 0x13A6D0 | STILL, WANDER, FOL_PATH, CHASE_OB, RUN_FROM, HOME, OPENDOOR |
| Behavior (9 bytes each) | 0x13A70F | GUARD, WANDER, HOME, PATROL, STATION |
| Animation state (pointers) | 0x13A764 | REST, STAND, WALK, SIDLE_R, SIDLE_L, RUN, ATTACK, ATTACK2, FLINCH, DIE, FIDGET |
| Unknown (pointers) | 0x13A7A0 | 360, BKWD, ARND, TWRD, AWAY |

The debug print at 0x127F3 uses them: "%2d Goal %s", "Cur %s", "Awr %d Morl %d",
"st %s fr %2d", "md %s bh %s". The table at 0x13A7A0 has no reference from the
AI code that was read. Its use is unknown.

### AI states (AIS_*)

| Value | DTU | Descent 1 |
|---|---|---|
| 0 | NONE | AIS_NONE |
| 1 | REST | AIS_REST |
| 2 | SRCH | AIS_SRCH |
| 3 | LOCK | AIS_LOCK |
| 4 | FLIN | AIS_FLIN |
| 5 | ATAK | AIS_FIRE |
| 6 | RECO | AIS_RECO |
| 7 | DEAD | AIS_ERR_ |

AIS_MAX is 8 (assert at 0x1581B).

### AI modes (AIM_*)

| Value | DTU | Descent 1 |
|---|---|---|
| 0 | STILL | AIM_STILL |
| 1 | WANDER | AIM_WANDER |
| 2 | FOL_PATH | AIM_FOLLOW_PATH |
| 3 | CHASE_OB | AIM_CHASE_OBJECT |
| 4 | RUN_FROM | AIM_RUN_FROM_OBJECT |
| 5 | HOME | AIM_HIDE |
| 6 | OPENDOOR | AIM_OPEN_DOOR (7 in Descent 1) |

Descent 1 mode 6 (AIM_FOLLOW_PATH_2) does not exist.

### Behaviors (AIB_*)

| Value | DTU | Start mode (0x10370) |
|---|---|---|
| 0x80 | GUARD | STILL |
| 0x81 | WANDER | STILL |
| 0x82 | HOME | STILL |
| 0x83 | PATROL | FOL_PATH |
| 0x84 | STATION | STILL |

A value outside 0x80 to 0x84 becomes 0x81 (0x1438B, 0x10492). A melee creature
with behavior STATION becomes WANDER at init (0x10622).

### Awareness

`ailp->player_awareness_type` has the values 0 to 5. The values 1 to 4 are
those of Descent 1. Value 5 is new: it forces a flinch (see "Goal state choice"). Scripts
use the same byte as an "anger" counter (see the script section).

## Structures

### ai_static (object +0x140)

The first 24 bytes are the AI part of the level record. The layout is the
Descent 1 `ai_static`.

| Object offset | Size | Field |
|---|---|---|
| +0x140 | 1 | behavior |
| +0x141 | 1 | flags[0] CURRENT_GUN |
| +0x142 | 1 | flags[1] CURRENT_STATE |
| +0x143 | 1 | flags[2] GOAL_STATE |
| +0x144 | 1 | flags[3] PATH_DIR |
| +0x145 | 1 | flags[4]. Set to 1 at init |
| +0x146 | 1 | flags[5] GOALSIDE (door side for mode OPENDOOR) |
| +0x147 | 1 | flags[6]. Tested by 0x16636 (to-hit penalty) |
| +0x148 to +0x14B | 4 | flags[7] to flags[10]. Init: 0, 0, 0xFF, not set |
| +0x14C | 2 | hide_segment (home or station segment) |
| +0x14E | 2 | hide_index |
| +0x150 | 2 | path_length |
| +0x152 | 2 | cur_path_index |
| +0x154 | 2 | follow_path_start_seg |
| +0x156 | 2 | follow_path_end_seg |
| +0x158 | 4 | danger_laser_signature |
| +0x15C | 2 | danger_laser_num |
| +0x15E | 4 | DTU flags. Zero at init. Must be nonzero for spell casting. Bits 2, 3, 4 of the low byte are set by the alert function. Bit 1 of +0x15F is set on wake up. Bit 6 of +0x15F is set on a failed morale check |

### Other object fields that the AI uses

| Offset | Size | Meaning |
|---|---|---|
| +0x0D | 1 | object flags. Bit 1: should be dead. Bit 6: not rendered and not processed |
| +0x6C | 4 | shields (hit points, fix) |
| +0x88 | 4 | status flags, see below |
| +0x8C | 4 | time of the last regeneration step |
| +0xFC | 12 | velocity |
| +0x136 | 4 | high word (+0x138): object that blocks the path, -1 = none. Guess |
| +0x13A | 2 | count of frames with a blocked path. Guess |
| +0x162 | 4 | model number, index of the animation record |
| +0x170 | 2 | animation state (0 to 10) |
| +0x172 | 4 | animation frame |
| +0x176 | 4 | goal animation state |
| +0x19E | 1 | mage spell points left |
| +0x19F | 1 | cleric spell points left |
| +0x1A2 | 4 | target object. 0 = the player, -1 = none |
| +0x1A8 | 4 | spell timer |
| +0x1AE | 1 | copy of the `pass_thru` byte of the monster table |
| +0x1B0 | 4 | script word, see below |
| +0x1C0 | 4 | spell timer |
| +0x1CC | 1 | activity: 0 = inactive, 2 = active |
| +0x1DB | 4 | a time stamp. Set to `GameTime + (256 - iq)` at init, wake up and each melee swing. 60.0 is added by "stands confused". The reader was not found |

Status flags at +0x88 that the AI reads. The names are guesses. The tests are
verified.

| Mask | Use in the AI |
|---|---|
| 0x00000004 | The creature cannot see. No wake up. Believed position is used in place of the real one |
| 0x00000010 | The creature notices nothing behind it |
| 0x00000020 | Confused. The morale function rolls a d10 first |
| 0x00000100 | Afraid. Morale value 0, and mode RUN_FROM in the goal state choice. Set by the damage function at 0x29F12 |
| 0x00000800 | Treated as flying for wake up and revive |
| 0x00002000, 0x08000000 | `+0x1DB` gets a second `256 - iq` |
| 0x00004000, 0x04000000 | Held. `do_ai_frame` returns before the animation step |
| 0x01000000 | Set at creation if `regeneration` is not 0. Needed for the revive code |
| 0x40000000 | Plus 1 to hit and plus 1 damage |

Script word at +0x1B0 (the script section has the full layout).

| Mask | Meaning in the AI |
|---|---|
| 0x000001FF | script id |
| 0x00000200 | peaceful. Never alerted. Set by "wanders" and "stands confused" |
| 0x00400000 | hostile |
| 0x00800000 | fleeing. Set by a failed morale check, cleared by a passed one |

### ai_local (0x153EB0, 47 bytes per object)

This is not the Descent 1 layout after offset 8.

| Offset | Size | Field |
|---|---|---|
| +0x00 | 1 | player_awareness_type (0 to 5) |
| +0x01 | 1 | retry_count. Nonzero means the last move was blocked |
| +0x02 | 1 | consecutive_retries (not seen in the code that was read) |
| +0x03 | 1 | mode |
| +0x04 | 1 | previous_visibility |
| +0x05 | 1 | flags. Bit 1: play the animation backward (revive). Bit 5: the melee hit landed, end the animation state early |
| +0x06 | 2 | goal_segment |
| +0x08 | 4 | set to GameTime on wake up. Use unknown |
| +0x10 | 4 | player_awareness_time |
| +0x14 | 4 | time_player_seen |
| +0x18 | 4 | time of the last attack sound (guess) |
| +0x1C | 4 | next_misc_sound_time (guess) |
| +0x20 | 4 | time_since_processed |
| +0x24 | 4 | start time of the current animation state |
| +0x28 | 4 | morale (integer) |
| +0x2C | 1 | signed. -1 at init. -2 makes a ranged creature use the melee reach test. A negative value selects the spell function 0x92D99, else 0x91FDC |
| +0x2D | 1 | "spell ready" flag. A spell caster goes to ATAK only if it is set. The code clears it then |

## init_ai_object (0x1038B)

Arguments: eax = object number, edx = behavior, ebx = hide segment.

1. The object must be of type 2 (robot). Else "RLH: trying to init a
   non-critter!".
2. Store the y position in the table at 0x30E2D4.
3. If the render type is 1: animation state STAND, first frame, blocker fields
   cleared.
4. Activity (+0x1CC) = 0. Mode = STILL. previous_visibility = 0.
5. Behavior 0 becomes WANDER. A behavior other than -1 is stored and gives the
   mode (table above).
6. Velocity = 0. CURRENT_STATE and GOAL_STATE come from the animation state
   (0x102B8).
7. Awareness = 0, awareness time = 3.0.
8. Morale = (flee value + attack value) / 2 for the difficulty level.
9. All time fields = GameTime. `+0x1DB` = GameTime + (256 - iq).
10. For GUARD, HOME, PATROL and STATION: hide_segment = the argument,
    goal_segment = the argument, hide_index = -1, cur_path_index = 0.
11. Spell points are copied from the monster table (+0xE4, +0xE5).
12. `ailp[0x2C]` = -1, `ailp[0x2D]` = 0.
13. A HUMANOID monster type gets the door opener bit (monster table +0x63 bit
    0). So all humanoids open doors.
14. STATION plus melee attack becomes WANDER.

State conversion helpers:

- 0x102B8, animation state to AI state: REST gives REST(1). WALK, SIDLE_R,
  SIDLE_L and RUN give SRCH(2) if hostile, else NONE. ATTACK and ATTACK2 give
  ATAK(5), or REST for a spell caster whose spell timers are not due. FLINCH
  gives FLIN(4). DIE gives DEAD(7). STAND and FIDGET give NONE.
- 0x10148, AI state to animation state: NONE, REST and RECO give STAND. SRCH
  gives WALK (STAND for STATION). LOCK gives RUN if the player is farther than
  30.0 and was seen in the last 0.5 seconds, else WALK (STAND for STATION).
  FLIN gives FLINCH. ATAK gives ATTACK or ATTACK2 at random (STAND for a spell
  caster whose timers are not due). DEAD gives DIE.

## do_ai_frame (0x13BF4)

Called from the object move function at 0x69981 for control type 1. Order of
operations:

1. Set `robptr`, `objnum`, `aip`, `ailp`. Set `player_visibility` = -1.
2. Return if the object has flag bit 6 at +0x0D, the monster has no
   `regeneration` and it is not the rat type.
3. If the animation state is DIE: activity = 2, go to step 8.
4. If the activity is not 2 (wake up test, 0x13CE0):
   - Assert `we_have_a_brain`.
   - The test runs only if the target (+0x1A2) is not -1, status bit 0x04 is
     clear, the player is not hidden (0x165C4) and the brain bit (creature
     segment, target segment) is set.
   - `dot` = (player position - creature position) dot forward vector.
   - If `dot` <= 0 (player behind): fail if player flag bit 2 or status bit
     0x10 is set. Else fail if `(tick + objnum + dot) % 255 > iq`.
   - Ray cast 0x10815 from the creature to the player. The start point is 5.0
     higher for a creature that does not fly. A free ray passes. A blocked ray
     passes only for a melee creature closer than 100.0 (for a spell caster
     only while its spell timers are not due).
   - On success: target = 0 if hostile, else -1. Activity = 2. Play
     `see_sound` with volume `str << 12`. Animation state STAND. If hostile:
     awareness 4, mode CHASE_OB, CURRENT_STATE REST, GOAL_STATE SRCH. Else
     awareness 0 and both states NONE. Call the alert function (0x160AB, level
     1). Call the morale function.
5. If the activity is still not 2 (0x13F7E):
   - Unless the control type is 11 or the creature dies: set y to the floor
     height of the segment. If the point is outside the mine, delete the
     object ("pushed critter %d through mine").
   - Idle animation: add 1 to the frame on every 10th tick (every frame for
     WALK and for states above SIDLE_L). At the end of the state pick the next
     one: FIDGET if `(FrameTime + objnum) % (iq >> (awareness + 1))` is 0, else
     STAND.
   - Note (code 0x14A54): step 20 puts each creature that is not active back to
     STAND and its first frame when it is processed. A creature nearer than
     100.0 is processed on each frame, so its idle animation never leaves frame
     0. The idle animation is seen only on creatures farther away, between two
     visits of step 19 (0.5 or 1.0 second).
6. In both cases (0x14134): reset the timers of `ailp` to GameTime, awareness
   time = 3.0, `+0x1DB` = GameTime + (256 - iq), danger laser = none, copy
   `pass_thru`. Then go to step 8.
7. If the activity is 2 (0x14206), on every 20th tick only: set the activity
   to 0 and repeat the test of step 4 against the player (brain bit, dot or
   player flag, ray cast). On success the activity is 2 again. If it stays 0,
   the creature is not dying and is not the rat type: both states NONE,
   animation state REST. So a creature that loses sight of the player goes to
   rest within 20 ticks.
8. Unless dying: call 0x9002C(object, 0) (spell effects on the creature, not
   read).
9. Fix an invalid behavior. Assert segnum and id.
10. Regeneration (0x1440C). Runs if `regeneration` is not 0 and status bit
    0x01000000 is set:
    - Every `regeneration` seconds add 1.0 to the shields while they are below
      `max_hit_points`.
    - When the shields are full, the DIE animation is on its last frame and
      the player is farther away than the sum of both radii: the creature
      revives. `ailp` flag bit 1 (backward animation), CURRENT_STATE REST,
      GOAL_STATE SRCH, animation state REST, activity 0, hostile flag set,
      fleeing flag clear, awareness 4, morale check, alert level 4, turn to
      face the player.
11. Danger laser: Descent 1 code (0x14655).
12. `time_since_processed` += FrameTime. Believed player position: the real
    position, or `Ai_cloak_info[objnum % 8]` if the player is hidden (0x165C4).
    `dist_to_player` is the distance to it.
13. Agitation path (0x1473A), Descent 1 code with new conditions: not dying,
    not STATION, mode STILL, morale >= attack value, target is the player,
    activity 2, `Overall_agitation` > 70, distance < 200.0, two random tests.
    Then `create_path_to_player(obj, 4 + agitation / 8 + difficulty, 1)`
    (0x170BC) and return.
14. Awareness decay (0x14825): Descent 1 code. Every 2.0 seconds the type
    drops by 1.
15. If awareness >= 1, the target is not -1 and the fleeing flag is clear:
    mode = CHASE_OB. If GOAL_STATE and CURRENT_STATE are both FLIN:
    GOAL_STATE = LOCK.
16. Return if a held bit (0x04004000) is set and the creature is not dying.
17. If one of the two AI states is not NONE: call the animation step
    (0x11018). Return if the creature is dying.
18. Spell casting (0x1490B): active, mage or cleric level above 0, object
    flags +0x0D bits 0 to 2 clear, `aip` flags at +0x15E not zero. If
    `ailp[0x2C]` is negative and the timer +0x1A8 is due: 0x92D99(object,
    distance). Else if the timer +0x1C0 is due: 0x91FDC(object).
19. Time slicing (0x1498B), Descent 1 code. With awareness below 3: return if
    the distance is above 150.0 and `time_since_processed` <= 1.0, or above
    100.0 and <= 0.5. A HOME or GUARD creature in mode FOL_PATH that is not in
    its home segment is always processed. The 250.0 tier of Descent 1 is gone.
    Then `time_since_processed = -((objnum & 3) * FrameTime) / 2`.
20. Inactive creature (0x14A1C; activity 0, not the rat type, not dying): both
    states NONE, animation state from 0x10148. Compute visibility. If visible
    now or before: turn toward the player with `turn_time`. Else turn randomly
    (0x10763). Return.
21. Door opener (0x14AF2), Descent 1 "brain" code for every creature with the
    door bit, unless the behavior is STATION or the mode is CHASE_OB. Mode
    STILL and player visible: path to a door (0x172DE, 8 + difficulty
    segments). Other modes: if the segment has a door that can open (0x132AA),
    mode = OPENDOOR and GOALSIDE = the side. Else, unless the mode is FOL_PATH,
    path to a door.
22. Blocked move (0x14BA1). Runs if `retry_count` is not 0, the animation
    state is WALK or RUN, the creature is not the rat type and the blocker
    field +0x138 is not 0. See "Blocked move" below. Then go to step 24.
23. Mode switch (0x15074, jump table 0x13BBC). See "Mode switch" below.
24. After the switch (0x157BF): compute visibility. If it is 2 (never true,
    see difference 2), awareness is 0 and the creature is hostile: awareness =
    3. Assert both states.
25. Goal state choice (0x15876). See "Goal state choice" below.
26. Return if GOAL_STATE is FLIN. Else switch on CURRENT_STATE (jump table
    0x13BD8). See "Current state switch" below.

### Blocked move (step 22)

- The blocker is the player, or the object in +0x138 if that is not negative.
- If the blocker has object type 18 and the path was blocked for 5 or more
  frames: turn toward it and set the animation state to ATTACK or ATTACK2 (by
  tick parity). If the type table entry (0x19B340 + id * 32) has bit 2, the
  object number goes to the list at 0x30F10C. Else, after 10 frames, call
  0xA6ACB(blocker, creature, 10.0) and set GOAL_STATE = SRCH. Guess: the
  creature breaks or damages the obstacle. Then move relative to the player
  with the circle distance of the table.
- Else by mode: RUN_FROM calls 0x123EF (move away). STILL does nothing. All
  other modes: if the player is not visible or the creature is inactive, set
  goal_segment to the believed player segment (0x2AA9A4), make a path
  (0x16FD9) and set mode FOL_PATH ("AI: %s looking for player!"). STATION does
  nothing here.
- Else pick a way around by `(tick + GameTime + objnum) % 100`: 0 = keep the
  direction, WALK. 1 = a random direction, WALK. 2 = move relative to the
  player with twice the circle distance. 3 = turn to the side, WALK. 4 =
  GOAL_STATE SRCH. 5 to 99 = side step: animation state SIDLE_R (3) or SIDLE_L
  (4), chosen by the side of the player. The velocity is set to the direction
  times the animation speed (0x74AA7). A creature that does not fly also gets
  this as thrust (+0x108).

### Mode switch (step 23)

`A` is the flee value and `B` is the attack value of the difficulty level.

- STILL (0x154BC). If (distance < 120.0 + 20.0 * difficulty, or awareness >=
  3) and the creature is hostile: compute visibility. If visible now or
  before, or a random test passes, and morale >= B: turn toward the player.
  If morale >= B: `do_firing_stuff`. If visible: a melee creature moves toward
  the player (0x125B9 with circle distance 0), a ranged one only evades. If
  not visible, not in its home segment, farther than 80.0 and the behavior is
  PATROL or HOME: path to the home segment (0x171F0, 15 segments), mode
  FOL_PATH. If the first condition fails: only turn toward the player when it
  is visible. So a peaceful creature looks at the player.
- WANDER (0x15096). Hostile: same as CHASE_OB. Not hostile: goal_segment =
  random segment, make a path (0x16FD9), follow it (0x17505).
- FOL_PATH (0x153B0). Compute visibility, `ai_follow_path`. Unless the
  behavior is PATROL: if morale >= B, GOAL_STATE = LOCK and `do_firing_stuff`.
- CHASE_OB (0x150DD). Circle distance = player radius for melee. For ranged it
  is the third `$MONSTER_AI` group plus the player radius plus `objnum & 15`
  (raw fix units, so nothing). Then the Descent 1 code:
  - visibility dropped from 2 and morale >= B: path to the player (8
    segments), GOAL_STATE SRCH. Never true, see difference 2.
  - not visible and farther than 80.0 (not STATION): HOME and GUARD go home
    (0x171F0), others get a 5 segment path (0x172DE). GOAL_STATE SRCH.
  - both states REST, visible, morale >= B and two random tests: GOAL_STATE
    SRCH ("Setting to search.").
  - player not seen for 8.0 seconds: if morale >= B, path to the player (10
    segments).
  - else if neither state is REST: move relative to the player (0x125B9). Then
    if morale >= B: `do_firing_stuff`. Else "Bad moralle (%d)": activity = 0
    and a new morale check.
- RUN_FROM (0x14D65). Compute visibility. Call 0x123EF(object,
  vec_to_player). The Descent 1 path following and bomb dropping are gone.
- HOME (0x1541A). Compute visibility, `ai_follow_path`. If morale >= B:
  GOAL_STATE = LOCK (a ranged creature keeps ATAK).
- OPENDOOR (0x156A5). Descent 1 code: turn and move to the center of the door
  side.
- Any other value: "Unknown mode = %i in NPC %i, behavior = %i". Then mode =
  CHASE_OB if morale >= B, or RUN_FROM if morale <= A.

### Goal state choice (step 25)

Runs only if awareness is not 0. It replaces `Ai_transition_table`.

1. Awareness 5: GOAL_STATE = FLIN (LOCK if CURRENT_STATE is FLIN already).
   Awareness becomes 2, awareness time 3.0.
2. CURRENT_STATE DEAD: GOAL_STATE = DEAD.
3. Status bit 0x100 set, or morale <= A: mode = RUN_FROM.
4. GOAL_STATE is REST, SRCH, LOCK or ATAK, and morale >= B:
   - melee (or `ailp[0x2C]` is -2): ATAK if distance <= creature radius +
     player radius + 2.0, else LOCK.
   - ranged without spell levels: ATAK.
   - spell caster: ATAK if `ailp[0x2D]` is set. The flag is cleared.
5. GOAL_STATE and CURRENT_STATE both FLIN: LOCK.
6. Both RECO and morale >= B: the same choice as in 4.

The same melee reach test (sum of radii + 2.0) is in `do_firing_stuff`,
`compute_vis_and_vec`, `ai_fire` and the current state switch.

### Current state switch (step 26)

- NONE (0x15B2F). Only if the segment of the creature is flagged in the table
  at 0x38C060 (index from 0x389A06, meaning not traced). Hostile: GOAL_STATE =
  SRCH. Else the idle animation of step 5.
- REST (0x15C56). If GOAL_STATE is REST, the player is visible and morale >=
  B: the choice of step 25 point 4 ("Went from rest to attack!"). Else, if not
  hostile: GOAL_STATE = NONE.
- SRCH (0x15DF0) and LOCK (0x15E6A). Visible and morale >= B: turn toward the
  player with `turn_time`. Else turn randomly.
- FLIN: nothing.
- ATAK (0x15ED4). Visible and morale >= B: turn toward the player. A ranged
  creature uses `turn_time[4]` (the fastest value), a melee one the value of
  the difficulty level. Else turn randomly. Then
  `ai_do_actual_firing_stuff`.
- RECO (0x15F9F). Every 4th frame (`obj_ref & 3`): same as SRCH.

## Sight

- 0x109E7 is `player_is_visible_from_object`. It returns the brain bit of
  (creature segment, player segment). Callers: 0x12CEA, 0x12D3F in
  `compute_vis_and_vec`.
- 0x109FE is the Descent 1 function with the ray cast. It has no caller.
- 0x165C4 "player is hidden from this creature". True if the player object
  has status bits 0x00030000 or player flag bit 0 (invisible), and the monster
  is not BLIND, and status bit 0x04 of the creature is clear, and (iq < 130 or
  creature level (0xAE3BB) < 10). So a BLIND creature and a smart creature of
  level 10 or more ignore invisibility.
- `compute_vis_and_vec` (0x12BE8). If the player is hidden or the creature has
  status bit 0x04: every 2.0 seconds move the believed position of
  `Ai_cloak_info` by a random vector, and use it. Else use the real position.
  Then call 0x109E7. Then store previous_visibility. If the result is not 0:
  time_player_seen = GameTime. The block for result 2 (goal state, attack
  sound, Descent 1 "see sound" timers) cannot run.
- `do_firing_stuff` (0x12916). Needs visibility >= 1. `dot` = forward vector
  dot vec_to_player. If `dot` >= 0.75 or the player is hidden, and GOAL_STATE
  is NONE, REST, SRCH, LOCK or ATAK: a creature that is not hostile gets
  GOAL_STATE NONE. A hostile one gets the choice of step 25 point 4, and
  awareness at least 1 with time 3.0. If `dot` >= 0.5 and GOAL_STATE is NONE,
  REST or SRCH: LOCK if hostile, else NONE. Descent 1 uses 7/8 in place of
  0.75.

Guess: because the shipped BRN files are copies of one matrix (see
`docs/formats/brn.md`), sight in the original game is wrong in most levels.

## Alert function (0x160AB)

Arguments: eax = object, edx = level (1 to 5). Called on wake up (1), on fire
(1), on revive (4) and by scripts (5).

1. Descent 1 `create_awareness_event`: add the event (0x16055). A random test
   adds 1 to `Overall_agitation`, limit 100.
2. If the level is 3 or more, for each other robot that is not active, not
   peaceful (bit 9) and not fleeing: if the brain bit (robot segment, caller
   segment) is set, a random test against `iq` passes and the ray cast 0x10815
   is free (a robot with status bit 0x10 must also face the caller): turn the
   robot to the caller, set its hostile flag, call 0xA6505 and set bit 3 or 4 of
   its +0x15E for level 3 or 4.

The function 0x16366 is the Descent 1 awareness spread (segment awareness to
objects). It raises the awareness of hostile creatures only, sets the time to
3.0 and calls the morale function for each raised creature.

Verified (code, 2026-11-22), details of 0x160AB:

- The loop runs from object 1 while the index is below
  `Highest_object_index` (the last object is never alerted). The tests:
  type robot, activity (+0x1CC) not 2, +0x1B0 bit 0x200 (peaceful) and bit
  0x800000 (fleeing) clear, `0x9810D(robot segment, caller segment)`.
- 0x161BC: `rand() % vm_vec_dist_quick(robot, caller) <= iq << 16`. rand()
  (0xD7AC4) is at most 0x7FFF, so this always passes.
- 0x10815(robot, 0): the ray from the robot to the player (not to the
  caller). Then the vector caller - robot; a robot with effect bit 4
  (deafness, +0x88 bit 0x10) needs a positive dot product with its forward
  vector. 0xD74C3 and 0x6B088 turn the robot to the caller.
- Hostile bit set, then 0xA6505(robot, sound) with sound = (a player field
  at 0x1D339F + index) % 2: every other robot plays its see sound. Levels 2,
  3 and 4 set bit 2, 3 or 4 of +0x15E (no reader found).

The callers and their level (`mov edx, level` before `call 0x160AB`):

| Call | Level | Caller object | Event |
|---|---|---|---|
| 0x11EBC | 1 | the creature | end of `ai_fire` |
| 0x13F04 | 1 | the creature | wake up in `do_ai_frame` |
| 0x7DBF0, 0x7DE67 | 1 | the player | `wall_open_door`, `wall_close_door` |
| 0x26147, 0x26199 | 2 | the player | the player runs into scenery fast (0x25FB0) |
| 0x26541 | 2 | the missile | a missile hits scenery (0x2641C) |
| 0x26693 | 2 | the player | the player hits a wall fast (0x265C1) |
| 0x26F78 | 2 | the missile | a missile hits a wall (0x26C2F) |
| 0x9347C | 2 | the burst | 0x93294 |
| 0x2707F | 3 | the creature | the player runs into a hostile creature (`collide_robot_and_player`) |
| 0x274BC | 3 | the creature | a missile of the player hits it (`collide_robot_and_weapon`) |
| 0x27B44 | 3 | the player | the melee hit of the player (0x278B5) |
| 0x9E712 | 3 | the creature | the player bumps a hostile creature (`player_physics`) |
| 0x14607 | 4 | the creature | the revive |
| 0x6A45F | 4 | the body | a body becomes a corpse (`remove_level_object`) |
| 0xA94A6 | 4 | the creature | a script makes it hostile (0xA9418) |
| 0x3E2C6 | 5 | the player | a rest (0x3E1F6) |
| 0xAB085 | 5 | the object | the script statement 0xC5 (set) with flag 3 (fleeing) |

### Awareness events (0x16055, 0x16304, 0x162A1, 0x16366), verified (code)

- 0x16055(obj, type): for type 2, 3 or 4 first 0x15FBC (the believed
  positions of the 8 `Ai_cloak_info` entries become the position of the
  player, with the time GameTime). Then, if fewer than 64 events (count
  0x13A6BC), the event gets the segment (+0x0E), the position (+0x14) and
  the type (16 bytes each at 0x153AB0). It returns 1, and `ai_alert` adds 1
  to the agitation 0x167174 (limit 100) when `rand() * (level + 4) >> 15`
  is above 4.
- 0x16366 (each frame, from 0x438AC): 0x16304 clears New_awareness (bytes at
  0x166DD0) of all segments and calls 0x162A1(event segment, type, 1) for
  each event, then sets the count to 0. Then each object with control type
  AI and the hostile bit whose awareness is below New_awareness of its
  segment gets that awareness, the time 3.0 (`Ai_local_info+0x10`) and
  `ai_morale`.
- 0x162A1(seg, type, depth): New_awareness[seg] = type if it is higher; if
  depth <= 5, the same for each child (>= 0) of the 6 sides with depth + 1
  and type 5 made 4. No visited set: one event makes up to 6^5 calls. The
  event reaches the segments up to 5 steps away, through any wall.

The port: `dtu_creature_alert`, `awareness_event` and
`awareness_events_frame` (dtuobj_aware.c) give the same values with a search by
distance. The agitation, the bits of +0x15E and the believed positions
(0x15FBC, see `compute_vis_and_vec`) are not in the port. The alerts of
0x25FB0, 0x265C1 and 0x93294 have no call in the port.

Corrections (verified in the bytes 2026-11-23, docs/audit/defects-1.3.md
D-112 to D-114; -origobj clears them):

- `Dtu_fix_noise_walls`: the noise passes only a side that a body can pass
  or that can be seen through (`WALL_IS_DOORWAY` with the fly or the render
  past flag). A closed door stops it.
- `Dtu_fix_alert_ray`: the ray of the wake test goes from the creature to
  the caller, not to the player.
- `Dtu_fix_alert_last`: the wake loop includes the last object slot.
- The roll 0x161BC is not in the port (D-040): it always passes in 1.3, and
  AD&D 2nd Edition alerts a creature that notices a fight.

### Regeneration and revive

See step 10 of `do_ai_frame`. The damage function clears all effects of a
creature that dies (0x2A1D0; spell damage 0x916D9), so the bit 0x01000000
is gone and the revive never runs in 1.3. The kill gives no experience while
the bit is set: the test before 0x4EA50 runs before the clear.
`remove_level_object` (0x6A3CC) also has a branch
for a body with the bit (goal state 2, no corpse), which the same clear
makes dead code. The port corrects this (`Dtu_fix_revive`, dtuobj.h,
`corpse_regen_frame`): the bit stays unless fire or acid took it, and the
corpse gets up with the values of step 10.

## How a creature moves

Verified (code), added with the port.

- 0x11EC9 (object, direction): the direction with y = 0 (not for FLYING) is
  made a unit vector and `direction * speed` is added to the velocity
  (0xD6FA2 adds). The speed is 0x74AA7 for the current animation state, so a
  creature moves only in the states WALK, SIDLE and RUN.
- `pmon.c` (0x9F286) multiplies the velocity by 0.5 at the start of each
  frame of an active creature. With the add of the AI on each frame the
  velocity goes to the speed of the state: v' = (v + add) / 2. The time to
  reach it is a number of frames, not a time.
- 0x11F32 `move_towards_player`: nothing in CURRENT_STATE ATAK (but for a
  FLYING melee creature). Else the forward vector becomes (direction x, old
  forward y, direction z) at once, with no turn rate, then 0x11EC9.
- 0x12547 `move_away_from_player`: nothing in CURRENT_STATE ATAK or with
  `ailp` flag 0x10. Else 0x11EC9 with the direction negated, and the animation
  frame is set to the last frame of STAND.
- 0x125B9 is `ai_move_relative_to_player` of Descent 1 (object, ailp, distance,
  vec_to_player, circle distance, evade only). Not for STATION and not for a
  held creature. Below the circle distance or with flag 0x10: move away, set
  flag 0x10. Below two times the distance or with flag 4: move around
  (0x12000), set flag 4. Else move toward, set flag 8. The flags are cleared at
  the end of an animation state.
- 0x106BA `ai_turn_towards_vector` is the Descent 1 function, but the new
  forward vector keeps the y part of the old one.

## Port status (engine/main/dtuobj.c, dtuhook.c)

In the port: the wake up test, the idle animation, the animation step, all
modes (STILL, WANDER, FOL_PATH, CHASE_OB, RUN_FROM, HOME, OPENDOOR), the goal
state choice, the current state switch, the morale check (without the level
values), paths (0x1686D and its callers), the door opener and the door that a
creature opens when it walks into it (0x25CAA), the blocked move with the
attack on scenery, the random turn, the weapon test, the to-hit roll, the
damage dice, ranged attacks (0x11B06, 0x1396E) with missiles, the special
attack of a monster with a weapon_type below 0 (a spell missile, 0x10040),
the damage of a spell (0x91BC3), regeneration of hit points, the death with
drops, gold and experience, the corpse, the sounds of a creature, the trap
hooks and the death of the player.

In the port since the item and thief work: the spells of a monster with a
mage or cleric level (0x92D99, 0x9231F, 0x91FDC, dtuspell.c, combat.md B10),
the push and the break of scenery (below), the NPC table (0xA7E97, below),
Hide In Shadows and Move Silently in the sight tests (0x165C4, 0x13D8E).

Not in the port: the evasion of weapons, the believed position of a hidden
player (`compute_vis_and_vec` 0x12BE8, 0x15FBC). In the port since the
monster audit (docs/audit/monsters.md): the alert function and the awareness
events, the revive of a regenerating creature (`Dtu_fix_revive`), the touch
spell of the lich.

Rules of the port that are not in the executable (marked "guess" in the
code): a hostile creature that sees the player keeps the awareness 1 at least
(the awareness events are missing), a creature that the player hits becomes
hostile, a dead player stays in place, a creature breaks the scenery that it
attacks (0xA6ACB is not read).

### Paths (0x1686D), verified

The segment filter at 0x16999 to 0x16AEE marks a segment as used (no path
goes through it), but for the start and the end segment:

- a segment with `special` 1 or 2;
- for a robot whose monster entry does not have the door bit (+0x63 bit 0):
  each segment with a wall on any side;
- a branch for an object without flag 0x08 at +0x89 and with a value at
  +0x11C, not a FLYING monster: a segment whose byte at +0x214 has bit 0
  clear, whose vector at +0x204 is not zero (vm_vec_mag 0xD707D), and whose
  value at +0x208 is not -1.5. In the level files the byte at +0x214 is 1
  exactly where the value at +0x208 is -1.5, and the vector is (0, 0, 0)
  elsewhere (docs/formats/lvl.md), so this branch never marks a segment.

So a creature with no door bit takes no path through a segment that has a
wall of any type, an open one too. The loop runs to `Highest_segment_index`
minus 1, so the last segment is never marked.

### Push of scenery (0x9FA41), verified, in the port

0x9FA41 is not a function: it is a block inside the creature move 0x9F153
(pmon.c), with a second copy at 0x9FC29. The player has its own copy at
0x9E5BE (pcons.c). **Code.**

- Condition: the creature `str` (Robot_info +0xDE, 0x38E422) is above
  `pushable` of the scenery entry. For the player the strength word P+0xB7
  (0x1D33D3) is compared. `pushable` is Dressing_info +0x18 (0x19B354), the
  default of the reader is 999. (Correction: earlier versions of this note
  said +0x14; that offset is relative to 0x19B340.)
- Effect: scenery velocity x and z (+0xFC, +0x104) = 8 times the velocity of
  the mover, thrust x and z (+0x108, +0x110) = 8 times its thrust; vel.y
  stays; movement type +0x0B = 1. The mover goes back to its start position
  (0x30F478). Then `collide_two_objects` 0x29455. No mass is used and the
  push does not break the scenery.
- The player (0x9E53D to 0x9E766), verified: the push happens at each hit.
  The counter 0x1452F1 (hits of the object 0x1452F5 in a row, cleared by a
  frame whose move is free, 0x9E423) only decides whether the position goes
  back to the start (below 9 hits, 0x9E598); the position of a try is the
  start anyway, so the count changes nothing. (Correction: an earlier
  version of this note said the push stops after 9 hits.) The move loop goes
  on with the next try (up to 8, section 4.5 of physics.md) and the object is
  not put into the ignore list: scenery stops the player. The player thrust
  is 0 at that time (pcons.c clears it before the move), so the scenery gets
  no thrust. The ignore list gets the object only after more than 30 hits of
  the same object in a row (0x14534F, object 0x14534B, 0x9E11A) and when the
  end point of the try is farther from it than the start, so that a body
  inside of it can get out. 0x9E54B counts a hit only when `rad2` of the
  body (0x30F4DC) is not below `hit_pnt.y` (a compare of a size with a
  coordinate). A frame with no object hit clears the count (0x9E797).
- The velocity of the push: in 0x9FE68 the velocity is halved and x, z are
  shifted right by 2 before the move, so in the frame of the push the
  scenery moves with the velocity of the mover. Then thrust and drag
  (0x6CB68 with the thrust as the acceleration, no mass) with thrust 0 and
  the drag of the object (0 for the scenery of the level files), so the
  velocity goes down by 8 times in each frame.
- The move of the scenery (0x9FE68) is not free: see objects.md, "Pushed
  scenery", for its object test, which stops a chair inside of the body of
  the player, and for the frame rate.
- The move of scenery with movement type 1 is 0x9FE68, each frame: the
  velocity is halved, x and z are shifted right by 2, gravity 0x6C662, then
  fvi. Movement type goes back to 0 on fvi result 3, or on result 1 with the
  flag 0x13D448 & 0xA. Sound 2706 is linked to the object while the speed is
  above 0x70. The slow down counts frames (frame rate dependent).
- `pushable` values of the tables: 1, 4, 6, 9, 12, 13, 15, 16, 90, 100,
  9999 (bench, chair, table, crate, barrel, bed). **Data.**

### Scenery that breaks (0xA6ACB, 0x6D4EC, 0x14CA9), verified

- The unbreakable flag is bit 0x04 of Dressing_info +0x04 (0x19B340 + 32
  id). The reader sets only `magic` (1) and `animates` (2); `cant_break` is
  matched and ignored (0x229FC). Bit 0x04 is set only by the level fixups
  at 0xAD3AF, 0xAD3CE and 0xAD431 (these also set `pushable` to 0x7FFF). So
  every other scenery object can break. **Code.**
- A melee weapon test (0x6CFF8, player or creature) that touches breakable
  scenery: the hit points are cut to 20.0, then 0x29A9C(obj, no attacker,
  100.0) takes 100.0 and 0xA6ACB(obj, 0, 10.0) breaks it (0x6D4EC to
  0x6D564). One hit breaks it. **Code.**
- A creature whose way is blocked by breakable scenery for 10 frames
  (+0x13A >= 10) in the attack states 6 or 7 calls 0xA6ACB(blocker,
  creature, 10.0) and sets ailp+3 = 2 (0x14CA9). An unbreakable one goes to
  the ignore list 0x30F10C. **Code.**
- 0xA6ACB for scenery: returns for a dead object (flags & 7) and for bit
  0x04; `explode_sound` (+0x0A) at volume 1.0 if it is not -1; nine
  explosions of `break_vclip` (+0x1C, default 137) at the size of the
  object, each moved by x, z += rand() % 11.0 - 5.0 and y += rand() % 0.6875
  - 0.3125; rand() (0xD7AC4) is at most 0x7FFF, so the modulo does nothing
  and the explosions are 5.0 to 4.5 to the side (a defect of 1.3); every
  type 18 object of the segment gets movement type 1; then 0x6A3CC removes
  the object (no broken model, no debris) and `object_create_egg` drops what
  it contains. Two level special cases: level 0x20 object 3 (0xA6786) and
  level 0 object 0x29 (a fire trap: "You set off a fire trap!!!", spell 16
  twice through 0x90CEF). **Code.**
- A missile that hits scenery (0x2641C) does no weapon damage. It casts its
  `PC_spell` on the scenery (0x90CEF for an area spell, else 0x8CFFF), and
  the spell cases decide (combat.md B3). **Code.**

The port: `dtu_break_dressing()` and the push in `dtuobj_scenery.c` (settings
`Dtu_fix_cant_break`, `Dtu_fix_push_body`, `Dtu_fix_push_rate` and
`Dtu_fix_push_wall`, see combat.md C3 and objects.md, "Pushed scenery"). The move
of the player (`dtu_player_physics_sim`, physics.c) stops at scenery and
keeps the count of 0x14534F (the port counts each hit). Setup 1.0 of the
port let the player go through the scenery after the hit and stopped the
push after 9 hits; at frame rates above 21 the push frame often had no slow
down, so the scenery moved with 8 times the speed of the player. Measured
(YPINN, chair obj 5 and table obj 3, strength 17, walk of 0.25 s into the
chair): see physics.md section 10. Of the two
level special cases the port has only the end of the game of level 0x20
object 3: the credits movie (movies.md), without the light and the sounds.

### Creature melee status effects (verified: none)

Version 1.3 has no poison, paralysis, level drain, sleep or disease in a
creature melee hit. The only hook is 0x27D82, which casts the `enemy_spell`
of the weapon (+0x3C) before the to-hit roll; all 276 `enemy_spell=`
entries of the 85 decoded tables are -1, and 0x27D82 passes the arguments
swapped (0x8CFFF(spell, attacker, player)). Robot_info has no poison or
paralyze flag, and the strings "poison", "paralyze", "drain" and "disease"
do not exist ("Energy drain" at 0xC0B83 is a level editor menu item).
Paralysis, sleep and fear on the player come only from spells (Hold bit 14,
Sleep bit 26, Fear bit 8), which creatures with a mage or cleric level cast
(see the section on creature spells in combat.md, B10). **Code.**

### Spell damage (0x91BC3), verified

Arguments: edx = target, ebx = spell, ecx = level. Four groups of dice of the
spell entry (Spell_info, 127 bytes): sides, number and the add for each
level.

| Bytes | Keywords |
|---|---|
| +0x3A, +0x3B, +0x3C | `damage_dice`, `#damage_dice`, `damage_level` |
| +0x3D, +0x3E, +0x3F | `fire_damage_dice`, `#fire_damage_dice`, `fire_damage_level` |
| +0x40, +0x41, +0x42 | `cold_damage_dice`, `#cold_damage_dice`, `cold_damage_level` (not used by 0x91BC3) |
| +0x43, +0x44, +0x45 | `acid_damage_dice`, `#acid_damage_dice`, `acid_damage_level` |
| +0x46, +0x47, +0x48 | `elec_damage_dice`, `#elec_damage_dice`, `elec_damage_level` |

(The offsets are from the stores of the reader at 0x24E87 to 0x25049. The
table starts at 0x1B5676, so the bytes of entry 0 are at 0x1B56B0 to
0x1B56BE.)

A group counts if its sides or its number is not 0: `number` rolls of
`rand % sides + 1`, plus `add * level`. Fire damage sets bit 0 of target
+0x15E. Then: a monster with `electricity_immune` takes no electric damage, a
monster with `fire_immune` no fire damage, one with `fire_resistant` one half.
The effect Resist Fire (bit 10 of +0x88) halves the fire damage. Protection
From Fire (bit 9) takes the fire damage from the counter +0x82 and only what
is left over goes through. Fire or acid damage clears bit 24 of +0x88
(regeneration). The result is the sum of the four groups. The two callers in
the spell function (0x91374, 0x917A0) take one half of it if the saving throw
0xAE596(target, 4) passes, and subtract it from the shields.

The port: `dtu_spell_damage_roll()`. `dtu_spell_hit()` (weak) applies it with
no saving throw when a spell missile hits. Which spells of the spell function
take this path is not read (guess: each spell with damage dice).

### Monster table defaults

The reader of the port used 0 where the reader of the executable has a
default (see the entry layout): `max_hit_points` 24, `thaco` 5, `iq` 70, `str`
10. A monster line with no `max_hit_points` (the mephits of MYRKUL) then had
no morale check, and the creature stayed inactive. The keyword `thac0` is now
read too. Fixed in `bmread.c`.

### Settings

Settings of the port (globals of `dtuobj.h`, 1 is the corrected rule and the
default, the option `-origobj` clears them):

| Setting | Rule of the executable | Corrected rule |
|---|---|---|
| `Dtu_fix_idle_anim` | one idle step (0x14013) per game frame | one step per 3/64 second |
| `Dtu_fix_ai_ticks` | the tests "(clock + objnum) % N == 0" are true in one game frame of N | true one time in N periods of 3/64 second. The test with 255 for a player behind the creature is a random number |
| `Dtu_fix_creature_move` | velocity * 0.5 per frame, two steps of 1/64 second at most, lift out of the floor 2.0 per frame, blocked counters per frame | the same per 3/64 second, all steps |
| `Dtu_fix_brain` | sight by the BRN matrix | sight by a ray (the BRN files of the disc fit no level) |
| `Dtu_fix_path_follow` | a walking creature on a path walks to the player (0x17CA8) | it walks to the next point of the path. A hostile creature in CHASE_OB that does not see the player takes a path to the player |
| `Dtu_fix_fire_once` | one missile in each frame that the fire frame lasts (0x13A2A) | one missile for each attack animation |
| `Dtu_fix_attack_sound` | the attack sound of compute_vis_and_vec needs visibility 2, which never comes | the sound plays by the rules of that code |
| `Dtu_fix_corpse_floor` | the corpse goes to the floor by the torso box (0x6A48B), a flyer has no gravity in DIE | the dying body falls, the corpse lies with its lowest drawn point on the floor, also in a loaded level. See "The corpse" |

| `Dtu_fix_revive` | the death clears the regeneration bit (0x2A1D0), so a regenerating creature never gets up | the bit stays unless fire or acid took it, and the corpse gets up (0x1440C); option `-origrevive` |

`Dtu_idle_anim_near` (default 0, option `-idleanim`) leaves out the reset of
step 20. `Dtu_creatures_attack` (default 1, option `-noattack`) is the attack
switch 0x13A6B4.

Test options: `-objlog FILE` writes each change of the state of a creature,
each attack roll, each damage, each missile, each path and each pickup with
the game time and the frame number. `-objlogframes` adds a line for each
animation frame. `-objlogpos N` adds the place, the mode and the path of each
active creature each N frames. `-testpos X,Y,Z[,HEADING]` puts the player at
a point of the level. The action `attack` of `-testmove` is the attack key.

### Measured

`-level BANE -testpos 25,-1,205,0`, the player standing, 16 seconds, at 20,
60 and 200 frames per second (before the paths): the skeleton sees the player
at 0.02 s in all three runs, walks at 1.07, 1.07 and 1.06 s, starts its first
attack at 3.17, 3.15 and 3.13 s, swings at 3.82, 3.80 and 3.76 s, and then
swings each 1.65, 1.65 and 1.62 s. All creatures together hit 19, 22 and 25
times for 104, 100 and 101 hit points.

`-level BANE -testpos -260.4,17.8,99.2,0`, 30 seconds at 20, 60 and 200
frames per second: the archer (obj 7) fires at 1.57, 1.53, 1.50 s and then
each 1.65 s. The first melee hit is at 4.32, 4.32, 3.98 s. The player dies two
times in each run. The creatures hit 11 times in each run for 44, 39 and 41
hit points. In the same place with `-noattack`, the axe skeleton (obj 8)
takes a path of 5 segments around the wall, attacks and breaks a bench in its
way at 4.5 s and reaches the player at 9 s.

`-level CRYPTS -testpos -45,5,195,270`: the orc Orcascim (door bit) takes a
path of 7 segments, enters the mode OPENDOOR and opens the closed door (wall
28, seg 40 side 4) at 8.2 s. Two orcs reach the player.

`-level MYRKUL -testpos 145.9,-23.5,256.9,180`: the mephits cast Burning
Hands (spell 4, 1d3 fire plus 2 per level) as a missile. Each hit does 4.

`-level BONES -testpos -20,-5,0,90`: trigger 1 runs trap 0, which gates a
skeleton (type 0) in segment 1. It kills the player in 6.1 s. The death
music plays 3 s later and the level starts again at 13.3 s.

### Improved pathfinding (Dtu_improved_pathfinding)

The setting of `dtuopts.h`, on by default. Off (or `-origobj`, or the test
option `-noimprovedpaths`, which changes nothing else) gives the rules above.
The code is in `dtuobj.c`. The creatures of the target game do not use
`aipath.c` or `ai.c`.

Causes found with `-objlogpos` (Code for the executable, the rest measured in
the port):

- The move of a creature (0x9E88D) stops the body for the frame at a wall hit
  and takes 1.75 times the velocity into the wall away. A body that walks into
  the corner of an opening stays at the corner. The top side is tested at 1.5
  times the half height above the center (0x3F58C), so a tall body cannot
  enter a corridor that it fits in (armOgre: 28.7 above the floor).
- The path goes to the center of each side, also through the top side of a
  segment (a hole in the ceiling), which a walking creature cannot use.
- A body stops at scenery (a table) and waits for the blocked move.
- With no sight and a distance above 80.0, CHASE_OB makes a random path on
  each frame and goes back to CHASE_OB on the next frame: the creature never
  moves (0x150DD with the awareness rule of 0x1487A).
- The test of 0x14206 fails at random when the player is behind the
  creature, so a creature on a path around a wall stops for one animation.

Changes:

- Path points: each opening of the path has a point in front of it and one
  behind it, on the line from the center of one segment to the center of the
  next, at the torso radius plus 0.5 from the opening (at most 0.4 of the way
  to the segment center). A later point (three at most) is taken when the line
  to it is free for the torso radius, at the creature height and to the point.
  No progress of 1.0 in 1.5 seconds: back to the point in front of the
  opening, then a new path without the segment behind it, after three new
  paths the path is given up. A walking creature takes no path through a top
  side.
- Move: the body slides along a wall (it moves to the hit point and goes on
  with the rest of the frame time), the top side is tested at the half
  height, and the body goes around scenery.
- A hostile creature that sees the player but has no free line for its body
  takes the path. With no sight above 80.0 it takes a path to the player, or
  walks along the random path. A creature on a path to the segment of the
  player stays active.
- Body: a hostile melee creature that sees the player stops at
  `creature_stop_dist()`: the reach of the weapon minus 1.0, at most the
  attack distance of `choose_attack_goal()` (size + player size + 2.0) minus
  1.0, at least torso radius + 2.5. The reach is the largest distance in front
  of the creature at which the weapon test of `weapon_test()` (frames 8 to the
  end of ATTACK and ATTACK2, test position 1.414 forward, segment from the
  weapon sphere outward by its radius) touches the player cylinder of radius
  2.5. The hit test does not change. Values (reach / stop): Ghoul 10.12 /
  9.12, Ghast 10.83 / 9.83, Zombie 8.93 / 7.93, ske_axe 10.96 / 9.96, ske_sci
  14.60 / 12.76, gskelsci 15.13 / 12.75, Banegard 13.22 / 12.22, GiantRat 10.19
  / 9.19, rmfg5 14.59 / 13.16, hmfg1 13.64 / 12.64, dmfw1 11.80 / 10.80,
  gorcaaxe 16.88 / 14.45, gorcasci and Orcascim 22.36 / 16.46, armOgre and
  Ogre2 28.52 / 20.25, Gargoyle 7.59 / 6.59, blackbat 7.39 / 6.39. Ranged
  creatures do not stop.
- Death: now `Dtu_fix_corpse_floor`, not this setting. See the corpse below.

The corpse (0x6A48B, Code): 0x6C30E gives d, the distance from the bottom of
the torso box of the frame (torso_half_height of the frame) to the floor. A
negative d lifts the body by -d, else the body goes down by d plus
torso_radius[0] (the first stack value that 0x748D9 fills). On the last frame
of DIE 0x748D9 gives a half height of 0.1. A creature that flies (blackbat,
Gargoyle) has no gravity in DIE, and 0x6C30E finds no floor below a body high
in the air, so its corpse stays where it died (port measured, rules of the
executable: 19.4 to 35.4 above the floor).

The drawn body is not the box of the file. The drawing takes body_center of
the frame away from the position (models.md, draw_polygon_object step 7), and
in DIE the body box of the file keeps the feet of the first frame of DIE
(verified with `tools/model.py` on `ghu_y.pof` of BAT.EGG, joint angles of
each frame applied):

| Ghoul frame | drawn points y | box1 y of the file |
|---|---|---|
| 84 (first of DIE) | -6.16 to 6.26 | -6.16 to 6.26 |
| 90 | -1.97 to 3.03 | -6.16 to 3.03 |
| 95 (last of DIE) | -0.55 to 2.53 | -6.16 to 2.53 |

For the blackbat (`bat_l.pof`) frame 95 is -1.15 to 2.30, the box -2.30 to
2.30. body_center.y of the last frame: Ghoul -1.81, blackbat 0.00. So the
lowest drawn point of the dead Ghoul is 1.26 above its position, not 6.16
below it.

By the code above the executable puts the dead Ghoul (torso_radius[0] 2.86)
at the floor plus 0.1 minus 2.86, so it is drawn from 1.5 below the floor to
1.6 above it (computed, not seen in the executable). An earlier rule of the
port (Dtu_improved_pathfinding) put box1_min.y of the last frame on the
floor: the dead Ghoul was drawn 7.42 above the floor, the blackbat 1.15. With
the setting off the port used body_half_height (4.35): the Ghoul 5.6 above
the floor.

`Dtu_fix_corpse_floor` (on, `-origobj` clears it): the dying body falls (also
a flyer), with the depth of the lowest drawn point of its frame as the half
height of the gravity, and at the end of DIE the lowest drawn point lies on
the highest floor under the center of the drawn body and four points of it
(at 0.6 of the half size), not more than 3.0 above the floor under the
position (a body on a step). The box of the drawn points comes from
`polyobj_frame_points_box()` (polyobj.c). The position stays at least 0.25
above the floor (in the segment); a body whose lowest point is higher than
that is drawn lower by the rest (`corpse_draw_drop()`). Only y changes, and a
position outside the segments is refused, so the body goes into no wall. A
corpse of a loaded level (an SG file of the executable or of the port) is put
on the floor the same way at the setup of the level.

Test options: `-testkill N[,1]` kills the active creature nearest to the
player each N frames (with 1: only one that hit a wall in the last 0.5 s) and
logs the corpse height above the floor. The `-objlogpos` lines have the
animation, sight, blocker and the wall, top, floor and object hits. A dying body has a `dying:` line with the height of its lowest drawn
point, and a corpse of a loaded level a `loaded dead:` line.

Measured (`-noattack`, 30 s at 60 frames per second, time to the attack
position, off and on):

- DROW1 `-testpos 139.8,21,219.4`: gorcaaxe obj 7 at the opening from seg
  146 to 155 (corner seg 146 side 0, at 129,19,203): 9.2 s and 4.2 s.
  gorcaaxe obj 8: not in 30 s and 10.2 s.
- DROW1 `-testpos 235.4,0,326.9`: gorcasci obj 57 and 58, blocked by a
  table (obj 54) in seg 249: not in 30 s, and 4.2 s.
- MYRKUL `-testpos -81.8,-26,188.9`: ske_axe obj 65 at a table in seg 310:
  13.7 s and 3.0 s.
- MYRKUL `-testpos 115.5,-25,359.5`: Gargoyle obj 25 at the low opening of
  seg 27 (top side): not in 30 s and 5.5 s.
- MYRKUL `-testpos 89,-26,303.1`: ske_axe obj 67 took a path up through the
  ceiling of seg 8 to seg 43: stuck 14.5 s and 5.0 s.
- 14 places in BANE, CRYPTS, DROW1, MYRKUL: 802 and 427 seconds of creatures
  that walk but do not move, 32 and 36 creatures at the player.
- The same DROW1 fight at 20, 60, 200 frames per second: the three gorcaaxe
  are at 16.0 from the player at 4.47, 4.50, 4.65 s (obj 6), 4.47, 4.25, 4.28
  s (obj 7), 10.46, 10.25, 10.19 s (obj 8), and all stop at 14.6.
- Smallest distance of melee creatures that attack: BANE ske_sci 6.9 and
  12.8, Ghast 6.5 and 9.8, Zombie 5.7 and 8.0, DROW1 rmfg5 8.3 and 13.7. With
  attacks: BANE 4 swings, 1 hit off, 3 swings, 2 hits on; DROW1 2 and 2 off,
  5 and 4 on.
- 19 deaths at walls (`-testkill 30,1`): -1.32 to -3.07 off, and 24 deaths
  0.00 to 0.09 on, with two bodies on a step 0.44 and 0.86. Flying: 19.4 to
  35.4 off, 0.01 to 0.04 on. These heights are of the box of the file, see the
  corpse above.
- `Dtu_fix_corpse_floor`, drawn heights (`-testkill 120 -objlogpos 3`, BAT,
  30 frames per second): 3 Ghouls and 5 blackbats 0.00 at the end of DIE
  (before the fix: Ghoul 7.42, blackbat 1.15). A blackbat killed 7.2 above
  the floor is at 0.3 after 0.2 s. The Ghoul body during DIE stays within
  2.2 of the floor. The save of the report (BAT, 3 Ghouls and 7 blackbats
  dead): 5.91 and 1.15 before the setup of the level, 0.00 after; with
  `-origobj` the Ghoul stays 7.42 above the floor.

Open: the stops in BANE are closed doors (no path for a creature without the
door bit, as in the executable). The armOgre of CRYPTS (height 23) does not
fit through the opening from seg 87 to seg 83 and walks back and forth. A
creature without a path still walks straight at a wall.

#### Corners, doors and ladders (2026-11-24)

The rules of the executable (Code):

- Doors: only a type with the door bit opens a door (0x25CAA, 0x1320F), and
  the bit comes from `door_opening=1` or the flag word `humanoid` of the
  monster line (0x1061B). **Data**: in the tables of THIEF1, BONES, BAT,
  CRYPTS, DROW1, KOBOLDS, BANE and MYRKUL only orcs, goblins, kobolds, Ogre2,
  `gbloser` and `Lich3` have `humanoid`. The human thieves of THIEF1
  (`shmtd2`, `sfmtd2`, ...), the drow of DROW1 and `hmfg1` of CRYPTS have no
  door bit, as the skeletons, ghouls, rats and Banegards. The door must have
  no key (`keys` 1) and the side that the creature passes or hits must not
  have the lock flag 8 (`WALL_DOOR_LOCKED`); the state is closed for 0x25CAA.
  A key door, a door with a lock (picked with 0xB3400) and a door that a
  trap locks (TAP toggle lock) stay closed. The lock flag is on each wall,
  so a door locked on one side only (THIEF1 wall 23, seg 21 side 4; wall 22
  on the other side is not locked) opens from the other side, for the
  player too (wall.c tests the side that the player hits). The port keeps
  this for the types with the door bit.
- Ladders: a creature does not climb. The ladder code 0x9EBEB
  (`pcons_push_fall`) is called from `player_physics` and from the fall
  damage 0x6C8C8 only for `ConsoleObject`, and the ladder textures
  (0x13A86B) are read only there and at the setup.
- A creature that cannot reach the player has no rule of its own: CHASE_OB
  moves it at the player (0x125B9), the blocked move (0x14BA1) steps to the
  side, and the paths of 0x150DD are made again. A melee creature under a
  player on a ladder walks under the player and stands there.

Causes found (`-objlogpos`, `-noattack`, 30 frames per second):

- The slide of the move took the hit point of fvi also when it was behind
  the start: at the edge of a side (the corner of an opening) fvi gives the
  point where the sphere first touches the edge, which is behind a body that
  touches the edge already. The body went back to it and forward again on
  each frame (BONES, `-testpos -50,-4,320`: the Rat obj 12 at the opening
  from seg 194 to seg 196, 0.3 back and forth for 16 s). The slide also took
  the normal of the face, which a body at an edge does not move off.
- `body_line_free()` tested the walls with the torso radius (1.0 at least),
  the move with a fourth of the cylinder plus 1.0. For a Rat (cylinder 0.93)
  the line was 1.06 wide and the move 1.23, so the line was free where the
  move hit the corner.
- A hostile creature with no door bit got no path through a segment with an
  open door, an illusion wall or a closed wall on any side (the filter of
  0x16999). In BAT, THIEF1 and CRYPTS most rooms have one, so the creature
  walked straight at a wall (THIEF1 `-testpos -64.3,5.9,542.7`: `shmtd6` obj
  94 behind the illusion wall of seg 620, 16.5 s at the wall; BAT
  `-testpos -85,-69,50`: the three Banegards, 12 s each).
- A body in a shallow pit (BONES seg 53, 2.5 under the floor of seg 43) had
  no path out (no path goes up through a top side) and no step over the rim
  (the lift of 0x6C186 is for a floor, the rim is a wall of the pit).
- A body that walks head on into scenery lost all of its speed in the slide
  around the scenery, which cleared the retry count, so the blocked move
  (0x14CA9) did not attack the scenery (CLAN1 `-testpos -200,-4,150`: the
  Zombie obj 93 at the bench obj 15, 17.5 s).

Changes (all with `Dtu_improved_pathfinding`, none with `-origobj` or
`-noimprovedpaths`):

- Move: a hit point behind the start keeps the body where it is (the
  "moved backwards" rule of Descent 1 physics). When the velocity does not
  go into the face (the body is at an edge of the side), the slide takes
  the direction from the nearest point of the edges to the body (level for
  an upright edge), so the body goes around the corner. A move
  away from a side that the body touches already is not a hit. A side of a
  segment that is open at the top, with its top edge less than 5.5 above the
  feet, is the rim of a step: the body goes up onto it, 2.0 at most for each
  hit. A slide around scenery that leaves less than a fourth of the speed
  stops the body, as without the setting, so the blocked move attacks or
  pushes the scenery.
- `body_line_free()` tests at least a fourth of the cylinder plus 0.25 (the
  walls add 1.0, as in the move).
- People open doors (a decision of the project): a hostile creature whose
  model is a person opens a door as a type with the door bit does, with the
  same tests (no key, no lock flag on the side), for its paths
  (`door_is_openable()`) and when it walks into the door (0x25CAA). The door
  opener of the brain code (0x14AF2) stays for the door bit only, and a
  creature that is not hostile keeps the rules of the executable. A person
  is a type whose model name (the `model=` word of its monster line, the
  name in `Pof_names`) starts with `hum_` (humans: the thieves `shmtd2`,
  `sfmtd2`, `shmfc3` and the others of THIEF*, `hmfg1`, `hffg1`, `hmft4`,
  `Jeryth`, the people of the inn), `haf` (halflings: `sfmtf2`, `fmtnpc`),
  `dwf_` (`dmfw1`), `drw` and `dlf_` (the drow `rmfg5`, `rmfb5`, `rmmk5`,
  `rffk5`, `rfft6` and the others), `elf_` (`priestes`), `orc_`, `gob_`,
  `kob` (door bit already), `ogr`, `ognew`, `armogr` (ogres), `troll`,
  `armtroll`, `lic_` (the lich), `min_` (the mind flayer), `ban_`
  (Banegards), `ghu_`, `gha_` (ghouls and ghasts, intelligent in AD&D 2e),
  `mum_` (mummies, the intelligence of ghouls in AD&D 2e), and the named
  `durnan`, `daland`, `thashasa`, `hal.`, `ibba`. Not people: the animals
  (rats, bats, spiders, beetles, scarabs), the skeletons (`ske_`, the guards
  `gua_`), zombies, the spirits (`magesprt`, `prstsprt`), beholders,
  dragons, mephits, gargoyles and `avatar`. The tables have no intelligence
  value (`iq` is the sight and morale number), so the list is by the model.
  The log line "obj N NAME: hp ..." of a level start has "person 0/1" and
  the model name.
- Paths: a hostile creature with no door bit takes no path through a side
  that its body cannot pass (a door that is not open, a closed wall), and is
  not kept out of a segment because of a wall on another side. It opens no
  door. A walker may go up through the top side of a segment that is less
  than 5.5 high (a pit).
- Out of reach: a hostile melee creature that walks, sees the player, is
  farther than its attack distance (`choose_attack_goal()`) and has the feet
  of the player above the top of its body (a ladder, a ledge, a crate),
  while the player does not jump or fall, backs away to the attack distance
  plus 4.0 in x and z, and waits (STAND) in a band of 10.0 beyond that,
  facing the player. It takes this rule only where it would walk straight
  at the player (no path is taken). When the player comes down, it attacks
  again. Log: "the player is above and out of reach, backs away" and
  "waits", "the player is in reach again".

Measured (`-noattack -invulnerability`, 20 s at 30 frames per second, the
player standing in a segment 4 to 6 segments from a creature, 14 places in
each of BAT, BONES, THIEF1, CRYPTS, DROW1, MYRKUL, BANE, KOBOLDS and CLAN1
and 16 more in BAT, BONES and THIEF1; "stuck" is the time that a creature
farther than 18.0 walks but moves less than 0.5 in 0.5 s while a way with no
closed door exists to the player; before and after):

| Level | stuck (s) | creatures within 18.0 |
|---|---|---|
| BAT | 69.0, 36.5 / 85.0, 59.0 | 46, 49 of 65 / 63, 63 of 82 |
| BONES | 104.5, 57.5 / 89.5, 51.5 | 50, 68 of 88 / 68, 70 of 120 |
| THIEF1 | 57.5, 4.0 / 72.5, 23.5 | 10, 18 of 54 / 26, 30 of 79 |
| CRYPTS | 49.5, 34.0 | 24, 24 of 47 |
| DROW1 | 113.0, 34.5 | 31, 37 of 67 |
| MYRKUL | 35.0, 24.0 | 39, 41 of 67 |
| BANE | 16.0, 18.0 | 39, 39 of 54 |
| KOBOLDS | 118.5, 80.5 | 38, 37 of 54 |
| CLAN1 | 62.5, 30.0 | 46, 48 of 80 |

With `-noimprovedpaths` and with `-origobj` the object logs of BONES
`-testpos -50,-4,320` and of the ladder test below are the same before and
after the change.

Ladder (BAT `-testpos -85,-68,154,0 -testmove wait:20,use:1,forward:28,...`:
the player holds the ladder of seg 142 side 5 at y -51.1, 17.0 above the
gskelpus obj 43 and 44): before, both stand under the player (distance
17.0, out of their attack distance 14.6) for the rest of the test. After,
they wait at 31.6 from the player; when the player has climbed up after
they came (`wait:150` first) they back away from 17.1 to 26.0 and wait; when
the player climbs down (`back:60` at 11.3 s) they come back at 12.0 s and
attack at 13.1 and 13.6 s.

Doors: CRYPTS `-testpos -45,5,195,270`, the orc Orcascim (door bit) opens
the door wall 28 at 8.1 s and the orcs come through, as before. BAT
`-testpos -25,-29,263,180 -testmove wait:10,use:1,wait:5,slideright:40`:
the player opens the door of seg 38/39 and steps into seg 40; the Ghoul obj
27 (no door bit) takes a path of 3 segments through the open door (before:
no path, it walked straight and got there too in this place). With the
door closed it stays behind it (no path, as before).

People and doors (25 s each): THIEF1 `-testpos 15.7,-4,482.7`: the thief
`shmtd2` obj 26 opens the door wall 127 (seg 482 side 4) at 4.7 s, obj 27
opens wall 110, and obj 20 and 26 stand at 13.1 from the player at the end
(before: "no way", both at the wall). DROW1 `-testpos 269.7,-4,265.1`: the
drow `rmfg5` obj 21 opens wall 28 (seg 234 side 5) at 2.3 s and stands at
4.4 from the player (before: STILL behind the door at 9.2). DROW1
`-testpos 247,-4,142.5`: the skeletons `ske_axe` obj 76 and 77 open no door
(76 stays at the closed door wall 16; the Ghoul obj 10 opens wall 20).
THIEF1 `-testpos 15.6,-4,402.7`: the thieves `sfmtd2` obj 5 and 6 stay in
seg 21 at the locked side of the door (wall 23), and no creature opens
wall 23. With `-noimprovedpaths` and with `-origobj` the DROW1 log is the
same as before (but the new fields of the start line).

Open: the armOgre of CRYPTS still does not fit through the low opening of
seg 103 (top side) and stands there. A creature with no door bit still
walks straight at a closed door that is between it and the player.

## Animation step (0x11018)

Arguments: eax = object, edx = distance, ebx = `vis_vec_pos`, ecx =
`gun_point`. This is where the AI sets the animation state. Other places:
wake up (STAND), idle code (STAND, FIDGET, REST), blocked move (WALK, SIDLE,
ATTACK), morale (RUN and the 0x10148 mapping).

1. If hostile, not dead, melee, distance > 30.0 and the player was seen in the
   last 0.5 seconds: animation state RUN (not for STATION), GOAL_STATE LOCK.
2. Frame = first frame + `elapsed * (frames - 1) / state_time`, rounded.
   `elapsed` is GameTime minus `ailp[0x24]`. `state_time` is the fix value at
   animation record +0xE4 + state * 4. With `ailp` flag bit 1 the frame runs
   backward from the last frame.
3. Ranged creature in CURRENT_STATE ATAK: the fire frame is the word at record
   +0xB8 for ATTACK and +0xBA for ATTACK2 (`fire_frames=`). If the step would
   jump over it, the frame is set to it.
4. Animation state DIE: turn the creature to lie on the floor (floor normal of
   the segment).
5. The state ends when the frame passes the end. It also ends early if
   GOAL_STATE is FLIN and differs from CURRENT_STATE, or if `ailp` flag bit 5
   is set.
6. At the end of DIE: stay on the last frame. Without status bit 0x04000000
   call 0x6A3CC(objnum).
7. At the end of another state: clear `ailp` flag bits 1 to 5. CURRENT_STATE =
   GOAL_STATE. Animation state = 0x10148 mapping. Mode RUN_FROM forces RUN. For
   ATTACK: use ATTACK2 if the height difference to the player is more than
   half the horizontal distance. Frame = first frame. `ailp[0x24]` = GameTime.
8. An animation state outside 0 to 10 prints "state INVALID (%d)" and becomes
   STAND.
9. Ranged creature closer than 200.0 on a fire frame: `gun_point` = position +
   forward vector * 1.0.

## Melee attack

### The weapon test that runs: 0x6CFF8 (physics.c)

Verified (code), added with the port of the creatures. This function does the
melee hits of version 1.3, for creatures and for the player. The movement code
calls it one time per frame for the moving object: `pmon.c` at 0x9F2B4 for a
creature (only an active creature gets that far, see physics.md) and the player
movement in the same way. Argument: eax = object.

1. The position of the object is moved forward for the time of the test and
   put back at the end (0x6D01A to 0x6D066): with s and c the sine and cosine of
   the heading minus 45 degrees, x gets s + c and z gets c - s. That is the
   square root of 2 along the heading in the x-z plane.
2. If the animation state is not 6, 7, 11 or 12: byte 0x1AF of the object = 0,
   return.
3. `rel` = frame minus the first frame of the state. A creature (type 2) can
   hit when `rel` >= 8. Any other object (the player) can hit when `rel` >= 6
   and `rel` <= frames of the state minus `rel` (so in the frames 6 to 8 of a
   state of 16 frames) (0x6D0CF to 0x6D127).
4. Weapon sphere of the frame: 0x7473F (models.md, "Where the weapon is"). No
   weapon box: return. For the weapon types 0x12, 0x13, 0x22, 0x23, 0x2B, 0x2C,
   0x2D and 0x45 the radius is multiplied by 8.0 (0x6D2B7 to 0x6D2F2).
5. One query of `find_vector_intersection` (0x3EFD9): from the center of the
   sphere, in the direction from the object position to the center, as far as
   the radius, with the radius of the sphere, objects are tested (flag 1), the
   object itself is the "this object" of the query.
6. The object is the player and the query hit an object or a wall: byte 0x1AF =
   0, and the start time of the attack animation of the player (0x1D3373) is
   moved forward by the time of the frames that are left, so the swing goes
   back. A wall hit calls 0x95019 (not read).
7. The query hit an object ("I hit something."):
   - type 18 (scenery): no effect if `Dressing_info` flag 4 is set. Else its
     hit points are limited to 20.0 and the damage function 0x29A9C is called
     with 100.0. Below 1.0 hit point 0xA6ACB breaks it, else 0x25D4F pushes it.
   - type 2 (creature): nothing if the attacker is a creature too. Else
     0x278B5(creature, attacker, weapon type, hit point): the player hits.
   - type 4 (player): if byte 0x1AF of the attacker is 0: set it to 1 and call
     0x27D82(player, attacker, weapon type, hit point). So a creature hits one
     time per swing. The byte is cleared in step 2 and at the end of an
     animation state (0x113C5).

0x27D82 (target, attacker, weapon type, point): casts the spell of the weapon
if it has one, calls the damage function 0x29A9C(target, attacker, 0, weapon
type), and if the hit points of the target went down: a push of 0.2 along the
line from the attacker to the target (0x6B70D adds the vector divided by the
mass to the thrust), blood (0x25C76) and the hit sound.

The damage function 0x29A9C with an attacker throws the to-hit roll (0x2A7F3).
A result of 0 or less ends it. Else the damage is the dice of 0x2AF5E, two
times the dice for a critical hit. For a creature as the target it then sets
CURRENT_STATE = FLIN and the animation start time (if `ailp[0x2C]` is not
negative), and below 1.0 hit point it starts the death: both AI states DEAD,
animation state DIE at its first frame, mode STILL, status flags 0, hit points
0, die sound, `object_create_egg` if the creature contains something (0x2A091 to
0x2A245).

`collide_robot_and_player` is 0x26FC1 (the table of `collide_two_objects` at
0x29355 is indexed by the place of the key (type A * 256 + type B - 0x101) in
the list at 0x292D7, counted from the end). It does not hit: it calls the alert
function with level 3 for a hostile creature and the bump 0x2627C. Before that,
0x26FE0 to 0x27054: a creature with the `rat=` bit (monster table +0xD9 bit 4)
and a player with boots (the item index of the boot slot, byte P+0x1AEC, is 0
or more; 0x27014 tests it against the entry count P+0xF3 and runs `int 3` if it
is not below it) is squashed (objects.md, "Rats").

### Dead code: do_ai_robot_hit_attack (0x1153A)

No instruction of the image calls or jumps to 0x1153A, and no data holds its
address (checked: all `call` and `jmp` targets and all 32-bit values of the
image). 0x28B34 has no other caller. So the function below does not run in
version 1.3. One result: `ailp` flag bit 5 ("the hit landed") is set only here
(0x11AEC), so the attack animation of a creature never ends early.

Arguments: eax = creature, edx = player object, ebx = collision point. It
handles both directions. It returns 0 at once if the player object has type
12.

Player hits creature. If the animation state of the player is 6, 7, 11 or 12:

1. Get the weapon polygon of the player for the current attack frame (0x748D9,
   frame from the attack start time 0x1D3373).
2. Test it against the body of the creature (0x7473F, 0x4F7D5).
3. On a hit call 0x278B5 (collide.c).
4. If the creature now has GOAL_STATE DEAD ("Another AIS_DEAD one, you killed
   him."): both states DEAD, mode STILL, animation restart. A flying creature
   starts to fall. Return 1.
5. Else GOAL_STATE = FLIN, player flag bit 3 at 0x1D335F is set (one hit per
   swing).

Creature hits player. If the monster is melee (attack_type 1), its animation
state is ATTACK or ATTACK2 and the attack switch 0x13A6B4 is not 0:

1. Play `attack_sound` (monster table +0x66).
2. `+0x1DB` = GameTime + (256 - iq), plus the same again with status bits
   0x2000 or 0x08000000.
3. Test the weapon polygon of the creature against the body of the player
   (two tries, 0x7473F).
4. On a hit call 0x28B34(player, creature, collision point, hit point). Set
   `ailp` flag bit 5 and GOAL_STATE = RECO.

0x28B34: play `claw_sound`, push the player (0x25DE6), set the creature
active, apply the spell of the weapon if it has one, then call the damage
function 0x29A9C(player, creature, (difficulty + 1) * 1.0, weapon).

### To-hit roll (0x2A7F3)

Arguments: eax = target, edx = attacker, ebx = weapon number (negative =
natural weapon). Result: 2 = critical hit, 1 = hit, 0 = miss, -1 = fumble.

1. `roll` = rand % 20 + 1.
2. A roll of 20 is a critical hit. A roll of 1 is a fumble. A creature that
   fumbles rolls a second d20 against `str`. If it is higher, the creature
   goes to CURRENT_STATE FLIN. The player gets one of three texts and the
   FIDGET animation, or loses the weapon (0xAE837).
3. Attacker with status bit 0x00200000: subtract min(speed / 0.25 + 1, 5).
   All bonuses of the steps 3 to 8 are added to the roll (the int at 0x1B7AA4),
   not to `thac0` (code 0x2AC84, 0x2ACAC).
4. Creature as attacker: `thac0` = monster table +0x74. Add `to_hit_bonus`
   (+0x70) for a natural weapon, or the bonus of the weapon (weapon table,
   s16 at +0x36, code 0x2AC73). Add the strength bonus
   0x2A789(`str`, 100): a table lookup at 0x13A956 + 4 * str, str limited to 1
   to 19 (values start -5, -3, -3, -2, -2, -1, -1, 0). Add 1 with status bit
   0x40000000.
5. Player as attacker: `thac0` is the signed byte at 0x1D4D78 ("COL: thac0 =
   %d"). Weapon and skill bonuses are added (not read in detail).
6. Target is the player: `ac` = word at 0x1D33C9. Subtract 4 from the roll if
   the player is hidden from the attacker (0x165C4).
7. Target is a creature: `ac` = monster table +0x76. Some status bits limit
   `ac` (0x2ADAF to 0x2AE1D).
8. Attacker with status bit 0x02: add 1. Attacker with status bit 0x08000000:
   subtract 4.
9. Miss if `thac0 - ac > roll`. Else hit.

### Damage dice (0x2AF5E)

For a creature with a natural weapon: sum of `#damage_dice` (+0x6E) rolls of
`rand % damage_dice + 1` (+0x6C), plus `damage_bonus` (+0x72), plus 1 with
status bit 0x40000000. Assert `RF.damage_dice > 0`.

## Ranged attack

### ai_do_actual_firing_stuff (0x1396E)

1. Return if the creature is not hostile.
2. Player visible and forward dot >= 0.75:
   - melee: nothing here if the player is in reach or dead. The hit test does
     the work.
   - ranged: only on a fire frame call `ai_fire`.
   - Unless the behavior is PATROL: in mode STILL or FOL_PATH set mode
     CHASE_OB if morale >= B, or RUN_FROM if morale <= A.
3. Player not visible: if the weapon has byte +1 = 1 in the weapon table (the
   Descent 1 homing test), the frame is a fire frame, CURRENT_STATE is ATAK
   and `Hit_pos` is farther than 40.0: `ai_fire`.
4. GOAL_STATE = RECO.

### ai_fire (0x11B06)

Arguments: eax = creature, edx = fire point, ebx = distance. Text: "AI:
critter fires.".

1. Return if the attack switch is 0, the creature is not hostile or the
   monster is melee.
2. Target = the player object, or the object in +0x1A2 if it is above 0.
3. `weapon_type` < 0: if the target is in reach (sum of radii + 2.0), touch
   attack 0x27D82(target, creature, weapon_type, point) ("AI: melee attack
   1!!!"). Else call 0x10040(-weapon_type, target, creature). Guess: it casts
   spell number `-weapon_type`.
4. `weapon_type` >= 0: return if the control type is 11 or the player is
   dead. With a hidden player the Descent 1 cloak test applies.
5. Aim. If the target is slower than 1.0 on x and z: aim at the believed
   position. Else lead the target: `t = distance / weapon speed`, aim at
   believed position + velocity * t / 2. The weapon speed is the fix at weapon
   table +0x6E + 4 * difficulty.
6. A monster with mage or cleric levels does not shoot. It does the touch
   attack if the target is in reach ("AI: melee attack 2!!!").
7. Else create the projectile (0x55B23, `Laser_create_new_easy`).
8. Call the alert function with level 1.

## Morale (0x10ADF)

Argument: eax = object. Texts: "can't do morale on %d", "AI: NPC (%d) mor =
%d", "AI: (%d) failed morale!".

### When it runs

| Call | Place |
|---|---|
| 0x13F65 | `do_ai_frame`, wake up |
| 0x145FB | `do_ai_frame`, revive |
| 0x153A6 | `do_ai_frame`, CHASE_OB with morale below the attack value ("Bad moralle") |
| 0x163D7 | awareness spread 0x16366, for each creature whose awareness rises |
| 0x178AE | `ai_follow_path` 0x17505, with awareness set to 1 |
| 0x29F1B | damage function 0x29A9C, after it sets status bit 0x100 (afraid) |
| 0x13736 | gate in a robot (0x13407) |
| 0x5F3A9, 0x8E430, 0x8F660, 0x9001F to 0x90645 | spell and effect code (not read) |
| 0xA94AD | script code: make hostile |
| 0xAE100, 0xAE212, 0xAE24F | turn undead (0xADC46) |

### Refusals

The function prints a text and returns if: the object is not a robot, the
control type is not 1 ("no AI"), `max_hit_points` <= 0, or the creature is
dead (GOAL_STATE or CURRENT_STATE DEAD, flag bit 1 at +0x0D, or animation
state DIE).

### Confusion

If status bit 0x20 is set, roll `rand % 10`:

| Roll | Text | Effect |
|---|---|---|
| 0 | "%d wanders" | morale = mean, GOAL_STATE NONE, peaceful bit set, hostile and fleeing clear |
| 1 to 5 | "%d stands confused" | the same, and activity = 0, `+0x1DB` = GameTime + 60.0 |
| 6 to 8 | "%d attacks" | the "pass" result below with the hostile flag set |
| 9 | "%d acts normal" | go on with the normal check |

### Morale number

`mor` is built as follows.

1. Status bit 0x100 (afraid): `mor` = 0. Skip the rest.
2. UNDEAD monster: `mor` = 0 if the fleeing flag is set (turned), else 20.
   Skip the rest.
3. `mor` = (300 - iq) / 8.
4. Subtract the wound level: `(1.0 - shields / max_hit_points) * 4`, rounded
   down (0 to 4).
5. Subtract 2 if 0x359F4 returns true (meaning not traced).
6. Subtract 4 if the player object has one of the status bits 0x21060000, or
   the creature has status bit 0x04.
7. Add `(level(creature) - (player value at 0x1D33CB + 1)) / 2`. 0xAE3BB gives
   the creature level.
8. Add 3 unless the behavior is WANDER or HOME.
9. Add the mage level and the cleric level of the monster.
10. Add 2 * awareness.
11. Add 4 if the creature has one of the status bits 0x21060000, or the player
    object has status bit 0x04.
12. Add `(player armor class - monster armor class) / 2`.
13. Add `level number / 2`.

### Roll and result

`roll` = rand % 19 + 2 (2 to 20).

- Pass (`roll <= mor`): clear the fleeing flag. If the creature is hostile or
  has a target: morale = attack value, mode CHASE_OB, GOAL_STATE SRCH, hostile
  flag set. Else: morale = mean, GOAL_STATE NONE.
- Fail: morale = flee value, mode RUN_FROM, hostile flag clear, fleeing flag
  set, animation state RUN, bit 6 of +0x15F set.

In both cases the animation state is set from CURRENT_STATE (0x10148).

So the stored morale has three values only. The tests in `do_ai_frame`
("morale >= attack value", "morale <= flee value") read the result of the last
check.

### Monster flags and morale

- UNDEAD: fixed morale 20, or 0 when turned. Verified.
- `iq`: a higher value lowers the morale number. Verified.
- NO_SCARE and NO_SLEEP: not read by the morale function. They are bits of the
  flag word +0x98. The spell function 0x92D99 tests that word with a mask
  (guess: scare and sleep spells check them).
- `pain_flee`, `patient`, `impatient`, `dark_lover`, `light_lover`,
  `prison_boy`, `jumping_jack`: the reader stores them in monster table +0xD0.
  0x69E66 copies the dword to object +0x1D7. No test of these bits was found in
  the executable. The patience byte (+0xD8) has no reader either.
- LIGHT_SENSITIVE: no reader found.
- BLIND: only in 0x165C4 (ignores invisibility).
- FLYING: start height of the sight ray, random direction in 3D, no thrust
  copy in the side step, falling on death.
- `noattack`: stored in +0xD9 bit 0. No reader found.

## Monster table values in the AI

The full table layout is in the next section. This is what the AI does with
the `$MONSTER_AI` numbers.

| Numbers | Offset | Use |
|---|---|---|
| 1 | none | read and dropped |
| 2 to 6 | +0x20 | field of view cosine. Passed to 0x109E7, which ignores it |
| 7 to 11 | +0x34 | turn time. Third argument of `ai_turn_towards_vector` (0x106BA) and of the random turn (0x10763). `docs/formats/tbl.md` calls it `firing_wait`. That is wrong |
| 12 to 16 | +0x48 | circle distance of a ranged creature in mode CHASE_OB (0x1511C). `docs/formats/tbl.md` calls it `turn_time`. That is wrong |
| 17 to 21 | +0x5C | no reader found |
| 22 to 26 | +0x80 | flee value `A` of the morale system |
| 27 to 31 | +0x8A | attack value `B` of the morale system |

Verified (data): in BANE.BIN group 2 is 0.8 to 0.4 and group 3 is 7.0 or 3.0.
These fit a turn time in seconds and a distance.

Other values: `iq` (+0xD4) is used for the notice test from behind, the FIDGET
rate, the morale number, the `+0x1DB` stamp and the invisibility test. `str`
(+0xDE) is the volume of the see sound, the fumble test and the to-hit bonus.
`regeneration` (+0x7E) is the number of seconds per hit point of a dead
creature. `speeds=` and `state_times=` go to the animation record of the model
and are read there (0x74AA7 for the speed, record +0xE4 for the times).

## Unknown

- The reader of object +0x1DB.
- The meaning of 0x359F4 in the morale number.
- The meaning of most status bits at object +0x88.
- The table at 0x13A7A0 ("360", "BKWD", "ARND", "TWRD", "AWAY").
- The segment table at 0x38C060 that gates the NONE state code.
- The inside of `ai_move_relative_to_player` (0x125B9), 0x123EF and the path
  code.
- The creature spell functions.

The two sections below were written from separate passes over the reader code
(bm.c), brain.c and dialogue.c.

## Monster table (Robot_info)

All addresses are linear addresses in `dtu_flat.bin` (base 0x10000).

### Summary

- `Robot_info[]` base is 0x38E344. One entry is 242 bytes (0xF2). The limit is 40 entries (assert `N_robot_types < MAX_ROBOT_TYPES`).
- There is no separate `Monster_info[]` array in the executable. All AD&D fields are inside the 242-byte entry.
- `$MONSTER` reader: 0x1DDE6 to 0x1F490 (bm.c). It parses into stack variables, then copies them into the entry at 0x1EF9F to 0x1F35E.
- `$MONSTER_AI` reader: 0x1DC02 to 0x1DCA8.
- Dispatcher: 0x1C3AE calls the `$MONSTER_AI` reader, 0x1C3CB calls the `$MONSTER` reader (function 0x1B2F5).
- The entry index for `$MONSTER` is `N_robot_types` (0x13DA54). The entry index for `$MONSTER_AI` is its own counter (0x13A847).

### Verified (code): entry layout

"Default" is the value the reader sets before it parses the line. Fix means 16.16 fixed point (`value * 65536.0`, constant at 0x121B3D).

| Offset | Size | Type | Filled by | Default | Conversion |
|---|---|---|---|---|---|
| +0x00 | 4 | int | `model=` (first model) | none | model number returned by the model loader 0x72DC5 |
| +0x04 | 2 | s16 | no keyword | -1 | Descent 1 place of `exp1_vclip_num` |
| +0x06 | 2 | s16 | no keyword | -1 | Descent 1 place of `exp1_sound_num` (guess) |
| +0x08 | 1 | byte | `contains_id` | 0 | integer |
| +0x09 | 1 | byte | `contains_count` | 0 | integer |
| +0x0A | 1 | byte | `contains_prob` | 0 | integer |
| +0x0B | 1 | byte | `contains_type` | 0 | word to object type: `monster` 2, `armor` 15, `misc` 16, `spell` 17 |
| +0x0C | 4 | int | `score_value` | 1000 | integer |
| +0x10 | 4 | fix | `lighting` | 0x8000 | fix. Must be 0 to 1.0 |
| +0x14 | 4 | fix | `mass` or `weight` | 0x40000 | fix |
| +0x18 | 4 | fix | `drag` | 0x8000 | fix |
| +0x1C | 4 | fix | `gravity` | 0x10000 | fix |
| +0x20 | 5 x 4 | fix | `$MONSTER_AI` numbers 2 to 6 | none | degrees to cosine (see below) |
| +0x34 | 5 x 4 | fix | `$MONSTER_AI` numbers 7 to 11 | none | fix. Turn time |
| +0x48 | 5 x 4 | fix | `$MONSTER_AI` numbers 12 to 16 | none | fix. Circle distance of ranged creatures (see "Monster table values in the AI") |
| +0x5C | 5 x 1 | byte | `$MONSTER_AI` numbers 17 to 21 | none | integer (`rapidfire_count`) |
| +0x61 | 1 | byte | `cloak_type` | 0 | integer |
| +0x62 | 1 | byte | `attack_type` | -1 | `projectile` 0, `melee` 1. -1 gives the error "Creature %s attack_type not specified" |
| +0x63 | 1 | byte | `door_opening` | 0 | 1 if the number is 1, bit 0 |
| +0x64 | 2 | s16 | `see_sound` | 170 | integer |
| +0x66 | 2 | s16 | `attack_sound` | 171 | integer |
| +0x68 | 2 | s16 | `claw_sound` | 190 | integer |
| +0x6A | 2 | s16 | `weapon_type` | -1 | integer |
| +0x6C | 2 | s16 | `damage_dice` | 6 | integer |
| +0x6E | 2 | s16 | `#damage_dice` | 1 | integer |
| +0x70 | 2 | s16 | `to_hit_bonus` | 0 | integer |
| +0x72 | 2 | s16 | `damage_bonus` | 0 | integer |
| +0x74 | 2 | s16 | `thaco` or `thac0` | 5 | integer. Replaced by the NPC table (see below) |
| +0x76 | 2 | s16 | `armor_class` | 0 | integer. Replaced by the NPC table |
| +0x78 | 2 | s16 | `max_hit_points` | 24 | integer |
| +0x7A | 2 | s16 | `max_hit_points` (second copy) | 24 | integer |
| +0x7C | 2 | s16 | no keyword | -1 | code sets it to 0x16 for the lich (0xACA16, 0xAD189) |
| +0x7E | 2 | s16 | `regeneration` | 0 | integer |
| +0x80 | 5 x 2 | s16 | `$MONSTER_AI` numbers 22 to 26 | none | integer (group A, -100 to -500). Flee value of the morale system |
| +0x8A | 5 x 2 | s16 | `$MONSTER_AI` numbers 27 to 31 | none | integer (group B, 0 to -400). Attack value of the morale system |
| +0x94 | 4 | int | `picture=` | 0 | value returned by 0x19DEF (bitmap index) |
| +0x98 | 2 | u16 | flag words, set 1 | 0 | see the flag table |
| +0x9A | 2 | s16 | `hit_sound` | 10000 | integer |
| +0x9C | 2 | s16 | `die_sound` | 10000 | integer |
| +0x9E | 1 | byte | `see_sound_max` | -1 | integer |
| +0x9F | 1 | byte | `bossiness` | 0 | integer |
| +0xA0 | 4 | int | `hit_vclip` | -1 | integer |
| +0xA4 | 4 | int | `blood` | -1 | integer |
| +0xA8 | 4 | u32 | flag word `pass_thru` | 0 | bit 0x02 |
| +0xAC | 3 x 4 | fix | `speeds=` (3 numbers) | 0 | fix, then times 2 |
| +0xB8 | 3 x 4 | fix | `speeds=` (copy) | 0 | same values as +0xAC |
| +0xC4 | 3 x 4 | fix | `speeds=` (copy) | 0 | same values as +0xAC |
| +0xD0 | 4 | u32 | flag words, set 2 | 0 | see the flag table |
| +0xD4 | 4 | int | `iq` | 70 | integer |
| +0xD8 | 1 | byte | computed | 100 | 100, plus 75 if `patient`, minus 75 if `impatient` |
| +0xD9 | 1 | byte | flag words, set 3 | 0 | see the flag table |
| +0xDA | 4 | int | `monster_hit_vclip` (3 spellings) | 9 | integer |
| +0xDE | 4 | int | `str` | 10 | integer |
| +0xE2 | 1 | byte | `spell_level` | 0 | integer. Replaced by the NPC table (mage level) |
| +0xE3 | 1 | byte | `spell_level`, only if `cleric_points` is not 0 | 0 | Replaced by the NPC table (cleric level) |
| +0xE4 | 1 | byte | `spell_points` | 0 | integer. Replaced by the NPC table (mage points) |
| +0xE5 | 1 | byte | `cleric_points` | 0 | integer. Replaced by the NPC table (cleric points) |
| +0xE6 | 4 | fix | `dead_adjust` | 0 | fix |
| +0xEA | 4 | int | `moola` | 0 | integer |
| +0xEE | 4 | fix | `regonzo` | 0 | fix |

Every byte from +0x00 to +0xF1 is written. +0x60 is the fifth byte of the group at +0x5C
(the loop at 0x1D8E4 stores 5 bytes, `mov byte ptr [ecx + ebx], al`, `cmp ebx, 5`), and
+0x92 to +0x93 is the fifth word of the group at +0x8A (DTU-Decomp `recovered/structures.md`).

The keyword at 0x121659 (between `weapon_type` and `mass`) is `iq`.

The keyword `spell=` is accepted and ignored (0x1EAAB).

### Verified (code): flag words

A flag word is a word of the line without `=`. The compare ignores case (0xD89C1), so `UNDEAD` and `undead` are the same.

| Word | Offset | Bit |
|---|---|---|
| `blind` | +0x98 | 0x0001 |
| `electricity_immune` | +0x98 | 0x0002 |
| `extraplanar` | +0x98 | 0x0004 |
| `fire_immune` | +0x98 | 0x0008 |
| `fire_resistant` | +0x98 | 0x0010 |
| `flying` | +0x98 | 0x0020 |
| `humanoid` | +0x98 | 0x0040 |
| `infravision` | +0x98 | 0x0080 |
| `light_sensitive` | +0x98 | 0x0100 (byte +0x99 bit 0x01) |
| `light_sensitive_2` | +0x98 | 0x0200 (byte +0x99 bit 0x02) |
| `magic_resistant` | +0x98 | 0x0400 (byte +0x99 bit 0x04) |
| `no_scare` | +0x98 | 0x0800 (byte +0x99 bit 0x08) |
| `no_sleep` | +0x98 | 0x1000 (byte +0x99 bit 0x10) |
| `undead` | +0x98 | 0x2000 (byte +0x99 bit 0x20) |
| `weapon_resistant` | +0x98 | 0x4000 (byte +0x99 bit 0x40) |
| `pass_thru` | +0xA8 | 0x02 |
| `dark_lover` | +0xD0 | 0x01 |
| `light_lover` | +0xD0 | 0x02 |
| `prison_boy` | +0xD0 | 0x04 |
| `pain_flee` | +0xD0 | 0x08 |
| `jumping_jack` | +0xD0 | 0x10 |
| `impatient` | +0xD0 | 0x20 |
| `patient` | +0xD0 | 0x40 |
| `noattack` | +0xD9 | 0x01. Also prints "NO ATTACK FLAG (%s)" |
| `rat=` | +0xD9 | 0x04. Also stores the entry index in the global 0x13A820 |
| (name test) | +0xD9 | 0x08. Set at 0x1F448 if the name starts with `rf`, `rm`, `av` or `dro` |

### Verified (code): $MONSTER_AI (0x1DC02)

Order of operations:

1. Read number 1 with atoi (0xDD9BB). The value is thrown away.
2. Take the index from the counter 0x13A847 and add 1 to the counter. Assert `robotnum < MAX_ROBOT_TYPES` (40). Add 1 to 0x13A887.
3. Read 5 fix values to +0x20 (0x1D861).
4. Read 5 fix values to +0x34 (0x1D861).
5. Read 5 fix values to +0x48 (0x1D861).
6. Read 5 bytes to +0x5C (0x1D8CB).
7. Read 5 s16 values to +0x80 (0x1D8A2). Group A.
8. Read 5 s16 values to +0x8A (0x1D8A2). Group B.
9. Convert the 5 values at +0x20 from degrees to a cosine (0x1D8F1). An angle over 179 gives "Warning: Bogus field of view" and is set to 179. Then `angle / 360 * 65536` is a fixed angle, and the sine and cosine function 0xD7E82 gives the result.

All five-value groups are indexed by the difficulty level (global 0x13CDCA). Example: 0x15916 loads 0x13CDCA and reads `[entry + level*2 + 0x88]`, then shifts right by 16. That is group B.

Warning for readers of the listing: Watcom loads an s16 field as a dword two bytes lower and then does `sar reg, 16`. So `mov eax, [edi+0x7E]` plus `sar eax, 16` reads +0x80, and `[edi+0x88]` plus `sar` reads +0x8A. The tables below already have the corrected offset.

### Verified (code): data that is not in the 242-byte entry

| Data | Base | Element | Note |
|---|---|---|---|
| `Robot_names[]` | 0x1B50EC | 32 bytes | `name=`. The byte 0x84 in the name becomes a space. Assert on an empty name |
| `ObjType[]` | 0x1B75E6 | 1 byte | set to 2 (robot) for the new entry, index is the counter 0x1B5650 |
| `ObjId[]` | 0x1B7438 | 1 byte | set to the entry index |
| NPC table | 0xA504A | 26 bytes | 191 compiled-in rows. Ends at 0xA63B0 with a negative byte at +20 |
| Animation data | pointers at 0x3880D0, one per model | | `state_times=`, `speeds=` and `fire_frames=` go to the model loader 0x72DC5 |
| Model names | 0x3878B0 | 13 bytes | used in error texts |

- `state_times=` fills entries 1 to 10 of a list of 19 fix values. The defaults of the 19 values come from 0x13DA08. The list is an argument of 0x72DC5 for the first model only.
- `fire_frames=` has 2 numbers (defaults 56 and 72). They are also arguments of 0x72DC5.
- `simple_model=` is the second model. Its number goes to `+0x50` of the animation record of the first model.
- NPC table row: name (16 bytes), a pointer (4 bytes), then 6 bytes: THAC0, armor class, mage level, mage points, cleric level, cleric points. At the end of the `$MONSTER` reader, 0xA7E97 finds the row by name (0xA7E47) and writes the 6 bytes to +0x74, +0x76, +0xE2, +0xE4, +0xE3, +0xE5. So the `thaco`, `armor_class`, `spell_level`, `spell_points` and `cleric_points` keywords are replaced for every monster that is in the table. A name that is not in the table prints "RLH: NPC [%s] not found!" and keeps the table values of the line.
- The NPC table is at 0xA504A: rows of 26 bytes (name 16 bytes, a pointer, 6 signed bytes), 191 rows; the search 0xA7E47 stops at a row whose THAC0 byte is negative and compares the names with 0xD89C1. Rows with spells (mage level/points, cleric level/points): beholder 32/0, xabash 4/64 and 1/10, dragon and hatchlng 4/4, halaster, lich1 to lich3, hmmnpc and Aleena 4/127, ibbalar 4/60, kob_spel 1/3, avatar and thashasa 4/127 and 4/127, the human and drow casters rmme1 to rmmk9b and hmmk1, hfmk1 (mage 1 to 4), rfft1 to rffk9, rmft1 to rmfk9, hffk1 to hmft7 (cleric 1 to 4), prstsprt cleric 3/100, Mage Spirit 3/100, priestes and drowfem cleric 4/127, Daspel 3/11. The port has the table as `engine/main/dtunpc.h` and applies THAC0 and armor class at the level start (`dtu_objects_level_setup`); the spell values are read by `dtuspell.c`. **Code, Data.**

### Creature setup by name (0xAC7B2), verified (code)

Sources: the DTU 1.3 decompilation (DTU-Decomp,
`recovered/creatures.md`, `recovered/src/creature_setup.c`), checked against
the DOS code with `check_creature_setup.py` (4424 calls) and
`check_creature_ai.py` (six setup-then-registration cases), and the level
start 0xAD5BF. The earlier notes of this section had the six names of the
first rule wrong: they are the names that the rule skips.

Order of the level start (0xAD5BF): the level fixes 0xACCDB (objects.md),
the dressing sounds, the texture lookup 0xAD744, the open of the script
file; then for each object 1 to the highest index: 0xAC7B2 for a creature
(call at 0xAD7A1, the object in EAX), then for a creature or an object with
a script id the test 0xAC3C3 (an entry with the same signature and script
id) and `add_npc` 0xAC431 (0xAD7C4). A full list (100) ends the loop: the
other objects get no setup. With no script file the loop does not run.

0xAC7B2 writes into the shared entry of the type (one `Robot_info` record
for each type of the level), the object and its `ai_local`. The names are
compared with ASCII case folding (32 characters, 4 for `lich`):

1. `weapon_type` below 0 (a spell as the weapon), not for `hatchlng`,
   `fmephit`, `imephit`, `lmephit`, `flayer`, `flaynpc`: weapon 0x21,
   attack type 0, dice 4, number of dice the higher of the mage level
   (+0xE2) and the cleric level (+0xE3).
2. One of: `spectatr`: attack type 0, weapon 0, 2d3. `hatchlng`: flags
   `|= 0x18`, score 7000. `gargoyle`: weapon 0x21, 4d4. `flayer` or
   `flaynpc`: attack type 0, weapon -22, 2d4, regeneration 0, score 3000 or
   6000, object +0x88 bit 0x01000000 cleared. A name that starts with
   `lich`: flags `|= 0x7042`, 10d1, touch spell (+0x7C) 22.
3. Quest flag 67 (the player is in the guild of the Shadow Thieves, set by
   the scripts 60 and 70, cleared by them when the player insults the
   master) and one of 15 names (`sfmtd2`, `sfmtf2`, `shmfb1`, `shmfb5`,
   `shmfc3`, `shmfg1`, `shmfg5`, `shmtd2`, `shmtd4`, `shmtd6`, `shmtf2`,
   `shmtf4`, `shmtf6`, `shdleadf`, `shdleadg`): awareness 0, hostile bit
   (0x400000) cleared, target -1.
4. Level 0 (YPINN), each creature: door bit, str 19, score 10000,
   `max_hit_points` 200 of the type; shields 200.0 and AI mode 0 of the
   object. Script 1, 2, 5, 7: attack type 1, weapon 14. Script 3: attack
   type 0, weapon 39; 4 and 8: weapon 44; both timers +0x1A8, +0x1C0 = game
   time. Others (6): no change.

The type entry is shared: the first creature of a type changes it and the
later ones see the change. The fallback of rule 1 runs once for each type:
in the 31 levels 44 placements would qualify by their table values, the
DOS code enters it for 24 type entries with no level fixes and for 22 with
them (the level fixes of KOBOLDS and FLAME give those types a weapon
first). All writes but the inn switch give the same entry in any order,
so the order has no effect for the player. The inn switch writes the
weapon of the type for each creature: the last script of a type wins (the
two Durnan objects, scripts 1 and 7, both give weapon 14).

`add_npc` (0xAC431) then writes the hostility again for each creature that
it adds: awareness 4, hostile bit, target 0, unless the script id is kept
and the script has a "say" statement or bit 9 is set. It reads only the
script id, the level number, the script and bit 9: no anger, no list state,
no quest flag. So rule 3 has an effect only for a creature that is peaceful
anyway or that is not added (a duplicate, a full list). With quest flag 67,
98 of the 107 placements of the 15 names in THIEF0 to THIEF2, BEHOLDER and
BEHOLDR2 are hostile again (scripts 55 and 63 of the ordinary members have
no "say"; their label 1, `IF gflag 67 SET flag1 ELSE SET flag2`, never runs
because a hostile creature shows its line 0x3EA and runs no script). A
player who joined the guild and did nothing against it is attacked. A
creature that the player attacked keeps its hostility through the state
of the level (the restore reads it after the setup), not through `add_npc`.

The port (dtuobj_setup.c): `creature_setup_by_name` and `level_start_npcs` after
the object setup and the object part of the level fixes, in the order of the
executable; `add_npc` is the old hostility part of `init_creature_ai`. The
type entries of the setup of the level file are kept for a restore
(`Setup_ext`). The fire_resistant bit 0x10 of the hatchling has no
MONSTER_F_ bit (fire_immune is tested first). The touch spell is stored and
not used (audit combat C16). The option `-setuplog` writes one session log
line for each setup (the type values before and after) and each `add_npc`;
`tools/check_creature_setup.py` compares these lines for all levels with the
C reference of the decompilation.

`Dtu_fix_guild_peace` (on; `-origobj` and `-origguildpeace` clear it): the
peace of rule 3 stays after `add_npc`. Test: `-level BEHOLDER -scriptflags
67 -statetest 0,0,52,161,24,1 -objlog FILE`: the 15 names are peaceful at
the start; object 52 hit for 1 point is hostile, and after the warp to HUB4
and back the state restore gives "obj 52 sfmtd2 hostile", the others
peaceful. With `-origguildpeace` they are all hostile but the ones with a
kept "say" script.

### Verified (code): readers of the flag bits

Scan method: a script followed registers loaded from 0x1671B4 or made from 0x38E344, and absolute operands inside 0x38E344 to 0x38E435. It does not follow a pointer that is passed to another function. So "none found" is not a proof.

| Flag | Readers in the AI code (0x10000 to 0x18000) | Other readers |
|---|---|---|
| blind 0x0001 | 0x16602 | none found |
| electricity_immune 0x0002 | none | 0x91D90 (spells.c) |
| extraplanar 0x0004 | none | 0x2AD55, 0x2AD8F (collide.c, to-hit function 0x2A849), 0x6A5B1 (object.c) |
| fire_immune 0x0008 | none | 0x2634B, 0x2B3B3 (collide.c damage), 0x3D2DA, 0x91DAF (spells.c) |
| fire_resistant 0x0010 | none | none found |
| flying 0x0020 | 0x1084B, 0x11835, 0x11EE4, 0x11F8A, 0x1247A, 0x1457F, 0x14EB7, 0x14F2F, 0x14FD7, 0x15053, 0x16A14, 0x16C60, 0x17CF3 | 0x25D35, 0x2A21D (collide.c), 0x6C770 (physics.c), 0x76E6C (render.c), 0x90A6F, 0x91706 (spells.c), 0x9F379, 0x9F4BF, 0x9F53C, 0x9FD41 (pmon.c), 0xBB0FF (state.c) |
| humanoid 0x0040 | 0x10612 | 0x6A435 (object.c), 0xAE54E, 0xB33E1 |
| infravision 0x0080 | none | none found |
| light_sensitive 0x0100, light_sensitive_2 0x0200 | none | none found as a byte test |
| magic_resistant 0x0400 | none | 0xAE77F |
| no_scare 0x0800, no_sleep 0x1000 | none | none found as a byte test. 0x92F5D (spells.c, function 0x92D99) tests the whole word against a mask in edx. This is the likely reader (guess) |
| undead 0x2000 | 0x10D53 (morale function) | 0x29EA0, 0x2ABC7, 0x2B221, 0x2B262 (collide.c), 0xAC4B4, 0xADDDD (turn undead) |
| weapon_resistant 0x4000 | none | 0x29D9D (collide.c, apply damage) |
| pass_thru (+0xA8) | 0x141D3 copies the byte to object +0x1AE. 0x12C83 tests object +0x1AE bit 0x80 | 0x47825 (gamesave.c), 0x6A0F1 (object.c): same copy to object +0x1AE |
| set 2 (+0xD0), all 7 words | none | 0x69E66 (object.c) copies the dword to object +0x1D7 and sets object +0x1D6 = 100 and object +0x1DB = 100. No bit test of object +0x1D7 was found |
| patience byte (+0xD8) | none | none found |
| noattack (+0xD9 0x01) | none | none found |
| rat (+0xD9 0x04) | none | 0x26FE0 (collide.c), 0x9F919 (pmon.c) |
| name flag (+0xD9 0x08) | none | 0x27905, 0x2ABFB, 0x2B31B (collide.c), 0x38695 (displays.c), 0x59D8D (mission.c), 0x8F528, 0x8F840 (spells), 0xAC22E |

Other whole-word users of +0x98: 0x9E66E (pcons.c, loads the word).

Object +0x1DB is set to 100 at creation and is changed by the AI (0x10551, 0x10CBD, 0x119A7, 0x11A03, 0x1271A, 0x13EE0, 0x1416D, 0x141A5, 0x1658C, 0x165BA). It is a time stamp (see "Other object fields that the AI uses").

### Verified (code): readers of the fields in the AI code (0x10000 to 0x18000)

| Offset | Field | Address | Instruction |
|---|---|---|---|
| +0x00 | model number | 0x1344C | `mov eax, dword [esi]` |
| +0x00 | model number | 0x1345E | `mov eax, dword [esi]` |
| +0x00 | model number | 0x134F9 | `mov eax, dword [esi]` |
| +0x00 | model number | 0x135CD | `mov eax, dword [esi]` |
| +0x14 | mass | 0x13613 | `mov eax, dword [esi+0x14]` |
| +0x18 | drag | 0x1361C | `mov eax, dword [esi+0x18]` |
| +0x1C | gravity | 0x13625 | `mov eax, dword [esi+0x1c]` |
| +0x34 | turn time[level] | 0x12526 | `mov ebx, dword [ebp+ebx*4+0x34]` |
| +0x34 | turn time[level] | 0x14AC3 | `mov ebx, dword [ebx+eax*4+0x34]` |
| +0x34 | turn time[level] | 0x14AE9 | `mov ebx, dword [ebx+eax*4+0x34]` |
| +0x34 | turn time[level] | 0x14C58 | `mov ebx, dword [edx+eax*4+0x34]` |
| +0x34 | turn time[level] | 0x14EDF | `mov ebx, dword [eax+edx*4+0x34]` |
| +0x34 | turn time[level] | 0x14F88 | `mov ebx, dword [eax+ebx*4+0x34]` |
| +0x34 | turn time[level] | 0x15565 | `mov ebx, dword [eax+edx*4+0x34]` |
| +0x34 | turn time[level] | 0x15690 | `mov ebx, dword [ebx+eax*4+0x34]` |
| +0x34 | turn time[level] | 0x15731 | `mov ebx, dword [edx+eax*4+0x34]` |
| +0x34 | turn time[level] | 0x15E55 | `mov ebx, dword [eax+ebx*4+0x34]` |
| +0x34 | turn time[level] | 0x15ECE | `mov ebx, dword [eax+edx*4+0x34]` |
| +0x34 | turn time[level] | 0x15F59 | `mov ebx, dword [edx+eax*4+0x34]` |
| +0x34 | turn time[level] | 0x17E14 | `mov ecx, dword [ebp+eax*4+0x34]` |
| +0x44 | turn time[4] | 0x15F2B | `mov ebx, dword [eax+0x44]` |
| +0x44 | turn time[4] | 0x17DFF | `mov eax, dword [ebp+0x44]` |
| +0x48 | circle distance[level] | 0x14D3D | `mov edi, dword [edx+eax*4+0x48]` |
| +0x48 | circle distance[level] | 0x14EFD | `mov eax, dword [eax+edx*4+0x48]` |
| +0x48 | circle distance[level] | 0x1511C | `mov edx, dword [eax+edx*4+0x48]` |
| +0x62 | attack_type | 0x1063C | `cmp byte [eax*2+0x38e3a6],0x1` |
| +0x62 | attack_type | 0x1093A | `cmp byte [ecx+0x62],0x1` |
| +0x62 | attack_type | 0x110F0 | `cmp byte [eax*2+0x38e3a6],0x1` |
| +0x62 | attack_type | 0x111D1 | `cmp byte [eax*2+0x38e3a6],0x0` |
| +0x62 | attack_type | 0x114E2 | `cmp byte [eax*2+0x38e3a6],0x0` |
| +0x62 | attack_type | 0x11F93 | `cmp byte [eax+0x38e3a6],0x1` |
| +0x62 | attack_type | 0x129C0 | `cmp byte [eax+0x62],0x1` |
| +0x62 | attack_type | 0x15110 | `cmp byte [eax+0x62],0x0` |
| +0x62 | attack_type | 0x15486 | `cmp byte [eax+0x62],0x0` |
| +0x62 | attack_type | 0x155B4 | `cmp byte [eax+0x62],0x1` |
| +0x62 | attack_type | 0x15933 | `cmp byte [edx+0x62],0x1` |
| +0x62 | attack_type | 0x15A60 | `cmp byte [eax+0x62],0x1` |
| +0x62 | attack_type | 0x15CB7 | `cmp byte [edx+0x62],0x1` |
| +0x62 | attack_type | 0x15F25 | `cmp byte [eax+0x62],0x0` |
| +0x63 | door_opening | 0x1061B | `or byte [eax+0x38e3a7],0x1` |
| +0x63 | door_opening | 0x13260 | `test byte [eax*2+0x38e3a7],0x1` |
| +0x63 | door_opening | 0x14B03 | `test byte [eax*2+0x38e3a7],0x1` |
| +0x63 | door_opening | 0x156B6 | `mov al, byte [eax*2+0x38e3a7]` |
| +0x63 | door_opening | 0x16A99 | `test byte [eax*2+0x38e3a7],0x1` |
| +0x64 | see_sound | 0x13E25 | `mov ax, word [eax+0x64]` |
| +0x64 | see_sound | 0x13E3D | `mov ax, word [eax+0x64]` |
| +0x6A | weapon_type | 0x13AD6 | `mov ecx, dword [edx+0x38e3ac] ; then sar 16` |
| +0x6A | weapon_type | 0x13B09 | `cmp word [edx+0x38e3ae],0x0` |
| +0x76 | armor_class | 0x10EE6 | `mov edx, dword [edi+0x74] ; then sar 16` |
| +0x78 | max_hit_points | 0x10B74 | `cmp word [edi+0x78],0x0` |
| +0x78 | max_hit_points | 0x10DCB | `mov ebx, dword [edi+0x76] ; then sar 16` |
| +0x78 | max_hit_points | 0x13635 | `mov eax, dword [esi+0x76] ; then sar 16` |
| +0x78 | max_hit_points | 0x14444 | `mov eax, dword [eax+0x76] ; then sar 16` |
| +0x78 | max_hit_points | 0x14466 | `mov eax, dword [eax+0x76] ; then sar 16` |
| +0x7E | regeneration | 0x13641 | `cmp word [esi+0x7e],0x0` |
| +0x7E | regeneration | 0x13C96 | `cmp word [eax+0x7e],0x1` |
| +0x7E | regeneration | 0x14411 | `cmp word [eax+0x7e],0x0` |
| +0x7E | regeneration | 0x14429 | `mov edx, dword [eax+0x7c] ; then sar 16` |
| +0x80 | flee value (group A) | 0x104FD | `mov esi, dword [eax+0x38e3c2] ; then sar 16` |
| +0x80 | flee value (group A) | 0x10C07 | `mov eax, dword [edi+0x7e] ; then sar 16` |
| +0x80 | flee value (group A) | 0x10C65 | `mov edx, dword [edi+eax*2+0x7e] ; then sar 16` |
| +0x80 | flee value (group A) | 0x10F87 | `mov eax, dword [edi+0x7e] ; then sar 16` |
| +0x80 | flee value (group A) | 0x10FD3 | `mov eax, dword [edi+eax*2+0x7e] ; then sar 16` |
| +0x80 | flee value (group A) | 0x157B1 | `mov edx, dword [edx+0x7e] ; then sar 16` |
| +0x80 | flee value (group A) | 0x158DB | `mov ebx, dword [ecx+ebx*2+0x7e] ; then sar 16` |
| +0x8A | attack value (group B) | 0x10506 | `mov eax, dword [eax+0x38e3cc] ; then sar 16` |
| +0x8A | attack value (group B) | 0x10C0D | `mov edx, dword [edi+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x10C6C | `mov eax, dword [edi+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x10CE2 | `mov eax, dword [edi+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x10F8D | `mov edx, dword [edi+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x12B39 | `mov edx, dword [edx+ecx*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x14778 | `mov edx, dword [ebx+edx*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15183 | `mov edx, dword [edx+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15260 | `mov edx, dword [edx+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x152E6 | `mov eax, dword [ebx+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x1534E | `mov edx, dword [eax+edx*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x153F8 | `mov ebx, dword [ebx+edx*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x1546D | `mov ebx, dword [eax+edx*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x1554A | `mov ebx, dword [eax+edx*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15580 | `mov edx, dword [edx+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15798 | `mov ebx, dword [edx+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x1591B | `mov ebx, dword [edx+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15A47 | `mov ecx, dword [eax+edx*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15C9F | `mov ebx, dword [edx+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15E2C | `mov edx, dword [ebx+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15EA5 | `mov edx, dword [ebx+eax*2+0x88] ; then sar 16` |
| +0x8A | attack value (group B) | 0x15F10 | `mov ebx, dword [eax+edx*2+0x88] ; then sar 16` |
| +0x98 | flags set 1 (low byte) | 0x10612 | `test byte [eax+0x38e3dc],0x40` |
| +0x98 | flags set 1 (low byte) | 0x1084B | `test byte [ecx+0x98],0x20` |
| +0x98 | flags set 1 (low byte) | 0x11835 | `test byte [eax*2+0x38e3dc],0x20` |
| +0x98 | flags set 1 (low byte) | 0x11EE4 | `test byte [eax*2+0x38e3dc],0x20` |
| +0x98 | flags set 1 (low byte) | 0x11F8A | `test byte [eax+0x38e3dc],0x20` |
| +0x98 | flags set 1 (low byte) | 0x1247A | `test byte [ebp+0x98],0x20` |
| +0x98 | flags set 1 (low byte) | 0x1457F | `test byte [eax*2+0x38e3dc],0x20` |
| +0x98 | flags set 1 (low byte) | 0x14EB7 | `test byte [eax+0x98],0x20` |
| +0x98 | flags set 1 (low byte) | 0x14F2F | `test byte [eax+0x98],0x20` |
| +0x98 | flags set 1 (low byte) | 0x14FD7 | `test byte [eax+0x98],0x20` |
| +0x98 | flags set 1 (low byte) | 0x15053 | `test byte [edx+0x98],0x20` |
| +0x98 | flags set 1 (low byte) | 0x16602 | `test byte [eax+0x98],0x1` |
| +0x98 | flags set 1 (low byte) | 0x16A14 | `test byte [eax*2+0x38e3dc],0x20` |
| +0x98 | flags set 1 (low byte) | 0x16C60 | `test byte [eax*2+0x38e3dc],0x20` |
| +0x98 | flags set 1 (low byte) | 0x17CF3 | `test byte [eax+0x38e3dc],0x20` |
| +0x99 | flags set 1 (high byte) | 0x10D53 | `test byte [eax*2+0x38e3dd],0x20` |
| +0xA8 | pass_thru | 0x141D3 | `mov al, byte [eax*2+0x38e3ec]` |
| +0xAC | speeds | 0x135FF | `mov ebx, dword [esi+eax*4+0xac]` |
| +0xD4 | iq | 0x10543 | `sub esi, dword [eax*2+0x38e418]` |
| +0xD4 | iq | 0x10DBF | `sub eax, dword [edi+0xd4]` |
| +0xD4 | iq | 0x11999 | `sub edx, dword [eax*2+0x38e418]` |
| +0xD4 | iq | 0x119CC | `mov edi, dword [eax*2+0x38e418]` |
| +0xD4 | iq | 0x119F8 | `mov ebx, dword [eax*2+0x38e418]` |
| +0xD4 | iq | 0x13DC7 | `cmp edx, dword [eax+0xd4]` |
| +0xD4 | iq | 0x140A6 | `mov ebx, dword [eax+0xd4]` |
| +0xD4 | iq | 0x1415F | `sub ebx, dword [eax+0xd4]` |
| +0xD4 | iq | 0x14181 | `mov esi, dword [eax+0xd4]` |
| +0xD4 | iq | 0x142DF | `mov eax, dword [eax+0xd4]` |
| +0xD4 | iq | 0x15C02 | `mov ebx, dword [eax+0xd4]` |
| +0xD4 | iq | 0x1618B | `mov eax, dword [eax*2+0x38e418]` |
| +0xD4 | iq | 0x165B0 | `sub ebx, dword [eax*2+0x38e418]` |
| +0xD4 | iq | 0x16614 | `cmp dword [eax+0xd4],0x82` |
| +0xDE | str | 0x13E10 | `mov edx, dword [eax+0xde]` |
| +0xE2 | mage level | 0x10229 | `cmp byte [eax+0x38e426],0x0` |
| +0xE2 | mage level | 0x10321 | `cmp byte [edx+0x38e426],0x0` |
| +0xE2 | mage level | 0x10957 | `cmp byte [eax+0x38e426],0x0` |
| +0xE2 | mage level | 0x10E4C | `mov dl, byte [edi+0xe2]` |
| +0xE2 | mage level | 0x11DE9 | `cmp byte [eax+0x38e426],0x0` |
| +0xE2 | mage level | 0x12A49 | `cmp byte [ecx+0x38e426],0x0` |
| +0xE2 | mage level | 0x12E14 | `cmp byte [eax+0x38e426],0x0` |
| +0xE2 | mage level | 0x1492B | `cmp byte [eax+0x38e426],0x0` |
| +0xE2 | mage level | 0x159DB | `cmp byte [eax+0x38e426],0x0` |
| +0xE2 | mage level | 0x15AE6 | `cmp byte [eax+0x38e426],0x0` |
| +0xE2 | mage level | 0x15D7D | `cmp byte [eax+0x38e426],0x0` |
| +0xE3 | cleric level | 0x10232 | `cmp byte [eax+0x38e427],0x0` |
| +0xE3 | cleric level | 0x1032A | `cmp byte [edx+0x38e427],0x0` |
| +0xE3 | cleric level | 0x10960 | `cmp byte [eax+0x38e427],0x0` |
| +0xE3 | cleric level | 0x10E56 | `mov al, byte [edi+0xe3]` |
| +0xE3 | cleric level | 0x11DF2 | `cmp byte [eax+0x38e427],0x0` |
| +0xE3 | cleric level | 0x12A52 | `cmp byte [ecx+0x38e427],0x0` |
| +0xE3 | cleric level | 0x12E1D | `cmp byte [eax+0x38e427],0x0` |
| +0xE3 | cleric level | 0x14934 | `cmp byte [eax+0x38e427],0x0` |
| +0xE3 | cleric level | 0x159E4 | `cmp byte [eax+0x38e427],0x0` |
| +0xE3 | cleric level | 0x15AF3 | `cmp byte [eax+0x38e427],0x0` |
| +0xE3 | cleric level | 0x15D86 | `cmp byte [eax+0x38e427],0x0` |
| +0xE4 | mage points | 0x105C7 | `mov al, byte [esi+0x38e428]` |
| +0xE5 | cleric points | 0x105E5 | `mov al, byte [esi+0x38e429]` |

### Verified (code): readers of the fields outside the AI code

| Offset | Field | Readers (function, source file if known) |
|---|---|---|
| +0x00 | model number | 0x3DA7D, 0x3DB13, 0x47849 (gamesave.c), 0x5FE5F, 0x5FE84 (newdemo.c), 0x6A092 (object.c), 0x7B52E, 0x7B55A (titles.c), 0xBE387 to 0xBE45D, 0xCA120, 0xCA1E1, 0xCD0F3 |
| +0x04 | s16, no keyword | 0x3C984 (compare with -1) |
| +0x08 to +0x0B | contains | 0x3CDBE (count), 0x3CDD1 (prob), 0x3CDDB (count), 0x3CDF0 (type), 0x3CDF6 (id). fireball.c, function 0x3CC0B, drop on death |
| +0x0C | score_value | 0x2A120 (collide.c, apply damage), 0x9163C (spells.c) |
| +0x14, +0x18, +0x1C | mass, drag, gravity | 0x3DB3B to 0x3DB77, 0x4787A to 0x478B6 (gamesave.c), 0xBE485 to 0xBE4C1. Copy to the physics data of the object |
| +0x48 | circle distance | 0x25D43 (collide.c) |
| +0x61 | cloak_type | 0x5FEAC (newdemo.c) |
| +0x62 | attack_type | 0x67812 (object.c), 0x93261 (spells), 0xAE4CE |
| +0x63 | door_opening | 0x25CC7 (collide.c), 0x5F3FB, 0xAE55B |
| +0x64 | see_sound | 0xA656C |
| +0x66 | attack_sound | 0xA9EF8, 0xAAB4D |
| +0x68 | claw_sound | 0x28B56, 0x28B6D |
| +0x6A | weapon_type | 0x671A1, 0x671AB, 0x677FE, 0x6781B (object.c), 0x6AE15 (paging.c), 0x6D1D3 (physics.c), 0x74777 |
| +0x6C, +0x6E | damage dice | 0x2AFE8, 0x2B00B, 0x2B03B (collide.c, damage function 0x2AF5E) |
| +0x70, +0x74, +0x76 | to_hit_bonus, thaco, armor_class | 0x2AC7B, 0x2AC5F, 0x2AD1D (collide.c, to-hit function 0x2A849) |
| +0x72 | damage_bonus | 0x2B048 |
| +0x78 | max_hit_points | 0x29ED5, 0x3DB9C, 0x8DED0, 0x8DFBF, 0x90156, 0xBE4E6, 0xCA14F, 0xCA210, 0xAE3CD |
| +0x7C | s16, no keyword | 0x2A64A, 0x2A654 (collide.c, apply damage: test for value 0x16) |
| +0x7E | regeneration | 0x3DBBD, 0xBE507: if not 0, object +0x88 = 0x1000000 |
| +0x80, +0x8A | groups A and B | 0x59DF8 (mission.c), 0xAA846, 0xAA850 |
| +0x9A | hit_sound | 0x25C25, 0x275A7, 0x275FD, 0x27B22, 0x27C0E, 0x27EA7 (collide.c), 0x90F44, 0x9102A (spells.c), 0xA9F39 |
| +0x9C | die_sound | 0x27661, 0x276C3, 0x27C3A, 0x27ED5, 0x2A157 (collide.c), 0x915CC (spells.c), 0xA6BE1 |
| +0xA0 | hit_vclip | 0x27567 |
| +0xAC | speeds | 0x6A0CE, 0x6A7E3 (object.c) |
| +0xD4 | iq | 0x29000 (collide.c, item pick up), 0x45A06 |
| +0xDA | monster_hit_vclip | 0x27546, 0x27BCF, 0x27E5C (collide.c) |
| +0xDE | str | 0x2AA10, 0x2AC9B (collide.c), 0x9FA5C, 0x9FC44 (pmon.c), 0xA6557 |
| +0xE2, +0xE3 | mage and cleric level | 0x921F8, 0x92EF8, 0x92F1D, 0x93206, 0x93215, 0x93247 (spells), 0x8F531, 0xAE6FD, 0xAE717 |
| +0xE4, +0xE5 | mage and cleric points | 0x92E67, 0x92E73 (spells, "%s's spells were recharged!"), 0xAE4FA, 0xAE506 |
| +0xEA | moola | 0x6A677 (object.c, "make gold") |

No reader was found for: +0x06, +0x10 (lighting), +0x20 (field of view), +0x5C (rapidfire_count), +0x7A, +0x94 (picture), +0x9E (see_sound_max), +0x9F (bossiness), +0xA4 (blood), +0xB8 and +0xC4 (speed copies), +0xD8, +0xE6 (dead_adjust), +0xEE (regonzo).

### Verified (data)

- `BANE.BIN` (decoded with `tools/egg.py bin`): each `$MONSTER_AI` line has number 1 = 4, 5 or 6, then 150/150/135/135/179 style degrees, 0.8 to 0.4, 7.0 or 3.0, 0 0 1 1 3, -100 to -500, 0 to -400. This matches the read order above.
- `BANE.BIN` has `iq=` values 70, 80, 100, 140, `str=` values 3 to 16, `dead_adjust=` 2, 6, 8, `regonzo=2.5`, `blood=32` or `33`, `regeneration=0`.
- NPC table: `xabash` is 5, -3, 4, 64, 1, 10. `halaster` is 1, -10, 4, 127, 0, 0. `goblin` is 20, 10, 0, 0, 0, 0.

### Guess

- +0x7C is a spell or special attack number. Only code sets it (0x16 for the lich).
- Groups A and B are no longer a guess. A is the flee value and B is the attack value of the morale system (see "Morale").
- The +0xD9 bit 0x08 marks people (name prefixes `rf`, `rm`, `av`, `dro`: race or class codes of human-like creatures).
- Field of view (+0x20) is dead data. This is verified: 0x109E7 receives it and ignores it.

## brain.c and the BRN file

### Verified (code): functions

| Address | Function | Callers |
|---|---|---|
| 0x97774 | load the brain file | 0x421F4 (game.c, level start, after "fix obj segs"), 0x49B2A (gamesave.c, level load) |
| 0x9784C | free the buffer (pointer 0x14295C) | 0x42667 (game.c, end of game), 0x49B25 (gamesave.c, just before the load) |
| 0x9786A | build the brain file | none. No call, no jump, no stored address |
| 0x9810D | bit lookup (entry with register saves, body at 0x98110) | 8 calls and 1 tail jump, see below |
| 0x9816B | 3-bit lookup (body at 0x9816D) | none. No call, no jump, no stored address |
| 0x981BA | ray test of the builder | 0x97FCE (builder only) |

Globals: buffer pointer 0x14295C, have-brain flag 0x142958, last byte address 0x142960, count of set bits 0x142978.

The have-brain flag 0x142958 has one reader: the assert `we_have_a_brain` at 0x13CE0 in `do_ai_frame` (ai.c line 0xE7D). So a level without a BRN file stops a debug build and runs with an all-zero matrix in a release build.

### Verified (code): loader 0x97774

1. Print "Do we have a brain file for this level?".
2. Allocate 80000 bytes (0x13880) and clear them.
3. Build the file name: level name, then `brn`.
4. Print "Brain file is %s". Open the file.
5. If it opens: read 80000 bytes, print "Yes we do!", close, set 0x142958 = 1.
6. If not: print "No we don't!", set 0x142958 = 0. The buffer stays allocated and zero.

### Verified (code): bit lookup 0x9810D

`eax` = segment A, `edx` = segment B. Result = `(buf[A*100 + B/8] >> (B%8)) & 1`. It means "A sees B".

Callers. In every case A and B are the segment numbers of two objects (object +0x0E).

| Call | Function | A | B | Use of the answer |
|---|---|---|---|---|
| 0x13D48 | `do_ai_frame` 0x13BF4 | the creature | its target object (object +0x1A2) | Gate before the vector to the target is made (0xD7110, 0xD71E0) and before the see and notice code. Skipped if the target is -1, if object +0x88 bit 4 is set, or if 0x165C4 returns true. Result 0: jump to 0x13F71, target is not seen |
| 0x14271 | `do_ai_frame` (step 7, periodic recheck) | the creature | the player object (0x30DF14) | Same gate, with the player as the target. Result 0: jump to 0x14300 |
| 0x1616D | 0x160AB (ai.c) | the creature | another object (pointer on the stack) | Gate before a random roll against `iq` (+0xD4, shifted left 16). Notice or hearing test (guess) |
| 0x109F9 (jump) | wrapper 0x109E7 | object in eax | the player object | Called from 0x12CEA and 0x12D3F (ai.c), with a field of view value from +0x20 loaded before. Line of sight shortcut |
| 0x386B6 | 0x37A6A (displays.c, item use) | an object | the player or user object | Only for creatures with +0xD9 bit 0x08. Then a vector between the two objects. Result 0: skip the creature |
| 0x42AFE | 0x42757 (game.c, cheat "They were sick anyway....") | each robot object | the player object | Loop over all objects. Each robot that sees the player segment gets damage from 0x29A9C (kill) |
| 0x90D81 | 0x90CEF (spells.c, blind and deaf effect) | the spell source | each object | Area effect. Only objects in a segment that the source sees are hit |
| 0xADE2A | 0xADC46 (turn undead) | the creature | the player object | Only undead (+0x99 bit 0x20) that see the player segment are tested |
| 0xAE045 | 0xADC46 (turn undead) | the creature | the player object | Second pass, counts the creatures |

The matrix is a pre-filter on segment level. No caller uses it for path finding or for door or object activation. The exact ray cast, where one exists, comes after it.

### Verified (code): the 3-bit lookup 0x9816B is dead code

- It has no caller and no reference in the image.
- It reads `buf[A*400 + B/2]` and returns `byte & 0x07` for even B, `byte & 0x70` for odd B (not shifted). So it is a 3-bit value per segment pair, two per byte.
- A row of 400 bytes times 800 segments is 320000 bytes. The buffer has 80000 bytes. The function does not fit the loaded file.
- No code writes such values into the buffer.

### Verified (code): builder 0x9786A

The builder is not reachable in this executable. Order of operations:

1. Print "Building a brainfile.". Set object 0 as the viewer (0x3B9CF4 = 0x2B0614) and object 1 as the probe (0x3B9CFC = 0x2B07F3).
2. "Flipping Trapdoors": for each trap print "Flippin' Trap #%d seg:%d, side %d." and switch it (0x4ADEE).
3. "Opening ALL doors" (0x7CFEE in a loop over the walls).
4. "Gorping out objects...": set the object list head of each segment (segment +0x210) to -1.
5. "Objects are setting up...": link object 0 into a segment, put it at the segment center (0x4AD9C), reset it (0x6B9B0).
6. "Building Robot Map": see below.
7. "Building Visibility Matrix": for each segment A (outer loop) and each segment B (inner loop):
   - set the highest object index to 2, clear all segment object lists,
   - put object 0 at the center of A and object 1 at the center of B (if both x values are equal, add 1 to the x of object 1), set byte +0x1CC of object 1 to 2,
   - call 0x981BA(object 1, position of object 1, vector). It finds the segment of the point (0x4BE0B) and casts a ray with radius 0x4000 from object 1 to object 0 (0x3EFD9, the fvi function). It returns true if the hit type is 2 (hit an object),
   - if true, or if A equals B: set bit B of row A (`buf[A*100 + B/8] |= 1 << (B%8)`), add 1 to 0x142978, and print "/%d & %d/" for A not equal to B.
   - Every 25 rows it prints progress and sometimes plays a sound (random).
8. Write 80000 bytes to the file `NAME.brn`. Print "Saws %d" (count of set bits) and "I just created a brain file, DON'T SAVE this level!!!".

The ray goes from the center of B to the center of A and hits a real object. This explains why the matrix is not symmetric.

### Verified (code): what "Building Robot Map" builds

Nothing that is kept. The loop at 0x97BBE to 0x97D81:

- Outer loop over all segments (only used for the progress text "(%d").
- Inner loop over all segments S. For 4 sides, taken from the table at 0x142968 (values 5, 2, 4, 0):
  - set `dist[k]` = 0x7FFFFFFF,
  - if the object list of S is empty and the child segment on that side exists: move object 1 to the center of the child, then store the distance between object 1 and object 0 in `dist[k]` (array of 4 at 0x3B9CE4) and in 0x3B9CF8.
- The 4 distances are never read and never written to the buffer. The only writes to the buffer are in step 7.

So the robot map (a 3-bit "which side leads to the player" code per segment pair, read by 0x9816B) was removed or never finished. Only its distance loop is left.

### Guess

- The planned robot map stored one of the 4 horizontal sides (5, 2, 4, 0 are the sides that are not floor and ceiling) for each segment pair, as a direction to walk. The 3-bit lookup would give that side.
- The builder was run from the level editor or from a debug key that is not in the release build.

## Scripts and NPCs

The byte code, the interpreter, the conversation window and the mission
flags are in [scripts.md](scripts.md), which corrects the statement and
condition tables of this section. The loader, the NPC list and the level
rule below are still the reference.

This section covers the script and dialogue code of DTUGAME.EXE 1.3. The debug
strings of this code start with `RLH:`. The assert file name is `dialogue.c`
(string at 0x131B8E). The code is at about 0xA6500 to 0xAE400. All addresses
are linear addresses in `dtu_flat.bin`.

Facts are marked as follows.

- Verified (code): read from the disassembly.
- Verified (data): checked against GABE.RLH, GABE.DAT and the 35 .LVL files.
- Guess: a reading that the code or data suggests but does not prove.

Coverage. The loader, the object binding, the entry points, the full statement
dispatch, the condition evaluator and the say loop are read. Most statement
handlers are read in outline only. The dialogue window functions (0xB1xxx,
0xB2FA2), the spell functions (0x8CFFF, 0x90CEF) and the sound functions
(0x2Dxxx, 0x2Exxx) are not read.

### Files

Verified (code and data).

- The scripts are in `GABx.RLH` (index) and `GABx.DAT` (data). The layout and
  the encryption are in `docs/formats/jrg.md`.
- The file name templates are `gabe.dat` (0xA6467) and `gabe.rlh` (0xA6477).
  The string `efgisnm` (0xA6470 and 0xA6480) holds one letter per language.
  The code copies both strings to the stack and writes
  `efgisnm[language]` over the fourth character of the name. The language
  number comes from 0xAEC4A. The open call is 0xA08C2 with mode string 0x131A40.
- The .RLH index has no header. A record is 8 bytes: u16 id, u16 size, u32
  offset in the .DAT file.
- GABE.RLH has 87 records. The ids are 1 to 8, then groups of `level * 10 + n`
  (20 to 26, 30 to 33, ... 320 to 323), then 508 to 511 (0x1FC to 0x1FF).

Relation to other files. Verified (code): this code does not read .SAY files.
.SAY lines are shown by triggers through `say_index` (see `tig.md`). The only
link is the trigger field `object_num` (see "Entry points" below). The stream
dialogue code (`str_dlg.c`, 0xA2AB7 and on) was not traced to the script code.

### Loader

| Address | Function | What it does |
|---|---|---|
| 0xAD84D | init dialogue system | Prints "RLH: Init dialogue system". Clears the state. Reads GABx.RLH record by record (8 bytes each) into table1 until a read fails. Reached by a jump at 0x40202 |
| 0xAD5BF | level init | Called from 0x494C9. Frees old scripts (0xAD992). Opens GABx.DAT (handle in 0x145521). Walks all objects and registers NPCs (see below). Loads scripts 0x1FC, 0x1FD, 0x1FE, 0x1FF. Closes the file. Prints "found %d objects, %d NPCs and %d scripts" |
| 0xAB96E | find script | Linear search of table1 for an id (low 9 bits). Sets the current slot from bits 9 to 12. Returns the table1 index or -1 |
| 0xAB9C9 | load script | "Script %d not in table1" if the search fails. Returns at once if bit 13 (loaded) is set. Fails with "Too many GABs" if 20 scripts are loaded. Allocates `size` bytes, seeks to `offset`, reads, decrypts (0xA74F8), stores the slot number and the loaded bit in the table1 id word |
| 0xA74F8 | decrypt | See `jrg.md` |
| 0xAD983 | close dialogue system | Called from 0x42749 |

"table1" is the in-memory copy of the .RLH index. The messages "not in table1"
(loader) and "not in table2" (runner, 0xABC9D) both mean that the search in
this one table failed.

Globals. Verified (code).

| Address | Size | Meaning |
|---|---|---|
| 0x3CE0FC | 8 * 100 | table1: the .RLH records. In memory the id word also holds the slot (bits 9 to 12) and the loaded flag (bit 13) |
| 0x14552C | u8 | number of table1 records |
| 0x3CE41C | ptr * 20 | start of each loaded script, by slot |
| 0x3D03AC | ptr * 20 | end of each loaded script, by slot |
| 0x14552B | u8 | number of loaded scripts (max 20) |
| 0x14552A | u8 | current slot |
| 0x145526 | ptr | script read pointer |
| 0x145525 | u8 | IF nesting depth |
| 0x145532 | u16 | current label |
| 0x145537 | ptr | current object |
| 0x145536 | u8 | current run mode (0 to 4) |
| 0x145534 | u8 | 1 while the dialogue window is open |
| 0x145535 | u8 | value written to object flag 0 after the run |
| 0x145531 | s8 | number of collected replies minus 1 |
| 0x3CE46C | | reply table (target label word and text per reply, max 10) |
| 0x3D42CC | 7 * 100 | NPC list (see below). Count in 0x145520 |
| 0x3D42C8 | ptr | temporary object of 0xAC055, or 0 |
| 0x3D03FC | 0x31E * 20 | buffers for the text alternatives |
| 0x3D4588 | 0x31E | expanded text |

### How an object gets its script

Verified (code and data).

- The script id is in the low 9 bits of the object field at +0x1B0.
- The level file stores this field (object record offset 201, level versions
  above 40, see `lvl.md`). The loader copies it.
- In the level files every robot has 0x400000 set. Plain monsters have id 0.
  Talking characters have an id from the .RLH file. Example: KOBOLDS.LVL uses
  ids 20 to 26, THIEF0.LVL uses 50 to 55.

Layout of the object field +0x1B0. Verified (code).

| Bits | Meaning |
|---|---|
| 0 to 8 | script id. 0 = no script |
| 9 | "never turns hostile" flag. Also makes the object count as a talker at level init |
| 10 to 19 | resume label (statement 0xDF, condition 0xDF) |
| 20 to 27 | object flags 0 to 7 (statements 0xC5 and 0xC6, conditions 0x00 to 0x07) |
| 24 to 27 | 4-bit object variable (statement 0xE0, value 0xE0). Overlaps flags 4 to 7 |
| 28 to 31 | talk counter (condition 0x97). Plus 1 after each run in mode 0, max 15 |

Object flags with a fixed meaning. Verified (code).

| Flag | Bit | Meaning |
|---|---|---|
| 0 | 20 | The AI calls the script when the object reaches the end of its path (mode 2). Set by statement 0xE5 |
| 1 | 21 | Wary. The object talks only if `rand % 20 + 1 <= player stat at 0x1D33DB`. Else it shows a line from label 0x3E9 |
| 2 | 22 | Hostile. The object does not talk. It shows a line from label 0x3EA |
| 3 | 23 | Blocks the change to hostile. Selects label 0x3EB. Setting it calls 0x160AB(object, 5) |

Level init per object (0xAD786 loop, then 0xAC431). Verified (code).

1. For a robot (type 2) call 0xAC7B2 (see "Creature setup by name").
2. Skip the object if it is not a robot and its script id is 0.
3. Skip it if 0xAC3C3 finds the same signature and id in the NPC list.
4. 0xAC431 adds it to the NPC list. The limit is 100 ("I can't handle more
   than %d NPCs in a level").
5. If `Robot_info[id] + 0x99` has bit 0x20 (undead), the name is matched
   against 9 prefixes (table 0x145602: `ske_`, `zombie`, `ghoul`, `banegard`,
   `ghast`, `mummy`, `gskel`, `lich`, `sfiend`). The index goes to the low
   nibble of the list entry. This is for turn undead.
6. The script id is cleared if `id / 10` is not the current level number
   (player byte at 0x1D338D). Exceptions: 0, 0x37, 0x3F, 0x1FC to 0x1FF.
   Verified (data): ids 55 and 63 are the only level ids used in more than
   one level (THIEF0, THIEF1, THIEF2).
7. If the id is 1 to 0x3E7 the script is loaded.
8. The object is peaceful if its script has a say statement (0xCD) or bit 9 is
   set. Then the anger byte is 0, flag 2 is cleared and +0x1A2 is -1.
9. Else the object is hostile: anger byte 4, flag 2 set, +0x1A2 is 0.

NPC list entry (7 bytes, base 0x3D42CC): u16 object number at +0, sort index at
+2 (0xADB7D sorts by the low nibble of +3), at +3 the turn undead class (low
nibble) and bit 4 (undead name found).

The "anger" byte is at `0x153EB0 + object_number * 47`. Values are 0 to 5.
Statements 0xC2 and 0xC3 change it. When it reaches 5 the object turns hostile
(0xA9418) unless flag 3 is set. The name "anger" is a guess.

### What makes an object an NPC

Verified (code).

- There is no NPC object type. In this code "NPC" means any entry of the NPC
  list: every robot of the level and every other object with a script id.
- A character that talks is a robot with a script id whose script has a say
  statement. A monster is a robot with id 0 or with a script without a say
  statement.
- 0xA9418 makes an object hostile: anger 4, flag 2 set, flag 3 and bit 9
  cleared, +0x1A2 = 0, `ai +3` = 2, +0x170 = 5, then 0x160AB(object, 4) and
  the morale function 0x10ADF.
- The message "Cannot gate in an NPC until you kill one" (0x134A6, function
  0x13407) is the Descent 1 gate limit. It counts robots whose byte at +0x7F
  is -1 and refuses if the count is above `2 * difficulty + 3`. It has
  nothing to do with scripts. After a gate the function sets +0x1B0 to 0 and
  calls 0xAC431, so a gated robot enters the NPC list as hostile.

Critter table at 0xA504A. Verified (code and data in the exe).

- 191 records of 26 bytes. A record with a negative byte at +0x14 ends it.
- +0x00: name, 16 bytes (`banegard`, `beetle`, ... `fmtnpc`, `Daspel`).
- +0x10: pointer to a pointer to the display name.
- +0x14 to +0x19: six bytes. 0xA7E97 (called from 0x1F3C9) copies them to the
  object: +0x14 to word +0x74, +0x15 to word +0x76, +0x16 to +0xE2, +0x17 to
  +0xE4, +0x18 to +0xE3, +0x19 to +0xE5. Guess: THAC0, armor class, mage
  level and spell points, priest level and spell points.
- 0xA7E47 finds a record by name. It prints "NPC [%s] not found!" on failure.
- 0xA7F03 returns the display name of an object (36 callers). For a robot it
  looks up the robot name (table 0x1B50EC, 32 bytes per robot id) in this
  table. On failure it prints "NPC name [%s] not found" and returns the robot
  name.
- Scripts address a kind of critter by the index in this table (one raw byte).
  0xA7F9B and 0xA8016 turn the index into a robot id by a prefix compare of
  the names.

### Entry points

`run_script(object, mode)` is 0xABBE5. Verified (code).

| Mode | Caller | When |
|---|---|---|
| 0 | 0xAC155 (from 0x951D1 in the player action function 0x95050), 0xAC055 | The player talks to the object |
| 1 | no caller found | Shows a line from the status label |
| 2 | 0x174FC (AI path code, function 0x17411) | The object reached the end of its path and flag 0 is set |
| 3 | 0x93EB3 (trigger code, function 0x93B29) | A trigger with `object_num` above 1 fired |
| 4 | 0xAA9E3 (statement 0xEE) | Another script calls this object's script |

Talk (0xAC155). Verified (code).

1. Return if the player object field +0x170 is 6, 7, 11 or 12.
2. 0xB30FD(player object, 7, 30) finds the target. The object number is at
   +0x1C of the result.
3. If the target has script id 0 and is a robot, show the text at 0x396C24.
4. Return if the target is dead (0xA66C9).
5. Special case: if the current item of the player has type 5 and id 13, set
   global flags 0x81 (if the robot has bit 8 at `Robot_info + 0xD9`) and 0x8F,
   run script 0x1FD, and stop if global flag 0xA0 is then set.
6. Call `run_script(target, 0)`.

`run_script` in order. Verified (code).

1. Fail if the object has script id 0, or is dead (modes 0 and 2, ids below
   0x1FC), or the script is not loaded.
2. Apply the level rule of step 6 above. If the id is from another level,
   clear the id and make the object hostile.
3. Set the current object and mode.
4. 0xAAA32 picks the start label (see below). Stop if it returns 0.
5. Mode 0 with a live robot and id below 0x14A: turn the robot to face the
   player, with frames drawn in between.
6. If the object byte +0x89 has bit 0x40, show "%s tries to speak, but
   nothing happens." and stop. Guess: silenced.
7. Set flag 0. Run the interpreter 0xAAE3E.
8. Write 0x145535 to flag 0. In mode 0 add 1 to the talk counter.
9. Close the dialogue window if it is open (0xA74D0).
10. If the object has less than 1.0 hit points, kill it (0xA6ACB).

Start label (0xAAA32). Verified (code).

- Status label: 0x3EA if flag 2, else 0x3EB if flag 3, else 0x3E9 if flag 1,
  else 0x3E8.
- If the status label is 0x3EA, or 0x3E9 and the wary roll fails: go to that
  label, show one of its lines half of the time (0xA8D72), play the robot
  sound for 0x3EA, and do not run the script.
- Modes 0, 3, 4: go to the resume label (bits 10 to 19) if it is not 0. Else
  take the first label below 0x3E8 in the script.
- Mode 1: go to the status label and show one of its lines.
- Mode 2: go to the label of the form `CB E7 <segment>` that matches the
  segment of the object (0xA78CF).

Verified (data): 54 of the scripts start with label 0x3E7. Labels 0x3E9
(17 scripts), 0x3EA (19), 0x3EB (15) and 0x3EC (7) hold only text lines.

`run_script_id(object, id)` is 0xAC055 (14 callers). With object 0 it creates
a temporary object at the player, gives it the id and runs it in mode 0. The
engine uses it for the global scripts: 0x1FC (0x3846A), 0x1FD (seven
callers), 0x1FE (0x37DEE), 0x1FF (0x429DA, after "Ready or not, here we go!").
Verified (data): script 0x1FC raises one ability score by class.

### Script byte code

Verified (code and data).

- A script is a byte stream. Bytes of 0x7A and above are codes. Text and
  numbers are NUL terminated ASCII strings.
- A number is a hex string, read with `sscanf("%hx")` (0xA76B9). Example:
  `3e7`.
- A text operand is a list of one or more strings. The list ends at the first
  byte of 0x7A or above. The game picks one string at random (0xA77BE, max 20).
- Inside text the bytes 0x09 to 0x11 are replaced (0xA7582): 0x09 = player
  name, 0x0A and 0x0B = a word for the player class, 0x0C and 0x0D = a word
  for the player race, 0x0E to 0x11 = a word for the player sex.
- A script is a list of blocks: `CB <label> statements CC`.

Number operand (0xA7DF0).

| Form | Value |
|---|---|
| hex string | the number |
| `D2 a b` | random. If b is 0: 1 to a. Else: a to b |
| `E0` | 4-bit variable of the current object |
| `E0 15 n` | 4-bit variable of object number n |

Operand bytes.

| Bytes | Meaning |
|---|---|
| 0x15 | prefix: the next number is an object number (index in Objects) |
| 0x7A to 0x7E | arithmetic: add, subtract, multiply, divide, set (0xA8DB2) |
| 0x7F to 0x84 | compare: <, <=, ==, !=, >, >= (0xA7BEE) |
| 0xA4 to 0xA8 | item class: WEAPON (type 5), ARMOR (15), MISC (16), SPELL (17), GOLD |
| 0xA9 to 0xAC | facing, value minus 0xA9 |
| 0xAD to 0xB2 | segment side 0 to 5 |
| 0xCA | "all" or "any" in 0xC3 |
| 0xEF | right side of a compare: player word at 0x1D33BD |

### Statement codes

Dispatch: 0xAAE3E, loop head at 0xAB5AB. The loop ends when the read pointer
reaches the end of the script. The code byte passes through 0xA8CA6 (0xF5 to
0xF8 become 0xF4, 0xFA to 0xFD become 0xF9). Then
`jmp cs:[(code - 0x99) * 4 + 0xAAC6E]` (97 entries). Unknown codes print
"Don't know statement code" and end the run. Verified (code). The "Seen"
column is the count in GABE.DAT.

| Code | Handler | Operands | Meaning | Seen |
|---|---|---|---|---|
| 99 | 0xAB4D5 | op n | player word 0x1D33D3. Handles the step to 18 with a percent roll (0x1D33D5). Guess: strength | 12 |
| 9A | 0xAB5F1 | op n | player word 0x1D33DF | 78 |
| 9B | 0xAB61F | op n | player word 0x1D33DD, then adds 0x8C01B(value, ...) to word 0x1D33BD | 16 |
| 9C | 0xAB683 | op n | player word 0x1D33D9 | 29 |
| 9D | 0xAB6B4 | op n | player word 0x1D33D7 | 28 |
| 9E | 0xAB6E7 | op n | player word 0x1D33DB | 55 |
| 9F | 0xAB71A | op n | player level (the higher of words 0x1D33CB and 0x1D33CD, plus 1), set through 0x8BA3A | 15 |
| A0 | 0xAB7AA | op n | player hit points (0x1D336F). A loss goes through the damage function 0x29A9C. A gain is capped at 0x1D33BB | 49 |
| B3 to BD | 0xAB2C4 | none | set the animation state of the object to code minus 0xB3 (0xA9E2B, object +0x170). State 9 also sets the hit points to 0. States 6, 7, 8 play a robot sound | 75 |
| BE | 0xAAE51 | text | narration. In the dialogue window if it is open, else as a HUD message. Opens the window in mode 0 (0xA79FD) | 268 |
| BF | 0xAAE5B | class id [E4 seg] [E3 seg] | give an item to the player (0xA8F77). `E4 ffff` creates it (0xB6390). `E4 seg` takes the item object that lies in the segment. Without E4 the object must carry it. GOLD adds to 0x1D33F7 | 290 |
| C0 | 0xAAE77 | class id [E4 seg] [E3 seg] | take an item from the player (0xA9191). The object then carries it, or `E3` puts it into a segment (0xFFFE: into a random robot or dressing object). GOLD subtracts if the player has enough | 64 |
| C1 | 0xAAE93 | op n | experience points (0xA8E77). Only add and subtract. Split in two for a dual class player | 57 |
| C2 | 0xAAE9D | op n | anger of the current object, capped at 5. 5 makes it hostile. 0 clears flag 2 | 61 |
| C3 | 0xAAF8C | who range op n | anger of other NPCs (0xA94CE). who = critter table index or CA. range = distance or CA | 38 |
| C4 | 0xAAF96 | seg side | operate the trigger of that wall (0xA9755). Calls check_trigger 0x939AD(seg, side, 2, object) or 0xA8324 if the trigger is off | 96 |
| C5 | 0xAAFAA | k [15 n] or F1 n | set object flag k (0 to 7), or set global flag n | 644 |
| C6 | 0xAAFAA | same | clear the flag | 133 |
| C7 | 0xAB242 | label | goto (0xA7872 searches the script from its start) | 982 |
| C8 | 0xAB256 | none | end. Goes to the exit label (0x3EC, or 0x3EA, 0x3EB, 0x3E9 by flag) and shows one of its lines half of the time (0xA7A57) | 699 |
| CC | 0xAB256 | none | end of block. Same handler as C8 | 1540 |
| CD | 0xAB469 | text, then reply list | say. See below | 1816 |
| D3 | 0xAB260 | sound volume [D8] | play a sound (0xA97B3). Volume 1 to 10. Sounds 0x2710 to 0x271B come from the player record (0x1D4E23 and on). Sound 0xFFFF starts a sequence with the movie FLAM1.MVE | 48 |
| D4 | 0xAB274 | sound volume object n [D9] | sound at an object (0xA9BE6, 0x2E627) | 9 |
| D5 | 0xAB288 | sound, more numbers [D9] | sound in a segment (0xA9CBE, 0x2E94B). Operands not decoded | 2 |
| D6 | 0xAB29C | object | stop the sound of an object (0x2E816) | 2 |
| D7 | 0xAB2B0 | seg | stop the sound of a segment (0x2EB77) | 1 |
| DA | 0xAB2DD | seg facing | teleport the current object. seg 0xFFFF removes it (0xA9F58) | 44 |
| DB | 0xAB2F1 | level seg facing | teleport the player (0xAA1FC, level change through 0x59C3C). seg 0xFFFF runs the special sequence 0xAA3F3: the player loses his items and goes to level 7 | 69 |
| DC | 0xAB32E | object seg facing | teleport another object. seg 0xFFFF removes it | 109 |
| DD | 0xAB367 | class id seg1 seg2 | move the item that lies in seg1 to seg2. seg2 0xFFFF deletes it | 10 |
| DE | 0xAB382 | who seg facing | summon a critter (0xAA14D). who = critter table index (one byte). Creates it with 0x1374D. who of 0xBE and above calls a stub | 45 |
| DF | 0xAB3C0 | [15 n] op n, or nothing | set the resume label (0xAA670). With no operand: the current label | 216 |
| E0 | 0xAB3CA | [15 n] op n | 4-bit object variable | 233 |
| E5 | 0xAB435 | seg ... E6 | walk a path through the segments (0xAA79F, 0xAA74E, 0x16FD9). Sets flag 0 | 16 |
| EA | 0xAB47B | n object | n up to 50. Calls 0x8CFFF. Guess: cast spell n on the object | 130 |
| EB | 0xAB48F | n seg | n up to 50. Calls 0x90CEF. Guess: cast spell n at the segment | 87 |
| EC | 0xAB4A3 | n | wait n timer ticks. The game keeps drawing (0x4391D) | 206 |
| ED | 0xAB4B7 | object, or C4 seg side | kill the object (0xA6ACB), or switch the trigger off (0xA83BD) | 39 |
| EE | 0xAB4C1 | object | run the script of another object in mode 4, then go on | 10 |
| F2 | 0xAB43F | condition F3 | IF. See below | 1931 |
| F4 to F8 | 0xAB449 | none | ELSE of depth 1 to 5. Reached after a true branch: skip to the ENDIF | 809 |
| F9 to FD | 0xAB45E | none | ENDIF of depth 1 to 5. Depth minus 1 | 1931 |

All other codes from 0x99 to 0xF9 go to the error at 0xAB94B.

### Conditions

`F2 term [E1 term | E2 term]... F3`. 0xA8BDB evaluates it. E1 is AND, E2 is
OR, strictly left to right. F3 adds 1 to the depth. If the result is false,
0xA8C49 skips to the ELSE or ENDIF byte of this depth. Verified (code and
data): ELSE is `0xF4 + depth - 1`, ENDIF is `0xF9 + depth - 1`.

A term (0xA8516) may start with E9 (NOT). A value term is followed by a
compare byte and a number.

| Code | Kind | Meaning |
|---|---|---|
| 00 to 07 [15 n] | flag | object flag |
| 85 to 8A | flag | player word at 0x1D33FF equals code minus 0x85 |
| 8B to 8E | flag | player class (bytes 0x1D33B3 and 0x1D33B4) equals code minus 0x8B |
| 8F to 94 | flag | player race (byte 0x1D33B6, or 5 if 0x1D33FB bit 0) equals code minus 0x8F |
| 95, 96 | flag | player sex (byte 0x1D33B7) equals code minus 0x95 |
| 97 [15 n] | value | talk counter |
| 98 | value | word at `0x153ED8 + object * 47`, plus 500 |
| 99 to 9E | value | the player words of statements 99 to 9E |
| 9F | value | player level |
| A0 | value | player hit points |
| A1 | value | 0x8445A() + 1. Guess: level number |
| A2 | value | number of the current object |
| A3 | value | run mode |
| C2 | value | anger of the current object |
| C3 who range cmp n | flag | true if any matching NPC in range has that anger (0xA80F6) |
| C4 seg side | flag | the trigger of that wall is on (trigger +0x0E) |
| C9 n | flag | object n is dead or gone (0xA66C9) |
| D0 class id | flag | the current object carries the item (+0x7C, +0x7D, +0x7E) |
| D1 class id | flag | the player has the item (0xB6BB3). GOLD: the player has at least that much |
| DF [15 n] | value | resume label |
| E0 [15 n] | value | 4-bit object variable |
| E7 | value | segment of the current object |
| E8 seg side | value | `fire_count` of the trigger (trigger +0x2E) |
| F0 n | flag | byte at 0x185FEC + n is not 0. Guess: segment n was visited |
| F1 n | flag | global flag n |
| number | value | a number, for example `D2 2 0 81 1` |

The names class, race and sex are guesses from the value ranges and the text
tokens.

Global flags. Verified (code). 0xADAFE(n) reads and 0xADB3B(n, value) writes
bit `n - 1` of a 32-byte array in the player record at 0x1D4D4F. Flags 0x21 to
0x33 are also copied to the dwords at 0x13CDD4 to 0x13CE1C.

### Say and replies

0xA8CBC handles `CD`. Verified (code and data).

1. Open the dialogue window if needed (0xA74AA).
2. Show the text as speech (0xA7A22(1), 0xB1F5F).
3. Read the reply list. Dispatch by compare chain:

| Code | Handler | Meaning |
|---|---|---|
| CF | 0xA7B56 | `CF label text`: add a reply. Max 10. Shown with 0xB1D99 |
| F2 | 0xA8C3A | IF, as above. Replies can be conditional |
| F4 to F8 | 0xA8C49 | ELSE |
| F9 to FD | | ENDIF |
| CE | 0xA7ADC | end of the list. 0xB2FA2 returns the choice. Goto the label of the reply. A negative choice ends the script (0xA7A57) |

Any other code prints "Don't know say code" and ends the run. `CD 00 CE` with
no replies is used after a `BE` text to wait for the player.

### Not covered

- The meaning of the player words of 99 to 9E and of 85 to 8A.
- Object flag 3, the byte at +0x1A2 and the word at 0x153ED8.
- The handlers 0xAC7B2 and 0xACCDB of the level init.
- The operands of D5 and the optional bytes D8 and D9.
- The dialogue window code and how the choice is read.
- Whether speech audio is tied to a say statement.
- Mode 1 has no caller that passes a constant 1.
