// Select a ground landing destination in the direction the player character faces.
// Add this action to the mech's ActionsManagerComponent default context.
class CGS_MechMarkThrowTargetAction : ScriptedUserAction
{
	protected CGS_MechTacticalThrowComponent GetThrower()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;
		CGS_MechTacticalThrowComponent c = CGS_MechTacticalThrowComponent.Cast(owner.FindComponent(CGS_MechTacticalThrowComponent));
		if (c)
			return c;
		IEntity root = owner.GetRootParent();
		if (!root)
			return null;
		return CGS_MechTacticalThrowComponent.Cast(root.FindComponent(CGS_MechTacticalThrowComponent));
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Mech: MARK THROW TARGET (FACING)";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		CGS_MechTacticalThrowComponent c = GetThrower();
		return c && c.CanOffer(user);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		return CanBeShownScript(user);
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		CGS_MechTacticalThrowComponent c = GetThrower();
		if (c)
			c.ServerMarkThrowTarget(pUserEntity);
	}
}
