//------------------------------------------------------------------------------------------------
//! CGS ANTI-AIR - automatic flak against aircraft (helicopters, gunships, anything flying).
//!  - Works on its own: no turret, no aiming, no gunner needed. It scans for HOSTILE vehicles
//!    flying at least Min Altitude above the ground within Range, leads them and fires flak.
//!  - Each shell bursts near the target after its travel time: explosive splash in Burst Radius.
//!    Accuracy gets worse with distance (Miss Radius + Miss Per 100 m), so far targets take
//!    more shells and close ones get shredded.
//!  - Fires in bursts (Burst Shots, then Burst Pause). Ammo, reload, heat and "destroyed part"
//!    rules work like every MechCore weapon; a destroyed mech never fires.
//! Put it on a weapon part prefab and install it on a hardpoint like any weapon module.
//! Give it its own weapon group (e.g. 9) so AI gunners don't treat it as a main weapon.
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "CGS Anti-Air: automatic flak that leads and shoots down hostile aircraft. No turret or gunner needed.")]
class CGS_AntiAirModuleComponentClass : BFA_MechWeaponModuleComponentClass
{
}

class CGS_AntiAirModuleComponent : BFA_MechWeaponModuleComponent
{
	// ---- Targeting ---------------------------------------------------------------------------
	[Attribute("1500", UIWidgets.Auto, "Range in metres.", category: "Anti-Air")]
	protected float m_fRange;

	[Attribute("12", UIWidgets.Auto, "Only vehicles at least this high above the ground count as aircraft (metres). Keeps it off cars and tanks.", category: "Anti-Air")]
	protected float m_fMinAltitude;

	[Attribute("0.5", UIWidgets.Auto, "Seconds between target scans.", category: "Anti-Air")]
	protected float m_fScanInterval;

	[Attribute("true", UIWidgets.CheckBox, "Only fire while someone (player or AI pilot) crews the mech. Off = always active.", category: "Anti-Air")]
	protected bool m_bRequiresCrew;

	// ---- Shells ------------------------------------------------------------------------------
	[Attribute("900", UIWidgets.Auto, "Shell speed in m/s (travel time + how far it leads the target).", category: "Flak")]
	protected float m_fShellSpeed;

	[Attribute("180", UIWidgets.Auto, "Explosive damage of one flak burst at its centre.", category: "Flak")]
	protected float m_fBurstDamage;

	[Attribute("9", UIWidgets.Auto, "Burst radius in metres (damage falls off to 0 at the edge).", category: "Flak")]
	protected float m_fBurstRadius;

	[Attribute("3", UIWidgets.Auto, "Miss radius in metres at point-blank range.", category: "Flak")]
	protected float m_fMissRadius;

	[Attribute("1.2", UIWidgets.Auto, "Extra miss radius per 100 m of distance (1.2 = 15 m spread at 1 km).", category: "Flak")]
	protected float m_fMissPer100m;

	// ---- Bursts ------------------------------------------------------------------------------
	[Attribute("8", UIWidgets.Auto, "Shells per burst (fired at Fire > Shots Per Minute).", category: "Bursts")]
	protected int m_iBurstShots;

	[Attribute("1.5", UIWidgets.Auto, "Seconds between bursts.", category: "Bursts")]
	protected float m_fBurstPause;

	// ---- Visuals -----------------------------------------------------------------------------
	[Attribute("4", UIWidgets.Auto, "Tracer streak length in metres.", category: "Visuals")]
	protected float m_fTracerLength;

	[Attribute("0.2", UIWidgets.Auto, "Tracer thickness.", category: "Visuals")]
	protected float m_fTracerThickness;

	// ---- Diagnostics -------------------------------------------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Print targets, bursts and hits to the log.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime (server) --------------------------------------------------------------------
	protected static ref array<CGS_AntiAirModuleComponent> s_aAll = {};
	protected static int s_iHeartbeatMs;

	protected IEntity m_pTarget;
	protected float m_fNextScan;
	protected int m_iShotsInBurst;
	protected float m_fBurstReadyAt;
	protected int m_iShellToken;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sModuleId == "WEAPON_MODULE")
			m_sModuleId = "ANTI_AIR";

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
		return m_fRange;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTick()
	{
		if (s_iHeartbeatMs != 0 && System.GetTickCount() - s_iHeartbeatMs < 1000)
			return;
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().Remove(StaticTick);
		GetGame().GetCallqueue().CallLater(StaticTick, 50, true);
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
			CGS_AntiAirModuleComponent aa = s_aAll[i];
			if (!aa || !aa.GetOwner())
			{
				s_aAll.Remove(i);
				continue;
			}
			aa.ServerTick();
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
	protected void ServerTick()
	{
		if (!m_bBoundToHardpoint || !m_pMechOwner)
			return;

		IEntity instigator = BFA_MechCoreComponent.BFA_ResolveKiller(m_pMechOwner, null);
		if (m_bRequiresCrew && !ChimeraCharacter.Cast(instigator))
		{
			m_pTarget = null;
			return;
		}

		float now = Now();
		if (now >= m_fNextScan || !IsValidTarget(m_pTarget, GetMechFaction()))
		{
			m_fNextScan = now + Math.Max(m_fScanInterval, 0.1);
			IEntity previous = m_pTarget;
			m_pTarget = FindTarget();
			if (m_bDebug && m_pTarget != previous)
				Print(string.Format("[CGS ANTI-AIR] %1 target: %2", m_sHardpointName, m_pTarget));
		}

		if (!m_pTarget || now < m_fBurstReadyAt)
			return;

		if (TryFireShot(instigator))
		{
			m_iShotsInBurst++;
			if (m_iShotsInBurst >= Math.Max(m_iBurstShots, 1))
			{
				m_iShotsInBurst = 0;
				m_fBurstReadyAt = now + Math.Max(m_fBurstPause, 0);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! No air target = no shot (the AI crew may hold this group's trigger; don't waste ammo).
	override protected bool TryFireShot(IEntity instigator)
	{
		if (!m_pTarget)
			return false;
		return super.TryFireShot(instigator);
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
	protected bool IsValidTarget(IEntity entity, Faction mechFaction)
	{
		if (!entity || !mechFaction || !Vehicle.Cast(entity))
			return false;
		if (entity == m_pMechOwner || entity.GetRootParent() == m_pMechOwner)
			return false;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.GetState() == EDamageState.DESTROYED)
			return false;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;
		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction || !mechFaction.IsFactionEnemy(faction))
			return false; // empty or friendly aircraft

		vector pos = entity.GetOrigin();
		if (vector.Distance(pos, GetOwner().GetOrigin()) > m_fRange)
			return false;

		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(pos[0], pos[2]);
		return pos[1] - ground >= m_fMinAltitude;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindTarget()
	{
		BaseWorld world = GetGame().GetWorld();
		Faction mechFaction = GetMechFaction();
		if (!world || !mechFaction)
			return null;

		s_aCollect.Clear();
		world.QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fRange, CollectEntity, null, EQueryEntitiesFlags.DYNAMIC);

		IEntity best;
		float bestDistance = m_fRange + 1;
		vector origin = GetOwner().GetOrigin();
		foreach (IEntity entity : s_aCollect)
		{
			if (!IsValidTarget(entity, mechFaction))
				continue;

			float distance = vector.Distance(origin, entity.GetOrigin());
			if (distance < bestDistance)
			{
				best = entity;
				bestDistance = distance;
			}
		}
		s_aCollect.Clear();
		return best;
	}

	protected static ref array<IEntity> s_aCollect = {};

	//------------------------------------------------------------------------------------------------
	protected bool CollectEntity(IEntity entity)
	{
		if (entity && Vehicle.Cast(entity))
			s_aCollect.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One flak shell: lead the target, scatter by distance, burst after the travel time.
	override protected void FireShot(IEntity instigator)
	{
		if (!m_pTarget)
			return;

		vector direction;
		vector start = GetMuzzle(direction);

		vector mins, maxs;
		m_pTarget.GetWorldBounds(mins, maxs);
		vector targetPos = (mins + maxs) * 0.5;

		vector velocity = vector.Zero;
		Physics physics = m_pTarget.GetPhysics();
		if (physics)
			velocity = physics.GetVelocity();

		float speed = Math.Max(m_fShellSpeed, 50);
		float distance = vector.Distance(start, targetPos);
		float flight = distance / speed;
		vector lead = targetPos + velocity * flight;
		flight = vector.Distance(start, lead) / speed;
		lead = targetPos + velocity * flight;

		float miss = m_fMissRadius + distance * 0.01 * m_fMissPer100m;
		vector scatter = Vector(Math.RandomFloat(-1, 1), Math.RandomFloat(-1, 1), Math.RandomFloat(-1, 1));
		if (scatter.LengthSq() > 1)
			scatter.Normalize();
		vector burst = lead + scatter * miss;

		BroadcastShot(start, burst, true, speed, m_fTracerLength, m_fTracerThickness);

		int delayMs = (int)(flight * 1000);
		if (delayMs < 20)
		{
			Detonate(burst, instigator, m_iShellToken);
			return;
		}
		GetGame().GetCallqueue().CallLater(Detonate, delayMs, false, burst, instigator, m_iShellToken);
	}

	//------------------------------------------------------------------------------------------------
	protected void Detonate(vector position, IEntity instigator, int token)
	{
		if (!Replication.IsServer() || token != m_iShellToken)
			return;

		int hurt = DealSplashDamage(position, m_fBurstDamage, m_fBurstRadius, EDamageType.EXPLOSIVE, instigator);
		if (m_bDebug && hurt > 0)
			Print(string.Format("[CGS ANTI-AIR] %1 burst at %2 hit %3 target(s)", m_sHardpointName, position, hurt));
	}

	//------------------------------------------------------------------------------------------------
	override void UnbindFromHardpoint()
	{
		m_iShellToken++; // shells in the air no longer count
		m_pTarget = null;
		super.UnbindFromHardpoint();
	}
}
