//------------------------------------------------------------------------------------------------
//! CGS FORCE PULSE - a "stomp" shockwave around the mech's feet.
//!  - Fires on its own when enough HOSTILE vehicles / soldiers are inside Trigger Radius
//!    (no gunner, no aiming), after an optional charge-up, then waits Cooldown seconds.
//!  - Everything inside Push Radius is thrown away from the centre and up: strongest at the
//!    centre, fading to nothing at the edge. Vehicles fly; soldiers on foot are knocked down
//!    (ragdoll). Optional damage on top.
//!  - Multiplayer: each machine pushes the things IT simulates (the driver's PC moves a driven
//!    car, the server moves AI vehicles and AI soldiers), so pushes never snap back.
//!  - Heat, ammo and "destroyed" rules work like every MechCore weapon; a dead mech never pulses.
//! Put it on a weapon part prefab; install it on a hardpoint whose Point sits under the mech
//! (the pulse centre is the part's position). Give it its own weapon group (e.g. 8).
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "CGS Force Pulse: automatic stomp shockwave that throws vehicles and knocks down infantry around the mech's feet.")]
class CGS_ForcePulseModuleComponentClass : BFA_MechWeaponModuleComponentClass
{
}

class CGS_ForcePulseModuleComponent : BFA_MechWeaponModuleComponent
{
	static const int EVENT_PULSE_CHARGE = 60;
	static const int EVENT_PULSE = 61;

	// ---- Trigger -----------------------------------------------------------------------------
	[Attribute("25", UIWidgets.Auto, "Pulse fires when hostiles are within this many metres (horizontal) of the centre.", category: "Trigger")]
	protected float m_fTriggerRadius;

	[Attribute("1", UIWidgets.Auto, "How many hostiles (vehicles or soldiers on foot) must be inside Trigger Radius.", category: "Trigger")]
	protected int m_iMinTargets;

	[Attribute("40", UIWidgets.Auto, "Only things at most this many metres above / below the centre count (ignores aircraft).", category: "Trigger")]
	protected float m_fMaxHeight;

	[Attribute("8", UIWidgets.Auto, "Seconds between pulses.", category: "Trigger")]
	protected float m_fCooldown;

	[Attribute("1", UIWidgets.Auto, "Charge-up in seconds before the pulse (warning effect + sound). 0 = instant.", category: "Trigger")]
	protected float m_fChargeTime;

	[Attribute("true", UIWidgets.CheckBox, "Only pulse while someone (player or AI pilot) crews the mech. Off = always active.", category: "Trigger")]
	protected bool m_bRequiresCrew;

	// ---- Push --------------------------------------------------------------------------------
	[Attribute("30", UIWidgets.Auto, "Everything inside this radius (metres) is pushed. Falls off to 0 at the edge.", category: "Push")]
	protected float m_fPushRadius;

	[Attribute("18", UIWidgets.Auto, "Sideways speed added to a vehicle at the centre (m/s).", category: "Push")]
	protected float m_fPushSpeed;

	[Attribute("8", UIWidgets.Auto, "Upward speed added at the centre (m/s).", category: "Push")]
	protected float m_fLiftSpeed;

	[Attribute("1.5", UIWidgets.Auto, "Random spin given to vehicles (radians/s) - flips them.", category: "Push")]
	protected float m_fSpin;

	[Attribute("true", UIWidgets.CheckBox, "Knock soldiers on foot down (ragdoll).", category: "Push")]
	protected bool m_bKnockDownInfantry;

	[Attribute("false", UIWidgets.CheckBox, "Also push friendly vehicles and soldiers (off = only hostiles and empty vehicles).", category: "Push")]
	protected bool m_bPushFriendlies;

	// ---- Damage ------------------------------------------------------------------------------
	[Attribute("150", UIWidgets.Auto, "Explosive damage to vehicles at the centre (falls off to 0 at the edge). 0 = none.", category: "Damage")]
	protected float m_fVehicleDamage;

	[Attribute("25", UIWidgets.Auto, "Damage to soldiers on foot at the centre. 0 = none.", category: "Damage")]
	protected float m_fInfantryDamage;

	// ---- Effects -----------------------------------------------------------------------------
	[Attribute("{DB460C99707F948E}Particles/Weapon/Explosion_TM62M_AT_Mine.ptc", UIWidgets.ResourcePickerThumbnail, "Particle at the centre when the pulse goes off.", "ptc", category: "Effects")]
	protected ResourceName m_sPulseParticle;

	[Attribute("{2E8B5767FDBED4A1}Particles/Logistics/Explosion/Ammo_Rack/Explosion_Ammo_Rack_Medium.ptc", UIWidgets.ResourcePickerThumbnail, "Particle repeated in a ring (the shockwave).", "ptc", category: "Effects")]
	protected ResourceName m_sRingParticle;

	[Attribute("8", UIWidgets.Auto, "How many ring particles.", category: "Effects")]
	protected int m_iRingCount;

	[Attribute("0.6", UIWidgets.Slider, "Ring size as a share of Push Radius.", "0.1 1 0.05", category: "Effects")]
	protected float m_fRingScale;

	[Attribute("", UIWidgets.EditBox, "Charge-up sound event (in Effects > Sound Project). Empty = silent.", category: "Effects")]
	protected string m_sChargeSoundEvent;

	[Attribute("", UIWidgets.EditBox, "Pulse sound event (in Effects > Sound Project). Empty = the Fire Sound Event.", category: "Effects")]
	protected string m_sPulseSoundEvent;

	// ---- Diagnostics -------------------------------------------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Print triggers, pulses and what was pushed.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime -----------------------------------------------------------------------------
	protected static ref array<CGS_ForcePulseModuleComponent> s_aAll = {};
	protected static int s_iHeartbeatMs;
	protected static ref array<IEntity> s_aCollect = {};

	protected float m_fReadyAt;
	protected float m_fNextCheck;
	protected bool m_bCharging;
	protected int m_iToken;
	protected IEntity m_pInstigator;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sModuleId == "WEAPON_MODULE")
			m_sModuleId = "FORCE_PULSE";

		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);
		EnsureTick();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override float GetAimRange()
	{
		return m_fPushRadius;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTick()
	{
		if (s_iHeartbeatMs != 0 && System.GetTickCount() - s_iHeartbeatMs < 1000)
			return;
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().Remove(StaticTick);
		GetGame().GetCallqueue().CallLater(StaticTick, 100, true);
		s_iHeartbeatMs = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StaticTick()
	{
		s_iHeartbeatMs = System.GetTickCount();
		if (!Replication.IsServer())
			return;

		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			CGS_ForcePulseModuleComponent pulse = s_aAll[i];
			if (!pulse || !pulse.GetOwner())
			{
				s_aAll.Remove(i);
				continue;
			}
			pulse.ServerTick();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Watch for hostiles under the feet.
	protected void ServerTick()
	{
		if (!m_bBoundToHardpoint || !m_pMechOwner || m_bCharging)
			return;

		float now = Now();
		if (now < m_fReadyAt || now < m_fNextCheck)
			return;
		m_fNextCheck = now + 0.25;

		IEntity instigator = BFA_MechCoreComponent.BFA_ResolveKiller(m_pMechOwner, null);
		if (m_bRequiresCrew && !ChimeraCharacter.Cast(instigator))
			return;

		int hostiles = CountHostiles();
		if (hostiles < Math.Max(m_iMinTargets, 1))
			return;

		// Ammo / heat / destroyed checks, then FireShot() starts the charge.
		m_pInstigator = instigator;
		if (TryFireShot(instigator) && m_bDebug)
			Print(string.Format("[CGS FORCE PULSE] %1 triggered by %2 hostile(s)", m_sHardpointName, hostiles));
	}

	//------------------------------------------------------------------------------------------------
	override protected void FireShot(IEntity instigator)
	{
		m_iToken++;
		m_fReadyAt = Now() + Math.Max(m_fCooldown, 0.5);

		if (m_fChargeTime <= 0)
		{
			Pulse(m_iToken);
			return;
		}

		m_bCharging = true;
		BroadcastModuleEvent(EVENT_PULSE_CHARGE);
		GetGame().GetCallqueue().CallLater(Pulse, (int)(m_fChargeTime * 1000), false, m_iToken);
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] The pulse: damage, push what the server simulates, tell every machine.
	protected void Pulse(int token)
	{
		if (token != m_iToken)
			return;
		m_bCharging = false;

		if (!m_bBoundToHardpoint || !m_pMechOwner || BFA_MechExtension.IsMechDestroyed(m_pMechOwner))
			return;

		ServerDamage();
		int pushed = ApplyPush(); // things the server simulates (AI vehicles, AI soldiers)
		BroadcastModuleEvent(EVENT_PULSE);

		if (m_bDebug)
			Print(string.Format("[CGS FORCE PULSE] %1 PULSE at %2, server pushed %3", m_sHardpointName, GetCentre(), pushed));
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: effects, and the push for things this machine simulates.
	override void ClientOnModuleEvent(int eventType, int value = 0)
	{
		super.ClientOnModuleEvent(eventType, value);

		if (eventType == EVENT_PULSE_CHARGE)
		{
			if (!m_sChargeSoundEvent.IsEmpty())
				PlayModuleSound(m_sChargeSoundEvent);
			return;
		}

		if (eventType != EVENT_PULSE)
			return;

		if (!Replication.IsServer())
			ApplyPush(); // e.g. the car this player is driving

		PlayPulseEffects();
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetCentre()
	{
		return GetOwner().GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	protected Faction GetMechFaction()
	{
		if (!m_pMechOwner)
			return null;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(m_pMechOwner.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return null;
		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			faction = affiliation.GetDefaultAffiliatedFaction();
		return faction;
	}

	//------------------------------------------------------------------------------------------------
	//! Hostile = enemy faction. Empty vehicles count as hostile only for the push (they're in the way).
	protected bool IsHostile(IEntity entity, Faction mechFaction)
	{
		if (!entity || !mechFaction)
			return false;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;
		Faction faction = affiliation.GetAffiliatedFaction();
		return faction && mechFaction.IsFactionEnemy(faction);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsFriendly(IEntity entity, Faction mechFaction)
	{
		if (!entity || !mechFaction)
			return false;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;
		Faction faction = affiliation.GetAffiliatedFaction();
		return faction && !mechFaction.IsFactionEnemy(faction);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAliveEntity(IEntity entity)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		return !damage || damage.GetState() != EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicles and soldiers on foot (not crew inside vehicles, not the mech itself) in a radius.
	protected void CollectTargets(float radius, notnull array<IEntity> outTargets)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		vector centre = GetCentre();
		s_aCollect.Clear();
		world.QueryEntitiesBySphere(centre, Math.Max(radius, m_fMaxHeight), CollectEntity, null, EQueryEntitiesFlags.DYNAMIC);

		foreach (IEntity entity : s_aCollect)
		{
			if (!entity || outTargets.Contains(entity))
				continue;
			if (entity == m_pMechOwner || entity.GetRootParent() == m_pMechOwner)
				continue;

			bool isVehicle = Vehicle.Cast(entity) != null;
			bool isCharacter = ChimeraCharacter.Cast(entity) != null;
			if (!isVehicle && !isCharacter)
				continue;
			if (isCharacter && entity.GetParent())
				continue; // inside a vehicle: the vehicle gets pushed

			vector pos = entity.GetOrigin();
			if (Math.AbsFloat(pos[1] - centre[1]) > m_fMaxHeight)
				continue;
			if (vector.DistanceXZ(pos, centre) > radius)
				continue;

			outTargets.Insert(entity);
		}
		s_aCollect.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectEntity(IEntity entity)
	{
		if (entity)
			s_aCollect.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountHostiles()
	{
		Faction mechFaction = GetMechFaction();
		array<IEntity> targets = {};
		CollectTargets(m_fTriggerRadius, targets);

		int count = 0;
		foreach (IEntity target : targets)
		{
			if (IsAliveEntity(target) && IsHostile(target, mechFaction))
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected float Falloff(IEntity entity)
	{
		float d = vector.DistanceXZ(entity.GetOrigin(), GetCentre());
		return Math.Clamp(1.0 - d / Math.Max(m_fPushRadius, 0.1), 0.0, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	protected vector AwayDirection(IEntity entity)
	{
		vector away = entity.GetOrigin() - GetCentre();
		away[1] = 0;
		if (away.LengthSq() < 0.01)
			away = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1));
		return away.Normalized();
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Damage (once, authoritative).
	protected void ServerDamage()
	{
		if (m_fVehicleDamage <= 0 && m_fInfantryDamage <= 0)
			return;

		Faction mechFaction = GetMechFaction();
		array<IEntity> targets = {};
		CollectTargets(m_fPushRadius, targets);

		foreach (IEntity target : targets)
		{
			if (!m_bPushFriendlies && IsFriendly(target, mechFaction))
				continue;

			float falloff = Falloff(target);
			float amount = m_fVehicleDamage;
			if (ChimeraCharacter.Cast(target))
				amount = m_fInfantryDamage;
			amount *= falloff;
			if (amount <= 0)
				continue;

			DealDamage(target, amount, EDamageType.EXPLOSIVE, target.GetOrigin(), AwayDirection(target), m_pInstigator);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Does THIS machine simulate this entity? (server: AI / unowned; client: what it drives / is)
	protected bool SimulatesHere(IEntity entity)
	{
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl)
			return Replication.IsServer();
		return rpl.IsOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Push what this machine simulates. Returns how many things were pushed.
	protected int ApplyPush()
	{
		Faction mechFaction = GetMechFaction();
		array<IEntity> targets = {};
		CollectTargets(m_fPushRadius, targets);

		int pushed = 0;
		foreach (IEntity target : targets)
		{
			if (!SimulatesHere(target))
				continue;
			if (!m_bPushFriendlies && IsFriendly(target, mechFaction))
				continue;

			float falloff = Falloff(target);
			if (falloff <= 0)
				continue;

			vector away = AwayDirection(target);

			ChimeraCharacter character = ChimeraCharacter.Cast(target);
			if (character)
			{
				if (!m_bKnockDownInfantry)
					continue;

				CharacterControllerComponent controller = character.GetCharacterController();
				if (controller)
					controller.Ragdoll();

				// Throw the ragdoll a little once it is active (best effort).
				vector throwVelocity = away * (m_fPushSpeed * 0.5 * falloff) + vector.Up * (m_fLiftSpeed * 0.5 * falloff);
				GetGame().GetCallqueue().CallLater(ThrowBody, 60, false, character, throwVelocity);
				pushed++;
				continue;
			}

			Physics physics = target.GetPhysics();
			if (!physics || !physics.IsDynamic())
				continue;

			vector velocity = physics.GetVelocity() + away * (m_fPushSpeed * falloff) + vector.Up * (m_fLiftSpeed * falloff);
			physics.SetVelocity(velocity);

			if (m_fSpin > 0)
			{
				vector spin = Vector(Math.RandomFloat(-1, 1), Math.RandomFloat(-0.3, 0.3), Math.RandomFloat(-1, 1)) * (m_fSpin * falloff);
				physics.SetAngularVelocity(physics.GetAngularVelocity() + spin);
			}
			pushed++;
		}

		if (m_bDebug && !Replication.IsServer() && pushed > 0)
			Print(string.Format("[CGS FORCE PULSE] pushed %1 local object(s)", pushed));
		return pushed;
	}

	//------------------------------------------------------------------------------------------------
	protected void ThrowBody(IEntity body, vector velocity)
	{
		if (!body)
			return;
		Physics physics = body.GetPhysics();
		if (physics)
			physics.SetVelocity(physics.GetVelocity() + velocity);
	}

	//------------------------------------------------------------------------------------------------
	//! Local effects: centre blast, shockwave ring, sound.
	protected void PlayPulseEffects()
	{
		if (System.IsConsoleApp())
			return;

		vector centre = GetCentre();
		BaseWorld world = GetGame().GetWorld();
		if (world)
			centre[1] = world.GetSurfaceY(centre[0], centre[2]); // effects on the ground
		SpawnLocalParticle(m_sPulseParticle, centre);

		int ring = Math.Max(m_iRingCount, 0);
		float ringRadius = m_fPushRadius * Math.Clamp(m_fRingScale, 0.1, 1.0);
		for (int i = 0; i < ring; i++)
		{
			float angle = Math.PI2 * i / ring;
			vector pos = centre + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * ringRadius;
			if (world)
				pos[1] = world.GetSurfaceY(pos[0], pos[2]);
			SpawnLocalParticle(m_sRingParticle, pos);
		}

		string sound = m_sPulseSoundEvent;
		if (sound.IsEmpty() && m_FX)
			sound = m_FX.m_sFireSoundEvent;
		if (!sound.IsEmpty())
			PlayModuleSound(sound);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnLocalParticle(ResourceName particle, vector position)
	{
		if (particle.IsEmpty())
			return;
		ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
		Math3D.MatrixIdentity4(spawnParams.Transform);
		spawnParams.Transform[3] = position;
		ParticleEffectEntity.SpawnParticleEffect(particle, spawnParams);
	}

	//------------------------------------------------------------------------------------------------
	override void UnbindFromHardpoint()
	{
		m_iToken++;
		m_bCharging = false;
		super.UnbindFromHardpoint();
	}
}
