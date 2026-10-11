#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryArchiveSnapshot.h"
#include "Ability/Inventory/InventoryDomain.h"

struct WHFRAMEWORK_API FInventoryTombstone
{
    uint32 Generation = 1;
    uint64 Revision = 0;
};

struct WHFRAMEWORK_API FInventoryJournalChange
{
    FGuid InventoryId;
    uint64 BeforeRevision = 0;
    uint64 AfterRevision = 0;
    uint32 Generation = 1;
    bool bCreated = false;
    bool bDeleted = false;
    TArray<uint8> AfterBytes;
};

struct WHFRAMEWORK_API FInventoryJournalRecord
{
    FGuid WorldId;
    FGuid TransactionId;
    uint64 Sequence = 0;
    TArray<FInventoryJournalChange> Inventories;
    TArray<FInventoryDomainRedo> Domains;
};

struct WHFRAMEWORK_API FInventoryCheckpoint
{
    FGuid WorldId;
    uint64 Sequence = 0;
    TMap<FGuid, TArray<uint8>> Inventories;
    TMap<FGuid, FInventoryTombstone> Tombstones;
    TArray<FInventoryDomainRecord> Domains;
};

class WHFRAMEWORK_API FInventoryJournal
{
public:
    FInventoryJournal();
    ~FInventoryJournal();
    FInventoryJournal(const FInventoryJournal&) = delete;
    FInventoryJournal& operator=(const FInventoryJournal&) = delete;

    bool Open(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId, bool bInCreateNew,
        FInventoryCheckpoint& OutCheckpoint, TArray<FInventoryJournalRecord>& OutRecords, FString& OutError);
    bool Append(FInventoryJournalRecord& InOutRecord, FString& OutError);
    bool Checkpoint(const FInventoryCheckpoint& InCheckpoint, FString& OutError);
    void Close();
    bool IsHealthy() const;
    uint64 GetSequence() const;
    bool NeedsCheckpoint() const;
    FGuid GetStoreId() const;

    static bool ExportLatest(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId,
        FInventoryArchiveSnapshot& OutSnapshot, FString& OutError);
    static bool PrepareFork(const FInventoryArchiveSnapshot& InSnapshot, const FString& InTargetDirectory,
        const FGuid& InTargetStoreId, const FGuid& InOperationId,
        TSharedPtr<FInventoryPreparedStore, ESPMode::ThreadSafe>& OutPrepared, FString& OutError);
    static bool MarkPreparedCommitted(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId,
        const FGuid& InOperationId, FString& OutError);
    static bool DiscardPrepared(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId,
        const FGuid& InOperationId, FString& OutError);
    static bool DiscardUnbound(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId, FString& OutError);

private:
    static constexpr int32 MaximumRecordBytes = 64 * 1024 * 1024;
    static constexpr int32 MaximumCheckpointBytes = FInventoryArchiveSnapshot::MaximumCheckpointBytes;
    FString Directory;
    FGuid WorldId;
    FGuid StoreId;
    uint64 Sequence = 0;
    uint64 SaveGeneration = 0;
    int64 JournalBytes = 0;
    bool bHealthy = false;
    int64 StoreLock = -1;

    static bool EncodeRecord(const FInventoryJournalRecord& InRecord, TArray<uint8>& OutBytes, FString& OutError);
    static bool DecodeRecord(TConstArrayView<uint8> InBytes, FInventoryJournalRecord& OutRecord, FString& OutError);
    static bool EncodeCheckpoint(const FInventoryCheckpoint& InCheckpoint, TArray<uint8>& OutBytes, FString& OutError);
    static bool DecodeCheckpoint(TConstArrayView<uint8> InBytes, FInventoryCheckpoint& OutCheckpoint, FString& OutError);
    static bool ReadFile(const FString& InPath, int64 InLimit, TArray<uint8>& OutBytes, FString& OutError);
    static bool WriteDurable(const FString& InPath, TConstArrayView<uint8> InBytes, bool bInAppend, FString& OutError);
    static bool ReplaceDurable(const FString& InTemporary, const FString& InTarget, FString& OutError);
    static bool SyncDirectory(const FString& InDirectory, FString& OutError);
    bool AcquireStoreLock(FString& OutError);
    void ReleaseStoreLock();
    bool ReadJournal(uint64 InCheckpointSequence, TArray<FInventoryJournalRecord>& OutRecords, bool bInRepairTail, FString& OutError);
    bool OpenInternal(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId, bool bInCreateNew,
        bool bInRepairTail, FInventoryCheckpoint& OutCheckpoint, TArray<FInventoryJournalRecord>& OutRecords, FString& OutError);
    bool SealPrepared(const FGuid& InOperationId, FString& OutError);
    bool VerifyPreparedReady(FString& OutError) const;
    bool RemoveLockedDirectory(FString& OutError);
    void SwapState(FInventoryJournal& InOther);
    static bool FoldRecords(FInventoryCheckpoint& InOutCheckpoint, TArray<FInventoryJournalRecord>& InRecords, FString& OutError);

    friend struct FInventoryArchiveSnapshot;
    friend class FInventoryPreparedStore;
    friend class UInventoryWorldSubsystem;
};
