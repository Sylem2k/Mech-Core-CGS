//------------------------------------------------------------------------------------------------
//! CGS MECH BRAIN hooks: MechCore's AI crew keeps doing everything for real occupants (players
//! and AI soldiers). When a seat is EMPTY and the mech has an active CGS_MechBrainComponent, the
//! brain takes that seat over: the driver seat drives, weapon seats aim and fire.
modded class BFA_MechAICrewComponent
{
	protected CGS_MechBrainComponent m_pCgsBrain;
	protected bool m_bCgsPlayerDriving;
	protected float m_fCgsCompanionDiagTimer;

	//------------------------------------------------------------------------------------------------
	protected CGS_MechBrainComponent CgsBrain()
	{
		// Re-query while missing. Prefab component initialization can precede
		// or follow the AI crew's first tick; caching a null here permanently
		// disables both autonomous movement and weapons for an empty mech.
		if (!m_pCgsBrain)
			m_pCgsBrain = CGS_MechBrainComponent.Find(GetOwner());
		return m_pCgsBrain;
	}

	//==================================================================================================
	// DRIVER
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	override protected void TickDriver(float dt)
	{
		CGS_MechBrainComponent brain = CgsBrain();
		if (!brain || !brain.DrivesEmptySeat() || !m_pMovement)
		{
			super.TickDriver(dt);
			return;
		}

		// Someone drives. A player / real AI: theirs. The hidden pilot: Game Master waypoints
		// win; with no waypoint the brain's mode (HOLD / PATROL / HUNT) takes over.
		IEntity driver = m_pCore.GetStationOccupant("DRIVER");
		if (driver)
		{
			if (driver != brain.GetPilot())
			{
				// A real player has exclusive movement control. Calling the base AI TickDriver
				// every frame would send STOP commands over the player's own inputs.
				if (!BFA_MechMovementComponent.IsAIControlled(driver))
				{
					if (!m_bCgsPlayerDriving)
						StopDriver(); // release prior AI movement only once at handoff
					m_bCgsPlayerDriving = true;
					return;
				}
				m_bCgsPlayerDriving = false;
				super.TickDriver(dt);
				return;
			}

			string reason;
			if (GetDriverWaypoint(driver, reason))
			{
				super.TickDriver(dt);
				return;
			}
		}

		m_bCgsPlayerDriving = false;
		IEntity owner = GetOwner();
		vector position = owner.GetOrigin();

		// Autonomous combat movement stays enabled for ordinary AI mechs.
		// Companion orders prioritize staying near the player; AI gunners still fight.
		if (!brain.CompanionOwnsMovement() && m_bCombatMovement && TickCombatMovement(driver, position, dt))
			return;

		vector destination;
		float arrive;
		string label;
		if (!brain.GetDestination(position, dt, GetMechFaction(), destination, arrive, label))
		{
			DriverStatus("brain - " + label);
			StopDriver();
			return;
		}

		if (brain.IsCompanionFollowing() && m_bDebug)
		{
			// This path prints at most once every 2 seconds per AI crew component.
			m_fCgsCompanionDiagTimer -= dt;
			if (m_fCgsCompanionDiagTimer <= 0)
			{
				m_fCgsCompanionDiagTimer = 2.0;
				float distance = vector.Distance(position, destination);
				Physics body = owner.GetPhysics();
				bool dynamicBody = body && body.IsDynamic();
				Print(string.Format("[CGS FOLLOW DRIVER] distance=%1 arrive=%2 stationOccupant=%3 physicsDynamic=%4 inputBefore=%5 destination=%6", distance, arrive, driver, dynamicBody, m_pMovement.GetForwardInput(), destination));
			}
		}
		CgsDriveTo(position, destination, arrive, label, dt);
	}

	//------------------------------------------------------------------------------------------------
	//! Same steering as MechCore's AI driver, without needing a driver character.
	protected void CgsDriveTo(vector position, vector destination, float arrive, string label, float dt)
	{
		IEntity owner = GetOwner();
		vector toTarget = destination - position;
		toTarget[1] = 0;
		float distance = toTarget.Length();

		if (distance <= arrive)
		{
			DriverStatus(string.Format("brain - holding, reached %1", label));
			StopDriver();
			return;
		}

		DriverStatus(string.Format("brain - %1, %2 m away", label, distance));
		m_bDriverActive = true;

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
	}

	//------------------------------------------------------------------------------------------------
	//! No driver character = no AI group waypoint. The brain's own leash (distance from home) is used.
	override protected bool IsCombatLeashed(IEntity driver, vector position)
	{
		if (driver)
			return super.IsCombatLeashed(driver, position);

		CGS_MechBrainComponent brain = CgsBrain();
		if (brain && brain.CompanionOwnsMovement())
			return true; // companion movement must not chase enemies off the follow/guard order
		return brain && brain.IsBeyondLeash(position);
	}

	//==================================================================================================
	// WEAPON STATIONS
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	// FIRE MODE: Game Master Hold fire / Fire at will for the whole mech (any MechCore mech),
	// brain Fire Mode for crewless mechs, and Return Fire.
	protected float m_fCgsLastHealth = -1;
	protected float m_fCgsLastShield = -1;
	protected float m_fCgsSinceHit = 100000;
	protected int m_iCgsLastFireMode = CGS_EMechFireMode.FIRE_AT_WILL;

	//------------------------------------------------------------------------------------------------
	//! Notices the hull or shield losing points (= the mech was hit).
	protected void CgsTrackHits(float dt)
	{
		m_fCgsSinceHit += dt;
		IEntity owner = GetOwner();

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(owner.FindComponent(SCR_DamageManagerComponent));
		if (damage)
		{
			HitZone zone = damage.GetDefaultHitZone();
			if (zone)
			{
				float health = zone.GetHealth();
				if (m_fCgsLastHealth >= 0 && health < m_fCgsLastHealth - 0.01)
					m_fCgsSinceHit = 0;
				m_fCgsLastHealth = health;
			}
		}

		CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(owner);
		if (shield)
		{
			float shield01 = shield.GetShield01();
			if (m_fCgsLastShield >= 0 && shield01 < m_fCgsLastShield - 0.0001)
				m_fCgsSinceHit = 0;
			m_fCgsLastShield = shield01;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Combat mode of the AI group aboard (driver first, then any AI crew). -1 = no AI aboard.
	protected int CgsGroupFireMode()
	{
		IEntity ai;
		IEntity driver = m_pCore.GetStationOccupant("DRIVER");
		if (driver && BFA_MechMovementComponent.IsAIControlled(driver))
			ai = driver;

		if (!ai)
		{
			array<string> ids = {};
			m_pCore.GetStationIds(ids);
			foreach (string id : ids)
			{
				IEntity occupant = m_pCore.GetStationOccupant(id);
				if (occupant && BFA_MechMovementComponent.IsAIControlled(occupant))
				{
					ai = occupant;
					break;
				}
			}
		}

		if (!ai)
			return -1;

		EAIGroupCombatMode mode;
		bool found;

		SCR_AIGroup group = SCR_AIGroup.Cast(GetDriverGroup(ai));
		if (group && group.GetGroupUtilityComponent())
		{
			mode = group.GetGroupUtilityComponent().GetCombatModeActual();
			found = true;
		}
		else
		{
			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ai.FindComponent(SCR_AICombatComponent));
			if (combat)
			{
				mode = combat.GetCombatMode();
				found = true;
			}
		}

		if (!found)
			return -1;

		string name = typename.EnumToString(EAIGroupCombatMode, mode);
		if (name.Contains("HOLD"))
			return CGS_EMechFireMode.HOLD_FIRE;
		if (name.Contains("RETURN") || name.Contains("DEFEN"))
			return CGS_EMechFireMode.RETURN_FIRE;
		return CGS_EMechFireMode.FIRE_AT_WILL;
	}

	//------------------------------------------------------------------------------------------------
	protected int CgsFireMode(CGS_MechBrainComponent brain)
	{
		int mode = -1;
		if (!brain || brain.FollowsGroupMode())
			mode = CgsGroupFireMode();

		if (mode < 0 && brain)
			mode = brain.GetFireMode();
		if (mode < 0)
			mode = CGS_EMechFireMode.FIRE_AT_WILL;

		if (mode != m_iCgsLastFireMode)
		{
			m_iCgsLastFireMode = mode;
			Print(string.Format("[CGS BRAIN] %1: fire mode %2", GetOwner(), typename.EnumToString(CGS_EMechFireMode, mode)));
		}
		return mode;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CgsMayShoot(int mode, CGS_MechBrainComponent brain)
	{
		if (mode == CGS_EMechFireMode.HOLD_FIRE)
			return false;

		if (mode == CGS_EMechFireMode.RETURN_FIRE)
		{
			float window = 20;
			if (brain)
				window = brain.GetReturnFireTime();
			return m_fCgsSinceHit <= window;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Track and aim at targets, but never pull the trigger.
	protected void CgsTickHoldFire(BFA_MechAIStationState state, float dt)
	{
		float settle = m_fAimSettleTime;
		m_fAimSettleTime = 1000000;
		TickStation(state, dt);
		m_fAimSettleTime = settle;
		CeaseFire(state);
		state.m_fTargetTime = 0;
	}

	//------------------------------------------------------------------------------------------------
	override protected void TickGunners(float dt)
	{
		if (!m_pAim || !m_pRouter)
			return;

		CgsTrackHits(dt);

		CGS_MechBrainComponent brain = CgsBrain();
		bool brainGuns = brain && brain.FiresEmptySeats();
		bool mayShoot = CgsMayShoot(CgsFireMode(brain), brain);

		array<string> stationIds = {};
		m_pCore.GetStationIds(stationIds);

		foreach (string stationId : stationIds)
		{
			if (!m_pCore.CanStationAim(stationId))
				continue;

			BFA_MechAIStationState state = GetStationState(stationId);
			IEntity occupant = m_pCore.GetStationOccupant(stationId);

			// A player in the seat: the seat is theirs. Empty seat without a brain: idle.
			bool player = occupant && !BFA_MechMovementComponent.IsAIControlled(occupant);
			if (player || (!occupant && !brainGuns))
			{
				CeaseFire(state);
				state.m_pTarget = null;
				continue;
			}

			// AI soldier in the seat, or the brain runs the empty seat.
			if (mayShoot)
				TickStation(state, dt);
			else
				CgsTickHoldFire(state, dt);
		}
	}
}
