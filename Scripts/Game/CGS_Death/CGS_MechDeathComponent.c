//------------------------------------------------------------------------------------------------
//! CGS MECH DEATH - a proper mech death instead of one small vehicle explosion. Put it on EVERY
//! mech (small mech: few small explosions; titan: long chain and a huge blast - see README).
//! When the hull is destroyed:
//!  1. A chain of explosions ripples over the body, legs and arms for Sequence Time seconds.
//!  2. One huge final blast at Final Blast Point (server: damages everything in Blast Radius).
//!  3. Fires keep burning on the wreck.
//! Runs on every machine from the replicated hull state, no network traffic. The explosion
//! points are picked the same way everywhere, so every player sees the same show. A mech that
//! is already dead when a player joins just shows the burning wreck.
[ComponentEditorProps(category: "CGS MechCore/Damage", description: "Mech death: chain explosions over the mech, a final blast and a burning wreck. Put it on every mech.")]
class CGS_MechDeathComponentClass : ScriptComponentClass
{
}

class CGS_MechDeathComponent : ScriptComponent
{
	// ---- Chain explosions ------------------------------------------------------------------
	[Attribute("2.5", UIWidgets.Auto, "Seconds of chain explosions before the final blast. Titan ~6.", category: "Chain")]
	protected float m_fSequenceTime;

	[Attribute("5", UIWidgets.Auto, "Number of chain explosions. Titan ~14.", category: "Chain")]
	protected int m_iChainExplosions;

	[Attribute("1", UIWidgets.Auto, "Random offset in metres around each explosion point. Titan ~6.", category: "Chain")]
	protected float m_fChainJitter;

	[Attribute("{2E8B5767FDBED4A1}Particles/Logistics/Explosion/Ammo_Rack/Explosion_Ammo_Rack_Medium.ptc", UIWidgets.ResourcePickerThumbnail, "Particle of each chain explosion.", "ptc", category: "Chain")]
	protected ResourceName m_sChainParticle;

	[Attribute("", UIWidgets.EditBox, "Chain explosion sound event (in Sound Project). Empty = silent.", category: "Chain")]
	protected string m_sChainSoundEvent;

	[Attribute("", UIWidgets.Auto, "Extra explosion points on the body (e.g. reactor, head). The legs, arms and hit boxes are used automatically.", category: "Chain")]
	protected ref array<ref PointInfo> m_aChainPoints;

	// ---- Final blast -----------------------------------------------------------------------
	[Attribute("", UIWidgets.Auto, "Where the final blast goes off (e.g. the reactor). Empty = Default Blast Height above the mech's origin.", category: "Final Blast")]
	protected ref PointInfo m_FinalBlastPoint;

	[Attribute("3", UIWidgets.Auto, "Final blast height above the mech's origin when Final Blast Point is not set (metres). Titan ~60.", category: "Final Blast")]
	protected float m_fDefaultBlastHeight;

	[Attribute("2", UIWidgets.Auto, "Spread of the final-blast particles in metres. Titan ~12.", category: "Final Blast")]
	protected float m_fFinalSpread;

	[Attribute("{DB460C99707F948E}Particles/Weapon/Explosion_TM62M_AT_Mine.ptc", UIWidgets.ResourcePickerThumbnail, "Particle of the final blast.", "ptc", category: "Final Blast")]
	protected ResourceName m_sFinalParticle;

	[Attribute("1", UIWidgets.Auto, "How many final-blast particles (spread around the blast point) - more = bigger.", category: "Final Blast")]
	protected int m_iFinalParticleCount;

	[Attribute("SOUND_LIGHTNING_STRIKE", UIWidgets.EditBox, "Final blast sound event (in Sound Project). Empty = silent.", category: "Final Blast")]
	protected string m_sFinalSoundEvent;

	[Attribute("12", UIWidgets.Auto, "Final blast damage radius in metres (falls off to 0 at the edge). 0 = no damage. Titan ~60.", category: "Final Blast")]
	protected float m_fBlastRadius;

	[Attribute("600", UIWidgets.Auto, "Final blast explosive damage at the centre. Titan ~3000.", category: "Final Blast")]
	protected float m_fBlastDamage;

	// ---- Wreck -----------------------------------------------------------------------------
	[Attribute("{79DD3A5F1F3069CD}Particles/Vehicle/Vehicle_fire_engine_small.ptc", UIWidgets.ResourcePickerThumbnail, "Fire left burning on the wreck (at several points). Empty = none.", "ptc", category: "Wreck")]
	protected ResourceName m_sWreckFireParticle;

	[Attribute("2", UIWidgets.Auto, "How many fires burn on the wreck. Titan ~5.", category: "Wreck")]
	protected int m_iWreckFires;

	// ---- Sound / test ----------------------------------------------------------------------
	[Attribute("{C45837D7F7445964}Sounds/Editor/LightningStrike/Editor_LightningStrike.acp", UIWidgets.ResourcePickerThumbnail, "Sound project (.acp) with the chain / final sound events.", "acp", category: "Sound")]
	protected ResourceName m_sSoundProject;

	[Attribute("false", UIWidgets.CheckBox, "Print the death sequence to the log.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime ---------------------------------------------------------------------------
	protected static ref array<CGS_MechDeathComponent> s_aAll = {};
	protected static int s_iHeartbeatMs;

	protected bool m_bSeenAlive;
	protected bool m_bDone;
	protected bool m_bRunning;
	protected int m_iStartMs;
	protected int m_iNextChain;
	protected ref array<int> m_aChainTimes = {};
	protected ref array<vector> m_aChainLocal = {}; // explosion points in the titan's local space
	protected ref array<IEntity> m_aChainAnchor = {}; // ... or on a part (follows it), null = titan

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
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
		if (!GetGame() || !GetGame().GetWorld())
			return;

		int now = GetGame().GetWorld().GetWorldTime();
		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			CGS_MechDeathComponent death = s_aAll[i];
			if (!death || !death.GetOwner())
			{
				s_aAll.Remove(i);
				continue;
			}
			death.Tick(now);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHullDestroyed()
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(GetOwner().FindComponent(SCR_DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick(int now)
	{
		if (m_bDone)
			return;

		if (m_bRunning)
		{
			RunSequence(now);
			return;
		}

		if (!IsHullDestroyed())
		{
			m_bSeenAlive = true;
			return;
		}

		// Already dead the first time we look (joined late / streamed in): just the wreck.
		if (!m_bSeenAlive)
		{
			m_bDone = true;
			SpawnWreckFires();
			return;
		}

		StartSequence(now);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartSequence(int now)
	{
		m_bRunning = true;
		m_iStartMs = now;
		m_iNextChain = 0;
		m_aChainTimes.Clear();
		m_aChainLocal.Clear();
		m_aChainAnchor.Clear();

		// Same seed everywhere = same explosions on every machine.
		RandomGenerator rng = new RandomGenerator();
		rng.SetSeed(7331);

		array<IEntity> anchors = {};
		CollectAnchors(GetOwner(), anchors, 0);

		IEntity owner = GetOwner();
		int count = Math.Max(m_iChainExplosions, 0);
		int totalMs = (int)(Math.Max(m_fSequenceTime, 0.5) * 1000);
		for (int i = 0; i < count; i++)
		{
			// Faster and faster towards the end.
			float t = (i + 1.0) / (count + 1.0);
			int at = Math.Round(totalMs * Math.Pow(t, 0.75)) + rng.RandIntInclusive(-150, 150);
			if (at < 0)
				at = 0;
			if (at > totalMs)
				at = totalMs;
			m_aChainTimes.Insert(at);

			vector jitter = Vector(rng.RandFloatXY(-1, 1), rng.RandFloatXY(-1, 1), rng.RandFloatXY(-1, 1)) * m_fChainJitter;

			int extra = 0;
			if (m_aChainPoints)
				extra = m_aChainPoints.Count();
			int pick = rng.RandInt(0, Math.Max(anchors.Count() + extra, 1));

			if (pick < anchors.Count())
			{
				m_aChainAnchor.Insert(anchors[pick]);
				m_aChainLocal.Insert(jitter);
			}
			else if (extra > 0)
			{
				m_aChainAnchor.Insert(null);
				m_aChainLocal.Insert(GetPointLocal(m_aChainPoints[pick - anchors.Count()]) + jitter);
			}
			else
			{
				m_aChainAnchor.Insert(null);
				m_aChainLocal.Insert(Vector(0, m_fDefaultBlastHeight, 0) + jitter * 3);
			}
		}

		if (m_bDebug)
			Print(string.Format("[CGS MECH DEATH] %1 destroyed: %2 chain explosions over %3 s from %4 anchor points", owner, count, m_fSequenceTime, anchors.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Legs, arms, pods and hit boxes (every child entity with a mesh or a collider).
	protected void CollectAnchors(IEntity parent, array<IEntity> anchors, int depth)
	{
		if (!parent || depth > 4)
			return;

		IEntity child = parent.GetChildren();
		while (child)
		{
			if (child.GetVObject() || child.GetPhysics())
				anchors.Insert(child);
			CollectAnchors(child, anchors, depth + 1);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetPointLocal(PointInfo point)
	{
		if (!point)
			return Vector(0, m_fDefaultBlastHeight, 0);

		point.Init(GetOwner());
		vector mat[4];
		point.GetModelTransform(mat);
		return mat[3];
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSequence(int now)
	{
		int elapsed = now - m_iStartMs;
		while (m_iNextChain < m_aChainTimes.Count() && elapsed >= m_aChainTimes[m_iNextChain])
		{
			IEntity anchor = m_aChainAnchor[m_iNextChain];
			vector position;
			if (anchor)
				position = anchor.GetOrigin() + m_aChainLocal[m_iNextChain];
			else
				position = GetOwner().CoordToParent(m_aChainLocal[m_iNextChain]);

			Boom(m_sChainParticle, m_sChainSoundEvent, position);
			m_iNextChain++;
		}

		if (elapsed < (int)(Math.Max(m_fSequenceTime, 0.5) * 1000))
			return;

		FinalBlast();
		SpawnWreckFires();
		m_bRunning = false;
		m_bDone = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void FinalBlast()
	{
		IEntity owner = GetOwner();
		vector center = owner.CoordToParent(GetPointLocal(m_FinalBlastPoint));

		int count = Math.Max(m_iFinalParticleCount, 1);
		for (int i = 0; i < count; i++)
		{
			vector offset = vector.Zero;
			if (i > 0)
			{
				float angle = (Math.PI2 / (count - 1)) * i;
				offset = Vector(Math.Cos(angle), 0.3, Math.Sin(angle)) * m_fFinalSpread;
			}

			string sound = string.Empty;
			if (i == 0)
				sound = m_sFinalSoundEvent;
			Boom(m_sFinalParticle, sound, center + offset);
		}

		if (m_bDebug)
			Print(string.Format("[CGS MECH DEATH] %1 final blast at %2", owner, center));

		if (Replication.IsServer())
			BlastDamage(center);
	}

	//------------------------------------------------------------------------------------------------
	protected void Boom(ResourceName particle, string soundEvent, vector position)
	{
		if (!particle.IsEmpty())
		{
			ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
			Math3D.MatrixIdentity4(spawnParams.Transform);
			spawnParams.Transform[3] = position;
			ParticleEffectEntity.SpawnParticleEffect(particle, spawnParams);
		}

		if (!soundEvent.IsEmpty() && !m_sSoundProject.IsEmpty())
		{
			vector mat[4];
			Math3D.MatrixIdentity4(mat);
			mat[3] = position;
			BFA_MechWeaponModuleComponent.BFA_MechPlaySound(m_sSoundProject, soundEvent, mat);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fires that stay on the wreck: on the body and on the parts (they follow the wreck).
	protected void SpawnWreckFires()
	{
		if (m_sWreckFireParticle.IsEmpty() || m_iWreckFires <= 0)
			return;

		IEntity owner = GetOwner();
		array<IEntity> anchors = {};
		CollectAnchors(owner, anchors, 0);

		RandomGenerator rng = new RandomGenerator();
		rng.SetSeed(1337);

		for (int i = 0; i < m_iWreckFires; i++)
		{
			IEntity parent = owner;
			if (!anchors.IsEmpty() && i > 0)
				parent = anchors[rng.RandInt(0, anchors.Count())];

			ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
			Math3D.MatrixIdentity4(spawnParams.Transform);
			if (parent == owner)
				spawnParams.Transform[3] = GetPointLocal(m_FinalBlastPoint);
			spawnParams.Parent = parent;
			ParticleEffectEntity.SpawnParticleEffect(m_sWreckFireParticle, spawnParams);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Explosive damage around the blast, falling off to 0 at Blast Radius.
	protected void BlastDamage(vector center)
	{
		if (m_fBlastRadius <= 0 || m_fBlastDamage <= 0)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		s_aBlastCollect.Clear();
		world.QueryEntitiesBySphere(center, m_fBlastRadius, CollectBlast, null, EQueryEntitiesFlags.ALL);

		IEntity owner = GetOwner();
		IEntity killer = BFA_MechCoreComponent.BFA_ResolveKiller(owner, null);

		array<IEntity> done = {};
		int hurt = 0;
		foreach (IEntity entity : s_aBlastCollect)
		{
			if (!entity)
				continue;

			IEntity root = entity.GetRootParent();
			if (!root)
				root = entity;
			if (root == owner || done.Contains(root))
				continue;
			done.Insert(root);

			SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(root.FindComponent(SCR_DamageManagerComponent));
			if (!manager)
				continue;
			HitZone zone = manager.GetDefaultHitZone();
			if (!zone)
				continue;

			float falloff = Math.Clamp(1.0 - vector.Distance(center, root.GetOrigin()) / m_fBlastRadius, 0.0, 1.0);
			if (falloff <= 0)
				continue;

			vector dir = root.GetOrigin() - center;
			if (dir.LengthSq() > 0.0001)
				dir.Normalize();
			else
				dir = vector.Up;

			vector hit[3];
			hit[0] = root.GetOrigin();
			hit[1] = dir;
			hit[2] = dir * -1.0;

			SCR_DamageContext context = new SCR_DamageContext(EDamageType.EXPLOSIVE, m_fBlastDamage * falloff, hit, root, zone, Instigator.CreateInstigator(killer), null, -1, -1);
			manager.HandleDamage(context);
			hurt++;
		}
		s_aBlastCollect.Clear();

		if (m_bDebug)
			Print(string.Format("[CGS MECH DEATH] final blast hit %1 objects within %2 m", hurt, m_fBlastRadius));
	}

	protected static ref array<IEntity> s_aBlastCollect = {};

	//------------------------------------------------------------------------------------------------
	protected bool CollectBlast(IEntity entity)
	{
		if (entity)
			s_aBlastCollect.Insert(entity);
		return true;
	}
}
