//------------------------------------------------------------------------------------------------
//! CGS TORSO TWIST (MechWarrior style): the upper body turns toward where a crew station aims,
//! independent of the legs. The legs (and the mech root) keep walking their own way.
//!  - Twisted: the listed SLOTS (torso part, back pack, exhausts ...) and the listed weapon
//!    MOUNTS (arms). Arms keep aiming on top of the twist, with their own limits.
//!  - Not twisted: the mech root (so the root's own mesh should be the hips / legs only - put
//!    the torso model in its own slot and list that slot here).
//!  - Runs on every machine from the replicated aim, so it looks the same for everyone and
//!    the server's guns fire from the twisted arms.
[ComponentEditorProps(category: "CGS MechCore/Movement", description: "Torso twist: the upper body (listed slots + arm mounts) turns toward the aim, separate from the legs.")]
class CGS_TorsoTwistComponentClass : ScriptComponentClass
{
}

class CGS_TorsoTwistComponent : ScriptComponent
{
	[Attribute("DRIVER", UIWidgets.EditBox, "Crew station whose aim the torso follows (e.g. DRIVER, or GUNNER on a two-crew mech).", category: "Twist")]
	protected string m_sAimStation;

	[Attribute("90", UIWidgets.Auto, "Most the torso can turn left or right from the legs, in degrees.", category: "Twist")]
	protected float m_fMaxTwist;

	[Attribute("70", UIWidgets.Auto, "How fast the torso turns, in degrees per second.", category: "Twist")]
	protected float m_fTwistSpeed;

	[Attribute("3", UIWidgets.Auto, "Ignore aim changes smaller than this many degrees (stops the torso twitching).", category: "Twist")]
	protected float m_fDeadzone;

	[Attribute("true", UIWidgets.CheckBox, "Turn back to centre when nobody is in the aim station.", category: "Twist")]
	protected bool m_bCentreWhenEmpty;

	[Attribute("0 0 0", UIWidgets.Auto, "Point the torso turns around, in the mech's own space (usually the waist).", category: "Twist")]
	protected vector m_vPivot;

	[Attribute("", UIWidgets.Auto, "Slot names (in the mech's SlotManager) that turn with the torso: the torso model, back pack, exhausts ... NOT the legs or hips.", category: "Parts")]
	protected ref array<string> m_aTwistSlots;

	[Attribute("", UIWidgets.Auto, "Weapon mount Ids (BFA_MechWeaponMountComponent) that turn with the torso: the arms. They still aim on top of the twist. Rigid arms: list the mount only. IK / animated arms: list the mount AND the arm's slot above.", category: "Parts")]
	protected ref array<string> m_aTwistMounts;

	[Attribute("false", UIWidgets.CheckBox, "Hide the mech root's own mesh (its slots stay visible). Use it when the root mesh is the whole body: put the same body model in a twisting 'Torso' slot instead.", category: "Parts")]
	protected bool m_bHideRootMesh;

	[Attribute("false", UIWidgets.CheckBox, "Print the twist once a second.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime -----------------------------------------------------------------------------
	protected static ref array<CGS_TorsoTwistComponent> s_aAll = {};
	protected static int s_iHeartbeatMs;

	protected float m_fTwist;          // current twist, degrees (+ = right)
	protected float m_fApplied = 0;    // twist last written to the parts
	protected float m_fLastTime;
	protected float m_fNextDebug;
	protected bool m_bCaptured;
	protected float m_fNextCapture;
	protected float m_fNextRefresh;

	protected ref array<IEntity> m_aSlotEntities = {};
	protected ref array<ref CGS_TwistRest> m_aSlotRest = {};
	protected ref array<ref CGS_TwistRest> m_aMountRest = {};

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aAll.Contains(this))
			s_aAll.Insert(this);
		EnsureTick();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Current twist in degrees (+ = right). For HUDs / add-ons.
	float GetTwist()
	{
		return m_fTwist;
	}

	//------------------------------------------------------------------------------------------------
	static CGS_TorsoTwistComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return CGS_TorsoTwistComponent.Cast(mech.FindComponent(CGS_TorsoTwistComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTick()
	{
		if (s_iHeartbeatMs != 0 && System.GetTickCount() - s_iHeartbeatMs < 1000)
			return;
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().Remove(StaticTick);
		GetGame().GetCallqueue().CallLater(StaticTick, 0, true);
		s_iHeartbeatMs = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StaticTick()
	{
		s_iHeartbeatMs = System.GetTickCount();
		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			CGS_TorsoTwistComponent twist = s_aAll[i];
			if (!twist || !twist.GetOwner())
			{
				s_aAll.Remove(i);
				continue;
			}
			twist.Tick();
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
	protected void Tick()
	{
		IEntity owner = GetOwner();
		float now = Now();
		float dt = Math.Clamp(now - m_fLastTime, 0, 0.1);
		m_fLastTime = now;

		float wanted = WantedTwist(owner);

		if (Math.AbsFloat(wanted - m_fTwist) > 0.01)
		{
			float step = Math.Max(m_fTwistSpeed, 1) * dt;
			m_fTwist = m_fTwist + Math.Clamp(wanted - m_fTwist, -step, step);
		}

		// Untwisted: (re)read the rest poses, so loadout swaps and late binds are picked up.
		if (Math.AbsFloat(m_fTwist) < 0.01 && Math.AbsFloat(m_fApplied) < 0.01)
		{
			m_fTwist = 0;
			if (now >= m_fNextCapture)
			{
				m_fNextCapture = now + 0.5;
				Capture(owner);
			}
			return;
		}

		if (!m_bCaptured)
			Capture(owner);

		// Re-apply now and then even when still: a loadout swap re-binds the arms to their
		// untwisted pose.
		if (Math.AbsFloat(m_fTwist - m_fApplied) < 0.02 && now < m_fNextRefresh)
			return;
		m_fNextRefresh = now + 0.5;
		HideRoot(owner);

		Apply(owner, m_fTwist);
		m_fApplied = m_fTwist;

		if (m_bDebug && now >= m_fNextDebug)
		{
			m_fNextDebug = now + 1;
			Print(string.Format("[CGS TORSO] %1 twist %2 (wanted %3)", owner, Math.Round(m_fTwist), Math.Round(wanted)));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Where the aim station looks, as a yaw from the legs' forward, clamped.
	protected float WantedTwist(IEntity owner)
	{
		if (BFA_MechExtension.IsMechDestroyed(owner))
			return m_fTwist; // freeze where it died

		if (m_bCentreWhenEmpty)
		{
			BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(owner.FindComponent(BFA_MechCoreComponent));
			if (core && !core.GetStationOccupant(m_sAimStation))
				return 0;
		}

		BFA_MechAimComponent aim = BFA_MechAimComponent.Cast(owner.FindComponent(BFA_MechAimComponent));
		if (!aim)
			return 0;

		vector local = aim.GetAimLocalPointForStation(m_sAimStation) - m_vPivot;
		if (local[0] * local[0] + local[2] * local[2] < 1.0)
			return m_fTwist;

		float yaw = Math.Atan2(local[0], local[2]) * Math.RAD2DEG;
		yaw = Math.Clamp(yaw, -Math.Max(m_fMaxTwist, 0), Math.Max(m_fMaxTwist, 0));

		if (Math.AbsFloat(yaw - m_fTwist) < Math.Max(m_fDeadzone, 0))
			return m_fTwist;
		return yaw;
	}

	//------------------------------------------------------------------------------------------------
	protected void Capture(IEntity owner)
	{
		HideRoot(owner);

		m_aSlotEntities.Clear();
		m_aSlotRest.Clear();
		m_aMountRest.Clear();

		SlotManagerComponent slots = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));
		if (slots && m_aTwistSlots)
		{
			foreach (string slotName : m_aTwistSlots)
			{
				EntitySlotInfo slot = slots.GetSlotByName(slotName);
				if (!slot || !slot.GetAttachedEntity())
					continue;

				IEntity part = slot.GetAttachedEntity();
				CGS_TwistRest rest = new CGS_TwistRest();
				part.GetLocalTransform(rest.m_Mat);
				m_aSlotEntities.Insert(part);
				m_aSlotRest.Insert(rest);
			}
		}

		BFA_MechWeaponMountComponent mounts = BFA_MechWeaponMountComponent.Cast(owner.FindComponent(BFA_MechWeaponMountComponent));
		if (mounts && m_aTwistMounts)
		{
			foreach (string mountId : m_aTwistMounts)
			{
				CGS_TwistRest mountRest = new CGS_TwistRest();
				mountRest.m_sId = mountId;
				if (mounts.GetMountRestModelTransform(mountId, mountRest.m_Mat))
					m_aMountRest.Insert(mountRest);
			}
		}

		m_bCaptured = !m_aSlotRest.IsEmpty() || !m_aMountRest.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Root mesh off, children on (not recursive). The death sequence shows it again for the wreck.
	protected void HideRoot(IEntity owner)
	{
		if (!m_bHideRootMesh || System.IsConsoleApp() || BFA_MechExtension.IsMechDestroyed(owner))
			return;
		owner.ClearFlags(EntityFlags.VISIBLE, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(IEntity owner, float twist)
	{
		// Rotation around the vertical axis through the pivot (mech space).
		vector rot[4];
		Math3D.AnglesToMatrix(Vector(twist, 0, 0), rot);
		rot[3] = m_vPivot - (rot[0] * m_vPivot[0] + rot[1] * m_vPivot[1] + rot[2] * m_vPivot[2]);

		for (int i = 0; i < m_aSlotEntities.Count(); i++)
		{
			IEntity part = m_aSlotEntities[i];
			if (!part)
				continue;

			vector mat[4];
			Math3D.MatrixMultiply4(rot, m_aSlotRest[i].m_Mat, mat);
			part.SetLocalTransform(mat);
			part.Update();
		}

		BFA_MechWeaponMountComponent mounts = BFA_MechWeaponMountComponent.Cast(owner.FindComponent(BFA_MechWeaponMountComponent));
		if (!mounts)
			return;

		foreach (CGS_TwistRest mountRest : m_aMountRest)
		{
			vector mountMat[4];
			Math3D.MatrixMultiply4(rot, mountRest.m_Mat, mountMat);
			mounts.SetMountRestModelTransform(mountRest.m_sId, mountMat);
		}

		// Arms take the new rest pose right away (not only on the next aim update).
		mounts.ApplyAllCurrentStationAims();
	}
}

//------------------------------------------------------------------------------------------------
class CGS_TwistRest
{
	string m_sId;
	vector m_Mat[4];
}
