#pragma once

#include "CoreMinimal.h"

class FInventoryJournal;
class UInventoryWorldSubsystem;

// 完整经济边界是可独立复制的值；逻辑世界身份与持久目录身份分别保存。
struct WHFRAMEWORK_API FInventoryArchiveSnapshot
{
    static constexpr int32 MaximumCheckpointBytes = 512 * 1024 * 1024;
    static constexpr int32 MaximumBytes = MaximumCheckpointBytes + 64;

    FGuid WorldId;
    FGuid StoreId;
    uint64 Sequence = 0;
    TArray<uint8> CheckpointBytes;

    bool Validate(FString& OutError) const;
    bool Encode(TArray<uint8>& OutBytes, FString& OutError) const;
    static bool Decode(TConstArrayView<uint8> InBytes, FInventoryArchiveSnapshot& OutSnapshot, FString& OutError);
};

// 原生准备对象只持有目标日志句柄；普通存档是否发布由 SaveGame 的 manifest 决定。
class WHFRAMEWORK_API FInventoryPreparedStore
{
public:
    ~FInventoryPreparedStore();
    FInventoryPreparedStore(const FInventoryPreparedStore&) = delete;
    FInventoryPreparedStore& operator=(const FInventoryPreparedStore&) = delete;

    FGuid GetWorldId() const;
    FGuid GetStoreId() const;
    FGuid GetSourceStoreId() const;
    FGuid GetOperationId() const;
    uint64 GetSequence() const;
    const FString& GetDirectory() const;
    bool IsReady() const;
    bool IsCommitted() const;
    bool IsAdopted() const;
    bool MarkCommitted(FString& OutError);
    void Close();

private:
    FInventoryPreparedStore();

    FGuid WorldId;
    FGuid StoreId;
    FGuid SourceStoreId;
    FGuid OperationId;
    uint64 Sequence = 0;
    uint32 CheckpointCrc = 0;
    FString Directory;
    TUniquePtr<FInventoryJournal> Journal;
    bool bCommitted = false;
    bool bAdopted = false;

    friend class FInventoryJournal;
    friend class UInventoryWorldSubsystem;
};
