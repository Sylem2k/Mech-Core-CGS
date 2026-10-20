//------------------------------------------------------------------------------------------------
//! CGS LASER SPOT - the "dot" a target designator paints. Put this on any entity that carries a
//! designator (the mech prefab for the mech designator module). It holds the live spot and sends
//! it to every machine (about 10 times a second while it moves), so:
//!  - laser-guided missiles (CGS Titan Missile, Guidance = Laser / Both) can fly to it,
//!  - friendly pilots see it on their HUD.
//! The spot sticks to a vehicle / mech / soldier it hits and follows it while it moves.
//! Needs an RplComponent on the same entity (mechs and items have one).
//! Carriers: the mech (CGS_TargetDesignator module) or a hand-held designator item
//! (CGS_HandDesignatorComponent, the spot is on the item and uses the carrier's faction).
//! OTHER SYSTEMS (gunships, artillery, airstrikes, AI): read the spots with GetAll() /
//! FindBestSpot(), or listen to GetOnLaserChanged() (server and clients: spot, lasing on/off).
[ComponentEditorProps(category: "CGS MechCore/Weapons", description: "Holds the live laser spot of a target designator on this entity and replicates it.")]
class CGS_LaserSpotComponentClass : ScriptComponentClass
{
}

class CGS_LaserSpotComponent : ScriptComponent
{
	[Attribute("100", UIWidgets.Auto, "Milliseconds between spot updates sent to clients while it moves.", category: "Laser Spot")]
	protected int m_iSyncIntervalMs;

	[RplProp(onRplName: "OnLasingReplicated")]
	protected bool m_bLasing;

	[RplProp()]
	protected vector m_vSpot;

	// Server only.
	protected IEntity m_pTarget;
	protected vector m_vTargetLocal;
	protected int m_iLastSyncMs;
	protected vector m_vLastSynced;

	protected IEntity m_pUser;        // who is lasing (server only)

	protected static ref array<CGS_LaserSpotComponent> s_aAll;
	protected static ref ScriptInvoker s_OnLaserChanged;

	//------------------------------------------------------------------------------------------------
	//! Fires with (CGS_LaserSpotComponent spot, bool lasing) when a laser turns on or off.
	static ScriptInvoker GetOnLaserChanged()
	{
		if (!s_OnLaserChanged)
			s_OnLaserChanged = new ScriptInvoker();
		return s_OnLaserChanged;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLasingReplicated()
	{
		if (s_OnLaserChanged)
			s_OnLaserChanged.Invoke(this, m_bLasing);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aAll)
			s_aAll = {};
		s_aAll.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aAll)
			s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	static array<CGS_LaserSpotComponent> GetAll()
	{
		if (!s_aAll)
			s_aAll = {};
		return s_aAll;
	}

	//------------------------------------------------------------------------------------------------
	static CGS_LaserSpotComponent Find(IEntity entity)
	{
		if (!entity)
			return null;
		return CGS_LaserSpotComponent.Cast(entity.FindComponent(CGS_LaserSpotComponent));
	}

	//------------------------------------------------------------------------------------------------
	bool IsLasing()
	{
		return m_bLasing;
	}

	//------------------------------------------------------------------------------------------------
	//! World position of the spot. On the server it follows the painted target exactly.
	vector GetSpot()
	{
		if (m_pTarget && Replication.IsServer())
			return m_pTarget.CoordToParent(m_vTargetLocal);
		return m_vSpot;
	}

	//------------------------------------------------------------------------------------------------
	//! Painted object (server only), or null for a spot on the ground.
	IEntity GetTarget()
	{
		return m_pTarget;
	}

	//------------------------------------------------------------------------------------------------
	Faction GetFaction()
	{
		return FactionOf(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Faction of an entity; an item without one uses whoever carries it (its root parent).
	static Faction FactionOf(IEntity entity)
	{
		if (!entity)
			return null;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation && entity.GetRootParent() && entity.GetRootParent() != entity)
			affiliation = FactionAffiliationComponent.Cast(entity.GetRootParent().FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return null;
		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			faction = affiliation.GetDefaultAffiliatedFaction();
		return faction;
	}

	//------------------------------------------------------------------------------------------------
	//! Can someone on 'faction' use this spot? Same / non-enemy faction yes. Unknown factions only
	//! match each other.
	bool IsFriendlyTo(Faction faction)
	{
		Faction mine = GetFaction();
		if (!mine || !faction)
			return mine == faction;
		if (mine == faction)
			return true;
		return !faction.IsFactionEnemy(mine);
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Who is lasing (server only): the mech, or the soldier holding the designator.
	IEntity GetUser()
	{
		if (m_pUser)
			return m_pUser;
		return GetOwner().GetRootParent();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: paint this spot. target = object it sticks to (or null for the ground).
	void ServerSetSpot(vector position, IEntity target, IEntity user = null)
	{
		if (!Replication.IsServer())
			return;
		m_pUser = user;

		m_pTarget = target;
		if (target)
			m_vTargetLocal = target.CoordToLocal(position);
		m_vSpot = position;

		int nowMs = 0;
		if (GetGame() && GetGame().GetWorld())
			nowMs = GetGame().GetWorld().GetWorldTime();

		bool started = !m_bLasing;
		m_bLasing = true;
		if (started)
			OnLasingReplicated();
		if (started || (nowMs - m_iLastSyncMs >= m_iSyncIntervalMs && vector.DistanceSq(position, m_vLastSynced) > 0.25))
		{
			m_iLastSyncMs = nowMs;
			m_vLastSynced = position;
			Replication.BumpMe();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: laser off.
	void ServerStop()
	{
		if (!Replication.IsServer() || !m_bLasing)
			return;
		m_bLasing = false;
		m_pTarget = null;
		m_pUser = null;
		Replication.BumpMe();
		OnLasingReplicated();
	}

	//------------------------------------------------------------------------------------------------
	//! Debug: one line per laser explaining why a missile would or would not take it.
	static string DescribeSpots(Faction faction, vector from, vector forward, float maxDistance, float coneDeg)
	{
		string text = "";
		float minCos = Math.Cos(Math.Clamp(coneDeg * 0.5, 1, 180) * Math.DEG2RAD);
		vector dir = forward.Normalized();
		int n = 0;
		foreach (CGS_LaserSpotComponent spot : GetAll())
		{
			if (!spot)
				continue;
			n++;
			string why = "OK";
			vector to = spot.GetSpot() - from;
			float distance = to.Length();
			float angle = 0;
			if (distance > 0.01)
				angle = Math.Acos(Math.Clamp(vector.Dot(to * (1.0 / distance), dir), -1, 1)) * Math.RAD2DEG;
			if (!spot.IsLasing())
				why = "laser off";
			else if (!spot.IsFriendlyTo(faction))
				why = "not friendly";
			else if (distance > maxDistance)
				why = "too far (" + Math.Round(distance).ToString() + " m > " + Math.Round(maxDistance).ToString() + ")";
			else if (vector.Dot(to * (1.0 / Math.Max(distance, 0.01)), dir) < minCos)
				why = "outside cone (" + Math.Round(angle).ToString() + " deg > " + Math.Round(coneDeg * 0.5).ToString() + ")";
			text = text + string.Format("\n   laser on %1: %2 m, %3 deg -> %4", spot.GetOwner(), Math.Round(distance), Math.Round(angle), why);
		}
		if (n == 0)
			text = "\n   no designators in the world (no CGS_LaserSpotComponent)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Best live spot for a missile: friendly to 'faction', within maxDistance of 'from' and inside
	//! a cone of coneDeg (full angle) around 'forward'. Nearest to the cone centre wins.
	static CGS_LaserSpotComponent FindBestSpot(Faction faction, vector from, vector forward, float maxDistance, float coneDeg)
	{
		CGS_LaserSpotComponent best;
		float bestCos = Math.Cos(Math.Clamp(coneDeg * 0.5, 1, 180) * Math.DEG2RAD);
		vector dir = forward.Normalized();

		foreach (CGS_LaserSpotComponent spot : GetAll())
		{
			if (!spot || !spot.IsLasing() || !spot.IsFriendlyTo(faction))
				continue;
			vector to = spot.GetSpot() - from;
			float distance = to.Length();
			if (distance < 1 || distance > maxDistance)
				continue;
			float cosAngle = vector.Dot(to * (1.0 / distance), dir);
			if (cosAngle >= bestCos)
			{
				bestCos = cosAngle;
				best = spot;
			}
		}
		return best;
	}
}
