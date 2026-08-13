# Session log

Newest entry at the top. Read the top entry to resume.

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
