class BFA_MechControllerComponentClass : VehicleControllerComponentClass
{
}

class BFA_MechControllerComponent : VehicleControllerComponent
{
	[Attribute(
		"true",
		UIWidgets.CheckBox,
		"Hide Pilot in Mech, eg if no windows to just control the Mech"
	)]
	protected bool m_bHidePilot;

	[Attribute(
		"33",
		UIWidgets.EditBox,
		"Milliseconds between multiplayer input packets. 33 = about 30 updates/sec.",
		category: "Multiplayer"
	)]
	protected int m_iNetInputIntervalMs;

	[Attribute(
		"16",
		UIWidgets.EditBox,
		"Milliseconds between local input polls. This does not depend on vehicle EOnFrame, which can stop on dedicated-server clients.",
		category: "Multiplayer"
	)]
	protected int m_iLocalInputPollMs;

	[Attribute(
		"DRIVER",
		UIWidgets.EditBox,
		"Crew station controlled by this vehicle controller.",
		category: "MechCore"
	)]
	protected string m_sCrewStationId;

	[Attribute(
		"1",
		UIWidgets.Auto,
		"Primary weapon group fired by CharacterFire for this station.",
		category: "MechCore"
	)]
	protected int m_iPrimaryWeaponGroup;

	[Attribute("true", UIWidgets.CheckBox, "Use the MECH control scheme (own keys for keyboard + controller, rebindable in Settings > Controls > Mech). Off = old borrowed infantry keys.", category: "MechCore")]
	protected bool m_bUseMechControls;

	protected float m_fMechLastFire = -1;
	protected bool m_bMechInputLogged;

	[Attribute("CharacterReload", UIWidgets.EditBox, "Input that cycles field loadout presets for the DRIVER (single-pilot mechs). Empty = no key. Only works if the hardpoint component's m_sSwapStation allows the driver.", category: "MechCore")]
	protected string m_sLoadoutAction;

	protected BFA_MechMovementComponent m_MovementComp;
	protected BFA_SentinelJumpJetComponent m_JumpJetComp;
	protected BFA_MechAimComponent m_AimComp;
	protected BFA_MechWeaponComponent m_WeaponComp;

	protected bool m_bLocalPilot;
	protected bool m_bJumpHeld;
	protected bool m_bSprintHeld;
	protected bool m_bInspectHeld;
	protected bool m_bFireHeld;

	protected int m_iHeldPrimaryWeaponGroup;

	protected bool m_bLoggedStrafeInput;
	protected bool m_bLoggedTurnInput;
	protected bool m_bLoggedControllerTurn;

	protected float m_fLocalForward;
	protected float m_fLocalStrafe;
	protected float m_fLocalTurn;
	protected float m_fTurnLeft;
	protected float m_fTurnRight;

	protected IEntity m_pLocalPilotEntity;

	// Local cockpit camera ONLY. The torso is its own visual prefab, so the
	// chassis, arms, legs and hardpoint weapons are NEVER touched.
	[Attribute("true", UIWidgets.CheckBox, "Hide ONLY the visual in the Torso slot when the local crew camera is in first person. Third person and outside views show the complete mech.", category: "Cockpit")]
	protected bool m_bHideExteriorInPilotView;

	[Attribute("Torso", UIWidgets.EditBox, "Slot containing the separate exterior BODY visual (not the cockpit, legs or arm meshes). Leave as Torso for standard frames.", category: "Cockpit")]
	protected string m_sPilotBodySlot;

	protected IEntity m_pCGSHiddenBody;
	protected bool m_bCGSLocalCrewSeat;
	protected int m_iCGSPilotNextVisibilityScan;
	protected IEntity m_pPendingPilotEntity;
	protected IEntity m_pPendingPilotVehicle;

	protected int m_iPendingPilotResolveAttempts;

	//------------------------------------------------------------------------------------------------
	protected void ActivateMechInputContexts(InputManager input)
	{
		if (!input)
			return;

		if (m_bUseMechControls)
		{
			// The Mech actions live in CharacterGeneralContext (adding a brand-new context did not work).
			input.ActivateContext("CharacterGeneralContext");
			// Q/E: vanilla Lean Adjust keeps Q/E, so it is read as a second turn source.
			input.ActivateContext("CharacterMovementLeanContext");
			// Weapon context too: the normal fire key is read as a backup to Mech Fire.
			input.ActivateContext("CharacterWeaponContext");
			input.ActivateContext("PlayerCameraContext");
			return;
		}

		input.ActivateContext("CharacterGeneralContext");
		input.ActivateContext("CharacterMovementContext");
		input.ActivateContext("CharacterMovementLeanContext");
		input.ActivateContext("CharacterWeaponContext");
		input.ActivateContext("PlayerCameraContext");
	}

	//------------------------------------------------------------------------------------------------
	//! MECH CONTROLS: every frame the driver's keys / sticks are read from the Mech actions.
	//! Keyboard and controller share the same actions (see Configs/System/chimeraInputCommon.conf).
	protected void MechControlsTick(InputManager input)
	{
		float forward = input.GetActionValue("MechMove") - input.GetActionValue("MechMoveBack");

		if (!m_bMechInputLogged && (Math.AbsFloat(forward) > 0.05 || input.GetActionValue("MechTurn") + input.GetActionValue("MechTurnLeft") > 0.05))
		{
			m_bMechInputLogged = true;
			Print("[BFA INPUT] Mech controls working (MechMove / MechTurn received).");
		}
		OnMoveForward(Math.Clamp(forward, -1.0, 1.0));

		float strafe = Math.Clamp(input.GetActionValue("MechStrafe") - input.GetActionValue("MechStrafeLeft"), -1.0, 1.0);
		m_fLocalStrafe = strafe;
		if (m_MovementComp)
			m_MovementComp.SetRightInput(strafe);

		float turn = input.GetActionValue("MechTurn") - input.GetActionValue("MechTurnLeft");
		turn += input.GetActionValue("CharacterLeanAdjustRight") - input.GetActionValue("CharacterLeanAdjustLeft");
		turn = Math.Clamp(turn, -1.0, 1.0);
		OnTurnRight(Math.Max(turn, 0.0));
		OnTurnLeft(Math.Max(-turn, 0.0));

		if (input.GetActionValue("MechSprint") > 0.5)
			OnSprintInput(1.0);
		else
			OnSprintInput(0.0);

		OnJumpInput(input.GetActionValue("MechJump"));
		OnFreelookInput(input.GetActionValue("MechFreelook"));

		float fire = Math.Max(input.GetActionValue("MechFire"), input.GetActionValue("CharacterFire"));
		if (fire != m_fMechLastFire)
		{
			m_fMechLastFire = fire;
			OnFireInput(fire);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLocalOccupant(
		IEntity vehicle,
		IEntity occupant
	)
	{
		if (!vehicle || !occupant || !GetGame())
			return false;

		PlayerController localPC =
			GetGame().GetPlayerController();

		if (!localPC)
			return false;

		PlayerManager playerManager =
			GetGame().GetPlayerManager();

		if (playerManager)
		{
			int occupantPlayerId =
				playerManager.GetPlayerIdFromControlledEntity(
					occupant
				);

			if (occupantPlayerId > 0)
			{
				PlayerController occupantPC =
					playerManager.GetPlayerController(
						occupantPlayerId
					);

				if (
					occupantPC
					&& occupantPC == localPC
				)
				{
					return true;
				}
			}
		}

		IEntity controlled =
			localPC.GetControlledEntity();

		if (!controlled)
		{
			controlled =
				SCR_PlayerController.GetLocalControlledEntity();
		}

		if (!controlled)
			return false;

		if (
			controlled == occupant
			|| controlled == vehicle
		)
		{
			return true;
		}

		IEntity controlledRoot =
			controlled.GetRootParent();

		if (controlledRoot == vehicle)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueLocalPilotResolve(
		IEntity vehicle,
		IEntity occupant
	)
	{
		if (!vehicle || !occupant)
			return;

		m_pPendingPilotVehicle = vehicle;
		m_pPendingPilotEntity = occupant;
		m_iPendingPilotResolveAttempts = 0;

		ScriptCallQueue queue =
			GetGame().GetCallqueue();

		if (!queue)
			return;

		queue.Remove(
			ResolveLocalPilotDelayed
		);

		queue.CallLater(
			ResolveLocalPilotDelayed,
			0,
			false
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveLocalPilotDelayed()
	{
		if (
			m_bLocalPilot
			|| !m_pPendingPilotVehicle
			|| !m_pPendingPilotEntity
		)
		{
			return;
		}

		if (
			IsLocalOccupant(
				m_pPendingPilotVehicle,
				m_pPendingPilotEntity
			)
		)
		{
			StartLocalInput(
				m_pPendingPilotEntity
			);

			m_pPendingPilotVehicle = null;
			m_pPendingPilotEntity = null;

			return;
		}

		m_iPendingPilotResolveAttempts++;

		if (
			m_iPendingPilotResolveAttempts >= 20
		)
		{
			m_pPendingPilotVehicle = null;
			m_pPendingPilotEntity = null;

			return;
		}

		ScriptCallQueue queue =
			GetGame().GetCallqueue();

		if (queue)
		{
			queue.CallLater(
				ResolveLocalPilotDelayed,
				100,
				false
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnMoveForward(float value)
	{
		m_fLocalForward =
			Math.Clamp(
				value,
				-1.0,
				1.0
			);

		if (m_MovementComp)
		{
			m_MovementComp.SetForwardInput(
				m_fLocalForward
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnMoveRight(float value)
	{
		InputManager input =
			GetGame().GetInputManager();

		if (
			input
			&& input.GetLastUsedInputDevice()
				== EInputDeviceType.GAMEPAD
		)
		{
			return;
		}

		m_fLocalStrafe =
			Math.Clamp(
				value,
				-1.0,
				1.0
			);

		if (m_MovementComp)
		{
			m_MovementComp.SetRightInput(
				m_fLocalStrafe
			);
		}

		if (
			!m_bLoggedStrafeInput
			&& Math.AbsFloat(m_fLocalStrafe) > 0.05
		)
		{
			m_bLoggedStrafeInput = true;

			BFA_MechLog.Log(
				"[BFA INPUT] A/D strafe action active"
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildTurnInput()
	{
		m_fLocalTurn =
			Math.Clamp(
				m_fTurnRight - m_fTurnLeft,
				-1.0,
				1.0
			);

		if (m_MovementComp)
		{
			m_MovementComp.SetTurnInput(
				m_fLocalTurn
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnTurnLeft(float value)
	{
		m_fTurnLeft =
			Math.Clamp(
				Math.AbsFloat(value),
				0.0,
				1.0
			);

		RebuildTurnInput();

		if (
			!m_bLoggedTurnInput
			&& m_fTurnLeft > 0.05
		)
		{
			m_bLoggedTurnInput = true;

			BFA_MechLog.Log(
				"[BFA INPUT] chassis turn action active"
			);
		}

		InputManager input =
			GetGame().GetInputManager();

		if (
			input
			&& input.GetLastUsedInputDevice()
				== EInputDeviceType.GAMEPAD
			&& m_fTurnLeft > 0.05
			&& !m_bLoggedControllerTurn
		)
		{
			m_bLoggedControllerTurn = true;

			BFA_MechLog.Log(
				"[BFA GAMEPAD] left stick X -> chassis turn"
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnTurnRight(float value)
	{
		m_fTurnRight =
			Math.Clamp(
				Math.AbsFloat(value),
				0.0,
				1.0
			);

		RebuildTurnInput();

		if (
			!m_bLoggedTurnInput
			&& m_fTurnRight > 0.05
		)
		{
			m_bLoggedTurnInput = true;

			BFA_MechLog.Log(
				"[BFA INPUT] chassis turn action active"
			);
		}

		InputManager input =
			GetGame().GetInputManager();

		if (
			input
			&& input.GetLastUsedInputDevice()
				== EInputDeviceType.GAMEPAD
			&& m_fTurnRight > 0.05
			&& !m_bLoggedControllerTurn
		)
		{
			m_bLoggedControllerTurn = true;

			BFA_MechLog.Log(
				"[BFA GAMEPAD] left stick X -> chassis turn"
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnSprintInput(float value)
	{
		m_bSprintHeld =
			value > 0.5;

		if (m_MovementComp)
		{
			m_MovementComp.SetSprintInput(
				m_bSprintHeld
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnJumpInput(float value)
	{
		bool pressed =
			value > 0.5;

		if (
			pressed
			&& !m_bJumpHeld
		)
		{
			SCR_PlayerController pc =
				SCR_PlayerController.Cast(
					GetGame().GetPlayerController()
				);

			if (pc)
			{
				pc.BFA_Mech_RequestJump(
					GetOwner()
				);
			}
		}

		m_bJumpHeld = pressed;
	}

	//------------------------------------------------------------------------------------------------
	void OnFireInput(float value)
	{
		SetLocalFireHeld(
			value > 0.5
		);
	}

	//------------------------------------------------------------------------------------------------
	protected int ResolveSelectedPrimaryWeaponGroup()
	{
		BFA_MechCoreComponent core =
			BFA_MechCoreComponent.Cast(
				GetOwner().FindComponent(
					BFA_MechCoreComponent
				)
			);

		if (core)
		{
			int selected =
				core.GetSelectedWeaponGroup(
					m_sCrewStationId
				);

			if (selected > 0)
				return selected;
		}

		return m_iPrimaryWeaponGroup;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLocalFireHeld(bool held)
	{
		if (held == m_bFireHeld)
			return;

		SCR_PlayerController pc =
			SCR_PlayerController.Cast(
				GetGame().GetPlayerController()
			);

		if (!pc)
			return;

		if (held)
		{
			m_iHeldPrimaryWeaponGroup =
				ResolveSelectedPrimaryWeaponGroup();

			if (
				m_iHeldPrimaryWeaponGroup < 1
			)
			{
				return;
			}
		}

		int group =
			m_iHeldPrimaryWeaponGroup;

		if (group < 1)
		{
			group =
				ResolveSelectedPrimaryWeaponGroup();
		}

		if (group < 1)
			return;

		m_bFireHeld = held;

		BFA_MechLog.Log(
			string.Format(
				"[BFA FIRE INPUT] held=%1 group=%2",
				m_bFireHeld,
				group
			)
		);

		pc.BFA_Mech_RequestFireState(
			GetOwner(),
			m_sCrewStationId,
			group,
			m_bFireHeld
		);

		if (!held)
			m_iHeldPrimaryWeaponGroup = 0;
	}

	//------------------------------------------------------------------------------------------------
	void OnToggleMissileBarrageInput()
	{
		if (!m_bLocalPilot || m_bFireHeld)
			return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestToggleMissileBarrage(GetOwner(), m_sCrewStationId, ResolveSelectedPrimaryWeaponGroup());
	}

	//------------------------------------------------------------------------------------------------
	void OnLoadoutInput()
	{
		if (!m_bLocalPilot)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestLoadoutPreset(GetOwner(), -1);
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Direct number-key group selection. Keep B to cycle; never switch mid-shot.
	protected void SelectNumberedWeaponGroup(int group)
	{
		if (!m_bLocalPilot || m_bFireHeld)
			return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestSelectWeaponGroup(GetOwner(), m_sCrewStationId, group);
	}

	void OnSelectMechGroup1()
	{
		SelectNumberedWeaponGroup(1);
	}

	void OnSelectMechGroup2()
	{
		SelectNumberedWeaponGroup(2);
	}

	void OnSelectMechGroup3()
	{
		SelectNumberedWeaponGroup(3);
	}

	void OnSelectMechGroup4()
	{
		SelectNumberedWeaponGroup(4);
	}

	void OnSelectMechGroup5()
	{
		SelectNumberedWeaponGroup(5);
	}

	void OnSelectMechGroup6()
	{
		SelectNumberedWeaponGroup(6);
	}

	void OnCycleWeaponInput()
	{
		if (
			!m_bLocalPilot
			|| m_bFireHeld
		)
		{
			return;
		}

		SCR_PlayerController pc =
			SCR_PlayerController.Cast(
				GetGame().GetPlayerController()
			);

		if (pc)
		{
			pc.BFA_Mech_RequestCycleWeaponGroup(
				GetOwner(),
				m_sCrewStationId
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnFreelookInput(float value)
	{
		bool inspect =
			value > 0.5;

		if (
			inspect == m_bInspectHeld
		)
		{
			return;
		}

		m_bInspectHeld = inspect;

		// V52:
		// Aim component now continues reading the camera even while
		// freelook is active. This state is informational only.
		if (m_AimComp)
		{
			m_AimComp.SetInspectMode(
				m_bInspectHeld
			);
		}

		if (m_bInspectHeld)
		{
			BFA_MechLog.Log(
				"[BFA CAMERA] FREELOOK active - gun aim TRACKING camera"
			);
		}
		else
		{
			BFA_MechLog.Log(
				"[BFA CAMERA] FREELOOK released - gun aim still TRACKING camera"
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The local gunner seat has a separate controller. Share this exact
	//! view state rather than duplicating visual-restore logic across stations.
	void CGS_BeginLocalCrewView()
	{
		m_bCGSLocalCrewSeat = true;
		CGS_RefreshPilotExteriorMask();
	}

	void CGS_EndLocalCrewView()
	{
		m_bCGSLocalCrewSeat = false;
		if (!m_bLocalPilot)
			CGS_RestorePilotExteriorMask();
	}

	//------------------------------------------------------------------------------------------------
	//! The character camera API reports the player's actual 1P/3P view. Merely
	//! being seated must NEVER hide the body while using an outside camera.
	//! If local camera state is unavailable, default to showing the whole mech.
	protected bool CGS_IsLocalFirstPerson()
	{
		if (!GetGame())
			return false;

		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;

		CharacterControllerComponent characterController;
		IEntity controlled = pc.GetControlledEntity();
		if (controlled)
			characterController = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
		if (!characterController && m_pLocalPilotEntity)
			characterController = CharacterControllerComponent.Cast(m_pLocalPilotEntity.FindComponent(CharacterControllerComponent));
		if (!characterController)
			return false;

		CameraHandlerComponent camera = characterController.GetCameraHandlerComponent();
		if (camera)
			return !camera.IsInThirdPerson();

		return !characterController.IsInThirdPersonView();
	}

	//------------------------------------------------------------------------------------------------
	//! Hide only one visual: the dedicated Torso slot's entity. NEVER walk
	//! child meshes, hide the root XOB, or change arms, legs, guns and cockpit.
	//! This affects only the local player's render flags, not the game server.
	void CGS_RefreshPilotExteriorMask()
	{
		if ((!m_bLocalPilot && !m_bCGSLocalCrewSeat) || !GetGame())
			return;

		int now = System.GetTickCount();
		if (now < m_iCGSPilotNextVisibilityScan)
			return;
		m_iCGSPilotNextVisibilityScan = now + 100;

		if (!m_bHideExteriorInPilotView || !CGS_IsLocalFirstPerson())
		{
			CGS_RestorePilotExteriorMask();
			return;
		}

		IEntity mech = GetOwner();
		if (!mech || m_sPilotBodySlot.IsEmpty())
			return;

		SlotManagerComponent slots = SlotManagerComponent.Cast(mech.FindComponent(SlotManagerComponent));
		if (!slots)
			return;

		EntitySlotInfo slot = slots.GetSlotByName(m_sPilotBodySlot);
		if (!slot)
			return;

		IEntity body = slot.GetAttachedEntity();
		if (!body || body.FindComponent(CGS_MechCockpitInteriorComponent))
		{
			CGS_RestorePilotExteriorMask();
			return;
		}

		if (m_pCGSHiddenBody == body)
			return;

		// Refit or changed frame: restore the previous entity before hiding
		// the replacement. Do not override somebody else's hidden state.
		CGS_RestorePilotExteriorMask();
		if (!body.GetVObject() || (body.GetFlags() & EntityFlags.VISIBLE) != EntityFlags.VISIBLE)
			return;

		m_pCGSHiddenBody = body;
		body.ClearFlags(EntityFlags.VISIBLE, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Restore body whenever switching to third person, leaving a seat,
	//! respawning or changing views. No root SetObject calls; avoids breaking
	//! CGS_TorsoTwistComponent's split torso and existing destruction models.
	protected void CGS_RestorePilotExteriorMask()
	{
		if (m_pCGSHiddenBody && m_pCGSHiddenBody.GetParent())
			m_pCGSHiddenBody.SetFlags(EntityFlags.VISIBLE, false);

		m_pCGSHiddenBody = null;
		m_iCGSPilotNextVisibilityScan = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void LocalInputTick()
	{
		if (!m_bLocalPilot)
			return;

		CGS_RefreshPilotExteriorMask();

		InputManager input =
			GetGame().GetInputManager();

		if (!input)
			return;

		ActivateMechInputContexts(
			input
		);

		if (m_bUseMechControls)
		{
			MechControlsTick(input);
			return;
		}

		OnMoveForward(
			Math.Clamp(
				input.GetActionValue(
					"CharacterForward"
				),
				-1.0,
				1.0
			)
		);

		if (
			input.GetActionValue(
				"CharacterSprint"
			) > 0.5
		)
		{
			OnSprintInput(1.0);
		}
		else
		{
			OnSprintInput(0.0);
		}

		OnJumpInput(
			input.GetActionValue(
				"CharacterJump"
			)
		);

		OnFreelookInput(
			input.GetActionValue(
				"Freelook"
			)
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void NetInputTick()
	{
		if (!m_bLocalPilot)
			return;

		SCR_PlayerController pc =
			SCR_PlayerController.Cast(
				GetGame().GetPlayerController()
			);

		if (!pc)
			return;

		pc.BFA_Mech_SendDriverInput(
			GetOwner(),
			m_fLocalForward,
			m_fLocalStrafe,
			m_fLocalTurn,
			m_bSprintHeld
		);

		if (m_AimComp)
		{
			pc.BFA_Mech_SendAimInput(
				GetOwner(),
				m_sCrewStationId,
				m_AimComp.GetLocalAimPointMechLocal()
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StartLocalInput(
		IEntity occupant
	)
	{
		if (m_bLocalPilot)
			return;

		m_bLocalPilot = true;
		m_pLocalPilotEntity = occupant;
		CGS_RefreshPilotExteriorMask();

		m_bJumpHeld = false;
		m_bSprintHeld = false;
		m_bInspectHeld = false;
		m_bFireHeld = false;

		m_iHeldPrimaryWeaponGroup = 0;

		m_bLoggedStrafeInput = false;
		m_bLoggedTurnInput = false;
		m_bLoggedControllerTurn = false;

		m_fLocalForward = 0;
		m_fLocalStrafe = 0;
		m_fLocalTurn = 0;
		m_fTurnLeft = 0;
		m_fTurnRight = 0;

		if (m_MovementComp)
		{
			m_MovementComp.SetForwardInput(0);
			m_MovementComp.SetRightInput(0);
			m_MovementComp.SetTurnInput(0);
			m_MovementComp.SetSprintInput(false);
		}

		if (m_AimComp)
		{
			m_AimComp.BeginLocalAim(
				m_sCrewStationId
			);
		}

		InputManager input =
			GetGame().GetInputManager();

		ActivateMechInputContexts(
			input
		);

		if (input && m_bUseMechControls)
		{
			m_fMechLastFire = -1;
			input.AddActionListener("MechNextGroup", EActionTrigger.DOWN, OnCycleWeaponInput);
			input.AddActionListener("MechSelectGroup1", EActionTrigger.DOWN, OnSelectMechGroup1);
			input.AddActionListener("MechSelectGroup2", EActionTrigger.DOWN, OnSelectMechGroup2);
			input.AddActionListener("MechSelectGroup3", EActionTrigger.DOWN, OnSelectMechGroup3);
			input.AddActionListener("MechSelectGroup4", EActionTrigger.DOWN, OnSelectMechGroup4);
			input.AddActionListener("MechSelectGroup5", EActionTrigger.DOWN, OnSelectMechGroup5);
			input.AddActionListener("MechSelectGroup6", EActionTrigger.DOWN, OnSelectMechGroup6);
			input.AddActionListener("MechLoadout", EActionTrigger.DOWN, OnLoadoutInput);
			input.AddActionListener("MechMissileMode", EActionTrigger.DOWN, OnToggleMissileBarrageInput);
		}
		else if (input)
		{
			input.AddActionListener(
				"CharacterForward",
				EActionTrigger.VALUE,
				OnMoveForward
			);

			input.AddActionListener(
				"CharacterRight",
				EActionTrigger.VALUE,
				OnMoveRight
			);

			input.AddActionListener(
				"CharacterLeanAdjustLeft",
				EActionTrigger.VALUE,
				OnTurnLeft
			);

			input.AddActionListener(
				"CharacterLeanAdjustRight",
				EActionTrigger.VALUE,
				OnTurnRight
			);

			input.AddActionListener(
				"CharacterSprint",
				EActionTrigger.VALUE,
				OnSprintInput
			);

			input.AddActionListener(
				"CharacterJump",
				EActionTrigger.VALUE,
				OnJumpInput
			);

			input.AddActionListener(
				"Freelook",
				EActionTrigger.VALUE,
				OnFreelookInput
			);

			input.AddActionListener(
				"CharacterFire",
				EActionTrigger.VALUE,
				OnFireInput
			);

			input.AddActionListener(
				"CharacterNextFireMode",
				EActionTrigger.DOWN,
				OnCycleWeaponInput
			);

			if (!m_sLoadoutAction.IsEmpty())
				input.AddActionListener(m_sLoadoutAction, EActionTrigger.DOWN, OnLoadoutInput);
		}

		SCR_PlayerController pc =
			SCR_PlayerController.Cast(
				GetGame().GetPlayerController()
			);

		if (pc)
		{
			pc.BFA_Mech_RequestPilotState(
				GetOwner(),
				true
			);
		}

		ScriptCallQueue queue =
			GetGame().GetCallqueue();

		if (queue)
		{
			queue.Remove(LocalInputTick);
			queue.Remove(NetInputTick);

			queue.CallLater(
				LocalInputTick,
				Math.Max(
					m_iLocalInputPollMs,
					10
				),
				true
			);

			queue.CallLater(
				NetInputTick,
				Math.Max(
					m_iNetInputIntervalMs,
					20
				),
				true
			);
		}

		BFA_MechLog.Log(
			"[BFA MP LOCAL] local pilot controls STARTED"
		);

		LocalInputTick();
		NetInputTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopLocalInput(
		IEntity vehicle
	)
	{
		if (!m_bLocalPilot)
			return;

		if (!m_bCGSLocalCrewSeat)
			CGS_RestorePilotExteriorMask();

		ScriptCallQueue queue =
			GetGame().GetCallqueue();

		if (queue)
		{
			queue.Remove(LocalInputTick);
			queue.Remove(NetInputTick);
			queue.Remove(ResolveLocalPilotDelayed);
		}

		InputManager input =
			GetGame().GetInputManager();

		if (input)
		{
			input.RemoveActionListener("MechNextGroup", EActionTrigger.DOWN, OnCycleWeaponInput);
			input.RemoveActionListener("MechSelectGroup1", EActionTrigger.DOWN, OnSelectMechGroup1);
			input.RemoveActionListener("MechSelectGroup2", EActionTrigger.DOWN, OnSelectMechGroup2);
			input.RemoveActionListener("MechSelectGroup3", EActionTrigger.DOWN, OnSelectMechGroup3);
			input.RemoveActionListener("MechSelectGroup4", EActionTrigger.DOWN, OnSelectMechGroup4);
			input.RemoveActionListener("MechSelectGroup5", EActionTrigger.DOWN, OnSelectMechGroup5);
			input.RemoveActionListener("MechSelectGroup6", EActionTrigger.DOWN, OnSelectMechGroup6);
			input.RemoveActionListener("MechLoadout", EActionTrigger.DOWN, OnLoadoutInput);
			input.RemoveActionListener("MechMissileMode", EActionTrigger.DOWN, OnToggleMissileBarrageInput);
			if (m_bUseMechControls && m_fMechLastFire > 0.5)
				OnFireInput(0);
			m_fMechLastFire = -1;

			input.RemoveActionListener(
				"CharacterForward",
				EActionTrigger.VALUE,
				OnMoveForward
			);

			input.RemoveActionListener(
				"CharacterRight",
				EActionTrigger.VALUE,
				OnMoveRight
			);

			input.RemoveActionListener(
				"CharacterLeanAdjustLeft",
				EActionTrigger.VALUE,
				OnTurnLeft
			);

			input.RemoveActionListener(
				"CharacterLeanAdjustRight",
				EActionTrigger.VALUE,
				OnTurnRight
			);

			input.RemoveActionListener(
				"CharacterSprint",
				EActionTrigger.VALUE,
				OnSprintInput
			);

			input.RemoveActionListener(
				"CharacterJump",
				EActionTrigger.VALUE,
				OnJumpInput
			);

			input.RemoveActionListener(
				"Freelook",
				EActionTrigger.VALUE,
				OnFreelookInput
			);

			input.RemoveActionListener(
				"CharacterFire",
				EActionTrigger.VALUE,
				OnFireInput
			);

			input.RemoveActionListener(
				"CharacterNextFireMode",
				EActionTrigger.DOWN,
				OnCycleWeaponInput
			);

			if (!m_sLoadoutAction.IsEmpty())
				input.RemoveActionListener(m_sLoadoutAction, EActionTrigger.DOWN, OnLoadoutInput);
		}

		if (m_MovementComp)
		{
			m_MovementComp.SetForwardInput(0);
			m_MovementComp.SetRightInput(0);
			m_MovementComp.SetTurnInput(0);
			m_MovementComp.SetSprintInput(false);
		}

		SCR_PlayerController pc =
			SCR_PlayerController.Cast(
				GetGame().GetPlayerController()
			);

		if (
			pc
			&& vehicle
		)
		{
			int releaseGroup =
				m_iHeldPrimaryWeaponGroup;

			if (releaseGroup < 1)
			{
				releaseGroup =
					ResolveSelectedPrimaryWeaponGroup();
			}

			if (releaseGroup > 0)
			{
				pc.BFA_Mech_RequestFireState(
					vehicle,
					m_sCrewStationId,
					releaseGroup,
					false
				);
			}

			pc.BFA_Mech_SendDriverInput(
				vehicle,
				0,
				0,
				0,
				false
			);

			pc.BFA_Mech_RequestPilotState(
				vehicle,
				false
			);
		}

		if (m_AimComp)
		{
			m_AimComp.SetInspectMode(
				false
			);

			m_AimComp.EndLocalAim();
		}

		m_bLocalPilot = false;
		m_bJumpHeld = false;
		m_bSprintHeld = false;
		m_bInspectHeld = false;
		m_bFireHeld = false;

		m_iHeldPrimaryWeaponGroup = 0;

		m_fLocalForward = 0;
		m_fLocalStrafe = 0;
		m_fLocalTurn = 0;
		m_fTurnLeft = 0;
		m_fTurnRight = 0;

		m_pLocalPilotEntity = null;
		m_pPendingPilotVehicle = null;
		m_pPendingPilotEntity = null;

		BFA_MechLog.Log(
			"[BFA MP LOCAL] local pilot controls STOPPED"
		);
	}

	//------------------------------------------------------------------------------------------------
	IEntity BFA_GetPilotOccupant()
	{
		PilotCompartmentSlot pilotSlot =
			GetPilotCompartmentSlot();

		if (!pilotSlot)
			return null;

		return pilotSlot.GetOccupant();
	}

	//------------------------------------------------------------------------------------------------
	override void OnCompartmentEntered(
		IEntity vehicle,
		BaseCompartmentManagerComponent mgr,
		IEntity occupant,
		int managerId,
		int slotID
	)
	{
		super.OnCompartmentEntered(
			vehicle,
			mgr,
			occupant,
			managerId,
			slotID
		);

		if (!mgr || !occupant)
			return;

		BFA_MechEvents.RaiseCrewChanged(vehicle, occupant, true);

		if (
			mgr.FindCompartment(
				slotID,
				managerId
			) != GetPilotCompartmentSlot()
		)
		{
			return;
		}

		NwkMovementComponent nwkMove =
			NwkMovementComponent.Cast(
				vehicle.FindComponent(
					NwkMovementComponent
				)
			);

		if (nwkMove)
			nwkMove.EnableSimulation(false);

		if (m_bHidePilot)
			occupant.ClearFlags(EntityFlags.VISIBLE);

		if (m_MovementComp)
			m_MovementComp.SetPilot(occupant);

		if (m_JumpJetComp)
			m_JumpJetComp.SetPilotActive(true);

		if (
			IsLocalOccupant(
				vehicle,
				occupant
			)
		)
		{
			StartLocalInput(
				occupant
			);
		}
		else
		{
			QueueLocalPilotResolve(
				vehicle,
				occupant
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnCompartmentLeft(
		IEntity vehicle,
		BaseCompartmentManagerComponent mgr,
		IEntity occupant,
		int managerId,
		int slotID
	)
	{
		super.OnCompartmentLeft(
			vehicle,
			mgr,
			occupant,
			managerId,
			slotID
		);

		if (!mgr || !occupant)
			return;

		BFA_MechEvents.RaiseCrewChanged(vehicle, occupant, false);

		if (
			mgr.FindCompartment(
				slotID,
				managerId
			) != GetPilotCompartmentSlot()
		)
		{
			return;
		}

		NwkMovementComponent nwkMove =
			NwkMovementComponent.Cast(
				vehicle.FindComponent(
					NwkMovementComponent
				)
			);

		if (nwkMove)
			nwkMove.EnableSimulation(false);

		if (m_bHidePilot)
			occupant.SetFlags(EntityFlags.VISIBLE);

		if (
			m_bLocalPilot
			&& (
				occupant == m_pLocalPilotEntity
				|| IsLocalOccupant(
					vehicle,
					occupant
				)
			)
		)
		{
			StopLocalInput(
				vehicle
			);
		}

		if (
			m_pPendingPilotEntity
				== occupant
		)
		{
			ScriptCallQueue queue =
				GetGame().GetCallqueue();

			if (queue)
			{
				queue.Remove(
					ResolveLocalPilotDelayed
				);
			}

			m_pPendingPilotEntity = null;
			m_pPendingPilotVehicle = null;
		}

		if (m_MovementComp)
		{
			if (Replication.IsServer())
			{
				m_MovementComp.ServerSetDriverInput(
					0,
					0,
					0,
					false
				);
			}

			m_MovementComp.SetPilot(
				null
			);
		}

		if (m_JumpJetComp)
		{
			m_JumpJetComp.SetPilotActive(
				false
			);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_MovementComp =
			BFA_MechMovementComponent.Cast(
				owner.FindComponent(
					BFA_MechMovementComponent
				)
			);

		m_JumpJetComp =
			BFA_SentinelJumpJetComponent.Cast(
				owner.FindComponent(
					BFA_SentinelJumpJetComponent
				)
			);

		m_AimComp =
			BFA_MechAimComponent.Cast(
				owner.FindComponent(
					BFA_MechAimComponent
				)
			);

		m_WeaponComp =
			BFA_MechWeaponComponent.Cast(
				owner.FindComponent(
					BFA_MechWeaponComponent
				)
			);

		NwkMovementComponent nwkMove =
			NwkMovementComponent.Cast(
				owner.FindComponent(
					NwkMovementComponent
				)
			);

		BFA_MechLog.Log(
			"[BFA MP BUILD] V52 FREELOOK_TRACKS_GUNS"
		);

		BFA_MechLog.Log(
			string.Format(
				"[BFA MP NWK] present=%1",
				nwkMove != null
			)
		);
	}

	//------------------------------------------------------------------------------------------------
	//! Stop every repeating/delayed call before this component goes away. Without this the
	//! call queue keeps invoking methods on a destroyed mech (death, despawn, GC cleanup).
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			// Removes input ticks AND input action listeners. A listener left bound to a deleted
			// mech is a crash waiting for the next key press.
			StopLocalInput(owner);

			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(LocalInputTick);
				queue.Remove(NetInputTick);
				queue.Remove(ResolveLocalPilotDelayed);
			}
		}

		super.OnDelete(owner);
	}
}
