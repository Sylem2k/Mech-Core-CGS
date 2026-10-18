//------------------------------------------------------------------------------------------------
//! CGS MELEE - swords, claws, fists, chainblades on a mech arm.
//!  - PLAYERS: press the weapon group's fire key to swing.
//!  - AI: swings by itself when a hostile is within Reach in front of the arm (the AI gunner's
//!    normal "fire" on this group is ignored, so it never swings at things far away).
//!  - The swing is a scripted arc: the arm's mount is turned by an offset on top of where it
//!    aims - wind-up, strike, recover. Works on IK arms and rigid arms, no new animations.
//!  - Damage is checked on the SERVER along the blade line (from the arm's pivot out to Reach)
//!    during the strike; every object is hit once per swing. Soldiers are knocked down.
//!  - Heat, ammo (none by default), "destroyed" and "dead mech" rules work like every weapon.
//! Put it on the sword / blade part (or the arm part itself) and bind it to a hardpoint whose
//! Aim Mount is the arm (or set Swing Mount Id).
enum CGS_EMeleeDamageType
{
	MELEE = 0,
	KINETIC = 1,
	EXPLOSIVE = 2,
	COLLISION = 3,
	FRAGMENTATION = 4
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "CGS Melee: sword / claw / fist swing on a mech arm. Players swing with the fire key, AI swings when enemies are in reach.")]
class CGS_MeleeModuleComponentClass : BFA_MechWeaponModuleComponentClass
{
}

class CGS_MeleeModuleComponent : BFA_MechWeaponModuleComponent
{
	static const int EVENT_SWING = 80; // value = 1 when this swing is mirrored

	// ---- Swing -------------------------------------------------------------------------------
	[Attribute("", UIWidgets.EditBox, "Mount Id of the arm that swings (e.g. ARM_R). Empty = the Aim Mount of the hardpoint this weapon is installed on.", category: "Swing")]
	protected string m_sSwingMountId;

	[Attribute("0.45", UIWidgets.Auto, "Seconds to raise the weapon before the strike.", category: "Swing")]
	protected float m_fWindupTime;

	[Attribute("0.3", UIWidgets.Auto, "Seconds of the strike itself (damage is dealt during this part).", category: "Swing")]
	protected float m_fStrikeTime;

	[Attribute("0.5", UIWidgets.Auto, "Seconds to return to the aim after the strike.", category: "Swing")]
	protected float m_fRecoverTime;

	[Attribute("1.2", UIWidgets.Auto, "Extra seconds after a swing before the next one can start.", category: "Swing")]
	protected float m_fCooldown;

	[Attribute("70 25 0", UIWidgets.Auto, "Where the strike STARTS: yaw degrees (+ = right), pitch degrees (+ = up), PUSH metres (+ = arm pushed forward along its aim, - = pulled back). Horizontal slash: 70 25 0 -> -70 -15 0. Overhead chop: 0 70 0 -> 0 -40 0. Punch: 5 5 -0.6 -> 0 -3 1.5.", category: "Swing")]
	protected vector m_vStrikeFrom;

	[Attribute("-70 -15 0", UIWidgets.Auto, "Where the strike ENDS: yaw, pitch degrees, push metres (see Strike From).", category: "Swing")]
	protected vector m_vStrikeTo;

	[Attribute("true", UIWidgets.CheckBox, "Every second swing goes the other way (back-hand), so repeated slashes look natural.", category: "Swing")]
	protected bool m_bAlternate;

	// ---- Reach / hits ------------------------------------------------------------------------
	[Attribute("14", UIWidgets.Auto, "How far the blade reaches from the arm's pivot (shoulder), in metres. Also how close the AI must be to swing.", category: "Reach")]
	protected float m_fReach;

	[Attribute("4", UIWidgets.Auto, "Where the blade starts along the arm, in metres from the pivot (nothing closer is hit).", category: "Reach")]
	protected float m_fBladeStart;

	[Attribute("1.5", UIWidgets.Auto, "Blade thickness: radius around the blade line that counts as a hit, in metres.", category: "Reach")]
	protected float m_fBladeRadius;

	[Attribute("6", UIWidgets.Auto, "Most objects one swing can hit (cleave).", category: "Reach")]
	protected int m_iMaxHitsPerSwing;

	[Attribute("false", UIWidgets.CheckBox, "Also hit friendly soldiers and vehicles.", category: "Reach")]
	protected bool m_bHitFriendlies;

	// ---- Damage ------------------------------------------------------------------------------
	[Attribute("150", UIWidgets.Auto, "Damage to a soldier on foot (100+ usually kills).", category: "Damage")]
	protected float m_fInfantryDamage;

	[Attribute("400", UIWidgets.Auto, "Damage to a vehicle or mech.", category: "Damage")]
	protected float m_fVehicleDamage;

	[Attribute("0", UIWidgets.ComboBox, "Damage type against soldiers.", enums: ParamEnumArray.FromEnum(CGS_EMeleeDamageType), category: "Damage")]
	protected CGS_EMeleeDamageType m_eInfantryDamageType;

	[Attribute("2", UIWidgets.ComboBox, "Damage type against vehicles (vehicle armour multipliers apply). EXPLOSIVE gets through most vehicle armour.", enums: ParamEnumArray.FromEnum(CGS_EMeleeDamageType), category: "Damage")]
	protected CGS_EMeleeDamageType m_eVehicleDamageType;

	[Attribute("true", UIWidgets.CheckBox, "Knock soldiers down (ragdoll) and throw them.", category: "Damage")]
	protected bool m_bKnockDown;

	[Attribute("10", UIWidgets.Auto, "Throw speed (m/s) for soldiers and server-driven vehicles that are hit, away from the mech.", category: "Damage")]
	protected float m_fKnockback;

	// ---- AI ----------------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "AI crews swing automatically when a hostile is in reach.", category: "AI")]
	protected bool m_bAutoSwingAI;

	[Attribute("60", UIWidgets.Auto, "AI only swings at targets within this many degrees of where the arm faces (left/right).", category: "AI")]
	protected float m_fAIArc;

	// ---- Effects -----------------------------------------------------------------------------
	[Attribute("", UIWidgets.EditBox, "Swoosh sound event at the start of the strike (in Effects > Sound Project). Empty = the Fire Sound Event.", category: "Effects")]
	protected string m_sSwingSoundEvent;

	[Attribute("", UIWidgets.EditBox, "Sound event when the blade hits something. Empty = silent.", category: "Effects")]
	protected string m_sHitSoundEvent;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Particle where the blade hits (sparks, blood). Empty = none.", "ptc", category: "Effects")]
	protected ResourceName m_sHitParticle;

	// ---- Diagnostics -------------------------------------------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Print swings and hits.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime -----------------------------------------------------------------------------
	protected static ref array<CGS_MeleeModuleComponent> s_aAll = {};
	protected static int s_iHeartbeatMs;
	protected static ref array<IEntity> s_aCollect = {};

	protected float m_fReadyAt;
	protected float m_fNextAICheck;
	protected int m_iSwingCount;
	protected IEntity m_pInstigator;

	// Server swing (hits)
	protected bool m_bServerSwing;
	protected float m_fServerStart;
	protected bool m_bServerMirror;
	protected ref array<IEntity> m_aHitThisSwing = {};

	// Local swing (visuals, every machine that shows effects)
	protected bool m_bLocalSwing;
	protected float m_fLocalStart;
	protected bool m_bLocalMirror;
	protected bool m_bLocalStrikeSound;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sModuleId == "WEAPON_MODULE")
			m_sModuleId = "MELEE";

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
		return m_fReach;
	}

	//------------------------------------------------------------------------------------------------
	protected float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	protected float SwingLength()
	{
		return Math.Max(m_fWindupTime, 0) + Math.Max(m_fStrikeTime, 0.05) + Math.Max(m_fRecoverTime, 0);
	}

	//==================================================================================================
	// TICK
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTick()
	{
		if (s_iHeartbeatMs != 0 && System.GetTickCount() - s_iHeartbeatMs < 1000)
			return;
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().Remove(StaticTick);
		GetGame().GetCallqueue().CallLater(StaticTick, 0, true); // every frame: smooth swings
		s_iHeartbeatMs = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StaticTick()
	{
		s_iHeartbeatMs = System.GetTickCount();

		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			CGS_MeleeModuleComponent melee = s_aAll[i];
			if (!melee || !melee.GetOwner())
			{
				s_aAll.Remove(i);
				continue;
			}
			melee.Tick();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		float now = Now();

		if (m_bLocalSwing)
			LocalSwingTick(now);

		if (!Replication.IsServer())
			return;

		if (m_bServerSwing)
			ServerSwingTick(now);
		else
			ServerAITick(now);
	}

	//==================================================================================================
	// TRIGGER
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Players swing on the trigger. An AI gunner "holding fire" on this group is ignored - the
	//! AI swings from ServerAITick when something is actually in reach.
	override protected void OnTriggerDown(IEntity instigator)
	{
		if (IsAI(instigator) || !CanSwingNow())
			return;
		m_pInstigator = instigator;
		TryFireShot(instigator);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerHeld(IEntity instigator)
	{
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerUp(IEntity instigator)
	{
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnFirePulse(IEntity instigator)
	{
		if (IsAI(instigator) || !CanSwingNow())
			return;
		m_pInstigator = instigator;
		TryFireShot(instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! Checked BEFORE TryFireShot, so heat / ammo are only spent when a swing really starts.
	protected bool CanSwingNow()
	{
		if (m_bServerSwing || Now() < m_fReadyAt)
			return false;
		return m_pMechOwner && !BFA_MechExtension.IsMechDestroyed(m_pMechOwner);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAI(IEntity instigator)
	{
		return instigator && BFA_MechMovementComponent.IsAIControlled(instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Called after the heat / ammo / destroyed checks passed: start a swing.
	override protected void FireShot(IEntity instigator)
	{
		float now = Now();
		if (m_bServerSwing || now < m_fReadyAt)
			return;
		if (!m_pMechOwner || BFA_MechExtension.IsMechDestroyed(m_pMechOwner))
			return;

		m_iSwingCount++;
		bool mirror = m_bAlternate && (m_iSwingCount % 2 == 0);

		m_bServerSwing = true;
		m_fServerStart = now;
		m_bServerMirror = mirror;
		m_aHitThisSwing.Clear();
		m_fReadyAt = now + SwingLength() + Math.Max(m_fCooldown, 0);

		int mirrorValue = 0;
		if (mirror)
			mirrorValue = 1;
		BroadcastModuleEvent(EVENT_SWING, mirrorValue);

		// Dedicated servers don't receive their own module events: nothing to show there anyway.
		// A listen host / Workbench does, and animates in ClientOnModuleEvent.

		if (m_bDebug)
			Print(string.Format("[CGS MELEE] %1 swing %2 (mirror %3)", m_sHardpointName, m_iSwingCount, mirror));
	}

	//------------------------------------------------------------------------------------------------
	//! Fire rate gate: a swing takes its full length plus cooldown (handled in FireShot).
	override protected int GetShotIntervalMs()
	{
		return 50;
	}

	//==================================================================================================
	// SWING SHAPE
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected float Ease(float t)
	{
		t = Math.Clamp(t, 0, 1);
		return t * t * (3 - 2 * t);
	}

	//------------------------------------------------------------------------------------------------
	//! Swing offset (yaw, pitch degrees) at 'elapsed' seconds into the swing. strikeT = -1 when
	//! not in the strike, else 0..1 through the strike. Returns false when the swing is over.
	protected bool SwingOffset(float elapsed, bool mirror, out float yawOff, out float pitchOff, out float strikeT, out float push)
	{
		strikeT = -1;
		float w = Math.Max(m_fWindupTime, 0);
		float s = Math.Max(m_fStrikeTime, 0.05);
		float r = Math.Max(m_fRecoverTime, 0);

		vector from = m_vStrikeFrom;
		vector to = m_vStrikeTo;
		if (mirror)
		{
			from[0] = -from[0];
			to[0] = -to[0];
		}

		vector offset;
		if (elapsed < w)
		{
			offset = from * Ease(elapsed / Math.Max(w, 0.01));
		}
		else if (elapsed < w + s)
		{
			float u = (elapsed - w) / s;
			strikeT = u;
			offset = from + (to - from) * Ease(u);
		}
		else if (elapsed < w + s + r)
		{
			offset = to * (1.0 - Ease((elapsed - w - s) / Math.Max(r, 0.01)));
		}
		else
		{
			yawOff = 0;
			pitchOff = 0;
			push = 0;
			return false;
		}

		yawOff = offset[0];
		pitchOff = offset[1];
		push = offset[2];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected BFA_MechWeaponMountComponent GetMounts()
	{
		if (!m_pMechOwner)
			return null;
		return BFA_MechWeaponMountComponent.Cast(m_pMechOwner.FindComponent(BFA_MechWeaponMountComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected string GetSwingMountId()
	{
		if (!m_sSwingMountId.IsEmpty())
			return m_sSwingMountId;
		if (!m_pMechOwner || m_iHardpointIndex < 0)
			return "";

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(m_pMechOwner.FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints || !hardpoints.m_aHardpoints || m_iHardpointIndex >= hardpoints.m_aHardpoints.Count())
			return "";

		BFA_MechHardpointDefinition hardpoint = hardpoints.m_aHardpoints[m_iHardpointIndex];
		if (!hardpoint)
			return "";
		return hardpoint.m_sAimMountId;
	}

	//==================================================================================================
	// LOCAL VISUALS
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	override void ClientOnModuleEvent(int eventType, int value = 0)
	{
		super.ClientOnModuleEvent(eventType, value);

		if (eventType == EVENT_SWING)
		{
			m_bLocalSwing = true;
			m_fLocalStart = Now();
			m_bLocalMirror = value == 1;
			m_bLocalStrikeSound = false;
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void LocalSwingTick(float now)
	{
		BFA_MechWeaponMountComponent mounts = GetMounts();
		string mountId = GetSwingMountId();

		float yawOff;
		float pitchOff;
		float strikeT;
		float push;
		bool running = SwingOffset(now - m_fLocalStart, m_bLocalMirror, yawOff, pitchOff, strikeT, push);

		if (!running || !m_pMechOwner || BFA_MechExtension.IsMechDestroyed(m_pMechOwner))
		{
			m_bLocalSwing = false;
			if (mounts && !mountId.IsEmpty())
				mounts.SetMountSwing(mountId, false, 0, 0);
			return;
		}

		if (mounts && !mountId.IsEmpty())
			mounts.SetMountSwing(mountId, true, yawOff, pitchOff, push);

		if (strikeT >= 0 && !m_bLocalStrikeSound)
		{
			m_bLocalStrikeSound = true;
			string sound = m_sSwingSoundEvent;
			if (sound.IsEmpty() && m_FX)
				sound = m_FX.m_sFireSoundEvent;
			if (!sound.IsEmpty())
				PlayModuleSound(sound);
		}
	}

	//==================================================================================================
	// SERVER HITS
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Blade line right now: from the arm pivot toward the aim point, turned by the swing offset
	//! (in the mech's own frame: yaw around its up axis, pitch up/down).
	protected bool GetBlade(float yawOff, float pitchOff, out vector bladeFrom, out vector bladeTo, float push = 0)
	{
		BFA_MechWeaponMountComponent mounts = GetMounts();
		string mountId = GetSwingMountId();

		vector origin;
		vector aimPoint;
		if (!mounts || mountId.IsEmpty() || !mounts.GetMountAim(mountId, origin, aimPoint))
		{
			// No mount: swing from the weapon part itself, facing the mech's forward.
			origin = GetOwner().GetOrigin();
			aimPoint = origin + m_pMechOwner.GetTransformAxis(2) * 10;
		}

		vector dirMech = m_pMechOwner.VectorToLocal(aimPoint - origin);
		if (dirMech.LengthSq() < 0.0001)
			dirMech = vector.Forward;
		dirMech.Normalize();

		float yaw = Math.Atan2(dirMech[0], dirMech[2]) * Math.RAD2DEG + yawOff;
		float pitch = Math.Asin(Math.Clamp(dirMech[1], -1, 1)) * Math.RAD2DEG + pitchOff;
		pitch = Math.Clamp(pitch, -89, 89);

		float cp = Math.Cos(pitch * Math.DEG2RAD);
		vector swungMech = Vector(Math.Sin(yaw * Math.DEG2RAD) * cp, Math.Sin(pitch * Math.DEG2RAD), Math.Cos(yaw * Math.DEG2RAD) * cp);
		vector dirWorld = m_pMechOwner.VectorToParent(swungMech);

		bladeFrom = origin + dirWorld * (Math.Max(m_fBladeStart, 0) + push);
		bladeTo = origin + dirWorld * (Math.Max(m_fReach, m_fBladeStart + 0.5) + push);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerSwingTick(float now)
	{
		float yawOff;
		float pitchOff;
		float strikeT;
		float push;
		bool running = SwingOffset(now - m_fServerStart, m_bServerMirror, yawOff, pitchOff, strikeT, push);
		if (!running || !m_pMechOwner || BFA_MechExtension.IsMechDestroyed(m_pMechOwner))
		{
			m_bServerSwing = false;
			return;
		}

		if (strikeT < 0 || m_aHitThisSwing.Count() >= Math.Max(m_iMaxHitsPerSwing, 1))
			return;

		vector bladeFrom;
		vector bladeTo;
		if (!GetBlade(yawOff, pitchOff, bladeFrom, bladeTo, push))
			return;

		HitAlongBlade(bladeFrom, bladeTo);
	}

	//------------------------------------------------------------------------------------------------
	protected void HitAlongBlade(vector bladeFrom, vector bladeTo)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float radius = Math.Max(m_fBladeRadius, 0.2);
		float length = vector.Distance(bladeFrom, bladeTo);
		int samples = Math.Clamp((int)Math.Ceil(length / radius), 1, 40);

		s_aCollect.Clear();
		for (int i = 0; i <= samples; i++)
		{
			vector point = bladeFrom + (bladeTo - bladeFrom) * (i * (1.0 / samples));
			world.QueryEntitiesBySphere(point, radius, CollectEntity, null, EQueryEntitiesFlags.DYNAMIC);
		}

		Faction mechFaction = GetMechFaction();
		array<IEntity> found = {};
		foreach (IEntity entity : s_aCollect)
		{
			IEntity target = ResolveTarget(entity);
			if (target && !found.Contains(target))
				found.Insert(target);
		}
		s_aCollect.Clear();

		vector direction = (bladeTo - bladeFrom).Normalized();
		foreach (IEntity target : found)
		{
			if (m_aHitThisSwing.Count() >= Math.Max(m_iMaxHitsPerSwing, 1))
				break;
			if (m_aHitThisSwing.Contains(target))
				continue;
			if (!m_bHitFriendlies && IsFriendly(target, mechFaction))
				continue;
			if (!IsAliveEntity(target))
				continue;

			m_aHitThisSwing.Insert(target);
			HitTarget(target, direction);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectEntity(IEntity entity)
	{
		if (entity)
			s_aCollect.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A soldier on foot, or the root vehicle / mech of whatever part was touched. Never ourselves.
	protected IEntity ResolveTarget(IEntity entity)
	{
		if (!entity)
			return null;

		IEntity root = entity.GetRootParent();
		if (!root)
			root = entity;
		if (root == m_pMechOwner)
			return null;

		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character)
		{
			if (character.GetParent())
				return null; // inside a vehicle: the vehicle takes the hit
			return character;
		}

		if (Vehicle.Cast(root))
			return root;
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void HitTarget(IEntity target, vector bladeDirection)
	{
		vector away = target.GetOrigin() - m_pMechOwner.GetOrigin();
		away[1] = 0;
		if (away.LengthSq() < 0.01)
			away = bladeDirection;
		away.Normalize();

		ChimeraCharacter character = ChimeraCharacter.Cast(target);
		if (character)
		{
			DealDamage(target, m_fInfantryDamage, ToDamageType(m_eInfantryDamageType), target.GetOrigin() + vector.Up, away, m_pInstigator);

			if (m_bKnockDown)
			{
				CharacterControllerComponent controller = character.GetCharacterController();
				if (controller)
					controller.Ragdoll();
				vector throwVelocity = away * m_fKnockback + vector.Up * (m_fKnockback * 0.5);
				GetGame().GetCallqueue().CallLater(ThrowBody, 60, false, character, throwVelocity);
			}
		}
		else
		{
			DealDamage(target, m_fVehicleDamage, ToDamageType(m_eVehicleDamageType), target.GetOrigin(), away, m_pInstigator);

			// Shove vehicles the server drives (AI / empty). Player-driven ones only take damage.
			RplComponent rpl = RplComponent.Cast(target.FindComponent(RplComponent));
			Physics physics = target.GetPhysics();
			if (m_fKnockback > 0 && physics && physics.IsDynamic() && (!rpl || rpl.IsOwner()))
				physics.SetVelocity(physics.GetVelocity() + away * m_fKnockback + vector.Up * (m_fKnockback * 0.3));
		}

		vector hitPos = target.GetOrigin() + vector.Up;
		BroadcastShot(hitPos, hitPos, true, 0, 0.01, 0.01, false);

		if (m_bDebug)
			Print(string.Format("[CGS MELEE] %1 hit %2", m_sHardpointName, target));
	}

	//------------------------------------------------------------------------------------------------
	protected EDamageType ToDamageType(CGS_EMeleeDamageType type)
	{
		if (type == CGS_EMeleeDamageType.KINETIC)
			return EDamageType.KINETIC;
		if (type == CGS_EMeleeDamageType.EXPLOSIVE)
			return EDamageType.EXPLOSIVE;
		if (type == CGS_EMeleeDamageType.COLLISION)
			return EDamageType.COLLISION;
		if (type == CGS_EMeleeDamageType.FRAGMENTATION)
			return EDamageType.FRAGMENTATION;
		return EDamageType.MELEE;
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
	//! Every machine: a hit happened at 'end' - particle + sound. (No muzzle flash, no ammo.)
	override void ClientOnShot(vector start, vector end)
	{
		if (System.IsConsoleApp())
			return;

		if (!m_sHitParticle.IsEmpty())
		{
			ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
			Math3D.MatrixIdentity4(spawnParams.Transform);
			spawnParams.Transform[3] = end;
			ParticleEffectEntity.SpawnParticleEffect(m_sHitParticle, spawnParams);
		}

		if (!m_sHitSoundEvent.IsEmpty())
			PlayModuleSound(m_sHitSoundEvent);
	}

	//==================================================================================================
	// AI
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! [Server] AI crews swing when a hostile is in reach in front of the arm.
	protected void ServerAITick(float now)
	{
		if (!m_bAutoSwingAI || !m_bBoundToHardpoint || !m_pMechOwner)
			return;
		if (now < m_fReadyAt || now < m_fNextAICheck)
			return;
		m_fNextAICheck = now + 0.2;

		IEntity crew = BFA_MechCoreComponent.BFA_ResolveKiller(m_pMechOwner, null);
		if (!crew || !IsAI(crew))
			return;

		if (!HostileInReach())
			return;

		if (!CanSwingNow())
			return;
		m_pInstigator = crew;
		TryFireShot(crew);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HostileInReach()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		vector bladeFrom;
		vector bladeTo;
		if (!GetBlade(0, 0, bladeFrom, bladeTo))
			return false;

		vector origin = bladeFrom - (bladeTo - bladeFrom).Normalized() * Math.Max(m_fBladeStart, 0);
		vector facing = bladeTo - origin;
		facing[1] = 0;
		if (facing.LengthSq() < 0.0001)
			return false;
		facing.Normalize();

		float reach = Math.Max(m_fReach, 1);
		s_aCollect.Clear();
		world.QueryEntitiesBySphere(origin, reach, CollectEntity, null, EQueryEntitiesFlags.DYNAMIC);

		Faction mechFaction = GetMechFaction();
		bool found;
		float minDot = Math.Cos(Math.Clamp(m_fAIArc, 5, 180) * Math.DEG2RAD);
		foreach (IEntity entity : s_aCollect)
		{
			IEntity target = ResolveTarget(entity);
			if (!target || !IsHostile(target, mechFaction) || !IsAliveEntity(target))
				continue;

			vector to = target.GetOrigin() - origin;
			to[1] = 0;
			if (to.LengthSq() < 0.0001)
			{
				found = true;
				break;
			}
			if (vector.Dot(to.Normalized(), facing) >= minDot)
			{
				found = true;
				break;
			}
		}
		s_aCollect.Clear();
		return found;
	}

	//==================================================================================================
	// FACTIONS
	//==================================================================================================

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
	override void UnbindFromHardpoint()
	{
		if (m_bLocalSwing)
		{
			BFA_MechWeaponMountComponent mounts = GetMounts();
			string mountId = GetSwingMountId();
			if (mounts && !mountId.IsEmpty())
				mounts.SetMountSwing(mountId, false, 0, 0);
		}
		m_bLocalSwing = false;
		m_bServerSwing = false;
		super.UnbindFromHardpoint();
	}
}
