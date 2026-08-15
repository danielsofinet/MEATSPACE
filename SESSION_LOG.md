# Session log

Newest entry at the top. Read the top entry to resume.

---

## 2026-08-15 — Session 3: the character, cosmetics, and animation

### Where we are

**MEATSPACE has a real character.** Built in MPFB, rigged to the UE5 Mannequin skeleton, with
hair, eyes, eyebrows, eyelashes and teeth, wearing a t-shirt, with runtime skin tone and hair
colour — running in game, strafing correctly.

The entire modular cosmetics pipeline is proven end to end.

### Built this session

**Appearance system** (`MsAppearanceComponent`, `MsAppearanceTypes.h`)
- Body split into **material sections**; equipping a garment **hides the section it covers**,
  which is how clipping is solved — the jacket replaces the torso rather than fighting it
- **Skin tone** as one material parameter (`SkinTone`) pushed to every section at once
- **Garment slots**: Head, Torso, Arms, Hands, Legs, Feet, Back. Each garment is a skeletal
  mesh on the same skeleton driven via `SetLeaderPoseComponent` — animates from the body's
  bones, no rigging, no anim blueprint, negligible cost
- **Garment tints** (`Tint` parameter) so one mesh covers many colourways
- **`GarmentHidesSkin`** per slot — hair must NOT hide the head section or it deletes the face
- All in one replicated struct, so co-op peers get a coherent appearance in one update

**Animation** (`MsAnimInstance` + `ABP_MsCharacter`)
- C++ exposes ground speed, direction relative to facing, in-air, vertical velocity, smoothed
  turn rate, aim pitch/yaw, and combat state. The graph does no maths.
- `Direction` is the important one: the character faces the camera and strafes, so without it
  every sideways step played a forward run
- `ABP_MsCharacter` (parent `MsAnimInstance`) drives the template's existing
  `BS_Idle_Walk_Run` with `Direction` + `GroundSpeed`. **Strafing now works.**

**Jump is done properly** — a `Locomotion` state machine inside `ABP_MsCharacter`:
Grounded (`BS_Idle_Walk_Run`) → JumpStart (`MM_Jump`) → Falling (`MM_Fall_Loop`) → Landing
(`MM_Land`) → Grounded, plus a Falling → Grounded shortcut when `NOT bIsInAir AND GroundSpeed
> 300` so a running landing does not stumble.

### PICK UP HERE — the animation graph is not finished

Still to layer onto the same state machine:
1. **Aim offset** — drive the upper body from `AimPitch` / `AimYaw` so the character points
   where the reticle is, instead of firing level at a flying clanker
2. **Upper-body sword layer** — blend the melee montages (`MM_Attack_01/02/03`) over
   locomotion on `bIsSwinging`, using a layered blend per bone from `spine_01`
3. **Weapon-specific locomotion** — `ActiveSlot` can switch to the rifle/pistol animation sets
   the template already ships

### Animation Blueprint gotchas

- **Booleans lose their `b` in the editor**: `bIsInAir` appears as **`Is In Air`**
- **My Blueprint hides inherited variables by default** — gear icon → Show Inherited
  Variables. Everything in this ABP comes from the C++ parent, so with it off the panel looks
  empty
- **A new transition has an EMPTY condition and never fires.** It looks completely normal in
  the state machine view. The compiler says so explicitly: *"will never be taken, please
  connect something to Can Enter Transition"* — read those warnings
- One-shot states need **`Automatic Rule Based on Sequence Player`** ticked in the transition's
  Details, and the animation's **Loop unticked**, or the sequence never ends
- Only the state machine may reach **Output Pose**. A leftover direct blendspace connection
  silently bypasses the entire state machine
- The ABP editor previews its own puppet that never jumps — switch the **debug target**
  dropdown to the live game instance to watch real values during PIE

### The character pipeline is documented

`ASSET_REQUESTS.md` now carries the **full proven workflow** — build in MPFB, keep a live
source, bake, merge, **rig before materials**, section, hand-weight the rigid parts, verify,
export. Follow it exactly for body type two. The order *is* the fix: rebuilding this way also
eliminated a thigh deformation that resisted weight painting entirely.

### Gotchas learned (all in ASSET_REQUESTS.md)

- **Garments: Data Transfer modifier, never Automatic Weights.** Bone heat fails *silently* on
  decimated or downloaded meshes — the mesh looks bound and is completely rigid
- **Rigid parts (hair, teeth, tongue, hats): one vertex group, `head`, weight 1.0.** Weight
  transfer is the wrong tool and they visibly lag
- **Never export until it deforms in Blender**, and **check the FBX file size** — a body is
  2–5 MB, near-zero means the mesh was not included
- `SetLeaderPoseComponent` must be called **after** the mesh is assigned, or the garment
  renders and never deforms
- Assets cannot be fitted to a **baked** MPFB human — keep `character_source.blend` alive
- A downloaded t-shirt was **121 MB**; game garments want 2,000–8,000 triangles. Watch the
  GitHub LFS quota (1 GB free, ~250 MB already spent)

### Next steps

1. **Finish the animation graph properly** — state machine, aim offset, sword layer
2. Body type two, using the documented workflow
3. Back to the onboarding level with a real character in it

---

## 2026-08-14 — Session 2: combat, camera, and the onboarding level

### Where we are

**The onboarding segment is playable end to end**: walk out, talk to grandpa, receive the
sword, get reported by a delivery drone, fight off two dropships of clankers, reach the
neighbour, cutscene. All systems are in and working; everything visual is placeholder.

**Daniel is now doing art** — main character, small clanker, dropship, snitch drone. Specs are
in `ASSET_REQUESTS.md` at the project root. This will take a while, so the next session may
open with assets arriving rather than with code.

### The big design decisions made this session

- **NOT wave-based.** The game is not arena survival. Players advance through a district and
  trigger encounters as they move. `MsWaveSpawner` was built on the wrong assumption and now
  survives only as a stress-testing tool. `MsEncounterVolume` is the real pacing mechanism.
- **Development is segment-based.** Build one level end to end rather than systems in the
  abstract. Segment 1 is the onboarding described above.
- **No separate character classes.** Everyone shares one base character; the four "classes"
  (sword / gun / AOE / healing) become **skill point builds**, not distinct characters.
- **Camera is a fixed forced-perspective third-person rig**, mouse-driven, zoomed out, close to
  Megabonk. Values below are tuned and baked.
- **Character customisation**: skin tone + body type at game start. One body built now, second
  later. See `meatspace-character-customisation` memory and `ASSET_REQUESTS.md`.

### Tuned camera values (do not re-derive these)

Pitch 18.7 (limits -35 to 55) · FOV 62.9 · distance 2450 (zoom 1500-4210) ·
dullness 0.6 hip / 0.15 aim · ADS FOV x0.28, distance x0.88 ·
hip reticle (0.076, -0.142) shoulder 0 · aim reticle (-0.040, -0.120) shoulder 110

Found by playtesting with the in-game tuning overlay. **They live in `BP_ThirdPersonCharacter`
Class Defaults now** — a value set there permanently overrides the C++ default, so change them
there, not in code.

### Controls

`WASD` move · `Space` jump · mouse turns camera · `LMB` attack · `RMB` aim (gun only) ·
`1`/`2` sword/gun · scroll swaps weapon · `G` grenade · `E` interact ·
`P` toggles the camera tuning overlay (`U/O` fov, `N/M` dullness, `J/L` look-up limit,
`I/K` shoulder, arrows reticle)

### Built this session

**Combat**
- `MsMeleeComponent` — sword. Three-phase swing (windup/active/recovery), swept *sector* hit
  volume: a vertical cylinder from ground to overhead out to Reach, hit when the swing's
  angular slice passes over a target. Generous by design. 3-step combo: right, left, 360 spin
  finisher; clicking mid-swing queues the next link.
- `MsWeaponComponent` — hitscan gun, server-authoritative, fires along the reticle ray.
- `MsGrenade` + `MsGrenadeComponent` — electrical AOE on `G`, radial falloff, does not hurt
  the thrower.
- `MsHealthComponent` — health, death, and a **partial** shield (splits incoming damage,
  regenerates only after several seconds without being hit). Shared by players and clankers.
- Contact damage, player death and respawn at a player start.

**Clankers**
- `MsClankerBase` — direct vector steering, no NavMesh or AIController, so they work in any
  level with no navigation build. Ground-snapping for walkers.
- `MsClankerSmall` — boids hive (separation/cohesion/alignment + seek). **10 HP: one shot.**
  Threatens by number, not durability.
- `MsClankerFlyer` — radial standoff, sine strafe, wander re-rolls on an irregular clock.
  Shoots with a **telegraphed** hitscan so it is dodgeable.

**Level and story systems**
- `MsEncounterVolume` — fights that belong to places. Two pacing modes: `TimedEscalation`
  (escalates on a clock, ends after a duration — used by the onboarding ambush) and
  `ClearToProceed` (reinforces on attrition, ends when everything is dead). Has a **facing
  arrow**; spawns are confined to an arc around it so dropships land where the player can see
  them. Reports **cleared**, which is the atom of district progression.
- `MsDropPod` — falls from the sky, slams down, opens, unloads. Telegraphs where the next
  group lands.
- `MsSnitchDrone` — the inciting incident. Only reacts to an **armed** player, so the first
  fight is caused by taking grandpa's sword rather than by walking down a street.
- `MsInteractable` / `MsNpc` — walk up, press `E`, dialogue advances. Outcome fields (grant a
  weapon, set the next objective) so the level wires up by filling in boxes.
- `MsObjectiveSubsystem` — one current objective plus a world target.
- `MsCutsceneSubsystem` — placeholder: fades to black, shows lines, disables input, **clears
  the battlefield**. Not a real cutscene system; that is Sequencer and waits for characters
  worth filming. What it provides is the hook in the right place.
- `MsStoryTrigger` — box that sets objectives, plays cutscenes and grants sword/gun/shield.
  Rewards apply *after* the cutscene so the player can see them arrive.
- `MsHUD` — crosshair, health bar, shield bar, grenade pip, objective, interaction prompt,
  dialogue panel, cutscene overlay. All code-drawn placeholder; becomes UMG later.

**Gating for the onboarding**
- `bStartWithSword` / `bStartWithGun` / `StartingMaxShield` on the character. The onboarding
  wants sword and gun **false** and shield **0** — grandpa grants the sword, the neighbour's
  cutscene grants gun and shield.

### Localization is live

The game's first player-facing text exists and is all `FText`/`LOCTEXT`, gatherable by UE's
pipeline. Formatting uses `FText::Format` with named arguments rather than concatenation, so
word order stays translatable. The rule set on day one is now doing real work.

### VERIFY at the start of next session

- **Was `bStartWithGun` unticked?** Last seen reading `gun YES` in the on-screen weapon
  readout. It should be unticked in `BP_ThirdPersonCharacter` → Class Defaults. If the readout
  still says `gun YES` after that, it is a real bug and needs investigating — the weapon
  readout prints lock state specifically to answer this.

### Next steps

**Daniel:** the four assets in `ASSET_REQUESTS.md`. Character is the priority and must be
rigged to the **UE5 Mannequin skeleton**, with **skin as its own material slot**.

**Claude, once the character lands:**
1. **Animation layer** — the character currently strafes and back-pedals using forward-run
   animations. Obvious the moment he stops being a grey mannequin. Needs directional
   locomotion in the Animation Blueprint.
2. `MsAppearance` — body type + skin tone, applied on spawn, replicated, saved.
3. More clanker types toward the taxonomy: 2 small, 3 mid, 3 flying.

**Not yet:** UMG HUD (the code-drawn one is deliberately placeholder), real cutscenes,
districts, skill trees.

### Gotchas learned this session

- **`MsStoryTrigger` does nothing unless its fields are filled.** Everything is optional, so an
  unconfigured trigger fires silently. `CutsceneLines` empty means no cutscene — that cost us
  a debugging round.
- **Blueprint Class Defaults permanently override C++ defaults.** Tune values there; do not
  expect a later C++ default change to take effect.
- **Do not name C++ members after Blueprint components.** `CameraBoom`/`FollowCamera` collided
  with the template's own components and the Blueprint refused to compile.
- **`AddOnScreenDebugMessage` with a `uint32` key is ambiguous** — cast to `uint64`. Hit twice.
- **PowerShell breaks on `[E]` in commit messages** (parses as an array index). Use a
  single-quoted here-string for multi-line git messages.
- Daniel's keyboard is a **Swedish QWERTY 65% with no F-row** — bind letters, digits or arrows
  in gameplay code, never punctuation. Live Coding for him is `Ctrl+Alt+Fn+-`.
- Landscape: use **Resize → Expand**, never Resample, to grow terrain without distorting sculpt.
  Enable Nanite on landscape only *after* sculpting, since edits force a rebuild.

---

## 2026-08-13 — Session 1: setup, and the gun works

### Where we are
**MEATSPACE has working combat.** C++ project, editor builds clean, and a
server-authoritative hitscan gun that Daniel confirmed fires and kills targets in-editor.
Everything below was built and verified in one session.

The project also still has the stock Third Person template content: Manny/Quinn with a full
animation set (rifle + pistol fire/reload/aim, **melee attacks**, hit reacts) and the greybox
level `Content/ThirdPerson/Lvl_ThirdPerson`. That is our combat testbed — and those melee
anims are what the sword will use next session.

### Decisions made
- **Engine: Unreal Engine 5.8.1.** Already installed; strongest replication support;
  Blender/Rhino pipeline feeds it directly.
- **C++ core, Blueprint for tuning.** Gameplay logic in C++ because (a) Claude cannot author
  binary Blueprint assets, so Blueprint-only would sideline Claude from writing the game,
  (b) netcode control for co-op melee, (c) Blueprint VM cost at high clanker counts.
  Daniel owns all tuning, animation, UI, VFX.
- **Co-op is backbone-only for now.** Build every feature server-authoritative so multiplayer
  is *possible* later without a rewrite — but **do not ask Daniel to test 2-window PIE.**
  He will not touch co-op until the game is much further along. Single-player Play is his loop.
- **Placeholder-first.** Prototype with engine primitives; request real assets only once a
  mechanic feels right.
- **Full localization.** English first, then French, then more. Every player-facing string is
  `FText` in a String Table from day one — never a hardcoded literal, never concatenated
  sentences. Rules in `CLAUDE.md`. Adopted while the game had zero text, so it cost nothing.

### Done this session

**Toolchain**
- Verified UE 5.8.1, MSVC 14.51.36231, Windows SDK 10.0.26100
- Build Tools 2026 shipped without a .NET Framework SDK, so `SwarmInterface` failed and the
  editor target would not build. Daniel installed `.NET Framework 4.8 SDK` + targeting pack.
- Converted MEATSPACE from Blueprint-only to a **C++ project** (module `MEATSPACE`)
- Fixed `.uproject` file association — `UnrealVersionSelector.exe` is **missing from this
  engine install**, so double-clicking a `.uproject` did nothing. Registered the association
  under `HKCU\Software\Classes` and put a `MEATSPACE.lnk` shortcut on the desktop.

**Version control**
- git + Git LFS (238 binary assets via LFS; `.git` stays ~129 MB)
- Pushed to **https://github.com/danielsofinet/MEATSPACE** (`origin/main`).
  Credentials cached in Windows Credential Manager, so pushes from Claude's session work now.
- ⚠ GitHub free LFS quota is 1 GB storage / 1 GB month bandwidth. Fine today; will bite once
  real 3D assets land.

**Gameplay — first combat code**
- `Character/MsCharacter` — thin `ACharacter` base owning the weapon. `BP_ThirdPersonCharacter`
  is **reparented to it**, keeping the template's mesh/camera/anims. LMB bound directly in
  C++ (no `IA_Fire` asset yet — Input Actions are editor-authored binary assets).
- `Combat/MsWeaponComponent` — hitscan gun. Shooter sees their own tracer instantly (cosmetic);
  **server does the authoritative trace and applies damage**; server multicasts FX to others;
  server rate-limits independently so a modified client cannot outpace the weapon.
  Tunables: `Damage`, `RoundsPerSecond`, `Range`, `bAutomatic`, `SpreadDegrees`, `bDrawDebugShots`.
- `Combat/MsTargetDummy` — cube, replicated health, respawns after `RespawnDelay`.
  On-screen readout tagged `[SERVER]`/`[CLIENT]`.
- `Build.cs`: added `ModuleDirectory` to `PublicIncludePaths` so subfolder includes resolve.

**Status: confirmed working in-editor by Daniel.** Feels right; no complaints raised about feel.

### Nothing is blocked

### Next step when we resume
**The sword** — the anti-ground melee weapon, and the harder of the two netcode problems.
1. `Combat/MsMeleeComponent` — swing arc trace (sphere/capsule sweep, not a line), windup →
   active frames → recovery, server-authoritative with the same predict-locally pattern
2. Hook it to the template's existing melee montages (`MM_Attack_01/02/03`, `MM_ChargedAttack`
   in `Content/Characters/Mannequins/Anims/Unarmed/Attack/`) — anims already exist, use them
3. Right mouse or a second key for swing; keep it a direct key bind for now
4. Make `MsTargetDummy` react differently to melee vs bullets so the gun/sword split is visible

After the sword: **clankers.** Daniel wants AI enemies set up next — ground clankers that the
sword handles, flying ones the gun handles. That is the point where the two weapons stop being
a tech demo and become the actual game loop.

### Notes / gotchas
- **Close the editor before a C++ build.** Live Coding locks the module; the build fails with
  "Unable to build while Live Coding is active". Small changes inside existing functions can
  use **Ctrl+Alt+F11** hot reload instead; new classes/headers need a full close-and-build.
- Adding a new C++ class means: Daniel closes editor → Claude builds → Daniel reopens.
- UBT warns MSVC 14.51 is newer than its preferred 14.50. Harmless so far. Suspect it first
  if we hit inexplicable compiler errors.
- "Unable to find Visual Studio SDK. Editor integration will be disabled" is **expected** —
  Build Tools, not the full VS IDE. Only affects "open in VS", which we don't use.
- Daniel is new to the Unreal editor UI. Give **explicit click paths**, not just menu names:
  which window, which panel, which tab. Reparenting is `Class Settings → Parent Class`, easier
  to find than `File → Reparent Blueprint`. Placing C++ actors is easiest by dragging from
  `Content Drawer → C++ Classes → MEATSPACE → <folder>` into the viewport.
- Start Claude Code **from the project folder** so `CLAUDE.md` auto-loads.
- Folder names containing `[` `]` break PowerShell's `-Path`; use `-LiteralPath`.
- `setup.exe modify --norestart` requires `--quiet` or `--passive`, or it just prints usage.
