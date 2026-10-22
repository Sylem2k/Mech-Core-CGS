// Recall action for a handheld CGS Mech Command Radio or a player-side context.
// The item must be held/carried by the caller; the server verifies player identity
// and assigned mech ownership before the companion can switch to FOLLOW.
class CGS_MechRecallPlayerAction : ScriptedUserAction
{
	override bool GetActionNameScript(out string outName)
	{
		outName = "CALL MY MECH";
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
