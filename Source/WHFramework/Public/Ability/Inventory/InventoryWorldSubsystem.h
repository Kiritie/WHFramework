#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Ability/Inventory/InventoryDomain.h"
#include "Ability/Inventory/InventoryPlanner.h"
#include "Ability/Inventory/InventoryJournal.h"
#include "Ability/Inventory/InventoryTransactionService.h"
#include "InventoryWorldSubsystem.generated.h"

class UAbilityInventoryBase;
class UInventoryItemAssetCatalog;
class UInventoryNetworkComponent;
class FInventoryTransactionService;
class FInventoryCommitCoordinator;

using FInventoryAccessValidator = TFunction<EInventoryResultCode(APlayerController*, const FGuid&, bool)>;
using FInventoryMutationValidator = TFunction<EInventoryResultCode(APlayerController*, const FGuid&, const FInventorySnapshot&, const FInventorySnapshot&)>;
using FInventorySnapshotProjector = TFunction<bool(const FInventoryPlan&, TArray<FInventoryDomainRedo>&, FString&)>;

struct WHFRAMEWORK_API FInventoryAccessBinding
{
    FGuid InventoryId;
    TWeakObjectPtr<UObject> LifetimeOwner;
    FVector Location = FVector::ZeroVector;
    float MaximumDistance = 350.f;
    bool bDistanceLimited = true;
    FInventoryAccessValidator Validator;
    FInventoryMutationValidator MutationValidator;
};

UCLASS()
class WHFRAMEWORK_API UInventoryWorldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    UInventoryWorldSubsystem();
    virtual ~UInventoryWorldSubsystem() override;
    virtual void Deinitialize() override;
    virtual void Tick(float InDeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;

    bool Configure(TConstArrayView<FInventoryItemDefinition> InItems, TConstArrayView<FInventoryProfileDefinition> InProfiles,
        TSharedPtr<FInventoryRuleProvider> InRules, FString& OutError);
    bool SetItemAssets(UInventoryItemAssetCatalog* InAssets);
    const UInventoryItemAssetCatalog* GetItemAssets() const;
    bool RegisterRecordDomain(FName InDomain, int32 InMaximumBytes, FInventoryRecordValidator InValidator, FString& OutError);
    bool RegisterDomainAdapter(TSharedRef<IInventoryDomainAdapter> InAdapter, FString& OutError);
    bool RegisterSnapshotProjector(FName InName, FInventorySnapshotProjector InProjector, FString& OutError);
    bool OpenStore(const FGuid& InWorldId, const FGuid& InStoreId, const FString& InDirectory, bool bInCreateNew, FString& OutError);
    bool CaptureArchiveSnapshot(FInventoryArchiveSnapshot& OutSnapshot, FString& OutError) const;
    bool FreezeArchive(const FGuid& InOperationId, FInventoryArchiveSnapshot& OutSnapshot, FString& OutError);
    bool ReleaseArchive(const FGuid& InOperationId, FString& OutError);
    bool AdoptArchive(const FGuid& InOperationId, FInventoryPreparedStore& InPrepared, FString& OutError);
    bool IsArchiveFrozen() const;
    bool SaveCheckpoint(FString& OutError);
    bool CloseStore(bool bInCheckpoint, FString& OutError);
    bool IsReady() const;
    bool IsQuarantined() const;
    const FString& GetFailureReason() const;
    void ReportProjectionFailure(const FString& InReason);

    FGuid GetWorldId() const;
    FGuid GetStoreId() const;
    FGuid GetWorldEpoch() const;
    const FInventoryItemRegistry& GetRegistry() const;
    const TMap<FPrimaryAssetId, FInventoryProfileDefinition>& GetProfiles() const;
    const FInventoryRuleProvider* GetRules() const;
    const FInventoryRecordStore& GetRecords() const;

    UAbilityInventoryBase* Resolve(const FGuid& InInventoryId);
    bool GetSnapshot(const FGuid& InInventoryId, FInventorySnapshot& OutSnapshot, FString& OutError) const;
    bool Retire(const FGuid& InInventoryId, FString& OutError);
    bool AcquireLease(const FGuid& InInventoryId);
    void ReleaseLease(const FGuid& InInventoryId);
    bool SetAccessBinding(FInventoryAccessBinding InBinding);
    void RemoveAccessBinding(const FGuid& InInventoryId);
    EInventoryResultCode ValidateAccess(APlayerController* InController, const FGuid& InPlayerId,
        const FGuid& InInventoryId, bool bInWrite) const;

    FInventoryAuthorityContext MakeServerAuthority(const FGuid& InActorId, FName InReason) const;
    FInventoryCommandResult ExecuteAuthorizedBatch(const FInventoryBatchRequest& InRequest, const FInventoryAuthorityContext& InAuthority);
    void EnqueueAuthorizedBatch(FInventoryBatchRequest InRequest, FInventoryAuthorityContext InAuthority,
        TFunction<void(const FInventoryCommandResult&)> InCompletion);

    FInventoryCommitted OnCommitted;
    FInventoryRecordsCommitted OnRecordsCommitted;
    FInventoryRetired OnRetired;
    FInventoryRetired OnDeleted;
    FSimpleMulticastDelegate OnClosing;
    FSimpleMulticastDelegate OnStoreAdopted;

private:
    UPROPERTY(Transient)
    TMap<FGuid, TObjectPtr<UAbilityInventoryBase>> Active;

    UPROPERTY(Transient)
    TObjectPtr<UInventoryItemAssetCatalog> ItemAssets;

    struct FQueuedRequest
    {
        FInventoryBatchRequest Request;
        FInventoryAuthorityContext Authority;
        TFunction<void(const FInventoryCommandResult&)> Completion;
    };

    FInventoryItemRegistry Registry;
    TMap<FPrimaryAssetId, FInventoryProfileDefinition> Profiles;
    TSharedPtr<FInventoryRuleProvider> Rules;
    FInventoryRecordStore Records;
    TMap<FName, TSharedRef<IInventoryDomainAdapter>> Adapters;
    TMap<FName, FInventorySnapshotProjector> SnapshotProjectors;
    TMap<FGuid, TArray<uint8>> Cold;
    TMap<FGuid, FInventoryTombstone> Tombstones;
    TMap<FGuid, FGuid> InstanceOwners;
    TMap<FGuid, int32> LeaseCounts;
    TSet<FGuid> PendingRetire;
    TMap<FGuid, FInventoryAccessBinding> AccessBindings;
    TArray<FQueuedRequest> Queue;
    TUniquePtr<FInventoryTransactionService> Transactions;
    FInventoryJournal Journal;
    FGuid WorldId;
    FGuid StoreId;
    FGuid WorldEpoch;
    FGuid ArchiveOperationId;
    FGuid ArchiveWorldEpoch;
    uint64 ArchiveSequence = 0;
    uint32 ArchiveCheckpointCrc = 0;
    FString FailureReason;
    bool bConfigured = false;
    bool bOpen = false;
    bool bQuarantined = false;
    bool bCommitting = false;
    bool bPublishing = false;
    bool bClosing = false;

    FInventoryAuthorityContext MakeClientAuthority(APlayerController* InController, const FGuid& InActorId,
        const FGuid& InConnectionId, TConstArrayView<FGuid> InInventories) const;
    bool ValidateInstances(TConstArrayView<FInventorySnapshot> InBefore, TConstArrayView<FInventorySnapshot> InAfter, FString& OutError) const;
    bool RebuildInstanceIndex(FString& OutError);
    bool CaptureCheckpoint(FInventoryCheckpoint& OutCheckpoint, FString& OutError) const;
    void Quarantine(const FString& InReason);

    friend class FInventoryTransactionService;
    friend class FInventoryCommitCoordinator;
    friend class UInventoryNetworkComponent;
};
