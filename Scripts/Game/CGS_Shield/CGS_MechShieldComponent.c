//------------------------------------------------------------------------------------------------
//! CGS SHIELD ADD-ON - energy shield for any MechCore mech.
//! Add this component to a mech prefab (next to BFA_MechCoreComponent). The hull must use the
//! BFA_MechHitZone class (all MechCore sample mechs do). Nothing in MechCore is edited: the
//! shield plugs in through BFA_MechExtension (see CGS_MechShieldExtension.c).
//!
//! While the shield is up it soaks hull damage from EVERY source (MechCore weapons, rockets,
//! tanks, mines). MechCore hitscan beams stop at the bubble surface. Knocked down -> waits,
//! recharges, and comes back once it reaches Reactivate At.
[ComponentEditorProps(category: "CGS MechCore", description: "Energy shield for a mech (CGS Shield add-on). Absorbs hull damage, recharges, optional bubble visual.")]
class CGS_MechShieldComponentClass : ScriptComponentClass
{
}

class CGS_MechShieldComponent : ScriptComponent
{
	// ---- Strength ------------------------------------------------------------------------------
	[Attribute("1500", UIWidgets.Auto, "Shield points. Compared to hull damage AFTER the hull's own armour multipliers, so a hull with Kinetic multiplier 0.01 also makes bullets cheap for the shield.", category: "Shield")]
	protected float m_fMaxShield;

	[Attribute("75", UIWidgets.Auto, "Shield points recharged per second.", category: "Shield")]
	protected float m_fRegenPerSecond;

	[Attribute("4", UIWidgets.Auto, "Seconds without being hit before recharging starts.", category: "Shield")]
	protected float m_fRegenDelay;

	[Attribute("10", UIWidgets.Auto, "When the shield is knocked down: seconds before it starts recharging.", category: "Shield")]
	protected float m_fBrokenDelay;

	[Attribute("0.25", UIWidgets.Slider, "When knocked down, the shield comes back up once it has recharged to this fraction (stops it flickering on and off).", "0 1 0.05", category: "Shield")]
	protected float m_fReactivateAt;

	[Attribute("0", UIWidgets.Slider, "Fraction of damage that always passes through to the hull, even with the shield up. 0 = shield takes everything.", "0 1 0.05", category: "Shield")]
	protected float m_fBleedThrough;

	// ---- Damage types --------------------------------------------------------------------------
	[Attribute("1", UIWidgets.Auto, "Shield cost per point of KINETIC damage (MechCore guns / autocannons).", category: "Damage Types")]
	protected float m_fKineticCost;

	[Attribute("1", UIWidgets.Auto, "Shield cost per point of EXPLOSIVE damage (MechCore rockets / missiles).", category: "Damage Types")]
	protected float m_fExplosiveCost;

	[Attribute("1", UIWidgets.Auto, "Shield cost per point of ENERGY damage (las cannons, beams).", category: "Damage Types")]
	protected float m_fEnergyCost;

	[Attribute("3", UIWidgets.Auto, "Shield cost per point of EMP damage. Above 1 = EMP drains shields fast.", category: "Damage Types")]
	protected float m_fEmpCost;

	[Attribute("1", UIWidgets.Auto, "Shield cost per point of damage from anything that is not a MechCore weapon (vanilla RPGs, tanks, mines, other mods).", category: "Damage Types")]
	protected float m_fOtherCost;

	// ---- Bubble --------------------------------------------------------------------------------
	[Attribute(desc: "BUBBLE CENTRE: Set class > PointInfo and place it in the middle of the mech (e.g. halfway down a tall titan). Empty = the mech's origin.", category: "Bubble")]
	protected ref PointInfo m_Center;

	[Attribute("6", UIWidgets.Auto, "Bubble radius in metres. Must cover the whole mech (a 55 m titan needs about 35).", category: "Bubble")]
	protected float m_fRadius;

	[Attribute("true", UIWidgets.CheckBox, "MechCore hitscan beams (las cannons etc.) from OUTSIDE stop at the bubble surface. Shots fired from inside the bubble pass freely.", category: "Bubble")]
	protected bool m_bBlockBeams;

	[Attribute("", UIWidgets.ResourceNamePicker, "Bubble look: a prefab (.et) with only a MeshObject - a sphere with a see-through shield material. No physics, no RplComponent. Scaled to Radius.", params: "et", category: "Bubble")]
	protected ResourceName m_sBubblePrefab;

	[Attribute("0", UIWidgets.Auto, "Radius of the bubble model at scale 1, in metres. 0 = measured automatically from the model (recommended).", category: "Bubble")]
	protected float m_fBubbleModelRadius;

	[Attribute("false", UIWidgets.CheckBox, "Bubble always visible while the shield is up. Off = it only flashes when hit.", category: "Bubble")]
	protected bool m_bBubbleAlwaysVisible;

	[Attribute("0.6", UIWidgets.Auto, "Seconds the bubble stays visible after a hit (when not always visible).", category: "Bubble")]
	protected float m_fBubbleFlashTime;

	// ---- Effects -------------------------------------------------------------------------------
	[Attribute("", UIWidgets.ResourceNamePicker, "Particle (.ptc) where a hit lands on the bubble.", params: "ptc", category: "Effects")]
	protected ResourceName m_sHitParticle;

	[Attribute("", UIWidgets.ResourceNamePicker, "Particle (.ptc) at the centre when the shield is knocked down.", params: "ptc", category: "Effects")]
	protected ResourceName m_sBreakParticle;

	[Attribute("", UIWidgets.ResourceNamePicker, "Particle (.ptc) at the centre when the shield comes back up.", params: "ptc", category: "Effects")]
	protected ResourceName m_sRestoreParticle;

	[Attribute("", UIWidgets.ResourceNamePicker, "Sound file (.acp) for the shield sounds below.", params: "acp", category: "Effects")]
	protected ResourceName m_sSoundProject;

	[Attribute("", UIWidgets.EditBox, "Hit sound event inside the .acp.", category: "Effects")]
	protected string m_sHitSoundEvent;

	[Attribute("", UIWidgets.EditBox, "Shield down sound event.", category: "Effects")]
	protected string m_sBreakSoundEvent;

	[Attribute("", UIWidgets.EditBox, "Shield back up sound event.", category: "Effects")]
	protected string m_sRestoreSoundEvent;

	// ---- Replicated state ----------------------------------------------------------------------
	[RplProp(onRplName: "OnRepShield")]
	protected float m_fShield;

	[RplProp(onRplName: "OnRepShield")]
	protected bool m_bDown;

	protected float m_fLastHitTime = -1000;
	protected float m_fLastRepShield;
	protected bool m_bLastRepDown;
	protected IEntity m_pBubble;
	protected vector m_aBubbleLocal[4]; // bubble transform in the mech's space (scale baked in)
	protected bool m_bTickRunning;
	protected bool m_bBubbleTried;

	// Every shielded mech in the world (for beam interception).
	protected static ref array<IEntity> s_aShieldedMechs = {};

	static const int TICK_MS = 250;

	//==================================================================================================
	// QUERIES (any machine)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	static CGS_MechShieldComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return CGS_MechShieldComponent.Cast(mech.FindComponent(CGS_MechShieldComponent));
	}

	//------------------------------------------------------------------------------------------------
	static void GetShieldedMechs(notnull array<IEntity> outMechs)
	{
		outMechs.Clear();
		foreach (IEntity mech : s_aShieldedMechs)
		{
			if (mech)
				outMechs.Insert(mech);
		}
	}

	//------------------------------------------------------------------------------------------------
	bool IsUp()
	{
		return !m_bDown && m_fShield > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! 0..1 for HUDs.
	float GetShield01()
	{
		if (m_fMaxShield <= 0)
			return 0;
		return Math.Clamp(m_fShield / m_fMaxShield, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	vector GetCenterWorld()
	{
		IEntity owner = GetOwner();
		if (!m_Center)
			return owner.GetOrigin();

		m_Center.Init(owner);
		vector local[4];
		m_Center.GetModelTransform(local);
		return owner.CoordToParent(local[3]);
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return Math.Max(m_fRadius, 0.5);
	}

	//------------------------------------------------------------------------------------------------
	//! Distance along the ray where it enters the bubble, or -1 (miss, shield down, or the ray
	//! starts inside the bubble).
	float IntersectRay(vector start, vector direction, float range)
	{
		if (!m_bBlockBeams || !IsUp())
			return -1;

		vector center = GetCenterWorld();
		float radius = GetRadius();
		vector toStart = start - center;
		float c = vector.Dot(toStart, toStart) - radius * radius;
		if (c <= 0)
			return -1; // shooter is inside the bubble

		float b = vector.Dot(toStart, direction);
		if (b > 0)
			return -1; // pointing away

		float disc = b * b - c;
		if (disc < 0)
			return -1;

		float t = -b - Math.Sqrt(disc);
		if (t < 0 || t > range)
			return -1;

		return t;
	}

	//==================================================================================================
	// SERVER
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! [Server] Hull damage arrives here while the shield exists. Returns what reaches the hull.
	float Absorb(float damage, int damageKind, IEntity shooterMech)
	{
		if (!Replication.IsServer() || damage <= 0 || !IsUp())
			return damage;

		float bleed = Math.Clamp(m_fBleedThrough, 0, 1);
		float passThrough = damage * bleed;
		float blocked = damage - passThrough;
		float cost = blocked * Math.Max(GetCost(damageKind), 0);

		m_fLastHitTime = GetTime();
		EnsureTick();
		BFA_MechLog.Log(string.Format("[CGS SHIELD] %1 hit: %2 damage (kind %3) -> shield cost %4, shield %5/%6", GetOwner(), damage, damageKind, cost, m_fShield, m_fMaxShield));

		vector hitPos = GetHitPoint(shooterMech);

		if (cost <= m_fShield)
		{
			m_fShield -= cost;
			Replication.BumpMe();
			PlayHit(hitPos);
			Rpc(RpcDo_Hit, hitPos);
			return passThrough;
		}

		// Not enough left: it soaks what it can, the rest reaches the hull, and it goes down.
		float soakedFraction = 0;
		if (cost > 0)
			soakedFraction = m_fShield / cost;

		m_fShield = 0;
		m_bDown = true;
		Replication.BumpMe();
		OnRepShield();

		PlayHit(hitPos);
		Rpc(RpcDo_Hit, hitPos);

		return passThrough + blocked * (1 - soakedFraction);
	}

	//------------------------------------------------------------------------------------------------
	protected float GetCost(int damageKind)
	{
		switch (damageKind)
		{
			case BFA_EMechDamageKind.KINETIC: return m_fKineticCost;
			case BFA_EMechDamageKind.EXPLOSIVE: return m_fExplosiveCost;
			case BFA_EMechDamageKind.ENERGY: return m_fEnergyCost;
			case BFA_EMechDamageKind.EMP: return m_fEmpCost;
		}
		return m_fOtherCost;
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetHitPoint(IEntity shooterMech)
	{
		vector center = GetCenterWorld();
		if (!shooterMech)
			return center;

		vector toShooter = shooterMech.GetOrigin() - center;
		if (toShooter.LengthSq() < 0.01)
			return center;

		return center + toShooter.Normalized() * GetRadius();
	}

	//------------------------------------------------------------------------------------------------
	protected float GetTime()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;
		return world.GetWorldTime() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureTick()
	{
		if (m_bTickRunning || !Replication.IsServer())
			return;

		m_bTickRunning = true;
		GetGame().GetCallqueue().CallLater(ServerTick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopTick()
	{
		if (!m_bTickRunning)
			return;

		m_bTickRunning = false;
		GetGame().GetCallqueue().Remove(ServerTick);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerTick()
	{
		if (m_fShield >= m_fMaxShield && !m_bDown)
		{
			StopTick();
			return;
		}

		float delay = m_fRegenDelay;
		if (m_bDown)
			delay = Math.Max(m_fBrokenDelay, m_fRegenDelay);

		if (GetTime() - m_fLastHitTime < delay)
			return;

		m_fShield = Math.Min(m_fShield + Math.Max(m_fRegenPerSecond, 0) * TICK_MS * 0.001, m_fMaxShield);

		if (m_bDown && m_fShield >= m_fMaxShield * Math.Clamp(m_fReactivateAt, 0, 1))
		{
			m_bDown = false;
			OnRepShield();
		}

		Replication.BumpMe();
	}

	//==================================================================================================
	// EFFECTS (local, never on a dedicated server)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_Hit(vector position)
	{
		PlayHit(position);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayHit(vector position)
	{
		if (System.IsConsoleApp())
			return;

		SpawnParticle(m_sHitParticle, position);
		PlaySound(m_sHitSoundEvent, position);
		FlashBubble();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on clients when the replicated values change, and on the server by hand.
	protected void OnRepShield()
	{
		bool wasDown = m_bLastRepDown;
		m_bLastRepDown = m_bDown;
		m_fLastRepShield = m_fShield;

		if (System.IsConsoleApp())
			return;

		if (m_bDown && !wasDown)
		{
			vector center = GetCenterWorld();
			SpawnParticle(m_sBreakParticle, center);
			PlaySound(m_sBreakSoundEvent, center);
		}
		else if (!m_bDown && wasDown)
		{
			vector center = GetCenterWorld();
			SpawnParticle(m_sRestoreParticle, center);
			PlaySound(m_sRestoreSoundEvent, center);
			FlashBubble();
		}

		UpdateBubbleVisibility(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnParticle(ResourceName particle, vector position)
	{
		if (particle.IsEmpty())
			return;

		ParticleEffectEntitySpawnParams params = new ParticleEffectEntitySpawnParams();
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = position;
		ParticleEffectEntity.SpawnParticleEffect(particle, params);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaySound(string eventName, vector position)
	{
		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = position;
		BFA_MechWeaponModuleComponent.BFA_MechPlaySound(m_sSoundProject, eventName, mat);
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateBubble()
	{
		if (m_pBubble || System.IsConsoleApp())
			return;

		if (m_sBubblePrefab.IsEmpty())
		{
			Print(string.Format("[CGS SHIELD] %1: no Bubble Prefab set - the shield works but is invisible.", GetOwner()), LogLevel.WARNING);
			return;
		}

		Resource resource = Resource.Load(m_sBubblePrefab);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[CGS SHIELD] %1: bubble prefab could not be loaded: %2", GetOwner(), m_sBubblePrefab), LogLevel.WARNING);
			return;
		}

		IEntity owner = GetOwner();
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = GetCenterWorld();

		m_pBubble = GetGame().SpawnEntityPrefabLocal(resource, owner.GetWorld(), params);
		if (!m_pBubble)
			return;

		// Measure the model BEFORE parenting it (once it is a child, bounds include the mech).
		// Many sphere models are not centred on their origin (a pendant bubble sits ~0.9 m up, at
		// chest height), so the bounds centre is placed on the bubble Center point.
		vector mins, maxs;
		m_pBubble.GetBounds(mins, maxs);
		vector boundsCenter = (mins + maxs) * 0.5;
		vector half = (maxs - mins) * 0.5;
		float measured = Math.Max(half[0], Math.Max(half[1], half[2]));

		float modelRadius = m_fBubbleModelRadius;
		if (modelRadius <= 0)
		{
			modelRadius = measured;
			if (modelRadius < 0.01 || modelRadius > 20)
			{
				Print(string.Format("[CGS SHIELD] %1: could not measure the bubble model (got %2 m) - using 1 m. Set Bubble Model Radius by hand.", GetOwner(), measured), LogLevel.WARNING);
				modelRadius = 1;
				boundsCenter = vector.Zero;
			}
		}
		if (modelRadius < 0.01)
			modelRadius = 1;

		float scale = GetRadius() / modelRadius;

		// The bubble is NOT parented to the mech: attaching a local entity to a replicated vehicle
		// is unreliable on multiplayer clients. Instead it follows the mech every frame (FollowMech).
		// Scale is baked into the matrix.
		m_aBubbleLocal[0] = Vector(scale, 0, 0);
		m_aBubbleLocal[1] = Vector(0, scale, 0);
		m_aBubbleLocal[2] = Vector(0, 0, scale);
		m_aBubbleLocal[3] = owner.CoordToLocal(GetCenterWorld()) - boundsCenter * scale;
		FollowMech(owner);

		// Follow every frame through the game's call queue: on multiplayer clients a vehicle's
		// component frame events are not reliable, the call queue always runs.
		GetGame().GetCallqueue().CallLater(FollowTick, 0, true);

		if (m_pBubble.GetPhysics() || m_pBubble.FindComponent(RplComponent))
			Print(string.Format("[CGS SHIELD] %1: bubble prefab %2 has physics or an RplComponent - use a prefab with only a MeshObject, or it will collide with the mech.", GetOwner(), m_sBubblePrefab), LogLevel.WARNING);

		Print(string.Format("[CGS SHIELD] %1: bubble spawned, model radius %2 m (centre offset %5), scale %3, always visible %4", GetOwner(), modelRadius, scale, m_bBubbleAlwaysVisible, boundsCenter));

		UpdateBubbleVisibility(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void FlashBubble()
	{
		if (!m_pBubble || m_bBubbleAlwaysVisible)
			return;

		UpdateBubbleVisibility(true);
		GetGame().GetCallqueue().Remove(EndFlash);
		GetGame().GetCallqueue().CallLater(EndFlash, (int)(Math.Max(m_fBubbleFlashTime, 0.05) * 1000), false);
	}

	//------------------------------------------------------------------------------------------------
	protected void EndFlash()
	{
		UpdateBubbleVisibility(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateBubbleVisibility(bool flashing)
	{
		if (!m_pBubble)
			return;

		bool visible = IsUp() && (m_bBubbleAlwaysVisible || flashing);
		if (visible)
			m_pBubble.SetFlags(EntityFlags.VISIBLE, true);
		else
			m_pBubble.ClearFlags(EntityFlags.VISIBLE, true);
	}

	//==================================================================================================
	// LIFETIME
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_fShield = Math.Max(m_fMaxShield, 0);
		m_bDown = false;
		m_fLastRepShield = m_fShield;
		m_bLastRepDown = false;

		if (!s_aShieldedMechs.Contains(owner))
			s_aShieldedMechs.Insert(owner);

		// Bubble on the first frame, after the mech has finished spawning its parts.
		if (!System.IsConsoleApp())
			SetEventMask(owner, EntityEvent.FRAME);

		Print(string.Format("[CGS SHIELD] %1: shield %2 pts, radius %3 m", owner, m_fMaxShield, GetRadius()));
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bBubbleTried)
		{
			m_bBubbleTried = true;
			ClearEventMask(owner, EntityEvent.FRAME);
			CreateBubble();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FollowTick()
	{
		FollowMech(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the (unparented) bubble on the mech.
	protected void FollowMech(IEntity owner)
	{
		if (!m_pBubble || !owner)
			return;

		vector ownerMat[4];
		owner.GetWorldTransform(ownerMat);
		vector world[4];
		Math3D.MatrixMultiply4(ownerMat, m_aBubbleLocal, world);
		m_pBubble.SetWorldTransform(world);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			queue.Remove(ServerTick);
			queue.Remove(EndFlash);
			queue.Remove(FollowTick);
		}

		s_aShieldedMechs.RemoveItem(owner);

		if (m_pBubble)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_pBubble);
			m_pBubble = null;
		}

		super.OnDelete(owner);
	}
}
