//------------------------------------------------------------------------------------------------
//! CGS SHIELD ADD-ON - hooks into MechCore through its extension point. No MechCore file is
//! edited; `modded class` adds this behaviour on top (other add-ons can do the same: we always
//! call super first).
modded class BFA_MechExtension
{
	//------------------------------------------------------------------------------------------------
	override float FilterMechDamage(IEntity mech, HitZone zone, float damage, BaseDamageContext ctx)
	{
		damage = super.FilterMechDamage(mech, zone, damage, ctx);

		CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(mech);
		if (!shield)
			return damage;

		return shield.Absorb(damage, s_iCurrentDamageKind, s_pCurrentShooterMech);
	}

	//------------------------------------------------------------------------------------------------
	//! Parts (legs, arms) are inside the bubble too: while the shield is up they only lose the
	//! Bleed Through share (0 = untouchable until the shield is down). The hull share of the same
	//! hit already drains the shield.
	override float FilterPartDamage(IEntity mech, IEntity part, float damage, BaseDamageContext ctx)
	{
		damage = super.FilterPartDamage(mech, part, damage, ctx);

		CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(mech);
		if (!shield || !shield.IsUp())
			return damage;

		return damage * Math.Clamp(shield.GetBleedThrough(), 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	override float InterceptRay(IEntity shooterMech, vector start, vector direction, float range, out IEntity blockingEntity)
	{
		float best = super.InterceptRay(shooterMech, start, direction, range, blockingEntity);

		array<IEntity> mechs = {};
		CGS_MechShieldComponent.GetShieldedMechs(mechs);
		foreach (IEntity mech : mechs)
		{
			if (mech == shooterMech)
				continue; // own guns fire out through their own bubble

			CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(mech);
			if (!shield)
				continue;

			float t = shield.IntersectRay(start, direction, range);
			if (t < 0)
				continue;

			if (best < 0 || t < best)
			{
				best = t;
				blockingEntity = mech;
			}
		}

		return best;
	}
}

//------------------------------------------------------------------------------------------------
//! Adds "SHIELD xx%" (or "SHIELD DOWN") to MechCore's built-in cockpit HUD status line.
modded class BFA_MechHUDDisplay
{
	//------------------------------------------------------------------------------------------------
	override void Update(IEntity mech, string station)
	{
		super.Update(mech, station);

		if (!m_wStatusText || !mech)
			return;

		CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(mech);
		if (!shield)
			return;

		string shieldText;
		if (shield.IsUp())
			shieldText = string.Format("SHIELD %1", Math.Round(shield.GetShield01() * 100)) + "%";
		else
			shieldText = "SHIELD DOWN";

		string current = m_wStatusText.GetText();
		if (current.StartsWith("SHIELD"))
		{
			int cut = current.IndexOf("   ");
			if (cut < 0)
				current = string.Empty;
			else
				current = current.Substring(cut + 3, current.Length() - cut - 3);
		}
		if (current.IsEmpty())
			m_wStatusText.SetText(shieldText);
		else
			m_wStatusText.SetText(shieldText + "   " + current);
	}
}
