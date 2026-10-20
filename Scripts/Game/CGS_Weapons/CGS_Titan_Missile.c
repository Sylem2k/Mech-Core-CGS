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
//!  - GUIDANCE: Crosshair (lock what is aimed at), Laser (fly to a friendly target designator
//!    spot, even one the shooter can't see) or Both (laser spot if there is one, else crosshair).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "CGS Titan Missile: homing missiles that lock what the crosshair is on, optional top-attack loft. Point defense can shoot them down.")]
class CGS_Titan_MissileClass : BFA_MechRocketLauncherModuleComponentClass
{
}

//------------------------------------------------------------------------------------------------
enum CGS_EMissileGuidance
{
	CROSSHAIR = 0,  // lock what the crosshair is on at launch
	LASER = 1,      // fly to a friendly laser spot (CGS Target Designator)
	BOTH = 2        // laser spot when there is one, otherwise the crosshair lock
}

//------------------------------------------------------------------------------------------------
//! One homing missile in flight. Server only.
class CGS_TitanMissileInFlight : BFA_MechRocketInFlight
{
	CGS_LaserSpotComponent m_pSpot;  // laser spot it follows (null = none)
	IEntity m_pTarget;        // locked object (root), null = fly to m_vTargetPoint
	vector m_vTargetLocal;    // aim spot on the target, in the target's own space
	vector m_vTargetPoint;    // world aim point when nothing is locked
}

//------------------------------------------------------------------------------------------------
//! A block of launch tubes laid out in rows and columns (one face of a rocket pod).
//! Place First Tube on the top-left tube mouth; the rest are worked out from the spacing.
[BaseContainerProps()]
class CGS_TubeGrid
{
	[Attribute(desc: "FIRST TUBE: place it on the TOP-LEFT tube mouth of this block with the gizmo. Blue (Z) axis points out of the tube, red (X) to the right, green (Y) up.")]
	ref PointInfo m_FirstTube;

	[Attribute("2", UIWidgets.Auto, "Tubes across (columns) in this block.")]
	int m_iColumns;

	[Attribute("12", UIWidgets.Auto, "Tubes down (rows) in this block.")]
	int m_iRows;

	[Attribute("0.5", UIWidgets.Auto, "Distance from one column to the next, to the RIGHT along the point's red axis (model units, before the weapon's scale).")]
	float m_fColumnSpacing;

	[Attribute("0.5", UIWidgets.Auto, "Distance from one row to the next, DOWN along the point's green axis.")]
	float m_fRowSpacing;

	[Attribute("0", UIWidgets.Auto, "Staggered tubes: every second row is shifted right by this much (0 = straight grid).")]
	float m_fOddRowShift;

	int GetTubeCount()
	{
		return Math.Max(m_iColumns, 0) * Math.Max(m_iRows, 0);
	}
}

class CGS_Titan_Missile : BFA_MechRocketLauncherModuleComponent
{
	//------------------------------------------------------------------------------------------------
	//! Homing: no lead needed.
	override float GetLeadSpeed()
	{
		return m_fHudLeadSpeed;
	}

	// ---- Tubes -----------------------------------------------------------------------------
	[Attribute(desc: "TUBE GRIDS (rocket pods): one entry per block of tubes. Each missile leaves a different tube, so a salvo comes out all over the pod. Empty = uses Muzzle Point / Barrel Points like any weapon.", category: "Tubes")]
	protected ref array<ref CGS_TubeGrid> m_aTubeGrids;

	[Attribute("true", UIWidgets.CheckBox, "Random tube order (every tube fires once before any repeats). Off = tube by tube, top to bottom.", category: "Tubes")]
	protected bool m_bRandomTubeOrder;

	protected ref array<int> m_aTubeBag = {};  // tubes left in this cycle (global tube numbers)

	// ---- Homing ----------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "Missiles steer onto the target. Off = straight like the rocket launcher.", category: "Homing")]
	protected bool m_bHoming;

	[Attribute("true", UIWidgets.CheckBox, "Lock onto the object under the crosshair and follow it when it moves. Off = fly to the spot that was aimed at.", category: "Homing")]
	protected bool m_bLockTargets;

	[Attribute("70", UIWidgets.Auto, "How fast the missile can turn, in degrees per second. Higher = harder to dodge.", category: "Homing")]
	protected float m_fTurnRate;

	[Attribute("20", UIWidgets.Auto, "Metres flown straight out of the tube before it starts steering.", category: "Homing")]
	protected float m_fArmingDistance;

	[Attribute("0", UIWidgets.ComboBox, "What the missiles fly to. CROSSHAIR = lock what is aimed at. LASER = a friendly target designator spot in front of the mech (it can be behind a hill). BOTH = the laser spot if there is one, otherwise the crosshair lock.", enums: ParamEnumArray.FromEnum(CGS_EMissileGuidance), category: "Homing")]
	protected CGS_EMissileGuidance m_eGuidance;

	[Attribute("90", UIWidgets.Auto, "Laser: a spot must be inside this cone (full angle, degrees) in front of the launcher / the missile to be picked up.", category: "Homing")]
	protected float m_fLaserConeDeg;

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
		vector start;
		if (GetTubeMuzzle(start, direction))
			ApplyFireAtAimPoint(start, direction);
		else
			start = GetMuzzle(direction);
		vector aimDirection = direction;

		// What is the crosshair on? (Straight line from the muzzle along the aim.)
		IEntity lockHit;
		vector lockPoint;
		TraceSegment(start, start + aimDirection * Math.Max(m_fRange, 10.0), m_pMechOwner, lockHit, lockPoint);

		CGS_TitanMissileInFlight missile = new CGS_TitanMissileInFlight();
		missile.m_vTargetPoint = lockPoint;

		// Laser guidance: a friendly spot in front of the launcher wins.
		CGS_LaserSpotComponent laserSpot;
		if (m_bHoming && m_eGuidance != CGS_EMissileGuidance.CROSSHAIR)
			laserSpot = CGS_LaserSpotComponent.FindBestSpot(CGS_LaserSpotComponent.FactionOf(m_pMechOwner), start, aimDirection, LaserRange(), m_fLaserConeDeg);

		if (laserSpot)
		{
			missile.m_pSpot = laserSpot;
			lockPoint = laserSpot.GetSpot();
			missile.m_vTargetPoint = lockPoint;
			lockHit = laserSpot.GetTarget();
		}
		else if (m_bLockTargets && lockHit && m_eGuidance != CGS_EMissileGuidance.LASER)
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
	protected float LaserRange()
	{
		return Math.Max(m_fRange, 100.0);
	}

	//------------------------------------------------------------------------------------------------
	//! For the HUD: does this launcher use laser spots?
	bool UsesLaser()
	{
		return m_bHoming && m_eGuidance != CGS_EMissileGuidance.CROSSHAIR;
	}

	//------------------------------------------------------------------------------------------------
	//! For the HUD: the spot this launcher would fly to right now (null = none).
	CGS_LaserSpotComponent FindLaserSpotNow()
	{
		if (!UsesLaser() || !m_pMechOwner)
			return null;
		vector direction;
		vector start = GetMuzzleRaw(direction, false);
		return CGS_LaserSpotComponent.FindBestSpot(CGS_LaserSpotComponent.FactionOf(m_pMechOwner), start, direction, LaserRange(), m_fLaserConeDeg);
	}

	//------------------------------------------------------------------------------------------------
	//! World start and direction of the next tube. False = no tube grids set.
	protected bool GetTubeMuzzle(out vector start, out vector direction)
	{
		if (!m_aTubeGrids || m_aTubeGrids.IsEmpty())
			return false;

		int total = 0;
		foreach (CGS_TubeGrid grid : m_aTubeGrids)
		{
			if (grid && grid.m_FirstTube)
				total += grid.GetTubeCount();
		}
		if (total <= 0)
			return false;

		// Refill the bag when a cycle is done (or the setup changed).
		if (m_aTubeBag.IsEmpty())
		{
			for (int n = 0; n < total; n++)
				m_aTubeBag.Insert(n);
		}

		int pick = 0;
		if (m_bRandomTubeOrder)
			pick = Math.RandomInt(0, m_aTubeBag.Count());
		int tube = m_aTubeBag[pick];
		if (m_bRandomTubeOrder)
			m_aTubeBag.Remove(pick);        // order does not matter in a random bag
		else
			m_aTubeBag.RemoveOrdered(pick); // keep tube 0, 1, 2 ... order
		if (tube >= total)
			tube = 0;

		// Which grid / row / column.
		foreach (CGS_TubeGrid g : m_aTubeGrids)
		{
			if (!g || !g.m_FirstTube)
				continue;

			int count = g.GetTubeCount();
			if (tube >= count)
			{
				tube -= count;
				continue;
			}

			int columns = Math.Max(g.m_iColumns, 1);
			int row = tube / columns;
			int column = tube % columns;

			IEntity weaponEntity = GetOwner();
			vector weaponWorld[4];
			weaponEntity.GetWorldTransform(weaponWorld);

			g.m_FirstTube.Init(weaponEntity);
			vector tubeModel[4];
			g.m_FirstTube.GetModelTransform(tubeModel);

			float across = column * g.m_fColumnSpacing;
			if (row % 2 == 1)
				across += g.m_fOddRowShift;

			vector local = tubeModel[3] + tubeModel[0] * across - tubeModel[1] * (row * g.m_fRowSpacing);

			vector axis = tubeModel[2];
			direction = weaponWorld[0] * axis[0] + weaponWorld[1] * axis[1] + weaponWorld[2] * axis[2];
			if (direction.LengthSq() < 0.000001)
				direction = weaponWorld[2];
			direction.Normalize();

			start = weaponWorld[3] + weaponWorld[0] * local[0] + weaponWorld[1] * local[1] + weaponWorld[2] * local[2];
			start = start + direction * Math.Max(m_fMuzzleClearance, 0.0);
			return true;
		}

		return false;
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

		// Laser: follow the spot while it is on; laser off = keep flying to where it last was.
		if (m_eGuidance != CGS_EMissileGuidance.CROSSHAIR)
		{
			if (missile.m_pSpot && !missile.m_pSpot.IsLasing())
				missile.m_pSpot = null;
			// No spot yet (and no crosshair lock): look for one ahead of the missile.
			if (!missile.m_pSpot && !missile.m_pTarget)
				missile.m_pSpot = CGS_LaserSpotComponent.FindBestSpot(CGS_LaserSpotComponent.FactionOf(missile.m_pMechOwner), missile.m_vPosition, missile.m_vDirection, LaserRange(), m_fLaserConeDeg);
			if (missile.m_pSpot)
			{
				missile.m_vTargetPoint = missile.m_pSpot.GetSpot();
				missile.m_pTarget = null;
			}
		}

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
