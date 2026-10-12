# CGS Titan Weapons

Two weapon components for your own weapon models. Put **one** on the weapon part prefab (the `GenericEntity` with your `MeshObject`), exactly where the autocannon or rocket component would go.

| Component | What it is |
|---|---|
| `CGS_Titan_Cannon` | Heavy gun: big shells with travel time, splash, charge-up, volleys |
| `CGS_Titan_Missile` | Homing missile rack: locks what the crosshair is on, top-attack loft; point defense can shoot the missiles down |

## Module IDs

These are set automatically if you leave **Module Id** on its default.

| Component | Module Id |
|---|---|
| `CGS_Titan_Cannon` | `TITAN_CANNON` |
| `CGS_Titan_Missile` | `TITAN_MISSILE` |

The mech's hardpoint module definition needs:

- the **same text** in **Weapon Module Id**
- a free **Id** (1–31)
- a **Class** that fits the slot: 1 ARM, 2 SHOULDER, 3 BACK, 4 UTILITY

---

## Weapon part prefab checklist

| Setting | What to put |
|---|---|
| **MeshObject** | Your model |
| **Muzzle Point** | On the barrel tip, blue (Z) axis pointing out of the barrel |
| or **Barrel Points** | One PointInfo per barrel / tube (shots cycle through them) |
| **Effects** | Muzzle particle, Sound Project (`.acp`) + Fire Sound Event, impact particle, projectile prefab (shell / missile model) |
| **Heat Per Shot** | Cannon ~25, missile ~15 (needs `CGS_MechHeatComponent` on the mech) |
| **Ammo** | Magazine size, reload time, spare magazines |

> **One weapon script per part.** Two weapon components on the same part prefab means both fire on every trigger pull: double damage, double rockets.

> **No `RplComponent` and no damage manager on weapon parts.** They are spawned on every machine.

---

## CGS_Titan_Cannon: suggested start values

| Category | Setting | Value | Notes |
|---|---|---|---|
| Fire | Shots Per Minute | 12 | One round every 5 s |
| Routing | Held Fire | off | One round per trigger pull |
| Titan Cannon | Range | 2500 | |
| | Direct Damage | 1500 | |
| | Splash Damage | 400 | |
| | Splash Radius | 10 | |
| | Spread | 0.1 | |
| | Shell Speed | 700 | Damage lands when the shell arrives |
| | Travel Time | on | |
| | Alt Fire Sound Event | `SOUND_SHOT_2` | Second shot sound in the same `.acp`. Empty = every shot sounds the same |
| Volley | Shells Per Round | 1 | 2 + two Barrel Points = both barrels fire |
| | Shell Interval Ms | 120 | |
| Charge | Charge Time | 0 | e.g. 1.5 for a big charge-up gun |
| | Charge Sound Event | `SOUND_CHARGE` | Event in the Effects `.acp` |
| Ammo | Ammo Type | heavy / AP | So it hurts tanks |
| | Magazine / Reload | 6 / 8 s | |

**Charge Time** must be shorter than the time between rounds (60 ÷ Shots Per Minute).

### Alternating fire sounds

With **Alt Fire Sound Event** set, shots alternate: Fire Sound Event, Alt, Fire, Alt…

Example (Red Titan cannon arm):

- `Cannon_shot.acp` has two events:
  - `SOUND_SHOT` → `cannon 1.wav`
  - `SOUND_SHOT_2` → `cannon 2.wav`
- On the cannon part:
  - **Effects > Sound Project** = `Cannon_shot.acp`
  - **Effects > Fire Sound Event** = `SOUND_SHOT`
  - **Titan Cannon > Alt Fire Sound Event** = `SOUND_SHOT_2`

Tip: mono `.wav` files are placed better in 3D than stereo.

---

## CGS_Titan_Missile: suggested start values

| Category | Setting | Value | Notes |
|---|---|---|---|
| Rocket | Rocket Speed | 90 | Slow enough to see, fast enough to hit |
| | Range | 1500 | |
| | Salvo Count | 4 | Missiles per trigger pull |
| | Salvo Interval Ms | 200 | |
| | Refire Ms | 3000 | |
| | Spread Deg | 2 | Spreads a salvo a little |
| Damage | Direct Hit Damage | 900 | |
| | Splash Damage | 300 | |
| | Splash Radius | 10 | |
| Homing | Homing | on | |
| | Lock Targets | on | Follows the locked vehicle / mech |
| | Turn Rate | 70 | deg/s; 40 = dodgeable, 120 = very hard to dodge |
| | Arming Distance | 20 | Flies straight out of the tube first |
| | Loft Angle | 35 | Back rack top attack; 0 for arm launchers |
| Ammo | Magazine / Reload | 8 / 10 s | |

The visible missile flies straight to the lock point; the real (server) missile steers. Against a moving target the impact effect appears where the real missile hit.

### Rocket pod: rockets from different tubes

Instead of placing one point per tube, describe each **block** of tubes as a grid (**Tubes** category):

1. **Tube Grids**: one entry per block (the Chaos pod has 2 blocks: left and right).
2. **First Tube**: move it with the gizmo onto the **top-left** tube mouth of that block. Blue (Z) arrow out of the tube, red (X) to the right, green (Y) up.
3. **Columns / Rows**: tubes across and down (Chaos pod: 2 × 12 per block).
4. **Column Spacing**: distance to the tube on the right. **Row Spacing**: distance to the tube below. Both in model units (before the weapon's scale). Use **Odd Row Shift** for staggered tubes.
5. **Random Tube Order** on: every tube fires once (random order) before any fires again, so a salvo ripples out all over the pod.

To check: turn Debug on, fire, and watch where the rockets start. Adjust spacing until they leave the tube mouths. Leave Tube Grids empty to use Muzzle Point / Barrel Points instead.

---

## Weapons on IK arms

The arm's hand bone (`J3`) **moves** with the IK but **does not turn**, so a muzzle on it keeps pointing level. Fix it on the weapon part:

| Setting | Value | Notes |
|---|---|---|
| Muzzle > Fire At Aim Point | ON | Shots leave the muzzle toward the crew's aim point |
| Muzzle > Max Aim Correction | 100 | Degrees; past this the shot goes along the muzzle |

Barrel Points / Muzzle Point use **PivotID** = the muzzle bones (`Cannon_1`…, or `J3`).

### Mount settings for an IK arm

On the mech's `BFA_MechWeaponMountComponent`:

| Setting | Value | Notes |
|---|---|---|
| Aim Driver | `ANIM_IK_TARGET` | |
| Ik Reach | arm length | How far out the hand reaches (Red Titan: right 56, left 49) |
| Ik Pivot Bone | `J1` | The shoulder |
| Ik Target From Rest | ON | `IK_Titan_Arm` moves the hand *from* its rest position |
| Ik End Bone | `J3` | |
| Pitch Min / Max | -75 / 30 | -75 lets it shoot at people under its feet |
| Yaw Min / Max | -45 / 45 | |

On the arm part prefab: **AnimationController** = `IK_Titan_Arm.agr` / `.asi`, **Start Node** = `IK Chain to IK Target`.

The arm aims from its **mount**, not from a hardpoint. An arm with no weapon of its own (e.g. an arm that only carries a pod) needs a mount but **no hardpoint**.

---

## Example: Red Titan layout

**Mounts**

| Mount | Slot | Notes |
|---|---|---|
| `ARM_R` | `Arm_R1` | IK arm |
| `ARM_L` | `Arm_L1` | IK arm |
| `Arm_Left_Pod_System` | `Arm_L1/Titan_Rocket_Pod` | Limits 0 = rides the arm |

**Hardpoints**

| Hardpoint | Module | Weapon Module Id | Group |
|---|---|---|---|
| `ARM_R` | 6 Titan Arm Cannon | `TITAN_CANNON` | 1 |
| `ARM_LEFT_POD` | 5 Titan Missile Pod | `TITAN_MISSILE` | 2 |
| `COUNTERMEASURES` | 4 Point Defense | `POINT_DEFENSE` | 10 |

The pod sits in the **left arm's** SlotManager (slot `Titan_Rocket_Pod`, PivotID `J3`), so it moves with the arm. A nested slot path `Part/Slot` lets a mount find it.

> **Every hardpoint with Bind Existing Aim Mount must find a weapon script whose Module Id matches its module.** If it doesn't, it hides the arm and spawns the module prefab again, which gives you a duplicate arm or pod. Remove the hardpoint when the part has no weapon script.

### AI

- The titan has one AI pilot in `DRIVER`; the weapons belong to `COMMANDER`. The driver aims and fires weapons on empty stations automatically.
- **AI Crew > Fire All Groups (BOSS)** = ON makes it fire the cannon *and* the pod at the same target.
- Kills are credited to the pilot.

---

## Balance knobs

What to turn down when a weapon is too strong, roughly in order of impact:

| Setting | Effect |
|---|---|
| Splash Radius | Biggest effect on infantry crowds |
| Salvo Count | Rockets per trigger pull |
| Salvo Interval Ms | Gap between rockets in a salvo |
| Refire Ms | Time between salvos |
| Reload Time | Pause after the magazine is empty |
| Direct / Splash Damage | Damage per hit |
| Shells Per Round | Cannon volley size |

---

## Testing

Turn on **Diagnostics > Debug** on the weapon part and watch the log:

```
[CGS TITAN CANNON] TITAN_CANNON_L charging 1.5 s
[CGS TITAN CANNON] ... fired, target=..., distance=1400 m, lands in 2000 ms
[CGS TITAN CANNON] impact at ...: direct=... (-1500 hp), splash targets=2
[CGS TITAN MISSILE] LAUNCH ... lock=<entity> point=...
[BFA ROCKET MODULE] DETONATE ...
```

For aim or IK problems, turn on **Debug Aim** on the mech's Weapon Mount component. It prints `[BFA AIM TRACE]` and `[BFA IK DIAG]`.

### Before release

Turn these **off**, since they spam the log:

| Where | Setting |
|---|---|
| Weapon parts | Diagnostics > Debug |
| Mech | Weapon Mount > Debug Aim |
| Mech hull | Log Damage |
| Autocannon | Effects > Debug Tracer |

Warnings always print. `-bfaMechDebug` on the command line shows the general mech log.

---

## Damage, hit boxes and death

### How a hit reaches the titan

```
bullet / rocket ─► HIT BOX (collider on a bone) ─► PART (leg / arm)  ─► HULL
                                                 └─► HULL (body boxes, weak spot)
```

- **Hit box** (`CGS_HitBoxZone`): a small prefab with a box collider and a damage manager. It takes no damage itself and passes every hit on, keeping the damage type. **Multiplier** > 1 makes it a weak spot.
- **Part** (`BFA_MechPartHitZone` on a leg or arm): has its own health and armour, and passes **Hull Share** of each hit to the hull.
  - Destroyed **leg**: the mech slows down and can't sprint (unless the movement component switches leg damage off).
  - Destroyed **arm** (WEAPON): its weapon stops firing, and so does anything riding on it (the pod on the left arm).
- **Hull** (`BFA_MechHitZone` on the mech): the main health bar. At 0 the mech dies.

### What needs colliders

| Thing | Collider | Why |
|---|---|---|
| Body | Hit box prefabs in the mech's SlotManager (no PivotID) | Hits the hull |
| Legs / arms | Hit box prefabs in the part's SlotManager with **PivotID J1 / J2 / J3** | Follow the IK animation; hits go to that part |
| Weak spot | One small hit box, Multiplier 3 | Rewards flanking |
| Weapon parts, pods, shield bubble | **None** | They would steal hits and block the titan's own shots |

Hit boxes use layer **VehicleFireView**: they stop bullets and block sight, but don't push characters around.

> **One big box collider in the model (`UBX_` in Blender) catches every hit before the hit boxes do.** Delete it from the body model once the hit boxes work.

The titan's own weapons ignore its own hit boxes, so the arms can fire out through them.

### Armour: heavy weapons only

| Setting | Value | Effect |
|---|---|---|
| Kinetic multiplier | 0.02 | Rifles and MGs barely scratch it |
| Fragmentation multiplier | 0.02 | Grenade fragments barely scratch it |
| Explosive multiplier | 1 | AT rockets and explosives do full damage |
| Damage Threshold | 20 | Anything weaker than this does nothing at all |

### Shield first

With the shield (`CGS_MechShieldComponent`) **Bleed Through = 0**, nothing gets through while the shield is up, not the body and not the legs or arms. Hits drain the shield; once it breaks, the hull and parts take damage.

### Death (`CGS_MechDeathComponent`, on every mech)

When the hull is destroyed:

1. A chain of explosions ripples over the body, legs, arms and hit boxes.
2. One final blast damages everything nearby.
3. **Collapse:** every arm and leg blows up, breaks off and is thrown away from the body; the torso crashes straight down. The falling pieces are local copies of the models (no physics, no network traffic); the real mech and its hit boxes are hidden.
4. Fires burn where the pieces land.

It runs on every machine from the replicated hull state, so every player sees the same thing. A mech that is already dead when a player joins only shows the burning wreck.

| Setting | Small mech (default) | Titan |
|---|---|---|
| Sequence Time | 2.5 s | 6 s |
| Chain Explosions | 5 | 14 |
| Chain Jitter | 1 m | 6 m |
| Default Blast Height | 3 m | 60 m |
| Final Spread | 2 m | 12 m |
| Final Particle Count | 1 | 3 |
| Blast Radius | 12 m | 60 m |
| Blast Damage | 600 | 3000 |
| Fling Speed | 3 m/s | 8 m/s |
| Spin Speed | 60 °/s | 25 °/s |
| Wreck Fires | 2 | 5 |

Set **Final Blast Point** on the reactor (or wherever the big boom should be). Turn on **Debug** to see the sequence in the log.

### Testing damage

On a test copy, turn on **Log Damage** on the hull and **Log Hits** on the hit boxes, then shoot each part:

```
[CGS HITBOX] LEGL SHIN hit: 600 EXPLOSIVE x1 -> Titan_Leg_Left_Test lost 360 (7640 / 8000 left)
[BFA DAMAGE] ...: 240 damage after armour, 0 after shield, hull 20000 / 20000
```

Turn **Log Hits** off on every hit box before release.

### Boss mode (`CGS_MechBossComponent`)

Lets one titan survive a whole server shooting at it. Put it on the mech next to the shield.

**Damage caps (per second).** Damage up to the cap counts fully; anything over it only counts **Over Cap Multiplier** (15%). So 100 players can't melt it in seconds, but more players still helps a little. The caps apply before the shield, so they also slow how fast the shield drains.

| Setting | Red Titan | What it limits |
|---|---|---|
| Heavy Per Second | 300 | Rockets, explosives, cannons on the hull |
| Small Arms Per Second | 25 | Bullets, fragments and fire on the hull. **Hard cap** (Small Arms Over Cap 0): 100 rifles never do more than this |
| Part Heavy / Small Arms Per Second | 150 / 10 | The same, for each leg and arm separately |
| Heavy Hit Min | 150 | Any single hit smaller than this counts as small arms, whatever its damage type (catches modded las / plasma guns) |

**Shield phases.** When the hull drops below each listed fraction (Red Titan: 0.66 and 0.33), the shield comes back to full at once.

**Minimum fight length** (if every second hits the heavy cap): shield 15,000 ÷ 300 = 50 s, ×3 with the phases, plus hull 60,000 ÷ 300 = 200 s. That's about **6 minutes of perfect pressure**; with players dying and reloading, expect 10–15 minutes.

Turn on **Debug** to get a summary every 10 s:

```
[CGS BOSS] last 10 s: 8400 damage in (840/s), 3150 counted after caps (315/s) | hull 51200 / 60000
[CGS BOSS] ... hull at 66 percent - PHASE 1: shield restored
```

If "damage in" is far above "counted", the caps are doing the work. Raise them for a shorter fight, lower them for a longer one.
