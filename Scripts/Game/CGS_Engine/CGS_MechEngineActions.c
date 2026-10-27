// CGS MECH CORE - startup/shutdown cockpit actions.
// Assign each action to its own ActionsManagerComponent context whose Position
// is a DRAGGABLE PointInfo in the chassis or cockpit prefab.
// The server validates the seated pilot before changing engine state.
class CGS_MechEngineStartAction : ScriptedUserAction
{
	protected CGS_MechEngineComponent GetEngine()
	{
		return CGS_MechEngineComponent.Find(GetOwner());
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "ENGINE: START CORE";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		CGS_MechEngineComponent engine = GetEngine();
		return engine && engine.CanOperate(user) && engine.GetEngineState() == CGS_MechEngineComponent.ENGINE_OFF;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (CanBeShownScript(user))
			return true;
		SetCannotPerformReason("Pilot only. Engine must be OFF.");
		return false;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		CGS_MechEngineComponent engine = CGS_MechEngineComponent.Find(pOwnerEntity);
		if (engine)
			engine.ServerStart(pUserEntity);
	}
}

//------------------------------------------------------------------------------------------------
class CGS_MechEngineStopAction : ScriptedUserAction
{
	protected CGS_MechEngineComponent GetEngine()
	{
		return CGS_MechEngineComponent.Find(GetOwner());
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "ENGINE: SHUT DOWN";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		CGS_MechEngineComponent engine = GetEngine();
		return engine && engine.CanOperate(user) && engine.GetEngineState() != CGS_MechEngineComponent.ENGINE_OFF;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (CanBeShownScript(user))
			return true;
		SetCannotPerformReason("Pilot only. Engine is already OFF.");
		return false;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		CGS_MechEngineComponent engine = CGS_MechEngineComponent.Find(pOwnerEntity);
		if (engine)
			engine.ServerStop(pUserEntity);
	}
}
