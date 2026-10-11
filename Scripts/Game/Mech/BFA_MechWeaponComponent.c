[BaseContainerProps()]
class BFA_MechWeaponDefinition
{
	[Attribute("WEAPON", UIWidgets.EditBox, "Stable weapon ID for diagnostics and future HUD/loadout use.", category: "Weapon")]
	string m_sWeaponId;

	[Attribute("", UIWidgets.EditBox, "Optional MechCore mount ID. When set, the weapon resolves through BFA_MechWeaponMountComponent first.", category: "Weapon")]
	string m_sMountId;

	[Attribute("", UIWidgets.EditBox, "Fallback root SlotManager slot containing the weapon entity.", category: "Weapon")]
	string m_sWeaponSlotName;

	[Attribute("10", UIWidgets.Auto, "Heat added to the mech per shot (needs CGS_MechHeatComponent on the mech).", category: "Heat")]
	float m_fHeatPerShot;

	[Attribute("DRIVER", UIWidgets.EditBox, "Preferred crew station. Leave blank to allow any station that the MechCore group mask authorizes.", category: "Weapon")]
	string m_sControlStation;

	[Attribute("1", UIWidgets.Auto, "Weapon group. Multiple weapons in the same group fire together (for example LEFT_ARM + RIGHT_ARM).", category: "Weapon")]
	int m_iWeaponGroup;

	[Attribute("300", UIWidgets.Auto, "Fire rate in rounds per minute.", category: "Weapon")]
	float m_fRoundsPerMinute;

	[Attribute("1500", UIWidgets.Auto, "Maximum trace distance in metres.", category: "Weapon")]
	float m_fRange;

	[Attribute("true", UIWidgets.CheckBox, "Apply real server-authoritative damage to the entity hit by this shot.", category: "Damage")]
	bool m_bApplyDamage;

	[Attribute("75", UIWidgets.Auto, "Base damage dealt by each successful hit.", category: "Damage")]
	float m_fDamage;

	[Attribute("true", UIWidgets.CheckBox, "If normal kinetic damage is completely absorbed, apply an armor-piercing TRUE-damage fallback to the same hit zone.", category: "Damage")]
	bool m_bArmorPiercingFallback;

	[Attribute("1.0", UIWidgets.Auto, "Multiplier applied to base damage for the armor-piercing fallback.", category: "Damage")]
	float m_fArmorPiercingMultiplier;

	[Attribute("true", UIWidgets.CheckBox, "Automatically place the muzzle at the forward face of the weapon entity bounds.", category: "Weapon")]
	bool m_bAutoMuzzleFromBounds;

	[Attribute("0 0 0", UIWidgets.Auto, "Optional local-space muzzle correction. X=side, Y=up, Z=forward.", category: "Weapon")]
	vector m_vMuzzleLocalOffset;

	[Attribute("0.05", UIWidgets.Auto, "Small clearance beyond the muzzle face.", category: "Weapon")]
	float m_fMuzzleClearance;

	[Attribute("false", UIWidgets.CheckBox, "Use the negative local-Z end of the weapon as the muzzle.", category: "Weapon")]
	bool m_bReverseMuzzleForward;

	[Attribute("300", UIWidgets.Auto, "Visual tracer travel speed in metres/second.", category: "Tracer")]
	float m_fTracerSpeed;

	[Attribute("25", UIWidgets.Auto, "Visible tracer streak length in metres.", category: "Tracer")]
	float m_fTracerLength;

	[Attribute("0.12", UIWidgets.Auto, "Debug tracer thickness.", category: "Tracer")]
	float m_fTracerThickness;

	// Runtime only.
	IEntity m_pWeaponEntity;
	bool m_bServerFireHeld;
	bool m_bLoggedFirstShot;
	bool m_bLoggedFirstDamage;
	bool m_bLoggedMissingWeapon;
	int m_iNextShotTimeMs;
	int m_iShotCount;
}

[ComponentEditorProps(category: "BFA MechCore", description: "Multi-weapon mech fire manager. Weapons are routed by station + group; all weapons in one group can fire together.")]
class BFA_MechWeaponComponentClass : ScriptGameComponentClass
{
}

class BFA_MechTracerVisual
{
	vector m_vStart;
	vector m_vEnd;
	float m_fAge;
	float m_fSpeed;
	float m_fLength;
	float m_fThickness;
}

class BFA_MechWeaponComponent : ScriptGameComponent
{
	[Attribute("", UIWidgets.Object, "LEGACY - leave empty for new mechs. Old built-in weapon list from before hardpoints; an entry is switched off automatically when a hardpoint uses its mount. New mechs: Hardpoints + weapon parts (see MECH_WEAPONS_SETUP).", category: "Weapons")]
	ref array<ref BFA_MechWeaponDefinition> m_aWeapons;

	protected SlotManagerComponent m_pSlotManager;
	protected bool m_bServerFireTickRunning;
	protected ref array<ref BFA_MechTracerVisual> m_aTracerVisuals = {};
	protected ref array<ref BFA_MechProjectileVisual> m_aProjectiles = {};
	protected bool m_bProjectileTickRunning;
	protected int m_iLastProjectileTickMs;
	protected int m_iNextShotId;
	protected bool m_bTracerTickRunning;
	protected int m_iLastTracerTickMs;

	//------------------------------------------------------------------------------------------------
	protected bool StationMatches(BFA_MechWeaponDefinition weapon, string stationId)
	{
		if (!weapon)
			return false;

		if (weapon.m_sControlStation.IsEmpty())
			return true;

		return weapon.m_sControlStation == stationId;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ResolveWeapon(BFA_MechWeaponDefinition weapon)
	{
		if (!weapon)
			return false;

		if (weapon.m_pWeaponEntity)
			return true;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (!weapon.m_sMountId.IsEmpty())
		{
			BFA_MechWeaponMountComponent mounts = BFA_MechWeaponMountComponent.Cast(owner.FindComponent(BFA_MechWeaponMountComponent));
			if (mounts)
				weapon.m_pWeaponEntity = mounts.GetMountEntity(weapon.m_sMountId);

			if (weapon.m_pWeaponEntity)
			{
				BFA_MechLog.Log(string.Format("[BFA FIRE] weapon=%1 mount=%2 group=%3", weapon.m_sWeaponId, weapon.m_sMountId, weapon.m_iWeaponGroup));
				return true;
			}
		}

		if (!m_pSlotManager)
			m_pSlotManager = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));

		if (!m_pSlotManager || weapon.m_sWeaponSlotName.IsEmpty())
			return false;

		EntitySlotInfo slot = m_pSlotManager.GetSlotByName(weapon.m_sWeaponSlotName);
		if (!slot)
			return false;

		weapon.m_pWeaponEntity = slot.GetAttachedEntity();
		if (!weapon.m_pWeaponEntity)
			return false;

		BFA_MechLog.Log(string.Format("[BFA FIRE] weapon=%1 slot=%2 group=%3", weapon.m_sWeaponId, weapon.m_sWeaponSlotName, weapon.m_iWeaponGroup));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A legacy definition must never fire from an arm whose existing weapon has been
	//! replaced (and hidden) by a different hardpoint module.
	protected bool IsLegacyWeaponSuppressed(BFA_MechWeaponDefinition weapon)
	{
		if (!weapon || weapon.m_sMountId.IsEmpty())
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(
			owner.FindComponent(BFA_MechHardpointComponent)
		);

		return hardpoints && hardpoints.IsAimMountReplaced(weapon.m_sMountId);
	}

	//------------------------------------------------------------------------------------------------
	protected int GetWorldTimeMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	protected int GetFireIntervalMs(BFA_MechWeaponDefinition weapon)
	{
		if (!weapon)
			return 1000;

		float rpm = Math.Max(weapon.m_fRoundsPerMinute, 1.0);
		int intervalMs = 60000.0 / rpm;
		return Math.Max(intervalMs, 20);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasHeldWeapons()
	{
		if (!m_aWeapons)
			return false;

		foreach (BFA_MechWeaponDefinition weapon : m_aWeapons)
		{
			if (weapon && weapon.m_bServerFireHeld)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopServerFireTick()
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(ServerFireTick);

		m_bServerFireTickRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void StartServerFireTick()
	{
		if (m_bServerFireTickRunning)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_bServerFireTickRunning = true;
		queue.CallLater(ServerFireTick, 20, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerFireTick()
	{
		if (!Replication.IsServer())
		{
			StopServerFireTick();
			return;
		}

		if (!HasHeldWeapons())
		{
			StopServerFireTick();
			return;
		}

		int nowMs = GetWorldTimeMs();
		foreach (BFA_MechWeaponDefinition weapon : m_aWeapons)
		{
			if (!weapon || !weapon.m_bServerFireHeld)
				continue;

			if (IsLegacyWeaponSuppressed(weapon))
			{
				weapon.m_bServerFireHeld = false;
				continue;
			}

			if (nowMs < weapon.m_iNextShotTimeMs)
				continue;

			FireOneShot(weapon);
			weapon.m_iNextShotTimeMs = nowMs + GetFireIntervalMs(weapon);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity ResolveStationInstigator(string stationId)
	{
		IEntity mech = GetOwner();
		if (!mech)
			return null;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(
			mech.FindComponent(BFA_MechCoreComponent)
		);

		if (core)
		{
			IEntity occupant = core.GetStationOccupant(stationId);
			if (occupant)
				return occupant;
		}

		return mech;
	}

	//------------------------------------------------------------------------------------------------
	//! Routes held fire to installed modular weapons.
	//! Returns the number of installed modules that accepted this station/group.
	protected int RouteHeldFireToModules(string stationId, int weaponGroup, bool held)
	{
		IEntity mech = GetOwner();
		if (!mech)
			return 0;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(
			mech.FindComponent(BFA_MechHardpointComponent)
		);

		if (!hardpoints)
			return 0;

		IEntity instigator = ResolveStationInstigator(stationId);
		int matchedCount = 0;
		int count = hardpoints.GetHardpointCount();

		for (int hp = 0; hp < count; hp++)
		{
			BFA_MechWeaponModuleComponent module = hardpoints.GetWeaponModuleAt(hp);
			if (!module || !module.MatchesFireRoute(stationId, weaponGroup))
				continue;

			matchedCount++;

			if (held)
				module.ServerTriggerDown(instigator);
			else
				module.ServerTriggerUp(instigator);
		}

		return matchedCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Routes one-shot/pulse fire to installed modular weapons.
	protected int RoutePulseFireToModules(string stationId, int weaponGroup)
	{
		IEntity mech = GetOwner();
		if (!mech)
			return 0;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(
			mech.FindComponent(BFA_MechHardpointComponent)
		);

		if (!hardpoints)
			return 0;

		IEntity instigator = ResolveStationInstigator(stationId);
		int matchedCount = 0;
		int count = hardpoints.GetHardpointCount();

		for (int hp = 0; hp < count; hp++)
		{
			BFA_MechWeaponModuleComponent module = hardpoints.GetWeaponModuleAt(hp);
			if (!module || !module.MatchesFireRoute(stationId, weaponGroup))
				continue;

			matchedCount++;
			module.ServerFirePulse(instigator);
		}

		return matchedCount;
	}

	//------------------------------------------------------------------------------------------------
	//! True if this station/group has at least one weapon that would fire right now
	//! (a bound module, or a legacy definition that is not suppressed). Used to skip empty
	//! groups when cycling and after loadout swaps - a mech may carry 1 gun or 18.
	bool HasLiveWeaponsInGroup(string stationId, int weaponGroup)
	{
		if (weaponGroup < 1)
			return false;

		IEntity mech = GetOwner();
		if (!mech)
			return false;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (hardpoints)
		{
			int count = hardpoints.GetHardpointCount();
			for (int hp = 0; hp < count; hp++)
			{
				BFA_MechWeaponModuleComponent module = hardpoints.GetWeaponModuleAt(hp);
				if (module && module.MatchesFireRoute(stationId, weaponGroup))
					return true;
			}
		}

		if (m_aWeapons)
		{
			foreach (BFA_MechWeaponDefinition weapon : m_aWeapons)
			{
				if (!weapon || weapon.m_iWeaponGroup != weaponGroup || !StationMatches(weapon, stationId))
					continue;

				if (!IsLegacyWeaponSuppressed(weapon))
					return true;
			}
		}

		return false;
	}

	//==================================================================================================
	// SHOT EFFECTS (replicated events -> local visuals on every machine)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Legacy entry point (kept for older custom modules). Shows the debug tracer only.
	void ServerBroadcastModuleTracer(vector start, vector end, float speed, float length, float thickness)
	{
		ServerBroadcastModuleShot(null, start, end, false, speed, length, thickness, false);
	}

	//------------------------------------------------------------------------------------------------
	//! One shot from a module, to every machine: muzzle flash, fire sound, visible projectile and
	//! (for hitscan) the impact when the projectile arrives. Everything is looked up from the
	//! module's Effects on each machine, so the network only carries the hardpoint and two points.
	//! serverImpact = true for server-simulated rounds (rockets): the impact comes later from
	//! ServerBroadcastModuleImpact() with the returned shot id.
	int ServerBroadcastModuleShot(BFA_MechWeaponModuleComponent module, vector start, vector end, bool hit, float speed, float length, float thickness, bool serverImpact = false)
	{
		if (!Replication.IsServer())
			return 0;

		m_iNextShotId++;
		if (m_iNextShotId > 1000000)
			m_iNextShotId = 1;
		int shotId = m_iNextShotId;

		int hardpointIndex = -1;
		if (module)
			hardpointIndex = module.GetHardpointIndex();

		int flags = 0;
		if (hit)
			flags = flags | BFA_EMechShotFlags.HIT;
		if (serverImpact)
			flags = flags | BFA_EMechShotFlags.SERVER_IMPACT;

		if (!System.IsConsoleApp())
			PlayShotLocal(hardpointIndex, shotId, start, end, flags, speed, length, thickness);

		// Rockets are few and long-lived: reliable. Rapid fire: unreliable (a lost flash is harmless).
		if (serverImpact)
			Rpc(RpcDo_ModuleShotReliable, hardpointIndex, shotId, start, end, flags, speed, length, thickness);
		else
			Rpc(RpcDo_ModuleShot, hardpointIndex, shotId, start, end, flags, speed, length, thickness);

		return shotId;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_ModuleShot(int hardpointIndex, int shotId, vector start, vector end, int flags, float speed, float length, float thickness)
	{
		if (!System.IsConsoleApp())
			PlayShotLocal(hardpointIndex, shotId, start, end, flags, speed, length, thickness);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ModuleShotReliable(int hardpointIndex, int shotId, vector start, vector end, int flags, float speed, float length, float thickness)
	{
		if (!System.IsConsoleApp())
			PlayShotLocal(hardpointIndex, shotId, start, end, flags, speed, length, thickness);
	}

	//------------------------------------------------------------------------------------------------
	protected BFA_MechWeaponModuleComponent FindLocalModule(int hardpointIndex)
	{
		if (hardpointIndex < 0)
			return null;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(GetOwner().FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints)
			return null;

		return hardpoints.GetInstalledModuleAt(hardpointIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayShotLocal(int hardpointIndex, int shotId, vector start, vector end, int flags, float speed, float length, float thickness)
	{
		BFA_MechWeaponModuleComponent module = FindLocalModule(hardpointIndex);
		BFA_MechWeaponFX fx;
		if (module)
		{
			module.ClientOnShot(start, end);
			fx = module.GetFX();
		}

		BFA_MechEvents.RaiseShotFired(GetOwner(), module);

		// No effects configured (or legacy weapon): the debug line, visible in Workbench only.
		if (!fx || fx.m_bDebugTracer)
			AddTracerVisual(start, end, speed, length, thickness);

		vector dir = end - start;
		float distance = dir.Length();
		if (distance < 0.01)
			return;
		dir = dir * (1.0 / distance);

		BFA_MechProjectileVisual visual = new BFA_MechProjectileVisual();
		visual.m_iShotId = shotId;
		visual.m_iHardpoint = hardpointIndex;
		visual.m_vStart = start;
		visual.m_vEnd = end;
		visual.m_vDir = dir;
		visual.m_fDistance = distance;
		visual.m_fSpeed = Math.Max(speed, 1.0);
		visual.m_bHit = (flags & BFA_EMechShotFlags.HIT) != 0;
		visual.m_bServerImpact = (flags & BFA_EMechShotFlags.SERVER_IMPACT) != 0;

		if (fx)
		{
			if (fx.m_fProjectileSpeed > 0)
				visual.m_fSpeed = fx.m_fProjectileSpeed;

			visual.m_fLingerLeft = Math.Max(fx.m_fLinger, 0.0);

			vector mat[4];
			Math3D.DirectionAndUpMatrix(dir, vector.Up, mat);
			mat[3] = start;

			if (!fx.m_sProjectilePrefab.IsEmpty())
			{
				Resource resource = Resource.Load(fx.m_sProjectilePrefab);
				if (resource && resource.IsValid())
				{
					EntitySpawnParams spawnParams = new EntitySpawnParams();
					spawnParams.TransformMode = ETransformMode.WORLD;
					spawnParams.Transform[0] = mat[0];
					spawnParams.Transform[1] = mat[1];
					spawnParams.Transform[2] = mat[2];
					spawnParams.Transform[3] = mat[3];
					visual.m_pEntity = GetGame().SpawnEntityPrefabLocal(resource, GetOwner().GetWorld(), spawnParams);
				}
			}

			if (!fx.m_sTrailParticle.IsEmpty())
			{
				ParticleEffectEntitySpawnParams trailParams = new ParticleEffectEntitySpawnParams();
				trailParams.Transform[0] = mat[0];
				trailParams.Transform[1] = mat[1];
				trailParams.Transform[2] = mat[2];
				trailParams.Transform[3] = mat[3];
				visual.m_pTrail = ParticleEffectEntity.SpawnParticleEffect(fx.m_sTrailParticle, trailParams);
			}
		}

		m_aProjectiles.Insert(visual);
		while (m_aProjectiles.Count() > 256)
		{
			DestroyProjectileVisual(m_aProjectiles[0]);
			m_aProjectiles.Remove(0);
		}

		StartProjectileTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartProjectileTick()
	{
		if (m_bProjectileTickRunning)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_bProjectileTickRunning = true;
		m_iLastProjectileTickMs = System.GetTickCount();
		queue.CallLater(ProjectileTick, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopProjectileTick()
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(ProjectileTick);

		m_bProjectileTickRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves every visible projectile each frame, plays hitscan impacts on arrival, and removes
	//! projectiles after their linger time.
	protected void ProjectileTick()
	{
		int nowMs = System.GetTickCount();
		float dt = Math.Clamp((nowMs - m_iLastProjectileTickMs) * 0.001, 0.0, 0.1);
		m_iLastProjectileTickMs = nowMs;

		for (int i = m_aProjectiles.Count() - 1; i >= 0; i--)
		{
			BFA_MechProjectileVisual visual = m_aProjectiles[i];
			if (!visual)
			{
				m_aProjectiles.Remove(i);
				continue;
			}

			if (visual.m_bArrived)
			{
				visual.m_fLingerLeft -= dt;
				if (visual.m_fLingerLeft <= 0)
				{
					DestroyProjectileVisual(visual);
					m_aProjectiles.Remove(i);
				}
				continue;
			}

			visual.m_fAge += dt;
			float travelled = visual.m_fAge * visual.m_fSpeed;
			if (travelled >= visual.m_fDistance)
			{
				MoveProjectileVisual(visual, visual.m_vEnd);
				ArriveProjectileVisual(visual, visual.m_vEnd, !visual.m_bServerImpact);
				continue;
			}

			MoveProjectileVisual(visual, visual.m_vStart + visual.m_vDir * travelled);
		}

		if (m_aProjectiles.IsEmpty())
			StopProjectileTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveProjectileVisual(BFA_MechProjectileVisual visual, vector position)
	{
		if (visual.m_pEntity)
			visual.m_pEntity.SetOrigin(position);

		if (visual.m_pTrail)
			visual.m_pTrail.SetOrigin(position);
	}

	//------------------------------------------------------------------------------------------------
	protected void ArriveProjectileVisual(BFA_MechProjectileVisual visual, vector position, bool playImpact)
	{
		visual.m_bArrived = true;

		if (visual.m_pEntity)
			visual.m_pEntity.ClearFlags(EntityFlags.VISIBLE, true);

		if (!playImpact)
			return;

		BFA_MechWeaponModuleComponent module = FindLocalModule(visual.m_iHardpoint);
		if (!module)
			return;

		BFA_MechWeaponFX fx = module.GetFX();
		if (!fx || (!visual.m_bHit && !fx.m_bImpactOnMiss))
			return;

		PlayImpactLocal(module.GetImpactParticle(), fx, position);
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyProjectileVisual(BFA_MechProjectileVisual visual)
	{
		if (!visual)
			return;

		if (visual.m_pEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(visual.m_pEntity);

		if (visual.m_pTrail)
			delete visual.m_pTrail;

		visual.m_pEntity = null;
		visual.m_pTrail = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayImpactLocal(ResourceName particle, BFA_MechWeaponFX fx, vector position)
	{
		SpawnImpactParticle(particle, position);

		if (fx && !fx.m_sImpactSoundEvent.IsEmpty())
		{
			ResourceName impactProject = fx.m_sImpactSoundProject;
			if (impactProject.IsEmpty())
				impactProject = fx.m_sSoundProject;

			vector soundMat[4];
			Math3D.MatrixIdentity4(soundMat);
			soundMat[3] = position;
			BFA_MechWeaponModuleComponent.BFA_MechPlaySound(impactProject, fx.m_sImpactSoundEvent, soundMat);
		}

		if (!fx || fx.m_sImpactPrefab.IsEmpty())
			return;

		Resource resource = Resource.Load(fx.m_sImpactPrefab);
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(spawnParams.Transform);
		spawnParams.Transform[3] = position;

		IEntity impact = GetGame().SpawnEntityPrefabLocal(resource, GetOwner().GetWorld(), spawnParams);
		if (impact)
			GetGame().GetCallqueue().CallLater(DeleteLocalEntity, (int)(Math.Max(fx.m_fImpactPrefabLifetime, 0.1) * 1000.0), false, impact);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteLocalEntity(IEntity entity)
	{
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Visual-only impact for server-simulated rounds (rockets, intercepts). Damage is never
	//! applied here. shotId ends that shot's visible projectile at the real impact point.
	void ServerBroadcastModuleImpact(ResourceName particle, vector position, int shotId = 0, int hardpointIndex = -1)
	{
		if (!Replication.IsServer())
			return;

		if (!System.IsConsoleApp())
			ImpactLocal(particle, position, shotId, hardpointIndex);

		Rpc(RpcDo_ModuleImpact, particle, position, shotId, hardpointIndex);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ModuleImpact(ResourceName particle, vector position, int shotId, int hardpointIndex)
	{
		if (!System.IsConsoleApp())
			ImpactLocal(particle, position, shotId, hardpointIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected void ImpactLocal(ResourceName particle, vector position, int shotId, int hardpointIndex)
	{
		if (shotId > 0)
		{
			foreach (BFA_MechProjectileVisual visual : m_aProjectiles)
			{
				if (!visual || visual.m_iShotId != shotId || visual.m_bArrived)
					continue;

				MoveProjectileVisual(visual, position);
				ArriveProjectileVisual(visual, position, false);
				break;
			}
		}

		BFA_MechWeaponFX fx;
		BFA_MechWeaponModuleComponent module = FindLocalModule(hardpointIndex);
		if (module)
			fx = module.GetFX();

		PlayImpactLocal(particle, fx, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Reload / empty / rearm events for effects and the client ammo mirror.
	void ServerBroadcastModuleEvent(BFA_MechWeaponModuleComponent module, int eventType, int value = 0)
	{
		if (!Replication.IsServer() || !module)
			return;

		int hardpointIndex = module.GetHardpointIndex();
		if (!System.IsConsoleApp())
			module.ClientOnModuleEvent(eventType, value);

		Rpc(RpcDo_ModuleEvent, hardpointIndex, eventType, value);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ModuleEvent(int hardpointIndex, int eventType, int value)
	{
		BFA_MechWeaponModuleComponent module = FindLocalModule(hardpointIndex);
		if (module)
			module.ClientOnModuleEvent(eventType, value);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnImpactParticle(ResourceName particle, vector position)
	{
		if (particle.IsEmpty())
			return;

		ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
		Math3D.MatrixIdentity4(spawnParams.Transform);
		spawnParams.Transform[3] = position;
		ParticleEffectEntity.SpawnParticleEffect(particle, spawnParams);
	}

	//------------------------------------------------------------------------------------------------
	//! Held fire for a routed station/group.
	//! Installed modular weapons take priority. Legacy test definitions remain as fallback.
	void ServerSetFireHeld(string stationId, int weaponGroup, bool held)
	{
		if (!Replication.IsServer())
			return;

		int moduleMatchedCount = RouteHeldFireToModules(stationId, weaponGroup, held);
		if (moduleMatchedCount > 0)
		{
			BFA_MechLog.Log(string.Format("[BFA MODULE ROUTER] station=%1 group=%2 held=%3 modules=%4", stationId, weaponGroup, held, moduleMatchedCount));
			return;
		}

		if (!m_aWeapons)
			return;

		bool matched = false;
		int matchedCount = 0;
		int nowMs = GetWorldTimeMs();

		foreach (BFA_MechWeaponDefinition weapon : m_aWeapons)
		{
			if (!weapon || weapon.m_iWeaponGroup != weaponGroup || !StationMatches(weapon, stationId))
				continue;

			if (IsLegacyWeaponSuppressed(weapon))
				continue;

			matched = true;
			matchedCount++;
			if (weapon.m_bServerFireHeld == held)
				continue;

			weapon.m_bServerFireHeld = held;
			if (held)
			{
				FireOneShot(weapon);
				weapon.m_iNextShotTimeMs = nowMs + GetFireIntervalMs(weapon);
			}
		}

		if (!matched)
		{
			BFA_MechLog.Log(string.Format("[BFA FIRE TEST] WEAPON FAIL - no weapons matched station=%1 group=%2", stationId, weaponGroup), LogLevel.WARNING);
			return;
		}

		BFA_MechLog.Log(string.Format("[BFA FIRE TEST] WEAPON STATE station=%1 group=%2 held=%3", stationId, weaponGroup, held));
		BFA_MechLog.Log("[BFA FIRE TEST] WEAPON MATCHED count=" + matchedCount.ToString());

		if (held)
			StartServerFireTick();
		else if (!HasHeldWeapons())
			StopServerFireTick();
	}

	//------------------------------------------------------------------------------------------------
	//! One-shot routed fire. Intended for shoulders/missiles/auxiliary weapons.
	void ServerFirePulse(string stationId, int weaponGroup)
	{
		if (!Replication.IsServer())
			return;

		int moduleMatchedCount = RoutePulseFireToModules(stationId, weaponGroup);
		if (moduleMatchedCount > 0)
		{
			BFA_MechLog.Log(string.Format("[BFA MODULE ROUTER] pulse station=%1 group=%2 modules=%3", stationId, weaponGroup, moduleMatchedCount));
			return;
		}

		if (!m_aWeapons)
			return;

		int nowMs = GetWorldTimeMs();
		bool matched = false;

		foreach (BFA_MechWeaponDefinition weapon : m_aWeapons)
		{
			if (!weapon || weapon.m_iWeaponGroup != weaponGroup || !StationMatches(weapon, stationId))
				continue;

			if (IsLegacyWeaponSuppressed(weapon))
				continue;

			matched = true;
			if (nowMs < weapon.m_iNextShotTimeMs)
				continue;

			FireOneShot(weapon);
			weapon.m_iNextShotTimeMs = nowMs + GetFireIntervalMs(weapon);
		}

		if (matched)
			BFA_MechLog.Log(string.Format("[BFA FIRE AUX] station=%1 group=%2", stationId, weaponGroup));
	}

	//------------------------------------------------------------------------------------------------
	protected vector BuildMuzzleStart(BFA_MechWeaponDefinition weapon, out vector direction)
	{
		direction = vector.Forward;

		if (!ResolveWeapon(weapon))
			return GetOwner().GetOrigin();

		vector weaponWorld[4];
		weapon.m_pWeaponEntity.GetWorldTransform(weaponWorld);

		direction = weaponWorld[2];
		if (direction.LengthSq() < 0.000001)
			direction = GetOwner().GetTransformAxis(2);

		if (direction.LengthSq() < 0.000001)
			direction = vector.Forward;

		direction.Normalize();
		if (weapon.m_bReverseMuzzleForward)
			direction = direction * -1.0;

		vector localMuzzle = vector.Zero;
		if (weapon.m_bAutoMuzzleFromBounds)
		{
			vector mins, maxs;
			weapon.m_pWeaponEntity.GetBounds(mins, maxs);
			localMuzzle = (mins + maxs) * 0.5;

			if (weapon.m_bReverseMuzzleForward)
				localMuzzle[2] = mins[2];
			else
				localMuzzle[2] = maxs[2];
		}

		localMuzzle = localMuzzle + weapon.m_vMuzzleLocalOffset;

		vector start = weaponWorld[3]
			+ weaponWorld[0] * localMuzzle[0]
			+ weaponWorld[1] * localMuzzle[1]
			+ weaponWorld[2] * localMuzzle[2];

		start = start + direction * Math.Max(weapon.m_fMuzzleClearance, 0.0);
		return start;
	}

	//------------------------------------------------------------------------------------------------
	protected vector TraceShot(BFA_MechWeaponDefinition weapon, vector start, vector direction, out IEntity hitEntity)
	{
		hitEntity = null;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return start + direction * Math.Max(weapon.m_fRange, 10.0);

		vector finalEnd = start + direction * Math.Max(weapon.m_fRange, 10.0);
		vector from = start;
		IEntity owner = GetOwner();

		for (int i = 0; i < 8; i++)
		{
			TraceParam param = new TraceParam();
			param.Start = from;
			param.End = finalEnd;
			param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			param.LayerMask = EPhysicsLayerPresets.Projectile;

			array<IEntity> excludes = { owner };
			if (weapon.m_pWeaponEntity)
				excludes.Insert(weapon.m_pWeaponEntity);
			param.ExcludeArray = excludes;

			float fraction = world.TraceMove(param, null);
			if (fraction >= 1.0)
				return finalEnd;

			vector hit = param.Start + (param.End - param.Start) * fraction;
			IEntity tracedEntity = param.TraceEnt;

			if (tracedEntity && tracedEntity.GetRootParent() == owner)
			{
				from = hit + direction * 0.25;
				continue;
			}

			hitEntity = tracedEntity;
			return hit;
		}

		return finalEnd;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_DamageManagerComponent ResolveDamageManager(IEntity hitEntity)
	{
		if (!hitEntity)
			return null;

		// Soldiers: vests, helmets, backpacks and weapons are separate entities, some with their
		// own damage manager (a vest has "Armor" with 10000 HP). A hit on any of them must hurt
		// the soldier, so a character root always wins.
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
			SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(
				current.FindComponent(SCR_DamageManagerComponent)
			);

			if (damageManager)
				return damageManager;

			current = current.GetParent();
		}

		IEntity root = hitEntity.GetRootParent();
		if (!root)
			return null;

		return SCR_DamageManagerComponent.Cast(root.FindComponent(SCR_DamageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity ResolveDamageInstigator(BFA_MechWeaponDefinition weapon)
	{
		IEntity mech = GetOwner();
		if (!mech || !weapon)
			return mech;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(
			mech.FindComponent(BFA_MechCoreComponent)
		);

		if (!core)
			return mech;

		IEntity occupant = core.GetStationOccupant(weapon.m_sControlStation);
		if (occupant)
			return occupant;

		return mech;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyRealDamage(BFA_MechWeaponDefinition weapon, IEntity hitEntity)
	{
		if (!Replication.IsServer() || !weapon || !weapon.m_bApplyDamage || weapon.m_fDamage <= 0.0 || !hitEntity)
			return;

		SCR_DamageManagerComponent damageManager = ResolveDamageManager(hitEntity);
		if (!damageManager)
		{
			BFA_MechLog.Log("[BFA FIRE TEST] DAMAGE SKIP - hit entity has no SCR_DamageManagerComponent");
			return;
		}

		HitZone hitZone = damageManager.GetDefaultHitZone();
		if (!hitZone)
		{
			BFA_MechLog.Log("[BFA FIRE TEST] DAMAGE SKIP - damage manager has no default hit zone");
			return;
		}

		float healthBefore = hitZone.GetHealth();
		float maxHealth = hitZone.GetMaxHealth();
		float baseMultiplier = hitZone.GetBaseDamageMultiplier();
		float kineticMultiplier = hitZone.GetDamageMultiplier(EDamageType.KINETIC);
		float damageReduction = hitZone.GetDamageReduction();
		float damageThreshold = hitZone.GetDamageThreshold();

		vector empty[3];
		empty[0] = vector.Zero;
		empty[1] = vector.Zero;
		empty[2] = vector.Zero;

		IEntity instigatorEntity = ResolveDamageInstigator(weapon);
		Instigator instigator = Instigator.CreateInstigator(instigatorEntity);

		// First try normal KINETIC damage so ordinary hit zones still use their
		// configured armor multipliers/reduction.
		SCR_DamageContext kineticContext = new SCR_DamageContext(
			EDamageType.KINETIC,
			weapon.m_fDamage,
			empty,
			GetOwner(),
			hitZone,
			instigator,
			null,
			-1,
			-1
		);

		damageManager.HandleDamage(kineticContext);

		float healthAfterKinetic = hitZone.GetHealth();
		float kineticHealthLost = healthBefore - healthAfterKinetic;

		// Heavy armored hulls can reduce the current KINETIC hit to exactly zero.
		// In that case, optionally apply a TRUE-damage fallback so mech anti-armor
		// weapons can still hurt armored vehicles without making wheels/components
		// receive a second hit when kinetic damage already worked.
		if (
			weapon.m_bArmorPiercingFallback
			&& healthBefore > 0.0
			&& kineticHealthLost <= 0.001
		)
		{
			float apDamage = weapon.m_fDamage * Math.Max(weapon.m_fArmorPiercingMultiplier, 0.0);

			if (apDamage > 0.0)
			{
				SCR_DamageContext armorPiercingContext = new SCR_DamageContext(
					EDamageType.TRUE,
					apDamage,
					empty,
					GetOwner(),
					hitZone,
					instigator,
					null,
					-1,
					-1
				);

				damageManager.HandleDamage(armorPiercingContext);

				float healthAfterAP = hitZone.GetHealth();
				float apHealthLost = healthAfterKinetic - healthAfterAP;

				BFA_MechLog.Log(string.Format(
					"[BFA DAMAGE AP] weapon=%1 apDamage=%2 healthBefore=%3 healthAfter=%4 healthLost=%5 zone=%6",
					weapon.m_sWeaponId,
					apDamage,
					healthAfterKinetic,
					healthAfterAP,
					apHealthLost,
					hitZone.GetName()
				));
			}
		}

		float healthAfter = hitZone.GetHealth();
		float totalHealthLost = healthBefore - healthAfter;

		BFA_MechLog.Log(string.Format(
			"[BFA DAMAGE CHECK] weapon=%1 amount=%2 healthBefore=%3 healthAfter=%4 healthLost=%5 maxHealth=%6",
			weapon.m_sWeaponId,
			weapon.m_fDamage,
			healthBefore,
			healthAfter,
			totalHealthLost,
			maxHealth
		));

		BFA_MechLog.Log(string.Format(
			"[BFA DAMAGE CHECK] zone=%1 baseMult=%2 kineticMult=%3 reduction=%4 threshold=%5 target=%6",
			hitZone.GetName(),
			baseMultiplier,
			kineticMultiplier,
			damageReduction,
			damageThreshold,
			hitEntity
		));
	}

	//------------------------------------------------------------------------------------------------
	protected void FireOneShot(BFA_MechWeaponDefinition weapon)
	{
		if (!Replication.IsServer() || !weapon)
			return;

		if (!ResolveWeapon(weapon))
		{
			if (!weapon.m_bLoggedMissingWeapon)
			{
				weapon.m_bLoggedMissingWeapon = true;
				BFA_MechLog.Log("[BFA FIRE TEST] SHOT FAIL - unresolved weapon: " + weapon.m_sWeaponId, LogLevel.WARNING);
			}
			return;
		}

		// Heat, destroyed weapon part ... (BFA_MechExtension hooks).
		BFA_MechExtension extension = BFA_MechExtension.Get();
		if (!extension.CanWeaponEntityFire(GetOwner(), weapon.m_pWeaponEntity))
			return;
		extension.OnWeaponFiredHeat(GetOwner(), weapon.m_pWeaponEntity, weapon.m_fHeatPerShot, weapon.m_sWeaponId);

		weapon.m_iShotCount++;
		BFA_MechLog.Log("[BFA FIRE TEST] SHOT #" + weapon.m_iShotCount.ToString() + " weapon=" + weapon.m_sWeaponId);

		vector direction;
		vector start = BuildMuzzleStart(weapon, direction);
		IEntity hitEntity;
		vector end = TraceShot(weapon, start, direction, hitEntity);

		if (hitEntity)
			BFA_MechLog.Log(string.Format("[BFA FIRE TEST] TRACE HIT weapon=%1 target=%2", weapon.m_sWeaponId, hitEntity));
		else
			BFA_MechLog.Log("[BFA FIRE TEST] TRACE MISS weapon=" + weapon.m_sWeaponId);

		ApplyRealDamage(weapon, hitEntity);

		if (!System.IsConsoleApp())
			AddTracerVisual(start, end, weapon.m_fTracerSpeed, weapon.m_fTracerLength, weapon.m_fTracerThickness);

		Rpc(RpcDo_TracerShot, start, end, weapon.m_fTracerSpeed, weapon.m_fTracerLength, weapon.m_fTracerThickness);

		// Fire sound: taken from the weapon part's module Effects > Sound (same place module weapons use).
		BFA_MechWeaponModuleComponent soundModule;
		if (weapon.m_pWeaponEntity)
			soundModule = BFA_MechWeaponModuleComponent.Cast(weapon.m_pWeaponEntity.FindComponent(BFA_MechWeaponModuleComponent));
		if (soundModule && soundModule.GetFX())
		{
			BFA_MechWeaponFX fx = soundModule.GetFX();
			if (!fx.m_sSoundProject.IsEmpty() && !fx.m_sFireSoundEvent.IsEmpty())
			{
				string soundProject = fx.m_sSoundProject;
				if (!System.IsConsoleApp())
					PlayShotSound(soundProject, fx.m_sFireSoundEvent, start);
				Rpc(RpcDo_ShotSound, soundProject, fx.m_sFireSoundEvent, start);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_ShotSound(string soundProject, string eventName, vector position)
	{
		if (System.IsConsoleApp())
			return;
		PlayShotSound(soundProject, eventName, position);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayShotSound(string soundProject, string eventName, vector position)
	{
		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = position;
		BFA_MechWeaponModuleComponent.BFA_MechPlaySound(soundProject, eventName, mat);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_TracerShot(vector start, vector end, float speed, float length, float thickness)
	{
		if (System.IsConsoleApp())
			return;

		BFA_MechLog.Log(string.Format("[BFA FIRE TEST] TRACER RECEIVED start=%1 end=%2", start, end));
		AddTracerVisual(start, end, speed, length, thickness);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddTracerVisual(vector start, vector end, float speed, float length, float thickness)
	{
		if (System.IsConsoleApp())
			return;

		BFA_MechTracerVisual tracer = new BFA_MechTracerVisual();
		tracer.m_vStart = start;
		tracer.m_vEnd = end;
		tracer.m_fAge = 0;
		tracer.m_fSpeed = Math.Max(speed, 1.0);
		tracer.m_fLength = Math.Max(length, 0.25);
		tracer.m_fThickness = Math.Max(thickness, 0.005);
		m_aTracerVisuals.Insert(tracer);

		while (m_aTracerVisuals.Count() > 128)
			m_aTracerVisuals.RemoveItem(m_aTracerVisuals[0]);

		if (m_bTracerTickRunning)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			m_iLastTracerTickMs = world.GetWorldTime();
		else
			m_iLastTracerTickMs = 0;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_bTracerTickRunning = true;
		queue.CallLater(TracerVisualTick, 16, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void TracerVisualTick()
	{
		if (System.IsConsoleApp())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		int nowMs = world.GetWorldTime();
		float dt = 0.016;
		if (m_iLastTracerTickMs > 0)
			dt = Math.Clamp((nowMs - m_iLastTracerTickMs) * 0.001, 0.001, 0.05);

		m_iLastTracerTickMs = nowMs;
		int flags = ShapeFlags.ONCE | ShapeFlags.NOZBUFFER;

		for (int i = m_aTracerVisuals.Count() - 1; i >= 0; i--)
		{
			BFA_MechTracerVisual tracer = m_aTracerVisuals[i];
			if (!tracer)
				continue;

			tracer.m_fAge = tracer.m_fAge + dt;
			float totalDistance = vector.Distance(tracer.m_vStart, tracer.m_vEnd);
			if (totalDistance < 0.01)
			{
				m_aTracerVisuals.RemoveItem(tracer);
				continue;
			}

			float headTravel = tracer.m_fAge * tracer.m_fSpeed;
			float tailTravel = headTravel - tracer.m_fLength;
			if (tailTravel >= totalDistance)
			{
				m_aTracerVisuals.RemoveItem(tracer);
				continue;
			}

			float headDistance = Math.Clamp(headTravel, 0.0, totalDistance);
			float tailDistance = Math.Clamp(tailTravel, 0.0, totalDistance);
			float headT = headDistance / totalDistance;
			float tailT = tailDistance / totalDistance;

			vector lineStart = vector.Lerp(tracer.m_vStart, tracer.m_vEnd, tailT);
			vector lineEnd = vector.Lerp(tracer.m_vStart, tracer.m_vEnd, headT);

			if (vector.Distance(lineStart, lineEnd) > 0.01)
				Shape.CreateArrow(lineStart, lineEnd, tracer.m_fThickness, Color.YELLOW, flags);
		}

		if (!m_aTracerVisuals.IsEmpty())
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(TracerVisualTick);

		m_bTracerTickRunning = false;
		m_iLastTracerTickMs = 0;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_bServerFireTickRunning = false;
		m_bTracerTickRunning = false;
		m_iLastTracerTickMs = 0;

		int weaponCount = 0;
		if (m_aWeapons)
		{
			weaponCount = m_aWeapons.Count();
			foreach (BFA_MechWeaponDefinition weapon : m_aWeapons)
			{
				if (!weapon)
					continue;

				weapon.m_bServerFireHeld = false;
				weapon.m_bLoggedFirstShot = false;
				weapon.m_bLoggedFirstDamage = false;
				weapon.m_bLoggedMissingWeapon = false;
				weapon.m_iNextShotTimeMs = 0;
				weapon.m_pWeaponEntity = null;
			}
		}

		BFA_MechLog.Log(string.Format("[BFA FIRE] V55 MODULAR_WEAPON_ROUTER initialized legacyWeapons=%1", weaponCount));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(ServerFireTick);
				queue.Remove(TracerVisualTick);
				queue.Remove(ProjectileTick);
				queue.Remove(DeleteLocalEntity);
			}
		}

		foreach (BFA_MechProjectileVisual visual : m_aProjectiles)
			DestroyProjectileVisual(visual);
		m_aProjectiles.Clear();
		m_bProjectileTickRunning = false;

		m_bServerFireTickRunning = false;
		m_bTracerTickRunning = false;

		super.OnDelete(owner);
	}
}
