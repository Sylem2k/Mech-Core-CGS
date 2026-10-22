// Radio GUARD: keep the assigned mech at its current location.
class CGS_MechRadioGuardAction : ScriptedUserAction
{
 override bool GetActionNameScript(out string outName)
 {
  outName = "GUARD POSITION";
  return true;
 }
 override bool CanBeShownScript(IEntity user)
 {
  return user != null && ChimeraCharacter.Cast(user) != null && GetOwner() != null;
 }
 override bool CanBePerformedScript(IEntity user)
 {
  return CanBeShownScript(user);
 }
 override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 {
  if (!Replication.IsServer())
   return;
  CGS_MechTacticalThrowComponent.GuardForPlayer(pUserEntity);
 }
}
