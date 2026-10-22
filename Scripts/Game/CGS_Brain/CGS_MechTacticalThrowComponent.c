//------------------------------------------------------------------------------------------------
//! EXPERIMENTAL: server-only arcing launch. Not activated on any shipping prefab.
//! Character physics and landing damage must be validated in Workbench before enabling.
[ComponentEditorProps(category: "CGS MechCore", description: "EXPERIMENTAL companion tactical throw; OFF by default.")]
class CGS_MechTacticalThrowComponentClass : ScriptComponentClass
{
}

class CGS_MechTacticalThrowComponent : ScriptComponent
{
	[Attribute("false", UIWidgets.CheckBox, "Enable experimental tactical throw on this mech.", category: "Tactical Throw")]
	protected bool m_bEnabled;

	[Attribute("false", UIWidgets.CheckBox, "ALLOW UNSAFE TEST: no landing damage protection is implemented yet. Only enable in a disposable test world.", category: "Tactical Throw")]
	protected bool m_bAllowUnprotectedTest;

	[Attribute("20", UIWidgets.Auto, "Horizontal throw distance (metres).", category: "Tactical Throw")]
	protected float m_fDistance;

	[Attribute("6", UIWidgets.Auto, "Height of the arc above the launch point (metres).", category: "Tactical Throw")]
	protected float m_fArcHeight;

	[Attribute("20", UIWidgets.Auto, "Cooldown between throws (seconds).", category: "Tactical Throw")]
	protected float m_fCooldown;

	[Attribute("6", UIWidgets.Auto, "Maximum player distance for activation (metres).", category: "Tactical Throw")]
	protected float m_fUseDistance;

	[Attribute("true", UIWidgets.CheckBox, "Tell the companion to FOLLOW once the launch succeeds.", category: "Tactical Throw")]
	protected bool m_bFollowAfterLaunch;

	protected float m_fNextThrowTime;

	protected CGS_MechBrainComponent GetBrain()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;
		return CGS_MechBrainComponent.Find(owner);
	}

	//! Client uses this only for showing the action. Server repeats all checks.
	bool CanOffer(IEntity user)
	{
		if (!m_bEnabled || !m_bAllowUnprotectedTest || !user || !GetOwner())
			return false;
		if (!ChimeraCharacter.Cast(user))
			return false;
		if (vector.DistanceSq(user.GetOrigin(), GetOwner().GetOrigin()) > Math.Pow(Math.Max(m_fUseDistance, 2.0), 2))
			return false;
		CGS_MechBrainComponent brain = GetBrain();
		return brain && brain.CanShowCompanionAction(user);
	}

	bool ServerThrow(IEntity user)
	{
		if (!Replication.IsServer() || !CanOffer(user) || !GetGame())
			return false;
		BaseWorld world = GetGame().GetWorld();
		PlayerManager players = GetGame().GetPlayerManager();
		if (!world || !players)
			return false;
		int playerId = players.GetPlayerIdFromControlledEntity(user);
		if (playerId <= 0 || players.GetPlayerControlledEntity(playerId) != user)
			return false;

		float now = world.GetWorldTime() * 0.001;
		if (now < m_fNextThrowTime)
			return false;

		IEntity mech = GetOwner();
		SCR_DamageManagerComponent health = SCR_DamageManagerComponent.Cast(user.FindComponent(SCR_DamageManagerComponent));
		if (health && health.GetState() == EDamageState.DESTROYED)
			return false;

		Physics body = user.GetPhysics();
		if (!body)
		{
			Print("[CGS THROW] Character physics unavailable; no launch performed.", LogLevel.WARNING);
			return false;
		}

		vector from = user.GetOrigin();
		vector forward = mech.GetTransformAxis(2);
		forward[1] = 0;
		if (forward.LengthSq() < 0.01)
			return false;
		forward.Normalize();

		float distance = Math.Clamp(m_fDistance, 5.0, 60.0);
		float apex = Math.Clamp(m_fArcHeight, 3.0, 16.0);
		float gravity = 9.81;
		float vertical = Math.Sqrt(2.0 * gravity * apex);
		float airtime = (2.0 * vertical) / gravity;
		vector target = from + forward * distance;
		float targetSurface = world.GetSurfaceY(target[0], target[2]);
		if (targetSurface > target[1] + 1.5)
		{
			Print("[CGS THROW] Landing ground is too high; launch cancelled.", LogLevel.WARNING);
			return false;
		}
		// This checks only landing height. Full obstacle sweeps and safe-landing protection
		// are required before this experiment can ship as gameplay.
		vector launchVelocity = forward * (distance / airtime);
		launchVelocity[1] = vertical;

		// This Physics API is a candidate only: confirm its behavior on Character entities
		// and server-to-client replication in Workbench before enabling a real prefab.
		body.SetVelocity(launchVelocity);
		m_fNextThrowTime = now + Math.Max(m_fCooldown, 1.0);
		if (m_bFollowAfterLaunch)
		{
			CGS_MechBrainComponent brain = GetBrain();
			if (brain)
				brain.ServerCommandCompanion(user, CGS_EMechCompanionOrder.FOLLOW);
		}
		Print(string.Format("[CGS THROW] launched player=%1 distance=%2 apex=%3", playerId, distance, apex));
		return true;
	}
}
