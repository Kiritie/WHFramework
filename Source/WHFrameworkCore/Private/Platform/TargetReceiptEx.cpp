// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/TargetReceiptEx.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

bool TryParseBuildVersionEx(const FJsonObject& Object, FBuildVersion& OutVersion)
{
	if (Object.TryGetNumberField(TEXT("MajorVersion"), OutVersion.MajorVersion) && Object.TryGetNumberField(TEXT("MinorVersion"), OutVersion.MinorVersion) && Object.TryGetNumberField(TEXT("PatchVersion"), OutVersion.PatchVersion))
	{
		Object.TryGetNumberField(TEXT("Changelist"), OutVersion.Changelist);
		Object.TryGetNumberField(TEXT("CompatibleChangelist"), OutVersion.CompatibleChangelist);
		Object.TryGetNumberField(TEXT("IsLicenseeVersion"), OutVersion.IsLicenseeVersion);
		Object.TryGetNumberField(TEXT("IsPromotedBuild"), OutVersion.IsPromotedBuild);
		Object.TryGetStringField(TEXT("BranchName"), OutVersion.BranchName);
		Object.TryGetStringField(TEXT("BuildId"), OutVersion.BuildId);
		Object.TryGetStringField(TEXT("BuildVersion"), OutVersion.BuildVersion);
		return true;
	}
	return false;
}

bool FTargetReceiptEx::Read(const FString& FileName)
{
	// Read the file from disk
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FileName))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		return false;
	}

	// Get the project file
	FString RelativeProjectFile;
	if(Object->TryGetStringField(TEXT("Project"), RelativeProjectFile))
	{
		ProjectFile = FPaths::Combine(FPaths::GetPath(FileName), RelativeProjectFile);
		FPaths::MakeStandardFilename(ProjectFile);
		ProjectDir = FPaths::GetPath(ProjectFile);
	}

	// Get the target name
	if (!Object->TryGetStringField(TEXT("TargetName"), TargetName))
	{
		return false;
	}
	if (!Object->TryGetStringField(TEXT("Platform"), Platform))
	{
		return false;
	}
	if (!Object->TryGetStringField(TEXT("Architecture"), Architecture))
	{
		return false;
	}

	// Read the configuration
	FString ConfigurationString;
	if (!Object->TryGetStringField(TEXT("Configuration"), ConfigurationString) || !LexTryParseString(Configuration, *ConfigurationString))
	{
		return false;
	}

	// Read the target type
	FString TargetTypeString;
	if (!Object->TryGetStringField(TEXT("TargetType"), TargetTypeString) || !LexTryParseString(TargetType, *TargetTypeString))
	{
		return false;
	}

	// Read the version information
	const TSharedPtr<FJsonObject>* VersionObject;
	if (!Object->TryGetObjectField(TEXT("Version"), VersionObject) || !VersionObject->IsValid() || !TryParseBuildVersionEx(*VersionObject->Get(), Version))
	{
		return false;
	}

	// Get the launch path
	if(!Object->TryGetStringField(TEXT("Launch"), Launch))
	{
		return false;
	}
	ExpandVariables(Launch);

	// Read the list of build products
	const TArray<TSharedPtr<FJsonValue>>* BuildProductsArray;
	if (Object->TryGetArrayField(TEXT("BuildProducts"), BuildProductsArray))
	{
		for(const TSharedPtr<FJsonValue>& BuildProductValue : *BuildProductsArray)
		{
			const TSharedPtr<FJsonObject>* BuildProductObject;
			if(!BuildProductValue->TryGetObject(BuildProductObject))
			{
				return false;
			}

			FBuildProductEx BuildProduct;
			if(!(*BuildProductObject)->TryGetStringField(TEXT("Type"), BuildProduct.Type) || !(*BuildProductObject)->TryGetStringField(TEXT("Path"), BuildProduct.Path))
			{
				return false;
			}

			ExpandVariables(BuildProduct.Path);

			BuildProducts.Add(MoveTemp(BuildProduct));
		}
	}

	// Read the list of runtime dependencies
	const TArray<TSharedPtr<FJsonValue>>* RuntimeDependenciesArray;
	if (Object->TryGetArrayField(TEXT("RuntimeDependencies"), RuntimeDependenciesArray))
	{
		for(const TSharedPtr<FJsonValue>& RuntimeDependencyValue : *RuntimeDependenciesArray)
		{
			const TSharedPtr<FJsonObject>* RuntimeDependencyObject;
			if(!RuntimeDependencyValue->TryGetObject(RuntimeDependencyObject))
			{
				return false;
			}

			FRuntimeDependencyEx RuntimeDependency;
			if(!(*RuntimeDependencyObject)->TryGetStringField(TEXT("Path"), RuntimeDependency.Path) || !(*RuntimeDependencyObject)->TryGetStringField(TEXT("Type"), RuntimeDependency.Type))
			{
				return false;
			}

			ExpandVariables(RuntimeDependency.Path);

			RuntimeDependencies.Add(MoveTemp(RuntimeDependency));
		}
	}

	// Read the list of additional properties
	const TArray<TSharedPtr<FJsonValue>>* AdditionalPropertiesArray;
	if (Object->TryGetArrayField(TEXT("AdditionalProperties"), AdditionalPropertiesArray))
	{
		for(const TSharedPtr<FJsonValue>& AdditionalPropertyValue : *AdditionalPropertiesArray)
		{
			const TSharedPtr<FJsonObject>* AdditionalPropertyObject;
			if(!AdditionalPropertyValue->TryGetObject(AdditionalPropertyObject))
			{
				return false;
			}

			FReceiptPropertyEx Property;
			if(!(*AdditionalPropertyObject)->TryGetStringField(TEXT("Name"), Property.Name) || !(*AdditionalPropertyObject)->TryGetStringField(TEXT("Value"), Property.Value))
			{
				return false;
			}

			ExpandVariables(Property.Value);

			AdditionalProperties.Add(MoveTemp(Property));
		}
	}

	return true;
}

FString FTargetReceiptEx::GetDefaultPath(const TCHAR* BaseDir, const TCHAR* TargetName, const TCHAR* Platform, EBuildConfiguration Configuration, const TCHAR* BuildArchitecture)
{
	const TCHAR* ArchitectureSuffix = TEXT("");
	if (BuildArchitecture != nullptr)
	{
		ArchitectureSuffix = BuildArchitecture;
	}

	if ((BuildArchitecture == nullptr || BuildArchitecture[0] == 0) && Configuration == EBuildConfiguration::Development)
	{
		return FPaths::Combine(BaseDir, FString::Printf(TEXT("Binaries/%s/%s.target"), Platform, TargetName));
	}
	else
	{
		return FPaths::Combine(BaseDir, FString::Printf(TEXT("Binaries/%s/%s-%s-%s%s.target"), Platform, TargetName, Platform, LexToString(Configuration), ArchitectureSuffix));
	}
}

void FTargetReceiptEx::ExpandVariables(FString& Path)
{
	static FString EngineDirPrefix = TEXT("$(EngineDir)");
	static FString ProjectDirPrefix = TEXT("$(ProjectDir)");
	if(Path.StartsWith(EngineDirPrefix))
	{
		FString EngineDir = FPaths::EngineDir();
		if (EngineDir.Len() > 0 && EngineDir[EngineDir.Len() - 1] == '/')
		{
			EngineDir = EngineDir.Left(EngineDir.Len() - 1);
		}
		Path = EngineDir + Path.Mid(EngineDirPrefix.Len());
	}
	else if(Path.StartsWith(ProjectDirPrefix) && ProjectDir.Len() > 0)
	{
		Path = ProjectDir + Path.Mid(ProjectDirPrefix.Len());
	}
}
