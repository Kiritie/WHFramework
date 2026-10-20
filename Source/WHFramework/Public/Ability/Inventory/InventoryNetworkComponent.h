#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ability/Inventory/InventoryDomain.h"
#include "InventoryNetworkComponent.generated.h"

class UInventoryWorldSubsystem;
class UPackageMap;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryClientParticipant
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid InventoryId;

    UPROPERTY(BlueprintReadOnly)
    FGuid SessionId;

    UPROPERTY()
    uint64 Revision = 0;

    UPROPERTY()
    uint32 Generation = 1;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryClientRequest
{
    GENERATED_BODY()

    UPROPERTY()
    uint64 RequestId = 0;

    UPROPERTY()
    FGuid ConnectionId;

    UPROPERTY()
    FGuid StoreId;

    UPROPERTY()
    FGuid WorldEpoch;

    UPROPERTY(BlueprintReadWrite)
    FName DomainAction;

    UPROPERTY(BlueprintReadWrite)
    EInventoryOperationType Operation = EInventoryOperationType::None;

    UPROPERTY(BlueprintReadWrite)
    FGuid SourceInventoryId;

    UPROPERTY(BlueprintReadWrite)
    FGuid TargetInventoryId;

    UPROPERTY(BlueprintReadWrite)
    TArray<FGuid> TargetInventories;

    UPROPERTY(BlueprintReadWrite)
    FInventorySlotKey SourceSlot;

    UPROPERTY(BlueprintReadWrite)
    FInventorySlotKey TargetSlot;

    UPROPERTY(BlueprintReadWrite)
    FGameplayTag SourceGroup;

    UPROPERTY(BlueprintReadWrite)
    FGameplayTag TargetGroup;

    UPROPERTY(BlueprintReadWrite)
    int32 Quantity = 1;

    UPROPERTY(BlueprintReadWrite)
    bool bAllowPartial = false;

    UPROPERTY(BlueprintReadWrite)
    bool bFavorite = false;

    UPROPERTY(BlueprintReadWrite)
    bool bAutomatic = false;

    UPROPERTY(BlueprintReadWrite)
    FPrimaryAssetId DefinitionId;

    UPROPERTY(BlueprintReadWrite)
    FGuid TargetEntityId;

    UPROPERTY(BlueprintReadWrite)
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY()
    FInventoryStackSignature ExpectedSignature;

    UPROPERTY()
    TArray<FInventoryClientParticipant> Participants;

    bool NetSerialize(FArchive& InArchive, UPackageMap* InMap, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FInventoryClientRequest> : public TStructOpsTypeTraitsBase2<FInventoryClientRequest>
{
    enum
    {
        WithNetSerializer = true
    };
};

USTRUCT()
struct WHFRAMEWORK_API FInventorySnapshotChunk
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid ConnectionId;

    UPROPERTY()
    FGuid StoreId;

    UPROPERTY()
    FGuid WorldEpoch;

    UPROPERTY()
    FGuid SessionId;

    UPROPERTY()
    FGuid InventoryId;

    UPROPERTY()
    FGuid TransferId;

    UPROPERTY()
    uint64 Revision = 0;

    UPROPERTY()
    uint32 Generation = 1;

    UPROPERTY()
    int32 Index = 0;

    UPROPERTY()
    int32 Count = 0;

    UPROPERTY()
    int32 TotalBytes = 0;

    UPROPERTY()
    uint32 Checksum = 0;

    UPROPERTY()
    TArray<uint8> Bytes;

    bool NetSerialize(FArchive& InArchive, UPackageMap* InMap, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FInventorySnapshotChunk> : public TStructOpsTypeTraitsBase2<FInventorySnapshotChunk>
{
    enum
    {
        WithNetSerializer = true
    };
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryDelta
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid ConnectionId;

    UPROPERTY()
    FGuid StoreId;

    UPROPERTY()
    FGuid WorldEpoch;

    UPROPERTY()
    FGuid SessionId;

    UPROPERTY(BlueprintReadOnly)
    FGuid InventoryId;

    UPROPERTY(BlueprintReadOnly)
    FGuid TransactionId;

    UPROPERTY()
    uint32 Generation = 1;

    UPROPERTY()
    uint64 FromRevision = 0;

    UPROPERTY()
    uint64 ToRevision = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FInventorySlotState> Slots;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryViewSnapshot, const FInventorySnapshot&);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryViewDelta, const FInventoryDelta&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FInventoryViewChanges, const FInventoryDelta&, const FInventoryChangeSet&);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryRequestFinished, const FInventoryCommandResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryRequestQueued, const FInventoryClientRequest&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FInventoryViewClosed, const FGuid&, EInventoryResultCode);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FInventoryConnectionReset, const FGuid&, const FGuid&, EInventoryResultCode);

using FInventoryClientActionHandler = TFunction<FInventoryCommandResult(const FInventoryClientRequest&, const FInventoryAuthorityContext&)>;

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class WHFRAMEWORK_API UInventoryNetworkComponent : public UActorComponent
{
    GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
    friend class FInventoryNetworkReplicaTest;
    friend class FInventoryArchiveAdoptionTest;
    friend class FInventoryConnectionLifecycleTest;
#endif

public:
    UInventoryNetworkComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type InEndPlayReason) override;
    virtual void TickComponent(float InDeltaTime, ELevelTick InTickType, FActorComponentTickFunction* InTickFunction) override;

    bool BindOwner(const FGuid& InPlayerId, const FGuid& InInventoryId);
    void UnbindOwner();
    bool RegisterAction(FName InAction, FInventoryClientActionHandler InHandler);
    void UnregisterAction(FName InAction);
    uint64 SubmitRequest(FInventoryClientRequest InRequest);
    TArray<FInventoryClientRequest> GetPendingRequests() const;
    bool GetPendingRequest(uint64 InRequestId, FInventoryClientRequest& OutRequest) const;
    bool IsRequestUnconfirmed(uint64 InRequestId) const;
    bool RetryRequest(uint64 InRequestId);
    void OpenInventory(const FGuid& InInventoryId, uint32 InGeneration = 1);
    void CloseInventory(const FGuid& InInventoryId);
    void ResyncInventory(const FGuid& InInventoryId);
    const FInventorySnapshot* FindView(const FGuid& InInventoryId) const;
    FGuid GetOwnerInventoryId() const;
    FGuid GetWorldId() const;
    FGuid GetWorldEpoch() const;
    FGuid GetStoreId() const;
    FGuid GetSessionId(const FGuid& InInventoryId) const;

    FInventoryViewSnapshot OnSnapshot;
    FInventoryViewDelta OnDelta;
    /** 已应用视图的前后差异；重同步不含逐笔事务身份。 */
    FInventoryViewChanges OnAppliedChanges;
    FInventoryRequestFinished OnResult;
    FInventoryRequestQueued OnRequestQueued;
    FInventoryViewClosed OnSessionClosed;
    FInventoryConnectionReset OnConnectionReset;

private:
    struct FServerSession
    {
        FGuid SessionId;
        FGuid InventoryId;
        uint32 Generation = 1;
    };

    struct FClientView
    {
        FGuid SessionId;
        FInventorySnapshot Snapshot;
    };

    struct FAssembly
    {
        FInventorySnapshotChunk Header;
        TArray<uint8> Bytes;
        TBitArray<> Received;
        int32 ReceivedCount = 0;
        double StartedAt = 0;
    };

    struct FPendingRequest
    {
        FInventoryClientRequest Request;
        double SentAt = 0;
        int32 Attempts = 1;
        bool bUnconfirmed = false;
    };

    struct FActionReceipt
    {
        uint64 RequestId = 0;
        TArray<uint8> Identity;
        FInventoryCommandResult Result;
    };

    TWeakObjectPtr<UInventoryWorldSubsystem> WorldSubsystem;
    TMap<FGuid, FServerSession> Sessions;
    TMap<FGuid, FClientView> Views;
    TSet<FGuid> LocallyClosedSessions;
    TMap<FGuid, FAssembly> Assemblies;
    TMap<uint64, FPendingRequest> Pending;
    TMap<FName, FInventoryClientActionHandler> Actions;
    TArray<FActionReceipt> Receipts;
    FDelegateHandle CommitHandle;
    FDelegateHandle DeleteHandle;
    FDelegateHandle ClosingHandle;
    FDelegateHandle AdoptedHandle;
    FGuid PlayerId;
    FGuid OwnerInventoryId;
    FGuid ConnectionId;
    FGuid WorldEpoch;
    FGuid ConnectionWorldId;
    FGuid ConnectionStoreId;
    uint64 NextRequestId = 1;
    uint64 ClientContextRevision = 0;
    uint64 RequestHighWaterMark = 0;
    double BudgetUpdatedAt = 0;
    double Tokens = 40;
    float SessionCheckAccumulator = 0;
    bool bResettingConnection = false;

    UFUNCTION(Server, Reliable)
    void ServerOpen(FGuid InConnectionId, FGuid InStoreId, FGuid InInventoryId, uint32 InGeneration);

    UFUNCTION(Server, Reliable)
    void ServerClose(FGuid InSessionId);

    UFUNCTION(Server, Reliable)
    void ServerResync(FGuid InInventoryId, FGuid InSessionId);

    UFUNCTION(Server, Reliable)
    void ServerSubmit(const FInventoryClientRequest& InRequest);

    UFUNCTION(Client, Reliable)
    void ClientBegin(FGuid InConnectionId, FGuid InWorldId, FGuid InStoreId, FGuid InWorldEpoch, FGuid InOwnerInventoryId);

    UFUNCTION(Client, Reliable)
    void ClientReset(FGuid InConnectionId, EInventoryResultCode InReason);

    UFUNCTION(Client, Reliable)
    void ClientSnapshot(const FInventorySnapshotChunk& InChunk);

    UFUNCTION(Client, Reliable)
    void ClientDelta(const FInventoryDelta& InDelta);

    UFUNCTION(Client, Reliable)
    void ClientResult(FGuid InConnectionId, const FInventoryCommandResult& InResult);

    UFUNCTION(Client, Reliable)
    void ClientClosed(FGuid InConnectionId, FGuid InInventoryId, FGuid InSessionId, EInventoryResultCode InReason);

    bool ConsumeBudget(double InCost);
    void SendSnapshot(const FServerSession& InSession);
    void CloseServerSession(const FGuid& InInventoryId, EInventoryResultCode InReason);
    void HandleCommit(const FInventoryChangeSet& InChanges);
    void HandleDeleted(const FGuid& InInventoryId);
    void HandleClosing();
    void HandleStoreAdopted();
    FInventoryCommandResult ExecuteRequest(const FInventoryClientRequest& InRequest, TConstArrayView<uint8> InIdentity);
    bool BuildIdentity(const FInventoryClientRequest& InRequest, TArray<uint8>& OutBytes) const;
    void DeliverResult(FInventoryCommandResult InResult);
};
