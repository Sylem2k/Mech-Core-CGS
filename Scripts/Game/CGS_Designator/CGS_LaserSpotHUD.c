//------------------------------------------------------------------------------------------------
//! CGS LASER SPOT HUD - pilot view:
//!  - every friendly laser spot as a small square with its distance (green; your own = cyan),
//!  - "LASER LOCK" under the crosshair when the selected weapon group has a laser-guided
//!    launcher and a friendly spot is in front of it.
//! Client only, display only.
modded class BFA_MechHUDDisplay
{
	protected static const int CGS_MAX_SPOTS = 8;

	protected ref array<ImageWidget> m_aCgsSpotMark = {};
	protected ref array<TextWidget> m_aCgsSpotText = {};
	protected TextWidget m_wCgsLaserLock;
	protected float m_fCgsNextLockCheck;
	protected bool m_bCgsLaserLock;
	protected bool m_bCgsLaserReady;

	//------------------------------------------------------------------------------------------------
	override void Update(IEntity mech, string station)
	{
		super.Update(mech, station);
		if (!mech || !GetGame() || !GetGame().GetWorld() || !GetGame().GetWorkspace())
			return;

		// Widgets (made on first use; Destroy() removes everything Track()ed).
		while (m_aCgsSpotMark.Count() < CGS_MAX_SPOTS)
		{
			ImageWidget newMark = MakeRect(12, 12, new Color(0.3, 1, 0.5, 0.9));
			if (!newMark)
				return;
			m_aCgsSpotMark.Insert(newMark);
			m_aCgsSpotText.Insert(MakeText(120, 18, 13, new Color(0.3, 1, 0.5, 1)));
		}
		if (!m_wCgsLaserLock)
			m_wCgsLaserLock = MakeText(240, 22, 16, new Color(1, 0.35, 0.3, 1));

		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		WorkspaceWidget ws = GetGame().GetWorkspace();
		float screenW = ws.DPIUnscale(ws.GetWidth());
		float screenH = ws.DPIUnscale(ws.GetHeight());
		Faction mechFaction = CGS_LaserSpotComponent.FactionOf(mech);
		CGS_LaserSpotComponent ownSpot = CGS_LaserSpotComponent.Find(mech);

		// Spots.
		int used = 0;
		foreach (CGS_LaserSpotComponent spot : CGS_LaserSpotComponent.GetAll())
		{
			if (used >= CGS_MAX_SPOTS)
				break;
			if (!spot || !spot.IsLasing() || !spot.IsFriendlyTo(mechFaction))
				continue;
			vector pos = spot.GetSpot();
			if (vector.Dot(pos - cam[3], cam[2]) <= 0.5)
				continue;
			vector screen = ws.ProjWorldToScreen(pos, GetGame().GetWorld());
			if (screen[0] < 0 || screen[1] < 0 || screen[0] > screenW || screen[1] > screenH)
				continue;

			Color color = new Color(0.3, 1, 0.5, 0.95);
			if (spot == ownSpot)
				color = new Color(0.3, 0.9, 1, 0.95);

			ImageWidget mark = m_aCgsSpotMark[used];
			TextWidget text = m_aCgsSpotText[used];
			mark.SetColor(color);
			text.SetColor(color);
			Place(mark, screen[0] - 6, screen[1] - 6);
			Place(text, screen[0] + 12, screen[1] - 9);
			text.SetText("LASER " + Math.Round(vector.Distance(mech.GetOrigin(), pos)).ToString() + " m");
			Show(mark, true);
			Show(text, true);
			used++;
		}
		for (int i = used; i < CGS_MAX_SPOTS; i++)
		{
			Show(m_aCgsSpotMark[i], false);
			Show(m_aCgsSpotText[i], false);
		}

		// Laser lock for the selected group (checked 4 times a second).
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		if (now >= m_fCgsNextLockCheck)
		{
			m_fCgsNextLockCheck = now + 0.25;
			m_bCgsLaserReady = false;
			m_bCgsLaserLock = false;
			foreach (BFA_MechWeaponModuleComponent weapon : m_aWeapons)
			{
				CGS_Titan_Missile launcher = CGS_Titan_Missile.Cast(weapon);
				if (!launcher || !launcher.UsesLaser())
					continue;
				m_bCgsLaserReady = true;
				if (launcher.FindLaserSpotNow())
				{
					m_bCgsLaserLock = true;
					break;
				}
			}
		}

		Show(m_wCgsLaserLock, m_bCgsLaserReady);
		if (m_bCgsLaserReady)
		{
			Place(m_wCgsLaserLock, screenW * 0.5 - 60, screenH * 0.5 + 60);
			if (m_bCgsLaserLock)
			{
				m_wCgsLaserLock.SetText("LASER LOCK");
				m_wCgsLaserLock.SetColor(new Color(1, 0.35, 0.3, 1));
			}
			else
			{
				m_wCgsLaserLock.SetText("NO LASER SPOT");
				m_wCgsLaserLock.SetColor(new Color(0.6, 0.6, 0.6, 0.8));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Destroy()
	{
		super.Destroy(); // removes every widget made with MakeRect / MakeText
		m_aCgsSpotMark.Clear();
		m_aCgsSpotText.Clear();
		m_wCgsLaserLock = null;
	}
}
