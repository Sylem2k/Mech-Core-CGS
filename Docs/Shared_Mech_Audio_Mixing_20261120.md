# Shared mech movement audio audit — 2026-11-20

## Confirmed implementation
- BFA_MechLegComponent emits SOUND_MECH_SERVO on leg swing and SOUND_MECH_STEP when the foot lands, using SCR_SoundManagerModule.CreateAndPlayAudioSource on the leg. It does not send a separate sound RPC.
- All 14 leg-related prefabs inspected in the Warfare project (light, medium, heavy and Warlord variants under Mech_Parts/Legs, Mech_Parts/Boss and Mech_Parts/WarLord) reference the same Core Sounds/SampleMech.acp, GUID 8B7D7D9A72F30BAD.
- This audio project also declares track and hover events. Separate Core Sounds/Tracked.acp and Sounds/Hover.acp are present and are NOT edited by this test. The downstream Warfare Sounds/Mech/TestMech.acp is separate too.
- Original shared walker mix used SFX (FinalMix mask 8192), STEP bank -8 dB, SERVO bank -16 dB and linear distance amplitude.

## Warlord combat sound lessons, merged Warfare PRs 163 and 164
- Route events into their appropriate FinalMix bus; gunshots use WPN_Shots (mask 2), bullet hits WNP_BulletHits (8), explosions WPN_Explosions (4), not generic SFX.
- Footfalls and leg servos are vehicle animation events; use VEH_Animations (mask 64) instead of a weapon bus.
- Lower bank gain and set explicit distance attenuation, rather than extending near-full loudness to long ranges.
- Do not play the same impact once through an inherited source and again through a boss RPC. The walker left and right leg contacts are separate intentional events; do not suppress them without evidence of duplication.
- Correct category does NOT guarantee any third-party earplug mod will filter an event; validate independently.

## Test changes, in Sounds/SampleMech.acp only
- STEP: -8 dB to -20 dB; custom attenuation points (distance m, amplitude) = (8,1), (32,.65), (90,.28), (170,.07), (260,0).
- SERVO: -16 dB to -26 dB; custom attenuation = (5,1), (22,.55), (60,.14), (100,.025), (145,0).
- Audio project's common output mixer SFX (8192) to VEH_Animations (64). This also changes bus selection for any track/hover event emitted from this shared project, but DOES NOT modify the distinct Tracked.acp or Hover.acp files.
- Seven original event identifiers and source WAV GUIDs remain intact. No changes to gait, hips, torso, weapon fire, replication, part damage or movement code.
- These are source-level settings, NOT tested or calibrated acoustic results. Warlord leg footsteps will also get the lower bank level because its leg prefabs reference this resource.

## Required Workbench and multiplayer checks
1. Reimport and Build Sound on Sounds/SampleMech.acp; verify no graph warnings, missing resources, or 'No audio source present in graph'.
2. Test both STEP and SERVO events from the Audio Editor first.
3. Spawn a fresh SampleMech: walk, sprint, turn hips in place, and stop; check that feet land audibly without deafening cockpit audio. Compare at about 5, 30, 100 and 250m.
4. Spawn a fresh Warlord and test its footsteps separately from its weapon/warhorn projects; confirm it remains appropriately imposing.
5. Dedicated multiplayer: driver and outside listener, compare close/distant sound, ensure no double-play or constant gain from remote sources, and verify a group of walkers does not saturate output.
6. Test the vehicle animation audio setting and WCS Earplugs explicitly. The bus may or may not be targeted by that plugin.
7. If sound is too weak, adjust only these two bank dB values; do not change locomotion, IK or replication to solve a sound mix problem.

## Follow-up only after listening test
- Consider size-specific light/heavy/Warlord sound profiles if sharing one sound curve proves inadequate.
- Independently tune tracked and hover engine/rolling audio and appropriate VEH_Engine/VEH_Tires buses (their dedicated projects remain unchanged here).
- Continue Warlord's shot/impact/explosion separation for weapon audio, without duplicating its per-weapon routing across every chassis.
