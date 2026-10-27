// CGS MechCore - cockpit START / SHUT DOWN actions.
// Add both actions to an ActionsManagerComponent on the mech or a cockpit child.
// Position the action context independently with its own PointInfo in Workbench.
// Engine component PointInfo fields are reference locations, NOT action creators.
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
		CGS_MechEngineComponent engine = CGS_MechEngineComponent.Find(pOwnerEntity);
		if (!engine)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.CGS_Mech_RequestEnginePower(engine.GetOwner(), true);
	}
}

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
		CGS_MechEngineComponent engine = CGS_MechEngineComponent.Find(pOwnerEntity);
		if (!engine)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.CGS_Mech_RequestEnginePower(engine.GetOwner(), false);
	}
}
