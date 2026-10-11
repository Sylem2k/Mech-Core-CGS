//------------------------------------------------------------------------------------------------
//! CGS PART DAMAGE. Mech parts (legs, weapon pods, arms) that have an SCR_DamageManagerComponent
//! with a BFA_MechPartHitZone keep their own health:
//!  - LEG destroyed    -> the mech loses that leg's Speed Loss and cannot sprint.
//!  - WEAPON destroyed -> every weapon module on that part stops firing.
//! Repairing the part (its hit zone health) brings it straight back. Works on every machine:
//! part health is replicated by the game, so the driver's prediction matches the server.
modded class BFA_MechExtension
{
	protected static const float CGS_MIN_LEG_SPEED = 0.15;
	protected static const int CGS_LEG_CACHE_MS = 500;

	protected ref map<IEntity, float> m_mCgsLegScale = new map<IEntity, float>();
	protected ref map<IEntity, bool> m_mCgsLegLost = new map<IEntity, bool>();
	protected ref map<IEntity, int> m_mCgsLegTime = new map<IEntity, int>();

	//------------------------------------------------------------------------------------------------
	override float GetMechSpeedScale(IEntity mech)
	{
		float scale = super.GetMechSpeedScale(mech);
		CgsUpdateLegs(mech);

		float legScale;
		if (m_mCgsLegScale.Find(mech, legScale))
			scale *= legScale;
		return scale;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanMechSprint(IEntity mech)
	{
		if (!super.CanMechSprint(mech))
			return false;

		CgsUpdateLegs(mech);
		bool lost;
		if (m_mCgsLegLost.Find(mech, lost) && lost)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanWeaponEntityFire(IEntity mech, IEntity weaponEntity)
	{
		if (!super.CanWeaponEntityFire(mech, weaponEntity))
			return false;

		if (weaponEntity && BFA_MechPartHitZone.IsPartDestroyed(weaponEntity))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMechDeleted(IEntity mech)
	{
		super.OnMechDeleted(mech);
		m_mCgsLegScale.Remove(mech);
		m_mCgsLegLost.Remove(mech);
		m_mCgsLegTime.Remove(mech);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-checks the legs at most twice a second (movement asks every physics step).
	protected void CgsUpdateLegs(IEntity mech)
	{
		if (!mech || !GetGame() || !GetGame().GetWorld())
			return;

		int now = GetGame().GetWorld().GetWorldTime();
		int last;
		if (m_mCgsLegTime.Find(mech, last) && now - last < CGS_LEG_CACHE_MS && now >= last)
			return;
		m_mCgsLegTime.Set(mech, now);

		// Bosses: leg damage switched off on the mech's movement component.
		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
		if (movement && !movement.LegDamageSlows())
		{
			m_mCgsLegScale.Set(mech, 1.0);
			m_mCgsLegLost.Set(mech, false);
			return;
		}

		float loss = 0;
		bool anyLost = false;
		CgsSumLegLoss(mech, loss, anyLost, 0);

		m_mCgsLegScale.Set(mech, Math.Max(1.0 - loss, CGS_MIN_LEG_SPEED));
		m_mCgsLegLost.Set(mech, anyLost);
	}

	//------------------------------------------------------------------------------------------------
	protected void CgsSumLegLoss(IEntity parent, inout float loss, inout bool anyLost, int depth)
	{
		if (!parent || depth > 4)
			return;

		IEntity child = parent.GetChildren();
		while (child)
		{
			BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(child);
			if (zone && zone.GetPartType() == BFA_EMechPart.LEG && zone.GetDamageState() == EDamageState.DESTROYED)
			{
				loss += zone.GetSpeedLoss();
				anyLost = true;
			}

			CgsSumLegLoss(child, loss, anyLost, depth + 1);
			child = child.GetSibling();
		}
	}
}
