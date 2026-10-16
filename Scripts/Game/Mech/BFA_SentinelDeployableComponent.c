[EntityEditorProps(
	category: "GameScripted/Mech",
	description: "Inventory deployable that spawns the BFA Sentinel mech in front of the player"
)]
class BFA_SentinelDeployableComponentClass : SCR_BaseDeployableInventoryItemComponentClass
{
}


class BFA_SentinelDeployableComponent : SCR_BaseDeployableInventoryItemComponent
{
	// Mech prefab spawned by this deployment item.
	[Attribute(
		"",
		UIWidgets.ResourcePickerThumbnail,
		"Mech prefab this deployable item spawns (the Sample Mechs mod sets it to Sample Mech 1)"
	)]
	protected ResourceName m_sMechPrefab;

	// Keep the mech far enough away that it does not appear on top of the player.
	[Attribute("4.5", UIWidgets.EditBox, "Distance in front of player")]
	protected float m_fForwardDistance;

	// Sentinel body origin is above the feet, so give it a little clearance.
	[Attribute("0.75", UIWidgets.EditBox, "Vertical offset above detected surface")]
	protected float m_fVerticalOffset;

	// Leave at 0 if Sentinel faces correctly. Set to 180 if it spawns backwards.
	[Attribute("0", UIWidgets.EditBox, "Additional yaw rotation")]
	protected float m_fYawOffset;

	[Attribute("4.0", UIWidgets.EditBox, "Surface trace start height")]
	protected float m_fTraceStartHeight;

	[Attribute("12.0", UIWidgets.EditBox, "Surface trace depth")]
	protected float m_fTraceDepth;


	//------------------------------------------------------------------------------------------------
	override void Deploy(IEntity userEntity = null, bool reload = false)
	{
		// Match the working shield deploy path: only authority actually spawns.
		if (!m_RplComponent || m_RplComponent.IsProxy())
			return;

		if (m_bIsDeployed)
			return;

		if (!userEntity)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		BaseWorld world = owner.GetWorld();
		if (!world)
			return;

		if (m_sMechPrefab.IsEmpty())
		{
			BFA_MechLog.Log("[BFA SENTINEL DEPLOY] No mech prefab configured.", LogLevel.ERROR);
			return;
		}

		Resource mechResource = Resource.Load(m_sMechPrefab);
		if (!mechResource.IsValid())
		{
			BFA_MechLog.Log(
				"[BFA SENTINEL DEPLOY] Failed to load mech prefab: " + m_sMechPrefab,
				LogLevel.ERROR
			);
			return;
		}


		// ============================================================
		// PLAYER FACING
		// ============================================================

		vector playerTransform[4];
		userEntity.GetWorldTransform(playerTransform);

		vector forward = playerTransform[2];
		forward[1] = 0;

		if (forward.LengthSq() < 0.001)
			forward = "0 0 1";

		forward.Normalize();


		// ============================================================
		// INITIAL POSITION IN FRONT OF PLAYER
		// ============================================================

		vector spawnPosition =
			userEntity.GetOrigin() +
			(forward * m_fForwardDistance);


		// ============================================================
		// FIND THE ACTUAL SURFACE
		//
		// Same idea as the working medical shield:
		// bridge / rooftop / building floor / terrain support.
		// ============================================================

		TraceParam trace = new TraceParam();

		trace.Start =
			spawnPosition +
			(vector.Up * m_fTraceStartHeight);

		trace.End =
			trace.Start -
			(vector.Up * m_fTraceDepth);

		trace.Flags =
			TraceFlags.WORLD |
			TraceFlags.ENTS;

		trace.LayerMask =
			EPhysicsLayerPresets.Projectile;

		array<IEntity> excludedEntities =
		{
			userEntity,
			owner
		};

		trace.ExcludeArray = excludedEntities;

		float traceResult =
			world.TraceMove(
				trace,
				null
			);

		if (traceResult < 1.0)
		{
			spawnPosition =
				trace.Start +
				(
					(trace.End - trace.Start) *
					traceResult
				);

			spawnPosition =
				spawnPosition +
				(vector.Up * m_fVerticalOffset);
		}
		else
		{
			spawnPosition[1] =
				world.GetSurfaceY(
					spawnPosition[0],
					spawnPosition[2]
				);

			spawnPosition[1] =
				spawnPosition[1] +
				m_fVerticalOffset;
		}


		// ============================================================
		// BUILD UPRIGHT ROTATION
		// ============================================================

		vector spawnTransform[4];
		userEntity.GetWorldTransform(spawnTransform);

		vector angles =
			Math3D.MatrixToAngles(
				spawnTransform
			);

		// Only yaw follows player.
		angles[1] = 0;
		angles[2] = 0;

		angles[0] =
			angles[0] +
			m_fYawOffset;

		Math3D.AnglesToMatrix(
			angles,
			spawnTransform
		);

		spawnTransform[3] =
			spawnPosition;


		// ============================================================
		// SPAWN SENTINEL
		// ============================================================

		EntitySpawnParams spawnParams =
			new EntitySpawnParams();

		spawnParams.TransformMode =
			ETransformMode.WORLD;

		spawnParams.Transform =
			spawnTransform;

		IEntity spawnedMech =
			GetGame().SpawnEntityPrefab(
				mechResource,
				world,
				spawnParams
			);

		if (!spawnedMech)
		{
			BFA_MechLog.Log("[BFA SENTINEL DEPLOY] SpawnEntityPrefab failed.", LogLevel.ERROR);
			return;
		}

		BFA_MechLog.Log(
			"[BFA SENTINEL DEPLOY] Sentinel deployed at " +
			spawnPosition.ToString()
		);


		// ============================================================
		// MARK USED
		// ============================================================

		m_bIsDeployed = true;

		SetItemOwner(
			GetGame()
				.GetPlayerManager()
				.GetPlayerIdFromControlledEntity(
					userEntity
				)
		);

		Replication.BumpMe();

		if (m_OnDeployedStateChanged)
		{
			m_OnDeployedStateChanged.Invoke(
				true,
				this
			);
		}


		// ============================================================
		// ONE-WAY DEPLOYMENT
		//
		// Unlike the medical shield, the Sentinel does NOT need a
		// SCR_DeployableInventoryItemReplacementComponent and does not
		// keep the packed item hidden inside the deployed entity.
		//
		// Only consume the item AFTER the mech successfully spawned.
		// ============================================================

		SCR_EntityHelper.DeleteEntityAndChildren(
			owner
		);
	}
}
