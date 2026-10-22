//------------------------------------------------------------------------------------------------
//! Player companion orders. Add these to the mech's ActionsManagerComponent (default context).
//! Server validation resides in CGS_MechBrainComponent.ServerCommandCompanion.
//! These actions are deliberately independent of MW Gameplay and reusable by other mech games.
class CGS_MechCompanionOrderAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "FOLLOW the player, or GUARD this position.", enums: ParamEnumArray.FromEnum(CGS_EMechCompanionOrder))]
	protected int m_eOrder;

	protected CGS_MechBrainComponent GetBrain()
	{
		IEntity entity = GetOwner();
		if (!entity)
			return null;
		return CGS_MechBrainComponent.Find(entity.GetRootParent());
	}

	override bool GetActionNameScript(out string outName)
	{
		if (m_eOrder == CGS_EMechCompanionOrder.GUARD)
			outName = "Mech: GUARD HERE";
		else
			outName = "Mech: FOLLOW ME";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		CGS_MechBrainComponent brain = GetBrain();
		return brain && brain.CanShowCompanionAction(user);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (!CanBeShownScript(user))
		{
			SetCannotPerformReason("Companion unavailable or too far away");
			return false;
		}
		return true;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		CGS_MechBrainComponent brain = CGS_MechBrainComponent.Find(pOwnerEntity.GetRootParent());
		if (brain)
			brain.ServerCommandCompanion(pUserEntity, m_eOrder);
	}
}
