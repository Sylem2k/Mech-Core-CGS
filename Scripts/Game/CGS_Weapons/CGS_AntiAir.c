//------------------------------------------------------------------------------------------------
//! CGS ANTI-AIR - automatic flak against aircraft (helicopters, gunships, anything flying).
//!  - Works on its own: no turret, no aiming, no gunner needed. It scans for HOSTILE vehicles
//!    flying at least Min Altitude above the ground within Range, leads them and fires flak.
//!  - Each shell bursts near the target after its travel time: explosive splash in Burst Radius.
//!    Accuracy gets worse with distance (Miss Radius + Miss Per 100 m), so far targets take
//!    more shells and close ones get shredded.
//!  - Fires in bursts (Burst Shots, then Burst Pause). Ammo, reload, heat and "destroyed part"
//!    rules work like every MechCore weapon; a destroyed mech never fires.
//!  - TURRET: the gun visibly swings onto its target. Either the whole part turns, or split the
//!    model into pieces: a turret piece in this part's slot Turret Slot (turns left / right) and a
//!    gun piece in the turret's slot Gun Slot (tilts up / down). Shots leave the gun piece.
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

	// ---- Area denial ---------------------------------------------------------------------------
	[Attribute("4", UIWidgets.Auto, "AREA DENIAL: spread multiplier when it first engages a target. The flak walks in from here to x1 over Settle Time - a passing aircraft gets a wall of scattered bursts, one that stays gets hit.", category: "Area Denial")]
	protected float m_fStartSpreadScale;

	[Attribute("8", UIWidgets.Auto, "Seconds of engaging the same target until the flak is fully settled (x1 spread). 0 = accurate at once.", category: "Area Denial")]
	protected float m_fSettleTime;

	[Attribute("12", UIWidgets.Auto, "Targets slower than this (m/s) count as hovering: no speed penalty.", category: "Area Denial")]
	protected float m_fHoverSpeed;

	[Attribute("40", UIWidgets.Auto, "Each this-many m/s above Hover Speed adds x1 to the spread (fast movers are hard to hit).", category: "Area Denial")]
	protected float m_fSpeedPerExtraSpread;

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
	// ---- Turret ------------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "Turn the gun onto its target (visual, every machine).", category: "Turret")]
	protected bool m_bAnimateTurret;

	[Attribute("", UIWidgets.EditBox, "Slot on THIS part holding the turret piece (turns left / right). Empty = the whole part turns.", category: "Turret")]
	protected string m_sTurretSlot;

	[Attribute("", UIWidgets.EditBox, "Slot on the TURRET piece holding the gun piece (tilts up / down). Empty = the turret piece tilts too.", category: "Turret")]
	protected string m_sGunSlot;

	[Attribute("120", UIWidgets.Auto, "Turn speed in degrees per second.", category: "Turret")]
	protected float m_fTurnSpeed;

	[Attribute("-5", UIWidgets.Auto, "Lowest gun elevation in degrees.", category: "Turret")]
	protected float m_fMinElevation;

	[Attribute("85", UIWidgets.Auto, "Highest gun elevation in degrees.", category: "Turret")]
	protected float m_fMaxElevation;

	[Attribute("4", UIWidgets.Auto, "Seconds without a target before the turret turns back to rest. 0 = stay where it is.", category: "Turret")]
	protected float m_fReturnToRestTime;

	[Attribute("0", UIWidgets.Auto, "Shots start this many metres in front of the gun piece's origin, along its forward (blue) axis. 0 = use Muzzle Point as usual.", category: "Turret")]
	protected float m_fBarrelLength;

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
	protected float m_fEngageStart;

	// Turret (every machine)
	protected static ref array<CGS_AntiAirModuleComponent> s_aVisual = {};
	protected static int s_iVisualHeartbeatMs;
	protected static int s_iLastVisualMs;
	protected bool m_bTurretBound;
	protected IEntity m_pYawPiece;
	protected IEntity m_pPitchPiece;          // may be the same entity as the yaw piece
	protected vector m_mYawRest[4];
	protected vector m_mPitchRest[4];
	protected float m_fYaw;
	protected float m_fElevation;
	protected vector m_vAimPoint;
	protected float m_fAimTime = -1000;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sModuleId == "WEAPON_MODULE")
			m_sModuleId = "ANTI_AIR";

		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);
		EnsureTick();

		if (m_bAnimateTurret && !s_aVisual.Contains(this))
			s_aVisual.Insert(this);
		EnsureVisualTick();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aAll.RemoveItem(this);
		s_aVisual.RemoveItem(this);
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
			if (m_pTarget != previous)
				m_fEngageStart = now;
			if (m_bDebug && m_pTarget != previous)
				Print(string.Format("[CGS ANTI-AIR] %1 target: %2", m_sHardpointName, m_pTarget));
		}

		if (m_pTarget)
		{
			m_vAimPoint = PredictAimPoint(GetShotStart());
			m_fAimTime = now;
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

		vector start = GetShotStart();

		vector mins, maxs;
		m_pTarget.GetWorldBounds(mins, maxs);
		vector targetPos = (mins + maxs) * 0.5;

		float speed = Math.Max(m_fShellSpeed, 50);
		float distance = vector.Distance(start, targetPos);
		vector lead = PredictAimPoint(start);
		float flight = vector.Distance(start, lead) / speed;

		float miss = (m_fMissRadius + distance * 0.01 * m_fMissPer100m) * GetSpreadScale();
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
	//! AREA DENIAL: wide at first and against fast movers, tight on anything that hangs around.
	protected float GetSpreadScale()
	{
		float scale = 1;

		if (m_fSettleTime > 0)
		{
			float settled = Math.Clamp((Now() - m_fEngageStart) / m_fSettleTime, 0, 1);
			scale = Math.Max(m_fStartSpreadScale, 1) + (1 - Math.Max(m_fStartSpreadScale, 1)) * settled;
		}

		if (m_pTarget && m_fSpeedPerExtraSpread > 0)
		{
			Physics physics = m_pTarget.GetPhysics();
			if (physics)
			{
				float speed = physics.GetVelocity().Length();
				if (speed > m_fHoverSpeed)
					scale += (speed - m_fHoverSpeed) / m_fSpeedPerExtraSpread;
			}
		}

		return scale;
	}

	//------------------------------------------------------------------------------------------------
	//! Where to aim so a shell meets the target (its centre, led by its velocity).
	protected vector PredictAimPoint(vector start)
	{
		if (!m_pTarget)
			return m_vAimPoint;

		vector mins, maxs;
		m_pTarget.GetWorldBounds(mins, maxs);
		vector targetPos = (mins + maxs) * 0.5;

		vector velocity = vector.Zero;
		Physics physics = m_pTarget.GetPhysics();
		if (physics)
			velocity = physics.GetVelocity();

		float speed = Math.Max(m_fShellSpeed, 50);
		float flight = vector.Distance(start, targetPos) / speed;
		vector lead = targetPos + velocity * flight;
		flight = vector.Distance(start, lead) / speed;
		return targetPos + velocity * flight;
	}

	//------------------------------------------------------------------------------------------------
	//! Shot start: the gun piece's barrel end when set, else the normal muzzle.
	protected vector GetShotStart()
	{
		if (m_fBarrelLength > 0 && BindTurret() && m_pPitchPiece)
			return m_pPitchPiece.CoordToParent(Vector(0, 0, m_fBarrelLength));

		vector direction;
		return GetMuzzle(direction);
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: the turret follows the shots it sees (server: its target all the time).
	override void ClientOnShot(vector start, vector end)
	{
		super.ClientOnShot(start, end);
		m_vAimPoint = end;
		m_fAimTime = Now();
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureVisualTick()
	{
		if (s_iVisualHeartbeatMs != 0 && System.GetTickCount() - s_iVisualHeartbeatMs < 1000)
			return;
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().Remove(StaticVisualTick);
		GetGame().GetCallqueue().CallLater(StaticVisualTick, 0, true);
		s_iVisualHeartbeatMs = System.GetTickCount();
		s_iLastVisualMs = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StaticVisualTick()
	{
		s_iVisualHeartbeatMs = System.GetTickCount();
		if (!GetGame() || !GetGame().GetWorld())
			return;

		int now = GetGame().GetWorld().GetWorldTime();
		float dt = 0;
		if (s_iLastVisualMs > 0 && now > s_iLastVisualMs)
			dt = Math.Min((now - s_iLastVisualMs) * 0.001, 0.1);
		s_iLastVisualMs = now;
		if (dt <= 0)
			return;

		for (int i = s_aVisual.Count() - 1; i >= 0; i--)
		{
			CGS_AntiAirModuleComponent aa = s_aVisual[i];
			if (!aa || !aa.GetOwner())
			{
				s_aVisual.Remove(i);
				continue;
			}
			aa.TurretTick(dt);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindSlotEntity(IEntity holder, string slotName)
	{
		if (!holder || slotName.IsEmpty())
			return null;
		SlotManagerComponent slots = SlotManagerComponent.Cast(holder.FindComponent(SlotManagerComponent));
		if (!slots)
			return null;
		EntitySlotInfo slot = slots.GetSlotByName(slotName);
		if (!slot)
			return null;
		return slot.GetAttachedEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Find the turning pieces once and remember their rest pose.
	protected bool BindTurret()
	{
		if (m_bTurretBound)
			return m_pYawPiece != null;

		IEntity owner = GetOwner();
		if (!owner || !owner.GetParent())
			return false; // not attached to a mech yet

		IEntity yawPiece = owner;
		if (!m_sTurretSlot.IsEmpty())
		{
			yawPiece = FindSlotEntity(owner, m_sTurretSlot);
			if (!yawPiece)
				return false; // slot not spawned yet - try again next frame
		}

		IEntity pitchPiece = yawPiece;
		if (!m_sGunSlot.IsEmpty())
		{
			pitchPiece = FindSlotEntity(yawPiece, m_sGunSlot);
			if (!pitchPiece)
				return false;
		}

		m_pYawPiece = yawPiece;
		m_pPitchPiece = pitchPiece;
		m_pYawPiece.GetLocalTransform(m_mYawRest);
		m_pPitchPiece.GetLocalTransform(m_mPitchRest);
		m_bTurretBound = true;

		if (m_bDebug)
			Print(string.Format("[CGS ANTI-AIR] turret bound: turns %1, tilts %2", m_pYawPiece, m_pPitchPiece));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected float MoveTowards(float current, float target, float step)
	{
		float delta = target - current;
		if (Math.AbsFloat(delta) <= step)
			return target;
		if (delta > 0)
			return current + step;
		return current - step;
	}

	//------------------------------------------------------------------------------------------------
	//! Rotate the turret pieces toward the aim point (or back to rest), smoothly.
	protected void TurretTick(float dt)
	{
		if (!m_bAnimateTurret || !BindTurret())
			return;

		float wantYaw = 0;
		float wantElevation = 0;
		bool aiming = m_fAimTime > 0 && (m_fReturnToRestTime <= 0 || Now() - m_fAimTime < m_fReturnToRestTime);
		if (!aiming && m_fReturnToRestTime <= 0)
			return; // hold the last pose

		if (aiming)
		{
			// Yaw: in the yaw piece's parent space, measured in its rest forward / right plane.
			IEntity yawParent = m_pYawPiece.GetParent();
			vector origin = m_pYawPiece.GetOrigin();
			vector dir = m_vAimPoint - origin;
			if (yawParent)
				dir = yawParent.VectorToLocal(dir);
			vector fwd = m_mYawRest[2].Normalized();
			vector right = m_mYawRest[0].Normalized();
			vector up = m_mYawRest[1].Normalized();
			wantYaw = Math.Atan2(vector.Dot(dir, right), vector.Dot(dir, fwd)) * Math.RAD2DEG;

			// Elevation: angle above the rest forward / right plane.
			float flat = Math.Sqrt(Math.Pow(vector.Dot(dir, right), 2) + Math.Pow(vector.Dot(dir, fwd), 2));
			wantElevation = Math.Atan2(vector.Dot(dir, up), Math.Max(flat, 0.001)) * Math.RAD2DEG;
			wantElevation = Math.Clamp(wantElevation, m_fMinElevation, m_fMaxElevation);
		}

		// Shortest way round for yaw.
		float yawDelta = wantYaw - m_fYaw;
		while (yawDelta > 180)
			yawDelta -= 360;
		while (yawDelta < -180)
			yawDelta += 360;

		float step = Math.Max(m_fTurnSpeed, 1) * dt;
		m_fYaw = MoveTowards(m_fYaw, m_fYaw + yawDelta, step);
		m_fElevation = MoveTowards(m_fElevation, wantElevation, step);

		ApplyPose();
	}

	//------------------------------------------------------------------------------------------------
	//! Rotate a rest pose: yaw turns forward toward right, elevation tilts forward toward up.
	protected void RotatePose(vector rest[4], float yawDeg, float elevationDeg, out vector result[4])
	{
		float cy = Math.Cos(yawDeg * Math.DEG2RAD);
		float sy = Math.Sin(yawDeg * Math.DEG2RAD);
		vector right = rest[0] * cy - rest[2] * sy;
		vector fwd = rest[2] * cy + rest[0] * sy;
		vector up = rest[1];

		float ce = Math.Cos(elevationDeg * Math.DEG2RAD);
		float se = Math.Sin(elevationDeg * Math.DEG2RAD);
		vector fwd2 = fwd * ce + up * se;
		vector up2 = up * ce - fwd * se;

		result[0] = right;
		result[1] = up2;
		result[2] = fwd2;
		result[3] = rest[3];
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPose()
	{
		vector mat[4];
		if (m_pPitchPiece == m_pYawPiece)
		{
			RotatePose(m_mYawRest, m_fYaw, m_fElevation, mat);
			m_pYawPiece.SetLocalTransform(mat);
			m_pYawPiece.Update();
			return;
		}

		RotatePose(m_mYawRest, m_fYaw, 0, mat);
		m_pYawPiece.SetLocalTransform(mat);
		m_pYawPiece.Update();

		vector gun[4];
		RotatePose(m_mPitchRest, 0, m_fElevation, gun);
		m_pPitchPiece.SetLocalTransform(gun);
		m_pPitchPiece.Update();
	}

	//------------------------------------------------------------------------------------------------
	override void UnbindFromHardpoint()
	{
		m_iShellToken++; // shells in the air no longer count
		m_pTarget = null;
		super.UnbindFromHardpoint();
	}
}
