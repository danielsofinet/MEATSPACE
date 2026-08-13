using UnrealBuildTool;

public class MEATSPACE : ModuleRules
{
	public MEATSPACE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets us include by path from the module root, e.g. "Combat/MsWeaponComponent.h".
		// Without this only same-directory includes resolve.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
