//------------------------------------------------------------------------------------------------
//! CGS HIP TWIST: independent lower chassis visual rotation for MechWarrior walkers.
//! Read the actual authoritative/predicted yaw from BFA_MechMovementComponent.
//! Do NOT capture keyboard input or simulate movement a second time.
//! On A/D, Hip and the named leg slot entities turn around the authored hip pivot;
//! the root, torso, camera, cockpit and weapons are never rotated by this component.
//! W/S movement direction and replicated yaw remain owned by the existing movement
//! component. All edits are strictly opt-in when this component is on a mech.
//
//! Called by the existing movement update BEFORE procedural foot IK.
//! The current local transforms are always computed from ORIGINAL rest poses,
//! never from a previously rotated transform (prevents cumulative leg drift).
[ComponentEditorProps(category: "CGS MechCore/Movement", description: "Hip twist: independently turn the Hip and listed leg slots while the torso/root stays forward. Requires BFA_MechMovementComponent independent hip drive.")]
class CGS_HipTwistComponentClass : ScriptComponentClass
{
}

class CGS_HipTwistComponent : ScriptComponent
{
	[Attribute("Hip", UIWidgets.EditBox, "Name of the lower-body Hip slot in the owner's SlotManager.", category: "Hip Twist")]
	protected string m_sHipSlot;

	[Attribute("", UIWidgets.Auto, "Names of leg slots that rotate with the Hip, e.g. Leg_L1 and Leg_R1. These are siblings of Hip in the current SampleMech.", category: "Hip Twist")]
	protected ref array<string> m_aLegSlots;

	[Attribute(desc: "Optional draggable hip pivot in MECH ROOT MODEL coordinates. If unset, the authored Hip slot origin is the pivot.", category: "Hip Twist")]
	protected ref PointInfo m_HipPivotPoint;

	[Attribute("false", UIWidgets.CheckBox, "Log captured Hip/Leg slots and commanded hip angles for Workbench tests.", category: "Diagnostics")]
	protected bool m_bDebug;

	protected ref array<ref CGS_HipTwistRest> m_aRest = {};
	protected bool m_bCaptured;
	protected bool m_bWarned;
	protected vector m_vPivotLocal;
	protected float m_fTwist;
	protected float m_fLastApplied = 9999;
	protected int m_iNextRefreshMs;
	protected int m_iNextDebugMs;

	//------------------------------------------------------------------------------------------------
	static CGS_HipTwistComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return CGS_HipTwistComponent.Cast(mech.FindComponent(CGS_HipTwistComponent));
	}

	float GetTwist()
	{
		return m_fTwist;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (m_bDebug)
			BFA_MechLog.Log(string.Format("[CGS HIP TWIST] component loaded on %1", owner));
	}

	//------------------------------------------------------------------------------------------------
	// Resolve all parts together: never leave only one leg captured/rotated.
	// Leg slot children may appear later during dedicated-server replication.
	protected bool CaptureRest(IEntity owner)
	{
		if (m_bCaptured)
			return true;
		if (!owner)
			return false;

		SlotManagerComponent slots = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));
		if (!slots)
			return false;

		EntitySlotInfo hipSlot = slots.GetSlotByName(m_sHipSlot);
		if (!hipSlot || !hipSlot.GetAttachedEntity())
		{
			WarnMissing("Hip");
			return false;
		}

		IEntity hip = hipSlot.GetAttachedEntity();
		ref array<ref CGS_HipTwistRest> candidates = {};

		CGS_HipTwistRest hipRest = new CGS_HipTwistRest();
		hipRest.m_pPart = hip;
		hip.GetLocalTransform(hipRest.m_Rest);
		candidates.Insert(hipRest);

		if (m_aLegSlots)
		{
			foreach (string name : m_aLegSlots)
			{
				if (name.IsEmpty())
					continue;
				EntitySlotInfo legSlot = slots.GetSlotByName(name);
				if (!legSlot || !legSlot.GetAttachedEntity())
				{
					WarnMissing(name);
					return false;
				}
				IEntity leg = legSlot.GetAttachedEntity();
				CGS_HipTwistRest legRest = new CGS_HipTwistRest();
				legRest.m_pPart = leg;
				leg.GetLocalTransform(legRest.m_Rest);
				candidates.Insert(legRest);
			}
		}

		// The model-local Hip origin is the default waist pivot, consistent
		// with how the existing CGS_TorsoTwistComponent handles its slots.
		m_vPivotLocal = hipRest.m_Rest[3];
		if (m_HipPivotPoint)
		{
			if (!m_HipPivotPoint.GetOwner())
				m_HipPivotPoint.Init(owner);
			vector pivotMat[4];
			m_HipPivotPoint.GetModelTransform(pivotMat);
			m_vPivotLocal = pivotMat[3];
		}

		m_aRest = candidates;
		m_bCaptured = true;
		m_bWarned = false;
		if (m_bDebug)
			BFA_MechLog.Log(string.Format("[CGS HIP TWIST] captured %1 slot entities, pivot %2", m_aRest.Count(), m_vPivotLocal));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void WarnMissing(string slotName)
	{
		if (m_bWarned)
			return;
		m_bWarned = true;
		BFA_MechLog.Log(string.Format("[CGS HIP TWIST] waiting for slot entity: %1", slotName), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Called on server, local predicted pilot and proxy by BFA_MechMovementComponent.
	//! This changes LOCAL slot pose only. It never changes root physics velocity.
	void ApplyHipYaw(float yawDegrees)
	{
		IEntity owner = GetOwner();
		if (!owner || !CaptureRest(owner))
			return;

		// If a leg is replaced/destroyed, never write into a deleted entity.
		// A complete refit will recapture from the NEW neutral slot transforms.
		foreach (CGS_HipTwistRest oldRest : m_aRest)
		{
			if (!oldRest || !oldRest.m_pPart || oldRest.m_pPart.IsDeleted())
			{
				m_bCaptured = false;
				m_aRest.Clear();
				m_fLastApplied = 9999;
				return;
			}
		}

		m_fTwist = yawDegrees;
		BaseWorld world = owner.GetWorld();
		if (!world)
			return;
		int nowMs = world.GetWorldTime();
		// Render changed angles promptly. Reassert the pose periodically if a
		// physics or attachment system touched one of the child transforms.
		if (Math.AbsFloat(yawDegrees - m_fLastApplied) < 0.02 && nowMs < m_iNextRefreshMs)
			return;

		m_fLastApplied = yawDegrees;
		m_iNextRefreshMs = nowMs + 250;

		vector rot[4];
		Math3D.AnglesToMatrix(Vector(yawDegrees, 0, 0), rot);
		rot[3] = m_vPivotLocal - (
			rot[0] * m_vPivotLocal[0] +
			rot[1] * m_vPivotLocal[1] +
			rot[2] * m_vPivotLocal[2]);

		foreach (CGS_HipTwistRest rest : m_aRest)
		{
			vector posed[4];
			Math3D.MatrixMultiply4(rot, rest.m_Rest, posed);
			rest.m_pPart.SetLocalTransform(posed);
			rest.m_pPart.Update();
		}

		if (m_bDebug && nowMs >= m_iNextDebugMs)
		{
			m_iNextDebugMs = nowMs + 1000;
			BFA_MechLog.Log(string.Format("[CGS HIP TWIST] commanded=%1 hipLocalYaw=%2 slots=%3",
				Math.Round(yawDegrees), Math.Round(m_aRest[0].m_pPart.GetLocalYawPitchRoll()[0]), m_aRest.Count()));
		}
	}
}

// Stable original, unrotated root-local slot pose.
class CGS_HipTwistRest
{
	IEntity m_pPart;
	vector m_Rest[4];
}
