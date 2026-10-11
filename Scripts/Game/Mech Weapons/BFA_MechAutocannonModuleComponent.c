//------------------------------------------------------------------------------------------------
//! AUTOCANNON - and the template for making your own weapon.
//!
//! This is ALL a weapon needs. Copy this file, rename the two classes, change FireShot().
//! The base class already handles: trigger (held auto-fire / single shot), fire rate
//! (m_fShotsPerMinute), magazine + reload + spare mags, barrels, muzzle position,
//! muzzle flash / sound / visible rounds / impacts for every player (Effects), crew stations,
//! weapon groups, aiming, AI gunners and loadout swapping.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted/Mech/Weapons", description: "Mech autocannon: fast hitscan shells with spread and optional HE splash. Also the template for custom weapons.")]
class BFA_MechAutocannonModuleComponentClass : BFA_MechWeaponModuleComponentClass
{
}

class BFA_MechAutocannonModuleComponent : BFA_MechWeaponModuleComponent
{
	// Your weapon's own settings. They show up in the prefab like any other property.
	[Attribute("1200", UIWidgets.Auto, "Range in metres.", category: "Autocannon")]
	protected float m_fRange;

	[Attribute("60", UIWidgets.Auto, "Damage per hit.", category: "Autocannon")]
	protected float m_fDamage;

	[Attribute("0.4", UIWidgets.Auto, "Spread in degrees.", category: "Autocannon")]
	protected float m_fSpread;

	[Attribute("0", UIWidgets.Auto, "Explosive splash damage per shell (0 = none).", category: "Autocannon")]
	protected float m_fSplashDamage;

	[Attribute("2", UIWidgets.Auto, "Splash radius in metres.", category: "Autocannon")]
	protected float m_fSplashRadius;

	[Attribute("900", UIWidgets.Auto, "How fast the visible shell flies (m/s). Visual only.", category: "Autocannon")]
	protected float m_fShellSpeed;

	//------------------------------------------------------------------------------------------------
	//! One shot. Runs on the server after fire rate and ammo were already checked.
	override protected void FireShot(IEntity instigator)
	{
		// 1. Where and which way.
		vector direction;
		vector start = GetMuzzle(direction);
		direction = ApplyConeSpread(direction, m_fSpread);

		// 2. What it hits.
		IEntity hitEntity;
		vector end = TraceShot(start, direction, m_fRange, hitEntity);

		// 3. Damage.
		if (hitEntity)
			DealDamage(hitEntity, m_fDamage, EDamageType.KINETIC, end, direction, instigator);

		if (m_fSplashDamage > 0)
		{
			IEntity alreadyHit;
			if (hitEntity)
				alreadyHit = hitEntity.GetRootParent();

			DealSplashDamage(end, m_fSplashDamage, m_fSplashRadius, EDamageType.EXPLOSIVE, instigator, alreadyHit);
		}

		// 4. Everyone sees and hears it (muzzle flash, sound, shell, impact from the Effects).
		BroadcastShot(start, end, hitEntity != null, m_fShellSpeed);
	}

	//------------------------------------------------------------------------------------------------
	override float GetAimRange()
	{
		return m_fRange;
	}
}
