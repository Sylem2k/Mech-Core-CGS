//------------------------------------------------------------------------------------------------
//! CGS EMP PULSE - a rare, huge electromagnetic pulse.
//!  - Fires on its own when enough HOSTILE vehicles are inside Radius, after a long charge-up
//!    (warning effect + sound), then needs a very long Recharge Time before it can fire again.
//!  - For Effect Time, every vehicle caught in it:
//!     * ground vehicles: engine dies and can't be restarted,
//!     * helicopters: controls go haywire - random tilt and shoves (engines can be killed too),
//!     * optional engine damage that stays after the effect ends.
//!  - Multiplayer: each machine disturbs the vehicles IT simulates (the pilot's PC flies the
//!    helicopter), so the effect is felt even with lag.
//!  - Like every MechCore weapon: heat, ammo, a destroyed part (e.g. a generator the players
//!    must destroy) or a dead mech stop it.
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "CGS EMP Pulse: rare pulse that kills vehicle engines and throws helicopter controls into chaos for a while. Very long recharge.")]
class CGS_EmpPulseModuleComponentClass : BFA_MechWeaponModuleComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One vehicle under the EMP on this machine.
class CGS_EmpVictim
{
	IEntity m_pVehicle;
	float m_fUntil;
	float m_fNextKick;
	float m_fOriginalStartChance = -1;
	bool m_bHelicopter;
}

class CGS_EmpPulseModuleComponent : BFA_MechWeaponModuleComponent
{
	static const int EVENT_EMP_CHARGE = 70;
	static const int EVENT_EMP = 71;

	// ---- Trigger -----------------------------------------------------------------------------
	[Attribute("200", UIWidgets.Auto, "Pulse radius in metres (also the trigger radius).", category: "EMP")]
	protected float m_fRadius;

	[Attribute("2", UIWidgets.Auto, "How many hostile vehicles must be inside Radius before it fires.", category: "EMP")]
	protected int m_iMinTargets;

	[Attribute("300", UIWidgets.Auto, "Recharge time in seconds before it can fire again (very long by design).", category: "EMP")]
	protected float m_fRechargeTime;

	[Attribute("4", UIWidgets.Auto, "Charge-up in seconds (warning effect + sound) before the pulse.", category: "EMP")]
	protected float m_fChargeTime;

	[Attribute("25", UIWidgets.Auto, "How long the effect lasts on each vehicle, in seconds.", category: "EMP")]
	protected float m_fEffectTime;

	[Attribute("true", UIWidgets.CheckBox, "Only fire while someone (player or AI pilot) crews the mech.", category: "EMP")]
	protected bool m_bRequiresCrew;

	[Attribute("false", UIWidgets.CheckBox, "Also hit friendly vehicles (off = only hostile and empty vehicles).", category: "EMP")]
	protected bool m_bHitFriendlies;

	// ---- Ground vehicles ---------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "Kill the engines of ground vehicles and stop them restarting during the effect.", category: "Ground Vehicles")]
	protected bool m_bStopEngines;

	// ---- Helicopters -------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "Throw helicopter controls into chaos (random tilt and shoves).", category: "Helicopters")]
	protected bool m_bDisruptHelicopters;

	[Attribute("false", UIWidgets.CheckBox, "Also kill helicopter engines (they will fall).", category: "Helicopters")]
	protected bool m_bStopHelicopterEngines;

	[Attribute("0.9", UIWidgets.Auto, "Random tilt kick (radians/s) given to helicopters at each jolt.", category: "Helicopters")]
	protected float m_fTiltStrength;

	[Attribute("4", UIWidgets.Auto, "Random shove (m/s) given to helicopters at each jolt.", category: "Helicopters")]
	protected float m_fShoveStrength;

	[Attribute("0.6", UIWidgets.Auto, "Seconds between jolts (random +-50%).", category: "Helicopters")]
	protected float m_fJoltInterval;

	// ---- Damage ------------------------------------------------------------------------------
	[Attribute("0", UIWidgets.Auto, "Damage to each vehicle's engine hit zone (stays after the effect). 0 = none.", category: "Damage")]
	protected float m_fEngineDamage;

	[Attribute("Engine", UIWidgets.EditBox, "Name of the engine hit zone on vehicles (vanilla vehicles use names containing 'Engine').", category: "Damage")]
	protected string m_sEngineHitZone;

	// ---- Effects -----------------------------------------------------------------------------
	[Attribute("{DB460C99707F948E}Particles/Weapon/Explosion_TM62M_AT_Mine.ptc", UIWidgets.ResourcePickerThumbnail, "Particle at the mech when the pulse goes off.", "ptc", category: "Effects")]
	protected ResourceName m_sPulseParticle;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Particle at the mech during the charge-up. Empty = none.", "ptc", category: "Effects")]
	protected ResourceName m_sChargeParticle;

	[Attribute("{2AEA0DCC3EDDCCFB}Particles/Vehicle/Vehicle_smoke_damaged_low_01.ptc", UIWidgets.ResourcePickerThumbnail, "Particle on each vehicle hit (sparks / smoke). Empty = none.", "ptc", category: "Effects")]
	protected ResourceName m_sVictimParticle;

	[Attribute("", UIWidgets.EditBox, "Charge-up sound event (in Effects > Sound Project).", category: "Effects")]
	protected string m_sChargeSoundEvent;

	[Attribute("", UIWidgets.EditBox, "Pulse sound event (in Effects > Sound Project). Empty = the Fire Sound Event.", category: "Effects")]
	protected string m_sPulseSoundEvent;

	// ---- Diagnostics -------------------------------------------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Print triggers, pulses and affected vehicles.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime -----------------------------------------------------------------------------
	protected static ref array<CGS_EmpPulseModuleComponent> s_aAll = {};
	protected static int s_iHeartbeatMs;
	protected static ref array<IEntity> s_aCollect = {};

	protected float m_fReadyAt;
	protected float m_fNextCheck;
	protected bool m_bCharging;
	protected int m_iToken;
	protected IEntity m_pInstigator;
	protected ref array<ref CGS_EmpVictim> m_aVictims = {};

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sModuleId == "WEAPON_MODULE")
			m_sModuleId = "EMP_PULSE";

		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);
		EnsureTick();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aAll.RemoveItem(this);
		ReleaseAll();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override float GetAimRange()
	{
		return m_fRadius;
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
		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			CGS_EmpPulseModuleComponent emp = s_aAll[i];
			if (!emp || !emp.GetOwner())
			{
				s_aAll.Remove(i);
				continue;
			}
			if (Replication.IsServer())
				emp.ServerTick();
			emp.VictimTick();
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
	//! [Server] Wait for enough hostile vehicles, then charge.
	protected void ServerTick()
	{
		if (!m_bBoundToHardpoint || !m_pMechOwner || m_bCharging)
			return;

		float now = Now();
		if (now < m_fReadyAt || now < m_fNextCheck)
			return;
		m_fNextCheck = now + 0.5;

		IEntity instigator = BFA_MechCoreComponent.BFA_ResolveKiller(m_pMechOwner, null);
		if (m_bRequiresCrew && !ChimeraCharacter.Cast(instigator))
			return;

		array<IEntity> vehicles = {};
		CollectVehicles(vehicles, true);
		if (vehicles.Count() < Math.Max(m_iMinTargets, 1))
			return;

		m_pInstigator = instigator;
		if (TryFireShot(instigator) && m_bDebug)
			Print(string.Format("[CGS EMP] %1 charging - %2 hostile vehicle(s) in range", m_sHardpointName, vehicles.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override protected void FireShot(IEntity instigator)
	{
		m_iToken++;
		m_fReadyAt = Now() + Math.Max(m_fRechargeTime, 1);

		if (m_fChargeTime <= 0)
		{
			Pulse(m_iToken);
			return;
		}

		m_bCharging = true;
		BroadcastModuleEvent(EVENT_EMP_CHARGE);
		GetGame().GetCallqueue().CallLater(Pulse, (int)(m_fChargeTime * 1000), false, m_iToken);
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] The pulse.
	protected void Pulse(int token)
	{
		if (token != m_iToken)
			return;
		m_bCharging = false;

		if (!m_bBoundToHardpoint || !m_pMechOwner || BFA_MechExtension.IsMechDestroyed(m_pMechOwner))
			return;

		if (m_fEngineDamage > 0)
			DamageEngines();

		int hit = AffectLocalVehicles();
		BroadcastModuleEvent(EVENT_EMP);

		if (m_bDebug)
			Print(string.Format("[CGS EMP] %1 PULSE - %2 vehicle(s) hit; ready again in %3 s", m_sHardpointName, hit, m_fRechargeTime));
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: effects, and the disruption for vehicles this machine simulates.
	override void ClientOnModuleEvent(int eventType, int value = 0)
	{
		super.ClientOnModuleEvent(eventType, value);

		if (eventType == EVENT_EMP_CHARGE)
		{
			SpawnLocalParticle(m_sChargeParticle, GetOwner().GetOrigin());
			if (!m_sChargeSoundEvent.IsEmpty())
				PlayModuleSound(m_sChargeSoundEvent);
			return;
		}

		if (eventType != EVENT_EMP)
			return;

		if (!Replication.IsServer())
			AffectLocalVehicles();

		SpawnLocalParticle(m_sPulseParticle, GetOwner().GetOrigin());
		string sound = m_sPulseSoundEvent;
		if (sound.IsEmpty() && m_FX)
			sound = m_FX.m_sFireSoundEvent;
		if (!sound.IsEmpty())
			PlayModuleSound(sound);
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
	//! Vehicles in Radius (not the mech). hostileOnly: crewed enemies only (for the trigger);
	//! otherwise also empty vehicles, and friendlies when Hit Friendlies is on.
	protected void CollectVehicles(notnull array<IEntity> outVehicles, bool hostileOnly)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		Faction mechFaction = GetMechFaction();
		s_aCollect.Clear();
		world.QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fRadius, CollectEntity, null, EQueryEntitiesFlags.DYNAMIC);

		foreach (IEntity entity : s_aCollect)
		{
			if (!entity || !Vehicle.Cast(entity) || outVehicles.Contains(entity))
				continue;
			if (entity == m_pMechOwner || entity.GetRootParent() == m_pMechOwner)
				continue;

			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
			if (damage && damage.GetState() == EDamageState.DESTROYED)
				continue;

			Faction faction;
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
			if (affiliation)
				faction = affiliation.GetAffiliatedFaction();

			bool hostile = faction && mechFaction && mechFaction.IsFactionEnemy(faction);
			bool friendly = faction && mechFaction && !mechFaction.IsFactionEnemy(faction);

			if (hostileOnly && !hostile)
				continue;
			if (!hostileOnly && friendly && !m_bHitFriendlies)
				continue;

			outVehicles.Insert(entity);
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
	protected bool SimulatesHere(IEntity entity)
	{
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl)
			return Replication.IsServer();
		return rpl.IsOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Start the effect on every vehicle in range that this machine simulates (the server also
	//! handles engines of everything: engine state is authoritative there).
	protected int AffectLocalVehicles()
	{
		array<IEntity> vehicles = {};
		CollectVehicles(vehicles, false);

		float until = Now() + Math.Max(m_fEffectTime, 1);
		int count = 0;
		foreach (IEntity vehicle : vehicles)
		{
			if (!Replication.IsServer() && !SimulatesHere(vehicle))
				continue;

			CGS_EmpVictim victim = FindVictim(vehicle);
			if (!victim)
			{
				victim = new CGS_EmpVictim();
				victim.m_pVehicle = vehicle;
				victim.m_bHelicopter = vehicle.FindComponent(HelicopterControllerComponent) != null;
				m_aVictims.Insert(victim);
			}
			victim.m_fUntil = until;

			if (!System.IsConsoleApp())
				SpawnLocalParticle(m_sVictimParticle, vehicle.GetOrigin());

			if (m_bDebug)
				Print(string.Format("[CGS EMP] hit %1 (helicopter %2)", vehicle, victim.m_bHelicopter));
			count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected CGS_EmpVictim FindVictim(IEntity vehicle)
	{
		foreach (CGS_EmpVictim victim : m_aVictims)
		{
			if (victim && victim.m_pVehicle == vehicle)
				return victim;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldStopEngine(CGS_EmpVictim victim)
	{
		if (victim.m_bHelicopter)
			return m_bStopHelicopterEngines;
		return m_bStopEngines;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine, every 0.1 s: keep engines dead and jolt helicopters until the effect ends.
	protected void VictimTick()
	{
		if (m_aVictims.IsEmpty())
			return;

		float now = Now();
		for (int i = m_aVictims.Count() - 1; i >= 0; i--)
		{
			CGS_EmpVictim victim = m_aVictims[i];
			if (!victim || !victim.m_pVehicle)
			{
				m_aVictims.Remove(i);
				continue;
			}

			BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(victim.m_pVehicle.FindComponent(BaseVehicleControllerComponent));

			if (now >= victim.m_fUntil)
			{
				Release(victim, controller);
				m_aVictims.Remove(i);
				continue;
			}

			// Engine: dead and unable to start.
			if (controller && ShouldStopEngine(victim))
			{
				if (victim.m_fOriginalStartChance < 0)
				{
					victim.m_fOriginalStartChance = controller.GetEngineStartupChance();
					controller.SetEngineStartupChance(0);
				}
				if (controller.IsEngineOn())
					controller.StopEngine(false);
			}

			// Helicopter: random jolts (only where its physics runs).
			if (victim.m_bHelicopter && m_bDisruptHelicopters && now >= victim.m_fNextKick && SimulatesHere(victim.m_pVehicle))
			{
				victim.m_fNextKick = now + Math.Max(m_fJoltInterval, 0.1) * Math.RandomFloat(0.5, 1.5);
				Physics physics = victim.m_pVehicle.GetPhysics();
				if (physics && physics.IsDynamic())
				{
					vector tilt = Vector(Math.RandomFloat(-1, 1), Math.RandomFloat(-0.5, 0.5), Math.RandomFloat(-1, 1)) * m_fTiltStrength;
					physics.SetAngularVelocity(physics.GetAngularVelocity() + tilt);
					vector shove = Vector(Math.RandomFloat(-1, 1), Math.RandomFloat(-1, 0.4), Math.RandomFloat(-1, 1)) * m_fShoveStrength;
					physics.SetVelocity(physics.GetVelocity() + shove);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Effect over: engines may start again.
	protected void Release(CGS_EmpVictim victim, BaseVehicleControllerComponent controller)
	{
		if (controller && victim.m_fOriginalStartChance >= 0)
			controller.SetEngineStartupChance(victim.m_fOriginalStartChance);
		victim.m_fOriginalStartChance = -1;

		if (m_bDebug)
			Print(string.Format("[CGS EMP] %1 recovered", victim.m_pVehicle));
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseAll()
	{
		foreach (CGS_EmpVictim victim : m_aVictims)
		{
			if (!victim || !victim.m_pVehicle)
				continue;
			BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(victim.m_pVehicle.FindComponent(BaseVehicleControllerComponent));
			Release(victim, controller);
		}
		m_aVictims.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Lasting damage to each vehicle's engine hit zone.
	protected void DamageEngines()
	{
		array<IEntity> vehicles = {};
		CollectVehicles(vehicles, false);

		vector empty[3];
		empty[0] = vector.Zero;
		empty[1] = vector.Zero;
		empty[2] = vector.Zero;

		foreach (IEntity vehicle : vehicles)
		{
			SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
			if (!manager)
				continue;

			HitZone engine = manager.GetHitZoneByName(m_sEngineHitZone);
			if (!engine)
				continue;

			SCR_DamageContext context = new SCR_DamageContext(EDamageType.TRUE, m_fEngineDamage, empty, vehicle, engine, Instigator.CreateInstigator(m_pInstigator), null, -1, -1);
			manager.HandleDamage(context);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnLocalParticle(ResourceName particle, vector position)
	{
		if (particle.IsEmpty() || System.IsConsoleApp())
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
