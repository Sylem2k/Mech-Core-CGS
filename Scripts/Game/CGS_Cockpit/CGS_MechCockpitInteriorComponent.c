//------------------------------------------------------------------------------------------------
//! Optional marker for the root of a modeled cockpit interior.
//! The cockpit must occupy its own "Cockpit" slot. The local first-person
//! visibility system hides ONLY the exterior model in the "Torso" slot,
//! leaving the cockpit, legs, arm meshes and all other entities untouched.
//! Keep cockpit geometry separate from the exterior torso XOB.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS MechCore/Cockpit", description: "Marks a cockpit interior sub-tree to remain visible to the local pilot.")]
class CGS_MechCockpitInteriorComponentClass : ScriptComponentClass
{
}

class CGS_MechCockpitInteriorComponent : ScriptComponent
{
}
