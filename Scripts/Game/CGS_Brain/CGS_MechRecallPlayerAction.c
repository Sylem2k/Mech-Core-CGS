// Remote recall is a PLAYER-side action; attach to a player's ActionsManagerComponent,
// not to the mech. The player's owned companion is looked up server-side.
// No V1 prefabs are changed. Separate phone/radio or keybind UI can be added later.
class CGS_MechRecallPlayerAction : ScriptedUserAction
{
	override bool GetActionNameScript(out string outName)
	{
		outName = "CALL MY MECH";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		IEntity owner = GetOwner();
		return user && ChimeraCharacter.Cast(user) && owner && (user == owner || user.GetRootParent() == owner);
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
