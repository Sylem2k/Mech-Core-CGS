// CGS MechCore - repair target registry, PHASE 1 (read-only).
// Use this to bind an arbitrary number of service access points to either
// a swappable weapon hardpoint or a named structural entity.
// Never duplicates hit-zone health, changes movement, or repairs automatically.
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

[ComponentEditorProps(category: "CGS MechCore/Repair", description: "Configurable read-only repair target registry. Does not heal until the repair actions are implemented.")]
class CGS_MechRepairComponentClass : ScriptComponentClass
{
}

class CGS_MechRepairComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object, "Repair access points: bind hardpoint name OR named structural child entity. Any number of sites.", category: "Repair")]
	protected ref array<ref CGS_MechRepairSite> m_aRepairSites;

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
