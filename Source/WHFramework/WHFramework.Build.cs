// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WHFramework : ModuleRules
{
	public WHFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HTTP",
				"Json",
				"AIModule",
				"CommonUI",
				"CommonInput",
				"EnhancedInput",
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"WHFrameworkCore",
				"WHFrameworkSlate"
			});
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RHI",
				"UMG",
				"Slate",
				"Paper2D",
				"Sockets",
				"Media",
				"MediaUtils",
				"MediaAssets",
				"SlateCore",
				"InputCore",
				"AppFramework",
				"ApplicationCore",
				"ImageWrapper",
				"ImageWriteQueue",
				"ProceduralMeshComponent",
				"DeveloperSettings",
				"LevelSequence",
				"MovieScene",
				"CinematicCamera"
			});
	}
}
