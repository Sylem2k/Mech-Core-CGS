//------------------------------------------------------------------------------------------------
//! CGS MECH BRAIN (MY FIRST MECH add-on). Lets a mech fight with NO crew inside: it drives and
//! fires every weapon station by itself. Add it to any mech that has BFA_MechAICrewComponent.
//!  - Players (or real AI soldiers) can still climb in: the station they sit in is theirs, the
//!    brain keeps running the empty ones.
//!  - Spawn a brain mech from Game Master and it starts working straight away - no AI to place.
//! The brain plugs into MechCore's AI crew through `modded class` (CGS_MechBrainHooks.c).

enum CGS_EMechBrainMode
{
	HOLD = 0,		// stay at home, fight from there
	PATROL = 1,		// wander around home, fight what it meets
	HUNT = 2,		// go looking for enemies within Hunt Radius, patrol when none
}

// Optional player-bound companion orders. These override the autonomous movement mode only.
enum CGS_EMechCompanionOrder
{
	FOLLOW = 0,
	GUARD = 1
}

enum CGS_EMechFireMode
{
	FIRE_AT_WILL = 0,	// shoot any enemy it sees
	RETURN_FIRE = 1,	// shoot only for a while after the mech is hit
	HOLD_FIRE = 2,		// aim, never shoot
}

[ComponentEditorProps(category: "CGS MechCore", description: "AI brain: the mech drives and fires by itself with no crew (CGS add-on).")]
class CGS_MechBrainComponentClass : ScriptComponentClass
{
}

class CGS_MechBrainComponent : ScriptComponent
{
	[Attribute("true", UIWidgets.CheckBox, "Brain switched on.", category: "Brain")]
	protected bool m_bEnabled;

	[Attribute("true", UIWidgets.CheckBox, "Brain drives the mech when the DRIVER seat is empty.", category: "Brain")]
	protected bool m_bDrive;

	[Attribute("true", UIWidgets.CheckBox, "Brain fires every weapon station whose seat is empty.", category: "Brain")]
	protected bool m_bGunners;

	[Attribute("2", UIWidgets.ComboBox, "HOLD = stay at home. PATROL = wander around home. HUNT = seek enemies within Hunt Radius (patrols when none).", category: "Brain", enums: ParamEnumArray.FromEnum(CGS_EMechBrainMode))]
	protected int m_eMode;

	[Attribute("", UIWidgets.EditBox, "Faction key the brain fights for (e.g. US, USSR, FIA). Empty = the pilot's side (or the vehicle's default with no pilot). Set = overrides the pilot too, so one pilot prefab works for every side.", category: "Brain")]
	protected string m_sFaction;

	// ---- Companion (optional, off for all existing mechs) ---------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Allow a nearby friendly player to claim and order this mech. Pilot takeover uses the existing crew station.", category: "Companion")]
	protected bool m_bCompanionEnabled;

	[Attribute("18", UIWidgets.Auto, "FOLLOW: stop this many metres from the owner.", category: "Companion")]
	protected float m_fCompanionFollowDistance;

	[Attribute("25", UIWidgets.Auto, "Player must be within this distance to claim or change orders.", category: "Companion")]
	protected float m_fCompanionCommandDistance;

	[Attribute("600", UIWidgets.Auto, "If owner is farther than this, hold instead of traveling across the map. 0 disables limit.", category: "Companion")]
	protected float m_fCompanionMaxFollowDistance;

	// ---- Weapons -------------------------------------------------------------------------------
	[Attribute("0", UIWidgets.ComboBox, "FIRE AT WILL = shoot enemies on sight. RETURN FIRE = shoot only after being hit. HOLD FIRE = aim but never shoot.", category: "Weapons", enums: ParamEnumArray.FromEnum(CGS_EMechFireMode))]
	protected int m_eFireMode;

	[Attribute("true", UIWidgets.CheckBox, "When an AI soldier is aboard (e.g. a pilot), use its group's combat mode from Game Master (Hold fire / Fire at will) instead of Fire Mode.", category: "Weapons")]
	protected bool m_bFollowGroupMode;

	[Attribute("20", UIWidgets.Auto, "RETURN FIRE: keep shooting this many seconds after the mech (or its shield) is hit.", category: "Weapons")]
	protected float m_fReturnFireTime;

	// ---- Pilot ---------------------------------------------------------------------------------
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Hidden pilot spawned in the DRIVER seat (a character prefab with CGS_HiddenPilotComponent). Gives the mech an AI group, so Game Master can give it waypoints and Hold fire. Empty = no pilot.", category: "Pilot", params: "et")]
	protected ResourceName m_sPilotPrefab;

	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourcePickerThumbnail, "AI group prefab the pilot joins (vanilla Group_Base).", category: "Pilot", params: "et")]
	protected ResourceName m_sPilotGroupPrefab;

	[Attribute("true", UIWidgets.CheckBox, "Reserve the DRIVER seat for the pilot (other AI will not try to take it). If the pilot ever leaves the seat it is put straight back.", category: "Pilot")]
	protected bool m_bLockDriverSeat;

	// ---- Movement ------------------------------------------------------------------------------
	[Attribute("150", UIWidgets.Auto, "PATROL / HUNT: wander up to this many metres from home.", category: "Movement")]
	protected float m_fPatrolRadius;

	[Attribute("6", UIWidgets.Auto, "Seconds to wait at each patrol point.", category: "Movement")]
	protected float m_fPatrolPause;

	[Attribute("800", UIWidgets.Auto, "HUNT: look for enemies this far away (no line of sight needed).", category: "Movement")]
	protected float m_fHuntRadius;

	[Attribute("0", UIWidgets.Auto, "Never chase farther than this from home (0 = no limit).", category: "Movement")]
	protected float m_fLeash;

	[Attribute("50", UIWidgets.Auto, "If the mech is moved further than this in one step (Game Master teleport), its home moves with it.", category: "Movement")]
	protected float m_fRehomeDistance;

	// ---- Runtime -------------------------------------------------------------------------------
	protected vector m_vHome;
	protected vector m_vLastPos;
	protected bool m_bHomeSet;
	protected vector m_vPatrolPoint;
	protected bool m_bHasPatrolPoint;
	protected float m_fPauseLeft;
	protected ref array<IEntity> m_aQuery = {};
	protected float m_fStartDelay = 0.5;
	protected bool m_bStarted;
	protected float m_fWatchTimer;
	protected IEntity m_pPilot;
	protected IEntity m_pPilotGroup;
	protected int m_iSeatTries;
	protected bool m_bPilotSeated;
	protected Faction m_pBrainFaction;

	// Owner is a server-validated player ID, never accepted from the client/action arguments.
	protected int m_iCompanionOwnerId;
	protected int m_eCompanionOrder = CGS_EMechCompanionOrder.GUARD;
	protected vector m_vCompanionGuardPoint;
	protected bool m_bCompanionGuardSet;

	//------------------------------------------------------------------------------------------------
	static CGS_MechBrainComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return CGS_MechBrainComponent.Cast(mech.FindComponent(CGS_MechBrainComponent));
	}

	//------------------------------------------------------------------------------------------------
	bool IsBrainOn()
	{
		return m_bEnabled && Replication.IsServer();
	}

	bool DrivesEmptySeat()
	{
		return IsBrainOn() && m_bDrive;
	}

	bool FiresEmptySeats()
	{
		return IsBrainOn() && m_bGunners;
	}

	int GetFireMode()
	{
		return m_eFireMode;
	}

	bool FollowsGroupMode()
	{
		return m_bFollowGroupMode;
	}

	float GetReturnFireTime()
	{
		return m_fReturnFireTime;
	}

	//==================================================================================================
	// PLAYER COMPANION (server owns orders and the owner-player binding)
	//==================================================================================================

	bool IsCompanionEnabled()
	{
		return m_bEnabled && m_bCompanionEnabled;
	}

	bool IsCompanionFollowing()
	{
		return IsCompanionEnabled() && m_iCompanionOwnerId > 0 && m_eCompanionOrder == CGS_EMechCompanionOrder.FOLLOW;
	}

	//! Companion FOLLOW and GUARD override combat chasing, NOT combat aiming or firing.
	bool CompanionOwnsMovement()
	{
		return IsCompanionEnabled() && Replication.IsServer();
	}

	protected IEntity CompanionOwnerCharacter()
	{
		if (m_iCompanionOwnerId <= 0 || !GetGame() || !GetGame().GetPlayerManager())
			return null;
		IEntity user = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iCompanionOwnerId);
		if (!user)
			return null;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(user.FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.GetState() == EDamageState.DESTROYED)
			return null;
		return user;
	}

	//! Can be called for client prompt visibility. The server always repeats the full check.
	bool CanShowCompanionAction(IEntity user)
	{
		if (!IsCompanionEnabled() || !user || !GetOwner())
			return false;
		float maxDistance = Math.Max(m_fCompanionCommandDistance, 2);
		return vector.DistanceSq(user.GetOrigin(), GetOwner().GetOrigin()) <= maxDistance * maxDistance;
	}

	//! Called only by server-side context actions. Never trust a player ID sent by a client.
	bool ServerCommandCompanion(IEntity user, int order)
	{
		if (!Replication.IsServer() || !CanShowCompanionAction(user) || !ChimeraCharacter.Cast(user))
			return false;
		if (order != CGS_EMechCompanionOrder.FOLLOW && order != CGS_EMechCompanionOrder.GUARD)
			return false;
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return false;
		int playerId = manager.GetPlayerIdFromControlledEntity(user);
		if (playerId <= 0 || manager.GetPlayerControlledEntity(playerId) != user)
			return false;
		if (m_iCompanionOwnerId > 0 && m_iCompanionOwnerId != playerId)
			return false;

		Faction playerFaction = CgsCharacterFaction(user);
		Faction mechFaction = ResolveFaction();
		if (!playerFaction || !mechFaction || mechFaction.IsFactionEnemy(playerFaction))
			return false;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(GetOwner().FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.GetState() == EDamageState.DESTROYED)
			return false;

		m_iCompanionOwnerId = playerId;
		m_eCompanionOrder = order;
		if (order == CGS_EMechCompanionOrder.GUARD)
		{
			m_vCompanionGuardPoint = GetOwner().GetOrigin();
			m_bCompanionGuardSet = true;
		}
		m_bHasPatrolPoint = false;
		Print(string.Format("[CGS COMPANION] %1 player=%2 order=%3", GetOwner(), playerId, typename.EnumToString(CGS_EMechCompanionOrder, order)));
		return true;
	}

	//! Disconnected owners release their claim so another friendly player can use the mech.
	protected void WatchCompanionOwner()
	{
		if (!IsCompanionEnabled() || m_iCompanionOwnerId <= 0 || !GetGame() || !GetGame().GetPlayerManager())
			return;
		PlayerManager manager = GetGame().GetPlayerManager();
		if (manager.GetPlayerController(m_iCompanionOwnerId))
			return;
		m_iCompanionOwnerId = 0;
		m_eCompanionOrder = CGS_EMechCompanionOrder.GUARD;
		m_vCompanionGuardPoint = GetOwner().GetOrigin();
		m_bCompanionGuardSet = true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!owner.FindComponent(BFA_MechAICrewComponent))
			Print(string.Format("[CGS BRAIN] %1: needs BFA_MechAICrewComponent on the same mech.", owner), LogLevel.WARNING);

		// Start-up runs from the frame (not the call queue) so many mechs can spawn at once.
		if (Replication.IsServer())
			SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bStarted)
		{
			m_fStartDelay -= timeSlice;
			if (m_fStartDelay > 0)
				return;

			m_bStarted = true;
			if (m_bEnabled && !m_bCompanionEnabled)
				SpawnPilot();
			ApplyFaction();
		}

		m_fWatchTimer -= timeSlice;
		if (m_fWatchTimer > 0)
			return;
		m_fWatchTimer = 0.5;

		if (!m_bEnabled)
			return;

		UpdateFaction();
		if (m_bCompanionEnabled)
			WatchCompanionOwner();
		else
			WatchPilot();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		RemovePilot();
		super.OnDelete(owner);
	}

	//==================================================================================================
	// HIDDEN PILOT
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	IEntity GetPilot()
	{
		return m_pPilot;
	}

	//------------------------------------------------------------------------------------------------
	protected PilotCompartmentSlot GetDriverSlot()
	{
		BFA_MechControllerComponent controller = BFA_MechControllerComponent.Cast(GetOwner().FindComponent(BFA_MechControllerComponent));
		if (!controller)
			return null;
		return controller.GetPilotCompartmentSlot();
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnPilot()
	{
		if (m_sPilotPrefab.IsEmpty() || m_pPilot)
			return;

		IEntity owner = GetOwner();
		PilotCompartmentSlot slot = GetDriverSlot();
		if (!slot)
		{
			Print(string.Format("[CGS BRAIN] %1: no DRIVER seat for the pilot.", owner), LogLevel.WARNING);
			return;
		}
		if (slot.GetOccupant())
			return; // someone is already driving

		Resource pilotRes = Resource.Load(m_sPilotPrefab);
		if (!pilotRes || !pilotRes.IsValid())
		{
			Print(string.Format("[CGS BRAIN] %1: pilot prefab not found: %2", owner, m_sPilotPrefab), LogLevel.WARNING);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		owner.GetTransform(params.Transform);

		m_pPilot = GetGame().SpawnEntityPrefab(pilotRes, owner.GetWorld(), params);
		if (!m_pPilot)
			return;

		if (!CGS_HiddenPilotComponent.IsHiddenPilot(m_pPilot))
			Print(string.Format("[CGS BRAIN] %1: pilot prefab has no CGS_HiddenPilotComponent - it will be visible and can be killed.", owner), LogLevel.WARNING);

		// Own AI group, so Game Master can select it and give waypoints / Hold fire.
		if (!m_sPilotGroupPrefab.IsEmpty())
		{
			Resource groupRes = Resource.Load(m_sPilotGroupPrefab);
			if (groupRes && groupRes.IsValid())
			{
				m_pPilotGroup = GetGame().SpawnEntityPrefab(groupRes, owner.GetWorld(), params);
				SCR_AIGroup group = SCR_AIGroup.Cast(m_pPilotGroup);
				if (group)
				{
					m_pBrainFaction = ResolveFaction();
					if (m_pBrainFaction)
						group.SetFaction(m_pBrainFaction);

					AIControlComponent aiControl = AIControlComponent.Cast(m_pPilot.FindComponent(AIControlComponent));
					if (aiControl)
					{
						aiControl.ActivateAI();
						AIAgent agent = aiControl.GetControlAIAgent();
						if (agent)
							group.AddAgent(agent);
						else
							Print(string.Format("[CGS BRAIN] %1: pilot has no AI agent - Game Master cannot command it.", owner), LogLevel.WARNING);
					}
				}
			}
		}

		m_iSeatTries = 0;
		m_bPilotSeated = false;
		Print(string.Format("[CGS BRAIN] %1: pilot spawned", owner));
	}

	//------------------------------------------------------------------------------------------------
	//! Seats the pilot, keeps it seated, removes it when the mech dies.
	protected void WatchPilot()
	{
		IEntity owner = GetOwner();

		if (!m_pPilot)
			return;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(owner.FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.GetState() == EDamageState.DESTROYED)
		{
			RemovePilot();
			ClearEventMask(owner, EntityEvent.FRAME);
			return;
		}

		PilotCompartmentSlot slot = GetDriverSlot();
		if (!slot)
			return;

		if (slot.GetOccupant() == m_pPilot)
		{
			if (!m_bPilotSeated)
			{
				m_bPilotSeated = true;
				if (m_bLockDriverSeat)
					slot.SetCompartmentAccessible(false);
				Print(string.Format("[CGS BRAIN] %1: pilot seated", owner));
			}
			return;
		}

		// Not in the seat (just spawned, or it got out): put it back.
		if (m_bPilotSeated)
			Print(string.Format("[CGS BRAIN] %1: pilot got out of the DRIVER seat - putting it back. (If this repeats: set the AI vehicle type on SCR_AIVehicleUsageComponent.)", owner), LogLevel.WARNING);
		m_bPilotSeated = false;
		if (slot.GetOccupant())
			return; // a player took the seat

		if (m_bLockDriverSeat)
			slot.SetCompartmentAccessible(true);

		m_iSeatTries++;
		if (m_iSeatTries > 20)
		{
			Print(string.Format("[CGS BRAIN] %1: pilot could not get into the DRIVER seat - removed.", owner), LogLevel.WARNING);
			RemovePilot();
			return;
		}

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(m_pPilot.FindComponent(SCR_CompartmentAccessComponent));
		if (access)
			access.GetInVehicle(owner, slot, true, -1, ECloseDoorAfterActions.INVALID, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemovePilot()
	{
		if (!Replication.IsServer())
			return;

		if (m_pPilot)
			RplComponent.DeleteRplEntity(m_pPilot, false);
		if (m_pPilotGroup)
			RplComponent.DeleteRplEntity(m_pPilotGroup, false);

		m_pPilot = null;
		m_pPilotGroup = null;
	}

	//------------------------------------------------------------------------------------------------
	//! An empty vehicle normally has no side. The brain gives it one, so it knows who to shoot and
	//! enemies know it is hostile.
	protected void ApplyFaction()
	{
		IEntity owner = GetOwner();
		if (!owner || !m_bEnabled)
			return;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
		{
			Print(string.Format("[CGS BRAIN] %1: no faction component - the brain cannot tell friend from foe.", owner), LogLevel.WARNING);
			return;
		}

		m_pBrainFaction = ResolveFaction();
		if (m_pBrainFaction)
			affiliation.SetAffiliatedFaction(m_pBrainFaction);


		// One pilot prefab for every side: a set Faction overrides the pilot's own.
		if (m_pPilot && m_pBrainFaction && !m_sFaction.IsEmpty())
		{
			FactionAffiliationComponent pilotAffiliation = FactionAffiliationComponent.Cast(m_pPilot.FindComponent(FactionAffiliationComponent));
			if (pilotAffiliation)
				pilotAffiliation.SetAffiliatedFaction(m_pBrainFaction);
		}

		Print(string.Format("[CGS BRAIN] %1: brain online, mode %2, faction %3", owner, typename.EnumToString(CGS_EMechBrainMode, m_eMode), CgsFactionName(m_pBrainFaction)));
	}

	//------------------------------------------------------------------------------------------------
	protected Faction CgsCharacterFaction(IEntity character)
	{
		if (!character)
			return null;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return null;

		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			faction = affiliation.GetDefaultAffiliatedFaction();
		return faction;
	}

	//------------------------------------------------------------------------------------------------
	//! Every half second: the mech (and the pilot's group) take the side of whoever drives,
	//! overriding the side set on the mech prefab.
	protected void UpdateFaction()
	{
		IEntity owner = GetOwner();
		Faction faction = ResolveFaction();
		if (!faction)
			return;

		if (faction != m_pBrainFaction)
		{
			m_pBrainFaction = faction;
			Print(string.Format("[CGS BRAIN] %1: side is now %2", owner, CgsFactionName(faction)));
		}

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
		if (affiliation && affiliation.GetAffiliatedFaction() != faction)
			affiliation.SetAffiliatedFaction(faction);

		SCR_AIGroup group = SCR_AIGroup.Cast(m_pPilotGroup);
		if (group && group.GetFaction() != faction)
			group.SetFaction(faction);
	}

	//------------------------------------------------------------------------------------------------
	//! Faction setting first; empty = the driver's side (hidden pilot, AI or player); nobody = the vehicle's default.
	protected Faction ResolveFaction()
	{
		IEntity owner = GetOwner();
		Faction faction;

		if (!m_sFaction.IsEmpty())
		{
			FactionManager manager = GetGame().GetFactionManager();
			if (manager)
				faction = manager.GetFactionByKey(m_sFaction);
			if (!faction)
				Print(string.Format("[CGS BRAIN] %1: faction '%2' not found in this scenario.", owner, m_sFaction), LogLevel.WARNING);
		}

		// Whoever drives decides the side (hidden pilot, AI soldier or player).
		if (!faction)
		{
			PilotCompartmentSlot slot = GetDriverSlot();
			if (slot)
				faction = CgsCharacterFaction(slot.GetOccupant());
		}

		if (!faction)
			faction = CgsCharacterFaction(m_pPilot);

		if (!faction)
		{
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
			if (affiliation)
			{
				faction = affiliation.GetAffiliatedFaction();
				if (!faction)
					faction = affiliation.GetDefaultAffiliatedFaction();
			}
		}

		return faction;
	}

	//------------------------------------------------------------------------------------------------
	protected string CgsFactionName(Faction faction)
	{
		if (!faction)
			return "none";
		return faction.GetFactionKey();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHome(vector position)
	{
		if (!m_bHomeSet)
		{
			m_vHome = position;
			m_vLastPos = position;
			m_bHomeSet = true;
			return;
		}

		if (m_fRehomeDistance > 0 && vector.Distance(position, m_vLastPos) > m_fRehomeDistance)
		{
			m_vHome = position;
			m_bHasPatrolPoint = false;
		}

		m_vLastPos = position;
	}

	//------------------------------------------------------------------------------------------------
	//! Leash for combat movement: true = too far from home, walk back.
	bool IsBeyondLeash(vector position)
	{
		if (m_fLeash <= 0 || !m_bHomeSet)
			return false;

		vector d = position - m_vHome;
		d[1] = 0;
		return d.Length() > m_fLeash;
	}

	//------------------------------------------------------------------------------------------------
	//! Where to walk when not fighting. False = stand still.
	bool GetDestination(vector position, float dt, Faction mechFaction, out vector destination, out float arrive, out string label)
	{
		UpdateHome(position);
		arrive = 10;

		// Companion orders take precedence over patrol, hunt and the static home leash.
		// Owner movement does not affect AI gunner target acquisition.
		if (IsCompanionEnabled())
		{
			if (!m_bCompanionGuardSet)
			{
				m_vCompanionGuardPoint = position;
				m_bCompanionGuardSet = true;
			}
			IEntity follow = CompanionOwnerCharacter();
			if (IsCompanionFollowing() && follow && ChimeraCharacter.Cast(follow))
			{
				vector toOwner = follow.GetOrigin() - position;
				toOwner[1] = 0;
				float distance = toOwner.Length();
				if (m_fCompanionMaxFollowDistance <= 0 || distance <= m_fCompanionMaxFollowDistance)
				{
					// If the owner dies or disconnects, stand guard near the mech's last position.
					m_vCompanionGuardPoint = position;
					m_bCompanionGuardSet = true;
					destination = follow.GetOrigin();
					arrive = Math.Max(m_fCompanionFollowDistance, 5);
					label = "following player";
					return true;
				}
			}
			// A missing/dead/too-distant owner means HOLD at the last guard point.
			destination = m_vCompanionGuardPoint;
			arrive = 8;
			label = "companion guard";
			return true;
		}

		if (IsBeyondLeash(position))
		{
			destination = m_vHome;
			label = "returning home (leash)";
			return true;
		}

		if (m_eMode == CGS_EMechBrainMode.HUNT)
		{
			IEntity prey = FindPrey(position, mechFaction);
			if (prey)
			{
				destination = prey.GetOrigin();
				arrive = 60;
				label = "hunting";
				return true;
			}
		}

		if (m_eMode == CGS_EMechBrainMode.HOLD)
		{
			destination = m_vHome;
			label = "holding home";
			return true;
		}

		// PATROL (and HUNT with nothing to hunt).
		if (m_fPauseLeft > 0)
		{
			m_fPauseLeft -= dt;
			label = "patrol pause";
			return false;
		}

		if (!m_bHasPatrolPoint)
		{
			float radius = Math.Max(m_fPatrolRadius, 20);
			float angle = Math.RandomFloat(0, Math.PI2);
			float dist = Math.RandomFloat(radius * 0.3, radius);
			m_vPatrolPoint = m_vHome + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist);
			m_bHasPatrolPoint = true;
		}

		vector d = m_vPatrolPoint - position;
		d[1] = 0;
		if (d.Length() <= arrive)
		{
			m_bHasPatrolPoint = false;
			m_fPauseLeft = Math.Max(m_fPatrolPause, 0);
			label = "patrol point reached";
			return false;
		}

		destination = m_vPatrolPoint;
		label = "patrolling";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectQueryEntity(IEntity entity)
	{
		if (entity)
			m_aQuery.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest living enemy character or vehicle within Hunt Radius (no line of sight needed).
	protected IEntity FindPrey(vector position, Faction mechFaction)
	{
		if (!mechFaction)
			return null;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		IEntity owner = GetOwner();
		float range = Math.Max(m_fHuntRadius, 50);
		m_aQuery.Clear();
		world.QueryEntitiesBySphere(position, range, CollectQueryEntity, null, EQueryEntitiesFlags.DYNAMIC);

		IEntity best;
		float bestSq = range * range;
		foreach (IEntity entity : m_aQuery)
		{
			if (!entity || entity == owner || entity.GetRootParent() == owner)
				continue;

			bool isCharacter = ChimeraCharacter.Cast(entity) != null;
			if (!isCharacter && !Vehicle.Cast(entity))
				continue;
			if (isCharacter && entity.GetParent())
				continue;

			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
			if (!affiliation)
				continue;
			Faction faction = affiliation.GetAffiliatedFaction();
			if (!faction || !mechFaction.IsFactionEnemy(faction))
				continue;

			DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
			if (damage && damage.GetState() == EDamageState.DESTROYED)
				continue;

			float dSq = vector.DistanceSq(position, entity.GetOrigin());
			if (dSq < bestSq)
			{
				best = entity;
				bestSq = dSq;
			}
		}

		m_aQuery.Clear();
		return best;
	}
}
