// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WHFramework : ModuleRules
{
	public WHFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;

		ShadowVariableWarningLevel = WarningLevel.Error;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"CommonUI",
				"CommonInput",
				"EnhancedInput"
			});
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UMG",
				"Slate",
				"HTTP",
				"Json",
				"Sockets",
				"Paper2D",
				"SlateCore",
				"InputCore",
				"ApplicationCore",
				"MediaAssets",
				"AIModule",
				"AppFramework",
				"RHI",
				"ImageWrapper",
				"ImageWriteQueue",
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"ProceduralMeshComponent"
			});
	}
}
