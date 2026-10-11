//------------------------------------------------------------------------------------------------
//! CGS HIDDEN PILOT. Put this on a character prefab (inherit a vanilla soldier, e.g. the US or
//! USSR rifleman, add this component, save as e.g. CGS_Pilot_US.et). The mech brain spawns it in
//! the DRIVER seat. It is invisible and cannot be hurt; it dies only with its mech.
[ComponentEditorProps(category: "CGS MechCore", description: "Invisible, unkillable mech pilot spawned by CGS_MechBrainComponent (CGS add-on).")]
class CGS_HiddenPilotComponentClass : ScriptComponentClass
{
}

class CGS_HiddenPilotComponent : ScriptComponent
{
	[Attribute("true", UIWidgets.CheckBox, "Hide the pilot's body and gear on every machine.", category: "Pilot")]
	protected bool m_bInvisible;

	[Attribute("true", UIWidgets.CheckBox, "Pilot takes no damage (it is removed when its mech is destroyed).", category: "Pilot")]
	protected bool m_bInvulnerable;

	protected float m_fHideTime;

	//------------------------------------------------------------------------------------------------
	static bool IsHiddenPilot(IEntity character)
	{
		return character && character.FindComponent(CGS_HiddenPilotComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_bInvulnerable && Replication.IsServer())
		{
			DamageManagerComponent damage = DamageManagerComponent.Cast(owner.FindComponent(DamageManagerComponent));
			if (damage)
				damage.EnableDamageHandling(false);
		}

		// Gear is attached a moment after spawn, so keep hiding for a few seconds.
		if (m_bInvisible)
		{
			m_fHideTime = 5.0;
			SetEventMask(owner, EntityEvent.FRAME);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		owner.ClearFlags(EntityFlags.VISIBLE, true);

		m_fHideTime -= timeSlice;
		if (m_fHideTime <= 0)
			ClearEventMask(owner, EntityEvent.FRAME);
	}
}
