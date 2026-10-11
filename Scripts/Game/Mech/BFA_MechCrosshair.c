//------------------------------------------------------------------------------------------------
//! What a crosshair looks like. Put one on a weapon part (module > Crosshair) to give that weapon
//! its own reticle, and one on the mech (BFA_MechAimComponent > Default Crosshair) as fallback.
//! Leave the texture empty to get the built-in "+" made of plain bars.
[BaseContainerProps()]
class BFA_MechCrosshair
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Crosshair image: a texture (.edds) or an imageset (.imageset, then set Image Name). Empty = built-in cross.", params: "edds imageset")]
	ResourceName m_sTexture;

	[Attribute("", UIWidgets.EditBox, "Image name inside the imageset (only when Texture is an .imageset).")]
	string m_sImageName;

	[Attribute("48", UIWidgets.Auto, "Size on screen (UI units, 1080p reference).")]
	float m_fSize;

	[Attribute("1 1 1 0.9", UIWidgets.ColorPicker, "Colour / opacity.")]
	ref Color m_Color;

	[Attribute("true", UIWidgets.CheckBox, "Also show a small marker where each gun of this weapon is ACTUALLY pointing (shows arms at their traverse limit or still turning).")]
	bool m_bShowGunMarkers;

	[Attribute("10", UIWidgets.Auto, "Gun marker size.")]
	float m_fGunMarkerSize;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional gun marker image (.edds / .imageset). Empty = small square.", params: "edds imageset")]
	ResourceName m_sGunMarkerTexture;

	[Attribute("", UIWidgets.EditBox, "Image name inside the gun marker imageset.")]
	string m_sGunMarkerImageName;
}

//------------------------------------------------------------------------------------------------
//! Local crosshair HUD for whoever sits in a crew station. Client only, pure UI - never on a
//! dedicated server. Owned by BFA_MechAimComponent.
class BFA_MechCrosshairHUD
{
	protected ref array<Widget> m_aMain = {};      // main reticle widgets
	protected ref array<Widget> m_aMarkers = {};    // per-gun markers (pooled)
	protected BFA_MechCrosshair m_Current;          // config the main reticle was built from
	protected bool m_bBuilt;

	static const int SORT = 1000;

	//------------------------------------------------------------------------------------------------
	protected static Color MakeColor(BFA_MechCrosshair config)
	{
		if (config && config.m_Color)
			return config.m_Color;

		return new Color(1, 1, 1, 0.9);
	}

	//------------------------------------------------------------------------------------------------
	protected ImageWidget CreateImage(ResourceName texture, string imageName, float width, float height, Color color)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		ImageWidget image = ImageWidget.Cast(workspace.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.STRETCH, color, SORT, null));
		if (!image)
			return null;

		if (!texture.IsEmpty())
		{
			if (texture.EndsWith(".imageset"))
				image.LoadImageFromSet(0, texture, imageName);
			else
				image.LoadImageTexture(0, texture);
		}

		image.SetColor(color);
		FrameSlot.SetAnchorMin(image, 0, 0);
		FrameSlot.SetAnchorMax(image, 0, 0);
		FrameSlot.SetAlignment(image, 0.5, 0.5);
		FrameSlot.SetSize(image, width, height);
		return image;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearList(notnull array<Widget> widgets)
	{
		foreach (Widget w : widgets)
		{
			if (w)
				w.RemoveFromHierarchy();
		}
		widgets.Clear();
	}

	//------------------------------------------------------------------------------------------------
	void Destroy()
	{
		ClearList(m_aMain);
		ClearList(m_aMarkers);
		m_Current = null;
		m_bBuilt = false;
	}

	//------------------------------------------------------------------------------------------------
	//! (Re)build the main reticle when the weapon's crosshair config changes.
	protected void BuildMain(BFA_MechCrosshair config)
	{
		ClearList(m_aMain);
		m_Current = config;
		m_bBuilt = true;

		Color color = MakeColor(config);
		float size = 48;
		if (config)
			size = Math.Max(config.m_fSize, 4);

		if (config && !config.m_sTexture.IsEmpty())
		{
			ImageWidget image = CreateImage(config.m_sTexture, config.m_sImageName, size, size, color);
			if (image)
				m_aMain.Insert(image);
			return;
		}

		// Built-in cross: four bars around an open centre plus a centre dot.
		float bar = size * 0.32;
		float thick = Math.Max(size * 0.05, 2);
		m_aMain.Insert(CreateImage("", "", thick, bar, color)); // up
		m_aMain.Insert(CreateImage("", "", thick, bar, color)); // down
		m_aMain.Insert(CreateImage("", "", bar, thick, color)); // left
		m_aMain.Insert(CreateImage("", "", bar, thick, color)); // right
		m_aMain.Insert(CreateImage("", "", thick, thick, color)); // dot
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaceMain(float x, float y)
	{
		if (m_aMain.Count() == 1)
		{
			if (m_aMain[0])
				FrameSlot.SetPos(m_aMain[0], x, y);
			return;
		}

		if (m_aMain.Count() < 5)
			return;

		float size = 48;
		if (m_Current)
			size = Math.Max(m_Current.m_fSize, 4);

		float offset = size * 0.34;
		FrameSlot.SetPos(m_aMain[0], x, y - offset);
		FrameSlot.SetPos(m_aMain[1], x, y + offset);
		FrameSlot.SetPos(m_aMain[2], x - offset, y);
		FrameSlot.SetPos(m_aMain[3], x + offset, y);
		FrameSlot.SetPos(m_aMain[4], x, y);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetListVisible(notnull array<Widget> widgets, bool visible)
	{
		foreach (Widget w : widgets)
		{
			if (w)
				w.SetVisible(visible);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Screen position of a world point. False when behind the camera.
	protected bool WorldToScreen(vector worldPos, vector camPos, vector camForward, out float x, out float y)
	{
		if (vector.Dot(worldPos - camPos, camForward) <= 0.05)
			return false;

		vector screen = GetGame().GetWorkspace().ProjWorldToScreen(worldPos, GetGame().GetWorld());
		x = screen[0];
		y = screen[1];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Called every local aim tick.
	//! aimPoint: what the station is looking at. gunPoints: where each gun of the selected weapon
	//! group really points (empty = no markers).
	void Update(BFA_MechCrosshair config, vector aimPoint, notnull array<vector> gunPoints, bool showMarkers)
	{
		if (!GetGame().GetWorkspace() || !GetGame().GetWorld())
			return;

		if (!m_bBuilt || config != m_Current)
		{
			BuildMain(config);
			ClearList(m_aMarkers); // marker look may differ per weapon too
		}

		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		vector camForward = cam[2];
		camForward.Normalize();

		float x, y;
		bool onScreen = WorldToScreen(aimPoint, cam[3], camForward, x, y);
		SetListVisible(m_aMain, onScreen);
		if (onScreen)
			PlaceMain(x, y);

		// Gun markers (pooled).
		int needed = 0;
		if (showMarkers)
			needed = gunPoints.Count();

		Color color = MakeColor(config);
		float markerSize = 10;
		ResourceName markerTexture;
		string markerImage;
		if (config)
		{
			markerSize = Math.Max(config.m_fGunMarkerSize, 2);
			markerTexture = config.m_sGunMarkerTexture;
			markerImage = config.m_sGunMarkerImageName;
		}

		while (m_aMarkers.Count() < needed)
			m_aMarkers.Insert(CreateImage(markerTexture, markerImage, markerSize, markerSize, color));

		for (int i = 0; i < m_aMarkers.Count(); i++)
		{
			Widget marker = m_aMarkers[i];
			if (!marker)
				continue;

			if (i >= needed)
			{
				marker.SetVisible(false);
				continue;
			}

			float mx, my;
			bool visible = WorldToScreen(gunPoints[i], cam[3], camForward, mx, my);
			marker.SetVisible(visible);
			if (visible)
				FrameSlot.SetPos(marker, mx, my);
		}
	}
}
