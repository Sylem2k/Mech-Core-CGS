[BaseContainerProps()]
class BFA_MechCrewStationDefinition
{
	[Attribute("DRIVER", UIWidgets.EditBox, "Station ID, unique per mech. DRIVER drives; any other name is a crew seat: COMMANDER, GUNNER, GUNNER2, GUNNER3, LEFT_TURRET... Weapon modules list this name as their Control Station. Up to 8 stations per mech.", category: "Crew Station")]
	string m_sStationId;

	[Attribute("true", UIWidgets.CheckBox, "This station may drive the mech root.", category: "Crew Station")]
	bool m_bCanDrive;

	[Attribute("true", UIWidgets.CheckBox, "This station may publish an aim direction.", category: "Crew Station")]
	bool m_bCanAim;

	[Attribute("1", UIWidgets.EditBox, "Bit mask of weapon groups this station may fire. Bit 0 = group 1, bit 1 = group 2, etc.", category: "Crew Station")]
	int m_iWeaponGroupMask;

	[Attribute("-1", UIWidgets.Auto, "Index in BaseCompartmentManagerComponent.GetCompartments(). -1 keeps the existing DRIVER pilot-seat bridge or a runtime seat bridge.", category: "Crew Station")]
	int m_iCompartmentIndex;

	[Attribute("1", UIWidgets.Auto, "Main weapon group selected when this station first becomes active.", category: "Weapon Routing")]
	int m_iPrimaryWeaponGroup;

	[Attribute("0", UIWidgets.Auto, "Optional alternate main weapon group. 0 = no primary/secondary swap for this station.", category: "Weapon Routing")]
	int m_iSecondaryWeaponGroup;

	[Attribute("0", UIWidgets.Auto, "Optional auxiliary weapon group. Intended for shoulders, missiles, utility weapons, etc. 0 = none.", category: "Weapon Routing")]
	int m_iAuxWeaponGroup;

	[Attribute("", UIWidgets.Auto, "Optional: every group the cycle key steps through, in order (e.g. 1,2,3 for main/secondary/heavy). Empty = primary <-> secondary. Groups with nothing mounted are skipped automatically.", category: "Weapon Routing")]
	ref array<int> m_aCycleWeaponGroups;

	// Runtime bridge for seats/controllers that are not backed by a compartment index.
	IEntity m_pRuntimeOccupant;
}

[ComponentEditorProps(category: "BFA MechCore", description: "Central crew/station capability, seat validation and weapon-bank routing layer for BFA mechs.")]
class BFA_MechCoreComponentClass : ScriptGameComponentClass
{
}

class BFA_MechCoreComponent : ScriptGameComponent
{
	[Attribute("false", UIWidgets.CheckBox, "Print MechCore info/diagnostic messages to the log (off = clean log; warnings still print). Same as the -bfaMechDebug launch parameter.", category: "Diagnostics")]
	protected bool m_bDebugLogging;

	[Attribute("", UIWidgets.Object, "Crew stations available on this mech.", category: "Crew")]
	ref array<ref BFA_MechCrewStationDefinition> m_aCrewStations;

	//! Most crew stations a mech can have (driver + commander + 6 gunners, or any mix).
	static const int MAX_STATIONS = 8;

	// Selected weapon group per crew station, by the station's position in Crew Stations.
	[RplProp(onRplName: "OnRepWeaponSelection")]
	protected int m_iSelGroup0;
	[RplProp(onRplName: "OnRepWeaponSelection")]
	protected int m_iSelGroup1;
	[RplProp(onRplName: "OnRepWeaponSelection")]
	protected int m_iSelGroup2;
	[RplProp(onRplName: "OnRepWeaponSelection")]
	protected int m_iSelGroup3;
	[RplProp(onRplName: "OnRepWeaponSelection")]
	protected int m_iSelGroup4;
	[RplProp(onRplName: "OnRepWeaponSelection")]
	protected int m_iSelGroup5;
	[RplProp(onRplName: "OnRepWeaponSelection")]
	protected int m_iSelGroup6;
	[RplProp(onRplName: "OnRepWeaponSelection")]
	protected int m_iSelGroup7;

	//------------------------------------------------------------------------------------------------
	//! Position of a station in Crew Stations (0..MAX_STATIONS-1), or -1. Same on every machine.
	int GetStationIndex(string stationId)
	{
		if (!m_aCrewStations || stationId.IsEmpty())
			return -1;

		int count = Math.Min(m_aCrewStations.Count(), MAX_STATIONS);
		for (int i = 0; i < count; i++)
		{
			BFA_MechCrewStationDefinition station = m_aCrewStations[i];
			if (station && station.m_sStationId == stationId)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected BFA_MechCrewStationDefinition FindStation(string stationId)
	{
		if (!m_aCrewStations || stationId.IsEmpty())
			return null;

		foreach (BFA_MechCrewStationDefinition station : m_aCrewStations)
		{
			if (station && station.m_sStationId == stationId)
				return station;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	bool HasStation(string stationId)
	{
		return FindStation(stationId) != null;
	}

	//------------------------------------------------------------------------------------------------
	bool CanStationDrive(string stationId)
	{
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return false;

		return station.m_bCanDrive;
	}

	//------------------------------------------------------------------------------------------------
	bool CanStationAim(string stationId)
	{
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return false;

		return station.m_bCanAim;
	}

	//------------------------------------------------------------------------------------------------
	bool CanStationFireGroup(string stationId, int weaponGroup)
	{
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station || weaponGroup < 1 || weaponGroup > 30)
			return false;

		int groupBit = 1 << (weaponGroup - 1);
		if ((station.m_iWeaponGroupMask & groupBit) != 0)
			return true;

		// Player-defined routing grants only groups with a live module belonging
		// to this station. The authored mask remains the default for all others.
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(GetOwner().FindComponent(BFA_MechHardpointComponent));
		return hardpoints && hardpoints.HasCustomWeaponGroupForStation(stationId, weaponGroup);
	}

	//------------------------------------------------------------------------------------------------
	int GetStationPrimaryWeaponGroup(string stationId)
	{
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return 0;

		return station.m_iPrimaryWeaponGroup;
	}

	//------------------------------------------------------------------------------------------------
	int GetStationSecondaryWeaponGroup(string stationId)
	{
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return 0;

		return station.m_iSecondaryWeaponGroup;
	}

	//------------------------------------------------------------------------------------------------
	int GetStationAuxWeaponGroup(string stationId)
	{
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return 0;

		return station.m_iAuxWeaponGroup;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetRepSelectedGroup(string stationId)
	{
		switch (GetStationIndex(stationId))
		{
			case 0: return m_iSelGroup0;
			case 1: return m_iSelGroup1;
			case 2: return m_iSelGroup2;
			case 3: return m_iSelGroup3;
			case 4: return m_iSelGroup4;
			case 5: return m_iSelGroup5;
			case 6: return m_iSelGroup6;
			case 7: return m_iSelGroup7;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetRepSelectedGroup(string stationId, int weaponGroup)
	{
		if (GetRepSelectedGroup(stationId) != weaponGroup)
			GetGame().GetCallqueue().CallLater(RaiseGroupChanged, 0, false);

		switch (GetStationIndex(stationId))
		{
			case 0: m_iSelGroup0 = weaponGroup; break;
			case 1: m_iSelGroup1 = weaponGroup; break;
			case 2: m_iSelGroup2 = weaponGroup; break;
			case 3: m_iSelGroup3 = weaponGroup; break;
			case 4: m_iSelGroup4 = weaponGroup; break;
			case 5: m_iSelGroup5 = weaponGroup; break;
			case 6: m_iSelGroup6 = weaponGroup; break;
			case 7: m_iSelGroup7 = weaponGroup; break;
		}
	}

	//------------------------------------------------------------------------------------------------
	int GetSelectedWeaponGroup(string stationId)
	{
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return 0;

		int selected = GetRepSelectedGroup(stationId);
		if (selected > 0)
			return selected;

		return station.m_iPrimaryWeaponGroup;
	}

	//------------------------------------------------------------------------------------------------
	//! Ordered list of groups this station can select with the cycle key.
	protected void GetCycleGroups(BFA_MechCrewStationDefinition station, notnull array<int> outGroups)
	{
		outGroups.Clear();
		if (!station)
			return;

		if (station.m_aCycleWeaponGroups && !station.m_aCycleWeaponGroups.IsEmpty())
		{
			foreach (int group : station.m_aCycleWeaponGroups)
			{
				if (group > 0 && !outGroups.Contains(group))
					outGroups.Insert(group);
			}
			return;
		}

		if (station.m_iPrimaryWeaponGroup > 0)
			outGroups.Insert(station.m_iPrimaryWeaponGroup);

		if (station.m_iSecondaryWeaponGroup > 0 && !outGroups.Contains(station.m_iSecondaryWeaponGroup))
			outGroups.Insert(station.m_iSecondaryWeaponGroup);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsGroupUsable(string stationId, int weaponGroup)
	{
		if (!CanStationFireGroup(stationId, weaponGroup))
			return false;

		BFA_MechWeaponComponent router = BFA_MechWeaponComponent.Cast(GetOwner().FindComponent(BFA_MechWeaponComponent));
		if (!router)
			return true;

		return router.HasLiveWeaponsInGroup(stationId, weaponGroup);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-owned weapon group selection. Steps through the station's cycle groups, skipping
	//! groups with nothing mounted. Works the same for a 1-gun mech (nothing to cycle) or 18.
	bool ServerCycleWeaponGroup(string stationId)
	{
		if (!Replication.IsServer())
			return false;

		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return false;

		array<int> groups = {};
		GetCycleGroups(station, groups);
		for (int customGroup = 1; customGroup <= 9; customGroup++)
		{
			if (!groups.Contains(customGroup) && IsGroupUsable(stationId, customGroup))
				groups.Insert(customGroup);
		}
		if (groups.Count() < 2)
			return false;

		int selected = GetSelectedWeaponGroup(stationId);
		int index = groups.Find(selected);

		for (int step = 1; step < groups.Count(); step++)
		{
			int candidate = groups[Math.Mod(index + step + groups.Count(), groups.Count())];
			if (candidate == selected || !IsGroupUsable(stationId, candidate))
				continue;

			SetRepSelectedGroup(stationId, candidate);
			Replication.BumpMe();
			BFA_MechLog.Log(string.Format("[BFA CORE WEAPONS] station=%1 selectedGroup=%2", stationId, candidate));
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: select a specific weapon group for a station (used by AI crews). Respects the
	//! station's allowed groups and skips groups with nothing mounted.
	bool ServerSelectWeaponGroup(string stationId, int weaponGroup)
	{
		if (!Replication.IsServer() || !IsGroupUsable(stationId, weaponGroup))
			return false;

		if (GetSelectedWeaponGroup(stationId) == weaponGroup)
			return true;

		SetRepSelectedGroup(stationId, weaponGroup);
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Usable weapon groups for a station, in cycle order (primary first). For AI crews.
	void GetUsableWeaponGroups(string stationId, notnull array<int> outGroups)
	{
		outGroups.Clear();
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return;

		array<int> groups = {};
		GetCycleGroups(station, groups);
		if (station.m_iAuxWeaponGroup > 0 && !groups.Contains(station.m_iAuxWeaponGroup))
			groups.Insert(station.m_iAuxWeaponGroup);
		for (int customGroup = 1; customGroup <= 9; customGroup++)
		{
			if (!groups.Contains(customGroup) && IsGroupUsable(stationId, customGroup))
				groups.Insert(customGroup);
		}

		foreach (int group : groups)
		{
			if (IsGroupUsable(stationId, group))
				outGroups.Insert(group);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Station ids in definition order. For AI crews.
	void GetStationIds(notnull array<string> outIds)
	{
		outIds.Clear();
		if (!m_aCrewStations)
			return;

		foreach (BFA_MechCrewStationDefinition station : m_aCrewStations)
		{
			if (station)
				outIds.Insert(station.m_sStationId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! After a loadout change: any station whose selected group is now empty moves to the first
	//! group in its cycle list that has something mounted.
	void ServerRevalidateWeaponSelection()
	{
		if (!Replication.IsServer() || !m_aCrewStations)
			return;

		bool changed = false;
		foreach (BFA_MechCrewStationDefinition station : m_aCrewStations)
		{
			if (!station)
				continue;

			string stationId = station.m_sStationId;
			int selected = GetSelectedWeaponGroup(stationId);
			if (selected < 1 || IsGroupUsable(stationId, selected))
				continue;

			array<int> groups = {};
			GetCycleGroups(station, groups);
			// A player may have moved the only weapon into a new custom group.
			// Include those assignments when selecting a valid replacement group.
			for (int customGroup = 1; customGroup <= 6; customGroup++)
			{
				if (!groups.Contains(customGroup) && IsGroupUsable(stationId, customGroup))
					groups.Insert(customGroup);
			}

			foreach (int candidate : groups)
			{
				if (candidate == selected || !IsGroupUsable(stationId, candidate))
					continue;

				SetRepSelectedGroup(stationId, candidate);
				changed = true;
				BFA_MechLog.Log(string.Format("[BFA CORE WEAPONS] station=%1 group %2 is empty, switched to %3", stationId, selected, candidate));
				break;
			}
		}

		if (changed)
			Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! DRIVER is still bridged to the existing pilot controller. GUNNER / COMMANDER can use
	//! a compartment index on future mechs without needing a new validation path.
	IEntity GetStationOccupant(string stationId)
	{
		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return null;

		if (stationId == "DRIVER")
		{
			BFA_MechControllerComponent controller = BFA_MechControllerComponent.Cast(GetOwner().FindComponent(BFA_MechControllerComponent));
			if (controller)
			{
				IEntity pilot = controller.BFA_GetPilotOccupant();
				if (pilot)
					return pilot;
			}
		}

		if (station.m_iCompartmentIndex >= 0)
		{
			BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(GetOwner().FindComponent(BaseCompartmentManagerComponent));
			if (manager)
			{
				array<BaseCompartmentSlot> compartments = new array<BaseCompartmentSlot>();
				manager.GetCompartments(compartments);

				if (station.m_iCompartmentIndex < compartments.Count())
				{
					BaseCompartmentSlot slot = compartments[station.m_iCompartmentIndex];
					if (slot)
						return slot.GetOccupant();
				}
			}
		}

		return station.m_pRuntimeOccupant;
	}

	//------------------------------------------------------------------------------------------------
	//! Who gets the kill. Damage needs a CHARACTER as instigator (a vehicle entity becomes
	//! INSTIGATOR_OTHER, which the game rejects with an error and no kill credit).
	//! Keeps 'preferred' when it is a character; otherwise the first crew member found on any
	//! station (e.g. an AI pilot in DRIVER firing COMMANDER weapons). Falls back to 'preferred'.
	static IEntity BFA_ResolveKiller(IEntity mech, IEntity preferred)
	{
		if (ChimeraCharacter.Cast(preferred))
			return preferred;

		if (!mech)
			return preferred;

		IEntity root = mech.GetRootParent();
		if (!root)
			root = mech;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(root.FindComponent(BFA_MechCoreComponent));
		if (core && core.m_aCrewStations)
		{
			foreach (BFA_MechCrewStationDefinition station : core.m_aCrewStations)
			{
				if (!station)
					continue;

				IEntity occupant = core.GetStationOccupant(station.m_sStationId);
				if (ChimeraCharacter.Cast(occupant))
					return occupant;
			}
		}

		if (preferred)
			return preferred;

		return mech;
	}

	//------------------------------------------------------------------------------------------------
	void ServerSetStationOccupant(string stationId, IEntity occupant)
	{
		if (!Replication.IsServer())
			return;

		BFA_MechCrewStationDefinition station = FindStation(stationId);
		if (!station)
			return;

		station.m_pRuntimeOccupant = occupant;
	}

	//------------------------------------------------------------------------------------------------
	//! Validate PlayerController against the physical occupant of a named station.
	bool IsControllerAtStation(PlayerController controller, string stationId)
	{
		if (!controller)
			return false;

		IEntity occupant = GetStationOccupant(stationId);
		if (!occupant)
			return false;

		IEntity controlled = controller.GetControlledEntity();
		if (controlled == occupant)
			return true;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			int occupantPlayerId = playerManager.GetPlayerIdFromControlledEntity(occupant);
			if (occupantPlayerId > 0)
			{
				PlayerController occupantController = playerManager.GetPlayerController(occupantPlayerId);
				if (occupantController == controller)
					return true;
			}
		}

		// IMPORTANT FOR MULTI-CREW MECHS:
		// Do NOT accept "controlled root == mech" here. Both DRIVER and GUNNER are
		// parented to the same vehicle root, so that fallback cannot tell seats apart and
		// could let one crew member operate another station. Physical seat identity wins.
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Used by the generic non-driver crew input component to discover whether the local player is
	//! currently sitting in GUNNER or COMMANDER without hardcoding a specific mech prefab.
	string FindStationForController(PlayerController controller, bool skipDrivingStations)
	{
		if (!controller || !m_aCrewStations)
			return string.Empty;

		foreach (BFA_MechCrewStationDefinition station : m_aCrewStations)
		{
			if (!station)
				continue;

			if (skipDrivingStations && station.m_bCanDrive)
				continue;

			if (IsControllerAtStation(controller, station.m_sStationId))
				return station.m_sStationId;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected void InitializeSelectedGroups()
	{
		if (!m_aCrewStations)
			return;

		if (m_aCrewStations.Count() > MAX_STATIONS)
			BFA_MechLog.Log(string.Format("[BFA CORE] %1 has %2 crew stations; only the first %3 work. Merge some or remove extras.", GetOwner(), m_aCrewStations.Count(), MAX_STATIONS), LogLevel.WARNING);

		array<string> seen = {};
		foreach (BFA_MechCrewStationDefinition station : m_aCrewStations)
		{
			if (!station)
				continue;

			if (seen.Contains(station.m_sStationId))
				BFA_MechLog.Log(string.Format("[BFA CORE] %1: two crew stations are both called '%2'. Give every station its own ID (GUNNER, GUNNER2, GUNNER3...).", GetOwner(), station.m_sStationId), LogLevel.WARNING);
			seen.Insert(station.m_sStationId);

			if (station.m_sStationId != "DRIVER" && station.m_iCompartmentIndex < 0)
				BFA_MechLog.Log(string.Format("[BFA CORE] %1: station '%2' has no Compartment Index, so nobody can sit in it. Set it to the seat's index in the compartment manager.", GetOwner(), station.m_sStationId), LogLevel.WARNING);

			if (GetRepSelectedGroup(station.m_sStationId) < 1)
				SetRepSelectedGroup(station.m_sStationId, station.m_iPrimaryWeaponGroup);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RaiseGroupChanged()
	{
		BFA_MechEvents.RaiseWeaponGroupChanged(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRepWeaponSelection()
	{
		RaiseGroupChanged();
		// HUD/reticle presentation can read GetSelectedWeaponGroup(). No transform work here.
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_bDebugLogging)
			BFA_MechLog.SetEnabled(true);

		BFA_MechExtension.Get().OnMechInit(owner);
		InitializeSelectedGroups();

		if (Replication.IsServer())
			Replication.BumpMe();

		int stationCount = 0;
		if (m_aCrewStations)
			stationCount = m_aCrewStations.Count();

		BFA_MechLog.Log(string.Format("[BFA CORE] V50 two-crew fire diagnostic initialized stations=%1", stationCount));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		BFA_MechExtension.Get().OnMechDeleted(owner);
		super.OnDelete(owner);
	}
}
