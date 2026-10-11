//------------------------------------------------------------------------------------------------
//! CGS TITAN MISSILE - homing missile launcher for titans.
//! Built on the core rocket launcher, so it already has: real flight time, salvos, magazine and
//! reload, direct hit + splash with line of sight, Ammo Type / crew damage settings, and enemy
//! point defense can shoot the missiles down.
//! On top of that:
//!  - LOCK: at launch it locks onto whatever the crosshair is on (vehicle, mech, soldier) and
//!    follows it while it moves. Nothing under the crosshair = flies to that spot.
//!  - LOFT: missiles can leave the tubes pointing up and then turn down onto the target
//!    (top attack), like a big missile rack.
//!  - Turn rate decides how hard it can steer; slow missiles with low turn rate can be dodged.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "CGS Titan Missile: homing missiles that lock what the crosshair is on, optional top-attack loft. Point defense can shoot them down.")]
class CGS_Titan_MissileClass : BFA_MechRocketLauncherModuleComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One homing missile in flight. Server only.
class CGS_TitanMissileInFlight : BFA_MechRocketInFlight
{
	IEntity m_pTarget;        // locked object (root), null = fly to m_vTargetPoint
	vector m_vTargetLocal;    // aim spot on the target, in the target's own space
	vector m_vTargetPoint;    // world aim point when nothing is locked
}

class CGS_Titan_Missile : BFA_MechRocketLauncherModuleComponent
{
	// ---- Homing ----------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "Missiles steer onto the target. Off = straight like the rocket launcher.", category: "Homing")]
	protected bool m_bHoming;

	[Attribute("true", UIWidgets.CheckBox, "Lock onto the object under the crosshair and follow it when it moves. Off = fly to the spot that was aimed at.", category: "Homing")]
	protected bool m_bLockTargets;

	[Attribute("70", UIWidgets.Auto, "How fast the missile can turn, in degrees per second. Higher = harder to dodge.", category: "Homing")]
	protected float m_fTurnRate;

	[Attribute("20", UIWidgets.Auto, "Metres flown straight out of the tube before it starts steering.", category: "Homing")]
	protected float m_fArmingDistance;

	[Attribute("0", UIWidgets.Auto, "LOFT: missiles leave the tube this many degrees upward, then dive onto the target (top attack). 0 = straight at it. 30-45 looks good on a back rack.", category: "Homing")]
	protected float m_fLoftAngle;

	// ---- Runtime ---------------------------------------------------------------------------
	protected int m_iLastSteerMs;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sModuleId == "WEAPON_MODULE")
			m_sModuleId = "TITAN_MISSILE";
	}

	//------------------------------------------------------------------------------------------------
	//! Same as the rocket launcher, plus the lock and the loft.
	override protected void LaunchRocket(IEntity instigator)
	{
		vector direction;
		vector start = GetMuzzle(direction);
		vector aimDirection = direction;

		// What is the crosshair on? (Straight line from the muzzle along the aim.)
		IEntity lockHit;
		vector lockPoint;
		TraceSegment(start, start + aimDirection * Math.Max(m_fRange, 10.0), m_pMechOwner, lockHit, lockPoint);

		CGS_TitanMissileInFlight missile = new CGS_TitanMissileInFlight();
		missile.m_vTargetPoint = lockPoint;
		if (m_bLockTargets && lockHit)
		{
			IEntity root = lockHit.GetRootParent();
			if (!root)
				root = lockHit;
			missile.m_pTarget = root;
			missile.m_vTargetLocal = root.CoordToLocal(lockPoint);
		}

		// Loft: tilt the launch upward (only when homing will bring it back down).
		if (m_bHoming && m_fLoftAngle > 0)
		{
			vector angles = direction.VectorToAngles();
			angles[1] = Math.Clamp(angles[1] + m_fLoftAngle, -89, 89);
			direction = angles.AnglesToVector();
		}
		direction = ApplyConeSpread(direction, m_fSpreadDeg);
		direction.Normalize();

		missile.m_vPosition = start;
		missile.m_vDirection = direction;
		missile.m_fTravelled = 0;
		missile.m_pInstigator = instigator;
		missile.m_pMechOwner = m_pMechOwner;
		m_aRockets.Insert(missile);

		// Visual: one projectile flying at the missile's speed to where it will land. Damage comes
		// from the server flight below; the impact broadcast ends the visual where it really hit.
		missile.m_iShotId = BroadcastShot(start, lockPoint, lockHit != null, m_fRocketSpeed, m_fTrailLength, m_fTrailThickness, true);

		if (m_bDebug)
			Print(string.Format("[CGS TITAN MISSILE] LAUNCH %1 lock=%2 point=%3 rounds=%4", m_sHardpointName, missile.m_pTarget, lockPoint, m_iRoundsInMagazine));

		StartFlightTick();
	}

	//------------------------------------------------------------------------------------------------
	//! Own flight ticker: steer, then the rocket launcher's FlightTick moves the missiles and
	//! handles hits / range. (FlightTick is called directly, never handed to the call queue here.)
	override protected void StartFlightTick()
	{
		if (m_bFlightTickRunning)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_bFlightTickRunning = true;
		m_iLastFlightTickMs = GetWorldTimeMs();
		m_iLastSteerMs = 0;
		queue.CallLater(MissileFlightTick, 16, true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void StopFlightTick()
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(MissileFlightTick);

		m_bFlightTickRunning = false;
		m_iLastFlightTickMs = 0;
		m_iLastSteerMs = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void MissileFlightTick()
	{
		if (Replication.IsServer() && m_bHoming && !m_aRockets.IsEmpty())
		{
			int nowMs = GetWorldTimeMs();
			int lastMs = m_iLastSteerMs;
			if (lastMs <= 0 || nowMs < lastMs)
				lastMs = m_iLastFlightTickMs;
			float dt = Math.Clamp((nowMs - lastMs) * 0.001, 0.001, 0.1);
			m_iLastSteerMs = nowMs;

			float maxTurnRad = Math.Max(m_fTurnRate, 0) * Math.DEG2RAD * dt;

			foreach (BFA_MechRocketInFlight rocket : m_aRockets)
			{
				CGS_TitanMissileInFlight missile = CGS_TitanMissileInFlight.Cast(rocket);
				if (missile)
					Steer(missile, maxTurnRad);
			}
		}

		FlightTick();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(MissileFlightTick);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Steer(CGS_TitanMissileInFlight missile, float maxTurnRad)
	{
		if (missile.m_fTravelled < m_fArmingDistance || maxTurnRad <= 0)
			return;

		vector targetPos = missile.m_vTargetPoint;
		if (missile.m_pTarget)
			targetPos = missile.m_pTarget.CoordToParent(missile.m_vTargetLocal);

		vector toTarget = targetPos - missile.m_vPosition;
		float distance = toTarget.Length();
		if (distance < 0.5)
			return;

		vector desired = toTarget * (1.0 / distance);
		vector current = missile.m_vDirection;

		float cosAngle = Math.Clamp(vector.Dot(current, desired), -1, 1);
		float angle = Math.Acos(cosAngle);
		if (angle < 0.0001)
			return;

		// Turn toward the target, at most maxTurnRad this tick.
		float t = Math.Min(maxTurnRad / angle, 1.0);
		vector steered = current * (1.0 - t) + desired * t;
		if (steered.LengthSq() < 0.000001)
			steered = desired;

		steered.Normalize();
		missile.m_vDirection = steered;
	}
}
