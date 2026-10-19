//------------------------------------------------------------------------------------------------
//! CGS PART DAMAGE. Mech parts (legs, weapon pods, arms) that have an SCR_DamageManagerComponent
//! with a BFA_MechPartHitZone keep their own health:
//!  - LEG damaged      -> the mech slows down bit by bit and the leg limps (BFA_MechLegComponent).
//!  - LEG destroyed    -> that leg's full Speed Loss, no sprint, no jump jets.
//!  - EVERY LEG gone   -> immobile: it can still turn, twist and shoot.
//!  - WEAPON destroyed -> every weapon module on that part stops firing.
//! Repairing the part (its hit zone health) brings it straight back. Works on every machine:
//! part health is replicated by the game, so the driver's prediction matches the server.
//! The leg numbers come from CGS_MechLegStatus (the HUD uses the same).
modded class BFA_MechExtension
{
	protected static const float CGS_MIN_LEG_SPEED = 0.15;

	//------------------------------------------------------------------------------------------------
	override float GetMechSpeedScale(IEntity mech)
	{
		float scale = super.GetMechSpeedScale(mech);
		CGS_MechLegStatus legs = CGS_MechLegStatus.Get(mech);
		if (legs)
			scale *= legs.GetSpeedScale(CGS_MIN_LEG_SPEED);
		return scale;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanMechSprint(IEntity mech)
	{
		if (!super.CanMechSprint(mech))
			return false;

		CGS_MechLegStatus legs = CGS_MechLegStatus.Get(mech);
		return !legs || !legs.AnyDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanMechJump(IEntity mech)
	{
		if (!super.CanMechJump(mech))
			return false;

		CGS_MechLegStatus legs = CGS_MechLegStatus.Get(mech);
		return !legs || !legs.AnyDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanWeaponEntityFire(IEntity mech, IEntity weaponEntity)
	{
		if (!super.CanWeaponEntityFire(mech, weaponEntity))
			return false;

		// The weapon's own part, or any part it rides on (pod on a destroyed arm stops too).
		IEntity part = weaponEntity;
		while (part && part != mech)
		{
			if (BFA_MechPartHitZone.IsPartDestroyed(part))
				return false;
			part = part.GetParent();
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMechDeleted(IEntity mech)
	{
		super.OnMechDeleted(mech);
		CGS_MechLegStatus.Forget(mech);
	}
}
