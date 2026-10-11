//------------------------------------------------------------------------------------------------
//! CGS WEAPON DAMAGE (MY FIRST MECH add-on). Plugs into MechCore through modded class; MechCore
//! is not edited. For every mech weapon part:
//!  - AMMO TYPE: real damage type per weapon (ball, AP, HE, HEDP, HEAT, incendiary) instead of
//!    every gun dealing plain KINETIC damage.
//!  - ARMOUR PIERCING: minimum share of the damage that always gets through vehicle armour
//!    (tanks shrug off ball ammo, HEAT missiles always hurt them).
//!  - CREW DAMAGE: a share of every hit / splash on a vehicle also hurts everyone inside, and the
//!    crew dies when the weapon destroys the vehicle.

enum CGS_EMechAmmo
{
	AUTO = 0,			// from Damage Kind: EXPLOSIVE kind = HE, otherwise BALL
	BALL = 1,			// rifle / MG bullets: kinetic, no armour piercing
	ARMOR_PIERCING = 2,	// AP / heavy MG: kinetic, some armour piercing
	HE = 3,				// high explosive: explosive, little armour piercing
	HEDP = 4,			// dual purpose (M230 30mm): explosive, good armour piercing
	HEAT = 5,			// anti-tank missiles: explosive, full armour piercing
	INCENDIARY = 6,		// fire: incendiary, little armour piercing
	FRAG = 7,			// shrapnel: fragmentation, shreds infantry, nothing through armour
}

enum CGS_EMechSplash
{
	EXPLOSIVE = 0,		// blast (good against vehicles and buildings)
	FRAGMENTATION = 1,	// shrapnel (good against infantry, weak against armour)
	MIXED = 2,			// half blast, half shrapnel (real HE shells)
}

modded class BFA_MechWeaponModuleComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Ammo type: decides the damage type and how well it gets through vehicle armour. AUTO = from Damage Kind.", category: "Ammo", enums: ParamEnumArray.FromEnum(CGS_EMechAmmo))]
	protected CGS_EMechAmmo m_eCgsAmmo;

	[Attribute("-1", UIWidgets.Auto, "Armour piercing 0-1: share of the damage that always gets through vehicle armour (1 = full damage to tanks). -1 = from Ammo Type.", category: "Ammo")]
	protected float m_fCgsArmorPiercing;

	[Attribute("0", UIWidgets.ComboBox, "Splash damage type (guns / cannons with Splash Damage). EXPLOSIVE = blast, FRAGMENTATION = shrapnel vs infantry, MIXED = half of each.", category: "Ammo", enums: ParamEnumArray.FromEnum(CGS_EMechSplash))]
	protected CGS_EMechSplash m_eCgsSplash;

	[Attribute("0.15", UIWidgets.Slider, "Share of each bullet / kinetic hit on a vehicle that also hurts every person inside. 0 = crew safe until the vehicle dies.", "0 1 0.05", category: "Crew Damage")]
	protected float m_fCrewShareKinetic;

	[Attribute("0.4", UIWidgets.Slider, "Share of each EXPLOSIVE / INCENDIARY hit or splash that hurts every person inside.", "0 1 0.05", category: "Crew Damage")]
	protected float m_fCrewShareExplosive;

	[Attribute("true", UIWidgets.CheckBox, "Everyone inside dies when this weapon destroys the vehicle.", category: "Crew Damage")]
	protected bool m_bKillCrewOnDestroy;

	protected bool m_bCgsInside;

	//------------------------------------------------------------------------------------------------
	protected CGS_EMechAmmo CgsAmmo()
	{
		if (m_eCgsAmmo != CGS_EMechAmmo.AUTO)
			return m_eCgsAmmo;
		if (m_eDamageKind == BFA_EMechDamageKind.EXPLOSIVE)
			return CGS_EMechAmmo.HE;
		return CGS_EMechAmmo.BALL;
	}

	//------------------------------------------------------------------------------------------------
	//! Damage type for a hit. Splash (already EXPLOSIVE) and TRUE damage stay as they are.
	protected EDamageType CgsDamageType(EDamageType requested)
	{
		if (requested == EDamageType.TRUE || requested == EDamageType.EXPLOSIVE)
			return requested;

		CGS_EMechAmmo ammo = CgsAmmo();
		if (ammo == CGS_EMechAmmo.HE || ammo == CGS_EMechAmmo.HEDP || ammo == CGS_EMechAmmo.HEAT)
			return EDamageType.EXPLOSIVE;
		if (ammo == CGS_EMechAmmo.INCENDIARY)
			return EDamageType.INCENDIARY;
		if (ammo == CGS_EMechAmmo.FRAG)
			return EDamageType.FRAGMENTATION;
		return EDamageType.KINETIC;
	}

	//------------------------------------------------------------------------------------------------
	protected float CgsArmorPiercing()
	{
		if (m_fCgsArmorPiercing >= 0)
			return Math.Min(m_fCgsArmorPiercing, 1.0);

		CGS_EMechAmmo ammo = CgsAmmo();
		if (ammo == CGS_EMechAmmo.ARMOR_PIERCING)
			return 0.35;
		if (ammo == CGS_EMechAmmo.HE || ammo == CGS_EMechAmmo.INCENDIARY)
			return 0.15;
		if (ammo == CGS_EMechAmmo.HEDP)
			return 0.5;
		if (ammo == CGS_EMechAmmo.HEAT)
			return 1.0;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	override protected float DealDamage(IEntity hitEntity, float amount, EDamageType type, vector hitPos, vector direction, IEntity instigator)
	{
		if (m_bCgsInside)
			return super.DealDamage(hitEntity, amount, type, hitPos, direction, instigator);

		// Splash from guns / cannons arrives as EXPLOSIVE: apply the chosen splash type.
		if (type == EDamageType.EXPLOSIVE && m_eCgsSplash != CGS_EMechSplash.EXPLOSIVE)
		{
			float splashLost;
			if (m_eCgsSplash == CGS_EMechSplash.FRAGMENTATION)
			{
				splashLost = super.DealDamage(hitEntity, amount, EDamageType.FRAGMENTATION, hitPos, direction, instigator);
				return splashLost + CgsAfterHit(hitEntity, amount, splashLost, EDamageType.FRAGMENTATION, hitPos, direction, instigator);
			}

			splashLost = super.DealDamage(hitEntity, amount * 0.5, EDamageType.EXPLOSIVE, hitPos, direction, instigator);
			splashLost += super.DealDamage(hitEntity, amount * 0.5, EDamageType.FRAGMENTATION, hitPos, direction, instigator);
			return splashLost + CgsAfterHit(hitEntity, amount, splashLost, EDamageType.EXPLOSIVE, hitPos, direction, instigator);
		}

		EDamageType useType = CgsDamageType(type);
		float lost = super.DealDamage(hitEntity, amount, useType, hitPos, direction, instigator);
		return lost + CgsAfterHit(hitEntity, amount, lost, useType, hitPos, direction, instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! After MechCore's hit on a vehicle: armour piercing top-up, then crew damage.
	//! Returns the extra hull health removed.
	protected float CgsAfterHit(IEntity hitEntity, float amount, float lost, EDamageType type, vector hitPos, vector direction, IEntity instigator)
	{
		if (m_bCgsInside || !Replication.IsServer() || !hitEntity || amount <= 0)
			return 0;

		IEntity vehicle = hitEntity.GetRootParent();
		if (!vehicle)
			vehicle = hitEntity;

		// Soldiers (on foot or hit directly in a seat) are handled normally; never our own mech.
		if (ChimeraCharacter.Cast(hitEntity) || ChimeraCharacter.Cast(vehicle) || vehicle == m_pMechOwner)
			return 0;

		m_bCgsInside = true;

		// Armour piercing: make sure at least this share of the damage reaches the hull.
		float extra = 0;
		float wanted = amount * CgsArmorPiercing();
		if (wanted > lost + 0.01)
			extra = DealDamage(vehicle, wanted - lost, EDamageType.TRUE, hitPos, direction, instigator);

		// Crew.
		array<IEntity> crew = {};
		CgsCollectCrew(vehicle, crew, 0);
		if (!crew.IsEmpty())
		{
			bool destroyed;
			SCR_DamageManagerComponent vehicleDamage = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
			if (vehicleDamage && vehicleDamage.GetState() == EDamageState.DESTROYED)
				destroyed = true;

			float share = m_fCrewShareKinetic;
			if (type == EDamageType.EXPLOSIVE || type == EDamageType.INCENDIARY)
				share = m_fCrewShareExplosive;

			foreach (IEntity person : crew)
			{
				if (destroyed && m_bKillCrewOnDestroy)
					DealDamage(person, 100000, EDamageType.TRUE, hitPos, direction, instigator);
				else if (share > 0)
					DealDamage(person, amount * share, type, hitPos, direction, instigator);
			}
		}

		m_bCgsInside = false;
		return extra;
	}

	//------------------------------------------------------------------------------------------------
	//! Everyone sitting in the vehicle (seated characters are children of the vehicle or its turrets).
	protected void CgsCollectCrew(IEntity parent, notnull array<IEntity> crew, int depth)
	{
		if (!parent || depth > 6)
			return;

		IEntity child = parent.GetChildren();
		while (child)
		{
			if (ChimeraCharacter.Cast(child))
			{
				// The hidden mech pilot only dies with its mech (handled by the brain).
				if (!CGS_HiddenPilotComponent.IsHiddenPilot(child))
					crew.Insert(child);
			}
			else
			{
				CgsCollectCrew(child, crew, depth + 1);
			}
			child = child.GetSibling();
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Rockets and missiles deal their damage through their own function; give them the same
//! armour piercing and crew damage.
modded class BFA_MechRocketLauncherModuleComponent
{
	//------------------------------------------------------------------------------------------------
	override protected float ApplyExplosiveDamage(SCR_DamageManagerComponent damageManager, float amount, vector position, vector direction, IEntity instigatorEntity, IEntity mechOwner)
	{
		float lost = super.ApplyExplosiveDamage(damageManager, amount, position, direction, instigatorEntity, mechOwner);
		if (!damageManager || m_bCgsInside)
			return lost;

		return lost + CgsAfterHit(damageManager.GetOwner(), amount, lost, EDamageType.EXPLOSIVE, position, direction, instigatorEntity);
	}
}
