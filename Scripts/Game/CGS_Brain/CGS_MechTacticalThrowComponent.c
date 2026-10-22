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

	[Attribute(desc: "THROW POINT: place the gizmo on the mech where the player should leave its hand / launcher, exactly like the Movement Ground Point. Its local blue Z axis is throw-forward.", category: "Tactical Throw")]
	protected ref PointInfo m_ThrowPoint;

	protected float m_fNextThrowTime;
	protected IEntity m_pThrownCharacter;
	protected vector m_vThrowStart;
	protected vector m_vThrowEnd;
	protected float m_fThrowElapsed;
	protected float m_fThrowDuration;
	protected float m_fThrowApex;
	protected int m_iThrowLogFrames;

	//------------------------------------------------------------------------------------------------
	//! Proof-of-concept only. Character-controller authority may undo SetOrigin after this frame.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer())
			SetEventMask(owner, EntityEvent.FRAME);
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer() || !m_pThrownCharacter || !GetGame())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			m_pThrownCharacter = null;
			return;
		}

		m_fThrowElapsed += Math.Max(timeSlice, 0);
		float t = Math.Min(m_fThrowElapsed / Math.Max(m_fThrowDuration, 0.1), 1.0);
		vector position = m_vThrowStart + (m_vThrowEnd - m_vThrowStart) * t;
		position[1] = m_vThrowStart[1] + (m_vThrowEnd[1] - m_vThrowStart[1]) * t + 4.0 * m_fThrowApex * t * (1.0 - t);

		float terrain = world.GetSurfaceY(position[0], position[2]);
		// Never reject the very first frame because the character's feet start near ground.
		// Once clear of the launch phase, reject terrain obstructions rather than phase through.
		if (t > 0.08 && t < 0.92 && position[1] <= terrain + 0.35)
		{
			Print(string.Format("[CGS THROW TEST] STOP: path hits terrain t=%1 desired=%2 terrain=%3", t, position, terrain), LogLevel.WARNING);
			m_pThrownCharacter = null;
			return;
		}
		// No wall/roof collision scan or landing protection yet: TEST WORLD ONLY.
		vector before = m_pThrownCharacter.GetOrigin();
		m_pThrownCharacter.SetOrigin(position);
		m_pThrownCharacter.Update();

		if (m_iThrowLogFrames < 6)
		{
			Print(string.Format("[CGS THROW TEST] frame=%1 before=%2 desired=%3 actual=%4", m_iThrowLogFrames, before, position, m_pThrownCharacter.GetOrigin()));
			m_iThrowLogFrames++;
		}

		if (t >= 1.0)
		{
			Print(string.Format("[CGS THROW TEST] completed; actual=%1", m_pThrownCharacter.GetOrigin()));
			m_pThrownCharacter = null;
		}
	}

	//------------------------------------------------------------------------------------------------
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

		// Default to the character's current position when no throw point was placed.
		// Placing a PointInfo on the mech overrides BOTH the arc origin and forward direction.
		vector from = user.GetOrigin();
		vector forward = mech.GetTransformAxis(2);
		if (m_ThrowPoint)
		{
			m_ThrowPoint.Init(mech);
			vector localPoint[4];
			vector worldMech[4];
			m_ThrowPoint.GetModelTransform(localPoint);
			mech.GetWorldTransform(worldMech);
			vector p = localPoint[3];
			from = worldMech[3] + worldMech[0] * p[0] + worldMech[1] * p[1] + worldMech[2] * p[2];
			vector axis = localPoint[2];
			forward = worldMech[0] * axis[0] + worldMech[1] * axis[1] + worldMech[2] * axis[2];
		}
		forward[1] = 0;
		if (forward.LengthSq() < 0.01)
			return false;
		forward.Normalize();

		float launchGround = world.GetSurfaceY(from[0], from[2]);
		if (from[1] < launchGround + 0.6)
		{
			Print(string.Format("[CGS THROW TEST] STOP: throw point is below terrain/too low. socket=%1 surface=%2; raise Throw Point in prefab.", from, launchGround), LogLevel.WARNING);
			return false;
		}

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


		// A direct physics velocity compiled but didn't visibly move the character.
		// Diagnose movement with a server-driven arc instead of claiming it succeeded.
		if (m_pThrownCharacter)
			return false;
		m_pThrownCharacter = user;
		m_vThrowStart = from;
		m_vThrowEnd = target;
		m_vThrowEnd[1] = Math.Max(targetSurface + 0.8, target[1]);
		m_fThrowElapsed = 0;
		m_fThrowDuration = airtime;
		m_fThrowApex = apex;
		m_iThrowLogFrames = 0;
		m_fNextThrowTime = now + Math.Max(m_fCooldown, 1.0);
		Print(string.Format("[CGS THROW TEST] arc started player=%1 from=%2 to=%3 duration=%4", playerId, from, m_vThrowEnd, airtime));
		if (m_bFollowAfterLaunch)
		{
			CGS_MechBrainComponent brain = GetBrain();
			if (brain)
				brain.ServerCommandCompanion(user, CGS_EMechCompanionOrder.FOLLOW);
		}
		Print(string.Format("[CGS THROW TEST] attempted player=%1 distance=%2 apex=%3; movement unverified", playerId, distance, apex));
		return true;
	}
}
