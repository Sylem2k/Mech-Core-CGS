// CGS MechCore - reliable engine control through the requesting player's controller.
// Kept self-contained: other SCR_PlayerController extensions expose private/protected
// helpers that cannot safely be referenced from this separate file.
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
		if (!Replication.IsServer() || !GetGame())
			return;

		RplComponent rpl = RplComponent.Cast(Replication.FindItem(mechId));
		if (!rpl)
			return;

		IEntity mech = rpl.GetEntity();
		if (!mech)
			return;

		IEntity root = mech.GetRootParent();
		if (root)
			mech = root;

		CGS_MechEngineComponent engine = CGS_MechEngineComponent.Cast(mech.FindComponent(CGS_MechEngineComponent));
		if (!engine)
			return;

		// If this mech uses multi-crew stations, require the exact DRIVER seat.
		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core && !core.IsControllerAtStation(this, "DRIVER"))
			return;

		BFA_MechControllerComponent controls = BFA_MechControllerComponent.Cast(mech.FindComponent(BFA_MechControllerComponent));
		if (!controls)
			return;

		IEntity pilot = controls.BFA_GetPilotOccupant();
		if (!pilot)
			return;

		// Bind the physical driver occupant to the RPC sender, not to a supplied user ID.
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
