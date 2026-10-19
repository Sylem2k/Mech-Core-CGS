//------------------------------------------------------------------------------------------------
//! DAMAGE PANEL of the built-in HUD (left side, above the MECH panel), MechWarrior 5 style:
//!  - left: a white front view of the mech - head, center / left / right torso, arms, legs -
//!    each piece coloured by the health of the part(s) in it:
//!      white = fine   yellow = damaged   orange = badly damaged   red = critical   dark = destroyed
//!    The center torso shows the hull. Pieces with no part are left out.
//!  - right: the mech's name, a segmented HULL bar and (with CGS_MechShieldComponent) a
//!    segmented SHIELD bar, each with "now / max" numbers.
//! Parts = entities with a BFA_MechPartHitZone. Its "Hud Section" says where it goes (AUTO =
//! legs from the mech's Leg Pairs, WEAPON parts are arms, side from the name or where it sits).
//! Client only, pure UI. Parts are re-read every 2 s (loadout swaps), health 4x a second.
//------------------------------------------------------------------------------------------------
//! Where a part is drawn on the HUD's damage picture.
enum CGS_EMechHudSection
{
	AUTO = 0,          // guessed from the part type and name (LEG + LEFT -> left leg ...)
	HEAD = 1,
	CENTER_TORSO = 2,
	LEFT_TORSO = 3,
	RIGHT_TORSO = 4,
	LEFT_ARM = 5,
	RIGHT_ARM = 6,
	LEFT_LEG = 7,
	RIGHT_LEG = 8,
	NONE = 9,          // not on the picture
}

class CGS_MechDollPiece
{
	int m_iSection;
	float m_fX;
	float m_fY;
	float m_fW;
	float m_fH;
	ImageWidget m_wEdge;
	ImageWidget m_wFill;
}

class CGS_MechPartsPanel
{
	static const float W = 340;
	static const float H = 206;
	static const float DOLL_W = 160;   // doll drawn at SCALE
	static const float DOLL_H = 210;
	static const float SCALE = 0.85;
	static const float PAD = 12;
	static const int SECTIONS = 10;
	static const int SEGMENTS = 10;
	static const float SEG_W = 14;
	static const float SEG_GAP = 2;
	static const float SEG_H = 12;

	protected ImageWidget m_wBg;
	protected ref array<ImageWidget> m_aBorder = {};
	protected TextWidget m_wLeft;
	protected TextWidget m_wRight;
	protected TextWidget m_wName;
	protected TextWidget m_wHullText;
	protected TextWidget m_wShieldText;
	protected ref array<ImageWidget> m_aHullSeg = {};
	protected ref array<ImageWidget> m_aShieldSeg = {};
	protected ref array<ref CGS_MechDollPiece> m_aPieces = {};

	protected ref array<IEntity> m_aParts = {};
	protected ref array<int> m_aPartSection = {};
	protected ref array<IEntity> m_aLeftLegs = {};   // from the mech's leg pairs (exact sides)
	protected ref array<IEntity> m_aRightLegs = {};
	protected IEntity m_pMech;
	protected string m_sMechName;
	protected float m_fNextList;
	protected float m_fNextHealth;

	// Per section (index = CGS_EMechHudSection): present, worst health, destroyed.
	protected ref array<bool> m_aHas = {};
	protected ref array<float> m_aHealth = {};
	protected ref array<bool> m_aDead = {};

	//------------------------------------------------------------------------------------------------
	//! Widgets come from the HUD so they are removed together with it.
	void Build(BFA_MechHUDDisplay hud)
	{
		Color white = new Color(0.95, 0.95, 0.95, 1);
		m_wBg = hud.PanelRect(W, H, new Color(0.02, 0.02, 0.03, 0.6));
		m_aBorder.Clear();
		for (int b = 0; b < 4; b++)
			m_aBorder.Insert(hud.PanelRect(W, 1, new Color(1, 1, 1, 0.55)));

		m_wLeft = hud.PanelText(16, 18, 13, white);
		m_wRight = hud.PanelText(16, 18, 13, white);
		m_wName = hud.PanelText(W - DOLL_W * SCALE - PAD * 3, 22, 14, white);
		m_wHullText = hud.PanelText(170, 18, 13, white);
		m_wShieldText = hud.PanelText(170, 18, 13, white);

		m_aHullSeg.Clear();
		m_aShieldSeg.Clear();
		for (int g = 0; g < SEGMENTS; g++)
		{
			m_aHullSeg.Insert(hud.PanelRect(SEG_W, SEG_H, white));
			m_aShieldSeg.Insert(hud.PanelRect(SEG_W, SEG_H, new Color(1, 0.5, 0.15, 1)));
		}

		m_aPieces.Clear();
		// Head + center torso (with waist).
		AddPiece(hud, CGS_EMechHudSection.HEAD, 66, 0, 28, 16);
		AddPiece(hud, CGS_EMechHudSection.CENTER_TORSO, 56, 18, 48, 74);
		AddPiece(hud, CGS_EMechHudSection.CENTER_TORSO, 62, 92, 36, 14);
		// Side torsos.
		AddMirrored(hud, CGS_EMechHudSection.LEFT_TORSO, CGS_EMechHudSection.RIGHT_TORSO, 32, 22, 22, 58);
		// Arms: upper + forearm.
		AddMirrored(hud, CGS_EMechHudSection.LEFT_ARM, CGS_EMechHudSection.RIGHT_ARM, 6, 20, 22, 42);
		AddMirrored(hud, CGS_EMechHudSection.LEFT_ARM, CGS_EMechHudSection.RIGHT_ARM, 10, 62, 16, 50);
		// Legs: thigh, shin, foot.
		AddMirrored(hud, CGS_EMechHudSection.LEFT_LEG, CGS_EMechHudSection.RIGHT_LEG, 38, 108, 30, 44);
		AddMirrored(hud, CGS_EMechHudSection.LEFT_LEG, CGS_EMechHudSection.RIGHT_LEG, 42, 152, 22, 42);
		AddMirrored(hud, CGS_EMechHudSection.LEFT_LEG, CGS_EMechHudSection.RIGHT_LEG, 34, 194, 36, 12);

		m_aHas.Clear();
		m_aHealth.Clear();
		m_aDead.Clear();
		for (int s = 0; s < SECTIONS; s++)
		{
			m_aHas.Insert(false);
			m_aHealth.Insert(1);
			m_aDead.Insert(false);
		}

		m_pMech = null;
		m_fNextList = 0;
		m_fNextHealth = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPiece(BFA_MechHUDDisplay hud, int section, float x, float y, float w, float h)
	{
		CGS_MechDollPiece piece = new CGS_MechDollPiece();
		piece.m_iSection = section;
		piece.m_fX = x;
		piece.m_fY = y;
		piece.m_fW = w;
		piece.m_fH = h;
		piece.m_wEdge = hud.PanelRect(w * SCALE + 2, h * SCALE + 2, new Color(0.05, 0.05, 0.05, 0.9));
		piece.m_wFill = hud.PanelRect(w * SCALE, h * SCALE, new Color(0.95, 0.95, 0.95, 0.95));
		m_aPieces.Insert(piece);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddMirrored(BFA_MechHUDDisplay hud, int leftSection, int rightSection, float x, float y, float w, float h)
	{
		AddPiece(hud, leftSection, x, y, w, h);
		AddPiece(hud, rightSection, DOLL_W - x - w, y, w, h);
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() * 0.001;
	}

	protected static void Show(Widget w, bool visible)
	{
		if (w)
			w.SetVisible(visible);
	}

	protected static void Place(Widget w, float x, float y)
	{
		if (w)
			FrameSlot.SetPos(w, x, y);
	}

	//------------------------------------------------------------------------------------------------
	//! bottom = the y where the panel's lower edge sits (top of the MECH panel minus a gap).
	void Update(IEntity mech, float x, float bottom)
	{
		if (!mech)
			return;

		float now = Now();
		if (mech != m_pMech || now >= m_fNextList)
		{
			if (mech != m_pMech)
				m_sMechName = MechName(mech);
			m_pMech = mech;
			m_fNextList = now + 2.0;
			m_fNextHealth = 0;
			CollectParts(mech);
		}

		if (now >= m_fNextHealth)
		{
			m_fNextHealth = now + 0.25;
			ReadHealth(mech);
		}

		float y = bottom - H;
		float dollX = x + PAD;
		float dollY = y + PAD;

		Place(m_wBg, x, y);
		// Thin white frame.
		if (m_aBorder.Count() == 4)
		{
			FrameSlot.SetSize(m_aBorder[0], W, 1);
			FrameSlot.SetSize(m_aBorder[1], W, 1);
			FrameSlot.SetSize(m_aBorder[2], 1, H);
			FrameSlot.SetSize(m_aBorder[3], 1, H);
			Place(m_aBorder[0], x, y);
			Place(m_aBorder[1], x, y + H - 1);
			Place(m_aBorder[2], x, y);
			Place(m_aBorder[3], x + W - 1, y);
		}

		// L / R under the doll's feet.
		Place(m_wLeft, dollX, dollY + DOLL_H * SCALE - 16);
		Place(m_wRight, dollX + DOLL_W * SCALE - 12, dollY + DOLL_H * SCALE - 16);
		if (m_wLeft)
			m_wLeft.SetText("L");
		if (m_wRight)
			m_wRight.SetText("R");

		// Right column: name, hull, shield.
		float rx = x + PAD * 2 + DOLL_W * SCALE;
		Place(m_wName, rx, y + PAD);
		if (m_wName)
			m_wName.SetText(m_sMechName);

		DrawHullAndShield(mech, rx, y + PAD + 36);

		foreach (CGS_MechDollPiece piece : m_aPieces)
		{
			int s = piece.m_iSection;
			bool has = m_aHas[s];
			Show(piece.m_wEdge, has);
			Show(piece.m_wFill, has);
			if (!has)
				continue;

			Place(piece.m_wEdge, dollX + piece.m_fX * SCALE - 1, dollY + piece.m_fY * SCALE - 1);
			Place(piece.m_wFill, dollX + piece.m_fX * SCALE, dollY + piece.m_fY * SCALE);
			if (piece.m_wFill)
				piece.m_wFill.SetColor(FillColor(m_aHealth[s], m_aDead[s]));
			if (piece.m_wEdge)
				piece.m_wEdge.SetColor(EdgeColor(m_aHealth[s], m_aDead[s]));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected Color FillColor(float health, bool dead)
	{
		if (dead)
			return new Color(0.15, 0.15, 0.15, 0.9);
		if (health > 0.75)
			return new Color(0.95, 0.95, 0.95, 0.95); // fine: white
		if (health > 0.5)
			return new Color(0.95, 0.9, 0.1, 0.95);   // damaged: yellow
		if (health > 0.25)
			return new Color(1, 0.55, 0.1, 0.95);     // badly damaged: orange
		return new Color(0.95, 0.15, 0.1, 0.95);       // critical: red
	}

	protected Color EdgeColor(float health, bool dead)
	{
		if (dead)
			return new Color(0.55, 0.55, 0.55, 0.9);
		return new Color(0.05, 0.05, 0.05, 0.9);
	}

	//------------------------------------------------------------------------------------------------
	//! Segmented bars with "now / max": HULL (white) and SHIELD (orange, only with a shield).
	protected void DrawHullAndShield(IEntity mech, float rx, float ry)
	{
		float hullNow = 0;
		float hullMax = 0;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(mech.FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.GetDefaultHitZone())
		{
			hullNow = Math.Max(damage.GetDefaultHitZone().GetHealth(), 0);
			hullMax = damage.GetDefaultHitZone().GetMaxHealth();
		}
		bool hasHull = hullMax > 0;
		Show(m_wHullText, hasHull);
		Place(m_wHullText, rx, ry);
		if (hasHull && m_wHullText)
			m_wHullText.SetText(string.Format("HULL    %1 / %2", Math.Round(hullNow), Math.Round(hullMax)));
		DrawSegments(m_aHullSeg, rx, ry + 20, hasHull, hullNow / Math.Max(hullMax, 1), new Color(0.95, 0.95, 0.95, 1));

		CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(mech);
		bool hasShield = shield && shield.GetMaxShield() > 0;
		Show(m_wShieldText, hasShield);
		Place(m_wShieldText, rx, ry + 44);
		if (hasShield && m_wShieldText)
			m_wShieldText.SetText(string.Format("SHIELD  %1 / %2", Math.Round(shield.GetShield()), Math.Round(shield.GetMaxShield())));
		float shield01 = 0;
		if (hasShield)
			shield01 = shield.GetShield01();
		DrawSegments(m_aShieldSeg, rx, ry + 64, hasShield, shield01, new Color(1, 0.5, 0.15, 1));
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSegments(array<ImageWidget> segs, float sx, float sy, bool visible, float frac, Color full)
	{
		frac = Math.Clamp(frac, 0, 1);
		// A segment is lit while any of its share is left (so 1% still shows one).
		int lit = (int)Math.Ceil(frac * SEGMENTS);
		Color empty = new Color(1, 1, 1, 0.12);
		for (int i = 0; i < segs.Count(); i++)
		{
			ImageWidget seg = segs[i];
			Show(seg, visible);
			if (!visible || !seg)
				continue;
			Place(seg, sx + i * (SEG_W + SEG_GAP), sy);
			if (i < lit)
				seg.SetColor(full);
			else
				seg.SetColor(empty);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadHealth(IEntity mech)
	{
		for (int s = 0; s < SECTIONS; s++)
		{
			m_aHas[s] = false;
			m_aHealth[s] = 1;
			m_aDead[s] = false;
		}

		// The hull is the center torso.
		float hull = BFA_MechHUD.GetHealth01(mech);
		if (hull >= 0)
		{
			m_aHas[CGS_EMechHudSection.CENTER_TORSO] = true;
			m_aHealth[CGS_EMechHudSection.CENTER_TORSO] = hull;
			m_aDead[CGS_EMechHudSection.CENTER_TORSO] = hull <= 0;
		}

		for (int i = 0; i < m_aParts.Count(); i++)
		{
			IEntity part = m_aParts[i];
			int section = m_aPartSection[i];
			BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(part);
			if (!zone || section <= 0 || section >= SECTIONS)
				continue;

			float health = 1;
			if (zone.GetMaxHealth() > 0)
				health = Math.Clamp(zone.GetHealth() / zone.GetMaxHealth(), 0, 1);
			bool dead = zone.GetDamageState() == EDamageState.DESTROYED;

			// Several parts in one piece: the worst one shows.
			if (!m_aHas[section] || health < m_aHealth[section])
				m_aHealth[section] = health;
			if (dead)
				m_aDead[section] = true;
			m_aHas[section] = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectParts(IEntity mech)
	{
		m_aParts.Clear();
		m_aPartSection.Clear();
		CollectLegSides(mech);
		AddParts(mech, mech, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The mech's Leg Pairs (Movement component) say exactly which leg is left and which right.
	protected void CollectLegSides(IEntity mech)
	{
		m_aLeftLegs.Clear();
		m_aRightLegs.Clear();
		BFA_MechMovementComponent movement = BFA_MechMovementComponent.Cast(mech.FindComponent(BFA_MechMovementComponent));
		SlotManagerComponent slots = SlotManagerComponent.Cast(mech.FindComponent(SlotManagerComponent));
		if (!movement || !slots || !movement.m_aLegPairs)
			return;

		foreach (BFA_MechLegPair pair : movement.m_aLegPairs)
		{
			if (!pair)
				continue;
			EntitySlotInfo left = slots.GetSlotByName(pair.m_sLeftLegSlotName);
			EntitySlotInfo right = slots.GetSlotByName(pair.m_sRightLegSlotName);
			if (left && left.GetAttachedEntity())
				m_aLeftLegs.Insert(left.GetAttachedEntity());
			if (right && right.GetAttachedEntity())
				m_aRightLegs.Insert(right.GetAttachedEntity());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! -1 = on a left leg, 1 = on a right leg, 0 = not on a leg (checks the part and what it sits on).
	protected int LegSide(IEntity mech, IEntity part)
	{
		IEntity e = part;
		while (e && e != mech)
		{
			if (m_aLeftLegs.Contains(e))
				return -1;
			if (m_aRightLegs.Contains(e))
				return 1;
			e = e.GetParent();
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddParts(IEntity mech, IEntity parent, int depth)
	{
		if (!parent || depth > 4)
			return;

		IEntity child = parent.GetChildren();
		while (child)
		{
			BFA_MechPartHitZone zone = BFA_MechPartHitZone.FindOn(child);
			if (zone && zone.ShowOnHUD())
			{
				int section = zone.GetHudSection();
				if (section == CGS_EMechHudSection.AUTO)
					section = GuessSection(mech, child, zone);
				if (section != CGS_EMechHudSection.NONE)
				{
					m_aParts.Insert(child);
					m_aPartSection.Insert(section);
				}
			}

			AddParts(mech, child, depth + 1);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! AUTO: legs are legs, weapons / arms are arms, cockpit / head is the head; the side comes
	//! from the name (LEFT / RIGHT, _L / _R) or else from where the part sits on the mech.
	protected int GuessSection(IEntity mech, IEntity part, BFA_MechPartHitZone zone)
	{
		// Legs from the leg pairs: always the right side.
		int legSide = LegSide(mech, part);
		if (legSide < 0)
			return CGS_EMechHudSection.LEFT_LEG;
		if (legSide > 0)
			return CGS_EMechHudSection.RIGHT_LEG;

		string name = zone.GetDisplayName();
		if (name.IsEmpty() && part.GetPrefabData())
		{
			name = part.GetPrefabData().GetPrefabName();
			int slash = name.LastIndexOf("/");
			if (slash >= 0)
				name = name.Substring(slash + 1, name.Length() - slash - 1);
		}
		name.ToUpper();
		name.Replace(".ET", "");

		int side = 0; // -1 left, 1 right
		if (name.Contains("LEFT") || name.EndsWith("_L") || name.EndsWith(" L"))
			side = -1;
		else if (name.Contains("RIGHT") || name.EndsWith("_R") || name.EndsWith(" R"))
			side = 1;
		else
		{
			float localX = mech.CoordToLocal(part.GetOrigin())[0];
			if (localX < -0.2)
				side = -1;
			else if (localX > 0.2)
				side = 1;
		}

		if (zone.GetPartType() == BFA_EMechPart.LEG || name.Contains("LEG"))
		{
			if (side > 0)
				return CGS_EMechHudSection.RIGHT_LEG;
			return CGS_EMechHudSection.LEFT_LEG;
		}

		if (name.Contains("HEAD") || name.Contains("COCKPIT"))
			return CGS_EMechHudSection.HEAD;

		if (name.Contains("TORSO") || zone.GetPartType() == BFA_EMechPart.ARMOR)
		{
			if (side < 0)
				return CGS_EMechHudSection.LEFT_TORSO;
			if (side > 0)
				return CGS_EMechHudSection.RIGHT_TORSO;
			return CGS_EMechHudSection.CENTER_TORSO;
		}

		// Weapons and arms.
		if (side > 0)
			return CGS_EMechHudSection.RIGHT_ARM;
		if (side < 0)
			return CGS_EMechHudSection.LEFT_ARM;
		return CGS_EMechHudSection.CENTER_TORSO;
	}

	//------------------------------------------------------------------------------------------------
	protected string MechName(IEntity mech)
	{
		string name;
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(mech.FindComponent(SCR_EditableEntityComponent));
		if (editable && editable.GetInfo())
			name = WidgetManager.Translate(editable.GetInfo().GetName());

		if (name.IsEmpty() && mech.GetPrefabData())
		{
			name = mech.GetPrefabData().GetPrefabName();
			int slash = name.LastIndexOf("/");
			if (slash >= 0)
				name = name.Substring(slash + 1, name.Length() - slash - 1);
			name.Replace(".et", "");
			name.Replace("_", " ");
		}

		name.ToUpper();
		if (name.Length() > 20)
			name = name.Substring(0, 20);
		return name;
	}
}
