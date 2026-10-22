//------------------------------------------------------------------------------------------------
//! Everything a weapon module LOOKS and SOUNDS like. Set it on the weapon part prefab
//! (BFA_MechWeaponModuleComponent > Effects). Pure presentation: played locally on every
//! machine from one small replicated shot event, never affects damage.
[BaseContainerProps()]
class BFA_MechWeaponFX
{
	// ---- Muzzle --------------------------------------------------------------------------------
	[Attribute("", UIWidgets.ResourceNamePicker, "Muzzle flash particle (.ptc) at the muzzle every shot.", params: "ptc", category: "Muzzle")]
	ResourceName m_sMuzzleParticle;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional backblast particle (.ptc), e.g. rocket pods. Spawned at the muzzle offset by m_vBackblastOffset, facing backwards.", params: "ptc", category: "Muzzle")]
	ResourceName m_sBackblastParticle;

	[Attribute(desc: "BACKBLAST POINT (recommended): place it at the back of the tube on the weapon part with the gizmo; the blast goes out along its blue (Z) axis, so point it backwards. When set, Backblast Offset is ignored.", category: "Muzzle")]
	ref PointInfo m_BackblastPoint;

	[Attribute("0 0 -2", UIWidgets.Auto, "Only without a Backblast Point: backblast position relative to the muzzle in weapon space (X side, Y up, Z forward).", category: "Muzzle")]
	vector m_vBackblastOffset;

	[Attribute("", UIWidgets.ResourceNamePicker, "Sound file (.acp) for this weapon - pick any vanilla / WCS / your own weapon sound. Nothing else to add to the prefab.", params: "acp", category: "Sound")]
	ResourceName m_sSoundProject;

	[Attribute("SOUND_SHOT", UIWidgets.EditBox, "Fire sound: the event name inside the .acp (open the .acp in Workbench to see its events).", category: "Sound")]
	string m_sFireSoundEvent;

	[Attribute("", UIWidgets.EditBox, "Reload start sound event (weapon part).", category: "Sound")]
	string m_sReloadSoundEvent;

	[Attribute("", UIWidgets.EditBox, "Dry-fire / empty sound event (weapon part).", category: "Sound")]
	string m_sEmptySoundEvent;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional separate sound file (.acp) for impacts / explosions.", params: "acp", category: "Sound")]
	ResourceName m_sImpactSoundProject;

	[Attribute("", UIWidgets.EditBox, "Impact sound event inside the impact .acp.", category: "Sound")]
	string m_sImpactSoundEvent;

	// ---- Projectile ----------------------------------------------------------------------------
	[Attribute("", UIWidgets.ResourceNamePicker, "Visible projectile (.et) flown along each shot: bolt, tracer or rocket model. Visual only - no RplComponent, no physics.", params: "et", category: "Projectile")]
	ResourceName m_sProjectilePrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "Trail particle (.ptc) attached to the projectile (smoke, glow).", params: "ptc", category: "Projectile")]
	ResourceName m_sTrailParticle;

	[Attribute("0", UIWidgets.Auto, "Visual projectile speed in m/s. 0 = the weapon's own speed.", category: "Projectile")]
	float m_fProjectileSpeed;

	[Attribute("0.4", UIWidgets.Auto, "Seconds the projectile and trail stay after arriving, so smoke can fade.", category: "Projectile")]
	float m_fLinger;

	// ---- Instant visible laser beam ---------------------------------------------------------------
	// One lightweight client-only mesh stretched from muzzle to raycast endpoint.
	// No collision, replication or damage. Leave empty for weapons that don't use lasers.
	[Attribute("", UIWidgets.ResourceNamePicker, "Instant visible beam prefab (.et): use Mech Core CGS_LaserBeam.et for hitscan lasers.", params: "et", category: "Laser Beam")]
	ResourceName m_sBeamPrefab;

	[Attribute("0.15", UIWidgets.Auto, "Seconds the whole beam remains visible after firing.", category: "Laser Beam")]
	float m_fBeamDuration;

	[Attribute("0.18", UIWidgets.Auto, "Beam radius/width scale (mesh local X and Y).", category: "Laser Beam")]
	float m_fBeamWidth;

	[Attribute("false", UIWidgets.CheckBox, "Continuous laser: one beam stays visible while the trigger is held, updating on each damage tick. OFF = short pulse per shot.", category: "Laser Beam")]
	bool m_bBeamContinuous;

	[Attribute("3.0", UIWidgets.Auto, "Failsafe: turn off a held beam if no new shot update arrives for this many seconds (e.g. lost release, overheat or despawn).", category: "Laser Beam")]
	float m_fBeamIdleTimeout;

	// ---- Impact --------------------------------------------------------------------------------
	[Attribute("", UIWidgets.ResourceNamePicker, "Impact particle (.ptc).", params: "ptc", category: "Impact")]
	ResourceName m_sImpactParticle;

	[Attribute("", UIWidgets.ResourceNamePicker, "Optional impact prefab (.et) spawned locally, e.g. carrying an impact sound. Deleted after m_fImpactPrefabLifetime.", params: "et", category: "Impact")]
	ResourceName m_sImpactPrefab;

	[Attribute("5", UIWidgets.Auto, "Seconds before the impact prefab is removed.", category: "Impact")]
	float m_fImpactPrefabLifetime;

	[Attribute("true", UIWidgets.CheckBox, "Play the impact on misses too (shot reached max range or hit terrain).", category: "Impact")]
	bool m_bImpactOnMiss;

	// ---- Debug ---------------------------------------------------------------------------------
	[Attribute("false", UIWidgets.CheckBox, "Also draw the old debug line tracer. Debug shapes are only visible in Workbench / diag builds, never to players.", category: "Debug")]
	bool m_bDebugTracer;
}

//------------------------------------------------------------------------------------------------
//! One visible projectile on this machine (client / listen host). Never exists on a dedicated server.
class BFA_MechProjectileVisual
{
	int m_iShotId;
	int m_iHardpoint;
	vector m_vStart;
	vector m_vEnd;
	vector m_vDir;
	float m_fDistance;
	float m_fSpeed;
	float m_fAge;
	float m_fLingerLeft;
	bool m_bHit;
	bool m_bServerImpact;
	bool m_bArrived;
	IEntity m_pEntity;
	GenericEntity m_pBeam;
	float m_fBeamRemaining;
	ParticleEffectEntity m_pTrail;
}

//------------------------------------------------------------------------------------------------
//! One LIVE beam per active laser hardpoint, updated by periodic authoritative shot RPCs.
//! Damage is NOT handled here. Safe for two arms and many simultaneous emitters.
class BFA_MechContinuousBeamVisual
{
	int m_iHardpoint;
	GenericEntity m_pEntity;
	int m_iLastUpdateMs;
	float m_fIdleTimeout;
}

//------------------------------------------------------------------------------------------------
//! Shot event flags (replicated as one int).
enum BFA_EMechShotFlags
{
	HIT = 1,            // the shot hit something (impact effect at the end point)
	SERVER_IMPACT = 2,  // the server will send the real impact (rockets); don't play one on arrival
}

//------------------------------------------------------------------------------------------------
//! Module events replicated for effects and client-side ammo mirrors (HUD).
enum BFA_EMechModuleEvent
{
	RELOAD_START = 1,
	RELOAD_END = 2,
	EMPTY = 3,
	REARMED = 4,
	CHARGES = 5,   // value = charges now (anti-missile and other charge-based modules)
}
