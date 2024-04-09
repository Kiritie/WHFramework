// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class WHFrameworkSlate : ModuleRules
{
	public WHFrameworkSlate(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"HTTP",
				"Json",
				"WHFrameworkCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AppFramework",
				"ApplicationCore",
				"Projects",
				"Slate",
				"SlateCore",
				"InputCore",
				"zlib",
				"DesktopPlatform",
				"ImageWrapper",
				"Engine",
				"GameplayTags"
			}
		);
	}
}
