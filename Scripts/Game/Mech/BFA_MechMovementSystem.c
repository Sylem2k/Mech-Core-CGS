class BFA_MechMovementSystem : GameSystem
{
	//------------------------------------------------------------------------------------------------
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		outInfo
			.SetAbstract(false)
			.AddPoint(ESystemPoint.Frame)
			.SetLocation(ESystemLocation.Both);
	}

	protected ref array<BFA_MechMovementComponent> m_aMovementComponents = {};

	//------------------------------------------------------------------------------------------------
	protected override void OnUpdate(ESystemPoint point)
	{
		float timeSlice;
		if (point == ESystemPoint.Frame)
			timeSlice = GetWorld().GetTimeSlice();
		else if (point == ESystemPoint.SimulatePhysics || point == ESystemPoint.PostSimulatePhysics)
			timeSlice = GetWorld().GetPhysicsTimeSlice();

		foreach (BFA_MechMovementComponent component : m_aMovementComponents)
		{
			if (component && component.GetOwner() && !component.GetOwner().IsDeleted())
				component.OnUpdate(timeSlice);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDiag(float timeSlice)
	{
		DbgUI.Begin("BFA_MechMovementSystem");
		DbgUI.Text("Movement components active: " + m_aMovementComponents.Count());

		if (DbgUI.Button("Dump active BFA_MechMovementComponents"))
		{
			foreach (BFA_MechMovementComponent component : m_aMovementComponents)
			{
				if (component)
					Print(component.GetOwner());
			}
		}

		DbgUI.End();
	}

	//------------------------------------------------------------------------------------------------
	void Register(BFA_MechMovementComponent component)
	{
		if (!component || !component.GetOwner())
			return;

		if (component.GetOwner().IsDeleted() || (component.GetOwner().GetFlags() & EntityFlags.USER5))
			return;

		if (!m_aMovementComponents.Contains(component))
		{
			m_aMovementComponents.Insert(component);
			BFA_MechLog.Log("[BFA MechCore] Movement system registered: " + component.GetOwner());
		}
	}

	//------------------------------------------------------------------------------------------------
	void Unregister(BFA_MechMovementComponent component)
	{
		m_aMovementComponents.RemoveItem(component);
	}
}
