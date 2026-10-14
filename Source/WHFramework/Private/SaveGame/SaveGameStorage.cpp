#include "SaveGame/SaveGameStorage.h"

#include "HAL/FileManager.h"
#include "Ability/Inventory/InventoryStoreLock.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGame/SaveGameGenerationLease.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_LINUX
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#endif

namespace
{
constexpr uint32 PendingWorldMagic = 0x57464F50;
constexpr uint32 PendingWorldVersion = 1;
constexpr int32 PendingWorldBytes = 101;

FString CanonicalSavePath(const FString& InPath)
{
	FString Path = FPaths::ConvertRelativePathToFull(InPath);
	FPaths::NormalizeDirectoryName(Path);
	FPaths::CollapseRelativeDirectories(Path);
#if PLATFORM_WINDOWS
	Path.ToLowerInline();
#endif
	return Path;
}

#if PLATFORM_WINDOWS
FString NativeSavePath(const FString& InPath)
{
	auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
	FString Path = Platform.ConvertToAbsolutePathForExternalAppForWrite(*InPath);
	FPaths::MakePlatformFilename(Path);
	if (Path.StartsWith(TEXT("\\\\?\\"))) return Path;
	return Path.StartsWith(TEXT("\\\\")) ? TEXT("\\\\?\\UNC\\") + Path.Mid(2) : TEXT("\\\\?\\") + Path;
}
#endif

bool SyncSaveDirectory(const FString& InDirectory)
{
#if PLATFORM_LINUX
	FTCHARToUTF8 Path(*FPaths::ConvertRelativePathToFull(InDirectory));
	const int Descriptor = ::open(Path.Get(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (Descriptor < 0) return false;
	int Result;
	do { Result = ::fsync(Descriptor); } while (Result < 0 && errno == EINTR);
	::close(Descriptor);
	return Result == 0;
#else
	// Windows 的文件 Flush 与 WRITE_THROUGH rename 负责发布；其目录句柄不支持同样的 fsync 协议。
	return true;
#endif
}

enum class ESavePathPresence : uint8 { Missing, Present, Unknown };

ESavePathPresence ProbeSavePath(const FString& InPath)
{
#if PLATFORM_WINDOWS
	if (::GetFileAttributesW(*NativeSavePath(InPath)) != INVALID_FILE_ATTRIBUTES) return ESavePathPresence::Present;
	const DWORD Error = ::GetLastError();
	return (Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND) ? ESavePathPresence::Missing : ESavePathPresence::Unknown;
#elif PLATFORM_LINUX
	FTCHARToUTF8 Path(*FPaths::ConvertRelativePathToFull(InPath));
	struct stat Value;
	if (::lstat(Path.Get(), &Value) == 0) return ESavePathPresence::Present;
	return (errno == ENOENT || errno == ENOTDIR) ? ESavePathPresence::Missing : ESavePathPresence::Unknown;
#else
	return ESavePathPresence::Unknown;
#endif
}

bool CreateSaveDirectoryDurable(const FString& InDirectory, const int32 InDepth = 0)
{
	if (InDirectory.IsEmpty() || InDepth > 128) return false;
	auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
	const FString Directory = FPaths::ConvertRelativePathToFull(InDirectory);
	if (Platform.DirectoryExists(*Directory)) return true;
	const FString Parent = FPaths::GetPath(Directory);
	return !Parent.IsEmpty() && Parent != Directory &&
		CreateSaveDirectoryDurable(Parent, InDepth + 1) && Platform.CreateDirectory(*Directory) &&
		SyncSaveDirectory(Directory) && SyncSaveDirectory(Parent);
}

bool FlushSaveTree(const FString& InDirectory, const int32 InDepth = 0)
{
	if (InDepth > 32) return false;
	auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
	if (!Platform.DirectoryExists(*InDirectory)) return false;
	const bool bFlushed = Platform.IterateDirectory(*InDirectory,
		[&Platform, InDepth](const TCHAR* Path, const bool bDirectory)
		{
			if (bDirectory) return FlushSaveTree(Path, InDepth + 1);
			TUniquePtr<IFileHandle> File(Platform.OpenWrite(Path, true));
			return File && File->Flush(true);
		});
	return bFlushed && SyncSaveDirectory(InDirectory);
}

int64 AcquirePendingWorldLock(const FString& InPath, FString& OutError)
{
#if PLATFORM_WINDOWS
	const FString Path = NativeSavePath(InPath);
	const HANDLE Handle = ::CreateFileW(*Path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		OutError = ::GetLastError() == ERROR_SHARING_VIOLATION ? TEXT("Save.PendingWorldBusy") : TEXT("Save.PendingLockOpenFailed");
		return -1;
	}
	return static_cast<int64>(reinterpret_cast<UPTRINT>(Handle));
#elif PLATFORM_LINUX
	FTCHARToUTF8 Path(*FPaths::ConvertRelativePathToFull(InPath));
	const int Descriptor = ::open(Path.Get(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (Descriptor < 0)
	{
		OutError = TEXT("Save.PendingLockOpenFailed");
		return -1;
	}
	int Result;
	do { Result = ::flock(Descriptor, LOCK_EX | LOCK_NB); } while (Result < 0 && errno == EINTR);
	if (Result < 0)
	{
		OutError = (errno == EWOULDBLOCK || errno == EAGAIN) ? TEXT("Save.PendingWorldBusy") : TEXT("Save.PendingLockFailed");
		::close(Descriptor);
		return -1;
	}
	return Descriptor;
#else
	OutError = TEXT("Save.PendingWorldPlatformUnsupported");
	return -1;
#endif
}

void ReleasePendingWorldLock(const int64 InHandle)
{
	if (InHandle == -1) return;
#if PLATFORM_WINDOWS
	::CloseHandle(reinterpret_cast<HANDLE>(static_cast<UPTRINT>(InHandle)));
#elif PLATFORM_LINUX
	::flock(static_cast<int>(InHandle), LOCK_UN);
	::close(static_cast<int>(InHandle));
#endif
}

struct FSaveInventoryRetirementLocks
{
	TArray<int64> Handles;
	~FSaveInventoryRetirementLocks() { Release(); }
	void Release()
	{
		for (const int64 Handle : Handles) ReleasePendingWorldLock(Handle);
		Handles.Reset();
	}
	bool Acquire(const FString& InWorldDirectory)
	{
		auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
		const FString InventoryDirectory = InWorldDirectory / TEXT("Inventory");
		if (!Platform.DirectoryExists(*InventoryDirectory)) return true;
		return Platform.IterateDirectory(*InventoryDirectory, [this, &Platform](const TCHAR* Path, const bool bDirectory)
		{
			if (!bDirectory) return true;
			FGuid StoreId;
			if (Handles.Num() >= 256 || !FGuid::ParseExact(FPaths::GetCleanFilename(Path), EGuidFormats::Digits, StoreId)) return false;
			FString Error;
			const int64 Handle = FInventoryStoreLock::Acquire(Path, Error);
			if (Handle == -1) return false;
			Handles.Add(Handle);
			return true;
		});
	}
};

bool IsValidPendingWorld(const FSavePendingWorld& InIntent)
{
	return InIntent.OperationId.IsValid() && InIntent.TargetSaveId.IsValid() &&
		InIntent.StoreId.IsValid() && InIntent.WorldId.IsValid() && InIntent.SourceGeneration >= 0 &&
		InIntent.TargetGeneration > 0 &&
		(InIntent.SourceGeneration == 0 || (InIntent.SourceSaveId.IsValid() && InIntent.SourceSaveId != InIntent.TargetSaveId));
}

bool SamePendingWorld(const FSavePendingWorld& A, const FSavePendingWorld& B)
{
	return A.OperationId == B.OperationId && A.TargetSaveId == B.TargetSaveId && A.SourceSaveId == B.SourceSaveId &&
		A.StoreId == B.StoreId && A.WorldId == B.WorldId && A.SourceGeneration == B.SourceGeneration &&
		A.TargetGeneration == B.TargetGeneration && A.bDeleteWorldOnAbort == B.bDeleteWorldOnAbort;
}

TArray<uint8> EncodePendingWorld(const FSavePendingWorld& InIntent)
{
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	uint32 Magic = PendingWorldMagic;
	uint32 Version = PendingWorldVersion;
	FSavePendingWorld Value = InIntent;
	uint8 DeleteWorld = Value.bDeleteWorldOnAbort ? 1 : 0;
	Writer << Magic << Version << Value.OperationId << Value.TargetSaveId << Value.SourceSaveId << Value.StoreId << Value.WorldId;
	Writer << Value.SourceGeneration << Value.TargetGeneration << DeleteWorld;
	uint32 Crc = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
	Writer << Crc;
	return Bytes;
}

bool MoveCompletedSavePath(const FString& InSource, const FString& InTarget, const bool bInReplace)
{
	auto& Platform = FPlatformFileManager::Get().GetPlatformFile();
#if PLATFORM_WINDOWS
	const FString Source = NativeSavePath(InSource);
	const FString Target = NativeSavePath(InTarget);
	// 同卷替换不先删除旧 manifest；文件 Flush 成功后才发布路径，失败保留旧代。
	const DWORD Flags = MOVEFILE_WRITE_THROUGH | (bInReplace ? MOVEFILE_REPLACE_EXISTING : 0);
	if (::MoveFileExW(*Source, *Target, Flags)) return true;
	DWORD MoveError = ::GetLastError();
	const DWORD Attributes = ::GetFileAttributesW(*Source);
	if (!bInReplace && Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		// 原生读句柄或文件系统过滤器可能短暂阻止目录 rename；调用者持续持有写入/退役租约。
		// 只重试同一次原子操作，不移动目标、不绕过权限、不提前报告退役成功。
		for (int32 Retry = 0; Retry < 7 &&
			(MoveError == ERROR_ACCESS_DENIED || MoveError == ERROR_SHARING_VIOLATION || MoveError == ERROR_LOCK_VIOLATION); ++Retry)
		{
			FPlatformProcess::SleepNoStats(0.001f * float(1 << FMath::Min(Retry, 5)));
			if (::MoveFileExW(*Source, *Target, Flags))
			{
				UE_LOG(LogTemp, Display, TEXT("Atomic save directory rename recovered after transient native rejection: retries=%d"), Retry + 1);
				return true;
			}
			MoveError = ::GetLastError();
		}
	}
	::SetLastError(MoveError);
	return false;
#elif PLATFORM_LINUX
	if (!bInReplace && (Platform.FileExists(*InTarget) || Platform.DirectoryExists(*InTarget))) return false;
	FTCHARToUTF8 Source(*FPaths::ConvertRelativePathToFull(InSource));
	FTCHARToUTF8 Target(*FPaths::ConvertRelativePathToFull(InTarget));
	if (::rename(Source.Get(), Target.Get()) != 0) return false;
	const FString SourceParent = FPaths::GetPath(InSource);
	const FString TargetParent = FPaths::GetPath(InTarget);
	return SyncSaveDirectory(TargetParent) &&
		(SourceParent == TargetParent || SyncSaveDirectory(SourceParent));
#else
	if (!bInReplace && (Platform.FileExists(*InTarget) || Platform.DirectoryExists(*InTarget))) return false;
	// 使用平台原生 rename，不能经过先 DeleteFile 再 MoveFile 的通用替换路径。
	return Platform.MoveFile(*InTarget, *InSource);
#endif
}
}

FSaveWorldSourceLease::FSaveWorldSourceLease(const int64 InNativeLock)
	: NativeLock(InNativeLock)
{
}

FSaveWorldSourceLease::~FSaveWorldSourceLease()
{
	ReleasePendingWorldLock(NativeLock);
}

FSavePendingWorldLease::FSavePendingWorldLease(FString InRootDirectory, const FSavePendingWorld& InIntent, const int64 InNativeLock)
	: RootDirectory(MoveTemp(InRootDirectory)), Intent(InIntent), NativeLock(InNativeLock)
{
}

FSavePendingWorldLease::~FSavePendingWorldLease()
{
	ReleasePendingWorldLock(NativeLock);
}

const FSavePendingWorld& FSavePendingWorldLease::GetIntent() const
{
	return Intent;
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
	return CreateSaveDirectoryDurable(GetProfilesDir()) && CreateSaveDirectoryDurable(GetWorldsDir());
}

bool FSaveGameStorage::CreateWorldDirectory(const FGuid& SaveId)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	return AcquireWorldWrite(SaveId, Writer) == ESaveWorldWriteResult::Acquired &&
		!IFileManager::Get().DirectoryExists(*GetWorldDir(SaveId)) && CreateSaveDirectoryDurable(GetGenerationsDir(SaveId));
}

bool FSaveGameStorage::DeleteWorldDirectory(const FGuid& SaveId)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	const ESaveWorldWriteResult WriteResult = AcquireWorldWrite(SaveId, Writer);
	if (WriteResult != ESaveWorldWriteResult::Acquired)
	{
		UE_LOG(LogTemp, Display, TEXT("Save retirement denied before filesystem access: world=%s writerResult=%d"), *SaveId.ToString(), int32(WriteResult));
		return false;
	}
	const FString IntentPath = GetPendingWorldPath(SaveId);
	if (!CreateSaveDirectoryDurable(FPaths::GetPath(IntentPath))) return false;
	FString LockError;
	const int64 OperationLock = AcquirePendingWorldLock(IntentPath + TEXT(".lock"), LockError);
	if (OperationLock == -1) return false;
	FSaveInventoryRetirementLocks Locks;
	Locks.Handles.Add(OperationLock);
	if (HasPendingWorld(SaveId)) return false;
	const FString WorldDirectory = GetWorldDir(SaveId);
	if (!IFileManager::Get().DirectoryExists(*WorldDirectory)) return true;
	if (!Locks.Acquire(WorldDirectory)) return false;
	const FString RetiredRoot = GetRootDir() / TEXT("RetiredWorlds");
	if (!CreateSaveDirectoryDurable(RetiredRoot)) return false;
	const FString Retired = RetiredRoot / (SaveId.ToString(EGuidFormats::Digits) + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
	return FSaveGameGenerationReadLease::TryRetireDirectory(WorldDirectory, Writer.Get(), [&]()
	{
		bool bDurable = MoveCompletedSavePath(WorldDirectory, Retired, false);
		const uint32 MoveError = bDurable ? 0 : FPlatformMisc::GetLastError();
		const bool bRetired = !IFileManager::Get().DirectoryExists(*WorldDirectory) && IFileManager::Get().DirectoryExists(*Retired);
		if (!bRetired)
		{
			UE_LOG(LogTemp, Display, TEXT("Save retirement rename rejected: nativeError=%u source=%s target=%s"), MoveError, *WorldDirectory, *Retired);
			return false;
		}
		// rename 可见不代表目录项已持久；再确认两侧目录后才能报告删除成功。
		if (!bDurable) bDurable = SyncSaveDirectory(RetiredRoot) && SyncSaveDirectory(GetWorldsDir());
		// 原始路径已经退出可加载命名空间，才释放 store.lock；Windows 可安全关闭待删除句柄。
		Locks.Release();
		if (bDurable) IFileManager::Get().DeleteDirectory(*Retired, false, true);
		// 未确认时保留退役副本并报告失败；调用方不能把已消失的原槽重新设为活动。
		// 确认成功后，剩余文件删除失败不改变退役事实，启动时重试清理。
		return bDurable;
	});
}

bool FSaveGameStorage::ReadManifest(const FGuid& SaveId, FSaveManifest& OutManifest) const
{
	FString Json;
	TSharedPtr<FJsonObject> Object;
	if (!SaveId.IsValid() || !FFileHelper::LoadFileToString(Json, *GetManifestPath(SaveId))) return false;
	// 在 TMap 物化前统计根字段，同名重复 key 不能被 JSON 对象的覆盖语义隐藏。
	const TSharedRef<TJsonReader<>> Tokens = TJsonReaderFactory<>::Create(Json);
	EJsonNotation Notation;
	int32 Depth = 0, GenerationFields = 0;
	while (Tokens->ReadNext(Notation))
	{
		if (Notation == EJsonNotation::Error) return false;
		if (Notation == EJsonNotation::ObjectEnd || Notation == EJsonNotation::ArrayEnd)
		{
			--Depth;
			continue;
		}
		if (Depth == 1 && Tokens->GetIdentifier().Equals(TEXT("CurrentGeneration"), ESearchCase::IgnoreCase) &&
			++GenerationFields > 1) return false;
		if (Notation == EJsonNotation::ObjectStart || Notation == EJsonNotation::ArrayStart) ++Depth;
	}
	if (GenerationFields != 1 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object) return false;
	bool bHasGeneration = false;
	double Generation = -1;
	for (const auto& Pair : Object->Values)
	{
		if (!FString(*Pair.Key).Equals(TEXT("CurrentGeneration"), ESearchCase::IgnoreCase)) continue;
		if (bHasGeneration || !Pair.Value || Pair.Value->Type != EJson::Number ||
			!Object->TryGetNumberField(Pair.Key, Generation)) return false;
		bHasGeneration = true;
	}
	// 缺字段不能由 USTRUCT 默认值偷偷变成可回收的 gen0；只接受精确的非负整数。
	if (!bHasGeneration || !FMath::IsFinite(Generation) || Generation < 0 || Generation > MAX_int32 ||
		Generation != static_cast<double>(static_cast<int32>(Generation))) return false;
	FSaveManifest Manifest;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Manifest, 0, 0) ||
		Manifest.SaveId != SaveId || Manifest.CurrentGeneration != static_cast<int32>(Generation) ||
		(Manifest.CurrentGeneration == 0 && Manifest.LastCommitOperationId.IsValid())) return false;
	OutManifest = MoveTemp(Manifest);
	return true;
}

bool FSaveGameStorage::WriteManifestAtomic(const FGuid& SaveId, const FSaveManifest& Manifest,
	const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& InPendingLease)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (Manifest.SaveId != SaveId || Manifest.CurrentGeneration < 0 ||
		(Manifest.CurrentGeneration == 0 && Manifest.LastCommitOperationId.IsValid()) ||
		AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired) return false;
	FSaveInventoryRetirementLocks OperationOwner;
	FString Error;
	if (InPendingLease)
	{
		if (!CheckPendingWorldLease(InPendingLease, Error) || InPendingLease->Intent.TargetSaveId != SaveId ||
			(Manifest.CurrentGeneration > 0 && (Manifest.LastCommitOperationId != InPendingLease->Intent.OperationId ||
				Manifest.CurrentGeneration != InPendingLease->Intent.TargetGeneration))) return false;
	}
	else
	{
		// 普通元数据/原槽写入与分支发布共用操作锁，不能以陈旧 gen0 绕过 pending。
		const FString IntentPath = GetPendingWorldPath(SaveId);
		if (!CreateSaveDirectoryDurable(FPaths::GetPath(IntentPath))) return false;
		const int64 Lock = AcquirePendingWorldLock(IntentPath + TEXT(".lock"), Error);
		if (Lock == -1) return false;
		OperationOwner.Handles.Add(Lock);
		if (HasPendingWorld(SaveId)) return false;
	}
	FSaveManifest Current;
	const bool bHasCurrent = ReadManifest(SaveId, Current);
	if ((!bHasCurrent && ProbeSavePath(GetManifestPath(SaveId)) != ESavePathPresence::Missing) ||
		(bHasCurrent && (Manifest.CurrentGeneration < Current.CurrentGeneration ||
			(Manifest.CurrentGeneration > 0 && Manifest.CurrentGeneration == Current.CurrentGeneration &&
				Manifest.LastCommitOperationId != Current.LastCommitOperationId)))) return false;
	FString Json;
	return FJsonObjectConverter::UStructToJsonObjectString(Manifest, Json) && WriteTextAtomic(GetManifestPath(SaveId), Json);
}

FString FSaveGameStorage::GetPendingWorldPath(const FGuid& SaveId) const
{
	return FPaths::Combine(GetRootDir(), TEXT("WorldOperations"), SaveId.ToString(EGuidFormats::Digits) + TEXT(".intent"));
}

bool FSaveGameStorage::HasPendingWorld(const FGuid& SaveId) const
{
	return !SaveId.IsValid() || ProbeSavePath(GetPendingWorldPath(SaveId)) != ESavePathPresence::Missing;
}

bool FSaveGameStorage::ConfirmManifestCommit(const FGuid& SaveId, const FGuid& OperationId, const int32 Generation, FString& OutError)
{
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired)
	{
		OutError = TEXT("Save.ManifestCommitBusy");
		return false;
	}
	FSaveManifest Before;
	if (!OperationId.IsValid() || Generation <= 0 || !ReadManifest(SaveId, Before) ||
		Before.LastCommitOperationId != OperationId || Before.CurrentGeneration != Generation ||
		!IsGenerationComplete(SaveId, Generation, Before))
	{
		OutError = TEXT("Save.ManifestCommitReceiptMismatch");
		return false;
	}
	TUniquePtr<IFileHandle> File(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*GetManifestPath(SaveId), true));
	const bool bFlushed = File && File->Flush(true);
	File.Reset();
	if (!bFlushed || !SyncSaveDirectory(GetWorldDir(SaveId)))
	{
		OutError = TEXT("Save.ManifestCommitSyncUncertain");
		return false;
	}
	FSaveManifest After;
	if (!ReadManifest(SaveId, After) || After.LastCommitOperationId != OperationId || After.CurrentGeneration != Generation ||
		!IsGenerationComplete(SaveId, Generation, After))
	{
		OutError = TEXT("Save.ManifestCommitChangedDuringConfirmation");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FSaveGameStorage::ReadPendingWorld(const FGuid& SaveId, FSavePendingWorld& OutIntent) const
{
	TUniquePtr<IFileHandle> File(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*GetPendingWorldPath(SaveId)));
	if (!File || File->Size() != PendingWorldBytes) return false;
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(PendingWorldBytes);
	if (!File->Read(Bytes.GetData(), Bytes.Num())) return false;
	FMemoryReader Reader(Bytes, true);
	uint32 Magic = 0, Version = 0, Crc = 0;
	uint8 DeleteWorld = 0;
	FSavePendingWorld Value;
	Reader << Magic << Version << Value.OperationId << Value.TargetSaveId << Value.SourceSaveId << Value.StoreId << Value.WorldId;
	Reader << Value.SourceGeneration << Value.TargetGeneration << DeleteWorld << Crc;
	Value.bDeleteWorldOnAbort = DeleteWorld == 1;
	if (Reader.IsError() || Reader.Tell() != Bytes.Num() || Magic != PendingWorldMagic || Version != PendingWorldVersion ||
		DeleteWorld > 1 || Crc != FCrc::MemCrc32(Bytes.GetData(), Bytes.Num() - sizeof(uint32)) ||
		Value.TargetSaveId != SaveId || !IsValidPendingWorld(Value)) return false;
	OutIntent = Value;
	return true;
}

bool FSaveGameStorage::AcquireWorldSourceLease(const FGuid& SaveId,
	TSharedPtr<FSaveWorldSourceLease, ESPMode::ThreadSafe>& OutLease, FString& OutError)
{
	OutError.Reset();
	if (!SaveId.IsValid() || OutLease)
	{
		OutError = TEXT("Save.SourceLeaseInvalid");
		return false;
	}
	const FString IntentPath = GetPendingWorldPath(SaveId);
	if (!EnsureRoot() || !CreateSaveDirectoryDurable(FPaths::GetPath(IntentPath)))
	{
		OutError = TEXT("Save.SourceLeaseDirectoryFailed");
		return false;
	}
	// 与目标分支准备和整槽删除使用同一个永不 unlink 的 inode。
	const int64 NativeLock = AcquirePendingWorldLock(IntentPath + TEXT(".lock"), OutError);
	if (NativeLock == -1) return false;
	TSharedPtr<FSaveWorldSourceLease, ESPMode::ThreadSafe> Lease = MakeShareable(new FSaveWorldSourceLease(NativeLock));
	if (HasPendingWorld(SaveId))
	{
		OutError = TEXT("Save.SourceRequiresRecovery");
		return false;
	}
	if (ProbeSavePath(GetWorldDir(SaveId)) != ESavePathPresence::Present ||
		!IFileManager::Get().DirectoryExists(*GetWorldDir(SaveId)))
	{
		OutError = TEXT("Save.SourceWorldUnavailable");
		return false;
	}
	OutLease = MoveTemp(Lease);
	return true;
}

bool FSaveGameStorage::BeginPendingWorld(const FSavePendingWorld& InIntent,
	TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& OutLease, FString& OutError)
{
	OutError.Reset();
	if (OutLease || !IsValidPendingWorld(InIntent))
	{
		OutError = TEXT("Save.PendingWorldInvalid");
		return false;
	}
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (AcquireWorldWrite(InIntent.TargetSaveId, Writer) != ESaveWorldWriteResult::Acquired)
	{
		OutError = TEXT("Save.PendingWorldBusy");
		return false;
	}
	const FString IntentPath = GetPendingWorldPath(InIntent.TargetSaveId);
	if (!EnsureRoot() || !CreateSaveDirectoryDurable(FPaths::GetPath(IntentPath)))
	{
		OutError = TEXT("Save.PendingDirectoryFailed");
		return false;
	}
	// 锁文件在 WorldDir 外，绝不 unlink；删除旧槽后仍只有同一个锁 inode。
	const int64 NativeLock = AcquirePendingWorldLock(IntentPath + TEXT(".lock"), OutError);
	if (NativeLock == -1) return false;
	TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease = MakeShareable(
		new FSavePendingWorldLease(CanonicalSavePath(GetRootDir()), InIntent, NativeLock));
	if (HasPendingWorld(InIntent.TargetSaveId))
	{
		OutError = TEXT("Save.PendingWorldRequiresRecovery");
		return false;
	}
	FSaveManifest Existing;
	const bool bWorldExists = IFileManager::Get().DirectoryExists(*GetWorldDir(InIntent.TargetSaveId));
	if (bWorldExists && (!ReadManifest(InIntent.TargetSaveId, Existing) || Existing.CurrentGeneration != 0))
	{
		OutError = TEXT("Save.PendingTargetNotEmpty");
		return false;
	}
	const FString StoreDirectory = GetWorldDir(InIntent.TargetSaveId) / TEXT("Inventory") / InIntent.StoreId.ToString(EGuidFormats::Digits);
	if (IFileManager::Get().DirectoryExists(*StoreDirectory) ||
		IFileManager::Get().DirectoryExists(*GetGenerationDir(InIntent.TargetSaveId, InIntent.TargetGeneration)))
	{
		OutError = TEXT("Save.PendingTargetAlreadyPrepared");
		return false;
	}
	if (!WriteBinaryAtomic(IntentPath, EncodePendingWorld(InIntent)))
	{
		// rename 后同步失败也可能留下可读 intent。保留它给恢复，不猜测磁盘提交结果。
		OutError = TEXT("Save.PendingIntentWriteFailed");
		return false;
	}
	OutLease = MoveTemp(Lease);
	return true;
}

bool FSaveGameStorage::CheckPendingWorldLease(
	const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& InLease, FString& OutError) const
{
	if (!InLease || InLease->NativeLock == -1 || InLease->RootDirectory != CanonicalSavePath(GetRootDir()))
	{
		OutError = TEXT("Save.PendingWorldLeaseMismatch");
		return false;
	}
	FSavePendingWorld Current;
	if (!ReadPendingWorld(InLease->Intent.TargetSaveId, Current) || !SamePendingWorld(Current, InLease->Intent))
	{
		OutError = TEXT("Save.PendingWorldIntentMismatch");
		return false;
	}
	return true;
}

bool FSaveGameStorage::RemovePendingWorldIntent(const FGuid& SaveId)
{
	const FString Path = GetPendingWorldPath(SaveId);
	return IFileManager::Get().Delete(*Path, false, true) && SyncSaveDirectory(FPaths::GetPath(Path));
}

bool FSaveGameStorage::CompletePendingWorld(
	const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& InLease, FString& OutError)
{
	OutError.Reset();
	if (!CheckPendingWorldLease(InLease, OutError)) return false;
	const FSavePendingWorld& Intent = InLease->Intent;
	FSaveManifest Manifest;
	if (!ReadManifest(Intent.TargetSaveId, Manifest) || Manifest.LastCommitOperationId != Intent.OperationId ||
		Manifest.CurrentGeneration != Intent.TargetGeneration || !IsGenerationComplete(Intent.TargetSaveId, Intent.TargetGeneration, Manifest))
	{
		OutError = TEXT("Save.PendingWorldNotPublished");
		return false;
	}
	// 再次同步可将“rename 已发生、第一次父目录同步失败”的回执安全收尾。
	if (!ConfirmManifestCommit(Intent.TargetSaveId, Intent.OperationId, Intent.TargetGeneration, OutError) ||
		!RemovePendingWorldIntent(Intent.TargetSaveId))
	{
		OutError = TEXT("Save.PendingWorldCompletionFailed");
		return false;
	}
	return true;
}

bool FSaveGameStorage::AbortPendingWorld(
	const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& InLease, FString& OutError)
{
	OutError.Reset();
	if (!CheckPendingWorldLease(InLease, OutError)) return false;
	const FSavePendingWorld& Intent = InLease->Intent;
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
	if (AcquireWorldWrite(Intent.TargetSaveId, Writer) != ESaveWorldWriteResult::Acquired)
	{
		OutError = TEXT("Save.PendingWorldBusy");
		return false;
	}
	TGuardValue<TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>> Writing(ActiveWorldWrite, Writer);
	FSaveManifest Manifest;
	const bool bHasManifest = ReadManifest(Intent.TargetSaveId, Manifest);
	if (bHasManifest && Manifest.CurrentGeneration > 0)
	{
		// 最终清单一旦指向已发布代，任何 abort 都只能收尾，不能删该代或其经济目录。
		if (Manifest.LastCommitOperationId == Intent.OperationId && Manifest.CurrentGeneration == Intent.TargetGeneration)
			return CompletePendingWorld(InLease, OutError);
		OutError = TEXT("Save.PendingWorldPublicationConflict");
		return false;
	}
	if (!bHasManifest && ProbeSavePath(GetManifestPath(Intent.TargetSaveId)) != ESavePathPresence::Missing)
	{
		OutError = TEXT("Save.PendingWorldManifestCorrupt");
		return false;
	}
	const FString WorldDirectory = GetWorldDir(Intent.TargetSaveId);
	if (Intent.bDeleteWorldOnAbort)
	{
		if (IFileManager::Get().DirectoryExists(*WorldDirectory) &&
			!FSaveGameGenerationReadLease::TryRetireDirectory(WorldDirectory, Writer.Get()))
		{
			OutError = TEXT("Save.PendingWorldRetireBlocked");
			return false;
		}
		if (!SyncSaveDirectory(GetWorldsDir()))
		{
			OutError = TEXT("Save.PendingWorldRetireSyncFailed");
			return false;
		}
	}
	else if (IFileManager::Get().DirectoryExists(*WorldDirectory))
	{
		// 用户事先建好的空槽保留名称和 gen0 manifest，仅回收此次分支数据。
		const FString StoreDirectory = WorldDirectory / TEXT("Inventory") / Intent.StoreId.ToString(EGuidFormats::Digits);
		if ((IFileManager::Get().DirectoryExists(*StoreDirectory) &&
			!FSaveGameGenerationReadLease::TryRetireDirectory(StoreDirectory, Writer.Get())) ||
			!CleanupUncommittedGenerations(Intent.TargetSaveId, 0) ||
			(IFileManager::Get().DirectoryExists(*(WorldDirectory / TEXT("Inventory"))) && !SyncSaveDirectory(WorldDirectory / TEXT("Inventory"))) ||
			!SyncSaveDirectory(GetGenerationsDir(Intent.TargetSaveId)))
		{
			OutError = TEXT("Save.PendingWorldRetireBlocked");
			return false;
		}
	}
	if (!RemovePendingWorldIntent(Intent.TargetSaveId))
	{
		OutError = TEXT("Save.PendingWorldIntentRemovalFailed");
		return false;
	}
	return true;
}

void FSaveGameStorage::RecoverPendingWorlds(TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	TArray<FString> Files;
	const FString OperationsDirectory = GetRootDir() / TEXT("WorldOperations");
	IFileManager::Get().FindFiles(Files, *(OperationsDirectory / TEXT("*.intent")), true, false);
	for (const FString& File : Files)
	{
		FGuid SaveId;
		if (!FGuid::ParseExact(FPaths::GetBaseFilename(File), EGuidFormats::Digits, SaveId)) continue;
		FString Error;
		const int64 NativeLock = AcquirePendingWorldLock(GetPendingWorldPath(SaveId) + TEXT(".lock"), Error);
		if (NativeLock == -1)
		{
			OutErrors.Add(SaveId.ToString() + TEXT(":") + Error);
			continue;
		}
		FSavePendingWorld Intent;
		if (!ReadPendingWorld(SaveId, Intent))
		{
			ReleasePendingWorldLock(NativeLock);
			OutErrors.Add(SaveId.ToString() + TEXT(":Save.PendingWorldIntentCorrupt"));
			continue;
		}
		TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease = MakeShareable(
			new FSavePendingWorldLease(CanonicalSavePath(GetRootDir()), Intent, NativeLock));
		// Abort 自己判读唯一发布清单；已提交则完成回执，否则只回收被 intent 授权的目标。
		if (!AbortPendingWorld(Lease, Error)) OutErrors.Add(SaveId.ToString() + TEXT(":") + Error);
	}
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
	if (!CreateSaveDirectoryDurable(FPaths::GetPath(Path))) return false;
	TUniquePtr<IFileHandle> File(Platform.OpenWrite(*Path));
	const bool bWritten = File && (Bytes.IsEmpty() || File->Write(Bytes.GetData(), Bytes.Num())) && File->Flush(true);
	File.Reset();
	return bWritten && SyncSaveDirectory(FPaths::GetPath(Path));
}

bool FSaveGameStorage::WriteBinaryAtomic(const FString& Path, const TArray<uint8>& Bytes)
{
	const FString TempPath = Path + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
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
	// 包含复制文件，先子文件/子目录再父目录同步；不收集所有文件路径。
	return FlushSaveTree(GetTempGenerationDir(SaveId, Generation));
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
	const FString RetiredRoot = GetRootDir() / TEXT("RetiredWorlds");
	if (IFileManager::Get().DirectoryExists(*RetiredRoot))
	{
		FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*RetiredRoot,
			[](const TCHAR* Path, const bool bDirectory)
			{
				const FString Name = FPaths::GetCleanFilename(Path);
				FGuid SaveId, RetirementId;
				if (bDirectory && Name.Len() == 65 && Name[32] == '.' &&
					FGuid::ParseExact(Name.Left(32), EGuidFormats::Digits, SaveId) &&
					FGuid::ParseExact(Name.Mid(33), EGuidFormats::Digits, RetirementId))
					IFileManager::Get().DeleteDirectory(Path, false, true);
				return true;
			});
		SyncSaveDirectory(RetiredRoot);
	}
	TArray<FString> Worlds;
	IFileManager::Get().FindFiles(Worlds, *(GetWorldsDir() / TEXT("*")), false, true);
	for(const FString& World : Worlds)
	{
		FGuid SaveId;
		if(FGuid::ParseExact(World, EGuidFormats::Digits, SaveId))
		{
			TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> Writer;
			if (AcquireWorldWrite(SaveId, Writer) != ESaveWorldWriteResult::Acquired) continue;
			FSaveInventoryRetirementLocks Owners;
			FString Error;
			const FString IntentPath = GetPendingWorldPath(SaveId);
			if (!CreateSaveDirectoryDurable(FPaths::GetPath(IntentPath))) continue;
			const int64 Lock = AcquirePendingWorldLock(IntentPath + TEXT(".lock"), Error);
			if (Lock == -1) continue;
			Owners.Handles.Add(Lock);
			// 检查 pending 和清理必须处在同一原生锁内，不能跨进程先检查、后误删新临时代。
			// 活动经济 Store 也阻止另一进程清理其正在保存的普通代次。
			if (HasPendingWorld(SaveId) || !Owners.Acquire(GetWorldDir(SaveId))) continue;
			TGuardValue<TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>> Writing(ActiveWorldWrite, Writer);
			FSaveManifest Manifest;
			if(ReadManifest(SaveId, Manifest))
			{
				CleanupUncommittedGenerations(SaveId, Manifest.CurrentGeneration);
				// 已发布代缺损属于损坏，保留原清单供明确恢复；常规清理不回退提交代次。
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

