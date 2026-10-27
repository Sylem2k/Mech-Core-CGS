# CGS MECH CORE / MW PROJECT — CONTINUOUS HANDOFF
Updated: 2026-10-27. This is my living first-person development log. I update this EXACT file after every meaningful change, including commits, attempted fixes, compile results, in-game tests, blockers and next steps. I do not replace it with another dated handoff.

## Repositories and rules
- Official published Core: https://github.com/Sylem2k/Mech-Core-CGS
- MW project (prefabs, frame setup, Mech Bay and content): https://github.com/Sylem2k/Reforger-Warfare--MechWarriorProject-main
- Assets: https://github.com/Sylem2k/CGS-MW-Assets
- Duplicate legacy Core: https://github.com/Sylem2k/CGS-Mech-Core — do not use for new development or confuse with published Core. Both contain the same addon GUID.
- I work on two PCs. My Workbench addons directory on one PC is C:\Users\Dylan\OneDrive\Documents\My Games\ArmaReforgerWorkbench\addons. My older local Core folder there is called CGS-Mech-Core and is connected to the DUPLICATE remote; do not assume it pulls official Core changes.
- I require fresh isolated feature branches, no unapproved main merges, file names unchanged for existing scripts, and full updated source rather than snippets. Changes only count as tested after I compile and test them in Workbench.
- Preserve previously working walker/tracked/hover movement, engine Start/Stop, cockpit buttons, weapon switching, and existing HUD.
- Keep this file synchronized in Core AND MW after every update. Record branch, commit, exact paths touched, test results, and outstanding steps. If a feature touches only one repository, still update both handoffs so either one can resume development.

## Current working and incomplete systems
- Engine starts OFF; pilot-only Start/Stop actions request authoritative replicated engine changes through SCR_PlayerController. An engine-running interlock blocks movement when engine OFF or STARTING. Earlier gameplay test confirmed controls and interlock worked, but subsequent branch combinations caused invisible buttons.
- Cockpit action fix uses HasLocalEffectOnlyScript() for both actions and larger, reachable action contexts. Main branches have these changes; user still needs to retest combined current builds.
- Walker, tracked, hover locomotion previously worked. Track and hover part prefabs now have BFA_MechPartHitZone configurations (700 HP per element); existing CGS_MechPartsPanel maps their SlotManager slots to left/right leg HUD. Compilation and game testing pending.
- CGS_MechRepairComponent is a READ-ONLY registry resolving named SlotManager attachments and installed hardpoint modules. It does NOT create actions, inflict damage or heal. PointInfo alone is not a usable action.
- Wrench in MW currently handles refitting and inventory tool verification; it does NOT yet heal parts. Need actual server-authoritative wrench repair action, development-only damage action, health API verification and track damage effects.
- MW Mech Bay owns loadout installation; SlotManager owns structural assemblies. Repair should not automatically spawn weapons or restore removed modules.
- No complete test of track HP -> degraded motion -> destroy -> wrench repair -> restored motion exists yet. Track/hover damage performance penalties and weapon HP% HUD are incomplete.
- Current prefabs have provisioned track service positions, not confirmed reachable by gameplay. The test must wire real repair actions to these points.
- Tracked/hover drive audio remains unresolved separately. Do not overwrite movement to fix audio.

## Latest code state (2026-10-27)
- Official Core main last checked: 61337309e1a43f16f54136fbbfceab9b2a54b393
- MW main last checked: 0912af7dfc33127209d1b0e115318efa04161915
- Assets main last checked: 7f543a8e57df9bc14e7dab825f990c24666344dd
- Core working backup: backup/working-engine-movement-20261027
- MW working backup: backup/working-assault-tracked-engine-20261027
- Core track/hover HUD test: feat/track-hover-hud-20261027
- MW track/hover prefab test: feat/track-hover-damage-hud-20261027
- Core/MW combined engine-button fix: fix/track-test-start-stop-20261027; changes exist on main at time of audit.
- The duplicate CGS-Mech-Core and official Mech-Core-CGS main trees matched in source after synchronization; remaining difference was resourceDatabase.rdb. Duplicate's PR #1 was prepared, not an alternative official source. Avoid wiping local Workbench resourceDatabase.rdb changes.

## Next acceptance test
1. Verify official Core and MW project clones point at the intended remotes; backup work from both PCs before migrating duplicate Core.
2. Compile both current main versions in Workbench and confirm Start and Stop controls are reachable from DRIVER seat, engine transitions OFF/STARTING/RUNNING, and tracked mech moves.
3. Check track and hover parts appear on the existing damage HUD.
4. Implement real external per-slot repair actions for Track_L1/L2/R1/R2 and hover equivalents, with server-side wrench verification and independent part HP. Include safe development-only damage commands/actions.
5. Implement track/hover movement degradation using validated part HP without rewriting working locomotion; test server and clients.
6. Extend later to walker legs, weapon hardpoints, and weapon HP% without disrupting existing HUD.
7. Consolidate shared systems gradually into official Core while keeping MW prefabs/config and Assets separate.

## Change log (append newest changes here, keeping prior entries)
- 2026-10-27: Established the living handoff convention in both Core and MW on independent docs-only feature branches. No gameplay script, prefab or main code changed. This file is the authoritative continuity note until updated again.

## Core-specific notes
Core owns CGS_MechEngineActions.c, CGS_MechEnginePlayerController.c, CGS_MechRepairComponent.c, CGS_MechPartsPanel.c and BFA_MechMovementComponent.c. Keep external MW dependency classes out of Core unless migrated together.

- 2026-10-27: Assault Tracked changed from four to TWO SlotManager track attachments on MW branch feat/assault-two-tracks-20261027, prefab commit 2a2f5a6d6d8916dd666865bba8182c6064b010a0. Retained Track_L1 and Track_R1; removed Track_L2 and Track_R2 plus their read-only CGS_MechRepairSite entries. Start/Stop actions and engine/movement scripts are untouched. Track part meshes may be SHORT because only front original meshes remain: inspect visually in Workbench before merging. Current repair sites still have NO usable repair actions. No compile or game test yet. Core movement/HUD still recognizes L2/R2 optionally for OTHER frames and does not require modifications.

- 2026-10-27: User still reports NO cockpit Start/Stop prompts and NO repair points with wrench equipped. Verified MW Assault Tracked original Start/Stop actions and contexts are present, but engine OFF and exact failure cause unresolved. Core's CGS_MechRepairComponent is READ-ONLY; NO repair ScriptedUserAction exists; wrench does not change that. In MW branch fix/assault-engine-action-fallback-20261027, added SECOND pair of pilot-only Start/Stop actions to default context with visibility range 30, preserving original cockpit contexts/actions, movement code and two-track prefab. Test commit 82d18087bc01c275596f7bc7372e36c1540812f1. This is a DIAGNOSTIC fallback, not a verified fix. Test from actual DRIVER seat; compile and collect actions/engine logs. Never claim repairs function until genuine server-authoritative wrench actions plus hit-zone healing are added.

- 2026-10-27: User supplied Workbench log Pasted text(20261027-221950).txt and reported no engine Start/Stop prompts at all. Tracked and Hover entities spawn, no Game script compilation error shown, warning '[BFA MOUNT] One or more mounts failed to resolve' (separate issue). Current Core CGS_MechEngineActions.c hid both buttons in CanBeShownScript when engine.CanOperate(user) failed. Created isolated paired branches fix/engine-action-visibility-probe-20261027. Core commit 6982e97e85a110248c9ff946c8f281c126fc66d4 temporarily SHOWS START/STOP for the matching engine state without requiring pilot lookup, while CanBePerformedScript STILL requires engine.CanOperate(user), and existing authoritative server RPC still validates DRIVER. MW branch inherits previous fallback actions bound to default context, in addition to named cockpit actions. DIAGNOSTIC ONLY: non-pilots may see disabled prompts. NOT gameplay-tested. Test with BOTH branches, actual DRIVER seat, check whether Start prompt appears; if visible but refused, troubleshoot station occupant; if absent, troubleshoot Workbench checkout/addon/script/prefab and ActionsManager registration, not pilot check. Revert broad visibility after cause is resolved. Repair wrench actions still not implemented; do not tell user repairs work.

- 2026-10-27: Added unconditional temporary CGS_MechEngineStatusProbeAction (Core commit 7f229de18392f538ead98d7b1a80bce2e389c088) and attached it to the Assault Tracked default action context (MW commit 4277c4f1a061a62d915bbe9a2d58552c323958d3). Its label reads ENGINE DIAG: OFF/RUNNING or COMPONENT MISSING and clicking logs [CGS ENGINE DIAG] engine, pilotAllowed and actor. If NO diagnostic action appears, suspect incorrect branch/addon loaded, ActionsManager registration, or interaction distance; not CanOperate pilot gating. This is TEMPORARY and must be removed after troubleshooting. The user must test paired branches, not main. Keep engine gameplay server authorization intact; no repair functionality added.

## 2026-10-27 — Restore last visible engine buttons: regression-first test (UNTESTED)
I clarified the actual history: START/STOP interaction prompts originally APPEARED but did nothing. After I connected button actions through the replicated engine RPC and engine RUNNING movement gate, the mech could start and drive. In a subsequent prefab update, the buttons DISAPPEARED. I should have compared the first bad change with the last known good change; modifying pilot permissions and adding new actions was premature.

I inspected the MW prefab Git history. On MW commit 31498ed ("ok button in view") and the immediately pre-regression revision 0694530a93, the original action-context points were:
- START: UserActionContext GUID {6AB70BFADE701996}, ContextName "Start Mech", PointInfo GUID {6AB70BFACC745ACE}, Offset 0 2.1406 0.6673, Radius 8
- STOP: UserActionContext GUID {6AB70BFABFB3E9E0}, ContextName "StopMech", PointInfo GUID {6AB70BFABFB3EAE6}, Offset 0.352 1.8377 0.8343, Radius 8
Commit c281278ada8 ("moved buttons now there not visable so cant drive it") first explicitly reported no buttons and changed both points/radii. Later changes moved points further, modified cache/filter and added fallback/diagnostic actions, without confirming the root cause.

I created paired branches fix/restore-working-engine-buttons-20261027 from official Core main and MW main. In MW prefab commit 4091b442a9e07f6ad9d2aaf12d74eca2ccb16de4, I restored ONLY the two original START and STOP UserActionContext blocks to their exact 0694530 version, including removal of later added Angles and FilterActionsUsingCache overrides. I did NOT revert engine script, pilot check, RPC, replicated engine state, movement interlock, track count, track hit zones, repair registry, other action contexts, or weapon systems. Existing extra default-context fallback actions and diagnostic action remain unchanged, for controlled A/B testing. In Core I added only this handoff documentation, no engine code changes. IMPORTANT: The previous exterior-switch redesign is an unmerged separate branch fix/exterior-engine-switches-20261027; DO NOT merge or use it during this regression test.

Next test: On home PC, from C:\Users\codya\Documents\My Games\ArmaReforgerWorkbench\addons, fetch MW and switch to fix/restore-working-engine-buttons-20261027. Core can stay on main (or official Core documentation-only matching branch) because NO Core source was modified by this test. Restart Workbench completely; inspect original exterior START/STOP prompts and attempt operation under same conditions as the last successful in-game test. Record exactly which actions appear, whether start transitions OFF -> STARTING -> RUNNING, and whether driving is permitted when RUNNING. If buttons visible but unusable, isolate pilot authorization separately rather than repositioning action points again. If original buttons remain invisible, inspect ActionsManager, world/model offsets, scaling 1.6, action-context discovery and addon checkout. Log outcomes before any further patch. Compilation and gameplay have NOT been verified. Repair registry remains READ-ONLY: no wrench repair action, track repair interaction or server-side healing yet.


## 2026-10-27 — In-cockpit buttons absent at user-positioned locations
User reported "buttons inside do not show where i put them" and provided Workbench log Pasted text(20261027-234132).txt. Log shows Assault_Gravemarch_Gun_Tracked1 and Hover1 spawn and no game-script compile failure in that excerpt; '[BFA MOUNT] One or more mounts failed to resolve' is also present but NOT proven related. Do not interpret early 'Slot_AvailableActions Has no Content!' warning as proof of action failure. Absence of '[CGS ENGINE] initialized...' is NOT conclusive: BFA_MechLog.Log suppresses NORMAL level output unless global log debug is enabled, even when engine m_bDebugEngine=1.

Confirmed by fetching actual source: CGS_MechEngineComponent.m_StartButtonPoint and m_StopButtonPoint are REFERENCE locations used by GetStartButtonWorldPoint/GetStopButtonWorldPoint; they do NOT create or reposition UserActions. Actual clickable START/STOP contexts are under MW prefab ActionsManagerComponent.ActionContexts, ContextName 'Start Mech' and 'StopMech', with ParentContextList of corresponding CGS_MechEngineStartAction/StopAction. These positions differ significantly from the engine component's reference PointInfos. Explain this distinction clearly; do not silently move user-positioned action gizmos.

Important latest-ref difference: Official MW main now includes user commit 144838c40931 ("moved buttons closer to seat"), with Start ActionContext offset -0.2108 1.8464 0.6673 and Stop offset 0.1552 1.7657 0.8343. Branch fix/restore-working-engine-buttons-20261027 was created earlier from main and keeps restored action-context offsets 0 2.1406 0.6673 (START) and 0.352 1.8377 0.8343 (STOP). Do NOT assume user's saved new Workbench placements exist in old feature branch. No new scripts or prefab edits were made in this turn. Before changing action positions again, confirm which MW branch Workbench loads, whether actions appear standing outside versus seated inside, and whether unconditional 'ENGINE DIAG' appears. Workbench test required to determine root cause; avoid unverified engine redesigns. Core current main engine actions are client pilot-only at activation, with server-validated DRIVER identity; exterior-switch overhaul remains a separate unmerged branch, not the selected fix. No functional wrench repairs implemented.
