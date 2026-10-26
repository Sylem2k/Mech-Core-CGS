//------------------------------------------------------------------------------------------------
//! Tag the root entity of a future interior cockpit prefab with this component.
//! The local-pilot exterior mask skips this entity and all children, so cockpit
//! dials, glass, frame, seats and panels remain visible while external armor is hidden.
//! Attach the cockpit as a separate child prefab/slot, not as part of the exterior XOB.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS MechCore/Cockpit", description: "Marks a cockpit interior sub-tree to remain visible to the local pilot.")]
class CGS_MechCockpitInteriorComponentClass : ScriptComponentClass
{
}

class CGS_MechCockpitInteriorComponent : ScriptComponent
{
}
