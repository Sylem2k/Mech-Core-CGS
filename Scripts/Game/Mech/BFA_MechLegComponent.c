[ComponentEditorProps(category: "GameScripted/Misc", description: "BFA procedural mech leg IK component")]
class BFA_MechLegComponentClass : ScriptComponentClass
{
}

class BFA_MechLegComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.EditBox, "Height added to the middle of a swing")]
	float m_fStepHeight;

	[Attribute("3", UIWidgets.EditBox, "Total height the leg can trace for obstacles")]
	float m_fObstacleStepHeight;

	[Attribute("0.4", UIWidgets.Slider, "Fraction of gait cycle spent in swing", "0.2 0.49 0.01")]
	float m_fSwingFraction;

	[Attribute("0.3", UIWidgets.EditBox, "Ignore measured root motion below this speed")]
	float m_fMotionDeadZone;

	[Attribute("0.2", UIWidgets.EditBox, "Proxy root-motion smoothing window in seconds")]
	float m_fProxySmoothTime;

	[Attribute("3.0", UIWidgets.EditBox, "Stride length at reference speed")]
	float m_fStrideLength;

	[Attribute("5.0", UIWidgets.EditBox, "Reference speed for stride growth")]
	float m_fStrideRefSpeed;

	[Attribute("0.25", UIWidgets.EditBox, "Stride growth exponent")]
	float m_fStrideGrowth;

	[Attribute("1.5", UIWidgets.EditBox, "Minimum stride length")]
	float m_fStrideMin;

	[Attribute("4.0", UIWidgets.EditBox, "Maximum stride length")]
	float m_fStrideMax;

	[Attribute("0.35", UIWidgets.EditBox, "Minimum gait cycles per second")]
	float m_fMinCadence;

	[Attribute("1.8", UIWidgets.EditBox, "Maximum gait cycles per second")]
	float m_fMaxCadence;

	[Attribute("3.0", UIWidgets.EditBox, "Maximum forward/back prediction for a foot target")]
	float m_fMaxFootLead;

	[Attribute("0.8", UIWidgets.EditBox, "Most a foot steps SIDEWAYS ahead of its rest spot when strafing, in metres (scaled by Size Scale). Lower = no wide side steps.")]
	protected float m_fMaxSideLead;

	[Attribute("0.35", UIWidgets.Slider, "Feet never cross: a foot stays at least this share of its rest distance on its own side of the body (0 = may reach the centre line).", "0 1 0.05")]
	protected float m_fNoCrossMargin;

	[Attribute("12.0", UIWidgets.EditBox, "Clamp measured root speed used by the visual rig")]
	float m_fMaxMeasuredSpeed;

	[Attribute(desc: "Optional: foot bone, only used by the IK debug view (DiagMenu). Not required.")]
	ref PointInfo m_LastBone;

	// ---- Your own leg model / rig ---------------------------------------------------------------
	[Attribute("1", UIWidgets.Auto, "SIZE SCALE of this leg. 1 = Sentinel leg. Scales step height, stride, foot lead and ground search so giant legs walk with giant steps. Use the same value as the mech's Movement > Size Scale.", category: "Leg Rig")]
	float m_fSizeScale;

	[Attribute("0 0 0", UIWidgets.Auto, "Where this foot rests, from the leg's attach point, in the MECH's directions: X = sideways OUT from the body, Y = up, Z = forward. Mirrored automatically for left legs. Spiders / quads: e.g. 1.5 0 0.5 puts the foot out to the side and a bit forward. 0 0 0 = straight below the hip (human-style).", category: "Leg Rig")]
	vector m_vFootRestOffset;

	[Attribute("TargetX", UIWidgets.EditBox, "Anim graph float variable for the IK target X (leg model space). Change only if your own graph uses other names.", category: "Leg Rig")]
	string m_sTargetVarX;

	[Attribute("TargetY", UIWidgets.EditBox, "Anim graph float variable for the IK target Y.", category: "Leg Rig")]
	string m_sTargetVarY;

	[Attribute("TargetZ", UIWidgets.EditBox, "Anim graph float variable for the IK target Z.", category: "Leg Rig")]
	string m_sTargetVarZ;

	[Attribute("SOUND_MECH_STEP", UIWidgets.EditBox, "Footstep sound event (needs SCR_SoundDataComponent on the leg). Empty = silent.", category: "Leg Rig")]
	string m_sStepSoundEvent;

	[Attribute("SOUND_MECH_SERVO", UIWidgets.EditBox, "Servo sound event played when the foot lifts for a step (same .acp as the footstep). Empty = silent.", category: "Leg Rig")]
	string m_sServoSoundEvent;

	[Attribute("0.1", UIWidgets.Auto, "Camera shake per footstep (0 = none). Small walkers / spiders: 0.", category: "Leg Rig")]
	float m_fStepCameraShake;

	[Attribute("1", UIWidgets.Slider, "LIMP: how much this leg limps when damaged (shorter, lower, dragging steps). 0 = never limps. Needs a LEG hit zone on the leg (Mech_Leg_Base.et has one).", "0 2 0.1", category: "Leg Rig")]
	protected float m_fLimpStrength;

	[Attribute("0.0", UIWidgets.EditBox, "Optional visual sole trim in metres. Leave at 0 unless a specific leg model has a known IK-marker-to-sole offset.")]
	float m_fFootSoleDrop;

	protected AnimationControllerComponent m_AnimController;
	protected vector m_vIKTargetVars;
	protected BFA_MechLegPair m_Pair;
	protected IEntity m_BodyEntity;
	protected bool m_bIsLeftLeg;
	protected bool m_bLegInitialized;

	// World-space foot state. During stance m_vPlantedFoot never moves with the mech root.
	protected vector m_vPlantedFoot;
	protected vector m_vSwingFrom;
	protected vector m_vSwingTarget;
	protected vector m_vCurrentIK;
	protected bool m_bWasSwinging;

	// V15: the mech root is the single clock for procedural gait.
	// Child legs no longer run their own EOnFrame/fallback loop.
	protected int m_iRootDrivenTicks;

	// Limp: 0 healthy .. 1 destroyed (times Limp Strength). Re-read twice a second.
	protected float m_fLimp;
	protected int m_iNextLimpCheckMs;


	//------------------------------------------------------------------------------------------------
	//! 0 = healthy, 1 = destroyed leg (more with Limp Strength above 1).
	float GetLimp()
	{
		return m_fLimp;
	}

	//------------------------------------------------------------------------------------------------
	float GetSwingFraction()
	{
		return Math.Clamp(m_fSwingFraction, 0.2, 0.49);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateLimp()
	{
		BaseWorld world = GetOwner().GetWorld();
		if (!world)
			return;
		int now = world.GetWorldTime();
		if (now < m_iNextLimpCheckMs && m_iNextLimpCheckMs - now < 1000)
			return;
		m_iNextLimpCheckMs = now + 500;

		m_fLimp = 0;
		if (m_fLimpStrength <= 0)
			return;

		if (m_BodyEntity)
		{
			BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(m_BodyEntity.FindComponent(BFA_MechMovementComponent));
			if (movement && !movement.LegDamageSlows())
				return; // boss
		}

		BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(GetOwner());
		if (!zone || zone.GetPartType() != BFA_EMechPart.LEG)
			return;

		float limp = 0;
		if (zone.GetDamageState() == EDamageState.DESTROYED)
			limp = 1.0;
		else if (zone.GetMaxHealth() > 0)
			limp = Math.Clamp(1.0 - zone.GetHealth() / zone.GetMaxHealth(), 0, 1) * 0.6; // hurt: a lighter limp
		m_fLimp = limp * m_fLimpStrength;
	}

	//------------------------------------------------------------------------------------------------
	float GetHeight()
	{
		return m_vPlantedFoot[1];
	}

	//------------------------------------------------------------------------------------------------
	float GetMotionDeadZone()
	{
		return m_fMotionDeadZone;
	}

	//------------------------------------------------------------------------------------------------
	float GetProxySmoothTime()
	{
		return m_fProxySmoothTime;
	}

	//------------------------------------------------------------------------------------------------
	float GetMinCadence()
	{
		return m_fMinCadence;
	}

	//------------------------------------------------------------------------------------------------
	float GetMaxCadence()
	{
		return m_fMaxCadence;
	}

	//------------------------------------------------------------------------------------------------
	float GetMaxMeasuredSpeed()
	{
		return m_fMaxMeasuredSpeed;
	}

	//------------------------------------------------------------------------------------------------
	float StrideAt(float speed)
	{
		float refSpeed = Math.Max(m_fStrideRefSpeed, 0.01);
		float baseSpeed = Math.Max(speed, 0.01);
		float stride = m_fStrideLength * Math.Pow(baseSpeed / refSpeed, m_fStrideGrowth);
		return Math.Clamp(stride, m_fStrideMin, m_fStrideMax);
	}

	//------------------------------------------------------------------------------------------------
	vector TraceGroundFromPos(vector pos)
	{
		BaseWorld world = GetOwner().GetWorld();
		if (!world)
			return pos;

		vector localUp = vector.Up;
		if (m_BodyEntity)
		{
			localUp = m_BodyEntity.GetTransformAxis(1);
			if (localUp.LengthSq() > 0.0001)
				localUp.Normalize();
			else
				localUp = vector.Up;
		}

		// Search far enough down for tall legs: the hip of a giant can be 10+ m above the ground.
		vector from = pos + localUp * m_fObstacleStepHeight;
		vector to = pos - localUp * Math.Max(m_fObstacleStepHeight * 2.0, 60.0);

		for (int i = 0; i < 4; i++)
		{
			TraceParam param = new TraceParam();
			param.Start = from;
			param.End = to;
			param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			param.LayerMask = EPhysicsLayerPresets.Projectile;

			array<IEntity> excludeArray = { GetOwner() };
			if (m_BodyEntity)
				excludeArray.Insert(m_BodyEntity);
			param.ExcludeArray = excludeArray;

			float traceDistance = world.TraceMove(param, null);
			if (traceDistance >= 1.0)
				break;

			vector hitPosition = param.Start + (param.End - param.Start) * traceDistance;

			// Skip another part of our own walker and continue just below it.
			if (param.TraceEnt && m_BodyEntity && param.TraceEnt.GetRootParent() == m_BodyEntity)
			{
				from = hitPosition - localUp * 0.05;
				continue;
			}

			if (DiagMenu.GetBool(SCR_DebugMenuID.DEBUGUI_BFA_MECHCORE_DEBUG_IKTARGET))
				Shape.CreateArrow(param.Start, hitPosition, 0.1, Color.DODGER_BLUE, ShapeFlags.ONCE);

			return hitPosition;
		}

		return Vector(pos[0], SCR_TerrainHelper.GetTerrainY(pos), pos[2]);
	}

	//------------------------------------------------------------------------------------------------
	void ApplyIKToAnimation()
	{
		if (!m_AnimController)
			return;

		vector localIKTargetPos = GetOwner().CoordToLocal(m_vCurrentIK);

		// Optional per-leg visual sole trim. V30 calibrates the Sentinel stance at the body-height
		// controller instead; this remains available only for models whose IK marker is
		// intentionally offset from the visible sole.
		localIKTargetPos[1] = localIKTargetPos[1] - Math.Max(m_fFootSoleDrop, 0.0);

		m_AnimController.SetFloatVariable(m_vIKTargetVars[0], localIKTargetPos[0]);
		m_AnimController.SetFloatVariable(m_vIKTargetVars[1], localIKTargetPos[1]);
		m_AnimController.SetFloatVariable(m_vIKTargetVars[2], localIKTargetPos[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Scales every distance tuned for a Sentinel-sized leg (once).
	protected bool m_bSizeScaled;
	protected void ApplySizeScale()
	{
		if (m_bSizeScaled)
			return;
		m_bSizeScaled = true;

		float k = m_fSizeScale;
		if (k <= 0 || Math.AbsFloat(k - 1.0) < 0.001)
			return;

		m_fStepHeight *= k;
		m_fObstacleStepHeight *= k;
		m_fStrideLength *= k;
		m_fStrideRefSpeed *= k;
		m_fStrideMin *= k;
		m_fStrideMax *= k;
		m_fMaxFootLead *= k;
		m_fMaxSideLead *= k;
		m_fMotionDeadZone *= k;
		m_fMaxMeasuredSpeed *= k;
	}

	//------------------------------------------------------------------------------------------------
	//! Where this foot stands when idle: below the leg's attach point plus Foot Rest Offset
	//! (body directions, X mirrored for left legs).
	protected vector GetFootHome()
	{
		vector home = GetOwner().GetOrigin();
		if (m_vFootRestOffset.LengthSq() < 0.000001)
			return home;

		IEntity body = m_BodyEntity;
		if (!body)
			body = GetOwner().GetRootParent();

		vector right = body.GetTransformAxis(0);
		vector up = body.GetTransformAxis(1);
		vector forward = body.GetTransformAxis(2);

		float side = 1.0;
		if (m_bIsLeftLeg)
			side = -1.0;

		return home + right * (m_vFootRestOffset[0] * side) + up * m_vFootRestOffset[1] + forward * m_vFootRestOffset[2];
	}

	//------------------------------------------------------------------------------------------------
	protected vector BuildSwingTarget()
	{
		vector neutral = GetFootHome();
		if (!m_Pair)
			return TraceGroundFromPos(neutral);

		float cadence = Math.Max(m_Pair.GetCadence(), 0.01);
		float swingFraction = Math.Clamp(m_fSwingFraction, 0.2, 0.49);

		// Raibert-style lookahead: remaining swing plus half the stance.
		float swingTime = swingFraction / cadence;
		float stanceTime = (1.0 - swingFraction) / cadence;
		float tau = Math.Clamp(swingTime + stanceTime * 0.5, 0.05, 1.5);

		vector lead = m_Pair.GetMeasuredVelocity() * tau;
		lead[1] = 0;

		// Limp: a hurt leg reaches less far.
		lead = lead * Math.Clamp(1.0 - 0.55 * m_fLimp, 0.2, 1.0);

		IEntity body = m_BodyEntity;
		if (!body)
			body = GetOwner().GetRootParent();

		// Turning: the rest spot swings round the body, so aim where it will be.
		vector target = neutral;
		float turn = m_Pair.GetYawRate() * tau;
		if (body && Math.AbsFloat(turn) > 0.001)
		{
			vector centre = body.GetOrigin();
			vector arm = neutral - centre;
			float c = Math.Cos(-turn);
			float sn = Math.Sin(-turn);
			// Rotate around world up (yaw + = right = clockwise seen from above).
			vector rotated = Vector(arm[0] * c - arm[2] * sn, arm[1], arm[0] * sn + arm[2] * c);
			target = centre + rotated;
		}

		if (!body)
		{
			float leadLen = lead.Length();
			float maxLead = Math.Max(m_fMaxFootLead, 0.1);
			if (leadLen > maxLead)
				lead = lead * (maxLead / leadLen);
			return TraceGroundFromPos(target + lead);
		}

		// Lead in the body's own directions: forward/back up to Max Foot Lead, sideways much less.
		vector leadLocal = body.VectorToLocal(lead);
		leadLocal[0] = Math.Clamp(leadLocal[0], -Math.Max(m_fMaxSideLead, 0.0), Math.Max(m_fMaxSideLead, 0.0));
		leadLocal[2] = Math.Clamp(leadLocal[2], -Math.Max(m_fMaxFootLead, 0.1), Math.Max(m_fMaxFootLead, 0.1));
		leadLocal[1] = 0;

		// Never cross over to the other leg's side.
		vector targetLocal = body.CoordToLocal(target) + leadLocal;
		vector restLocal = body.CoordToLocal(neutral);
		float restSide = restLocal[0];
		float minSide = Math.AbsFloat(restSide) * Math.Clamp(m_fNoCrossMargin, 0, 1);
		if (restSide >= 0)
			targetLocal[0] = Math.Max(targetLocal[0], minSide);
		else
			targetLocal[0] = Math.Min(targetLocal[0], -minSide);

		return TraceGroundFromPos(body.CoordToParent(targetLocal));
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginSwing()
	{
		m_vSwingFrom = m_vPlantedFoot;
		m_vSwingTarget = BuildSwingTarget();
		m_vCurrentIK = m_vSwingFrom;

		if (!m_sServoSoundEvent.IsEmpty())
			SCR_SoundManagerModule.CreateAndPlayAudioSource(GetOwner(), m_sServoSoundEvent);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateSwing(float legPhase)
	{
		float swingFraction = Math.Clamp(m_fSwingFraction, 0.2, 0.49);
		float s = Math.Clamp(legPhase / swingFraction, 0.0, 1.0);
		float eased = s * s * (3.0 - 2.0 * s);
		// Limp: a hurt leg barely lifts its foot (drags).
		float arc = 4.0 * s * (1.0 - s) * m_fStepHeight * Math.Clamp(1.0 - 0.65 * m_fLimp, 0.15, 1.0);

		m_vCurrentIK = vector.Lerp(m_vSwingFrom, m_vSwingTarget, eased);
		m_vCurrentIK[1] = m_vCurrentIK[1] + arc;

		// Never let the visual foot arc go below the terrain under the current interpolation point.
		vector ground = TraceGroundFromPos(m_vCurrentIK);
		if (m_vCurrentIK[1] < ground[1])
			m_vCurrentIK[1] = ground[1];
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishSwing()
	{
		m_vPlantedFoot = m_vSwingTarget;
		m_vCurrentIK = m_vPlantedFoot;

		if (!m_sStepSoundEvent.IsEmpty())
			SCR_SoundManagerModule.CreateAndPlayAudioSource(GetOwner(), m_sStepSoundEvent);

		if (m_fStepCameraShake > 0)
			SCR_CameraShakeManagerComponent.AddCameraShake(m_fStepCameraShake, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick(IEntity owner, float timeSlice)
	{
		if (!m_bLegInitialized || !owner || !m_Pair || timeSlice <= 0)
			return;

		UpdateLimp();
		m_Pair.UpdateObservedMotion(timeSlice);

		float legPhase = m_Pair.GetLegPhase(m_bIsLeftLeg);
		bool moving = m_Pair.GetMeasuredSpeed() > 0;
		bool swinging = moving && legPhase < Math.Clamp(m_fSwingFraction, 0.2, 0.49);

		if (swinging && !m_bWasSwinging)
			BeginSwing();

		if (swinging)
		{
			UpdateSwing(legPhase);
		}
		else
		{
			if (m_bWasSwinging)
				FinishSwing();

			m_vCurrentIK = m_vPlantedFoot;
		}

		m_bWasSwinging = swinging;
		ApplyIKToAnimation();

		if (DiagMenu.GetBool(SCR_DebugMenuID.DEBUGUI_BFA_MECHCORE_DEBUG_IK))
			ShowDebug(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by BFA_MechLegPair from the mech root frame.
	void TickFromRoot(float timeSlice)
	{
		if (!m_bLegInitialized || timeSlice <= 0)
			return;

		m_iRootDrivenTicks++;
		Tick(GetOwner(), timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	int GetRootDrivenTicks()
	{
		return m_iRootDrivenTicks;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from BFA_MechLegPair after slot children are ready.
	bool InitLeg(BFA_MechLegPair pair, IEntity bodyEntity, bool isLeftLeg)
	{
		if (m_bLegInitialized)
			return true;

		if (!pair || !bodyEntity)
			return false;

		m_Pair = pair;
		m_BodyEntity = bodyEntity;
		m_bIsLeftLeg = isLeftLeg;
		ApplySizeScale();
		if (m_LastBone)
			m_LastBone.Init(GetOwner());

		m_AnimController = AnimationControllerComponent.Cast(GetOwner().FindComponent(AnimationControllerComponent));
		if (!m_AnimController)
		{
			BFA_MechLog.Log(string.Format("[BFA LEG] %1: no AnimationControllerComponent on the leg prefab - the leg cannot move. Add one with an IK anim graph (see MECH_LEGS_GUIDE.txt).", GetOwner()), LogLevel.WARNING);
			return false;
		}

		vector initPos = GetFootHome();
		m_vPlantedFoot = TraceGroundFromPos(initPos);
		m_vSwingFrom = m_vPlantedFoot;
		m_vSwingTarget = m_vPlantedFoot;
		m_vCurrentIK = m_vPlantedFoot;
		m_bWasSwinging = false;

		m_vIKTargetVars[0] = m_AnimController.BindFloatVariable(m_sTargetVarX);
		m_vIKTargetVars[1] = m_AnimController.BindFloatVariable(m_sTargetVarY);
		m_vIKTargetVars[2] = m_AnimController.BindFloatVariable(m_sTargetVarZ);

		if (m_vIKTargetVars[0] < 0 || m_vIKTargetVars[1] < 0 || m_vIKTargetVars[2] < 0)
			BFA_MechLog.Log(string.Format("[BFA LEG] %1: anim graph has no float variable(s) '%2' '%3' '%4' - the foot will not follow. Add them to your graph or set Leg Rig > Target Var names.", GetOwner(), m_sTargetVarX, m_sTargetVarY, m_sTargetVarZ), LogLevel.WARNING);

		m_bLegInitialized = true;

		if (m_bIsLeftLeg)
			BFA_MechLog.Log(string.Format("[BFA FOOT SOLE] drop=%1", m_fFootSoleDrop));

		// Keep the child active so its animation graph consumes TargetX/Y/Z,
		// but let the mech root own the only gait tick.
		GetOwner().SetFlags(EntityFlags.ACTIVE);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsInitialized()
	{
		return m_bLegInitialized;
	}

	//------------------------------------------------------------------------------------------------
	void ShowDebug(IEntity owner)
	{
		if (!m_LastBone)
			return;

		if (!m_LastBone.GetOwner())
			m_LastBone.Init(owner);

		vector lastBoneWorldMat[4];
		m_LastBone.GetWorldTransform(lastBoneWorldMat);

		Shape.CreateSphere(COLOR_RED, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, owner.GetOrigin(), 0.1);
		DebugTextWorldSpace.Create(GetGame().GetWorld(), "Leg Origin", DebugTextFlags.ONCE | DebugTextFlags.CENTER | DebugTextFlags.FACE_CAMERA,
			owner.GetOrigin()[0], owner.GetOrigin()[1], owner.GetOrigin()[2]);

		Shape.CreateSphere(COLOR_GREEN, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, m_vPlantedFoot, 0.1);
		DebugTextWorldSpace.Create(GetGame().GetWorld(), "Planted Foot", DebugTextFlags.ONCE | DebugTextFlags.CENTER | DebugTextFlags.FACE_CAMERA,
			m_vPlantedFoot[0], m_vPlantedFoot[1], m_vPlantedFoot[2]);

		Shape.CreateSphere(Color.YELLOW, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, m_vCurrentIK, 0.1);
		Shape.CreateSphere(COLOR_RED, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, lastBoneWorldMat[3], 0.1);
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		ShowDebug(owner);
	}

	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}
}

modded class ArmaReforgerScripted
{
	//------------------------------------------------------------------------------------------------
	override bool OnGameStart()
	{
		DiagMenu.RegisterMenu(SCR_DebugMenuID.DEBUGUI_BFA_MECHCORE_MAIN, "BFA MechCore", "");
		DiagMenu.RegisterBool(SCR_DebugMenuID.DEBUGUI_BFA_MECHCORE_DEBUG_IK, "", "Debug IK", "BFA MechCore");
		DiagMenu.RegisterBool(SCR_DebugMenuID.DEBUGUI_BFA_MECHCORE_DEBUG_IKTARGET, "", "Show IK Target", "BFA MechCore");
		return super.OnGameStart();
	}
}

modded enum SCR_DebugMenuID
{
	DEBUGUI_BFA_MECHCORE_MAIN,
	DEBUGUI_BFA_MECHCORE_DEBUG_IK,
	DEBUGUI_BFA_MECHCORE_DEBUG_IKTARGET,
}
