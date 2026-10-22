//------------------------------------------------------------------------------------------------
//! EXPERIMENTAL: server-only arcing launch. Not activated on any shipping prefab.
//! V2 regression isolation: flight code restored exactly from proven commit d2ef093. Mark targeting is intentionally not used by throw on this test branch.
[ComponentEditorProps(category: "CGS MechCore", description: "EXPERIMENTAL companion tactical throw; OFF by default.")]
class CGS_MechTacticalThrowComponentClass : ScriptComponentClass
{
}

class CGS_MechTacticalThrowComponent : ScriptComponent
{
	[Attribute("false", UIWidgets.CheckBox, "Enable experimental tactical throw on this mech.", category: "Tactical Throw")]
	protected bool m_bEnabled;

	[Attribute("false", UIWidgets.CheckBox, "ALLOW UNSAFE TEST: landing protection remains experimental; no collision-damage immunity or overhead obstacle sweep. Test in a disposable world only.", category: "Tactical Throw")]
	protected bool m_bAllowUnprotectedTest;

	[Attribute("20", UIWidgets.Auto, "Horizontal throw distance (metres).", category: "Tactical Throw")]
	protected float m_fDistance;

	[Attribute("6", UIWidgets.Auto, "Height of the arc above the launch point (metres).", category: "Tactical Throw")]
	protected float m_fArcHeight;

	[Attribute("20", UIWidgets.Auto, "Cooldown between throws (seconds).", category: "Tactical Throw")]
	protected float m_fCooldown;

	[Attribute("6", UIWidgets.Auto, "Maximum player distance for activation (metres).", category: "Tactical Throw")]
	protected float m_fUseDistance;

	[Attribute("false", UIWidgets.CheckBox, "After launching, order the mech to FOLLOW. OFF: order it to GUARD its current position and provide support fire with its existing AI weapons. This does not override HOLD FIRE.", category: "Tactical Throw")]
	protected bool m_bFollowAfterLaunch;

	[Attribute(desc: "THROW POINT: place the gizmo on the mech where the player should leave its hand / launcher, exactly like the Movement Ground Point. Its local blue Z axis is throw-forward.", category: "Tactical Throw")]
	protected ref PointInfo m_ThrowPoint;

	[Attribute("0.5", UIWidgets.Auto, "Seconds to blend from the player's position to the throw arc without an abrupt velocity change.", category: "Tactical Throw")]
	protected float m_fLaunchBlendSeconds;

	[Attribute("true", UIWidgets.CheckBox, "EXPERIMENT: use a smooth, velocity-matched launch transition to reduce first-person camera jerk. OFF uses the older blend.", category: "Tactical Throw")]
	protected bool m_bSmoothCameraLaunch;

	[Attribute("1.15", UIWidgets.Auto, "Height above detected ground where the throw ends. Keep above 1m for first tests.", category: "Tactical Throw")]
	protected float m_fLandingClearance;

	[Attribute("0.4", UIWidgets.Auto, "Seconds to gently stabilize velocity at end of throw (test only).", category: "Tactical Throw")]
	protected float m_fLandingStabilizeSeconds;

	[Attribute("false", UIWidgets.CheckBox, "CAMERA EXPERIMENT: explicitly Update() the thrown character after SetOrigin each frame. OFF tests whether forcing the entity update causes camera stutter; ON is the previous behavior.", category: "Tactical Throw")]
	protected bool m_bForceCharacterUpdateEachFrame;

	[Attribute("false", UIWidgets.CheckBox, "V2 TEST: use physics-driven flight. If its probe fails, stop instead of automatically teleporting via V1. OFF selects V1 manually.", category: "Tactical Throw V2")]
	protected bool m_bV2TryPhysicsFlight;

	[Attribute("0.35", UIWidgets.Auto, "Time to confirm physics launch movement. A failed V2 probe releases the character without teleporting into V1.", category: "Tactical Throw V2")]
	protected float m_fV2ProbeSeconds;

	[Attribute("0.7", UIWidgets.Auto, "Distance player must move during probe before treating native launch as working.", category: "Tactical Throw V2")]
	protected float m_fV2MinimumMovement;

	[Attribute("2.0", UIWidgets.Auto, "While native flight is active, slow downward speed this far above terrain. Terrain-only test.", category: "Tactical Throw V2")]
	protected float m_fV2CushionHeight;

	[Attribute("true", UIWidgets.CheckBox, "Allow a friendly player to mark a landing destination in front of them before throwing.", category: "Tactical Throw V2")]
	protected bool m_bAllowMarkedTarget;

	[Attribute("75", UIWidgets.Auto, "Maximum distance from the mech's throw point to a marked destination (metres).", category: "Tactical Throw V2")]
	protected float m_fMarkMaximumDistance;

	[Attribute("true", UIWidgets.CheckBox, "Raycast from the character eye height along its forward aim direction. This is NOT yet client camera crosshair targeting.", category: "Tactical Throw V2")]
	protected bool m_bTraceMarkTarget;

	[Attribute("1.6", UIWidgets.Auto, "Height above feet where target trace starts.", category: "Tactical Throw V2")]
	protected float m_fMarkEyeHeight;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Optional visible world particle at marked landing location. Supply a persistent marker .ptc; no default FX.", category: "Tactical Throw V2", params: "ptc")]
	protected ResourceName m_sMarkEffect;

	protected ParticleEffectEntity m_pMarkEffectInstance;

	protected void ClearMarkEffect()
	{
		if (!m_pMarkEffectInstance)
			return;
		delete m_pMarkEffectInstance;
		m_pMarkEffectInstance = null;
	}

	protected void ShowMarkEffect(vector worldPosition)
	{
		ClearMarkEffect();
		if (m_sMarkEffect.IsEmpty())
			return;
		ParticleEffectEntitySpawnParams params();
		params.Transform[3] = worldPosition;
		m_pMarkEffectInstance = ParticleEffectEntity.SpawnParticleEffect(m_sMarkEffect, params);
		if (!m_pMarkEffectInstance)
			Print("[CGS THROW MARK] configured particle did not spawn", LogLevel.WARNING);
	}

	protected bool m_bHasMarkedTarget;
	protected vector m_vMarkedTarget;
	protected int m_iMarkedPlayerId;

	// Registered on the server for a player-side recall interaction.
	protected static ref array<CGS_MechTacticalThrowComponent> s_aActiveThrowers = {};

	static bool RecallForPlayer(IEntity user)
	{
		if (!Replication.IsServer() || !user)
			return false;
		foreach (CGS_MechTacticalThrowComponent thrower : s_aActiveThrowers)
		{
			if (!thrower)
				continue;
			CGS_MechBrainComponent brain = thrower.GetBrain();
			if (brain && brain.ServerRecallCompanion(user))
				return true;
		}
		Print("[CGS RECALL] No claimed companion accepted remote recall", LogLevel.WARNING);
		return false;
	}

	//! Radio GUARD looks up the assigned companion exactly as remote recall does.
	static bool GuardForPlayer(IEntity user)
	{
		if (!Replication.IsServer() || !user)
			return false;
		foreach (CGS_MechTacticalThrowComponent thrower : s_aActiveThrowers)
		{
			if (!thrower)
				continue;
			CGS_MechBrainComponent brain = thrower.GetBrain();
			if (brain && brain.ServerRadioGuardCompanion(user))
				return true;
		}
		Print("[CGS RADIO] No claimed companion accepted GUARD POSITION", LogLevel.WARNING);
		return false;
	}

	//! Mark a ground destination based on the player's current facing direction.
	//! This is NOT camera crosshair or map-click targeting; those require a player RPC.
	bool ServerMarkThrowTarget(IEntity user)
	{
		if (!Replication.IsServer() || !CanOffer(user) || !m_bAllowMarkedTarget || !GetGame())
			return false;
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return false;
		int id = manager.GetPlayerIdFromControlledEntity(user);
		if (id <= 0 || manager.GetPlayerControlledEntity(id) != user)
			return false;
		vector direction = user.GetTransformAxis(2);
		if (direction.LengthSq() < 0.01)
			return false;
		direction.Normalize();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;
		float maxRange = Math.Clamp(m_fMarkMaximumDistance, 5, 100);
		vector eye = user.GetOrigin() + vector.Up * Math.Max(m_fMarkEyeHeight, 0.5);
		vector candidate = eye + direction * maxRange;
		if (m_bTraceMarkTarget)
		{
			TraceParam trace = new TraceParam();
			trace.Start = eye;
			trace.End = candidate;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.Exclude = user;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			float fraction = world.TraceMove(trace, null);
			if (fraction < 1.0)
				candidate = eye + (candidate - eye) * fraction;
		}
		// Target ground elevation, not a high wall or sky location.
		candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);
		// Use the SAME actual throw socket origin as ServerThrow. The player's
		// position can be several metres away from that socket, so a 75 m
		// player-facing mark was previously rejected as 77+ m at launch.
		IEntity mech = GetOwner();
		vector socket = user.GetOrigin();
		if (m_ThrowPoint)
		{
			m_ThrowPoint.Init(mech);
			vector socketLocal[4];
			vector mechTransform[4];
			m_ThrowPoint.GetModelTransform(socketLocal);
			mech.GetWorldTransform(mechTransform);
			vector p = socketLocal[3];
			socket = mechTransform[3] + mechTransform[0] * p[0] + mechTransform[1] * p[1] + mechTransform[2] * p[2];
		}
		vector deltaFromSocket = candidate - socket;
		deltaFromSocket[1] = 0;
		float socketDistance = deltaFromSocket.Length();
		if (socketDistance < 5)
		{
			Print("[CGS THROW MARK] target too close to throw socket", LogLevel.WARNING);
			return false;
		}
		if (socketDistance > maxRange)
		{
			deltaFromSocket.Normalize();
			candidate = socket + deltaFromSocket * (maxRange - 0.25);
			candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);
			Print(string.Format("[CGS THROW MARK] clamped to socket range from %1 to %2 m", socketDistance, maxRange - 0.25));
		}
		m_vMarkedTarget = candidate;
		m_iMarkedPlayerId = id;
		m_bHasMarkedTarget = true;
		ShowMarkEffect(candidate);
		Print(string.Format("[CGS THROW MARK] owner=%1 landing target=%2 (aim-direction trace with ground projection)", id, candidate));
		return true;
	}

	protected bool m_bV2Probing;
	protected bool m_bV2Flying;
	protected float m_fV2Elapsed;
	protected vector m_vV2StartPos;
	protected vector m_vV2LaunchVelocity;
	[Attribute("false", UIWidgets.CheckBox, "V2 TEST: raise character slightly to clear grounded state before applying launch velocity. OFF preserves prior V2 behavior.", category: "Tactical Throw V2")]
	protected bool m_bV2GroundRelease;

	[Attribute("1.2", UIWidgets.Auto, "Ground release vertical clearance (metres); only used when Ground Release enabled.", category: "Tactical Throw V2")]
	protected float m_fV2ReleaseHeight;

	[Attribute("0.08", UIWidgets.Auto, "Time after one-time lift before applying native launch velocity, seconds.", category: "Tactical Throw V2")]
	protected float m_fV2ReleaseDelay;

	protected bool m_bV2PendingRelease;
	protected float m_fV2ReleaseElapsed;


	protected vector m_vV2PreviousPos;
	protected float m_fV2MovementAccum;
	protected int m_iV2ProbeSamples;

	protected vector m_vPlayerStart;
	protected float m_fLandingStabilizeLeft;
	protected bool m_bLandingStabilizing;

	protected float m_fNextThrowTime;
	protected IEntity m_pThrownCharacter;
	protected vector m_vThrowStart;
	protected vector m_vThrowEnd;
	protected float m_fThrowElapsed;
	protected float m_fThrowDuration;
	protected float m_fThrowApex;
	protected int m_iThrowLogFrames;

	//------------------------------------------------------------------------------------------------
	//! Proof-of-concept only. Character-controller authority may undo SetOrigin after this frame.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer())
		{
			SetEventMask(owner, EntityEvent.FRAME);
			if (s_aActiveThrowers.Find(this) == -1)
				s_aActiveThrowers.Insert(this);
		}
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer() || !m_pThrownCharacter || !GetGame())
			return;

		// V2 uses regular character physics rather than SetOrigin while native flight works.
		// Earlier raw velocity experiments did not visibly move players, so always probe.
		if (m_bV2Probing || m_bV2Flying)
		{
			m_fV2Elapsed += Math.Max(timeSlice, 0);
			vector actualPos = m_pThrownCharacter.GetOrigin();
			Physics probePhysics = m_pThrownCharacter.GetPhysics();
			BaseWorld probeWorld = GetGame().GetWorld();
			if (m_bV2Probing && m_fV2Elapsed >= Math.Max(m_fV2ProbeSeconds, 0.15))
			{
				float displacement = vector.Distance(actualPos, m_vV2StartPos);
				Print(string.Format("[CGS THROW V2] native motion probe displacement=%1 actual=%2", displacement, actualPos));
				m_bV2Probing = false;
				if (displacement < Math.Max(m_fV2MinimumMovement, 0.2))
				{
					// A failed native launch must not silently teleport the player
					// through the scripted V1 arc. Leave the character controller alone.
					Print(string.Format("[CGS THROW V2] no native movement; aborting without V1 fallback pos=%1", actualPos), LogLevel.WARNING);
					m_bV2Flying = false;
					m_pThrownCharacter = null;
					return;
				}
				m_bV2Flying = true;
				Print("[CGS THROW V2] physics flight active; no forced character position updates");
			}
			if (m_bV2Flying)
			{
				if (!probePhysics || !probeWorld)
				{
					m_bV2Flying = false;
					m_pThrownCharacter = null;
					return;
				}
				float ground = probeWorld.GetSurfaceY(actualPos[0], actualPos[2]);
				float clearance = actualPos[1] - ground;
				vector v = probePhysics.GetVelocity();
				// Apply fall cushioning near terrain, modeled on the old grav-chute logic.
				if (clearance <= Math.Max(m_fV2CushionHeight, 1.0) && v[1] < 0)
				{
					v[1] = Math.Max(v[1], -2.0);
					v[0] = v[0] * 0.35;
					v[2] = v[2] * 0.35;
					probePhysics.SetVelocity(v);
				}
				if (clearance <= 1.3 && m_fV2Elapsed > 0.8)
				{
					m_bV2Flying = false;
					m_bLandingStabilizing = true;
					m_fLandingStabilizeLeft = Math.Max(m_fLandingStabilizeSeconds, 0.1);
					Print(string.Format("[CGS THROW V2] landing approached position=%1; stabilizing", actualPos));
				}
				else if (m_fV2Elapsed > 9.0)
				{
					Print("[CGS THROW V2] flight timeout; character returned to normal controller", LogLevel.WARNING);
					m_bV2Flying = false;
					m_pThrownCharacter = null;
				}
			}
			return;
		}

		if (m_bLandingStabilizing)
		{
			Physics activePhysics = m_pThrownCharacter.GetPhysics();
			if (activePhysics)
				activePhysics.SetVelocity(vector.Zero);
			m_fLandingStabilizeLeft -= timeSlice;
			if (m_fLandingStabilizeLeft <= 0)
			{
				Print(string.Format("[CGS THROW TEST] landing stabilization ended position=%1", m_pThrownCharacter.GetOrigin()));
				m_bLandingStabilizing = false;
				m_pThrownCharacter = null;
			}
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			m_pThrownCharacter = null;
			return;
		}

		m_fThrowElapsed += Math.Max(timeSlice, 0);
		float t = Math.Min(m_fThrowElapsed / Math.Max(m_fThrowDuration, 0.1), 1.0);
		vector position = m_vThrowStart + (m_vThrowEnd - m_vThrowStart) * t;
		position[1] = m_vThrowStart[1] + (m_vThrowEnd[1] - m_vThrowStart[1]) * t + 4.0 * m_fThrowApex * t * (1.0 - t);

		// Smoothly connect the player's real starting position to the trajectory.
		// The original smoothstep blend had a hard velocity discontinuity when it ended,
		// which can cause a visible camera jerk even though the player stays upright.
		// Cubic Hermite here matches the flight arc's velocity at the join.
		float blendDuration = Math.Max(0, m_fLaunchBlendSeconds);
		if (blendDuration > 0 && m_fThrowElapsed < blendDuration)
		{
			float u = Math.Clamp(m_fThrowElapsed / blendDuration, 0, 1);
			if (m_bSmoothCameraLaunch)
			{
				float duration = Math.Max(m_fThrowDuration, 0.1);
				float endT = Math.Min(blendDuration / duration, 1.0);
				vector endPoint = m_vThrowStart + (m_vThrowEnd - m_vThrowStart) * endT;
				endPoint[1] = m_vThrowStart[1] + (m_vThrowEnd[1] - m_vThrowStart[1]) * endT + 4.0 * m_fThrowApex * endT * (1.0 - endT);
				vector endVelocity = (m_vThrowEnd - m_vThrowStart) * (1.0 / duration);
				endVelocity[1] = endVelocity[1] + 4.0 * m_fThrowApex * (1.0 - 2.0 * endT) / duration;
				float u2 = u * u;
				float u3 = u2 * u;
				float startWeight = 2.0 * u3 - 3.0 * u2 + 1.0;
				float endWeight = -2.0 * u3 + 3.0 * u2;
				float velocityWeight = u3 - u2;
				position = m_vPlayerStart * startWeight + endPoint * endWeight + endVelocity * (blendDuration * velocityWeight);
			}
			else
			{
				float blend = u * u * (3.0 - 2.0 * u);
				position = m_vPlayerStart + (position - m_vPlayerStart) * blend;
			}
		}

		float terrain = world.GetSurfaceY(position[0], position[2]);
		// Never reject the very first frame because the character's feet start near ground.
		// Once clear of the launch phase, reject terrain obstructions rather than phase through.
		if (t > 0.08 && t < 0.92 && position[1] <= terrain + 0.35)
		{
			Print(string.Format("[CGS THROW TEST] STOP: path hits terrain t=%1 desired=%2 terrain=%3", t, position, terrain), LogLevel.WARNING);
			m_pThrownCharacter = null;
			return;
		}
		// No wall/roof collision scan or landing protection yet: TEST WORLD ONLY.
		vector before = m_pThrownCharacter.GetOrigin();
		m_pThrownCharacter.SetOrigin(position);
		// SetOrigin updates the spatial location; the extra explicit entity Update
		// might be interfering with the normal character-controller/camera update.
		// Test with it disabled; switch it back on to compare with the previous build.
		if (m_bForceCharacterUpdateEachFrame)
			m_pThrownCharacter.Update();

		if (m_iThrowLogFrames < 6)
		{
			Print(string.Format("[CGS THROW TEST] frame=%1 before=%2 desired=%3 actual=%4", m_iThrowLogFrames, before, position, m_pThrownCharacter.GetOrigin()));
			m_iThrowLogFrames++;
		}

		if (t >= 1.0)
		{
			Physics landingPhysics = m_pThrownCharacter.GetPhysics();
			if (landingPhysics)
				landingPhysics.SetVelocity(vector.Zero);
			m_bLandingStabilizing = true;
			m_fLandingStabilizeLeft = Math.Max(m_fLandingStabilizeSeconds, 0.1);
			Print(string.Format("[CGS THROW TEST] arrived above ground=%1; stabilizing velocity for %2 seconds", m_pThrownCharacter.GetOrigin(), m_fLandingStabilizeLeft));
		}
	}

	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
			s_aActiveThrowers.RemoveItem(this);
		ClearMarkEffect();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected CGS_MechBrainComponent GetBrain()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;
		return CGS_MechBrainComponent.Find(owner);
	}

	//! Client uses this only for showing the action. Server repeats all checks.
	bool CanOffer(IEntity user)
	{
		if (!m_bEnabled || !m_bAllowUnprotectedTest || !user || !GetOwner())
			return false;
		if (!ChimeraCharacter.Cast(user))
			return false;
		if (vector.DistanceSq(user.GetOrigin(), GetOwner().GetOrigin()) > Math.Pow(Math.Max(m_fUseDistance, 2.0), 2))
			return false;
		CGS_MechBrainComponent brain = GetBrain();
		return brain && brain.CanShowCompanionAction(user);
	}

	bool ServerThrow(IEntity user)
	{
		if (!Replication.IsServer() || !CanOffer(user) || !GetGame())
			return false;
		BaseWorld world = GetGame().GetWorld();
		PlayerManager players = GetGame().GetPlayerManager();
		if (!world || !players)
			return false;
		int playerId = players.GetPlayerIdFromControlledEntity(user);
		if (playerId <= 0 || players.GetPlayerControlledEntity(playerId) != user)
			return false;

		float now = world.GetWorldTime() * 0.001;
		if (now < m_fNextThrowTime)
			return false;

		IEntity mech = GetOwner();
		SCR_DamageManagerComponent health = SCR_DamageManagerComponent.Cast(user.FindComponent(SCR_DamageManagerComponent));
		if (health && health.GetState() == EDamageState.DESTROYED)
			return false;

		Physics body = user.GetPhysics();
		if (!body)
		{
			Print("[CGS THROW] Character physics unavailable; no launch performed.", LogLevel.WARNING);
			return false;
		}

		// Default to the character's current position when no throw point was placed.
		// Placing a PointInfo on the mech overrides BOTH the arc origin and forward direction.
		vector from = user.GetOrigin();
		vector forward = mech.GetTransformAxis(2);
		if (m_ThrowPoint)
		{
			m_ThrowPoint.Init(mech);
			vector localPoint[4];
			vector worldMech[4];
			m_ThrowPoint.GetModelTransform(localPoint);
			mech.GetWorldTransform(worldMech);
			vector p = localPoint[3];
			from = worldMech[3] + worldMech[0] * p[0] + worldMech[1] * p[1] + worldMech[2] * p[2];
			vector axis = localPoint[2];
			forward = worldMech[0] * axis[0] + worldMech[1] * axis[1] + worldMech[2] * axis[2];
		}
		forward[1] = 0;
		if (forward.LengthSq() < 0.01)
			return false;
		forward.Normalize();

		float launchGround = world.GetSurfaceY(from[0], from[2]);
		if (from[1] < launchGround + 0.6)
		{
			Print(string.Format("[CGS THROW TEST] STOP: throw point is below terrain/too low. socket=%1 surface=%2; raise Throw Point in prefab.", from, launchGround), LogLevel.WARNING);
			return false;
		}

		float distance = Math.Clamp(m_fDistance, 5.0, 60.0);
		float apex = Math.Clamp(m_fArcHeight, 3.0, 16.0);
		float gravity = 9.81;
		float vertical = Math.Sqrt(2.0 * gravity * apex);
		float airtime = (2.0 * vertical) / gravity;
		vector target = from + forward * distance;
		bool useMark = m_bAllowMarkedTarget && m_bHasMarkedTarget && m_iMarkedPlayerId == playerId;
		if (useMark)
		{
			// Only alter the horizontal destination inputs. Keep the proven
			// V2 physics launch, probe, camera and landing implementation intact.
			vector markedDelta = m_vMarkedTarget - from;
			markedDelta[1] = 0;
			float markedDistance = markedDelta.Length();
			float maxMarkedDistance = Math.Clamp(m_fMarkMaximumDistance, 5.0, 100.0);
			if (markedDistance >= 5.0 && markedDistance <= maxMarkedDistance)
			{
				distance = markedDistance;
				forward = markedDelta.Normalized();
				target = m_vMarkedTarget;
				Print(string.Format("[CGS THROW MARK] using selected destination=%1 distance=%2", target, distance));
			}
			else
			{
				// Player may have walked between MARK and THROW. For a modest
				// overshoot, retain the chosen heading and cap its distance.
				// Large or too-close invalid marks still use the fixed throw.
				if (markedDistance > maxMarkedDistance && markedDistance <= maxMarkedDistance + 10.0)
				{
					forward = markedDelta.Normalized();
					distance = maxMarkedDistance - 0.25;
					target = from + forward * distance;
					target[1] = world.GetSurfaceY(target[0], target[2]);
					Print(string.Format("[CGS THROW MARK] minor range overshoot corrected: %1 to %2 m", markedDistance, distance));
				}
				else
				{
					useMark = false;
					Print(string.Format("[CGS THROW MARK] destination outside launch range distance=%1 max=%2; using fixed throw", markedDistance, maxMarkedDistance), LogLevel.WARNING);
				}
			}
		}
		float targetSurface = world.GetSurfaceY(target[0], target[2]);
		if (targetSurface > target[1] + 1.5)
		{
			Print("[CGS THROW] Landing ground is too high; launch cancelled.", LogLevel.WARNING);
			return false;
		}
		// This checks only landing height. Full obstacle sweeps and safe-landing protection
		// are required before this experiment can ship as gameplay.


		// A direct physics velocity compiled but didn't visibly move the character.
		// Diagnose movement with a server-driven arc instead of claiming it succeeded.
		if (m_pThrownCharacter)
			return false;
		m_pThrownCharacter = user;
		// Clear the saved mark only after the launch was accepted.
		if (useMark)
		{
			m_bHasMarkedTarget = false;
			ClearMarkEffect();
		}
		m_vPlayerStart = user.GetOrigin();
		m_vThrowStart = from;
		m_vThrowEnd = target;
		// End close enough to the ground to prevent a damaging free fall after SetOrigin stops.
		// Terrain-only first pass: bridges/rooftops need further work.
		m_vThrowEnd[1] = targetSurface + Math.Max(m_fLandingClearance, 1.0);
		m_bLandingStabilizing = false;
		m_fLandingStabilizeLeft = 0;
		m_fThrowElapsed = 0;
		m_fThrowDuration = airtime;
		m_fThrowApex = apex;
		m_iThrowLogFrames = 0;
		m_fNextThrowTime = now + Math.Max(m_fCooldown, 1.0);
		m_bV2Probing = false;
		m_bV2Flying = false;
		if (m_bV2TryPhysicsFlight)
		{
			// Test a native push from the player's actual position, avoiding a camera
			// teleport to the mech socket. Use socket orientation to calculate direction.
			m_vV2StartPos = user.GetOrigin();
			m_vV2LaunchVelocity = forward * (distance / Math.Max(airtime, 0.1));
			m_vV2LaunchVelocity[1] = vertical;
			m_fV2Elapsed = 0;
			m_bV2Probing = true;
			body.SetVelocity(m_vV2LaunchVelocity);
			Print(string.Format("[CGS THROW V2] physics launch attempted player=%1 velocity=%2; probing", playerId, m_vV2LaunchVelocity));
		}
		Print(string.Format("[CGS THROW TEST] arc started player=%1 from=%2 to=%3 duration=%4", playerId, from, m_vThrowEnd, airtime));
		CGS_MechBrainComponent brain = GetBrain();
		if (brain)
		{
			int postThrowOrder = CGS_EMechCompanionOrder.GUARD;
			if (m_bFollowAfterLaunch)
				postThrowOrder = CGS_EMechCompanionOrder.FOLLOW;

			if (!brain.ServerCommandCompanion(user, postThrowOrder))
				Print("[CGS THROW TEST] Post-throw companion order rejected; check owner/faction/range", LogLevel.WARNING);
			else
				Print(string.Format("[CGS THROW TEST] Companion order after launch: %1", typename.EnumToString(CGS_EMechCompanionOrder, postThrowOrder)));
		}
		Print(string.Format("[CGS THROW TEST] attempted player=%1 distance=%2 apex=%3; movement unverified", playerId, distance, apex));
		return true;
	}
}
