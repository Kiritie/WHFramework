#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryDomain.h"

class UInventoryWorldSubsystem;

class WHFRAMEWORK_API FInventoryTransactionService
{
public:
    explicit FInventoryTransactionService(UInventoryWorldSubsystem& InWorld);
    FInventoryCommandResult Execute(const FInventoryBatchRequest& InRequest, const FInventoryAuthorityContext& InAuthority);
    void ForgetConnection(const FGuid& InConnectionId);

private:
    struct FReceipt
    {
        uint64 RequestId = 0;
        TArray<uint8> Identity;
        FInventoryCommandResult Result;
    };

    struct FConnectionReceipts
    {
        FGuid ActorId;
        uint64 HighWaterMark = 0;
        TArray<FReceipt> Recent;
    };

    UInventoryWorldSubsystem& World;
    TMap<FGuid, FConnectionReceipts> Connections;

    FInventoryCommandResult ExecuteNew(const FInventoryBatchRequest& InRequest, const FInventoryAuthorityContext& InAuthority);
};
