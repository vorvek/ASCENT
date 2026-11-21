# Frame rate dependence in the object, physics, AI, weapon and render files

This list covers object.c, physics.c, collide.c, fvi.c, gameseg.c, laser.c, fireball.c,
ai.c, aipath.c, wall.c, switch.c, fuelcen.c, cntrlcen.c, hostage.c, morph.c, lighting.c
and render.c. Nothing in the list is changed. The original game ran at about 20 to 30
frames per second. `FT` is 1/64 second.

The list comes from a search for `FrameCount`, `rand()`, per-frame counters and fixed
steps, and from a read of the main per-frame functions. It is not a proof that no other
place exists.

## A. Counters and tests on FrameCount

| File | Function | Behavior |
|---|---|---|
| laser.c | `track_track_goal` | A homing weapon that lost its target looks for a new one only when `(objnum ^ FrameCount) % 4 == 0`: each 4th frame. |
| laser.c | `Laser_do_weapon_sequence` | A slow weapon (speed below 2.0) is deleted only when `(FrameCount ^ signature) & 3` is 0: each 4th frame. |
| ai.c | `do_ai_frame` | `obj_ref = objnum ^ FrameCount`. `obj_ref & 1` gates the turn towards the player in several modes. `obj_ref & 3` gates the visibility checks in the states AIS_RECO and others. A robot turns and looks more times per second at a higher frame rate. |
| ai.c | `do_ai_frame` | `SKIP_AI_COUNT` counts frames. `phys_apply_rot` (physics.c) sets it to 2 when a robot is hit. |
| ai.c | `do_ai_frame` | `retry_count` and `consecutive_retries` count physics retries per frame. More than 3 changes the robot mode. `consecutive_retries /= 2` on each frame without a retry. |
| ai.c | `move_around_player` | The direction comes from `FrameCount` with a mask. The code scales the mask with FrameTime only down to 1/32 second. Above 32 frames per second the direction changes faster. |
| ai.c | `move_away_from_player` | The side slip direction uses `(FrameCount + 3*objnum) >> 5`: it changes each 32 frames. |
| ai.c | `do_boss_stuff` | `Boss_hit_this_frame` is a flag per frame. |
| aipath.c | `maybe_ai_path_garbage_collect` | The intervals between garbage collections are 1, 16 and 256 frames (`Last_frame_garbage_collected`). |
| wall.c | `remove_obsolete_stuck_objects` | Checks one slot per frame: `FrameCount % MAX_STUCK_OBJECTS`. |
| fuelcen.c | `robotmaker_proc` | A debug message limit of 20 frames (`FrameCount_last_msg`). No effect on the game. |
| cntrlcen.c | `do_controlcen_frame` | The reactor looks for the player only when `FrameCount % 8 == 0`, until it is hit or sees the player. |
| lighting.c | `apply_light`, `set_dynamic_light` | Dynamic light is computed for half of the vertices per frame (`FrameCount & 1`). Visual only. |
| render.c | `render_frame` | Increments `FrameCount`. `RL_framecount` in `render_start_frame` marks the rotated points of one frame (no time dependence). |
| object.c | `start_player_death_sequence` | `Killed_in_frame = FrameCount` (used to detect a double kill in one frame). |

## B. Random checks per frame

These checks compare `rand()` with a constant or happen one time per frame, per object or
per event. A check that compares `rand()` with a multiple of FrameTime is correct on
average, but it saturates or loses resolution at extreme frame rates.

| File | Function | Behavior |
|---|---|---|
| ai.c | `ai_turn_randomly` | `rand() > 0x7400` per call. |
| ai.c | `ai_move_relative_to_player` | `rand() < 8192` per call: 25 percent per processed frame to evade. |
| ai.c | `do_ai_frame` | `(rand() > 0x4000)` per frame decides if a robot that does not see the player computes visibility. |
| ai.c | `do_ai_frame` | `rand() < FrameTime/4`, `rand() < FrameTime/8`, `rand() < FrameTime*player_visibility`: scaled with FrameTime (correct on average). `FrameTime/8` becomes 0 at very high frame rates. |
| ai.c | `do_boss_dying_frame` | `rand() < FrameTime*16`, `rand() < FrameTime*8`: scaled with FrameTime. |
| ai.c | `ai_fire_laser_at_player` | `rand() > 16384` and the aim error are per shot (not per frame). The lead of the shot uses `speed * FrameTime`. |
| ai.c | `add_awareness_event`, `create_awareness_event` | Random per event. The number of events grows with the number of weapon collisions, not with the frame rate. |
| aipath.c | `attempt_to_resume_path` | `rand() > 8192` per call. |
| aipath.c | `ai_follow_path` | `rand_num = rand()` per frame for the choice at the end of a path. |
| cntrlcen.c | `do_controlcen_dead_frame` | `rand() < FrameTime*4`: scaled. |
| cntrlcen.c | `do_controlcen_frame` | `rand() < 32767/4` per shot (not per frame). |
| object.c | `dead_player_frame` | `rand() < FrameTime*4`: scaled. |
| object.c | `create_small_fireball_on_object` | `rand() < 8192` per fireball for the sound. |
| fuelcen.c | `controlcen_proc` | Adds a random value to the rotation velocity of the player on EACH frame of the countdown. The shake grows with the frame rate. |
| collide.c | `scrape_object_on_wall` | On each frame of contact with a volatile wall: a bump with force 8.0 and a random rotation velocity. The damage itself uses FrameTime. |
| fireball.c | `do_exploding_wall_frame` | The number of fireballs comes from the elapsed time (correct). The positions use `rand()`. |
| laser.c | `do_laser_firing` | `Laser_offset` and the vulcan spread use `rand()` per shot (not per frame). |

## C. Fixed steps and per-frame factors

| File | Function | Behavior |
|---|---|---|
| physics.c | `do_physics_sim`, `do_physics_sim_rot` | Drag and thrust are applied in steps of FT (1/64 s), then one partial step that is linear: `velocity *= 1 - k*drag`. Above 64 frames per second only the partial step runs. The result is close to the original but not the same, and the fixed point products lose bits when FrameTime is small. |
| physics.c | `do_physics_sim` | `frame_vec = velocity * sim_time` is truncated. A slow object gets a zero vector at a high frame rate. The loop then stops, and the code after it sets the velocity from the real movement (`moved_vec * 1/FrameTime`), which is zero: the object stops. |
| physics.c | `do_physics_sim` | The velocity after the move is `moved_vec / FrameTime`. The error of the division grows when FrameTime is small. |
| physics.c | `do_physics_sim` | The retry limit is per frame: 3 passes for an object, 8 for the player. |
| physics.c | `do_physics_sim` | BUMP_HACK moves a stuck player `size/5` towards the segment center on each frame. |
| physics.c | `do_physics_sim_rot` | `tangles = rotvel * FrameTime` goes into a 16-bit `fixang`. A slow rotation is truncated to zero at a high frame rate. |
| physics.c | `do_physics_align_object`, `set_object_turnroll` | Use `ROLL_RATE * FrameTime` (correct), with truncation to `fixang`. |
| physics.c | `physics_turn_towards_vector` | Sets the rotation velocity from a rate. No FrameTime, but the callers call it per frame or per hit. |
| collide.c | `collide_object_with_wall` (from physics.c) | The hit speed is `wall_part / moved_time`. Wall damage and wall hit sounds happen per collision event, and the number of events per second follows the frame rate when the player pushes against a wall. |
| ai.c | `move_towards_vector` | When the robot does not face its velocity: `velocity = velocity/2 + goal * FrameTime*32`. The halving happens on each frame. |
| ai.c | `move_towards_vector`, `move_around_player`, `move_away_from_player` | The acceleration uses FrameTime (correct), then the speed is clamped per frame. |
| ai.c | `ai_turn_towards_vector` | Blend of the forward vector with `FrameTime * AI_TURN_SCALE / rate` and a new normalization on each frame. This is not linear in FrameTime. The test `dot < F1_0 - FrameTime/2` also depends on FrameTime. |
| ai.c | `ai_frame_animation` | `delta * FrameTime * DELTA_ANG_SCALE` in 16-bit angles: truncation at a high frame rate. |
| ai.c | `do_ai_frame` | `time_since_processed = -((objnum & 3) * FrameTime) / 2` spreads the work over frames. Far robots are processed by time (correct). |
| aipath.c | `ai_follow_path` | `vel_scale = F1_0 - FrameTime/2` (minimum 1/2) scales the velocity on each frame in the mode AIM_RUN_FROM_OBJECT. |
| aipath.c | `ai_follow_path`, `player_follow_path` | `threshold_distance = speed * FrameTime * 2 + 2.0`. The distance at which a robot takes the next path point depends on FrameTime. |
| laser.c | `Laser_do_weapon_sequence` | Homing: the code adds the unit vector to the target to the unit velocity one time per frame (two times for a weapon that is not a polygon model) and normalizes. The turn rate is per frame: a homing weapon turns much faster at a higher frame rate. |
| laser.c | `Laser_do_weapon_sequence` | Homing speed-up `max_speed * FrameTime/2` and life loss `absdot*16 * FrameTime` use FrameTime (correct). The life loss depends on the turn above. |
| laser.c | `homing_missile_turn_towards_velocity` | Blend with `FrameTime * HOMING_MISSILE_SCALE` and normalization per frame: not linear in FrameTime. |
| laser.c | `do_laser_firing_player` | The fire timer runs on GameTime (correct). It resets when the last shot is older than `2*FrameTime`. The plasma delay parameter is `FrameTime/2`. More than one shot can leave in one frame at a low frame rate. |
| fuelcen.c | `controlcen_proc` | `size` first uses `FrameTime & 0xF`, then the next line overwrites it. No effect. |
| lighting.c | `compute_object_light` | Ramp with `LIGHT_RATE * FrameTime` (correct). |
| object.c | `draw_cloaked_object` | The cloak pulse uses static variables that all cloaked objects share. The pulse rate grows with the number of visible cloaked objects. The timer itself uses FrameTime. |
| object.c | `object_move_one` | `lifeleft -= FrameTime` (correct). One physics step per frame for each object. |
| render.c | `draw_3d_reticle`, `render_frame` | `flash_ang` and `Zoom_factor` use FrameTime (correct). |
| wall.c, switch.c, morph.c, hostage.c, fireball.c | door, trigger, morph, hostage and explosion timers | Use FrameTime (correct). |

## D. Related, not frame rate

- `rand()`: the code expects the range 0 to 32767 of Watcom C (`(rand() * n) >> 15`,
  `rand() - 16384`). The Microsoft C runtime has the same range. glibc returns 31 bits,
  which breaks these expressions and can index outside arrays. The project needs one
  `rand()` with the range and the sequence of Watcom C for all platforms.
- `qsort()` in render.c (`build_object_lists`) and aipath.c (`ai_path_garbage_collect`):
  the order of equal elements depends on the C library.
