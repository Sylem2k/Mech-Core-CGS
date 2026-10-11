//------------------------------------------------------------------------------------------------
//! CGS WEAPON TWEAKS (MY FIRST MECH add-on). MechCore is not edited; these hook in with
//! `modded class`:
//!  - Weapon Mount > Weapon Scale : make any weapon part bigger/smaller on THIS mech only
//!    (titan-sized guns from normal parts). Muzzles, barrels and aiming scale with it.
//!  - Effects > Tracer Every       : show the tracer/trail on only every Nth shot.
//!  - Effects > Projectile Scale   : size of the flying tracer / missile model.

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
modded class BFA_MechWeaponMountDefinition
{
	[Attribute("1", UIWidgets.Auto, "WEAPON SCALE: size of the weapon in this slot on this mech (1 = as modelled, 4 = four times bigger for a titan). Muzzle, barrels and aiming scale with it.", category: "Mount")]
	float m_fWeaponScale;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
modded class BFA_MechWeaponFX
{
	[Attribute("1", UIWidgets.Auto, "Show the projectile prefab / trail on only every Nth shot (1 = every shot, 5 = every 5th like a real belt). Saves frame rate on fast guns.", category: "Projectile")]
	int m_iTracerEvery;

	[Attribute("1", UIWidgets.Auto, "Size of the flying projectile model (tracer, rocket, missile). 1 = as modelled. Match it to the weapon's scale on big mechs.", category: "Projectile")]
	float m_fProjectileScale;
}

//------------------------------------------------------------------------------------------------
modded class BFA_MechWeaponMountComponent
{
	//------------------------------------------------------------------------------------------------
	//! Forces the weapon's size after anything MechCore does to its transform (rest pose, hardpoint
	//! Point, aiming). The axes are normalised first, so applying it again never grows the gun.
	protected void CgsEnforceScale(BFA_MechWeaponMountDefinition mount)
	{
		if (!mount || !mount.m_pEntity)
			return;

		float scale = mount.m_fWeaponScale;
		if (scale <= 0 || Math.AbsFloat(scale - 1.0) < 0.001)
			return;

		vector mat[4];
		mount.m_pEntity.GetLocalTransform(mat);
		float current = mat[0].Length();
		if (Math.AbsFloat(current - scale) < 0.001)
			return;

		for (int s = 0; s < 3; s++)
		{
			vector axis = mat[s];
			if (axis.LengthSq() > 0.000001)
				axis.Normalize();
			mat[s] = axis * scale;
		}

		mount.m_pEntity.SetLocalTransform(mat);
		mount.m_pEntity.Update();
	}

	//------------------------------------------------------------------------------------------------
	override protected bool ResolveMount(BFA_MechWeaponMountDefinition mount)
	{
		bool alreadyResolved = mount && mount.m_bResolved && mount.m_pEntity && mount.m_pParent;
		bool ok = super.ResolveMount(mount);
		if (ok && !alreadyResolved && mount.m_fWeaponScale > 0 && Math.AbsFloat(mount.m_fWeaponScale - 1.0) >= 0.001)
		{
			CgsEnforceScale(mount);
			Print(string.Format("[CGS SCALE] mount %1 -> weapon scale %2", mount.m_sMountId, mount.m_fWeaponScale));
		}
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	override void SetMountRestOffset(string mountId, vector offset, vector angles)
	{
		super.SetMountRestOffset(mountId, offset, angles);
		CgsEnforceScale(FindMount(mountId));
	}

	//------------------------------------------------------------------------------------------------
	override void SetMountRestModelTransform(string mountId, vector modelMat[4])
	{
		super.SetMountRestModelTransform(mountId, modelMat);
		CgsEnforceScale(FindMount(mountId));
	}

	//------------------------------------------------------------------------------------------------
	override protected void ApplyAimDriver(BFA_MechWeaponMountDefinition mount, float yaw, float pitch, vector finalMat[4])
	{
		super.ApplyAimDriver(mount, yaw, pitch, finalMat);
		if (mount.m_iAimDriver == BFA_EMechMountAimDriver.ENTITY_ROTATION)
			CgsEnforceScale(mount);
	}
}

//------------------------------------------------------------------------------------------------
modded class BFA_MechWeaponComponent
{
	protected ref map<int, int> m_mCgsTracerCounters;

	//------------------------------------------------------------------------------------------------
	override protected void PlayShotLocal(int hardpointIndex, int shotId, vector start, vector end, int flags, float speed, float length, float thickness)
	{
		BFA_MechWeaponModuleComponent module = FindLocalModule(hardpointIndex);
		BFA_MechWeaponFX fx;
		if (module)
			fx = module.GetFX();

		bool drawTracer = true;
		if (fx && fx.m_iTracerEvery > 1)
		{
			if (!m_mCgsTracerCounters)
				m_mCgsTracerCounters = new map<int, int>();

			int count = m_mCgsTracerCounters.Get(hardpointIndex);
			m_mCgsTracerCounters.Set(hardpointIndex, count + 1);
			drawTracer = (count % fx.m_iTracerEvery) == 0;
		}

		if (drawTracer)
		{
			super.PlayShotLocal(hardpointIndex, shotId, start, end, flags, speed, length, thickness);
			CgsScaleNewProjectile(shotId, fx);
			return;
		}

		// Not a tracer round: everything except the flying projectile and its trail.
		ResourceName projectile = fx.m_sProjectilePrefab;
		ResourceName trail = fx.m_sTrailParticle;
		fx.m_sProjectilePrefab = ResourceName.Empty;
		fx.m_sTrailParticle = ResourceName.Empty;

		super.PlayShotLocal(hardpointIndex, shotId, start, end, flags, speed, length, thickness);

		fx.m_sProjectilePrefab = projectile;
		fx.m_sTrailParticle = trail;
	}

	//------------------------------------------------------------------------------------------------
	//! The projectile is moved with SetOrigin only, so scaling it once at spawn sticks.
	protected void CgsScaleNewProjectile(int shotId, BFA_MechWeaponFX fx)
	{
		if (!fx || fx.m_fProjectileScale <= 0 || Math.AbsFloat(fx.m_fProjectileScale - 1.0) < 0.001)
			return;

		if (m_aProjectiles.IsEmpty())
			return;

		BFA_MechProjectileVisual visual = m_aProjectiles[m_aProjectiles.Count() - 1];
		if (!visual || !visual.m_pEntity || visual.m_iShotId != shotId)
			return;

		vector mat[4];
		visual.m_pEntity.GetWorldTransform(mat);
		for (int s = 0; s < 3; s++)
			mat[s] = mat[s] * fx.m_fProjectileScale;
		visual.m_pEntity.SetWorldTransform(mat);
	}
}
