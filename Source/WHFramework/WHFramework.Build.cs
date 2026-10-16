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
				"DeveloperSettings",
				"SlateCore",
				"UMG",
				"Engine",
				"HTTP",
				"Json",
				"JsonUtilities",
				"AIModule",
				"NavigationSystem",
				"CommonUI",
				"CommonInput",
				"EnhancedInput",
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"ProceduralMeshComponent",
				"PhysicsCore",
				"StructUtils",
				"WHFrameworkCore",
				"WHFrameworkSlate"
			});


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RHI",
				"RenderCore",
				"Chaos",
				"Slate",
				"Projects",
				"Paper2D",
				"Sockets",
				"Media",
				"MediaUtils",
				"MediaAssets",
				"InputCore",
				"AppFramework",
				"ApplicationCore",
				"ImageWrapper",
				"ImageWriteQueue",
				"LevelSequence",
				"MovieScene",
				"CinematicCamera"
			});
		if (Target.bCompileRecast)
		{
			PrivateDependencyModuleNames.Add("Navmesh");
		}
		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib", "OpenSSL");
	}
}
