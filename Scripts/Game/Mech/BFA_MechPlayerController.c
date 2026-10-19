modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Reliable pilot-state handshake. The compartment event still remains the primary source of truth;
	//! this fills the dedicated-server timing gap where the server copy may not have assigned m_Pilot yet.
	void BFA_Mech_RequestPilotState(IEntity mech, bool active)
	{
		if (!mech)
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			BFA_Mech_ServerPilotState(rpl.Id(), active);
			return;
		}

		Rpc(BFA_Mech_RpcAsk_PilotState, rpl.Id(), active);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_PilotState(RplId mechId, bool active)
	{
		BFA_Mech_ServerPilotState(mechId, active);
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerPilotState(RplId mechId, bool active)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
		if (!movement)
			return;

		IEntity controlled = GetControlledEntity();
		if (!controlled)
			return;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core && !core.IsControllerAtStation(this, "DRIVER"))
			return;

		BFA_MechControllerComponent mechController = BFA_MechControllerComponent.Cast(mech.FindComponent(BFA_MechControllerComponent));
		IEntity seatPilot = null;
		if (mechController)
			seatPilot = mechController.BFA_GetPilotOccupant();

		if (active)
		{
			// Prefer the actual compartment occupant. On dedicated MP GetControlledEntity()
			// can be the character, the vehicle root, or another vehicle-parented entity.
			if (seatPilot)
			{
				PlayerManager playerManager = GetGame().GetPlayerManager();
				if (playerManager)
				{
					int seatPlayerId = playerManager.GetPlayerIdFromControlledEntity(seatPilot);
					if (seatPlayerId > 0)
					{
						PlayerController seatPC = playerManager.GetPlayerController(seatPlayerId);
						if (seatPC && seatPC != this)
							return;
					}
				}

				movement.SetPilot(seatPilot);
				return;
			}

			IEntity controlledRoot = controlled.GetRootParent();
			if (controlled != mech && controlledRoot != mech)
				return;

			movement.SetPilot(controlled);
			return;
		}

		if (seatPilot && movement.IsPilot(seatPilot))
		{
			movement.SetPilot(null);
			return;
		}

		if (movement.IsPilot(controlled))
			movement.SetPilot(null);
	}

	//------------------------------------------------------------------------------------------------
	//! Continuous mech input must travel through the player's owned PlayerController.
	void BFA_Mech_SendDriverInput(IEntity mech, float forward, float strafe, float turn, bool sprint)
	{
		if (!mech)
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		forward = Math.Clamp(forward, -1.0, 1.0);
		strafe = Math.Clamp(strafe, -1.0, 1.0);
		turn = Math.Clamp(turn, -1.0, 1.0);

		if (Replication.IsServer())
		{
			BFA_Mech_ServerDriverInput(rpl.Id(), forward, strafe, turn, sprint, -1);
			return;
		}

		// Sequence each input and record the local prediction at send time. The server echoes the
		// newest sequence it applied in every root snapshot, which lets the driver compare its
		// prediction to the server like-for-like and blend back if they drifted apart.
		int inputSeq = -1;
		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
		if (movement)
			inputSeq = movement.ClientRecordInputSent();

		Rpc(BFA_Mech_RpcAsk_DriverInput, rpl.Id(), forward, strafe, turn, sprint, inputSeq);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_DriverInput(RplId mechId, float forward, float strafe, float turn, bool sprint, int inputSeq)
	{
		BFA_Mech_ServerDriverInput(mechId, forward, strafe, turn, sprint, inputSeq);
	}

	//------------------------------------------------------------------------------------------------
	protected bool BFA_Mech_IsDriver(BFA_MechMovementComponent movement, IEntity mech)
	{
		if (!movement || !mech)
			return false;

		// Multi-crew mechs must validate the exact DRIVER station first. A generic
		// "controlled root == mech" check cannot distinguish DRIVER from GUNNER.
		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core)
			return core.IsControllerAtStation(this, "DRIVER");

		IEntity controlled = GetControlledEntity();
		if (!controlled)
			return false;

		// Compatibility path for old one-seat mechs without BFA_MechCoreComponent.
		// Strongest test: who is physically in the pilot compartment?
		// dedicated-MP controlled-entity changes that were rejecting fire/turn requests.
		BFA_MechControllerComponent mechController = BFA_MechControllerComponent.Cast(mech.FindComponent(BFA_MechControllerComponent));
		if (mechController)
		{
			IEntity seatPilot = mechController.BFA_GetPilotOccupant();
			if (seatPilot)
			{
				if (seatPilot == controlled)
					return true;

				PlayerManager playerManager = GetGame().GetPlayerManager();
				if (playerManager)
				{
					int seatPlayerId = playerManager.GetPlayerIdFromControlledEntity(seatPilot);
					if (seatPlayerId > 0)
					{
						PlayerController seatPC = playerManager.GetPlayerController(seatPlayerId);
						if (seatPC == this)
							return true;
					}
				}
			}
		}

		if (movement.IsPilot(controlled))
			return true;

		if (controlled == mech)
			return true;

		IEntity controlledRoot = controlled.GetRootParent();
		if (controlledRoot == mech)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BFA_Mech_IsStationController(IEntity mech, string stationId)
	{
		if (!mech || stationId.IsEmpty())
			return false;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core)
			return core.IsControllerAtStation(this, stationId);

		// Compatibility fallback for older mechs that have not added the core component yet.
		if (stationId == "DRIVER")
		{
			BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
			return BFA_Mech_IsDriver(movement, mech);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerDriverInput(RplId mechId, float forward, float strafe, float turn, bool sprint, int inputSeq)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
		if (!movement)
			return;

		if (!BFA_Mech_IsDriver(movement, mech))
			return;

		movement.ServerSetDriverInput(forward, strafe, turn, sprint, inputSeq);
	}

	//------------------------------------------------------------------------------------------------
	//! Continuous station aim travels through the owned PlayerController.
	//! The payload is the crosshair POINT in the mech's local space (not a cannon yaw/pitch),
	//! so any number of arms/shoulders - rigid or animated - converge on exactly the same spot.
	//! The aim component sanitizes the length on the server.
	void BFA_Mech_SendAimInput(IEntity mech, string stationId, vector localAimPoint)
	{
		if (!mech || stationId.IsEmpty())
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			BFA_Mech_ServerAimInput(rpl.Id(), stationId, localAimPoint);
			return;
		}

		Rpc(BFA_Mech_RpcAsk_AimInput, rpl.Id(), stationId, localAimPoint);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_AimInput(RplId mechId, string stationId, vector localAimPoint)
	{
		BFA_Mech_ServerAimInput(mechId, stationId, localAimPoint);
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerAimInput(RplId mechId, string stationId, vector localAimPoint)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core && !core.CanStationAim(stationId))
			return;

		if (!BFA_Mech_IsStationController(mech, stationId))
			return;

		BFA_MechAimComponent aim = BFA_MechAimComponent.Cast(mech.FindComponent(BFA_MechAimComponent));
		if (aim)
			aim.ServerSetStationAim(stationId, localAimPoint);
	}


	//------------------------------------------------------------------------------------------------
	//! Fire state is routed by crew station + weapon group. The server owns cadence and shots.
	void BFA_Mech_RequestFireState(IEntity mech, string stationId, int weaponGroup, bool held)
	{
		if (!mech || stationId.IsEmpty() || weaponGroup < 1)
		{
			BFA_MechLog.Log("[BFA FIRE TEST] CLIENT FAIL - invalid fire request arguments", LogLevel.WARNING);
			return;
		}

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
		{
			BFA_MechLog.Log("[BFA FIRE TEST] CLIENT FAIL - mech has no RplComponent", LogLevel.WARNING);
			return;
		}

		BFA_MechLog.Log(string.Format("[BFA FIRE TEST] CLIENT ROUTE station=%1 group=%2 held=%3", stationId, weaponGroup, held));

		if (Replication.IsServer())
		{
			BFA_Mech_ServerFireState(rpl.Id(), stationId, weaponGroup, held);
			return;
		}

		Rpc(BFA_Mech_RpcAsk_FireState, rpl.Id(), stationId, weaponGroup, held);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_FireState(RplId mechId, string stationId, int weaponGroup, bool held)
	{
		BFA_MechLog.Log(string.Format("[BFA FIRE TEST] SERVER RPC station=%1 group=%2 held=%3", stationId, weaponGroup, held));
		BFA_Mech_ServerFireState(mechId, stationId, weaponGroup, held);
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerFireState(RplId mechId, string stationId, int weaponGroup, bool held)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
		{
			BFA_MechLog.Log("[BFA FIRE TEST] SERVER FAIL - mech RplId did not resolve", LogLevel.WARNING);
			return;
		}

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core && !core.CanStationFireGroup(stationId, weaponGroup))
		{
			BFA_MechLog.Log("[BFA FIRE TEST] SERVER REJECT - station cannot fire requested group", LogLevel.WARNING);
			return;
		}

		if (!BFA_Mech_IsStationController(mech, stationId))
		{
			BFA_MechLog.Log("[BFA FIRE TEST] SERVER REJECT - sender is not current station occupant", LogLevel.WARNING);
			return;
		}

		BFA_MechWeaponComponent weapon = BFA_MechWeaponComponent.Cast(mech.FindComponent(BFA_MechWeaponComponent));
		if (!weapon)
		{
			BFA_MechLog.Log("[BFA FIRE TEST] SERVER FAIL - mech has no BFA_MechWeaponComponent", LogLevel.WARNING);
			return;
		}

		BFA_MechLog.Log(string.Format("[BFA FIRE TEST] SERVER ACCEPT station=%1 group=%2 held=%3", stationId, weaponGroup, held));
		weapon.ServerSetFireHeld(stationId, weaponGroup, held);
	}

	//------------------------------------------------------------------------------------------------
	//! Cycle the station main trigger between its configured PRIMARY and SECONDARY weapon groups.
	void BFA_Mech_RequestCycleWeaponGroup(IEntity mech, string stationId)
	{
		if (!mech || stationId.IsEmpty())
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			BFA_Mech_ServerCycleWeaponGroup(rpl.Id(), stationId);
			return;
		}

		Rpc(BFA_Mech_RpcAsk_CycleWeaponGroup, rpl.Id(), stationId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_CycleWeaponGroup(RplId mechId, string stationId)
	{
		BFA_Mech_ServerCycleWeaponGroup(mechId, stationId);
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerCycleWeaponGroup(RplId mechId, string stationId)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		if (!BFA_Mech_IsStationController(mech, stationId))
			return;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core)
			core.ServerCycleWeaponGroup(stationId);
	}

	//------------------------------------------------------------------------------------------------
	//! Switch the station's selected weapon group between chain fire and group fire.
	void BFA_Mech_RequestToggleChainFire(IEntity mech, string stationId)
	{
		if (!mech || stationId.IsEmpty())
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			BFA_Mech_ServerToggleChainFire(rpl.Id(), stationId);
			return;
		}

		Rpc(BFA_Mech_RpcAsk_ToggleChainFire, rpl.Id(), stationId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_ToggleChainFire(RplId mechId, string stationId)
	{
		BFA_Mech_ServerToggleChainFire(mechId, stationId);
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerToggleChainFire(RplId mechId, string stationId)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech || !BFA_Mech_IsStationController(mech, stationId))
			return;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		BFA_MechWeaponComponent router = BFA_MechWeaponComponent.Cast(mech.FindComponent(BFA_MechWeaponComponent));
		if (!core || !router)
			return;

		int group = core.GetSelectedWeaponGroup(stationId);
		if (group < 1 || !core.CanStationFireGroup(stationId, group))
			return;

		router.ServerToggleChainFire(stationId, group);
	}

	//------------------------------------------------------------------------------------------------
	//! Reliable one-shot auxiliary fire, intended for shoulders/missiles/utility weapons.
	void BFA_Mech_RequestFirePulse(IEntity mech, string stationId, int weaponGroup)
	{
		if (!mech || stationId.IsEmpty() || weaponGroup < 1)
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			BFA_Mech_ServerFirePulse(rpl.Id(), stationId, weaponGroup);
			return;
		}

		Rpc(BFA_Mech_RpcAsk_FirePulse, rpl.Id(), stationId, weaponGroup);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_FirePulse(RplId mechId, string stationId, int weaponGroup)
	{
		BFA_Mech_ServerFirePulse(mechId, stationId, weaponGroup);
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerFirePulse(RplId mechId, string stationId, int weaponGroup)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core && !core.CanStationFireGroup(stationId, weaponGroup))
			return;

		if (!BFA_Mech_IsStationController(mech, stationId))
			return;

		BFA_MechWeaponComponent weapons = BFA_MechWeaponComponent.Cast(mech.FindComponent(BFA_MechWeaponComponent));
		if (weapons)
			weapons.ServerFirePulse(stationId, weaponGroup);
	}

	//------------------------------------------------------------------------------------------------
	//! One-shot jump request. Reliable because losing this packet would lose the button press.
	void BFA_Mech_RequestJump(IEntity mech)
	{
		if (!mech)
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			BFA_Mech_ServerJump(rpl.Id());
			return;
		}

		Rpc(BFA_Mech_RpcAsk_Jump, rpl.Id());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_Jump(RplId mechId)
	{
		BFA_Mech_ServerJump(mechId);
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerJump(RplId mechId)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
		if (!BFA_Mech_IsDriver(movement, mech))
			return;

		BFA_SentinelJumpJetComponent jumpJets = BFA_SentinelJumpJetComponent.Cast(mech.FindComponent(BFA_SentinelJumpJetComponent));
		if (jumpJets)
			jumpJets.ServerTryActivate();
	}

	//------------------------------------------------------------------------------------------------
	//! Generic optional hardpoint/module request for current and future BFA mechs.
	void BFA_Mech_RequestSetModule(IEntity mech, int hardpointIndex, int moduleId)
	{
		if (!mech)
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			BFA_Mech_ServerSetModule(rpl.Id(), hardpointIndex, moduleId);
			return;
		}

		Rpc(BFA_Mech_RpcAsk_SetModule, rpl.Id(), hardpointIndex, moduleId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_SetModule(RplId mechId, int hardpointIndex, int moduleId)
	{
		BFA_Mech_ServerSetModule(mechId, hardpointIndex, moduleId);
	}

	//------------------------------------------------------------------------------------------------
	//! Which crew station the sender physically occupies on this mech ("" = none).
	protected string BFA_Mech_FindSenderStation(IEntity mech)
	{
		if (!mech)
			return string.Empty;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (core)
			return core.FindStationForController(this, false);

		// Old one-seat mechs without the core component: the pilot is the only crew.
		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
		if (BFA_Mech_IsDriver(movement, mech))
			return "DRIVER";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerSetModule(RplId mechId, int hardpointIndex, int moduleId)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		string stationId = BFA_Mech_FindSenderStation(mech);
		if (stationId.IsEmpty())
			return;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (hardpoints)
			hardpoints.ServerRequestModule(stationId, hardpointIndex, moduleId);
	}

	//------------------------------------------------------------------------------------------------
	//! Switch the mech to a configured loadout preset. presetIndex -1 = next preset.
	void BFA_Mech_RequestLoadoutPreset(IEntity mech, int presetIndex)
	{
		if (!mech)
			return;

		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			BFA_Mech_ServerLoadoutPreset(rpl.Id(), presetIndex);
			return;
		}

		Rpc(BFA_Mech_RpcAsk_LoadoutPreset, rpl.Id(), presetIndex);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void BFA_Mech_RpcAsk_LoadoutPreset(RplId mechId, int presetIndex)
	{
		BFA_Mech_ServerLoadoutPreset(mechId, presetIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected void BFA_Mech_ServerLoadoutPreset(RplId mechId, int presetIndex)
	{
		IEntity mech = BFA_Mech_ResolveEntity(mechId);
		if (!mech)
			return;

		string stationId = BFA_Mech_FindSenderStation(mech);
		if (stationId.IsEmpty())
			return;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (hardpoints)
			hardpoints.ServerRequestPreset(stationId, presetIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity BFA_Mech_ResolveEntity(RplId mechId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(mechId));
		if (!rpl || !rpl.GetEntity())
			return null;

		IEntity entity = rpl.GetEntity();
		IEntity root = entity.GetRootParent();
		if (root)
			return root;

		return entity;
	}
}
