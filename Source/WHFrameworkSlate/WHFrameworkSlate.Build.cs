// Copyright Epic Games, Inc. All Rights Reserved.

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
				"ImageWrapper",
				// "Engine",
				"GameplayTags"
			}
		);
		
		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		const string ResourcesDir = "$(PluginDir)/Resources/Slate/...";
		RuntimeDependencies.Add(ResourcesDir, StagedFileType.NonUFS);
	}
}
