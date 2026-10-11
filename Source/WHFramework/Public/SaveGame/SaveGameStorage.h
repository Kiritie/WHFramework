#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "SaveGame/SaveGameGenerationLease.h"

struct FSaveManifest;

// 复制源的跨进程读生命周期：与整槽删除/其他同源复制互斥，不写入 intent。
class WHFRAMEWORK_API FSaveWorldSourceLease
{
public:
    ~FSaveWorldSourceLease();
    FSaveWorldSourceLease(const FSaveWorldSourceLease&) = delete;
    FSaveWorldSourceLease& operator=(const FSaveWorldSourceLease&) = delete;

private:
    friend class FSaveGameStorage;
    explicit FSaveWorldSourceLease(int64 InNativeLock);
    int64 NativeLock = -1;
};

// 只属于一次新槽/空槽分支，不授权覆盖已有非空槽。所有路径均由 GUID 推导。
struct WHFRAMEWORK_API FSavePendingWorld
{
    FGuid OperationId;
    FGuid TargetSaveId;
    FGuid SourceSaveId;
    FGuid StoreId;
    FGuid WorldId;
    int32 SourceGeneration = 0;
    int32 TargetGeneration = 1;
    bool bDeleteWorldOnAbort = false;
};

// Worker 和 GT 通过纯原生共享对象持有同一跨进程操作锁。析构从不删除磁盘数据。
class WHFRAMEWORK_API FSavePendingWorldLease
{
public:
    ~FSavePendingWorldLease();
    FSavePendingWorldLease(const FSavePendingWorldLease&) = delete;
    FSavePendingWorldLease& operator=(const FSavePendingWorldLease&) = delete;
    const FSavePendingWorld& GetIntent() const;

private:
    friend class FSaveGameStorage;
    FSavePendingWorldLease(FString InRootDirectory, const FSavePendingWorld& InIntent, int64 InNativeLock);
    FString RootDirectory;
    FSavePendingWorld Intent;
    int64 NativeLock = -1;
};

class WHFRAMEWORK_API FSaveGameStorage
{
public:
	explicit FSaveGameStorage(int32 InUserIndex = 0, const FString& InRootDirectory = FString(),
		const FGuid& InTransactionId = FGuid());
	ESaveWorldWriteResult BeginWorldWrite(const FGuid& SaveId,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	void EndWorldWrite();

	const FString& GetRootDir() const;
	FString GetProfilesDir() const;
	FString GetProfileDir(FName ProfileName) const;
	FString GetProfileModuleFilePath(FName ProfileName, FName ModuleName) const;
	FString GetWorldsDir() const;
	FString GetWorldDir(const FGuid& SaveId) const;
	FString GetManifestPath(const FGuid& SaveId) const;
	FString GetGenerationsDir(const FGuid& SaveId) const;
	FString GetGenerationDir(const FGuid& SaveId, int32 Generation) const;
	FString GetTempGenerationDir(const FGuid& SaveId, int32 Generation) const;
	FString GetModuleFilePath(const FGuid& SaveId, int32 Generation, FName ModuleName) const;
	FString GetTempModuleFilePath(const FGuid& SaveId, int32 Generation, FName ModuleName) const;
	FString GetLastActiveSavePath() const;

	bool EnsureRoot();
	bool CreateWorldDirectory(const FGuid& SaveId);
	bool DeleteWorldDirectory(const FGuid& SaveId);
	bool ReadManifest(const FGuid& SaveId, FSaveManifest& OutManifest) const;
	bool WriteManifestAtomic(const FGuid& SaveId, const FSaveManifest& Manifest,
        const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& InPendingLease = nullptr);
    bool ConfirmManifestCommit(const FGuid& SaveId, const FGuid& OperationId, int32 Generation, FString& OutError);
	bool EnumerateManifests(TArray<FSaveManifest>& OutManifests) const;

    // 必须在读取复制源 Manifest 前取得，并保持到复制 Finish 完成；普通代次还须持有读租约。
    bool AcquireWorldSourceLease(const FGuid& SaveId,
        TSharedPtr<FSaveWorldSourceLease, ESPMode::ThreadSafe>& OutLease, FString& OutError);
    // Begin 必须先于任何分支数据写入；TargetGeneration 在接纳时固定。
    bool BeginPendingWorld(const FSavePendingWorld& InIntent,
        TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& OutLease, FString& OutError);
    // 普通 Manifest 是唯一提交点；Complete 仅清理回执，Abort 永不删除已经发布的分支。
    bool CompletePendingWorld(const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& InLease, FString& OutError);
    bool AbortPendingWorld(const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& InLease, FString& OutError);
    // 启动时先恢复 intent，再清理临时代；忙碌/损坏的 intent 保留并阻止常规清理。
    void RecoverPendingWorlds(TArray<FString>& OutErrors);
    FString GetPendingWorldPath(const FGuid& SaveId) const;
    bool HasPendingWorld(const FGuid& SaveId) const;

	bool ReadLastActiveSave(FGuid& OutSaveId) const;
	bool WriteLastActiveSave(const FGuid& SaveId);
	void ClearLastActiveSave();
	bool WriteBinary(const FString& Path, const TArray<uint8>& Bytes);
	static bool WriteBinaryDurable(const FString& Path, TConstArrayView<uint8> Bytes);
	bool WriteBinaryAtomic(const FString& Path, const TArray<uint8>& Bytes);
	bool ReadBinary(const FString& Path, TArray<uint8>& OutBytes) const;
	bool WriteTextAtomic(const FString& Path, const FString& Text);
	bool PrepareTempGeneration(const FGuid& SaveId, int32 PreviousGeneration, int32 NewGeneration,
		const FString* InCapturedVoxelGeneration = nullptr,
		const TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>* InCapturedSourceLease = nullptr);
	bool FlushTempGeneration(const FGuid& SaveId, int32 Generation);
	bool CommitGeneration(const FGuid& SaveId, int32 Generation);
	bool CleanupUncommittedGenerations(const FGuid& SaveId, int32 CurrentGeneration);
	void CleanupAllTempGenerations();
	bool CopyCurrentGeneration(const FGuid& Source, const FGuid& Target, int32 SourceGeneration, int32 TargetGeneration);
	bool CleanupTempGenerations(const FGuid& SaveId);
	bool IsGenerationComplete(const FGuid& SaveId, int32 Generation, const FSaveManifest& Manifest) const;
	int32 RecoverLatestCompleteGeneration(const FGuid& SaveId, const FSaveManifest& Manifest) const;

private:
    bool ReadPendingWorld(const FGuid& SaveId, FSavePendingWorld& OutIntent) const;
    bool CheckPendingWorldLease(const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& InLease,
        FString& OutError) const;
    bool RemovePendingWorldIntent(const FGuid& SaveId);
	ESaveWorldWriteResult AcquireWorldWrite(const FGuid& SaveId,
		TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>& OutWriter);
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> ActiveWorldWrite;
	int32 UserIndex = 0;
	FString RootDirectory;
	FGuid WriteTransactionId;
};

