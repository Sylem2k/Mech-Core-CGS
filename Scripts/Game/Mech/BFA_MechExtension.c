//------------------------------------------------------------------------------------------------
//! What kind of damage a MechCore weapon deals (add-ons such as shields read this).
enum BFA_EMechDamageKind
{
	KINETIC = 0,
	EXPLOSIVE = 1,
	ENERGY = 2,
	EMP = 3,
}

//------------------------------------------------------------------------------------------------
//! EXTENSION POINT FOR ADD-ONS. MechCore calls these empty methods at key moments; an add-on
//! changes them from outside with `modded class BFA_MechExtension` and overrides only what it
//! needs (always call super). Without add-ons the defaults below run and nothing changes.
//!
//!   modded class BFA_MechExtension
//!   {
//!       override float FilterMechDamage(IEntity mech, HitZone zone, float damage, BaseDamageContext ctx)
//!       {
//!           damage = super.FilterMechDamage(mech, zone, damage, ctx);
//!           return MyShield_Absorb(mech, damage, ctx);
//!       }
//!   }
//!
//! `modded class` replaces the class everywhere, including the `new` in Get(), so this plain
//! singleton is all an add-on needs. Extra replicated state: add [RplProp] fields to an existing
//! MechCore component (e.g. BFA_MechCoreComponent) with `modded class` - no prefab edits needed.
//! Pattern credit: M.A.W.S. Sci-Fi Expansion Pack documentation (example code, free to use).
class BFA_MechExtension
{
	protected static ref BFA_MechExtension s_Instance;

	// Set by MechCore weapons while they apply damage (server), so a damage filter knows what
	// hit the mech. -1 = not a MechCore weapon (vanilla RPG, tank, mine ...).
	static int s_iCurrentDamageKind = -1;
	static IEntity s_pCurrentShooterMech;
	//! While a part passes its share of a hit on to the hull: the ORIGINAL damage type of that
	//! hit (the hull receives it as TRUE damage). -1 = not forwarding.
	static int s_iPartForwardType = -1;

	//------------------------------------------------------------------------------------------------
	static BFA_MechExtension Get()
	{
		if (!s_Instance)
			s_Instance = new BFA_MechExtension();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Damage that reaches a mech's hull AFTER armour. Return what is left.
	float FilterMechDamage(IEntity mech, HitZone zone, float damage, BaseDamageContext ctx)
	{
		return damage;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Damage a PART (leg, arm, pod) is about to lose from its own health, after its
	//! armour. Return what is left (e.g. a shield protects the parts too).
	float FilterPartDamage(IEntity mech, IEntity part, float damage, BaseDamageContext ctx)
	{
		return damage;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Hitscan beams ask this before they hit anything. Return the distance along the
	//! ray where something (e.g. an enemy shield bubble) stops the beam and set blockingEntity to
	//! the entity that takes the hit; return -1 for no interception.
	float InterceptRay(IEntity shooterMech, vector start, vector direction, float range, out IEntity blockingEntity)
	{
		blockingEntity = null;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] May the weapon on this entity fire right now? (heat, destroyed parts ...)
	//! Called for BOTH weapon modules and the older weapon definitions (BFA_MechWeaponComponent).
	bool CanWeaponEntityFire(IEntity mech, IEntity weaponEntity)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] A weapon just fired one shot / rocket and adds this much heat.
	void OnWeaponFiredHeat(IEntity mech, IEntity weaponEntity, float heat, string weaponName)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Weapon modules call these; they forward to the two hooks above.
	bool CanModuleFire(IEntity mech, BFA_MechWeaponModuleComponent module)
	{
		if (!module)
			return true;
		return CanWeaponEntityFire(mech, module.GetOwner());
	}

	void OnModuleFired(IEntity mech, BFA_MechWeaponModuleComponent module)
	{
		if (module)
			OnWeaponFiredHeat(mech, module.GetOwner(), module.GetHeatPerShot(), module.GetModuleId());
	}

	//------------------------------------------------------------------------------------------------
	//! [All machines, every physics step] Multiplier on the mech's walk / sprint / strafe speed.
	//! Must give the same answer on the server and the driver's machine (use replicated state).
	float GetMechSpeedScale(IEntity mech)
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! [All machines, every physics step] May the mech sprint right now?
	bool CanMechSprint(IEntity mech)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] May the mech use its jump jets right now? (a crippled leg says no)
	bool CanMechJump(IEntity mech)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server, every heat tick] Extra heat lost per second on top of the heat component's own
	//! Cooling Per Second (gameplay mods: heatsinks). Default 0.
	float GetExtraCooling(IEntity mech)
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] A loadout change is about to start (field swap, preset, mech bay ...).
	//! targetModules[i] = module id hardpoint i will carry (0 = empty); look modules up with
	//! hardpoints.GetModuleDefinition(id) and hardpoint names with hardpoints.GetHardpointName(i).
	//! Return false and set reason to refuse it (gameplay mods: tonnage, hardpoint types ...).
	bool CanApplyLoadout(IEntity mech, BFA_MechHardpointComponent hardpoints, notnull array<int> targetModules, out string reason)
	{
		reason = string.Empty;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! [All machines] A mech finished spawning / is being deleted. Spawn local visuals, register it.
	void OnMechInit(IEntity mech)
	{
	}

	void OnMechDeleted(IEntity mech)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! Mech hull hit zone: identical to the vanilla flammable hull, plus the add-on damage filter.
class BFA_MechHitZone : SCR_FlammableHitZone
{
	[Attribute("0", UIWidgets.CheckBox, "TESTING: print every hit on this hull to the log (damage after armour, after shield, health left).")]
	protected bool m_bLogDamage;

	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		float damage = super.ComputeEffectiveDamage(damageContext, isDOT);
		if (!Replication.IsServer())
			return damage;

		if (damage <= 0)
		{
			if (m_bLogDamage && !isDOT)
				Print(string.Format("[BFA DAMAGE] %1: hit stopped by armour (type %2)", GetOwner(), typename.EnumToString(EDamageType, damageContext.damageType)));
			return damage;
		}

		float afterFilter = BFA_MechExtension.Get().FilterMechDamage(GetOwner(), this, damage, damageContext);

		if (m_bLogDamage && !isDOT)
			Print(string.Format("[BFA DAMAGE] %1: %2 damage after armour, %3 after shield, hull %4 / %5", GetOwner(), damage, afterFilter, GetHealth() - afterFilter, GetMaxHealth()));

		return afterFilter;
	}
}

//------------------------------------------------------------------------------------------------
//! Hit zone for mech PARTS that are separate entities (legs, arms, weapon pods). Put an
//! SCR_DamageManagerComponent on the part with this as its only (default) hit zone.
//! The part loses its own health (MaxHealth) AND passes Hull Share of each hit to the mech's
//! hull (still filtered by the shield). Destroyed LEG = slower, no sprint; destroyed WEAPON =
//! that weapon stops firing. Repairing the part brings it back.
enum BFA_EMechPart
{
	ARMOR = 0,	// plating / decoration: only passes damage to the hull
	LEG = 1,	// damaged = slower + limps; destroyed = no sprint / jump; all destroyed = immobile
	WEAPON = 2,	// destroyed = the weapon on this part stops firing
}

class BFA_MechPartHitZone : SCR_HitZone
{
	[Attribute("1", UIWidgets.ComboBox, "What this part is. LEG: destroyed = mech slows down. WEAPON: destroyed = its weapon stops firing. ARMOR: just passes damage to the hull.", enums: ParamEnumArray.FromEnum(BFA_EMechPart))]
	protected BFA_EMechPart m_ePart;

	[Attribute("1", UIWidgets.Auto, "Share of the damage (after this part's multipliers) that reaches the mech's hull. 0.5 = legs are half as vulnerable as the body.")]
	protected float m_fHullShare;

	[Attribute("0.4", UIWidgets.Slider, "LEG only: share of the mech's speed this leg costs when destroyed. It is lost gradually as the leg takes damage (half health = half of it). Losses of several legs add up; the mech never drops below 15% unless every leg is destroyed (then it cannot walk).", "0 1 0.05")]
	protected float m_fSpeedLoss;

	[Attribute("", UIWidgets.EditBox, "Name of this part (e.g. LEFT LEG, RIGHT ARM). Also helps AUTO pick its place on the HUD damage picture. Empty = the part's prefab name.")]
	protected string m_sDisplayName;

	// Temporary per-part projectile diagnostic. Disabled by default on every other mech.
	[Attribute("false", UIWidgets.CheckBox, "TEST ONLY: log damage events reaching this part's own hit zone.")]
	protected bool m_bLogHits;

	[Attribute("true", UIWidgets.CheckBox, "Show this part on the HUD's damage picture.")]
	protected bool m_bShowOnHUD;

	[Attribute("0", UIWidgets.ComboBox, "Which piece of the HUD's damage picture this part colours. AUTO = guessed from the part type and its name (e.g. LEG + 'Left' = left leg, weapon/arm + 'R' = right arm).", enums: ParamEnumArray.FromEnum(CGS_EMechHudSection))]
	protected CGS_EMechHudSection m_eHudSection;

	//------------------------------------------------------------------------------------------------
	float GetSpeedLoss()
	{
		return m_fSpeedLoss;
	}

	//------------------------------------------------------------------------------------------------
	string GetDisplayName()
	{
		return m_sDisplayName;
	}

	//------------------------------------------------------------------------------------------------
	bool ShowOnHUD()
	{
		return m_bShowOnHUD;
	}

	//------------------------------------------------------------------------------------------------
	CGS_EMechHudSection GetHudSection()
	{
		return m_eHudSection;
	}

	//------------------------------------------------------------------------------------------------
	BFA_EMechPart GetPartType()
	{
		return m_ePart;
	}

	//------------------------------------------------------------------------------------------------
	//! The part's own hit zone on an entity (null if it has none).
	static BFA_MechPartHitZone FindOn(IEntity part)
	{
		if (!part)
			return null;
		SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(part.FindComponent(SCR_DamageManagerComponent));
		if (!manager)
			return null;
		return BFA_MechPartHitZone.Cast(manager.GetDefaultHitZone());
	}

	//------------------------------------------------------------------------------------------------
	//! Destroyed = its health is gone (repairs bring it back).
	static bool IsPartDestroyed(IEntity part)
	{
		BFA_MechPartHitZone zone = FindOn(part);
		return zone && zone.GetDamageState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		float damage = super.ComputeEffectiveDamage(damageContext, isDOT);
		if (m_bLogHits && Replication.IsServer() && !isDOT)
			Print(string.Format("[CGS TRACK HIT] %1: type=%2 effective=%3 healthBefore=%4/%5", m_sDisplayName, typename.EnumToString(EDamageType, damageContext.damageType), damage, GetHealth(), GetMaxHealth()));
		if (damage <= 0 || !Replication.IsServer())
			return damage;

		IEntity part = GetOwner();
		if (!part || m_fHullShare <= 0)
			return damage;

		IEntity mech = part.GetRootParent();
		if (!mech || mech == part)
			return damage;

		SCR_DamageManagerComponent hullManager = SCR_DamageManagerComponent.Cast(mech.FindComponent(SCR_DamageManagerComponent));
		if (!hullManager)
			return damage;

		HitZone hull = hullManager.GetDefaultHitZone();
		if (!hull)
			return damage;

		vector empty[3];
		empty[0] = vector.Zero;
		empty[1] = vector.Zero;
		empty[2] = vector.Zero;

		// TRUE: the part's multipliers already acted as its armour; the shield still filters it.
		SCR_DamageContext context = new SCR_DamageContext(EDamageType.TRUE, damage * m_fHullShare, empty, mech, hull, damageContext.instigator, null, -1, -1);
		int previousForward = BFA_MechExtension.s_iPartForwardType;
		BFA_MechExtension.s_iPartForwardType = damageContext.damageType;
		hullManager.HandleDamage(context);
		BFA_MechExtension.s_iPartForwardType = previousForward;

		// The part loses its own health too (MaxHealth on this hit zone). At 0 it is destroyed:
		// LEG = slower mech, WEAPON = that weapon stops. Repairs restore it.
		// Add-ons may protect it (shield up = the part is not hurt).
		return BFA_MechExtension.Get().FilterPartDamage(mech, part, damage, damageContext);
	}
}
