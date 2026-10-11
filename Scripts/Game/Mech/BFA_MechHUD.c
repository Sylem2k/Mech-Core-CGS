//------------------------------------------------------------------------------------------------
//! MechCore events for HUD mods (Talos etc.), scoring, sounds, anything external.
//! Subscribe from your own addon - no need to edit MechCore:
//!
//!   BFA_MechEvents.GetOnShotFired().Insert(MyOnShot);
//!   void MyOnShot(IEntity mech, BFA_MechWeaponModuleComponent weapon) { ... }
//!
//! Events fire LOCALLY on every machine that sees them (clients and listen host; shots and
//! reload events never fire on a dedicated server because nothing is shown there).
class BFA_MechEvents
{
	// (IEntity mech, string stationId) - the LOCAL player started / stopped operating a station.
	protected static ref ScriptInvoker s_OnLocalStationChanged;
	// (IEntity mech) - some station's selected weapon group changed. Re-read with BFA_MechHUD.
	protected static ref ScriptInvoker s_OnWeaponGroupChanged;
	// (IEntity mech, BFA_MechWeaponModuleComponent weapon) - a weapon fired (shown locally).
	protected static ref ScriptInvoker s_OnShotFired;
	// (IEntity mech, BFA_MechWeaponModuleComponent weapon, int BFA_EMechModuleEvent, int value)
	// reload start/end, empty, rearmed, anti-missile charges changed (value = charges).
	protected static ref ScriptInvoker s_OnWeaponEvent;
	// (IEntity mech, int hardpointIndex, BFA_MechWeaponModuleComponent weapon) - a weapon was
	// installed on a hardpoint (spawn, loadout swap). weapon == null = hardpoint emptied.
	protected static ref ScriptInvoker s_OnWeaponInstalled;
	// (IEntity mech, IEntity occupant, bool entered) - someone got in / out of any seat.
	protected static ref ScriptInvoker s_OnCrewChanged;

	static ScriptInvoker GetOnLocalStationChanged() { if (!s_OnLocalStationChanged) s_OnLocalStationChanged = new ScriptInvoker(); return s_OnLocalStationChanged; }
	static ScriptInvoker GetOnWeaponGroupChanged() { if (!s_OnWeaponGroupChanged) s_OnWeaponGroupChanged = new ScriptInvoker(); return s_OnWeaponGroupChanged; }
	static ScriptInvoker GetOnShotFired() { if (!s_OnShotFired) s_OnShotFired = new ScriptInvoker(); return s_OnShotFired; }
	static ScriptInvoker GetOnWeaponEvent() { if (!s_OnWeaponEvent) s_OnWeaponEvent = new ScriptInvoker(); return s_OnWeaponEvent; }
	static ScriptInvoker GetOnWeaponInstalled() { if (!s_OnWeaponInstalled) s_OnWeaponInstalled = new ScriptInvoker(); return s_OnWeaponInstalled; }
	static ScriptInvoker GetOnCrewChanged() { if (!s_OnCrewChanged) s_OnCrewChanged = new ScriptInvoker(); return s_OnCrewChanged; }

	// Internal raisers (MechCore calls these). Cheap when nobody subscribed.
	static void RaiseLocalStationChanged(IEntity mech, string stationId) { if (s_OnLocalStationChanged) s_OnLocalStationChanged.Invoke(mech, stationId); }
	static void RaiseWeaponGroupChanged(IEntity mech) { if (s_OnWeaponGroupChanged) s_OnWeaponGroupChanged.Invoke(mech); }
	static void RaiseShotFired(IEntity mech, BFA_MechWeaponModuleComponent weapon) { if (s_OnShotFired) s_OnShotFired.Invoke(mech, weapon); }
	static void RaiseWeaponEvent(IEntity mech, BFA_MechWeaponModuleComponent weapon, int eventType, int value) { if (s_OnWeaponEvent) s_OnWeaponEvent.Invoke(mech, weapon, eventType, value); }
	static void RaiseWeaponInstalled(IEntity mech, int hardpointIndex, BFA_MechWeaponModuleComponent weapon) { if (s_OnWeaponInstalled) s_OnWeaponInstalled.Invoke(mech, hardpointIndex, weapon); }
	static void RaiseCrewChanged(IEntity mech, IEntity occupant, bool entered) { if (s_OnCrewChanged) s_OnCrewChanged.Invoke(mech, occupant, entered); }
}

//------------------------------------------------------------------------------------------------
//! Read-only data for HUDs, for the LOCAL player's mech. All of it works on clients.
//!
//!   IEntity mech = BFA_MechHUD.GetLocalMech();           // null = not in a mech station
//!   string station = BFA_MechHUD.GetLocalStation();       // "DRIVER", "GUNNER", ...
//!   array<BFA_MechWeaponModuleComponent> guns = {};
//!   BFA_MechHUD.GetSelectedWeapons(guns);                 // weapons the trigger fires now
//!   guns[0].GetDisplayName(), GetRoundsInMagazine(), GetMagazineSize(), GetSpareMagazines(),
//!   IsReloading(), GetReloadProgress(), UsesAmmo()
//!
//! Turn MechCore's own crosshair off when your HUD draws one:
//!   BFA_MechHUD.SetBuiltInCrosshairEnabled(false);
class BFA_MechHUD
{
	protected static IEntity s_LocalMech;
	protected static string s_LocalStation;
	protected static bool s_bBuiltInCrosshairDisabled;
	protected static bool s_bBuiltInHUDDisabled;

	//------------------------------------------------------------------------------------------------
	//! Turn MechCore's built-in cockpit HUD (weapons + mech panels) off when your HUD replaces it.
	static void SetBuiltInHUDEnabled(bool enabled)
	{
		s_bBuiltInHUDDisabled = !enabled;
	}

	static bool IsBuiltInHUDEnabled()
	{
		return !s_bBuiltInHUDDisabled;
	}

	//------------------------------------------------------------------------------------------------
	// Set by BFA_MechAimComponent when the local player starts / stops operating a station.
	static void SetLocalStation(IEntity mech, string stationId)
	{
		if (s_LocalMech == mech && s_LocalStation == stationId)
			return;

		s_LocalMech = mech;
		s_LocalStation = stationId;
		BFA_MechEvents.RaiseLocalStationChanged(mech, stationId);
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetLocalMech()
	{
		return s_LocalMech;
	}

	static string GetLocalStation()
	{
		if (!s_LocalMech)
			return string.Empty;

		return s_LocalStation;
	}

	//------------------------------------------------------------------------------------------------
	//! True for any MechCore mech (lets other mods skip mechs in their vehicle loops).
	static bool IsMech(IEntity entity)
	{
		return entity && entity.FindComponent(BFA_MechCoreComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	static void SetBuiltInCrosshairEnabled(bool enabled)
	{
		s_bBuiltInCrosshairDisabled = !enabled;
	}

	static bool IsBuiltInCrosshairEnabled()
	{
		return !s_bBuiltInCrosshairDisabled;
	}

	//------------------------------------------------------------------------------------------------
	static int GetSelectedGroup()
	{
		if (!s_LocalMech)
			return 0;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(s_LocalMech.FindComponent(BFA_MechCoreComponent));
		if (!core)
			return 0;

		return core.GetSelectedWeaponGroup(s_LocalStation);
	}

	//------------------------------------------------------------------------------------------------
	//! Weapons a station fires with a given group (group 0 = its currently selected group).
	static void GetStationWeapons(IEntity mech, string stationId, int group, notnull array<BFA_MechWeaponModuleComponent> outWeapons)
	{
		outWeapons.Clear();
		if (!mech)
			return;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (!core || !hardpoints)
			return;

		if (group <= 0)
			group = core.GetSelectedWeaponGroup(stationId);

		for (int hp = 0; hp < hardpoints.GetHardpointCount(); hp++)
		{
			BFA_MechWeaponModuleComponent module = hardpoints.GetInstalledModuleAt(hp);
			if (module && module.MatchesFireRoute(stationId, group))
				outWeapons.Insert(module);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void GetSelectedWeapons(notnull array<BFA_MechWeaponModuleComponent> outWeapons)
	{
		GetStationWeapons(s_LocalMech, s_LocalStation, 0, outWeapons);
	}

	//------------------------------------------------------------------------------------------------
	//! Every weapon group the local station can switch to (for a weapon list on the HUD).
	static void GetLocalUsableGroups(notnull array<int> outGroups)
	{
		outGroups.Clear();
		if (!s_LocalMech)
			return;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(s_LocalMech.FindComponent(BFA_MechCoreComponent));
		if (core)
			core.GetUsableWeaponGroups(s_LocalStation, outGroups);
	}

	//------------------------------------------------------------------------------------------------
	//! True while any hardpoint of the mech is mid loadout-swap.
	static bool IsSwapping(IEntity mech)
	{
		if (!mech)
			return false;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints)
			return false;

		for (int hp = 0; hp < hardpoints.GetHardpointCount(); hp++)
		{
			if (hardpoints.IsHardpointSwapping(hp))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Hull health 0..1 (default hit zone of the mech's damage manager). -1 = no damage manager.
	static float GetHealth01(IEntity mech)
	{
		if (!mech)
			return -1;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(mech.FindComponent(SCR_DamageManagerComponent));
		if (!damage)
			return -1;

		HitZone zone = damage.GetDefaultHitZone();
		if (!zone || zone.GetMaxHealth() <= 0)
			return -1;

		return Math.Clamp(zone.GetHealth() / zone.GetMaxHealth(), 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDestroyed(IEntity mech)
	{
		if (!mech)
			return true;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(mech.FindComponent(SCR_DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground speed in m/s (from movement between calls; works on every machine).
	static float GetSpeed(IEntity mech)
	{
		if (!mech)
			return 0;

		Physics physics = mech.GetPhysics();
		if (physics)
		{
			vector velocity = physics.GetVelocity();
			velocity[1] = 0;
			if (velocity.LengthSq() > 0.0001)
				return velocity.Length();
		}

		// Clients may not simulate the body: estimate from position.
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		int now = world.GetWorldTime();
		if (s_SpeedMech != mech)
		{
			s_SpeedMech = mech;
			s_vSpeedLastPos = mech.GetOrigin();
			s_iSpeedLastMs = now;
			s_fSpeed = 0;
			return 0;
		}

		int elapsed = now - s_iSpeedLastMs;
		if (elapsed >= 100)
		{
			vector moved = mech.GetOrigin() - s_vSpeedLastPos;
			moved[1] = 0;
			s_fSpeed = moved.Length() / (elapsed * 0.001);
			s_vSpeedLastPos = mech.GetOrigin();
			s_iSpeedLastMs = now;
		}

		return s_fSpeed;
	}

	protected static IEntity s_SpeedMech;
	protected static vector s_vSpeedLastPos;
	protected static int s_iSpeedLastMs;
	protected static float s_fSpeed;

	//------------------------------------------------------------------------------------------------
	//! Anti-missile charges on the mech (first point-defense module). -1 = none fitted.
	static int GetPointDefenseCharges(IEntity mech, out int maxCharges)
	{
		maxCharges = 0;
		if (!mech)
			return -1;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints)
			return -1;

		for (int hp = 0; hp < hardpoints.GetHardpointCount(); hp++)
		{
			BFA_MechPointDefenseModuleComponent pd = BFA_MechPointDefenseModuleComponent.Cast(hardpoints.GetInstalledModuleAt(hp));
			if (pd)
			{
				maxCharges = pd.GetMaxCharges();
				return pd.GetCharges();
			}
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Jump jet charges left (-1 = no jump jets).
	static int GetJumpJetCharges(IEntity mech)
	{
		if (!mech)
			return -1;

		BFA_SentinelJumpJetComponent jets = BFA_SentinelJumpJetComponent.Cast(mech.FindComponent(BFA_SentinelJumpJetComponent));
		if (!jets)
			return -1;

		return jets.GetRemainingCharges();
	}

	//------------------------------------------------------------------------------------------------
	//! Who sits in a station (null = empty).
	static IEntity GetStationOccupant(IEntity mech, string stationId)
	{
		if (!mech)
			return null;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (!core)
			return null;

		return core.GetStationOccupant(stationId);
	}
}
