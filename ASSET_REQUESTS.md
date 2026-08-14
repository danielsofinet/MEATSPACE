# Asset requests

Claude writes requests here. Daniel delivers into `Content/_Incoming/`, then ticks the box.
Nothing gets requested until the mechanic it serves already works with placeholders.

**Standing conventions**
- Export **FBX** (or glTF) from Blender
- **1 Unreal unit = 1 cm.** Model at real-world scale — a 1.8 m character is 180 units tall
- Apply all transforms before export (scale 1,1,1 / rotation 0)
- Pivot at the object's logical origin: characters and props at **between-the-feet / base**,
  weapons at the **grip**
- Real-world scale beats "looks right in Blender"

**Why these, why now** — the camera is locked (52°→18.7° pitch, ~2450 back, 62.9 FOV), the
route is validated at ~1 minute, and the fight beats work. Everything below has a proven
gameplay shape, so the art can't be invalidated by a design change. Anything *not* listed here
is still moving and shouldn't be modelled yet.

---

## Open requests

### 1. Main character — highest priority

The whole game is one base character; specialisation later comes from skill points, not from
separate models. So this is *the* character.

- **Height 180 units.** Camera framing is built around this
- **MUST be rigged to the UE5 Mannequin skeleton** (`SK_Mannequin`), or be retargetable to it
  via IK Rig. This is not negotiable without cost: the project already uses the template's
  full animation set (rifle, pistol, melee attacks, hit reacts, jumps). Deviating means either
  retargeting work or animating from scratch
- Needs a **right-hand weapon socket** — I'll wire sword and gun to it
- Silhouette matters more than surface detail: at this camera distance he's small in frame, so
  he must be readable as *one recognisable shape*. Save the detail for cutscenes and zoom
- Budget: 30–60k tris is comfortable (skeletal meshes don't use Nanite)

#### Built for customisation — decide these now, not later

The game will offer **skin tone** and **body type** at character creation, plus more later.
Build **one body now**; the second comes later and is cheap *only if* the following hold.
These are the two mistakes that are genuinely expensive to retrofit:

**1. Skin must be its own material slot.**
Separate from clothing, gear and hair. Skin tone is applied at runtime as a material
parameter, so if skin shares a material with the jacket, tinting the skin tints the jacket.
One mesh then covers every skin tone with no extra art.

- Skin material needs a **colour/tint input** I can drive (I'll wire the parameter; you just
  need the slot to exist and the base texture to tint sensibly — avoid baking a specific skin
  tone into the albedo, keep it neutral and let the tint do the work)

**2. Rig to the UE5 Mannequin skeleton, and keep the rig reusable.**
Body type two will be a duplicate of this mesh with adjusted proportions, **rigged to the same
skeleton**. Same skeleton means every animation works on both bodies forever, with no extra
animation work — this is the whole reason the second body is cheap.

**3. Keep UVs consistent between bodies when the second arrives.**
Easy while modelling, painful to retrofit. If both bodies share a UV layout they share
textures, so every future skin/outfit is authored once instead of twice.

**Design note:** consider offering **body type** and **pronouns/voice** as *separate* choices
rather than one "sex" setting driving both. Technically identical — two meshes either way —
but it means a player is not forced to pick a body to get the pronouns they want. Cheap to
decide now, awkward to retrofit because it changes the saved data model.

---

### MODULAR CLOTHING — build the character this way from the start

Cosmetics are core to MEATSPACE's art direction, so the character is **modular from day one**.
This is the right call to make now: modular changes how the *body* is built, not just what
sits on top of it, so retrofitting means rebuilding the character.

#### How it works in engine

One **body** skeletal mesh is the leader. Each clothing piece is its own skeletal mesh rigged
to the **same skeleton**, attached to the body and driven by it (`SetLeaderPoseComponent`), so
every piece animates from the body's bones automatically. Swapping an outfit is swapping a
mesh pointer — no re-rigging, no per-outfit animation.

#### Slots to design around

| Slot | Covers |
|---|---|
| `Head` | hair, helmet, mask |
| `Torso` | jacket, shirt, armour |
| `Arms` | sleeves, bracers |
| `Hands` | gloves |
| `Legs` | trousers, skirt |
| `Feet` | boots |
| `Back` | pack, cape, sheath |

Not every slot needs filling — an empty slot just shows the body underneath.

#### What this requires of the body mesh

**Split the body into material sections matching those slots** (head / torso / arms / hands /
legs / feet). At runtime I hide the sections covered by clothing, which is how clipping gets
solved — a jacket hides the torso section rather than fighting with it.

This is the single most important requirement here, and it is a *modelling* decision. A body
authored as one undivided section cannot have parts hidden, and every garment will clip.

#### Rules for every clothing piece

- Rigged and weighted to the **same UE5 Mannequin skeleton** as the body
- Exported as its **own FBX**, referencing that shared skeleton
- Modelled **slightly proud of the body surface** so it never intersects during animation
- Its own material slot, so colourways can be driven as parameters like skin tone
- Keep the **body's UV layout fixed forever** — every future skin and outfit is authored
  against it, so changing it later invalidates all of them

#### Cost, honestly

Each equipped piece is an extra skinned mesh. On the player character that is negligible —
it matters for crowds, and clankers are not modular. No concern at MEATSPACE's scale.

The runtime side (`MsAppearance`: body type, skin tone, and a mesh per slot, applied on spawn,
replicated for co-op, saved to profile) gets built once the first body plus one clothing piece
exist to test with. **Deliver a body and a single jacket and I can build and prove the whole
system** — everything after that is content.

### 2. Small clanker — most-seen asset in the game

- **~70 units tall**, footprint fitting a **35-unit-radius sphere** (that's its collision)
- Pivot **between the feet**
- Dies in one hit, arrives in packs of 5–11. It will be seen thousands of times more than
  anything else — worth the most care per polygon
- Must read at distance as a *silhouette in motion*. Whatever says "cheap disposable robot"
- Enable Nanite on import

### 3. Dropship / drop pod

Currently a scaled cylinder, and it's carrying a real story beat — this is how reinforcements
arrive, and its fall is the player's warning.

- **~300 units tall**, pivot at the **base** (it lands on that point)
- Should read clearly while **falling from 4500 units up** — top and side silhouette both matter
- Ideally suggests opening — doors, ramp, split hull — even if not animated yet
- Enable Nanite

### 4. Snitch drone

The inciting incident of the whole game. Currently a cone.

- **~50 units**, pivot at centre (it hovers and bobs)
- Should read as **civilian delivery infrastructure, not military** — the point is that
  ordinary machinery reports you
- Somewhere for a light or lens to live, so "it has noticed you" can be shown later

### 5. Grandpa and the neighbour

- Same 180-unit scale and UE5 skeleton as the player, if possible — reusing the rig means they
  can be animated with the same library
- Static A-pose is fine for now; they only stand and talk

---

## Deliberately NOT requested yet

- **The sword and the gun.** Their *hit volumes* are tuned but their visual scale isn't locked,
  and the sword's reach (260 units) is intentionally far longer than any realistic blade.
  Model these once the character exists and we can see them in his hands
- **Buildings, props, foliage.** Blockout first — the route is still moving
- **Flying clanker variants.** Only one flyer archetype is proven; the other two aren't designed

---

## Delivered

*(nothing yet)*
