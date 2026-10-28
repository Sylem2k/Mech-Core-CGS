// CGS MechCore - repair target registry with server-side track damage and repair tests.
// Use this to bind an arbitrary number of service access points to either
// a swappable weapon hardpoint or a named structural entity.
// Uses the real BFA_MechPartHitZone health. Does not alter movement or install modules.
[BaseContainerProps()]
class CGS_MechRepairSite
{
	[Attribute("SERVICE", UIWidgets.EditBox, "Unique repair access label, such as LEFT ARM SERVICE.", category: "Repair")]
	string m_sLabel;

	[Attribute("", UIWidgets.EditBox, "For weapon/module repair: exact BFA hardpoint name. Empty for structural parts.", category: "Repair")]
	string m_sHardpoint;

	[Attribute("", UIWidgets.EditBox, "For structural repair: exact chassis slot name or child entity name. Empty for weapon hardpoints.", category: "Repair")]
	string m_sPartEntityName;

	[Attribute(desc: "Local PointInfo for this repair access location. Multiple sites may target the same part.", category: "Repair")]
	ref PointInfo m_Position;
}

[ComponentEditorProps(category: "CGS MechCore/Repair", description: "Configurable repair targets, track damage test and server-side hit-zone restoration.")]
class CGS_MechRepairComponentClass : ScriptComponentClass
{
}

class CGS_MechRepairComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object, "Repair access points: bind hardpoint name OR named structural child entity. Any number of sites.", category: "Repair")]
	protected ref array<ref CGS_MechRepairSite> m_aRepairSites;

	[Attribute("false", UIWidgets.CheckBox, "TEMPORARY TEST ONLY: allows test-damage actions on the two track slots.", category: "Track Test")]
	protected bool m_bEnableTrackDamageTest;

	[Attribute("false", UIWidgets.CheckBox, "TEMPORARY TEST ONLY: allows test-damage actions on two independently installed arm mounts.", category: "Arm Test")]
	protected bool m_bEnableArmDamageTest;

	bool ArmDamageTestEnabled()
	{
		return m_bEnableArmDamageTest;
	}

	[Attribute("5", UIWidgets.Auto, "Maximum distance in metres from the chosen repair point for on-foot service actions.", category: "Repair")]
	protected float m_fRepairReach;

	bool TrackDamageTestEnabled()
	{
		return m_bEnableTrackDamageTest;
	}

	// No alternate HP pool: repair and test damage change the same actual hit zone.
	// Never assume service points create their own actions: the prefab's
	// ActionsManager must define a UserActionContext for each physical track.
	bool CanService(IEntity user, int index)
	{
		if (!user || !ChimeraCharacter.Cast(user))
			return false;
		if (index < 0 || index >= GetRepairSiteCount())
			return false;
		IEntity owner = GetOwner();
		if (!owner || user.GetRootParent() == owner)
			return false; // mechanic must be standing outside
		if (vector.Distance(user.GetOrigin(), GetRepairSiteWorldPosition(index)) > Math.Max(1.0, m_fRepairReach))
			return false;
		Physics physics = owner.GetPhysics();
		if (physics && physics.GetVelocity().Length() > 0.5)
			return false;
		return true;
	}

	// Restrict temporary damage test to the existing two-track test frame.
	bool IsTrackSite(int index)
	{
		if (index < 0 || index >= GetRepairSiteCount() || !m_aRepairSites[index])
			return false;
		string slot = m_aRepairSites[index].m_sPartEntityName;
		return slot == "Track_L1" || slot == "Track_R1";
	}

	// SERVER ONLY: applies ordinary true-damage through the installed hit zone.
	// Keep the existing track entry point and add a SEPARATE arm test permission.
	bool ServerTestDamageTrack(int index, IEntity operator, float amount)
	{
		if (!m_bEnableTrackDamageTest || !IsTrackSite(index))
			return false;

		return ServerTestDamagePart(index, operator, amount);
	}

	bool IsArmSite(int index)
	{
		if (index < 0 || index >= GetRepairSiteCount() || !m_aRepairSites[index])
			return false;
		string slot = m_aRepairSites[index].m_sPartEntityName;
		return slot == "Arm_L" || slot == "Arm_R";
	}

	bool ServerTestDamageArm(int index, IEntity operator, float amount)
	{
		if (!m_bEnableArmDamageTest || !IsArmSite(index))
			return false;

		return ServerTestDamagePart(index, operator, amount);
	}

	protected bool ServerTestDamagePart(int index, IEntity operator, float amount)
	{
		if (!Replication.IsServer() || !CanService(operator, index))
			return false;

		IEntity target = ResolveRepairTarget(index);
		BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(target);
		if (!target || !zone || zone.GetHealth() <= 0 || amount <= 0)
			return false;

		SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(target.FindComponent(SCR_DamageManagerComponent));
		if (!manager)
			return false;

		vector empty[3];
		empty[0] = vector.Zero;
		empty[1] = vector.Zero;
		empty[2] = vector.Zero;

		float previous = zone.GetHealth();
		SCR_DamageContext damage = new SCR_DamageContext(EDamageType.TRUE, amount, empty, target, zone, Instigator.CreateInstigator(operator), null, -1, -1);
		manager.HandleDamage(damage);
		if (IsTrackSite(index))
			Print(string.Format("[CGS TRACK TEST] DAMAGE %1: %2 -> %3 / %4 HP", GetRepairSiteLabel(index), previous, zone.GetHealth(), zone.GetMaxHealth()));
		else
			Print(string.Format("[CGS ARM TEST] DAMAGE %1: %2 -> %3 / %4 HP", GetRepairSiteLabel(index), previous, zone.GetHealth(), zone.GetMaxHealth()));
		return zone.GetHealth() < previous;
	}

	// Field maintenance: wrench restores exactly 2% of the part's maximum HP
	// per completed action, but field health NEVER exceeds 25%.
	// The passed legacy amount is deliberately ignored: client-authored prefab
	// amounts must not circumvent the server's repair policy.
	bool ServerRepairPart(int index, IEntity operator, float amount)
	{
		return ServerApplyManualRepair(index, operator, 0.25, false);
	}

	// Bay maintenance: 15% max HP per wrench use, up to 100% total.
	// The MW bay server RPC MUST validate bay presence, tool, requester and
	// selected parked mech before calling this. A mechanic works from the
	// bay service console, NOT from the arm or track's physical access point.
	bool ServerRepairPartAtBay(int index, IEntity operator)
	{
		return ServerApplyManualRepair(index, operator, 1.0, true);
	}

	// Standing outside, on foot, within reach of the parked mech; the bay
	// station itself does not have to be close to the part high on the frame.
	protected bool CanServiceFromBay(IEntity operator, int index)
	{
		if (!operator || !ChimeraCharacter.Cast(operator))
			return false;
		if (index < 0 || index >= GetRepairSiteCount())
			return false;

		IEntity owner = GetOwner();
		if (!owner || operator.GetRootParent() == owner)
			return false;
		if (vector.Distance(operator.GetOrigin(), owner.GetOrigin()) > 30.0)
			return false;

		Physics physics = owner.GetPhysics();
		if (physics && physics.GetVelocity().Length() > 0.5)
			return false;

		return true;
	}

	protected bool ServerApplyManualRepair(int index, IEntity operator, float cap01, bool atBay)
	{
		if (!Replication.IsServer())
			return false;
		if (atBay)
		{
			if (!CanServiceFromBay(operator, index))
				return false;
		}
		else if (!CanService(operator, index))
			return false;

		IEntity target = ResolveRepairTarget(index);
		BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(target);
		if (!zone || zone.GetMaxHealth() <= 0)
			return false;

		float maxHealth = zone.GetMaxHealth();
		float limit = maxHealth * cap01;
		float previous = zone.GetHealth();
		if (previous >= limit - 0.001)
			return false;

		// Field = 2% of full part HP per use, capped at 25%.
		// Mech Bay = 15% per use, capped at 100%.
		float step01 = 0.02;
		if (cap01 >= 1.0)
			step01 = 0.15;
		zone.SetHealth(Math.Min(limit, previous + maxHealth * step01));
		if (IsTrackSite(index))
			Print(string.Format("[CGS TRACK TEST] REPAIR %1: %2 -> %3 / %4 HP (cap=%5%%)", GetRepairSiteLabel(index), previous, zone.GetHealth(), maxHealth, cap01 * 100));
		else
			Print(string.Format("[CGS ARM TEST] REPAIR %1: %2 -> %3 / %4 HP (cap=%5%%)", GetRepairSiteLabel(index), previous, zone.GetHealth(), maxHealth, cap01 * 100));
		return zone.GetHealth() > previous;
	}

	// Server-authorized Mech Bay crew finishes an asynchronous repair order.
	// Caller verifies parked bay, target mech, operator/job and completion time.
	// No proximity requirement: the requesting player can leave the garage while
	// the crew works, but the mech must remain inside its original bay.
	bool ServerCrewRepairPart(int index)
	{
		if (!Replication.IsServer())
			return false;

		IEntity target = ResolveRepairTarget(index);
		BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(target);
		if (!zone || zone.GetMaxHealth() <= 0)
			return false;

		float previous = zone.GetHealth();
		if (previous >= zone.GetMaxHealth())
			return false;

		zone.SetHealth(zone.GetMaxHealth());
		Print(string.Format("[CGS BAY CREW] FINISHED %1: %2 -> %3 / %4 HP", GetRepairSiteLabel(index), previous, zone.GetHealth(), zone.GetMaxHealth()));
		return zone.GetHealth() > previous;
	}

	int GetRepairSiteCount()
	{
		if (!m_aRepairSites)
			return 0;
		return m_aRepairSites.Count();
	}

	string GetRepairSiteLabel(int index)
	{
		if (index < 0 || index >= GetRepairSiteCount() || !m_aRepairSites[index])
			return "";
		return m_aRepairSites[index].m_sLabel;
	}

	vector GetRepairSiteWorldPosition(int index)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return vector.Zero;
		if (index < 0 || index >= GetRepairSiteCount())
			return owner.GetOrigin();
		CGS_MechRepairSite site = m_aRepairSites[index];
		if (!site || !site.m_Position)
			return owner.GetOrigin();
		site.m_Position.Init(owner);
		vector mat[4];
		site.m_Position.GetModelTransform(mat);
		return owner.CoordToParent(mat[3]);
	}

	// Find a child by exact authored entity name, bounded to protect against bad hierarchy loops.
	protected IEntity FindPartByName(IEntity parent, string name, int depth)
	{
		if (!parent || depth > 12 || name.IsEmpty())
			return null;
		IEntity child = parent.GetChildren();
		while (child)
		{
			if (child.GetName() == name)
				return child;
			IEntity found = FindPartByName(child, name, depth + 1);
			if (found)
				return found;
			child = child.GetSibling();
		}
		return null;
	}

	IEntity ResolveRepairTarget(int index)
	{
		if (index < 0 || index >= GetRepairSiteCount())
			return null;
		CGS_MechRepairSite site = m_aRepairSites[index];
		if (!site)
			return null;
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		if (!site.m_sHardpoint.IsEmpty())
		{
			BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(owner.FindComponent(BFA_MechHardpointComponent));
			if (!hardpoints)
				return null;
			int hp = hardpoints.FindHardpointIndex(site.m_sHardpoint);
			if (hp < 0)
				return null;
			// Re-evaluated every request: swapped modules do not leave stale targets.
			return hardpoints.GetModuleEntityAt(hp);
		}

		if (!site.m_sPartEntityName.IsEmpty())
		{
			SlotManagerComponent slots = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));
			if (slots)
			{
				EntitySlotInfo slot = slots.GetSlotByName(site.m_sPartEntityName);
				if (slot)
					return slot.GetAttachedEntity();
			}
			return FindPartByName(owner, site.m_sPartEntityName, 0);
		}
		return null;
	}

	// Read actual existing hit-zone health, not a separate repair-only counter.
	// 0..1; -1 means missing target or target has no part hit zone.
	float GetRepairTargetHealth01(int index)
	{
		IEntity target = ResolveRepairTarget(index);
		if (!target)
			return -1;
		BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(target);
		if (!zone || zone.GetMaxHealth() <= 0)
			return -1;
		return Math.Clamp(zone.GetHealth() / zone.GetMaxHealth(), 0, 1);
	}
}
