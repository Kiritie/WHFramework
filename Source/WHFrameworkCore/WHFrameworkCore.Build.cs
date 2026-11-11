// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class WHFrameworkCore : ModuleRules
{
	public WHFrameworkCore(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"HTTP",
				"Json",
				"StructUtils",
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
				"Engine",
				"GameplayTags",
				"EngineSettings"
			}
		);

		string externalLib = Path.Combine(ModuleDirectory, "ThirdParty", "Library");
		string externalSource = Path.Combine(ModuleDirectory, "ThirdParty", "Include");
		
		PublicIncludePaths.AddRange(
			new string[] {
				externalSource,
			}
		);

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalLibraries.Add(Path.Combine(externalLib, "Zip", "IOS", "IOS", "libzip.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add(Path.Combine(externalLib, "Zip", "Android", "armeabi-v7a", "libzip.a"));
			// for arm64
			PublicAdditionalLibraries.Add(Path.Combine(externalLib, "Zip", "Android", "arm64-v8a", "libzip.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(externalLib, "Zip", "Win64", "dll", "zip.lib"));
			
            AddRuntimeDependencies(new string[] {
                "zip.dll"
            }, Path.Combine(externalLib, "Zip", "Win64", "dll"), false);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(externalLib, "Zip", "Mac", "libzip.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(Path.Combine(externalLib, "Zip", "Linux", "libzip.a"));
		}
		
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}
	}
	
	void AddRuntimeDependencies(string[] DllNames, string LibraryPath, bool DelayLoad)
	{
		foreach (string DllName in DllNames)
		{
			if (DelayLoad) PublicDelayLoadDLLs.Add(DllName);
			string Source = Path.Combine(LibraryPath, DllName);
			RuntimeDependencies.Add(Path.Combine("$(PluginDir)", "Binaries", "Win64", DllName), Source);
			RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", DllName), Source);
		}
	}
}
