using UnrealBuildTool;

public class MEATSPACETarget : TargetRules
{
	public MEATSPACETarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("MEATSPACE");
	}
}
