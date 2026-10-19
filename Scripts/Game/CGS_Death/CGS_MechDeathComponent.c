//------------------------------------------------------------------------------------------------
//! CGS MECH DEATH - a proper mech death instead of one small vehicle explosion. Put it on EVERY
//! mech (small mech: few small explosions; titan: long chain and a huge blast - see README).
//! When the hull is destroyed:
//!  1. A chain of explosions ripples over the body, legs and arms for Sequence Time seconds.
//!  2. One huge final blast at Final Blast Point (server: damages everything in Blast Radius).
//!  3. COLLAPSE: every arm and leg blows up, breaks off and falls; the torso crashes down.
//!     The falling pieces are local copies of the models (no physics, no network), so the
//!     real mech can stay where it is; its own models and hit boxes are hidden.
//!  4. Fires keep burning on the wreck pieces.
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

	// ---- Collapse --------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "At the final blast the arms and legs blow up, break off and fall, and the torso crashes down. Off = the mech just stays where it is.", category: "Collapse")]
	protected bool m_bCollapse;

	[Attribute("{DB460C99707F948E}Particles/Weapon/Explosion_TM62M_AT_Mine.ptc", UIWidgets.ResourcePickerThumbnail, "Explosion on each arm / leg as it breaks off. Empty = none.", "ptc", category: "Collapse")]
	protected ResourceName m_sPartBlastParticle;

	[Attribute("3", UIWidgets.Auto, "How fast the arms and legs are thrown away from the body (m/s). Titan ~8.", category: "Collapse")]
	protected float m_fFlingSpeed;

	[Attribute("60", UIWidgets.Auto, "How fast the falling pieces tumble (degrees per second). Titan ~25 (big things turn slowly).", category: "Collapse")]
	protected float m_fSpinSpeed;

	[Attribute("9.81", UIWidgets.Auto, "Fall acceleration of the pieces (m/s2).", category: "Collapse")]
	protected float m_fGravity;

	[Attribute("{2E8B5767FDBED4A1}Particles/Logistics/Explosion/Ammo_Rack/Explosion_Ammo_Rack_Medium.ptc", UIWidgets.ResourcePickerThumbnail, "Effect when a piece hits the ground. Empty = none.", "ptc", category: "Collapse")]
	protected ResourceName m_sLandParticle;

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

	protected VObject m_pBodyObject;                  // the torso model, kept even if the game swaps it out
	protected ref array<ref CGS_MechDebris> m_aDebris = {};
	protected int m_iLastTickMs;
	protected int m_iFiresLeft;
	protected ref array<IEntity> m_aHidden = {};       // real mech parts we hid (kept hidden)
	protected ref array<IEntity> m_aFires = {};        // wreck fires (removed with the mech)

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_pBodyObject = owner.GetVObject();
		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);
		EnsureTick();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bDebug && m_bRunning)
			Print(string.Format("[CGS MECH DEATH] %1 was DELETED during its death sequence (something removed the mech)", owner), LogLevel.WARNING);
		s_aAll.RemoveItem(this);
		ClearDebris();
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
		GetGame().GetCallqueue().CallLater(StaticTick, 0, true); // every frame: falling pieces move smoothly
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
		float dt = 0;
		if (m_iLastTickMs > 0 && now > m_iLastTickMs)
			dt = Math.Min((now - m_iLastTickMs) * 0.001, 0.1);
		m_iLastTickMs = now;

		if (m_bDone)
		{
			KeepPartsHidden();
			UpdateDebris(dt, true);
			return;
		}

		if (m_bRunning)
		{
			KeepBodyVisible();
			RunSequence(now);
			return;
		}

		if (!IsHullDestroyed())
		{
			m_bSeenAlive = true;
			if (!m_pBodyObject)
				m_pBodyObject = GetOwner().GetVObject();
			return;
		}

		// Already dead the first time we look (joined late / streamed in): the wreck is
		// already lying on the ground - no explosions, pieces dropped straight away.
		if (!m_bSeenAlive)
		{
			m_bDone = true;
			CGS_MechShieldComponent deadShield = CGS_MechShieldComponent.Find(GetOwner());
			if (deadShield)
				deadShield.OnMechDestroyed();
			if (m_bCollapse)
			{
				Collapse(false);
				for (int i = 0; i < 1500 && UpdateDebris(0.02, false); i++) {}
			}
			SpawnWreckFires();
			return;
		}

		StartSequence(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Leg / arm scripts may switch their parts back on; keep the real parts hidden after the
	//! collapse (the falling copies are what players see).
	protected void KeepPartsHidden()
	{
		if (m_aDebris.IsEmpty())
			return;

		IEntity owner = GetOwner();
		if (owner.GetFlags() & EntityFlags.VISIBLE)
			owner.ClearFlags(EntityFlags.VISIBLE, false);

		foreach (IEntity hidden : m_aHidden)
		{
			if (hidden && (hidden.GetFlags() & EntityFlags.VISIBLE))
				hidden.ClearFlags(EntityFlags.VISIBLE, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The game's vehicle destruction can swap the torso model away the moment the hull dies;
	//! keep it on screen until the collapse takes over.
	protected void KeepBodyVisible()
	{
		IEntity owner = GetOwner();
		if (!m_pBodyObject || !m_bCollapse)
			return;

		if (owner.GetVObject() != m_pBodyObject)
			owner.SetObject(m_pBodyObject, "");
		owner.SetFlags(EntityFlags.VISIBLE, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartSequence(int now)
	{
		// Shield off for good and bubble gone.
		CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(GetOwner());
		if (shield)
			shield.OnMechDestroyed();

		if (m_bDebug)
			Print(string.Format("[CGS MECH DEATH] %1 hull destroyed - starting death sequence (body model %2)", GetOwner(), m_pBodyObject));
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
		if (m_bCollapse)
			Collapse(true);
		SpawnWreckFires();
		m_bRunning = false;
		m_bDone = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every model on the mech (torso, legs, arms, pods) becomes a falling piece; the real
	//! ones are hidden and their colliders removed.
	protected void Collapse(bool withBlasts)
	{
		IEntity owner = GetOwner();
		vector bodyCenter = owner.CoordToParent(GetPointLocal(m_FinalBlastPoint));

		RandomGenerator rng = new RandomGenerator();
		rng.SetSeed(4242);
		m_iFiresLeft = m_iWreckFires;

		array<IEntity> pieces = {};
		CollectPieces(owner, pieces, 0);

		// Torso first (from the cached model: the game may have removed it).
		if (m_pBodyObject)
			AddDebris(owner, m_pBodyObject, vector.Zero, rng, 0.25);

		foreach (IEntity piece : pieces)
		{
			vector mins, maxs;
			piece.GetWorldBounds(mins, maxs);
			vector center = (mins + maxs) * 0.5;

			if (withBlasts && !m_sPartBlastParticle.IsEmpty())
				Boom(m_sPartBlastParticle, string.Empty, center);

			vector away = center - bodyCenter;
			away[1] = 0;
			if (away.LengthSq() > 0.0001)
				away.Normalize();
			else
				away = Vector(rng.RandFloatXY(-1, 1), 0, rng.RandFloatXY(-1, 1)).Normalized();

			vector velocity = away * m_fFlingSpeed * rng.RandFloatXY(0.6, 1.2) + vector.Up * m_fFlingSpeed * rng.RandFloatXY(0.2, 0.6);
			AddDebris(piece, piece.GetVObject(), velocity, rng, 1);
		}

		// Hide the real mech and take its colliders away (invisible boxes would stop bullets).
		foreach (IEntity piece2 : pieces)
		{
			if (!m_aHidden.Contains(piece2))
				m_aHidden.Insert(piece2);
		}
		owner.ClearFlags(EntityFlags.VISIBLE, true);
		foreach (IEntity hidden : m_aHidden)
		{
			if (!hidden)
				continue;
			hidden.ClearFlags(EntityFlags.VISIBLE, true);
			RemoveColliders(hidden);
		}

		if (m_bDebug)
			Print(string.Format("[CGS MECH DEATH] %1 collapsed into %2 pieces", owner, m_aDebris.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Child models: legs, arms, pods ... (not the pilot, not particles, not hit boxes).
	protected void CollectPieces(IEntity parent, array<IEntity> pieces, int depth)
	{
		if (!parent || depth > 4)
			return;

		// Children in the hierarchy ...
		IEntity child = parent.GetChildren();
		while (child)
		{
			AddPiece(child, pieces, depth);
			child = child.GetSibling();
		}

		// ... and everything sitting in a slot (legs can be slotted without being a child).
		SlotManagerComponent slots = SlotManagerComponent.Cast(parent.FindComponent(SlotManagerComponent));
		if (slots)
		{
			array<EntitySlotInfo> infos = {};
			slots.GetSlotInfos(infos);
			foreach (EntitySlotInfo info : infos)
			{
				if (info)
					AddPiece(info.GetAttachedEntity(), pieces, depth);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPiece(IEntity entity, array<IEntity> pieces, int depth)
	{
		if (!entity || entity == GetOwner() || pieces.Contains(entity) || ChimeraCharacter.Cast(entity) || ParticleEffectEntity.Cast(entity))
			return;

		if (entity.GetVObject())
			pieces.Insert(entity);
		else if (!m_aHidden.Contains(entity))
			m_aHidden.Insert(entity); // hit boxes etc.: no model, just hide / disable

		CollectPieces(entity, pieces, depth + 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveColliders(IEntity entity)
	{
		if (!entity || ChimeraCharacter.Cast(entity))
			return;

		entity.ClearFlags(EntityFlags.TRACEABLE, true);
		Physics physics = entity.GetPhysics();
		if (physics)
			physics.Destroy();

		IEntity child = entity.GetChildren();
		while (child)
		{
			RemoveColliders(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddDebris(IEntity source, VObject model, vector velocity, RandomGenerator rng, float spinScale)
	{
		if (!source || !model)
			return;

		vector mat[4];
		source.GetWorldTransform(mat);

		// World scale (legs are often scaled in their prefab).
		float scale = 1;
		IEntity e = source;
		while (e)
		{
			scale *= e.GetScale();
			e = e.GetParent();
		}
		for (int a = 0; a < 3; a++)
		{
			if (mat[a].LengthSq() > 0.000001)
				mat[a] = mat[a].Normalized();
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3];
		IEntity debris = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!debris)
			return;

		debris.SetObject(model, "");
		debris.SetScale(scale);

		CGS_MechDebris piece = new CGS_MechDebris();
		piece.m_pEntity = debris;
		piece.m_vPosition = mat[3];
		piece.m_fStartY = mat[3][1];
		piece.m_vAngles = Math3D.MatrixToAngles(mat);
		piece.m_vVelocity = velocity;
		piece.m_vSpin = Vector(rng.RandFloatXY(-1, 1), rng.RandFloatXY(-1, 1), rng.RandFloatXY(-1, 1)) * m_fSpinSpeed * spinScale;
		piece.m_fScale = scale;
		m_aDebris.Insert(piece);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the falling pieces. Returns true while any piece is still falling.
	protected bool UpdateDebris(float dt, bool effects)
	{
		if (m_aDebris.IsEmpty() || dt <= 0)
			return false;

		BaseWorld world = GetGame().GetWorld();
		bool anyFalling = false;
		foreach (CGS_MechDebris piece : m_aDebris)
		{
			if (!piece || !piece.m_pEntity || piece.m_bLanded)
				continue;

			// Safety net: a piece that never finds ground (no terrain under it, e.g. in the prefab
			// editor, or a bad spawn spot) is removed before it falls out of the world.
			piece.m_fAge = piece.m_fAge + dt;
			if (piece.m_fAge > 12 || piece.m_vPosition[1] < piece.m_fStartY - 150)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(piece.m_pEntity);
				piece.m_pEntity = null;
				piece.m_bLanded = true;
				continue;
			}

			piece.m_vVelocity = piece.m_vVelocity + vector.Up * (-m_fGravity * dt);
			piece.m_vPosition = piece.m_vPosition + piece.m_vVelocity * dt;
			piece.m_vAngles = piece.m_vAngles + piece.m_vSpin * dt;
			piece.Apply();

			vector mins, maxs;
			piece.m_pEntity.GetWorldBounds(mins, maxs);
			float ground = world.GetSurfaceY(piece.m_vPosition[0], piece.m_vPosition[2]);
			if (mins[1] <= ground)
			{
				// Rest on the ground (a bit sunk in looks heavier than floating).
				vector rest = piece.m_vPosition;
				rest[1] = rest[1] + (ground - mins[1]) - (maxs[1] - mins[1]) * 0.08;
				piece.m_vPosition = rest;
				piece.m_bLanded = true;
				piece.Apply();

				vector landPoint = Vector(rest[0], ground, rest[2]);
				if (effects && !m_sLandParticle.IsEmpty())
					Boom(m_sLandParticle, string.Empty, landPoint);

				// Wreck fires burn where the pieces came down.
				if (m_iFiresLeft > 0 && !m_sWreckFireParticle.IsEmpty())
				{
					m_iFiresLeft--;
					ParticleEffectEntitySpawnParams fireParams = new ParticleEffectEntitySpawnParams();
					Math3D.MatrixIdentity4(fireParams.Transform);
					fireParams.Transform[3] = landPoint;
					IEntity fire = ParticleEffectEntity.SpawnParticleEffect(m_sWreckFireParticle, fireParams);
					if (fire)
						m_aFires.Insert(fire);
				}
				continue;
			}

			anyFalling = true;
		}

		return anyFalling;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearDebris()
	{
		foreach (CGS_MechDebris piece : m_aDebris)
		{
			if (piece && piece.m_pEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(piece.m_pEntity);
		}
		m_aDebris.Clear();

		foreach (IEntity fire : m_aFires)
		{
			if (fire)
				SCR_EntityHelper.DeleteEntityAndChildren(fire);
		}
		m_aFires.Clear();
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

		// Collapsed: the fires start where the pieces land (see UpdateDebris).
		if (!m_aDebris.IsEmpty())
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
			IEntity fire = ParticleEffectEntity.SpawnParticleEffect(m_sWreckFireParticle, spawnParams);
			if (fire)
				m_aFires.Insert(fire);
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

//------------------------------------------------------------------------------------------------
//! One falling wreck piece (a local copy of a mech model).
class CGS_MechDebris
{
	IEntity m_pEntity;
	vector m_vPosition;
	vector m_vAngles;
	vector m_vVelocity;
	vector m_vSpin;
	float m_fScale = 1;
	bool m_bLanded;
	float m_fStartY;   // height it started falling from
	float m_fAge;      // seconds falling

	void Apply()
	{
		if (!m_pEntity)
			return;

		vector mat[4];
		Math3D.AnglesToMatrix(m_vAngles, mat);
		mat[3] = m_vPosition;
		m_pEntity.SetWorldTransform(mat);
		m_pEntity.SetScale(m_fScale);
	}
}

//------------------------------------------------------------------------------------------------
//! A destroyed mech never fires and never walks again (AI crews, point defense, everything).
modded class BFA_MechExtension
{
	//------------------------------------------------------------------------------------------------
	static bool IsMechDestroyed(IEntity mech)
	{
		if (!mech)
			return false;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(mech.FindComponent(SCR_DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanWeaponEntityFire(IEntity mech, IEntity weaponEntity)
	{
		if (IsMechDestroyed(mech))
			return false;
		return super.CanWeaponEntityFire(mech, weaponEntity);
	}

	//------------------------------------------------------------------------------------------------
	override float GetMechSpeedScale(IEntity mech)
	{
		if (IsMechDestroyed(mech))
			return 0;
		return super.GetMechSpeedScale(mech);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanMechSprint(IEntity mech)
	{
		if (IsMechDestroyed(mech))
			return false;
		return super.CanMechSprint(mech);
	}
}
