#include "SaveGame/SaveGameStorage.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveGameModuleTypes.h"

FSaveGameStorage::FSaveGameStorage(int32 InUserIndex)
	: UserIndex(InUserIndex)
{
}

FString FSaveGameStorage::GetRootDir() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
}

FString FSaveGameStorage::GetProfilesDir() const
{
	return FPaths::Combine(GetRootDir(), TEXT("Profiles"));
}

FString FSaveGameStorage::GetProfileDir(FName ProfileName) const
{
	return FPaths::Combine(GetProfilesDir(), ProfileName.ToString());
}

FString FSaveGameStorage::GetProfileModuleFilePath(FName ProfileName, FName ModuleName) const
{
	return FPaths::Combine(GetProfileDir(ProfileName), ModuleName.ToString() + TEXT(".bin"));
}

FString FSaveGameStorage::GetWorldsDir() const
{
	return FPaths::Combine(GetRootDir(), TEXT("Worlds"));
}

FString FSaveGameStorage::GetWorldDir(const FGuid& SaveId) const
{
	return FPaths::Combine(GetWorldsDir(), SaveId.ToString(EGuidFormats::Digits));
}

FString FSaveGameStorage::GetManifestPath(const FGuid& SaveId) const
{
	return FPaths::Combine(GetWorldDir(SaveId), TEXT("manifest.json"));
}

FString FSaveGameStorage::GetGenerationsDir(const FGuid& SaveId) const
{
	return FPaths::Combine(GetWorldDir(SaveId), TEXT("generations"));
}

FString FSaveGameStorage::GetGenerationDir(const FGuid& SaveId, int32 Generation) const
{
	return FPaths::Combine(GetGenerationsDir(SaveId), FString::Printf(TEXT("%08d"), Generation));
}

FString FSaveGameStorage::GetTempGenerationDir(const FGuid& SaveId, int32 Generation) const
{
	return GetGenerationDir(SaveId, Generation) + TEXT(".tmp");
}

FString FSaveGameStorage::GetModuleFilePath(const FGuid& SaveId, int32 Generation, FName ModuleName) const
{
	return FPaths::Combine(GetGenerationDir(SaveId, Generation), TEXT("modules"), ModuleName.ToString() + TEXT(".bin"));
}

FString FSaveGameStorage::GetTempModuleFilePath(const FGuid& SaveId, int32 Generation, FName ModuleName) const
{
	return FPaths::Combine(GetTempGenerationDir(SaveId, Generation), TEXT("modules"), ModuleName.ToString() + TEXT(".bin"));
}

FString FSaveGameStorage::GetLastActiveSavePath() const
{
	return FPaths::Combine(GetRootDir(), TEXT("last_active.txt"));
}

bool FSaveGameStorage::EnsureRoot()
{
	return IFileManager::Get().MakeDirectory(*GetProfilesDir(), true) && IFileManager::Get().MakeDirectory(*GetWorldsDir(), true);
}

bool FSaveGameStorage::CreateWorldDirectory(const FGuid& SaveId)
{
	return SaveId.IsValid() && !IFileManager::Get().DirectoryExists(*GetWorldDir(SaveId)) && IFileManager::Get().MakeDirectory(*GetGenerationsDir(SaveId), true);
}

bool FSaveGameStorage::DeleteWorldDirectory(const FGuid& SaveId)
{
	return IFileManager::Get().DeleteDirectory(*GetWorldDir(SaveId), false, true);
}

bool FSaveGameStorage::ReadManifest(const FGuid& SaveId, FSaveManifest& OutManifest) const
{
	FString Json;
	return FFileHelper::LoadFileToString(Json, *GetManifestPath(SaveId)) && FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutManifest, 0, 0) && OutManifest.SaveId == SaveId;
}

bool FSaveGameStorage::WriteManifestAtomic(const FGuid& SaveId, const FSaveManifest& Manifest)
{
	FString Json;
	return FJsonObjectConverter::UStructToJsonObjectString(Manifest, Json) && WriteTextAtomic(GetManifestPath(SaveId), Json);
}

bool FSaveGameStorage::EnumerateManifests(TArray<FSaveManifest>& OutManifests) const
{
	OutManifests.Reset();
	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *(GetWorldsDir() / TEXT("*")), false, true);
	for(const FString& Directory : Directories)
	{
		FGuid SaveId;
		FSaveManifest Manifest;
		if(FGuid::ParseExact(Directory, EGuidFormats::Digits, SaveId) && ReadManifest(SaveId, Manifest))
		{
			OutManifests.Add(MoveTemp(Manifest));
		}
	}
	return true;
}

bool FSaveGameStorage::ReadLastActiveSave(FGuid& OutSaveId) const
{
	OutSaveId.Invalidate();
	FString Text;
	if(!FFileHelper::LoadFileToString(Text, *GetLastActiveSavePath()))
	{
		return false;
	}
	Text.TrimStartAndEndInline();
	FGuid SaveId;
	FSaveManifest Manifest;
	if(!FGuid::Parse(Text, SaveId) || !ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	   !IsGenerationComplete(SaveId, Manifest.CurrentGeneration, Manifest))
	{
		return false;
	}
	OutSaveId = SaveId;
	return true;
}

bool FSaveGameStorage::WriteLastActiveSave(const FGuid& SaveId)
{
	FSaveManifest Manifest;
	if(!SaveId.IsValid() || !ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	   !IsGenerationComplete(SaveId, Manifest.CurrentGeneration, Manifest))
	{
		return false;
	}
	return WriteTextAtomic(GetLastActiveSavePath(), SaveId.ToString(EGuidFormats::DigitsWithHyphens));
}

void FSaveGameStorage::ClearLastActiveSave()
{
	IFileManager::Get().Delete(*GetLastActiveSavePath(), false, true);
}

bool FSaveGameStorage::WriteBinary(const FString& Path, const TArray<uint8>& Bytes)
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveArrayToFile(Bytes, *Path);
}

bool FSaveGameStorage::WriteBinaryAtomic(const FString& Path, const TArray<uint8>& Bytes)
{
	const FString TempPath = Path + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	IFileManager::Get().Delete(*TempPath, false, true);
	if(!FFileHelper::SaveArrayToFile(Bytes, *TempPath))
	{
		return false;
	}
	if(!IFileManager::Get().Move(*Path, *TempPath, true, true, false, true))
	{
		IFileManager::Get().Delete(*TempPath, false, true);
		return false;
	}
	return true;
}

bool FSaveGameStorage::ReadBinary(const FString& Path, TArray<uint8>& OutBytes) const
{
	return FFileHelper::LoadFileToArray(OutBytes, *Path);
}

bool FSaveGameStorage::WriteTextAtomic(const FString& Path, const FString& Text)
{
	FTCHARToUTF8 Utf8(*Text);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	return WriteBinaryAtomic(Path, Bytes);
}

bool FSaveGameStorage::PrepareTempGeneration(const FGuid& SaveId, int32 PreviousGeneration, int32 NewGeneration)
{
	const FString TempDirectory = GetTempGenerationDir(SaveId, NewGeneration);
	IFileManager::Get().DeleteDirectory(*TempDirectory, false, true);
	if(!IFileManager::Get().MakeDirectory(*FPaths::Combine(TempDirectory, TEXT("modules")), true))
	{
		return false;
	}
	if(PreviousGeneration > 0)
	{
		const FString SourceRegions = FPaths::Combine(GetGenerationDir(SaveId, PreviousGeneration), TEXT("voxel"), TEXT("regions"));
		const FString TargetRegions = FPaths::Combine(TempDirectory, TEXT("voxel"), TEXT("regions"));
		if(IFileManager::Get().DirectoryExists(*SourceRegions) && !FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*TargetRegions, *SourceRegions, true))
		{
			return false;
		}
	}
	return true;
}

bool FSaveGameStorage::CommitGeneration(const FGuid& SaveId, int32 Generation)
{
	return IFileManager::Get().Move(*GetGenerationDir(SaveId, Generation), *GetTempGenerationDir(SaveId, Generation), false, true, false, true);
}

void FSaveGameStorage::CleanupUncommittedGenerations(const FGuid& SaveId, int32 CurrentGeneration)
{
	CleanupTempGenerations(SaveId);
	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *(GetGenerationsDir(SaveId) / TEXT("*")), false, true);
	for(const FString& Directory : Directories)
	{
		if(FCString::Atoi(*Directory) > CurrentGeneration)
		{
			IFileManager::Get().DeleteDirectory(*FPaths::Combine(GetGenerationsDir(SaveId), Directory), false, true);
		}
	}
}

void FSaveGameStorage::CleanupAllTempGenerations()
{
	TArray<FString> Worlds;
	IFileManager::Get().FindFiles(Worlds, *(GetWorldsDir() / TEXT("*")), false, true);
	for(const FString& World : Worlds)
	{
		FGuid SaveId;
		if(FGuid::ParseExact(World, EGuidFormats::Digits, SaveId))
		{
			FSaveManifest Manifest;
			if(ReadManifest(SaveId, Manifest))
			{
				CleanupUncommittedGenerations(SaveId, Manifest.CurrentGeneration);
				const int32 Recovered = RecoverLatestCompleteGeneration(SaveId, Manifest);
				if(Recovered != Manifest.CurrentGeneration)
				{
					Manifest.CurrentGeneration = Recovered;
					WriteManifestAtomic(SaveId, Manifest);
				}
			}
		}
	}
}

bool FSaveGameStorage::CopyCurrentGeneration(const FGuid& Source, const FGuid& Target, int32 SourceGeneration, int32 TargetGeneration)
{
	return FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*GetGenerationDir(Target, TargetGeneration), *GetGenerationDir(Source, SourceGeneration), true);
}

void FSaveGameStorage::CleanupTempGenerations(const FGuid& SaveId)
{
	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *(GetGenerationsDir(SaveId) / TEXT("*.tmp")), false, true);
	for(const FString& Directory : Directories)
	{
		IFileManager::Get().DeleteDirectory(*FPaths::Combine(GetGenerationsDir(SaveId), Directory), false, true);
	}
}

bool FSaveGameStorage::IsGenerationComplete(const FGuid& SaveId, int32 Generation, const FSaveManifest& Manifest) const
{
	if(Generation <= 0 || !IFileManager::Get().DirectoryExists(*GetGenerationDir(SaveId, Generation)))
	{
		return false;
	}
	for(const TPair<FName, int32>& Module : Manifest.ModuleVersions)
	{
		if(!IFileManager::Get().FileExists(*GetModuleFilePath(SaveId, Generation, Module.Key)))
		{
			return false;
		}
	}
	return true;
}

int32 FSaveGameStorage::RecoverLatestCompleteGeneration(const FGuid& SaveId, const FSaveManifest& Manifest) const
{
	for(int32 Generation = Manifest.CurrentGeneration; Generation > 0; --Generation)
	{
		if(IsGenerationComplete(SaveId, Generation, Manifest))
		{
			return Generation;
		}
	}
	return 0;
}
