using UnrealBuildTool;

public class CvarScopes : ModuleRules
{
	public CvarScopes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"EditorFramework",
			"UnrealEd",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"Json",
			"JsonUtilities"
		});
	}
}
