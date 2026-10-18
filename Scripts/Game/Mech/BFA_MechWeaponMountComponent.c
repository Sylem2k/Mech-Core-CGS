//! How a mount turns the solved aim into motion. Every mech can be built differently.
enum BFA_EMechMountAimDriver
{
	ENTITY_ROTATION = 0, // rigid gun/pod: rotate the slot entity itself (SampleMech2 today)
	ANIM_ANGLES = 1,     // animated arm: write solved yaw/pitch (degrees) into anim-graph float variables
	ANIM_IK_TARGET = 2   // animated arm with an IK chain: write a target point (entity-local X/Y/Z) for the hand
}

[BaseContainerProps()]
class BFA_MechWeaponMountDefinition
{
	// Runtime: the nested slot path warning was printed already.
	bool m_bWarnedSlotPath;

	[Attribute("MOUNT", UIWidgets.EditBox, "Stable mount ID used by weapons and future hardpoints.", category: "Mount")]
	string m_sMountId;

	[Attribute("", UIWidgets.EditBox, "Root SlotManager slot containing the movable weapon/arm entity. For a weapon in a part's own slot use Part/Slot, e.g. Arm_L1/Titan_Rocket_Pod.", category: "Mount")]
	string m_sSlotName;

	[Attribute("DRIVER", UIWidgets.EditBox, "Crew station whose aim direction controls this mount.", category: "Mount")]
	string m_sControlStation;

	[Attribute("1", UIWidgets.Auto, "Weapon group associated with this mount.", category: "Mount")]
	int m_iWeaponGroup;

	[Attribute("-70", UIWidgets.Auto, "Minimum yaw offset in degrees.", category: "Traverse")]
	float m_fYawMin;

	[Attribute("70", UIWidgets.Auto, "Maximum yaw offset in degrees.", category: "Traverse")]
	float m_fYawMax;

	[Attribute("-45", UIWidgets.Auto, "Minimum pitch offset in degrees.", category: "Traverse")]
	float m_fPitchMin;

	[Attribute("45", UIWidgets.Auto, "Maximum pitch offset in degrees.", category: "Traverse")]
	float m_fPitchMax;

	[Attribute("false", UIWidgets.CheckBox, "Invert pitch for meshes whose local pitch convention is opposite.", category: "Traverse")]
	bool m_bInvertPitch;

	[Attribute("1000", UIWidgets.Auto, "Distance used when converging this mount on a station aim direction.", category: "Traverse")]
	float m_fConvergenceDistance;

	[Attribute("0", UIWidgets.ComboBox, "How this mount moves. ENTITY_ROTATION = rotate the slot entity (rigid gun). ANIM_ANGLES = set yaw/pitch variables on the entity's anim graph (animated arm). ANIM_IK_TARGET = set an IK target X/Y/Z on the anim graph (arm with IK chain).", category: "Aim Driver", enums: ParamEnumArray.FromEnum(BFA_EMechMountAimDriver))]
	int m_iAimDriver;

	[Attribute("AimYaw", UIWidgets.EditBox, "ANIM_ANGLES: float variable receiving yaw in degrees (already clamped to the traverse limits).", category: "Aim Driver")]
	string m_sAnimYawVariable;

	[Attribute("AimPitch", UIWidgets.EditBox, "ANIM_ANGLES: float variable receiving pitch in degrees.", category: "Aim Driver")]
	string m_sAnimPitchVariable;

	[Attribute("TargetX", UIWidgets.EditBox, "ANIM_IK_TARGET: X variable (entity-local metres), same convention as the leg IK graph.", category: "Aim Driver")]
	string m_sIkTargetXVariable;

	[Attribute("TargetY", UIWidgets.EditBox, "ANIM_IK_TARGET: Y variable.", category: "Aim Driver")]
	string m_sIkTargetYVariable;

	[Attribute("TargetZ", UIWidgets.EditBox, "ANIM_IK_TARGET: Z variable.", category: "Aim Driver")]
	string m_sIkTargetZVariable;

	[Attribute("3.0", UIWidgets.Auto, "ANIM_IK_TARGET: how far from the shoulder pivot the hand target is placed along the aim line, in metres. Keep inside the arm's reach.", category: "Aim Driver")]
	float m_fIkReach;

	[Attribute("", UIWidgets.EditBox, "ANIM_IK_TARGET: bone the arm swings from (e.g. J1). The aim line starts at this bone instead of the arm's origin - set it when the shoulder is not at the model origin. Empty = arm origin.", category: "Aim Driver")]
	string m_sIkPivotBone;

	[Attribute("false", UIWidgets.CheckBox, "ANIM_IK_TARGET: the IK graph moves the hand target FROM the hand's rest position (TargetX/Y/Z = offset), not to an absolute point. Turn on when the arm stretches straight past the target.", category: "Aim Driver")]
	bool m_bIkTargetFromRest;

	[Attribute("J3", UIWidgets.EditBox, "ANIM_IK_TARGET: the hand bone at the end of the IK chain (used with Target From Rest).", category: "Aim Driver")]
	string m_sIkEndBone;

	[Attribute("false", UIWidgets.CheckBox, "ANIM_IK_TARGET: SWING FROM REST. Aiming straight ahead keeps the arm in its modelled rest pose; aiming elsewhere swings the hand from there, as if the arm turned at the shoulder. Use it when the arm folds up or reaches oddly because its rest pose is not pointing straight out from the shoulder. Needs Target From Rest.", category: "Aim Driver")]
	bool m_bIkSwingFromRest;

	[Attribute("0", UIWidgets.Auto, "ANIM_IK_TARGET + Swing From Rest: how far the hand swings, in metres, for each radian of aim (about the shoulder-to-hand distance). 0 = use Ik Reach.", category: "Aim Driver")]
	float m_fIkSwingRadius;

	[Attribute("0", UIWidgets.Auto, "ANIM_IK_TARGET: seconds for the hand to glide most of the way to a new target (0 = instant). 0.3-0.6 stops arms snapping when the target changes.", category: "Aim Driver")]
	float m_fIkSmoothTime;

	[Attribute("", UIWidgets.EditBox, "ANIM_IK_TARGET: HAND AIM. Float variable in the arm's anim graph that turns the hand bone left/right (degrees). The IK only moves the hand, it never turns it, so a weapon on the hand stays pointing straight ahead and the wrist looks twisted. Add the variable + a bone rotation node on the hand in the graph, then name it here. Empty = off.", category: "Aim Driver")]
	string m_sIkHandYawVariable;

	[Attribute("", UIWidgets.EditBox, "ANIM_IK_TARGET: HAND AIM. Float variable that tilts the hand bone up/down (degrees). Empty = off.", category: "Aim Driver")]
	string m_sIkHandPitchVariable;

	[Attribute("false", UIWidgets.CheckBox, "ANIM_IK_TARGET: flip the hand pitch sign (turn on if the hand tilts up when it should tilt down).", category: "Aim Driver")]
	bool m_bIkHandInvertPitch;

	[Attribute("80", UIWidgets.Auto, "ANIM_IK_TARGET: most the hand may turn from its rest direction, in degrees.", category: "Aim Driver")]
	float m_fIkHandMaxAngle;

	[Attribute("", UIWidgets.EditBox, "FOLLOW ARM: Mount Id of an IK arm (e.g. ARM_L). This weapon rides on that arm: it moves with the arm and still aims on top of it. Empty = fixed to the body.", category: "Follow Arm")]
	string m_sFollowArmMount;

	[Attribute("", UIWidgets.EditBox, "Arm bone it rides on: J1 = upper arm, J2 = forearm, J3 = hand. Empty = the bone closest to the weapon (recommended).", category: "Follow Arm")]
	string m_sFollowBone;

	IEntity m_pEntity;
	IEntity m_pParent;
	float m_fDiagNextTime;

	// Runtime anim-graph binding for ANIM_* drivers.
	AnimationControllerComponent m_pAnimController;
	int m_iAnimVarA = -1;
	int m_iAnimVarB = -1;
	int m_iAnimVarC = -1;
	bool m_bAnimBound;
	bool m_bAnimBindFailed;
	bool m_bIkPivotFromBone;
	vector m_vIkPivotModel;            // pivot bone position in the arm's model space (bind pose)
	vector m_vIkEndRest;               // hand bone rest position in the arm's model space
	bool m_bIkEndRestKnown;
	vector m_vIkSmoothed;              // smoothed target sent to the IK graph
	bool m_bSwingActive;               // a melee swing is driving this mount
	float m_fSwingYaw;                 // degrees added on top of the aim while swinging
	float m_fSwingPitch;
	float m_fSwingPush;                // metres the mount is pushed along its aim (punch / thrust)
	int m_iHandYawVar = -1;
	int m_iHandPitchVar = -1;
	bool m_bHandBindTried;
	bool m_bHandRestKnown;
	vector m_vHandRestRight;           // hand bone rest axes in the arm's model space
	vector m_vHandRestUp;
	vector m_vHandRestFwd;
	bool m_bIkSmoothInit;
	float m_fIkSmoothLastTime;

	// Follow-arm runtime.
	bool m_bFollowReady;
	bool m_bFollowFailed;
	int m_iFollowBone = -1;
	IEntity m_pFollowArm;
	vector m_mFollowBoneRest[4];       // bone in mech model space when the rest pose was captured
	float m_fAimYaw;
	float m_fAimPitch;
	vector m_vLastAimPoint;            // world point this mount was last told to aim at
	bool m_bHasAimPoint;
	vector m_mBaseLocalTransform[4];
	vector m_mSlotLocalTransform[4];   // slot pose as placed in the SlotManager (before hardpoint offset)
	bool m_bResolved;
}

[ComponentEditorProps(category: "BFA MechCore", description: "Generic multi-mount aimer. Local mounts converge on the exact camera point; remote mounts use replicated station aim direction.")]
class BFA_MechWeaponMountComponentClass : ScriptGameComponentClass
{
}

class BFA_MechWeaponMountComponent : ScriptGameComponent
{
	[Attribute("", UIWidgets.Object, "Movable weapon/arm/shoulder mounts on this mech.", category: "Weapon Mounts")]
	ref array<ref BFA_MechWeaponMountDefinition> m_aMounts;

	[Attribute("false", UIWidgets.CheckBox, "Print aim solver diagnostics once per second per mount while a local station is aiming.", category: "Diagnostics")]
	protected bool m_bDebugAim;

	static const string DRIVER_STATION = "DRIVER";

	protected ref map<string, float> m_mTraceNext = new map<string, float>();

	//------------------------------------------------------------------------------------------------
	//! Debug Aim: print a message at most once every 2 s per key.
	protected void Trace(string key, string message)
	{
		if (!m_bDebugAim)
			return;
		float now = 0;
		if (GetOwner() && GetOwner().GetWorld())
			now = GetOwner().GetWorld().GetWorldTime() * 0.001;
		float next;
		if (m_mTraceNext.Find(key, next) && now < next)
			return;
		m_mTraceNext.Set(key, now + 2.0);
		Print("[BFA AIM TRACE] " + message);
	}

	protected SlotManagerComponent m_pSlotManager;
	protected int m_iResolveAttempts;
	protected bool m_bHasFollowers;

	// One shared follow ticker for every mech (removing a method from the call queue can cancel
	// it on OTHER instances, so instances never add/remove their own).
	protected static ref array<BFA_MechWeaponMountComponent> s_aFollowComps = {};
	protected static int s_iFollowHeartbeatMs;

	//------------------------------------------------------------------------------------------------
	protected float WrapDegrees(float value)
	{
		while (value > 180.0)
			value = value - 360.0;

		while (value < -180.0)
			value = value + 360.0;

		return value;
	}

	//------------------------------------------------------------------------------------------------
	protected BFA_MechWeaponMountDefinition FindMount(string mountId)
	{
		if (!m_aMounts || mountId.IsEmpty())
			return null;

		foreach (BFA_MechWeaponMountDefinition mount : m_aMounts)
		{
			if (mount && mount.m_sMountId == mountId)
				return mount;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ResolveMount(BFA_MechWeaponMountDefinition mount)
	{
		if (!mount)
			return false;

		if (mount.m_bResolved && mount.m_pEntity && mount.m_pParent)
			return true;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (!m_pSlotManager)
			m_pSlotManager = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));

		if (!m_pSlotManager || mount.m_sSlotName.IsEmpty())
			return false;

		// "Arm_L1/Titan_Rocket_Pod" = a slot on a part that sits in one of the mech's slots
		// (e.g. a pod in the left arm's own SlotManager, riding on its hand bone).
		array<string> path = {};
		mount.m_sSlotName.Split("/", path, true);
		if (path.IsEmpty())
			return false;

		EntitySlotInfo slot = m_pSlotManager.GetSlotByName(path[0]);
		for (int p = 1; slot && p < path.Count(); p++)
		{
			IEntity holder = slot.GetAttachedEntity();
			if (!holder)
				return false;

			SlotManagerComponent holderSlots = SlotManagerComponent.Cast(holder.FindComponent(SlotManagerComponent));
			if (!holderSlots)
			{
				if (!mount.m_bWarnedSlotPath)
				{
					mount.m_bWarnedSlotPath = true; // once, not every frame
					BFA_MechLog.Log(string.Format("[BFA MOUNT] %1: '%2' (%3) has no SlotManager for slot '%4'", mount.m_sMountId, path[p - 1], holder, path[p]), LogLevel.WARNING);
				}
				return false;
			}
			slot = holderSlots.GetSlotByName(path[p]);
		}

		if (!slot)
			return false;

		mount.m_pEntity = slot.GetAttachedEntity();
		if (!mount.m_pEntity)
			return false;

		mount.m_pParent = mount.m_pEntity.GetParent();
		if (!mount.m_pParent)
			return false;

		// Save the original transform as the permanent REST pose.
		mount.m_pEntity.GetLocalTransform(mount.m_mSlotLocalTransform);
		for (int i = 0; i < 4; i++)
			mount.m_mBaseLocalTransform[i] = mount.m_mSlotLocalTransform[i];
		mount.m_bResolved = true;

		BFA_MechLog.Log(string.Format("[BFA MOUNT] id=%1 slot=%2", mount.m_sMountId, mount.m_sSlotName));
		BFA_MechLog.Log(string.Format("[BFA MOUNT] station=%1 group=%2", mount.m_sControlStation, mount.m_iWeaponGroup));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveAllDelayed()
	{
		bool allResolved = true;

		if (m_aMounts)
		{
			foreach (BFA_MechWeaponMountDefinition mount : m_aMounts)
			{
				if (mount && !ResolveMount(mount))
					allResolved = false;
			}
		}

		if (allResolved)
		{
			StartFollowing();
			ApplyAllCurrentStationAims();
			return;
		}

		m_iResolveAttempts++;

		if (m_iResolveAttempts >= 50)
		{
			BFA_MechLog.Log("[BFA MOUNT] One or more mounts failed to resolve.", LogLevel.WARNING);
			StartFollowing(); // the mounts that did resolve still follow their arms
			return;
		}

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.CallLater(ResolveAllDelayed, 100, false);
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetMountEntity(string mountId)
	{
		BFA_MechWeaponMountDefinition mount = FindMount(mountId);

		if (!mount)
			return null;

		if (!ResolveMount(mount))
			return null;

		return mount.m_pEntity;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves/rotates a mount's REST pose relative to where its slot placed it (hardpoint
	//! m_vOffset / m_vAngles on bind-existing hardpoints). Aim is applied on top of this pose.
	//! Calling it again replaces the previous offset (never accumulates).
	void SetMountRestOffset(string mountId, vector offset, vector angles)
	{
		BFA_MechWeaponMountDefinition mount = FindMount(mountId);
		if (!mount || !ResolveMount(mount))
			return;

		vector offsetMat[4];
		Math3D.AnglesToMatrix(angles, offsetMat);
		offsetMat[3] = offset;

		Math3D.MatrixMultiply4(mount.m_mSlotLocalTransform, offsetMat, mount.m_mBaseLocalTransform);

		if (mount.m_iAimDriver == BFA_EMechMountAimDriver.ENTITY_ROTATION && mount.m_pEntity)
		{
			mount.m_pEntity.SetLocalTransform(mount.m_mBaseLocalTransform);
			mount.m_pEntity.Update();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sets a mount's REST pose from a transform in mech model space (hardpoint Point).
	void SetMountRestModelTransform(string mountId, vector modelMat[4])
	{
		BFA_MechWeaponMountDefinition mount = FindMount(mountId);
		if (!mount || !ResolveMount(mount))
			return;

		// Slot children hang off the mech root, so model space == the mount's local space.
		// Deeper parents: convert through the parent's model transform.
		vector parentModel[4];
		Math3D.MatrixIdentity4(parentModel);
		if (mount.m_pParent && mount.m_pParent != GetOwner())
		{
			vector parentWorld[4];
			vector ownerWorld[4];
			mount.m_pParent.GetWorldTransform(parentWorld);
			GetOwner().GetWorldTransform(ownerWorld);
			RelativeTransform(ownerWorld, parentWorld, parentModel);
		}

		RelativeTransform(parentModel, modelMat, mount.m_mBaseLocalTransform);

		// Keep the weapon's own scale (e.g. a part saved with scale 2) - the Point only moves
		// and rotates it.
		float slotScale = mount.m_mSlotLocalTransform[0].Length();
		float newScale = mount.m_mBaseLocalTransform[0].Length();
		if (slotScale > 0.0001 && newScale > 0.0001)
		{
			float k = slotScale / newScale;
			for (int s = 0; s < 3; s++)
				mount.m_mBaseLocalTransform[s] = mount.m_mBaseLocalTransform[s] * k;
		}

		if (mount.m_iAimDriver == BFA_EMechMountAimDriver.ENTITY_ROTATION && mount.m_pEntity)
		{
			mount.m_pEntity.SetLocalTransform(mount.m_mBaseLocalTransform);
			mount.m_pEntity.Update();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A mount's REST pose in mech model space (not its current aimed pose).
	bool GetMountRestModelTransform(string mountId, out vector modelMat[4])
	{
		BFA_MechWeaponMountDefinition mount = FindMount(mountId);
		if (!mount || !ResolveMount(mount))
			return false;

		if (!mount.m_pParent || mount.m_pParent == GetOwner())
		{
			for (int i = 0; i < 4; i++)
				modelMat[i] = mount.m_mBaseLocalTransform[i];
			return true;
		}

		vector parentWorld[4];
		vector ownerWorld[4];
		vector parentModel[4];
		mount.m_pParent.GetWorldTransform(parentWorld);
		GetOwner().GetWorldTransform(ownerWorld);
		RelativeTransform(ownerWorld, parentWorld, parentModel);
		Math3D.MatrixMultiply4(parentModel, mount.m_mBaseLocalTransform, modelMat);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! child expressed in parent's space (res = inverse(parent) * child). Handles uniform scale.
	static void RelativeTransform(vector parent[4], vector child[4], out vector res[4])
	{
		vector delta = child[3] - parent[3];
		vector axes[3];
		for (int a = 0; a < 3; a++)
		{
			axes[a] = parent[a];
			float lenSq = axes[a].LengthSq();
			if (lenSq > 0.000001)
				axes[a] = axes[a] * (1.0 / lenSq);
		}

		for (int i = 0; i < 3; i++)
			res[i] = Vector(vector.Dot(child[i], axes[0]), vector.Dot(child[i], axes[1]), vector.Dot(child[i], axes[2]));

		res[3] = Vector(vector.Dot(delta, axes[0]), vector.Dot(delta, axes[1]), vector.Dot(delta, axes[2]));
	}

	//------------------------------------------------------------------------------------------------
	string GetMountStation(string mountId)
	{
		BFA_MechWeaponMountDefinition mount = FindMount(mountId);

		if (!mount)
			return string.Empty;

		return mount.m_sControlStation;
	}

	//------------------------------------------------------------------------------------------------
	int GetMountWeaponGroup(string mountId)
	{
		BFA_MechWeaponMountDefinition mount = FindMount(mountId);

		if (!mount)
			return 0;

		return mount.m_iWeaponGroup;
	}

	//------------------------------------------------------------------------------------------------
	protected vector LocalAimToWorld(vector localDirection)
	{
		IEntity owner = GetOwner();

		if (!owner)
			return vector.Forward;

		vector right = owner.GetTransformAxis(0);
		vector up = owner.GetTransformAxis(1);
		vector forward = owner.GetTransformAxis(2);

		if (right.LengthSq() > 0.000001)
			right.Normalize();

		if (up.LengthSq() > 0.000001)
			up.Normalize();

		if (forward.LengthSq() > 0.000001)
			forward.Normalize();

		vector worldDirection = right * localDirection[0] + up * localDirection[1] + forward * localDirection[2];

		if (worldDirection.LengthSq() < 0.000001)
			worldDirection = forward;

		if (worldDirection.LengthSq() < 0.000001)
			worldDirection = vector.Forward;

		worldDirection.Normalize();

		return worldDirection;
	}

	//------------------------------------------------------------------------------------------------
	// Exact local-camera aim path.
	// aimPointWorld is the point underneath the player's crosshair.
	protected void ApplyMountAimPoint(BFA_MechWeaponMountDefinition mount, vector aimPointWorld)
	{
		if (!ResolveMount(mount))
		{
			Trace("res" + mount.m_sMountId, string.Format("mount %1: slot '%2' not resolved (no entity in that slot)", mount.m_sMountId, mount.m_sSlotName));
			return;
		}

		if (!mount.m_pParent)
		{
			Trace("par" + mount.m_sMountId, string.Format("mount %1: slot entity has no parent", mount.m_sMountId));
			return;
		}

		// Rest pose right now (moves with the arm when this weapon rides on one).
		vector base[4];
		GetLiveBase(mount, base);

		// Convert the camera target into the mount parent's coordinate system.
		vector aimPointParent = mount.m_pParent.CoordToLocal(aimPointWorld);

		// Direction from the mount's original pivot position to the target.
		vector targetDirectionParent = aimPointParent - base[3];

		if (targetDirectionParent.LengthSq() < 0.000001)
			return;

		targetDirectionParent.Normalize();

		// Original REST orientation of the weapon.
		vector baseRight = base[0];
		vector baseUp = base[1];
		vector baseForward = base[2];

		if (baseRight.LengthSq() < 0.000001)
			return;

		if (baseUp.LengthSq() < 0.000001)
			return;

		if (baseForward.LengthSq() < 0.000001)
			return;

		baseRight.Normalize();
		baseUp.Normalize();
		baseForward.Normalize();

		// Convert the target direction into the gun's own REST coordinate frame.
		vector relativeDirection = Vector(
			vector.Dot(targetDirectionParent, baseRight),
			vector.Dot(targetDirectionParent, baseUp),
			vector.Dot(targetDirectionParent, baseForward)
		);

		if (relativeDirection.LengthSq() < 0.000001)
			return;

		relativeDirection.Normalize();

		// Convert the direction into yaw/pitch relative to the weapon's REST orientation.
		vector directionMat[4];
		Math3D.DirectionAndUpMatrix(relativeDirection, vector.Up, directionMat);

		vector deltaAngles = Math3D.MatrixToAngles(directionMat);

		float yaw = WrapDegrees(deltaAngles[0]);
		float pitch = deltaAngles[1];

		if (mount.m_bInvertPitch)
			pitch = -pitch;

		float rawYaw = yaw;
		float rawPitch = pitch;

		yaw = Math.Clamp(yaw, mount.m_fYawMin, mount.m_fYawMax);
		pitch = Math.Clamp(pitch, mount.m_fPitchMin, mount.m_fPitchMax);

		if (m_bDebugAim)
		{
			float now = 0;
			if (GetOwner() && GetOwner().GetWorld())
				now = GetOwner().GetWorld().GetWorldTime() * 0.001;

			if (now >= mount.m_fDiagNextTime)
			{
				mount.m_fDiagNextTime = now + 1.0;

				vector gunForward = mount.m_pEntity.GetTransformAxis(2);
				vector toTarget = aimPointWorld - mount.m_pEntity.GetOrigin();
				if (toTarget.LengthSq() > 0.000001)
					toTarget.Normalize();

				float errDeg = Math.Acos(Math.Clamp(vector.Dot(gunForward.Normalized(), toTarget), -1.0, 1.0)) * Math.RAD2DEG;

				BFA_MechLog.Log(string.Format(
					"[BFA AIM DIAG] mount=%1 raw yaw=%2 pitch=%3 -> clamped yaw=%4 pitch=%5 | gunFwd=%6 toTarget=%7 offByDeg=%8 parent=%9",
					mount.m_sMountId, rawYaw, rawPitch, yaw, pitch, gunForward, toTarget, errDeg, mount.m_pParent
				));
			}
		}

		mount.m_vLastAimPoint = aimPointWorld;
		mount.m_bHasAimPoint = true;
		mount.m_fAimYaw = yaw;
		mount.m_fAimPitch = pitch;
		ApplyMountPose(mount, base);
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the mount's pose from its stored aim (+ melee swing offset) and applies it.
	protected void ApplyMountPose(BFA_MechWeaponMountDefinition mount, vector base[4])
	{
		float yaw = mount.m_fAimYaw;
		float pitch = mount.m_fAimPitch;
		if (mount.m_bSwingActive)
		{
			yaw = Math.Clamp(yaw + mount.m_fSwingYaw, -170, 170);
			pitch = Math.Clamp(pitch + mount.m_fSwingPitch, -85, 85);
		}

		// Build the local aim rotation.
		vector deltaMat[4];
		Math3D.AnglesToMatrix(Vector(yaw, pitch, 0), deltaMat);
		deltaMat[3] = vector.Zero;

		// Apply aim rotation on top of the original mount transform.
		vector finalMat[4];
		Math3D.MatrixMultiply4(base, deltaMat, finalMat);

		// Never allow aiming to move the physical mount position (a melee punch may push it).
		finalMat[3] = base[3];
		if (mount.m_bSwingActive && mount.m_fSwingPush != 0)
			finalMat[3] = base[3] + finalMat[2].Normalized() * mount.m_fSwingPush;

		ApplyAimDriver(mount, yaw, pitch, finalMat);
	}

	//------------------------------------------------------------------------------------------------
	//! MELEE: swing a mount by an offset (degrees) on top of where it aims. Call every frame while
	//! swinging on every machine that shows it; active = false puts the mount back on its aim.
	void SetMountSwing(string mountId, bool active, float yawOffset, float pitchOffset, float push = 0)
	{
		BFA_MechWeaponMountDefinition mount = FindMount(mountId);
		if (!mount || !ResolveMount(mount) || !mount.m_pParent)
			return;

		mount.m_bSwingActive = active;
		mount.m_fSwingYaw = yawOffset;
		mount.m_fSwingPitch = pitchOffset;
		mount.m_fSwingPush = push;
		if (!active)
			mount.m_fSwingPush = 0;

		vector base[4];
		GetLiveBase(mount, base);
		ApplyMountPose(mount, base);
	}

	//------------------------------------------------------------------------------------------------
	//! MELEE: where a mount swings from (its entity origin) and the point it currently aims at.
	bool GetMountAim(string mountId, out vector originWorld, out vector aimPointWorld)
	{
		BFA_MechWeaponMountDefinition mount = FindMount(mountId);
		if (!mount || !ResolveMount(mount) || !mount.m_pEntity)
			return false;

		originWorld = mount.m_pEntity.GetOrigin();
		if (mount.m_bIkPivotFromBone)
			originWorld = mount.m_pEntity.CoordToParent(mount.m_vIkPivotModel);

		if (mount.m_bHasAimPoint)
			aimPointWorld = mount.m_vLastAimPoint;
		else
			aimPointWorld = originWorld + mount.m_pEntity.GetTransformAxis(2) * 10;
		return true;
	}

	//==================================================================================================
	// FOLLOW ARM - weapons that ride on an IK arm
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! The mount's rest pose in its parent's space right now. For a weapon riding on an arm this is
	//! the rest pose carried along by the arm bone; otherwise the fixed rest pose.
	protected void GetLiveBase(BFA_MechWeaponMountDefinition mount, out vector base[4])
	{
		for (int i = 0; i < 4; i++)
			base[i] = mount.m_mBaseLocalTransform[i];

		if (mount.m_sFollowArmMount.IsEmpty() || !PrepareFollow(mount))
			return;

		vector boneNow[4];
		if (!GetArmBoneModel(mount, boneNow))
			return;

		// Rest pose expressed relative to the bone at capture time, carried by the bone now.
		vector relative[4];
		RelativeTransform(mount.m_mFollowBoneRest, mount.m_mBaseLocalTransform, relative);
		Math3D.MatrixMultiply4(boneNow, relative, base);
	}

	//------------------------------------------------------------------------------------------------
	//! Arm bone transform in the mech's model space (arm slot transform * bone model matrix).
	protected bool GetArmBoneModel(BFA_MechWeaponMountDefinition mount, out vector boneMech[4])
	{
		if (!mount.m_pFollowArm || mount.m_iFollowBone == -1)
			return false;

		Animation anim = mount.m_pFollowArm.GetAnimation();
		if (!anim)
			return false;

		vector boneArm[4];
		if (!anim.GetBoneMatrix(mount.m_iFollowBone, boneArm))
			return false;

		vector armLocal[4];
		mount.m_pFollowArm.GetLocalTransform(armLocal);
		Math3D.MatrixMultiply4(armLocal, boneArm, boneMech);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the arm and bone, and captures the bone's rest pose once (before the arm is driven).
	protected bool PrepareFollow(BFA_MechWeaponMountDefinition mount)
	{
		if (mount.m_bFollowReady)
			return true;
		if (mount.m_bFollowFailed)
			return false;

		BFA_MechWeaponMountDefinition arm = FindMount(mount.m_sFollowArmMount);
		if (!arm || arm == mount)
		{
			mount.m_bFollowFailed = true;
			BFA_MechLog.Log(string.Format("[BFA FOLLOW] %1: Follow Arm Mount '%2' not found", mount.m_sMountId, mount.m_sFollowArmMount), LogLevel.WARNING);
			return false;
		}

		if (!ResolveMount(arm))
			return false; // arm not spawned yet - try again later

		Animation anim = arm.m_pEntity.GetAnimation();
		if (!anim)
		{
			mount.m_bFollowFailed = true;
			BFA_MechLog.Log(string.Format("[BFA FOLLOW] %1: arm '%2' has no skeleton", mount.m_sMountId, mount.m_sFollowArmMount), LogLevel.WARNING);
			return false;
		}

		mount.m_pFollowArm = arm.m_pEntity;

		string boneName = mount.m_sFollowBone;
		if (boneName.IsEmpty())
			boneName = PickClosestBone(mount, anim);

		mount.m_iFollowBone = anim.GetBoneIndex(boneName);
		if (mount.m_iFollowBone == -1)
		{
			mount.m_bFollowFailed = true;
			BFA_MechLog.Log(string.Format("[BFA FOLLOW] %1: bone '%2' not found on arm '%3'", mount.m_sMountId, boneName, mount.m_sFollowArmMount), LogLevel.WARNING);
			return false;
		}

		if (!GetArmBoneModel(mount, mount.m_mFollowBoneRest))
			return false;

		if (arm.m_bAnimBound)
			BFA_MechLog.Log(string.Format("[BFA FOLLOW] %1: arm '%2' was already moving when the rest pose was captured - the weapon may sit offset", mount.m_sMountId, mount.m_sFollowArmMount), LogLevel.WARNING);

		mount.m_bFollowReady = true;
		BFA_MechLog.Log(string.Format("[BFA FOLLOW] %1 rides on %2 bone %3", mount.m_sMountId, mount.m_sFollowArmMount, boneName));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! J1-J2 = upper arm, J2-J3 = forearm, J3 = hand: the segment nearest the weapon's rest spot.
	protected string PickClosestBone(BFA_MechWeaponMountDefinition mount, Animation anim)
	{
		array<string> names = {"J1", "J2", "J3"};
		array<vector> heads = {};
		vector armLocal[4];
		mount.m_pFollowArm.GetLocalTransform(armLocal);

		foreach (string n : names)
		{
			TNodeId id = anim.GetBoneIndex(n);
			vector m[4];
			if (id == -1 || !anim.GetBoneMatrix(id, m))
				return "J3";
			vector mech[4];
			Math3D.MatrixMultiply4(armLocal, m, mech);
			heads.Insert(mech[3]);
		}

		vector p = mount.m_mBaseLocalTransform[3];
		float best = SegmentDistance(p, heads[0], heads[1]);
		string bestName = "J1";

		float d = SegmentDistance(p, heads[1], heads[2]);
		if (d < best)
		{
			best = d;
			bestName = "J2";
		}

		d = vector.Distance(p, heads[2]);
		if (d < best)
			bestName = "J3";

		return bestName;
	}

	//------------------------------------------------------------------------------------------------
	protected static float SegmentDistance(vector p, vector a, vector b)
	{
		vector ab = b - a;
		float lenSq = ab.LengthSq();
		if (lenSq < 0.000001)
			return vector.Distance(p, a);
		float t = Math.Clamp(vector.Dot(p - a, ab) / lenSq, 0, 1);
		return vector.Distance(p, a + ab * t);
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame: weapons riding on arms are moved with the arm, keeping their current aim.
	protected void FollowUpdate()
	{
		if (!m_aMounts)
			return;

		foreach (BFA_MechWeaponMountDefinition mount : m_aMounts)
		{
			if (!mount || mount.m_sFollowArmMount.IsEmpty() || !mount.m_bResolved || !mount.m_pEntity)
				continue;

			if (mount.m_iAimDriver != BFA_EMechMountAimDriver.ENTITY_ROTATION)
				continue;

			vector base[4];
			GetLiveBase(mount, base);
			if (!mount.m_bFollowReady)
				continue;

			vector deltaMat[4];
			Math3D.AnglesToMatrix(Vector(mount.m_fAimYaw, mount.m_fAimPitch, 0), deltaMat);
			deltaMat[3] = vector.Zero;

			vector finalMat[4];
			Math3D.MatrixMultiply4(base, deltaMat, finalMat);
			finalMat[3] = base[3];

			ApplyAimDriver(mount, mount.m_fAimYaw, mount.m_fAimPitch, finalMat);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureFollowTick()
	{
		if (s_iFollowHeartbeatMs != 0 && System.GetTickCount() - s_iFollowHeartbeatMs < 1000)
			return;

		if (!GetGame())
			return;
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		queue.Remove(StaticFollowTick);
		queue.CallLater(StaticFollowTick, 0, true);
		s_iFollowHeartbeatMs = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StaticFollowTick()
	{
		s_iFollowHeartbeatMs = System.GetTickCount();
		for (int i = s_aFollowComps.Count() - 1; i >= 0; i--)
		{
			BFA_MechWeaponMountComponent comp = s_aFollowComps[i];
			if (!comp || !comp.GetOwner())
			{
				s_aFollowComps.Remove(i);
				continue;
			}
			comp.FollowUpdate();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool BindAnimDriver(BFA_MechWeaponMountDefinition mount)
	{
		if (mount.m_bAnimBound)
			return true;

		if (mount.m_bAnimBindFailed || !mount.m_pEntity)
			return false;

		mount.m_pAnimController = AnimationControllerComponent.Cast(mount.m_pEntity.FindComponent(AnimationControllerComponent));
		if (!mount.m_pAnimController)
		{
			mount.m_bAnimBindFailed = true;
			BFA_MechLog.Log(string.Format("[BFA MOUNT] %1 uses an ANIM aim driver but its slot entity has no AnimationControllerComponent", mount.m_sMountId), LogLevel.WARNING);
			return false;
		}

		if (mount.m_iAimDriver == BFA_EMechMountAimDriver.ANIM_ANGLES)
		{
			mount.m_iAnimVarA = mount.m_pAnimController.BindFloatVariable(mount.m_sAnimYawVariable);
			mount.m_iAnimVarB = mount.m_pAnimController.BindFloatVariable(mount.m_sAnimPitchVariable);
		}
		else
		{
			mount.m_iAnimVarA = mount.m_pAnimController.BindFloatVariable(mount.m_sIkTargetXVariable);
			mount.m_iAnimVarB = mount.m_pAnimController.BindFloatVariable(mount.m_sIkTargetYVariable);
			mount.m_iAnimVarC = mount.m_pAnimController.BindFloatVariable(mount.m_sIkTargetZVariable);
		}

		mount.m_bAnimBound = true;

		// Shoulder pivot from a bone (read once from the bind pose: the chain root never moves).
		// Hand rest position (IK variables are still 0 here, so the arm is in its rest pose).
		mount.m_bIkEndRestKnown = false;
		if (mount.m_bIkTargetFromRest)
		{
			Animation restAnim = mount.m_pEntity.GetAnimation();
			if (restAnim)
			{
				TNodeId endId = restAnim.GetBoneIndex(mount.m_sIkEndBone);
				vector endMat[4];
				if (endId != -1 && restAnim.GetBoneMatrix(endId, endMat))
				{
					mount.m_vIkEndRest = endMat[3];
					mount.m_bIkEndRestKnown = true;
				}
			}
			if (mount.m_bIkEndRestKnown)
				Print(string.Format("[BFA MOUNT] %1: IK target is an offset from hand bone %2 at rest %3", mount.m_sMountId, mount.m_sIkEndBone, mount.m_vIkEndRest));
			else
				Print(string.Format("[BFA MOUNT] %1: hand bone '%2' not found - IK target sent as absolute", mount.m_sMountId, mount.m_sIkEndBone), LogLevel.WARNING);
		}

		mount.m_bIkPivotFromBone = false;
		if (!mount.m_sIkPivotBone.IsEmpty())
		{
			Animation anim = mount.m_pEntity.GetAnimation();
			if (anim)
			{
				TNodeId boneId = anim.GetBoneIndex(mount.m_sIkPivotBone);
				vector boneMat[4];
				if (boneId != -1 && anim.GetBoneMatrix(boneId, boneMat))
				{
					mount.m_vIkPivotModel = boneMat[3];
					mount.m_bIkPivotFromBone = true;
				}
			}

			if (!mount.m_bIkPivotFromBone)
				BFA_MechLog.Log(string.Format("[BFA MOUNT] %1: IK pivot bone '%2' not found - using the arm origin", mount.m_sMountId, mount.m_sIkPivotBone), LogLevel.WARNING);
			else
				BFA_MechLog.Log(string.Format("[BFA MOUNT] %1: IK pivot bone %2 at %3", mount.m_sMountId, mount.m_sIkPivotBone, mount.m_vIkPivotModel));
		}

		// Keep the arm entity active so its anim graph consumes the variables (same as the legs).
		mount.m_pEntity.SetFlags(EntityFlags.ACTIVE);

		BFA_MechLog.Log(string.Format("[BFA MOUNT] %1 anim driver=%2 vars=%3,%4,%5", mount.m_sMountId, mount.m_iAimDriver, mount.m_iAnimVarA, mount.m_iAnimVarB, mount.m_iAnimVarC));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Turn the solved, clamped aim into motion for this kind of mount.
	//! finalMat is the rigid-mount result in the mount parent's space; anim drivers use it to know
	//! where the clamped aim line points.
	protected void ApplyAimDriver(BFA_MechWeaponMountDefinition mount, float yaw, float pitch, vector finalMat[4])
	{
		if (mount.m_iAimDriver == BFA_EMechMountAimDriver.ANIM_ANGLES)
		{
			if (!BindAnimDriver(mount))
				return;

			mount.m_pAnimController.SetFloatVariable(mount.m_iAnimVarA, yaw);
			mount.m_pAnimController.SetFloatVariable(mount.m_iAnimVarB, pitch);
			return;
		}

		if (mount.m_iAimDriver == BFA_EMechMountAimDriver.ANIM_IK_TARGET)
		{
			if (!BindAnimDriver(mount))
			{
				Trace("bind" + mount.m_sMountId, string.Format("mount %1: IK arm has no AnimationControllerComponent / bind failed (entity %2)", mount.m_sMountId, mount.m_pEntity));
				return;
			}

			// Hand target = shoulder pivot + clamped aim line * reach, expressed in the arm
			// entity's own space (same convention the leg IK uses).
			vector pivotWorld = mount.m_pParent.CoordToParent(mount.m_mBaseLocalTransform[3]);
			if (mount.m_bIkPivotFromBone)
				pivotWorld = mount.m_pEntity.CoordToParent(mount.m_vIkPivotModel);
			vector aimWorld = mount.m_pParent.VectorToParent(finalMat[2]);
			if (aimWorld.LengthSq() > 0.000001)
				aimWorld.Normalize();

			vector targetLocal;
			if (mount.m_bIkSwingFromRest && mount.m_bIkEndRestKnown)
			{
				// Offset from the rest pose: straight ahead = rest, other aims swing the hand
				// along the arc a stiff arm would trace from the shoulder.
				vector restWorld = mount.m_pParent.VectorToParent(mount.m_mBaseLocalTransform[2]);
				if (restWorld.LengthSq() > 0.000001)
					restWorld.Normalize();
				float radius = mount.m_fIkSwingRadius;
				if (radius <= 0)
					radius = mount.m_fIkReach;
				targetLocal = mount.m_pEntity.VectorToLocal((aimWorld - restWorld) * Math.Max(radius, 0.1));
			}
			else
			{
				vector targetWorld = pivotWorld + aimWorld * Math.Max(mount.m_fIkReach, 0.1);
				targetLocal = mount.m_pEntity.CoordToLocal(targetWorld);
				if (mount.m_bIkEndRestKnown)
					targetLocal = targetLocal - mount.m_vIkEndRest;
			}

			// Melee punch / thrust: the hand goes further along the aim.
			if (mount.m_bSwingActive && mount.m_fSwingPush != 0)
				targetLocal = targetLocal + mount.m_pEntity.VectorToLocal(aimWorld) * mount.m_fSwingPush;

			targetLocal = SmoothIkTarget(mount, targetLocal);

			mount.m_pAnimController.SetFloatVariable(mount.m_iAnimVarA, targetLocal[0]);
			mount.m_pAnimController.SetFloatVariable(mount.m_iAnimVarB, targetLocal[1]);
			mount.m_pAnimController.SetFloatVariable(mount.m_iAnimVarC, targetLocal[2]);

			AimIkHand(mount, aimWorld);

			if (m_bDebugAim)
				DebugIkArm(mount, yaw, pitch, aimWorld, targetLocal);
			return;
		}

		// ENTITY_ROTATION: rigid gun.
		mount.m_pEntity.SetLocalTransform(finalMat);
		mount.m_pEntity.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! HAND AIM: turns the hand bone (through two anim-graph variables) so a weapon on the hand
	//! points along the aim line instead of staying level while the IK moves the wrist.
	protected void AimIkHand(BFA_MechWeaponMountDefinition mount, vector aimWorld)
	{
		if (mount.m_sIkHandYawVariable.IsEmpty() && mount.m_sIkHandPitchVariable.IsEmpty())
			return;
		if (!mount.m_pAnimController || !mount.m_pEntity)
			return;

		if (!mount.m_bHandBindTried)
		{
			mount.m_bHandBindTried = true;
			if (!mount.m_sIkHandYawVariable.IsEmpty())
				mount.m_iHandYawVar = mount.m_pAnimController.BindFloatVariable(mount.m_sIkHandYawVariable);
			if (!mount.m_sIkHandPitchVariable.IsEmpty())
				mount.m_iHandPitchVar = mount.m_pAnimController.BindFloatVariable(mount.m_sIkHandPitchVariable);

			if (mount.m_iHandYawVar == -1 && mount.m_iHandPitchVar == -1)
				BFA_MechLog.Log(string.Format("[BFA MOUNT] %1: hand aim variables '%2' / '%3' not found in the arm's anim graph - hand aim off", mount.m_sMountId, mount.m_sIkHandYawVariable, mount.m_sIkHandPitchVariable), LogLevel.WARNING);

			// Rest direction of the hand. The IK never turns the hand and our variables are
			// still 0 here, so the bone's current axes are its rest axes.
			Animation anim = mount.m_pEntity.GetAnimation();
			if (anim)
			{
				TNodeId handId = anim.GetBoneIndex(mount.m_sIkEndBone);
				vector handMat[4];
				if (handId != -1 && anim.GetBoneMatrix(handId, handMat))
				{
					mount.m_vHandRestRight = handMat[0].Normalized();
					mount.m_vHandRestUp = handMat[1].Normalized();
					mount.m_vHandRestFwd = handMat[2].Normalized();
					mount.m_bHandRestKnown = true;
				}
			}
			if (!mount.m_bHandRestKnown)
				BFA_MechLog.Log(string.Format("[BFA MOUNT] %1: hand bone '%2' not found - hand aim off", mount.m_sMountId, mount.m_sIkEndBone), LogLevel.WARNING);
		}

		if (!mount.m_bHandRestKnown)
			return;

		// Aim direction in the hand's rest frame.
		vector right = mount.m_pEntity.VectorToParent(mount.m_vHandRestRight);
		vector up = mount.m_pEntity.VectorToParent(mount.m_vHandRestUp);
		vector fwd = mount.m_pEntity.VectorToParent(mount.m_vHandRestFwd);

		float x = vector.Dot(aimWorld, right);
		float y = vector.Dot(aimWorld, up);
		float z = vector.Dot(aimWorld, fwd);

		float limit = Math.Max(mount.m_fIkHandMaxAngle, 0);
		float handYaw = Math.Clamp(Math.Atan2(x, z) * Math.RAD2DEG, -limit, limit);
		float flat = Math.Sqrt(x * x + z * z);
		float handPitch = Math.Clamp(Math.Atan2(y, flat) * Math.RAD2DEG, -limit, limit);
		if (mount.m_bIkHandInvertPitch)
			handPitch = -handPitch;

		if (mount.m_iHandYawVar != -1)
			mount.m_pAnimController.SetFloatVariable(mount.m_iHandYawVar, handYaw);
		if (mount.m_iHandPitchVar != -1)
			mount.m_pAnimController.SetFloatVariable(mount.m_iHandPitchVar, handPitch);
	}

	//------------------------------------------------------------------------------------------------
	//! Glides the IK target toward the wanted point (exponential, frame-rate independent).
	protected vector SmoothIkTarget(BFA_MechWeaponMountDefinition mount, vector wanted)
	{
		float now = 0;
		if (GetOwner() && GetOwner().GetWorld())
			now = GetOwner().GetWorld().GetWorldTime() * 0.001;

		if (mount.m_fIkSmoothTime <= 0 || !mount.m_bIkSmoothInit)
		{
			mount.m_vIkSmoothed = wanted;
			mount.m_bIkSmoothInit = true;
			mount.m_fIkSmoothLastTime = now;
			return wanted;
		}

		float dt = Math.Clamp(now - mount.m_fIkSmoothLastTime, 0.0, 0.25);
		mount.m_fIkSmoothLastTime = now;
		if (dt <= 0)
			return mount.m_vIkSmoothed;

		// ~95% of the way in Smooth Time (much faster during a melee swing).
		float smoothTime = mount.m_fIkSmoothTime;
		if (mount.m_bSwingActive)
			smoothTime = Math.Min(smoothTime, 0.05);
		float alpha = 1.0 - Math.Pow(0.05, dt / smoothTime);
		mount.m_vIkSmoothed = mount.m_vIkSmoothed + (wanted - mount.m_vIkSmoothed) * alpha;
		return mount.m_vIkSmoothed;
	}

	//------------------------------------------------------------------------------------------------
	//! Once a second: where the IK arm was told to reach, and where its hand (J3) really points.
	protected void DebugIkArm(BFA_MechWeaponMountDefinition mount, float yaw, float pitch, vector aimWorld, vector targetLocal)
	{
		// Own timer (the aim solver's diagnostics share m_fDiagNextTime and would swallow this).
		float now = 0;
		if (GetOwner() && GetOwner().GetWorld())
			now = GetOwner().GetWorld().GetWorldTime() * 0.001;
		float next;
		if (m_mTraceNext.Find("ik" + mount.m_sMountId, next) && now < next)
			return;
		m_mTraceNext.Set("ik" + mount.m_sMountId, now + 1.0);

		string hand = "no skeleton";
		Animation anim = mount.m_pEntity.GetAnimation();
		if (anim)
		{
			TNodeId j3 = anim.GetBoneIndex("J3");
			vector bone[4];
			if (j3 != -1 && anim.GetBoneMatrix(j3, bone))
			{
				vector handFwd = mount.m_pEntity.VectorToParent(bone[2]);
				vector handPos = mount.m_pEntity.CoordToParent(bone[3]);
				if (handFwd.LengthSq() > 0.000001)
					handFwd.Normalize();
				float off = Math.Acos(Math.Clamp(vector.Dot(handFwd, aimWorld), -1, 1)) * Math.RAD2DEG;
				vector mech = GetOwner().CoordToLocal(handPos);
				vector targetAbs = targetLocal;
				if (mount.m_bIkEndRestKnown)
					targetAbs = targetLocal + mount.m_vIkEndRest;
				vector targetMech = GetOwner().CoordToLocal(mount.m_pEntity.CoordToParent(targetAbs));
				hand = string.Format("handInMech=%1 targetInMech=%2 handFwdInMech=%3 offByDeg=%4", mech, targetMech, GetOwner().VectorToLocal(handFwd), off);
			}
			else
				hand = "bone J3 not found";
		}

		BFA_MechLog.Log(string.Format("[BFA IK DIAG] %1 yaw=%2 pitch=%3 aim=%4 targetLocal=%5 %6", mount.m_sMountId, yaw, pitch, aimWorld, targetLocal, hand));
		Print(string.Format("[BFA IK DIAG] %1 yaw=%2 pitch=%3 aim=%4 targetLocal=%5 %6", mount.m_sMountId, yaw, pitch, aimWorld, targetLocal, hand));
	}

	//------------------------------------------------------------------------------------------------
	//! Exact aim from a point given in the mech's own local space (what the network carries).
	void ApplyStationAimLocalPoint(string stationId, vector mechLocalPoint)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		ApplyStationAimPoint(stationId, owner.CoordToParent(mechLocalPoint));
	}

	//------------------------------------------------------------------------------------------------
	// Replicated/server path.
	// Converts the station direction into a distant world point and uses the same solver.
	protected void ApplyMountAimDirection(BFA_MechWeaponMountDefinition mount, vector mechLocalAimDirection)
	{
		if (!ResolveMount(mount))
			return;

		vector worldDirection = LocalAimToWorld(mechLocalAimDirection);

		vector aimPointWorld = mount.m_pEntity.GetOrigin() + worldDirection * Math.Max(mount.m_fConvergenceDistance, 10.0);

		ApplyMountAimPoint(mount, aimPointWorld);
	}

	//------------------------------------------------------------------------------------------------
	// Replicated aim path.
	void ApplyStationAim(string stationId, vector mechLocalAimDirection)
	{
		if (!m_aMounts || stationId.IsEmpty())
			return;

		foreach (BFA_MechWeaponMountDefinition mount : m_aMounts)
		{
			if (!mount)
				continue;

			bool aims = StationAimsMount(stationId, mount);
			if (mount.m_iAimDriver != BFA_EMechMountAimDriver.ENTITY_ROTATION)
				Trace("sd" + stationId + mount.m_sMountId, string.Format("station %1 aim direction -> mount %2 (station %3): %4", stationId, mount.m_sMountId, mount.m_sControlStation, aims));
			if (!aims)
				continue;

			ApplyMountAimDirection(mount, mechLocalAimDirection);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Local precision aim path.
	// This is the function V52 BFA_MechAimComponent needs.
	void ApplyStationAimPoint(string stationId, vector aimPointWorld)
	{
		if (!m_aMounts || stationId.IsEmpty())
			return;

		foreach (BFA_MechWeaponMountDefinition mount : m_aMounts)
		{
			if (!mount)
				continue;

			bool aims = StationAimsMount(stationId, mount);
			if (mount.m_iAimDriver != BFA_EMechMountAimDriver.ENTITY_ROTATION)
				Trace("st" + stationId + mount.m_sMountId, string.Format("station %1 aim point -> mount %2 (station %3): %4", stationId, mount.m_sMountId, mount.m_sControlStation, aims));
			if (!aims)
				continue;

			ApplyMountAimPoint(mount, aimPointWorld);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A mount follows its own station's aim. When nobody sits in that station (e.g. COMMANDER
	//! weapons on a boss crewed by a single AI pilot), the DRIVER's aim moves it instead - the
	//! driver can already fire those weapons, so its guns and arms must point where it shoots.
	protected bool StationAimsMount(string stationId, BFA_MechWeaponMountDefinition mount)
	{
		if (mount.m_sControlStation == stationId)
			return true;

		if (stationId != DRIVER_STATION || mount.m_sControlStation.IsEmpty())
			return false;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(GetOwner().FindComponent(BFA_MechCoreComponent));
		if (!core)
			return false;

		IEntity occupant = core.GetStationOccupant(mount.m_sControlStation);
		if (occupant)
			Trace("occ" + mount.m_sMountId, string.Format("mount %1: station %2 is occupied by %3, so the driver does not aim it", mount.m_sMountId, mount.m_sControlStation, occupant));
		return occupant == null;
	}

	//------------------------------------------------------------------------------------------------
	//! The point the mount holding this entity was last told to aim at (the station's crosshair /
	//! AI target). Weapons on IK arms use it: an IK hand bone moves but does not turn, so its axis
	//! alone cannot be trusted as the firing direction.
	bool GetAimPointForEntity(IEntity weaponEntity, out vector aimPoint)
	{
		if (!m_aMounts || !weaponEntity)
			return false;

		foreach (BFA_MechWeaponMountDefinition mount : m_aMounts)
		{
			if (mount && mount.m_bHasAimPoint && mount.m_pEntity == weaponEntity)
			{
				aimPoint = mount.m_vLastAimPoint;
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	void ApplyAllCurrentStationAims()
	{
		BFA_MechAimComponent aim = BFA_MechAimComponent.Cast(GetOwner().FindComponent(BFA_MechAimComponent));

		if (!aim)
			return;

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(GetOwner().FindComponent(BFA_MechCoreComponent));
		if (!core)
			return;

		array<string> stationIds = {};
		core.GetStationIds(stationIds);
		foreach (string stationId : stationIds)
			ApplyStationAimLocalPoint(stationId, aim.GetAimLocalPointForStation(stationId));
	}

	//------------------------------------------------------------------------------------------------
	// Draw one green convergence marker for every mount controlled by this station.
	void DrawStationConvergence(string stationId, vector camOrigin, vector camRight, vector camUp, vector camForward, float planeDistance, float aimDistance)
	{
		if (!m_aMounts || stationId.IsEmpty())
			return;

		int flags = ShapeFlags.ONCE | ShapeFlags.NOZBUFFER;

		foreach (BFA_MechWeaponMountDefinition mount : m_aMounts)
		{
			if (!mount || mount.m_sControlStation != stationId || !ResolveMount(mount))
				continue;

			vector gunOrigin = mount.m_pEntity.GetOrigin();
			vector gunForward = mount.m_pEntity.GetTransformAxis(2);

			if (gunForward.LengthSq() < 0.000001)
				continue;

			gunForward.Normalize();

			vector gunPoint = gunOrigin + gunForward * Math.Max(aimDistance, 10.0);
			vector cameraToGunPoint = gunPoint - camOrigin;

			if (cameraToGunPoint.LengthSq() < 0.000001)
				continue;

			vector gunViewDir = cameraToGunPoint.Normalized();

			float facing = vector.Dot(gunViewDir, camForward);

			if (facing <= 0.01)
				continue;

			float alongView = planeDistance / facing;

			vector gunMarker = camOrigin + gunViewDir * alongView;

			Shape.CreateSphere(Color.GREEN, flags, gunMarker, 0.012);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Captures every follower's rest pose (arms still in their bind pose) and starts the ticker.
	protected void StartFollowing()
	{
		m_bHasFollowers = false;
		if (!m_aMounts)
			return;

		foreach (BFA_MechWeaponMountDefinition mount : m_aMounts)
		{
			if (mount && !mount.m_sFollowArmMount.IsEmpty())
			{
				m_bHasFollowers = true;
				PrepareFollow(mount);
			}
		}

		if (!m_bHasFollowers)
			return;

		if (!s_aFollowComps.Contains(this))
			s_aFollowComps.Insert(this);
		EnsureFollowTick();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_iResolveAttempts = 0;

		ScriptCallQueue queue = GetGame().GetCallqueue();

		if (queue)
			queue.CallLater(ResolveAllDelayed, 400, false);

		BFA_MechLog.Log("[BFA MOUNT] V52 CAMERA_POINT_AIM initialized");
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
				queue.Remove(ResolveAllDelayed);
		}

		s_aFollowComps.RemoveItem(this);

		super.OnDelete(owner);
	}
}
