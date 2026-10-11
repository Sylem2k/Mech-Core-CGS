[ComponentEditorProps(category: "GameScripted/Mech/Weapons", description: "Anti-missile point defense. Automatically shoots down incoming rockets/missiles while its crew station is manned; the station can also fire a manual defensive burst.")]
class BFA_MechPointDefenseModuleComponentClass : BFA_MechWeaponModuleComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Active protection for a mech.
//!
//! AUTO:   while the control station (default COMMANDER) is occupied - by a player OR an AI -
//!         the system scans every m_iScanIntervalMs and intercepts the most urgent threat that is
//!         on course to pass close to the mech, spending one charge per kill.
//! MANUAL: the station's pulse key (aux fire) fires a short defensive burst that destroys every
//!         threat inside m_fBurstRadius for one charge, even with auto disabled.
//!
//! Threats:
//!  - rockets fired by any MechCore rocket launcher (always reliable; we own their simulation)
//!  - vanilla rockets/missiles (best effort): entities near the mech carrying one of the
//!    component types listed in m_aVanillaThreatComponents, moving slower than
//!    m_fMaxThreatSpeed (filters out bullets and kinetic tank shells).
//!
//! Charges recharge one at a time. Nothing here is replicated except visuals (via the weapon
//! router), so it works the same for AI crews later.
class BFA_MechPointDefenseModuleComponent : BFA_MechWeaponModuleComponent
{
	// ---- Detection -------------------------------------------------------------------------
	[Attribute("60", UIWidgets.Auto, "Detection radius in metres around the mech.", category: "Point Defense")]
	protected float m_fDetectRadius;

	[Attribute("8", UIWidgets.Auto, "A threat counts only if its course passes within this many metres of the mech centre.", category: "Point Defense")]
	protected float m_fProtectRadius;

	[Attribute("6", UIWidgets.Auto, "Threats closer than this are too close to engage automatically.", category: "Point Defense")]
	protected float m_fMinEngageRange;

	[Attribute("2.0", UIWidgets.Auto, "Only engage threats expected to arrive within this many seconds.", category: "Point Defense")]
	protected float m_fMaxTimeToImpact;

	[Attribute("50", UIWidgets.Auto, "Milliseconds between automatic scans.", category: "Point Defense")]
	protected int m_iScanIntervalMs;

	// ---- Charges ---------------------------------------------------------------------------
	[Attribute("4", UIWidgets.Auto, "Maximum stored intercept charges.", category: "Charges")]
	protected int m_iMaxCharges;

	[Attribute("6.0", UIWidgets.Auto, "Seconds to recharge one charge.", category: "Charges")]
	protected float m_fRechargeTime;

	[Attribute("0.4", UIWidgets.Auto, "Minimum seconds between automatic intercepts.", category: "Charges")]
	protected float m_fInterceptCooldown;

	[Attribute("1.0", UIWidgets.Auto, "Chance (0-1) that an automatic intercept succeeds. The charge is spent either way.", category: "Charges")]
	protected float m_fInterceptChance;

	// ---- Modes -----------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "Automatic interception while the control station is manned.", category: "Modes")]
	protected bool m_bAutoMode;

	[Attribute("true", UIWidgets.CheckBox, "Automatic mode needs someone (player or AI) at the control station. Off = always active.", category: "Modes")]
	protected bool m_bAutoRequiresCrew;

	[Attribute("25", UIWidgets.Auto, "Manual burst radius in metres.", category: "Modes")]
	protected float m_fBurstRadius;

	// ---- Vanilla threats -------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "Also try to intercept vanilla rockets/missiles (best effort).", category: "Vanilla Threats")]
	protected bool m_bInterceptVanilla;

	[Attribute("", UIWidgets.Auto, "Script class names that mark an entity as a vanilla rocket/missile. Unknown names are ignored safely. Defaults are used when empty.", category: "Vanilla Threats")]
	protected ref array<string> m_aVanillaThreatComponents;

	[Attribute("", UIWidgets.Auto, "Script class names of warhead/fuze components. A vanilla threat must ALSO carry one of these, so plain bullets are never targeted. Defaults are used when empty.", category: "Vanilla Threats")]
	protected ref array<string> m_aVanillaWarheadComponents;

	[Attribute("450", UIWidgets.Auto, "Ignore anything faster than this (m/s): bullets and kinetic tank shells.", category: "Vanilla Threats")]
	protected float m_fMaxThreatSpeed;

	// ---- Visuals ---------------------------------------------------------------------------
	[Attribute("{2E8B5767FDBED4A1}Particles/Logistics/Explosion/Ammo_Rack/Explosion_Ammo_Rack_Medium.ptc", UIWidgets.ResourceNamePicker, "Effect where a threat is destroyed (.ptc). Swap for a smaller effect when you have one.", params: "ptc", category: "Visuals")]
	protected ResourceName m_sInterceptParticle;

	[Attribute("0.06", UIWidgets.Auto, "Intercept beam/tracer thickness.", category: "Visuals")]
	protected float m_fBeamThickness;

	[Attribute("40", UIWidgets.Auto, "Ignore anything slower than this (m/s). Carried ammo, dropped launchers and items moved by a soldier getting in never count.", category: "Vanilla Threats")]
	protected float m_fMinThreatSpeed;

	[Attribute("false", UIWidgets.CheckBox, "Print point-defense diagnostics.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime (server) ------------------------------------------------------------------
	protected int m_iCharges;
	protected float m_fRechargeProgress;
	protected float m_fCooldownLeft;
	protected bool m_bScanRunning;
	protected int m_iLastScanMs;

	protected ref array<IEntity> m_aQuery = {};
	protected ref map<IEntity, vector> m_mLastThreatPos = new map<IEntity, vector>();
	protected ref map<IEntity, vector> m_mLastThreatVel = new map<IEntity, vector>();
	protected ref array<typename> m_aThreatTypes = {};
	protected ref array<typename> m_aWarheadTypes = {};

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_iCharges = Math.Max(m_iMaxCharges, 0);
		m_fRechargeProgress = 0;
		m_fCooldownLeft = 0;
		m_bScanRunning = false;

		BuildThreatTypes();
		BFA_MechLog.Log("[BFA POINT DEFENSE] initialized");
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		StopScan();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void BindToHardpoint(IEntity mechOwner, int hardpointIndex, string hardpointName, int hardpointClass, string routeStation = "", int routeGroup = 0)
	{
		super.BindToHardpoint(mechOwner, hardpointIndex, hardpointName, hardpointClass, routeStation, routeGroup);

		if (Replication.IsServer() && m_bBoundToHardpoint)
			StartScan();
	}

	//------------------------------------------------------------------------------------------------
	override void UnbindFromHardpoint()
	{
		StopScan();
		super.UnbindFromHardpoint();
	}

	//------------------------------------------------------------------------------------------------
	// Read-only state for HUD / AI.
	int GetCharges()
	{
		return m_iCharges;
	}

	int GetMaxCharges()
	{
		return m_iMaxCharges;
	}

	bool IsAutoActive()
	{
		return m_bAutoMode && IsCrewed();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildThreatTypes()
	{
		array<string> moveNames = {};
		if (m_aVanillaThreatComponents && !m_aVanillaThreatComponents.IsEmpty())
		{
			moveNames.Copy(m_aVanillaThreatComponents);
		}
		else
		{
			moveNames.Insert("MissileMoveComponent");
			moveNames.Insert("ShellMoveComponent");
		}

		array<string> warheadNames = {};
		if (m_aVanillaWarheadComponents && !m_aVanillaWarheadComponents.IsEmpty())
			warheadNames.Copy(m_aVanillaWarheadComponents);
		else
			warheadNames.Insert("BaseTriggerComponent");

		ResolveTypes(moveNames, m_aThreatTypes);
		ResolveTypes(warheadNames, m_aWarheadTypes);

		BFA_MechLog.Log(string.Format("[BFA POINT DEFENSE] vanilla threat types resolved: move=%1 warhead=%2", m_aThreatTypes.Count(), m_aWarheadTypes.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Unknown class names are skipped, so a wrong name can never break the module.
	protected void ResolveTypes(notnull array<string> names, notnull array<typename> outTypes)
	{
		outTypes.Clear();
		foreach (string name : names)
		{
			typename t = name.ToType();
			if (t)
				outTypes.Insert(t);
			else
				BFA_MechLog.Log("[BFA POINT DEFENSE] component type not found (ignored): " + name, LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsCrewed()
	{
		if (!m_bAutoRequiresCrew)
			return true;

		if (!m_pMechOwner)
			return false;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(m_pMechOwner.FindComponent(BFA_MechCoreComponent));
		if (!core)
			return true;

		string station = GetControlStation();
		if (station.IsEmpty())
			return true;

		// Any occupant counts - a player or an AI.
		return core.GetStationOccupant(station) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected void StartScan()
	{
		if (m_bScanRunning)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_bScanRunning = true;
		m_iLastScanMs = GetWorldTimeMs();
		queue.CallLater(ScanTick, Math.Max(m_iScanIntervalMs, 20), true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopScan()
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
				queue.Remove(ScanTick);
		}

		m_bScanRunning = false;
		m_mLastThreatPos.Clear();
		m_mLastThreatVel.Clear();
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
	protected vector GetDefenseCenter()
	{
		if (m_pMechOwner)
			return m_pMechOwner.GetOrigin() + vector.Up * 1.0;

		return GetOwner().GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	protected void ScanTick()
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint)
		{
			StopScan();
			return;
		}

		int nowMs = GetWorldTimeMs();
		float dt = Math.Clamp((nowMs - m_iLastScanMs) * 0.001, 0.001, 0.5);
		m_iLastScanMs = nowMs;

		// Recharge and cooldown always tick.
		if (m_iCharges < m_iMaxCharges)
		{
			m_fRechargeProgress += dt;
			if (m_fRechargeProgress >= Math.Max(m_fRechargeTime, 0.1))
			{
				m_fRechargeProgress = 0;
				m_iCharges++;
				BroadcastModuleEvent(BFA_EMechModuleEvent.CHARGES, m_iCharges);
				if (m_bDebug)
					Print(string.Format("[BFA POINT DEFENSE] recharged charges=%1/%2", m_iCharges, m_iMaxCharges));
			}
		}

		m_fCooldownLeft = Math.Max(m_fCooldownLeft - dt, 0.0);

		// Vanilla velocity tracking must run every scan even when not engaging.
		array<IEntity> threatEntities = {};
		array<vector> vanillaVel = {};
		CollectVanillaThreats(dt, m_fDetectRadius, threatEntities, vanillaVel);

		if (!m_bAutoMode || !IsCrewed() || m_iCharges <= 0 || m_fCooldownLeft > 0)
			return;

		// Pick the single most urgent threat (soonest arrival).
		vector center = GetDefenseCenter();
		float bestTime = Math.Max(m_fMaxTimeToImpact, 0.1);
		BFA_MechRocketLauncherModuleComponent bestLauncher;
		BFA_MechRocketInFlight bestRocket;
		IEntity bestVanilla;
		vector bestPos;

		array<BFA_MechRocketLauncherModuleComponent> launchers = {};
		BFA_MechRocketLauncherModuleComponent.GetActiveLaunchers(launchers);
		foreach (BFA_MechRocketLauncherModuleComponent launcher : launchers)
		{
			for (int r = 0; r < launcher.GetRocketCount(); r++)
			{
				BFA_MechRocketInFlight rocket = launcher.GetRocketAt(r);
				if (!rocket || rocket.m_pMechOwner == m_pMechOwner)
					continue;

				float t;
				if (!IsInboundThreat(center, rocket.m_vPosition, rocket.m_vDirection * launcher.GetRocketSpeed(), true, t))
					continue;

				if (t < bestTime)
				{
					bestTime = t;
					bestLauncher = launcher;
					bestRocket = rocket;
					bestVanilla = null;
					bestPos = rocket.m_vPosition;
				}
			}
		}

		for (int v = 0; v < threatEntities.Count(); v++)
		{
			float tv;
			if (!IsInboundThreat(center, threatEntities[v].GetOrigin(), vanillaVel[v], true, tv))
				continue;

			if (tv < bestTime)
			{
				bestTime = tv;
				bestLauncher = null;
				bestRocket = null;
				bestVanilla = threatEntities[v];
				bestPos = threatEntities[v].GetOrigin();
			}
		}

		if (!bestRocket && !bestVanilla)
			return;

		m_iCharges--;
		BroadcastModuleEvent(BFA_EMechModuleEvent.CHARGES, m_iCharges);
		m_fCooldownLeft = Math.Max(m_fInterceptCooldown, 0.0);

		if (Math.RandomFloat01() > Math.Clamp(m_fInterceptChance, 0.0, 1.0))
		{
			if (m_bDebug)
				Print(string.Format("[BFA POINT DEFENSE] AUTO intercept MISSED charges=%1", m_iCharges));
			return;
		}

		DestroyThreat(bestLauncher, bestRocket, bestVanilla, bestPos);

		if (m_bDebug)
			Print(string.Format("[BFA POINT DEFENSE] AUTO intercept at %1 eta=%2s charges=%3", bestPos, bestTime, m_iCharges));
	}

	//------------------------------------------------------------------------------------------------
	//! Is this object on a course that passes within m_fProtectRadius of the mech, soon?
	protected bool IsInboundThreat(vector center, vector position, vector velocity, bool respectMinRange, out float timeToClosest)
	{
		timeToClosest = 999;

		vector rel = position - center;
		float distance = rel.Length();
		if (distance > Math.Max(m_fDetectRadius, 1.0))
			return false;

		if (respectMinRange && distance < Math.Max(m_fMinEngageRange, 0.0))
			return false;

		float speedSq = velocity.LengthSq();
		if (speedSq < 1.0)
			return false;

		// Time of closest approach; negative = moving away.
		float t = -vector.Dot(rel, velocity) / speedSq;
		if (t <= 0 || t > Math.Max(m_fMaxTimeToImpact, 0.1))
			return false;

		vector closest = rel + velocity * t;
		if (closest.Length() > Math.Max(m_fProtectRadius, 0.5))
			return false;

		timeToClosest = t;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectQueryEntity(IEntity entity)
	{
		if (entity)
			m_aQuery.Insert(entity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsVanillaThreatEntity(IEntity entity)
	{
		if (!entity || m_aThreatTypes.IsEmpty())
			return false;

		if (m_pMechOwner && entity.GetRootParent() == m_pMechOwner)
			return false;

		// A rocket in flight is a free entity. Anything attached to something - ammo in a
		// soldier's inventory, a loaded launcher, a vehicle's magazine - is never a threat.
		// (Without this, an AI with RPG rounds climbing in got "intercepted" on the spot.)
		if (entity.GetParent())
			return false;

		if (ChimeraCharacter.Cast(entity) || Vehicle.Cast(entity))
			return false;

		bool moves = false;
		foreach (typename moveType : m_aThreatTypes)
		{
			if (entity.FindComponent(moveType))
			{
				moves = true;
				break;
			}
		}

		if (!moves)
			return false;

		// No warhead types resolved: fall back to movement + speed filter only.
		if (m_aWarheadTypes.IsEmpty())
			return true;

		foreach (typename warheadType : m_aWarheadTypes)
		{
			if (entity.FindComponent(warheadType))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds vanilla rockets/missiles near the mech and estimates their velocity from how far they
	//! moved since the last scan (projectiles do not all expose physics velocity).
	protected void CollectVanillaThreats(float dt, float radius, notnull array<IEntity> outEntities, notnull array<vector> outVelocities)
	{
		outEntities.Clear();
		outVelocities.Clear();

		if (!m_bInterceptVanilla || m_aThreatTypes.IsEmpty())
		{
			m_mLastThreatPos.Clear();
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		m_aQuery.Clear();
		world.QueryEntitiesBySphere(GetDefenseCenter(), Math.Max(radius, 1.0), CollectQueryEntity, null, EQueryEntitiesFlags.ALL);

		map<IEntity, vector> seen = new map<IEntity, vector>();
		map<IEntity, vector> seenVel = new map<IEntity, vector>();
		float maxSpeed = Math.Max(m_fMaxThreatSpeed, 1.0);
		float minSpeed = Math.Max(m_fMinThreatSpeed, 0.0);

		foreach (IEntity entity : m_aQuery)
		{
			if (!IsVanillaThreatEntity(entity))
				continue;

			vector position = entity.GetOrigin();
			seen.Insert(entity, position);

			vector last;
			if (!m_mLastThreatPos.Find(entity, last))
				continue; // need two samples for a velocity

			vector velocity = (position - last) * (1.0 / dt);
			seenVel.Insert(entity, velocity);

			float speed = velocity.Length();
			if (speed > maxSpeed || speed < minSpeed)
				continue;

			// Needs two matching velocity samples in a row: real rockets fly steadily, while a
			// teleport (getting in/out, inventory moves, respawn) is a single jump.
			vector lastVelocity;
			if (!m_mLastThreatVel.Find(entity, lastVelocity))
				continue;

			float lastSpeed = lastVelocity.Length();
			if (lastSpeed < minSpeed || Math.AbsFloat(speed - lastSpeed) > Math.Max(speed, lastSpeed) * 0.35)
				continue;

			if (vector.Dot(velocity, lastVelocity) < 0.85 * speed * lastSpeed)
				continue;

			outEntities.Insert(entity);
			outVelocities.Insert(velocity);
		}

		m_mLastThreatPos = seen;
		m_mLastThreatVel = seenVel;
		m_aQuery.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyThreat(BFA_MechRocketLauncherModuleComponent launcher, BFA_MechRocketInFlight rocket, IEntity threatEntity, vector position)
	{
		BroadcastInterceptVisual(position);

		if (launcher && rocket)
		{
			launcher.ServerInterceptRocket(rocket);
			return;
		}

		if (threatEntity)
		{
			// Server copy is authoritative for damage. Some vanilla projectiles are simulated
			// locally on clients too; those may still be seen briefly but cannot deal damage.
			m_mLastThreatPos.Remove(threatEntity);
			m_mLastThreatVel.Remove(threatEntity);
			SCR_EntityHelper.DeleteEntityAndChildren(threatEntity);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BroadcastInterceptVisual(vector targetPos)
	{
		if (!m_pMechOwner)
			return;

		BFA_MechWeaponComponent router = BFA_MechWeaponComponent.Cast(m_pMechOwner.FindComponent(BFA_MechWeaponComponent));
		if (!router)
			return;

		vector from = GetOwner().GetOrigin();
		BroadcastShot(from, targetPos, false, 800, Math.Max(vector.Distance(from, targetPos), 1.0), m_fBeamThickness);

		ResourceName particle = GetImpactParticle();
		if (particle.IsEmpty())
			particle = m_sInterceptParticle;

		if (!particle.IsEmpty())
			router.ServerBroadcastModuleImpact(particle, targetPos, 0, m_iHardpointIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! MANUAL: one charge destroys every threat inside the burst radius right now.
	protected void FireManualBurst()
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint)
			return;

		if (m_iCharges <= 0)
		{
			if (m_bDebug)
				Print("[BFA POINT DEFENSE] MANUAL burst - no charges");
			return;
		}

		m_iCharges--;
		BroadcastModuleEvent(BFA_EMechModuleEvent.CHARGES, m_iCharges);

		vector center = GetDefenseCenter();
		float radius = Math.Max(m_fBurstRadius, 1.0);
		float radiusSq = radius * radius;
		int destroyed = 0;

		array<BFA_MechRocketLauncherModuleComponent> launchers = {};
		BFA_MechRocketLauncherModuleComponent.GetActiveLaunchers(launchers);
		foreach (BFA_MechRocketLauncherModuleComponent launcher : launchers)
		{
			for (int r = launcher.GetRocketCount() - 1; r >= 0; r--)
			{
				BFA_MechRocketInFlight rocket = launcher.GetRocketAt(r);
				if (!rocket || rocket.m_pMechOwner == m_pMechOwner)
					continue;

				if (vector.DistanceSq(rocket.m_vPosition, center) > radiusSq)
					continue;

				DestroyThreat(launcher, rocket, null, rocket.m_vPosition);
				destroyed++;
			}
		}

		if (m_bInterceptVanilla && !m_aThreatTypes.IsEmpty())
		{
			BaseWorld world = GetGame().GetWorld();
			if (world)
			{
				m_aQuery.Clear();
				world.QueryEntitiesBySphere(center, radius, CollectQueryEntity, null, EQueryEntitiesFlags.ALL);

				array<IEntity> targets = {};
				foreach (IEntity entity : m_aQuery)
				{
					if (IsVanillaThreatEntity(entity))
						targets.Insert(entity);
				}
				m_aQuery.Clear();

				foreach (IEntity target : targets)
				{
					if (!target)
						continue;

					DestroyThreat(null, null, target, target.GetOrigin());
					destroyed++;
				}
			}
		}

		if (destroyed == 0)
			BroadcastInterceptVisual(center + vector.Up * 3.0); // show the burst even when nothing was there

		if (m_bDebug)
			Print(string.Format("[BFA POINT DEFENSE] MANUAL burst destroyed=%1 charges=%2", destroyed, m_iCharges));
	}

	//------------------------------------------------------------------------------------------------
	// Routing: the aux/pulse key fires the burst; it also works if the station selects this group.
	override protected void OnFirePulse(IEntity instigator)
	{
		FireManualBurst();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerDown(IEntity instigator)
	{
		FireManualBurst();
	}

	//------------------------------------------------------------------------------------------------
	//! Clients keep the charge count for HUDs.
	override void ClientOnModuleEvent(int eventType, int value = 0)
	{
		if (eventType == BFA_EMechModuleEvent.CHARGES && !Replication.IsServer())
			m_iCharges = value;

		super.ClientOnModuleEvent(eventType, value);
	}
}
