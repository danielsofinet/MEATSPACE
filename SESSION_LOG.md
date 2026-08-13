# Session log

Newest entry at the top. Read the top entry to resume.

---

## 2026-08-13 — Session 1: setup

### Where we are
**MEATSPACE is now a C++ project and the editor target builds clean.** Toolchain fully
verified end to end. No gameplay code yet — that is deliberate, next session's work.
This session was about deciding how we work and proving the toolchain, not building the game.

The project still contains the stock Third Person template content: Manny/Quinn with a full
animation set (rifle + pistol fire/reload/aim, melee attacks, hit reacts) and a greybox
prototyping level `Content/ThirdPerson/Lvl_ThirdPerson`. That is our combat testbed —
no need to build one.

### Decisions made
- **Engine: Unreal Engine 5.8.1.** Already installed; best replication support for co-op;
  Blender/Rhino pipeline feeds it directly.
- **C++ core, Blueprint for tuning.** Gameplay logic in C++ because (a) Claude cannot author
  binary Blueprint assets, so Blueprint-only would sideline Claude from writing the game,
  (b) netcode control for co-op melee, (c) Blueprint VM cost at high clanker counts.
  Daniel still owns all tuning, animation, UI, VFX.
- **Co-op from day one.** Listen server, 4 players, server-authoritative. Never build a
  feature single-player and retrofit networking — that is a rewrite, not a patch.
- **Placeholder-first.** Prototype with engine primitives; request real assets only once a
  mechanic feels right.
- **Full localization.** English first, then French, then more. Every player-facing string is
  `FText` in a String Table from day one — never a hardcoded literal, never concatenated
  sentences. Rules in `CLAUDE.md`. Adopted while the game had zero text, so it cost nothing.

### Done this session
- Verified UE 5.8.1, MSVC 14.51.36231, Windows SDK 10.0.26100
- Fixed a broken toolchain: Build Tools 2026 shipped without a .NET Framework SDK, so
  `SwarmInterface` failed and the **editor** target would not build (the game target was fine).
  Daniel installed `.NET Framework 4.8 SDK` + targeting pack via the VS Installer.
- **Converted MEATSPACE from Blueprint-only to a C++ project**: added `Source/` with module
  `MEATSPACE` (`MEATSPACE.Target.cs`, `MEATSPACEEditor.Target.cs`, `MEATSPACE.Build.cs`,
  module impl), and added the `Modules` block to `MEATSPACE.uproject`.
  Editor target builds clean — `UnrealEditor-MEATSPACE.dll` in 55s.
- Set up git + Git LFS (238 binary assets tracked through LFS; `.git` is 129 MB, not GBs)
- Created `CLAUDE.md` (conventions + role split), `ASSET_REQUESTS.md`, this log,
  `.gitignore`, `.gitattributes`
- Disk cleanup before starting: freed ~241 GB (C: went 286 GB → 528 GB free)

### Nothing is blocked
Toolchain is fully working. Both game and editor targets build.

### Next step when we resume
First gameplay code — **core combat**, built networked from the start:
1. `AMsCharacter` — TPS pawn, camera boom, Enhanced Input. Reparent the existing
   `BP_ThirdPersonCharacter` to it so we keep the template's animation setup.
2. `UMsWeaponComponent` — server-authoritative hitscan gun (the anti-air weapon)
3. A damageable target dummy so the gun visibly *does* something
4. Then the sword — melee trace, the harder netcode problem of the two
5. Validate in PIE with **2 client windows** before calling any of it done

Daniel asked for a gun first, then paused to finish the work setup. Setup is done — the gun
is the natural starting point next session. No assets needed from him yet; engine primitives
until the feel is right.

### Notes / gotchas
- **Close the editor before a C++ build.** Live Coding holds a lock on the module and the
  build fails with "Unable to build while Live Coding is active". For small tweaks you can
  instead hit **Ctrl+Alt+F11** in the editor to hot-reload; that is unreliable for new classes.
- UBT warns MSVC 14.51 is newer than its preferred 14.50. Harmless so far. Suspect it first
  if we hit inexplicable compiler errors.
- "Unable to find Visual Studio SDK. Editor integration will be disabled" is **expected and
  harmless** — Daniel has Build Tools, not the full VS IDE. Only affects "open in VS" from the
  editor, which we don't use.
- Start Claude Code **from the project folder** so `CLAUDE.md` auto-loads. Session 1 ran from
  `C:\Windows\System32`, which is a bad default.
- Folder names containing `[` `]` break PowerShell's `-Path`; use `-LiteralPath`.
- `setup.exe modify --norestart` requires `--quiet` or `--passive` alongside it, or it just
  prints the usage table and does nothing.
