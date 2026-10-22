[ComponentEditorProps(category: "GameScripted/Mech/Weapons", description: "Swappable mech Las Cannon module. Owns its fire rate, hitscan, damage, armor-piercing fallback and tracer settings.")]
class BFA_MechLasCannonModuleComponentClass : BFA_MechWeaponModuleComponentClass
{
}

class BFA_MechLasCannonModuleComponent : BFA_MechWeaponModuleComponent
{
	[Attribute("300", UIWidgets.Auto, "Fire rate in rounds per minute.", category: "Las Cannon")]
	protected float m_fRoundsPerMinute;

	[Attribute("1500", UIWidgets.Auto, "Maximum beam/hitscan distance in metres.", category: "Las Cannon")]
	protected float m_fRange;

	[Attribute("75", UIWidgets.Auto, "Base kinetic damage per successful hit.", category: "Damage")]
	protected float m_fDamage;

	[Attribute("true", UIWidgets.CheckBox, "If normal kinetic damage is completely absorbed, apply TRUE damage to the same hit zone.", category: "Damage")]
	protected bool m_bArmorPiercingFallback;

	[Attribute("1.0", UIWidgets.Auto, "Multiplier applied to base damage for armor-piercing fallback.", category: "Damage")]
	protected float m_fArmorPiercingMultiplier;

	[Attribute("300", UIWidgets.Auto, "Visual tracer travel speed in metres/second.", category: "Tracer")]
	protected float m_fTracerSpeed;

	[Attribute("25", UIWidgets.Auto, "Visible tracer streak length in metres.", category: "Tracer")]
	protected float m_fTracerLength;

	[Attribute("0.12", UIWidgets.Auto, "Tracer thickness.", category: "Tracer")]
	protected float m_fTracerThickness;

	[Attribute("false", UIWidgets.CheckBox, "Print Las Cannon hit/damage diagnostics.", category: "Diagnostics")]
	protected bool m_bDebug;

	protected bool m_bFireLoopRunning;
	protected int m_iNextShotTimeMs;
	protected IEntity m_pCurrentInstigator;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_bFireLoopRunning = false;
		m_iNextShotTimeMs = 0;
		m_pCurrentInstigator = null;

		BFA_MechLog.Log("[BFA LAS MODULE] initialized");
	}

	//------------------------------------------------------------------------------------------------
	protected int GetWorldTimeMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	protected int GetFireIntervalMs()
	{
		float rpm = Math.Max(m_fRoundsPerMinute, 1.0);
		int intervalMs = 60000.0 / rpm;
		return Math.Max(intervalMs, 20);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartFireLoop()
	{
		if (m_bFireLoopRunning || !m_bSupportsHeldFire)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		m_bFireLoopRunning = true;
		queue.CallLater(FireTick, 20, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopFireLoop()
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(FireTick);

		m_bFireLoopRunning = false;
		m_pCurrentInstigator = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void FireTick()
	{
		if (!Replication.IsServer() || !m_bTriggerHeld)
		{
			StopFireLoop();
			return;
		}

		int nowMs = GetWorldTimeMs();
		if (nowMs < m_iNextShotTimeMs)
			return;

		FireOneShot(m_pCurrentInstigator);
		m_iNextShotTimeMs = nowMs + GetFireIntervalMs();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerDown(IEntity instigator)
	{
		m_pCurrentInstigator = instigator;

		int nowMs = GetWorldTimeMs();
		FireOneShot(instigator);
		m_iNextShotTimeMs = nowMs + GetFireIntervalMs();

		StartFireLoop();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerHeld(IEntity instigator)
	{
		m_pCurrentInstigator = instigator;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerUp(IEntity instigator)
	{
		StopFireLoop();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnFirePulse(IEntity instigator)
	{
		int nowMs = GetWorldTimeMs();
		if (nowMs < m_iNextShotTimeMs)
			return;

		FireOneShot(instigator);
		m_iNextShotTimeMs = nowMs + GetFireIntervalMs();
	}




	//------------------------------------------------------------------------------------------------
	protected void ApplyDamage(IEntity hitEntity, IEntity instigatorEntity, vector hitPosition, vector hitDirection)
	{
		if (!Replication.IsServer() || m_fDamage <= 0 || !hitEntity)
			return;

		// Shared damage path now applies infantry effectiveness and a tunable armour floor.
		float dealt = DealDamage(hitEntity, m_fDamage, EDamageType.KINETIC, hitPosition, hitDirection, instigatorEntity);

		// Legacy optional armour-piercing fallback: only when NO health was removed at all.
		if (m_bArmorPiercingFallback && dealt <= 0.001 && m_fArmorPiercingMultiplier > 0)
			dealt += DealDamage(hitEntity, m_fDamage * m_fArmorPiercingMultiplier, EDamageType.TRUE, hitPosition, hitDirection, instigatorEntity);

		if (m_bDebug)
			Print(string.Format("[BFA LAS MODULE] target=%1 applied=%2 requested=%3", hitEntity, dealt, m_fDamage));
	}

	//------------------------------------------------------------------------------------------------
	protected void FireOneShot(IEntity instigatorEntity)
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint)
			return;

		if (!ConsumeRound())
			return;

		vector direction;
		vector start = GetMuzzle(direction);

		IEntity hitEntity;
		vector end = TraceShot(start, direction, m_fRange, hitEntity);

		ApplyDamage(hitEntity, instigatorEntity, end, direction);
		BroadcastShot(start, end, hitEntity != null, m_fTracerSpeed, m_fTracerLength, m_fTracerThickness);

		if (m_bDebug)
		{
			if (hitEntity)
				Print(string.Format("[BFA LAS MODULE] HIT module=%1 hardpoint=%2 target=%3", m_sModuleId, m_sHardpointName, hitEntity));
			else
				BFA_MechLog.Log(string.Format("[BFA LAS MODULE] MISS module=%1 hardpoint=%2", m_sModuleId, m_sHardpointName));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stop every repeating/delayed call before this component goes away. Without this the
	//! call queue keeps invoking methods on a destroyed mech (death, despawn, GC cleanup).
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			StopFireLoop();
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override float GetAimRange()
	{
		return m_fRange;
	}
}
