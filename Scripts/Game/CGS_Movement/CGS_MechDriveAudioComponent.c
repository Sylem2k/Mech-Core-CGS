// CGS MechCore — tracked / hover audio, phase 2.
// This is deliberately independent of BFA_MechMovementComponent.
// Mount once on the MECH ROOT, not on each individual track/pad.
// Audio sources live on the Track_L1 / HoverPad_L1 part (same as walker-leg audio).
// Do not add network RPCs for sound effects: each listening client plays locally.
[ComponentEditorProps(category: "CGS MechCore/Movement", description: "Tracked/hover motor and movement sounds. One per vehicle ROOT; uses the mounted track or hover-pad sound prefab as emitter. No leg IK.")]
class CGS_MechDriveAudioComponentClass : ScriptComponentClass
{
}

class CGS_MechDriveAudioComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.Auto, "1 = tracked chassis; 2 = hover chassis. Must match BFA_MechMovementComponent locomotion.", category: "Drive Audio")]
	protected int m_iDriveType;

	[Attribute("SOUND_MECH_TRACK_CLANK", UIWidgets.EditBox, "Tracked moving/pivoting clank event on Tracked.acp.", category: "Drive Audio")]
	protected string m_sTrackClank;

	[Attribute("SOUND_MECH_TRACK_RUMBLE", UIWidgets.EditBox, "Tracked rolling rumble sample.", category: "Drive Audio")]
	protected string m_sTrackRumble;

	[Attribute("SOUND_MECH_HOVER_IDLE", UIWidgets.EditBox, "Hover idle engine-bed sample.", category: "Drive Audio")]
	protected string m_sHoverIdle;

	[Attribute("SOUND_MECH_HOVER_IDLE_PULSE", UIWidgets.EditBox, "Hover idle pressure-pulse sample.", category: "Drive Audio")]
	protected string m_sHoverPulse;

	[Attribute("SOUND_MECH_HOVER_THRUST", UIWidgets.EditBox, "Hover movement/turn thrust sample.", category: "Drive Audio")]
	protected string m_sHoverThrust;

	[Attribute("false", UIWidgets.CheckBox, "Workbench test: print throttled audio diagnostics and fire one sound shortly after spawning. Disable after sound works.", category: "Drive Audio")]
	protected bool m_bDebugSound;

	protected bool m_bHaveSample;
	protected bool m_bDebugFirstShot;
	protected vector m_vPrevPosition;
	protected float m_fPrevYaw;
	protected float m_fPrevSampleTime;
	protected float m_fNextSampleTime;
	protected float m_fNextEffectTime;
	protected float m_fNextBedTime;
	protected float m_fNextLogTime;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	//! The audio prefab must be attached to the same left/right named slots
	//! that carry the visible XOBs. Only ONE emitter is used per mech, so Assault
	//! frames with four treads/pads do not play four duplicate engine beds.
	protected IEntity FindDriveEmitter(IEntity root)
	{
		if (!root)
			return null;

		SlotManagerComponent slots = SlotManagerComponent.Cast(root.FindComponent(SlotManagerComponent));
		if (!slots)
			return null;

		string leftName = "Track_L1";
		string rightName = "Track_R1";
		if (m_iDriveType == 2)
		{
			leftName = "HoverPad_L1";
			rightName = "HoverPad_R1";
		}

		EntitySlotInfo slot = slots.GetSlotByName(leftName);
		if (slot)
		{
			IEntity candidate = slot.GetAttachedEntity();
			if (candidate && candidate.FindComponent(SoundComponent) && candidate.FindComponent(SCR_SoundDataComponent))
				return candidate;
		}

		slot = slots.GetSlotByName(rightName);
		if (slot)
		{
			IEntity candidate = slot.GetAttachedEntity();
			if (candidate && candidate.FindComponent(SoundComponent) && candidate.FindComponent(SCR_SoundDataComponent))
				return candidate;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void LogDriveAudio(string message, string eventName, float now)
	{
		if (!m_bDebugSound || now < m_fNextLogTime)
			return;

		m_fNextLogTime = now + 4.0;
		BFA_MechLog.Log(string.Format("[CGS DRIVE AUDIO] type=%1 event=%2 status=%3", m_iDriveType, eventName, message));
	}

	//------------------------------------------------------------------------------------------------
	//! Same tested Enfusion helper used by the original BFA_MechLegComponent.
	//! It looks up the mapped SoundComponent and SCR_SoundDataComponent on
	//! the chosen track/hover part. It does not replicate per-step RPCs.
	protected bool PlayDriveAudio(IEntity root, string eventName, float now)
	{
		if (eventName.IsEmpty())
			return false;

		IEntity emitter = FindDriveEmitter(root);
		if (!emitter)
		{
			LogDriveAudio("NO_PART_SOUND_EMITTER", eventName, now);
			return false;
		}

		SCR_SoundDataComponent data = SCR_SoundDataComponent.Cast(emitter.FindComponent(SCR_SoundDataComponent));
		if (!data || !data.GetAudioSourceConfiguration(eventName))
		{
			LogDriveAudio("EVENT_NOT_MAPPED_ON_PART", eventName, now);
			return false;
		}

		SCR_SoundManagerModule.CreateAndPlayAudioSource(emitter, eventName);
		LogDriveAudio("PLAY_REQUESTED", eventName, now);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!owner || !GetGame() || !GetGame().GetWorld() || System.IsConsoleApp())
			return;

		// Never run on an ordinary walker, and avoid effects on wrong chassis.
		if (m_iDriveType != 1 && m_iDriveType != 2)
			return;
		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(owner.FindComponent(BFA_MechMovementComponent));
		if (!movement || movement.GetLocomotionMode() != m_iDriveType)
			return;

		// Low-frequency sampling. At most ~7 tests/s per nearby tracked/hover mech.
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		if (now < m_fNextSampleTime)
			return;
		m_fNextSampleTime = now + 0.15;

		PlayerController pc = GetGame().GetPlayerController();
		if (pc && pc.GetControlledEntity())
		{
			if (vector.Distance(pc.GetControlledEntity().GetOrigin(), owner.GetOrigin()) > 200.0)
			{
				m_bHaveSample = false;
				return;
			}
		}

		vector pos = owner.GetOrigin();
		float yaw = owner.GetYawPitchRoll()[0];
		if (!m_bHaveSample)
		{
			m_bHaveSample = true;
			m_vPrevPosition = pos;
			m_fPrevYaw = yaw;
			m_fPrevSampleTime = now;
			m_fNextEffectTime = now + 0.3;
			m_fNextBedTime = now + 0.3;
			return;
		}

		float dt = Math.Clamp(now - m_fPrevSampleTime, 0.01, 1.0);
		m_fPrevSampleTime = now;
		vector delta = pos - m_vPrevPosition;
		delta[1] = 0;
		m_vPrevPosition = pos;
		float yawDelta = yaw - m_fPrevYaw;
		m_fPrevYaw = yaw;
		while (yawDelta > 180.0)
			yawDelta -= 360.0;
		while (yawDelta < -180.0)
			yawDelta += 360.0;

		// Normalized speeds instead of distance-per-render-frame thresholds.
		bool moving = (delta.Length() / dt) > 0.15 || (Math.AbsFloat(yawDelta) / dt) > 2.0;

		// Single proof-of-wiring sound in the two debug-only Light test frames.
		if (m_bDebugSound && !m_bDebugFirstShot && now >= m_fNextEffectTime)
		{
			m_bDebugFirstShot = true;
			if (m_iDriveType == 1)
				PlayDriveAudio(owner, m_sTrackClank, now);
			else
				PlayDriveAudio(owner, m_sHoverIdle, now);
		}

		// Audio files are short, approximately 4 s engine-bed samples.
		// These are retriggered one-shots, NOT native seamless playback loops.
		if (now >= m_fNextBedTime)
		{
			if (m_iDriveType == 1)
			{
				if (moving && PlayDriveAudio(owner, m_sTrackRumble, now))
					m_fNextBedTime = now + 3.85;
			}
			else if (PlayDriveAudio(owner, m_sHoverIdle, now))
				m_fNextBedTime = now + 3.85;
		}

		if (now < m_fNextEffectTime)
			return;

		if (m_iDriveType == 1)
		{
			if (moving && PlayDriveAudio(owner, m_sTrackClank, now))
				m_fNextEffectTime = now + 0.7;
		}
		else if (moving)
		{
			if (PlayDriveAudio(owner, m_sHoverThrust, now))
				m_fNextEffectTime = now + 1.1;
		}
		else if (PlayDriveAudio(owner, m_sHoverPulse, now))
		{
			m_fNextEffectTime = now + 3.4;
		}
	}
}
