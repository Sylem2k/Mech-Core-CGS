class BFA_MechMovementComponentClass : ScriptGameComponentClass
{
}

class BFA_MechMovementComponent : ScriptGameComponent
{
	[Attribute("", UIWidgets.Object, "One entry per LEFT/RIGHT leg pair: the two slot names of the legs (see MECH_LEGS_GUIDE). Biped = 1, quad = 2, spider = 3.", category: "Legs")]
	ref array<ref BFA_MechLegPair> m_aLegPairs;

	[Attribute("1", UIWidgets.Auto, "Normal forward/reverse speed in m/s.", category: "Body")]
	float m_fSpeed;

	[Attribute("true", UIWidgets.CheckBox, "LEG DAMAGE: a destroyed leg slows this mech and stops it sprinting. Untick for bosses that should never limp.", category: "Body")]
	bool m_bLegDamageSlows;

	[Attribute("true", UIWidgets.CheckBox, "Allow this mech to sprint while moving forward.", category: "Body")]
	bool m_bCanSprint;

	[Attribute("8.0", UIWidgets.Auto, "Forward sprint speed in m/s. Normal speed is still m_fSpeed.", category: "Body")]
	float m_fSprintSpeed;

	[Attribute("3.0", UIWidgets.Auto, "Side-strafe speed in m/s.", category: "Body")]
	float m_fStrafeSpeed;

	[Attribute("4.0", UIWidgets.Auto, "Side-strafe speed while sprint is held.", category: "Body")]
	float m_fSprintStrafeSpeed;

	[Attribute("1", UIWidgets.Auto, "Minimum speed (m/s) the body may rise or fall to follow the ground. Ground Rise/Fall Speed below normally win; this is only a floor.", category: "Body")]
	float m_fAvgHeightSpeed;

	[Attribute("1", UIWidgets.Auto, "TURN RATE in radians per second at full turn input (1 = about 57 degrees/s). Lower for giants.", category: "Body")]
	float m_fAngularSpeed;

	[Attribute("1", UIWidgets.Auto, "SIZE SCALE. 1 = Sentinel-sized (about 4 m). A 15 m giant = about 4. Scales ground probes, climb/obstacle limits and vertical speeds so big mechs behave right. Body heights below are NOT scaled - set them for your mech.", category: "Body")]
	float m_fSizeScale;

	[Attribute(desc: "GROUND POINT (recommended): place this point where the mech's feet touch the ground when it stands normally (set class > PointInfo, then move it to the sole of a foot). The standing height is measured from it, so you never have to guess a number. When set, Body Height Pilot / Empty below are ignored.", category: "Body")]
	ref PointInfo m_GroundPoint;

	[Attribute("0", UIWidgets.Auto, "With a Ground Point: how many metres the body lowers when nobody is driving (parked crouch). 0 = stays standing.", category: "Body")]
	float m_fParkedCrouch;

	[Attribute("false", UIWidgets.CheckBox, "BIG MECH GROUND: stand on the TERRAIN averaged under the footprint, ignoring trees, rocks and buildings (a giant steps over them instead of being lifted onto them). Switched on automatically when Size Scale is 3 or more.", category: "Body")]
	bool m_bTerrainSupport;

	[Attribute("1", UIWidgets.Auto, "Only used without a Ground Point: height of the mech's origin above the ground while someone drives.", category: "Body")]
	float m_fBodyHeightPilot;

	[Attribute("1", UIWidgets.Auto, "Only used without a Ground Point: height of the mech's origin above the ground while parked (no driver). Normally the same as Body Height Pilot.", category: "Body")]
	float m_fBodyHeightEmpty;

	[Attribute("2.0", UIWidgets.Auto, "Forward/rear ground probe distance used to anticipate hills before the body collider reaches them.", category: "Body")]
	float m_fGroundProbeForward;

	[Attribute("1.5", UIWidgets.Auto, "Left/right ground probe distance used for cross-slope support.", category: "Body")]
	float m_fGroundProbeSide;

	[Attribute("6.0", UIWidgets.Auto, "Maximum upward terrain-follow speed in m/s.", category: "Body")]
	float m_fGroundRiseSpeed;

	[Attribute("4.0", UIWidgets.Auto, "Maximum downward terrain-follow speed in m/s.", category: "Body")]
	float m_fGroundFallSpeed;

	[Attribute("4.0", UIWidgets.Auto, "Height-servo gain. Higher values catch terrain sooner; vertical speed is still clamped.", category: "Body")]
	float m_fGroundFollowGain;

	[Attribute("0.10", UIWidgets.Auto, "How quickly the support target may rise toward newly detected uphill terrain. Front probes provide look-ahead, so this can stay smooth without hitting the slope.", category: "Body")]
	float m_fGroundRiseSmoothTime;

	[Attribute("0.35", UIWidgets.Auto, "How slowly the support target drops after a ridge/crest. Higher values remove the elevator-like drop when the highest probe leaves a hill.", category: "Body")]
	float m_fGroundFallSmoothTime;

	[Attribute("0.12", UIWidgets.Auto, "Response time for the vertical body velocity. Prevents abrupt up/down changes while keeping enough authority to climb.", category: "Body")]
	float m_fGroundVerticalResponse;

	[Attribute("1.50", UIWidgets.Auto, "Maximum abrupt support step, in metres, that the height follower may use. Taller isolated samples are ignored as support instead of lifting the cockpit onto them.", category: "Stability")]
	float m_fMaxObstacleStepUp;

	[Attribute("2.25", UIWidgets.Auto, "A direct obstacle must rise at least this far above the ground under the mech before forward/reverse drive can be blocked. This is intentionally more permissive for forests, jungle, rivers and rough terrain.", category: "Stability")]
	float m_fObstacleBlockRise;

	[Attribute("0.90", UIWidgets.Auto, "How close a DIRECT center-line obstacle must be before it may block travel. Side/corner support probes never stop the mech.", category: "Stability")]
	float m_fObstacleBlockDistance;

	[Attribute("6.0", UIWidgets.Auto, "Pitch/roll gyro strength while the hull is within the normal tilt range.", category: "Stability")]
	float m_fHullGyroGain;

	[Attribute("12.0", UIWidgets.Auto, "Stronger upright gyro used after the hull exceeds the emergency tilt angle.", category: "Stability")]
	float m_fHullEmergencyGyroGain;

	[Attribute("22.0", UIWidgets.Auto, "At this many degrees of tilt, forward/reverse drive is suppressed until the hull recovers.", category: "Stability")]
	float m_fHullEmergencyTiltDeg;

	[Attribute("0.10", UIWidgets.Auto, "Response time for pitch/roll stabilization.", category: "Stability")]
	float m_fHullGyroResponse;

	[Attribute("1.5", UIWidgets.Auto, "Maximum pitch/roll angular speed in rad/s. Prevents a rock impact from turning the cockpit into a propeller.", category: "Stability")]
	float m_fHullMaxPitchRollRate;

	[Attribute("0.066", UIWidgets.Auto, "Server root snapshot interval in seconds. 0.066 is about 15 updates/sec.", category: "Multiplayer")]
	float m_fNetSnapshotInterval;

	[Attribute("0.12", UIWidgets.Auto, "Observer interpolation delay in seconds. Remote mechs are drawn this far in the past so there are always two server snapshots to blend between. Keep at roughly 1.5-2x the snapshot interval.", category: "Multiplayer")]
	float m_fNetInterpDelay;

	[Attribute("0.10", UIWidgets.Auto, "Maximum time in seconds an observer may extrapolate past the newest snapshot when a packet is late, before holding position.", category: "Multiplayer")]
	float m_fNetMaxExtrapolation;

	[Attribute("3.0", UIWidgets.Auto, "If two consecutive snapshots are farther apart than this many metres (teleport/respawn), observers snap instead of blending across the gap.", category: "Multiplayer")]
	float m_fNetSnapDistance;

	[Attribute("1.0", UIWidgets.Auto, "Driver reconciliation: horizontal error in metres between the driver's prediction and the server that is tolerated without correction.", category: "Multiplayer")]
	float m_fReconcileDeadband;

	[Attribute("4.0", UIWidgets.Auto, "Driver reconciliation: yaw error in degrees tolerated without correction. Never corrected while the driver is actively turning.", category: "Multiplayer")]
	float m_fReconcileYawDeadbandDeg;

	[Attribute("3", UIWidgets.Auto, "Driver reconciliation: the error must stay above the deadband for this many server snapshots in a row before any correction. Filters out normal network jitter.", category: "Multiplayer")]
	int m_iReconcileConfirmCount;

	[Attribute("1.5", UIWidgets.Auto, "Driver reconciliation: fraction of the remaining error removed per second (1/s). Higher corrects faster but is more noticeable.", category: "Multiplayer")]
	float m_fReconcileGain;

	[Attribute("0.75", UIWidgets.Auto, "Driver reconciliation: maximum extra correction speed in m/s added on top of normal movement. Keep low so a correction never feels like a speed change.", category: "Multiplayer")]
	float m_fReconcileMaxSpeed;

	[Attribute("4.0", UIWidgets.Auto, "Driver reconciliation: errors larger than this many metres are snapped immediately instead of blended.", category: "Multiplayer")]
	float m_fReconcileSnapDistance;

	[Attribute("false", UIWidgets.CheckBox, "Print a line every time the driver's prediction is blended toward the server.", category: "Multiplayer")]
	bool m_bDebugReconcile;

	[Attribute("33", UIWidgets.EditBox, "Milliseconds between observer visual ticks. This runs independently of EOnFrame on remote clients.", category: "Multiplayer")]
	protected int m_iRemoteVisualTickMs;

	float m_fBodyHeight;
	float m_fVerticalVelocity;
	protected bool m_bSupportFilterInitialized;
	protected float m_fFilteredSupportY;
	protected bool m_bFrontObstacle;
	protected bool m_bRearObstacle;
	protected int m_iFrontObstacleFrames;
	protected int m_iRearObstacleFrames;
	protected float m_fDiagObstacleRise;
	protected float m_fDiagTiltDeg;

	ref array<ref BFA_MechLegPair> m_aAllLegs = {};

	// Server-side validated driver input. Only the server simulates from these, so they are
	// deliberately NOT replicated (no client reads them; replicating them cost an update every
	// time an analog input changed).
	protected float m_fFwd;
	protected float m_fStrafe;
	protected float m_fTurn;
	protected bool m_bSprint;

	// Newest driver input sequence the server has applied. -1 = accept anything (reset on
	// pilot change). Older out-of-order unreliable input packets are dropped.
	protected int m_iServerLastInputSeq = -1;

	// Server-authored root snapshots. Sent ONLY through RpcDo_ServerRootSnapshot (and the
	// owner copy). The previous RplProp copy of the same data was removed: observers never
	// received its callback on the standalone walker prefab, so it only doubled bandwidth.
	protected vector m_vNetPosition;
	protected vector m_vNetRotation;
	protected int m_iNetTransformSequence;
	protected int m_iNetTransformSeen;
	protected bool m_bNetTransformReady;
	protected float m_fNetSnapshotTimer;

	// Observer snapshot buffer (#5). Times are server world time in seconds.
	static const int BFA_NET_BUFFER_SIZE = 16;
	protected ref array<float> m_aSnapTime = {};
	protected ref array<vector> m_aSnapPos = {};
	protected ref array<vector> m_aSnapRot = {};
	protected bool m_bClockOffsetValid;
	protected float m_fClockOffset; // local world time - server world time, seconds (includes latency)
	protected float m_fLastRemoteTickTime = -1;

	// Driver reconciliation (#4). History of predicted root state per sent input sequence.
	static const int BFA_PRED_HISTORY_SIZE = 64;
	protected int m_iClientInputSeq;
	protected ref array<int> m_aPredSeq = {};
	protected ref array<vector> m_aPredPos = {};
	protected ref array<float> m_aPredYaw = {};
	protected int m_iLastReconciledAck = -1;
	protected vector m_vReconcilePending;   // world-space metres still to correct
	protected float m_fReconcileYawPending; // degrees still to correct
	protected float m_fDiagReconcileError;
	protected int m_iDiagReconcileSnaps;
	protected int m_iReconcileStreak;
	protected vector m_vReconcileErrorSum;
	protected int m_iReconcileYawStreak;
	protected float m_fReconcileYawErrorSum;

	// Immediate local input keeps singleplayer/listen-server control responsive and preserves
	// the exact direct-input behavior the mech used before the MP conversion.
	protected float m_fLocalFwd;
	protected float m_fLocalStrafe;
	protected float m_fLocalTurn;
	protected bool m_bLocalSprint;

	IEntity m_Pilot;
	SlotManagerComponent m_SlotManager;

	// In dedicated MP the slot children may arrive after the root entity initializes.
	// Body-height correction MUST NOT run until both leg children are ready, otherwise a
	// failed pair reads as Y=0 and drags the whole mech below the terrain.
	protected bool m_bLegPairsReady;
	protected bool m_bLegRetryQueued;

	// Jump-jet state exists only on the physics owner. Jump events themselves are
	// replicated by BFA_SentinelJumpJetComponent.
	protected bool m_bAirborne;
	protected bool m_bJumpGroundClearanceValid;
	protected float m_fJumpForwardBoost;
	protected float m_fJumpGravity = 9.81;
	protected float m_fJumpLandingCushionDistance = 1.5;
	protected float m_fJumpGroundClearance;

	// V12 diagnostic telemetry. Prints one compact line per second while the local pilot is driving.
	// This tells us whether the stutter is caused by sparse EOnFrame ticks or by another system
	// overwriting the rigid-body velocity between our updates.
	protected int m_iDiagFrames;
	protected int m_iDiagOwnerFrames;
	protected float m_fDiagAccumTime;
	protected float m_fDiagMinDt = 999.0;
	protected float m_fDiagMaxDt;
	protected float m_fDiagVelBefore;
	protected float m_fDiagVelAfter;
	protected float m_fDiagSupportY;
	protected float m_fDiagTargetY;
	protected float m_fDiagHeightError;
	protected bool m_bDiagHaveStartPos;
	protected vector m_vDiagStartPos;

	// MP ownership/replication diagnostic. Separate timer so dedicated server and proxy
	// copies report even though they never have a local pilot.
	protected float m_fMpDiagAccumTime;

	// Remote observers are driven from a bounded callqueue tick instead of depending on
	// EOnFrame. Dedicated-server clients can receive only sparse frame events for vehicles
	// they do not own, which was enough to defeat the old watchdog without actually
	// producing usable visual updates.
	protected bool m_bRemoteVisualTickRunning;

	//------------------------------------------------------------------------------------------------
	void SetPilot(IEntity pilot)
	{
		m_Pilot = pilot;

		if (pilot)
			m_fBodyHeight = m_fBodyHeightPilot;
		else
			m_fBodyHeight = m_fBodyHeightEmpty;

		// New driver (or none): accept their input sequence from scratch.
		m_iServerLastInputSeq = -1;

		// The exit "stop" input is sent unreliably. If that packet is lost the server would keep
		// walking an empty mech with the last held input, so clear it here on the server as well.
		if (!pilot && Replication.IsServer())
		{
			m_fFwd = 0;
			m_fStrafe = 0;
			m_fTurn = 0;
			m_bSprint = false;
		}

		ResetReconciliation();

		string role = "CLIENT";
		if (Replication.IsServer())
			role = "SERVER";
		BFA_MechLog.Log(string.Format("[BFA MP PILOT] role=%1 active=%2 rplOwner=%3", role, pilot != null, IsPhysicsOwner()));

		if (!pilot && IsPhysicsOwner())
			ResetJumpJetState();
		else if (pilot && !m_bAirborne && IsPhysicsOwner())
			m_fVerticalVelocity = 0;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetPilot()
	{
		return m_Pilot;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPilot(IEntity entity)
	{
		return entity && m_Pilot == entity;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPhysicsOwner()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		return !rpl || rpl.IsOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Local driver input. This is intentionally NOT replicated from the vehicle component.
	//! The controller also sends the same wish through SCR_PlayerController for dedicated MP.
	void SetForwardInput(float value)
	{
		m_fLocalFwd = Math.Clamp(value, -1.0, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	void SetRightInput(float value)
	{
		// CharacterRight is now lateral movement. A/D strafes instead of steering.
		m_fLocalStrafe = Math.Clamp(value, -1.0, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	void SetTurnInput(float value)
	{
		// Q/E and controller left-stick X use lean-adjust actions to rotate the chassis; A/D remain keyboard strafe.
		m_fLocalTurn = Math.Clamp(value, -1.0, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	void SetSprintInput(bool active)
	{
		m_bLocalSprint = active;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLocalPilot()
	{
		if (!m_Pilot || !GetGame())
			return false;

		// An AI driver is never "local". Without this, a player sitting in another seat of the
		// same mech (gunner/commander) would be mistaken for the driver and the AI's input ignored.
		if (IsAIControlled(m_Pilot))
			return false;

		PlayerController localPC = GetGame().GetPlayerController();
		if (!localPC)
			return false;

		// Strong MP identity check. The local controlled entity can switch between the
		// character and vehicle root while seated, so direct entity equality is not enough.
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			int pilotPlayerId = playerManager.GetPlayerIdFromControlledEntity(m_Pilot);
			if (pilotPlayerId > 0)
			{
				PlayerController pilotPC = playerManager.GetPlayerController(pilotPlayerId);
				if (pilotPC && pilotPC == localPC)
					return true;
			}
		}

		IEntity controlled = localPC.GetControlledEntity();
		if (!controlled)
			controlled = SCR_PlayerController.GetLocalControlledEntity();

		if (!controlled)
			return false;

		if (controlled == m_Pilot || controlled == GetOwner())
			return true;

		return controlled.GetRootParent() == GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! True if this character is driven by AI rather than a player.
	static bool IsAIControlled(IEntity character)
	{
		if (!character)
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && playerManager.GetPlayerIdFromControlledEntity(character) > 0)
			return false;

		// Not a player and has an AI control component = AI. Activation state is not required:
		// it can read as inactive while the character sits in a vehicle seat.
		return AIControlComponent.Cast(character.FindComponent(AIControlComponent)) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetEffectiveForwardInput()
	{
		if (HasLocalPilot())
			return m_fLocalFwd;

		return m_fFwd;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetEffectiveStrafeInput()
	{
		if (HasLocalPilot())
			return m_fLocalStrafe;

		return m_fStrafe;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetEffectiveTurnInput()
	{
		if (HasLocalPilot())
			return m_fLocalTurn;

		return m_fTurn;
	}

	//------------------------------------------------------------------------------------------------
	protected bool GetEffectiveSprintInput()
	{
		if (HasLocalPilot())
			return m_bLocalSprint;

		return m_bSprint;
	}

	//------------------------------------------------------------------------------------------------
	//! Only the server may author replicated driver input.
	//! inputSeq: client input sequence (-1 for local/server callers that are never reordered).
	void ServerSetDriverInput(float forward, float strafe, float turn, bool sprint, int inputSeq = -1)
	{
		if (!Replication.IsServer())
			return;

		if (inputSeq >= 0)
		{
			// Unreliable packets can arrive out of order. Never let an older input overwrite a newer one.
			if (m_iServerLastInputSeq >= 0 && inputSeq <= m_iServerLastInputSeq)
				return;

			m_iServerLastInputSeq = inputSeq;
		}

		float newFwd = Math.Clamp(forward, -1.0, 1.0);
		float newStrafe = Math.Clamp(strafe, -1.0, 1.0);
		float newTurn = Math.Clamp(turn, -1.0, 1.0);
		bool newSprint = sprint && m_bCanSprint;

		if (m_fFwd == newFwd && m_fStrafe == newStrafe && m_fTurn == newTurn && m_bSprint == newSprint)
			return;

		m_fFwd = newFwd;
		m_fStrafe = newStrafe;
		m_fTurn = newTurn;
		m_bSprint = newSprint;
	}

	//------------------------------------------------------------------------------------------------
	float GetForwardInput()
	{
		return GetEffectiveForwardInput();
	}

	//------------------------------------------------------------------------------------------------
	float GetRightInput()
	{
		return GetEffectiveStrafeInput();
	}

	//------------------------------------------------------------------------------------------------
	float GetTurnInput()
	{
		return GetEffectiveTurnInput();
	}

	//------------------------------------------------------------------------------------------------
	bool LegDamageSlows()
	{
		return m_bLegDamageSlows;
	}

	//------------------------------------------------------------------------------------------------
	bool IsSprinting()
	{
		return m_bCanSprint && GetEffectiveSprintInput();
	}

	//------------------------------------------------------------------------------------------------
	bool IsAirborne()
	{
		return m_bAirborne;
	}

	//------------------------------------------------------------------------------------------------
	//! Called only on the machine that owns/simulates the mech physics.
	bool ApplyJumpJetBoost(float upwardSpeed, float forwardBoost, float gravity, float landingCushionDistance)
	{
		if (!IsPhysicsOwner())
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		Physics rb = owner.GetPhysics();
		if (!rb)
			return false;

		if (!m_bAirborne)
		{
			m_bAirborne = true;
			m_bSupportFilterInitialized = false;
			m_bJumpGroundClearanceValid = false;
			CaptureJumpGroundClearance();
		}

		m_fJumpGravity = Math.Max(gravity, 0.0);
		m_fJumpLandingCushionDistance = Math.Max(landingCushionDistance, 0.0);

		if (m_fVerticalVelocity < 0)
			m_fVerticalVelocity = 0;

		m_fVerticalVelocity = m_fVerticalVelocity + Math.Max(upwardSpeed, 0.0);
		m_fJumpForwardBoost = m_fJumpForwardBoost + Math.Max(forwardBoost, 0.0);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	void ResetJumpJetState()
	{
		m_bAirborne = false;
		m_bJumpGroundClearanceValid = false;
		m_fJumpForwardBoost = 0;
		m_fVerticalVelocity = 0;
		m_bSupportFilterInitialized = false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TraceGroundBelow(vector worldPos, out float groundY)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		BaseWorld world = owner.GetWorld();
		if (!world)
			return false;

		vector from = worldPos + vector.Up * 4.0;
		vector to = worldPos - vector.Up * 50.0;

		// The visual legs are children of the mech root. A ground probe must never
		// use one of those children as "terrain", otherwise every foot swing feeds
		// back into the root height and makes the whole vehicle bob/jolt.
		for (int i = 0; i < 6; i++)
		{
			TraceParam param = new TraceParam();
			param.Start = from;
			param.End = to;
			param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			param.LayerMask = EPhysicsLayerPresets.Projectile;
			array<IEntity> excludeArray = { owner };
			param.ExcludeArray = excludeArray;

			float traceDistance = world.TraceMove(param, null);
			if (traceDistance >= 1.0)
				break;

			vector hitPosition = param.Start + (param.End - param.Start) * traceDistance;

			if (param.TraceEnt && param.TraceEnt.GetRootParent() == owner)
			{
				from = hitPosition - vector.Up * 0.05;
				continue;
			}

			groundY = hitPosition[1];
			return true;
		}

		// Terrain fallback keeps the mech stable even if the physics trace finds
		// no entity/world hit (for example while streaming).
		groundY = SCR_TerrainHelper.GetTerrainY(worldPos);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Samples the ground under the footprint of the root collider instead of only
	//! directly under the mech origin. On an uphill the front of the body collider
	//! reaches the slope before the center does; a center-only probe therefore reacts
	//! too late and lets physics shove the root sideways/backward.
	protected void ConsiderSupportProbe(vector probe, float centerY, int travelSide, inout float highest)
	{
		float y;
		if (!TraceGroundBelow(probe, y))
			return;

		float rise = y - centerY;
		float maxStep = Math.Max(m_fMaxObstacleStepUp, 0.1);

		// Support probes are ONLY for body height. They never stop travel.
		// This is important in forests/jungle/creeks where a rock, root or bank can sit
		// beside a leg without actually blocking the mech's center path.
		if (rise > maxStep)
			return;

		if (y > highest)
			highest = y;
	}

	//------------------------------------------------------------------------------------------------
	//! Narrow obstacle detector used only to stop a truly direct, close obstacle.
	//! Unlike the wide support footprint, this samples only the center travel line.
	protected bool SampleDirectTravelObstacle(vector origin, vector forward, float centerY, int direction, out float rise)
	{
		rise = 0;

		float distance = Math.Max(m_fObstacleBlockDistance, 0.25);
		vector probe = origin + forward * (distance * direction);

		float y;
		if (!TraceGroundBelow(probe, y))
			return false;

		rise = y - centerY;
		return rise > Math.Max(m_fObstacleBlockRise, 0.5);
	}

	//------------------------------------------------------------------------------------------------
	bool UsesTerrainSupport()
	{
		return m_bTerrainSupport || m_fSizeScale >= 3.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Big mechs: average TERRAIN height under the footprint (centre + 8 probes). Objects are
	//! ignored, so a 15 m tree or a boulder beside a leg never lifts a 50 m titan.
	protected float SampleTerrainSupport(IEntity owner, vector origin)
	{
		vector forward = owner.GetTransformAxis(2);
		forward[1] = 0;
		if (forward.LengthSq() > 0.0001)
			forward.Normalize();
		else
			forward = vector.Forward;

		vector right = Vector(forward[2], 0, -forward[0]);
		float fwdDist = Math.Max(m_fGroundProbeForward, 0.0);
		float sideDist = Math.Max(m_fGroundProbeSide, 0.0);
		BaseWorld world = owner.GetWorld();

		float sum = SCR_TerrainHelper.GetTerrainY(origin, world);
		sum += SCR_TerrainHelper.GetTerrainY(origin + forward * fwdDist, world);
		sum += SCR_TerrainHelper.GetTerrainY(origin - forward * fwdDist, world);
		sum += SCR_TerrainHelper.GetTerrainY(origin + right * sideDist, world);
		sum += SCR_TerrainHelper.GetTerrainY(origin - right * sideDist, world);
		sum += SCR_TerrainHelper.GetTerrainY(origin + forward * fwdDist + right * sideDist, world);
		sum += SCR_TerrainHelper.GetTerrainY(origin + forward * fwdDist - right * sideDist, world);
		sum += SCR_TerrainHelper.GetTerrainY(origin - forward * fwdDist + right * sideDist, world);
		sum += SCR_TerrainHelper.GetTerrainY(origin - forward * fwdDist - right * sideDist, world);
		return sum / 9.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Samples the support under the root footprint, but rejects isolated tall steps.
	//! Smooth hills are still anticipated by the front probes; a rock/ledge that rises
	//! too far above the center sample becomes an obstacle instead of a vertical target.
	protected bool SampleSupportGround(out float supportY)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		vector origin = owner.GetOrigin();

		if (UsesTerrainSupport())
		{
			supportY = SampleTerrainSupport(owner, origin);
			m_fDiagObstacleRise = 0;
			m_iFrontObstacleFrames = 0;
			m_iRearObstacleFrames = 0;
			m_bFrontObstacle = false;
			m_bRearObstacle = false;
			return true;
		}

		vector forward = owner.GetTransformAxis(2);
		forward[1] = 0;
		if (forward.LengthSq() > 0.0001)
			forward.Normalize();
		else
			forward = vector.Forward;

		vector right = Vector(forward[2], 0, -forward[0]);

		float fwdDist = Math.Max(m_fGroundProbeForward, 0.0);
		float sideDist = Math.Max(m_fGroundProbeSide, 0.0);

		float centerY;
		if (!TraceGroundBelow(origin, centerY))
			return false;

		m_fDiagObstacleRise = 0;

		float highest = centerY;

		// Wide footprint probes are SUPPORT ONLY. A rock 2-3 m off to one side may be
		// useful for knowing the terrain under a foot, but it must never stop the mech.
		ConsiderSupportProbe(origin + forward * fwdDist, centerY, 0, highest);
		ConsiderSupportProbe(origin + forward * fwdDist + right * sideDist, centerY, 0, highest);
		ConsiderSupportProbe(origin + forward * fwdDist - right * sideDist, centerY, 0, highest);
		ConsiderSupportProbe(origin - forward * fwdDist, centerY, 0, highest);
		ConsiderSupportProbe(origin - forward * fwdDist + right * sideDist, centerY, 0, highest);
		ConsiderSupportProbe(origin - forward * fwdDist - right * sideDist, centerY, 0, highest);
		ConsiderSupportProbe(origin + right * sideDist, centerY, 0, highest);
		ConsiderSupportProbe(origin - right * sideDist, centerY, 0, highest);

		// Actual blocking is intentionally narrow and close: center line only, less than
		// one metre ahead/behind by default, and only for a tall abrupt rise. Require the
		// condition for several consecutive frames so a single foliage/geometry hit does
		// not make the walker pulse-stop.
		float frontRise;
		bool frontCandidate = SampleDirectTravelObstacle(origin, forward, centerY, 1, frontRise);
		if (frontCandidate)
			m_iFrontObstacleFrames++;
		else
			m_iFrontObstacleFrames = 0;

		float rearRise;
		bool rearCandidate = SampleDirectTravelObstacle(origin, forward, centerY, -1, rearRise);
		if (rearCandidate)
			m_iRearObstacleFrames++;
		else
			m_iRearObstacleFrames = 0;

		m_bFrontObstacle = m_iFrontObstacleFrames >= 4;
		m_bRearObstacle = m_iRearObstacleFrames >= 4;

		if (m_bFrontObstacle)
			m_fDiagObstacleRise = frontRise;
		else if (m_bRearObstacle)
			m_fDiagObstacleRise = rearRise;

		supportY = highest;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CaptureJumpGroundClearance()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		vector origin = owner.GetOrigin();

		float groundY;
		if (!TraceGroundBelow(origin, groundY))
			return;

		m_fJumpGroundClearance = origin[1] - groundY;

		if (m_fJumpGroundClearance < 0.5)
			m_fJumpGroundClearance = 0.5;

		m_bJumpGroundClearanceValid = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateAirborne(float timeSlice)
	{
		if (!m_bAirborne)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		m_fVerticalVelocity = m_fVerticalVelocity - (m_fJumpGravity * timeSlice);

		if (m_fVerticalVelocity >= 0)
			return;

		if (!m_bJumpGroundClearanceValid)
			CaptureJumpGroundClearance();

		if (!m_bJumpGroundClearanceValid)
			return;

		vector origin = owner.GetOrigin();

		float groundY;
		if (!TraceGroundBelow(origin, groundY))
			return;

		float landingOriginY = groundY + m_fJumpGroundClearance;

		if (origin[1] > landingOriginY + m_fJumpLandingCushionDistance)
			return;

		origin[1] = landingOriginY;
		owner.SetOrigin(origin);

		ResetJumpJetState();
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueLegRetry()
	{
		if (m_bLegRetryQueued || m_bLegPairsReady)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_bLegRetryQueued = true;
		queue.CallLater(TryInitializeLegPairs, 100, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryInitializeLegPairs()
	{
		m_bLegRetryQueued = false;

		IEntity owner = GetOwner();
		if (!owner || !m_SlotManager || !m_aLegPairs || m_aLegPairs.IsEmpty())
			return;

		m_aAllLegs.Clear();

		bool allReady = true;
		foreach (int i, BFA_MechLegPair legPair : m_aLegPairs)
		{
			if (!legPair)
			{
				allReady = false;
				continue;
			}

			bool isEvenPair = (Math.Mod(i, 2) == 0);
			if (legPair.Init(m_SlotManager, owner, isEvenPair))
				m_aAllLegs.Insert(legPair);
			else
				allReady = false;
		}

		if (allReady && !m_aAllLegs.IsEmpty())
		{
			m_bLegPairsReady = true;
			BFA_MechLog.Log("[BFA MechCore] Movement initialized with " + m_aAllLegs.Count() + " leg pair(s).");
			return;
		}

		// Dedicated-server slot children can arrive a few frames after the vehicle root.
		// Retry instead of treating missing children as height zero.
		QueueLegRetry();
	}

	//------------------------------------------------------------------------------------------------
	protected float GetHullTiltDeg(IEntity owner)
	{
		if (!owner)
			return 0;

		vector up = owner.GetTransformAxis(1);
		if (up.LengthSq() <= 0.0001)
			return 0;

		up.Normalize();
		float d = Math.Clamp(vector.Dot(up, vector.Up), -1.0, 1.0);
		return Math.Acos(d) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! Keep the heavy cockpit/root upright. The visual legs never carry root physics, so
	//! pitch/roll from a rock collision is unwanted body torque, not a gait feature.
	protected void ApplyHullStabilizer(IEntity owner, inout vector angularVelocity, float timeSlice)
	{
		if (!owner)
			return;

		vector up = owner.GetTransformAxis(1);
		if (up.LengthSq() <= 0.0001)
			return;

		up.Normalize();
		vector targetUp = vector.Up;

		m_fDiagTiltDeg = GetHullTiltDeg(owner);

		// Same world-space error form used by the MAWS-style hull gyro: cross(up, targetUp),
		// with yaw deliberately removed so steering remains independent.
		vector err = Vector(
			up[1] * targetUp[2] - up[2] * targetUp[1],
			0,
			up[0] * targetUp[1] - up[1] * targetUp[0]
		);

		float gain = Math.Max(m_fHullGyroGain, 0.0);
		if (m_fDiagTiltDeg >= Math.Max(m_fHullEmergencyTiltDeg, 1.0))
			gain = Math.Max(m_fHullEmergencyGyroGain, gain);

		vector targetRate = err * gain;
		float response = Math.Max(m_fHullGyroResponse, 0.001);
		float alpha = Math.Clamp(timeSlice / response, 0.0, 1.0);

		angularVelocity[0] = angularVelocity[0] + (targetRate[0] - angularVelocity[0]) * alpha;
		angularVelocity[2] = angularVelocity[2] + (targetRate[2] - angularVelocity[2]) * alpha;

		float maxRate = Math.Max(m_fHullMaxPitchRollRate, 0.1);
		angularVelocity[0] = Math.Clamp(angularVelocity[0], -maxRate, maxRate);
		angularVelocity[2] = Math.Clamp(angularVelocity[2], -maxRate, maxRate);
	}

	//------------------------------------------------------------------------------------------------
	void Move(float timeSlice)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		Physics rb = owner.GetPhysics();
		if (!rb || !rb.IsDynamic())
			return;

		float forwardInput = GetEffectiveForwardInput();
		float strafeInput = GetEffectiveStrafeInput();
		float turnInput = GetEffectiveTurnInput();
		BFA_MechExtension extension = BFA_MechExtension.Get();
		bool sprinting = m_bCanSprint && GetEffectiveSprintInput() && extension.CanMechSprint(owner);
		float speedScale = Math.Clamp(extension.GetMechSpeedScale(owner), 0.0, 2.0);

		vector localFwd = owner.GetTransformAxis(2);
		localFwd[1] = 0;
		if (localFwd.LengthSq() <= 0.0001)
			return;
		localFwd.Normalize();

		vector localRight = Vector(localFwd[2], 0, -localFwd[0]);

		// Normalize W/S + A/D so a diagonal input never grants free extra speed.
		float inputMagnitude = Math.Sqrt(forwardInput * forwardInput + strafeInput * strafeInput);
		if (inputMagnitude > 1.0)
		{
			forwardInput = forwardInput / inputMagnitude;
			strafeInput = strafeInput / inputMagnitude;
		}

		m_fDiagTiltDeg = GetHullTiltDeg(owner);

		// Do not let the height follower turn a tall rock into "terrain". If the
		// look-ahead probes see an abrupt step, stop only the direction driving into it.
		if (forwardInput > 0 && m_bFrontObstacle)
			forwardInput = 0;
		else if (forwardInput < 0 && m_bRearObstacle)
			forwardInput = 0;

		// If a collision has already tipped the hull, remove translation until the
		// stabilizer has brought the cockpit back under control. Turning remains available.
		if (m_fDiagTiltDeg >= Math.Max(m_fHullEmergencyTiltDeg, 1.0))
		{
			forwardInput = 0;
			strafeInput = 0;
		}

		float baseForwardSpeed = m_fSpeed;
		if (sprinting && forwardInput > 0)
			baseForwardSpeed = Math.Max(m_fSprintSpeed, m_fSpeed);

		float baseStrafeSpeed = Math.Max(m_fStrafeSpeed, 0.0);
		if (sprinting)
			baseStrafeSpeed = Math.Max(m_fSprintStrafeSpeed, baseStrafeSpeed);

		float forwardSpeed = baseForwardSpeed * forwardInput * speedScale;
		float strafeSpeed = baseStrafeSpeed * strafeInput * speedScale;

		if (m_bAirborne)
			forwardSpeed = forwardSpeed + m_fJumpForwardBoost;

		vector currentVelocity = rb.GetVelocity();
		m_fDiagVelBefore = vector.Dot(currentVelocity, localFwd);

		// Exact horizontal drive. Forward and lateral movement are authored independently;
		// vertical motion stays owned by the terrain/jump controller.
		vector velocity = localFwd * forwardSpeed + localRight * strafeSpeed;

		// Driver client only: blend the prediction back toward the server root (#4).
		if (!Replication.IsServer() && HasLocalPilot())
			velocity = velocity + ConsumeReconcileVelocity(timeSlice);

		velocity[1] = m_fVerticalVelocity;
		rb.SetVelocity(velocity);

		vector velocityAfter = rb.GetVelocity();
		m_fDiagVelAfter = vector.Dot(velocityAfter, localFwd);

		vector angularVelocity = rb.GetAngularVelocity();
		ApplyHullStabilizer(owner, angularVelocity, timeSlice);
		angularVelocity[1] = m_fAngularSpeed * turnInput;
		rb.SetAngularVelocity(angularVelocity);
	}

	//------------------------------------------------------------------------------------------------
	float GetAvgBodyHeight()
	{
		// Kept for diagnostics/UI only. The procedural feet no longer drive root
		// physics. They are visual outputs derived from root motion.
		IEntity owner = GetOwner();
		if (!owner)
			return 0;

		float groundY;
		if (!SampleSupportGround(groundY))
			return owner.GetOrigin()[1];

		return groundY + m_fBodyHeight;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateBodyHeight(float timeSlice)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Sample the whole body footprint. The front probes still detect an uphill
		// before the collider reaches it, but the raw highest sample is intentionally
		// filtered before it becomes the body target. This prevents the chassis from
		// behaving like an elevator when a corner probe suddenly finds/leaves a ridge.
		float rawSupportY;
		if (!SampleSupportGround(rawSupportY))
		{
			m_fVerticalVelocity = 0;
			m_bSupportFilterInitialized = false;
			return;
		}

		if (!m_bSupportFilterInitialized)
		{
			m_fFilteredSupportY = rawSupportY;
			m_bSupportFilterInitialized = true;
		}
		else
		{
			float smoothTime;
			if (rawSupportY > m_fFilteredSupportY)
				smoothTime = Math.Max(m_fGroundRiseSmoothTime, 0.001);
			else
				smoothTime = Math.Max(m_fGroundFallSmoothTime, 0.001);

			float supportAlpha = Math.Clamp(timeSlice / smoothTime, 0.0, 1.0);
			m_fFilteredSupportY = m_fFilteredSupportY + (rawSupportY - m_fFilteredSupportY) * supportAlpha;
		}

		float targetY = m_fFilteredSupportY + m_fBodyHeight;
		float currentY = owner.GetOrigin()[1];
		float error = targetY - currentY;

		m_fDiagSupportY = m_fFilteredSupportY;
		m_fDiagTargetY = targetY;
		m_fDiagHeightError = error;

		float desiredVertical = 0;
		if (Math.AbsFloat(error) >= 0.03)
		{
			float gain = Math.Max(m_fGroundFollowGain, 0.1);
			float wanted = error * gain;

			float riseLimit = Math.Max(m_fGroundRiseSpeed, Math.Max(m_fAvgHeightSpeed, 0.01));
			float fallLimit = Math.Max(m_fGroundFallSpeed, Math.Max(m_fAvgHeightSpeed, 0.01));
			desiredVertical = Math.Clamp(wanted, -fallLimit, riseLimit);
		}

		// Smooth the velocity command itself as a second stage. The footprint can
		// still change by metres over one second on steep terrain; easing the command
		// removes abrupt body pulses without reducing the final climb authority.
		float response = Math.Max(m_fGroundVerticalResponse, 0.001);
		float velocityAlpha = Math.Clamp(timeSlice / response, 0.0, 1.0);
		m_fVerticalVelocity = m_fVerticalVelocity + (desiredVertical - m_fVerticalVelocity) * velocityAlpha;

		if (Math.AbsFloat(m_fVerticalVelocity) < 0.01 && desiredVertical == 0)
			m_fVerticalVelocity = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Drive the visual leg rig from the mech root frame on every machine.
	protected void UpdateProceduralLegs(float timeSlice)
	{
		if (!m_bLegPairsReady || !m_aAllLegs || m_aAllLegs.IsEmpty())
			return;

		foreach (BFA_MechLegPair legPair : m_aAllLegs)
		{
			if (legPair && legPair.IsInitialized())
				legPair.TickFromRoot(timeSlice);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Temporary MP diagnostic. Prints on server, physics owner and remote proxies so we can
	//! compare the same mech's root position and replicated input on each machine.
	protected void PrintMpDiag(IEntity owner, float timeSlice)
	{
		m_fMpDiagAccumTime += timeSlice;
		if (m_fMpDiagAccumTime < 1.0)
			return;

		m_fMpDiagAccumTime = 0;

		string role = "CLIENT";
		if (Replication.IsServer())
			role = "SERVER";

		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		bool rplOwner = true;
		if (rpl)
			rplOwner = rpl.IsOwner();

		vector pos = owner.GetOrigin();
		vector vel = vector.Zero;
		Physics rb = owner.GetPhysics();
		if (rb)
			vel = rb.GetVelocity();

		BFA_MechLog.Log(string.Format(
			"[BFA MP DIAG] role=%1 rplOwner=%2 hasPilot=%3 localPilot=%4 pos=%5",
			role,
			rplOwner,
			m_Pilot != null,
			HasLocalPilot(),
			pos
		));

		BFA_MechLog.Log(string.Format(
			"[BFA MP DIAG] vel=%1 repFwd=%2 repStrafe=%3 repTurn=%4 sprint=%5",
			vel,
			m_fFwd,
			m_fStrafe,
			m_fTurn,
			m_bSprint
		));

		BFA_MechLog.Log(string.Format(
			"[BFA MP ROOTSYNC] seq=%1 ready=%2 authPos=%3",
			m_iNetTransformSequence,
			m_bNetTransformReady,
			m_vNetPosition
		));
	}

	//------------------------------------------------------------------------------------------------
	protected float GetWorldTimeSeconds()
	{
		IEntity owner = GetOwner();
		if (!owner || !owner.GetWorld())
			return 0;

		return owner.GetWorld().GetWorldTime() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	//! Smooth real-time clock for the client side of interpolation. World time can advance in
	//! steps; this cannot. Only ever compared against itself (via the clock offset).
	protected float GetLocalClockSeconds()
	{
		return System.GetTickCount() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	//! Shortest signed difference b - a in degrees, in [-180, 180].
	protected static float BFA_DeltaAngle(float a, float b)
	{
		float d = b - a;
		while (d > 180.0)
			d -= 360.0;
		while (d < -180.0)
			d += 360.0;
		return d;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector BFA_LerpAngles(vector a, vector b, float t)
	{
		vector r;
		r[0] = a[0] + BFA_DeltaAngle(a[0], b[0]) * t;
		r[1] = a[1] + BFA_DeltaAngle(a[1], b[1]) * t;
		r[2] = a[2] + BFA_DeltaAngle(a[2], b[2]) * t;
		return r;
	}

	//==================================================================================================
	// #4 DRIVER RECONCILIATION
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected void ResetReconciliation()
	{
		m_aPredSeq.Clear();
		m_aPredPos.Clear();
		m_aPredYaw.Clear();
		m_iLastReconciledAck = -1;
		m_vReconcilePending = vector.Zero;
		m_fReconcileYawPending = 0;
		m_iReconcileStreak = 0;
		m_vReconcileErrorSum = vector.Zero;
		m_iReconcileYawStreak = 0;
		m_fReconcileYawErrorSum = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Called by SCR_PlayerController on the driver's client each time an input packet is sent.
	//! Records where the local prediction was at that moment so the server's later answer for
	//! this sequence can be compared like-for-like (cancels out network latency).
	int ClientRecordInputSent()
	{
		m_iClientInputSeq++;

		IEntity owner = GetOwner();
		if (!owner)
			return m_iClientInputSeq;

		m_aPredSeq.Insert(m_iClientInputSeq);
		m_aPredPos.Insert(owner.GetOrigin());
		m_aPredYaw.Insert(owner.GetYawPitchRoll()[0]);

		while (m_aPredSeq.Count() > BFA_PRED_HISTORY_SIZE)
		{
			m_aPredSeq.RemoveOrdered(0);
			m_aPredPos.RemoveOrdered(0);
			m_aPredYaw.RemoveOrdered(0);
		}

		return m_iClientInputSeq;
	}

	//------------------------------------------------------------------------------------------------
	protected void ReconcileDriver(vector serverPos, vector serverRot, int ackSeq)
	{
		if (ackSeq < 0 || ackSeq <= m_iLastReconciledAck)
			return;

		int index = m_aPredSeq.Find(ackSeq);
		if (index < 0)
			return;

		m_iLastReconciledAck = ackSeq;

		vector predictedPos = m_aPredPos[index];
		float predictedYaw = m_aPredYaw[index];

		// Drop history the server has already confirmed.
		for (int i = 0; i <= index; i++)
		{
			m_aPredSeq.RemoveOrdered(0);
			m_aPredPos.RemoveOrdered(0);
			m_aPredYaw.RemoveOrdered(0);
		}

		// Vertical is owned by the terrain/jump follower on both machines; only correct it on a snap.
		vector error = serverPos - predictedPos;
		vector horizontal = error;
		horizontal[1] = 0;
		float horizontalError = horizontal.Length();
		float yawError = BFA_DeltaAngle(predictedYaw, serverRot[0]);

		m_fDiagReconcileError = horizontalError;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (horizontalError >= Math.Max(m_fReconcileSnapDistance, 0.5))
		{
			// Large divergence (stuck on geometry, long hitch): snap to the server.
			vector origin = owner.GetOrigin() + error;
			owner.SetOrigin(origin);

			vector ypr = owner.GetYawPitchRoll();
			ypr[0] = ypr[0] + yawError;
			owner.SetYawPitchRoll(ypr);

			m_vReconcilePending = vector.Zero;
			m_fReconcileYawPending = 0;

			// Unconfirmed history is now in the old frame; discard it.
			m_aPredSeq.Clear();
			m_aPredPos.Clear();
			m_aPredYaw.Clear();

			m_iDiagReconcileSnaps++;
			BFA_MechLog.Log(string.Format("[BFA RECONCILE] SNAP error=%1m yaw=%2deg", horizontalError, yawError), LogLevel.WARNING);
			return;
		}

		// Only correct drift that is REAL: the error must stay over the deadband for several
		// snapshots in a row. Single readings over the line are just network jitter, and correcting
		// them is what made the speed surge. The committed correction is the average of the streak.
		// After committing, shift remaining history by the same amount so it is not counted twice.
		bool corrected = false;
		int confirmCount = Math.Max(m_iReconcileConfirmCount, 1);

		if (horizontalError > Math.Max(m_fReconcileDeadband, 0.0))
		{
			m_iReconcileStreak++;
			m_vReconcileErrorSum = m_vReconcileErrorSum + horizontal;

			if (m_iReconcileStreak >= confirmCount)
			{
				vector average = m_vReconcileErrorSum * (1.0 / m_iReconcileStreak);
				m_vReconcilePending = average;
				for (int p = 0; p < m_aPredPos.Count(); p++)
					m_aPredPos[p] = m_aPredPos[p] + average;

				m_iReconcileStreak = 0;
				m_vReconcileErrorSum = vector.Zero;
				corrected = true;
			}
		}
		else
		{
			m_iReconcileStreak = 0;
			m_vReconcileErrorSum = vector.Zero;
		}

		// Yaw: never while the driver is turning (input timing alone makes a few degrees of
		// apparent error mid-turn), and only after the same confirmation streak.
		bool turning = Math.AbsFloat(GetEffectiveTurnInput()) > 0.05;
		if (!turning && Math.AbsFloat(yawError) > Math.Max(m_fReconcileYawDeadbandDeg, 0.0))
		{
			m_iReconcileYawStreak++;
			m_fReconcileYawErrorSum = m_fReconcileYawErrorSum + yawError;

			if (m_iReconcileYawStreak >= confirmCount)
			{
				float averageYaw = m_fReconcileYawErrorSum / m_iReconcileYawStreak;
				m_fReconcileYawPending = averageYaw;
				for (int y = 0; y < m_aPredYaw.Count(); y++)
					m_aPredYaw[y] = m_aPredYaw[y] + averageYaw;

				m_iReconcileYawStreak = 0;
				m_fReconcileYawErrorSum = 0;
				corrected = true;
			}
		}
		else
		{
			m_iReconcileYawStreak = 0;
			m_fReconcileYawErrorSum = 0;
		}

		if (corrected && m_bDebugReconcile)
			Print(string.Format("[BFA RECONCILE] blend error=%1m yaw=%2deg ack=%3", horizontalError, yawError, ackSeq));
	}

	//------------------------------------------------------------------------------------------------
	//! Positional part of the blend, as extra velocity (physics-friendly, no teleport).
	//! Called from Move() on the driver's client only.
	protected vector ConsumeReconcileVelocity(float timeSlice)
	{
		if (m_vReconcilePending.LengthSq() < 0.0001 || timeSlice <= 0)
		{
			m_vReconcilePending = vector.Zero;
			return vector.Zero;
		}

		vector correctionVel = m_vReconcilePending * Math.Max(m_fReconcileGain, 0.1);
		float maxSpeed = Math.Max(m_fReconcileMaxSpeed, 0.1);
		float speed = correctionVel.Length();
		if (speed > maxSpeed)
			correctionVel = correctionVel * (maxSpeed / speed);

		vector step = correctionVel * timeSlice;
		if (step.LengthSq() >= m_vReconcilePending.LengthSq())
		{
			correctionVel = m_vReconcilePending * (1.0 / timeSlice);
			m_vReconcilePending = vector.Zero;
		}
		else
		{
			m_vReconcilePending = m_vReconcilePending - step;
		}

		return correctionVel;
	}

	//------------------------------------------------------------------------------------------------
	//! Yaw part of the blend. Applied directly to the transform in small steps so it cannot
	//! depend on the engine's angular-velocity sign convention.
	protected void ApplyReconcileYaw(IEntity owner, float timeSlice)
	{
		if (!owner || Math.AbsFloat(m_fReconcileYawPending) < 0.01 || timeSlice <= 0)
		{
			m_fReconcileYawPending = 0;
			return;
		}

		float alpha = Math.Clamp(timeSlice * Math.Max(m_fReconcileGain, 0.1), 0.0, 1.0);
		float step = m_fReconcileYawPending * alpha;

		vector ypr = owner.GetYawPitchRoll();
		ypr[0] = ypr[0] + step;
		owner.SetYawPitchRoll(ypr);

		m_fReconcileYawPending = m_fReconcileYawPending - step;
	}

	//==================================================================================================
	// #5 / #6 SNAPSHOTS: single RPC path + observer interpolation buffer
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected void AcceptServerTransform(vector pos, vector rot, int sequence, float serverTime, int ackSeq)
	{
		// Unreliable packets may arrive out of order. Ignore anything older than the newest accepted.
		if (sequence <= m_iNetTransformSeen)
			return;

		m_iNetTransformSeen = sequence;
		m_vNetPosition = pos;
		m_vNetRotation = rot;
		m_bNetTransformReady = true;

		// Clock offset = local time - server time (includes one-way latency). Track the minimum
		// (least-delayed packet) and let it relax slowly upward to follow clock drift.
		float offset = GetLocalClockSeconds() - serverTime;
		if (!m_bClockOffsetValid || offset < m_fClockOffset)
		{
			m_fClockOffset = offset;
			m_bClockOffsetValid = true;
		}
		else
		{
			m_fClockOffset = m_fClockOffset + (offset - m_fClockOffset) * 0.02;
		}

		m_aSnapTime.Insert(serverTime);
		m_aSnapPos.Insert(pos);
		m_aSnapRot.Insert(rot);

		while (m_aSnapTime.Count() > BFA_NET_BUFFER_SIZE)
		{
			m_aSnapTime.RemoveOrdered(0);
			m_aSnapPos.RemoveOrdered(0);
			m_aSnapRot.RemoveOrdered(0);
		}

		if (HasLocalPilot())
			ReconcileDriver(pos, rot, ackSeq);
	}

	//------------------------------------------------------------------------------------------------
	//! The ONE server->client root snapshot path. Broadcast to observers.
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_ServerRootSnapshot(vector pos, vector rot, int sequence, float serverTime, int ackSeq)
	{
		if (Replication.IsServer())
			return;

		AcceptServerTransform(pos, rot, sequence, serverTime, ackSeq);
	}

	//------------------------------------------------------------------------------------------------
	//! Same snapshot to the Rpl owner (the driver's client after ownership transfer), so the
	//! driver always gets its input ack whether or not Broadcast includes the owner. The shared
	//! sequence number makes a duplicate harmless.
	[RplRpc(RplChannel.Unreliable, RplRcver.Owner)]
	protected void RpcDo_OwnerRootSnapshot(vector pos, vector rot, int sequence, float serverTime, int ackSeq)
	{
		if (Replication.IsServer())
			return;

		AcceptServerTransform(pos, rot, sequence, serverTime, ackSeq);
	}

	//------------------------------------------------------------------------------------------------
	//! Dedicated/server authority publishes the root it actually simulated. Clients never send
	//! positions to the server; they only send input wishes through SCR_PlayerController.
	protected void PublishServerTransform(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer() || !owner)
			return;

		m_fNetSnapshotTimer += timeSlice;

		float interval = Math.Max(m_fNetSnapshotInterval, 0.033);
		if (!m_Pilot)
			interval = Math.Max(interval, 0.20);

		if (m_fNetSnapshotTimer < interval)
			return;

		m_fNetSnapshotTimer = 0;
		m_vNetPosition = owner.GetOrigin();
		m_vNetRotation = owner.GetLocalYawPitchRoll();
		m_iNetTransformSequence++;

		float serverTime = GetWorldTimeSeconds();

		Rpc(RpcDo_ServerRootSnapshot, m_vNetPosition, m_vNetRotation, m_iNetTransformSequence, serverTime, m_iServerLastInputSeq);

		// Only when a client actually owns the mech (pilot seated, ownership transferred).
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (m_Pilot && rpl && !rpl.IsOwner())
			Rpc(RpcDo_OwnerRootSnapshot, m_vNetPosition, m_vNetRotation, m_iNetTransformSequence, serverTime, m_iServerLastInputSeq);
	}

	//------------------------------------------------------------------------------------------------
	//! Observer copies render the server root a fixed delay in the past, blending between the
	//! two buffered snapshots around that time. Procedural legs then derive their pose from this
	//! smooth observed root motion; no leg transforms are replicated.
	protected void ApplyServerTransform(IEntity owner, float timeSlice)
	{
		if (!owner || !m_bNetTransformReady || m_aSnapTime.IsEmpty())
			return;

		int count = m_aSnapTime.Count();
		vector pos;
		vector rot;

		if (count == 1 || !m_bClockOffsetValid)
		{
			pos = m_aSnapPos[count - 1];
			rot = m_aSnapRot[count - 1];
		}
		else
		{
			float renderTime = GetLocalClockSeconds() - m_fClockOffset - Math.Max(m_fNetInterpDelay, 0.0);

			if (renderTime <= m_aSnapTime[0])
			{
				pos = m_aSnapPos[0];
				rot = m_aSnapRot[0];
			}
			else if (renderTime >= m_aSnapTime[count - 1])
			{
				// Late packet: extrapolate briefly along the last segment, then hold.
				int a = count - 2;
				int b = count - 1;
				float span = m_aSnapTime[b] - m_aSnapTime[a];
				float over = Math.Min(renderTime - m_aSnapTime[b], Math.Max(m_fNetMaxExtrapolation, 0.0));

				pos = m_aSnapPos[b];
				rot = m_aSnapRot[b];

				vector segment = m_aSnapPos[b] - m_aSnapPos[a];
				float snapDist = Math.Max(m_fNetSnapDistance, 0.25);
				if (span > 0.001 && over > 0 && segment.LengthSq() < snapDist * snapDist)
				{
					float t = over / span;
					pos = pos + segment * t;
					rot = BFA_LerpAngles(m_aSnapRot[a], m_aSnapRot[b], 1.0 + t);
				}
			}
			else
			{
				int i = count - 1;
				while (i > 0 && m_aSnapTime[i - 1] > renderTime)
					i--;

				int from = i - 1;
				int to = i;
				float span2 = m_aSnapTime[to] - m_aSnapTime[from];
				float t2 = 1.0;
				if (span2 > 0.0001)
					t2 = Math.Clamp((renderTime - m_aSnapTime[from]) / span2, 0.0, 1.0);

				vector gap = m_aSnapPos[to] - m_aSnapPos[from];
				float snapDist2 = Math.Max(m_fNetSnapDistance, 0.25);

				if (gap.LengthSq() >= snapDist2 * snapDist2)
				{
					// Teleport/respawn between these two snapshots: do not slide across the map.
					pos = m_aSnapPos[to];
					rot = m_aSnapRot[to];
				}
				else
				{
					pos = m_aSnapPos[from] + gap * t2;
					rot = BFA_LerpAngles(m_aSnapRot[from], m_aSnapRot[to], t2);
				}
			}
		}

		owner.SetOrigin(pos);
		owner.SetYawPitchRoll(rot);

		Physics rb = owner.GetPhysics();
		if (rb)
		{
			rb.SetVelocity(vector.Zero);
			rb.SetAngularVelocity(vector.Zero);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnUpdate(float timeSlice)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;


		// V12: count the actual component frame cadence.
		m_iDiagFrames++;
		m_fDiagAccumTime += timeSlice;
		m_fDiagMinDt = Math.Min(m_fDiagMinDt, timeSlice);
		m_fDiagMaxDt = Math.Max(m_fDiagMaxDt, timeSlice);

		if (!m_bDiagHaveStartPos)
		{
			m_vDiagStartPos = owner.GetOrigin();
			m_bDiagHaveStartPos = true;
		}

		// INPUT CONTEXT FIX:
		// Enfusion input contexts must be kept active continuously while the mech is
		// being driven. V3-v12 only re-activated them from the 33 ms network timer.
		// At ~100 FPS that left roughly two out of every three frames with the
		// CharacterForward action reading 0, which is exactly what the telemetry showed.
		// Restore the original working behavior: activate the contexts on EVERY frame
		// for the local pilot.
		if (HasLocalPilot())
		{
			InputManager input = GetGame().GetInputManager();
			if (input)
			{
				input.ActivateContext("CharacterGeneralContext");
				input.ActivateContext("CharacterMovementContext");
				input.ActivateContext("CharacterMovementLeanContext");
				input.ActivateContext("CharacterWeaponContext");
				input.ActivateContext("PlayerCameraContext");
			}
		}

		// IMPORTANT: do not tick observer legs before applying the newest server root.
		// In V25 the observer calculated world-space foot IK from the OLD root and then moved
		// the whole walker to the new root afterwards. That leaves the remote leg targets one
		// transform behind and can make the walker look like it is floating above the terrain.
		// Server/local-prediction legs still tick in their original place below.

		// The current SampleMech is a standalone Vehicle, not an inherited vanilla wheeled
		// vehicle. Entering the pilot seat transfers Rpl ownership to the client, but the stock
		// vehicle root transform is not being published back to the dedicated server.
		//
		// V23 therefore makes the SERVER root authoritative and keeps local-driver prediction
		// only for responsiveness. Observers follow buffered server RPC snapshots (#5/#6).
		bool isServer = Replication.IsServer();
		bool localPrediction = !isServer && HasLocalPilot();

		if (!isServer && !localPrediction)
		{
			// V26 REMOTE GROUND IK:
			// 1) move the observer root to the latest authoritative server transform
			// 2) only then derive gait + world-space planted-foot IK from that observed root
			// This keeps the remote feet visually attached to the terrain instead of carrying
			// the previous frame's local-space target upward with the body.
			ApplyServerTransform(owner, timeSlice);
			UpdateProceduralLegs(timeSlice);
			return;
		}

		// Server and local driver prediction keep the established V20 gait ordering.
		// Their leg rig observes the root produced by the previous physics frame, which is
		// already the tested smooth singleplayer behavior.
		UpdateProceduralLegs(timeSlice);

		// Count authoritative/predicted simulation frames separately from Rpl ownership.
		m_iDiagOwnerFrames++;

		if (m_bAirborne)
		{
			UpdateAirborne(timeSlice);
		}
		else
		{
			UpdateBodyHeight(timeSlice);
		}

		Move(timeSlice);

		if (localPrediction)
			ApplyReconcileYaw(owner, timeSlice);

		if (isServer)
			PublishServerTransform(owner, timeSlice);

	}

	//------------------------------------------------------------------------------------------------
	protected void PrintRootDiag(IEntity owner)
	{
		if (!HasLocalPilot() || m_fDiagAccumTime < 1.0)
			return;

		vector nowPos = owner.GetOrigin();
		vector delta = nowPos - m_vDiagStartPos;
		delta[1] = 0;
		float distance = delta.Length();

		BFA_MechLog.Log(string.Format(
			"[BFA ROOT DIAG] frames=%1 ownerFrames=%2 avgDtMs=%3 minDtMs=%4 maxDtMs=%5 distance=%6 velBefore=%7 velAfter=%8 fwdInput=%9",
			m_iDiagFrames,
			m_iDiagOwnerFrames,
			(m_fDiagAccumTime / Math.Max(m_iDiagFrames, 1)) * 1000.0,
			m_fDiagMinDt * 1000.0,
			m_fDiagMaxDt * 1000.0,
			distance,
			m_fDiagVelBefore,
			m_fDiagVelAfter,
			GetEffectiveForwardInput()
		));

		BFA_MechLog.Log(string.Format(
			"[BFA SLOPE DIAG] supportY=%1 targetY=%2 heightError=%3 verticalVel=%4",
			m_fDiagSupportY,
			m_fDiagTargetY,
			m_fDiagHeightError,
			m_fVerticalVelocity
		));

		if (!Replication.IsServer())
		{
			BFA_MechLog.Log(string.Format(
				"[BFA RECONCILE DIAG] lastError=%1m pending=%2m yawPending=%3deg snaps=%4 history=%5",
				m_fDiagReconcileError,
				m_vReconcilePending.Length(),
				m_fReconcileYawPending,
				m_iDiagReconcileSnaps,
				m_aPredSeq.Count()
			));
		}

		BFA_MechLog.Log(string.Format(
			"[BFA STABILITY DIAG] frontBlock=%1 rearBlock=%2 obstacleRise=%3 tiltDeg=%4",
			m_bFrontObstacle,
			m_bRearObstacle,
			m_fDiagObstacleRise,
			m_fDiagTiltDeg
		));

		if (m_bLegPairsReady && m_aAllLegs && !m_aAllLegs.IsEmpty() && m_aAllLegs[0])
		{
			BFA_MechLegPair diagPair = m_aAllLegs[0];
			BFA_MechLog.Log(string.Format(
				"[BFA LEG DIAG] speed=%1 cadence=%2 phase=%3 rootTicks=%4",
				diagPair.GetMeasuredSpeed(),
				diagPair.GetCadence(),
				diagPair.GetPhase(),
				diagPair.GetRootDrivenTickCount()
			));
		}
		else
		{
			BFA_MechLog.Log("[BFA LEG DIAG] NOT READY");
		}

		m_iDiagFrames = 0;
		m_iDiagOwnerFrames = 0;
		m_fDiagAccumTime = 0;
		m_fDiagMinDt = 999.0;
		m_fDiagMaxDt = 0;
		m_vDiagStartPos = nowPos;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		m_bSupportFilterInitialized = false;
		m_bNetTransformReady = false;
		m_iNetTransformSeen = 0;
		m_fNetSnapshotTimer = 0;
		m_aSnapTime.Clear();
		m_aSnapPos.Clear();
		m_aSnapRot.Clear();
		m_bClockOffsetValid = false;
		m_fLastRemoteTickTime = -1;
		ResetReconciliation();

		if (Replication.IsServer())
		{
			m_vNetPosition = owner.GetOrigin();
			m_vNetRotation = owner.GetLocalYawPitchRoll();
		}

		SetPilot(null);

		if (!m_aLegPairs || m_aLegPairs.IsEmpty())
		{
			BFA_MechLog.Log("[BFA MechCore] No leg pairs configured on mech.", LogLevel.ERROR);
			return;
		}

		m_SlotManager = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));
		if (!m_SlotManager)
		{
			BFA_MechLog.Log("[BFA MechCore] Missing SlotManagerComponent on mech: " + owner, LogLevel.ERROR);
			return;
		}

		TryInitializeLegPairs();
	}

	//------------------------------------------------------------------------------------------------
	//! Remote observer tick. This intentionally runs independently of EOnFrame because a
	//! non-owned walker on a dedicated-server client may only receive occasional frame events.
	//! Local pilots do NOT use this path; they keep the real per-frame prediction path.
	protected void RemoteVisualTick()
	{
		if (Replication.IsServer() || System.IsConsoleApp())
			return;

		if (HasLocalPilot())
		{
			m_fLastRemoteTickTime = -1;
			return;
		}

		ObserverUpdate(-1);
	}

	//------------------------------------------------------------------------------------------------
	//! Observer update, called from EVERY rendered frame when the engine delivers EOnFrame, and
	//! from the 33 ms fallback timer when it does not. Both share one clock, so the mech is never
	//! advanced twice in the same frame. Updating only from the 30 Hz timer made remote mechs step
	//! visibly at 60+ fps.
	protected void ObserverUpdate(float frameDt)
	{
		float now = GetLocalClockSeconds();

		// Frame path: the engine's own frame time step, every rendered frame.
		if (frameDt >= 0)
		{
			m_fLastRemoteTickTime = now;
			OnUpdate(Math.Clamp(frameDt, 0.0, 0.1));
			return;
		}

		// Fallback timer path: only when frames have stopped arriving for this proxy.
		if (m_fLastRemoteTickTime >= 0 && now - m_fLastRemoteTickTime < 0.05)
			return;

		float dt = 0.033;
		if (m_fLastRemoteTickTime >= 0)
			dt = Math.Min(now - m_fLastRemoteTickTime, 0.1);

		m_fLastRemoteTickTime = now;
		OnUpdate(dt);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// Server simulation and the local driver's prediction need true render/physics frames.
		if (Replication.IsServer() || HasLocalPilot())
		{
			OnUpdate(timeSlice);
			return;
		}

		// Remote observers: update every rendered frame when frames arrive. ObserverUpdate()
		// shares its clock with the fallback timer, so nothing is double-ticked.
		if (!System.IsConsoleApp())
			ObserverUpdate(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);

		ApplySizeScale();

		ApplyGroundPoint(owner);

		// Stand at the parked height from the first frame. Before this, the height stayed 0
		// until someone got in or out, so every mech spawned with its origin ON the ground -
		// barely visible on a small walker, a sunken body on a big one.
		m_fBodyHeight = m_fBodyHeightEmpty;

		BFA_MechLog.Log("[BFA MP BUILD] V50 FIRE_DIAGNOSTIC_COMPILE_FIX");

		if (!Replication.IsServer() && !System.IsConsoleApp())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(RemoteVisualTick);
				queue.CallLater(RemoteVisualTick, Math.Max(m_iRemoteVisualTickMs, 20), true);
				m_bRemoteVisualTickRunning = true;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stop every repeating/delayed call before this component goes away. Without this the
	//! call queue keeps invoking methods on a destroyed mech (death, despawn, GC cleanup).
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(RemoteVisualTick);
				queue.Remove(TryInitializeLegPairs);
			}

			m_bRemoteVisualTickRunning = false;
			m_bLegRetryQueued = false;
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! One knob for big (or small) mechs: everything measured in metres that was tuned for a
	//! Sentinel-sized walker grows with the mech.
	protected void ApplySizeScale()
	{
		float k = m_fSizeScale;
		if (k <= 0 || Math.AbsFloat(k - 1.0) < 0.001)
			return;

		m_fGroundProbeForward *= k;
		m_fGroundProbeSide *= k;
		m_fMaxObstacleStepUp *= k;
		m_fObstacleBlockRise *= k;
		m_fObstacleBlockDistance *= k;
		m_fGroundRiseSpeed *= k;
		m_fGroundFallSpeed *= k;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground Point -> standing heights. The point sits at the sole of the feet in the mech's own
	//! space, so the origin must stand exactly that far above the ground.
	protected void ApplyGroundPoint(IEntity owner)
	{
		if (!m_GroundPoint || !owner)
			return;

		m_GroundPoint.Init(owner);
		vector mat[4];
		m_GroundPoint.GetModelTransform(mat);

		vector groundPos = mat[3];
		float standHeight = -groundPos[1];
		if (standHeight < 0)
		{
			BFA_MechLog.Log(string.Format("[BFA BODY] %1: Ground Point is ABOVE the mech's origin (Y %2). Move it down to the soles of the feet.", owner, groundPos[1]), LogLevel.WARNING);
			standHeight = 0;
		}
		else if (standHeight < 0.05)
		{
			BFA_MechLog.Log(string.Format("[BFA BODY] %1: Ground Point is at the origin, so standing height = 0. Fine if the model's origin is already at the feet; otherwise set the point's Offset Y to the soles (e.g. 0 -12 0).", owner), LogLevel.WARNING);
		}

		m_fBodyHeightPilot = standHeight;
		m_fBodyHeightEmpty = Math.Max(standHeight - Math.Max(m_fParkedCrouch, 0), 0);

		BFA_MechLog.Log(string.Format("[BFA BODY] standing height from Ground Point: %1 m (parked %2 m)", m_fBodyHeightPilot, m_fBodyHeightEmpty));
	}
}
