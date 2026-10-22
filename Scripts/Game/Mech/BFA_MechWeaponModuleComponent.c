[ComponentEditorProps(category: "GameScripted/Mech", description: "Base API for swappable mech weapon modules. Weapon-specific scripts inherit from this component and implement their own firing behavior.")]
class BFA_MechWeaponModuleComponentClass : ScriptComponentClass
{
}

class BFA_MechWeaponModuleComponent : ScriptComponent
{
	[Attribute("WEAPON_MODULE", UIWidgets.EditBox, "Stable module identifier used by diagnostics and future loadout/HUD systems.", category: "Weapon Module")]
	protected string m_sModuleId;

	[Attribute("Weapon Module", UIWidgets.EditBox, "Human-readable weapon/module name.", category: "Weapon Module")]
	protected string m_sDisplayName;

	[Attribute("GUNNER", UIWidgets.EditBox, "Crew station allowed to operate this weapon module.", category: "Routing")]
	protected string m_sControlStation;

	[Attribute("1", UIWidgets.Auto, "Weapon group this module listens to.", category: "Routing")]
	protected int m_iWeaponGroup;

	[Attribute("true", UIWidgets.CheckBox, "Module supports held/automatic fire.", category: "Routing")]
	protected bool m_bSupportsHeldFire;

	[Attribute("false", UIWidgets.CheckBox, "Module supports one-shot pulse fire.", category: "Routing")]
	protected bool m_bSupportsPulseFire;

	// ---- Effects (muzzle flash, sound, visible projectile, impact) ------------------------------
	[Attribute("", UIWidgets.Object, "What this weapon looks and sounds like. Played on every machine from one replicated shot event.", category: "Effects")]
	protected ref BFA_MechWeaponFX m_FX;

	// ---- Heat ----------------------------------------------------------------------------------
	[Attribute("1", UIWidgets.Auto, "Heat added to the mech per shot / rocket (needs CGS_MechHeatComponent on the mech). MG ~0.5, cannon ~3, missile ~8.", category: "Heat")]
	protected float m_fHeatPerShot;

	// ---- Pilot HUD ---------------------------------------------------------------------------
	[Attribute("0", UIWidgets.Auto, "Shot speed (m/s) for the pilot HUD's lead pip on a locked target. 0 = automatic: rockets and travelling shells use their own speed, instant-hit weapons show no pip.", category: "HUD")]
	protected float m_fHudLeadSpeed;

	// ---- Damage kind (read by add-ons such as shields) ------------------------------------------
	[Attribute("0", UIWidgets.ComboBox, "What kind of damage this weapon deals. Add-ons (e.g. energy shields) treat kinds differently.", enums: ParamEnumArray.FromEnum(BFA_EMechDamageKind), category: "Damage")]
	protected BFA_EMechDamageKind m_eDamageKind;

	// Weapon-specific effectiveness against different classes, independent of engine EDamageType.
	[Attribute("1", UIWidgets.Auto, "Damage multiplier against infantry (AI or player).", category: "Damage")]
	protected float m_fDamageVsInfantry;

	[Attribute("1", UIWidgets.Auto, "Damage multiplier against vehicles and mechs.", category: "Damage")]
	protected float m_fDamageVsVehicles;

	[Attribute("0.3", UIWidgets.Auto, "Minimum share of a DIRECT infantry hit that reaches health despite armour. 0 = normal armour handling. This does not change shield/vehicle damage.", category: "Damage")]
	protected float m_fInfantryArmourFloor;


	// ---- Crosshair -----------------------------------------------------------------------------
	[Attribute("", UIWidgets.Object, "This weapon's own crosshair. Empty = the mech's default crosshair.", category: "Crosshair")]
	protected ref BFA_MechCrosshair m_Crosshair;

	// ---- Muzzle --------------------------------------------------------------------------------
	[Attribute(desc: "Muzzle placed on the WEAPON PART with the gizmo (can follow a bone via PivotID). Shots start here and fly along its blue (Z) axis. When set, it replaces the automatic muzzle and offset below.", category: "Muzzle")]
	protected ref PointInfo m_MuzzlePoint;

	[Attribute("true", UIWidgets.CheckBox, "Only without Muzzle Point / Barrel Points: place the muzzle at the front face of the weapon model's bounds automatically.", category: "Muzzle")]
	protected bool m_bAutoMuzzleFromBounds;

	[Attribute("0 0 0", UIWidgets.Auto, "Only without Muzzle Point / Barrel Points: muzzle correction in weapon space (X side, Y up, Z forward).", category: "Muzzle")]
	protected vector m_vMuzzleLocalOffset;

	[Attribute("0.05", UIWidgets.Auto, "Extra distance in front of the muzzle where shots start.", category: "Muzzle")]
	protected float m_fMuzzleClearance;

	[Attribute("false", UIWidgets.CheckBox, "Only without Muzzle Point / Barrel Points (a point fires along its own blue axis): the model's forward is -Z (fires the other way).", category: "Muzzle")]
	protected bool m_bReverseMuzzleForward;

	[Attribute("false", UIWidgets.CheckBox, "Shots leave the muzzle toward the point the crew is aiming at, instead of along the muzzle's own axis. Turn ON for weapons on IK arms (the hand bone moves with the arm but does not turn).", category: "Muzzle")]
	protected bool m_bFireAtAimPoint;

	[Attribute("100", UIWidgets.Auto, "Fire At Aim Point: largest correction in degrees from the muzzle's own direction. Beyond it the shot goes along the muzzle (no firing out of the back of the arm).", category: "Muzzle")]
	protected float m_fMaxAimCorrection;

	// ---- Simple fire (used only by weapons that override FireShot) -----------------------------
	[Attribute("120", UIWidgets.Auto, "Shots per minute for the built-in trigger handling (weapons that only override FireShot).", category: "Fire")]
	protected float m_fShotsPerMinute;

	// ---- Barrels -------------------------------------------------------------------------------
	[Attribute(desc: "BARREL POINTS (recommended for multi-barrel / multi-tube weapons): add one PointInfo per tube and place each on the weapon part with the gizmo (can follow a bone via PivotID). Shots cycle through them and fly along each point's blue (Z) axis. When set, Muzzle Point and Barrel Offsets are ignored.", category: "Muzzle")]
	protected ref array<ref PointInfo> m_aBarrelPoints;

	[Attribute("", UIWidgets.Auto, "LEGACY (only without Barrel Points): extra muzzle positions as numbers in weapon space (X side, Y up, Z forward), added to the muzzle. Shots cycle through them.", category: "Muzzle")]
	protected ref array<vector> m_aBarrelOffsets;

	// ---- Ammo ----------------------------------------------------------------------------------
	[Attribute("0", UIWidgets.Auto, "Rounds per magazine. 0 = unlimited (no reloading).", category: "Ammo")]
	protected int m_iMagazineSize;

	[Attribute("3", UIWidgets.Auto, "Reload time in seconds.", category: "Ammo")]
	protected float m_fReloadTime;

	[Attribute("-1", UIWidgets.Auto, "Spare magazines carried. -1 = unlimited, 0 = only the loaded one. Refilled by ServerRearm().", category: "Ammo")]
	protected int m_iSpareMagazines;

	[Attribute("true", UIWidgets.CheckBox, "Reload automatically when the magazine runs dry.", category: "Ammo")]
	protected bool m_bAutoReload;

	// Ammo runtime. Authoritative on the server; mirrored on clients from shot/reload events
	// (good enough for a HUD, never used for game logic on clients).
	protected int m_iRoundsInMagazine;
	protected int m_iSparesLeft;
	protected bool m_bReloading;
	protected int m_iReloadEndMs;
	protected int m_iBarrelIndex;
	protected bool m_bEmptyNotified;
	protected int m_iNextShotReadyMs;
	protected int m_iShotsFired;       // server: shots that passed ConsumeRound (chain fire uses it)
	protected IEntity m_pAutoFireInstigator;
	protected bool m_bAutoFireRunning;
	protected ref array<IEntity> m_aSplashCollect = {};

	// Runtime binding supplied by BFA_MechHardpointComponent.
	protected IEntity m_pMechOwner;
	protected int m_iHardpointIndex = -1;
	protected string m_sHardpointName;
	protected int m_iHardpointClass;
	protected bool m_bBoundToHardpoint;
	protected bool m_bTriggerHeld;

	// Routing supplied by the hardpoint. The same weapon prefab can be a main gun on one mech
	// and a secondary on another, so the HARDPOINT decides group/station when it sets them.
	// Empty / 0 = use this module's own attributes.
	protected string m_sRouteStationOverride;
	protected int m_iRouteGroupOverride;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_pMechOwner = null;
		m_iHardpointIndex = -1;
		m_sHardpointName = string.Empty;
		m_iHardpointClass = BFA_EMechHardpointClass.ANY;
		m_bBoundToHardpoint = false;
		m_bTriggerHeld = false;

		m_iRoundsInMagazine = Math.Max(m_iMagazineSize, 0);
		m_iSparesLeft = m_iSpareMagazines;
		m_bReloading = false;
		m_iReloadEndMs = 0;
		m_iBarrelIndex = 0;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(FinishReload);
				queue.Remove(AutoFireTick);
			}
		}

		super.OnDelete(owner);
	}

	//==================================================================================================
	// AMMO (server authoritative)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected int GetModuleTimeMs()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	bool UsesAmmo()
	{
		return m_iMagazineSize > 0;
	}

	int GetRoundsInMagazine()
	{
		return m_iRoundsInMagazine;
	}

	int GetMagazineSize()
	{
		return m_iMagazineSize;
	}

	//! -1 = unlimited.
	int GetSpareMagazines()
	{
		return m_iSparesLeft;
	}

	bool IsReloading()
	{
		return m_bReloading;
	}

	//! 0..1 while reloading, 1 otherwise.
	float GetReloadProgress()
	{
		if (!m_bReloading)
			return 1.0;

		float total = Math.Max(m_fReloadTime, 0.1) * 1000.0;
		float left = m_iReloadEndMs - GetModuleTimeMs();
		return Math.Clamp(1.0 - left / total, 0.0, 1.0);
	}

	//! True when a shot may be fired right now.
	bool HasAmmo()
	{
		if (m_iMagazineSize <= 0)
			return true;

		return !m_bReloading && m_iRoundsInMagazine > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Call once per shot from your fire code (server). Returns false = do not fire.
	protected bool ConsumeRound()
	{
		// Heat, destroyed weapon part ... (BFA_MechExtension hooks).
		if (m_pMechOwner && !BFA_MechExtension.Get().CanModuleFire(m_pMechOwner, this))
			return false;

		if (m_iMagazineSize <= 0)
		{
			m_iShotsFired++;
			if (m_pMechOwner)
				BFA_MechExtension.Get().OnModuleFired(m_pMechOwner, this);
			return true;
		}

		if (m_bReloading)
			return false;

		if (m_iRoundsInMagazine <= 0)
		{
			if (!m_bEmptyNotified)
			{
				m_bEmptyNotified = true;
				BroadcastModuleEvent(BFA_EMechModuleEvent.EMPTY);
			}
			if (m_bAutoReload)
				StartReload();
			return false;
		}

		m_iRoundsInMagazine--;
		m_iShotsFired++;
		if (m_iRoundsInMagazine <= 0 && m_bAutoReload)
			StartReload();

		if (m_pMechOwner)
			BFA_MechExtension.Get().OnModuleFired(m_pMechOwner, this);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: how many shots this weapon has fired (counts up forever). Chain fire watches it to
	//! know when a weapon has taken its turn.
	int GetShotsFired()
	{
		return m_iShotsFired;
	}

	//------------------------------------------------------------------------------------------------
	float GetHeatPerShot()
	{
		return m_fHeatPerShot;
	}

	//------------------------------------------------------------------------------------------------
	//! Manual reload (e.g. bound to a key later). Server.
	void ServerReload()
	{
		if (!Replication.IsServer() || m_iMagazineSize <= 0 || m_bReloading)
			return;

		if (m_iRoundsInMagazine >= m_iMagazineSize)
			return;

		StartReload();
	}

	//------------------------------------------------------------------------------------------------
	//! Refill magazine and spares (rearm zone, supply truck, GM action). Server.
	void ServerRearm()
	{
		if (!Replication.IsServer())
			return;

		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(FinishReload);

		m_bReloading = false;
		m_bEmptyNotified = false;
		m_iRoundsInMagazine = Math.Max(m_iMagazineSize, 0);
		m_iSparesLeft = m_iSpareMagazines;
		BroadcastModuleEvent(BFA_EMechModuleEvent.REARMED);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartReload()
	{
		if (m_bReloading || m_iMagazineSize <= 0)
			return;

		if (m_iSparesLeft == 0)
			return; // out of magazines until rearmed

		m_bReloading = true;
		int reloadMs = Math.Max(m_fReloadTime, 0.1) * 1000.0;
		m_iReloadEndMs = GetModuleTimeMs() + reloadMs;

		if (Replication.IsServer() && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(FinishReload, reloadMs, false);

		OnReloadStarted();
		BroadcastModuleEvent(BFA_EMechModuleEvent.RELOAD_START);
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishReload()
	{
		if (!m_bReloading)
			return;

		m_bReloading = false;
		m_bEmptyNotified = false;
		m_iRoundsInMagazine = Math.Max(m_iMagazineSize, 0);
		if (m_iSparesLeft > 0)
			m_iSparesLeft--;

		OnReloadFinished();
		BroadcastModuleEvent(BFA_EMechModuleEvent.RELOAD_END);
	}

	//------------------------------------------------------------------------------------------------
	// Optional hooks for subclasses.
	protected void OnReloadStarted()
	{
	}

	protected void OnReloadFinished()
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void BroadcastModuleEvent(int eventType, int value = 0)
	{
		if (!Replication.IsServer() || !m_pMechOwner)
			return;

		BFA_MechWeaponComponent router = BFA_MechWeaponComponent.Cast(m_pMechOwner.FindComponent(BFA_MechWeaponComponent));
		if (router)
			router.ServerBroadcastModuleEvent(this, eventType, value);
	}

	//==================================================================================================
	// BARRELS / MUZZLE
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Offset of the barrel the next shot uses (added to the module's own muzzle position), and
	//! advances to the next barrel. Zero when no extra barrels are configured.
	protected PointInfo PickBarrelPoint(bool advance)
	{
		if (!m_aBarrelPoints || m_aBarrelPoints.IsEmpty())
			return null;

		if (m_iBarrelIndex < 0 || m_iBarrelIndex >= m_aBarrelPoints.Count())
			m_iBarrelIndex = 0;

		PointInfo point = m_aBarrelPoints[m_iBarrelIndex];
		if (advance)
			m_iBarrelIndex = (m_iBarrelIndex + 1) % m_aBarrelPoints.Count();
		return point;
	}

	//------------------------------------------------------------------------------------------------
	protected vector TakeNextBarrelOffset()
	{
		if (!m_aBarrelOffsets || m_aBarrelOffsets.IsEmpty())
			return vector.Zero;

		if (m_iBarrelIndex < 0 || m_iBarrelIndex >= m_aBarrelOffsets.Count())
			m_iBarrelIndex = 0;

		vector offset = m_aBarrelOffsets[m_iBarrelIndex];
		m_iBarrelIndex = (m_iBarrelIndex + 1) % m_aBarrelOffsets.Count();
		return offset;
	}

	//------------------------------------------------------------------------------------------------
	int GetBarrelCount()
	{
		if (m_aBarrelPoints && !m_aBarrelPoints.IsEmpty())
			return m_aBarrelPoints.Count();

		if (!m_aBarrelOffsets || m_aBarrelOffsets.IsEmpty())
			return 1;

		return m_aBarrelOffsets.Count();
	}

	//==================================================================================================
	// EFFECTS (local, every machine except a dedicated server)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	BFA_MechWeaponFX GetFX()
	{
		return m_FX;
	}

	//------------------------------------------------------------------------------------------------
	//! Impact particle used when this module's shots end. Override to supply a legacy attribute.
	ResourceName GetImpactParticle()
	{
		if (m_FX)
			return m_FX.m_sImpactParticle;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the router on every machine that shows effects, once per shot.
	void ClientOnShot(vector start, vector end)
	{
		// Mirror the ammo count on clients (the server already consumed it).
		if (!Replication.IsServer() && m_iMagazineSize > 0 && m_iRoundsInMagazine > 0)
			m_iRoundsInMagazine--;

		if (!m_FX)
			return;

		vector dir = end - start;
		if (dir.LengthSq() < 0.000001)
			dir = vector.Forward;
		dir.Normalize();

		vector mat[4];
		Math3D.DirectionAndUpMatrix(dir, vector.Up, mat);
		mat[3] = start;

		if (!m_FX.m_sMuzzleParticle.IsEmpty())
		{
			ParticleEffectEntitySpawnParams muzzleParams = new ParticleEffectEntitySpawnParams();
			muzzleParams.Transform[0] = mat[0];
			muzzleParams.Transform[1] = mat[1];
			muzzleParams.Transform[2] = mat[2];
			muzzleParams.Transform[3] = mat[3];
			ParticleEffectEntity.SpawnParticleEffect(m_FX.m_sMuzzleParticle, muzzleParams);
		}

		if (!m_FX.m_sBackblastParticle.IsEmpty())
		{
			IEntity weapon = GetOwner();
			vector back = start;
			vector backDir = dir * -1.0;
			if (weapon)
			{
				vector weaponMat[4];
				weapon.GetWorldTransform(weaponMat);
				if (m_FX.m_BackblastPoint)
				{
					m_FX.m_BackblastPoint.Init(weapon);
					vector bbModel[4];
					m_FX.m_BackblastPoint.GetModelTransform(bbModel);
					vector bbPos = bbModel[3];
					vector bbAxis = bbModel[2];
					back = weaponMat[3] + weaponMat[0] * bbPos[0] + weaponMat[1] * bbPos[1] + weaponMat[2] * bbPos[2];
					vector bbDir = weaponMat[0] * bbAxis[0] + weaponMat[1] * bbAxis[1] + weaponMat[2] * bbAxis[2];
					if (bbDir.LengthSq() > 0.000001)
						backDir = bbDir.Normalized();
				}
				else
				{
					back = start + weaponMat[0] * m_FX.m_vBackblastOffset[0] + weaponMat[1] * m_FX.m_vBackblastOffset[1] + weaponMat[2] * m_FX.m_vBackblastOffset[2];
				}
			}

			vector backMat[4];
			Math3D.DirectionAndUpMatrix(backDir, vector.Up, backMat);
			ParticleEffectEntitySpawnParams backParams = new ParticleEffectEntitySpawnParams();
			backParams.Transform[0] = backMat[0];
			backParams.Transform[1] = backMat[1];
			backParams.Transform[2] = backMat[2];
			backParams.Transform[3] = back;
			ParticleEffectEntity.SpawnParticleEffect(m_FX.m_sBackblastParticle, backParams);
		}

		PlayModuleSound(m_FX.m_sFireSoundEvent);
	}

	//------------------------------------------------------------------------------------------------
	//! Reload / empty events on every machine that shows effects. Also keeps the client ammo mirror.
	void ClientOnModuleEvent(int eventType, int value = 0)
	{
		BFA_MechEvents.RaiseWeaponEvent(m_pMechOwner, this, eventType, value);

		if (!Replication.IsServer())
		{
			if (eventType == BFA_EMechModuleEvent.RELOAD_START)
			{
				m_bReloading = true;
				m_iReloadEndMs = GetModuleTimeMs() + (int)(Math.Max(m_fReloadTime, 0.1) * 1000.0);
			}
			else if (eventType == BFA_EMechModuleEvent.RELOAD_END)
			{
				m_bReloading = false;
				m_iRoundsInMagazine = Math.Max(m_iMagazineSize, 0);
				if (m_iSparesLeft > 0)
					m_iSparesLeft--;
			}
			else if (eventType == BFA_EMechModuleEvent.REARMED)
			{
				m_bReloading = false;
				m_iRoundsInMagazine = Math.Max(m_iMagazineSize, 0);
				m_iSparesLeft = m_iSpareMagazines;
			}
		}

		if (!m_FX)
			return;

		if (eventType == BFA_EMechModuleEvent.RELOAD_START)
			PlayModuleSound(m_FX.m_sReloadSoundEvent);
		else if (eventType == BFA_EMechModuleEvent.EMPTY)
			PlayModuleSound(m_FX.m_sEmptySoundEvent);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayModuleSound(string eventName)
	{
		if (eventName.IsEmpty() || !GetOwner())
			return;

		// Sound file picked in Effects > Sound: play it straight from the .acp at the weapon.
		if (m_FX && !m_FX.m_sSoundProject.IsEmpty())
		{
			vector mat[4];
			GetOwner().GetWorldTransform(mat);
			BFA_MechPlaySound(m_FX.m_sSoundProject, eventName, mat);
			return;
		}

		// Fallback: an SCR_SoundDataComponent on the weapon part (old setup).
		if (GetOwner().FindComponent(SCR_SoundDataComponent))
			SCR_SoundManagerModule.CreateAndPlayAudioSource(GetOwner(), eventName);
	}

	//------------------------------------------------------------------------------------------------
	//! Plays one event from an .acp at a world transform (no components needed).
	static void BFA_MechPlaySound(ResourceName project, string eventName, vector mat[4])
	{
		if (project.IsEmpty() || eventName.IsEmpty())
			return;

		array<string> signalNames = {};
		array<float> signalValues = {};
		AudioSystem.PlayEvent(project, eventName, mat, signalNames, signalValues);
	}

	//------------------------------------------------------------------------------------------------
	//! Helper for subclasses: send one shot to every machine (muzzle flash, sound, projectile,
	//! impact). Returns the shot id (pass it to the impact broadcast for server-simulated rounds).
	protected int BroadcastShot(vector start, vector end, bool hit, float speed, float length = 1, float thickness = 0.1, bool serverImpact = false)
	{
		if (!m_pMechOwner)
			return 0;

		BFA_MechWeaponComponent router = BFA_MechWeaponComponent.Cast(m_pMechOwner.FindComponent(BFA_MechWeaponComponent));
		if (!router)
			return 0;

		return router.ServerBroadcastModuleShot(this, start, end, hit, speed, length, thickness, serverImpact);
	}

	//------------------------------------------------------------------------------------------------
	void BindToHardpoint(IEntity mechOwner, int hardpointIndex, string hardpointName, int hardpointClass, string routeStation = "", int routeGroup = 0)
	{
		m_pMechOwner = mechOwner;
		m_iHardpointIndex = hardpointIndex;
		m_sHardpointName = hardpointName;
		m_iHardpointClass = hardpointClass;
		m_sRouteStationOverride = routeStation;
		m_iRouteGroupOverride = routeGroup;
		m_bBoundToHardpoint = mechOwner != null && hardpointIndex >= 0;

		if (m_bBoundToHardpoint)
		{
			BFA_MechLog.Log(string.Format(
				"[BFA WEAPON MODULE] bound module=%1 name=%2 hardpoint=%3 index=%4 group=%5 station=%6",
				m_sModuleId,
				m_sDisplayName,
				m_sHardpointName,
				m_iHardpointIndex,
				GetWeaponGroup(),
				GetControlStation()
			));
		}
	}

	//------------------------------------------------------------------------------------------------
	void UnbindFromHardpoint()
	{
		if (m_bTriggerHeld && Replication.IsServer())
			ServerTriggerUp(null);

		m_pMechOwner = null;
		m_iHardpointIndex = -1;
		m_sHardpointName = string.Empty;
		m_iHardpointClass = BFA_EMechHardpointClass.ANY;
		m_sRouteStationOverride = string.Empty;
		m_iRouteGroupOverride = 0;
		m_bBoundToHardpoint = false;
		m_bTriggerHeld = false;
	}

	//------------------------------------------------------------------------------------------------
	bool IsBoundToHardpoint()
	{
		return m_bBoundToHardpoint;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetMechOwner()
	{
		return m_pMechOwner;
	}

	//------------------------------------------------------------------------------------------------
	int GetHardpointIndex()
	{
		return m_iHardpointIndex;
	}

	//------------------------------------------------------------------------------------------------
	string GetHardpointName()
	{
		return m_sHardpointName;
	}

	//------------------------------------------------------------------------------------------------
	int GetHardpointClass()
	{
		return m_iHardpointClass;
	}

	//------------------------------------------------------------------------------------------------
	string GetModuleId()
	{
		return m_sModuleId;
	}

	//------------------------------------------------------------------------------------------------
	string GetDisplayName()
	{
		return m_sDisplayName;
	}

	//------------------------------------------------------------------------------------------------
	string GetControlStation()
	{
		if (!m_sRouteStationOverride.IsEmpty())
			return m_sRouteStationOverride;

		return m_sControlStation;
	}

	//------------------------------------------------------------------------------------------------
	int GetWeaponGroup()
	{
		if (m_iRouteGroupOverride > 0)
			return m_iRouteGroupOverride;

		return m_iWeaponGroup;
	}

	//------------------------------------------------------------------------------------------------
	bool SupportsHeldFire()
	{
		return m_bSupportsHeldFire;
	}

	//------------------------------------------------------------------------------------------------
	bool SupportsPulseFire()
	{
		return m_bSupportsPulseFire;
	}

	//------------------------------------------------------------------------------------------------
	bool IsTriggerHeld()
	{
		return m_bTriggerHeld;
	}

	//------------------------------------------------------------------------------------------------
	bool MatchesFireRoute(string stationId, int weaponGroup)
	{
		if (!m_bBoundToHardpoint)
			return false;

		if (weaponGroup != GetWeaponGroup())
			return false;

		string station = GetControlStation();
		if (!station.IsEmpty() && station != stationId)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the future mech weapon router on the server when the trigger is first pressed.
	void ServerTriggerDown(IEntity instigator)
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint)
			return;

		if (m_bTriggerHeld)
			return;

		m_bTriggerHeld = true;
		OnTriggerDown(instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! Called repeatedly by the future router while the trigger remains held.
	void ServerTriggerHeld(IEntity instigator)
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint || !m_bTriggerHeld)
			return;

		if (!m_bSupportsHeldFire)
			return;

		OnTriggerHeld(instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the future mech weapon router when the trigger is released.
	void ServerTriggerUp(IEntity instigator)
	{
		if (!Replication.IsServer())
			return;

		if (!m_bTriggerHeld)
			return;

		m_bTriggerHeld = false;
		OnTriggerUp(instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! Used by shoulders, missiles and other one-shot/auxiliary modules.
	void ServerFirePulse(IEntity instigator)
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint || !m_bSupportsPulseFire)
			return;

		OnFirePulse(instigator);
	}

	//==================================================================================================
	// TRIGGER HOOKS
	//
	// EASY WAY (new weapons): override only FireShot() - see Scripts/Game/Mech Weapons/README.txt.
	// The defaults below handle fire rate (m_fShotsPerMinute), held auto-fire, pulse fire and ammo.
	//
	// FULL CONTROL: override these four hooks yourself (Las Cannon, Rocket Launcher do).
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected void OnTriggerDown(IEntity instigator)
	{
		m_pAutoFireInstigator = instigator;
		TryFireShot(instigator);

		if (m_bSupportsHeldFire)
			StartAutoFire();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTriggerHeld(IEntity instigator)
	{
		m_pAutoFireInstigator = instigator;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTriggerUp(IEntity instigator)
	{
		StopAutoFire();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFirePulse(IEntity instigator)
	{
		TryFireShot(instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE METHOD a simple weapon overrides. Server only. Called once per shot after the fire
	//! rate and ammo checks passed. Use the helpers below: GetMuzzle(), TraceShot(), DealDamage(),
	//! BroadcastShot().
	protected void FireShot(IEntity instigator)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Fire-rate + ammo gate around FireShot().
	protected bool TryFireShot(IEntity instigator)
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint)
			return false;

		int nowMs = GetModuleTimeMs();
		if (nowMs < m_iNextShotReadyMs)
			return false;

		if (!ConsumeRound())
			return false;

		m_iNextShotReadyMs = nowMs + GetShotIntervalMs();
		FireShot(instigator);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetShotIntervalMs()
	{
		return Math.Max((int)(60000.0 / Math.Max(m_fShotsPerMinute, 1.0)), 20);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartAutoFire()
	{
		if (m_bAutoFireRunning || !GetGame() || !GetGame().GetCallqueue())
			return;

		m_bAutoFireRunning = true;
		GetGame().GetCallqueue().CallLater(AutoFireTick, 20, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopAutoFire()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(AutoFireTick);

		m_bAutoFireRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void AutoFireTick()
	{
		if (!Replication.IsServer() || !m_bTriggerHeld || !m_bBoundToHardpoint)
		{
			StopAutoFire();
			return;
		}

		TryFireShot(m_pAutoFireInstigator);
	}

	//==================================================================================================
	// WEAPON HELPERS (server) - everything a weapon needs, so custom weapons stay short
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Fire At Aim Point: turn 'direction' toward where the crew aims (see m_bFireAtAimPoint).
	protected void ApplyFireAtAimPoint(vector start, inout vector direction)
	{
		if (!m_bFireAtAimPoint || !m_pMechOwner)
			return;

		BFA_MechWeaponMountComponent mounts = BFA_MechWeaponMountComponent.Cast(m_pMechOwner.FindComponent(BFA_MechWeaponMountComponent));
		vector aimPoint;
		if (!mounts || !mounts.GetAimPointForEntity(GetOwner(), aimPoint))
			return;

		vector toAim = aimPoint - start;
		if (toAim.LengthSq() < 0.01)
			return;
		toAim.Normalize();

		float angle = Math.Acos(Math.Clamp(vector.Dot(toAim, direction), -1, 1)) * Math.RAD2DEG;
		if (angle <= Math.Max(m_fMaxAimCorrection, 0))
			direction = toAim;
	}

	//------------------------------------------------------------------------------------------------
	//! World muzzle position and firing direction of this weapon (cycles barrels if configured).
	protected vector GetMuzzle(out vector direction, bool advanceBarrel = true)
	{
		vector muzzleStart = GetMuzzleRaw(direction, advanceBarrel);
		ApplyFireAtAimPoint(muzzleStart, direction);
		return muzzleStart;
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetMuzzleRaw(out vector direction, bool advanceBarrel = true)
	{
		IEntity weaponEntity = GetOwner();
		direction = vector.Forward;
		if (!weaponEntity)
			return vector.Zero;

		vector weaponWorld[4];
		weaponEntity.GetWorldTransform(weaponWorld);

		PointInfo barrelPoint = PickBarrelPoint(advanceBarrel);
		PointInfo usePoint = barrelPoint;
		if (!usePoint)
			usePoint = m_MuzzlePoint;

		if (usePoint)
		{
			usePoint.Init(weaponEntity);
			vector muzzleModel[4];
			usePoint.GetModelTransform(muzzleModel);

			vector muzzleLocal = muzzleModel[3];
			if (advanceBarrel && !barrelPoint)
				muzzleLocal = muzzleLocal + TakeNextBarrelOffset();

			vector muzzleAxis = muzzleModel[2];
			direction = weaponWorld[0] * muzzleAxis[0] + weaponWorld[1] * muzzleAxis[1] + weaponWorld[2] * muzzleAxis[2];
			if (direction.LengthSq() < 0.000001)
				direction = weaponWorld[2];
			direction.Normalize();

			vector pointStart = weaponWorld[3] + weaponWorld[0] * muzzleLocal[0] + weaponWorld[1] * muzzleLocal[1] + weaponWorld[2] * muzzleLocal[2];
			return pointStart + direction * Math.Max(m_fMuzzleClearance, 0.0);
		}

		direction = weaponWorld[2];
		if (direction.LengthSq() < 0.000001 && m_pMechOwner)
			direction = m_pMechOwner.GetTransformAxis(2);
		if (direction.LengthSq() < 0.000001)
			direction = vector.Forward;
		direction.Normalize();

		if (m_bReverseMuzzleForward)
			direction = direction * -1.0;

		vector localMuzzle = vector.Zero;
		if (m_bAutoMuzzleFromBounds)
		{
			vector mins, maxs;
			weaponEntity.GetBounds(mins, maxs);
			localMuzzle = (mins + maxs) * 0.5;

			if (m_bReverseMuzzleForward)
				localMuzzle[2] = mins[2];
			else
				localMuzzle[2] = maxs[2];
		}

		localMuzzle = localMuzzle + m_vMuzzleLocalOffset;
		if (advanceBarrel)
			localMuzzle = localMuzzle + TakeNextBarrelOffset();

		vector start = weaponWorld[3]
			+ weaponWorld[0] * localMuzzle[0]
			+ weaponWorld[1] * localMuzzle[1]
			+ weaponWorld[2] * localMuzzle[2];

		return start + direction * Math.Max(m_fMuzzleClearance, 0.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Crosshair for this weapon (null = mech default).
	BFA_MechCrosshair GetCrosshair()
	{
		return m_Crosshair;
	}

	//------------------------------------------------------------------------------------------------
	//! Shot speed for the pilot HUD's lead pip (0 = instant hit, no pip). Override per weapon.
	float GetLeadSpeed()
	{
		return m_fHudLeadSpeed;
	}

	//------------------------------------------------------------------------------------------------
	//! How far this weapon reaches; used for the gun markers on the crosshair. Override per weapon.
	float GetAimRange()
	{
		return 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Where this gun is really pointing right now (any machine). Used by the crosshair HUD.
	vector GetPointedPoint()
	{
		vector direction;
		vector start = GetMuzzle(direction, false);
		IEntity hit;
		return TraceShot(start, direction, GetAimRange(), hit);
	}

	//------------------------------------------------------------------------------------------------
	//! Wrap every HandleDamage call of a weapon in these so add-ons know the damage kind.
	protected void BeginDamage()
	{
		BFA_MechExtension.s_iCurrentDamageKind = m_eDamageKind;
		BFA_MechExtension.s_pCurrentShooterMech = m_pMechOwner;
	}

	protected void EndDamage()
	{
		BFA_MechExtension.s_iCurrentDamageKind = -1;
		BFA_MechExtension.s_pCurrentShooterMech = null;
	}

	//------------------------------------------------------------------------------------------------
	BFA_EMechDamageKind GetDamageKind()
	{
		return m_eDamageKind;
	}

	//------------------------------------------------------------------------------------------------
	//! Random cone spread in degrees.
	protected vector ApplyConeSpread(vector direction, float spreadDeg)
	{
		if (spreadDeg <= 0.0)
			return direction;

		vector angles = direction.VectorToAngles();
		angles[0] = angles[0] + Math.RandomFloat(-spreadDeg, spreadDeg);
		angles[1] = angles[1] + Math.RandomFloat(-spreadDeg, spreadDeg);

		vector result = angles.AnglesToVector();
		result.Normalize();
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Instant (hitscan) trace that ignores the firing mech and this weapon.
	//! Returns the end point; hitEntity is null on a miss.
	protected vector TraceShot(vector start, vector direction, float range, out IEntity hitEntity)
	{
		hitEntity = null;
		range = Math.Max(range, 1.0);
		vector finalEnd = start + direction * range;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return finalEnd;

		vector from = start;
		for (int i = 0; i < 8; i++)
		{
			TraceParam param = new TraceParam();
			param.Start = from;
			param.End = finalEnd;
			param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			param.LayerMask = EPhysicsLayerPresets.Projectile;

			array<IEntity> excludes = {};
			if (m_pMechOwner)
				excludes.Insert(m_pMechOwner);
			if (GetOwner())
				excludes.Insert(GetOwner());
			param.ExcludeArray = excludes;

			float fraction = world.TraceMove(param, null);
			if (fraction >= 1.0)
				return finalEnd;

			vector hit = param.Start + (param.End - param.Start) * fraction;
			IEntity traced = param.TraceEnt;

			// Skip our own mech's parts and keep going.
			if (traced && m_pMechOwner && traced.GetRootParent() == m_pMechOwner)
			{
				from = hit + direction * 0.25;
				continue;
			}

			hitEntity = traced;
			return ApplyRayInterception(start, direction, hit, hitEntity);
		}

		IEntity none;
		hitEntity = none;
		return ApplyRayInterception(start, direction, finalEnd, hitEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Lets add-ons stop a beam early (e.g. at an enemy shield bubble).
	protected vector ApplyRayInterception(vector start, vector direction, vector end, inout IEntity hitEntity)
	{
		float length = vector.Distance(start, end);
		IEntity blocker;
		float blockDistance = BFA_MechExtension.Get().InterceptRay(m_pMechOwner, start, direction, length, blocker);
		if (blockDistance < 0 || blockDistance >= length || !blocker)
			return end;

		hitEntity = blocker;
		return start + direction * blockDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! The damage manager a hit should go to. Soldiers' gear (vests, helmets) always passes the hit
	//! to the soldier; vehicle parts go to the part or its vehicle.
	protected SCR_DamageManagerComponent ResolveDamageManager(IEntity hitEntity)
	{
		if (!hitEntity)
			return null;

		IEntity hitRoot = hitEntity.GetRootParent();
		if (hitRoot && ChimeraCharacter.Cast(hitRoot))
		{
			SCR_DamageManagerComponent characterDamage = SCR_DamageManagerComponent.Cast(hitRoot.FindComponent(SCR_DamageManagerComponent));
			if (characterDamage)
				return characterDamage;
		}

		IEntity current = hitEntity;
		for (int i = 0; current && i < 8; i++)
		{
			SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(current.FindComponent(SCR_DamageManagerComponent));
			if (damageManager)
				return damageManager;

			current = current.GetParent();
		}

		if (!hitRoot)
			return null;

		return SCR_DamageManagerComponent.Cast(hitRoot.FindComponent(SCR_DamageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Deal damage to whatever was hit. Returns the health actually removed.
	//! type: EDamageType.KINETIC (bullets), EXPLOSIVE, INCENDIARY, TRUE (ignores armour) ...
	// Original six-argument DealDamage signature is kept for CGS_CrewDamage overrides.
	// A temporary override preserves the original shooter for missiles already in flight.
	protected IEntity m_pDamageShooterOverride;

	//------------------------------------------------------------------------------------------------
	protected float DealDamageWithShooter(IEntity hitEntity, float amount, EDamageType type, vector hitPos, vector direction, IEntity instigator, IEntity shooter)
	{
		IEntity previousShooter = m_pDamageShooterOverride;
		m_pDamageShooterOverride = shooter;
		float lost = DealDamage(hitEntity, amount, type, hitPos, direction, instigator);
		m_pDamageShooterOverride = previousShooter;
		return lost;
	}

	//------------------------------------------------------------------------------------------------
	protected float AdjustDamageForTarget(IEntity target, float amount)
	{
		if (!target)
			return amount;
		IEntity root = target.GetRootParent();
		if (!root)
			root = target;
		if (ChimeraCharacter.Cast(root))
			return amount * Math.Max(m_fDamageVsInfantry, 0.0);
		if (Vehicle.Cast(root))
			return amount * Math.Max(m_fDamageVsVehicles, 0.0);
		return amount;
	}

	//------------------------------------------------------------------------------------------------
	protected float DealDamage(IEntity hitEntity, float amount, EDamageType type, vector hitPos, vector direction, IEntity instigator)
	{
		if (!Replication.IsServer() || !hitEntity || amount <= 0)
			return 0;

		SCR_DamageManagerComponent damageManager = ResolveDamageManager(hitEntity);
		if (!damageManager)
			return 0;

		HitZone hitZone = damageManager.GetDefaultHitZone();
		if (!hitZone)
			return 0;

		amount = AdjustDamageForTarget(hitEntity, amount);
		if (amount <= 0)
			return 0;

		float healthBefore = hitZone.GetHealth();
		vector hitTransform[3];
		hitTransform[0] = hitPos;
		hitTransform[1] = direction;
		hitTransform[2] = direction * -1.0;

		IEntity shooter = m_pDamageShooterOverride;
		if (!shooter)
			shooter = m_pMechOwner;
		instigator = BFA_MechCoreComponent.BFA_ResolveKiller(shooter, instigator);
		Instigator damageInstigator = Instigator.CreateInstigator(instigator);
		SCR_DamageContext context = new SCR_DamageContext(type, amount, hitTransform, shooter, hitZone, damageInstigator, null, -1, -1);
		BeginDamage();
		BFA_MechExtension.s_pCurrentShooterMech = shooter;
		// A kinetic shell's blast is EXPLOSIVE, even if its direct hit is KINETIC.
		// ENERGY splash (such as a PPC) remains ENERGY for shield interactions.
		if (type == EDamageType.EXPLOSIVE && m_eDamageKind == BFA_EMechDamageKind.KINETIC)
			BFA_MechExtension.s_iCurrentDamageKind = BFA_EMechDamageKind.EXPLOSIVE;
		damageManager.HandleDamage(context);
		EndDamage();

		float lost = Math.Max(healthBefore - hitZone.GetHealth(), 0.0);
		IEntity root = hitEntity.GetRootParent();
		if (!root)
			root = hitEntity;
		if (type != EDamageType.TRUE && ChimeraCharacter.Cast(root) && m_fInfantryArmourFloor > 0 && healthBefore > 0)
		{
			float minimumLost = Math.Min(healthBefore, amount * Math.Clamp(m_fInfantryArmourFloor, 0.0, 1.0));
			float shortfall = minimumLost - lost;
			if (shortfall > 0.001)
			{
				SCR_DamageContext floorContext = new SCR_DamageContext(EDamageType.TRUE, shortfall, hitTransform, shooter, hitZone, damageInstigator, null, -1, -1);
				BeginDamage();
		BFA_MechExtension.s_pCurrentShooterMech = shooter;
				damageManager.HandleDamage(floorContext);
				EndDamage();
			}
		}
		return Math.Max(healthBefore - hitZone.GetHealth(), 0.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Splash damage around a point, once per object, with line of sight from the blast.
	//! Returns how many objects were hurt. Linear falloff to zero at radius.
	protected int DealSplashDamage(vector center, float amount, float radius, EDamageType type, IEntity instigator, IEntity skipRoot = null)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!Replication.IsServer() || !world || amount <= 0 || radius <= 0)
			return 0;

		m_aSplashCollect.Clear();
		world.QueryEntitiesBySphere(center, radius, CollectSplashCandidate, null, EQueryEntitiesFlags.ALL);

		array<IEntity> roots = {};
		array<IEntity> parts = {};
		array<float> distances = {};
		foreach (IEntity entity : m_aSplashCollect)
		{
			if (!entity)
				continue;

			IEntity root = entity.GetRootParent();
			if (!root)
				root = entity;

			if (root == m_pMechOwner || (skipRoot && root == skipRoot))
				continue;

			float d = vector.Distance(center, entity.GetOrigin());
			int index = roots.Find(root);
			if (index < 0)
			{
				roots.Insert(root);
				parts.Insert(entity);
				distances.Insert(d);
			}
			else if (d < distances[index])
			{
				parts[index] = entity;
				distances[index] = d;
			}
		}
		m_aSplashCollect.Clear();

		int hurt = 0;
		for (int r = 0; r < roots.Count(); r++)
		{
			float falloff = Math.Clamp(1.0 - distances[r] / radius, 0.0, 1.0);
			if (falloff <= 0)
				continue;

			// Blast must reach the object (no wall / terrain in between).
			vector toPart = parts[r].GetOrigin() - (center + vector.Up * 0.25);
			float toPartLength = toPart.Length();
			IEntity blocker;
			if (toPartLength > 0.05)
				TraceShot(center + vector.Up * 0.25, toPart * (1.0 / toPartLength), toPartLength, blocker);
			if (blocker && blocker.GetRootParent() != roots[r] && blocker != roots[r])
				continue;

			vector dir = parts[r].GetOrigin() - center;
			if (dir.LengthSq() > 0.0001)
				dir.Normalize();
			else
				dir = vector.Up;

			if (DealDamage(roots[r], amount * falloff, type, center, dir, instigator) > 0)
				hurt++;
		}

		return hurt;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectSplashCandidate(IEntity entity)
	{
		if (entity)
			m_aSplashCollect.Insert(entity);

		return true;
	}
}
