//------------------------------------------------------------------------------------------------
//! Add to a mech's ActionsManagerComponent alongside companion FOLLOW / GUARD.
//! Requires CGS_MechTacticalThrowComponent on the mech; disabled by default.
class CGS_MechTacticalThrowAction : ScriptedUserAction
{
	protected CGS_MechTacticalThrowComponent GetThrower()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;
		CGS_MechTacticalThrowComponent thrower = CGS_MechTacticalThrowComponent.Cast(owner.FindComponent(CGS_MechTacticalThrowComponent));
		if (thrower)
			return thrower;
		IEntity root = owner.GetRootParent();
		if (root && root != owner)
			return CGS_MechTacticalThrowComponent.Cast(root.FindComponent(CGS_MechTacticalThrowComponent));
		return null;
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Mech: TACTICAL THROW (TEST)";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		CGS_MechTacticalThrowComponent thrower = GetThrower();
		return thrower && thrower.CanOffer(user);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (CanBeShownScript(user))
			return true;
		SetCannotPerformReason("Experimental tactical throw unavailable");
		return false;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		CGS_MechTacticalThrowComponent thrower = GetThrower();
		if (thrower)
			thrower.ServerThrow(pUserEntity);
	}
}
