#include "SaveGame/SaveGameStorage.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGame/SaveGameGenerationLease.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
bool MoveCompletedSavePath(const FString& InSource, const FString& InTarget, const bool bInReplace)
{
	auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
#if PLATFORM_WINDOWS
	const auto NativePath = [&Platform](const FString& InPath)
	{
		FString Path = Platform.ConvertToAbsolutePathForExternalAppForWrite(*InPath);
		FPaths::MakePlatformFilename(Path);
		if (Path.StartsWith(TEXT("\\\\?\\"))) return Path;
		return Path.StartsWith(TEXT("\\\\")) ? TEXT("\\\\?\\UNC\\") + Path.Mid(2) : TEXT("\\\\?\\") + Path;
	};
	const FString Source = NativePath(InSource);
	const FString Target = NativePath(InTarget);
	// 同卷替换不先删除旧 manifest；文件 Flush 成功后才发布路径，失败保留旧代。
	return ::MoveFileExW(*Source, *Target, MOVEFILE_WRITE_THROUGH | (bInReplace ? MOVEFILE_REPLACE_EXISTING : 0)) != 0;
#else
	if (!bInReplace && (Platform.FileExists(*InTarget) || Platform.DirectoryExists(*InTarget))) return false;
	// 使用平台原生 rename，不能经过先 DeleteFile 再 MoveFile 的通用替换路径。
	return Platform.MoveFile(*InTarget, *InSource);
#endif
}
}

FSaveGameStorage::FSaveGameStorage(int32 InUserIndex, const FString& InRootDirectory, const FGuid& InTransactionId)
	: UserIndex(InUserIndex), RootDirectory(InRootDirectory.IsEmpty() ?
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")) : InRootDirectory),
		WriteTransactionId(InTransactionId.IsValid() ? InTransactionId : FGuid::NewGuid())
{
}

ESaveWorldWriteResult FSaveGameStorage::BeginWorldWrite(const FGuid& SaveId,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources, const FVoxelGenerationConsumerKey& InConsumer)
{
	if (!SaveId.IsValid() || ActiveWorldWrite) return ESaveWorldWriteResult::InvalidArgument;
	return FSaveGameGenerationWriteLease::Acquire(GetWorldDir(SaveId), WriteTransactionId,
		InResources, InConsumer, ActiveWorldWrite);
}

void FSaveGameStorage::EndWorldWrite()
{
	ActiveWorldWrite.Reset();
}

ESaveWorldWriteResult FSaveGameStorage::AcquireWorldWrite(const FGuid& SaveId,
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>& OutWriter)
{
	if (!SaveId.IsValid() || OutWriter) return ESaveWorldWriteResult::InvalidArgument;
	if (ActiveWorldWrite)
	{
		if (!ActiveWorldWrite->Matches(GetWorldDir(SaveId))) return ESaveWorldWriteResult::Busy;
		OutWriter = ActiveWorldWrite;
		return ESaveWorldWriteResult::Acquired;
	}
	return FSaveGameGenerationWriteLease::Acquire(GetWorldDir(SaveId), WriteTransactionId, nullptr, {}, OutWriter);
}

const FString& FSaveGameStorage::GetRootDir() const
{
	return RootDirectory;
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
	return GetGenerationDir(SaveId, Generation) + TEXT(".") + WriteTransactionId.ToString(EGuidFormats::Digits) + TEXT(".tmp");
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
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	return AcquireWorldWrite(SaveId, Writer) == ESaveWorldWriteResult::Acquired &&
		!IFileManager::Get().DirectoryExists(*GetWorldDir(SaveId)) && IFileManager::Get().MakeDirectory(*GetGenerationsDir(SaveId), true);
}

bool FSaveGameStorage::DeleteWorldDirectory(const FGuid& SaveId)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	return AcquireWorldWrite(SaveId, Writer) == ESaveWorldWriteResult::Acquired &&
		FSaveGameGenerationReadLease::TryRetireDirectory(GetWorldDir(SaveId), Writer.Get());
}

bool FSaveGameStorage::ReadManifest(const FGuid& SaveId, FSaveManifest& OutManifest) const
{
	FString Json;
	return FFileHelper::LoadFileToString(Json, *GetManifestPath(SaveId)) && FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutManifest, 0, 0) && OutManifest.SaveId == SaveId;
}

bool FSaveGameStorage::WriteManifestAtomic(const FGuid& SaveId, const FSaveManifest& Manifest)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (Manifest.SaveId != SaveId || AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired) return false;
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

bool FSaveGameStorage::WriteBinaryDurable(const FString& Path, TConstArrayView<uint8> Bytes)
{
	auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
	if (!Platform.CreateDirectoryTree(*FPaths::GetPath(Path))) return false;
	TUniquePtr<IFileHandle> File(Platform.OpenWrite(*Path));
	return File && (Bytes.IsEmpty() || File->Write(Bytes.GetData(), Bytes.Num())) && File->Flush(true);
}

bool FSaveGameStorage::WriteBinaryAtomic(const FString& Path, const TArray<uint8>& Bytes)
{
	const FString TempPath = Path + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if(!WriteBinaryDurable(TempPath, Bytes))
	{
		IFileManager::Get().Delete(*TempPath, false, true);
		return false;
	}
	if(!MoveCompletedSavePath(TempPath, Path, true))
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

bool FSaveGameStorage::PrepareTempGeneration(const FGuid& SaveId, int32 PreviousGeneration, int32 NewGeneration,
	const FString* InCapturedVoxelGeneration,
	const TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>* InCapturedSourceLease)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (PreviousGeneration < 0 || NewGeneration <= 0 ||
		AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired) return false;
	const FString TempDirectory = GetTempGenerationDir(SaveId, NewGeneration);
	if (IFileManager::Get().DirectoryExists(*TempDirectory) && !FSaveGameGenerationReadLease::TryRetireDirectory(TempDirectory, Writer.Get()))
	{
		return false;
	}
	if(!IFileManager::Get().MakeDirectory(*FPaths::Combine(TempDirectory, TEXT("modules")), true))
	{
		return false;
	}
	// Voxel 的不可变基线属于捕获的运行世界，另存目标槽不能决定该基线。
	const FString SourceGeneration = InCapturedVoxelGeneration ? *InCapturedVoxelGeneration :
		PreviousGeneration > 0 ? GetGenerationDir(SaveId, PreviousGeneration) : FString();
	if (!SourceGeneration.IsEmpty())
	{
		const auto SourceLease = InCapturedSourceLease ? *InCapturedSourceLease : FSaveGameGenerationReadLease::Acquire(SourceGeneration);
		if (!SourceLease || !SourceLease->Matches(SourceGeneration))
		{
			return false;
		}
		const FString SourceRegions = FPaths::Combine(SourceGeneration, TEXT("voxel"), TEXT("regions"));
		const FString TargetRegions = FPaths::Combine(TempDirectory, TEXT("voxel"), TEXT("regions"));
		if(IFileManager::Get().DirectoryExists(*SourceRegions) && !FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*TargetRegions, *SourceRegions, true))
		{
			return false;
		}
		const FString SourceActors = FPaths::Combine(SourceGeneration, TEXT("voxel"), TEXT("actors"));
		const FString TargetActors = FPaths::Combine(TempDirectory, TEXT("voxel"), TEXT("actors"));
		if (IFileManager::Get().DirectoryExists(*SourceActors) &&
			!FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*TargetActors, *SourceActors, true))
		{
			return false;
		}
	}
	return true;
}

bool FSaveGameStorage::FlushTempGeneration(const FGuid& SaveId, const int32 Generation)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (Generation <= 0 || AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired) return false;
	// Worker 按文件逐一 flush，包含从旧代复制的文件；不先收集全部路径数组。
	auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
	return Platform.IterateDirectoryRecursively(*GetTempGenerationDir(SaveId, Generation),
		[&Platform](const TCHAR* Path, const bool bDirectory)
		{
			if (bDirectory) return true;
			TUniquePtr<IFileHandle> File(Platform.OpenWrite(Path, true));
			return File && File->Flush(true);
		});
}

bool FSaveGameStorage::CommitGeneration(const FGuid& SaveId, int32 Generation)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	return Generation > 0 && AcquireWorldWrite(SaveId, Writer) == ESaveWorldWriteResult::Acquired &&
		MoveCompletedSavePath(GetTempGenerationDir(SaveId, Generation), GetGenerationDir(SaveId, Generation), false);
}

bool FSaveGameStorage::CleanupUncommittedGenerations(const FGuid& SaveId, int32 CurrentGeneration)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (CurrentGeneration < 0 || AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired) return false;
	TGuardValue<TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>> Writing(ActiveWorldWrite, Writer);
	bool bCleaned = CleanupTempGenerations(SaveId);
	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *(GetGenerationsDir(SaveId) / TEXT("*")), false, true);
	for(const FString& Directory : Directories)
	{
		if(FCString::Atoi(*Directory) > CurrentGeneration)
		{
			bCleaned &= FSaveGameGenerationReadLease::TryRetireDirectory(FPaths::Combine(GetGenerationsDir(SaveId), Directory), Writer.Get());
		}
	}
	return bCleaned;
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
			TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
			if (AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired) continue;
			TGuardValue<TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>> Writing(ActiveWorldWrite, Writer);
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
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (!Source.IsValid() || Source == Target || SourceGeneration <= 0 || TargetGeneration <= 0 ||
		AcquireWorldWrite(Target, Writer) != ESaveWorldWriteResult::Acquired) return false;
	TGuardValue<TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>> Writing(ActiveWorldWrite, Writer);
	if (IFileManager::Get().DirectoryExists(*GetGenerationDir(Target, TargetGeneration))) return false;
	const auto SourceLease = FSaveGameGenerationReadLease::Acquire(GetGenerationDir(Source, SourceGeneration));
	if (!SourceLease)
	{
		return false;
	}
	const FString TempDirectory = GetTempGenerationDir(Target, TargetGeneration);
	if (IFileManager::Get().DirectoryExists(*TempDirectory) &&
		!FSaveGameGenerationReadLease::TryRetireDirectory(TempDirectory, Writer.Get())) return false;
	const bool bCommitted = FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(
		*TempDirectory, *GetGenerationDir(Source, SourceGeneration), false) &&
		FlushTempGeneration(Target, TargetGeneration) && CommitGeneration(Target, TargetGeneration);
	if (!bCommitted) FSaveGameGenerationReadLease::TryRetireDirectory(TempDirectory, Writer.Get());
	return bCommitted;
}

bool FSaveGameStorage::CleanupTempGenerations(const FGuid& SaveId)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired) return false;
	bool bCleaned = true;
	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *(GetGenerationsDir(SaveId) / TEXT("*.tmp")), false, true);
	for(const FString& Directory : Directories)
	{
		bCleaned &= FSaveGameGenerationReadLease::TryRetireDirectory(FPaths::Combine(GetGenerationsDir(SaveId), Directory), Writer.Get());
	}
	return bCleaned;
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
