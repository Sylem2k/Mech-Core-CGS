CGS TITAN WEAPONS
=================

Two weapon components for your own weapon models. Put ONE on the weapon part prefab
(GenericEntity with your MeshObject), exactly where the autocannon / rocket component would go.

  CGS_Titan_Cannon   heavy gun: big shells with travel time, splash, charge-up, volleys
  CGS_Titan_Missile  homing missile rack: locks what the crosshair is on, top-attack loft,
                     point defense can shoot the missiles down

Module IDs (set automatically if you leave Module Id on its default):
  CGS_Titan_Cannon  -> TITAN_CANNON
  CGS_Titan_Missile -> TITAN_MISSILE
The mech's hardpoint module definition needs the SAME text in Weapon Module Id, a free Id
(1-31) and a Class that fits the slot (1 ARM, 2 SHOULDER, 3 BACK, 4 UTILITY).


WEAPON PART PREFAB - CHECKLIST
------------------------------
  MeshObject            your model
  Muzzle Point          on the barrel tip, blue (Z) axis pointing out of the barrel
    or Barrel Points    one PointInfo per barrel / tube (shots cycle through them)
  Effects               muzzle particle, Sound project (.acp) + Fire Sound Event,
                        impact particle, projectile prefab (shell / missile model)
  Heat Per Shot         cannon ~25, missile ~15 (needs CGS_MechHeatComponent on the mech)
  Ammo                  magazine size, reload time, spare magazines
  NO RplComponent, NO damage manager on weapon parts (they are spawned on every machine).


CGS_Titan_Cannon - suggested start values
-----------------------------------------
  Fire > Shots Per Minute   12        (one round every 5 s)
  Routing > Held Fire       off       (one round per trigger pull)
  Titan Cannon
    Range                   2500
    Direct Damage           1500
    Splash Damage           400
    Splash Radius           10
    Spread                  0.1
    Shell Speed             700       damage lands when the shell arrives
    Travel Time             on
  Volley
    Shells Per Round        1         2 + two Barrel Points = both barrels fire
    Shell Interval Ms       120
  Charge
    Charge Time             0         e.g. 1.5 for a big charge-up gun
    Charge Sound Event      event in the Effects .acp (e.g. SOUND_CHARGE)
  Ammo > Ammo Type          a heavy / AP type so it hurts tanks
  Ammo                      Magazine 6, Reload 8 s

  Charge Time must be shorter than the time between rounds (60 / Shots Per Minute).


CGS_Titan_Missile - suggested start values
------------------------------------------
  (Rocket category - inherited from the rocket launcher)
    Rocket Speed            90        slow enough to see, fast enough to hit
    Range                   1500
    Salvo Count             4         missiles per trigger pull
    Salvo Interval Ms       200
    Refire Ms               3000
    Spread Deg              2         spreads a salvo a little
  (Damage)
    Direct Hit Damage       900
    Splash Damage           300
    Splash Radius           10
  Homing
    Homing                  on
    Lock Targets            on        follows the locked vehicle / mech
    Turn Rate               70        deg/s; 40 = dodgeable, 120 = very hard to dodge
    Arming Distance         20        flies straight out of the tube first
    Loft Angle              35        back rack top attack; 0 for arm launchers
  Ammo                      Magazine 8, Reload 10 s

  The visible missile flies straight to the lock point; the real (server) missile steers.
  Against a moving target the impact effect appears where the real missile hit.


TESTING
-------
Turn on Diagnostics > Debug and watch the log:
  [CGS TITAN CANNON] TITAN_CANNON_L charging 1.5 s
  [CGS TITAN CANNON] ... fired, target=..., distance=1400 m, lands in 2000 ms
  [CGS TITAN CANNON] impact at ...: direct=... (-1500 hp), splash targets=2
  [CGS TITAN MISSILE] LAUNCH ... lock=<entity> point=...
  [BFA ROCKET MODULE] DETONATE ...


ROCKET POD - ROCKETS FROM DIFFERENT TUBES (Tubes category, CGS_Titan_Missile)
---------------------------------------------------------------------------
Instead of placing one point per tube, describe each BLOCK of tubes as a grid:
  1. Tube Grids > one entry per block (the Chaos pod has 2 blocks: left and right).
  2. First Tube: move it with the gizmo onto the TOP-LEFT tube mouth of that block.
     Blue (Z) arrow out of the tube, red (X) to the right, green (Y) up.
  3. Columns / Rows: tubes across and down (pod: 2 x 12 per block).
  4. Column Spacing: distance to the tube on the right. Row Spacing: distance to the tube below.
     In model units (before the weapon's scale). Odd Row Shift for staggered tubes.
  5. Random Tube Order on: every tube fires once (random order) before any fires again,
     so a salvo ripples out all over the pod.
Check: turn Debug on, fire, watch where the rockets start, adjust spacing until they leave
the tube mouths. Leave Tube Grids empty to use Muzzle Point / Barrel Points instead.
