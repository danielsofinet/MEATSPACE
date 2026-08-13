using UnrealBuildTool;

public class MEATSPACEEditorTarget : TargetRules
{
	public MEATSPACEEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("MEATSPACE");
	}
}
