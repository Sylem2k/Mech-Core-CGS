// CGS MechWarrior-inspired REPLACEMENT pilot layout.
// Selected only by CGS_PilotHUDComponent.m_bReferenceLayout on the piloted mech.
//
// The classic BFA_MechHUDDisplay does NOT draw behind this layout. Existing
// CGS_PilotHUD contact scanning, target brackets and missile detection are
// reused instead of creating another sensor or networked HUD system.
// All widgets are local-only and removed with the cockpit HUD instance.

class CGS_MWReferenceDollPart
{
    int m_iSection;
    int m_iView; // 0 frontal schematic, 1 rear schematic
    float m_fX;
    float m_fY;
    float m_fW;
    float m_fH;
    ImageWidget m_wCell;
}

class CGS_MWReferenceHUD : CGS_PilotHUD
{
    static const float MW_TAPE_WIDTH = 650;
    static const int MW_TAPE_TICKS = 49;
    static const int MW_CIRCLE_STEPS = 48;
    static const int MW_RADAR_STEPS = 32;
    static const int MW_RADAR_RINGS = 4;
    static const int MW_MAX_WEAPON_ROWS = 6;

    protected string m_sHudStation;

    protected ref array<ImageWidget> m_aTapeTicks = {};
    protected ref array<TextWidget> m_aTapeLabels = {};
    protected ref array<ImageWidget> m_aSightOuter = {};
    protected ref array<ImageWidget> m_aSightInner = {};
    protected ref array<ImageWidget> m_aRadarRings = {};
    protected ref array<ref CGS_MWReferenceDollPart> m_aDollParts = {};
    protected ref array<TextWidget> m_aWeaponNames = {};
    protected ref array<TextWidget> m_aWeaponAmmo = {};
    protected ref array<BFA_MechWeaponModuleComponent> m_aMWWeapons = {};
    // Real selected weapon pointing markers. Separate from camera-centered sight.
    protected ref array<ImageWidget> m_aGunAimPips = {};
    protected ref array<TextWidget> m_aGunAimIds = {};
    protected TextWidget m_wWeaponAimLegend;

    protected TextWidget m_wHeading;
    protected TextWidget m_wHipBearing;
    protected TextWidget m_wTargetReadout;
    protected TextWidget m_wRadarCaption;
    protected TextWidget m_wFrontLabel;
    protected TextWidget m_wRearLabel;
    protected TextWidget m_wArmorTitle;
    protected TextWidget m_wArmorValue;
    protected TextWidget m_wHeatSpeed;
    protected TextWidget m_wWeaponTitle;
    protected TextWidget m_wWeaponMode;
    protected ImageWidget m_wSightLeft;
    protected ImageWidget m_wSightRight;
    protected ImageWidget m_wSightTop;
    protected ImageWidget m_wSightBottom;
    protected ImageWidget m_wSightH;
    protected ImageWidget m_wSightV;
    protected ImageWidget m_wTopLine;
    protected ImageWidget m_wArmorPanel;
    protected ImageWidget m_wWeaponPanel;
    protected ImageWidget m_wHullTrack;
    protected ImageWidget m_wHullFill;
    protected ImageWidget m_wHeatTrack;
    protected ImageWidget m_wHeatFill;
    protected ref CGS_MWReferencePartData m_pPartData;

    protected float m_fLastWidth = -1;
    protected float m_fLastHeight = -1;
    protected float m_fLastRadarX = -1;
    protected float m_fLastRadarY = -1;
    protected float m_fNextDataRefresh;

    override void Update(IEntity mech, string station)
    {
        m_sHudStation = station;
        super.Update(mech, station);
    }

    override protected void Build()
    {
        super.Build();

        Color mint = new Color(0.38, 0.97, 0.79, 0.94);
        Color dim = new Color(0.25, 0.77, 0.65, 0.46);
        Color panel = new Color(0.01, 0.035, 0.042, 0.62);

        m_wHeading = Text(160, 28, 17, mint);
        m_wHipBearing = Text(420, 18, 12, mint);
        m_wTopLine = Rect(MW_TAPE_WIDTH, 1, dim);
        for (int i = 0; i < MW_TAPE_TICKS; i++)
        {
            m_aTapeTicks.Insert(Rect(2, 7, dim));
            m_aTapeLabels.Insert(Text(42, 16, 11, mint));
        }

        for (int n = 0; n < MW_CIRCLE_STEPS; n++)
            m_aSightOuter.Insert(Rect(3, 4, mint));
        for (int n = 0; n < 24; n++)
            m_aSightInner.Insert(Rect(2, 3, mint));

        m_wSightLeft = Rect(2, 145, dim);
        m_wSightRight = Rect(2, 145, dim);
        m_wSightTop = Rect(35, 2, mint);
        m_wSightBottom = Rect(35, 2, mint);
        m_wSightH = Rect(20, 1, mint);
        m_wSightV = Rect(1, 20, mint);
        m_wTargetReadout = Text(230, 18, 12, mint);
        m_wWeaponAimLegend = Text(390, 22, 12, mint);
        for (int i = 0; i < MW_MAX_WEAPON_ROWS; i++)
        {
            m_aGunAimPips.Insert(Rect(9, 9, mint));
            m_aGunAimIds.Insert(Text(35, 16, 11, mint));
        }

        for (int n = 0; n < MW_RADAR_RINGS * MW_RADAR_STEPS; n++)
            m_aRadarRings.Insert(Rect(2, 2, dim));
        m_wRadarCaption = Text(200, 20, 11, mint);

        m_wArmorPanel = Rect(305, 182, panel);
        m_wArmorTitle = Text(275, 18, 13, mint);
        m_wFrontLabel = Text(88, 18, 12, mint);
        m_wRearLabel = Text(88, 18, 12, mint);
        m_wArmorValue = Text(290, 22, 13, mint);
        m_wHullTrack = Rect(266, 6, dim);
        m_wHullFill = Rect(266, 6, mint);
        m_wHeatTrack = Rect(266, 5, dim);
        m_wHeatFill = Rect(266, 5, mint);
        m_wHeatSpeed = Text(290, 24, 12, mint);

        // Two views of one set of component health. No fictional directional
        // armor HP is invented: both use the same actual mech hit-zone values.
        for (int view = 0; view < 2; view++)
        {
            AddDoll(view, CGS_EMechHudSection.HEAD, 31, 0, 14, 10);
            AddDoll(view, CGS_EMechHudSection.CENTER_TORSO, 27, 12, 23, 28);
            AddDoll(view, CGS_EMechHudSection.LEFT_TORSO, 14, 15, 12, 22);
            AddDoll(view, CGS_EMechHudSection.RIGHT_TORSO, 51, 15, 12, 22);
            AddDoll(view, CGS_EMechHudSection.LEFT_ARM, 0, 14, 12, 34);
            AddDoll(view, CGS_EMechHudSection.RIGHT_ARM, 65, 14, 12, 34);
            AddDoll(view, CGS_EMechHudSection.LEFT_LEG, 20, 43, 16, 41);
            AddDoll(view, CGS_EMechHudSection.RIGHT_LEG, 41, 43, 16, 41);
        }

        m_wWeaponPanel = Rect(345, 248, panel);
        m_wWeaponTitle = Text(320, 22, 17, mint);
        m_wWeaponMode = Text(320, 20, 12, mint);
        for (int i = 0; i < MW_MAX_WEAPON_ROWS; i++)
        {
            m_aWeaponNames.Insert(Text(220, 20, 12, mint));
            m_aWeaponAmmo.Insert(Text(110, 20, 12, mint));
        }

        m_pPartData = new CGS_MWReferencePartData();
    }

    protected void AddDoll(int view, int section, float x, float y, float w, float h)
    {
        CGS_MWReferenceDollPart p = new CGS_MWReferenceDollPart();
        p.m_iView = view;
        p.m_iSection = section;
        // Reverse the front/rear geometry, but not its underlying HP values.
        if (view == 1)
            x = 77 - x - w;
        p.m_fX = x;
        p.m_fY = y;
        p.m_fW = w;
        p.m_fH = h;
        p.m_wCell = Rect(w, h, new Color(0.24, 0.9, 0.73, 0.88));
        m_aDollParts.Insert(p);
    }

    override void Destroy()
    {
        m_aTapeTicks.Clear();
        m_aTapeLabels.Clear();
        m_aSightOuter.Clear();
        m_aSightInner.Clear();
        m_aRadarRings.Clear();
        m_aDollParts.Clear();
        m_aWeaponNames.Clear();
        m_aWeaponAmmo.Clear();
        m_aMWWeapons.Clear();
        m_aGunAimPips.Clear();
        m_aGunAimIds.Clear();
        m_wWeaponAimLegend = null;
        m_pPartData = null;
        m_fLastWidth = -1;
        m_fLastHeight = -1;
        m_fLastRadarX = -1;
        m_fLastRadarY = -1;
        super.Destroy();
    }

    // This layout has its own compass/radar. Do not stack the previous PR's
    // minimal heading ribbon and 2-ring decorations over this replacement.
    override protected void DrawReferencePresentation(IEntity mech, bool enabled, bool radarVisible, float screenW, float screenH)
    {
    }

    override protected void DrawRadar(IEntity mech, CGS_PilotHUDComponent cfg, float screenW, float screenH, bool visible)
    {
        // Preserve original world-space contact validation, blip positioning,
        // sensor range and torso marker, but remove the old square backing.
        super.DrawRadar(mech, cfg, screenW, screenH, visible);
        Show(m_wRadarBg, false);
        Show(m_wRadarH, false);
        Show(m_wRadarV, false);
        Show(m_wRadarRange, false);

        for (int i = 0; i < m_aRadarRings.Count(); i++)
            Show(m_aRadarRings[i], visible);
        Show(m_wRadarCaption, visible);

        if (!visible)
            return;

        float radarX = (screenW - RADAR) * 0.5;
        float radarY = screenH - RADAR - MARGIN;
        if (radarX != m_fLastRadarX || radarY != m_fLastRadarY)
        {
            m_fLastRadarX = radarX;
            m_fLastRadarY = radarY;
            float cx = radarX + RADAR * 0.5;
            float cy = radarY + RADAR * 0.5;
            for (int i = 0; i < m_aRadarRings.Count(); i++)
            {
                int ringIndex = i / MW_RADAR_STEPS;
                int tick = i % MW_RADAR_STEPS;
                float radius = (RADAR * 0.5 - 5) * (ringIndex + 1) / MW_RADAR_RINGS;
                float angle = 6.2831853 * tick / MW_RADAR_STEPS;
                Place(m_aRadarRings[i],
                    cx + Math.Cos(angle) * radius - 1,
                    cy + Math.Sin(angle) * radius - 1);
            }
        }

        float sensorRange = 800;
        if (cfg)
        {
            sensorRange = cfg.m_fSensorRange;
            if (cfg.m_fRadarRange > 0)
                sensorRange = cfg.m_fRadarRange;
        }
        Place(m_wRadarCaption, radarX, radarY - 19);
        if (m_wRadarCaption)
            m_wRadarCaption.SetText(string.Format("TACTICAL SCAN  %1m", Math.Round(sensorRange)));
    }

    override protected void DrawStatus(IEntity mech, float screenW, float screenH, float now, bool visible)
    {
        // Keep missile-incoming detection from original CGS overlay while
        // replacing its heat/speed/twist *layout* with the reference panels.
        super.DrawStatus(mech, screenW, screenH, now, visible);
        Show(m_wHeatBg, false);
        Show(m_wHeatBar, false);
        Show(m_wHeatText, false);
        Show(m_wTwistBg, false);
        Show(m_wTwistCentre, false);
        Show(m_wTwistMark, false);
        Show(m_wSpeedText, false);

        float lowerYaw = WrapMWHeading(mech.GetYawPitchRoll()[0]);
        CGS_TorsoTwistComponent torso = CGS_TorsoTwistComponent.Find(mech);
        float upperYaw = lowerYaw;
        float twistYaw = 0;
        if (torso)
        {
            twistYaw = torso.GetTwist();
            upperYaw = WrapMWHeading(lowerYaw + twistYaw);
        }
        DrawCompass(screenW, screenH, lowerYaw, upperYaw, twistYaw);

        if (screenW != m_fLastWidth || screenH != m_fLastHeight)
        {
            m_fLastWidth = screenW;
            m_fLastHeight = screenH;
            PositionReticle(screenW, screenH);
        }

        if (now < m_fNextDataRefresh)
            return;
        m_fNextDataRefresh = now + 0.10;

        DrawArmor(mech, screenW, screenH);
        DrawWeapons(mech, screenW, screenH);
        DrawWeaponAimPips(mech, screenW, screenH);
    }

    protected float WrapMWHeading(float value)
    {
        while (value < 0)
            value = value + 360;
        while (value >= 360)
            value = value - 360;
        return value;
    }

    protected float ShortMWAngle(float a, float b)
    {
        float delta = b - a;
        while (delta > 180)
            delta = delta - 360;
        while (delta < -180)
            delta = delta + 360;
        return delta;
    }

    protected void DrawCompass(float screenW, float screenH, float hips, float upper, float twist)
    {
        float span = Math.Min(screenW * 0.65, MW_TAPE_WIDTH);
        float x0 = (screenW - span) * 0.5;
        float cx = screenW * 0.5;
        Place(m_wHeading, cx - 70, 18);
        Place(m_wHipBearing, cx - 190, 40);
        Place(m_wTopLine, x0, 64);
        Size(m_wTopLine, span, 1);
        if (m_wHeading)
            m_wHeading.SetText(string.Format("HDG %1", Math.Round(upper)));
        if (m_wHipBearing)
            m_wHipBearing.SetText(string.Format("HIPS %1    TORSO %2    WAIST %3",
                Math.Round(hips), Math.Round(upper), Math.Round(twist)));

        int centreTick = (int)Math.Round(upper / 5.0);
        for (int i = 0; i < MW_TAPE_TICKS; i++)
        {
            int value = (centreTick + i - MW_TAPE_TICKS / 2) * 5;
            float bearing = WrapMWHeading(value);
            float delta = ShortMWAngle(upper, bearing);
            bool isVisible = Math.AbsFloat(delta) <= 90;
            ImageWidget tick = m_aTapeTicks[i];
            TextWidget label = m_aTapeLabels[i];
            Show(tick, isVisible);
            Show(label, isVisible);
            if (!isVisible)
                continue;

            float x = cx + delta * span / 180.0;
            int norm = (int)bearing;
            bool major = norm % 30 == 0;
            if (major)
            {
                Place(tick, x, 52);
                Size(tick, 2, 10);
            }
            else
            {
                Place(tick, x, 57);
                Size(tick, 1, 5);
            }
            bool withLabel = norm % 45 == 0;
            Show(label, withLabel);
            if (!withLabel)
                continue;

            Place(label, x - 13, 72);
            string cardinal = string.Format("%1", norm);
            if (norm == 0)
                cardinal = "N";
            else if (norm == 90)
                cardinal = "E";
            else if (norm == 180)
                cardinal = "S";
            else if (norm == 270)
                cardinal = "W";
            if (label)
                label.SetText(cardinal);
        }
    }

    protected void PositionReticle(float screenW, float screenH)
    {
        // Centered on the actual crew camera's crosshair, not a mech-mounted
        // world point. Existing aim/weapon code still controls real convergence.
        float cx = screenW * 0.5;
        float cy = screenH * 0.50;
        float scale = Math.Clamp(screenH / 920.0, 0.60, 1.0);
        float r = 136 * scale;

        for (int i = 0; i < m_aSightOuter.Count(); i++)
        {
            float a = 6.2831853 * i / MW_CIRCLE_STEPS;
            Place(m_aSightOuter[i], cx + Math.Sin(a) * r - 1.5,
                cy + Math.Cos(a) * r - 1.5);
        }
        for (int j = 0; j < m_aSightInner.Count(); j++)
        {
            float a2 = 6.2831853 * j / 24.0;
            Place(m_aSightInner[j], cx + Math.Sin(a2) * r * 0.32 - 1,
                cy + Math.Cos(a2) * r * 0.32 - 1);
        }

        float edge = r * 1.28;
        Place(m_wSightLeft, cx - edge, cy - r * 0.5);
        Place(m_wSightRight, cx + edge, cy - r * 0.5);
        Size(m_wSightLeft, 2, r);
        Size(m_wSightRight, 2, r);
        Place(m_wSightTop, cx - 17, cy - r - 17);
        Place(m_wSightBottom, cx - 17, cy + r + 15);
        Place(m_wSightH, cx - 10, cy);
        Place(m_wSightV, cx, cy - 10);
        Place(m_wTargetReadout, cx + r * 0.3, cy + r * 0.48);
    }

    // Exact bore/pointing points from the selected real weapon modules.
    // Guns converge on camera trace; missile launch-lines are ONLY their
    // present muzzle direction, not a guaranteed terminal or lock position.
    protected void DrawWeaponAimPips(IEntity mech, float screenW, float screenH)
    {
        BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
        bool canAim = core && core.CanStationAim(m_sHudStation);
        for (int i = 0; i < m_aGunAimPips.Count(); i++)
        {
            Show(m_aGunAimPips[i], false);
            Show(m_aGunAimIds[i], false);
        }
        Show(m_wWeaponAimLegend, canAim);
        Show(m_wTargetReadout, canAim);
        if (!canAim)
            return;

        vector camera[4];
        GetGame().GetWorld().GetCurrentCamera(camera);
        vector forward = camera[2].Normalized();
        float aimX = screenW * 0.5;
        float aimY = screenH * 0.5;
        BFA_MechAimComponent aim = BFA_MechAimComponent.Cast(mech.FindComponent(BFA_MechAimComponent));
        if (aim)
        {
            float ax, ay;
            vector target = aim.GetLocalAimPointWorld();
            if (target != vector.Zero && ToScreen(target, camera[3], forward, ax, ay))
            {
                aimX = ax;
                aimY = ay;
            }
        }

        int guns = 0;
        int rockets = 0;
        int guided = 0;
        int rows = Math.Min(m_aMWWeapons.Count(), MW_MAX_WEAPON_ROWS);
        for (int j = 0; j < rows; j++)
        {
            BFA_MechWeaponModuleComponent weapon = m_aMWWeapons[j];
            if (!weapon)
                continue;
            bool rocket = BFA_MechRocketLauncherModuleComponent.Cast(weapon) != null;
            bool homing = CGS_Titan_Missile.Cast(weapon) != null;
            if (rocket)
                rockets++;
            else
                guns++;
            if (homing)
                guided++;

            // This is the same real per-muzzle point used by the original
            // BFA_MechCrosshairHUD (and kept when that HUD is hidden).
            float x, y;
            if (!ToScreen(weapon.GetPointedPoint(), camera[3], forward, x, y))
                continue;
            if (x < 0 || x > screenW || y < 0 || y > screenH)
                continue;

            ImageWidget marker = m_aGunAimPips[j];
            TextWidget id = m_aGunAimIds[j];
            Show(marker, true);
            Show(id, true);
            Place(marker, x - 4.5, y - 4.5);
            Place(id, x + 6, y - 9);
            if (rocket)
            {
                Tint(marker, new Color(1, 0.68, 0.23, 0.96));
                if (id)
                    id.SetText(string.Format("R%1", j + 1));
            }
            else
            {
                float dx = x - aimX;
                float dy = y - aimY;
                if (dx * dx + dy * dy <= 196)
                    Tint(marker, new Color(0.25, 1, 0.71, 0.96));
                else
                    Tint(marker, new Color(0.97, 0.79, 0.26, 0.96));
                if (id)
                    id.SetText(string.Format("G%1", j + 1));
            }
        }

        Place(m_wWeaponAimLegend, screenW * 0.5 - 190, screenH * 0.5 + 186);
        if (m_wWeaponAimLegend)
        {
            string info = "G = GUN BORE / CAMERA AIM";
            if (rockets > 0)
                info = "R = ROCKET / MISSILE LAUNCH LINE";
            if (guns > 0 && rockets > 0)
                info = "G = GUN    R = MISSILE LAUNCH";
            if (guided > 0)
                info = info + " (GUIDANCE AFTER LAUNCH)";
            m_wWeaponAimLegend.SetText(info);
        }
        if (m_wTargetReadout)
        {
            if (guided > 0)
                m_wTargetReadout.SetText("MISSILE AIM / NO LOCK DATA");
            else if (rockets > 0)
                m_wTargetReadout.SetText("ROCKET LAUNCH AIM");
            else
                m_wTargetReadout.SetText("WEAPON AIM");
        }
    }

    protected Color MWHealthColor(float health, bool dead)
    {
        if (dead || health <= 0)
            return new Color(0.11, 0.18, 0.18, 0.8);
        if (health < 0.25)
            return new Color(0.98, 0.23, 0.12, 0.97);
        if (health < 0.5)
            return new Color(0.98, 0.55, 0.13, 0.97);
        if (health < 0.75)
            return new Color(0.96, 0.91, 0.22, 0.96);
        return new Color(0.24, 0.93, 0.73, 0.96);
    }

    protected void DrawArmor(IEntity mech, float screenW, float screenH)
    {
        if (m_pPartData)
            m_pPartData.Poll(mech);

        float scale = Math.Clamp(screenH / 920.0, 0.67, 1.0);
        float panelW = 305 * scale;

        // The two component dolls occupy y=45..129 in panel coordinates.
        // The old HULL text started at y=125, on top of the legs, and the
        // bottom heat/speed row could extend outside the small-screen panel.
        // Keep the footer below BOTH dolls with enough unscaled text height
        // (Text widgets remain 22/24px tall even when the dolls shrink).
        float dollBottom = (45 + 84) * scale;
        float hullValueY = dollBottom + 12;
        float hullBarY = hullValueY + 24;
        float heatBarY = hullBarY + 13;
        float heatSpeedY = heatBarY + 11;
        float panelH = heatSpeedY + 24 + 10;
        float px = 20;
        float py = screenH - panelH - 22;

        Place(m_wArmorPanel, px, py);
        Size(m_wArmorPanel, panelW, panelH);
        Place(m_wArmorTitle, px + 8, py + 5);
        Place(m_wFrontLabel, px + 23 * scale, py + 25 * scale);
        Place(m_wRearLabel, px + 142 * scale, py + 25 * scale);
        if (m_wArmorTitle)
        {
            if (screenW < 1100 || panelW < 280)
                m_wArmorTitle.SetText("MECH / COMPONENTS");
            else
                m_wArmorTitle.SetText("MECH STATUS / COMPONENTS");
        }
        if (m_wFrontLabel)
            m_wFrontLabel.SetText("FRONT");
        if (m_wRearLabel)
            m_wRearLabel.SetText("REAR");

        float dollX = px + 18 * scale;
        float dollY = py + 45 * scale;
        for (int i = 0; i < m_aDollParts.Count(); i++)
        {
            CGS_MWReferenceDollPart part = m_aDollParts[i];
            if (!part || !part.m_wCell)
                continue;
            float ox = part.m_iView * 118 * scale;
            Place(part.m_wCell,
                dollX + ox + part.m_fX * scale, dollY + part.m_fY * scale);
            Size(part.m_wCell, part.m_fW * scale, part.m_fH * scale);

            if (!m_pPartData || !m_pPartData.HasPart(part.m_iSection))
                Tint(part.m_wCell, new Color(0.25, 0.60, 0.54, 0.22));
            else
                Tint(part.m_wCell, MWHealthColor(m_pPartData.PartHealth(part.m_iSection),
                    m_pPartData.PartDead(part.m_iSection)));
        }

        float hull = BFA_MechHUD.GetHealth01(mech);
        if (hull < 0)
            hull = 1;
        Place(m_wHullTrack, px + 8 * scale, py + hullBarY);
        Place(m_wHullFill, px + 8 * scale, py + hullBarY);
        Size(m_wHullTrack, 266 * scale, 5 * scale);
        Size(m_wHullFill, Math.Max(hull * 266 * scale, 1), 5 * scale);
        Tint(m_wHullFill, MWHealthColor(hull, hull <= 0));

        Place(m_wArmorValue, px + 8 * scale, py + hullValueY);
        if (m_wArmorValue)
            m_wArmorValue.SetText(string.Format("HULL %1", Math.Round(hull * 100)) + "%");

        float heat = 0;
        CGS_MechHeatComponent heatComponent = CGS_MechHeatComponent.Find(mech);
        bool hasHeat = heatComponent != null;
        if (hasHeat)
            heat = Math.Clamp(heatComponent.GetHeat01(), 0, 1);

        Show(m_wHeatTrack, hasHeat);
        Show(m_wHeatFill, hasHeat);
        Place(m_wHeatTrack, px + 8 * scale, py + heatBarY);
        Place(m_wHeatFill, px + 8 * scale, py + heatBarY);
        Size(m_wHeatTrack, 266 * scale, 4 * scale);
        Size(m_wHeatFill, Math.Max(heat * 266 * scale, 0.5), 4 * scale);
        Place(m_wHeatSpeed, px + 8 * scale, py + heatSpeedY);
        if (m_wHeatSpeed)
        {
            string heatText = "HEAT N/A";
            if (hasHeat)
                heatText = string.Format("HEAT %1", Math.Round(heat * 100)) + "%";
            if (panelW < 260)
                m_wHeatSpeed.SetText(heatText +
                    string.Format("   SPD %1", Math.Round(BFA_MechHUD.GetSpeed(mech) * 3.6)));
            else
                m_wHeatSpeed.SetText(heatText +
                    string.Format("    SPEED %1 km/h", Math.Round(BFA_MechHUD.GetSpeed(mech) * 3.6)));
        }
    }

    protected void DrawWeapons(IEntity mech, float screenW, float screenH)
    {
        BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
        bool canShoot = core && core.CanStationAim(m_sHudStation);
        m_aMWWeapons.Clear();
        int group = 0;
        if (canShoot)
        {
            group = core.GetSelectedWeaponGroup(m_sHudStation);
            BFA_MechHUD.GetStationWeapons(mech, m_sHudStation, group, m_aMWWeapons);
        }

        float scale = Math.Clamp(screenH / 920.0, 0.67, 1.0);
        float panelW = Math.Max(255, 345 * scale);
        int rows = Math.Min(m_aMWWeapons.Count(), MW_MAX_WEAPON_ROWS);
        float panelH = (75 + 25 * Math.Max(rows, 1)) * scale;
        float px = screenW - panelW - 20;
        float py = screenH - panelH - 22;

        Place(m_wWeaponPanel, px, py);
        Size(m_wWeaponPanel, panelW, panelH);
        Place(m_wWeaponTitle, px + 9 * scale, py + 6 * scale);
        Place(m_wWeaponMode, px + 9 * scale, py + 30 * scale);
        if (m_wWeaponTitle)
            m_wWeaponTitle.SetText("WEAPON SYSTEMS");
        if (m_wWeaponMode)
            m_wWeaponMode.SetText(string.Format("%1   GROUP %2", m_sHudStation, group));

        for (int i = 0; i < MW_MAX_WEAPON_ROWS; i++)
        {
            bool used = canShoot && i < rows;
            Show(m_aWeaponNames[i], used);
            Show(m_aWeaponAmmo[i], used);
            if (!used)
                continue;

            BFA_MechWeaponModuleComponent weapon = m_aMWWeapons[i];
            Place(m_aWeaponNames[i], px + 10 * scale, py + (58 + i * 25) * scale);
            Place(m_aWeaponAmmo[i], px + panelW - 102, py + (58 + i * 25) * scale);
            Size(m_aWeaponNames[i], panelW - 120, 20);
            Size(m_aWeaponAmmo[i], 97, 20);
            if (m_aWeaponNames[i])
            {
                string label = string.Format("%1. %2", i + 1, weapon.GetDisplayName());
                if (label.Length() > 21)
                    label = label.Substring(0, 20) + ".";
                m_aWeaponNames[i].SetText(label);
            }
            string ammo = "--";
            if (weapon.IsReloading())
                ammo = "RELOAD";
            else if (weapon.UsesAmmo())
                ammo = string.Format("%1/%2", weapon.GetRoundsInMagazine(), weapon.GetMagazineSize());
            if (m_aWeaponAmmo[i])
                m_aWeaponAmmo[i].SetText(ammo);
        }

        if (!canShoot || rows == 0)
        {
            if (m_wWeaponMode)
                m_wWeaponMode.SetText("NO FITTED WEAPONS");
        }
    }
}
