MECH CONTROLS (actions in CharacterGeneralContext) - keyboard/mouse and controller, rebind in Settings > Controls > Mech
Action          Keyboard/mouse   Controller        Used by
MechMove        W                Left stick up/down  driver (forward/back; stick gives both)
MechMoveBack    S                -                   driver
MechTurn        E                Left stick L/R      driver (turn right; stick gives both)
MechTurnLeft    Q                -                   driver
MechStrafe      D                D-pad right         driver
MechStrafeLeft  A                D-pad left          driver
MechSprint      Left Shift       L3 (left stick click)
MechHoverAscend Space           A                   hover only: ascend while held (collective up)
MechHoverDescend Left Ctrl       D-pad down          hover only: descend while held (collective down)
MechJump        Space            A                   walker/tracked jump jets; on hover chassis this instead raises collective
MechFreelook    Left Alt         R3 (right stick click)
MechFire        Left mouse       RT                  driver + gunners
MechFire2       Right mouse      LT                  gunner secondary (aux) group
MechNextGroup   B                RB                  cycle weapon group
MechLoadout     R                D-pad up            field loadout swap
Aiming / camera still uses the normal camera controls (mouse / right stick).
Off switch: "Use Mech Controls" on BFA_MechControllerComponent / BFA_MechCrewStationControllerComponent
(unticked = old borrowed infantry keys).

HOVER COLLECTIVE (all non-Titan hover frames): Space ascends, Left Ctrl descends.
Release both keys to hold the selected WORLD altitude while driving horizontally.
When above a rooftop, descend until the roof support surface is reached; the
original hover clearance still protects the chassis from touching the surface.
Default vertical rates: 6 m/s climb, 4.5 m/s descent. Extra lift ceiling:
45 metres above the normal terrain/roof hover altitude. Adjust these in the
Hover Collective attributes on each hover frame's BFA_MechMovementComponent.
A pilot exiting mid-air leaves the hover mech holding its current altitude.
Walker/tracked input, jump jets, and Titan behavior are unchanged.
