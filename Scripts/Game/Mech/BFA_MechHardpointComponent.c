enum BFA_EMechHardpointClass
{
	ANY = 0,
	ARM = 1,
	SHOULDER = 2,
	BACK = 3,
	UTILITY = 4
}

[BaseContainerProps()]
class BFA_MechHardpointDefinition
{
	[Attribute("Hardpoint", UIWidgets.EditBox, "Human-readable hardpoint name.", category: "Hardpoint")]
	string m_sName;

	[Attribute("0", UIWidgets.EditBox, "0 ANY, 1 ARM, 2 SHOULDER, 3 BACK, 4 UTILITY.", category: "Hardpoint")]
	int m_iClass;

	[Attribute("", UIWidgets.EditBox, "Optional BFA_MechWeaponMountComponent mount ID associated with this hardpoint.", category: "Hardpoint")]
	string m_sAimMountId;

	[Attribute("false", UIWidgets.CheckBox, "Bind the weapon module component already present on the aim-mount entity instead of spawning a separate module prefab.", category: "Hardpoint")]
	bool m_bBindExistingAimMount;

	[Attribute("0", UIWidgets.Auto, "Module ID installed when this mech first spawns. 0 starts empty.", category: "Hardpoint")]
	int m_iDefaultModuleId;

	[Attribute("", UIWidgets.EditBox, "Animated arms: bone on the aim-mount entity the spawned weapon is attached to (e.g. a hand/wrist bone), so it follows the arm animation. Empty = attach to the entity origin (rigid mounts).", category: "Hardpoint")]
	string m_sAttachBone;

	[Attribute(desc: "WHERE the weapon sits, placed on the MECH with the gizmo (can follow a bone via PivotID). When set, it overrides the slot position (bind-existing) and the offset below. The weapon still aims from here.", category: "Hardpoint")]
	ref PointInfo m_Point;

	[Attribute("0 0 0", UIWidgets.Auto, "LEGACY (used only when Point is empty). Position offset in metres (X right, Y up, Z forward). Spawned modules: relative to the aim mount / attach bone / mech. Bind-existing: moves the slot's own weapon from where the SlotManager placed it.", category: "Hardpoint")]
	vector m_vOffset;

	[Attribute("0 0 0", UIWidgets.Auto, "LEGACY (used only when Point is empty). Rotation offset in degrees (yaw pitch roll), same rules as Offset.", category: "Hardpoint")]
	vector m_vAngles;

	[Attribute("false", UIWidgets.CheckBox, "Use the module's right-side prefab when one is provided.", category: "Hardpoint")]
	bool m_bRightSide;

	[Attribute("", UIWidgets.EditBox, "Crew station that fires whatever is mounted here (e.g. GUNNER). Empty = use the weapon module's own setting.", category: "Routing")]
	string m_sControlStation;

	[Attribute("0", UIWidgets.Auto, "Weapon group this hardpoint fires on (e.g. 1 = main pair, 2 = secondary pair). 0 = use the weapon module's own setting. This is how one weapon prefab can be a main gun on one mech and a secondary on another.", category: "Routing")]
	int m_iWeaponGroup;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class BFA_MechLoadoutSlot
{
	[Attribute("", UIWidgets.EditBox, "Hardpoint name (m_sName) this entry sets.", category: "Loadout")]
	string m_sHardpoint;

	[Attribute("0", UIWidgets.Auto, "Module ID to install. 0 = empty.", category: "Loadout")]
	int m_iModuleId;
}

//------------------------------------------------------------------------------------------------
//! A named loadout the crew can switch to in the field. Hardpoints not listed keep their module.
[BaseContainerProps()]
class BFA_MechLoadoutPreset
{
	[Attribute("Loadout", UIWidgets.EditBox, "Display name.", category: "Loadout")]
	string m_sName;

	[Attribute("", UIWidgets.Object, "Hardpoint -> module assignments for this loadout.", category: "Loadout")]
	ref array<ref BFA_MechLoadoutSlot> m_aSlots;
}

[BaseContainerProps()]
class BFA_MechModuleDefinition
{
	[Attribute("1", UIWidgets.EditBox, "Module ID. 1-1023. 0 is reserved for empty.", category: "Module")]
	int m_iId;

	[Attribute("Module", UIWidgets.EditBox, "Display/debug name.", category: "Module")]
	string m_sLabel;

	[Attribute("0", UIWidgets.EditBox, "0 ANY, 1 ARM, 2 SHOULDER, 3 BACK, 4 UTILITY.", category: "Module")]
	int m_iClass;

	[Attribute("", UIWidgets.EditBox, "On hardpoints that bind an existing aim-mount weapon: the m_sModuleId the existing weapon component must report to be reused (e.g. LAS_CANNON). If it does not match, the existing weapon is hidden and m_sPrefab is spawned on the aim mount instead.", category: "Module")]
	string m_sWeaponModuleId;

	[Attribute("", UIWidgets.ResourceNamePicker, "Default/left prefab mounted for this module.", params: "et", category: "Module")]
	ResourceName m_sPrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional right-side prefab for mirrored hardpoints.", params: "et", category: "Module")]
	ResourceName m_sRightPrefab;

	[Attribute("0", UIWidgets.EditBox, "Payload weight in tonnes. Available to movement/loadout rules.", category: "Module")]
	float m_fWeightTons;

	[Attribute("0", UIWidgets.EditBox, "Optional field-swap time in seconds.", category: "Module")]
	float m_fSwapTime;
}

[ComponentEditorProps(category: "GameScripted/Mech", description: "Generic replicated mech hardpoints/modules. Replicates compact IDs; module entities are built locally on every machine.")]
class BFA_MechHardpointComponentClass : ScriptComponentClass
{
}

class BFA_MechHardpointComponent : ScriptComponent
{
	static const int BFA_MODULE_BITS = 10;
	static const int BFA_MODULE_MASK = 1023;
	static const int BFA_HARDPOINTS_PER_WORD = 3;
	static const int BFA_MAX_HARDPOINTS = 18;

	[Attribute("", UIWidgets.Object, "Hardpoints available on this mech. Up to 18 are supported by the six replicated loadout words.", category: "Hardpoints")]
	ref array<ref BFA_MechHardpointDefinition> m_aHardpoints;

	[Attribute("", UIWidgets.Object, "Modules this mech is allowed to mount.", category: "Modules")]
	ref array<ref BFA_MechModuleDefinition> m_aModules;

	// Player's optional six-group membership per hardpoint, 0 = prefab default.
	// Five slots per replicated word, six bits per slot = 30 bits / word.
	// Four words cover all 18 hardpoints. A bit means the weapon is in that group.
	[RplProp(onRplName: "OnRoutingReplicated")]
	protected int m_iRouteGroups0;
	[RplProp(onRplName: "OnRoutingReplicated")]
	protected int m_iRouteGroups1;
	[RplProp(onRplName: "OnRoutingReplicated")]
	protected int m_iRouteGroups2;
	[RplProp(onRplName: "OnRoutingReplicated")]
	protected int m_iRouteGroups3;

	[RplProp(onRplName: "OnLoadoutReplicated")]
	protected int m_iLoadout0;

	[RplProp(onRplName: "OnLoadoutReplicated")]
	protected int m_iLoadout1;

	[RplProp(onRplName: "OnLoadoutReplicated")]
	protected int m_iLoadout2;

	[RplProp(onRplName: "OnLoadoutReplicated")]
	protected int m_iLoadout3;
	[RplProp(onRplName: "OnLoadoutReplicated")]
	protected int m_iLoadout4;
	[RplProp(onRplName: "OnLoadoutReplicated")]
	protected int m_iLoadout5;

	[Attribute("", UIWidgets.Object, "Field loadouts the crew can cycle through. Optional.", category: "Swapping")]
	ref array<ref BFA_MechLoadoutPreset> m_aLoadoutPresets;

	[Attribute("", UIWidgets.EditBox, "Crew station allowed to change modules (e.g. GUNNER). Empty = any crew station on this mech.", category: "Swapping")]
	string m_sSwapStation;

	[Attribute("true", UIWidgets.CheckBox, "Only allow swapping while the mech is (nearly) stopped.", category: "Swapping")]
	bool m_bSwapRequiresStationary;

	[Attribute("0.5", UIWidgets.Auto, "Maximum root speed in m/s that still counts as stopped.", category: "Swapping")]
	float m_fSwapMaxSpeed;

	[Attribute("1.0", UIWidgets.Auto, "Minimum swap time in seconds, even for modules with m_fSwapTime 0.", category: "Swapping")]
	float m_fMinSwapTime;

	// Bit per hardpoint currently being swapped. Those hardpoints cannot fire. For HUD/sounds.
	[RplProp(onRplName: "OnSwapStateReplicated")]
	protected int m_iSwappingMask;

	// Index into m_aLoadoutPresets last applied; -1 = default/custom.
	[RplProp(onRplName: "OnSwapStateReplicated")]
	protected int m_iActivePreset = -1;

	// Server-only pending swap target.
	protected int m_iPendingLoadout0;
	protected int m_iPendingLoadout1;
	protected int m_iPendingLoadout2;
	protected int m_iPendingLoadout3;
	protected int m_iPendingLoadout4;
	protected int m_iPendingLoadout5;
	protected int m_iPendingPreset = -1;

	protected ref array<int> m_aMountedIds = {};
	protected ref array<IEntity> m_aModuleEntities = {};
	protected ref array<BFA_MechWeaponModuleComponent> m_aWeaponModules = {};
	// Existing aim-mount weapons hidden because a different module replaced them.
	protected ref array<IEntity> m_aHiddenAimMounts = {};
	protected bool m_bInitialized;

	// Rebuild retry. Slot children / aim mounts can arrive after the root on dedicated MP,
	// the same timing gap the leg pairs had. Never give up silently on the first attempt.
	static const int BFA_REBUILD_RETRY_MS = 250;
	static const int BFA_REBUILD_MAX_ATTEMPTS = 20;
	protected int m_iRebuildAttempts;
	protected bool m_bRebuildRetryQueued;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		InitializeArrays();

		if (Replication.IsServer())
			ApplyDefaultLoadout();

		// Gives attached/child entities time to finish construction on spawn and late join.
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			m_bRebuildRetryQueued = true;
			queue.CallLater(RebuildRetry, 100, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(RebuildRetry);
				queue.Remove(FinishSwap);
			}
		}

		m_bRebuildRetryQueued = false;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildRetry()
	{
		m_bRebuildRetryQueued = false;
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueRebuildRetry()
	{
		if (m_bRebuildRetryQueued)
			return;

		if (m_iRebuildAttempts >= BFA_REBUILD_MAX_ATTEMPTS)
		{
			BFA_MechLog.Log(string.Format(
				"[BFA HARDPOINT] GAVE UP after %1 attempts - some hardpoints never bound. Weapon fire may be using the legacy fallback.",
				m_iRebuildAttempts
			), LogLevel.WARNING);
			return;
		}

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_iRebuildAttempts++;
		m_bRebuildRetryQueued = true;
		queue.CallLater(RebuildRetry, BFA_REBUILD_RETRY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetAimMountHidden(int hardpointIndex, IEntity aimMount, bool hidden)
	{
		if (hardpointIndex < 0 || hardpointIndex >= m_aHiddenAimMounts.Count())
			return;

		if (hidden)
		{
			if (!aimMount)
				return;

			// Non-recursive: the replacement module is spawned as a child of this entity so
			// it keeps following the arm aim, and must stay visible.
			aimMount.ClearFlags(EntityFlags.VISIBLE, false);
			m_aHiddenAimMounts[hardpointIndex] = aimMount;
			return;
		}

		IEntity previous = m_aHiddenAimMounts[hardpointIndex];
		if (previous)
			previous.SetFlags(EntityFlags.VISIBLE, false);

		m_aHiddenAimMounts[hardpointIndex] = null;
	}

	//------------------------------------------------------------------------------------------------
	//! True when an existing aim-mount weapon has been replaced by a different module.
	//! The legacy weapon router uses this so a hidden Las Cannon can never fire.
	bool IsAimMountReplaced(string aimMountId)
	{
		if (aimMountId.IsEmpty() || !m_aHardpoints)
			return false;

		int count = GetHardpointCount();
		for (int hp = 0; hp < count && hp < m_aHiddenAimMounts.Count(); hp++)
		{
			BFA_MechHardpointDefinition hardpoint = m_aHardpoints[hp];
			if (!hardpoint || hardpoint.m_sAimMountId != aimMountId)
				continue;

			if (m_aHiddenAimMounts[hp] || IsHardpointSwapping(hp))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Should the weapon component already sitting on the aim mount be reused for this module?
	protected bool ShouldReuseExistingWeapon(BFA_MechModuleDefinition module, BFA_MechWeaponModuleComponent existing)
	{
		if (!module || !existing)
			return false;

		// Explicit match: module definition names the weapon it expects on the arm.
		if (!module.m_sWeaponModuleId.IsEmpty())
			return module.m_sWeaponModuleId == existing.GetModuleId();

		// Legacy: no ID and no prefab means "whatever is already on the arm".
		return module.m_sPrefab.IsEmpty() && module.m_sRightPrefab.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The hardpoint's Point in mech model space. False when no Point is set.
	protected bool GetHardpointModelTransform(BFA_MechHardpointDefinition hardpoint, out vector mat[4])
	{
		if (!hardpoint || !hardpoint.m_Point || !GetOwner())
			return false;

		hardpoint.m_Point.Init(GetOwner());
		hardpoint.m_Point.GetModelTransform(mat);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void InitializeArrays()
	{
		m_aMountedIds.Clear();
		m_aModuleEntities.Clear();
		m_aWeaponModules.Clear();
		m_aHiddenAimMounts.Clear();

		int count = 0;
		if (m_aHardpoints)
			count = Math.Min(m_aHardpoints.Count(), BFA_MAX_HARDPOINTS);

		for (int i = 0; i < count; i++)
		{
			m_aMountedIds.Insert(-1);
			m_aModuleEntities.Insert(null);
			m_aWeaponModules.Insert(null);
			m_aHiddenAimMounts.Insert(null);
		}

		m_bInitialized = true;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetGroupRouteWord(int index)
	{
		switch (index)
		{
			case 0: return m_iRouteGroups0;
			case 1: return m_iRouteGroups1;
			case 2: return m_iRouteGroups2;
			case 3: return m_iRouteGroups3;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetGroupRouteWord(int index, int value)
	{
		switch (index)
		{
			case 0: m_iRouteGroups0 = value; break;
			case 1: m_iRouteGroups1 = value; break;
			case 2: m_iRouteGroups2 = value; break;
			case 3: m_iRouteGroups3 = value; break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Raw per-weapon selection: bits 0-5 mean groups 1-6.
	//! Zero means the prefab's original single group, not an empty weapon.
	int GetCustomWeaponGroupMask(int hardpointIndex)
	{
		if (hardpointIndex < 0 || hardpointIndex >= GetHardpointCount())
			return 0;
		int word = GetGroupRouteWord(hardpointIndex / 5);
		int shift = Math.Mod(hardpointIndex, 5) * 6;
		return (word >> shift) & 63;
	}

	//------------------------------------------------------------------------------------------------
	//! Authored default: prefer the hardpoint setting, then the module's
	//! original group. Runtime overrides must not affect this fallback.
	int GetDefaultWeaponGroupMask(int hardpointIndex)
	{
		if (hardpointIndex < 0 || hardpointIndex >= GetHardpointCount())
			return 0;
		int defaultGroup = 0;
		if (m_aHardpoints && m_aHardpoints[hardpointIndex])
			defaultGroup = m_aHardpoints[hardpointIndex].m_iWeaponGroup;
		if (defaultGroup <= 0)
		{
			BFA_MechWeaponModuleComponent module = GetInstalledModuleAt(hardpointIndex);
			if (module)
				defaultGroup = module.GetModuleDefaultWeaponGroup();
		}
		if (defaultGroup >= 1 && defaultGroup <= 6)
			return 1 << (defaultGroup - 1);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Effective groups for the Mech Lab; custom masks allow membership in multiple.
	int GetEffectiveWeaponGroupMask(int hardpointIndex)
	{
		int groups = GetCustomWeaponGroupMask(hardpointIndex);
		if (groups > 0)
			return groups;
		return GetDefaultWeaponGroupMask(hardpointIndex);
	}

	//------------------------------------------------------------------------------------------------
	bool IsWeaponInGroup(int hardpointIndex, int weaponGroup)
	{
		if (weaponGroup < 1 || weaponGroup > 6)
			return false;
		return (GetEffectiveWeaponGroupMask(hardpointIndex) & (1 << (weaponGroup - 1))) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Backward-compatible getter used by the older inline group control.
	//! A multi-group weapon reports its first selected group here, but firing
	//! routes through the full six-bit mask instead of using this one value.
	int GetCustomWeaponGroup(int hardpointIndex)
	{
		int mask = GetCustomWeaponGroupMask(hardpointIndex);
		for (int group = 1; group <= 6; group++)
		{
			if ((mask & (1 << (group - 1))) != 0)
				return group;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyCustomGroupToModule(int hardpointIndex)
	{
		if (hardpointIndex < 0 || hardpointIndex >= m_aWeaponModules.Count())
			return;
		BFA_MechWeaponModuleComponent module = m_aWeaponModules[hardpointIndex];
		if (!module)
			return;
		int chosen = GetCustomWeaponGroup(hardpointIndex);
		if (chosen <= 0 && m_aHardpoints && hardpointIndex < m_aHardpoints.Count() && m_aHardpoints[hardpointIndex])
			chosen = m_aHardpoints[hardpointIndex].m_iWeaponGroup;
		module.SetRuntimeWeaponGroup(chosen);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRoutingReplicated()
	{
		for (int hp = 0; hp < GetHardpointCount(); hp++)
			ApplyCustomGroupToModule(hp);
	}

	//------------------------------------------------------------------------------------------------
	//! Only the trusted server-side Mech Lab/controller may call this.
	//! A nonzero mask puts a weapon in any combination of groups 1-6.
	//! Zero restores the original prefab group.
	bool ServerSetCustomWeaponGroupMask(int hardpointIndex, int groupMask)
	{
		if (!Replication.IsServer() || hardpointIndex < 0 || hardpointIndex >= GetHardpointCount() || groupMask < 0 || groupMask > 63)
			return false;
		if (GetCustomWeaponGroupMask(hardpointIndex) == groupMask)
			return true;
		int wordIndex = hardpointIndex / 5;
		int shift = Math.Mod(hardpointIndex, 5) * 6;
		int word = GetGroupRouteWord(wordIndex);
		word = (word & ~(63 << shift)) | (groupMask << shift);
		SetGroupRouteWord(wordIndex, word);
		ApplyCustomGroupToModule(hardpointIndex);
		Replication.BumpMe();
		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(GetOwner().FindComponent(BFA_MechCoreComponent));
		if (core)
			core.ServerRevalidateWeaponSelection();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Retain old single-group requests for backward-compatible callers.
	bool ServerSetCustomWeaponGroup(int hardpointIndex, int group)
	{
		if (group < 0 || group > 6)
			return false;
		if (group == 0)
			return ServerSetCustomWeaponGroupMask(hardpointIndex, 0);
		return ServerSetCustomWeaponGroupMask(hardpointIndex, 1 << (group - 1));
	}

	//------------------------------------------------------------------------------------------------
	//! A custom group is usable only if a weapon at this station belongs to it.
	bool HasCustomWeaponGroupForStation(string stationId, int group)
	{
		if (group < 1 || group > 6)
			return false;
		int bit = 1 << (group - 1);
		for (int hp = 0; hp < GetHardpointCount(); hp++)
		{
			if ((GetCustomWeaponGroupMask(hp) & bit) == 0)
				continue;
			BFA_MechWeaponModuleComponent module = GetInstalledModuleAt(hp);
			if (!module)
				continue;
			string station = module.GetControlStation();
			if (station.IsEmpty() || station == stationId)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
		protected void OnLoadoutReplicated()
	{
		// A new loadout gets a fresh retry budget.
		m_iRebuildAttempts = 0;
		Rebuild();
	}


	//------------------------------------------------------------------------------------------------
	protected void ApplyDefaultLoadout()
	{
		if (!Replication.IsServer() || !m_aHardpoints)
			return;

		// A non-zero replicated loadout means this mech already has an authored/runtime loadout.
		if (m_iLoadout0 != 0 || m_iLoadout1 != 0 || m_iLoadout2 != 0 || m_iLoadout3 != 0 || m_iLoadout4 != 0 || m_iLoadout5 != 0)
			return;

		bool changed = false;
		int count = GetHardpointCount();

		for (int hp = 0; hp < count; hp++)
		{
			BFA_MechHardpointDefinition hardpoint = m_aHardpoints[hp];
			if (!hardpoint)
				continue;

			int moduleId = hardpoint.m_iDefaultModuleId;
			if (moduleId <= 0 || moduleId > BFA_MODULE_MASK)
				continue;

			if (!ModuleFitsHardpoint(hp, moduleId))
				continue;

			int wordIndex = hp / BFA_HARDPOINTS_PER_WORD;
			int slotInWord = Math.Mod(hp, BFA_HARDPOINTS_PER_WORD);
			int shift = slotInWord * BFA_MODULE_BITS;
			int mask = BFA_MODULE_MASK << shift;
			int word = GetLoadoutWord(wordIndex);

			word = (word & ~mask) | (moduleId << shift);
			SetLoadoutWord(wordIndex, word);
			changed = true;
		}

		if (changed)
			Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int GetHardpointCount()
	{
		if (!m_aHardpoints)
			return 0;

		return Math.Min(m_aHardpoints.Count(), BFA_MAX_HARDPOINTS);
	}

	//------------------------------------------------------------------------------------------------
	int GetModuleAt(int hardpointIndex)
	{
		if (hardpointIndex < 0 || hardpointIndex >= GetHardpointCount())
			return 0;

		int wordIndex = hardpointIndex / BFA_HARDPOINTS_PER_WORD;
		int slotInWord = Math.Mod(hardpointIndex, BFA_HARDPOINTS_PER_WORD);
		int shift = slotInWord * BFA_MODULE_BITS;
		int word = GetLoadoutWord(wordIndex);

		return (word >> shift) & BFA_MODULE_MASK;
	}


	//------------------------------------------------------------------------------------------------
	IEntity GetModuleEntityAt(int hardpointIndex)
	{
		if (hardpointIndex < 0 || hardpointIndex >= m_aModuleEntities.Count())
			return null;

		return m_aModuleEntities[hardpointIndex];
	}

	//------------------------------------------------------------------------------------------------
	//! The module installed on a hardpoint even while it is swapping (effects only - use
	//! GetWeaponModuleAt() for anything that fires).
	BFA_MechWeaponModuleComponent GetInstalledModuleAt(int hardpointIndex)
	{
		if (hardpointIndex < 0 || hardpointIndex >= m_aWeaponModules.Count())
			return null;

		return m_aWeaponModules[hardpointIndex];
	}

	//------------------------------------------------------------------------------------------------
	BFA_MechWeaponModuleComponent GetWeaponModuleAt(int hardpointIndex)
	{
		if (hardpointIndex < 0 || hardpointIndex >= m_aWeaponModules.Count())
			return null;

		// A hardpoint mid-swap is offline.
		if (IsHardpointSwapping(hardpointIndex))
			return null;

		return m_aWeaponModules[hardpointIndex];
	}

	//------------------------------------------------------------------------------------------------
	string GetHardpointName(int hardpointIndex)
	{
		if (!m_aHardpoints || hardpointIndex < 0 || hardpointIndex >= GetHardpointCount())
			return string.Empty;

		BFA_MechHardpointDefinition hardpoint = m_aHardpoints[hardpointIndex];
		if (!hardpoint)
			return string.Empty;

		return hardpoint.m_sName;
	}

	//------------------------------------------------------------------------------------------------
	int FindHardpointIndex(string hardpointName)
	{
		if (!m_aHardpoints || hardpointName.IsEmpty())
			return -1;

		int count = GetHardpointCount();
		for (int i = 0; i < count; i++)
		{
			BFA_MechHardpointDefinition hardpoint = m_aHardpoints[i];
			if (hardpoint && hardpoint.m_sName == hardpointName)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	BFA_MechWeaponModuleComponent GetWeaponModuleByHardpointName(string hardpointName)
	{
		int index = FindHardpointIndex(hardpointName);
		if (index < 0)
			return null;

		return GetWeaponModuleAt(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only mutation. Use SCR_PlayerController.BFA_Mech_RequestSetModule from clients.
	void ServerSetModule(int hardpointIndex, int moduleId)
	{
		if (!Replication.IsServer())
			return;

		if (hardpointIndex < 0 || hardpointIndex >= GetHardpointCount())
			return;

		if (moduleId < 0 || moduleId > BFA_MODULE_MASK)
			return;

		if (moduleId != 0 && !ModuleFitsHardpoint(hardpointIndex, moduleId))
			return;

		int wordIndex = hardpointIndex / BFA_HARDPOINTS_PER_WORD;
		int slotInWord = Math.Mod(hardpointIndex, BFA_HARDPOINTS_PER_WORD);
		int shift = slotInWord * BFA_MODULE_BITS;
		int mask = BFA_MODULE_MASK << shift;
		int word = GetLoadoutWord(wordIndex);

		word = (word & ~mask) | (moduleId << shift);
		SetLoadoutWord(wordIndex, word);

		Replication.BumpMe();

		// The authoring machine does not receive its own onRpl callback.
		m_iRebuildAttempts = 0;
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	protected int GetLoadoutWord(int wordIndex)
	{
		if (wordIndex == 0)
			return m_iLoadout0;
		if (wordIndex == 1) return m_iLoadout1;
		if (wordIndex == 2) return m_iLoadout2;
		if (wordIndex == 3) return m_iLoadout3;
		if (wordIndex == 4) return m_iLoadout4;
		return m_iLoadout5;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLoadoutWord(int wordIndex, int value)
	{
		if (wordIndex == 0)
			m_iLoadout0 = value;
		else if (wordIndex == 1)
			m_iLoadout1 = value;
		else if (wordIndex == 2) m_iLoadout2 = value;
		else if (wordIndex == 3) m_iLoadout3 = value;
		else if (wordIndex == 4) m_iLoadout4 = value;
		else m_iLoadout5 = value;
	}

	//------------------------------------------------------------------------------------------------
	//! Module entry by id (null for 0 / unknown). For add-ons and gameplay mods.
	BFA_MechModuleDefinition GetModuleDefinition(int moduleId)
	{
		return FindModule(moduleId);
	}

	//------------------------------------------------------------------------------------------------
	protected BFA_MechModuleDefinition FindModule(int moduleId)
	{
		if (!m_aModules)
			return null;

		foreach (BFA_MechModuleDefinition module : m_aModules)
		{
			if (module && module.m_iId == moduleId)
				return module;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Can this module go on this hardpoint (class check)? For loadout screens and add-ons.
	bool CanModuleFit(int hardpointIndex, int moduleId)
	{
		if (moduleId == 0)
			return hardpointIndex >= 0 && hardpointIndex < GetHardpointCount();
		return ModuleFitsHardpoint(hardpointIndex, moduleId);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ModuleFitsHardpoint(int hardpointIndex, int moduleId)
	{
		if (!m_aHardpoints || hardpointIndex < 0 || hardpointIndex >= m_aHardpoints.Count())
			return false;

		BFA_MechHardpointDefinition hardpoint = m_aHardpoints[hardpointIndex];
		BFA_MechModuleDefinition module = FindModule(moduleId);
		if (!hardpoint || !module)
			return false;

		if (hardpoint.m_iClass == BFA_EMechHardpointClass.ANY || module.m_iClass == BFA_EMechHardpointClass.ANY)
			return true;

		return hardpoint.m_iClass == module.m_iClass;
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName GetPrefabFor(int hardpointIndex, BFA_MechModuleDefinition module)
	{
		if (!module || !m_aHardpoints)
			return ResourceName.Empty;

		BFA_MechHardpointDefinition hardpoint = m_aHardpoints[hardpointIndex];
		if (hardpoint && hardpoint.m_bRightSide && !module.m_sRightPrefab.IsEmpty())
			return module.m_sRightPrefab;

		return module.m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	protected void Rebuild()
	{
		if (!m_bInitialized)
			InitializeArrays();

		IEntity owner = GetOwner();
		if (!owner)
			return;

		BFA_MechWeaponMountComponent mounts = BFA_MechWeaponMountComponent.Cast(
			owner.FindComponent(BFA_MechWeaponMountComponent)
		);

		// Set when a hardpoint could not be built yet because its aim mount / slot child
		// has not arrived. Permanent config errors (unknown module, wrong class) do NOT retry.
		bool pending = false;

		int count = GetHardpointCount();
		for (int hp = 0; hp < count; hp++)
		{
			int want = GetModuleAt(hp);

			if (m_aMountedIds[hp] == want)
				continue;

			// Tear down whatever was there before.
			if (m_aWeaponModules[hp])
			{
				m_aWeaponModules[hp].UnbindFromHardpoint();
				m_aWeaponModules[hp] = null;
				BFA_MechEvents.RaiseWeaponInstalled(owner, hp, null);
			}

			if (m_aModuleEntities[hp])
			{
				SCR_EntityHelper.DeleteEntityAndChildren(m_aModuleEntities[hp]);
				m_aModuleEntities[hp] = null;
			}

			SetAimMountHidden(hp, null, false);

			if (want == 0)
			{
				m_aMountedIds[hp] = 0;
				continue;
			}

			BFA_MechModuleDefinition module = FindModule(want);
			if (!module || !ModuleFitsHardpoint(hp, want))
			{
				BFA_MechLog.Log(string.Format("[BFA HARDPOINT] config error - module %1 unknown or does not fit hardpoint %2", want, hp), LogLevel.WARNING);
				continue;
			}

			BFA_MechHardpointDefinition hardpoint = m_aHardpoints[hp];
			if (!hardpoint)
				continue;

			// Resolve the aim mount first. If the hardpoint names one, every path below needs it,
			// so a missing entity means "not spawned yet" and we retry instead of guessing.
			IEntity aimMount = null;
			if (!hardpoint.m_sAimMountId.IsEmpty())
			{
				if (mounts)
					aimMount = mounts.GetMountEntity(hardpoint.m_sAimMountId);

				if (!aimMount)
				{
					pending = true;
					continue;
				}
			}

			// Current SampleMech2 layout: a weapon prefab already occupies the movable aim slot.
			if (hardpoint.m_bBindExistingAimMount)
			{
				if (!aimMount)
				{
					BFA_MechLog.Log(string.Format("[BFA HARDPOINT] config error - %1 binds existing aim mount but has no m_sAimMountId", hardpoint.m_sName), LogLevel.WARNING);
					continue;
				}

				BFA_MechWeaponModuleComponent existingWeaponModule =
					BFA_MechWeaponModuleComponent.Cast(
						aimMount.FindComponent(BFA_MechWeaponModuleComponent)
					);

				// Offset / angles on a bind-existing hardpoint move the weapon that sits in the
				// slot (applied to the mount's rest pose so aiming keeps working). Zero = exactly
				// where the SlotManager placed it.
				if (mounts)
				{
					vector pointModel[4];
					if (GetHardpointModelTransform(hardpoint, pointModel))
						mounts.SetMountRestModelTransform(hardpoint.m_sAimMountId, pointModel);
					else
						mounts.SetMountRestOffset(hardpoint.m_sAimMountId, hardpoint.m_vOffset, hardpoint.m_vAngles);
				}

				if (ShouldReuseExistingWeapon(module, existingWeaponModule))
				{
					existingWeaponModule.BindToHardpoint(owner, hp, hardpoint.m_sName, hardpoint.m_iClass, hardpoint.m_sControlStation, hardpoint.m_iWeaponGroup);
					// Routing will be applied as soon as the module is registered.

					m_aWeaponModules[hp] = existingWeaponModule;
					ApplyCustomGroupToModule(hp);
					m_aMountedIds[hp] = want;
					BFA_MechEvents.RaiseWeaponInstalled(owner, hp, existingWeaponModule);

					BFA_MechLog.Log(string.Format(
						"[BFA HARDPOINT] reused existing weapon hardpoint=%1 module=%2 weapon=%3 aimMount=%4",
						hardpoint.m_sName,
						module.m_sLabel,
						existingWeaponModule.GetModuleId(),
						hardpoint.m_sAimMountId
					));

					continue;
				}

				// A different module is wanted. The existing weapon stays in the slot (it carries
				// the aim pivot) but is hidden and left unbound so it cannot fire. The new module
				// is spawned below as a child of the aim mount so it inherits the arm aim.
				if (module.m_sPrefab.IsEmpty() && module.m_sRightPrefab.IsEmpty())
				{
					BFA_MechLog.Log(string.Format(
						"[BFA HARDPOINT] config error - module %1 (%2) does not match the existing weapon on %3 and has no prefab to spawn",
						want, module.m_sLabel, hardpoint.m_sName
					), LogLevel.WARNING);
					continue;
				}

				SetAimMountHidden(hp, aimMount, true);
			}

			// Spawned-module path: shoulders/back/utility, and arm replacements.
			ResourceName prefabName = GetPrefabFor(hp, module);
			if (prefabName.IsEmpty())
				continue;

			Resource prefab = Resource.Load(prefabName);
			if (!prefab || !prefab.IsValid())
			{
				BFA_MechLog.Log(string.Format("[BFA HARDPOINT] config error - could not load prefab %1", prefabName), LogLevel.WARNING);
				SetAimMountHidden(hp, null, false);
				continue;
			}

			EntitySpawnParams spawnParams = new EntitySpawnParams();
			spawnParams.TransformMode = ETransformMode.WORLD;
			owner.GetWorldTransform(spawnParams.Transform);

			IEntity moduleEntity = GetGame().SpawnEntityPrefab(prefab, owner.GetWorld(), spawnParams);
			if (!moduleEntity)
			{
				SetAimMountHidden(hp, null, false);
				continue;
			}

			IEntity attachParent = owner;
			if (aimMount)
				attachParent = aimMount;

			TNodeId attachNode = -1;
			if (!hardpoint.m_sAttachBone.IsEmpty())
			{
				Animation anim = attachParent.GetAnimation();
				if (anim)
					attachNode = anim.GetBoneIndex(hardpoint.m_sAttachBone);

				if (attachNode < 0)
				{
					attachNode = -1;
					BFA_MechLog.Log(string.Format("[BFA HARDPOINT] %1: bone '%2' not found on %3, attaching to origin", hardpoint.m_sName, hardpoint.m_sAttachBone, attachParent), LogLevel.WARNING);
				}
			}

			attachParent.AddChild(moduleEntity, attachNode);

			vector localTransform[4];
			vector hardpointModel[4];
			vector parentRestModel[4];
			bool placedByPoint = false;

			// Point placed on the mech: express it relative to the parent's REST pose, so the
			// weapon lands exactly on the gizmo and still follows the arm when it aims.
			if (attachNode < 0 && GetHardpointModelTransform(hardpoint, hardpointModel))
			{
				bool parentKnown = true;
				if (aimMount)
					parentKnown = mounts && mounts.GetMountRestModelTransform(hardpoint.m_sAimMountId, parentRestModel);
				else
					Math3D.MatrixIdentity4(parentRestModel);

				if (parentKnown)
				{
					BFA_MechWeaponMountComponent.RelativeTransform(parentRestModel, hardpointModel, localTransform);
					placedByPoint = true;
				}
			}

			if (!placedByPoint)
			{
				Math3D.AnglesToMatrix(hardpoint.m_vAngles, localTransform);
				localTransform[3] = hardpoint.m_vOffset;
			}

			moduleEntity.SetLocalTransform(localTransform);
			moduleEntity.Update();

			m_aModuleEntities[hp] = moduleEntity;
			m_aMountedIds[hp] = want;

			BFA_MechWeaponModuleComponent weaponModule =
				BFA_MechWeaponModuleComponent.Cast(
					moduleEntity.FindComponent(BFA_MechWeaponModuleComponent)
				);

			if (weaponModule)
			{
				weaponModule.BindToHardpoint(owner, hp, hardpoint.m_sName, hardpoint.m_iClass, hardpoint.m_sControlStation, hardpoint.m_iWeaponGroup);
				m_aWeaponModules[hp] = weaponModule;
					ApplyCustomGroupToModule(hp);
				BFA_MechEvents.RaiseWeaponInstalled(owner, hp, weaponModule);
			}

			BFA_MechLog.Log(string.Format(
				"[BFA HARDPOINT] spawned module hardpoint=%1 module=%2 prefab=%3 weapon=%4 replacedExisting=%5",
				hardpoint.m_sName,
				module.m_sLabel,
				prefabName,
				weaponModule != null,
				m_aHiddenAimMounts[hp] != null
			));
		}

		if (pending)
		{
			QueueRebuildRetry();
			return;
		}

		if (m_iRebuildAttempts > 0)
			BFA_MechLog.Log(string.Format("[BFA HARDPOINT] all hardpoints bound after %1 retries", m_iRebuildAttempts));

		// Loadout settled: make sure no crew station is left on an empty weapon group.
		if (Replication.IsServer())
		{
			BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(owner.FindComponent(BFA_MechCoreComponent));
			if (core)
				core.ServerRevalidateWeaponSelection();
		}
	}

	//------------------------------------------------------------------------------------------------
	//==================================================================================================
	// FIELD SWAPPING
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	bool IsHardpointSwapping(int hardpointIndex)
	{
		if (hardpointIndex < 0 || hardpointIndex >= BFA_MAX_HARDPOINTS)
			return false;

		return (m_iSwappingMask & (1 << hardpointIndex)) != 0;
	}

	//------------------------------------------------------------------------------------------------
	bool IsSwapInProgress()
	{
		return m_iSwappingMask != 0;
	}

	//------------------------------------------------------------------------------------------------
	int GetActivePresetIndex()
	{
		return m_iActivePreset;
	}

	//------------------------------------------------------------------------------------------------
	string GetPresetName(int presetIndex)
	{
		if (!m_aLoadoutPresets || presetIndex < 0 || presetIndex >= m_aLoadoutPresets.Count() || !m_aLoadoutPresets[presetIndex])
			return string.Empty;

		return m_aLoadoutPresets[presetIndex].m_sName;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSwapStateReplicated()
	{
		// Presentation hook (HUD, swap sounds, arm animation). No gameplay state here.
		if (m_iSwappingMask != 0)
			BFA_MechLog.Log(string.Format("[BFA LOADOUT] swapping mask=%1", m_iSwappingMask));
	}

	//------------------------------------------------------------------------------------------------
	//! Override in a mech-specific subclass for rearm zones, ammo trucks, etc.
	bool CanSwapNow(string stationId, out string reason)
	{
		reason = string.Empty;

		if (!m_sSwapStation.IsEmpty() && stationId != m_sSwapStation)
		{
			reason = "station not allowed";
			return false;
		}

		if (IsSwapInProgress())
		{
			reason = "swap already in progress";
			return false;
		}

		if (m_bSwapRequiresStationary)
		{
			IEntity owner = GetOwner();
			Physics rb;
			if (owner)
				rb = owner.GetPhysics();

			if (rb && rb.GetVelocity().Length() > Math.Max(m_fSwapMaxSpeed, 0.0))
			{
				reason = "mech must be stopped";
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetWordFrom(int w0, int w1, int w2, int w3, int w4, int w5, int wordIndex)
	{
		if (wordIndex == 0)
			return w0;
		if (wordIndex == 1) return w1;
		if (wordIndex == 2) return w2;
		if (wordIndex == 3) return w3;
		if (wordIndex == 4) return w4;
		return w5;
	}

	//------------------------------------------------------------------------------------------------
	protected int ModuleFromWords(int w0, int w1, int w2, int w3, int w4, int w5, int hardpointIndex)
	{
		int wordIndex = hardpointIndex / BFA_HARDPOINTS_PER_WORD;
		int shift = Math.Mod(hardpointIndex, BFA_HARDPOINTS_PER_WORD) * BFA_MODULE_BITS;
		return (GetWordFrom(w0, w1, w2, w3, w4, w5, wordIndex) >> shift) & BFA_MODULE_MASK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: switch to a named preset. presetIndex -1 = next preset after the active one.
	bool ServerRequestPreset(string stationId, int presetIndex)
	{
		if (!Replication.IsServer() || !m_aLoadoutPresets || m_aLoadoutPresets.IsEmpty())
			return false;

		if (presetIndex >= m_aLoadoutPresets.Count())
			return false;

		// Cycling: skip presets identical to what is already mounted, so every press does something.
		if (presetIndex < 0)
		{
			int count = m_aLoadoutPresets.Count();
			for (int step = 1; step <= count; step++)
			{
				int candidate = Math.Mod(m_iActivePreset + step + count, count);
				int c0, c1, c2, c3, c4, c5;
				if (!BuildPresetWords(candidate, c0, c1, c2, c3, c4, c5))
					continue;

				if (c0 != m_iLoadout0 || c1 != m_iLoadout1 || c2 != m_iLoadout2 || c3 != m_iLoadout3 || c4 != m_iLoadout4 || c5 != m_iLoadout5)
					return ServerBeginSwap(stationId, c0, c1, c2, c3, c4, c5, candidate);
			}

			return false;
		}

		int b0, b1, b2, b3, b4, b5;
		if (!BuildPresetWords(presetIndex, b0, b1, b2, b3, b4, b5))
			return false;

		return ServerBeginSwap(stationId, b0, b1, b2, b3, b4, b5, presetIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Resulting loadout words if a preset were applied on top of the current loadout.
	protected bool BuildPresetWords(int presetIndex, out int w0, out int w1, out int w2, out int w3, out int w4, out int w5)
	{
		w0 = m_iLoadout0;
		w1 = m_iLoadout1;
		w2 = m_iLoadout2;
		w3 = m_iLoadout3;
		w4 = m_iLoadout4;
		w5 = m_iLoadout5;

		if (!m_aLoadoutPresets || presetIndex < 0 || presetIndex >= m_aLoadoutPresets.Count())
			return false;

		BFA_MechLoadoutPreset preset = m_aLoadoutPresets[presetIndex];
		if (!preset)
			return false;

		if (preset.m_aSlots)
		{
			foreach (BFA_MechLoadoutSlot slot : preset.m_aSlots)
			{
				if (!slot)
					continue;

				int hp = FindHardpointIndex(slot.m_sHardpoint);
				if (hp < 0)
				{
					BFA_MechLog.Log(string.Format("[BFA LOADOUT] preset '%1' names unknown hardpoint '%2'", preset.m_sName, slot.m_sHardpoint), LogLevel.WARNING);
					continue;
				}

				int moduleId = slot.m_iModuleId;
				if (moduleId < 0 || moduleId > BFA_MODULE_MASK || (moduleId != 0 && !ModuleFitsHardpoint(hp, moduleId)))
				{
					BFA_MechLog.Log(string.Format("[BFA LOADOUT] preset '%1': module %2 does not fit %3", preset.m_sName, moduleId, slot.m_sHardpoint), LogLevel.WARNING);
					continue;
				}

				int wordIndex = hp / BFA_HARDPOINTS_PER_WORD;
				int shift = Math.Mod(hp, BFA_HARDPOINTS_PER_WORD) * BFA_MODULE_BITS;
				int mask = BFA_MODULE_MASK << shift;

				if (wordIndex == 0)
					w0 = (w0 & ~mask) | (moduleId << shift);
				else if (wordIndex == 1)
					w1 = (w1 & ~mask) | (moduleId << shift);
				else if (wordIndex == 2) w2 = (w2 & ~mask) | (moduleId << shift);
				else if (wordIndex == 3) w3 = (w3 & ~mask) | (moduleId << shift);
				else if (wordIndex == 4) w4 = (w4 & ~mask) | (moduleId << shift);
				else w5 = (w5 & ~mask) | (moduleId << shift);
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: change a single hardpoint, with the same swap time and rules as presets.
	bool ServerRequestModule(string stationId, int hardpointIndex, int moduleId)
	{
		if (!Replication.IsServer())
			return false;

		if (hardpointIndex < 0 || hardpointIndex >= GetHardpointCount())
			return false;

		if (moduleId < 0 || moduleId > BFA_MODULE_MASK || (moduleId != 0 && !ModuleFitsHardpoint(hardpointIndex, moduleId)))
			return false;

		int w0 = m_iLoadout0;
		int w1 = m_iLoadout1;
		int w2 = m_iLoadout2;
		int w3 = m_iLoadout3;
		int w4 = m_iLoadout4;
		int w5 = m_iLoadout5;

		int wordIndex = hardpointIndex / BFA_HARDPOINTS_PER_WORD;
		int shift = Math.Mod(hardpointIndex, BFA_HARDPOINTS_PER_WORD) * BFA_MODULE_BITS;
		int mask = BFA_MODULE_MASK << shift;

		if (wordIndex == 0)
			w0 = (w0 & ~mask) | (moduleId << shift);
		else if (wordIndex == 1)
			w1 = (w1 & ~mask) | (moduleId << shift);
		else if (wordIndex == 2) w2 = (w2 & ~mask) | (moduleId << shift);
		else if (wordIndex == 3) w3 = (w3 & ~mask) | (moduleId << shift);
		else if (wordIndex == 4) w4 = (w4 & ~mask) | (moduleId << shift);
		else w5 = (w5 & ~mask) | (moduleId << shift);

		return ServerBeginSwap(stationId, w0, w1, w2, w3, w4, w5, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: change several hardpoints in one swap (loadout screens, mech bays).
	//! modules[i] = module id for hardpoint i (0 = empty, -1 = leave as it is).
	bool ServerRequestLoadout(string stationId, notnull array<int> modules)
	{
		if (!Replication.IsServer())
			return false;

		int w0 = m_iLoadout0;
		int w1 = m_iLoadout1;
		int w2 = m_iLoadout2;
		int w3 = m_iLoadout3;
		int w4 = m_iLoadout4;
		int w5 = m_iLoadout5;
		int count = Math.Min(modules.Count(), GetHardpointCount());
		for (int hp = 0; hp < count; hp++)
		{
			int moduleId = modules[hp];
			if (moduleId < 0)
				continue;
			if (moduleId > BFA_MODULE_MASK || (moduleId != 0 && !ModuleFitsHardpoint(hp, moduleId)))
				return false;

			int wordIndex = hp / BFA_HARDPOINTS_PER_WORD;
			int shift = Math.Mod(hp, BFA_HARDPOINTS_PER_WORD) * BFA_MODULE_BITS;
			int mask = BFA_MODULE_MASK << shift;
			if (wordIndex == 0)
				w0 = (w0 & ~mask) | (moduleId << shift);
			else if (wordIndex == 1)
				w1 = (w1 & ~mask) | (moduleId << shift);
			else if (wordIndex == 2) w2 = (w2 & ~mask) | (moduleId << shift);
			else if (wordIndex == 3) w3 = (w3 & ~mask) | (moduleId << shift);
			else if (wordIndex == 4) w4 = (w4 & ~mask) | (moduleId << shift);
			else w5 = (w5 & ~mask) | (moduleId << shift);
		}

		return ServerBeginSwap(stationId, w0, w1, w2, w3, w4, w5, -1);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ServerBeginSwap(string stationId, int w0, int w1, int w2, int w3, int w4, int w5, int presetIndex)
	{
		string reason;
		if (!CanSwapNow(stationId, reason))
		{
			BFA_MechLog.Log(string.Format("[BFA LOADOUT] swap refused station=%1 reason=%2", stationId, reason));
			return false;
		}

		// Add-ons / gameplay mods may refuse the new loadout (too heavy, wrong hardpoint type ...).
		array<int> targets = {};
		for (int t = 0; t < GetHardpointCount(); t++)
			targets.Insert(ModuleFromWords(w0, w1, w2, w3, w4, w5, t));
		if (!BFA_MechExtension.Get().CanApplyLoadout(GetOwner(), this, targets, reason))
		{
			BFA_MechLog.Log(string.Format("[BFA LOADOUT] swap refused station=%1 reason=%2", stationId, reason));
			return false;
		}

		int swappingMask = 0;
		float swapTime = Math.Max(m_fMinSwapTime, 0.0);
		int count = GetHardpointCount();

		for (int hp = 0; hp < count; hp++)
		{
			int current = GetModuleAt(hp);
			int target = ModuleFromWords(w0, w1, w2, w3, w4, w5, hp);
			if (current == target)
				continue;

			swappingMask = swappingMask | (1 << hp);

			// Longest module swap time wins (removing a module uses the old module's time).
			BFA_MechModuleDefinition incoming = FindModule(target);
			if (incoming)
				swapTime = Math.Max(swapTime, incoming.m_fSwapTime);

			BFA_MechModuleDefinition outgoing = FindModule(current);
			if (outgoing)
				swapTime = Math.Max(swapTime, outgoing.m_fSwapTime);
		}

		if (swappingMask == 0)
		{
			// Nothing changes, but remember which preset is selected.
			if (presetIndex >= 0 && presetIndex != m_iActivePreset)
			{
				m_iActivePreset = presetIndex;
				Replication.BumpMe();
			}

			return true;
		}

		// Release triggers on hardpoints going offline so nothing keeps firing mid-swap.
		for (int i = 0; i < count; i++)
		{
			if ((swappingMask & (1 << i)) == 0 || i >= m_aWeaponModules.Count())
				continue;

			BFA_MechWeaponModuleComponent module = m_aWeaponModules[i];
			if (module && module.IsTriggerHeld())
				module.ServerTriggerUp(null);
		}

		m_iPendingLoadout0 = w0;
		m_iPendingLoadout1 = w1;
		m_iPendingLoadout2 = w2;
		m_iPendingLoadout3 = w3;
		m_iPendingLoadout4 = w4;
		m_iPendingLoadout5 = w5;
		m_iPendingPreset = presetIndex;
		m_iSwappingMask = swappingMask;
		Replication.BumpMe();

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			queue.Remove(FinishSwap);
			queue.CallLater(FinishSwap, (int)(swapTime * 1000.0), false);
		}
		else
		{
			FinishSwap();
		}

		BFA_MechLog.Log(string.Format("[BFA LOADOUT] swap started station=%1 preset=%2 mask=%3 time=%4s", stationId, GetPresetName(presetIndex), swappingMask, swapTime));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishSwap()
	{
		if (!Replication.IsServer())
			return;

		m_iLoadout0 = m_iPendingLoadout0;
		m_iLoadout1 = m_iPendingLoadout1;
		m_iLoadout2 = m_iPendingLoadout2;
		m_iLoadout3 = m_iPendingLoadout3;
		m_iLoadout4 = m_iPendingLoadout4;
		m_iLoadout5 = m_iPendingLoadout5;

		if (m_iPendingPreset >= 0)
			m_iActivePreset = m_iPendingPreset;
		else
			m_iActivePreset = -1;

		m_iSwappingMask = 0;
		m_iPendingPreset = -1;
		Replication.BumpMe();

		// The authoring machine does not receive its own onRpl callback.
		m_iRebuildAttempts = 0;
		Rebuild();

		BFA_MechLog.Log(string.Format("[BFA LOADOUT] swap finished preset=%1", GetPresetName(m_iActivePreset)));
	}

	//------------------------------------------------------------------------------------------------
	float GetPayloadTons()
	{
		float total = 0;

		for (int hp = 0; hp < GetHardpointCount(); hp++)
		{
			BFA_MechModuleDefinition module = FindModule(GetModuleAt(hp));
			if (module)
				total += Math.Max(module.m_fWeightTons, 0.0);
		}

		return total;
	}
}
