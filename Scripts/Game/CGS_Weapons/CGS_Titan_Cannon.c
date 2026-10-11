//------------------------------------------------------------------------------------------------
//! CGS TITAN CANNON - heavy main gun for titans.
//!  - One trigger pull = one round. Optional CHARGE-UP before it fires (with its own sound).
//!  - Each round can be a VOLLEY: several shells, one per barrel (Barrel Points), a short gap apart.
//!  - Big direct hit + explosive splash. Damage lands when the visible shell ARRIVES (travel time),
//!    so a 700 m/s shell at 1400 m hits 2 s later, like you see it.
//!  - Ammo Type / Armour Piercing / Crew Damage settings (Ammo category) work like on every weapon.
//! Put it on the weapon part prefab (your cannon model) instead of the autocannon component.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "CGS Titan Cannon: heavy shells with travel time, splash, optional charge-up and multi-barrel volleys.")]
class CGS_Titan_CannonClass : BFA_MechWeaponModuleComponentClass
{
}

class CGS_Titan_Cannon : BFA_MechWeaponModuleComponent
{
	static const int EVENT_CHARGE = 50; // module event: charge-up started (plays the charge sound)

	// ---- Shell -----------------------------------------------------------------------------
	[Attribute("2500", UIWidgets.Auto, "Range in metres.", category: "Titan Cannon")]
	protected float m_fRange;

	[Attribute("1500", UIWidgets.Auto, "Damage of a direct hit (per shell).", category: "Titan Cannon")]
	protected float m_fDirectDamage;

	[Attribute("400", UIWidgets.Auto, "Explosive splash damage at the impact (per shell). 0 = none.", category: "Titan Cannon")]
	protected float m_fSplashDamage;

	[Attribute("10", UIWidgets.Auto, "Splash radius in metres. Damage falls off to zero at the edge.", category: "Titan Cannon")]
	protected float m_fSplashRadius;

	[Attribute("0.1", UIWidgets.Auto, "Spread in degrees (0 = laser accurate).", category: "Titan Cannon")]
	protected float m_fSpread;

	[Attribute("700", UIWidgets.Auto, "Shell speed in m/s. The visible shell flies at this speed and damage lands when it arrives.", category: "Titan Cannon")]
	protected float m_fShellSpeed;

	[Attribute("true", UIWidgets.CheckBox, "Damage lands when the shell arrives (travel time). Off = instant hit like the autocannon.", category: "Titan Cannon")]
	protected bool m_bTravelTime;

	[Attribute("8", UIWidgets.Auto, "Visible shell streak length in metres.", category: "Titan Cannon")]
	protected float m_fShellLength;

	[Attribute("0.5", UIWidgets.Auto, "Visible shell streak thickness.", category: "Titan Cannon")]
	protected float m_fShellThickness;

	// ---- Volley ----------------------------------------------------------------------------
	[Attribute("1", UIWidgets.Auto, "Shells per round. With Barrel Points set, each shell leaves the next barrel (2 barrels + 2 shells = both barrels fire).", category: "Volley")]
	protected int m_iShellsPerRound;

	[Attribute("120", UIWidgets.Auto, "Milliseconds between the shells of one volley.", category: "Volley")]
	protected int m_iShellIntervalMs;

	// ---- Charge ----------------------------------------------------------------------------
	[Attribute("0", UIWidgets.Auto, "Charge-up in seconds before the round fires (0 = fires at once). Set Shots Per Minute low enough that the charge fits (12 spm = one round every 5 s).", category: "Charge")]
	protected float m_fChargeTime;

	[Attribute("", UIWidgets.EditBox, "Charge-up sound event inside the Effects sound file (.acp). Empty = silent charge.", category: "Charge")]
	protected string m_sChargeSoundEvent;

	// ---- Diagnostics -----------------------------------------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Print every round, hit and splash to the log.", category: "Diagnostics")]
	protected bool m_bDebug;

	// ---- Runtime (server) ------------------------------------------------------------------
	protected bool m_bCharging;
	protected int m_iChargeToken;  // a newer charge / volley makes older scheduled calls do nothing
	protected IEntity m_pRoundInstigator;
	protected int m_iShellsLeft;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sModuleId == "WEAPON_MODULE")
			m_sModuleId = "TITAN_CANNON";

		m_bCharging = false;
		m_iShellsLeft = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! No new round while one is still charging.
	override protected bool TryFireShot(IEntity instigator)
	{
		if (m_bCharging || m_iShellsLeft > 0)
			return false;

		return super.TryFireShot(instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! One round (ammo and fire rate already checked). Charges first if set, then fires the volley.
	override protected void FireShot(IEntity instigator)
	{
		m_pRoundInstigator = instigator;
		m_iChargeToken++;

		if (m_fChargeTime <= 0)
		{
			StartVolley();
			return;
		}

		m_bCharging = true;
		BroadcastModuleEvent(EVENT_CHARGE);

		if (m_bDebug)
			Print(string.Format("[CGS TITAN CANNON] %1 charging %2 s", m_sHardpointName, m_fChargeTime));

		GetGame().GetCallqueue().CallLater(ChargeDone, (int)(m_fChargeTime * 1000), false, m_iChargeToken);
	}

	//------------------------------------------------------------------------------------------------
	protected void ChargeDone(int token)
	{
		if (token != m_iChargeToken || !m_bCharging)
			return;

		m_bCharging = false;
		if (!m_bBoundToHardpoint)
			return; // swapped out while charging

		StartVolley();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartVolley()
	{
		m_iShellsLeft = Math.Max(m_iShellsPerRound, 1);
		VolleyStep(m_iChargeToken);
	}

	//------------------------------------------------------------------------------------------------
	protected void VolleyStep(int token)
	{
		if (token != m_iChargeToken || m_iShellsLeft <= 0)
			return;

		if (!m_bBoundToHardpoint)
		{
			m_iShellsLeft = 0;
			return;
		}

		FireShell(m_pRoundInstigator);
		m_iShellsLeft--;

		if (m_iShellsLeft > 0)
			GetGame().GetCallqueue().CallLater(VolleyStep, Math.Max(m_iShellIntervalMs, 20), false, token);
	}

	//------------------------------------------------------------------------------------------------
	protected void FireShell(IEntity instigator)
	{
		vector direction;
		vector start = GetMuzzle(direction);
		direction = ApplyConeSpread(direction, m_fSpread);

		IEntity hitEntity;
		vector end = TraceShot(start, direction, m_fRange, hitEntity);

		// Everyone sees the shell fly (muzzle flash, sound, projectile, impact from Effects).
		BroadcastShot(start, end, hitEntity != null, m_fShellSpeed, m_fShellLength, m_fShellThickness);

		int delayMs = 0;
		if (m_bTravelTime)
			delayMs = (int)(vector.Distance(start, end) / Math.Max(m_fShellSpeed, 1.0) * 1000.0);

		if (m_bDebug)
			Print(string.Format("[CGS TITAN CANNON] %1 fired, target=%2 distance=%3 m, lands in %4 ms", m_sHardpointName, hitEntity, Math.Round(vector.Distance(start, end)), delayMs));

		if (delayMs < 20)
		{
			ShellLands(hitEntity, end, direction, instigator);
			return;
		}

		GetGame().GetCallqueue().CallLater(ShellLands, delayMs, false, hitEntity, end, direction, instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Shell arrives: direct hit, then splash around the impact.
	protected void ShellLands(IEntity hitEntity, vector position, vector direction, IEntity instigator)
	{
		if (!Replication.IsServer())
			return;

		float directLost = 0;
		if (hitEntity)
			directLost = DealDamage(hitEntity, m_fDirectDamage, EDamageType.KINETIC, position, direction, instigator);

		int splashed = 0;
		if (m_fSplashDamage > 0 && m_fSplashRadius > 0)
		{
			IEntity skipRoot;
			if (hitEntity)
				skipRoot = hitEntity.GetRootParent();
			splashed = DealSplashDamage(position, m_fSplashDamage, m_fSplashRadius, EDamageType.EXPLOSIVE, instigator, skipRoot);
		}

		if (m_bDebug)
			Print(string.Format("[CGS TITAN CANNON] impact at %1: direct=%2 (-%3 hp), splash targets=%4", position, hitEntity, directLost, splashed));
	}

	//------------------------------------------------------------------------------------------------
	//! Plays the charge-up sound on every machine.
	override void ClientOnModuleEvent(int eventType, int value = 0)
	{
		super.ClientOnModuleEvent(eventType, value);

		if (eventType == EVENT_CHARGE && !m_sChargeSoundEvent.IsEmpty())
			PlayModuleSound(m_sChargeSoundEvent);
	}

	//------------------------------------------------------------------------------------------------
	override void UnbindFromHardpoint()
	{
		// Cancels a charge / volley in progress (scheduled calls see a new token and stop).
		m_iChargeToken++;
		m_bCharging = false;
		m_iShellsLeft = 0;
		super.UnbindFromHardpoint();
	}

	//------------------------------------------------------------------------------------------------
	override float GetAimRange()
	{
		return m_fRange;
	}
}
