[ComponentEditorProps(category: "BFA MechCore", description: "Lets AI characters crew a mech: an AI driver follows its group's waypoints, AI weapon stations find, aim at and shoot hostile targets. Server only.")]
class BFA_MechAICrewComponentClass : ScriptGameComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Per-station AI gunner state (server).
class BFA_MechAIStationState
{
	string m_sStationId;
	IEntity m_pTarget;
	float m_fTargetTime;      // seconds the current target has been held
	float m_fBurstTimer;      // >0 = firing, <0 = pausing between bursts
	int m_iFiringGroup;       // group currently held down, 0 = none
	ref array<int> m_aAllFiring = {};   // Fire All Groups: every group held down right now
	float m_fRescanTimer;
}

//------------------------------------------------------------------------------------------------
//! AI crew brain for MechCore mechs.
//!
//! Vanilla AI can sit in the seats but cannot operate a walker: the mech's controls only read a
//! player's input. This component runs on the SERVER and drives the exact same server functions
//! the player path uses, so AI crews get aiming, two-arm convergence, weapon groups, modules and
//! multiplayer replication for free:
//!
//!  DRIVER (AI)   reads the AI group's current waypoint and steers toward it with
//!                BFA_MechMovementComponent.ServerSetDriverInput(). Stops inside the waypoint
//!                radius, turns on the spot for sharp turns, backs out when stuck.
//!  WEAPON STATIONS (AI)   every station that can aim (GUNNER, COMMANDER, ...): scans for the
//!                nearest visible hostile, sets the station aim point on it, picks a weapon group
//!                (rockets for vehicles, primary for infantry) and fires in bursts.
//!
//! Players are never overridden: a seat held by a player is skipped.
class BFA_MechAICrewComponent : ScriptGameComponent
{
	// ---- General ---------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "AI may drive this mech.", category: "AI Crew")]
	protected bool m_bAIDriver;

	[Attribute("true", UIWidgets.CheckBox, "AI may operate this mech's weapon stations.", category: "AI Crew")]
	protected bool m_bAIGunners;

	[Attribute("true", UIWidgets.CheckBox, "Several AI gunners spread over different targets when there is more than one (boss mechs). Off = everyone shoots the closest target.", category: "AI Crew")]
	protected bool m_bSpreadTargets;

	[Attribute("100", UIWidgets.EditBox, "Milliseconds between AI updates.", category: "AI Crew")]
	protected int m_iTickMs;

	[Attribute("false", UIWidgets.CheckBox, "Print AI crew decisions.", category: "AI Crew")]
	protected bool m_bDebug;

	// ---- Driver ----------------------------------------------------------------------------
	[Attribute("6", UIWidgets.Auto, "Minimum distance in metres at which a waypoint counts as reached.", category: "AI Driver")]
	protected float m_fArriveDistance;

	[Attribute("35", UIWidgets.Auto, "Heading error (degrees) above which the mech slows down to turn.", category: "AI Driver")]
	protected float m_fSlowTurnAngle;

	[Attribute("80", UIWidgets.Auto, "Heading error (degrees) above which the mech turns on the spot.", category: "AI Driver")]
	protected float m_fTurnInPlaceAngle;

	[Attribute("120", UIWidgets.Auto, "Sprint toward waypoints farther than this many metres (0 = never sprint).", category: "AI Driver")]
	protected float m_fSprintDistance;

	[Attribute("3", UIWidgets.Auto, "Seconds of no progress while driving forward before backing out.", category: "AI Driver")]
	protected float m_fStuckTime;

	[Attribute("true", UIWidgets.CheckBox, "With no waypoint, follow the squad leader when they are on foot (works for AI recruited into a player's squad).", category: "AI Driver")]
	protected bool m_bFollowLeader;

	[Attribute("15", UIWidgets.Auto, "Stop this many metres from the squad leader.", category: "AI Driver")]
	protected float m_fFollowDistance;

	[Attribute("true", UIWidgets.CheckBox, "With no waypoint or leader, move toward targets the AI stations are engaging.", category: "AI Driver")]
	protected bool m_bAdvanceOnTargets;

	[Attribute("150", UIWidgets.Auto, "When closing on a target, stop at this distance and fight.", category: "AI Driver")]
	protected float m_fAdvanceStopDistance;

	// ---- Combat movement -------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "AI driver manoeuvres to fight when enemies are seen (advance, strafe, back off), then resumes its waypoint.", category: "AI Combat Movement")]
	protected bool m_bCombatMovement;

	[Attribute("200", UIWidgets.Auto, "Advance until the enemy is closer than this (metres).", category: "AI Combat Movement")]
	protected float m_fCombatMaxRange;

	[Attribute("40", UIWidgets.Auto, "Back off when the enemy is closer than this (metres).", category: "AI Combat Movement")]
	protected float m_fCombatMinRange;

	[Attribute("1", UIWidgets.Slider, "Strafe input while engaging (0 = stand still).", params: "0 1 0.05", category: "AI Combat Movement")]
	protected float m_fCombatStrafe;

	[Attribute("2", UIWidgets.Auto, "Minimum seconds before changing strafe direction.", category: "AI Combat Movement")]
	protected float m_fStrafeSwitchMin;

	[Attribute("5", UIWidgets.Auto, "Maximum seconds before changing strafe direction.", category: "AI Combat Movement")]
	protected float m_fStrafeSwitchMax;

	[Attribute("15", UIWidgets.Auto, "Seconds to keep hunting an enemy after losing sight of it.", category: "AI Combat Movement")]
	protected float m_fCombatMemory;

	[Attribute("250", UIWidgets.Auto, "With a waypoint, fight only within this many metres of it (0 = no limit).", category: "AI Combat Movement")]
	protected float m_fCombatLeash;

	// ---- Gunners ---------------------------------------------------------------------------
	[Attribute("400", UIWidgets.Auto, "Maximum engagement range in metres.", category: "AI Gunners")]
	protected float m_fEngageRange;

	[Attribute("0.6", UIWidgets.Auto, "Seconds between target scans per station.", category: "AI Gunners")]
	protected float m_fScanInterval;

	[Attribute("0.5", UIWidgets.Auto, "Seconds to settle aim on a new target before firing.", category: "AI Gunners")]
	protected float m_fAimSettleTime;

	[Attribute("1.5", UIWidgets.Auto, "Burst length in seconds.", category: "AI Gunners")]
	protected float m_fBurstTime;

	[Attribute("1.0", UIWidgets.Auto, "Pause between bursts in seconds.", category: "AI Gunners")]
	protected float m_fBurstPause;

	[Attribute("true", UIWidgets.CheckBox, "Prefer non-primary groups (e.g. rockets) against vehicles.", category: "AI Gunners")]
	protected bool m_bHeavyWeaponsVsVehicles;

	[Attribute("false", UIWidgets.CheckBox, "BOSS: fire EVERY attack group at once (cannon arm, missile arm, pods ...) instead of picking one group per target.", category: "AI Gunners")]
	protected bool m_bFireAllGroups;

	// ---- Runtime ---------------------------------------------------------------------------
	protected BFA_MechCoreComponent m_pCore;
	protected BFA_MechMovementComponent m_pMovement;
	protected BFA_MechAimComponent m_pAim;
	protected BFA_MechWeaponComponent m_pRouter;
	protected BFA_MechHardpointComponent m_pHardpoints;

	protected ref array<ref BFA_MechAIStationState> m_aStations = {};
	protected ref array<IEntity> m_aQuery = {};
	protected bool m_bTickRunning;
	protected int m_iLastTickMs;

	// Driver state
	protected bool m_bDriverActive;
	protected float m_fTurnSign = 1.0;       // calibrated at runtime (engine turn direction)
	protected float m_fCalibYawError;
	protected float m_fCalibTimer;
	protected bool m_bTurnSignConfirmed;

	// Combat state
	protected IEntity m_pCombatTarget;
	protected IEntity m_pDriverScanTarget;
	protected vector m_vCombatLastKnown;
	protected float m_fCombatLastSeen;
	protected float m_fCombatScanTimer;
	protected float m_fStrafeTimer;
	protected float m_fStrafeDir = 1.0;
	protected bool m_bReturningToLeash;
	protected vector m_vLastPos;
	protected float m_fNoProgressTime;
	protected float m_fReverseTime;
	protected string m_sLastDriverStatus;
	protected float m_fDriverDiagTimer;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		// Staggered first target search (see TickDriver).
		m_fCombatScanTimer = Math.RandomFloat(0, Math.Max(m_fScanInterval, 0.1));

		if (!Replication.IsServer())
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			m_bTickRunning = true;
			queue.CallLater(Tick, Math.Max(m_iTickMs, 50), true);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
				queue.Remove(Tick);
		}

		m_bTickRunning = false;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ResolveComponents()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (!m_pCore)
			m_pCore = BFA_MechCoreComponent.Cast(owner.FindComponent(BFA_MechCoreComponent));
		if (!m_pMovement)
			m_pMovement = BFA_MechMovementComponent.Cast(owner.FindComponent(BFA_MechMovementComponent));
		if (!m_pAim)
			m_pAim = BFA_MechAimComponent.Cast(owner.FindComponent(BFA_MechAimComponent));
		if (!m_pRouter)
			m_pRouter = BFA_MechWeaponComponent.Cast(owner.FindComponent(BFA_MechWeaponComponent));
		if (!m_pHardpoints)
			m_pHardpoints = BFA_MechHardpointComponent.Cast(owner.FindComponent(BFA_MechHardpointComponent));

		return m_pCore != null;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetWorldTimeMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!Replication.IsServer() || !ResolveComponents())
			return;

		int nowMs = GetWorldTimeMs();
		float dt = 0.1;
		if (m_iLastTickMs > 0)
			dt = Math.Clamp((nowMs - m_iLastTickMs) * 0.001, 0.01, 0.5);
		m_iLastTickMs = nowMs;

		if (m_bAIDriver)
			TickDriver(dt);

		if (m_bAIGunners)
			TickGunners(dt);
	}

	//==================================================================================================
	// DRIVER
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected AIWaypoint GetDriverWaypoint(IEntity driver, out string reason)
	{
		reason = string.Empty;

		AIControlComponent aiControl = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
		if (!aiControl)
		{
			reason = "driver has no AIControlComponent";
			return null;
		}

		AIAgent agent = aiControl.GetControlAIAgent();
		if (!agent)
		{
			reason = "driver has no AI agent";
			return null;
		}

		AIGroup group = agent.GetParentGroup();
		if (!group)
		{
			reason = "driver is not in an AI group";
			return null;
		}

		AIWaypoint waypoint = group.GetCurrentWaypoint();
		if (waypoint)
			return waypoint;

		// Some group states have waypoints queued without a "current" one yet.
		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		if (!waypoints.IsEmpty() && waypoints[0])
			return waypoints[0];

		reason = "AI group has no waypoint (place a Move waypoint for the group)";
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected AIGroup GetDriverGroup(IEntity driver)
	{
		AIControlComponent aiControl = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
		if (!aiControl)
			return null;

		AIAgent agent = aiControl.GetControlAIAgent();
		if (!agent)
			return null;

		return agent.GetParentGroup();
	}

	//------------------------------------------------------------------------------------------------
	//! True if the entity is sitting in this mech.
	protected bool IsInThisMech(IEntity entity)
	{
		if (!entity)
			return false;

		IEntity owner = GetOwner();
		if (entity.GetRootParent() == owner)
			return true;

		array<string> stationIds = {};
		m_pCore.GetStationIds(stationIds);
		foreach (string stationId : stationIds)
		{
			if (m_pCore.GetStationOccupant(stationId) == entity)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAliveEntity(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		return !damage || damage.GetState() != EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Who the mech should follow when there is no waypoint: the squad leader, if they are on foot.
	//! Covers AI-led groups and AI recruited into a player's squad (the player is the leader).
	protected IEntity GetFollowLeader(AIGroup group)
	{
		if (!group)
			return null;

		IEntity leader;

		// AI recruited by a player live in a "slave" group whose master is the player's group.
		SCR_AIGroup scrGroup = SCR_AIGroup.Cast(group);
		if (scrGroup)
		{
			SCR_AIGroup master = scrGroup.GetMaster();
			if (master)
			{
				int leaderId = master.GetLeaderID();
				if (leaderId > 0)
					leader = GetGame().GetPlayerManager().GetPlayerControlledEntity(leaderId);
			}

			if (!leader && scrGroup.GetLeaderID() > 0)
				leader = GetGame().GetPlayerManager().GetPlayerControlledEntity(scrGroup.GetLeaderID());
		}

		if (!leader)
		{
			AIAgent leaderAgent = group.GetLeaderAgent();
			if (leaderAgent)
				leader = leaderAgent.GetControlledEntity();
		}

		if (!leader || IsInThisMech(leader) || !IsAliveEntity(leader))
			return null;

		return leader;
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest target any AI station is currently engaging (or null).
	protected IEntity GetEngagedTarget(vector position, out float distance)
	{
		IEntity best;
		distance = float.MAX;
		foreach (BFA_MechAIStationState state : m_aStations)
		{
			if (!state.m_pTarget || !IsAliveEntity(state.m_pTarget))
				continue;

			float d = vector.Distance(position, state.m_pTarget.GetOrigin());
			if (d < distance)
			{
				distance = d;
				best = state.m_pTarget;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the AI driver should go, in priority order:
	//!  1. the group's waypoint (Game Master / mission waypoints, squad radial "Move" orders)
	//!  2. follow the squad leader when they are outside the mech
	//!  3. close in on a target the AI stations are engaging but cannot reach well
	//!  otherwise hold position (the stations still fight).
	//! Override in a subclass for custom behaviour (patrol routes, objectives, ...).
	protected bool ResolveDriverDestination(IEntity driver, vector position, out vector destination, out float arrive, out string label)
	{
		string reason;
		AIWaypoint waypoint = GetDriverWaypoint(driver, reason);
		if (waypoint)
		{
			destination = waypoint.GetOrigin();
			arrive = Math.Max(m_fArriveDistance, waypoint.GetCompletionRadius());
			label = "waypoint";
			return true;
		}

		if (m_bFollowLeader)
		{
			IEntity leader = GetFollowLeader(GetDriverGroup(driver));
			if (leader)
			{
				destination = leader.GetOrigin();
				arrive = Math.Max(m_fFollowDistance, m_fArriveDistance);
				label = "following squad leader";
				return true;
			}
		}

		if (m_bAdvanceOnTargets)
		{
			float targetDistance;
			IEntity target = GetEngagedTarget(position, targetDistance);
			if (target)
			{
				destination = target.GetOrigin();
				arrive = Math.Max(m_fAdvanceStopDistance, m_fArriveDistance);
				label = "closing on target";
				return true;
			}
		}

		label = "no waypoint, no leader to follow, no target";
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Debug: print the driver's state only when it changes, so the log stays readable.
	protected void DriverStatus(string status)
	{
		if (!m_bDebug || status == m_sLastDriverStatus)
			return;

		m_sLastDriverStatus = status;
		Print("[BFA AI DRIVER] " + status);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopDriver()
	{
		if (m_bDriverActive && m_pMovement)
			m_pMovement.ServerSetDriverInput(0, 0, 0, false, -1);

		m_bDriverActive = false;
		m_fNoProgressTime = 0;
		m_fReverseTime = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickDriver(float dt)
	{
		if (!m_pMovement)
			return;

		IEntity driver = m_pCore.GetStationOccupant("DRIVER");
		if (!driver)
		{
			DriverStatus("idle - driver seat empty");
			StopDriver();
			return;
		}

		if (!BFA_MechMovementComponent.IsAIControlled(driver))
		{
			DriverStatus("idle - driver is a player");
			StopDriver();
			return;
		}

		IEntity owner = GetOwner();
		vector position = owner.GetOrigin();

		// Fighting takes over from travelling while there is an enemy to fight.
		if (m_bCombatMovement && TickCombatMovement(driver, position, dt))
			return;

		vector destination;
		float arrive;
		string label;
		if (!ResolveDriverDestination(driver, position, destination, arrive, label))
		{
			DriverStatus("holding - " + label);
			StopDriver();
			return;
		}

		vector toTarget = destination - position;
		toTarget[1] = 0;
		float distance = toTarget.Length();

		if (distance <= arrive)
		{
			DriverStatus(string.Format("holding - reached %1 (%2 m)", label, distance));
			StopDriver();
			return;
		}

		DriverStatus(string.Format("driving - %1, %2 m away", label, distance));
		m_bDriverActive = true;

		// Signed heading error in degrees, from the target's position in the mech's own frame
		// (local +X right, +Z forward).
		vector local = owner.VectorToLocal(toTarget);
		float headingError = Math.Atan2(local[0], local[2]) * Math.RAD2DEG;
		float absError = Math.AbsFloat(headingError);

		float turn = Math.Clamp(headingError / 45.0, -1.0, 1.0) * m_fTurnSign;
		float forward = 1.0;
		if (absError > Math.Max(m_fTurnInPlaceAngle, 1.0))
			forward = 0.0;
		else if (absError > Math.Max(m_fSlowTurnAngle, 1.0))
			forward = 0.4;

		bool sprint = m_fSprintDistance > 0 && distance > m_fSprintDistance && absError < 15.0;

		CalibrateTurnSign(absError, Math.AbsFloat(turn), dt);
		UpdateStuck(position, forward, dt);

		if (m_fReverseTime > 0)
		{
			m_fReverseTime -= dt;
			forward = -0.6;
			turn = 0.7 * m_fTurnSign;
			sprint = false;
		}

		m_pMovement.ServerSetDriverInput(forward, 0, turn, sprint, -1);

		m_fDriverDiagTimer += dt;
		if (m_bDebug && m_fDriverDiagTimer >= 1.0)
		{
			m_fDriverDiagTimer = 0;

			// Speed shows whether the input is reaching the body: input sent but speed ~0 means
			// the movement side is not applying it.
			float speed = 0;
			Physics rb = owner.GetPhysics();
			if (rb)
				speed = rb.GetVelocity().Length();

			BFA_MechLog.Log(string.Format("[BFA AI DRIVER] dist=%1 heading=%2 fwd=%3 turn=%4 sign=%5 speed=%6", distance, headingError, forward, turn, m_fTurnSign, speed));
		}
	}

	//==================================================================================================
	// COMBAT MOVEMENT
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected float GetWorldSeconds()
	{
		return GetWorldTimeMs() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	//! Updates who the mech is fighting. Uses the stations' targets first, otherwise the driver looks
	//! for itself (so a mech with only an AI driver still reacts). Remembers where the enemy was last
	//! seen for m_fCombatMemory seconds so the mech can hunt a target that ducked out of sight.
	protected bool UpdateCombatTarget(vector position, float dt, out vector targetPos, out bool visible)
	{
		float stationDistance;
		IEntity candidate = GetEngagedTarget(position, stationDistance);

		if (!candidate)
		{
			m_fCombatScanTimer -= dt;
			if (m_fCombatScanTimer <= 0)
			{
				m_fCombatScanTimer = Math.Max(m_fScanInterval, 0.1);
				m_pDriverScanTarget = FindBestTarget();
			}

			if (IsValidTarget(m_pDriverScanTarget))
				candidate = m_pDriverScanTarget;
		}

		float now = GetWorldSeconds();
		if (candidate && HasLineOfSight(GetAimPoint(candidate), candidate))
		{
			m_pCombatTarget = candidate;
			m_vCombatLastKnown = candidate.GetOrigin();
			m_fCombatLastSeen = now;
			targetPos = m_vCombatLastKnown;
			visible = true;
			return true;
		}

		if (m_pCombatTarget && IsAlive(m_pCombatTarget) && now - m_fCombatLastSeen <= m_fCombatMemory)
		{
			targetPos = m_vCombatLastKnown;
			visible = false;
			return true;
		}

		m_pCombatTarget = null;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Leash: while the group has a waypoint, the mech fights only within m_fCombatLeash metres of it.
	//! Past that it walks back until it is within half the leash, then may fight again.
	protected bool IsCombatLeashed(IEntity driver, vector position)
	{
		if (m_fCombatLeash <= 0)
			return false;

		string reason;
		AIWaypoint waypoint = GetDriverWaypoint(driver, reason);
		if (!waypoint)
		{
			m_bReturningToLeash = false;
			return false;
		}

		vector toWaypoint = waypoint.GetOrigin() - position;
		toWaypoint[1] = 0;
		float distance = toWaypoint.Length();
		if (m_bReturningToLeash)
		{
			if (distance <= m_fCombatLeash * 0.5)
				m_bReturningToLeash = false;
		}
		else if (distance > m_fCombatLeash)
		{
			m_bReturningToLeash = true;
		}

		return m_bReturningToLeash;
	}

	//------------------------------------------------------------------------------------------------
	//! Combat manoeuvring. Returns false when there is nothing to fight (normal travel continues).
	//!  far / out of sight  advance (to the last known position if it is hidden)
	//!  in the band         face it and strafe side to side, switching direction now and then
	//!  too close           back off while still facing it
	//! Override in a subclass for other tactics (hold, flank, keep max range, ...).
	protected bool TickCombatMovement(IEntity driver, vector position, float dt)
	{
		vector targetPos;
		bool visible;
		if (!UpdateCombatTarget(position, dt, targetPos, visible))
			return false;

		if (IsCombatLeashed(driver, position))
		{
			DriverStatus("combat - too far from waypoint, returning");
			return false;
		}

		IEntity owner = GetOwner();
		vector toTarget = targetPos - position;
		toTarget[1] = 0;
		float distance = toTarget.Length();

		vector local = owner.VectorToLocal(toTarget);
		float headingError = Math.Atan2(local[0], local[2]) * Math.RAD2DEG;
		float absError = Math.AbsFloat(headingError);
		float turn = Math.Clamp(headingError / 45.0, -1.0, 1.0) * m_fTurnSign;

		float forward = 0;
		float strafe = 0;
		bool sprint = false;
		string status;

		if (!visible || distance > m_fCombatMaxRange)
		{
			if (!visible && distance <= Math.Max(m_fArriveDistance, 5.0))
			{
				// Reached where it was last seen and it is not here: give up the hunt.
				m_pCombatTarget = null;
				StopDriver();
				DriverStatus("combat - lost the target");
				return true;
			}

			forward = 1.0;
			if (absError > Math.Max(m_fTurnInPlaceAngle, 1.0))
				forward = 0.0;
			else if (absError > Math.Max(m_fSlowTurnAngle, 1.0))
				forward = 0.4;

			sprint = m_fSprintDistance > 0 && distance > m_fCombatMaxRange + m_fSprintDistance && absError < 15.0;

			if (visible)
				status = "combat - advancing";
			else
				status = "combat - hunting last known position";
		}
		else if (distance < m_fCombatMinRange)
		{
			forward = -0.6;
			status = "combat - too close, backing off";
		}
		else
		{
			m_fStrafeTimer -= dt;
			if (m_fStrafeTimer <= 0)
			{
				m_fStrafeTimer = Math.RandomFloatInclusive(Math.Max(m_fStrafeSwitchMin, 0.5), Math.Max(m_fStrafeSwitchMax, m_fStrafeSwitchMin + 0.1));
				if (Math.RandomFloat01() < 0.75)
					m_fStrafeDir = -m_fStrafeDir;
			}

			// Square up first; only strafe once roughly facing the enemy.
			if (absError < 45.0)
				strafe = m_fStrafeDir * Math.Clamp(m_fCombatStrafe, 0.0, 1.0);

			status = "combat - engaging (strafing)";
		}

		CalibrateTurnSign(absError, Math.AbsFloat(turn), dt);
		UpdateStuck(position, forward, dt);

		if (m_fReverseTime > 0)
		{
			m_fReverseTime -= dt;
			forward = -0.6;
			strafe = 0;
			turn = 0.7 * m_fTurnSign;
			sprint = false;
		}

		DriverStatus(status);
		m_bDriverActive = true;
		m_pMovement.ServerSetDriverInput(forward, strafe, turn, sprint, -1);

		m_fDriverDiagTimer += dt;
		if (m_bDebug && m_fDriverDiagTimer >= 1.0)
		{
			m_fDriverDiagTimer = 0;
			Print(string.Format("[BFA AI DRIVER] combat dist=%1 visible=%2 heading=%3 fwd=%4 strafe=%5 turn=%6", distance, visible, headingError, forward, strafe, turn));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The engine's turn direction is not assumed: if turning hard makes the heading error grow
	//! for a second, the sign is flipped. Settles within the first second of driving.
	protected void CalibrateTurnSign(float absError, float turnAmount, float dt)
	{
		// Once a hard turn has visibly reduced the heading error, the direction is known for good.
		if (m_bTurnSignConfirmed)
			return;

		if (turnAmount < 0.5 || absError < 20.0)
		{
			m_fCalibTimer = 0;
			return;
		}

		// Baseline is taken when the hard turn STARTS (not from an earlier leg / waypoint).
		if (m_fCalibTimer <= 0)
			m_fCalibYawError = absError;

		m_fCalibTimer += dt;
		if (m_fCalibTimer < 1.0)
			return;

		if (absError > m_fCalibYawError + 5.0)
		{
			m_fTurnSign = -m_fTurnSign;
			if (m_bDebug)
				Print(string.Format("[BFA AI DRIVER] turning the wrong way, flipped turn sign=%1", m_fTurnSign));
		}
		else if (absError < m_fCalibYawError - 5.0)
		{
			m_bTurnSignConfirmed = true;
			if (m_bDebug)
				Print(string.Format("[BFA AI DRIVER] turn direction confirmed, sign=%1", m_fTurnSign));
		}

		m_fCalibTimer = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateStuck(vector position, float forward, float dt)
	{
		vector moved = position - m_vLastPos;
		moved[1] = 0;
		float speed = moved.Length() / Math.Max(dt, 0.01);
		m_vLastPos = position;

		if (forward > 0.3 && speed < 0.3 && m_fReverseTime <= 0)
		{
			m_fNoProgressTime += dt;
			if (m_fNoProgressTime >= Math.Max(m_fStuckTime, 0.5))
			{
				m_fReverseTime = 1.5;
				m_fNoProgressTime = 0;
				if (m_bDebug)
					Print("[BFA AI DRIVER] stuck - backing out");
			}
		}
		else
		{
			m_fNoProgressTime = 0;
		}
	}

	//==================================================================================================
	// WEAPON STATIONS
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected BFA_MechAIStationState GetStationState(string stationId)
	{
		foreach (BFA_MechAIStationState state : m_aStations)
		{
			if (state.m_sStationId == stationId)
				return state;
		}

		BFA_MechAIStationState created = new BFA_MechAIStationState();
		created.m_sStationId = stationId;
		// Staggered: many AI mechs do not all search for targets on the same frame.
		created.m_fRescanTimer = Math.RandomFloat(0, Math.Max(m_fScanInterval, 0.1));
		m_aStations.Insert(created);
		return created;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickGunners(float dt)
	{
		if (!m_pAim || !m_pRouter)
			return;

		array<string> stationIds = {};
		m_pCore.GetStationIds(stationIds);

		foreach (string stationId : stationIds)
		{
			if (!m_pCore.CanStationAim(stationId))
				continue;

			BFA_MechAIStationState state = GetStationState(stationId);
			IEntity occupant = m_pCore.GetStationOccupant(stationId);

			if (!occupant || !BFA_MechMovementComponent.IsAIControlled(occupant))
			{
				CeaseFire(state);
				state.m_pTarget = null;
				continue;
			}

			TickStation(state, dt);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickStation(BFA_MechAIStationState state, float dt)
	{
		// Keep or find a target.
		state.m_fRescanTimer -= dt;
		if (!IsValidTarget(state.m_pTarget) || state.m_fRescanTimer <= 0)
		{
			state.m_fRescanTimer = Math.Max(m_fScanInterval, 0.1);
			IEntity best = FindBestTarget(state);
			if (best != state.m_pTarget)
			{
				CeaseFire(state);
				state.m_pTarget = best;
				state.m_fTargetTime = 0;
				state.m_fBurstTimer = 0;

				if (m_bDebug && best)
					Print(string.Format("[BFA AI GUNNER] %1 target=%2", state.m_sStationId, best));
			}
		}

		if (!state.m_pTarget)
		{
			CeaseFire(state);
			return;
		}

		// Aim: the same server call a player's aim goes through, so all mounts converge on it.
		vector aimWorld = GetAimPoint(state.m_pTarget);
		m_pAim.ServerSetStationAim(state.m_sStationId, GetOwner().CoordToLocal(aimWorld));

		state.m_fTargetTime += dt;
		if (state.m_fTargetTime < Math.Max(m_fAimSettleTime, 0.0))
			return;

		if (!HasLineOfSight(aimWorld, state.m_pTarget))
		{
			CeaseFire(state);
			return;
		}

		// Weapon choice: heavy (non-primary) groups against vehicles, primary against infantry.
		int group = ChooseWeaponGroup(state.m_sStationId, state.m_pTarget);
		if (group < 1)
		{
			CeaseFire(state);
			return;
		}

		if (group != state.m_iFiringGroup)
		{
			CeaseFire(state);
			m_pCore.ServerSelectWeaponGroup(state.m_sStationId, group);
		}

		// Bursts: hold for m_fBurstTime, release for m_fBurstPause. Releasing and re-pressing also
		// re-triggers single-shot weapons such as rocket pods.
		state.m_fBurstTimer -= dt;
		if (state.m_iFiringGroup == 0)
		{
			if (state.m_fBurstTimer <= 0)
			{
				m_pRouter.ServerSetFireHeld(state.m_sStationId, group, true);
				state.m_iFiringGroup = group;

				if (m_bFireAllGroups)
				{
					array<int> groups = {};
					m_pCore.GetUsableWeaponGroups(state.m_sStationId, groups);
					foreach (int other : groups)
					{
						if (other == group || !GroupHasOffensiveWeapons(state.m_sStationId, other))
							continue;
						m_pRouter.ServerSetFireHeld(state.m_sStationId, other, true);
						state.m_aAllFiring.Insert(other);
					}
				}
				state.m_fBurstTimer = Math.Max(m_fBurstTime, 0.1);
			}
		}
		else if (state.m_fBurstTimer <= 0)
		{
			CeaseFire(state);
			state.m_fBurstTimer = Math.Max(m_fBurstPause, 0.1);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CeaseFire(BFA_MechAIStationState state)
	{
		if (state.m_iFiringGroup > 0 && m_pRouter)
			m_pRouter.ServerSetFireHeld(state.m_sStationId, state.m_iFiringGroup, false);

		if (m_pRouter)
		{
			foreach (int other : state.m_aAllFiring)
				m_pRouter.ServerSetFireHeld(state.m_sStationId, other, false);
		}
		state.m_aAllFiring.Clear();

		state.m_iFiringGroup = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Groups that only contain defensive modules (anti-missile) are never used to attack.
	protected bool GroupHasOffensiveWeapons(string stationId, int group)
	{
		if (!m_pHardpoints)
			return true;

		bool sawModule = false;
		for (int hp = 0; hp < m_pHardpoints.GetHardpointCount(); hp++)
		{
			BFA_MechWeaponModuleComponent module = m_pHardpoints.GetWeaponModuleAt(hp);
			if (!module || !module.MatchesFireRoute(stationId, group))
				continue;

			sawModule = true;
			if (!BFA_MechPointDefenseModuleComponent.Cast(module))
				return true;
		}

		// No modules at all: legacy weapon definitions, which are always offensive.
		return !sawModule;
	}

	//------------------------------------------------------------------------------------------------
	protected int ChooseWeaponGroup(string stationId, IEntity target)
	{
		array<int> groups = {};
		m_pCore.GetUsableWeaponGroups(stationId, groups);

		array<int> offensive = {};
		foreach (int group : groups)
		{
			if (GroupHasOffensiveWeapons(stationId, group))
				offensive.Insert(group);
		}

		if (offensive.IsEmpty())
			return 0;

		// offensive[0] is the station's primary group.
		if (m_bHeavyWeaponsVsVehicles && IsVehicleTarget(target) && offensive.Count() > 1)
			return offensive[1];

		return offensive[0];
	}

	//==================================================================================================
	// TARGETING
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected Faction GetMechFaction()
	{
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(GetOwner().FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return null;

		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			faction = affiliation.GetDefaultAffiliatedFaction();

		return faction;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsVehicleTarget(IEntity target)
	{
		return Vehicle.Cast(target) != null;
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
		if (!faction)
			return false;

		return mechFaction.IsFactionEnemy(faction);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAlive(IEntity entity)
	{
		if (!entity)
			return false;

		DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (!damage)
			return true;

		return damage.GetState() != EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsValidTarget(IEntity target)
	{
		if (!target || !IsAlive(target))
			return false;

		float range = Math.Max(m_fEngageRange, 10.0);
		return vector.DistanceSq(target.GetOrigin(), GetOwner().GetOrigin()) <= range * range;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectQueryEntity(IEntity entity)
	{
		if (entity)
			m_aQuery.Insert(entity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest living, hostile, visible character or vehicle. Characters sitting inside a vehicle
	//! are skipped - the vehicle itself is the target.
	protected IEntity FindBestTarget(BFA_MechAIStationState forState = null)
	{
		BaseWorld world = GetGame().GetWorld();
		Faction mechFaction = GetMechFaction();
		if (!world || !mechFaction)
			return null;

		IEntity owner = GetOwner();
		vector origin = owner.GetOrigin();
		float range = Math.Max(m_fEngageRange, 10.0);

		m_aQuery.Clear();
		world.QueryEntitiesBySphere(origin, range, CollectQueryEntity, null, EQueryEntitiesFlags.DYNAMIC);

		IEntity best;
		float bestDistanceSq = range * range;

		foreach (IEntity entity : m_aQuery)
		{
			if (!entity || entity == owner || entity.GetRootParent() == owner)
				continue;

			bool isCharacter = ChimeraCharacter.Cast(entity) != null;
			bool isVehicle = Vehicle.Cast(entity) != null;
			if (!isCharacter && !isVehicle)
				continue;

			if (isCharacter && entity.GetParent())
				continue; // crew inside a vehicle

			if (!IsAlive(entity) || !IsHostile(entity, mechFaction))
				continue;

			float distanceSq = vector.DistanceSq(origin, entity.GetOrigin());
			if (forState && m_bSpreadTargets && IsTargetTakenByOtherStation(entity, forState))
				distanceSq = distanceSq * 4.0; // counts as twice as far: free targets first
			if (distanceSq >= bestDistanceSq)
				continue;

			if (!HasLineOfSight(GetAimPoint(entity), entity))
				continue;

			best = entity;
			bestDistanceSq = distanceSq;
		}

		m_aQuery.Clear();
		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsTargetTakenByOtherStation(IEntity target, BFA_MechAIStationState self)
	{
		foreach (BFA_MechAIStationState other : m_aStations)
		{
			if (other && other != self && other.m_pTarget == target)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetAimPoint(IEntity target)
	{
		if (ChimeraCharacter.Cast(target))
			return target.GetOrigin() + vector.Up * 1.0; // centre mass

		vector mins, maxs;
		target.GetWorldBounds(mins, maxs);
		return (mins + maxs) * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLineOfSight(vector targetPoint, IEntity target)
	{
		BaseWorld world = GetGame().GetWorld();
		IEntity owner = GetOwner();
		if (!world || !owner)
			return false;

		TraceParam param = new TraceParam();
		param.Start = owner.GetOrigin() + vector.Up * 2.0;
		param.End = targetPoint;
		param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		param.LayerMask = EPhysicsLayerPresets.Projectile;
		array<IEntity> excludes = { owner };
		param.ExcludeArray = excludes;

		float fraction = world.TraceMove(param, null);
		if (fraction >= 1.0)
			return true;

		IEntity hit = param.TraceEnt;
		if (!hit)
			return false;

		if (hit.GetRootParent() == owner)
			return true; // our own parts; good enough for a simple check

		return hit == target || hit.GetRootParent() == target;
	}
}
