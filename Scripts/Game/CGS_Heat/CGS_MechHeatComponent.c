//------------------------------------------------------------------------------------------------
//! CGS HEAT (MechWarrior style). Add to a mech to give it a heat gauge:
//!  - every shot adds the weapon part's Heat Per Shot (Heat category on the weapon module),
//!  - sprinting adds heat every second, the mech cools down all the time,
//!  - WARNING / DANGER bands with an alarm for the crew,
//!  - at Max Heat the mech OVERHEATS: with Shutdown on it powers down (no moving, no weapons)
//!    until cooled to Recover At and a minimum restart time has passed; with Shutdown off it
//!    only slows down and locks its weapons (the old behaviour),
//!  - OVERRIDE (toggle key): the pilot refuses to shut down. Heat keeps climbing past Max and
//!    every second above it burns the mech's internal structure (hull damage),
//!  - COOLANT FLUSH (key): limited charges, each dumps a chunk of heat at once and can end a
//!    shutdown early. AI pilots flush on their own when it gets hot.
//! The cockpit HUD shows HEAT xx% / DANGER / SHUTDOWN / OVERRIDE and the coolant left.
//! Server decides, every machine sees the result.
[ComponentEditorProps(category: "CGS MechCore", description: "Heat: shots and sprinting heat the mech; overheating shuts it down (MechWarrior style) unless overridden; coolant flush.")]
class CGS_MechHeatComponentClass : ScriptComponentClass
{
}

class CGS_MechHeatComponent : ScriptComponent
{
	// ---- Heat ----------------------------------------------------------------------------------
	[Attribute("100", UIWidgets.Auto, "Heat the mech can hold. Reaching it = OVERHEAT.", category: "Heat")]
	protected float m_fMaxHeat;

	[Attribute("12", UIWidgets.Auto, "Heat lost per second (always cooling).", category: "Heat")]
	protected float m_fCoolingPerSecond;

	[Attribute("6", UIWidgets.Auto, "Heat added per second while sprinting.", category: "Heat")]
	protected float m_fSprintHeatPerSecond;

	[Attribute("0.75", UIWidgets.Slider, "WARNING above this fraction of Max Heat (HUD + alarm).", "0 1 0.05", category: "Heat")]
	protected float m_fWarningAt;

	[Attribute("0.9", UIWidgets.Slider, "DANGER above this fraction of Max Heat (HUD + faster alarm).", "0 1 0.05", category: "Heat")]
	protected float m_fDangerAt;

	// ---- Overheat --------------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "SHUTDOWN: overheating powers the mech down (no moving, no weapons) until it recovers. Off = it only slows down (Overheat Speed).", category: "Overheat")]
	protected bool m_bShutdown;

	[Attribute("0.4", UIWidgets.Slider, "After an overheat the mech recovers once heat drops to this fraction of Max Heat.", "0 1 0.05", category: "Overheat")]
	protected float m_fRecoverAt;

	[Attribute("4", UIWidgets.Auto, "Shutdown lasts at least this many seconds (restart sequence), even if it cools faster.", category: "Overheat")]
	protected float m_fMinShutdownTime;

	[Attribute("1.5", UIWidgets.Auto, "Cooling is multiplied by this while overheated / shut down (vents open).", category: "Overheat")]
	protected float m_fOverheatCoolingMultiplier;

	[Attribute("0.5", UIWidgets.Slider, "Speed while overheated when Shutdown is OFF (1 = full speed).", "0 1 0.05", category: "Overheat")]
	protected float m_fOverheatSpeed;

	[Attribute("true", UIWidgets.CheckBox, "Overheating locks all weapons until the mech recovers.", category: "Overheat")]
	protected bool m_bLockWeapons;

	// ---- Override --------------------------------------------------------------------------------
	[Attribute("true", UIWidgets.CheckBox, "Pilots may OVERRIDE the shutdown (toggle key).", category: "Override")]
	protected bool m_bAllowOverride;

	[Attribute("1.5", UIWidgets.Auto, "With override, heat can climb to this many times Max Heat.", category: "Override")]
	protected float m_fOverrideHeatCap;

	[Attribute("40", UIWidgets.Auto, "Hull damage per second while overridden ABOVE Max Heat, at the very top of the override range (scales up from 0 just above Max).", category: "Override")]
	protected float m_fOverrideDamagePerSecond;

	// ---- Coolant ---------------------------------------------------------------------------------
	[Attribute("3", UIWidgets.Auto, "Coolant flushes per life. 0 = no coolant flush.", category: "Coolant")]
	protected int m_iCoolantCharges;

	[Attribute("0.5", UIWidgets.Slider, "Each flush removes this fraction of Max Heat at once.", "0 1 0.05", category: "Coolant")]
	protected float m_fCoolantAmount;

	[Attribute("8", UIWidgets.Auto, "Seconds between flushes.", category: "Coolant")]
	protected float m_fCoolantCooldown;

	[Attribute("true", UIWidgets.CheckBox, "AI pilots flush coolant on their own when heat passes Danger.", category: "Coolant")]
	protected bool m_bAIUsesCoolant;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Steam particle when coolant is flushed (at the mech). Empty = none.", "ptc", category: "Coolant")]
	protected ResourceName m_sCoolantParticle;

	// ---- Sounds ----------------------------------------------------------------------------------
	[Attribute("", UIWidgets.ResourceNamePicker, "Sound file (.acp) for the heat alarm, shutdown, restart and coolant sounds. Empty = no sounds.", params: "acp", category: "Sounds")]
	protected ResourceName m_sSoundProject;

	[Attribute("", UIWidgets.EditBox, "Alarm event (played for the crew in WARNING / DANGER).", category: "Sounds")]
	protected string m_sAlarmEvent;

	[Attribute("", UIWidgets.EditBox, "Shutdown event.", category: "Sounds")]
	protected string m_sShutdownEvent;

	[Attribute("", UIWidgets.EditBox, "Restart (power up) event.", category: "Sounds")]
	protected string m_sRestartEvent;

	[Attribute("", UIWidgets.EditBox, "Coolant flush event.", category: "Sounds")]
	protected string m_sCoolantEvent;

	[Attribute("false", UIWidgets.CheckBox, "TESTING: print every shot's heat to the log.", category: "Heat")]
	protected bool m_bDebug;

	// ---- Replicated ----------------------------------------------------------------------------
	[RplProp()]
	protected float m_fHeat;

	[RplProp(onRplName: "OnStateReplicated")]
	protected bool m_bOverheated;

	[RplProp()]
	protected bool m_bOverride;

	[RplProp(onRplName: "OnStateReplicated")]
	protected int m_iCoolantLeft = -1;

	protected float m_fLastRepHeat;
	protected int m_iLastLoggedQuarter;
	protected float m_fShutdownSince;
	protected float m_fNextCoolantAt;
	protected bool m_bWasOverheated;
	protected int m_iLastCoolantSeen = -1;
	protected BFA_MechMovementComponent m_pMovement;

	//------------------------------------------------------------------------------------------------
	static CGS_MechHeatComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return CGS_MechHeatComponent.Cast(mech.FindComponent(CGS_MechHeatComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	//! 0..1 of Max Heat (can go above 1 while overridden: up to Override Heat Cap).
	float GetHeat01()
	{
		if (m_fMaxHeat <= 0)
			return 0;
		return Math.Max(m_fHeat / m_fMaxHeat, 0);
	}

	bool IsOverheated()
	{
		return m_bOverheated;
	}

	bool IsShutDown()
	{
		return m_bOverheated && m_bShutdown;
	}

	bool IsOverride()
	{
		return m_bOverride;
	}

	bool IsWarning()
	{
		return GetHeat01() >= m_fWarningAt;
	}

	bool IsDanger()
	{
		return GetHeat01() >= m_fDangerAt;
	}

	int GetCoolantLeft()
	{
		if (m_iCoolantLeft < 0)
			return m_iCoolantCharges;
		return m_iCoolantLeft;
	}

	bool LocksWeapons()
	{
		return m_bOverheated && (m_bLockWeapons || m_bShutdown);
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

		float cap = m_fMaxHeat;
		if (m_bOverride && m_bAllowOverride)
			cap = m_fMaxHeat * Math.Max(m_fOverrideHeatCap, 1);
		m_fHeat = Math.Min(m_fHeat + amount, cap);

		int quarter = Math.Floor(GetHeat01() * 4);
		if (m_bDebug && quarter > m_iLastLoggedQuarter)
			Print(string.Format("[CGS HEAT] %1: heat %2 percent", GetOwner(), quarter * 25));
		m_iLastLoggedQuarter = quarter;

		CheckOverheat();
		CgsReplicate(false);
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Max Heat reached: shut down / overheat, unless the pilot overrides.
	protected void CheckOverheat()
	{
		if (m_bOverheated || m_fHeat < m_fMaxHeat)
			return;
		if (m_bOverride && m_bAllowOverride)
			return;

		m_bOverheated = true;
		m_fShutdownSince = Now();
		if (m_bShutdown)
			Print(string.Format("[CGS HEAT] %1: OVERHEAT - SHUTDOWN", GetOwner()));
		else
			Print(string.Format("[CGS HEAT] %1: OVERHEAT", GetOwner()));
		CgsReplicate(true);
	}

	//==================================================================================================
	// CREW ACTIONS (server)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! [Server] Toggle the shutdown override.
	void ServerToggleOverride()
	{
		if (!Replication.IsServer() || !m_bAllowOverride)
			return;

		m_bOverride = !m_bOverride;

		// Override on while shut down: power back up at once (the classic emergency restart).
		if (m_bOverride && m_bOverheated)
		{
			m_bOverheated = false;
			Print(string.Format("[CGS HEAT] %1: shutdown OVERRIDDEN", GetOwner()));
		}

		// Override off while above Max: the shutdown happens now.
		if (!m_bOverride)
			CheckOverheat();

		CgsReplicate(true);
	}

	//------------------------------------------------------------------------------------------------
	//! [Server] Coolant flush.
	void ServerFlushCoolant()
	{
		if (!Replication.IsServer() || m_iCoolantCharges <= 0 || m_fMaxHeat <= 0)
			return;

		float now = Now();
		if (now < m_fNextCoolantAt || GetCoolantLeft() <= 0 || m_fHeat <= 0)
			return;

		m_iCoolantLeft = GetCoolantLeft() - 1;
		m_fNextCoolantAt = now + Math.Max(m_fCoolantCooldown, 0);
		m_fHeat = Math.Max(m_fHeat - m_fMaxHeat * Math.Clamp(m_fCoolantAmount, 0, 1), 0);
		Print(string.Format("[CGS HEAT] %1: COOLANT FLUSH (%2 left)", GetOwner(), m_iCoolantLeft));

		TryRecover(now, true);
		CgsReplicate(true);
	}

	//==================================================================================================
	// TICK
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_pMovement = BFA_MechMovementComponent.Cast(owner.FindComponent(BFA_MechMovementComponent));

		if (Replication.IsServer())
		{
			m_iCoolantLeft = m_iCoolantCharges;
			SetEventMask(owner, EntityEvent.FRAME);
			if (m_bDebug)
				Print(string.Format("[CGS HEAT] %1: heat gauge on (max %2, cooling %3/s)", owner, m_fMaxHeat, m_fCoolingPerSecond));
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_fMaxHeat <= 0)
			return;

		float now = Now();

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

		// Overridden above Max: internal damage.
		if (m_bOverride && m_fHeat > m_fMaxHeat)
			OverrideDamage(owner, timeSlice);

		AIUseCoolant(owner);

		if (TryRecover(now, false))
			return;

		CgsReplicate(false);
	}

	//------------------------------------------------------------------------------------------------
	protected bool TryRecover(float now, bool ignoreMinTime)
	{
		if (!m_bOverheated || m_fHeat > m_fMaxHeat * Math.Clamp(m_fRecoverAt, 0, 1))
			return false;
		if (m_bShutdown && !ignoreMinTime && now - m_fShutdownSince < Math.Max(m_fMinShutdownTime, 0))
			return false;

		m_bOverheated = false;
		Print(string.Format("[CGS HEAT] %1: recovered", GetOwner()));
		CgsReplicate(true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OverrideDamage(IEntity owner, float timeSlice)
	{
		if (m_fOverrideDamagePerSecond <= 0 || BFA_MechExtension.IsMechDestroyed(owner))
			return;

		float range = m_fMaxHeat * Math.Max(Math.Max(m_fOverrideHeatCap, 1) - 1, 0.01);
		float over = Math.Clamp((m_fHeat - m_fMaxHeat) / range, 0, 1);
		float amount = m_fOverrideDamagePerSecond * over * timeSlice;
		if (amount <= 0)
			return;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(owner.FindComponent(SCR_DamageManagerComponent));
		if (!damage)
			return;
		HitZone hull = damage.GetDefaultHitZone();
		if (!hull)
			return;

		vector hitTransform[3];
		hitTransform[0] = owner.GetOrigin();
		hitTransform[1] = vector.Up;
		hitTransform[2] = vector.Up * -1;

		IEntity pilot = BFA_MechCoreComponent.BFA_ResolveKiller(owner, null);
		SCR_DamageContext context = new SCR_DamageContext(EDamageType.TRUE, amount, hitTransform, owner, hull, Instigator.CreateInstigator(pilot), null, -1, -1);
		damage.HandleDamage(context);
	}

	//------------------------------------------------------------------------------------------------
	protected void AIUseCoolant(IEntity owner)
	{
		if (!m_bAIUsesCoolant || GetCoolantLeft() <= 0 || GetHeat01() < m_fDangerAt)
			return;

		IEntity crew = BFA_MechCoreComponent.BFA_ResolveKiller(owner, null);
		if (!crew || !BFA_MechMovementComponent.IsAIControlled(crew))
			return;

		ServerFlushCoolant();
	}

	//------------------------------------------------------------------------------------------------
	//! Send to clients only when the gauge moved by 1% or the state flipped.
	protected void CgsReplicate(bool force)
	{
		if (!force && Math.AbsFloat(m_fHeat - m_fLastRepHeat) < m_fMaxHeat * 0.01)
			return;
		m_fLastRepHeat = m_fHeat;
		Replication.BumpMe();

		// The server doesn't get its own RplProp callbacks: a listen host / Workbench plays the
		// shutdown / restart / coolant effects here.
		if (force && !System.IsConsoleApp())
			OnStateReplicated();
	}

	//==================================================================================================
	// EFFECTS (every machine)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Shutdown / restart / coolant changed: sounds and steam.
	protected void OnStateReplicated()
	{
		if (System.IsConsoleApp())
			return;

		if (m_bOverheated != m_bWasOverheated)
		{
			if (m_bOverheated && m_bShutdown)
				PlayHeatSound(m_sShutdownEvent);
			else if (!m_bOverheated && m_bShutdown)
				PlayHeatSound(m_sRestartEvent);
			m_bWasOverheated = m_bOverheated;
		}

		int left = GetCoolantLeft();
		if (m_iLastCoolantSeen >= 0 && left < m_iLastCoolantSeen)
		{
			PlayHeatSound(m_sCoolantEvent);
			if (!m_sCoolantParticle.IsEmpty())
			{
				ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
				spawnParams.Parent = GetOwner();
				Math3D.MatrixIdentity4(spawnParams.Transform);
				ParticleEffectEntity.SpawnParticleEffect(m_sCoolantParticle, spawnParams);
			}
		}
		m_iLastCoolantSeen = left;
	}

	//------------------------------------------------------------------------------------------------
	void PlayHeatSound(string eventName)
	{
		if (eventName.IsEmpty() || m_sSoundProject.IsEmpty() || System.IsConsoleApp())
			return;
		vector mat[4];
		GetOwner().GetWorldTransform(mat);
		BFA_MechWeaponModuleComponent.BFA_MechPlaySound(m_sSoundProject, eventName, mat);
	}

	//------------------------------------------------------------------------------------------------
	string GetAlarmEvent()
	{
		return m_sAlarmEvent;
	}
}

//------------------------------------------------------------------------------------------------
//! Plugs heat into MechCore: shots add heat, overheating locks weapons and slows / stops the mech.
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
//! Client -> server: heat actions (1 = toggle override, 2 = coolant flush).
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void CGS_Heat_RequestAction(IEntity mech, string stationId, int action)
	{
		if (!mech)
			return;
		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		if (Replication.IsServer())
		{
			CGS_Heat_ServerAction(rpl.Id(), stationId, action);
			return;
		}
		Rpc(CGS_Heat_RpcAsk_Action, rpl.Id(), stationId, action);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void CGS_Heat_RpcAsk_Action(RplId mechId, string stationId, int action)
	{
		CGS_Heat_ServerAction(mechId, stationId, action);
	}

	//------------------------------------------------------------------------------------------------
	protected void CGS_Heat_ServerAction(RplId mechId, string stationId, int action)
	{
		// Own lookups: this file compiles before Scripts/Game/Mech, so the BFA_Mech_* helpers
		// on this class are not visible here yet.
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(mechId));
		if (!rpl || !rpl.GetEntity())
			return;
		IEntity mech = rpl.GetEntity().GetRootParent();
		if (!mech)
			mech = rpl.GetEntity();

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		if (!core || stationId.IsEmpty() || !core.IsControllerAtStation(this, stationId))
			return;

		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(mech);
		if (!heat)
			return;

		if (action == 1)
			heat.ServerToggleOverride();
		else if (action == 2)
			heat.ServerFlushCoolant();
	}
}

//------------------------------------------------------------------------------------------------
//! Override / coolant keys for the crew (MechHeatOverride, MechCoolant in the input config).
modded class BFA_MechCrewStationControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void StartLocalStation(string stationId)
	{
		bool wasActive = m_bLocalStationActive;
		super.StartLocalStation(stationId);
		if (wasActive || !m_bLocalStationActive)
			return;

		InputManager input = GetGame().GetInputManager();
		if (input)
		{
			input.AddActionListener("MechHeatOverride", EActionTrigger.DOWN, CGS_OnHeatOverrideInput);
			input.AddActionListener("MechCoolant", EActionTrigger.DOWN, CGS_OnCoolantInput);
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void StopLocalStation()
	{
		InputManager input = GetGame().GetInputManager();
		if (input)
		{
			input.RemoveActionListener("MechHeatOverride", EActionTrigger.DOWN, CGS_OnHeatOverrideInput);
			input.RemoveActionListener("MechCoolant", EActionTrigger.DOWN, CGS_OnCoolantInput);
		}
		super.StopLocalStation();
	}

	//------------------------------------------------------------------------------------------------
	void CGS_OnHeatOverrideInput()
	{
		if (!m_bLocalStationActive)
			return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.CGS_Heat_RequestAction(GetOwner(), m_sLocalStation, 1);
	}

	//------------------------------------------------------------------------------------------------
	void CGS_OnCoolantInput()
	{
		if (!m_bLocalStationActive)
			return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.CGS_Heat_RequestAction(GetOwner(), m_sLocalStation, 2);
	}
}

//------------------------------------------------------------------------------------------------
//! Cockpit HUD: HEAT xx% / DANGER / SHUTDOWN / OVERRIDE, coolant left, and the crew alarm.
modded class BFA_MechHUDDisplay
{
	protected float m_fCgsNextAlarm;

	//------------------------------------------------------------------------------------------------
	override void Update(IEntity mech, string station)
	{
		super.Update(mech, station);

		if (!m_wStatusText || !mech)
			return;

		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(mech);
		if (!heat)
			return;

		int percent = Math.Round(heat.GetHeat01() * 100);
		string heatText;
		if (heat.IsShutDown())
			heatText = string.Format("SHUTDOWN  HEAT %1", percent) + "%";
		else if (heat.IsOverheated())
			heatText = string.Format("OVERHEAT  HEAT %1", percent) + "%";
		else if (heat.IsDanger())
			heatText = string.Format("DANGER  HEAT %1", percent) + "%";
		else
			heatText = string.Format("HEAT %1", percent) + "%";

		if (heat.IsOverride())
			heatText = heatText + "  OVERRIDE";
		if (heat.GetCoolantLeft() > 0)
			heatText = heatText + string.Format("  COOLANT %1", heat.GetCoolantLeft());

		string current = m_wStatusText.GetText();
		int cut = current.IndexOf("   HEAT");
		array<string> tags = {"   SHUTDOWN", "   OVERHEAT", "   DANGER"};
		foreach (string tag : tags)
		{
			int at = current.IndexOf(tag);
			if (at >= 0 && (cut < 0 || at < cut))
				cut = at;
		}
		if (cut >= 0)
			current = current.Substring(0, cut);
		else if (current.StartsWith("HEAT") || current.StartsWith("SHUTDOWN") || current.StartsWith("OVERHEAT") || current.StartsWith("DANGER"))
			current = string.Empty;

		if (current.IsEmpty())
			m_wStatusText.SetText(heatText);
		else
			m_wStatusText.SetText(current + "   " + heatText);

		// Crew alarm: every 1.5 s in WARNING, every 0.6 s in DANGER / above max.
		if (heat.IsWarning() && !heat.IsShutDown() && GetGame() && GetGame().GetWorld())
		{
			float now = GetGame().GetWorld().GetWorldTime() * 0.001;
			if (now >= m_fCgsNextAlarm)
			{
				heat.PlayHeatSound(heat.GetAlarmEvent());
				if (heat.IsDanger())
					m_fCgsNextAlarm = now + 0.6;
				else
					m_fCgsNextAlarm = now + 1.5;
			}
		}
	}
}
