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
