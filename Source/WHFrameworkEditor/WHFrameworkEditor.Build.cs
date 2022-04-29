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

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			});
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			});
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			});
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"UnrealEd",
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
				// ... add private dependencies that you statically link with here ...	
			});
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			});
	}
}
