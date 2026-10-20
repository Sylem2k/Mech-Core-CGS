//------------------------------------------------------------------------------------------------
//! CGS HAND DESIGNATOR - the soldier version of the target designator. Put it (and a
//! CGS_LaserSpotComponent) on a hand-held item, e.g. a copy of the binoculars.
//!   - Hold the item in your hands and HOLD FIRE (left mouse): laser on. Release: laser off.
//!   - The laser goes where you look (your camera), from you, up to the range.
//!   - Hits a vehicle / mech / soldier: the spot sticks to it. Ground: stays on the ground.
//!   - Friendly mechs with laser-guided missiles (CGS Titan Missile, Guidance Laser / Both) can
//!     fire at it, and every other system that reads laser spots can use it.
//! While lasing, the holder sees "LASER  523 m" under the crosshair and a mark on the spot.
//! Network: the holder's machine sends where it looks 10 times a second through its player
//! controller; the server traces the laser itself (and checks the item is really in the
//! sender's hands), so nobody can paint from across the map.
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "Hand-held target designator: hold fire with this item in hand to paint a laser spot. Needs CGS_LaserSpotComponent on the same item.")]
class CGS_HandDesignatorComponentClass : ScriptComponentClass
{
}

class CGS_HandDesignatorComponent : ScriptComponent
{
	[Attribute("1500", UIWidgets.Auto, "Laser range in metres.", category: "Designator")]
	protected float m_fRange;

	[Attribute("100", UIWidgets.Auto, "Milliseconds between updates while lasing.", category: "Designator")]
	protected int m_iUpdateMs;

	[Attribute("CharacterFire", UIWidgets.EditBox, "Input action held to lase (CharacterFire = left mouse / trigger).", category: "Designator")]
	protected string m_sInputAction;

	[Attribute("true", UIWidgets.CheckBox, "Only stick the spot to vehicles, mechs and soldiers. Off = also to buildings and props.", category: "Designator")]
	protected bool m_bStickToUnitsOnly;

	// Client.
	protected bool m_bLocalLasing;
	protected int m_iNextSendMs;
	protected TextWidget m_wText;
	protected ImageWidget m_wMark;

	// Server.
	protected int m_iServerLastMs;
	protected bool m_bServerTimeoutRunning;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		// Only machines with a player need the frame check (not a dedicated server).
		if (RplSession.Mode() != RplMode.Dedicated)
			SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		RemoveWidgets();
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ServerTimeoutCheck);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	float GetRange()
	{
		return m_fRange;
	}

	//------------------------------------------------------------------------------------------------
	protected int NowMs()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Is this item in the hands of this character?
	bool IsHeldBy(IEntity character)
	{
		if (!character)
			return false;
		IEntity owner = GetOwner();
		if (owner.GetRootParent() != character)
			return false;

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (gadgets && gadgets.GetHeldGadget() == owner)
			return true;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		return controller && controller.GetRightHandItem() == owner;
	}

	//==================================================================================================
	// CLIENT (holder)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// Cheap early out: not carried by the local player.
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player || owner.GetRootParent() != player)
		{
			if (m_bLocalLasing)
				SetLocalLasing(false, player);
			return;
		}

		bool want = false;
		InputManager input = GetGame().GetInputManager();
		if (input && IsHeldBy(player) && !GetGame().GetMenuManager().IsAnyMenuOpen())
			want = input.GetActionValue(m_sInputAction) > 0.5;

		if (want != m_bLocalLasing)
			SetLocalLasing(want, player);

		if (m_bLocalLasing && NowMs() >= m_iNextSendMs)
			SendView(true);

		UpdateWidgets();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLocalLasing(bool on, IEntity player)
	{
		m_bLocalLasing = on;
		if (on)
		{
			SendView(true);
		}
		else
		{
			SendView(false);
			RemoveWidgets();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SendView(bool on)
	{
		m_iNextSendMs = NowMs() + Math.Max(m_iUpdateMs, 30);
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		pc.CGS_SendDesignator(GetOwner(), on, cam[3], cam[2]);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateWidgets()
	{
		CGS_LaserSpotComponent spot = CGS_LaserSpotComponent.Find(GetOwner());
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws || !m_bLocalLasing)
			return;

		if (!m_wText)
		{
			m_wText = TextWidget.Cast(ws.CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, new Color(1, 0.3, 0.25, 1), 990, null));
			m_wMark = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.STRETCH, new Color(1, 0.3, 0.25, 0.9), 990, null));
			if (!m_wText || !m_wMark)
				return;
			m_wText.SetExactFontSize(18);
			FrameSlot.SetSize(m_wText, 260, 24);
			FrameSlot.SetSize(m_wMark, 12, 12);
			m_wMark.SetColor(new Color(1, 0.3, 0.25, 0.9));
		}

		float sw = ws.DPIUnscale(ws.GetWidth());
		float sh = ws.DPIUnscale(ws.GetHeight());
		FrameSlot.SetPos(m_wText, sw * 0.5 - 50, sh * 0.5 + 40);

		bool haveSpot = spot && spot.IsLasing();
		m_wMark.SetVisible(false);
		if (!haveSpot)
		{
			m_wText.SetText("LASER  no return");
			return;
		}

		vector pos = spot.GetSpot();
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		float distance = 0;
		if (player)
			distance = vector.Distance(player.GetOrigin(), pos);
		m_wText.SetText("LASER  " + Math.Round(distance).ToString() + " m");

		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		if (vector.Dot(pos - cam[3], cam[2]) > 0.5)
		{
			vector screen = ws.ProjWorldToScreen(pos, GetGame().GetWorld());
			FrameSlot.SetPos(m_wMark, screen[0] - 6, screen[1] - 6);
			m_wMark.SetVisible(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveWidgets()
	{
		if (m_wText)
			m_wText.RemoveFromHierarchy();
		if (m_wMark)
			m_wMark.RemoveFromHierarchy();
		m_wText = null;
		m_wMark = null;
	}

	//==================================================================================================
	// SERVER
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Server: the holder says where they look (on) or that they let go (off).
	void ServerUpdate(IEntity user, bool on, vector eye, vector direction)
	{
		if (!Replication.IsServer())
			return;
		CGS_LaserSpotComponent spot = CGS_LaserSpotComponent.Find(GetOwner());
		if (!spot)
		{
			Print("[CGS DESIGNATOR] the hand designator item has no CGS_LaserSpotComponent", LogLevel.WARNING);
			return;
		}

		// Must really be in this player's hands, and the eye must be near them.
		if (!on || !IsHeldBy(user) || vector.Distance(eye, user.GetOrigin()) > 8 || direction.LengthSq() < 0.5)
		{
			spot.ServerStop();
			return;
		}

		direction.Normalize();
		IEntity hit;
		vector end = Trace(user, eye, direction, hit);
		if (!hit && vector.DistanceSq(eye, end) >= (m_fRange - 1) * (m_fRange - 1))
			spot.ServerStop();
		else
			spot.ServerSetSpot(end, StickTarget(hit), user);

		// The holder's machine stops sending (crash, disconnect): switch off after a moment.
		m_iServerLastMs = NowMs();
		if (!m_bServerTimeoutRunning && GetGame().GetCallqueue())
		{
			m_bServerTimeoutRunning = true;
			GetGame().GetCallqueue().CallLater(ServerTimeoutCheck, 250, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerTimeoutCheck()
	{
		CGS_LaserSpotComponent spot = CGS_LaserSpotComponent.Find(GetOwner());
		if (!spot || !spot.IsLasing() || NowMs() - m_iServerLastMs > 600)
		{
			if (spot)
				spot.ServerStop();
			GetGame().GetCallqueue().Remove(ServerTimeoutCheck);
			m_bServerTimeoutRunning = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected vector Trace(IEntity user, vector start, vector direction, out IEntity hitEntity)
	{
		hitEntity = null;
		vector finalEnd = start + direction * Math.Max(m_fRange, 1);
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return finalEnd;

		TraceParam param = new TraceParam();
		param.Start = start;
		param.End = finalEnd;
		param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		param.LayerMask = EPhysicsLayerPresets.Projectile;
		array<IEntity> excludes = {};
		excludes.Insert(user);
		excludes.Insert(GetOwner());
		param.ExcludeArray = excludes;

		float fraction = world.TraceMove(param, null);
		if (fraction >= 1.0)
			return finalEnd;
		hitEntity = param.TraceEnt;
		return start + (finalEnd - start) * fraction;
	}

	//------------------------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------------------------
//! Carries the hand designator's view to the server (the player controller is the one thing a
//! client may send RPCs through).
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void CGS_SendDesignator(IEntity item, bool on, vector eye, vector direction)
	{
		if (!item)
			return;
		RplComponent rpl = RplComponent.Cast(item.FindComponent(RplComponent));
		if (!rpl)
			return;
		if (Replication.IsServer())
		{
			CGS_ServerDesignator(rpl.Id(), on, eye, direction);
			return;
		}
		Rpc(CGS_RpcAsk_Designator, rpl.Id(), on, eye, direction);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void CGS_RpcAsk_Designator(RplId itemId, bool on, vector eye, vector direction)
	{
		CGS_ServerDesignator(itemId, on, eye, direction);
	}

	//------------------------------------------------------------------------------------------------
	protected void CGS_ServerDesignator(RplId itemId, bool on, vector eye, vector direction)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(itemId));
		if (!rpl || !rpl.GetEntity())
			return;
		CGS_HandDesignatorComponent designator = CGS_HandDesignatorComponent.Cast(rpl.GetEntity().FindComponent(CGS_HandDesignatorComponent));
		if (designator)
			designator.ServerUpdate(GetControlledEntity(), on, eye, direction);
	}
}
