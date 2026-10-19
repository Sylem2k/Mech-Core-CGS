[ComponentEditorProps(category: "GameScripted/Mech/Weapons", description: "Swappable mech Rocket Launcher module. Server-simulated rockets with travel time, direct-hit + splash explosive damage, magazine and reload.")]
class BFA_MechRocketLauncherModuleComponentClass : BFA_MechWeaponModuleComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One rocket in flight. Server only.
class BFA_MechRocketInFlight
{
	vector m_vPosition;
	vector m_vDirection;
	float m_fTravelled;
	IEntity m_pInstigator;
	IEntity m_pMechOwner;
	int m_iShotId;   // visual projectile on clients, ended by the impact broadcast
}

//------------------------------------------------------------------------------------------------
//! Module ID 2 test weapon.
//! - One trigger pull = one salvo (no automatic fire while held).
//! - Rockets are simulated on the server in straight flight with real travel time,
//!   swept with a trace every tick, so fast targets can actually dodge.
//! - Visuals reuse the router's replicated tracer (same speed as the rocket) plus a
//!   replicated impact particle, so this module adds no new replicated state of its own.
class BFA_MechRocketLauncherModuleComponent : BFA_MechWeaponModuleComponent
{
	// ---- Flight ----------------------------------------------------------------------------
	[Attribute("120", UIWidgets.Auto, "Rocket speed in m/s.", category: "Rocket")]
	protected float m_fRocketSpeed;

	[Attribute("800", UIWidgets.Auto, "Maximum flight distance in metres. The rocket self-detonates at this range.", category: "Rocket")]
	protected float m_fRange;

	[Attribute("1", UIWidgets.Auto, "Rockets per trigger pull.", category: "Rocket")]
	protected int m_iSalvoCount;

	[Attribute("150", UIWidgets.Auto, "Delay between rockets in one salvo, in milliseconds.", category: "Rocket")]
	protected int m_iSalvoIntervalMs;

	[Attribute("1000", UIWidgets.Auto, "Minimum time between trigger pulls, in milliseconds.", category: "Rocket")]
	protected int m_iRefireMs;

	[Attribute("0.0", UIWidgets.Auto, "Random spread cone half-angle in degrees.", category: "Rocket")]
	protected float m_fSpreadDeg;

	// Magazine / reload: base module "Ammo" category (m_iMagazineSize, m_fReloadTime, spares).

	// ---- Damage ----------------------------------------------------------------------------
	[Attribute("400", UIWidgets.Auto, "Explosive damage applied to the entity the rocket directly hits.", category: "Damage")]
	protected float m_fDirectHitDamage;

	[Attribute("150", UIWidgets.Auto, "Explosive splash damage at the centre of the blast.", category: "Damage")]
	protected float m_fSplashDamage;

	[Attribute("6.0", UIWidgets.Auto, "Splash radius in metres. Damage falls off linearly to zero at this distance.", category: "Damage")]
	protected float m_fSplashRadius;

	[Attribute("false", UIWidgets.CheckBox, "Allow splash damage to hurt the firing mech.", category: "Damage")]
	protected bool m_bSplashHurtsSelf;

	// ---- Visuals ---------------------------------------------------------------------------
	[Attribute("6", UIWidgets.Auto, "Visible rocket streak length in metres.", category: "Visuals")]
	protected float m_fTrailLength;

	[Attribute("0.25", UIWidgets.Auto, "Rocket streak thickness.", category: "Visuals")]
	protected float m_fTrailThickness;

	[Attribute("{2E8B5767FDBED4A1}Particles/Logistics/Explosion/Ammo_Rack/Explosion_Ammo_Rack_Medium.ptc", UIWidgets.ResourceNamePicker, "Impact particle effect (.ptc). Visual only - damage is applied by this script.", params: "ptc", category: "Visuals")]
	protected ResourceName m_sImpactParticle;

	// ---- Diagnostics -----------------------------------------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Print rocket launch/impact/damage diagnostics.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime (server) ------------------------------------------------------------------
	protected ref array<ref BFA_MechRocketInFlight> m_aRockets = {};

	// Server registry of every launcher, so defensive modules (anti-missile) on any mech can find
	// rockets in flight without scanning the whole world.
	protected static ref array<BFA_MechRocketLauncherModuleComponent> s_aActiveLaunchers = {};
	protected ref array<IEntity> m_aSplashQuery = {};
	protected bool m_bFlightTickRunning;
	protected int m_iLastFlightTickMs;

	protected int m_iNextTriggerTimeMs;

	protected int m_iSalvoRemaining;
	protected IEntity m_pSalvoInstigator;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_aRockets.Clear();
		m_bFlightTickRunning = false;
		m_iLastFlightTickMs = 0;
		m_iNextTriggerTimeMs = 0;
		m_iSalvoRemaining = 0;
		m_pSalvoInstigator = null;

		if (Replication.IsServer() && !s_aActiveLaunchers.Contains(this))
			s_aActiveLaunchers.Insert(this);

		BFA_MechLog.Log("[BFA ROCKET MODULE] initialized");
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(FlightTick);
				queue.Remove(SalvoTick);
			}
		}

		m_aRockets.Clear();
		m_bFlightTickRunning = false;
		s_aActiveLaunchers.RemoveItem(this);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Unbinding (module swapped out) cancels any salvo still queued. Rockets already in
	//! flight keep flying and still credit the original instigator.
	override void UnbindFromHardpoint()
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
				queue.Remove(SalvoTick);
		}

		m_iSalvoRemaining = 0;
		m_pSalvoInstigator = null;

		super.UnbindFromHardpoint();
	}

	//------------------------------------------------------------------------------------------------
	//! Legacy m_sImpactParticle still works when the Effects impact particle is empty.
	override ResourceName GetImpactParticle()
	{
		ResourceName fxParticle = super.GetImpactParticle();
		if (!fxParticle.IsEmpty())
			return fxParticle;

		return m_sImpactParticle;
	}

	//------------------------------------------------------------------------------------------------
	// Interception API (server). Used by anti-missile / point-defense modules.
	static void GetActiveLaunchers(notnull array<BFA_MechRocketLauncherModuleComponent> outLaunchers)
	{
		outLaunchers.Clear();
		for (int i = s_aActiveLaunchers.Count() - 1; i >= 0; i--)
		{
			if (!s_aActiveLaunchers[i])
			{
				s_aActiveLaunchers.Remove(i);
				continue;
			}

			outLaunchers.Insert(s_aActiveLaunchers[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	int GetRocketCount()
	{
		return m_aRockets.Count();
	}

	//------------------------------------------------------------------------------------------------
	BFA_MechRocketInFlight GetRocketAt(int index)
	{
		if (index < 0 || index >= m_aRockets.Count())
			return null;

		return m_aRockets[index];
	}

	//------------------------------------------------------------------------------------------------
	float GetRocketSpeed()
	{
		return Math.Max(m_fRocketSpeed, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	override float GetLeadSpeed()
	{
		if (m_fHudLeadSpeed > 0)
			return m_fHudLeadSpeed;
		return GetRocketSpeed();
	}

	//------------------------------------------------------------------------------------------------
	//! Destroy a rocket mid-air. No damage is applied; a small effect plays where it died.
	bool ServerInterceptRocket(BFA_MechRocketInFlight rocket)
	{
		if (!Replication.IsServer() || !rocket)
			return false;

		int index = m_aRockets.Find(rocket);
		if (index < 0)
			return false;

		vector position = rocket.m_vPosition;
		m_aRockets.Remove(index);
		BroadcastImpact(position, rocket.m_pMechOwner, rocket.m_iShotId);

		if (m_bDebug)
			Print(string.Format("[BFA ROCKET MODULE] INTERCEPTED at %1 travelled=%2", position, rocket.m_fTravelled));

		return true;
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
	// Trigger handling: every pull (held route) or pulse fires one salvo.
	override protected void OnTriggerDown(IEntity instigator)
	{
		TryStartSalvo(instigator);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnFirePulse(IEntity instigator)
	{
		TryStartSalvo(instigator);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryStartSalvo(IEntity instigator)
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint)
			return;

		int nowMs = GetWorldTimeMs();
		if (nowMs < m_iNextTriggerTimeMs || m_iSalvoRemaining > 0)
			return;

		if (m_bReloading)
		{
			if (m_bDebug)
				Print("[BFA ROCKET MODULE] trigger ignored - reloading");
			return;
		}

		m_iNextTriggerTimeMs = nowMs + Math.Max(m_iRefireMs, 50);
		m_iSalvoRemaining = Math.Max(m_iSalvoCount, 1);
		m_pSalvoInstigator = instigator;

		SalvoTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void SalvoTick()
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint || m_iSalvoRemaining <= 0)
		{
			m_iSalvoRemaining = 0;
			return;
		}

		if (!ConsumeRound())
		{
			m_iSalvoRemaining = 0;
			return;
		}

		LaunchRocket(m_pSalvoInstigator);
		m_iSalvoRemaining--;

		if (m_iSalvoRemaining > 0)
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
				queue.CallLater(SalvoTick, Math.Max(m_iSalvoIntervalMs, 20), false);
			else
				m_iSalvoRemaining = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnReloadStarted()
	{
		if (m_bDebug)
			Print(string.Format("[BFA ROCKET MODULE] reloading %1s hardpoint=%2", m_fReloadTime, m_sHardpointName));
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnReloadFinished()
	{
		if (m_bDebug)
			Print(string.Format("[BFA ROCKET MODULE] reloaded rounds=%1 hardpoint=%2", m_iRoundsInMagazine, m_sHardpointName));
	}



	//------------------------------------------------------------------------------------------------
	//! Straight-line trace that skips the firing mech and all of its children.
	//! Returns the fraction hit (1.0 = clear).
	protected float TraceSegment(vector from, vector to, IEntity mechOwner, out IEntity hitEntity, out vector hitPos)
	{
		hitEntity = null;
		hitPos = to;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 1.0;

		vector dir = to - from;
		float totalLength = dir.Length();
		if (totalLength < 0.0001)
			return 1.0;

		dir = dir * (1.0 / totalLength);
		vector start = from;

		for (int i = 0; i < 8; i++)
		{
			TraceParam param = new TraceParam();
			param.Start = start;
			param.End = to;
			param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			param.LayerMask = EPhysicsLayerPresets.Projectile;

			array<IEntity> excludes = {};
			if (mechOwner)
				excludes.Insert(mechOwner);

			IEntity moduleEntity = GetOwner();
			if (moduleEntity)
				excludes.Insert(moduleEntity);

			param.ExcludeArray = excludes;

			float fraction = world.TraceMove(param, null);
			if (fraction >= 1.0)
				return 1.0;

			vector hit = param.Start + (param.End - param.Start) * fraction;
			IEntity traced = param.TraceEnt;

			if (traced && mechOwner && traced.GetRootParent() == mechOwner)
			{
				start = hit + dir * 0.25;
				continue;
			}

			hitEntity = traced;
			hitPos = hit;
			return vector.Distance(from, hit) / totalLength;
		}

		return 1.0;
	}

	//------------------------------------------------------------------------------------------------
	protected void LaunchRocket(IEntity instigator)
	{
		vector direction;
		vector start = GetMuzzle(direction);
		direction = ApplyConeSpread(direction, m_fSpreadDeg);

		BFA_MechRocketInFlight rocket = new BFA_MechRocketInFlight();
		rocket.m_vPosition = start;
		rocket.m_vDirection = direction;
		rocket.m_fTravelled = 0;
		rocket.m_pInstigator = instigator;
		rocket.m_pMechOwner = m_pMechOwner;
		m_aRockets.Insert(rocket);

		// Visual: predict the straight-line end point now and replicate one tracer moving at
		// the rocket's real speed. Damage still comes from the server simulation below.
		IEntity predictedHit;
		vector predictedEnd;
		TraceSegment(start, start + direction * Math.Max(m_fRange, 10.0), m_pMechOwner, predictedHit, predictedEnd);
		rocket.m_iShotId = BroadcastShot(start, predictedEnd, predictedHit != null, m_fRocketSpeed, m_fTrailLength, m_fTrailThickness, true);

		if (m_bDebug)
			Print(string.Format("[BFA ROCKET MODULE] LAUNCH hardpoint=%1 rounds=%2 start=%3", m_sHardpointName, m_iRoundsInMagazine, start));

		StartFlightTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartFlightTick()
	{
		if (m_bFlightTickRunning)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_bFlightTickRunning = true;
		m_iLastFlightTickMs = GetWorldTimeMs();
		queue.CallLater(FlightTick, 16, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopFlightTick()
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(FlightTick);

		m_bFlightTickRunning = false;
		m_iLastFlightTickMs = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void FlightTick()
	{
		if (!Replication.IsServer() || m_aRockets.IsEmpty())
		{
			m_aRockets.Clear();
			StopFlightTick();
			return;
		}

		int nowMs = GetWorldTimeMs();
		float dt = Math.Clamp((nowMs - m_iLastFlightTickMs) * 0.001, 0.001, 0.1);
		m_iLastFlightTickMs = nowMs;

		float maxRange = Math.Max(m_fRange, 10.0);
		float speed = Math.Max(m_fRocketSpeed, 1.0);

		for (int i = m_aRockets.Count() - 1; i >= 0; i--)
		{
			BFA_MechRocketInFlight rocket = m_aRockets[i];
			if (!rocket)
			{
				m_aRockets.Remove(i);
				continue;
			}

			float step = Math.Min(speed * dt, maxRange - rocket.m_fTravelled);
			vector from = rocket.m_vPosition;
			vector to = from + rocket.m_vDirection * step;

			IEntity hitEntity;
			vector hitPos;
			float fraction = TraceSegment(from, to, rocket.m_pMechOwner, hitEntity, hitPos);

			if (fraction < 1.0)
			{
				Detonate(rocket, hitPos, hitEntity);
				m_aRockets.Remove(i);
				continue;
			}

			rocket.m_vPosition = to;
			rocket.m_fTravelled = rocket.m_fTravelled + step;

			if (rocket.m_fTravelled >= maxRange - 0.01)
			{
				Detonate(rocket, to, null);
				m_aRockets.Remove(i);
			}
		}

		if (m_aRockets.IsEmpty())
			StopFlightTick();
	}


	//------------------------------------------------------------------------------------------------
	protected float ApplyExplosiveDamage(SCR_DamageManagerComponent damageManager, float amount, vector position, vector direction, IEntity instigatorEntity, IEntity mechOwner)
	{
		if (!damageManager || amount <= 0.0)
			return 0;

		HitZone hitZone = damageManager.GetDefaultHitZone();
		if (!hitZone)
			return 0;

		float healthBefore = hitZone.GetHealth();

		vector hitTransform[3];
		hitTransform[0] = position;
		hitTransform[1] = direction;
		hitTransform[2] = direction * -1.0;

		instigatorEntity = BFA_MechCoreComponent.BFA_ResolveKiller(mechOwner, instigatorEntity);

		Instigator instigator = Instigator.CreateInstigator(instigatorEntity);

		SCR_DamageContext context = new SCR_DamageContext(
			EDamageType.EXPLOSIVE,
			amount,
			hitTransform,
			mechOwner,
			hitZone,
			instigator,
			null,
			-1,
			-1
		);

		BeginDamage();
		damageManager.HandleDamage(context);
		EndDamage();

		return healthBefore - hitZone.GetHealth();
	}

	//------------------------------------------------------------------------------------------------
	//! Splash only reaches objects the blast can "see": a trace from just above the blast point
	//! to the target part must not hit something else first (terrain, a wall, another vehicle).
	protected bool HasBlastLineOfSight(vector blastPos, IEntity part, IEntity targetRoot, IEntity mechOwner)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || !part)
			return true;

		vector from = blastPos + vector.Up * 0.25;
		vector to = part.GetOrigin();

		TraceParam param = new TraceParam();
		param.Start = from;
		param.End = to;
		param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		param.LayerMask = EPhysicsLayerPresets.Projectile;

		array<IEntity> excludes = {};
		if (mechOwner)
			excludes.Insert(mechOwner);
		param.ExcludeArray = excludes;

		float fraction = world.TraceMove(param, null);
		if (fraction >= 1.0)
			return true;

		IEntity blocker = param.TraceEnt;
		if (!blocker)
			return false; // terrain in the way

		IEntity blockerRoot = blocker.GetRootParent();
		if (!blockerRoot)
			blockerRoot = blocker;

		return blockerRoot == targetRoot;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectSplashEntity(IEntity entity)
	{
		if (entity)
			m_aSplashQuery.Insert(entity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Detonate(BFA_MechRocketInFlight rocket, vector position, IEntity directHit)
	{
		if (!Replication.IsServer() || !rocket)
			return;

		IEntity mechOwner = rocket.m_pMechOwner;

		// Damage managers already hit by this explosion. Prevents a vehicle with many child
		// parts from taking splash once per part.
		array<SCR_DamageManagerComponent> damaged = {};

		// 1) Direct hit.
		float directLost = 0;
		SCR_DamageManagerComponent directManager = ResolveDamageManager(directHit);
		if (directManager)
		{
			directLost = ApplyExplosiveDamage(directManager, m_fDirectHitDamage, position, rocket.m_vDirection, rocket.m_pInstigator, mechOwner);
			damaged.Insert(directManager);
		}

		// 2) Splash with linear falloff - ONCE per object (vehicle, soldier, building), not once
		// per part. A vehicle's wheels, windows and panels each carry their own damage manager;
		// hitting every one of them made a single rocket count ~40 splash targets. The root's
		// closest part decides the falloff, and the blast needs line of sight to that part.
		int splashTargets = 0;
		float radius = Math.Max(m_fSplashRadius, 0.0);
		BaseWorld world = GetGame().GetWorld();

		if (world && radius > 0.0 && m_fSplashDamage > 0.0)
		{
			m_aSplashQuery.Clear();
			world.QueryEntitiesBySphere(position, radius, CollectSplashEntity, null, EQueryEntitiesFlags.ALL);

			IEntity directRoot;
			if (directHit)
				directRoot = directHit.GetRootParent();

			// Closest part per root object.
			array<IEntity> roots = {};
			array<IEntity> closestParts = {};
			array<float> closestDistances = {};

			foreach (IEntity entity : m_aSplashQuery)
			{
				if (!entity)
					continue;

				IEntity root = entity.GetRootParent();
				if (!root)
					root = entity;

				if (!m_bSplashHurtsSelf && mechOwner && root == mechOwner)
					continue;

				// The directly hit object already took the direct-hit damage.
				if (directRoot && root == directRoot)
					continue;

				float distance = vector.Distance(position, entity.GetOrigin());
				int index = roots.Find(root);
				if (index < 0)
				{
					roots.Insert(root);
					closestParts.Insert(entity);
					closestDistances.Insert(distance);
				}
				else if (distance < closestDistances[index])
				{
					closestParts[index] = entity;
					closestDistances[index] = distance;
				}
			}

			m_aSplashQuery.Clear();

			for (int r = 0; r < roots.Count(); r++)
			{
				IEntity splashRoot = roots[r];
				IEntity part = closestParts[r];

				float falloff = Math.Clamp(1.0 - (closestDistances[r] / radius), 0.0, 1.0);
				if (falloff <= 0.0)
					continue;

				// Prefer the object's main damage manager (vehicle hull, character), else the part's.
				SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(splashRoot.FindComponent(SCR_DamageManagerComponent));
				if (!manager)
					manager = ResolveDamageManager(part);

				if (!manager || damaged.Contains(manager))
					continue;

				if (!HasBlastLineOfSight(position, part, splashRoot, mechOwner))
					continue;

				damaged.Insert(manager);

				vector toTarget = part.GetOrigin() - position;
				if (toTarget.LengthSq() > 0.0001)
					toTarget.Normalize();
				else
					toTarget = rocket.m_vDirection;

				ApplyExplosiveDamage(manager, m_fSplashDamage * falloff, position, toTarget, rocket.m_pInstigator, mechOwner);
				splashTargets++;
			}
		}

		BroadcastImpact(position, mechOwner, rocket.m_iShotId);

		if (m_bDebug)
		{
			Print(string.Format(
				"[BFA ROCKET MODULE] DETONATE pos=%1 direct=%2 directLost=%3 splashTargets=%4 travelled=%5",
				position,
				directHit,
				directLost,
				splashTargets,
				rocket.m_fTravelled
			));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected BFA_MechWeaponComponent GetRouter(IEntity mechOwner)
	{
		if (!mechOwner)
			return null;

		return BFA_MechWeaponComponent.Cast(mechOwner.FindComponent(BFA_MechWeaponComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected void BroadcastImpact(vector position, IEntity mechOwner, int shotId = 0)
	{
		BFA_MechWeaponComponent router = GetRouter(mechOwner);
		if (router)
			router.ServerBroadcastModuleImpact(GetImpactParticle(), position, shotId, m_iHardpointIndex);
	}

	//------------------------------------------------------------------------------------------------
	override float GetAimRange()
	{
		return m_fRange;
	}
}
