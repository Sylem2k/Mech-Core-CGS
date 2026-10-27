// CGS MechCore - cockpit START / SHUT DOWN actions (temporary visibility probe).
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
		// Diagnostic only: expose the prompt regardless of local pilot-seat resolution.
		// Activation still requires CanOperate() and is revalidated by server RPC.
		return engine && engine.GetEngineState() == CGS_MechEngineComponent.ENGINE_OFF;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		CGS_MechEngineComponent engine = GetEngine();
		if (engine && engine.CanOperate(user) && CanBeShownScript(user))
			return true;
		SetCannotPerformReason("Pilot only. Engine must be OFF.");
		return false;
	}

	// Execute on the pilot's local client. A reliable RPC on that
	// player's controller asks the dedicated server to change engine state.
	// Running PerformAction only on the server cannot obtain the client's
	// GetGame().GetPlayerController() on a headless dedicated server.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
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
		// Diagnostic only: do not hide prompt when client pilot-seat lookup fails.
		return engine && engine.GetEngineState() != CGS_MechEngineComponent.ENGINE_OFF;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		CGS_MechEngineComponent engine = GetEngine();
		if (engine && engine.CanOperate(user) && CanBeShownScript(user))
			return true;
		SetCannotPerformReason("Pilot only. Engine is already OFF.");
		return false;
	}

	// See the START action: send this from the owner client over
	// CGS_Mech_RequestEnginePower's reliable, server-validated RPC.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
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


// ------------------------------------------------------------------------------------------------
// TEMPORARY WORKBENCH DIAGNOSTIC: unconditional action in the default vehicle context.
// If this action does not appear, the cause is NOT CanOperate(), but the loaded
// prefab/action context/addon. It never changes engine state.
class CGS_MechEngineStatusProbeAction : ScriptedUserAction
{
	override bool GetActionNameScript(out string outName)
	{
		CGS_MechEngineComponent engine = CGS_MechEngineComponent.Find(GetOwner());
		if (!engine)
			outName = "ENGINE DIAG: COMPONENT MISSING";
		else
			outName = "ENGINE DIAG: " + engine.GetEngineStateName() + " (CLICK TO LOG)";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		CGS_MechEngineComponent engine = CGS_MechEngineComponent.Find(pOwnerEntity);
		if (!engine)
		{
			BFA_MechLog.Log("[CGS ENGINE DIAG] No engine component found on action owner or root!", LogLevel.WARNING);
			return;
		}

		BFA_MechLog.Log(string.Format("[CGS ENGINE DIAG] engine=%1 pilotAllowed=%2 actor=%3 vehicle=%4",
			engine.GetEngineStateName(), engine.CanOperate(pUserEntity), pUserEntity, engine.GetOwner()));
	}
}
