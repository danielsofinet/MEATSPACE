# MEATSPACE

Co-op third-person shooter. You and your friends take the world back from the AI that took
everyone's jobs. Enemies are robots called **clankers**. Reclaim districts, kill the boss AI
that runs each one.

**Read `SESSION_LOG.md` before doing anything.** The machine gets shut down between
sessions, so that file is the only memory of where we left off.

---

## Who does what

| Daniel (user) | Claude (senior dev) |
|---|---|
| 3D assets, modelling, texturing (Blender, Rhino 8) | All code: gameplay, netcode, progression, tooling |
| Creative direction — what the game *should* be | Technical decisions — how it gets built |
| Tuning values in-editor once exposed | Exposing those values as `EditAnywhere` |

Claude proposes and leads on architecture but **discusses before committing** — Daniel knows
the direction. Daniel is new to game dev: explain in plain language, move in small verifiable
steps, and show something playable at each one.

Claude cannot author Blueprint assets (they are binary `.uasset` graphs). That is *why*
gameplay logic lives in C++ — see below.

---

## Stack

- **Unreal Engine 5.8.1** — `C:\Program Files\Epic Games\UE_5.8`
- **MSVC 14.51.36231** via Visual Studio Build Tools 2026
  - UBT warns this toolset is newer than its preferred 14.50. Compiles fine. If we ever hit
    bizarre compiler errors, suspect this first.
- **Windows SDK 10.0.26100**
- Project root: `C:\Users\D\Documents\Unreal Projects\MEATSPACE`

### Build

```powershell
# Editor target (what you open to work in)
cmd /c '"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" MEATSPACEEditor Win64 Development -Project="C:\Users\D\Documents\Unreal Projects\MEATSPACE\MEATSPACE.uproject" -WaitMutex'
```

---

## Architecture rules

**C++ for systems, Blueprint for tuning and content.** Never gameplay logic in Blueprint —
it can't be diffed, reviewed, or written by Claude, and it costs real performance once
clanker counts get high.

**Server-authoritative, always.** This is co-op (listen server, 4 players). The server decides
what hit what. Clients predict for responsiveness and get corrected. Every combat feature is
built networked from the start — never single-player-first, because retrofitting netcode is a
rewrite.

**Expose everything tunable.** Damage, fire rate, swing arcs, health, speeds — all
`UPROPERTY(EditAnywhere, BlueprintReadWrite)` so Daniel can iterate on feel without a compile.

---

## Naming

| Kind | Prefix | Example |
|---|---|---|
| C++ classes | `Ms` | `AMsCharacter`, `UMsWeaponComponent` |
| Blueprint | `BP_` | `BP_Clanker_Flying` |
| Widget BP | `WBP_` | `WBP_HealthBar` |
| Static mesh | `SM_` | `SM_Rifle` |
| Skeletal mesh | `SK_` | `SK_ClankerSmall` |
| Material / instance | `M_` / `MI_` | `M_ClankerBody` |
| Texture | `T_` | `T_ClankerBody_D` |
| Animation | `A_` | `A_Sword_Swing01` |

---

## Roadmap

1. **Core combat** ← we are here. Gun (anti-air) + sword (anti-ground) that feel good.
2. Clanker variety — small, big, flying, each with a speciality
3. Districts + boss AI per district
4. 4 classes (tank, AOE, damage, heals) with per-level skills
5. Maybe: defending claimed districts

---

## Conventions for working

- Start Claude Code **from this folder** so this file auto-loads.
- Placeholder-first: prototype with engine primitives (capsules, cubes). Only request real
  assets from Daniel once the mechanic feels right — no point modelling a gun we redesign.
- When Claude needs an asset, it goes in `ASSET_REQUESTS.md` with format, scale, pivot, and
  socket expectations spelled out.
- Test co-op with 2 client windows in PIE, every feature, every time.
