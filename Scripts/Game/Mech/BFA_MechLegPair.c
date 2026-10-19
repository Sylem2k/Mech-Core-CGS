[BaseContainerProps()]
class BFA_MechLegPair
{
	[Attribute("", UIWidgets.EditBox, "Left leg of pair", category: "Legs")]
	string m_sLeftLegSlotName;

	[Attribute("", UIWidgets.EditBox, "Right leg of pair", category: "Legs")]
	string m_sRightLegSlotName;

	[Attribute("-1", UIWidgets.Auto, "Gait timing of this pair, 0..1 of a step cycle. -1 = automatic: pairs alternate (pair 1 = 0, pair 2 = 0.5, pair 3 = 0 ...) which gives a trot on 4 legs and a tripod walk on 6. Same value on every pair = all left legs step together.", category: "Gait")]
	float m_fPhaseOffset;

	[Attribute("0.5", UIWidgets.Auto, "Right leg timing relative to the left leg (0.5 = opposite, normal walking; 0 = both together, e.g. a hopping/bounding pair).", category: "Gait")]
	float m_fRightLegPhase;

	protected float m_fAutoPhaseOffset;
	protected bool m_bWarnedSlots;

	protected BFA_MechLegComponent m_LeftLegComp;
	protected BFA_MechLegComponent m_RightLegComp;
	protected IEntity m_BodyEntity;
	protected bool m_bInitialized;

	// Shared gait state. Both legs read the same measured root motion and phase.
	protected vector m_vLastRootPos;
	protected vector m_vMeasuredVelocity;
	protected bool m_bHasLastRootPos;
	protected float m_fMeasuredSpeed;
	protected float m_fSignedForwardSpeed;
	protected float m_fPhase = 0.45;
	protected float m_fCadence;
	protected bool m_bHadMotion;
	protected int m_iLastMotionUpdateMs = -1;
	protected float m_fLastYaw;
	protected bool m_bHasLastYaw;
	protected float m_fYawRate;        // body turn rate, radians per second (+ = right)
	protected float m_fHipRadius = -1; // average distance from the body origin to the hips (flat)
	protected float m_fGaitSpeed;      // max(walking speed, hip speed from turning)

	//------------------------------------------------------------------------------------------------
	protected float Wrap01(float value)
	{
		float wrapped = value - Math.Floor(value);
		if (wrapped < 0)
			wrapped += 1.0;
		return wrapped;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsProxy()
	{
		if (!m_BodyEntity || Replication.IsServer())
			return false;

		RplComponent rpl = RplComponent.Cast(m_BodyEntity.FindComponent(RplComponent));
		return rpl && !rpl.IsOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Called by either leg. The world-time guard means the pair only measures the root once per frame/tick.
	void UpdateObservedMotion(float fallbackDt)
	{
		if (!m_bInitialized || !m_BodyEntity || !m_LeftLegComp)
			return;

		BaseWorld world = m_BodyEntity.GetWorld();
		if (!world)
			return;

		int nowMs = world.GetWorldTime();
		if (m_iLastMotionUpdateMs == nowMs)
			return;

		float dt = fallbackDt;
		if (m_iLastMotionUpdateMs >= 0)
		{
			dt = (nowMs - m_iLastMotionUpdateMs) * 0.001;
			if (dt <= 0)
				dt = fallbackDt;
		}
		m_iLastMotionUpdateMs = nowMs;

		if (dt <= 0)
			return;
		dt = Math.Clamp(dt, 0.001, 0.1);

		vector rootPos = m_BodyEntity.GetOrigin();
		if (!m_bHasLastRootPos)
		{
			m_vLastRootPos = rootPos;
			m_bHasLastRootPos = true;
			return;
		}

		vector delta = rootPos - m_vLastRootPos;
		m_vLastRootPos = rootPos;
		delta[1] = 0;

		vector rawVelocity = delta * (1.0 / dt);
		float rawSpeed = rawVelocity.Length();

		// A replication correction must never turn into a crazy gait-speed spike.
		float maxMeasured = Math.Max(m_LeftLegComp.GetMaxMeasuredSpeed(), 0.1);
		if (rawSpeed > maxMeasured)
		{
			rawVelocity = rawVelocity * (maxMeasured / rawSpeed);
			rawSpeed = maxMeasured;
		}

		if (IsProxy())
		{
			float smoothTime = Math.Max(m_LeftLegComp.GetProxySmoothTime(), 0.01);
			float alpha = Math.Clamp(dt / smoothTime, 0.0, 1.0);
			m_vMeasuredVelocity = m_vMeasuredVelocity + (rawVelocity - m_vMeasuredVelocity) * alpha;
		}
		else
		{
			m_vMeasuredVelocity = rawVelocity;
		}

		m_fMeasuredSpeed = m_vMeasuredVelocity.Length();

		vector forward = m_BodyEntity.GetTransformAxis(2);
		forward[1] = 0;
		if (forward.LengthSq() > 0.0001)
			forward.Normalize();

		m_fSignedForwardSpeed = vector.Dot(m_vMeasuredVelocity, forward);

		// Turning in place moves the hips around the body: count that as walking too, so the
		// legs step round instead of twisting on planted feet.
		float yaw = Math.Atan2(forward[0], forward[2]);
		if (m_bHasLastYaw)
		{
			float dYaw = yaw - m_fLastYaw;
			if (dYaw > Math.PI)
				dYaw -= Math.PI2;
			else if (dYaw < -Math.PI)
				dYaw += Math.PI2;
			float rawYawRate = dYaw / dt;
			float yawAlpha = Math.Clamp(dt / 0.15, 0.0, 1.0);
			m_fYawRate = m_fYawRate + (rawYawRate - m_fYawRate) * yawAlpha;
		}
		m_fLastYaw = yaw;
		m_bHasLastYaw = true;

		float turnSpeed = Math.AbsFloat(m_fYawRate) * Math.Max(GetHipRadius(), 0.5);

		float deadZone = Math.Max(m_LeftLegComp.GetMotionDeadZone(), 0.0);
		if (m_fMeasuredSpeed < deadZone)
		{
			m_vMeasuredVelocity = vector.Zero;
			m_fMeasuredSpeed = 0;
			m_fSignedForwardSpeed = 0;
		}
		if (turnSpeed < deadZone)
			turnSpeed = 0;

		float gaitSpeed = Math.Max(m_fMeasuredSpeed, turnSpeed);
		m_fGaitSpeed = gaitSpeed;

		if (gaitSpeed > 0)
		{
			float stride = m_LeftLegComp.StrideAt(gaitSpeed);
			m_fCadence = Math.Clamp(
				gaitSpeed / Math.Max(stride, 0.01),
				m_LeftLegComp.GetMinCadence(),
				m_LeftLegComp.GetMaxCadence()
			);

			m_fPhase = Wrap01(m_fPhase + m_fCadence * dt * LimpRate());
			m_bHadMotion = true;
		}
		else
		{
			m_fCadence = 0;

			// Stop in a double-stance part of the cycle so a foot can never freeze in mid-air.
			if (m_bHadMotion)
			{
				m_fPhase = 0.45;
				m_bHadMotion = false;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Limping rhythm: the hurt leg's step is slow (drag), the good leg's step is quick (hop).
	protected float LimpRate()
	{
		if (!m_LeftLegComp || !m_RightLegComp)
			return 1;

		float limpL = m_LeftLegComp.GetLimp();
		float limpR = m_RightLegComp.GetLimp();
		if (limpL <= 0 && limpR <= 0)
			return 1;

		float rate = 1;
		if (GetLegPhase(true) < m_LeftLegComp.GetSwingFraction())
			rate *= (1.0 - 0.45 * Math.Min(limpL, 1.5)) * (1.0 + 0.3 * Math.Min(limpR, 1.5));
		if (GetLegPhase(false) < m_RightLegComp.GetSwingFraction())
			rate *= (1.0 - 0.45 * Math.Min(limpR, 1.5)) * (1.0 + 0.3 * Math.Min(limpL, 1.5));
		return Math.Clamp(rate, 0.3, 1.6);
	}

	//------------------------------------------------------------------------------------------------
	//! Single root-driven gait tick. Both legs use one measured root motion and one phase.
	void TickFromRoot(float timeSlice)
	{
		if (!m_bInitialized || timeSlice <= 0)
			return;

		UpdateObservedMotion(timeSlice);

		if (m_LeftLegComp)
			m_LeftLegComp.TickFromRoot(timeSlice);

		if (m_RightLegComp)
			m_RightLegComp.TickFromRoot(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	float GetPhase()
	{
		return m_fPhase;
	}

	//------------------------------------------------------------------------------------------------
	int GetRootDrivenTickCount()
	{
		if (!m_LeftLegComp)
			return 0;

		return m_LeftLegComp.GetRootDrivenTicks();
	}

	//------------------------------------------------------------------------------------------------
	//! Speed that drives the gait: walking OR turning in place (> 0 = legs step).
	float GetMeasuredSpeed()
	{
		return m_fGaitSpeed;
	}

	//! Body turn rate in radians per second (+ = right).
	float GetYawRate()
	{
		return m_fYawRate;
	}

	//! Average flat distance from the body origin to the hips.
	float GetHipRadius()
	{
		if (m_fHipRadius >= 0)
			return m_fHipRadius;
		if (!m_BodyEntity || !m_LeftLegComp || !m_RightLegComp)
			return 0;

		vector l = m_BodyEntity.CoordToLocal(m_LeftLegComp.GetOwner().GetOrigin());
		vector r = m_BodyEntity.CoordToLocal(m_RightLegComp.GetOwner().GetOrigin());
		l[1] = 0;
		r[1] = 0;
		m_fHipRadius = (l.Length() + r.Length()) * 0.5;
		return m_fHipRadius;
	}

	//------------------------------------------------------------------------------------------------
	vector GetMeasuredVelocity()
	{
		return m_vMeasuredVelocity;
	}

	//------------------------------------------------------------------------------------------------
	float GetSignedForwardSpeed()
	{
		return m_fSignedForwardSpeed;
	}

	//------------------------------------------------------------------------------------------------
	float GetCadence()
	{
		return m_fCadence;
	}

	//------------------------------------------------------------------------------------------------
	float GetLegPhase(bool leftLeg)
	{
		float pairOffset = m_fPhaseOffset;
		if (pairOffset < 0)
			pairOffset = m_fAutoPhaseOffset;

		float phase = Wrap01(m_fPhase + pairOffset);
		if (leftLeg)
			return phase;

		return Wrap01(phase + m_fRightLegPhase);
	}

	//------------------------------------------------------------------------------------------------
	float GetAverageHeight()
	{
		if (!m_bInitialized || !m_LeftLegComp || !m_RightLegComp)
			return 0;

		return (m_LeftLegComp.GetHeight() + m_RightLegComp.GetHeight()) * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	bool IsInitialized()
	{
		return m_bInitialized;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true only when both attached leg entities and their components are fully ready.
	bool Init(SlotManagerComponent slotManager, IEntity bodyEntity, bool leftLegFirst)
	{
		if (m_bInitialized)
			return true;

		if (!slotManager || !bodyEntity)
			return false;

		if (m_sLeftLegSlotName.IsEmpty() || m_sRightLegSlotName.IsEmpty())
			return false;

		EntitySlotInfo leftSlot = slotManager.GetSlotByName(m_sLeftLegSlotName);
		EntitySlotInfo rightSlot = slotManager.GetSlotByName(m_sRightLegSlotName);
		if (!leftSlot || !rightSlot)
		{
			if (!m_bWarnedSlots)
			{
				m_bWarnedSlots = true;
				BFA_MechLog.Log(string.Format("[BFA LEG] leg pair '%1' / '%2': no SlotManager slot with that name on the mech. Slot names must match m_aLegPairs exactly.", m_sLeftLegSlotName, m_sRightLegSlotName), LogLevel.WARNING);
			}
			return false;
		}

		IEntity leftLeg = leftSlot.GetAttachedEntity();
		IEntity rightLeg = rightSlot.GetAttachedEntity();
		if (!leftLeg || !rightLeg)
			return false;

		m_LeftLegComp = BFA_MechLegComponent.Cast(leftLeg.FindComponent(BFA_MechLegComponent));
		m_RightLegComp = BFA_MechLegComponent.Cast(rightLeg.FindComponent(BFA_MechLegComponent));
		if (!m_LeftLegComp || !m_RightLegComp)
		{
			if (!m_bWarnedSlots)
			{
				m_bWarnedSlots = true;
				BFA_MechLog.Log(string.Format("[BFA LEG] leg pair '%1' / '%2': the leg prefab has no BFA_MechLegComponent. Inherit Prefabs/Vehicles/Mech/Core/Mech_Leg_Base.et for your leg.", m_sLeftLegSlotName, m_sRightLegSlotName), LogLevel.WARNING);
			}
			return false;
		}

		m_BodyEntity = bodyEntity;
		m_vLastRootPos = bodyEntity.GetOrigin();
		m_bHasLastRootPos = true;
		m_iLastMotionUpdateMs = -1;
		m_fPhase = 0.45;

		// Alternate pairs are half a cycle apart (trot / tripod) unless the mech maker set one.
		m_fAutoPhaseOffset = 0.5;
		if (leftLegFirst)
			m_fAutoPhaseOffset = 0;

		if (!m_LeftLegComp.InitLeg(this, bodyEntity, true))
			return false;

		if (!m_RightLegComp.InitLeg(this, bodyEntity, false))
			return false;

		m_bInitialized = true;
		return true;
	}
}
