// Copyright Epic Games, Inc. All Rights Reserved.

using System;
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
				"GameplayTags",
				"EngineSettings"
			}
		);
		
		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

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
		string binariesDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "Binaries", "Win64"));
		string engineBinaries = Path.GetFullPath(Path.Combine(EngineDirectory, "Binaries", "Win64"));
		foreach (string iter in DllNames)
		{
			if (DelayLoad) PublicDelayLoadDLLs.Add(iter);
			string dllPath = Path.Combine(LibraryPath, iter);
			string destDllPath = Path.Combine(binariesDir, iter);
			string engineDllPath = Path.Combine(engineBinaries, iter);
			if (!Directory.Exists(binariesDir))
			{
				Directory.CreateDirectory(binariesDir);
			}
			try
			{
				System.IO.File.Delete(destDllPath);
			}
			catch (Exception)
			{
				// ignored
			}
			if (!System.IO.File.Exists(destDllPath) && System.IO.File.Exists(dllPath))
			{
				System.IO.File.Copy(dllPath, destDllPath, false);
			}
			if (!System.IO.File.Exists(engineDllPath) && System.IO.File.Exists(dllPath))
			{
				System.IO.File.Copy(dllPath, engineDllPath, false);
			}
			RuntimeDependencies.Add(destDllPath);
			RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", iter), destDllPath);
		}
	}
}
