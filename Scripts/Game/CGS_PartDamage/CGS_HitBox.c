//------------------------------------------------------------------------------------------------
//! CGS HIT BOX - a collider that takes no damage itself and passes every hit on.
//! Put it on a small hit-box prefab (RigidBody box + SCR_DamageManagerComponent with this as the
//! only hit zone) and slot that prefab onto a bone (PivotID J1 / J2 / J3) so it follows the
//! animation. Each hit goes to the nearest PART above it (a leg / arm with BFA_MechPartHitZone),
//! or to the mech's HULL when there is none (body boxes, weak spots).
//! Damage keeps its type, so the part's / hull's own armour (multipliers) still decides how much
//! it hurts. Multiplier > 1 = weak spot, < 1 = extra armour.
class CGS_HitBoxZone : SCR_HitZone
{
	[Attribute("1", UIWidgets.Auto, "Damage multiplier before it is passed on. 3 = weak spot (triple damage), 0.5 = armoured spot.")]
	protected float m_fMultiplier;

	[Attribute("", UIWidgets.EditBox, "Name shown in the log (e.g. LEFT SHIN, REACTOR). Empty = the prefab name.")]
	protected string m_sLabel;

	[Attribute("false", UIWidgets.CheckBox, "TESTING: print every hit on this box and where it went.")]
	protected bool m_bLogHits;

	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		float damage = super.ComputeEffectiveDamage(damageContext, isDOT);
		if (damage <= 0 || !Replication.IsServer())
			return 0;

		IEntity box = GetOwner();
		if (!box)
			return 0;

		SCR_DamageManagerComponent targetManager;
		HitZone targetZone;
		IEntity target = FindTarget(box, targetManager, targetZone);
		if (!target || !targetManager || !targetZone)
		{
			if (m_bLogHits)
				Print(string.Format("[CGS HITBOX] %1 hit for %2 but found no part or hull above it", GetLabel(), damage), LogLevel.WARNING);
			return 0;
		}

		float passed = damage * Math.Max(m_fMultiplier, 0);
		if (passed <= 0)
			return 0;

		vector empty[3];
		empty[0] = vector.Zero;
		empty[1] = vector.Zero;
		empty[2] = vector.Zero;

		float before = targetZone.GetHealth();
		SCR_DamageContext context = new SCR_DamageContext(damageContext.damageType, passed, empty, target, targetZone, damageContext.instigator, null, -1, -1);
		targetManager.HandleDamage(context);

		if (m_bLogHits)
			Print(string.Format("[CGS HITBOX] %1 hit: %2 %3 x%4 -> %5 lost %6 (%7 / %8 left)", GetLabel(), damage, typename.EnumToString(EDamageType, damageContext.damageType), m_fMultiplier, target, before - targetZone.GetHealth(), targetZone.GetHealth(), targetZone.GetMaxHealth()));

		// The box itself never loses health.
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest part above the box (leg / arm), else the root's hull.
	protected IEntity FindTarget(IEntity box, out SCR_DamageManagerComponent manager, out HitZone zone)
	{
		IEntity current = box.GetParent();
		while (current)
		{
			BFA_MechPartHitZone part = BFA_MechPartHitZone.FindOn(current);
			if (part)
			{
				manager = SCR_DamageManagerComponent.Cast(current.FindComponent(SCR_DamageManagerComponent));
				zone = part;
				return current;
			}

			if (!current.GetParent())
				break; // root: use its hull below
			current = current.GetParent();
		}

		IEntity root = box.GetRootParent();
		if (!root || root == box)
			return null;

		manager = SCR_DamageManagerComponent.Cast(root.FindComponent(SCR_DamageManagerComponent));
		if (!manager)
			return null;

		zone = manager.GetDefaultHitZone();
		return root;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetLabel()
	{
		if (!m_sLabel.IsEmpty())
			return m_sLabel;

		IEntity box = GetOwner();
		if (box && box.GetPrefabData())
			return box.GetPrefabData().GetPrefabName();
		return "hitbox";
	}
}
