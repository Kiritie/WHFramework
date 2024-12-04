// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WHFrameworkDeveloper : ModuleRules
{
	public WHFrameworkDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			});
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"KismetCompiler",
				"CoreUObject",
				"Engine",
				"Kismet",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"ApplicationCore",
				"ClassViewer",
				"UnrealEd",
				"BlueprintGraph",
				"GraphEditor"
			});
	}
}
