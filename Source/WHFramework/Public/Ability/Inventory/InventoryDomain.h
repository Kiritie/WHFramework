#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryTypes.h"

class APlayerController;
class UInventoryWorldSubsystem;
class FInventoryTransactionService;
class FInventoryCommitCoordinator;

class WHFRAMEWORK_API FInventoryAuthorityContext
{
public:
    FGuid GetActorId() const;
    FGuid GetConnectionId() const;
    FGuid GetWorldEpoch() const;
    FGuid GetStoreId() const;
    APlayerController* GetController() const;
    bool IsServerDomain() const;

private:
    FGuid ActorId;
    FGuid ConnectionId;
    FGuid WorldEpoch;
    FGuid StoreId;
    FName Reason;
    TWeakObjectPtr<APlayerController> Controller;
    TSet<FGuid> AllowedInventories;
    bool bServerDomain = false;

    friend class UInventoryWorldSubsystem;
    friend class FInventoryTransactionService;
};

struct WHFRAMEWORK_API FInventoryRecordKey
{
    FName Domain;
    FGuid EntityId;

    bool operator==(const FInventoryRecordKey& InOther) const;
    friend WHFRAMEWORK_API uint32 GetTypeHash(const FInventoryRecordKey& InKey);
};

struct WHFRAMEWORK_API FInventoryDomainRecord
{
    FInventoryRecordKey Key;
    uint64 Revision = 0;
    bool bDeleted = false;
    TArray<uint8> Bytes;
};

struct WHFRAMEWORK_API FInventoryDomainRedo
{
    FName Domain;
    FGuid EntityId;
    uint64 ExpectedRevision = 0;
    uint64 TargetRevision = 1;
    bool bDelete = false;
    TArray<uint8> AfterBytes;
};

struct WHFRAMEWORK_API FInventoryExpectedRevision
{
    FGuid InventoryId;
    uint64 Revision = 0;
    uint32 Generation = 1;
};

struct WHFRAMEWORK_API FInventoryBatchRequest
{
    uint64 RequestId = 0;
    TArray<uint8> RequestIdentity;
    TArray<FInventoryOperation> Operations;
    TArray<FInventoryExpectedRevision> ExpectedRevisions;
    TArray<FInventorySnapshot> CreateInventories;
    TArray<FGuid> DeleteInventories;
    TArray<FInventoryDomainRedo> DomainChanges;
};

using FInventoryRecordValidator = TFunction<bool(TConstArrayView<uint8>, FString&)>;

class WHFRAMEWORK_API FInventoryRecordStore
{
public:
    bool RegisterDomain(FName InDomain, int32 InMaxBytes, FInventoryRecordValidator InValidator, FString& OutError);
    const FInventoryDomainRecord* Find(FName InDomain, const FGuid& InEntityId) const;
    bool IsAlive(FName InDomain, const FGuid& InEntityId) const;
    void Enumerate(FName InDomain, TArray<FInventoryDomainRecord>& OutRecords) const;
    bool ValidateRedo(const FInventoryDomainRedo& InRedo, FString& OutError) const;
    bool ValidateRecord(const FInventoryDomainRecord& InRecord, FString& OutError) const;

private:
    struct FDefinition
    {
        int32 MaxBytes = 0;
        FInventoryRecordValidator Validator;
    };

    TMap<FName, FDefinition> Definitions;
    TMap<FInventoryRecordKey, FInventoryDomainRecord> Records;

    bool Apply(const FInventoryDomainRedo& InRedo, FString& OutError);
    bool Restore(TConstArrayView<FInventoryDomainRecord> InRecords, FString& OutError);
    void Capture(TArray<FInventoryDomainRecord>& OutRecords) const;

    friend class FInventoryCommitCoordinator;
    friend class UInventoryWorldSubsystem;
};

class WHFRAMEWORK_API IInventoryDomainAdapter
{
public:
    virtual ~IInventoryDomainAdapter() = default;
    virtual FName GetDomain() const = 0;
    virtual bool Prepare(const FGuid& InTransactionId, TConstArrayView<FInventoryDomainRedo> InChanges, FString& OutError) = 0;
    virtual bool ValidatePrepared(const FGuid& InTransactionId, FString& OutError) const = 0;
    virtual bool ApplyCommitted(const FGuid& InTransactionId, const FInventoryDomainRedo& InRedo, FString& OutError) = 0;
    virtual bool Restore(const FInventoryDomainRecord& InRecord, FString& OutError) = 0;
    virtual void Publish(const FGuid& InTransactionId) = 0;
    virtual void Cancel(const FGuid& InTransactionId) = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryCommitted, const FInventoryChangeSet&);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryRecordsCommitted, const TArray<FInventoryDomainRecord>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FInventoryRetired, const FGuid&);
