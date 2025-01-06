// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WHFrameworkEditor : ModuleRules
{
	public WHFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"WHFramework",
				"WHFrameworkCore",
				"WHFrameworkSlate"
			});
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"Settings",
				"KismetCompiler",
				"ToolMenus",
				"ToolWidgets",
				"CoreUObject",
				"AssetRegistry",
				"AssetDefinition",
				"AssetTools",
				"Engine",
				"Kismet",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"ApplicationCore",
				"PropertyEditor",
				"ClassViewer",
				"UnrealEd",
				"BlueprintGraph",
				"LevelEditor",
				"GraphEditor"
			});
	}
}
