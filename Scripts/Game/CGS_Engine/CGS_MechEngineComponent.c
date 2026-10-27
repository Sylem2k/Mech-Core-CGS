// CGS MECH CORE - reusable engine startup foundation.
// Opt-in component: add to a mech root together with its existing RplComponent.
// No legacy movement, weapon, cockpit or sound scripts are modified by this file.
[ComponentEditorProps(category: "CGS MechCore/Engine", description: "Server-authoritative mech power core with adjustable PointInfo engine and service locations.")]
class CGS_MechEngineComponentClass : ScriptComponentClass
{
}

class CGS_MechEngineComponent : ScriptComponent
{
	// 0 = OFF, 1 = STARTING, 2 = RUNNING, 3 = STOPPING (reserved).
	static const int ENGINE_OFF = 0;
	static const int ENGINE_STARTING = 1;
	static const int ENGINE_RUNNING = 2;
	static const int ENGINE_STOPPING = 3;

	[Attribute("4", UIWidgets.Auto, "Seconds between pressing START and engine READY.", category: "Startup")]
	protected float m_fStartDuration;

	[Attribute("false", UIWidgets.CheckBox, "Compatibility option for existing mechs: engine already running at spawn. OFF is recommended for new interactive cockpit prefabs.", category: "Startup")]
	protected bool m_bStartRunning;

	[Attribute("false", UIWidgets.CheckBox, "Log startup and shutdown transitions (test only).", category: "Diagnostics")]
	protected bool m_bDebugEngine;

	// All points are in mech-root local space and can be moved per prefab.
	[Attribute(desc: "MECH ENGINE: PointInfo placed at the engine / sound / VFX origin. Empty = mech root.", category: "Points")]
	protected ref PointInfo m_EnginePoint;

	[Attribute(desc: "START BUTTON: PointInfo aligned with the physical cockpit START button. Empty = mech root.", category: "Points")]
	protected ref PointInfo m_StartButtonPoint;

	[Attribute(desc: "STOP BUTTON: PointInfo aligned with the physical cockpit STOP switch. Empty = mech root.", category: "Points")]
	protected ref PointInfo m_StopButtonPoint;

	[Attribute(desc: "FUEL PORT: reserved for the later CGS fuel system. Empty = mech root.", category: "Points")]
	protected ref PointInfo m_FuelPoint;

	[Attribute(desc: "REPAIR PORT: reserved for the later CGS repair system. Empty = mech root.", category: "Points")]
	protected ref PointInfo m_RepairPoint;

	// Network state is set ONLY by the authoritative server.
	[RplProp()]
	protected int m_iEngineState;

	protected int m_iStartSequence;

	//------------------------------------------------------------------------------------------------
	static CGS_MechEngineComponent Find(IEntity entity)
	{
		if (!entity)
			return null;

		IEntity root = entity.GetRootParent();
		if (!root)
			root = entity;
		return CGS_MechEngineComponent.Cast(root.FindComponent(CGS_MechEngineComponent));
	}

	//------------------------------------------------------------------------------------------------
	int GetEngineState()
	{
		return m_iEngineState;
	}

	//------------------------------------------------------------------------------------------------
	bool IsRunning()
	{
		return m_iEngineState == ENGINE_RUNNING;
	}

	//------------------------------------------------------------------------------------------------
	bool IsStarting()
	{
		return m_iEngineState == ENGINE_STARTING;
	}

	//------------------------------------------------------------------------------------------------
	string GetEngineStateName()
	{
		if (m_iEngineState == ENGINE_STARTING)
			return "STARTING";
		if (m_iEngineState == ENGINE_RUNNING)
			return "RUNNING";
		if (m_iEngineState == ENGINE_STOPPING)
			return "STOPPING";
		return "OFF";
	}

	//------------------------------------------------------------------------------------------------
	protected vector PointToWorld(IEntity owner, PointInfo point)
	{
		if (!owner)
			return vector.Zero;
		if (!point)
			return owner.GetOrigin();

		point.Init(owner);
		vector mat[4];
		point.GetModelTransform(mat);
		return owner.CoordToParent(mat[3]);
	}

	//------------------------------------------------------------------------------------------------
	vector GetEngineWorldPoint()
	{
		return PointToWorld(GetOwner(), m_EnginePoint);
	}

	vector GetStartButtonWorldPoint()
	{
		return PointToWorld(GetOwner(), m_StartButtonPoint);
	}

	vector GetStopButtonWorldPoint()
	{
		return PointToWorld(GetOwner(), m_StopButtonPoint);
	}

	vector GetFuelWorldPoint()
	{
		return PointToWorld(GetOwner(), m_FuelPoint);
	}

	vector GetRepairWorldPoint()
	{
		return PointToWorld(GetOwner(), m_RepairPoint);
	}

	//------------------------------------------------------------------------------------------------
	// Only the seated PILOT can start or stop the core. Not passengers / people outside.
	bool CanOperate(IEntity user)
	{
		if (!user)
			return false;

		IEntity root = GetOwner();
		if (!root)
			return false;

		BFA_MechControllerComponent controls = BFA_MechControllerComponent.Cast(root.FindComponent(BFA_MechControllerComponent));
		if (!controls)
			return false;

		IEntity pilot = controls.BFA_GetPilotOccupant();
		return pilot && pilot == user;
	}

	//------------------------------------------------------------------------------------------------
	// This is intentionally a SERVER action, even when a client interacts with a switch.
	bool ServerStart(IEntity user)
	{
		if (!Replication.IsServer() || !CanOperate(user))
			return false;
		if (m_iEngineState != ENGINE_OFF)
			return false;

		m_iEngineState = ENGINE_STARTING;
		Replication.BumpMe();
		m_iStartSequence++;

		if (m_bDebugEngine)
			BFA_MechLog.Log("[CGS ENGINE] STARTING: " + GetOwner());

		if (GetGame() && GetGame().GetCallqueue())
		{
			int ms = Math.Max(1, m_fStartDuration * 1000);
			GetGame().GetCallqueue().CallLater(FinishStartup, ms, false, m_iStartSequence);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Sequence token prevents a cancelled start from completing after an emergency stop.
	protected void FinishStartup(int sequence)
	{
		if (!Replication.IsServer() || sequence != m_iStartSequence || m_iEngineState != ENGINE_STARTING)
			return;
		m_iEngineState = ENGINE_RUNNING;
		Replication.BumpMe();

		if (m_bDebugEngine)
			BFA_MechLog.Log("[CGS ENGINE] RUNNING: " + GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	bool ServerStop(IEntity user)
	{
		if (!Replication.IsServer() || !CanOperate(user))
			return false;
		if (m_iEngineState == ENGINE_OFF)
			return false;

		m_iStartSequence++;
		m_iEngineState = ENGINE_OFF;
		Replication.BumpMe();

		if (m_bDebugEngine)
			BFA_MechLog.Log("[CGS ENGINE] OFF: " + GetOwner());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;

		m_iStartSequence = 0;
		if (m_bStartRunning)
			m_iEngineState = ENGINE_RUNNING;
		else
			m_iEngineState = ENGINE_OFF;
		Replication.BumpMe();

		if (m_bDebugEngine)
			BFA_MechLog.Log(string.Format("[CGS ENGINE] initialized state=%1 root=%2", GetEngineStateName(), owner));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// A previously scheduled FinishStartup still checks its sequence and state.
		m_iStartSequence++;
		super.OnDelete(owner);
	}
}
