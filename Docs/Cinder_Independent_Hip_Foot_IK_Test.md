# Cinder Walker 01/02 — hip-directed foot IK repair (Workbench test)

## Observed fault

In the user's recording, **commanded hip yaw and attached hip yaw agree**, but both legs spread, cross and reach sideways when the independent hip turns. This demonstrates a mismatch between the lower-body rotation and the *foot target reference frame* rather than missing steering input.

## Source causes in Mech Core main

- `BFA_MechLegComponent.GetWalkerGaitBasis()` looked for case-sensitive slot `"Hip"` only. **Both Cinder `Walker_01.et` and `Walker_02.et` name it `"hip"`**, causing a silent return to the *unrotated root chassis* while `CGS_HipTwistComponent` rotates the separate hip and leg slot entities.
- Simply changing to `"hip"` is NOT sufficient: **Walker_02's authored hip slot has a rest angle of -180 degrees**, while its legs are root siblings. Using the hip prefab's raw forward/right axes reverses foot lead and no-cross constraints even with a correct lookup.
- `BFA_MechLegPair.UpdateObservedMotion()` already gets independent hip heading and counts hip yaw changes in gait rate. The movement/steering machinery itself should be preserved.

## Implementation

Only `Scripts/Game/Mech/BFA_MechLegComponent.c` changes:

1. Resolve actual `"hip"` slot for Cinder first and original `"Hip"` for legacy SampleMech second; use the hip child ENTITY POSITION as the yaw pivot, **not** its potentially -180-degree mesh orientation. Retry if slot children have not spawned yet.
2. For opted-in independent hip walkers only, calculate the foot gait forward/right basis from the *same root axes plus* `GetIndependentHipYaw() + GetWalkerDriveForwardOffsetDeg()` that W/S already uses.
3. Project swing lead into these lower-body axes and retain existing side-lead and forward-lead limits. Constrain landing targets to remain on the correct physical side of the turned hip using `m_fNoCrossMargin`. This aims to prevent feet being steered towards the old torso-facing rest positions and crossing under the walker.
4. Preserve **world-space foot planting**, ground tracing, step phases, leg damage/limp, hip yaw replication, weapon systems, camera behavior, sound and animations. Keep the original root-based swing calculation unchanged on all non-independent-hip walkers.
5. A one-time `[BFA GAIT HIP BASIS]` message for each leg identifies that the rotating hip pivot is actually resolved in a fresh spawned instance.

## Mandatory Workbench tests — not yet performed

1. Build current Mech Core with this branch and latest CGS-Walkers main. Spawn **fresh Walker_01.et** and **Walker_02.et** separately. Both now have independent Hip Twist configuration after CGS-Walkers PR #21.
2. Idle: the feet stand at their authored rest locations. Check for `[BFA GAIT HIP BASIS]` in script logs. A missing message means the hip child was never resolved or legs did not initialize.
3. Stationary A/D: at 0°, ~45°, ~90°, and beyond the root-follow limit, both legs visibly replant on their physical left/right sides instead of swinging wide, crossing, or trying to follow the old facing direction. Check the Cinder servo HUD's `STEER / CMD / HIP ENTITY YAW` values and `[CGS HIP TWIST]` logs.
4. Move W/S after each partial turn: foot strides align with hip forward, the chassis advances along its intended facing, and A/D alone does not strafe.
5. Verify authored Walker_02 **-180° hip rest orientation** does not cause backward steps or inverted left/right landing positions. Compare Walker_01 whose hip rest yaw is 0°.
6. Test uneven terrain, rapid A→D reversals, destroyed/damaged legs and repair. Confirm no unstable foot target jump on spawn, no crossing and no sinking.
7. Test an unrelated regular Mech Core walker/SampleMech (legacy `"Hip"`) and observe that its non-independent/legacy gait is unchanged.
8. Test pilot and remote observer on a dedicated MP server. Gait uses existing server root and hip yaw data; do not assume remote IK sync until visually confirmed.

## Scope and limitation

This fixes a confirmed **gait-reference bug**. It does **not** claim that large 90-degree turns have perfectly natural acceleration or reach. If the gait still stretches during turns after this patch, the next measured change should be an opt-in **foot reach / forced replant threshold** on independently steered legs, not a walking direction offset, knee rig rebuild or camera yaw rewrite.

Source-level QA only; requires Enfusion Workbench compilation and gameplay validation. Keep PR draft until both Cinder walkers and a non-Cinder control prefab pass.
