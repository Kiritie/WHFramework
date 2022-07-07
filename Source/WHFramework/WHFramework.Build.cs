// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WHFramework : ModuleRules
{
	public WHFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;

		ShadowVariableWarningLevel = WarningLevel.Error;
		
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			});
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UMG",
				"Slate",
				"HTTP",
				"Json",
				"Sockets",
				"SlateCore",
				"InputCore",
				"ApplicationCore",
				"MediaAssets",
				"AIModule",
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"ProceduralMeshComponent"
			});
	}
}
