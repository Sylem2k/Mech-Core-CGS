//------------------------------------------------------------------------------------------------
//! CGS PILOT HUD (MechWarrior style), drawn for the local crew on top of the built-in HUD.
//! DISPLAY ONLY: no target lock, no extra keys, it never touches the camera or the controls.
//!  - TARGET BRACKETS around enemy (red) and friendly (green) vehicles, mechs and nearby soldiers,
//!    with range. Enemies only show while not hidden behind terrain.
//!  - RADAR (bottom centre): enemy / friendly blips, mech-up, range label, torso mark.
//!  - STATUS STRIP: heat bar, torso twist gauge, speed, and a flashing MISSILE INCOMING warning.
//! Optional per-mech settings: CGS_PilotHUDComponent on the mech. Without it, defaults are used.
//! Client only: nothing here is replicated.

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS MechCore/HUD", description: "Pilot HUD settings for this mech: sensor range, radar, brackets.")]
class CGS_PilotHUDComponentClass : ScriptComponentClass
{
}

class CGS_PilotHUDComponent : ScriptComponent
{
	[Attribute("true", UIWidgets.CheckBox, "Show the pilot HUD (target brackets, radar, status strip) in this mech.", category: "Pilot HUD")]
	bool m_bEnabled;

	[Attribute("800", UIWidgets.Auto, "Sensor range in metres: vehicles and mechs further away are not shown.", category: "Pilot HUD")]
	float m_fSensorRange;

	[Attribute("250", UIWidgets.Auto, "Soldiers on foot are shown within this many metres (0 = never).", category: "Pilot HUD")]
	float m_fInfantryRange;

	[Attribute("true", UIWidgets.CheckBox, "Enemies hidden behind terrain are not shown (friendlies always are).", category: "Pilot HUD")]
	bool m_bNeedLineOfSight;

	[Attribute("false", UIWidgets.CheckBox, "Also bracket empty / neutral vehicles (grey).", category: "Pilot HUD")]
	bool m_bShowNeutral;

	[Attribute("0", UIWidgets.Auto, "Radar range in metres. 0 = the sensor range.", category: "Pilot HUD")]
	float m_fRadarRange;


	[Attribute("true", UIWidgets.CheckBox, "Warn when rockets / missiles are flying at this mech.", category: "Pilot HUD")]
	bool m_bMissileWarning;

	static CGS_PilotHUDComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return CGS_PilotHUDComponent.Cast(mech.FindComponent(CGS_PilotHUDComponent));
	}
}

//------------------------------------------------------------------------------------------------
//! One tracked contact (client).
class CGS_HudContact
{
	IEntity m_pEntity;
	int m_iSide;          // 0 neutral, 1 friendly, 2 hostile
	bool m_bInfantry;
	float m_fRadius;
	vector m_vCentreLocal;  // bounds centre in the entity's own space (read once)
	bool m_bCentreKnown;
	vector m_vLastPos;
	float m_fLastTime;
	vector m_vVelocity;
	bool m_bSeen;         // passed the sensor checks on the last scan
}

//------------------------------------------------------------------------------------------------
class CGS_PilotHUD
{
	static const int SORT = 985;
	static const int MAX_BRACKETS = 24;
	static const int MAX_BLIPS = 40;
	static const float RADAR = 190;
	static const float MARGIN = 28;

	protected ref array<Widget> m_aAll = {};
	protected bool m_bBuilt;

	// Brackets: 4 edge lines + range text each
	protected ref array<ImageWidget> m_aBrTop = {};
	protected ref array<ImageWidget> m_aBrBottom = {};
	protected ref array<ImageWidget> m_aBrLeft = {};
	protected ref array<ImageWidget> m_aBrRight = {};
	protected ref array<TextWidget> m_aBrText = {};

	// Radar
	protected ImageWidget m_wRadarBg;
	protected ImageWidget m_wRadarH;
	protected ImageWidget m_wRadarV;
	protected ImageWidget m_wRadarSelf;
	protected ImageWidget m_wRadarTorso;
	protected TextWidget m_wRadarRange;
	protected ref array<ImageWidget> m_aBlips = {};

	// Status strip
	protected ImageWidget m_wHeatBg;
	protected ImageWidget m_wHeatBar;
	protected TextWidget m_wHeatText;
	protected ImageWidget m_wTwistBg;
	protected ImageWidget m_wTwistCentre;
	protected ImageWidget m_wTwistMark;
	protected TextWidget m_wSpeedText;
	protected TextWidget m_wWarning;

	// Sensor data
	protected ref array<ref CGS_HudContact> m_aContacts = {};
	protected ref array<IEntity> m_aCollect = {};
	protected float m_fNextScan;
	protected float m_fNextThreatScan;
	protected float m_fWarningUntil;
	protected ref map<IEntity, vector> m_mThreatPos = new map<IEntity, vector>();
	protected ref array<typename> m_aThreatTypes;

	//------------------------------------------------------------------------------------------------
	protected Widget Track(Widget w)
	{
		if (w)
			m_aAll.Insert(w);
		return w;
	}

	//------------------------------------------------------------------------------------------------
	protected ImageWidget Rect(float w, float h, Color color, int sortOffset = 0)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		ImageWidget img = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.STRETCH, color, SORT + sortOffset, null));
		if (!img)
			return null;
		img.SetColor(color);
		FrameSlot.SetAnchorMin(img, 0, 0);
		FrameSlot.SetAnchorMax(img, 0, 0);
		FrameSlot.SetAlignment(img, 0, 0);
		FrameSlot.SetSize(img, w, h);
		Track(img);
		return img;
	}

	//------------------------------------------------------------------------------------------------
	protected TextWidget Text(float w, float h, int size, Color color)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		TextWidget txt = TextWidget.Cast(ws.CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, color, SORT + 2, null));
		if (!txt)
			return null;
		txt.SetColor(color);
		txt.SetExactFontSize(size);
		FrameSlot.SetAnchorMin(txt, 0, 0);
		FrameSlot.SetAnchorMax(txt, 0, 0);
		FrameSlot.SetAlignment(txt, 0, 0);
		FrameSlot.SetSize(txt, w, h);
		Track(txt);
		return txt;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Place(Widget w, float x, float y)
	{
		if (w)
			FrameSlot.SetPos(w, x, y);
	}

	protected static void Size(Widget w, float x, float y)
	{
		if (w)
			FrameSlot.SetSize(w, x, y);
	}

	protected static void Show(Widget w, bool visible)
	{
		if (w)
			w.SetVisible(visible);
	}

	protected static void Tint(Widget w, Color c)
	{
		if (w)
			w.SetColor(c);
	}

	//------------------------------------------------------------------------------------------------
	protected Color SideColor(int side)
	{
		if (side == 2)
			return new Color(1, 0.25, 0.2, 0.95);
		if (side == 1)
			return new Color(0.3, 1, 0.4, 0.9);
		return new Color(0.75, 0.75, 0.75, 0.7);
	}

	//------------------------------------------------------------------------------------------------
	protected void Build()
	{
		Destroy();
		m_bBuilt = true;

		Color clear = new Color(1, 1, 1, 1);
		for (int i = 0; i < MAX_BRACKETS; i++)
		{
			m_aBrTop.Insert(Rect(10, 2, clear));
			m_aBrBottom.Insert(Rect(10, 2, clear));
			m_aBrLeft.Insert(Rect(2, 10, clear));
			m_aBrRight.Insert(Rect(2, 10, clear));
			m_aBrText.Insert(Text(140, 18, 12, clear));
		}

		m_wRadarBg = Rect(RADAR, RADAR, new Color(0.02, 0.06, 0.03, 0.55));
		m_wRadarH = Rect(RADAR, 1, new Color(0.3, 1, 0.4, 0.25), 1);
		m_wRadarV = Rect(1, RADAR, new Color(0.3, 1, 0.4, 0.25), 1);
		m_wRadarSelf = Rect(6, 10, new Color(0.3, 1, 0.4, 0.9), 2);
		m_wRadarTorso = Rect(6, 6, new Color(1, 0.85, 0.2, 0.9), 2);
		m_wRadarRange = Text(RADAR, 16, 11, new Color(0.3, 1, 0.4, 0.8));
		for (int b = 0; b < MAX_BLIPS; b++)
			m_aBlips.Insert(Rect(6, 6, clear, 2));

		m_wHeatBg = Rect(RADAR, 8, new Color(1, 1, 1, 0.12));
		m_wHeatBar = Rect(RADAR, 8, new Color(0.3, 1, 0.4, 0.9), 1);
		m_wHeatText = Text(RADAR, 16, 12, new Color(0.92, 0.92, 0.92, 1));
		m_wTwistBg = Rect(RADAR, 4, new Color(1, 1, 1, 0.12));
		m_wTwistCentre = Rect(2, 10, new Color(0.3, 1, 0.4, 0.7), 1);
		m_wTwistMark = Rect(6, 10, new Color(1, 0.85, 0.2, 0.95), 2);
		m_wSpeedText = Text(RADAR, 16, 12, new Color(0.92, 0.92, 0.92, 1));
		m_wWarning = Text(420, 34, 26, new Color(1, 0.2, 0.15, 1));
	}

	//------------------------------------------------------------------------------------------------
	void Destroy()
	{
		foreach (Widget w : m_aAll)
		{
			if (w)
				w.RemoveFromHierarchy();
		}
		m_aAll.Clear();
		m_aBrTop.Clear();
		m_aBrBottom.Clear();
		m_aBrLeft.Clear();
		m_aBrRight.Clear();
		m_aBrText.Clear();
		m_aBlips.Clear();
		m_bBuilt = false;
	}

	//------------------------------------------------------------------------------------------------
	protected float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() * 0.001;
	}

	//==================================================================================================
	// SENSORS
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected bool CollectEntity(IEntity entity)
	{
		if (entity)
			m_aCollect.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected Faction FactionOf(IEntity entity)
	{
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return null;
		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			faction = affiliation.GetDefaultAffiliatedFaction();
		return faction;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAlive(IEntity entity)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		return !damage || damage.GetState() != EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Terrain between the camera and the target? (cheap: terrain / world only)
	protected bool InSight(vector from, vector to)
	{
		BaseWorld world = GetGame().GetWorld();
		TraceParam param = new TraceParam();
		param.Start = from;
		param.End = to;
		param.Flags = TraceFlags.WORLD;
		param.LayerMask = EPhysicsLayerPresets.Projectile;
		return world.TraceMove(param, null) >= 0.98;
	}

	//------------------------------------------------------------------------------------------------
	protected float BoundRadius(IEntity entity)
	{
		vector mins;
		vector maxs;
		entity.GetBounds(mins, maxs);
		float r = (maxs - mins).Length() * 0.5;
		return Math.Clamp(r, 0.6, 60);
	}

	//------------------------------------------------------------------------------------------------
	protected vector CentreOf(CGS_HudContact c)
	{
		if (c.m_bInfantry)
			return c.m_pEntity.GetOrigin() + vector.Up * 1.0;
		if (!c.m_bCentreKnown)
		{
			vector mins;
			vector maxs;
			c.m_pEntity.GetBounds(mins, maxs);
			c.m_vCentreLocal = (mins + maxs) * 0.5;
			c.m_bCentreKnown = true;
		}
		return c.m_pEntity.CoordToParent(c.m_vCentreLocal);
	}

	//------------------------------------------------------------------------------------------------
	protected void Scan(IEntity mech, CGS_PilotHUDComponent cfg, vector camPos, vector camForward)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float range = 800;
		float infantryRange = 250;
		bool needSight = true;
		bool neutral = false;
		if (cfg)
		{
			range = cfg.m_fSensorRange;
			infantryRange = cfg.m_fInfantryRange;
			needSight = cfg.m_bNeedLineOfSight;
			neutral = cfg.m_bShowNeutral;
		}

		float now = Now();
		vector mechPos = mech.GetOrigin();
		Faction mechFaction = FactionOf(mech);

		m_aCollect.Clear();
		world.QueryEntitiesBySphere(mechPos, Math.Max(range, 10), CollectEntity, null, EQueryEntitiesFlags.DYNAMIC);

		foreach (CGS_HudContact old : m_aContacts)
		{
			if (old)
				old.m_bSeen = false;
		}

		foreach (IEntity hit : m_aCollect)
		{
			if (!hit)
				continue;

			IEntity target;
			bool infantry;
			ChimeraCharacter character = ChimeraCharacter.Cast(hit);
			if (character)
			{
				if (character.GetParent())
					continue; // crew: the vehicle is the contact
				target = character;
				infantry = true;
			}
			else
			{
				IEntity root = hit.GetRootParent();
				if (!root)
					root = hit;
				if (!Vehicle.Cast(root))
					continue;
				target = root;
			}

			if (target == mech || target.GetRootParent() == mech)
				continue;

			CGS_HudContact contact = FindContact(target);
			if (contact && contact.m_bSeen)
				continue; // already handled this scan (several parts of one vehicle)

			float dist = vector.Distance(target.GetOrigin(), mechPos);
			if (infantry && dist > infantryRange)
				continue;
			if (!IsAlive(target))
				continue;

			int side = 0;
			Faction faction = FactionOf(target);
			if (faction && mechFaction)
			{
				if (mechFaction.IsFactionEnemy(faction))
					side = 2;
				else
					side = 1;
			}
			if (side == 0 && !neutral)
				continue;

			if (!contact)
			{
				contact = new CGS_HudContact();
				contact.m_pEntity = target;
				contact.m_bInfantry = infantry;
				contact.m_fRadius = 0.9;
				if (!infantry)
					contact.m_fRadius = BoundRadius(target);
				contact.m_vLastPos = target.GetOrigin();
				contact.m_fLastTime = now;
				m_aContacts.Insert(contact);
			}
			contact.m_iSide = side;

			if (side == 2 && needSight && !InSight(camPos, CentreOf(contact)))
				continue;

			contact.m_bSeen = true;
		}
		m_aCollect.Clear();

		// Drop contacts not seen any more.
		for (int i = m_aContacts.Count() - 1; i >= 0; i--)
		{
			CGS_HudContact c2 = m_aContacts[i];
			if (!c2 || !c2.m_pEntity || !c2.m_bSeen)
				m_aContacts.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected CGS_HudContact FindContact(IEntity entity)
	{
		if (!entity)
			return null;
		foreach (CGS_HudContact c : m_aContacts)
		{
			if (c && c.m_pEntity == entity)
				return c;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame: velocity estimate for the lead pip.
	protected void TrackVelocities(float now)
	{
		foreach (CGS_HudContact c : m_aContacts)
		{
			IEntity e;
			if (c)
				e = c.m_pEntity;
			if (!e || !c)
				continue;
			float dt = now - c.m_fLastTime;
			if (dt < 0.1)
				continue;
			vector pos = e.GetOrigin();
			vector raw = (pos - c.m_vLastPos) * (1.0 / dt);
			if (raw.Length() > 400)
				raw = vector.Zero; // teleport
			c.m_vVelocity = c.m_vVelocity + (raw - c.m_vVelocity) * 0.5;
			c.m_vLastPos = pos;
			c.m_fLastTime = now;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rockets / missiles flying at the mech (vanilla projectiles).
	protected void ScanThreats(IEntity mech, float dt)
	{
		if (!m_aThreatTypes)
		{
			m_aThreatTypes = {};
			array<string> names = {"MissileMoveComponent", "ShellMoveComponent"};
			foreach (string n : names)
			{
				typename t = n.ToType();
				if (t)
					m_aThreatTypes.Insert(t);
			}
		}
		if (m_aThreatTypes.IsEmpty() || dt <= 0)
			return;

		BaseWorld world = GetGame().GetWorld();
		vector mechPos = mech.GetOrigin();
		m_aCollect.Clear();
		// DYNAMIC only: projectiles move. ALL would also list every rock and tree around the mech.
		world.QueryEntitiesBySphere(mechPos, 300, CollectEntity, null, EQueryEntitiesFlags.DYNAMIC);

		map<IEntity, vector> seen = new map<IEntity, vector>();
		foreach (IEntity e : m_aCollect)
		{
			if (!e)
				continue;
			bool isThreat;
			foreach (typename t : m_aThreatTypes)
			{
				if (e.FindComponent(t))
				{
					isThreat = true;
					break;
				}
			}
			if (!isThreat)
				continue;

			vector pos = e.GetOrigin();
			seen.Insert(e, pos);

			vector last;
			if (!m_mThreatPos.Find(e, last))
				continue;

			vector vel = (pos - last) * (1.0 / dt);
			float speed = vel.Length();
			if (speed < 20 || speed > 900)
				continue;

			// Closest approach to the mech within the next 4 s.
			vector rel = mechPos + vector.Up * 2 - pos;
			float t = vector.Dot(rel, vel) / (speed * speed);
			if (t <= 0 || t > 4)
				continue;
			vector closest = pos + vel * t;
			if (vector.Distance(closest, mechPos + vector.Up * 2) < 25)
				m_fWarningUntil = Now() + 1.0;
		}
		m_aCollect.Clear();
		m_mThreatPos = seen;
	}

	//==================================================================================================
	// DRAW
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected bool ToScreen(vector worldPos, vector camPos, vector camForward, out float x, out float y)
	{
		if (vector.Dot(worldPos - camPos, camForward) <= 0.5)
			return false;
		vector screen = GetGame().GetWorkspace().ProjWorldToScreen(worldPos, GetGame().GetWorld());
		x = screen[0];
		y = screen[1];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected string NameOf(IEntity e, bool infantry)
	{
		if (infantry)
			return "INFANTRY";

		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(e.FindComponent(SCR_EditableEntityComponent));
		if (editable && editable.GetInfo())
		{
			string name = editable.GetInfo().GetName();
			if (!name.IsEmpty())
				return WidgetManager.Translate(name);
		}

		EntityPrefabData data = e.GetPrefabData();
		if (data)
		{
			string path = data.GetPrefabName();
			int slash = path.LastIndexOf("/");
			if (slash >= 0)
				path = path.Substring(slash + 1, path.Length() - slash - 1);
			path.Replace(".et", "");
			path.Replace("_", " ");
			return path;
		}
		return "CONTACT";
	}

	//------------------------------------------------------------------------------------------------
	void Update(IEntity mech, string station)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		if (!ws || !world || !mech)
			return;

		CGS_PilotHUDComponent cfg = CGS_PilotHUDComponent.Find(mech);
		if (cfg && !cfg.m_bEnabled)
		{
			Destroy();
			return;
		}

		if (!m_bBuilt)
			Build();

		float screenW = ws.DPIUnscale(ws.GetWidth());
		float screenH = ws.DPIUnscale(ws.GetHeight());

		vector cam[4];
		world.GetCurrentCamera(cam);
		vector camPos = cam[3];
		vector camForward = cam[2].Normalized();
		vector camRight = cam[0].Normalized();

		float now = Now();
		if (now >= m_fNextScan)
		{
			m_fNextScan = now + 0.5;
			Scan(mech, cfg, camPos, camForward);
		}
		TrackVelocities(now);

		bool warn = !cfg || cfg.m_bMissileWarning;
		if (warn && now >= m_fNextThreatScan)
		{
			float threatDt = 0.25;
			if (m_fNextThreatScan > 0)
				threatDt = now - (m_fNextThreatScan - 0.25);
			m_fNextThreatScan = now + 0.25;
			ScanThreats(mech, Math.Max(threatDt, 0.05));
		}

		DrawBrackets(mech, camPos, camForward, camRight);
		DrawRadar(mech, cfg, screenW, screenH);
		DrawStatus(mech, screenW, screenH, now);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawBrackets(IEntity mech, vector camPos, vector camForward, vector camRight)
	{
		int used = 0;
		vector mechPos = mech.GetOrigin();
		foreach (CGS_HudContact c : m_aContacts)
		{
			IEntity e;
			if (c)
				e = c.m_pEntity;
			if (used >= MAX_BRACKETS)
				break;
			if (!e || !c || !c.m_bSeen)
				continue;

			vector centre = CentreOf(c);
			float x, y;
			if (!ToScreen(centre, camPos, camForward, x, y))
				continue;

			float ex, ey;
			float half = 8;
			if (ToScreen(centre + camRight * c.m_fRadius, camPos, camForward, ex, ey))
				half = Math.Clamp(Math.AbsFloat(ex - x), 7, 120);

			float thick = 2;
			Color color = SideColor(c.m_iSide);
			float size = half * 2;
			ImageWidget top = m_aBrTop[used];
			ImageWidget bottom = m_aBrBottom[used];
			ImageWidget left = m_aBrLeft[used];
			ImageWidget right = m_aBrRight[used];
			TextWidget label = m_aBrText[used];

			Size(top, size, thick);
			Size(bottom, size, thick);
			Size(left, thick, size);
			Size(right, thick, size);
			Place(top, x - half, y - half);
			Place(bottom, x - half, y + half - thick);
			Place(left, x - half, y - half);
			Place(right, x + half - thick, y - half);
			Place(label, x - half, y + half + 2);

			Tint(top, color);
			Tint(bottom, color);
			Tint(left, color);
			Tint(right, color);
			Tint(label, color);

			string text = string.Format("%1m", Math.Round(vector.Distance(e.GetOrigin(), mechPos)));
			if (label)
				label.SetText(text);

			Show(top, true);
			Show(bottom, true);
			Show(left, true);
			Show(right, true);
			Show(label, true);
			used++;
		}

		for (int i = used; i < MAX_BRACKETS; i++)
		{
			Show(m_aBrTop[i], false);
			Show(m_aBrBottom[i], false);
			Show(m_aBrLeft[i], false);
			Show(m_aBrRight[i], false);
			Show(m_aBrText[i], false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawRadar(IEntity mech, CGS_PilotHUDComponent cfg, float screenW, float screenH)
	{
		float range = 800;
		if (cfg)
		{
			range = cfg.m_fSensorRange;
			if (cfg.m_fRadarRange > 0)
				range = cfg.m_fRadarRange;
		}
		range = Math.Max(range, 10);

		float rx = (screenW - RADAR) * 0.5;
		float ry = screenH - RADAR - MARGIN;
		float cx = rx + RADAR * 0.5;
		float cy = ry + RADAR * 0.5;
		float r = RADAR * 0.5 - 4;

		Place(m_wRadarBg, rx, ry);
		Place(m_wRadarH, rx, cy);
		Place(m_wRadarV, cx, ry);
		Place(m_wRadarSelf, cx - 3, cy - 5);
		Place(m_wRadarRange, rx + 4, ry + 2);
		if (m_wRadarRange)
			m_wRadarRange.SetText(string.Format("RADAR %1m", Math.Round(range)));

		// Torso facing mark on the radar edge.
		float twist = 0;
		CGS_TorsoTwistComponent torso = CGS_TorsoTwistComponent.Find(mech);
		if (torso)
			twist = torso.GetTwist() * Math.DEG2RAD;
		Place(m_wRadarTorso, cx + Math.Sin(twist) * r - 3, cy - Math.Cos(twist) * r - 3);

		int used = 0;
		foreach (CGS_HudContact c : m_aContacts)
		{
			IEntity e;
			if (c)
				e = c.m_pEntity;
			if (used >= MAX_BLIPS)
				break;
			if (!e || !c || !c.m_bSeen)
				continue;

			vector local = mech.CoordToLocal(e.GetOrigin());
			float bx = local[0] / range * r;
			float by = -local[2] / range * r;
			float len = Math.Sqrt(bx * bx + by * by);
			if (len > r)
			{
				bx = bx * (r / len);
				by = by * (r / len);
			}

			float s = 6;
			if (c.m_bInfantry)
				s = 4;

			ImageWidget blip = m_aBlips[used];
			Size(blip, s, s);
			Place(blip, cx + bx - s * 0.5, cy + by - s * 0.5);
			Tint(blip, SideColor(c.m_iSide));
			Show(blip, true);
			used++;
		}
		for (int i = used; i < MAX_BLIPS; i++)
			Show(m_aBlips[i], false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawStatus(IEntity mech, float screenW, float screenH, float now)
	{
		float sx = (screenW - RADAR) * 0.5;
		float sy = screenH - RADAR - MARGIN - 64;

		// Heat.
		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(mech);
		bool hasHeat = heat != null;
		Show(m_wHeatBg, hasHeat);
		Show(m_wHeatBar, hasHeat);
		Show(m_wHeatText, hasHeat);
		if (hasHeat)
		{
			float h = heat.GetHeat01();
			Place(m_wHeatBg, sx, sy + 18);
			Place(m_wHeatBar, sx, sy + 18);
			Size(m_wHeatBar, Math.Max(RADAR * Math.Clamp(h, 0, 1), 0.5), 8);
			if (h >= 0.9)
				Tint(m_wHeatBar, new Color(1, 0.25, 0.2, 0.95));
			else if (h >= 0.75)
				Tint(m_wHeatBar, new Color(1, 0.75, 0.2, 0.95));
			else
				Tint(m_wHeatBar, new Color(0.3, 1, 0.4, 0.9));

			string ht = string.Format("HEAT %1", Math.Round(h * 100)) + "%";
			if (heat.IsShutDown())
				ht = ht + "  SHUTDOWN";
			else if (heat.IsOverride())
				ht = ht + "  OVERRIDE";
			Place(m_wHeatText, sx, sy);
			if (m_wHeatText)
				m_wHeatText.SetText(ht);
		}

		// Torso twist gauge: centre tick = legs, yellow mark = torso.
		CGS_TorsoTwistComponent torso = CGS_TorsoTwistComponent.Find(mech);
		bool hasTwist = torso != null;
		Show(m_wTwistBg, hasTwist);
		Show(m_wTwistCentre, hasTwist);
		Show(m_wTwistMark, hasTwist);
		if (hasTwist)
		{
			float ty = sy + 34;
			Place(m_wTwistBg, sx, ty + 3);
			Place(m_wTwistCentre, sx + RADAR * 0.5 - 1, ty);
			float frac = Math.Clamp(torso.GetTwist() / 90.0, -1, 1);
			Place(m_wTwistMark, sx + RADAR * 0.5 + frac * (RADAR * 0.5 - 3) - 3, ty);
		}

		// Speed.
		Place(m_wSpeedText, sx, sy + 46);
		if (m_wSpeedText)
			m_wSpeedText.SetText(string.Format("SPEED %1 km/h", Math.Round(BFA_MechHUD.GetSpeed(mech) * 3.6)));

		// Missile warning (flashing).
		int flash = now * 4;
		bool warning = now < m_fWarningUntil && flash % 2 == 0;
		Show(m_wWarning, warning);
		if (warning)
		{
			Place(m_wWarning, (screenW - 420) * 0.5, screenH * 0.22);
			if (m_wWarning)
				m_wWarning.SetText("MISSILE INCOMING");
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Draws the pilot HUD together with the built-in HUD (same lifetime).
modded class BFA_MechHUDDisplay
{
	protected ref CGS_PilotHUD m_CgsPilotHUD;

	override void Update(IEntity mech, string station)
	{
		super.Update(mech, station);
		if (!m_CgsPilotHUD)
			m_CgsPilotHUD = new CGS_PilotHUD();
		m_CgsPilotHUD.Update(mech, station);
	}

	override void Destroy()
	{
		if (m_CgsPilotHUD)
		{
			m_CgsPilotHUD.Destroy();
			m_CgsPilotHUD = null;
		}
		super.Destroy();
	}
}
