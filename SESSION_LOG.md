# Session log

Newest entry at the top. Read the top entry to resume.

---

## 2026-08-13 — Session 1: setup

### Where we are
Project exists and **opens in the editor**, but it is still **Blueprint-only** — no C++ yet,
no gameplay code, no content. This session was about deciding how we work and proving the
toolchain, not building the game.

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

### Done this session
- Verified UE 5.8.1, MSVC 14.51.36231, Windows SDK 10.0.26100
- **Proved the compiler works**: built a throwaway C++ project at `C:\Users\D\Dev\MEATSPACE`,
  `Meatspace.exe` linked clean in 85s. That folder is scratch — delete it after the real
  project is converted to C++.
- Created `CLAUDE.md` (conventions + role split), `ASSET_REQUESTS.md`, this log,
  `.gitignore`, `.gitattributes` (LFS for `.uasset`/`.umap`/art)
- Disk cleanup before starting: freed ~241 GB (C: went 286 GB → 528 GB free)

### BLOCKED — Daniel owns this
The **editor** C++ target will not build. `SwarmInterface` needs a .NET Framework SDK that
Build Tools 2026 was installed without. `NETFXSDK` and the targeting packs are absent.
The *game* target builds fine — this affects the editor only.

Fix, in an **elevated** terminal (a separate admin window; the Claude session can stay open):

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify `
  --installPath "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools" `
  --add Microsoft.Net.Component.4.8.SDK `
  --add Microsoft.Net.Component.4.8.TargetingPack --norestart
```

Or: Visual Studio Installer → Build Tools 2026 → Modify → Individual components →
tick `.NET Framework 4.8 SDK` and `.NET Framework 4.8 targeting pack`.

### Next step when we resume
1. Confirm the SDK landed — re-run the editor build in `CLAUDE.md`
2. Convert `MEATSPACE` to a C++ project (add `Source/`, module `MEATSPACE`)
   — *deliberately not done yet*: the moment `Source/` exists, the project won't open until it
   compiles, and we didn't want Daniel locked out of his own project while blocked
3. Then first gameplay: `AMsCharacter` (TPS pawn, camera boom, Enhanced Input) →
   `UMsWeaponComponent` (server-authoritative hitscan) → a damageable target dummy
4. Validate in PIE with 2 client windows before calling it done

### Notes / gotchas
- UBT warns MSVC 14.51 is newer than its preferred 14.50. Harmless so far. Suspect it first
  if we hit inexplicable compiler errors.
- Start Claude Code **from the project folder** so `CLAUDE.md` auto-loads. Session 1 ran from
  `C:\Windows\System32`, which is a bad default.
- Folder names containing `[` `]` break PowerShell's `-Path`; use `-LiteralPath`.
