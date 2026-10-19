[ComponentEditorProps(
	category: "BFA MechCore",
	description: "Crew-station camera aim. Local mounts continuously follow the camera sight point, including while freelooking."
)]
class BFA_MechAimComponentClass : ScriptGameComponentClass
{
}

class BFA_MechAimComponent : ScriptGameComponent
{
	[Attribute(
		"1000",
		UIWidgets.Auto,
		"Distance along the camera sight used for local weapon convergence.",
		category: "Aim"
	)]
	protected float m_fReticleAimDistance;

	[Attribute(
		"16",
		UIWidgets.EditBox,
		"Milliseconds between local camera/reticle aim ticks. Independent of vehicle EOnFrame for dedicated MP clients.",
		category: "Multiplayer"
	)]
	protected int m_iLocalAimTickMs;

	[Attribute(
		"8",
		UIWidgets.Auto,
		"Closest distance in metres the guns will converge at. Stops the arms crossing wildly when the crosshair is on something right in front of the cockpit.",
		category: "Aim"
	)]
	protected float m_fMinConvergenceDistance;

	[Attribute("true", UIWidgets.CheckBox, "Show MechCore's own crosshair. HUD mods can also turn it off from code with BFA_MechHUD.SetBuiltInCrosshairEnabled(false).", category: "Crosshair")]
	protected bool m_bShowCrosshair;

	[Attribute("true", UIWidgets.CheckBox, "Show MechCore's cockpit HUD (weapon ammo + mech status) to the crew. HUD mods can also turn it off with BFA_MechHUD.SetBuiltInHUDEnabled(false).", category: "Crosshair")]
	protected bool m_bShowHUD;

	protected ref BFA_MechHUDDisplay m_HUDDisplay;

	[Attribute("", UIWidgets.Object, "Crosshair used when the selected weapon has none of its own. Empty = built-in white cross.", category: "Crosshair")]
	protected ref BFA_MechCrosshair m_DefaultCrosshair;

	[Attribute("true", UIWidgets.CheckBox, "Show where each gun of the selected weapon group really points (when the weapon's crosshair allows it).", category: "Crosshair")]
	protected bool m_bShowGunMarkers;

	[Attribute("false", UIWidgets.CheckBox, "Also draw the old debug-shape sight (Workbench / diag builds only).", category: "Crosshair")]
	protected bool m_bDebugSight;

	protected ref BFA_MechCrosshairHUD m_CrosshairHUD;
	protected ref array<vector> m_aGunPoints = {};

	// Replicated station aim as a POINT in the mech's local space (not just a direction), so the
	// server and every client converge all mounts on exactly what the station is looking at.
	// Two arms a few metres apart need the real distance to hit the same spot up close.
	// One per crew station, by the station's position in MechCore > Crew Stations (max 8).
	[RplProp(onRplName: "OnRepAimChanged")]
	protected vector m_vRepAim0;
	[RplProp(onRplName: "OnRepAimChanged")]
	protected vector m_vRepAim1;
	[RplProp(onRplName: "OnRepAimChanged")]
	protected vector m_vRepAim2;
	[RplProp(onRplName: "OnRepAimChanged")]
	protected vector m_vRepAim3;
	[RplProp(onRplName: "OnRepAimChanged")]
	protected vector m_vRepAim4;
	[RplProp(onRplName: "OnRepAimChanged")]
	protected vector m_vRepAim5;
	[RplProp(onRplName: "OnRepAimChanged")]
	protected vector m_vRepAim6;
	[RplProp(onRplName: "OnRepAimChanged")]
	protected vector m_vRepAim7;

	//------------------------------------------------------------------------------------------------
	protected int StationIndex(string stationId)
	{
		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(GetOwner().FindComponent(BFA_MechCoreComponent));
		if (!core)
			return -1;
		return core.GetStationIndex(stationId);
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetRepAim(int index)
	{
		switch (index)
		{
			case 0: return m_vRepAim0;
			case 1: return m_vRepAim1;
			case 2: return m_vRepAim2;
			case 3: return m_vRepAim3;
			case 4: return m_vRepAim4;
			case 5: return m_vRepAim5;
			case 6: return m_vRepAim6;
			case 7: return m_vRepAim7;
		}
		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetRepAim(int index, vector value)
	{
		switch (index)
		{
			case 0: m_vRepAim0 = value; break;
			case 1: m_vRepAim1 = value; break;
			case 2: m_vRepAim2 = value; break;
			case 3: m_vRepAim3 = value; break;
			case 4: m_vRepAim4 = value; break;
			case 5: m_vRepAim5 = value; break;
			case 6: m_vRepAim6 = value; break;
			case 7: m_vRepAim7 = value; break;
		}
	}

	protected vector m_vLocalAimDirection;
	protected vector m_vLocalAimPointWorld;

	protected string m_sLocalStation;

	protected bool m_bLocalAimActive;
	protected bool m_bInspectMode;
	protected bool m_bLocalAimTickRunning;
	protected bool m_bLoggedTracking;

	//------------------------------------------------------------------------------------------------
	protected vector NormalizeAim(vector direction)
	{
		if (direction.LengthSq() < 0.000001)
			return vector.Forward;

		direction.Normalize();
		return direction;
	}

	//------------------------------------------------------------------------------------------------
	protected vector WorldDirectionToMechLocal(vector worldDirection)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return vector.Forward;

		worldDirection = NormalizeAim(worldDirection);

		vector right = owner.GetTransformAxis(0);
		vector up = owner.GetTransformAxis(1);
		vector forward = owner.GetTransformAxis(2);

		if (right.LengthSq() > 0.000001)
			right.Normalize();

		if (up.LengthSq() > 0.000001)
			up.Normalize();

		if (forward.LengthSq() > 0.000001)
			forward.Normalize();

		vector localDirection = Vector(
			vector.Dot(worldDirection, right),
			vector.Dot(worldDirection, up),
			vector.Dot(worldDirection, forward)
		);

		return NormalizeAim(localDirection);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyAimLocalPointToMounts(string stationId, vector mechLocalPoint)
	{
		BFA_MechWeaponMountComponent mounts =
			BFA_MechWeaponMountComponent.Cast(
				GetOwner().FindComponent(BFA_MechWeaponMountComponent)
			);

		if (mounts)
			mounts.ApplyStationAimLocalPoint(stationId, SanitizeAimPoint(mechLocalPoint));
	}

	//------------------------------------------------------------------------------------------------
	//! Never trust or use a degenerate point: zero/unset becomes "straight ahead, far away".
	protected vector SanitizeAimPoint(vector mechLocalPoint)
	{
		float lengthSq = mechLocalPoint.LengthSq();
		if (lengthSq < 1.0)
			return vector.Forward * Math.Max(m_fReticleAimDistance, 10.0);

		float maxDistance = Math.Max(m_fReticleAimDistance, 10.0) * 2.0;
		if (lengthSq > maxDistance * maxDistance)
			mechLocalPoint = mechLocalPoint * (maxDistance / Math.Sqrt(lengthSq));

		return mechLocalPoint;
	}

	//------------------------------------------------------------------------------------------------
	//! Find what the crosshair is actually on, ignoring this mech and everything attached to it.
	protected vector TraceCrosshairPoint(vector camOrigin, vector camForward)
	{
		float maxDistance = Math.Max(m_fReticleAimDistance, 10.0);
		vector farPoint = camOrigin + camForward * maxDistance;

		BaseWorld world = GetGame().GetWorld();
		IEntity owner = GetOwner();
		if (!world || !owner)
			return farPoint;

		vector from = camOrigin;
		for (int i = 0; i < 6; i++)
		{
			TraceParam param = new TraceParam();
			param.Start = from;
			param.End = farPoint;
			param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			param.LayerMask = EPhysicsLayerPresets.Projectile;
			array<IEntity> excludes = { owner };
			param.ExcludeArray = excludes;

			float fraction = world.TraceMove(param, null);
			if (fraction >= 1.0)
				return farPoint;

			vector hit = param.Start + (param.End - param.Start) * fraction;

			// Skip our own arms, guns, crew and legs.
			if (param.TraceEnt && param.TraceEnt.GetRootParent() == owner)
			{
				from = hit + camForward * 0.1;
				continue;
			}

			float distance = vector.Distance(camOrigin, hit);
			float minDistance = Math.Max(m_fMinConvergenceDistance, 1.0);
			if (distance < minDistance)
				return camOrigin + camForward * minDistance;

			return hit;
		}

		return farPoint;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyAimPointToMounts(string stationId, vector aimPointWorld)
	{
		BFA_MechWeaponMountComponent mounts =
			BFA_MechWeaponMountComponent.Cast(
				GetOwner().FindComponent(BFA_MechWeaponMountComponent)
			);

		if (mounts)
			mounts.ApplyStationAimPoint(stationId, aimPointWorld);
	}

	//------------------------------------------------------------------------------------------------
	void BeginLocalAim(string stationId)
	{
		if (stationId.IsEmpty())
			stationId = "DRIVER";

		m_sLocalStation = stationId;

		m_bLocalAimActive = true;
		m_bInspectMode = false;
		m_bLoggedTracking = false;

		m_vLocalAimDirection = GetAimDirectionForStation(stationId);

		IEntity owner = GetOwner();

		if (owner)
		{
			m_vLocalAimPointWorld =
				owner.GetOrigin()
				+ LocalDirectionToWorld(m_vLocalAimDirection)
				* Math.Max(m_fReticleAimDistance, 10.0);
		}
		else
		{
			m_vLocalAimPointWorld = vector.Zero;
		}

		ScriptCallQueue queue = GetGame().GetCallqueue();

		if (queue)
		{
			queue.Remove(LocalAimTick);

			queue.CallLater(
				LocalAimTick,
				Math.Max(m_iLocalAimTickMs, 10),
				true
			);

			m_bLocalAimTickRunning = true;
		}

		BFA_MechHUD.SetLocalStation(GetOwner(), stationId);
		BFA_MechLog.Log("[BFA AIM MP] local station aim STARTED station=" + stationId);

		LocalAimTick();
	}

	//------------------------------------------------------------------------------------------------
	void EndLocalAim()
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();

		if (queue)
			queue.Remove(LocalAimTick);

		m_bLocalAimTickRunning = false;
		m_bLocalAimActive = false;
		m_bInspectMode = false;
		m_sLocalStation = string.Empty;

		if (m_CrosshairHUD)
		{
			m_CrosshairHUD.Destroy();
			m_CrosshairHUD = null;
		}
		DestroyHUDDisplay();

		if (BFA_MechHUD.GetLocalMech() == GetOwner())
			BFA_MechHUD.SetLocalStation(null, string.Empty);

		BFA_MechLog.Log("[BFA AIM MP] local station aim STOPPED");
	}

	//------------------------------------------------------------------------------------------------
	//! V52:
	//! Freelook is no longer allowed to freeze weapon tracking.
	//! We still keep this state so the camera/controller code can tell us freelook is active,
	//! but UpdateLocalAimFromCamera() ALWAYS continues updating the weapon aim.
	void SetInspectMode(bool enabled)
	{
		m_bInspectMode = enabled;
	}

	//------------------------------------------------------------------------------------------------
	protected vector LocalDirectionToWorld(vector localDirection)
	{
		IEntity owner = GetOwner();

		if (!owner)
			return vector.Forward;

		vector right = owner.GetTransformAxis(0);
		vector up = owner.GetTransformAxis(1);
		vector forward = owner.GetTransformAxis(2);

		if (right.LengthSq() > 0.000001)
			right.Normalize();

		if (up.LengthSq() > 0.000001)
			up.Normalize();

		if (forward.LengthSq() > 0.000001)
			forward.Normalize();

		vector worldDirection =
			right * localDirection[0]
			+ up * localDirection[1]
			+ forward * localDirection[2];

		return NormalizeAim(worldDirection);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateLocalAimFromCamera()
	{
		if (!m_bLocalAimActive)
			return;

		if (System.IsConsoleApp())
			return;

		// V52 IMPORTANT:
		// DO NOT stop updating aim while DRIVER freelook is active.
		//
		// Previously this code did:
		//
		// if (m_bInspectMode && m_sLocalStation == "DRIVER")
		//     return;
		//
		// That was the reason the camera could look down while the guns remained frozen.

		BaseWorld world = GetGame().GetWorld();

		if (!world)
			return;

		vector cam[4];
		world.GetCurrentCamera(cam);

		vector camForward = cam[2];

		if (camForward.LengthSq() < 0.000001)
			return;

		camForward.Normalize();

		// Store the camera direction relative to the mech.
		// This is what gets sent across the network.
		m_vLocalAimDirection = WorldDirectionToMechLocal(camForward);

		// Exact point underneath the local player's crosshair: whatever the sight line actually
		// hits, so both arms converge on the target instead of on a fixed point 1000 m away.
		m_vLocalAimPointWorld = TraceCrosshairPoint(cam[3], camForward);

		// Continuously drive all mounts owned by this crew station.
		// This now happens during freelook too.
		ApplyAimPointToMounts(
			m_sLocalStation,
			m_vLocalAimPointWorld
		);

		if (!m_bLoggedTracking)
		{
			m_bLoggedTracking = true;

			BFA_MechLog.Log(
				"[BFA AIM] camera-point tracking active station="
				+ m_sLocalStation
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSight()
	{
		if (!m_bLocalAimActive)
			return;

		if (System.IsConsoleApp())
			return;

		BaseWorld world = GetGame().GetWorld();

		if (!world)
			return;

		vector cam[4];
		world.GetCurrentCamera(cam);

		vector camForward = cam[2];
		vector camRight = cam[0];
		vector camUp = cam[1];

		if (
			camForward.LengthSq() < 0.000001
			|| camRight.LengthSq() < 0.000001
			|| camUp.LengthSq() < 0.000001
		)
		{
			return;
		}

		camForward.Normalize();
		camRight.Normalize();
		camUp.Normalize();

		int flags =
			ShapeFlags.ONCE
			| ShapeFlags.NOZBUFFER;

		const float planeDistance = 2.0;

		vector sightCenter =
			cam[3]
			+ camForward
			* planeDistance;

		// Yellow camera crosshair.
		Shape.CreateSphere(
			Color.YELLOW,
			flags,
			sightCenter + camRight * 0.030,
			0.006
		);

		Shape.CreateSphere(
			Color.YELLOW,
			flags,
			sightCenter - camRight * 0.030,
			0.006
		);

		Shape.CreateSphere(
			Color.YELLOW,
			flags,
			sightCenter + camUp * 0.030,
			0.006
		);

		Shape.CreateSphere(
			Color.YELLOW,
			flags,
			sightCenter - camUp * 0.030,
			0.006
		);

		BFA_MechWeaponMountComponent mounts =
			BFA_MechWeaponMountComponent.Cast(
				GetOwner().FindComponent(BFA_MechWeaponMountComponent)
			);

		if (mounts)
		{
			mounts.DrawStationConvergence(
				m_sLocalStation,
				cam[3],
				camRight,
				camUp,
				camForward,
				planeDistance,
				m_fReticleAimDistance
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void LocalAimTick()
	{
		if (!m_bLocalAimActive)
			return;

		UpdateLocalAimFromCamera();
		UpdateCrosshair();
		UpdateHUDDisplay();

		if (m_bDebugSight)
			DrawSight();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHUDDisplay()
	{
		if (System.IsConsoleApp())
			return;

		bool wanted = m_bLocalAimActive && m_bShowHUD && BFA_MechHUD.IsBuiltInHUDEnabled();
		if (!wanted)
		{
			DestroyHUDDisplay();
			return;
		}

		if (!m_HUDDisplay)
			m_HUDDisplay = new BFA_MechHUDDisplay();

		m_HUDDisplay.Update(GetOwner(), m_sLocalStation);
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyHUDDisplay()
	{
		if (m_HUDDisplay)
		{
			m_HUDDisplay.Destroy();
			m_HUDDisplay = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Real on-screen crosshair (UI widgets, visible to players in MP). Uses the selected weapon
	//! group's crosshair; gun markers show where each gun in that group actually points.
	protected void UpdateCrosshair()
	{
		if (System.IsConsoleApp() || !m_bLocalAimActive)
			return;

		if (!m_bShowCrosshair || !BFA_MechHUD.IsBuiltInCrosshairEnabled())
		{
			if (m_CrosshairHUD)
			{
				m_CrosshairHUD.Destroy();
				m_CrosshairHUD = null;
			}
			return;
		}

		IEntity owner = GetOwner();
		if (!owner)
			return;

		BFA_MechCrosshair config = m_DefaultCrosshair;
		bool markers = m_bShowGunMarkers;
		m_aGunPoints.Clear();

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(owner.FindComponent(BFA_MechCoreComponent));
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(owner.FindComponent(BFA_MechHardpointComponent));

		// Stations that cannot shoot (a pure driver) get no crosshair.
		if (core && !core.CanStationAim(m_sLocalStation))
		{
			if (m_CrosshairHUD)
			{
				m_CrosshairHUD.Destroy();
				m_CrosshairHUD = null;
			}
			return;
		}

		if (core && hardpoints)
		{
			int group = core.GetSelectedWeaponGroup(m_sLocalStation);
			bool picked = false;

			for (int hp = 0; hp < hardpoints.GetHardpointCount(); hp++)
			{
				BFA_MechWeaponModuleComponent module = hardpoints.GetWeaponModuleAt(hp);
				if (!module || !module.MatchesFireRoute(m_sLocalStation, group))
					continue;

				if (!picked)
				{
					picked = true;
					if (module.GetCrosshair())
						config = module.GetCrosshair();
				}

				m_aGunPoints.Insert(module.GetPointedPoint());
			}
		}

		if (config)
			markers = markers && config.m_bShowGunMarkers;

		if (!m_CrosshairHUD)
			m_CrosshairHUD = new BFA_MechCrosshairHUD();

		m_CrosshairHUD.Update(config, m_vLocalAimPointWorld, m_aGunPoints, markers);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_bLocalAimActive && !m_bLocalAimTickRunning)
			LocalAimTick();
	}

	//------------------------------------------------------------------------------------------------
	vector GetLocalAimDirection()
	{
		return NormalizeAim(m_vLocalAimDirection);
	}

	//------------------------------------------------------------------------------------------------
	vector GetLocalAimPointWorld()
	{
		return m_vLocalAimPointWorld;
	}

	//------------------------------------------------------------------------------------------------
	//! What the network carries: the crosshair point in this mech's local space.
	vector GetLocalAimPointMechLocal()
	{
		IEntity owner = GetOwner();
		if (!owner || m_vLocalAimPointWorld == vector.Zero)
			return SanitizeAimPoint(vector.Zero);

		return SanitizeAimPoint(owner.CoordToLocal(m_vLocalAimPointWorld));
	}

	//------------------------------------------------------------------------------------------------
	vector GetAimLocalPointForStation(string stationId)
	{
		if (m_bLocalAimActive && stationId == m_sLocalStation)
			return GetLocalAimPointMechLocal();

		return SanitizeAimPoint(GetRepAim(StationIndex(stationId)));
	}

	//------------------------------------------------------------------------------------------------
	string GetLocalAimStation()
	{
		return m_sLocalStation;
	}

	//------------------------------------------------------------------------------------------------
	vector GetAimDirectionForStation(string stationId)
	{
		if (
			m_bLocalAimActive
			&& stationId == m_sLocalStation
		)
		{
			return NormalizeAim(m_vLocalAimDirection);
		}

		return NormalizeAim(GetAimLocalPointForStation(stationId));
	}

	//------------------------------------------------------------------------------------------------
	//! mechLocalPoint: the station's crosshair point in this mech's local space.
	void ServerSetStationAim(
		string stationId,
		vector mechLocalPoint
	)
	{
		if (!Replication.IsServer())
			return;

		mechLocalPoint = SanitizeAimPoint(mechLocalPoint);

		int index = StationIndex(stationId);
		if (index < 0)
			return;

		SetRepAim(index, mechLocalPoint);

		Replication.BumpMe();

		// Local pilot/gunner already has exact camera-point prediction.
		// Server and other clients use the replicated point instead.
		if (
			!m_bLocalAimActive
			|| m_sLocalStation != stationId
		)
		{
			ApplyAimLocalPointToMounts(
				stationId,
				mechLocalPoint
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRepAimChanged()
	{
		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(GetOwner().FindComponent(BFA_MechCoreComponent));
		if (!core)
			return;

		array<string> stationIds = {};
		core.GetStationIds(stationIds);
		foreach (string stationId : stationIds)
		{
			if (m_bLocalAimActive && m_sLocalStation == stationId)
				continue;

			int index = core.GetStationIndex(stationId);
			if (index < 0)
				continue;

			ApplyAimLocalPointToMounts(stationId, GetRepAim(index));
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		SetEventMask(
			owner,
			EntityEvent.FRAME
		);

		m_vLocalAimDirection = vector.Forward;
		m_vLocalAimPointWorld = vector.Zero;

		m_sLocalStation = string.Empty;

		m_bLocalAimActive = false;
		m_bInspectMode = false;
		m_bLocalAimTickRunning = false;
		m_bLoggedTracking = false;

		BFA_MechLog.Log(
			"[BFA AIM] V52 FREELOOK_TRACKS_GUNS initialized"
		);
	}

	//------------------------------------------------------------------------------------------------
	//! Stop every repeating/delayed call before this component goes away. Without this the
	//! call queue keeps invoking methods on a destroyed mech (death, despawn, GC cleanup).
	override void OnDelete(IEntity owner)
	{
		if (m_CrosshairHUD)
		{
			m_CrosshairHUD.Destroy();
			m_CrosshairHUD = null;
		}
		DestroyHUDDisplay();

		if (GetGame())
		{
			// Removes LocalAimTick and clears local aim state.
			EndLocalAim();
		}

		super.OnDelete(owner);
	}
}
