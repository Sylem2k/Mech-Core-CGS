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

	[Attribute(desc: "DRAGGABLE TORSO PIVOT: move this PointInfo gizmo to the mech's waist rotation joint. It is in mech-root local space and drives torso, cockpit, weapon mounts, and root-mounted guns. If blank, the Legacy Pivot is used.", category: "Twist")]
	protected ref PointInfo m_TorsoPivotPoint;

	[Attribute("0 0 0", UIWidgets.Auto, "Legacy local-space pivot. Used ONLY when Torso Pivot Point has not been configured, preserving old prefabs.", category: "Twist")]
	protected vector m_vPivot;

	[Attribute("", UIWidgets.Auto, "Slot names (in the mech's SlotManager) that turn with the torso: the torso model, back pack, exhausts ... NOT the legs or hips.", category: "Parts")]
	protected ref array<string> m_aTwistSlots;

	[Attribute("", UIWidgets.Auto, "Weapon mount Ids (BFA_MechWeaponMountComponent) that turn with the torso: the arms. They still aim on top of the twist. Rigid arms: list the mount only. IK / animated arms: list the mount AND the arm's slot above.", category: "Parts")]
	protected ref array<string> m_aTwistMounts;

	[Attribute("true", UIWidgets.CheckBox, "Automatically twist all configured aim mounts (every arm/gun group). Explicit m_aTwistMounts can add more.", category: "Parts")]
	protected bool m_bAutoDiscoverWeaponMounts;

	[Attribute("true", UIWidgets.CheckBox, "Rotate all root-mounted torso, chest, shoulder and back hardpoint weapons along with the torso. Aim-mount children are handled separately.", category: "Parts")]
	protected bool m_bTwistRootMountedWeapons;

	[Attribute("true", UIWidgets.CheckBox, "Independently aim root-mounted chest/torso/shoulder/back weapon modules up, down, left and right. Each hardpoint has its own limits. Disable on special bosses until visual/hitbox validation.", category: "Parts")]
	protected bool m_bAimRootMountedWeapons;

	[Attribute("false", UIWidgets.CheckBox, "Remove the mech root's own model (its slots stay visible). Use it when the root mesh is the whole body: put the same body model in a twisting 'Torso' slot instead.", category: "Parts")]
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
	protected float m_fNextMountScan;
	protected float m_fNextRootWeaponAim;
	// All upper-body parts use the same local pivot within one tick.
	protected vector m_vEffectivePivot;

	protected ref array<IEntity> m_aSlotEntities = {};
	protected ref array<ref CGS_TwistRest> m_aSlotRest = {};
	protected ref array<ref CGS_TwistRest> m_aMountRest = {};
	protected ref array<IEntity> m_aRootWeaponEntities = {};
	protected ref array<ref CGS_TwistRest> m_aRootWeaponRest = {};

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
	//! Read the PointInfo gizmo's MODEL-LOCAL location, not its world position.
	//! This preserves rotation math when the mech rotates or moves in the world.
	//! When no PointInfo is configured, older m_vPivot values still work.
	protected vector ResolvePivotLocal(IEntity owner)
	{
		if (!m_TorsoPivotPoint || !owner)
			return m_vPivot;

		if (!m_TorsoPivotPoint.GetOwner())
			m_TorsoPivotPoint.Init(owner);

		vector pivotMat[4];
		m_TorsoPivotPoint.GetModelTransform(pivotMat);
		return pivotMat[3];
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;
		m_vEffectivePivot = ResolvePivotLocal(owner);
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
			// Root weapons still need pitch/yaw aiming while the torso is centered.
			ApplyRootWeaponAims(owner, 0);
			return;
		}

		if (!m_bCaptured)
			Capture(owner);

		// Refit-created weapons can appear while the torso is already twisted.
		// Discover them before the unchanged-yaw fast path skips this tick.
		bool newWeapons = RefreshRootWeapons(owner);
		bool newMounts = false;
		if (now >= m_fNextMountScan)
		{
			m_fNextMountScan = now + 0.25;
			newMounts = RefreshMissingMounts(owner);
		}

		// Re-apply now and then even when still: a loadout swap re-binds the arms to their
		// untwisted pose.
		if (!newWeapons && !newMounts && Math.AbsFloat(m_fTwist - m_fApplied) < 0.02 && now < m_fNextRefresh)
		{
			ApplyRootWeaponAims(owner, m_fTwist);
			return;
		}
		m_fNextRefresh = now + 0.5;
		HideRoot(owner);

		Apply(owner, m_fTwist);
		m_fApplied = m_fTwist;
		ApplyRootWeaponAims(owner, m_fTwist, true);

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

		vector local = aim.GetAimLocalPointForStation(m_sAimStation) - m_vEffectivePivot;
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
		// Preserve the ORIGINAL root weapon rest poses across captures. If a gun
		// is already aimed upwards, re-capturing its current transform would
		// progressively bake aim into rest and make the barrel drift on each scan.
		// RefreshRootWeapons handles additions, removals and swaps by entity ID.

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

		// A modeled cockpit stays separate from the exterior Torso slot but
		// follows its upper-body twist automatically. No prefab edits needed
		// to the existing explicit m_aTwistSlots array.
		if (slots && (!m_aTwistSlots || !m_aTwistSlots.Contains("Cockpit")))
		{
			EntitySlotInfo torsoSlot = slots.GetSlotByName("Torso");
			EntitySlotInfo cockpitSlot = slots.GetSlotByName("Cockpit");
			if (torsoSlot && torsoSlot.GetAttachedEntity() && cockpitSlot && cockpitSlot.GetAttachedEntity())
			{
				IEntity interior = cockpitSlot.GetAttachedEntity();
				CGS_TwistRest interiorRest = new CGS_TwistRest();
				interior.GetLocalTransform(interiorRest.m_Mat);
				m_aSlotEntities.Insert(interior);
				m_aSlotRest.Insert(interiorRest);
			}
		}

		BFA_MechWeaponMountComponent mounts = BFA_MechWeaponMountComponent.Cast(owner.FindComponent(BFA_MechWeaponMountComponent));
		if (mounts)
		{
			array<string> mountIds = {};
			if (m_bAutoDiscoverWeaponMounts)
				mounts.GetMountIds(mountIds);
			if (m_aTwistMounts)
			{
				foreach (string explicitMount : m_aTwistMounts)
				{
					if (!mountIds.Contains(explicitMount))
						mountIds.Insert(explicitMount);
				}
			}
			foreach (string mountId : mountIds)
			{
				CGS_TwistRest mountRest = new CGS_TwistRest();
				mountRest.m_sId = mountId;
				if (mounts.GetMountRestModelTransform(mountId, mountRest.m_Mat))
					m_aMountRest.Insert(mountRest);
			}
		}

		RefreshRootWeapons(owner);
		m_bCaptured = !m_aSlotRest.IsEmpty() || !m_aMountRest.IsEmpty();
		foreach (IEntity rootWeapon : m_aRootWeaponEntities)
		{
			if (rootWeapon)
				m_bCaptured = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Resolve aim mounts spawned after the first upper-body capture. Initial
	//! prefab slots can be ready before their weapon entities and late refits.
	protected bool RefreshMissingMounts(IEntity owner)
	{
		BFA_MechWeaponMountComponent mounts = BFA_MechWeaponMountComponent.Cast(owner.FindComponent(BFA_MechWeaponMountComponent));
		if (!mounts)
			return false;
		array<string> ids = {};
		if (m_bAutoDiscoverWeaponMounts)
			mounts.GetMountIds(ids);
		if (m_aTwistMounts)
		{
			foreach (string namedMount : m_aTwistMounts)
			{
				if (!ids.Contains(namedMount))
					ids.Insert(namedMount);
			}
		}
		bool changed = false;
		foreach (string mountId : ids)
		{
			bool known = false;
			foreach (CGS_TwistRest oldRest : m_aMountRest)
			{
				if (oldRest && oldRest.m_sId == mountId)
				{
					known = true;
					break;
				}
			}
			if (known)
				continue;
			CGS_TwistRest rest = new CGS_TwistRest();
			rest.m_sId = mountId;
			if (!mounts.GetMountRestModelTransform(mountId, rest.m_Mat))
				continue;
			m_aMountRest.Insert(rest);
			m_bCaptured = true;
			changed = true;
		}
		return changed;
	}

	//------------------------------------------------------------------------------------------------
		//! Root-mounted weapons are children of the mech itself, not the animated arm
	//! mounts or bones. Snapshot each one's untwisted local transform once. Newly
	//! installed weapons are captured on sight, even while the torso is turning.
	protected bool RefreshRootWeapons(IEntity owner)
	{
		if (!m_bTwistRootMountedWeapons || !owner)
			return false;
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(owner.FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints)
			return false;

		bool changed = false;
		int count = hardpoints.GetHardpointCount();
		while (m_aRootWeaponEntities.Count() < count)
		{
			m_aRootWeaponEntities.Insert(null);
			m_aRootWeaponRest.Insert(null);
		}
		for (int hp = 0; hp < count; hp++)
		{
			IEntity weapon = null;
			if (hardpoints.IsRootMountedHardpoint(hp))
				weapon = hardpoints.GetModuleEntityAt(hp);
			if (weapon == m_aRootWeaponEntities[hp])
				continue;

			m_aRootWeaponEntities[hp] = weapon;
			m_aRootWeaponRest[hp] = null;
			if (!weapon)
				continue;
			CGS_TwistRest rest = new CGS_TwistRest();
			weapon.GetLocalTransform(rest.m_Mat);
			m_aRootWeaponRest[hp] = rest;
			changed = true;
		}
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the root's own model away (slots, legs and arms stay visible - hiding the root entity
	//! itself would hide all of its children too). The death sequence puts it back for the wreck.
	protected void HideRoot(IEntity owner)
	{
		if (!m_bHideRootMesh || System.IsConsoleApp() || BFA_MechExtension.IsMechDestroyed(owner))
			return;
		if (owner.GetVObject())
			owner.SetObject(null, "");
	}


	//------------------------------------------------------------------------------------------------
	//! Aim each fitted root weapon toward its crew station's replicated camera
	//! convergence point. Torso yaw is the base transform; independent per-weapon
	//! yaw/pitch is applied on top, never to the chassis or to arm/bone mounts.
	//! Run at 30 Hz when stable, or immediately when torso yaw changes.
	protected void ApplyRootWeaponAims(IEntity owner, float torsoYaw, bool force = false)
	{
		if (!m_bTwistRootMountedWeapons || !owner)
			return;

		float now = Now();
		if (!force && now < m_fNextRootWeaponAim)
			return;
		m_fNextRootWeaponAim = now + 0.033;

		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(owner.FindComponent(BFA_MechHardpointComponent));
		BFA_MechAimComponent aim = BFA_MechAimComponent.Cast(owner.FindComponent(BFA_MechAimComponent));

		vector torsoMat[4];
		Math3D.AnglesToMatrix(Vector(torsoYaw, 0, 0), torsoMat);
		torsoMat[3] = m_vEffectivePivot - (torsoMat[0] * m_vEffectivePivot[0] + torsoMat[1] * m_vEffectivePivot[1] + torsoMat[2] * m_vEffectivePivot[2]);

		for (int hp = 0; hp < m_aRootWeaponEntities.Count(); hp++)
		{
			IEntity gun = m_aRootWeaponEntities[hp];
			CGS_TwistRest rest = m_aRootWeaponRest[hp];
			if (!gun || !rest)
				continue;

			vector base[4];
			Math3D.MatrixMultiply4(torsoMat, rest.m_Mat, base);
			float appliedYaw = 0;
			float appliedPitch = 0;
			vector finalMat[4];
			for (int i = 0; i < 4; i++)
				finalMat[i] = base[i];

			string station;
			float yawMin, yawMax, pitchMin, pitchMax;
			if (m_bAimRootMountedWeapons && hardpoints && aim && hardpoints.GetRootWeaponAimConfig(hp, station, yawMin, yawMax, pitchMin, pitchMax))
			{
				vector toAim = aim.GetAimLocalPointForStation(station) - base[3];
				if (toAim.LengthSq() > 0.000001)
				{
					toAim.Normalize();
					vector right = base[0];
					vector up = base[1];
					vector forward = base[2];
					if (right.LengthSq() > 0.000001 && up.LengthSq() > 0.000001 && forward.LengthSq() > 0.000001)
					{
						right.Normalize();
						up.Normalize();
						forward.Normalize();

						// Same rest-space convergence method as BFA_MechWeaponMountComponent.
						vector relative = Vector(vector.Dot(toAim, right), vector.Dot(toAim, up), vector.Dot(toAim, forward));
						vector lookMat[4];
						Math3D.DirectionAndUpMatrix(relative, vector.Up, lookMat);
						vector angles = Math3D.MatrixToAngles(lookMat);

						float yaw = angles[0];
						while (yaw > 180)
							yaw -= 360;
						while (yaw < -180)
							yaw += 360;
						yaw = Math.Clamp(yaw, yawMin, yawMax);
						float pitch = Math.Clamp(angles[1], pitchMin, pitchMax);
						appliedYaw = yaw;
						appliedPitch = pitch;

						vector aimMat[4];
						Math3D.AnglesToMatrix(Vector(yaw, pitch, 0), aimMat);
						aimMat[3] = vector.Zero;
						Math3D.MatrixMultiply4(base, aimMat, finalMat);
						finalMat[3] = base[3];
					}
				}
			}

			// On large MP servers, skip mesh/physics transform writes unless the
			// torso or barrel angles changed. Reapply twice a second to recover
			// safely if another gameplay system adjusted the entity.
			if (!force && now < rest.m_fNextRootReapply
				&& Math.AbsFloat(torsoYaw - rest.m_fLastRootTorsoYaw) < 0.05
				&& Math.AbsFloat(appliedYaw - rest.m_fLastRootYaw) < 0.05
				&& Math.AbsFloat(appliedPitch - rest.m_fLastRootPitch) < 0.05)
				continue;

			rest.m_fLastRootTorsoYaw = torsoYaw;
			rest.m_fLastRootYaw = appliedYaw;
			rest.m_fLastRootPitch = appliedPitch;
			rest.m_fNextRootReapply = now + 0.5;
			gun.SetLocalTransform(finalMat);
			gun.Update();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(IEntity owner, float twist)
	{
		// Rotation around the vertical axis through the pivot (mech space).
		vector rot[4];
		Math3D.AnglesToMatrix(Vector(twist, 0, 0), rot);
		rot[3] = m_vEffectivePivot - (rot[0] * m_vEffectivePivot[0] + rot[1] * m_vEffectivePivot[1] + rot[2] * m_vEffectivePivot[2]);

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
	// Used for root gun aim only; each freshly captured module starts dirty.
	float m_fLastRootYaw = 9999;
	float m_fLastRootPitch = 9999;
	float m_fLastRootTorsoYaw = 9999;
	float m_fNextRootReapply;
}
