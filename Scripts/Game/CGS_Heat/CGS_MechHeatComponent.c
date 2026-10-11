//------------------------------------------------------------------------------------------------
//! CGS HEAT. Add to a mech to give it a heat gauge:
//!  - every shot adds the weapon part's Heat Per Shot (Heat category on the weapon module),
//!  - sprinting adds heat every second, the mech cools down all the time,
//!  - at Max Heat the mech OVERHEATS: weapons lock, it slows down and cannot sprint, until it has
//!    cooled to Recover At. Optional full shutdown instead of slowing.
//! The cockpit HUD shows HEAT xx% / OVERHEAT. Server decides, every machine sees the result.
[ComponentEditorProps(category: "CGS MechCore", description: "Heat: shots and sprinting heat the mech, overheating locks weapons and slows it.")]
class CGS_MechHeatComponentClass : ScriptComponentClass
{
}

class CGS_MechHeatComponent : ScriptComponent
{
	[Attribute("100", UIWidgets.Auto, "Heat the mech can hold. Reaching it = OVERHEAT.", category: "Heat")]
	protected float m_fMaxHeat;

	[Attribute("12", UIWidgets.Auto, "Heat lost per second (always cooling).", category: "Heat")]
	protected float m_fCoolingPerSecond;

	[Attribute("6", UIWidgets.Auto, "Heat added per second while sprinting.", category: "Heat")]
	protected float m_fSprintHeatPerSecond;

	[Attribute("0.4", UIWidgets.Slider, "After an overheat the mech recovers once heat drops to this fraction of Max Heat.", "0 1 0.05", category: "Heat")]
	protected float m_fRecoverAt;

	[Attribute("1.5", UIWidgets.Auto, "Cooling is multiplied by this while overheated (vents open).", category: "Heat")]
	protected float m_fOverheatCoolingMultiplier;

	[Attribute("0.5", UIWidgets.Slider, "Speed while overheated (1 = full speed).", "0 1 0.05", category: "Overheat")]
	protected float m_fOverheatSpeed;

	[Attribute("false", UIWidgets.CheckBox, "SHUTDOWN: overheating stops the mech completely (speed 0) until it recovers.", category: "Overheat")]
	protected bool m_bShutdown;

	[Attribute("true", UIWidgets.CheckBox, "Overheating locks all weapons until the mech recovers.", category: "Overheat")]
	protected bool m_bLockWeapons;

	[Attribute("true", UIWidgets.CheckBox, "TESTING: print every shot's heat to the log.", category: "Heat")]
	protected bool m_bDebug;

	// ---- Replicated ----------------------------------------------------------------------------
	[RplProp()]
	protected float m_fHeat;

	[RplProp()]
	protected bool m_bOverheated;

	protected float m_fLastRepHeat;
	protected int m_iLastLoggedQuarter;
	protected BFA_MechMovementComponent m_pMovement;

	//------------------------------------------------------------------------------------------------
	static CGS_MechHeatComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return CGS_MechHeatComponent.Cast(mech.FindComponent(CGS_MechHeatComponent));
	}

	//------------------------------------------------------------------------------------------------
	float GetHeat01()
	{
		if (m_fMaxHeat <= 0)
			return 0;
		return Math.Clamp(m_fHeat / m_fMaxHeat, 0, 1);
	}

	bool IsOverheated()
	{
		return m_bOverheated;
	}

	bool LocksWeapons()
	{
		return m_bOverheated && m_bLockWeapons;
	}

	float GetSpeedScale()
	{
		if (!m_bOverheated)
			return 1;
		if (m_bShutdown)
			return 0;
		return m_fOverheatSpeed;
	}

	//------------------------------------------------------------------------------------------------
	void DebugShot(string weaponName, float amount)
	{
		if (m_bDebug)
			Print(string.Format("[CGS HEAT] %1 fired: +%2 heat (now %3 / %4)", weaponName, amount, Math.Round(m_fHeat + amount), m_fMaxHeat));
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Add heat (a shot, a jump-jet burst ...).
	void AddHeat(float amount)
	{
		if (!Replication.IsServer() || amount <= 0 || m_fMaxHeat <= 0)
			return;

		m_fHeat = Math.Min(m_fHeat + amount, m_fMaxHeat);

		int quarter = Math.Floor(GetHeat01() * 4);
		if (quarter > m_iLastLoggedQuarter)
			Print(string.Format("[CGS HEAT] %1: heat %2 percent", GetOwner(), quarter * 25));
		m_iLastLoggedQuarter = quarter;
		if (!m_bOverheated && m_fHeat >= m_fMaxHeat)
		{
			m_bOverheated = true;
			Print(string.Format("[CGS HEAT] %1: OVERHEAT", GetOwner()));
		}
		CgsReplicate(false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_pMovement = BFA_MechMovementComponent.Cast(owner.FindComponent(BFA_MechMovementComponent));

		if (Replication.IsServer())
		{
			SetEventMask(owner, EntityEvent.FRAME);
			Print(string.Format("[CGS HEAT] %1: heat gauge on (max %2, cooling %3/s)", owner, m_fMaxHeat, m_fCoolingPerSecond));
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_fMaxHeat <= 0)
			return;

		if (m_pMovement && m_pMovement.IsSprinting() && !m_bOverheated)
			AddHeat(m_fSprintHeatPerSecond * timeSlice);

		if (m_fHeat > 0)
		{
			float cooling = m_fCoolingPerSecond;
			if (m_bOverheated)
				cooling *= Math.Max(m_fOverheatCoolingMultiplier, 0);
			m_fHeat = Math.Max(m_fHeat - cooling * timeSlice, 0);
			m_iLastLoggedQuarter = Math.Min(m_iLastLoggedQuarter, Math.Floor(GetHeat01() * 4));
		}

		if (m_bOverheated && m_fHeat <= m_fMaxHeat * Math.Clamp(m_fRecoverAt, 0, 1))
		{
			m_bOverheated = false;
			Print(string.Format("[CGS HEAT] %1: recovered", owner));
			CgsReplicate(true);
			return;
		}

		CgsReplicate(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Send to clients only when the gauge moved by 1% or the state flipped.
	protected void CgsReplicate(bool force)
	{
		if (!force && Math.AbsFloat(m_fHeat - m_fLastRepHeat) < m_fMaxHeat * 0.01)
			return;
		m_fLastRepHeat = m_fHeat;
		Replication.BumpMe();
	}
}

//------------------------------------------------------------------------------------------------
//! Plugs heat into MechCore: shots add heat, overheating locks weapons and slows the mech.
modded class BFA_MechExtension
{
	//------------------------------------------------------------------------------------------------
	override bool CanWeaponEntityFire(IEntity mech, IEntity weaponEntity)
	{
		if (!super.CanWeaponEntityFire(mech, weaponEntity))
			return false;

		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(mech);
		return !heat || !heat.LocksWeapons();
	}

	//------------------------------------------------------------------------------------------------
	override void OnWeaponFiredHeat(IEntity mech, IEntity weaponEntity, float heat, string weaponName)
	{
		super.OnWeaponFiredHeat(mech, weaponEntity, heat, weaponName);

		CGS_MechHeatComponent heatComp = CGS_MechHeatComponent.Find(mech);
		if (!heatComp)
			return;

		heatComp.DebugShot(weaponName, heat);
		heatComp.AddHeat(heat);
	}

	//------------------------------------------------------------------------------------------------
	override float GetMechSpeedScale(IEntity mech)
	{
		float scale = super.GetMechSpeedScale(mech);
		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(mech);
		if (heat)
			scale *= heat.GetSpeedScale();
		return scale;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanMechSprint(IEntity mech)
	{
		if (!super.CanMechSprint(mech))
			return false;
		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(mech);
		return !heat || !heat.IsOverheated();
	}
}

//------------------------------------------------------------------------------------------------
//! Adds "HEAT xx%" / "OVERHEAT" to the end of the cockpit HUD status line.
modded class BFA_MechHUDDisplay
{
	//------------------------------------------------------------------------------------------------
	override void Update(IEntity mech, string station)
	{
		super.Update(mech, station);

		if (!m_wStatusText || !mech)
			return;

		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(mech);
		if (!heat)
			return;

		string heatText;
		if (heat.IsOverheated())
			heatText = "OVERHEAT";
		else
			heatText = string.Format("HEAT %1", Math.Round(heat.GetHeat01() * 100)) + "%";

		string current = m_wStatusText.GetText();
		int cut = current.IndexOf("   HEAT");
		if (cut < 0)
			cut = current.IndexOf("   OVERHEAT");
		if (cut >= 0)
			current = current.Substring(0, cut);
		else if (current.StartsWith("HEAT") || current.StartsWith("OVERHEAT"))
			current = string.Empty;

		if (current.IsEmpty())
			m_wStatusText.SetText(heatText);
		else
			m_wStatusText.SetText(current + "   " + heatText);
	}
}
