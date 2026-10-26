[ComponentEditorProps(category: "BFA MechCore", description: "Generic local input bridge for non-driver crew stations such as GUNNER and COMMANDER.")]
class BFA_MechCrewStationControllerComponentClass : ScriptGameComponentClass
{
}

class BFA_MechCrewStationControllerComponent : ScriptGameComponent
{
	[Attribute("100", UIWidgets.EditBox, "Milliseconds between local seat checks.", category: "Crew")]
	protected int m_iSeatWatchMs;

	[Attribute("33", UIWidgets.EditBox, "Milliseconds between station aim network updates.", category: "Multiplayer")]
	protected int m_iNetAimMs;

	[Attribute("16", UIWidgets.EditBox, "Milliseconds between direct CharacterFire polls. This is a fallback for vehicle/cargo seats where VALUE callbacks can be suppressed.", category: "Weapons")]
	protected int m_iFirePollMs;

	[Attribute("true", UIWidgets.CheckBox, "Bind CharacterMelee (middle mouse / R3) as one-shot auxiliary fire for the station auxiliary group.", category: "Weapons")]
	protected bool m_bBindAuxFire;

	[Attribute("true", UIWidgets.CheckBox, "Use the MECH control scheme (own keys for keyboard + controller, rebindable in Settings > Controls > Mech). Off = old borrowed infantry keys.", category: "Weapons")]
	protected bool m_bUseMechControls;

	[Attribute("CharacterReload", UIWidgets.EditBox, "Input action that cycles to the next field loadout preset (BFA_MechHardpointComponent presets). Empty = no key.", category: "Weapons")]
	protected string m_sLoadoutAction;

	protected BFA_MechCoreComponent m_pCore;
	protected BFA_MechAimComponent m_pAim;
	protected string m_sLocalStation;
	protected bool m_bLocalStationActive;
	protected bool m_bFireHeld;
	protected bool m_bInspectHeld;
	protected int m_iHeldPrimaryGroup;

	//------------------------------------------------------------------------------------------------
	protected void ActivateCrewInputContexts(InputManager input)
	{
		if (!input)
			return;

		if (m_bUseMechControls)
		{
			// The Mech actions live in CharacterGeneralContext (adding a brand-new context did not work).
			input.ActivateContext("CharacterGeneralContext");
			// Weapon context too: the normal fire key is read as a backup to Mech Fire.
			input.ActivateContext("CharacterWeaponContext");
			input.ActivateContext("PlayerCameraContext");
			return;
		}

		input.ActivateContext("CharacterGeneralContext");
		input.ActivateContext("CharacterMovementContext");
		input.ActivateContext("CharacterWeaponContext");
		input.ActivateContext("PlayerCameraContext");
	}

	//------------------------------------------------------------------------------------------------
	protected int ResolveSelectedGroup()
	{
		if (!m_pCore || m_sLocalStation.IsEmpty())
			return 0;

		return m_pCore.GetSelectedWeaponGroup(m_sLocalStation);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetPrimaryFireHeld(bool held)
	{
		if (!m_bLocalStationActive || held == m_bFireHeld)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
		{
			Print("[BFA FIRE] LOCAL FAIL - no SCR_PlayerController", LogLevel.WARNING);
			return;
		}

		if (held)
		{
			m_iHeldPrimaryGroup = ResolveSelectedGroup();
			if (m_iHeldPrimaryGroup < 1)
			{
				Print("[BFA FIRE] LOCAL FAIL - station has no selected primary weapon group", LogLevel.WARNING);
				return;
			}
		}

		int group = m_iHeldPrimaryGroup;
		if (group < 1)
			group = ResolveSelectedGroup();

		if (group < 1)
		{
			Print("[BFA FIRE] LOCAL FAIL - could not resolve weapon group", LogLevel.WARNING);
			return;
		}

		m_bFireHeld = held;
		Print(string.Format("[BFA FIRE] %1 trigger %2 (weapon group %3)", m_sLocalStation, held, group));
		pc.BFA_Mech_RequestFireState(GetOwner(), m_sLocalStation, group, held);

		if (!held)
			m_iHeldPrimaryGroup = 0;
	}

	//------------------------------------------------------------------------------------------------
	void OnFireInput(float value)
	{
		bool held = value > 0.5;
		if (held != m_bFireHeld)
			BFA_MechLog.Log(string.Format("[BFA FIRE TEST] ACTION EVENT value=%1 held=%2", value, held));

		SetPrimaryFireHeld(held);
	}

	//------------------------------------------------------------------------------------------------
	//! Direct fire polling is intentionally kept in addition to the action listener.
	//! Cargo/vehicle input contexts can allow camera aim while suppressing CharacterFire VALUE events.
	//! This gives us both a diagnostic and a reliable fallback path.
	protected void FireWatchTick()
	{
		if (!m_bLocalStationActive)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		ActivateCrewInputContexts(input);

		float value = input.GetActionValue("CharacterFire");
		if (m_bUseMechControls)
			value = Math.Max(value, input.GetActionValue("MechFire"));
		bool held = value > 0.5;
		if (held != m_bFireHeld)
			BFA_MechLog.Log(string.Format("[BFA FIRE TEST] POLL value=%1 held=%2", value, held));

		SetPrimaryFireHeld(held);
	}

	//------------------------------------------------------------------------------------------------
	void OnFreelookInput(float value)
	{
		bool inspect = value > 0.5;
		if (inspect == m_bInspectHeld)
			return;

		m_bInspectHeld = inspect;
		if (m_pAim)
			m_pAim.SetInspectMode(m_bInspectHeld);
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Direct number-key group selection. Keep B to cycle; never switch mid-shot.
	protected void SelectNumberedWeaponGroup(int group)
	{
		if (!m_bLocalStationActive || m_bFireHeld)
			return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestSelectWeaponGroup(GetOwner(), m_sLocalStation, group);
	}

	void OnSelectMechGroup1()
	{
		SelectNumberedWeaponGroup(1);
	}

	void OnSelectMechGroup2()
	{
		SelectNumberedWeaponGroup(2);
	}

	void OnSelectMechGroup3()
	{
		SelectNumberedWeaponGroup(3);
	}

	void OnSelectMechGroup4()
	{
		SelectNumberedWeaponGroup(4);
	}

	void OnSelectMechGroup5()
	{
		SelectNumberedWeaponGroup(5);
	}

	void OnSelectMechGroup6()
	{
		SelectNumberedWeaponGroup(6);
	}

	void OnCycleWeaponInput()
	{
		if (!m_bLocalStationActive || m_bFireHeld)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestCycleWeaponGroup(GetOwner(), m_sLocalStation);
	}

	//------------------------------------------------------------------------------------------------
	//! X: selected weapon group chain fire <-> group fire.
	void OnChainFireInput()
	{
		if (!m_bLocalStationActive || m_bFireHeld)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestToggleChainFire(GetOwner(), m_sLocalStation);
	}

	//------------------------------------------------------------------------------------------------
	void OnAuxFireInput()
	{
		if (!m_bLocalStationActive || !m_pCore)
			return;

		int auxGroup = m_pCore.GetStationAuxWeaponGroup(m_sLocalStation);
		if (auxGroup < 1)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestFirePulse(GetOwner(), m_sLocalStation, auxGroup);
	}

	//------------------------------------------------------------------------------------------------
	void OnLoadoutInput()
	{
		if (!m_bLocalStationActive || m_bFireHeld)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestLoadoutPreset(GetOwner(), -1);
	}

	//------------------------------------------------------------------------------------------------
	protected void NetAimTick()
	{
		if (!m_bLocalStationActive || !m_pAim)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_SendAimInput(GetOwner(), m_sLocalStation, m_pAim.GetLocalAimPointMechLocal());
	}

	//------------------------------------------------------------------------------------------------
	protected void StartLocalStation(string stationId)
	{
		if (stationId.IsEmpty() || m_bLocalStationActive)
			return;

		m_sLocalStation = stationId;
		m_bLocalStationActive = true;

		BFA_MechControllerComponent viewController = BFA_MechControllerComponent.Cast(GetOwner().FindComponent(BFA_MechControllerComponent));
		if (viewController)
			viewController.CGS_BeginLocalCrewView();
		m_bFireHeld = false;
		m_bInspectHeld = false;
		m_iHeldPrimaryGroup = 0;

		if (m_pAim)
			m_pAim.BeginLocalAim(m_sLocalStation);

		InputManager input = GetGame().GetInputManager();
		ActivateCrewInputContexts(input);
		if (input && m_bUseMechControls)
		{
			input.AddActionListener("MechFire", EActionTrigger.VALUE, OnFireInput);
			input.AddActionListener("MechFreelook", EActionTrigger.VALUE, OnFreelookInput);
			input.AddActionListener("MechNextGroup", EActionTrigger.DOWN, OnCycleWeaponInput);
			input.AddActionListener("MechSelectGroup1", EActionTrigger.DOWN, OnSelectMechGroup1);
			input.AddActionListener("MechSelectGroup2", EActionTrigger.DOWN, OnSelectMechGroup2);
			input.AddActionListener("MechSelectGroup3", EActionTrigger.DOWN, OnSelectMechGroup3);
			input.AddActionListener("MechSelectGroup4", EActionTrigger.DOWN, OnSelectMechGroup4);
			input.AddActionListener("MechSelectGroup5", EActionTrigger.DOWN, OnSelectMechGroup5);
			input.AddActionListener("MechSelectGroup6", EActionTrigger.DOWN, OnSelectMechGroup6);
			if (m_bBindAuxFire)
				input.AddActionListener("MechFire2", EActionTrigger.DOWN, OnAuxFireInput);
			input.AddActionListener("MechLoadout", EActionTrigger.DOWN, OnLoadoutInput);
		}
		else if (input)
		{
			input.AddActionListener("CharacterFire", EActionTrigger.VALUE, OnFireInput);
			input.AddActionListener("Freelook", EActionTrigger.VALUE, OnFreelookInput);
			input.AddActionListener("CharacterNextFireMode", EActionTrigger.DOWN, OnCycleWeaponInput);
			if (m_bBindAuxFire)
				input.AddActionListener("CharacterMelee", EActionTrigger.DOWN, OnAuxFireInput);
			if (!m_sLoadoutAction.IsEmpty())
				input.AddActionListener(m_sLoadoutAction, EActionTrigger.DOWN, OnLoadoutInput);
		}
		if (input)
			input.AddActionListener("MechChainFire", EActionTrigger.DOWN, OnChainFireInput);

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			queue.Remove(NetAimTick);
			queue.CallLater(NetAimTick, Math.Max(m_iNetAimMs, 20), true);

			queue.Remove(FireWatchTick);
			queue.CallLater(FireWatchTick, Math.Max(m_iFirePollMs, 10), true);
		}

		Print(string.Format("[BFA CREW] START station=%1 selectedGroup=%2", m_sLocalStation, ResolveSelectedGroup()));
		NetAimTick();
		FireWatchTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopLocalStation()
	{
		if (!m_bLocalStationActive)
			return;

		BFA_MechControllerComponent viewController = BFA_MechControllerComponent.Cast(GetOwner().FindComponent(BFA_MechControllerComponent));
		if (viewController)
			viewController.CGS_EndLocalCrewView();

		if (m_bFireHeld)
			SetPrimaryFireHeld(false);

		InputManager input = GetGame().GetInputManager();
		if (input)
		{
			input.RemoveActionListener("MechFire", EActionTrigger.VALUE, OnFireInput);
			input.RemoveActionListener("MechFreelook", EActionTrigger.VALUE, OnFreelookInput);
			input.RemoveActionListener("MechNextGroup", EActionTrigger.DOWN, OnCycleWeaponInput);
			input.RemoveActionListener("MechSelectGroup1", EActionTrigger.DOWN, OnSelectMechGroup1);
			input.RemoveActionListener("MechSelectGroup2", EActionTrigger.DOWN, OnSelectMechGroup2);
			input.RemoveActionListener("MechSelectGroup3", EActionTrigger.DOWN, OnSelectMechGroup3);
			input.RemoveActionListener("MechSelectGroup4", EActionTrigger.DOWN, OnSelectMechGroup4);
			input.RemoveActionListener("MechSelectGroup5", EActionTrigger.DOWN, OnSelectMechGroup5);
			input.RemoveActionListener("MechSelectGroup6", EActionTrigger.DOWN, OnSelectMechGroup6);
			input.RemoveActionListener("MechFire2", EActionTrigger.DOWN, OnAuxFireInput);
			input.RemoveActionListener("MechLoadout", EActionTrigger.DOWN, OnLoadoutInput);
			input.RemoveActionListener("MechChainFire", EActionTrigger.DOWN, OnChainFireInput);

			input.RemoveActionListener("CharacterFire", EActionTrigger.VALUE, OnFireInput);
			input.RemoveActionListener("Freelook", EActionTrigger.VALUE, OnFreelookInput);
			input.RemoveActionListener("CharacterNextFireMode", EActionTrigger.DOWN, OnCycleWeaponInput);
			if (m_bBindAuxFire)
				input.RemoveActionListener("CharacterMelee", EActionTrigger.DOWN, OnAuxFireInput);
			if (!m_sLoadoutAction.IsEmpty())
				input.RemoveActionListener(m_sLoadoutAction, EActionTrigger.DOWN, OnLoadoutInput);
		}

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			queue.Remove(NetAimTick);
			queue.Remove(FireWatchTick);
		}

		if (m_pAim)
		{
			m_pAim.SetInspectMode(false);
			m_pAim.EndLocalAim();
		}

		BFA_MechLog.Log("[BFA CREW] local station controls STOPPED station=" + m_sLocalStation);

		m_sLocalStation = string.Empty;
		m_bLocalStationActive = false;
		m_bFireHeld = false;
		m_bInspectHeld = false;
		m_iHeldPrimaryGroup = 0;
	}

	//------------------------------------------------------------------------------------------------
	// ONE shared seat watcher for every mech. Removing a per-mech repeating call from the call queue
	// can cancel the calls of OTHER mechs too, which left gunner seats dead after any mech was
	// deleted. A single static tick that walks the list avoids that.
	protected static ref array<BFA_MechCrewStationControllerComponent> s_aAllStations = {};
	protected static int s_iLastSeatTickMs;
	protected bool m_bLoggedNoStation;

	//------------------------------------------------------------------------------------------------
	protected static void StaticSeatWatchTick()
	{
		s_iLastSeatTickMs = System.GetTickCount();
		for (int i = s_aAllStations.Count() - 1; i >= 0; i--)
		{
			BFA_MechCrewStationControllerComponent station = s_aAllStations[i];
			if (!station)
			{
				s_aAllStations.Remove(i);
				continue;
			}
			station.SeatWatchTick();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureStaticSeatWatch()
	{
		// Restart after a game reload (the call queue is new, the static list is not).
		if (System.GetTickCount() - s_iLastSeatTickMs < 1000 && s_iLastSeatTickMs != 0)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		queue.Remove(StaticSeatWatchTick);
		queue.CallLater(StaticSeatWatchTick, 100, true);
		s_iLastSeatTickMs = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! True when the local player sits anywhere in this mech.
	protected bool IsLocalPlayerAboard(PlayerController pc)
	{
		IEntity me = pc.GetControlledEntity();
		return me && me.GetRootParent() == GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	void SeatWatchTick()
	{
		if (!m_pCore)
			return;

		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return;

		if (m_bLocalStationActive)
		{
			BFA_MechControllerComponent viewController = BFA_MechControllerComponent.Cast(GetOwner().FindComponent(BFA_MechControllerComponent));
			if (viewController)
				viewController.CGS_RefreshPilotExteriorMask();

			if (!m_pCore.IsControllerAtStation(pc, m_sLocalStation))
				StopLocalStation();
			return;
		}

		string stationId = m_pCore.FindStationForController(pc, true);
		if (!stationId.IsEmpty())
		{
			m_bLoggedNoStation = false;
			StartLocalStation(stationId);
			return;
		}

		// Diagnostic: aboard, not driving, but no gunner station matched the seat.
		if (!m_bLoggedNoStation && IsLocalPlayerAboard(pc) && !m_pCore.IsControllerAtStation(pc, "DRIVER"))
		{
			m_bLoggedNoStation = true;
			Print(string.Format("[BFA CREW] %1: you are aboard but no gunner station matches your seat. Check each station's Compartment Index (seat order in the compartment manager, 0 = first).", GetOwner()), LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_pCore = BFA_MechCoreComponent.Cast(owner.FindComponent(BFA_MechCoreComponent));
		m_pAim = BFA_MechAimComponent.Cast(owner.FindComponent(BFA_MechAimComponent));
		m_sLocalStation = string.Empty;
		m_bLocalStationActive = false;
		m_bFireHeld = false;
		m_bInspectHeld = false;
		m_iHeldPrimaryGroup = 0;

		if (!s_aAllStations.Contains(this))
			s_aAllStations.Insert(this);
		EnsureStaticSeatWatch();
	}

	//------------------------------------------------------------------------------------------------
	//! Stop every repeating/delayed call before this component goes away. Without this the
	//! call queue keeps invoking methods on a destroyed mech (death, despawn, GC cleanup).
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			// Releases the trigger, removes input listeners and aim/fire ticks.
			StopLocalStation();

			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(NetAimTick);
				queue.Remove(FireWatchTick);
			}
		}
		s_aAllStations.RemoveItem(this);

		super.OnDelete(owner);
	}
}
