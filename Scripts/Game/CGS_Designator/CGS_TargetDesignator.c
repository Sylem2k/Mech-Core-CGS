//------------------------------------------------------------------------------------------------
//! CGS TARGET DESIGNATOR - mech weapon module that paints a laser spot for laser-guided missiles.
//! Put it on a hardpoint like any weapon and give it its own weapon group.
//!   - TOGGLE (default): press fire on its group = laser on, press again = off. The laser stays on
//!     while the pilot switches to the missile group and fires (one pilot can do both).
//!   - Toggle off: hold fire = laser on, release = laser off.
//!   - The laser goes from the muzzle to where the crew aims (turn on Fire At Aim Point if the
//!     designator sits on an animated arm).
//!   - Whatever it hits gets the spot: a vehicle / mech / soldier keeps it while it moves.
//!     Blocked = the spot is on the blocker. Nothing within range = no spot.
//!   - Costs heat per second while on (needs CGS_MechHeatComponent on the mech).
//! The mech also needs CGS_LaserSpotComponent (it carries the spot to every machine).
//! Missiles: CGS Titan Missile with Guidance = Laser or Both.
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "Target designator: hold fire to paint a laser spot for laser-guided missiles. Needs CGS_LaserSpotComponent on the mech.")]
class CGS_TargetDesignatorClass : BFA_MechWeaponModuleComponentClass
{
}

class CGS_TargetDesignator : BFA_MechWeaponModuleComponent
{
	[Attribute("1500", UIWidgets.Auto, "Laser range in metres.", category: "Designator")]
	protected float m_fDesignatorRange;

	[Attribute("2", UIWidgets.Auto, "Heat per second while the laser is on.", category: "Designator")]
	protected float m_fHeatPerSecond;

	[Attribute("100", UIWidgets.Auto, "Milliseconds between laser updates (100 = 10 per second).", category: "Designator")]
	protected int m_iUpdateMs;

	[Attribute("true", UIWidgets.CheckBox, "Only stick the spot to vehicles, mechs and soldiers. Off = also to buildings and props.", category: "Designator")]
	protected bool m_bStickToUnitsOnly;

	[Attribute("true", UIWidgets.CheckBox, "Press = on, press again = off (one pilot can lase and fire missiles). Off = hold fire to lase.", category: "Designator")]
	protected bool m_bToggle;

	protected bool m_bLaserRunning;
	protected bool m_bWarnedNoSpot;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (m_sModuleId == "WEAPON_MODULE")
			m_sModuleId = "TARGET_DESIGNATOR";
	}

	//------------------------------------------------------------------------------------------------
	//! Range for the HUD / Mech Lab.
	override float GetAimRange()
	{
		return m_fDesignatorRange;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerDown(IEntity instigator)
	{
		if (m_bToggle && m_bLaserRunning)
			StopLaser();
		else
			StartLaser();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerHeld(IEntity instigator)
	{
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTriggerUp(IEntity instigator)
	{
		if (!m_bToggle)
			StopLaser();
	}

	//------------------------------------------------------------------------------------------------
	//! For HUDs: is the laser on?
	bool IsLaserOn()
	{
		return m_bLaserRunning;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnFirePulse(IEntity instigator)
	{
	}

	//------------------------------------------------------------------------------------------------
	override void UnbindFromHardpoint()
	{
		StopLaser();
		super.UnbindFromHardpoint();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		StopLaser();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected CGS_LaserSpotComponent GetSpotComponent()
	{
		CGS_LaserSpotComponent spot = CGS_LaserSpotComponent.Find(m_pMechOwner);
		if (!spot && !m_bWarnedNoSpot)
		{
			m_bWarnedNoSpot = true;
			Print("[CGS DESIGNATOR] the mech has no CGS_LaserSpotComponent - add it to the mech prefab, the laser does nothing without it", LogLevel.WARNING);
		}
		return spot;
	}

	//------------------------------------------------------------------------------------------------
	protected void StartLaser()
	{
		if (!Replication.IsServer() || m_bLaserRunning || !GetGame() || !GetGame().GetCallqueue())
			return;
		m_bLaserRunning = true;
		LaserTick();
		GetGame().GetCallqueue().CallLater(LaserTick, Math.Max(m_iUpdateMs, 30), true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopLaser()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(LaserTick);
		m_bLaserRunning = false;

		CGS_LaserSpotComponent spot = CGS_LaserSpotComponent.Find(m_pMechOwner);
		if (spot)
			spot.ServerStop();
	}

	//------------------------------------------------------------------------------------------------
	protected void LaserTick()
	{
		bool keepOn = m_bTriggerHeld || m_bToggle;
		if (!Replication.IsServer() || !keepOn || !m_bBoundToHardpoint || !m_pMechOwner || BFA_MechExtension.IsMechDestroyed(m_pMechOwner))
		{
			StopLaser();
			return;
		}

		CGS_LaserSpotComponent spot = GetSpotComponent();
		if (!spot)
			return;

		vector direction;
		vector start = GetMuzzle(direction, false);
		IEntity hit;
		vector end = TraceShot(start, direction, m_fDesignatorRange, hit);

		// Nothing within range: no spot.
		if (!hit && vector.DistanceSq(start, end) >= (m_fDesignatorRange - 1) * (m_fDesignatorRange - 1))
			spot.ServerStop();
		else
			spot.ServerSetSpot(end, StickTarget(hit));

		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(m_pMechOwner);
		if (heat && m_fHeatPerSecond > 0)
			heat.AddHeat(m_fHeatPerSecond * Math.Max(m_iUpdateMs, 30) * 0.001);
	}

	//------------------------------------------------------------------------------------------------
	//! What the spot sticks to: the root of a vehicle / soldier (or any object when allowed).
	protected IEntity StickTarget(IEntity hit)
	{
		if (!hit)
			return null;
		IEntity root = hit.GetRootParent();
		if (!root)
			root = hit;
		if (!m_bStickToUnitsOnly)
			return root;
		if (Vehicle.Cast(root) || ChimeraCharacter.Cast(root))
			return root;
		return null;
	}
}
