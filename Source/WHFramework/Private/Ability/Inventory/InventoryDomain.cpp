#include "Ability/Inventory/InventoryDomain.h"

#include "GameFramework/PlayerController.h"

FGuid FInventoryAuthorityContext::GetActorId() const
{
    return ActorId;
}

FGuid FInventoryAuthorityContext::GetConnectionId() const
{
    return ConnectionId;
}

FGuid FInventoryAuthorityContext::GetWorldEpoch() const
{
    return WorldEpoch;
}

FGuid FInventoryAuthorityContext::GetStoreId() const
{
    return StoreId;
}

APlayerController* FInventoryAuthorityContext::GetController() const
{
    return Controller.Get();
}

bool FInventoryAuthorityContext::IsServerDomain() const
{
    return bServerDomain;
}

bool FInventoryRecordKey::operator==(const FInventoryRecordKey& InOther) const
{
    return Domain == InOther.Domain && EntityId == InOther.EntityId;
}

uint32 GetTypeHash(const FInventoryRecordKey& InKey)
{
    return HashCombine(GetTypeHash(InKey.Domain), GetTypeHash(InKey.EntityId));
}

bool FInventoryRecordStore::RegisterDomain(FName InDomain, int32 InMaxBytes, FInventoryRecordValidator InValidator, FString& OutError)
{
    if(InDomain.IsNone() || InMaxBytes < 1 || InMaxBytes > 16 * 1024 * 1024 || !InValidator || Definitions.Contains(InDomain))
    {
        OutError = TEXT("Inventory.DomainRegistrationInvalid");
        return false;
    }
    FDefinition Definition;
    Definition.MaxBytes = InMaxBytes;
    Definition.Validator = MoveTemp(InValidator);
    Definitions.Add(InDomain, MoveTemp(Definition));
    return true;
}

const FInventoryDomainRecord* FInventoryRecordStore::Find(FName InDomain, const FGuid& InEntityId) const
{
    return Records.Find(FInventoryRecordKey{InDomain, InEntityId});
}

bool FInventoryRecordStore::IsAlive(FName InDomain, const FGuid& InEntityId) const
{
    const FInventoryDomainRecord* Record = Find(InDomain, InEntityId);
    return Record && !Record->bDeleted;
}

void FInventoryRecordStore::Enumerate(FName InDomain, TArray<FInventoryDomainRecord>& OutRecords) const
{
    OutRecords.Reset();
    for(const TPair<FInventoryRecordKey, FInventoryDomainRecord>& Pair : Records)
    {
        if(Pair.Key.Domain == InDomain)
        {
            OutRecords.Add(Pair.Value);
        }
    }
    OutRecords.Sort([](const FInventoryDomainRecord& InLeft, const FInventoryDomainRecord& InRight)
    {
        return InventoryOrder::GuidLess(InLeft.Key.EntityId, InRight.Key.EntityId);
    });
}

bool FInventoryRecordStore::ValidateRecord(const FInventoryDomainRecord& InRecord, FString& OutError) const
{
    const FDefinition* Definition = Definitions.Find(InRecord.Key.Domain);
    if(!Definition || !InRecord.Key.EntityId.IsValid() || InRecord.Revision == 0 || InRecord.Bytes.Num() > Definition->MaxBytes)
    {
        OutError = TEXT("Inventory.DomainRecordInvalid");
        return false;
    }
    if(InRecord.bDeleted)
    {
        if(!InRecord.Bytes.IsEmpty())
        {
            OutError = TEXT("Inventory.TombstoneHasPayload");
            return false;
        }
        return true;
    }
    return Definition->Validator(InRecord.Bytes, OutError);
}

bool FInventoryRecordStore::ValidateRedo(const FInventoryDomainRedo& InRedo, FString& OutError) const
{
    const FInventoryDomainRecord* Before = Find(InRedo.Domain, InRedo.EntityId);
    if(InRedo.ExpectedRevision == MAX_uint64 || InRedo.TargetRevision != InRedo.ExpectedRevision + 1 ||
       (Before ? Before->Revision : 0) != InRedo.ExpectedRevision)
    {
        OutError = TEXT("Inventory.DomainRevisionStale");
        return false;
    }
    FInventoryDomainRecord After;
    After.Key = FInventoryRecordKey{InRedo.Domain, InRedo.EntityId};
    After.Revision = InRedo.TargetRevision;
    After.bDeleted = InRedo.bDelete;
    After.Bytes = InRedo.AfterBytes;
    return ValidateRecord(After, OutError);
}

bool FInventoryRecordStore::Apply(const FInventoryDomainRedo& InRedo, FString& OutError)
{
    const FInventoryDomainRecord* Existing = Find(InRedo.Domain, InRedo.EntityId);
    if(Existing && Existing->Revision == InRedo.TargetRevision)
    {
        if(Existing->bDeleted == InRedo.bDelete && Existing->Bytes == InRedo.AfterBytes)
        {
            return true;
        }
        OutError = TEXT("Inventory.DomainRedoDiverged");
        return false;
    }
    if(!ValidateRedo(InRedo, OutError))
    {
        return false;
    }
    FInventoryDomainRecord Record;
    Record.Key = FInventoryRecordKey{InRedo.Domain, InRedo.EntityId};
    Record.Revision = InRedo.TargetRevision;
    Record.bDeleted = InRedo.bDelete;
    Record.Bytes = InRedo.AfterBytes;
    Records.Add(Record.Key, MoveTemp(Record));
    return true;
}

bool FInventoryRecordStore::Restore(TConstArrayView<FInventoryDomainRecord> InRecords, FString& OutError)
{
    TMap<FInventoryRecordKey, FInventoryDomainRecord> Restored;
    for(const FInventoryDomainRecord& Record : InRecords)
    {
        if(Restored.Contains(Record.Key) || !ValidateRecord(Record, OutError))
        {
            if(OutError.IsEmpty())
            {
                OutError = TEXT("Inventory.DuplicateDomainRecord");
            }
            return false;
        }
        Restored.Add(Record.Key, Record);
    }
    Records = MoveTemp(Restored);
    return true;
}

void FInventoryRecordStore::Capture(TArray<FInventoryDomainRecord>& OutRecords) const
{
    Records.GenerateValueArray(OutRecords);
    OutRecords.Sort([](const FInventoryDomainRecord& InLeft, const FInventoryDomainRecord& InRight)
    {
        if(InLeft.Key.Domain != InRight.Key.Domain)
        {
            return InLeft.Key.Domain.LexicalLess(InRight.Key.Domain);
        }
        return InventoryOrder::GuidLess(InLeft.Key.EntityId, InRight.Key.EntityId);
    });
}
