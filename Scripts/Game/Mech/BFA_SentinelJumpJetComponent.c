[ComponentEditorProps(category: "GameScripted/Mech", description: "Sentinel jump jets with server validation, replicated jump events, local effects and physics-owner movement")]
class BFA_SentinelJumpJetComponentClass : ScriptComponentClass
{
}

class BFA_SentinelJumpJetComponent : ScriptComponent
{
	[Attribute("12", UIWidgets.Slider, "Upward speed added per jump-jet burst.", "0 40 0.5", category: "Jump Jets")]
	protected float m_fUpwardSpeed;

	[Attribute("5", UIWidgets.Slider, "Forward speed added while airborne.", "0 25 0.5", category: "Jump Jets")]
	protected float m_fForwardBoost;

	[Attribute("9.81", UIWidgets.Slider, "Gravity applied while the Sentinel is airborne.", "0 30 0.1", category: "Jump Jets")]
	protected float m_fGravity;

	[Attribute("2", UIWidgets.Slider, "Number of jump-jet bursts available before recharge.", "1 5 1", category: "Jump Jets")]
	protected int m_iChargesPerCycle;

	[Attribute("3.0", UIWidgets.Slider, "Recharge time after all charges are spent.", "0 15 0.1", category: "Jump Jets")]
	protected float m_fRechargeTime;

	[Attribute("0.8", UIWidgets.Slider, "How long the thruster effect stays active after each burst.", "0.1 5 0.05", category: "Jump Jets")]
	protected float m_fThrusterDuration;

	[Attribute("1.5", UIWidgets.Slider, "Distance above the landing height where the landing cushion catches the mech. Raise for big mechs.", "0.1 20 0.1", category: "Jump Jets")]
	protected float m_fLandingCushionDistance;

	[Attribute(desc: "First Sentinel jump-jet particle point.", category: "Effects")]
	protected ref PointInfo m_ThrusterEffectPoint1;

	[Attribute(desc: "Second Sentinel jump-jet particle point.", category: "Effects")]
	protected ref PointInfo m_ThrusterEffectPoint2;

	[Attribute("{E60F6CCD3E871566}Particles/Vehicle/Vehicle_smoke_car_exhaust_normal.ptc", UIWidgets.ResourceNamePicker, "Thruster particle effect.", params: "ptc", category: "Effects")]
	protected ResourceName m_sThrusterEffect;

	[Attribute("SOUND_TURN_ON", UIWidgets.EditBox, "Thruster start sound event.", category: "Effects")]
	protected string m_sThrusterSoundOn;

	[Attribute("SOUND_TURN_OFF", UIWidgets.EditBox, "Thruster stop sound event.", category: "Effects")]
	protected string m_sThrusterSoundOff;

	protected BFA_MechMovementComponent m_MovementComp;

	[RplProp(onRplName: "OnJumpReplicated")]
	protected int m_iJumpCounter;

	[RplProp()]
	protected int m_iRemainingCharges;

	protected int m_iJumpSeen;
	protected bool m_bPilotActive;
	protected bool m_bBurstLocked;

	protected ParticleEffectEntity m_ThrusterInstance1;
	protected ParticleEffectEntity m_ThrusterInstance2;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_MovementComp = BFA_MechMovementComponent.Cast(owner.FindComponent(BFA_MechMovementComponent));

		if (m_ThrusterEffectPoint1)
			m_ThrusterEffectPoint1.Init(owner);

		if (m_ThrusterEffectPoint2)
			m_ThrusterEffectPoint2.Init(owner);

		if (Replication.IsServer())
		{
			m_iRemainingCharges = Math.Max(m_iChargesPerCycle, 1);
			Replication.BumpMe();
		}
	}

	//------------------------------------------------------------------------------------------------
	void SetPilotActive(bool active)
	{
		if (!active)
			StopThrusterEffect();

		if (!Replication.IsServer())
			return;

		m_bPilotActive = active;
	}

	//------------------------------------------------------------------------------------------------
	//! Compatibility entry point. Always routes the client request through its PlayerController.
	void TryActivate()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.BFA_Mech_RequestJump(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Called only by the server after PlayerController validation.
	void ServerTryActivate()
	{
		if (!Replication.IsServer())
			return;

		if (!m_bPilotActive || !m_MovementComp)
			return;

		if (m_bBurstLocked || m_iRemainingCharges <= 0)
			return;

		// Destroyed leg, overheated ... (BFA_MechExtension hooks).
		if (!BFA_MechExtension.Get().CanMechJump(GetOwner()))
			return;

		m_iRemainingCharges = Math.Max(m_iRemainingCharges - 1, 0);
		m_iJumpCounter++;
		m_bBurstLocked = true;

		Replication.BumpMe();

		// RplProp callbacks do not fire on the machine that authored the value.
		OnJumpReplicated();

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
		{
			ServerFinishBurst();
			return;
		}

		queue.Remove(ServerFinishBurst);
		queue.CallLater(ServerFinishBurst, m_fThrusterDuration * 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnJumpReplicated()
	{
		if (m_iJumpCounter == m_iJumpSeen)
			return;

		m_iJumpSeen = m_iJumpCounter;

		StartThrusterFeedback();

		// The dedicated server owns the authoritative V23 root even when the vehicle's
		// Rpl ownership has transferred to the driver. The local physics owner also applies
		// the same boost for client-side prediction.
		if (m_MovementComp && (Replication.IsServer() || m_MovementComp.IsPhysicsOwner()))
		{
			m_MovementComp.ApplyJumpJetBoost(
				m_fUpwardSpeed,
				m_fForwardBoost,
				m_fGravity,
				m_fLandingCushionDistance
			);
		}

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
		{
			StopLocalFeedback();
			return;
		}

		queue.Remove(StopLocalFeedback);
		queue.CallLater(StopLocalFeedback, m_fThrusterDuration * 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerFinishBurst()
	{
		if (!Replication.IsServer())
			return;

		if (m_iRemainingCharges > 0)
		{
			m_bBurstLocked = false;
			return;
		}

		if (m_fRechargeTime <= 0)
		{
			ServerRecharge();
			return;
		}

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
		{
			ServerRecharge();
			return;
		}

		queue.Remove(ServerRecharge);
		queue.CallLater(ServerRecharge, m_fRechargeTime * 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerRecharge()
	{
		if (!Replication.IsServer())
			return;

		m_iRemainingCharges = Math.Max(m_iChargesPerCycle, 1);
		m_bBurstLocked = false;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int GetRemainingCharges()
	{
		return m_iRemainingCharges;
	}

	//------------------------------------------------------------------------------------------------
	protected void StartThrusterFeedback()
	{
		StopThrusterEffect();

		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (!m_sThrusterEffect.IsEmpty())
		{
			ParticleEffectEntitySpawnParams spawnParams();
			spawnParams.Parent = owner;
			spawnParams.UseFrameEvent = true;

			if (m_ThrusterEffectPoint1)
			{
				m_ThrusterEffectPoint1.GetModelTransform(spawnParams.Transform);
				m_ThrusterInstance1 = ParticleEffectEntity.SpawnParticleEffect(m_sThrusterEffect, spawnParams);
			}

			if (m_ThrusterEffectPoint2)
			{
				m_ThrusterEffectPoint2.GetModelTransform(spawnParams.Transform);
				m_ThrusterInstance2 = ParticleEffectEntity.SpawnParticleEffect(m_sThrusterEffect, spawnParams);
			}
		}

		PlayConfiguredSound(m_sThrusterSoundOn);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopLocalFeedback()
	{
		StopThrusterEffect();
		PlayConfiguredSound(m_sThrusterSoundOff);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayConfiguredSound(string soundName)
	{
		if (soundName.IsEmpty())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		SCR_SoundManagerModule.CreateAndPlayAudioSource(owner, soundName);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopThrusterEffect()
	{
		if (m_ThrusterInstance1)
		{
			delete m_ThrusterInstance1;
			m_ThrusterInstance1 = null;
		}

		if (m_ThrusterInstance2)
		{
			delete m_ThrusterInstance2;
			m_ThrusterInstance2 = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stop every repeating/delayed call before this component goes away. Without this the
	//! call queue keeps invoking methods on a destroyed mech (death, despawn, GC cleanup).
	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
			{
				queue.Remove(ServerFinishBurst);
				queue.Remove(StopLocalFeedback);
				queue.Remove(ServerRecharge);
			}

			// Do not leave thruster particles/sound behind if the mech dies mid-burst.
			StopLocalFeedback();
		}

		super.OnDelete(owner);
	}
}
