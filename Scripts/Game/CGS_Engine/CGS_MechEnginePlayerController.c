// CGS MechCore - engine control requests, isolated from existing mech movement RPCs.
// Requests travel on the player's owned controller; the server validates DRIVER station.
modded class SCR_PlayerController
{
	void CGS_Mech_RequestEnginePower(IEntity mech, bool start)
	{
		if (!mech)
			return;

		IEntity root = mech.GetRootParent();
		if (root)
			mech = root;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			CGS_Mech_ServerEnginePower(rpl.Id(), start);
			return;
		}

		Rpc(CGS_Mech_RpcAsk_EnginePower, rpl.Id(), start);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void CGS_Mech_RpcAsk_EnginePower(RplId mechId, bool start)
	{
		CGS_Mech_ServerEnginePower(mechId, start);
	}

	protected void CGS_Mech_ServerEnginePower(RplId mechId, bool start)
	{
		if (!Replication.IsServer())
			return;

		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		CGS_MechEngineComponent engine = CGS_MechEngineComponent.Cast(mech.FindComponent(CGS_MechEngineComponent));
		if (!engine)
			return;

		// The authenticated player controller must own the exact DRIVER station.
		// Never trust the client's user entity or a client-supplied pilot ID.
		if (!BFA_Mech_IsStationController(mech, "DRIVER"))
			return;

		IEntity pilot = null;
		BFA_MechControllerComponent controls = BFA_MechControllerComponent.Cast(mech.FindComponent(BFA_MechControllerComponent));
		if (controls)
			pilot = controls.BFA_GetPilotOccupant();

		// A seat occupant must exist, and must be controlled by this requester.
		if (!pilot || !GetGame())
			return;

		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return;

		int pilotId = manager.GetPlayerIdFromControlledEntity(pilot);
		if (pilotId <= 0 || manager.GetPlayerController(pilotId) != this)
			return;

		if (start)
			engine.ServerStart(pilot);
		else
			engine.ServerStop(pilot);
	}
}
