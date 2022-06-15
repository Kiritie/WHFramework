// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WHFrameworkEditor : ModuleRules
{
	public WHFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		bLegacyPublicIncludePaths = false;

		ShadowVariableWarningLevel = WarningLevel.Error;
		
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			});
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"UnrealEd",
				"KismetCompiler",
				"ToolMenus",
				"CoreUObject",
				"AssetRegistry",
				"AssetTools",
				"Engine",
				"Kismet",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"WHFramework",
				"PropertyEditor",
				"ClassViewer",
				"UnrealEd",
				"BlueprintGraph",
			});
	}
}
