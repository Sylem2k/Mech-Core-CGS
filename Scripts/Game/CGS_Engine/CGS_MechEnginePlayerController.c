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

		// Always resolve the operator from THIS authenticated PlayerController.
		// Never accept a claimed operator identity from the client's RPC arguments.
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return;

		IEntity user;
		BFA_MechControllerComponent controls = BFA_MechControllerComponent.Cast(mech.FindComponent(BFA_MechControllerComponent));
		if (!controls)
			return;

		IEntity pilot = controls.BFA_GetPilotOccupant();
		if (pilot)
		{
			int pilotId = manager.GetPlayerIdFromControlledEntity(pilot);
			if (pilotId > 0 && manager.GetPlayerController(pilotId) == this)
			{
				// Preserve exact DRIVER station verification for in-seat control.
				BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
				if (core && !core.IsControllerAtStation(this, "DRIVER"))
					return;

				user = pilot;
			}
		}

		if (!user)
		{
			// No valid seated pilot identity: permit ONLY a player controlling
			// their own on-foot character close to the physical exterior switch.
			user = GetControlledEntity();
			if (!user || !ChimeraCharacter.Cast(user))
				return;

			int userId = manager.GetPlayerIdFromControlledEntity(user);
			if (userId <= 0 || manager.GetPlayerController(userId) != this)
				return;
		}

		// Revalidate physical distance, chosen START/STOP point and opt-in flag
		// on the authoritative server. Out-of-range RPCs change nothing.
		if (!engine.CanOperateSwitch(user, start))
			return;

		if (start)
			engine.ServerStart(user);
		else
			engine.ServerStop(user);
	}
}
