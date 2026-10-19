//------------------------------------------------------------------------------------------------
//! LEG STATUS of a mech: how many legs it has, how hurt they are and how much speed that costs.
//! Legs are parts with a BFA_MechPartHitZone set to LEG (see Mech_Leg_Base.et). Part health is
//! replicated by the game, so every machine gets the same answer. Cached for a quarter second
//! (movement asks every physics step).
//!  - Speed loss grows as a leg is damaged and is the leg's full Speed Loss once it is destroyed.
//!  - Every leg destroyed (two-legged mech: both) = IMMOBILE: it can still turn and shoot.
//!  - Bosses (Movement > Leg Damage Slows off) are never hurt by their legs.
class CGS_MechLegStatus
{
	protected static const int CACHE_MS = 250;
	protected static ref map<IEntity, ref CGS_MechLegStatus> s_mCache = new map<IEntity, ref CGS_MechLegStatus>();

	int m_iLegs;
	int m_iDamaged;        // hurt (below 100%) but still working
	int m_iDestroyed;
	float m_fSpeedLoss;    // 0..: sum over the legs
	float m_fWorstHealth = 1; // lowest leg health 0..1
	bool m_bIgnored;       // boss: legs never slow it

	protected int m_iTimeMs = -100000;

	//------------------------------------------------------------------------------------------------
	static CGS_MechLegStatus Get(IEntity mech)
	{
		if (!mech || !GetGame() || !GetGame().GetWorld())
			return null;

		int now = GetGame().GetWorld().GetWorldTime();
		CGS_MechLegStatus status;
		if (s_mCache.Find(mech, status) && status && now - status.m_iTimeMs < CACHE_MS && now >= status.m_iTimeMs)
			return status;

		if (!status)
		{
			if (s_mCache.Count() > 128)
				s_mCache.Clear(); // old mechs that were never forgotten
			status = new CGS_MechLegStatus();
			s_mCache.Set(mech, status);
		}
		status.Refresh(mech, now);
		return status;
	}

	//------------------------------------------------------------------------------------------------
	static void Forget(IEntity mech)
	{
		if (mech)
			s_mCache.Remove(mech);
	}

	//------------------------------------------------------------------------------------------------
	bool IsImmobile()
	{
		return !m_bIgnored && m_iLegs >= 1 && m_iDestroyed >= m_iLegs;
	}

	//------------------------------------------------------------------------------------------------
	bool AnyDestroyed()
	{
		return !m_bIgnored && m_iDestroyed > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Speed multiplier from the legs: 1 = healthy, 0 = immobile, never below minScale otherwise.
	float GetSpeedScale(float minScale)
	{
		if (m_bIgnored)
			return 1;
		if (IsImmobile())
			return 0;
		return Math.Clamp(1.0 - m_fSpeedLoss, minScale, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refresh(IEntity mech, int now)
	{
		m_iTimeMs = now;
		m_iLegs = 0;
		m_iDamaged = 0;
		m_iDestroyed = 0;
		m_fSpeedLoss = 0;
		m_fWorstHealth = 1;

		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
		m_bIgnored = movement && !movement.LegDamageSlows();
		if (m_bIgnored)
			return;

		AddLegs(mech, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddLegs(IEntity parent, int depth)
	{
		if (!parent || depth > 4)
			return;

		IEntity child = parent.GetChildren();
		while (child)
		{
			BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(child);
			if (zone && zone.GetPartType() == BFA_EMechPart.LEG)
			{
				m_iLegs++;
				float health = 1;
				if (zone.GetMaxHealth() > 0)
					health = Math.Clamp(zone.GetHealth() / zone.GetMaxHealth(), 0, 1);
				m_fWorstHealth = Math.Min(m_fWorstHealth, health);

				if (zone.GetDamageState() == EDamageState.DESTROYED)
				{
					m_iDestroyed++;
					m_fSpeedLoss += zone.GetSpeedLoss();
				}
				else
				{
					if (health < 0.999)
						m_iDamaged++;
					// Gradual: the more damaged the leg, the more of its Speed Loss.
					m_fSpeedLoss += zone.GetSpeedLoss() * (1.0 - health);
				}
			}

			AddLegs(child, depth + 1);
			child = child.GetSibling();
		}
	}
}
