//------------------------------------------------------------------------------------------------
//! CGS MECH BOSS - makes a mech survive a whole server shooting at it.
//!  - DAMAGE CAP per second: past it, extra damage only counts Over Cap Multiplier (15%).
//!    100 players cannot melt it in seconds, but more people still helps a little.
//!    Small arms (bullets, fragments) have their OWN, smaller budget, so rifles chip in a bit
//!    and never crowd out the rockets.
//!  - Same idea per leg / arm (each part has its own budget).
//!  - SHIELD PHASES: when the hull drops below a phase (e.g. 66% and 33%), the shield comes
//!    back to full at once.
//! Server only. Put it on the boss mech next to the shield.
[ComponentEditorProps(category: "CGS MechCore/Damage", description: "Boss: damage per second caps (heavy and small arms separately) and shield phases.")]
class CGS_MechBossComponentClass : ScriptComponentClass
{
}

class CGS_MechBossComponent : ScriptComponent
{
	// ---- Hull caps ---------------------------------------------------------------------------
	[Attribute("300", UIWidgets.Auto, "Hull: heavy damage (rockets, explosives, cannons ...) that counts fully per second. 0 = no cap.", category: "Damage Cap")]
	protected float m_fHeavyPerSecond;

	[Attribute("25", UIWidgets.Auto, "Hull: small arms damage (bullets, fragments) that counts fully per second. 0 = no cap.", category: "Damage Cap")]
	protected float m_fSmallArmsPerSecond;

	[Attribute("150", UIWidgets.Auto, "Each leg / arm: heavy damage per second that counts fully. 0 = no cap.", category: "Damage Cap")]
	protected float m_fPartHeavyPerSecond;

	[Attribute("10", UIWidgets.Auto, "Each leg / arm: small arms damage per second that counts fully. 0 = no cap.", category: "Damage Cap")]
	protected float m_fPartSmallArmsPerSecond;

	[Attribute("0.15", UIWidgets.Slider, "HEAVY damage over the cap still counts this much (0 = hard cap, 1 = no cap).", "0 1 0.05", category: "Damage Cap")]
	protected float m_fOverCapMultiplier;

	[Attribute("0", UIWidgets.Slider, "SMALL ARMS damage over the cap still counts this much. 0 = hard cap: no matter how many rifles, never more than Small Arms Per Second.", "0 1 0.05", category: "Damage Cap")]
	protected float m_fSmallArmsOverCap;

	[Attribute("150", UIWidgets.Auto, "Any single hit SMALLER than this (after armour) counts as small arms, whatever its damage type. Catches modded guns (las, plasma ...) that use unusual damage types.", category: "Damage Cap")]
	protected float m_fHeavyHitMin;

	// ---- Phases ------------------------------------------------------------------------------
	[Attribute("", UIWidgets.Auto, "Hull health fractions where the shield comes back to full (e.g. 0.66 and 0.33). Each fires once.", category: "Phases")]
	protected ref array<float> m_aShieldPhases;

	// ---- Diagnostics -------------------------------------------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Print a damage summary every 10 seconds while the boss is being hit.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime (server) --------------------------------------------------------------------
	protected ref map<string, float> m_mWindowStart = new map<string, float>();
	protected ref map<string, float> m_mWindowDamage = new map<string, float>();
	protected ref array<bool> m_aPhaseDone = {};

	protected float m_fReportStart;
	protected float m_fReportIn;
	protected float m_fReportOut;

	//------------------------------------------------------------------------------------------------
	static CGS_MechBossComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return CGS_MechBossComponent.Cast(mech.FindComponent(CGS_MechBossComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSmallArms(int damageType)
	{
		return damageType == EDamageType.KINETIC || damageType == EDamageType.FRAGMENTATION || damageType == EDamageType.MELEE || damageType == EDamageType.INCENDIARY;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CountsAsSmallArms(float damage, int damageType)
	{
		return IsSmallArms(damageType) || damage < m_fHeavyHitMin;
	}

	//------------------------------------------------------------------------------------------------
	//! Soft cap in a rolling 1-second window per bucket.
	protected float Cap(string bucket, float damage, float perSecond, float overCap)
	{
		if (damage <= 0 || perSecond <= 0)
			return damage;

		float now = Now();
		float start;
		if (!m_mWindowStart.Find(bucket, start) || now - start >= 1.0)
		{
			m_mWindowStart.Set(bucket, now);
			m_mWindowDamage.Set(bucket, 0);
		}

		float used = m_mWindowDamage.Get(bucket);
		float room = Math.Max(perSecond - used, 0);
		float passed = damage;
		if (damage > room)
			passed = room + (damage - room) * Math.Clamp(overCap, 0, 1);

		m_mWindowDamage.Set(bucket, used + damage);
		return passed;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Hull damage after armour, before the shield.
	float FilterHull(HitZone zone, float damage, int damageType)
	{
		CheckPhases(zone);

		float result;
		if (CountsAsSmallArms(damage, damageType))
			result = Cap("hull_small", damage, m_fSmallArmsPerSecond, m_fSmallArmsOverCap);
		else
			result = Cap("hull_heavy", damage, m_fHeavyPerSecond, m_fOverCapMultiplier);

		Report(damage, result);
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] A leg's / arm's own damage after its armour.
	float FilterPart(IEntity part, float damage, int damageType)
	{
		if (!part)
			return damage;

		// Each part gets its own pair of buckets.
		string bucket = part.ToString();
		if (CountsAsSmallArms(damage, damageType))
			return Cap(bucket + "_small", damage, m_fPartSmallArmsPerSecond, m_fSmallArmsOverCap);
		return Cap(bucket + "_heavy", damage, m_fPartHeavyPerSecond, m_fOverCapMultiplier);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPhases(HitZone zone)
	{
		if (!zone || !m_aShieldPhases || m_aShieldPhases.IsEmpty())
			return;

		while (m_aPhaseDone.Count() < m_aShieldPhases.Count())
			m_aPhaseDone.Insert(false);

		float max = zone.GetMaxHealth();
		if (max <= 0)
			return;
		float fraction = zone.GetHealth() / max;
		if (fraction <= 0)
			return; // dead: no more phases

		for (int i = 0; i < m_aShieldPhases.Count(); i++)
		{
			if (m_aPhaseDone[i] || fraction > m_aShieldPhases[i])
				continue;

			m_aPhaseDone[i] = true;
			CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(GetOwner());
			if (shield)
				shield.ServerRestoreFull();

			Print(string.Format("[CGS BOSS] %1 hull at %2 percent - PHASE %3: shield restored", GetOwner(), Math.Round(fraction * 100), i + 1));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(float damageIn, float damageOut)
	{
		if (!m_bDebug)
			return;

		float now = Now();
		if (m_fReportStart <= 0)
			m_fReportStart = now;

		m_fReportIn += damageIn;
		m_fReportOut += damageOut;

		float span = now - m_fReportStart;
		if (span < 10)
			return;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(GetOwner().FindComponent(SCR_DamageManagerComponent));
		string hull = "?";
		if (damage && damage.GetDefaultHitZone())
			hull = string.Format("%1 / %2", Math.Round(damage.GetDefaultHitZone().GetHealth()), damage.GetDefaultHitZone().GetMaxHealth());

		Print(string.Format("[CGS BOSS] last %1 s: %2 damage in (%3/s), %4 counted after caps (%5/s) | hull %6", Math.Round(span), Math.Round(m_fReportIn), Math.Round(m_fReportIn / span), Math.Round(m_fReportOut), Math.Round(m_fReportOut / span), hull));

		m_fReportStart = now;
		m_fReportIn = 0;
		m_fReportOut = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! Hooks the boss caps into MechCore's damage filters. The cap runs BEFORE the shield, so it
//! also limits how fast the shield drains.
modded class BFA_MechExtension
{
	//------------------------------------------------------------------------------------------------
	override float FilterMechDamage(IEntity mech, HitZone zone, float damage, BaseDamageContext ctx)
	{
		CGS_MechBossComponent boss = CGS_MechBossComponent.Find(mech);
		if (boss && damage > 0)
		{
			int type = ctx.damageType;
			if (type == EDamageType.TRUE && s_iPartForwardType >= 0)
				type = s_iPartForwardType; // a part's share: judge it by the original hit
			damage = boss.FilterHull(zone, damage, type);
		}

		return super.FilterMechDamage(mech, zone, damage, ctx);
	}

	//------------------------------------------------------------------------------------------------
	override float FilterPartDamage(IEntity mech, IEntity part, float damage, BaseDamageContext ctx)
	{
		CGS_MechBossComponent boss = CGS_MechBossComponent.Find(mech);
		if (boss && damage > 0)
			damage = boss.FilterPart(part, damage, ctx.damageType);

		return super.FilterPartDamage(mech, part, damage, ctx);
	}
}
