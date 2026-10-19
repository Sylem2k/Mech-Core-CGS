//------------------------------------------------------------------------------------------------
//! MechCore's built-in cockpit HUD (client only, pure UI, built in code - no layout file).
//!
//!   bottom right  WEAPONS: station + selected group, one row per weapon in that group
//!                 (name, rounds / magazine, spare magazines, reload bar), other groups listed
//!   bottom left   MECH: hull bar + %, speed, leg status, anti-missile charges, jump jets, swap status
//!   above it      DAMAGE: MechWarrior-style picture of the mech, coloured by part health - CGS_MechPartsPanel
//!
//! Turn it off per mech (BFA_MechAimComponent > Show HUD) or globally from a HUD mod:
//!   BFA_MechHUD.SetBuiltInHUDEnabled(false);
class BFA_MechHUDDisplay
{
	static const int SORT = 990;
	static const int MAX_WEAPON_ROWS = 8;
	static const float ROW_H = 26;
	static const float PANEL_W = 360;
	static const float MARGIN = 28;

	protected ref array<Widget> m_aAll = {};

	// Weapons panel
	protected ImageWidget m_wWeaponsBg;
	protected TextWidget m_wWeaponsTitle;
	protected ref array<TextWidget> m_aRowName = {};
	protected ref array<TextWidget> m_aRowAmmo = {};
	protected ref array<ImageWidget> m_aRowBarBg = {};
	protected ref array<ImageWidget> m_aRowBar = {};
	protected TextWidget m_wOtherGroups;

	// Mech panel
	protected ImageWidget m_wMechBg;
	protected TextWidget m_wMechTitle;
	protected ImageWidget m_wHullBarBg;
	protected ImageWidget m_wHullBar;
	protected TextWidget m_wHullText;
	protected TextWidget m_wStatusText;

	protected bool m_bBuilt;
	protected ref CGS_MechPartsPanel m_PartsPanel;
	protected ref array<BFA_MechWeaponModuleComponent> m_aWeapons = {};
	protected ref array<int> m_aGroups = {};

	//------------------------------------------------------------------------------------------------
	protected Widget Track(Widget w)
	{
		if (w)
			m_aAll.Insert(w);
		return w;
	}

	//------------------------------------------------------------------------------------------------
	protected ImageWidget MakeRect(float w, float h, Color color)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		ImageWidget img = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.STRETCH, color, SORT, null));
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
	protected TextWidget MakeText(float w, float h, int fontSize, Color color)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		TextWidget txt = TextWidget.Cast(ws.CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, color, SORT + 1, null));
		if (!txt)
			return null;

		txt.SetColor(color);
		txt.SetExactFontSize(fontSize);
		FrameSlot.SetAnchorMin(txt, 0, 0);
		FrameSlot.SetAnchorMax(txt, 0, 0);
		FrameSlot.SetAlignment(txt, 0, 0);
		FrameSlot.SetSize(txt, w, h);
		Track(txt);
		return txt;
	}

	//------------------------------------------------------------------------------------------------
	protected void Build()
	{
		Destroy();
		m_bBuilt = true;

		Color bg = new Color(0.02, 0.02, 0.03, 0.55);
		Color title = new Color(0.95, 0.75, 0.35, 1);
		Color white = new Color(0.92, 0.92, 0.92, 1);
		Color barBg = new Color(1, 1, 1, 0.12);
		Color bar = new Color(0.95, 0.75, 0.35, 0.9);

		m_wWeaponsBg = MakeRect(PANEL_W, 60, bg);
		m_wWeaponsTitle = MakeText(PANEL_W - 16, 22, 16, title);
		for (int i = 0; i < MAX_WEAPON_ROWS; i++)
		{
			m_aRowName.Insert(MakeText(210, ROW_H, 15, white));
			m_aRowAmmo.Insert(MakeText(130, ROW_H, 15, new Color(0.95, 0.75, 0.35, 1)));
			m_aRowBarBg.Insert(MakeRect(PANEL_W - 16, 3, barBg));
			m_aRowBar.Insert(MakeRect(PANEL_W - 16, 3, bar));
		}
		m_wOtherGroups = MakeText(PANEL_W - 16, 20, 13, new Color(0.8, 0.8, 0.8, 0.85));

		m_wMechBg = MakeRect(260, 96, bg);
		m_wMechTitle = MakeText(244, 22, 16, title);
		m_wHullBarBg = MakeRect(244, 8, barBg);
		m_wHullBar = MakeRect(244, 8, new Color(0.35, 0.85, 0.4, 0.95));
		m_wHullText = MakeText(244, 20, 14, white);
		m_wStatusText = MakeText(244, 40, 14, white);

		m_PartsPanel = new CGS_MechPartsPanel();
		m_PartsPanel.Build(this);
	}

	//------------------------------------------------------------------------------------------------
	//! For extra panels (CGS_MechPartsPanel): widgets that are removed together with this HUD.
	ImageWidget PanelRect(float w, float h, Color color)
	{
		return MakeRect(w, h, color);
	}

	TextWidget PanelText(float w, float h, int fontSize, Color color)
	{
		return MakeText(w, h, fontSize, color);
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
		m_aRowName.Clear();
		m_aRowAmmo.Clear();
		m_aRowBarBg.Clear();
		m_aRowBar.Clear();
		m_PartsPanel = null;
		m_bBuilt = false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetText(TextWidget w, string text)
	{
		if (w)
			w.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Place(Widget w, float x, float y)
	{
		if (w)
			FrameSlot.SetPos(w, x, y);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show(Widget w, bool visible)
	{
		if (w)
			w.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	//! Plain, short label: underscores become spaces (some fonts render them badly) and long
	//! names are cut so they never run into the ammo column.
	protected static string ShortName(string text, int maxChars)
	{
		text.Replace("_", " ");
		if (text.Length() > maxChars)
			text = text.Substring(0, maxChars - 1) + ".";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string AmmoText(BFA_MechWeaponModuleComponent weapon)
	{
		if (!weapon.UsesAmmo())
			return "--";

		if (weapon.IsReloading())
			return "RELOAD";

		string text = string.Format("%1 / %2", weapon.GetRoundsInMagazine(), weapon.GetMagazineSize());
		int spares = weapon.GetSpareMagazines();
		if (spares >= 0)
			text = text + string.Format("  [%1]", spares);

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Called every local aim tick while the local player is in a mech station.
	void Update(IEntity mech, string station)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws || !mech)
			return;

		if (!m_bBuilt)
			Build();

		float screenW = ws.DPIUnscale(ws.GetWidth());
		float screenH = ws.DPIUnscale(ws.GetHeight());

		// ---------------- Weapons panel (bottom right) ----------------
		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		bool canShoot = core && core.CanStationAim(station);

		m_aWeapons.Clear();
		int group = 0;
		if (canShoot)
		{
			group = core.GetSelectedWeaponGroup(station);
			BFA_MechHUD.GetStationWeapons(mech, station, group, m_aWeapons);
		}

		int rows = Math.Min(m_aWeapons.Count(), MAX_WEAPON_ROWS);
		float panelH = 36 + Math.Max(rows, 1) * (ROW_H + 10) + 26;
		float px = screenW - PANEL_W - MARGIN;
		float py = screenH - panelH - MARGIN;

		Show(m_wWeaponsBg, canShoot);
		Show(m_wWeaponsTitle, canShoot);
		Show(m_wOtherGroups, canShoot);
		if (m_wWeaponsBg)
			FrameSlot.SetSize(m_wWeaponsBg, PANEL_W, panelH);
		Place(m_wWeaponsBg, px, py);
		Place(m_wWeaponsTitle, px + 8, py + 6);
		string fireMode;
		if (m_aWeapons.Count() > 1)
		{
			fireMode = "   [GROUP]";
			BFA_MechWeaponComponent router = BFA_MechWeaponComponent.Cast(mech.FindComponent(BFA_MechWeaponComponent));
			if (router && router.IsChainFire(group))
				fireMode = "   [CHAIN]";
		}
		SetText(m_wWeaponsTitle, string.Format("%1  -  GROUP %2", station, group) + fireMode);

		for (int i = 0; i < MAX_WEAPON_ROWS; i++)
		{
			bool used = canShoot && i < rows;
			Show(m_aRowName[i], used);
			Show(m_aRowAmmo[i], used);
			Show(m_aRowBarBg[i], used);
			Show(m_aRowBar[i], used);
			if (!used)
				continue;

			BFA_MechWeaponModuleComponent weapon = m_aWeapons[i];
			float ry = py + 34 + i * (ROW_H + 10);
			Place(m_aRowName[i], px + 8, ry);
			Place(m_aRowAmmo[i], px + 222, ry);
			Place(m_aRowBarBg[i], px + 8, ry + ROW_H + 3);
			Place(m_aRowBar[i], px + 8, ry + ROW_H + 3);

			SetText(m_aRowName[i], ShortName(string.Format("%1. %2", i + 1, weapon.GetDisplayName()), 22));
			SetText(m_aRowAmmo[i], AmmoText(weapon));

			// Bar: reload progress while reloading, otherwise magazine fill.
			float fill = 1.0;
			if (weapon.IsReloading())
				fill = weapon.GetReloadProgress();
			else if (weapon.UsesAmmo() && weapon.GetMagazineSize() > 0)
			{
				float magSize = weapon.GetMagazineSize();
				fill = weapon.GetRoundsInMagazine() / magSize;
			}

			if (m_aRowBar[i])
				FrameSlot.SetSize(m_aRowBar[i], Math.Max((PANEL_W - 16) * Math.Clamp(fill, 0, 1), 0.5), 3);
		}

		// Other groups this station can switch to.
		string others;
		if (canShoot)
		{
			core.GetUsableWeaponGroups(station, m_aGroups);
			foreach (int g : m_aGroups)
			{
				if (g == group)
					continue;

				array<BFA_MechWeaponModuleComponent> groupWeapons = {};
				BFA_MechHUD.GetStationWeapons(mech, station, g, groupWeapons);
				if (groupWeapons.IsEmpty())
					continue;

				if (!others.IsEmpty())
					others = others + "   ";
				others = others + string.Format("G%1 %2", g, ShortName(groupWeapons[0].GetDisplayName(), 16));
				if (groupWeapons.Count() > 1)
					others = others + string.Format(" x%1", groupWeapons.Count());
			}
		}
		Place(m_wOtherGroups, px + 8, py + panelH - 22);
		SetText(m_wOtherGroups, others);

		// ---------------- Mech panel (bottom left) ----------------
		float mx = MARGIN;
		float my = screenH - 96 - MARGIN;
		Place(m_wMechBg, mx, my);
		Place(m_wMechTitle, mx + 8, my + 6);
		Place(m_wHullBarBg, mx + 8, my + 32);
		Place(m_wHullBar, mx + 8, my + 32);
		Place(m_wHullText, mx + 8, my + 42);
		Place(m_wStatusText, mx + 8, my + 62);

		SetText(m_wMechTitle, ShortName(station, 24));

		if (m_PartsPanel)
			m_PartsPanel.Update(mech, mx, my - 8);

		float hull = BFA_MechHUD.GetHealth01(mech);
		if (hull < 0)
			hull = 1;
		if (m_wHullBar)
		{
			FrameSlot.SetSize(m_wHullBar, Math.Max(244 * hull, 0.5), 8);
			if (hull > 0.5)
				m_wHullBar.SetColor(new Color(0.35, 0.85, 0.4, 0.95));
			else if (hull > 0.25)
				m_wHullBar.SetColor(new Color(0.95, 0.75, 0.25, 0.95));
			else
				m_wHullBar.SetColor(new Color(0.95, 0.25, 0.2, 0.95));
		}

		float kmh = BFA_MechHUD.GetSpeed(mech) * 3.6;
		SetText(m_wHullText, string.Format("HULL %1", Math.Round(hull * 100)) + "%" + string.Format("   SPEED %1 km/h", Math.Round(kmh)));

		string status;
		CGS_MechLegStatus legs = CGS_MechLegStatus.Get(mech);
		if (legs && !legs.m_bIgnored)
		{
			if (legs.IsImmobile())
				status = "LEGS DESTROYED - IMMOBILE";
			else if (legs.m_iDestroyed > 0)
				status = "LEG DESTROYED";
			else if (legs.m_fWorstHealth < 0.75)
				status = string.Format("LEG DAMAGED %1", Math.Round(legs.m_fWorstHealth * 100)) + "%";
		}

		int maxCharges;
		int charges = BFA_MechHUD.GetPointDefenseCharges(mech, maxCharges);
		if (charges >= 0)
		{
			if (!status.IsEmpty())
				status = status + "   ";
			status = status + string.Format("AMS %1/%2", charges, maxCharges);
		}

		int jets = BFA_MechHUD.GetJumpJetCharges(mech);
		if (jets >= 0)
		{
			if (!status.IsEmpty())
				status = status + "   ";
			status = status + string.Format("JETS %1", jets);
		}

		if (BFA_MechHUD.IsSwapping(mech))
		{
			if (!status.IsEmpty())
				status = status + "   ";
			status = status + "SWAPPING LOADOUT";
		}

		SetText(m_wStatusText, status);
	}
}
