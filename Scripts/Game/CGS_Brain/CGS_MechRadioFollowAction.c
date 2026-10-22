// Radio FOLLOW: command the assigned mech to follow its owner.
class CGS_MechRadioFollowAction : ScriptedUserAction
{
 override bool GetActionNameScript(out string outName)
 {
  outName = "FOLLOW ME";
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
  CGS_MechTacticalThrowComponent.RecallForPlayer(pUserEntity);
 }
}
